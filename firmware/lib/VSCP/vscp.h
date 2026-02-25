#pragma once

/**
 * @file vscp.h
 * @brief Thin wrapper over the open-source vscp-arduino library.
 *
 * Includes the standard VSCP Level 1 class/type constants from
 * BlueAndi/vscp-arduino (https://github.com/BlueAndi/vscp-arduino).
 * Adds project-specific aliases and the payload-encoding helpers used
 * by can_messages.cpp and vscp_messages.h.
 *
 * Address negotiation (nickname discovery) and the decision matrix are
 * implemented in vscp_node.cpp using the VSCP C++ class from the library.
 */

/* ============================================================================
 * Standard VSCP Level 1 class and type constants from vscp-arduino library.
 * ========================================================================== */
#include <framework/core/vscp_class_l1.h>
#include <framework/core/vscp_type_measurement.h>
#include <framework/core/vscp_type_information.h>
#include <framework/core/vscp_type_control.h>

#include <stdint.h>

/* ============================================================================
 * Project aliases for library class constants.
 * ========================================================================== */
#define VSCP_CLASS1_MEASUREMENT   VSCP_CLASS_L1_MEASUREMENT   ///< 10
#define VSCP_CLASS1_INFORMATION   VSCP_CLASS_L1_INFORMATION   ///< 20
#define VSCP_CLASS1_ALARM         VSCP_CLASS_L1_ALARM         ///<  1

/// Convenience alias for backward compatibility.
#define VSCP_TYPE_INFORMATION_HEARTBEAT  VSCP_TYPE_INFORMATION_NODE_HEARTBEAT  ///< 9

/* ============================================================================
 * VSCP Measurement Data Coding byte (byte 0 of every CLASS1.MEASUREMENT event)
 * Bit layout:  [7:5] coding-type  |  [4:3] unit  |  [2:0] sensor index
 * ========================================================================== */

/** Data coding type: normalised integer (bits [7:5]) */
#define VSCP_DCTYPE_NORMALIZED_INT  0

/** Temperature unit: Degree Celsius (opt. unit 1 per VSCP spec for Type 6) */
#define VSCP_UNIT_TEMP_CELSIUS      1

/** Energy unit: Kilowatt-hour (opt. unit 1 per VSCP spec for Type 13) */
#define VSCP_UNIT_ENERGY_KWH        1

/** Power unit: Watt (default unit per VSCP spec for Type 14) */
#define VSCP_UNIT_POWER_WATT        0

/** Flow unit: Litres per second (opt. unit 1 per VSCP spec for Type 36) */
#define VSCP_UNIT_FLOW_LS           1

/* ============================================================================
 * VSCP PGN helper – make J1939Manager produce a VSCP Level 1 CAN identifier.
 *
 * J1939Manager::buildCanId() produces:
 *   can_id = (priority << 26) | (pgn << 8) | source_address
 * The VSCP Level 1 29-bit CAN ID layout (hard-coded=0) is:
 *   can_id = (priority << 26) | (vscp_class << 16) | (vscp_type << 8) | node_id
 * Setting pgn = VSCP_PGN(class, type) = (class << 8) | type maps them identically.
 * ========================================================================== */
#define VSCP_PGN(vscp_class, vscp_type) \
    (((uint32_t)(vscp_class) << 8) | ((uint32_t)(vscp_type)))

/* ============================================================================
 * Inline encoding helpers for CLASS1.MEASUREMENT normalised-integer payloads.
 * Output: [coding_byte][exponent (int8)][value bytes MSB first]
 * ========================================================================== */

static inline uint8_t vscp_coding_byte(uint8_t dc_type,
                                        uint8_t unit,
                                        uint8_t sensor_idx)
{
    return (uint8_t)(((dc_type    & 0x07u) << 5) |
                     ((unit       & 0x03u) << 3) |
                      (sensor_idx & 0x07u));
}

static inline void vscp_encode_int16(uint8_t *buf, uint8_t unit,
                                      uint8_t sensor_idx, int8_t exponent,
                                      int16_t value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)((uint16_t)value >> 8);
    buf[3] = (uint8_t)((uint16_t)value & 0xFFu);
}

static inline void vscp_encode_uint16(uint8_t *buf, uint8_t unit,
                                       uint8_t sensor_idx, int8_t exponent,
                                       uint16_t value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value & 0xFFu);
}

static inline void vscp_encode_uint32(uint8_t *buf, uint8_t unit,
                                       uint8_t sensor_idx, int8_t exponent,
                                       uint32_t value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)((value >> 24) & 0xFFu);
    buf[3] = (uint8_t)((value >> 16) & 0xFFu);
    buf[4] = (uint8_t)((value >>  8) & 0xFFu);
    buf[5] = (uint8_t)( value        & 0xFFu);
}
