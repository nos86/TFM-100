/**
 * @file dip_switch.h
 * @brief Read DIP switch settings used to derive node address.
 *
 * Provides the public API for reading the hardware DIP switch value. The
 * implementation lives in `src/dip_switch.cpp`.
 */
#pragma once

#include <stdint.h>

/**
 * @brief Read the DIP switch value (raw bits).
 *
 * The function returns the DIP switch numerical value which is combined with
 * the `NODE_ID_BASE` constant to compute the node source address.
 *
 * @return raw DIP switch value (0..255)
 */
uint8_t dip_switch_read(void);
