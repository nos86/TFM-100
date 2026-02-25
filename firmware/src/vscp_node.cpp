/**
 * @file vscp_node.cpp
 * @brief VSCP node: nickname discovery, manual decision matrix, CAN transport.
 *
 * Uses the BlueAndi/vscp-arduino library (MIT) low-level C API directly.
 * Avoids the C++ VSCP class wrapper (VSCP.cpp + SwTimer + DigInDebounce) to
 * minimise flash usage on the ATmega32U4 (28 KB limit).
 *
 * Flash savings vs C++ wrapper approach:
 *   - VSCP_CONFIG_ENABLE_DM=0 removes vscp_dm.c from linking (~2-3 KB)
 *   - No VSCP C++ object removes VSCP.cpp, SwTimer, DigInDebounce (~1-2 KB)
 *   - 2-row manual DM adds only ~100 bytes
 *
 * EEPROM layout (VSCP_CONFIG_ENABLE_DM=0, all optional PS storage disabled):
 *   0x00: nickname (1 byte)
 *   0x01: segment controller CRC (1 byte)
 *   0x02: node control flags (1 byte)
 *   0x03–0x07: user ID (5 bytes)
 *   0x08–0x0F: DM row 0 (8 bytes, manual)
 *   0x10–0x17: DM row 1 (8 bytes, manual)
 *   0x1A: POWER region (unchanged)
 *   0x20: ENERGY region (unchanged)
 *   0x40: DIAGNOSTICS region (unchanged)
 */

#include "vscp_node.h"

#include <Arduino.h>
#include <mcp_can.h>

// vscp-arduino library – low-level C core API only (no C++ VSCP class)
#include <framework/core/vscp_core.h>
#include <framework/core/vscp_dev_data.h>
#include <framework/user/vscp_timer.h>
#include <framework/user/vscp_tp_adapter.h>
#include <framework/core/vscp_class_l1.h>
#include <framework/core/vscp_type_control.h>
#include <framework/core/vscp_types.h>

#include <avr/eeprom.h>

#include "config.h"
#include "energy.h"

// ---------------------------------------------------------------------------
// External objects defined in main.cpp
// ---------------------------------------------------------------------------
extern MCP_CAN CAN0;
extern Energy  energyObj;

// ---------------------------------------------------------------------------
// Manual 2-row DM stored in EEPROM at 0x08–0x17.
// Row format matches vscp_dm_MatrixRow (8 bytes):
//   [0] oaddr, [1] flags, [2] classMask, [3] classFilter,
//   [4] typeMask, [5] typeFilter, [6] action, [7] actionPar
// ---------------------------------------------------------------------------
static const uint16_t DM_BASE     = 0x08u;
static const uint8_t  DM_ROW_SZ   = 8u;
static const uint8_t  DM_NUM_ROWS = 2u;
static const uint8_t  DM_FLAG_EN  = 0x80u;

// VSCP timer period (ms) – required by vscp_core for discovery timeouts
static const uint16_t VSCP_TIMER_MS = 250u;
static uint32_t s_timer_last = 0u;

// Init-button state
static int s_btn_last = HIGH;

// ---------------------------------------------------------------------------
// CAN transport – READ callback
//
// Called by vscp_core_process() for every message it needs to read.
// We also run the manual DM here: if the incoming event matches a DM row
// we execute the action immediately (before returning to the core).
// ---------------------------------------------------------------------------
static BOOL vscp_can_read(vscp_RxMessage * const rxMsg)
{
    if (rxMsg == nullptr)             return FALSE;
    if (CAN0.checkReceive() != CAN_MSGAVAIL) return FALSE;

    INT32U canId = 0u;
    INT8U  ext = 0, len = 0;
    INT8U  data[8] = {0};
    CAN0.readMsgBuf(&canId, &ext, &len, data);

    if (ext == 0) return FALSE;  // VSCP Level 1 uses extended frames only

    rxMsg->priority  = (VSCP_PRIORITY)((canId >> 26u) & 0x07u);
    rxMsg->hardCoded = ((canId >> 25u) & 0x01u) ? TRUE : FALSE;
    rxMsg->vscpClass = (uint16_t)((canId >> 16u) & 0x01FFu);
    rxMsg->vscpType  = (uint8_t) ((canId >>  8u) & 0xFFu);
    rxMsg->oAddr     = (uint8_t) ( canId          & 0xFFu);
    rxMsg->dataSize  = (len <= VSCP_L1_DATA_SIZE) ? len : VSCP_L1_DATA_SIZE;
    for (uint8_t i = 0u; i < rxMsg->dataSize; ++i) rxMsg->data[i] = data[i];

    // Manual DM: match event against DM rows and execute action.
    // With VSCP_CONFIG_ENABLE_DM=0, the core does not process the DM, so we
    // intercept here while the message is still in hand.
    dm_execute(rxMsg);

    return TRUE;
}

// ---------------------------------------------------------------------------
// CAN transport – WRITE callback
// ---------------------------------------------------------------------------
static BOOL vscp_can_write(vscp_TxMessage const * const txMsg)
{
    if (txMsg == nullptr || txMsg->dataSize > VSCP_L1_DATA_SIZE) return FALSE;

    uint32_t canId =
        ((uint32_t)(txMsg->priority)             << 26u) |
        ((uint32_t)(txMsg->hardCoded ? 1u : 0u)  << 25u) |
        ((uint32_t)(txMsg->vscpClass)            << 16u) |
        ((uint32_t)(txMsg->vscpType)             <<  8u) |
         (uint32_t)(txMsg->oAddr);

    byte ret = CAN0.sendMsgBuf(canId, 1u, txMsg->dataSize,
                                const_cast<byte *>(txMsg->data));
    return (ret == CAN_OK) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Manual DM – factory defaults (written once on blank EEPROM)
// ---------------------------------------------------------------------------
static void dm_init_defaults()
{
    // Quick check: if action byte of row 0 is already correct, skip
    if (eeprom_read_byte((const uint8_t *)(DM_BASE + 6u)) == DM_ACTION_ENERGY_RESET)
        return;

    // Row 0: CLASS1.CONTROL / Reset → reset 24h counter (par = 0x01)
    eeprom_write_byte((uint8_t *)(DM_BASE + 0u), 0x00u);
    eeprom_write_byte((uint8_t *)(DM_BASE + 1u), DM_FLAG_EN);
    eeprom_write_byte((uint8_t *)(DM_BASE + 2u), 0xFFu);
    eeprom_write_byte((uint8_t *)(DM_BASE + 3u), (uint8_t)VSCP_CLASS_L1_CONTROL);
    eeprom_write_byte((uint8_t *)(DM_BASE + 4u), 0xFFu);
    eeprom_write_byte((uint8_t *)(DM_BASE + 5u), (uint8_t)VSCP_TYPE_CONTROL_RESET);
    eeprom_write_byte((uint8_t *)(DM_BASE + 6u), DM_ACTION_ENERGY_RESET);
    eeprom_write_byte((uint8_t *)(DM_BASE + 7u), 0x01u);

    // Row 1: CLASS1.CONTROL / Reset → reset 24h + total (par = 0xFF)
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 0u), 0x00u);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 1u), DM_FLAG_EN);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 2u), 0xFFu);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 3u), (uint8_t)VSCP_CLASS_L1_CONTROL);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 4u), 0xFFu);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 5u), (uint8_t)VSCP_TYPE_CONTROL_RESET);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 6u), DM_ACTION_ENERGY_RESET);
    eeprom_write_byte((uint8_t *)(DM_BASE + DM_ROW_SZ + 7u), 0xFFu);
}

// ---------------------------------------------------------------------------
// Manual DM – match received event against rows and execute action
// ---------------------------------------------------------------------------
static void dm_execute(vscp_RxMessage const * const msg)
{
    if (msg == nullptr) return;

    for (uint8_t row = 0u; row < DM_NUM_ROWS; ++row)
    {
        uint16_t base  = DM_BASE + (uint16_t)(row * DM_ROW_SZ);
        uint8_t  flags = eeprom_read_byte((const uint8_t *)(base + 1u));
        if (!(flags & DM_FLAG_EN)) continue;

        uint8_t cMask = eeprom_read_byte((const uint8_t *)(base + 2u));
        uint8_t cFilt = eeprom_read_byte((const uint8_t *)(base + 3u));
        uint8_t tMask = eeprom_read_byte((const uint8_t *)(base + 4u));
        uint8_t tFilt = eeprom_read_byte((const uint8_t *)(base + 5u));

        if (((uint8_t)msg->vscpClass & cMask) != cFilt) continue;
        if ((msg->vscpType           & tMask) != tFilt)  continue;

        uint8_t action    = eeprom_read_byte((const uint8_t *)(base + 6u));
        uint8_t actionPar = eeprom_read_byte((const uint8_t *)(base + 7u));

        if (action == DM_ACTION_ENERGY_RESET)
        {
            if (actionPar == 0x01u)      energyObj.reset24h();
            else if (actionPar == 0xFFu) energyObj.resetAll();
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void vscp_node_setup(uint8_t node_id)
{
    // Build GUID: FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:MODEL:VARIANT:FW:NODE
    vscp_dev_data_Container devData;
    devData.guid[0]  = 0xFFu; devData.guid[1]  = 0xFFu;
    devData.guid[2]  = 0xFFu; devData.guid[3]  = 0xFFu;
    devData.guid[4]  = 0xFFu; devData.guid[5]  = 0xFFu;
    devData.guid[6]  = 0xFFu; devData.guid[7]  = 0xFEu;
    devData.guid[8]  = 0x00u; devData.guid[9]  = 0x00u;
    devData.guid[10] = 0x00u; devData.guid[11] = 0x00u;
    devData.guid[12] = (uint8_t)MODEL;
    devData.guid[13] = (uint8_t)VARIANT;
    devData.guid[14] = (uint8_t)FIRMWARE_VERSION;
    devData.guid[15] = node_id;
    devData.zone     = 0xFFu;
    devData.subZone  = 0xFFu;
    vscp_dev_data_set(&devData);

    // Register CAN transport callbacks
    vscp_tp_adapter_set(vscp_can_read, vscp_can_write);

    // Initialise the VSCP core – starts nickname discovery on first process()
    (void)vscp_core_init();

    // Init button: ground to re-trigger nickname discovery
    pinMode(VSCP_INIT_BUTTON_PIN, INPUT_PULLUP);
    s_btn_last    = HIGH;
    s_timer_last  = millis();

    // Write factory-default DM entries if EEPROM is blank
    dm_init_defaults();
}

void vscp_node_process()
{
    // Drive the VSCP state machine (nickname discovery, protocol handling)
    (void)vscp_core_process();

    // Timer tick required by vscp_core for discovery/probe timeouts
    uint32_t now = millis();
    if ((now - s_timer_last) >= VSCP_TIMER_MS)
    {
        vscp_timer_process(VSCP_TIMER_MS);
        s_timer_last = now;
    }

    // Init button: ground VSCP_INIT_BUTTON_PIN to restart nickname discovery
    int btn = digitalRead(VSCP_INIT_BUTTON_PIN);
    if (btn == LOW && s_btn_last == HIGH)
        vscp_core_startNodeSegmentInit();
    s_btn_last = btn;
}
