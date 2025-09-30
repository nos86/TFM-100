#pragma once
#include <stdint.h>

/**
 * @file CircularQueue.h
 * @brief Fixed-size circular (ring) byte queue template.
 *
 * This header declares the template class Queue which implements a simple
 * circular buffer for uint8_t items. The template parameter N is the
 * underlying storage size and one slot is reserved to distinguish full vs
 * empty states (so the usable capacity is N-1).
 *
 * The implementation is provided in the accompanying `CircularQueue.tpp` and
 * is included at the bottom of this header so the template can be instantiated
 * by users. All operations are non-blocking and return boolean success where
 * appropriate.
 */

template <uint8_t N = 100>
class Queue
{
public:
    /**
     * @brief Construct an empty Queue.
     *
     * Initializes internal pointers (first/last) so the queue is empty.
     */
    Queue();

    /**
     * @brief Enqueue a single byte.
     *
     * @param item The byte to enqueue.
     * @return true if the item was added, false if the queue was full.
     */
    bool enqueue(uint8_t item);

    /**
     * @brief Enqueue multiple bytes from a buffer.
     *
     * Attempts to enqueue `length` bytes from `items`. If there is not enough
     * free space to hold all bytes the call fails and no bytes are enqueued.
     *
     * @param items Pointer to the source buffer containing bytes to enqueue.
     * @param length Number of bytes to enqueue.
     * @return true on success (all bytes enqueued), false otherwise.
     */
    bool enqueue(uint8_t *items, uint8_t length);

    /**
     * @brief Dequeue a single byte.
     *
     * @return The dequeued byte. If the queue is empty this returns 0.
     * @note The caller may want to check isEmpty() before calling to
     *       distinguish an actual 0 value from the empty-case return.
     */
    uint8_t dequeue();

    /**
     * @brief Dequeue up to `length` bytes into the provided buffer.
     *
     * If `length` is greater than the number of available bytes, only the
     * available bytes are written and the function returns after copying them.
     *
     * @param items Pointer to the destination buffer to receive bytes.
     * @param length Maximum number of bytes to dequeue.
     */
    void dequeue(uint8_t *items, uint8_t length);

    /**
     * @brief Check whether the queue is empty.
     *
     * @return true if empty, false otherwise.
     */
    bool isEmpty();

    /**
     * @brief Check whether the queue is full.
     *
     * Because one slot is reserved to disambiguate full vs empty, the queue
     * is considered full when only one free slot remains.
     *
     * @return true if full, false otherwise.
     */
    bool isFull();

    /**
     * @brief Current number of bytes stored in the queue.
     *
     * @return A value in range [0, capacity()].
     */
    uint8_t size();

    /**
     * @brief Maximum number of bytes that can be stored (usable capacity).
     *
     * Because one element is reserved to differentiate full and empty states
     * the usable capacity is N-1.
     *
     * @return Usable capacity.
     */
    uint8_t capacity();

private:
    uint8_t items[N]; /**< Underlying storage array. */
    uint8_t last;     /**< Index of the next element to write. */
    uint8_t first;    /**< Index of the next element to read. */
};

#include "CircularQueue.tpp"
