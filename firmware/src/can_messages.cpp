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
 * HeartbeatMessage – VSCP CLASS1.INFORMATION, Type Node Heartbeat (9)
 * Payload (8 bytes, per VSCP spec):
 *   byte 0: user-specified  → node status (NodeStatus_t)
 *   byte 1: Zone            → 0xFF (all zones, per VSCP spec)
 *   byte 2: Sub-zone        → 0xFF (all sub-zones, per VSCP spec)
 *   byte 3: optional        → firmware version
 *   byte 4: optional        → (variant[3:0] << 4) | model[3:0]
 *   byte 5: optional        → reserved
 *   bytes 6-7: optional     → uptime (last 16 bits, big-endian)
 * ========================================================================== */
uint8_t *HeartbeatMessage::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 8;
    uint8_t *data = new uint8_t[8];
    data[0] = (uint8_t)node_status;               // user-specified (byte 0)
    data[1] = 0xFFu;                               // zone: 0xFF = all zones
    data[2] = 0xFFu;                               // sub-zone: 0xFF = all sub-zones
    data[3] = (uint8_t)(FIRMWARE_VERSION & 0xFFu); // optional: firmware version
    data[4] = (uint8_t)(((VARIANT & 0x0Fu) << 4) | (MODEL & 0x0Fu)); // optional: variant|model
    data[5] = 0x00u;                               // reserved
    uint32_t uptime = scheduler.getUptime();
    data[6] = (uint8_t)((uptime >>  8) & 0xFFu);  // optional: uptime 16-bit MSB
    data[7] = (uint8_t)( uptime        & 0xFFu);  // optional: uptime 16-bit LSB
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
 * VSCP_FlowRate – CLASS1.MEASUREMENT, Flow (type 36), sensor 0
 * Unit: Litres per second (VSCP_UNIT_FLOW_LS = 1), exponent -3
 * Converts internal l/h to L/s × 10^3 (milli-L/s):
 *   value = round(flow_lh / 3.6) = round(flow_lh × 10 / 36)
 * Payload: 4 bytes
 * ========================================================================== */
uint8_t *VSCP_FlowRate::buildPayload(uint8_t *len)
{
    if (len == nullptr)
        return nullptr;

    *len = 4;
    uint16_t flow_lh = flowObj.getFlow();
    // l/h ÷ 3600 = l/s; × 1000 = milli-l/s (exponent -3)
    // Integer: round((flow_lh × 10 + 18) / 36)
    uint16_t mls = (uint16_t)(((uint32_t)flow_lh * 10u + 18u) / 36u);

    uint8_t *data = new uint8_t[4];
    vscp_encode_uint16(data, VSCP_UNIT_FLOW_LS, 0, -3, mls);
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
