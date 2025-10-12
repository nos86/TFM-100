#include "dtc_dict.h"
#include <avr/pgmspace.h>
#include <string.h>

#pragma once

#define MAGIC_NUMBER 0x5886

typedef enum
{
    DTC_DSMEepromFailure = 0,
    DTC_SupplyLineRtdHighThres,
    DTC_SupplyLineRtdLowThres,
    DTC_SupplyLineRefInHigh,
    DTC_SupplyLineRefInLow,
    DTC_SupplyLineRtdInLow,
    DTC_SupplyLineUOV,
    DTC_SupplyLineDeviceError,
    DTC_ReturnLineRtdHighThres,
    DTC_ReturnLineRtdLowThres,
    DTC_ReturnLineRefInHigh,
    DTC_ReturnLineRefInLow,
    DTC_ReturnLineRtdInLow,
    DTC_ReturnLineUOV,
    DTC_ReturnLineDeviceError,
    DTC_CANBusOff,
    DTC_CANBusErrorPassive,
    DTC_CANBusDeviceError,

} dtc_enum_t;

typedef enum
{
    OFF = 0,
    ONE_BLINK = 1,
    TWO_BLINKS = 2,
    THREE_BLINKS = 3,
    SOLID = 4,
    FLASHING = 5,
    FLICKERING = 6,
} red_lamp_state_t;

const char DTC_DSMEepromFailure_[] PROGMEM = "DTC_DSMEepromFailure";
const char DTC_SupplyLineRtdHighThres_[] PROGMEM = "DTC_SupplyLineRtdHighThres";
const char DTC_SupplyLineRtdLowThres_[] PROGMEM = "DTC_SupplyLineRtdLowThres";
const char DTC_SupplyLineRefInHigh_[] PROGMEM = "DTC_SupplyLineRefInHigh";
const char DTC_SupplyLineRefInLow_[] PROGMEM = "DTC_SupplyLineRefInLow";
const char DTC_SupplyLineRtdInLow_[] PROGMEM = "DTC_SupplyLineRtdInLow";
const char DTC_SupplyLineUOV_[] PROGMEM = "DTC_SupplyLineUOV";
const char DTC_SupplyLineDeviceError_[] PROGMEM = "DTC_SupplyLineDeviceError";
const char DTC_ReturnLineRtdHighThres_[] PROGMEM = "DTC_ReturnLineRtdHighThres";
const char DTC_ReturnLineRtdLowThres_[] PROGMEM = "DTC_ReturnLineRtdLowThres";
const char DTC_ReturnLineRefInHigh_[] PROGMEM = "DTC_ReturnLineRefInHigh";
const char DTC_ReturnLineRefInLow_[] PROGMEM = "DTC_ReturnLineRefInLow";
const char DTC_ReturnLineRtdInLow_[] PROGMEM = "DTC_ReturnLineRtdInLow";
const char DTC_ReturnLineUOV_[] PROGMEM = "DTC_ReturnLineUOV";
const char DTC_ReturnLineDeviceError_[] PROGMEM = "DTC_ReturnLineDeviceError";
const char DTC_CANBusOff_[] PROGMEM = "DTC_CANBusErrorBusOff";
const char DTC_CANBusErrorPassive_[] PROGMEM = "DTC_CANBusErrorPassive";
const char DTC_CANBusDeviceError_[] PROGMEM = "DTC_CANBusDeviceError";
// // Add more strings as needed

const dtc_entity_t dtc_dict_entries[] PROGMEM = {
    {523700, 0, DTC_DSMEepromFailure_, 0, red_lamp_state_t::THREE_BLINKS}, // DTC_DSMEepromFailure (diagnostics EEPROM failure)

    {441, 0, DTC_SupplyLineRtdHighThres_, 5, red_lamp_state_t::FLASHING},   // DTC_SupplyLineRtdHighThres
    {441, 1, DTC_SupplyLineRtdLowThres_, 5, red_lamp_state_t::FLASHING},    // DTC_SupplyLineRtdLowThres
    {441, 3, DTC_SupplyLineRefInHigh_, 5, red_lamp_state_t::FLASHING},      // DTC_SupplyLineRefInHigh
    {441, 4, DTC_SupplyLineRefInLow_, 5, red_lamp_state_t::FLASHING},       // DTC_SupplyLineRefInLow
    {441, 15, DTC_SupplyLineRtdInLow_, 5, red_lamp_state_t::FLASHING},      // DTC_SupplyLineRtdInLow
    {441, 5, DTC_SupplyLineUOV_, 5, red_lamp_state_t::FLASHING},            // DTC_SupplyLineUOV
    {441, 23, DTC_SupplyLineDeviceError_, 0, red_lamp_state_t::FLICKERING}, // DTC_SupplyLineDeviceError

    {442, 0, DTC_ReturnLineRtdHighThres_, 5, red_lamp_state_t::FLASHING},   // DTC_ReturnLineRtdHighThres
    {442, 1, DTC_ReturnLineRtdLowThres_, 5, red_lamp_state_t::FLASHING},    // DTC_ReturnLineRtdLowThres
    {442, 3, DTC_ReturnLineRefInHigh_, 5, red_lamp_state_t::FLASHING},      // DTC_ReturnLineRefInHigh
    {442, 4, DTC_ReturnLineRefInLow_, 5, red_lamp_state_t::FLASHING},       // DTC_ReturnLineRefInLow
    {442, 15, DTC_ReturnLineRtdInLow_, 5, red_lamp_state_t::FLASHING},      // DTC_ReturnLineRtdInLow
    {442, 5, DTC_ReturnLineUOV_, 5, red_lamp_state_t::FLASHING},            // DTC_ReturnLineUOV
    {442, 23, DTC_ReturnLineDeviceError_, 0, red_lamp_state_t::FLICKERING}, // DTC_ReturnLineDeviceError

    {639, 12, DTC_CANBusOff_, 5, red_lamp_state_t::THREE_BLINKS},         // DTC_CANBusOff
    {639, 2, DTC_CANBusErrorPassive_, 5, red_lamp_state_t::THREE_BLINKS}, // DTC_CANBusErrorPassive
    {639, 23, DTC_CANBusDeviceError_, 0, red_lamp_state_t::FLICKERING},   // DTC_CANBusDeviceError

    // Add more DTC definitions as needed
};

/**
 * @brief Concrete implementation of DTC_Dict for TFM-100 firmware
 * @remarks Uses the dtc_dict_entries[] PROGMEM array defined in dtc.h
 */
class TFM100_DTC_Dict : public DTC_List
{
public:
    /**
     * @brief Constructor using the global DTC entries table
     */
    TFM100_DTC_Dict()
    {
        tbl_ = dtc_dict_entries;
        numberOfErrors_ = sizeof(dtc_dict_entries) / sizeof(dtc_entity_t);
    }

    /**
     * @brief Get SPN (Suspect Parameter Number) for a DTC entry
     * @param idx Index of the DTC entry
     * @param out_spn Output SPN value
     * @return true if index is valid and SPN retrieved successfully
     */
    bool getSPN(uint16_t idx, uint32_t &out_spn) const override
    {
        if (!idxOK(idx))
            return false;
        out_spn = pgm_read_dword(&tbl_[idx].spn);
        return true;
    }

    /**
     * @brief Get FMI (Failure Mode Identifier) for a DTC entry
     * @param idx Index of the DTC entry
     * @param out_fmi Output FMI value (masked to 5 bits)
     * @return true if index is valid and FMI retrieved successfully
     */
    bool getFMI(uint16_t idx, uint8_t &out_fmi) const override
    {
        if (!idxOK(idx))
            return false;
        out_fmi = pgm_read_byte(&tbl_[idx].fmi) & 0x1F;
        return true;
    }

    /**
     * @brief Get severity level for a DTC entry
     * @param idx Index of the DTC entry
     * @return Severity level, or 0xFF if index is invalid
     */
    uint8_t getSeverity(uint16_t idx) const override
    {
        if (!idxOK(idx))
            return 0xFF; // Invalid severity
        return pgm_read_byte(&tbl_[idx].severity);
    }

    /**
     * @brief Get human-readable name for a DTC entry
     * @param idx Index of the DTC entry
     * @return Pointer to string in RAM (copied from PROGMEM), or nullptr if index is invalid
     */
    const char *getName(uint16_t idx) const override
    {
        if (!idxOK(idx))
            return nullptr;

        // Read the pointer from PROGMEM struct
        const char *progmem_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&tbl_[idx].name));

        // Use static buffer to copy PROGMEM string to RAM
        static char buffer[64]; // Adjust size as needed
        strcpy_P(buffer, progmem_ptr);
        return buffer;
    }

    /**
     * @brief Get debounce ticks for a DTC entry
     * @param idx Index of the DTC entry
     * @return Debounce ticks, or 0xFF if index is invalid
     */
    uint8_t getDebounceTicks(uint16_t idx) const override
    {
        if (!idxOK(idx))
            return 0xFF;
        return pgm_read_byte(&tbl_[idx].debounce_ticks);
    }

    /**
     * @brief Get total number of DTC entries
     * @return Number of DTC entries in the dictionary
     */
    uint16_t getNumberOfErrors() const override
    {
        return numberOfErrors_;
    }

    /**
     * @brief Get magic number for EEPROM storage
     * @return Magic number
     */
    uint16_t getMagicNumber() const override
    {
        return MAGIC_NUMBER;
    }

private:
    const dtc_entity_t *tbl_; ///< Pointer to PROGMEM DTC table
    uint8_t numberOfErrors_;  ///< Number of entries in the table

    inline bool idxOK(uint16_t idx) const { return idx < numberOfErrors_; }
};
