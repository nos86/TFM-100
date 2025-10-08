#pragma once
#include "J1939Manager.h"

/**
 * @file HeartbeatMessage.h
 * @brief Periodic heartbeat message wrapper implementing `IJ1939Message` behavior.
 *
 * This lightweight class exposes a periodic J1939 heartbeat PGN and integrates
 * with the `J1939Manager`'s registration system via `PeriodicMessage`.
 */
class HeartbeatMessage : public PeriodicMessage
{
public:
    /**
     * @brief Construct a heartbeat message with a default 1s interval.
     * @param interval_ms Minimum interval between heartbeat transmissions.
     */
    HeartbeatMessage(uint16_t interval_ms = 1000) : PeriodicMessage(interval_ms) {}

    /** @brief Return the PGN used for heartbeat messages. */
    uint32_t getPGN() override { return 0xFF10; }

    /**
     * @brief Build heartbeat payload into a buffer and return pointer.
     *
     * The caller (manager) will copy payload bytes when `shallPayloadCopied()`
     * returns true.
     */
    uint8_t *buildPayload(uint8_t *len) override;

    /** @brief Heartbeat payloads are copied into the manager's pool. */
    bool shallPayloadCopied() override { return true; }
};