/**
 * @file LEDs.cpp
 * @brief Implementation of the LED indicator state machine.
 *
 * The LEDs module implements a small non-blocking state machine used to
 * indicate node status and error conditions. The green LED reports node
 * state (RUN/STOP/SLEEP/SETUP/CLI) while the red LED reports error conditions
 * (HW failure, CAN warnings/off, PT100 errors).
 *
 * Patterns summary:
 * - Flicker (10Hz): implemented by toggling a bit every 50ms (faster 'flicker')
 * - Blink (2.5Hz): toggled every 200ms group (200ms ON/OFF pattern)
 * - Flash sequences (single/double/triple/quad): built from 200ms ticks
 *   using counters that set/clear flash bits at specific steps. After the
 *   sequence the pattern waits ~1000ms before repeating.
 *
 * Timing details:
 * - The core loop accumulates millis() deltas into `LEDtmr50ms` and consumes
 *   50ms quanta. Every 50ms we toggle the 10Hz flicker bit. Every 4 * 50ms =
 *   200ms we update blink and flash counters.
 */

/**
 * @defgroup LED_bitmasks LED bitmasks
 * @{
 * Bitmasks used in `GenericLedStatus` to represent active visual patterns.
 * - LED_flicker: 10Hz toggling used for quick 'setup' feedback.
 * - LED_blink: 2.5Hz blink used for CLI/reading error indications.
 * - LED_flash_n: sequence bits used by single/double/triple/quadruple flash.
 */
#define LED_flicker 0x01U /**< LED flickering 10Hz */
#define LED_blink 0x02U   /**< LED blinking 2.5Hz */
#define LED_flash_1 0x04U /**< LED single flash */
#define LED_flash_2 0x08U /**< LED double flash */
#define LED_flash_3 0x10U /**< LED triple flash */
#define LED_flash_4 0x20U /**< LED quadruple flash */
/** @} */

#include "LEDs.h"
#include "flow.h"
#include "PT100.h"
#include "cli.h"
#include <status.h>

extern PT100 supply_sensor;      // main.cpp
extern PT100 return_sensor;      // main.cpp
extern Flow flowObj;             // main.cpp
extern NodeStatus_t node_status; // main.cpp
extern CLIScreenManager cli;     // main.cpp
extern uint8_t severity;         // main.cpp

LEDs::LEDs(uint8_t redPin, uint8_t greenPin)
{
    // Store pins and configure as outputs. LEDs are active-low on this board
    // (driven LOW to turn on), so initialize them HIGH (off).
    LedRedPin = redPin;
    LedGreenPin = greenPin;
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
}

void LEDs::process(bool HwFailure)
{
    // ``tick`` becomes true when at least one 50ms quantum has been processed.
    bool tick = false;

    // Accumulate elapsed milliseconds and consume 50ms quanta. This allows
    // the function to be called at arbitrary intervals while keeping timing
    // stable.
    LEDtmr50ms += (millis() - last_millis);
    last_millis = millis();
    while (LEDtmr50ms >= 50)
    {
        tick = true;
        LEDtmr50ms -= 50;

        /* 10Hz flicker: toggle every 50ms */
        GenericLedStatus ^= LED_flicker;

        /* 200ms group: update blink/flash counters every 4 * 50ms */
        if (++LEDtmr200ms > 3)
        {
            LEDtmr200ms = 0;
            /* 2.5Hz blink: toggle every 200ms */
            GenericLedStatus ^= LED_blink;

            /* Single flash sequence: sets LED_flash_1 on the first 200ms
               step, clears it on subsequent steps and resets after 6 steps. */
            switch (++LEDtmrflash_1)
            {
            case 1:
                GenericLedStatus |= LED_flash_1;
                break;
            case 6:
                LEDtmrflash_1 = 0;
            default:
                GenericLedStatus &= ~LED_flash_1;
                break;
            }

            /* Double flash: on at steps 1 and 3, reset after 8 steps */
            switch (++LEDtmrflash_2)
            {
            case 1:
            case 3:
                GenericLedStatus |= LED_flash_2;
                break;
            case 8:
                LEDtmrflash_2 = 0;
            default:
                GenericLedStatus &= ~LED_flash_2;
                break;
            }

            /* Triple flash: on at steps 1,3,5, reset after 10 steps */
            switch (++LEDtmrflash_3)
            {
            case 1:
            case 3:
            case 5:
                GenericLedStatus |= LED_flash_3;
                break;
            case 10:
                LEDtmrflash_3 = 0;
            default:
                GenericLedStatus &= ~LED_flash_3;
                break;
            }

            /* Quadruple flash: on at steps 1,3,5,7, reset after 12 steps */
            switch (++LEDtmrflash_4)
            {
            case 1:
            case 3:
            case 5:
            case 7:
                GenericLedStatus |= LED_flash_4;
                break;
            case 12:
                LEDtmrflash_4 = 0;
            default:
                GenericLedStatus &= ~LED_flash_4;
                break;
            }
        }

    } /* while (LEDtmr50ms >= 50) */

    if (tick)
    {
        uint8_t rd_co, gr_co;

        switch (severity)
        {
        case 0:
            rd_co = 0;
            break;
        case 1:
            rd_co = GenericLedStatus & LED_flash_1;
            break;
        case 2:
            rd_co = GenericLedStatus & LED_flash_2;
            break;
        case 3:
            rd_co = GenericLedStatus & LED_flash_3;
            break;
        case 4:
            rd_co = 1;
            break;
        case 5:
            rd_co = GenericLedStatus & LED_blink;
            break;
        default:
            rd_co = GenericLedStatus & LED_flicker;
            break;
        }

        /* Decide green LED (status) output bit:
         * - CLI connected: show 2.5Hz blink
         * - RUN: solid ON
         * - STOP: always off
         * - SLEEP: single flash pattern
         * - SETUP: inverted flicker (visible pattern)
         */
        if (cli.clientConnected)
            gr_co = GenericLedStatus & LED_blink;
        else if (node_status == RUN)
            gr_co = 1;
        else if (node_status == STOP)
            gr_co = GenericLedStatus & 0; // Always off
        else if (node_status == SLEEP)
            gr_co = GenericLedStatus & LED_flash_1;
        else if (node_status == SETUP)
            gr_co = (GenericLedStatus & LED_flicker) == 0; // Inverted
        else if (node_status == RUN)
            gr_co = 1;
        else
            gr_co = 0;

        // LEDs are active-low: write LOW to turn LED ON
        if (rd_co)
            digitalWrite(LedRedPin, LOW);
        else
            digitalWrite(LedRedPin, HIGH);
        if (gr_co)
            digitalWrite(LedGreenPin, LOW);
        else
            digitalWrite(LedGreenPin, HIGH);

    } /* if (tick) */
}
