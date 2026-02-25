#pragma once
#include "J1939Manager.h"
#include <vscp.h>

/**
 * @file HeartbeatMessage.h
 * @brief Periodic VSCP heartbeat message (CLASS1.INFORMATION, Type Heartbeat).
 *
 * Sends a VSCP Level 1 heartbeat event every @p interval_ms milliseconds.
 * The CAN identifier is built by J1939Manager using the VSCP_PGN helper so
 * that it conforms to the VSCP Level 1 29-bit extended frame format.
 */
class HeartbeatMessage : public PeriodicMessage
{
public:
    /**
     * @brief Construct a heartbeat message with a configurable interval.
     * @param interval_ms Minimum interval between heartbeat transmissions.
     */
    HeartbeatMessage(uint16_t interval_ms = 1000) : PeriodicMessage(interval_ms) {}

    /** @brief VSCP CLASS1.INFORMATION / Type Heartbeat PGN. */
    uint32_t getPGN() override
    {
        return VSCP_PGN(VSCP_CLASS1_INFORMATION, VSCP_TYPE_INFORMATION_HEARTBEAT);
    }

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