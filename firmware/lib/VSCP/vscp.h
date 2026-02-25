#pragma once

/**
 * @file vscp.h
 * @brief VSCP (Very Simple Control Protocol) Level 1 constants and encoding helpers.
 * @author TFM-100 project
 *
 * Defines the standard VSCP event classes, measurement types, data-coding
 * byte constants, and inline helpers used by the CAN and serial communication
 * layers. All definitions follow the VSCP specification (vscp.org).
 *
 * ### CAN ID compatibility with J1939Manager
 * J1939Manager::buildCanId() produces:
 *   can_id = (priority << 26) | (pgn << 8) | source_address
 *
 * The VSCP Level 1 CAN identifier (hardcoded = 0) has the same bit layout:
 *   can_id = (priority << 26) | (vscp_class << 16) | (vscp_type << 8) | node_id
 *
 * Therefore VSCP_PGN(class, type) = (class << 8) | type can be passed to
 * J1939Manager as the PGN and the resulting CAN ID is a valid VSCP Level 1 ID.
 */

#include <stdint.h>

/* ============================================================================
 * VSCP Event Classes (Level 1, 9-bit)
 * ========================================================================== */

#define VSCP_CLASS1_ALARM           1   ///< Alarm / error events
#define VSCP_CLASS1_INFORMATION    20   ///< General information events
#define VSCP_CLASS1_MEASUREMENT    10   ///< Measurement events

/* ============================================================================
 * VSCP Types – CLASS1.INFORMATION (class 20)
 * ========================================================================== */

#define VSCP_TYPE_INFORMATION_HEARTBEAT   9   ///< Node heartbeat (alive signal)

/* ============================================================================
 * VSCP Types – CLASS1.MEASUREMENT (class 10)
 * ========================================================================== */

#define VSCP_TYPE_MEASUREMENT_TEMPERATURE     6   ///< Temperature (K / °C / °F)
#define VSCP_TYPE_MEASUREMENT_ENERGY         10   ///< Energy (J or kWh)
#define VSCP_TYPE_MEASUREMENT_POWER          11   ///< Power (W or kW)
#define VSCP_TYPE_MEASUREMENT_VOLUME_FLOW    40   ///< Volume flow rate

/* ============================================================================
 * VSCP Measurement Data Coding byte (byte 0 of every CLASS1.MEASUREMENT event)
 *
 * Bit layout:  [7:5] coding-type  |  [4:3] unit  |  [2:0] sensor index
 * ========================================================================== */

/** Data coding types (bits [7:5]) */
#define VSCP_DCTYPE_NORMALIZED_INT  0   ///< Normalised integer (exponent + integer bytes)
#define VSCP_DCTYPE_SINGLE_FLOAT    1   ///< IEEE 754 single-precision float
#define VSCP_DCTYPE_STRING          2   ///< ASCII string

/** Units – Temperature (bits [4:3]) */
#define VSCP_UNIT_TEMP_KELVIN       0   ///< Kelvin
#define VSCP_UNIT_TEMP_CELSIUS      1   ///< Degree Celsius
#define VSCP_UNIT_TEMP_FAHRENHEIT   2   ///< Degree Fahrenheit

/** Units – Energy (bits [4:3]) */
#define VSCP_UNIT_ENERGY_JOULE      0   ///< Joule
#define VSCP_UNIT_ENERGY_KWH        1   ///< Kilowatt-hour

/** Units – Power (bits [4:3]) */
#define VSCP_UNIT_POWER_WATT        0   ///< Watt
#define VSCP_UNIT_POWER_KW          1   ///< Kilowatt

/** Units – Volume Flow (bits [4:3]) */
#define VSCP_UNIT_FLOW_M3S          0   ///< Cubic metres per second
#define VSCP_UNIT_FLOW_LS           1   ///< Litres per second
#define VSCP_UNIT_FLOW_LM           2   ///< Litres per minute
#define VSCP_UNIT_FLOW_LH           3   ///< Litres per hour

/* ============================================================================
 * VSCP PGN helper – make J1939Manager produce a VSCP Level 1 CAN identifier.
 * ========================================================================== */

/**
 * @brief Encode a VSCP class and type into a J1939Manager-compatible PGN.
 *
 * Usage:  uint32_t pgn = VSCP_PGN(VSCP_CLASS1_MEASUREMENT,
 *                                  VSCP_TYPE_MEASUREMENT_TEMPERATURE);
 */
#define VSCP_PGN(vscp_class, vscp_type) \
    (((uint32_t)(vscp_class) << 8) | ((uint32_t)(vscp_type)))

/* ============================================================================
 * Inline encoding helpers
 * ========================================================================== */

/**
 * @brief Build the VSCP measurement data-coding byte.
 *
 * @param dc_type    Coding type  (e.g. VSCP_DCTYPE_NORMALIZED_INT)
 * @param unit       Measurement unit (e.g. VSCP_UNIT_TEMP_CELSIUS)
 * @param sensor_idx Sensor index 0-7
 * @return           Coding byte to use as payload[0]
 */
static inline uint8_t vscp_coding_byte(uint8_t dc_type,
                                        uint8_t unit,
                                        uint8_t sensor_idx)
{
    return (uint8_t)(((dc_type   & 0x07u) << 5) |
                     ((unit      & 0x03u) << 3) |
                      (sensor_idx & 0x07u));
}

/**
 * @brief Encode a signed 16-bit normalised-integer measurement (4 bytes).
 *
 * Output layout: [coding_byte][exponent (int8)][value MSB][value LSB]
 *
 * @param buf        Output buffer – must hold at least 4 bytes
 * @param unit       Measurement unit constant
 * @param sensor_idx Sensor index 0-7
 * @param exponent   Signed decimal exponent (e.g. -2 → value × 10^-2)
 * @param value      Signed integer value
 */
static inline void vscp_encode_int16(uint8_t *buf,
                                      uint8_t  unit,
                                      uint8_t  sensor_idx,
                                      int8_t   exponent,
                                      int16_t  value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)((uint16_t)value >> 8);
    buf[3] = (uint8_t)((uint16_t)value & 0xFFu);
}

/**
 * @brief Encode an unsigned 16-bit normalised-integer measurement (4 bytes).
 *
 * Same layout as vscp_encode_int16 but for unsigned values.
 */
static inline void vscp_encode_uint16(uint8_t  *buf,
                                       uint8_t   unit,
                                       uint8_t   sensor_idx,
                                       int8_t    exponent,
                                       uint16_t  value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value & 0xFFu);
}

/**
 * @brief Encode an unsigned 32-bit normalised-integer measurement (6 bytes).
 *
 * Output layout: [coding_byte][exponent][MSB..LSB of uint32]
 *
 * @param buf        Output buffer – must hold at least 6 bytes
 */
static inline void vscp_encode_uint32(uint8_t  *buf,
                                       uint8_t   unit,
                                       uint8_t   sensor_idx,
                                       int8_t    exponent,
                                       uint32_t  value)
{
    buf[0] = vscp_coding_byte(VSCP_DCTYPE_NORMALIZED_INT, unit, sensor_idx);
    buf[1] = (uint8_t)exponent;
    buf[2] = (uint8_t)((value >> 24) & 0xFFu);
    buf[3] = (uint8_t)((value >> 16) & 0xFFu);
    buf[4] = (uint8_t)((value >>  8) & 0xFFu);
    buf[5] = (uint8_t)( value        & 0xFFu);
}
