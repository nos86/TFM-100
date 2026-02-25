#pragma once

/**
 * @file vscp_node.h
 * @brief VSCP node integration using the BlueAndi/vscp-arduino library.
 *
 * This module integrates the open-source vscp-arduino library to provide:
 *
 *  1. **CAN address negotiation** – VSCP nickname discovery (CLASS1.PROTOCOL)
 *     runs automatically inside vscp_node_process(); no segment controller
 *     is required.  The acquired nickname is persisted in EEPROM (0x00–0x07).
 *
 *  2. **Decision matrix (DM)** – 2 EEPROM-backed rows are factory-initialised
 *     to respond to CLASS1.CONTROL / Reset events:
 *       row 0 (0x08–0x0F): actionPar=0x01 → reset 24h energy counter
 *       row 1 (0x10–0x17): actionPar=0xFF → reset 24h + total energy counters
 *     The DM is implemented manually (VSCP_CONFIG_ENABLE_DM=0 keeps the
 *     library's DM module out of flash to fit the 28 KB ATmega32U4 limit).
 *
 *  3. **VSCP transport** – incoming CAN frames are forwarded to the VSCP
 *     core for protocol processing via the read callback.  Outgoing VSCP
 *     protocol frames (probe, ACK, …) are sent via the write callback.
 *
 * Outgoing measurement events (temperature, flow, power, energy) continue
 * to use J1939Manager + VSCP_PGN() which produces identical CAN identifiers.
 *
 * @note EEPROM layout (VSCP_CONFIG_ENABLE_DM=0):
 *   0x00–0x07: VSCP core (nickname, seg-CRC, flags, user-ID)
 *   0x08–0x0F: Manual DM row 0
 *   0x10–0x17: Manual DM row 1
 *   0x1A:      POWER region (unchanged)
 *   0x20:      ENERGY region (unchanged)
 *   0x40:      DIAGNOSTICS region (unchanged)
 */

#include <stdint.h>

// ---- DM action IDs -------------------------------------------------------
/// Action ID used in manual DM rows for energy counter reset.
/// actionPar = 0x01 → 24h only; 0xFF → 24h + total
#define DM_ACTION_ENERGY_RESET   0x10

// ---- VSCP init button ----------------------------------------------------
/// Pin used to trigger a new nickname-discovery cycle (INPUT_PULLUP).
/// Ground this pin to start segment initialisation.
#define VSCP_INIT_BUTTON_PIN     5

// ---- Public API ----------------------------------------------------------
/**
 * @brief Initialise the VSCP node layer.
 *
 * Must be called once in setup(), **after** the MCP2515 CAN controller has
 * been initialised.  Configures the library transport callbacks, sets device
 * data (GUID/zone/sub-zone), writes factory-default DM entries if the EEPROM
 * is blank, and starts the nickname discovery state machine.
 *
 * @param node_id  Physical node address read from the DIP switches.
 */
void vscp_node_setup(uint8_t node_id);

/**
 * @brief Drive the VSCP protocol stack.
 *
 * Must be called every main-loop iteration.  Runs the nickname-discovery /
 * segment-controller handshake, ticks the VSCP timer, handles the init
 * button, and executes the manual decision matrix for received events.
 */
void vscp_node_process();
