
#include "diagnostics.h"
#include "dtc_dict.h"

Diagnostics::Diagnostics(DTC_List *dict)
    : dict_(dict)
{
    // Attempt to load persisted diagnostics memory (application hook).
    // If loading fails, initialize with clear().
    eepromLoadOk_ = loadFromEEPROM();
    if (!eepromLoadOk_)
    {
        clear();
    }
}

void Diagnostics::setRaw(uint8_t dtc_idx, bool raw)
{
    // Update status if exists
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) != DTC_EMPTY &&
            errors[i].dtc_idx == dtc_idx)
        {
            dtc_set_raw(&errors[i], raw);
            return;
        }
    }
    // Ignore if error is not set
    if (!raw)
        return;

    // Add new error if not exists
    uint8_t position = UINT8_MAX;
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) == DTC_EMPTY)
        {
            position = i;
            break;
        }
    }
    // If no empty slot, overwrite the first History
    if (position == UINT8_MAX)
    {
        for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
        {
            if (dtc_get_state(&errors[i]) == DTC_HISTORY)
            {
                position = i;
                break;
            }
        }
    }
    // If still no slot, skip adding the new error
    if (position == UINT8_MAX)
        return;
    // Add new error
    errors[position].dtc_idx = dtc_idx;
    errors[position].occurrence = 0;
    errors[position].flags = 0;
    dtc_set_raw(&errors[position], true);
    dtc_set_debounce(&errors[position], 0);
    // Use helper to set state and persist
    setStateAndPersist(position, DTC_PENDING);
    // OnEntry callback is triggered by setStateAndPersist
    // notify count changed if increased
    if (onCountChangedCb)
        onCountChangedCb(numberOfErrors());
    return;
}

void Diagnostics::periodic_update()
{
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        auto &err = errors[i];
        dtc_state_t state = dtc_get_state(&err);
        bool raw = dtc_get_raw(&err);
        uint8_t deb = dtc_get_debounce(&err);
        if (deb < 15)
            dtc_set_debounce(&err, ++deb);

        switch (state)
        {
        case DTC_EMPTY:
            // Nothing to do
            break;
        case DTC_PENDING:
            if (raw)
            {
                if (deb >= dict_->getDebounceTicks(err.dtc_idx) || deb == 15)
                {
                    if (err.occurrence < 127)
                        err.occurrence++;
                    setStateAndPersist(i, DTC_ACTIVE);
                    dtc_set_debounce(&err, 0);
                }
            }
            else
            {
                dtc_set_debounce(&err, 0);
                setStateAndPersist(i, DTC_HISTORY);
            }
            break;
        case DTC_ACTIVE:
            if (!raw)
            {
                dtc_set_debounce(&err, 1);
                setStateAndPersist(i, DTC_HEALING);
            }
            break;
        case DTC_HEALING:
            if (!raw)
            {
                if (deb >= dict_->getDebounceTicks(err.dtc_idx) || deb == 15)
                {
                    setStateAndPersist(i, DTC_HISTORY);
                    dtc_set_debounce(&err, 0);
                }
            }
            else
            {
                dtc_set_debounce(&err, 0);
                setStateAndPersist(i, DTC_ACTIVE);
            }
            break;
        case DTC_HISTORY:
            if (raw)
            {
                setStateAndPersist(i, DTC_PENDING);
                dtc_set_debounce(&err, 1);
            }
            break;
        }
    }
}

void Diagnostics::clear()
{
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        errors[i].dtc_idx = 0xFF; // Invalid index
        errors[i].occurrence = 0;
        errors[i].flags = 0; // Clear all flags
        dtc_set_state(&errors[i], DTC_EMPTY);
    }
    // After clearing, ensure EEPROM reflects empty memory
    saveToEEPROM();
    if (onCountChangedCb)
        onCountChangedCb(numberOfErrors());
}

/**
 * @brief Get the DTC index for the active error at the given position
 * @param pos Position in the list of active errors (0-based)
 * @return DTC index if found, or 0xFF if not found
 */
uint8_t Diagnostics::getIndexForActiveErrorAt(uint8_t pos) const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) == DTC_ACTIVE)
        {
            if (count == pos)
                return errors[i].dtc_idx;
            count++;
        }
    }
    return 0xFF; // Not found
}

uint8_t Diagnostics::numberOfActiveErrors() const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) == DTC_ACTIVE)
            count++;
    }
    return count;
}

uint8_t Diagnostics::numberOfErrors() const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) != DTC_EMPTY)
            count++;
    }
    return count;
}

const char *Diagnostics::getErrorDescription(uint8_t idx) const
{
    if (idx >= DIAGNOSTICS_MEMORY_SIZE)
        return "";

    const char *name = dict_->getName(errors[idx].dtc_idx);
    if (name)
        return name;
    return "Unknown DTC";
}
const uint8_t Diagnostics::getOccurrenceCount(uint8_t idx) const

{
    if (idx >= DIAGNOSTICS_MEMORY_SIZE)
        return 0;
    return errors[idx].occurrence;
}

uint8_t Diagnostics::getErrorSeverity(uint8_t idx) const
{
    if (idx >= DIAGNOSTICS_MEMORY_SIZE)
        return 0xFF;

    return dict_->getSeverity(errors[idx].dtc_idx);
}

uint8_t Diagnostics::getMaxSeverity() const
{
    uint8_t max_severity = 0;
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        if (dtc_get_state(&errors[i]) == DTC_ACTIVE)
        {
            uint8_t sev = dict_->getSeverity(errors[i].dtc_idx);
            if (sev != 0xFF && sev > max_severity)
                max_severity = sev;
        }
    }
    return max_severity;
}

void Diagnostics::saveEntryToEEPROM(uint8_t index)
{
    if (index >= DIAGNOSTICS_MEMORY_SIZE)
        return;
    // Ensure offset and length fit in hook parameters (uint16_t offset, uint8_t length)
    const uint16_t offset = (uint16_t)(2 + index * sizeof(dtc_history_t));
    const uint8_t length = (uint8_t)sizeof(dtc_history_t);
    if (length == 0)
        return;
    diag_save_to_persistent(offset, reinterpret_cast<const uint8_t *>(&errors[index]), length);
}

// Helper: set state for entry index and persist only if it actually changes
void Diagnostics::setStateAndPersist(uint8_t index, dtc_state_t newState)
{
    if (index >= DIAGNOSTICS_MEMORY_SIZE)
        return;
    dtc_state_t old = dtc_get_state(&errors[index]);
    if (old == newState)
        return;
    dtc_set_state(&errors[index], newState);
    // Persist only when entering ACTIVE or HISTORY states
    if (newState == DTC_ACTIVE || newState == DTC_HISTORY)
    {
        saveEntryToEEPROM(index);
    }
    if (onEntryChangedCb)
        onEntryChangedCb(index, &errors[index]);
}

// Persist diagnostics memory to EEPROM if enabled
void Diagnostics::saveToEEPROM()
{
    // Serialize magic and write it first, then each entry individually.
    const uint16_t magic = dict_->getMagicNumber();
    uint8_t buf[2];
    buf[0] = (uint8_t)(magic & 0xFF);
    buf[1] = (uint8_t)((magic >> 8) & 0xFF);
    diag_save_to_persistent(0, buf, 2);
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
        saveEntryToEEPROM(i);
}

// Load diagnostics memory from EEPROM if enabled and magic matches
bool Diagnostics::loadFromEEPROM()
{
    uint8_t buf[2];
    bool ok = diag_load_from_persistent(0, buf, 2);
    if (!ok)
        return false;
    // verify magic
    uint16_t magic = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    if (magic != dict_->getMagicNumber())
        return false;
    // Load each entry individually
    for (uint8_t i = 0; i < DIAGNOSTICS_MEMORY_SIZE; i++)
    {
        const uint16_t offset = (uint16_t)(2 + i * sizeof(dtc_history_t));
        const uint8_t length = (uint8_t)sizeof(dtc_history_t);
        ok = diag_load_from_persistent(offset, reinterpret_cast<uint8_t *>(&errors[i]), length);
        if (!ok)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Default (weak) platform persistence hooks
// Applications may override these by providing their own implementations
// with the same signatures (without the weak attribute), e.g. in main.cpp.
// ---------------------------------------------------------------------------

// Default platform persistence hooks are no-ops here so the diagnostics
// library remains agnostic. Applications should provide their own
// implementations of these functions (without the weak attribute)
// if they want persistence behavior.
void __attribute__((weak)) diag_save_to_persistent(uint16_t offset, const uint8_t *data, uint8_t length)
{
    (void)offset;
    (void)data;
    (void)length;
    // no-op
}

bool __attribute__((weak)) diag_load_from_persistent(uint16_t offset, uint8_t *data, uint8_t length)
{
    (void)offset;
    (void)data;
    (void)length;
    // Default: nothing loaded
    return false;
}
