#pragma once

#include <stdint.h>

/**
 * @file ILockableBuffer.h
 * @brief Abstract interface for a lockable byte buffer used by J1939 code.
 *
 * Implementations provide a byte buffer that callers can lock for exclusive
 * access across potentially concurrent contexts (interrupt handlers, tasks,
 * etc.). The interface is intentionally small: callers lock the buffer,
 * access `data()` and `size()` and then call `unlock()` when finished.
 *
 * Note: this interface does not prescribe the locking mechanism (spinlock,
 * mutex, critical-section), nor does it imply dynamic allocation; concrete
 * implementations must document their concurrency model and ownership rules.
 */
class ILockableBuffer
{
public:
    virtual ~ILockableBuffer() = default;

    /**
     * @brief Acquire exclusive access to the buffer.
     *
     * This call blocks or waits according to the concrete implementation's
     * semantics. After `lock()` returns, the caller must assume it has sole
     * access to the buffer until `unlock()` is called.
     */
    virtual void lock() = 0;

    /**
     * @brief Release exclusive access previously acquired with `lock()`.
     *
     * The caller must not access `data()` after calling `unlock()` unless
     * it locks again.
     */
    virtual void unlock() = 0;

    /**
     * @brief Obtain a pointer to the buffer's data storage.
     *
     * Callers should hold the lock while accessing the returned pointer. The
     * buffer pointer may be invalidated by the implementation if unlock is
     * called or if the buffer is resized by the implementation.
     *
     * @return Pointer to the first byte of the buffer.
     */
    virtual uint8_t *data() = 0;

    /**
     * @brief Number of bytes available in the buffer.
     *
     * @return The current buffer size in bytes. If the concrete
     * implementation supports dynamic resizing, document whether this value
     * may change while the lock is held.
     */
    virtual uint8_t size() const = 0;
};