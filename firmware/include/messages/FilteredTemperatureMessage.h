#pragma once
#include <J1939Manager.h>

/**
 * @file FilteredTemperatureMessage.h
 * @brief Periodic filtered temperature and flow message.
 *
 * Combines filtered temperature readings and flow data into a single PGN.
 * Intended to be registered with the `J1939Manager` as a `PeriodicMessage`.
 */
class CAN_FilteredTemperatureAndFlow : public PeriodicMessage
{
public:
    /**
     * @brief Construct with optional transmit interval.
     * @param interval_ms Interval between transmissions in milliseconds.
     */
    CAN_FilteredTemperatureAndFlow(uint16_t interval_ms = 1000) : PeriodicMessage(interval_ms) {}

    /** @brief PGN for filtered temperature and flow message. */
    uint32_t getPGN() override { return 0xFF12; }

    /**
     * @brief Build combined payload for temperature and flow.
     *
     * Manager will perform a copy of the returned payload when
     * `shallPayloadCopied()` returns true.
     */
    uint8_t *buildPayload(uint8_t *len) override;

    /** @brief Use COPY mode for payloads. */
    bool shallPayloadCopied() override { return true; }
};