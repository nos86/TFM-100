/**
 * @file can_messages.cpp
 * @brief VSCP Level 1 message payload builders for the TFM-100.
 *
 * Each buildPayload() produces a VSCP CLASS1.MEASUREMENT or
 * CLASS1.INFORMATION event payload encoded as a normalised integer:
 *   byte 0 – data coding byte  (type | unit | sensor index)
 *   byte 1 – decimal exponent  (signed int8, e.g. -2 → value × 10^-2)
 *   bytes 2+ – integer value   (MSB first / big-endian)
 *
 * The returned buffer is heap-allocated with new[]; J1939Manager copies it
 * into its internal pool then calls delete[].
 *
 * CAN identifier format (VSCP Level 1):
 *   bits [28:26] priority | bits [24:16] vscp_class | bits [15:8] vscp_type
 *   | bits [7:0] source node id
 * J1939Manager::buildCanId() produces this automatically when getPGN() returns
 * VSCP_PGN(class, type).
 */

#include <Arduino.h>
#include <flow.h>
#include <energy.h>
#include <scheduler.h>
#include "can_messages.h"
#include "config.h"
#include "PT100.h"
#include "status.h"

#ifndef FIRMWARE_VERSION
#warning "FIRMWARE_VERSION not defined"
#elif FIRMWARE_VERSION == 0xFF
#warning "FIRMWARE_VERSION is 0xFF, please update it"
#endif

#ifndef MODEL
#warning "MODEL not defined"
#endif

#ifndef VARIANT
#warning "VARIANT not defined"
#endif

// External application variables (defined in main.cpp)
extern Flow flowObj;
extern PT100 supply_sensor;
extern PT100 return_sensor;
extern Scheduler scheduler;
extern NodeStatus_t node_status;
extern Energy energyObj;

/* ============================================================================
 * HeartbeatMessage – VSCP CLASS1.INFORMATION, Type Heartbeat
 * Payload (8 bytes):
 *   byte 0: node status (NodeStatus_t enum value)
 *   byte 1: firmware version
 *   byte 2: (variant[3:0] << 4) | model[3:0]
 *   byte 3: reserved (0x00)
 *   bytes 4-7: uptime in seconds, big-endian uint32
 * ========================================================================== */
uint8_t *HeartbeatMessage::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 8;
    uint8_t *data = new uint8_t[8];
    data[0] = (uint8_t)node_status;
    data[1] = (uint8_t)(FIRMWARE_VERSION & 0xFFu);
    data[2] = (uint8_t)(((VARIANT & 0x0Fu) << 4) | (MODEL & 0x0Fu));
    data[3] = 0x00u; // reserved
    uint32_t uptime = scheduler.getUptime();
    data[4] = (uint8_t)((uptime >> 24) & 0xFFu);
    data[5] = (uint8_t)((uptime >> 16) & 0xFFu);
    data[6] = (uint8_t)((uptime >>  8) & 0xFFu);
    data[7] = (uint8_t)( uptime        & 0xFFu);
    return data;
}

/* ============================================================================
 * VSCP_SupplyTemperature – CLASS1.MEASUREMENT, Temperature, sensor 0
 * Celsius, exponent -2  (value = centi-degrees, e.g. 2345 → 23.45 °C)
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_SupplyTemperature::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    int32_t raw = (int32_t)roundf(supply_sensor.average_temperature * 100.0f);
    if (raw > 32767)  raw = 32767;
    if (raw < -32768) raw = -32768;

    uint8_t *data = new uint8_t[4];
    vscp_encode_int16(data, VSCP_UNIT_TEMP_CELSIUS, 0, -2, (int16_t)raw);
    return data;
}

/* ============================================================================
 * VSCP_ReturnTemperature – CLASS1.MEASUREMENT, Temperature, sensor 1
 * Celsius, exponent -2
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_ReturnTemperature::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    int32_t raw = (int32_t)roundf(return_sensor.average_temperature * 100.0f);
    if (raw > 32767)  raw = 32767;
    if (raw < -32768) raw = -32768;

    uint8_t *data = new uint8_t[4];
    vscp_encode_int16(data, VSCP_UNIT_TEMP_CELSIUS, 1, -2, (int16_t)raw);
    return data;
}

/* ============================================================================
 * VSCP_FlowRate – CLASS1.MEASUREMENT, Volume Flow, sensor 0
 * Unit: litres per hour, exponent 0
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_FlowRate::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    uint16_t flow = flowObj.getFlow();

    uint8_t *data = new uint8_t[4];
    vscp_encode_uint16(data, VSCP_UNIT_FLOW_LH, 0, 0, flow);
    return data;
}

/* ============================================================================
 * VSCP_Power – CLASS1.MEASUREMENT, Power, sensor 0
 * Unit: Watts (kW × 1000), exponent 0, stored as uint16
 * Clamped to [0, 65535 W].
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_Power::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    float kw = getThermalPower(supply_sensor.average_temperature,
                               return_sensor.average_temperature,
                               flowObj.getFlow());
    float watts = kw * 1000.0f;
    if (watts < 0.0f)   watts = 0.0f;
    if (watts > 65535.0f) watts = 65535.0f;
    uint16_t w = (uint16_t)roundf(watts);

    uint8_t *data = new uint8_t[4];
    vscp_encode_uint16(data, VSCP_UNIT_POWER_WATT, 0, 0, w);
    return data;
}

/* ============================================================================
 * VSCP_Energy24h – CLASS1.MEASUREMENT, Energy, sensor 0
 * Unit: kWh, exponent -2  (value = centi-kWh, max 655.35 kWh)
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_Energy24h::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    float e24 = energyObj.getEnergy24h();
    if (e24 < 0.0f)    e24 = 0.0f;
    if (e24 > 655.35f) e24 = 655.35f;
    uint16_t centi = (uint16_t)roundf(e24 * 100.0f);

    uint8_t *data = new uint8_t[4];
    vscp_encode_uint16(data, VSCP_UNIT_ENERGY_KWH, 0, -2, centi);
    return data;
}

/* ============================================================================
 * VSCP_EnergyTotal – CLASS1.MEASUREMENT, Energy, sensor 1
 * Unit: kWh, exponent 0  (integer kWh, stored as uint32, max ~4 GWh)
 * Payload: 6 bytes
 * ========================================================================== */
uint8_t *VSCP_EnergyTotal::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 6;
    float et = energyObj.getEnergyTotal();
    if (et < 0.0f) et = 0.0f;
    // clamp to uint32 max (≈ 4 295 000 MWh, more than sufficient)
    if (et > 4294967295.0f) et = 4294967295.0f;
    uint32_t kwh = (uint32_t)et;

    uint8_t *data = new uint8_t[6];
    vscp_encode_uint32(data, VSCP_UNIT_ENERGY_KWH, 1, 0, kwh);
    return data;
}
