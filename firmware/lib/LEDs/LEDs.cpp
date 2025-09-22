/*
 * CANopen LED Behavior
 * ----------------------------------------------------------------------
 * | Behavior          | Green LED           | Red LED                   |
 * ----------------------------------------------------------------------
 * | Off               | Init                | No Error                  |
 * | Flickering (10Hz) | Setup               | Hardware Failure          |
 * | Blinking (2.5Hz)  | -                   | PT100 Reading Error       |
 * | Single Flash      | Sleep               | CAN warning               |
 * | On                | RUN                 | CAN Bus Off               |
 * ----------------------------------------------------------------------
 *
 * Flashing is implemented as following:
 * - 200ms on
 * - 200ms off (except for the last flash)
 * - 200ms on (except for the last flash)
 * - 1000ms off
 */

#include "LEDs.h"
#include <stdlib.h>
#include <string.h>
#include "flow.h"
#include "PT100.h"

extern PT100 supply_sensor;      // main.cpp
extern PT100 return_sensor;      // main.cpp
extern Flow flowObj;             // main.cpp
extern NodeStatus_t node_status; // main.cpp
extern bool CANbusOff;           // main.cpp
extern bool CANbusWarn;          // main.cpp

LEDs_t *LEDs;
uint32_t last_millis = 0;

void LEDs_init(uint8_t redPin, uint8_t greenPin)
{
    if (LEDs)
        free(LEDs);
    LEDs = (LEDs_t *)malloc(sizeof(LEDs_t));
    (void)memset(LEDs, 0, sizeof(LEDs_t));
    LEDs->LedRedPin = redPin;
    LEDs->LedGreenPin = greenPin;
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
}

void LEDs_process(bool HwFailure)
{
    bool tick = false;

    LEDs->LEDtmr50ms += (millis() - last_millis);
    last_millis = millis();
    while (LEDs->LEDtmr50ms >= 50)
    {
        tick = true;
        LEDs->LEDtmr50ms -= 50;

        /* calculate 10Hz flickering */
        LEDs->GenericLedStatus ^= LED_flicker;

        /* calculate 200ms events */
        if (++LEDs->LEDtmr200ms > 3)
        {
            LEDs->LEDtmr200ms = 0;
            /* calculate 2,5Hz blinking */
            LEDs->GenericLedStatus ^= LED_blink;

            /* calculate Single Flashing */
            switch (++LEDs->LEDtmrflash_1)
            {
            case 1:
                LEDs->GenericLedStatus |= LED_flash_1;
                break;
            case 6:
                LEDs->LEDtmrflash_1 = 0;
            default:
                LEDs->GenericLedStatus &= ~LED_flash_1;
                break;
            }

            /* calculate Double Flashing */
            switch (++LEDs->LEDtmrflash_2)
            {
            case 1:
            case 3:
                LEDs->GenericLedStatus |= LED_flash_2;
                break;
            case 8:
                LEDs->LEDtmrflash_2 = 0;
            default:
                LEDs->GenericLedStatus &= ~LED_flash_2;
                break;
            }

            /* calculate Triple Flashing */
            switch (++LEDs->LEDtmrflash_3)
            {
            case 1:
            case 3:
            case 5:
                LEDs->GenericLedStatus |= LED_flash_3;
                break;
            case 10:
                LEDs->LEDtmrflash_3 = 0;
            default:
                LEDs->GenericLedStatus &= ~LED_flash_3;
                break;
            }

            /* calculate Quadruple Flashing */
            switch (++LEDs->LEDtmrflash_4)
            {
            case 1:
            case 3:
            case 5:
            case 7:
                LEDs->GenericLedStatus |= LED_flash_4;
                break;
            case 12:
                LEDs->LEDtmrflash_4 = 0;
            default:
                LEDs->GenericLedStatus &= ~LED_flash_4;
                break;
            }
        }

    } /* while (LEDs->LEDtmr50ms >= 50) */

    if (tick)
    {
        uint8_t rd_co, gr_co;

        /* red ERROR LED */
        if (HwFailure)
        {
            rd_co = LEDs->GenericLedStatus & LED_flicker;
        }
        else if (CANbusOff)
        {
            rd_co = 1;
        }
        else if (CANbusWarn)
        {
            rd_co = LEDs->GenericLedStatus & LED_flash_1;
        }
        else if (supply_sensor.errorDetected || return_sensor.errorDetected)
        {
            rd_co = LEDs->GenericLedStatus & LED_blink;
        }
        else
        {
            rd_co = 0;
        }

        /* green RUN LED */
        if (0) // cli_connected)
        {
            gr_co = 1;
        }
        else if (node_status == STOP)
        {
            gr_co = LEDs->GenericLedStatus & LED_blink;
        }
        else if (node_status == SLEEP)
        {
            gr_co = LEDs->GenericLedStatus & LED_flash_1;
        }
        else if (node_status == SETUP)
        {
            gr_co = (LEDs->GenericLedStatus & LED_flicker) == 0; // Inverted
        }
        else if (node_status == RUN)
        {
            gr_co = 1;
        }
        else
        {
            gr_co = 0;
        }

        // Set the LEDs
        if (rd_co)
        {
            digitalWrite(LEDs->LedRedPin, LOW);
        }
        else
        {
            digitalWrite(LEDs->LedRedPin, HIGH);
        }
        if (gr_co)
        {
            digitalWrite(LEDs->LedGreenPin, LOW);
        }
        else
        {
            digitalWrite(LEDs->LedGreenPin, HIGH);
        }

    } /* if (tick) */
}
