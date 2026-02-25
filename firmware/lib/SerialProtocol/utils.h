/**
 * @file utils.h
 * @brief Very small, dependency-free helpers to format small strings without pulling full printf.
 * @author helper
 * @note Keep implementations to allow compiler to optimize and avoid extra code size.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Format an unsigned long as "0x" followed by 6 uppercase hex digits into dst.
 * @param dst Output buffer (must have space for at least 9 chars: "0x" + 6 + '\0')
 * @param value Unsigned long value to format
 * @return None
 */
static inline void mini_hex6(char *dst, unsigned long value)
{
    const char hex[] = "0123456789ABCDEF";
    dst[0] = '0';
    dst[1] = 'x';
    for (int i = 0; i < 6; ++i)
    {
        unsigned int shift = (5 - i) * 4;
        unsigned int nibble = (value >> shift) & 0x0F;
        dst[2 + i] = hex[nibble];
    }
    dst[8] = '\0';
}

/**
 * @brief Format an unsigned integer into dst, padded left to width (width <= 10).
 * @param dst Output buffer (must have at least width+1 bytes)
 * @param value Unsigned long value to format
 * @param width Minimum width (padded with spaces on the left)
 * @return None
 * @remarks If width is small and value larger, it prints full value.
 */
static inline void mini_udec_padded(char *dst, unsigned long value, uint8_t width)
{
    char tmp[12];
    int len = 0;
    if (value == 0)
    {
        tmp[len++] = '0';
    }
    else
    {
        unsigned long v = value;
        while (v > 0 && len < (int)(sizeof(tmp) - 1))
        {
            tmp[len++] = '0' + (v % 10);
            v /= 10;
        }
        for (int i = 0; i < len / 2; ++i)
        {
            char c = tmp[i];
            tmp[i] = tmp[len - 1 - i];
            tmp[len - 1 - i] = c;
        }
    }
    int pad = (int)width - len;
    char *p = dst;
    while (pad-- > 0)
        *p++ = ' ';
    for (int i = 0; i < len; ++i)
        *p++ = tmp[i];
    *p = '\0';
}

/**
 * @brief Converts a float to string with specified decimals, minimal RAM usage.
 * @param buf Output buffer (must be large enough)
 * @param f Float value to convert
 * @param decimals Number of decimal places
 * @return None
 * @remarks Uses integer math, no heap, buffer must be at least 12 bytes for 2 decimals.
 */
static inline void ftostr(char *buf, float f, uint8_t decimals)
{
    int32_t mult = 1;
    for (int i = 0; i < decimals; ++i)
        mult *= 10;
    int32_t value = (int32_t)(f * mult + (f < 0 ? -0.5f : 0.5f));
    int32_t int_part = value / mult;
    int32_t frac_part = abs(value % mult);

    // Print integer part
    char *p = buf;
    if (int_part < 0)
    {
        *p++ = '-';
        int_part = -int_part;
    }
    itoa(int_part, p, 10);
    while (*p)
        ++p;

    // Print decimal part
    if (decimals > 0)
    {
        *p++ = '.';
        int pow10 = mult / 10;
        for (int i = 0; i < decimals; ++i)
        {
            *p++ = '0' + (frac_part / pow10) % 10;
            pow10 /= 10;
        }
    }
    *p = '\0';
}