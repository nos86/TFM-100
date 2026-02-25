#pragma once

/**
 * @file vscp_messages.h
 * @brief VSCP Level 1 periodic CAN message classes for the TFM-100.
 *
 * Each class below implements IJ1939Message (via PeriodicMessage) and
 * produces a VSCP Level 1 CAN frame when registered with J1939Manager.
 * The getPGN() return value encodes the VSCP class and type using the
 * VSCP_PGN() macro so that J1939Manager::buildCanId() generates a valid
 * 29-bit VSCP CAN identifier (priority | class | type | source-node).
 *
 * Data payload follows the VSCP CLASS1.MEASUREMENT normalised-integer
 * encoding: [coding_byte][exponent][value bytes, MSB first].
 *
 * @note buildPayload() allocates the returned buffer with new[].
 *       J1939Manager::processRegisteredMessages() always copies then frees it.
 */

#include <J1939Manager.h>
#include <vscp.h>

/* --------------------------------------------------------------------------
 * Supply temperature – CLASS1.MEASUREMENT, Type Temperature, sensor 0
 * Unit: Celsius, scaling ×10^-2  (centi-degrees stored as int16)
 * Payload: 4 bytes
 * -------------------------------------------------------------------------- */
class VSCP_SupplyTemperature : public PeriodicMessage
{
public:
    explicit VSCP_SupplyTemperature(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_TEMPERATURE);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};

/* --------------------------------------------------------------------------
 * Return temperature – CLASS1.MEASUREMENT, Type Temperature, sensor 1
 * Unit: Celsius, scaling ×10^-2  (centi-degrees stored as int16)
 * Payload: 4 bytes
 * -------------------------------------------------------------------------- */
class VSCP_ReturnTemperature : public PeriodicMessage
{
public:
    explicit VSCP_ReturnTemperature(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_TEMPERATURE);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};

/* --------------------------------------------------------------------------
 * Volume flow rate – CLASS1.MEASUREMENT, Type Volume Flow, sensor 0
 * Unit: litres per hour (l/h), no scaling (exponent = 0)
 * Payload: 4 bytes
 * -------------------------------------------------------------------------- */
class VSCP_FlowRate : public PeriodicMessage
{
public:
    explicit VSCP_FlowRate(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_VOLUME_FLOW);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};

/* --------------------------------------------------------------------------
 * Thermal power – CLASS1.MEASUREMENT, Type Power, sensor 0
 * Unit: Watts, no scaling (kW × 1000 → W, stored as uint16)
 * Payload: 4 bytes
 * -------------------------------------------------------------------------- */
class VSCP_Power : public PeriodicMessage
{
public:
    explicit VSCP_Power(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_POWER);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};

/* --------------------------------------------------------------------------
 * Energy last 24 h – CLASS1.MEASUREMENT, Type Energy, sensor 0
 * Unit: kWh, scaling ×10^-2  (centi-kWh stored as uint16)
 * Payload: 4 bytes
 * -------------------------------------------------------------------------- */
class VSCP_Energy24h : public PeriodicMessage
{
public:
    explicit VSCP_Energy24h(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_ENERGY);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};

/* --------------------------------------------------------------------------
 * Total accumulated energy – CLASS1.MEASUREMENT, Type Energy, sensor 1
 * Unit: kWh, no scaling (integer kWh stored as uint32)
 * Payload: 6 bytes
 * -------------------------------------------------------------------------- */
class VSCP_EnergyTotal : public PeriodicMessage
{
public:
    explicit VSCP_EnergyTotal(uint16_t interval_ms = 1000)
        : PeriodicMessage(interval_ms) {}

    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_ENERGY);
    }

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }
};
