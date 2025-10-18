/**
 * @file SerialProtocol.h
 * @brief Minimal line-based text protocol parser and sender (transport agnostic)
 * @author Generated
 * @note Ultra-light, no dynamic memory, no printf/snprintf, compatible with AVR/Arduino
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// If building for Arduino, offer a compile-time reminder to call proto.begin()
// so a sane default Serial writer is installed. Define SERIALPROTOCOL_NO_BEGIN_WARNING
// to suppress this reminder. When ARDUINO is not defined the class remains
// transport-agnostic and begin() is a no-op unless begin(w) is used.
#ifdef ARDUINO
#endif

class SerialProtocol
{
public:
    enum LogLevel : char
    {
        LOG_ERROR = 'E',
        LOG_WARNING = 'W',
        LOG_INFO = 'I',
        LOG_DEBUG = 'D'
    };

    enum CalibrationType : char
    {
        INTEGER = 'I',
        FLOAT = 'F',
        STRING = 'S',
    };

    // Callback types (more descriptive names)
    typedef void (*calibration_callback_t)(uint8_t id, const char *value, uint8_t vlen);
    typedef void (*read_callback_t)(uint8_t chip, uint32_t addr, uint8_t len);
    typedef void (*write_callback_t)(uint8_t chip, uint32_t addr, const uint8_t *data, uint8_t dlen);
    typedef void (*reboot_callback_t)(void);

    // Constructor
    SerialProtocol();

    // Feed raw incoming bytes (from Serial.read or equivalent)
    // This is safe to call from main loop; it accumulates until newline.
    void feed(const uint8_t *data, size_t len);

    // Send helpers (board -> host)
    void send_param(const char *key, uint32_t value);                                        // P;key;value\n
    void send_param(const char *key, int32_t value);                                         // P;key;value\n
    void send_param(const char *key, float value, uint8_t decimals);                         // P;key;value\n
    void send_param(const char *key, const char *value);                                     // P;key;value\n
    void send_log(LogLevel level, const char *message);                                      // L;level;message\n
    void send_diag_count(uint8_t count);                                                     // D;count
    void send_diag_info(uint8_t idx, uint32_t spn, uint8_t fmi, uint8_t status, uint8_t oc); // D;index;SPN-FMI;status;OC

    void send_calibration_status(uint8_t id, char type, uint8_t value);

    // send V;chip;addrhex;lenhex;datahex\n (datalen limited)
    void send_v_dump(uint8_t chip, uint32_t addr, const uint8_t *data, uint8_t len);

    // Set application callbacks for downlink commands (descriptive setter names)
    void set_calibration_callback(calibration_callback_t cb) { cb_calibration = cb; }
    void set_read_callback(read_callback_t cb) { cb_read = cb; }
    void set_write_callback(write_callback_t cb) { cb_write = cb; }
    void set_reboot_callback(reboot_callback_t cb) { cb_reboot = cb; }

    // Instance writer setter (public) - optional per-instance writer overrides weak hook
    typedef void (*raw_writer_t)(const uint8_t *data, size_t len);
    void set_writer(raw_writer_t w);

#ifdef ARDUINO
    // Default begin() installs a Serial writer
    void begin();
#endif
    // Custom begin() with explicit writer
    void begin(raw_writer_t w);

private:
    // RX buffer
    static const size_t RX_BUFFER_SIZE = 128;
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t rx_index;

    // Temporary work buffers
    static const size_t LINE_BUFFER_SIZE = 128;
    char line_buffer[LINE_BUFFER_SIZE];

    // Max data bytes for W/V commands
    static const size_t DATA_MAX_BYTES = 48;
    uint8_t data_buffer[DATA_MAX_BYTES];

    // Callbacks
    calibration_callback_t cb_calibration;
    read_callback_t cb_read;
    write_callback_t cb_write;
    reboot_callback_t cb_reboot;

    // Internal helpers
    void process_line(const char *line, size_t len);
    static uint8_t hex_char_val(char c); // returns 0xFF on invalid
    static bool parse_hex_bytes(const char *s, size_t slen, uint8_t *out, size_t max_out, size_t *out_len);
    static uint32_t parse_hex_u32(const char *s, size_t slen);
    static uint8_t parse_dec_u8(const char *s, size_t slen);
    static size_t copy_cstr_to_buf(const char *s, size_t slen, char *out, size_t max_out);
    // Hybrid writer: instance-level writer overrides weak hook when set
    void emit(const char *s, size_t len);

    // Instance writer (optional)
    raw_writer_t writer;
};
