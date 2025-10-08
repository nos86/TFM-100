#pragma once

#include <stdint.h>

/**
 * @file IJ1939Message.h
 * @brief Interface and helper base for J1939 messages.
 *
 * This header defines the abstract interface `IJ1939Message` which all
 * J1939 message classes implement. Implementations provide a PGN, prepare
 * payload data and decide when the message should be sent.
 */

// IJ1939Message Interface
class IJ1939Message
{
public:
    virtual ~IJ1939Message() = default;

    /**
     * @brief Return the PGN (Parameter Group Number) identifying this message.
     *
     * @return 29-bit J1939 PGN (stored in a 32-bit value)
     */
    virtual uint32_t getPGN() = 0;

    /**
     * @brief Build or return the message payload.
     *
     * The function should return a pointer to a buffer containing the payload
     * and write the payload length to `len`. The ownership semantics are
     * controlled by `shallPayloadCopied()`; if that returns true the caller
     * will make an internal copy.
     *
     * @param len Output pointer where the length of the payload is written.
     * @return Pointer to the payload buffer.
     */
    virtual uint8_t *buildPayload(uint8_t *len) = 0;

    /**
     * @brief Query whether this message should be sent at the provided time.
     *
     * Implementations typically use internal timers or state to decide when
     * to transmit. The `now_ms` parameter is the current time in milliseconds
     * from a monotonic clock (platform-specific) used for scheduling.
     *
     * @param now_ms Current time in milliseconds.
     * @return true if the message should be sent now, false otherwise.
     */
    virtual bool shouldSend(uint16_t now_ms) = 0;

    /**
     * @brief Message priority (0 = highest, 7 = lowest).
     *
     * Default implementation returns 6 which is appropriate for many
     * informational messages. Override to change priority.
     *
     * @return Priority value in range [0..7].
     */
    virtual uint8_t getPriority() { return 6; }

    /**
     * @brief Destination address for the message (0xFF means broadcast).
     *
     * Default implementation broadcasts the message. Override to provide a
     * specific destination.
     *
     * @return Destination address (0x00..0xFF)
     */
    virtual uint8_t getDestination() { return 0xFF; }

    /**
     * @brief Whether the payload should be copied by the caller.
     *
     * If true the caller will copy payload bytes returned by
     * `buildPayload()` into its own buffer. If false the pointer returned is
     * assumed valid until the message is sent and the caller may use it
     * directly without copying.
     *
     * @return true if payload should be copied, false if the pointer is
     *         guaranteed valid for immediate use.
     */
    virtual bool shallPayloadCopied() { return true; }
};

// Base class for periodic messages
class PeriodicMessage : public IJ1939Message
{
private:
    uint16_t interval_ms_;  /**< Period between sends in milliseconds. */
    uint16_t last_sent_ms_; /**< Monotonic timestamp of last send. */

public:
    /**
     * @brief Create a periodic message helper.
     *
     * @param interval_ms The minimum interval between sends in ms.
     */
    PeriodicMessage(uint16_t interval_ms) : interval_ms_(interval_ms), last_sent_ms_(0) {}

    /**
     * @brief Decide whether enough time has passed to send again.
     *
     * This updates `last_sent_ms_` when the interval has elapsed.
     *
     * @param now_ms Current time in milliseconds.
     * @return true when the period has elapsed and the message should be sent.
     */
    bool shouldSend(uint16_t now_ms) override
    {
        if ((now_ms - last_sent_ms_) >= interval_ms_)
        {
            last_sent_ms_ = now_ms;
            return true;
        }
        return false;
    }
};