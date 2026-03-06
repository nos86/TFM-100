#pragma once

#include <stdint.h>

/**
 * @file LEDs.h
 * @brief Simple LED indicator state machine used for status and error signalling.
 *
 * This class implements a small non-blocking LED state machine used by the
 * firmware to indicate RUN/STOP/ERROR states and provide simple blink
 * patterns. It is driven by calling `process()` from the main loop.
 */
class LEDs
{
public:
    /**
     * @brief Construct an LEDs controller.
     *
     * @param redPin Arduino pin connected to the red LED.
     * @param greenPin Arduino pin connected to the green LED.
     */
    LEDs(uint8_t redPin, uint8_t greenPin);

    /**
     * @brief Progress the LED state machine.
     *
     * This method should be called frequently (e.g., every main loop
     * iteration).
     */
    void process();

private:
    uint8_t LedRedPin;   /**< Pin connected to the red LED. */
    uint8_t LedGreenPin; /**< Pin connected to the green LED. */

    uint32_t last_millis = 0; /**< Last timestamp used for timing (ms) */
    uint32_t LEDtmr50ms;      /**< 50 ms timer tick counter */
    uint8_t LEDtmr200ms;      /**< 200 ms timer tick counter */
    uint8_t LEDtmrflash_1;    /**< Single-flash pattern timer */
    uint8_t LEDtmrflash_2;    /**< Double-flash pattern timer */
    uint8_t LEDtmrflash_3;    /**< Triple-flash pattern timer */
    uint8_t LEDtmrflash_4;    /**< Quadruple-flash pattern timer */
    uint8_t GenericLedStatus; /**< Led status bitfield (see LED_bitmasks) */
};
