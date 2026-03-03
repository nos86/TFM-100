#include "Arduino.h"
#include "EEPROM.h"
#include "energy.h"

/**
 * @brief Load persisted energy state from EEPROM into this instance
 * @return true if valid data was loaded (magic matches), false otherwise
 */
bool Energy::loadFromEEPROM()
{
    uint16_t stored_magic = 0;
    EEPROM.get(ENERGY_EEPROM_OFFSET + EEPROM_MAGIC, stored_magic);
    if (stored_magic != magic)
    {
        eeprom_failure = true;
        return false;
    }

    EEPROM.get(ENERGY_EEPROM_OFFSET + EEPROM_ENERGY_TOTAL, energy_total);
    EEPROM.get(ENERGY_EEPROM_OFFSET + EEPROM_ENERGY_24H, energy_24h);
    return true;
}

/**
 * @brief Persist current energy state into EEPROM
 * @return true on success
 */
bool Energy::saveToEEPROM(bool update_magic)
{
    // Write magic first
    if (update_magic)
        EEPROM.put(ENERGY_EEPROM_OFFSET + EEPROM_MAGIC, magic);
    EEPROM.put(ENERGY_EEPROM_OFFSET + EEPROM_ENERGY_TOTAL, energy_total);
    EEPROM.put(ENERGY_EEPROM_OFFSET + EEPROM_ENERGY_24H, energy_24h);
    // Optionally write last update timestamp
    return true;
}

/**
 * @brief Increase stored energy counters using measured volume.
 * @param supply_temp Supply temperature in °C
 * @param return_temp Return temperature in °C
 * @param volume_liters Volume passed since last sample in litres
 */
void Energy::increase_energy(float supply_temp, float return_temp, float volume_liters)
{
    // Compute energy increment in kWh using simple formula
    float dE = energyFromVolume_kWh(supply_temp, return_temp, volume_liters);

    // Update totals. Signed dE allows positive (heating) and negative (cooling)
    energy_total += dE;
    energy_24h += dE;

    // Mark as dirty; actual EEPROM write is deferred to persistIfDue()
    eeprom_dirty = true;
}

/**
 * @brief Persist energy to EEPROM if dirty and the minimum interval has elapsed.
 * @param now_ms Current millis() timestamp
 */
void Energy::persistIfDue(uint32_t now_ms)
{
    if (!eeprom_dirty)
        return;
    if ((now_ms - last_eeprom_save_ms) >= ENERGY_EEPROM_SAVE_INTERVAL_MS)
    {
        saveToEEPROM();
        eeprom_dirty = false;
        last_eeprom_save_ms = now_ms;
    }
}
