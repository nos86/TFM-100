/**
 * @file dtc_dict.h
 * @brief Abstract base class for DTC (Diagnostic Trouble Code) dictionary management
 * @author Salvo Musumeci
 * @note This abstract class must be implemented in main.cpp with concrete DTC data
 */

#pragma once
#include <stdint.h>

typedef struct __attribute__((__packed__))
{
    uint32_t spn;
    uint8_t fmi;
    const char *name;
    uint8_t debounce_ticks;
    uint8_t severity;
} dtc_entity_t;

/**
 * @brief Abstract base class for DTC dictionary operations
 * @remarks This class must be inherited and implemented with concrete DTC data in main.cpp
 */
class DTC_List
{
public:
    /**
     * @brief Get SPN (Suspect Parameter Number) for a DTC entry
     * @param idx Index of the DTC entry
     * @param out_spn Output SPN value
     * @return true if index is valid and SPN retrieved successfully
     */
    virtual bool getSPN(uint16_t idx, uint32_t &out_spn) const = 0;

    /**
     * @brief Get FMI (Failure Mode Identifier) for a DTC entry
     * @param idx Index of the DTC entry
     * @param out_fmi Output FMI value (masked to 5 bits)
     * @return true if index is valid and FMI retrieved successfully
     */
    virtual bool getFMI(uint16_t idx, uint8_t &out_fmi) const = 0;

    /**
     * @brief Get severity level for a DTC entry
     * @param idx Index of the DTC entry
     * @return Severity level, or 0xFF if index is invalid
     */
    virtual uint8_t getSeverity(uint16_t idx) const = 0;

    /**
     * @brief Get debounce ticks for a DTC entry
     * @param idx Index of the DTC entry
     * @return Debounce ticks, or 0xFF if index is invalid
     */
    virtual uint8_t getDebounceTicks(uint16_t idx) const = 0;

    /**
     * @brief Get human-readable name for a DTC entry
     * @param idx Index of the DTC entry
     * @return Pointer to PROGMEM string, or nullptr if index is invalid
     */
    virtual const char *getName(uint16_t idx) const = 0;

    /**
     * @brief Get total number of DTC entries
     * @return Number of DTC entries in the dictionary
     */
    virtual uint16_t getNumberOfErrors() const = 0;

    /**
     * @brief Get magic number for EEPROM storage
     * @return Magic number
     */
    virtual uint16_t getMagicNumber() const = 0;
};
