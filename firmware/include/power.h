#include "Arduino.h"
#include "config.h"
#include "energy.h"

#pragma once

/**
 * @file power.h
 * @brief Power tracking and persistence for the TFM-100 firmware.
 * @author Salvo Musumeci
 * @note All functions use the following units unless otherwise stated:
 *  - Temperature: degrees Celsius (°C)
 *  - Volume: litres (L)
 *  - Flow: litres per hour (L/h)
 *  - Power returned in kilowatts (kW)
 */

class Power
{
public:
    Power()
    {
        // Attempt to load persisted state from EEPROM
        if (!loadFromEEPROM())
        {
            // No valid data, start from default maximum power
            max_power = 24.0f;
            saveToEEPROM(true); // write magic
        }
    }

    /**
     * @brief Update power statistics based on current sensor readings.
     * @param supply_temp Supply temperature in °C
     * @param return_temp Return temperature in °C
     * @param flow_lph Volumetric flow in litres per hour (L/h)
     * @return Instantaneous thermal power in kilowatts (kW)
     * @remarks Updates max_power if new value exceeds previous maximum.
     */
    float updatePower(float supply_temp, float return_temp, float flow_lph);

    /**
     * @brief Get the percentage of current power relative to the maximum power calibrated.
     * @return Power as a percentage of max_power (0.0f to 100.0f)
     * @remarks Returns 0 if max_power is zero.
     */
    float getPowerPercent();

    /**
     * @brief Set a new maximum power value.
     * @param new_max_power New maximum power value (kW)
     * @return true if max_power updated, false if new_max_power is not greater than current
     * @remarks Persists the new value to EEPROM if updated.
     */
    bool setMaxPower(float new_max_power);

    /**
     * @brief Get the maximum power calibrated.
     * @return Maximum power (kW)
     */
    float getMaxPower() const { return max_power; }

    /**
     * @brief Get the current power consumption.
     * @return Current power (kW)
     */
    float getPower() const { return power; }

    /**
     * @brief Check if EEPROM load was successful.
     * @return true if EEPROM data was loaded correctly, false otherwise
     * @remarks Returns false if last EEPROM load failed or data was invalid.
     */
    bool eepromLoadOk() const { return !eeprom_failure; }

private:
    /**
     * @brief Magic number used to validate EEPROM data integrity.
     * @note Change this value if EEPROM layout changes.
     */
    const uint16_t magic = 0x944A; // Magic number to validate EEPROM data

    float power = 0.0f;          // Current/instantaneous power (kW)
    float max_power = 0.0f;      // Maximum observed power (kW)
    bool eeprom_failure = false; // True if last EEPROM load failed

    enum EEPROM_Layout
    {
        EEPROM_MAGIC = 0,
        EEPROM_MAX_POWER = 2,
    };

    /**
     * @brief Load persisted power state from EEPROM into this instance
     * @return true if valid data was loaded (magic matches), false otherwise
     */
    bool loadFromEEPROM();

    /**
     * @brief Persist current power state into EEPROM
     * @return true on success
     */
    bool saveToEEPROM(bool update_magic = false);


};