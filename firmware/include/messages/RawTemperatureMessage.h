 #pragma once
#include <J1939Manager.h>

/**
 * @file RawTemperatureMessage.h
 * @brief Periodic raw temperature message for J1939 transmission.
 *
 * This message wraps raw sensor data into a J1939 PGN and is intended to be
 * registered with `J1939Manager` as a `PeriodicMessage`.
 */
class CAN_Temperature : public PeriodicMessage
{
public:
    /**
     * @brief Construct with optional sampling interval.
     * @param interval_ms Sampling interval in milliseconds (default 1000ms).
     */
    CAN_Temperature(uint16_t interval_ms = 1000) : PeriodicMessage(interval_ms) {}

    /** @brief PGN for raw temperature message. */
    uint32_t getPGN() override { return 0xFF11; }

    /**
     * @brief Build temperature payload into the provided buffer pointer.
     *
     * The manager will copy the payload when `shallPayloadCopied()` returns
     * true.
     */
    uint8_t *buildPayload(uint8_t *len) override;

    /** @brief Use COPY mode: manager will copy the payload into its pool. */
    bool shallPayloadCopied() override { return true; }
};