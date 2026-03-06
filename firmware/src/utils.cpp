
#include <stdint.h>
#include <config.h>
#include <EEPROM.h>
// -----------------------------------------------------------------------------
// Persistence hooks (default application implementation using AVR EEPROM)
// These implement the required low-level write/read of raw bytes at a
// logical offset. The library composes logical offsets starting at 0 for
// diagnostics; we map them to physical EEPROM addresses by adding
// DIAGNOSTICS_EEPROM_OFFSET.
// -----------------------------------------------------------------------------
void diag_save_to_persistent(uint16_t offset, const uint8_t *data, uint8_t length)
{
    uint16_t phys = (uint16_t)(DIAGNOSTICS_EEPROM_OFFSET + offset);
    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t v = data[i];
        // EEPROM.update writes only if value differs, minimizing wear
        EEPROM.update(phys + i, v);
    }
}

bool diag_load_from_persistent(uint16_t offset, uint8_t *data, uint8_t length)
{
    uint16_t phys = (uint16_t)(DIAGNOSTICS_EEPROM_OFFSET + offset);
    for (uint8_t i = 0; i < length; i++)
    {
        data[i] = EEPROM.read(phys + i);
    }
    return true; // read always succeeds (caller is responsible for magic validation)
}