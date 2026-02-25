/**
 * @file vscp_node.cpp
 * @brief VSCP node integration: transport, nickname discovery, decision matrix.
 *
 * Uses the BlueAndi/vscp-arduino library (MIT) to implement:
 *   - CAN address negotiation (VSCP nickname discovery, CLASS1.PROTOCOL)
 *   - Decision matrix (DM) with factory-default entries for energy reset
 *   - CAN transport read/write callbacks bridging vscp-arduino to MCP2515
 *
 * See vscp_node.h for the public API.
 */

#include "vscp_node.h"

#include <Arduino.h>
#include <mcp_can.h>

// vscp-arduino library main class
#include <VSCP.h>

// Core headers for DM/PS direct access (needed for factory-default init)
#include <framework/core/vscp_ps.h>
#include <framework/core/vscp_dm.h>
#include <framework/core/vscp_class_l1.h>
#include <framework/core/vscp_type_control.h>
#include <framework/core/vscp_types.h>

#include "config.h"
#include "energy.h"

// ---------------------------------------------------------------------------
// External objects defined in main.cpp
// ---------------------------------------------------------------------------
extern MCP_CAN CAN0;
extern Energy  energyObj;

// ---------------------------------------------------------------------------
// Global VSCP instance (used by vscp_node_process via vscp.process())
// ---------------------------------------------------------------------------
VSCP vscp;

// ---------------------------------------------------------------------------
// CAN transport – READ callback
//
// Called by the VSCP core each time it needs to process an incoming frame.
// Returns TRUE and fills *rxMsg if a frame is available; FALSE otherwise.
// ---------------------------------------------------------------------------
static BOOL vscp_can_read(vscp_RxMessage * const rxMsg)
{
    if (rxMsg == nullptr)
        return FALSE;

    if (CAN0.checkReceive() != CAN_MSGAVAIL)
        return FALSE;

    INT32U canId = 0;
    INT8U  ext   = 0;
    INT8U  len   = 0;
    INT8U  data[8] = {0};

    CAN0.readMsgBuf(&canId, &ext, &len, data);

    // Ignore standard (11-bit) frames – VSCP Level 1 always uses extended frames
    if (ext == 0)
        return FALSE;

    // Parse VSCP Level 1 29-bit CAN ID:
    //   bits [28:26] = priority
    //   bit  [25]    = hard-coded flag
    //   bits [24:16] = VSCP class (9 bits)
    //   bits [15:8]  = VSCP type
    //   bits [7:0]   = originating node address
    rxMsg->priority  = (VSCP_PRIORITY)((canId >> 26) & 0x07u);
    rxMsg->hardCoded = ((canId >> 25) & 0x01u) ? TRUE : FALSE;
    rxMsg->vscpClass = (uint16_t)((canId >> 16) & 0x01FFu);
    rxMsg->vscpType  = (uint8_t) ((canId >>  8) & 0xFFu);
    rxMsg->oAddr     = (uint8_t) ( canId        & 0xFFu);
    rxMsg->dataSize  = (len <= VSCP_L1_DATA_SIZE) ? len : VSCP_L1_DATA_SIZE;
    for (uint8_t i = 0; i < rxMsg->dataSize; ++i)
        rxMsg->data[i] = data[i];

    return TRUE;
}

// ---------------------------------------------------------------------------
// CAN transport – WRITE callback
//
// Called by the VSCP core to send a protocol frame (probe, ACK, etc.).
// Builds the 29-bit CAN ID from the message fields and forwards to MCP2515.
// ---------------------------------------------------------------------------
static BOOL vscp_can_write(vscp_TxMessage const * const txMsg)
{
    if (txMsg == nullptr || txMsg->dataSize > VSCP_L1_DATA_SIZE)
        return FALSE;

    // Reconstruct VSCP Level 1 CAN ID from message fields
    uint32_t canId = ((uint32_t)(txMsg->priority)                    << 26) |
                     ((uint32_t)(txMsg->hardCoded ? 0x01u : 0x00u)   << 25) |
                     ((uint32_t)(txMsg->vscpClass)                   << 16) |
                     ((uint32_t)(txMsg->vscpType)                    <<  8) |
                      (uint32_t)(txMsg->oAddr);

    // Send extended frame (1) via MCP2515
    byte ret = CAN0.sendMsgBuf(canId, 1, txMsg->dataSize,
                                const_cast<byte *>(txMsg->data));
    return (ret == CAN_OK) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Decision matrix – action handler
//
// Called by the VSCP core when an incoming event matches a DM row.
// action  : application-defined action ID (DM_ACTION_ENERGY_RESET)
// par     : action parameter (0x01 = 24h only, 0xFF = 24h + total)
// ---------------------------------------------------------------------------
static void vscp_action_handler(unsigned char action,
                                 unsigned char par,
                                 vscp_RxMessage const * const /* msg */)
{
    if (action == DM_ACTION_ENERGY_RESET)
    {
        if (par == 0x01u)
        {
            energyObj.reset24h();
        }
        else if (par == 0xFFu)
        {
            energyObj.resetAll();
        }
    }
}

// ---------------------------------------------------------------------------
// DM factory defaults
//
// Writes two decision-matrix rows to EEPROM on first boot (when the DM
// region is blank / all 0xFF).  Both rows match CLASS1.CONTROL / Reset
// events sent to this node; actionPar selects 24h-only or full reset.
//
// DM row layout (VSCP_DM_ROW_SIZE = 8 bytes):
//   [0] oaddr       – originating address (ignored; CHECK_OADDR flag not set)
//   [1] flags       – VSCP_DM_FLAG_ENABLE (0x80)
//   [2] classMask   – 0xFF (match lower 8 bits of class)
//   [3] classFilter – VSCP_CLASS_L1_CONTROL (30)
//   [4] typeMask    – 0xFF (match all type bits)
//   [5] typeFilter  – VSCP_TYPE_CONTROL_RESET (9)
//   [6] action      – DM_ACTION_ENERGY_RESET
//   [7] actionPar   – 0x01 (row 0) or 0xFF (row 1)
// ---------------------------------------------------------------------------
static void vscp_dm_init_factory_defaults()
{
    // Read action byte of row 0 (offset 6 within the DM region)
    uint8_t existingAction = 0xFFu;
    vscp_ps_readDMMultiple(6u, &existingAction, 1u);

    if (existingAction == DM_ACTION_ENERGY_RESET)
        return;  // Already initialised

    vscp_dm_MatrixRow row0, row1;

    row0.oaddr       = 0x00u;
    row0.flags       = VSCP_DM_FLAG_ENABLE;
    row0.classMask   = 0xFFu;
    row0.classFilter = (uint8_t)VSCP_CLASS_L1_CONTROL;
    row0.typeMask    = 0xFFu;
    row0.typeFilter  = (uint8_t)VSCP_TYPE_CONTROL_RESET;
    row0.action      = DM_ACTION_ENERGY_RESET;
    row0.actionPar   = 0x01u;  // 24h counter only

    row1.oaddr       = 0x00u;
    row1.flags       = VSCP_DM_FLAG_ENABLE;
    row1.classMask   = 0xFFu;
    row1.classFilter = (uint8_t)VSCP_CLASS_L1_CONTROL;
    row1.typeMask    = 0xFFu;
    row1.typeFilter  = (uint8_t)VSCP_TYPE_CONTROL_RESET;
    row1.action      = DM_ACTION_ENERGY_RESET;
    row1.actionPar   = 0xFFu;  // 24h + total

    vscp_ps_writeDMMultiple(0u,              reinterpret_cast<const uint8_t *>(&row0), VSCP_DM_ROW_SIZE);
    vscp_ps_writeDMMultiple(VSCP_DM_ROW_SIZE, reinterpret_cast<const uint8_t *>(&row1), VSCP_DM_ROW_SIZE);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void vscp_node_setup(uint8_t node_id)
{
    // Build a 16-byte GUID from firmware constants and node_id.
    // Format: FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:MODEL:VARIANT:FW:NODE
    VSCPGuid guid;
    guid[0]  = 0xFFu; guid[1]  = 0xFFu; guid[2]  = 0xFFu; guid[3]  = 0xFFu;
    guid[4]  = 0xFFu; guid[5]  = 0xFFu; guid[6]  = 0xFFu; guid[7]  = 0xFEu;
    guid[8]  = 0x00u; guid[9]  = 0x00u; guid[10] = 0x00u; guid[11] = 0x00u;
    guid[12] = (uint8_t)MODEL;
    guid[13] = (uint8_t)VARIANT;
    guid[14] = (uint8_t)FIRMWARE_VERSION;
    guid[15] = node_id;

    // Initialise the vscp-arduino library.
    // Transport + action callbacks are registered here; nickname discovery
    // will start automatically on the first vscp.process() call.
    vscp.setup(
        LED_GREEN,            // status lamp: blinks during nickname discovery
        VSCP_INIT_BUTTON_PIN, // ground to re-trigger nickname discovery
        guid,
        0xFFu,                // zone:     0xFF = all zones
        0xFFu,                // sub-zone: 0xFF = all sub-zones
        vscp_can_read,
        vscp_can_write,
        vscp_action_handler
    );

    // Write factory-default decision-matrix entries if EEPROM is blank.
    vscp_dm_init_factory_defaults();
}

void vscp_node_process()
{
    vscp.process();
}
