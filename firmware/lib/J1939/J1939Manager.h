#pragma once
#include <stdint.h>
#include "CircularQueue.h"
#include "ILockableBuffer.h"
#include "IJ1939Message.h"

/**
 * @file J1939Manager.h
 * @brief High-level manager for sending J1939 messages (single-frame and TP/BAM).
 *
 * The J1939Manager implements a small transmission queue, a circular payload
 * pool for COPY-mode payloads and support for LOCKED-mode payloads using an
 * `ILockableBuffer` to avoid copying large data. Registered periodic message
 * objects (implementing `IJ1939Message`) can be polled via `process()` to
 * produce outgoing messages.
 */

// ======================
// CAN Driver Interface
// ======================
/**
 * @brief Minimal CAN driver interface required by J1939Manager.
 *
 * Implement this interface with your CAN controller driver. Only
 * `sendExtendedFrame()` is required; it should send a 29-bit ID frame with
 * the provided payload and return true on success.
 */
struct ICanDriver
{
  virtual bool sendExtendedFrame(uint32_t can_id, const uint8_t *data, uint8_t len) = 0;
  virtual ~ICanDriver() {}
};

// ======================
// TX Errors
// ======================
/**
 * @brief Errors returned/recorded by the manager during transmission.
 */
enum class J1939TxError : uint8_t
{
  NONE = 0,      /**< No error */
  BUSY,          /**< Manager is busy */
  POOL_NO_SPACE, /**< Not enough room in the payload pool */
  QUEUE_FULL,    /**< Descriptor queue is full */
  LOCK_FAIL,     /**< Failed to lock ILockableBuffer */
  CAN_SEND_FAIL, /**< Underlying CAN driver failed to send */
  TIMEOUT,       /**< Transport session timed out */
};

// ======================
// Message states
// ======================
/**
 * @brief Internal state machine states for message processing.
 */
enum class J1939MsgState : uint8_t
{
  FREE = 0,
  PENDING,
  SENDING_SF,
  BAM_CM,
  BAM_DT,
  DONE,
  ERROR
};

// ======================
// Queue descriptor
// ======================
/**
 * @brief Descriptor for a queued outgoing J1939 message.
 *
 * `src` is non-null when LOCKED mode is used (no copy into pool). When
 * `src == nullptr` the manager expects the payload bytes to be present in
 * the internal pool in FIFO order.
 */
struct J1939Descriptor
{
  uint32_t pgn;         /**< Application PGN */
  uint16_t len;         /**< Payload length */
  ILockableBuffer *src; /**< Locked source (LOCKED mode) or nullptr for COPY mode */
  uint8_t prio;         /**< Priority (0..7) */
};

// ======================
// Optional callbacks
// ======================
/**
 * @brief Optional callbacks invoked by the manager to report progress.
 *
 * The callbacks receive the descriptor being processed. They are optional
 * and may be nullptr in the callback structure.
 */
struct J1939Callbacks
{
  void (*on_start)(const J1939Descriptor &) = nullptr;    /**< Called when a transfer starts */
  void (*on_progress)(const J1939Descriptor &) = nullptr; /**< Called during multi-frame transfers */
  void (*on_done)(const J1939Descriptor &) = nullptr;     /**< Called when transfer completes successfully */
  void (*on_error)(const J1939Descriptor &) = nullptr;    /**< Called when transfer fails */
};

// ======================
// Verify definition of configuration macros
// ======================

// In case of missing definition, fallback values are used and warning is shown at build time
#ifndef J1939_MESSAGE_QUEUE
#define J1939_MESSAGE_QUEUE 8
#warning "J1939_MESSAGE_QUEUE is not defined by project, using default 8"
#endif

#ifndef J1939_MAX_REGISTERED_MESSAGES
#define J1939_MAX_REGISTERED_MESSAGES 10
#warning "J1939_MAX_REGISTERED_MESSAGES is not defined by project, using default 10"
#endif

#ifndef J1939_PAYLOAD_POOL_SIZE
#define J1939_PAYLOAD_POOL_SIZE 200
#warning "J1939_PAYLOAD_POOL_SIZE is not defined by project, using default 200 bytes"
#endif

// Static asserts to validate configuration values
static_assert(J1939_MESSAGE_QUEUE >= 1 && J1939_MESSAGE_QUEUE <= 255, "J1939_MESSAGE_QUEUE must be in range 1..255");
static_assert(J1939_MAX_REGISTERED_MESSAGES >= 1 && J1939_MAX_REGISTERED_MESSAGES <= 255, "J1939_MAX_REGISTERED_MESSAGES must be in range 1..255");
static_assert(J1939_PAYLOAD_POOL_SIZE >= 1 && J1939_PAYLOAD_POOL_SIZE <= 255, "J1939_PAYLOAD_POOL_SIZE must be in range 1..255");

/**
 * @brief Compile-time configuration values for the manager.
 */
struct J1939Config
{

  static constexpr uint8_t MESSAGE_QUEUE = J1939_MESSAGE_QUEUE;                     // maximum number of messages in queue
  static constexpr uint8_t MAX_REGISTERED_MESSAGES = J1939_MAX_REGISTERED_MESSAGES; // maximum number of registered messages
  static constexpr uint8_t POOL_SIZE = J1939_PAYLOAD_POOL_SIZE;                     // size of circular pool (in bytes)
  static constexpr uint16_t GAP_DT_MS = 35;                                         // pacing between TP.DT
  static constexpr uint16_t TIMEOUT_TOTAL_MS = 5000;                                // session timeout
};

/**
 * @brief Main manager class for outgoing J1939 messages.
 *
 * Responsibilities:
 * - Maintain a queue of outgoing message descriptors
 * - Store payloads in an internal circular pool when COPY mode is used
 * - Support LOCKED mode to avoid copying large buffers
 * - Handle single-frame and multi-frame (BAM/DT) transmissions with
 *   pacing and callbacks.
 */
class J1939Manager
{
public:
  /**
   * @brief Construct a manager with a CAN driver and optional callbacks.
   *
   * @param driver Reference to an implementation of `ICanDriver` used for sending frames.
   * @param cb Optional callbacks for reporting progress/events.
   */
  J1939Manager(ICanDriver &driver, const J1939Callbacks &cb = {}); // TODO: check if callbacks are useful

  /**
   * @brief Initialize the manager with a source address.
   *
   * @param sa Source address (network node address).
   */
  void begin(uint8_t sa);

  /**
   * @brief Main periodic processing function.
   *
   * Call from your main loop or scheduler with a millisecond tick to allow
   * the manager to process queued messages and registered periodic messages.
   *
   * @param now_ms Current monotonic time in milliseconds.
   */
  void process(uint16_t now_ms);

  // Manage registered messages
  /**
   * @brief Register a periodic message object for automatic polling.
   *
   * The manager will call `process()` on registered `IJ1939Message` objects
   * from `process()` to allow them to produce outgoing descriptors. Use this
   * API to register objects that produce periodic or event-driven messages.
   *
   * Ownership: the caller retains ownership of the `message` object. The
   * manager stores the pointer and will not delete it. The caller must ensure
   * the object remains valid while registered (unregister before destruction).
   *
   * Thread-safety / reentrancy: this function is not interrupt-safe. Call it
   * from the main context during initialization or ensure proper locking when
   * used from multiple contexts.
   *
   * @param message Pointer to an object implementing `IJ1939Message`.
   * @return true if registration succeeded; false if the registration list is full
   *         or the same message is already registered.
   */
  bool registerMessage(IJ1939Message *message);

  /**
   * @brief Unregister a previously registered message object.
   *
   * After successful unregistration the manager will no longer call
   * `process()` on the message. It is safe to delete the object after
   * unregisterMessage() returns true.
   *
   * @param message Pointer previously registered with `registerMessage()`.
   * @return true if the message was found and removed; false otherwise.
   */
  bool unregisterMessage(IJ1939Message *message);

  /**
   * @brief Return the number of currently registered message objects.
   *
   * @return Number of registered `IJ1939Message` pointers (0..J1939Config::MAX_REGISTERED_MESSAGES).
   */
  uint8_t getRegisteredCount() const;

  // Global state
  bool busy() const;
  J1939TxError lastError() const;

  /**
   * @brief Enqueue a payload in COPY mode.
   *
   * The provided payload is copied into the internal circular pool. If there
   * is not enough free space in the pool or the descriptor queue is full the
   * call fails.
   *
   * @param pgn The PGN for the message.
   * @param data Pointer to the payload to copy.
   * @param len Length in bytes of the payload.
   * @param da Destination address (default broadcast 0xFF).
   * @param prio Priority (0=highest, 7=lowest, default 6).
   * @return true if enqueued successfully.
   */
  bool sendCopy(uint32_t pgn, const uint8_t *data, uint8_t len, uint8_t da = 0xFF, uint8_t prio = 6);

  /**
   * @brief Enqueue a payload in LOCKED mode using an `ILockableBuffer`.
   *
   * LOCKED mode avoids copying large payloads; the manager will call `lock()`
   * on the provided buffer and read from it during transmission, calling
   * `unlock()` afterwards.
   *
   * @param pgn The PGN for the message.
   * @param src Reference to a lockable buffer containing the payload.
   * @param da Destination address (default broadcast 0xFF).
   * @param prio Priority (0=highest, 7=lowest, default 6).
   * @return true if the descriptor was queued successfully.
   */
  bool sendLocked(uint32_t pgn, ILockableBuffer &src, uint8_t da = 0xFF, uint8_t prio = 6);

private:
  ICanDriver &can_;
  Queue<J1939Config::POOL_SIZE> pool_; // Direct instance with size configurable by macro
  uint8_t sa_;
  J1939Callbacks cb_;

  // Descriptor queue
  J1939Descriptor queue_[J1939Config::MESSAGE_QUEUE];
  uint8_t q_head_;
  uint8_t q_tail_;
  uint8_t q_count_;
  uint8_t sequence_counter;

  // Number of bytes already consumed from the pool for the active descriptor
  // (used to correctly cleanup the pool when a transfer is aborted).
  uint16_t current_consumed_;

  // Buffer for CAN Frame
  uint8_t scratch_[8];

  // Current state
  J1939TxError last_error_;

  // Timestamp of last DT (or BAM) transmission in milliseconds. Used to enforce
  // the inter-frame gap between TP.DT frames.
  uint16_t last_tp_tx_ms_;

  IJ1939Message *registered_messages_[J1939Config::MAX_REGISTERED_MESSAGES];
  uint8_t registered_count_;

  // Helpers (declarations only)
  bool enqueueDescriptor(J1939Descriptor &desc);
  void startNext(uint16_t now_ms);
  void processCurrent(uint16_t now_ms);
  void releaseCurrent(bool success = true);
  void processRegisteredMessages(uint16_t now_ms);
  uint32_t buildCanId(uint8_t prio, uint32_t pgn, uint8_t sa);
};
