/**
 * @file SerialProtocol.cpp
 * @brief Implementation of the minimal line-based protocol parser and sender.
 */

#include "SerialProtocol.h"
#include <string.h>
#include "utils.h"
// Arduino Serial is used only by default begin() writer; guard so the library
// remains framework-agnostic when ARDUINO is not defined.
#ifdef ARDUINO
#include <Arduino.h>
#endif

SerialProtocol::SerialProtocol()
    : rx_index(0), cb_calibration(NULL), cb_read(NULL), cb_write(NULL), cb_reboot(NULL), cb_erase_diagnostics(NULL), writer(NULL)
{
}

void SerialProtocol::set_writer(raw_writer_t w)
{
    writer = w;
}

// Default serial writer used by begin() when building for Arduino
#ifdef ARDUINO
void SerialProtocol::begin()
{
    // install default serial writer: use a non-capturing lambda that forwards to the global Serial.write.
    // A non-capturing lambda can be converted to a plain function pointer compatible with raw_writer_t.
    writer = +[](const uint8_t *buf, size_t len)
    {
        Serial.write(buf, len);
    };
}
#endif

void SerialProtocol::begin(raw_writer_t w)
{
    writer = w;
}

void SerialProtocol::feed(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t c = data[i];
        // Accept CR LF as newline; convert CR to LF and ignore duplicate
        if (c == '\r')
            continue;

        if (c == '\n')
        {
            // process
            size_t l = rx_index;
            if (l > 0 && l < LINE_BUFFER_SIZE)
            {
                // copy to line buffer and NUL-terminate
                for (size_t j = 0; j < l; j++)
                    line_buffer[j] = (char)rx_buffer[j];
                line_buffer[l] = '\0';
                process_line(line_buffer, l);
            }
            rx_index = 0;
            continue;
        }

        // store if space
        if (rx_index < RX_BUFFER_SIZE)
            rx_buffer[rx_index++] = c;
        else
            rx_index = 0; // overflow: reset buffer
    }
}

void SerialProtocol::emit(const char *s, size_t len)
{
    if (!s || len == 0)
        return;
    if (writer)
    {
        writer((const uint8_t *)s, len);
        return;
    }
}

// Process a full line (without trailing newline)
void SerialProtocol::process_line(const char *line, size_t len)
{
    if (len == 0)
        return;
    // tokenization on ';' - minimal, avoid strtok
    const char *p = line;
    const char *end = line + len;
    // find first token
    const char *t0 = p;
    const char *q = t0;
    while (q < end && *q != ';')
        q++;
    size_t t0len = (size_t)(q - t0);

    if (t0len == 0)
        return;

    char cmd = t0[0];

    // advance p after separator if any
    if (q < end && *q == ';')
        q++;

    // Helper to read next token
    auto next_token = [&](const char *&out_s, size_t &out_len)
    {
        if (q >= end)
        {
            out_s = NULL;
            out_len = 0;
            return;
        }
        const char *s = q;
        const char *r = s;
        while (r < end && *r != ';')
            r++;
        out_s = s;
        out_len = (size_t)(r - s);
        if (r < end && *r == ';')
            r++;
        q = r;
    };

    // parse commands
    if (cmd == 'C')
    {
        // C;id;value
        const char *s1;
        size_t l1;
        const char *s2;
        size_t l2;
        next_token(s1, l1);
        next_token(s2, l2);
        if (s1 && l1 > 0)
        {
            uint8_t id = parse_dec_u8(s1, l1);
            if (cb_calibration)
            {
                // Ensure value is NUL-terminated in temporary buffer
                size_t n = copy_cstr_to_buf(s2 ? s2 : "", s2 ? l2 : 0, line_buffer, LINE_BUFFER_SIZE);
                cb_calibration(id, line_buffer, (uint8_t)n);
            }
        }
        return;
    }

    if (cmd == 'R')
    {
        // R;chip;addrhex;lenhex
        const char *s1;
        size_t l1;
        const char *s2;
        size_t l2;
        const char *s3;
        size_t l3;
        next_token(s1, l1);
        next_token(s2, l2);
        next_token(s3, l3);
        if (s1 && l1 > 0 && s2 && l2 > 0 && s3 && l3 > 0)
        {
            uint8_t chip = parse_dec_u8(s1, l1);
            uint32_t addr = parse_hex_u32(s2, l2);
            uint8_t lenv = parse_dec_u8(s3, l3);
            if (cb_read)
                cb_read(chip, addr, lenv);
        }
        return;
    }

    if (cmd == 'W')
    {
        // W;chip;addrhex;datahex
        const char *s1;
        size_t l1;
        const char *s2;
        size_t l2;
        const char *s3;
        size_t l3;
        next_token(s1, l1);
        next_token(s2, l2);
        next_token(s3, l3);
        if (s1 && l1 > 0 && s2 && l2 > 0 && s3 && l3 > 0)
        {
            uint8_t chip = parse_dec_u8(s1, l1);
            uint32_t addr = parse_hex_u32(s2, l2);
            size_t dlen = 0;
            if (l3 / 2 > DATA_MAX_BYTES) // too large
                return;
            if (!parse_hex_bytes(s3, l3, data_buffer, DATA_MAX_BYTES, &dlen))
                return;
            if (cb_write)
                cb_write(chip, addr, data_buffer, (uint8_t)dlen);
        }
        return;
    }

    if (cmd == 'B')
    {
        if (cb_reboot)
            cb_reboot();
        return;
    }

    if (cmd == 'E')
    {
        // E - Erase diagnostics memory. No parameters.
        if (cb_erase_diagnostics)
        {
            cb_erase_diagnostics();
            send_response('E', true);
        }
        else
            send_response('E', false, 0xFF);
        return;
    }

    // Unknown command: ignore
}

// Send Param
void SerialProtocol::send_param(const char *key, uint32_t value)
{
    // Format: P;key;value\n
    // compute lengths
    char numbuf[11]; // max 10 digits + NUL
    itoa(value, numbuf, 10);
    return send_param(key, numbuf);
}

void SerialProtocol::send_param(const char *key, int32_t value)
{
    // Format: P;key;value\n
    // compute lengths
    char numbuf[12]; // max 11 digits + NUL
    itoa(value, numbuf, 10);
    return send_param(key, numbuf);
}

void SerialProtocol::send_param(const char *key, float value, uint8_t decimals)
{
    // Format: P;key;value\n
    // Use snprintf to format float with specified decimals
    char numbuf[32];
    ftostr(numbuf, value, decimals);
    return send_param(key, numbuf);
}

void SerialProtocol::send_param(const char *key, const char *value)
{
    // Format: P;key;value\n
    size_t total = 1 + 1 + strlen(key) + 1 + strlen(value) + 1; // 5 + k + v
    if (total >= LINE_BUFFER_SIZE)
        return;
    strcpy(line_buffer, "P;");
    strcat(line_buffer, key);
    strcat(line_buffer, ";");
    strcat(line_buffer, value);
    strcat(line_buffer, "\n\r");
    emit(line_buffer, strlen(line_buffer));
}

// Send Log
void SerialProtocol::send_log(LogLevel level, const char *message)
{
    // L;level;message\n
    size_t total = 1 + 1 + 1 + 1 + strlen(message) + 1; // 5 + m
    if (total >= LINE_BUFFER_SIZE)
        return;
    strcpy(line_buffer, "L;x;");
    line_buffer[2] = (char)level;
    strcat(line_buffer, message);
    strcat(line_buffer, "\n\r");
    emit(line_buffer, strlen(line_buffer));
}

// Send Diagnostics
void SerialProtocol::send_diag_count(uint8_t count)
{
    // index decimal
    char numbuf[4];
    itoa(count, numbuf, 10);

    strcpy(line_buffer, "D;");
    strcat(line_buffer, numbuf);
    strcat(line_buffer, "\n\r");
    emit(line_buffer, strlen(line_buffer));
}

void SerialProtocol::send_diag_info(uint8_t idx, const char *description, uint32_t spn, uint8_t fmi, const char *status, uint8_t oc)
{
    // D;index;Description;SPN-FMI;status;OC
    char numbuf[9]; // 8 HEX Digit + NUL
    size_t total = 1 + 1 + 3 + 1 + strlen(description) + 1 + 8 + 1 + 2 + 1 + 2 + 1 + 3 + 1;
    if (total >= LINE_BUFFER_SIZE)
        return;
    strcpy(line_buffer, "D;");
    // INDEX
    itoa(idx, numbuf, 10);
    strcat(line_buffer, numbuf);
    strcat(line_buffer, ";");

    // DESCRIPTION
    strcat(line_buffer, description);
    strcat(line_buffer, ";");

    // SPN-FMI
    itoa(spn, numbuf, 16);
    strcat(line_buffer, numbuf);
    strcat(line_buffer, "-");
    itoa(fmi, numbuf, 16);
    strcat(line_buffer, numbuf);
    strcat(line_buffer, ";");

    // status
    strcat(line_buffer, status);
    strcat(line_buffer, ";");

    // oc
    itoa(oc, numbuf, 10);
    strcat(line_buffer, numbuf);
    strcat(line_buffer, "\n\r");
    emit(line_buffer, strlen(line_buffer));
}

// Send Calibration Status
void SerialProtocol::send_calibration_status(uint8_t id, char type, uint8_t value)
{
    // Format: C;id;type;value\n
    char numbuf[10];
    itoa(value, numbuf, 10);

    strcpy(line_buffer, "C;");
    strcat(line_buffer, numbuf);
    strcat(line_buffer, "\n\r");
    emit(line_buffer, strlen(line_buffer));
}

// Helpers for parsing
uint8_t SerialProtocol::hex_char_val(char c)
{
    if (c >= '0' && c <= '9')
        return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F')
        return (uint8_t)(10 + (c - 'A'));
    if (c >= 'a' && c <= 'f')
        return (uint8_t)(10 + (c - 'a'));
    return 0xFF;
}

bool SerialProtocol::parse_hex_bytes(const char *s, size_t slen, uint8_t *out, size_t max_out, size_t *out_len)
{
    // Expect even number of hex chars
    if ((slen & 1) != 0)
        return false;
    size_t bytes = slen / 2;
    if (bytes > max_out)
        return false;
    for (size_t i = 0; i < bytes; i++)
    {
        uint8_t hi = hex_char_val(s[2 * i]);
        uint8_t lo = hex_char_val(s[2 * i + 1]);
        if (hi == 0xFF || lo == 0xFF)
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = bytes;
    return true;
}

uint32_t SerialProtocol::parse_hex_u32(const char *s, size_t slen)
{
    uint32_t v = 0;
    for (size_t i = 0; i < slen; i++)
    {
        uint8_t h = hex_char_val(s[i]);
        if (h == 0xFF)
            break;
        v = (v << 4) | h;
    }
    return v;
}

uint8_t SerialProtocol::parse_dec_u8(const char *s, size_t slen)
{
    uint8_t v = 0;
    for (size_t i = 0; i < slen; i++)
    {
        char c = s[i];
        if (c < '0' || c > '9')
            break;
        v = (uint8_t)(v * 10 + (c - '0'));
    }
    return v;
}

size_t SerialProtocol::copy_cstr_to_buf(const char *s, size_t slen, char *out, size_t max_out)
{
    size_t n = slen < (max_out - 1) ? slen : (max_out - 1);
    for (size_t i = 0; i < n; i++)
        out[i] = s[i];
    out[n] = '\0';
    return n;
}

// Hex encode helper
static inline char to_hex(uint8_t v)
{
    v &= 0x0F;
    if (v < 10)
        return (char)('0' + v);
    return (char)('A' + (v - 10));
}

void SerialProtocol::send_v_dump(uint8_t chip, uint32_t addr, const uint8_t *data, uint8_t len)
{
    // V;chip;addrhex;lenhex;datahex\n
    // chip decimal small
    char chnum[4];
    int cni = 0;
    uint8_t cv = chip;
    if (cv == 0)
        chnum[cni++] = '0';
    else
    {
        char tmp[3];
        int ti = 0;
        while (cv > 0 && ti < 3)
        {
            tmp[ti++] = (char)('0' + (cv % 10));
            cv /= 10;
        }
        while (ti--)
            chnum[cni++] = tmp[ti];
    }

    // addrhex - up to 8 hex digits
    char addrhex[9];
    int ah = 0;
    uint32_t a = addr;
    // produce at least 1 digit
    char tmpa[8];
    int tai = 0;
    if (a == 0)
        tmpa[tai++] = '0';
    else
    {
        while (a != 0 && tai < 8)
        {
            uint8_t nib = (uint8_t)(a & 0xF);
            tmpa[tai++] = to_hex(nib);
            a >>= 4;
        }
    }
    while (tai--)
        addrhex[ah++] = tmpa[tai];
    addrhex[ah] = '\0';

    // lenhex (1 byte) two hex digits
    char lenhex[3];
    lenhex[0] = to_hex((uint8_t)(len >> 4));
    lenhex[1] = to_hex((uint8_t)(len & 0xF));
    lenhex[2] = '\0';

    // datahex
    size_t dh = (size_t)len * 2;
    // conservative required size: 'V;'+chip+';'+addrhex+';'+lenhex(2)+';'+datahex(dh)+'\n'
    size_t required = 1 + 1 + cni + 1 + ah + 1 + 2 + 1 + dh + 1;
    if (required >= LINE_BUFFER_SIZE)
        return; // avoid overflow

    size_t idx = 0;
    line_buffer[idx++] = 'V';
    line_buffer[idx++] = ';';
    for (int i = 0; i < cni; i++)
    {
        line_buffer[idx++] = chnum[i];
    }
    line_buffer[idx++] = ';';
    for (int i = 0; i < ah; i++)
    {
        line_buffer[idx++] = addrhex[i];
    }
    line_buffer[idx++] = ';';
    line_buffer[idx++] = lenhex[0];
    line_buffer[idx++] = lenhex[1];
    line_buffer[idx++] = ';';
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        line_buffer[idx++] = to_hex((uint8_t)(b >> 4));
        line_buffer[idx++] = to_hex((uint8_t)(b & 0xF));
    }
    line_buffer[idx++] = '\n';
    emit(line_buffer, idx);
}

void SerialProtocol::send_response(char cmd, bool success, uint8_t error_code)
{
    // Format: cmd;OK\n\r or cmd;ERR;error_code\n\r
    size_t idx = 0;
    line_buffer[idx++] = cmd;
    line_buffer[idx++] = ';';

    if (success)
    {
        line_buffer[idx++] = 'O';
        line_buffer[idx++] = 'K';
        line_buffer[idx++] = '\n';
        line_buffer[idx++] = '\r';
        emit(line_buffer, idx);
    }
    else
    {
        line_buffer[idx++] = 'E';
        line_buffer[idx++] = 'R';
        line_buffer[idx++] = 'R';
        line_buffer[idx++] = ';';
        // Append error code as decimal
        char numbuf[4];
        itoa(error_code, numbuf, 10);
        size_t i = 0;
        while (numbuf[i] && idx < (LINE_BUFFER_SIZE - 3))
        {
            line_buffer[idx++] = numbuf[i++];
        }
        line_buffer[idx++] = '\n';
        line_buffer[idx++] = '\r';
        emit(line_buffer, idx);
    }
}
