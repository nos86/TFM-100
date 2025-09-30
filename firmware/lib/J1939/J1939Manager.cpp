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
        scratch_[i] = 0;
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
    if (desc.len <= 8)
    {
        // Single-frame messages will be attempted immediately by processCurrent().
        if (cb_.on_start)
            cb_.on_start(desc);
    }
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

        if (desc.src)
        {
            // LOCKED mode: read directly from the provided buffer (must lock first)
            // Note: lock() may be a no-op for some implementations but is required by interface
            desc.src->lock();
            for (uint8_t i = 0; i < desc.len; i++)
                scratch_[i] = desc.src->data()[i];
        }
        else
        {
            // COPY mode: payload bytes were previously copied into the pool in FIFO order
            for (uint8_t i = 0; i < desc.len; i++)
                scratch_[i] = pool_.dequeue();
        }

        // Send single-frame payload
        transmission_success = can_.sendExtendedFrame(can_id, scratch_, len_frame);
        is_final_frame = true; // Single frames finish in one shot
        sequence_counter = 0;  // Reset sequence counter
    }
    else
    {
        // === MULTI-FRAME (TP/BAM) PROCESSING ===
        if (sequence_counter == 0)
        {
            // Send BAM Connection Management (CM) frame to announce a large transfer
            can_id = buildCanId(6, 0xEC00, sa_); // BAM uses PGN 0xEC00 and priority 6
            len_frame = 8;

            // Lock the source buffer in LOCKED mode for the whole transfer
            if (desc.src)
                desc.src->lock();

            // Fill BAM CM fields: control byte, total size, packets count and PGN
            scratch_[0] = 0x20;                    // BAM control byte
            scratch_[1] = desc.len & 0xFF;         // Total message size LSB
            scratch_[2] = (desc.len >> 8) & 0xFF;  // Total message size MSB
            scratch_[3] = (desc.len + 6) / 7;      // Number of DT packets (7 bytes/data each)
            scratch_[4] = 0xFF;                    // Reserved
            scratch_[5] = desc.pgn & 0xFF;         // PGN LSB
            scratch_[6] = (desc.pgn >> 8) & 0xFF;  // PGN middle byte
            scratch_[7] = (desc.pgn >> 16) & 0xFF; // PGN MSB

            transmission_success = can_.sendExtendedFrame(can_id, scratch_, len_frame);

            if (transmission_success)
            {
                // Start sending DT frames from sequence 1
                sequence_counter = 1;
                last_tp_tx_ms_ = now_ms; // Pace DT frames relative to BAM
                if (cb_.on_start)
                    cb_.on_start(desc);
            }
            is_final_frame = false;
        }
        else
        {
            // Prepare a Data Transfer (DT) frame: 1 byte sequence + up to 7 data bytes
            can_id = buildCanId(6, 0xEB00, sa_); // DT uses PGN 0xEB00 and priority 6
            scratch_[0] = sequence_counter;      // Sequence number (1-based)

            // Compute offset and how many bytes to send this DT
            uint16_t data_offset = (sequence_counter - 1) * 7;
            uint8_t bytes_to_send = min(desc.len - data_offset, 7);
            len_frame = bytes_to_send + 1; // +1 for sequence number

            if (desc.src)
            {
                // LOCKED mode: copy directly from the locked buffer
                for (uint8_t i = 0; i < bytes_to_send; i++)
                    scratch_[i + 1] = desc.src->data()[data_offset + i];
            }
            else
            {
                // COPY mode: dequeue bytes previously copied into the pool
                for (uint8_t i = 0; i < bytes_to_send; i++)
                    scratch_[i + 1] = pool_.dequeue();
            }

            // Check for final frame
            is_final_frame = (data_offset + bytes_to_send >= desc.len);

            // Enforce inter-frame gap between DT frames
            if ((uint16_t)(now_ms - last_tp_tx_ms_) < J1939Config::GAP_DT_MS)
            {
                // Not enough time has passed; try again later without consuming sequence_counter
                return;
            }

            // Send DT
            transmission_success = can_.sendExtendedFrame(can_id, scratch_, len_frame);

            if (transmission_success)
            {
                if (is_final_frame)
                {
                    // Completed transfer: sequence_counter reset, cleanup in releaseCurrent
                    sequence_counter = 0;
                    last_tp_tx_ms_ = now_ms;
                }
                else
                {
                    // Move to next sequence and record time for pacing
                    sequence_counter++;
                    last_tp_tx_ms_ = now_ms;
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
        uint16_t consumed = 0;
        if (desc.len <= 8)
        {
            consumed = desc.len;
        }
        else
        {
            if (sequence_counter == 0)
                consumed = desc.len; // final frame was sent and consumed remaining bytes
            else
                consumed = (sequence_counter - 1) * 7;
            if (consumed > desc.len)
                consumed = desc.len;
        }

        // Remove any remaining bytes for this descriptor from the pool to keep pool state consistent
        uint16_t remaining_bytes = (desc.len > consumed) ? (desc.len - consumed) : 0;
        for (uint16_t i = 0; i < remaining_bytes; i++)
            pool_.dequeue();
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
            // If payload was provided, enqueue a copy. Free the returned buffer to avoid leaks.
            if (data != nullptr && len > 0)
            {
                bool ok = sendCopy(msg->getPGN(), data, len, msg->getDestination(), msg->getPriority());
                // Many message implementations allocate with new[]. The manager doesn't take ownership
                // of the returned pointer beyond copying it into the internal pool, so free it here.
                delete[] data;
                if (!ok)
                    last_error_ = J1939TxError::POOL_NO_SPACE;
            }
        }
    }
}

uint32_t J1939Manager::buildCanId(uint8_t prio, uint32_t pgn, uint8_t sa)
{
    uint32_t can_id = 0;
    // J1939 29-bit identifier layout: priority (3b) | reserved/data (??) | PGN (18b) | SA (8b)
    // We place priority in bits 26..28, PGN in bits 8..31 (shifted) and SA in low byte.
    can_id |= ((uint32_t)(prio & 0x07) << 26);
    can_id |= (pgn << 8);
    can_id |= (sa & 0xFF);
    return can_id;
}
