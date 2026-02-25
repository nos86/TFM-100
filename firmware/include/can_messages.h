#pragma once

/**
 * @file can_messages.h
 * @brief Declarations for application J1939 message helpers.
 *
 * This header exposes small `IJ1939Message` helpers (via `PeriodicMessage`) for
 * heartbeat, raw temperature and filtered temperature+flow PGNs. These classes
 * are intended to be registered with `J1939Manager` so that the manager can
 * periodically poll them and enqueue transmissions.
 *
 * Important: see implementation notes in `src/can_messages.cpp` about the
 * lifetime/ownership of buffers returned by `buildPayload()`.
 */

#include <J1939Manager.h>
#include <messages/HeartbeatMessage.h>
#include <messages/FilteredTemperatureMessage.h>
#include <messages/RawTemperatureMessage.h>
#include <messages/PowerAndEnergy.h>
