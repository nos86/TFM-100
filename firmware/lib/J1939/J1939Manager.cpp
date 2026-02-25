/**
 * @file J1939Manager.cpp
 * @brief J1939 transport layer manager implementation: queueing and TP/BAM handling
 * @author Salvo Musumeci
 * @note Implements COPY and LOCKED buffer modes. Must respect tight RAM limits on
 *       ATmega32U4 (avoid dynamic allocations outside small pools).
 */

#include <J1939Manager.h>
#include <stdint.h>

// Inline helper: minimum of two uint8_t values (used for DT payload sizing)
inline uint8_t min(uint8_t a, uint8_t b) { return (a < b) ? a : b; }

// Constructor: initialize internal queues, counters and buffers
J1939Manager::J1939Manager(ICanDriver &driver, const J1939Callbacks &cb)
    : can_(driver), pool_(), sa_(0), cb_(cb)
{
    q_head_ = 0;
    q_tail_ = 0;
    q_count_ = 0;
    sequence_counter = 0;
    last_error_ = J1939TxError::NONE;
    last_tp_tx_ms_ = 0;

    registered_count_ = 0;
    for (uint8_t i = 0; i < J1939Config::MAX_REGISTERED_MESSAGES; i++)
    {
        registered_messages_[i] = nullptr;
    }

    for (int i = 0; i < 8; i++)
    {
        frame_payload_[i] = 0;
    }
}

// Initialize manager with source address (SA)
void J1939Manager::begin(uint8_t sa)
{
    sa_ = sa;
}

// Manage registered messages
// Register a periodic IJ1939Message to be polled in process()
bool J1939Manager::registerMessage(IJ1939Message *message)
{
    if (message == nullptr || registered_count_ >= J1939Config::MAX_REGISTERED_MESSAGES)
    {
        return false;
    }

    for (uint8_t i = 0; i < registered_count_; i++)
    {
        if (registered_messages_[i] == message)
        {
            return false;
        }
    }

    registered_messages_[registered_count_] = message;
    registered_count_++;
    return true;
}

// Unregister a previously registered IJ1939Message. Returns true if removed.
bool J1939Manager::unregisterMessage(IJ1939Message *message)
{
    if (message == nullptr)
    {
        return false;
    }

    for (uint8_t i = 0; i < registered_count_; i++)
    {
        if (registered_messages_[i] == message)
        {
            for (uint8_t j = i; j < registered_count_ - 1; j++)
            {
                registered_messages_[j] = registered_messages_[j + 1];
            }
            registered_messages_[registered_count_ - 1] = nullptr;
            registered_count_--;
            return true;
        }
    }
    return false;
}

uint8_t J1939Manager::getRegisteredCount() const
{
    return registered_count_;
}

// Process the message queue and registered messages
// Main periodic processing: advance current TX, poll registered messages and start next queued descriptor
void J1939Manager::process(uint16_t now_ms)
{
    processCurrent(now_ms);            // Process current message state machine
    processRegisteredMessages(now_ms); // Poll registered IJ1939Message instances
    if (q_count_ > 0 && sequence_counter == 0)
        startNext(now_ms);
}

// Return if there are messages in queue or being processed
bool J1939Manager::busy() const
{
    return q_count_ > 0;
}

J1939TxError J1939Manager::lastError() const
{
    return last_error_;
}
// COPY mode: copy payload bytes into the internal circular pool and enqueue a descriptor.
// The function checks pool free space and queue capacity before enqueuing. On failure
// it rolls back the descriptor insertion to keep the queue consistent.
bool J1939Manager::sendCopy(uint32_t pgn, const uint8_t *data, uint8_t len, uint8_t da, uint8_t prio)
{
    J1939Descriptor desc = {
        .pgn = pgn,
        .len = len,
        .src = nullptr, // COPY mode
        .prio = prio};

    // Check for space in pool and in message queue
    uint8_t pool_capacity = pool_.capacity();
    uint8_t pool_used = pool_.size();
    uint8_t pool_free = (pool_capacity >= pool_used) ? (pool_capacity - pool_used) : 0;

    if ((len > pool_free) || (q_count_ >= J1939Config::MESSAGE_QUEUE))
    {
        last_error_ = J1939TxError::POOL_NO_SPACE;
        return false;
    }

    // Reserve a descriptor slot first. If this fails the queue is full.
    if (!enqueueDescriptor(desc))
        return false;

    // Copy bytes into pool; if copying fails rollback the descriptor enqueue
    if (!pool_.enqueue((uint8_t *)data, len))
    {
        // Rollback queue enqueue
        q_tail_ = (q_tail_ == 0) ? J1939Config::MESSAGE_QUEUE - 1 : q_tail_ - 1;
        q_count_--;
        last_error_ = J1939TxError::POOL_NO_SPACE;
        return false;
    }

    return true;
}

bool J1939Manager::sendLocked(uint32_t pgn, ILockableBuffer &src, uint8_t da, uint8_t prio)
{
    J1939Descriptor desc = {
        .pgn = pgn,
        .len = src.size(),
        .src = &src, // LOCKED mode
        .prio = prio};
    // LOCKED mode: descriptor is queued; the buffer will be locked during multi-frame sessions
    return enqueueDescriptor(desc);
}

bool J1939Manager::enqueueDescriptor(J1939Descriptor &desc)
{
    // Check if queue is full
    if (q_count_ >= J1939Config::MESSAGE_QUEUE)
    {
        last_error_ = J1939TxError::QUEUE_FULL;
        return false;
    }
    // Copy descriptor into queue at q_tail_. This is a shallow copy: when
    // desc.src != nullptr the pointer is stored and the buffer must remain
    // valid until releaseCurrent() unlocks it.
    queue_[q_tail_].pgn = desc.pgn;
    queue_[q_tail_].len = desc.len;
    queue_[q_tail_].src = desc.src;
    queue_[q_tail_].prio = desc.prio;
    q_tail_ = (q_tail_ + 1) % J1939Config::MESSAGE_QUEUE;
    q_count_++;
    last_error_ = J1939TxError::NONE;
    return true;
}

void J1939Manager::startNext(uint16_t now_ms)
{
    // If there's no queued message, nothing to start
    if (q_count_ == 0)
        return;

    // Reset sequence counter to indicate fresh transmission for head descriptor
    sequence_counter = 0;

    // For single-frame messages, we can optionally trigger on_start immediately
    J1939Descriptor &desc = queue_[q_head_];
    // If LOCKED mode is used, lock the source buffer now so that the subsequent
    // call to processCurrent() can safely read from it without duplicating lock()
    if (desc.src)
    {
        desc.src->lock();
    }
    if (cb_.on_start)
        cb_.on_start(desc);
}

void J1939Manager::processCurrent(uint16_t now_ms)
{
    // If queue is empty, nothing to process
    if (q_count_ == 0)
        return;

    // Reference to the active descriptor at queue head
    J1939Descriptor &desc = queue_[q_head_];
    uint32_t can_id;
    uint8_t len_frame;
    bool transmission_success = false;
    bool is_final_frame = false;

    // === SINGLE FRAME PROCESSING ===
    if (desc.len <= 8)
    {
        // Build CAN id and copy payload into scratch buffer
        can_id = buildCanId(desc.prio, desc.pgn, sa_);
        len_frame = desc.len;
        // fill entire scratch from offset 0
        fillFramePayload(desc, 0, 0, len_frame);
        transmission_success = can_.sendExtendedFrame(can_id, frame_payload_, len_frame);
        is_final_frame = true; // Single frames finish in one shot
        sequence_counter = 0;  // Reset sequence counter
    }
    else
    {
        // === MULTI-FRAME (TP/BAM) PROCESSING ===
        if (sequence_counter == 0)
        {
            // Send BAM Connection Management (CM) frame to announce a large transfer
            can_id = buildCanId(6, 0xECFF, sa_); // BAM uses PGN 0xECFF (Broadcast) and priority 6
            len_frame = 8;

            // The source buffer (if any) was locked by startNext() prior to
            // beginning the multi-frame transfer.

            // Fill BAM CM fields: control byte, total size, packets count and PGN
            frame_payload_[0] = 0x20;                    // BAM control byte
            frame_payload_[1] = desc.len & 0xFF;         // Total message size LSB
            frame_payload_[2] = (desc.len >> 8) & 0xFF;  // Total message size MSB
            frame_payload_[3] = (desc.len + 6) / 7;      // Number of DT packets (7 bytes/data each)
            frame_payload_[4] = 0xFF;                    // Reserved
            frame_payload_[5] = desc.pgn & 0xFF;         // PGN LSB
            frame_payload_[6] = (desc.pgn >> 8) & 0xFF;  // PGN middle byte
            frame_payload_[7] = (desc.pgn >> 16) & 0xFF; // PGN MSB

            transmission_success = can_.sendExtendedFrame(can_id, frame_payload_, len_frame);
            if (transmission_success)
            {
                // Start sending DT frames from sequence 1
                sequence_counter = 1;
                last_tp_tx_ms_ = now_ms; // Pace DT frames relative to BAM
            }
            is_final_frame = false;
        }
        else if ((uint16_t)(now_ms - last_tp_tx_ms_) < J1939Config::GAP_DT_MS)
        {
            return; // pacing
        }
        else
        {
            // Prepare a Data Transfer (DT) frame: 1 byte sequence + up to 7 data bytes
            can_id = buildCanId(6, 0xEBFF, sa_);  // DT uses PGN 0xEBFF (Broadcast) and priority 6
            frame_payload_[0] = sequence_counter; // Sequence number (1-based)

            // Compute offset and how many bytes to send this DT
            uint16_t data_offset = (sequence_counter - 1) * 7;
            uint8_t bytes_to_send = min(desc.len - data_offset, 7);
            // Fill available data bytes starting at index 1
            fillFramePayload(desc, 1, data_offset, bytes_to_send);
            is_final_frame = (data_offset + bytes_to_send >= desc.len);

            // Pad remaining bytes to 8-byte CAN frame with 0xFF for interoperability
            for (uint8_t i = 1 + bytes_to_send; i < 8; i++)
                frame_payload_[i] = 0xFF;
            len_frame = 8;

            // Send DT (always as full 8-byte frame)
            transmission_success = can_.sendExtendedFrame(can_id, frame_payload_, len_frame);
            if (transmission_success)
            {
                last_tp_tx_ms_ = now_ms;
                if (is_final_frame)
                {
                    // Completed transfer: sequence_counter reset, cleanup in releaseCurrent
                    sequence_counter = 0;
                }
                else
                {
                    // Move to next sequence and record time for pacing
                    sequence_counter++;
                    if (cb_.on_progress)
                        cb_.on_progress(desc);
                }
            }
        }
    }

    // Handle finalization or failure of the attempted transmission
    if (transmission_success)
    {
        last_error_ = J1939TxError::NONE;
        if (is_final_frame)
        {
            // All bytes consumed for final frame - release descriptor and trigger on_done
            releaseCurrent(true);
        }
    }
    else
    {
        last_error_ = J1939TxError::CAN_SEND_FAIL;
        // On failure release/cleanup current descriptor (releaseCurrent will deduce consumed bytes)
        releaseCurrent(false);
    }
}

// Fill scratch_ buffer from descriptor source or pool.
void J1939Manager::fillFramePayload(J1939Descriptor &desc, uint8_t destIndex, uint16_t data_offset, uint8_t count)
{
    if (desc.src)
    {
        const uint8_t *srcdata = desc.src->data();
        for (uint8_t i = 0; i < count; i++)
            frame_payload_[destIndex + i] = srcdata[data_offset + i];
    }
    else
    {
        for (uint8_t i = 0; i < count; i++)
            frame_payload_[destIndex + i] = pool_.dequeue();
    }
}

void J1939Manager::releaseCurrent(bool success)
{
    // If there's no current message being processed, nothing to release
    if (q_count_ == 0)
        return;

    J1939Descriptor &desc = queue_[q_head_];

    // If LOCKED mode was used, unlock the buffer now. The caller may have
    // already unlocked; implementations of unlock() should be idempotent.
    if (desc.src)
    {
        desc.src->unlock();
    }
    else
    {
        // COPY mode: figure how many bytes were already consumed from pool.
        // For single-frame messages, all bytes were consumed. For multi-frame
        // messages, (sequence_counter - 1) DT frames were sent (each up to 7 bytes)
        // unless sequence_counter==0 which indicates completion.
        // If the transfer completed normally the pool bytes for
        // this descriptor are already fully consumed; otherwise remove the
        // remaining bytes (if any) to keep pool state consistent.

        // For single-frame messages, all bytes were consumed
        // For multi-frame messages, (sequence_counter - 1) DT frames were sent (each up to 7 bytes)

        if ((desc.len > 8) && !success)
        {
            uint16_t consumed = (sequence_counter == 0 ? 0 : (sequence_counter - 1) * 7);
            uint16_t remaining_bytes = desc.len - consumed;
            for (uint16_t i = 0; i < remaining_bytes; i++)
                pool_.dequeue();
        }
    }

    // Advance the queue to remove the current descriptor
    q_head_ = (q_head_ + 1) % J1939Config::MESSAGE_QUEUE;
    q_count_--;
    sequence_counter = 0;

    // Call appropriate callback after cleanup
    if (success)
    {
        if (cb_.on_done)
            cb_.on_done(desc);
    }
    else
    {
        if (cb_.on_error)
            cb_.on_error(desc);
    }
}

void J1939Manager::processRegisteredMessages(uint16_t now_ms)
{
    for (uint8_t i = 0; i < registered_count_; i++)
    {
        IJ1939Message *msg = registered_messages_[i];
        if (msg != nullptr && msg->shouldSend(now_ms))
        {
            uint8_t *data;
            uint8_t len;
            data = msg->buildPayload(&len);
            // If payload was provided, enqueue a copy.
            // Ownership/lifetime of `data` stays with the message implementation:
            // this manager never deletes the returned pointer.
            if (data != nullptr && len > 0)
            {
                bool ok = sendCopy(msg->getPGN(), data, len, msg->getDestination(), msg->getPriority());
                if (!ok)
                    last_error_ = J1939TxError::POOL_NO_SPACE;
            }
        }
    }
}

uint32_t J1939Manager::buildCanId(uint8_t prio, uint32_t pgn, uint8_t sa)
{
    uint32_t can_id = 0;
    // J1939 29-bit identifier layout: priority (3b) | reserved/data (1b) | PGN (18b) | SA (8b)
    // We place priority in bits 26..28, PGN in bits 8..31 (shifted) and SA in low byte.
    can_id |= ((uint32_t)(prio & 0x07) << 26);
    can_id |= (pgn << 8);
    can_id |= (sa & 0xFF);
    return can_id;
}
