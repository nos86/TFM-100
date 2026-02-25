#pragma once
#include <J1939Manager.h>

/**
 * @file PowerAndEnergyMessage.h
 * @brief Periodic power and energy message.
 *
 * Combines power and energy data into a single PGN.
 * Intended to be registered with the `J1939Manager` as a `PeriodicMessage`.
 */
class CAN_PowerAndEnergy : public PeriodicMessage
{
public:
    /**
     * @brief Construct with optional transmit interval.
     * @param interval_ms Interval between transmissions in milliseconds.
     */
    CAN_PowerAndEnergy(uint16_t interval_ms = 1000) : PeriodicMessage(interval_ms) {}

    /** @brief PGN for power and energy message. */
    uint32_t getPGN() override { return 0xFF13; }

    /**
     * @brief Build combined payload for power and energy.
     *
     * Manager will perform a copy of the returned payload when
     * `shallPayloadCopied()` returns true.
     */
    uint8_t *buildPayload(uint8_t *len) override;

    /** @brief Use COPY mode for payloads. */
    bool shallPayloadCopied() override { return true; }
};