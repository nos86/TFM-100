#include "power.h"
#include "EEPROM.h"

float Power::updatePower(float supply_temp, float return_temp, float flow_lph)
{
    power = getThermalPower(supply_temp, return_temp, flow_lph);
    if (power < 0.0f)
        return 0.0f;

    if (power > max_power)
    {
        max_power = power;
        saveToEEPROM();
    }
    return power;
}

float Power::getPowerPercent()
{
    if (max_power == 0.0f)
        return 0.0f;
    return (power / max_power) * 100.0f;
}

/**
 * @brief Load persisted energy state from EEPROM into this instance
 * @return true if valid data was loaded (magic matches), false otherwise
 */
bool Power::loadFromEEPROM()
{
    uint16_t stored_magic = 0;
    EEPROM.get(POWER_EEPROM_OFFSET + EEPROM_MAGIC, stored_magic);
    if (stored_magic != magic)
    {
        eeprom_failure = true;
        return false;
    }

    EEPROM.get(POWER_EEPROM_OFFSET + EEPROM_MAX_POWER, max_power);
    return true;
}

/**
 * @brief Persist current power state into EEPROM
 * @return true on success
 */
bool Power::saveToEEPROM(bool update_magic)
{
    // Write magic first
    if (update_magic)
        EEPROM.put(POWER_EEPROM_OFFSET + EEPROM_MAGIC, magic);
    EEPROM.put(POWER_EEPROM_OFFSET + EEPROM_MAX_POWER, max_power);
    return true;
}

bool Power::setMaxPower(float new_max_power)
{
    if (new_max_power <= 0.0f)
        return false;

    max_power = new_max_power;
    return saveToEEPROM();
}