/**
 * @file CircularQueue.tpp
 * @brief Implementation of the template methods for Queue<T>.
 *
 * This file contains the method definitions for the `Queue` template
 * declared in `CircularQueue.h`. It is intended to be included at the end
 * of the header and must not be compiled separately. Keeping the
 * implementation in a ".tpp" file keeps the header concise while allowing
 * the compiler to instantiate templates as needed.
 */

#include <stdint.h>
#include "CircularQueue.h"

template <uint8_t N>
Queue<N>::Queue()
{
    last = 0;
    first = 0;
}

template <uint8_t N>
bool Queue<N>::enqueue(uint8_t item)
{
    if (isFull())
    {
        return false;
    }
    items[last] = item;
    last = (last + 1) % N;
    return true;
}

template <uint8_t N>
bool Queue<N>::enqueue(uint8_t *items, uint8_t length)
{
    if (length > capacity() - size())
    {
        return false;
    }
    for (uint8_t i = 0; i < length; i++)
    {
        enqueue(items[i]);
    }
    return true;
}

template <uint8_t N>
uint8_t Queue<N>::dequeue()
{
    if (isEmpty())
    {
        return 0; // or some error value
    }
    uint8_t item = items[first];
    first = (first + 1) % N;
    return item;
}

template <uint8_t N>
void Queue<N>::dequeue(uint8_t *items, uint8_t length)
{
    if (length > size())
    {
        length = size(); // Adjust length to available items
    }
    for (uint8_t i = 0; i < length; i++)
    {
        items[i] = dequeue();
    }
}

template <uint8_t N>
bool Queue<N>::isEmpty()
{
    return first == last;
}

template <uint8_t N>
bool Queue<N>::isFull()
{
    return (last + 1) % N == first;
}

template <uint8_t N>
uint8_t Queue<N>::size()
{
    if (last >= first)
    {
        return last - first;
    }
    return N - first + last;
}

template <uint8_t N>
uint8_t Queue<N>::capacity()
{
    return N - 1; // One slot is used to differentiate full vs empty
}