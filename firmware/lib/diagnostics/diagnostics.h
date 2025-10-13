#pragma once

#include "dtc_dict.h"
#include <stdint.h>

#ifndef DIAGNOSTICS_MEMORY_SIZE
#warning "DIAGNOSTICS_MEMORY_SIZE is not defined by project, using default 16"
#define DIAGNOSTICS_MEMORY_SIZE 16 // Define the size of the error history
#endif

typedef enum
{
    DTC_EMPTY = 0,
    DTC_PENDING = 1,
    DTC_ACTIVE = 2,
    DTC_HEALING = 3,
    DTC_HISTORY = 4
} dtc_state_t;

typedef struct
{
    uint8_t dtc_idx;
    uint8_t occurrence;
    union
    {
        uint8_t flags; // storage reale: [3:0]=debounce, [6:4]=state, [7]=raw
        struct
        {
            unsigned debounce : 4;  // 0..15
            unsigned state : 3;     // dtc_state_t (0..7)
            unsigned fault_raw : 1; // 0/1
        } bits;                     // <- comodo per debug/lettura, evita in percorsi hot
    };
} dtc_history_t;

// Utility functions for dtc_history_t manipulation
uint8_t dtc_get_debounce(const dtc_history_t *s);
void dtc_set_debounce(dtc_history_t *s, uint8_t deb);
dtc_state_t dtc_get_state(const dtc_history_t *s);
void dtc_set_state(dtc_history_t *s, dtc_state_t st);
bool dtc_get_raw(const dtc_history_t *s);
void dtc_set_raw(dtc_history_t *s, bool raw);

class Diagnostics
{

public:
    /**
     * @brief Constructor for Diagnostics system
     * @param dict Pointer to concrete DTC_Dict implementation
     */
    Diagnostics(DTC_List *dict);

    void setRaw(uint8_t dtc_idx, bool raw);
    void periodic_update();
    void clear();
    uint8_t numberOfActiveErrors() const;
    uint8_t numberOfErrors() const;
    const dtc_history_t *getMemory() const { return errors; }
    const char *getErrorDescription(uint8_t idx) const;
    const uint8_t getOccurrenceCount(uint8_t idx) const;
    uint8_t getErrorSeverity(uint8_t idx) const;
    uint8_t getMaxSeverity() const;
    uint8_t getIndexForActiveErrorAt(uint8_t pos) const;

private:
    DTC_List *dict_;                               // Pointer to DTC dictionary implementation
    dtc_history_t errors[DIAGNOSTICS_MEMORY_SIZE]; // DTC history array
};
