#include "Arduino.h"
#include "config.h"

#pragma once

/**
 * @file energy.h
 * @brief Energy calculation helpers and persistence for the TFM-100 firmware.
 * @author Salvo Musumeci
 * @note All functions use the following units unless otherwise stated:
 *  - Temperature: degrees Celsius (°C)
 *  - Volume: litres (L)
 *  - Flow: litres per hour (L/h)
 *  - Specific heat capacity: kJ/(kg·K)
 *  - Density: kg/L
 *  - Power returned in kilowatts (kW)
 *  - Energy returned/stored in kilowatt-hours (kWh)
 */

// Physical constants (water) used by the simple energy calculations.
// Use temperature-dependent values for higher metering accuracy if required.
#define cp 4.186f    // kJ/kg°C for water
#define density 1.0f // kg/L for water

/**
 * @brief Compute instantaneous thermal power.
 * @param supply_temp_degC Supply temperature in °C
 * @param return_temp_degC Return temperature in °C
 * @param flow_lph Volumetric flow in litres per hour (L/h)
 * @return Thermal power in kilowatts (kW). Signed: positive means heat delivered when supply > return.
 *
 * Units:
 * - cp: kJ/(kg·K)
 * - density: kg/L
 * - L/h -> kg/h -> kJ/h -> kJ/s -> kW (divide by 3600)
 */
inline float getThermalPower(float supply_temp, float return_temp, float flow_lph)
{
    const float seconds_per_hour = 3600.0f;

    float delta_t = supply_temp - return_temp;

    // optional deadband to avoid noise integration
    const float DT_DEADBAND = 0.05f; // °C
    if (fabs(delta_t) < DT_DEADBAND)
        delta_t = 0.0f;

    // return signed power (positive when supply > return)
    return (flow_lph * density * cp * delta_t) / seconds_per_hour;
}

/**
 * @brief Compute energy increment (kWh) from measured volume.
 * @param supply_temp_degC Supply temperature in °C
 * @param return_temp_degC Return temperature in °C
 * @param volume_liters Volume passed since last sample in liters
 * @return energy increment in kWh, or 0 if supply_temp ≤ return_temp + deadband
 */
inline float energyFromVolume_kWh(float supply_temp_degC, float return_temp_degC, float volume_liters)
{
    float delta_t = supply_temp_degC - return_temp_degC;

    // Do not accumulate energy when return temperature is at or above supply temperature.
    // A negative (or near-zero) delta_T indicates a temperature anomaly or idle circuit
    // with sensor offset; counting it would reduce the stored total energy incorrectly.
    const float DT_DEADBAND = 0.05f; // °C — one-sided lower-bound guard: only accumulate when supply exceeds return by at least this amount.
                                     // Note: getThermalPower uses a symmetric fabs-based deadband; this is intentionally asymmetric.
    if (delta_t < DT_DEADBAND)
        return 0.0f;

    float energy_kJ = volume_liters * density * cp * delta_t; // kJ
    float energy_kWh = energy_kJ / 3600.0f;                   // kWh (since 1 kWh = 3600 kJ)
    return energy_kWh;
}

class Energy
{
public:
    Energy()
    {
        // Attempt to load persisted state from EEPROM
        if (!loadFromEEPROM())
        {
            // No valid data, start from zero
            energy_24h = 0.0f;
            energy_total = 0.0f;
            saveToEEPROM(true); // write magic
        }
    }

    /**
     * @brief Increase stored energy counters using measured volume.
     * @param supply_temp Supply temperature in °C
     * @param return_temp Return temperature in °C
     * @param volume_liters Volume passed since last sample in litres
     * @remarks energy values are stored in kWh
     */
    void increase_energy(float supply_temp, float return_temp, float volume_liters);

    /** @brief Return energy consumed in the last 24 hours (kWh) */
    float getEnergy24h() { return energy_24h; }
    /** @brief Return total accumulated energy (kWh) */
    float getEnergyTotal() { return energy_total; }

    /**
     * @brief Persist energy to EEPROM if dirty and the minimum interval has elapsed.
     * @param now_ms Current millis() timestamp
     * @remarks Call periodically (e.g., every scheduler tick) to bound EEPROM write rate.
     *          Writes at most once every ENERGY_EEPROM_SAVE_INTERVAL_MS milliseconds.
     */
    void persistIfDue(uint32_t now_ms);

    /**
     * @brief Check if EEPROM load was successful.
     * @return true if EEPROM data was loaded correctly, false otherwise
     * @remarks Returns false if last EEPROM load failed or data was invalid.
     */
    bool eepromLoadOk() const { return !eeprom_failure; }

    /**
     * @brief Reset both 24h and total energy and persist to EEPROM
     */
    void resetAll()
    {
        energy_24h = 0.0f;
        energy_total = 0.0f;
        saveToEEPROM();
    }

    /** @brief Reset only the 24h energy counter and persist */
    void reset24h()
    {
        energy_24h = 0.0f;
        saveToEEPROM();
    }

private:
    const uint16_t magic = 0x1943; // Magic number to validate EEPROM data

    float energy_24h = 0.0f;     // Energy consumed in last 24 hours (kWh)
    float energy_total = 0.0f;   // Total energy consumed (kWh)
    bool eeprom_failure = false; // True if last EEPROM load failed
    bool eeprom_dirty = false;   // True if in-RAM values differ from last EEPROM save
    uint32_t last_eeprom_save_ms = 0; // millis() at last EEPROM persist

    enum EEPROM_Layout
    {
        EEPROM_MAGIC = 0,
        EEPROM_ENERGY_TOTAL = 2,
        EEPROM_ENERGY_24H = 6,
    };

    /**
     * @brief Load persisted energy state from EEPROM into this instance
     * @return true if valid data was loaded (magic matches), false otherwise
     */
    bool loadFromEEPROM();

    /**
     * @brief Persist current energy state into EEPROM
     * @return true on success
     */
    bool saveToEEPROM(bool update_magic = false);
};