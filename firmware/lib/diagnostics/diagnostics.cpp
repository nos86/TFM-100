
#include "diagnostics.h"
#include "dtc_dict.h"

Diagnostics::Diagnostics(DTC_List *dict)
    : dict_(dict)
{
    clear();
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
    dtc_set_state(&errors[position], DTC_PENDING);
    dtc_set_raw(&errors[position], true);
    dtc_set_debounce(&errors[position], 0);
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
                    dtc_set_state(&err, DTC_ACTIVE);
                    dtc_set_debounce(&err, 0);
                    if (err.occurrence < 127)
                        err.occurrence++;
                }
            }
            else
            {
                dtc_set_debounce(&err, 0);
                dtc_set_state(&err, DTC_HISTORY);
            }
            break;
        case DTC_ACTIVE:
            if (!raw)
            {
                dtc_set_debounce(&err, 1);
                dtc_set_state(&err, DTC_HEALING);
            }
            break;
        case DTC_HEALING:
            if (!raw)
            {
                if (deb >= dict_->getDebounceTicks(err.dtc_idx) || deb == 15)
                {
                    dtc_set_state(&err, DTC_HISTORY);
                    dtc_set_debounce(&err, 0);
                }
            }
            else
            {
                dtc_set_debounce(&err, 0);
                dtc_set_state(&err, DTC_ACTIVE);
            }
            break;
        case DTC_HISTORY:
            if (raw)
            {
                dtc_set_state(&err, DTC_PENDING);
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
        if (dtc_get_state(&errors[i]) != DTC_EMPTY)
        {
            uint8_t sev = dict_->getSeverity(errors[i].dtc_idx);
            if (sev != 0xFF && sev > max_severity)
                max_severity = sev;
        }
    }
    return max_severity;
}
