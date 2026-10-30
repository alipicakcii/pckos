// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/kprint.hh>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <library/mem.hh>
#include <library/convert.hh>
#include <library/kernel_log.hh>

using namespace lib;

int __kputchar(int ic)
{
    kernel_log_write_char(static_cast<char>(ic));
    return ic;
}
static bool __kprint(const char *data, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < length; i++)
        if (__kputchar(bytes[i]) == EOF)
            return false;
    return true;
}

int __kprintf(const char *format, ...)
{
    va_list parameters;
    va_start(parameters, format);

    int written = 0;

    while (*format != '\0')
    {
        size_t maxrem = INT_MAX - written;

        if (format[0] != '%' || format[1] == '%')
        {
            if (format[0] == '%')
                format++;
            size_t amount = 1;
            while (format[amount] && format[amount] != '%')
                amount++;
            if (maxrem < amount)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(format, amount))
                return -1;
            format += amount;
            written += amount;
            continue;
        }

        const char *format_begun_at = format++;

        if (*format == 'c')
        {
            format++;
            char c = (char)va_arg(parameters, int /* char promotes to int */);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(&c, sizeof(c)))
                return -1;
            written++;
        }

        else if (*format == 's')
        {
            format++;
            const char *str = va_arg(parameters, const char *);
            size_t len = strlen(str);
            if (maxrem < len)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(str, len))
                return -1;
            written += len;
        }
        else if (*format == 'd')
        {
            format++;
            int c = (int)va_arg(parameters, int /* char promotes to int */);
            char buffer[32];
            signed_itoa(c, buffer, 10);
            size_t len = strlen(buffer);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(buffer, len))
                return -1;
            written += len;
        }
        else if (*format == 'x')
        {
            format++;
            unsigned int c = (unsigned int)va_arg(parameters, int /* char promotes to int */);
            char buffer[32];
            unsigned_itoa(c, buffer, 16);
            size_t len = strlen(buffer);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(buffer, len))
                return -1;
            written += len;
        }
        else if (*format == 'u')
        {
            format++;
            unsigned int c = (unsigned int)va_arg(parameters, int /* char promotes to int */);
            char buffer[32];
            unsigned_itoa(c, buffer, 10);
            size_t len = strlen(buffer);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(buffer, len))
                return -1;
            written += len;
        }
        else if (*format == 'b')
        {
            format++;
            unsigned int c = (unsigned int)va_arg(parameters, int /* char promotes to int */);
            char buffer[32];
            unsigned_itoa(c, buffer, 2);
            size_t len = strlen(buffer);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(buffer, len))
                return -1;
            written += len;
        }
        else if (*format == 'e')
        {
            format++;
            unsigned long long c = (unsigned long long)va_arg(parameters, unsigned long long /* char promotes to int */);
            char buffer[64];
            itoa_long_long(c, buffer, 16);
            size_t len = strlen(buffer);
            if (!maxrem)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(buffer, len))
                return -1;
            written += len;
        }
        else
        {
            format = format_begun_at;
            size_t len = strlen(format);
            if (maxrem < len)
            {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!__kprint(format, len))
                return -1;
            written += len;
            format += len;
        }
    }

    va_end(parameters);
    return written;
}
