#pragma once

/**
 * @file can_messages.h
 * @brief VSCP message helpers registered with J1939Manager.
 *
 * This header re-exports all IJ1939Message helpers used by the application.
 * The CAN transport is handled by J1939Manager; each message class below
 * produces a VSCP Level 1 event payload and a VSCP-compatible CAN identifier
 * (via VSCP_PGN()) so that other VSCP nodes on the bus can decode them.
 *
 * Measurement encoding follows the VSCP CLASS1.MEASUREMENT normalised-integer
 * format: data byte 0 = coding byte, byte 1 = decimal exponent, remaining
 * bytes = integer value MSB first.
 */

#include <J1939Manager.h>
#include <vscp.h>
#include <messages/HeartbeatMessage.h>
#include <messages/vscp_messages.h>
