#pragma once

/**
 * @file vscp_node.h
 * @brief VSCP node integration using the BlueAndi/vscp-arduino library.
 *
 * This module integrates the open-source vscp-arduino library to provide:
 *
 *  1. **CAN address negotiation** – VSCP nickname discovery (CLASS1.PROTOCOL)
 *     runs automatically inside vscp_node_process(); no segment controller
 *     is required.  The acquired nickname is persisted in EEPROM (0x00–0x17).
 *
 *  2. **Decision matrix (DM)** – 2 EEPROM-backed rows are factory-initialised
 *     to respond to CLASS1.CONTROL / Reset events:
 *       row 0: actionPar=0x01 → reset 24h energy counter
 *       row 1: actionPar=0xFF → reset 24h + total energy counters
 *     The DM can be reconfigured remotely via VSCP register-write protocol.
 *
 *  3. **VSCP transport** – incoming CAN frames are forwarded to the VSCP
 *     core for protocol processing via the read callback.  Outgoing VSCP
 *     protocol frames (probe, ACK, …) are sent via the write callback.
 *
 * Outgoing measurement events (temperature, flow, power, energy) continue
 * to use J1939Manager + VSCP_PGN() which produces identical CAN identifiers.
 *
 * @note EEPROM layout: VSCP core uses 0x00–0x17 (8 bytes node data +
 *       2 DM rows × 8 bytes = 24 bytes).  Existing project regions
 *       (POWER @ 0x1A, ENERGY @ 0x20, DIAGNOSTICS @ 0x40) are unaffected.
 */

#include <stdint.h>

// ---- DM action IDs -------------------------------------------------------
/// Reset 24h energy counter only (used as DM action)
#define DM_ACTION_ENERGY_RESET   0x10
/// actionPar = 0x01 → 24h only; 0xFF → 24h + total

// ---- VSCP init button ----------------------------------------------------
/// Pin used to trigger a new nickname-discovery cycle (INPUT_PULLUP).
/// Ground this pin to start segment initialisation.
#define VSCP_INIT_BUTTON_PIN     5

// ---- Public API ----------------------------------------------------------
/**
 * @brief Initialise the VSCP node layer.
 *
 * Must be called once in setup(), **after** the MCP2515 CAN controller has
 * been initialised.  Configures the library transport/action callbacks,
 * writes factory-default DM entries if the EEPROM is blank, and starts the
 * nickname discovery state machine.
 *
 * @param node_id  Physical node address read from the DIP switches.
 */
void vscp_node_setup(uint8_t node_id);

/**
 * @brief Drive the VSCP protocol stack.
 *
 * Must be called every main-loop iteration.  Reads incoming CAN frames,
 * runs the nickname-discovery / segment-controller handshake, processes the
 * decision matrix, and manages the node status lamp.
 */
void vscp_node_process();
