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

    // Persistence is controlled entirely by application-provided hooks.
    // Do not provide an offset via constructor or API in the library; the
    // application is responsible for managing any address/namespace.

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
    /**
     * @brief Returns true if the diagnostics memory was successfully loaded from persistent storage
     */
    bool eepromLoadOk() const { return eepromLoadOk_; }

private:
    DTC_List *dict_;                                              // Pointer to DTC dictionary implementation
    dtc_history_t errors[DIAGNOSTICS_MEMORY_SIZE];                // DTC history array
    bool eepromLoadOk_ = false;                                   // Indicates whether last loadFromEEPROM succeeded
    void saveEntryToEEPROM(uint8_t index);                        // Save a single entry to EEPROM
    void saveToEEPROM();                                          // Persist memory via platform hook (no-op by default)
    bool loadFromEEPROM();                                        // Load memory via platform hook (no-op by default). Returns true on success
    void setStateAndPersist(uint8_t index, dtc_state_t newState); //WS Helper to set state and persist when changed
};

/*
 * Platform persistence hooks
 *
 * Applications may provide their own implementations of these functions
 * (for example in `main.cpp`) to control how diagnostics memory is
 * persisted. If not provided, the library supplies weak default
 * implementations that will make no operation.
 *
 * Signatures (hooks operate on raw bytes):
 *  - void diag_save_to_persistent(uint16_t offset, const uint8_t *data, uint8_t length);
 *    -> Write `length` raw bytes starting at logical `offset` (application maps logical offset to physical storage).
 *  - bool diag_load_from_persistent(uint16_t offset, uint8_t *data, uint8_t length);
 *    -> Read `length` bytes into `data` from logical `offset`. Return true on success.
 */
void diag_save_to_persistent(uint16_t offset, const uint8_t *data, uint8_t length);
bool diag_load_from_persistent(uint16_t offset, uint8_t *data, uint8_t length);
