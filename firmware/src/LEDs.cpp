/*
 * CANopen LED Behavior
 * ----------------------------------------------------------------------
 * | Behavior          | Green LED           | Red LED                   |
 * ----------------------------------------------------------------------
 * | Off               | STOP                | No Error                  |
 * | Flickering (10Hz) | Setup               | Hardware Failure          |
 * | Blinking (2.5Hz)  | CLI connected       | PT100 Reading Error       |
 * | Single Flash      | SLEEP               | CAN warning               |
 * | On                | RUN                 | CAN Bus Off               |
 * ----------------------------------------------------------------------
 *
 * Flashing is implemented as following:
 * - 200ms on
 * - 200ms off (except for the last flash)
 * - 200ms on (except for the last flash)
 * - 1000ms off
 */

/**
 * @defgroup LED_bitmasks LED bitmasks
 * @{
 * Bitmasks for the LED indicators
 */
#define LED_flicker 0x01U /**< LED flickering 10Hz */
#define LED_blink 0x02U   /**< LED blinking 2,5Hz */
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
extern bool CANbusOff;           // main.cpp
extern bool CANbusWarn;          // main.cpp
extern CLIScreenManager cli;     // main.cpp

LEDs::LEDs(uint8_t redPin, uint8_t greenPin)
{
    LedRedPin = redPin;
    LedGreenPin = greenPin;
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
}

void LEDs::process(bool HwFailure)
{
    bool tick = false;

    LEDtmr50ms += (millis() - last_millis);
    last_millis = millis();
    while (LEDtmr50ms >= 50)
    {
        tick = true;
        LEDtmr50ms -= 50;

        /* calculate 10Hz flickering */
        GenericLedStatus ^= LED_flicker;

        /* calculate 200ms events */
        if (++LEDtmr200ms > 3)
        {
            LEDtmr200ms = 0;
            /* calculate 2,5Hz blinking */
            GenericLedStatus ^= LED_blink;

            /* calculate Single Flashing */
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

            /* calculate Double Flashing */
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

            /* calculate Triple Flashing */
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

            /* calculate Quadruple Flashing */
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
        /* red ERROR LED */
        if (HwFailure)
            rd_co = GenericLedStatus & LED_flicker;
        else if (CANbusOff)
            rd_co = 1;
        else if (CANbusWarn)
            rd_co = GenericLedStatus & LED_flash_1;
        else if (supply_sensor.errorDetected || return_sensor.errorDetected)
            rd_co = GenericLedStatus & LED_blink;
        else
            rd_co = 0;
        /* green RUN LED */
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

        // Set the LEDs
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
