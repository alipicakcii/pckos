// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/io.hh>
#include <user/lib/io.hh>
#include <stddef.h>
#include <limits.h>

namespace user::io
{
    namespace
    {
        struct Output
        {
            int descriptor;
            char buffer[128]{};
            uint32_t used = 0;
            int32_t total = 0;
            int32_t error = 0;

            void flush()
            {
                if (error != 0 || used == 0) return;
                const int32_t result = write_all(descriptor, buffer, used);
                if (result < 0) error = result;
                else if (static_cast<uint32_t>(result) != used) error = -5;
                used = 0;
            }

            void put(char c)
            {
                if (error != 0) return;
                if (total == INT_MAX) { error = -75; return; }
                buffer[used++] = c;
                ++total;
                if (used == sizeof(buffer)) flush();
            }

            void repeat(char c, uint32_t count)
            {
                while (count-- != 0 && error == 0) put(c);
            }
        };
    }

    int32_t vprint(int descriptor, const char *format, va_list arguments)
    {
        if (format == nullptr) return -14;
        if (descriptor != user::stdout_descriptor && descriptor != user::stderr_descriptor)
            return -9;
        Output out{descriptor};
        while (*format != '\0' && out.error == 0)
        {
            if (*format != '%') { out.put(*format++); continue; }
            ++format;
            if (*format == '%') { out.put(*format++); continue; }
            bool left = false, zero = false, plus = false, space = false, alternate = false;
            for (;; ++format)
            {
                if (*format == '-') left = true;
                else if (*format == '0') zero = true;
                else if (*format == '+') plus = true;
                else if (*format == ' ') space = true;
                else if (*format == '#') alternate = true;
                else break;
            }
            uint32_t width = 0;
            if (*format == '*')
            {
                const int value = va_arg(arguments, int);
                left = left || value < 0;
                width = value < 0 ? 0U - static_cast<uint32_t>(value) : value;
                ++format;
            }
            else while (*format >= '0' && *format <= '9')
            {
                if (width > 4096) return -22;
                width = width * 10 + static_cast<uint32_t>(*format++ - '0');
            }
            int precision = -1;
            if (*format == '.')
            {
                ++format;
                precision = 0;
                if (*format == '*') { precision = va_arg(arguments, int); ++format; }
                else while (*format >= '0' && *format <= '9')
                {
                    if (precision > 4096) return -22;
                    precision = precision * 10 + *format++ - '0';
                }
            }
            if (width > 4096 || precision > 4096) return -22;
            char modifier = 0;
            bool small = false;
            if (*format == 'l' || *format == 'z' || *format == 'h')
            {
                modifier = *format++;
                if (modifier == 'h' && *format == 'h') { small = true; ++format; }
                if (modifier == 'l' && *format == 'l') return -22;
            }
            const char conversion = *format;
            if (conversion == '\0') return -22;
            ++format;
            if (conversion == 's' || conversion == 'c')
            {
                if (modifier != 0) return -22;
                char character = 0;
                const char *text;
                uint32_t size = 0;
                if (conversion == 'c')
                {
                    character = static_cast<char>(va_arg(arguments, int));
                    text = &character;
                    size = 1;
                }
                else
                {
                    text = va_arg(arguments, const char *);
                    if (text == nullptr) text = "(null)";
                    while ((precision < 0 || size < static_cast<uint32_t>(precision)) &&
                           text[size] != '\0') ++size;
                }
                const uint32_t padding = width > size ? width - size : 0;
                if (!left) out.repeat(' ', padding);
                for (uint32_t i = 0; i < size && out.error == 0; ++i) out.put(text[i]);
                if (left) out.repeat(' ', padding);
                continue;
            }
            const bool signed_value = conversion == 'd' || conversion == 'i';
            const bool pointer = conversion == 'p';
            if (!signed_value && !pointer && conversion != 'u' && conversion != 'x' &&
                conversion != 'X' && conversion != 'o') return -22;
            if (pointer && modifier != 0) return -22;
            uintptr_t value;
            bool negative = false;
            if (pointer) value = reinterpret_cast<uintptr_t>(va_arg(arguments, void *));
            else if (signed_value)
            {
                intptr_t number = modifier == 'l' ? va_arg(arguments, long) :
                    modifier == 'z' ? va_arg(arguments, ptrdiff_t) : va_arg(arguments, int);
                if (modifier == 'h') number = small ? static_cast<signed char>(number) :
                                                     static_cast<short>(number);
                negative = number < 0;
                value = negative ? uintptr_t(0) - static_cast<uintptr_t>(number) : number;
            }
            else
            {
                value = modifier == 'l' ? va_arg(arguments, unsigned long) :
                    modifier == 'z' ? va_arg(arguments, size_t) : va_arg(arguments, unsigned int);
                if (modifier == 'h') value = small ? static_cast<unsigned char>(value) :
                                                    static_cast<unsigned short>(value);
            }
            const uint32_t base = pointer || conversion == 'x' || conversion == 'X' ? 16 :
                                  conversion == 'o' ? 8 : 10;
            const char *digits = conversion == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char reverse[sizeof(uintptr_t) * 8];
            uint32_t count = 0;
            const bool nonzero = value != 0;
            if (nonzero || precision != 0 || pointer)
                do { reverse[count++] = digits[value % base]; value /= base; } while (value != 0);
            char prefix[2];
            uint32_t prefix_size = 0;
            if (signed_value)
            {
                if (negative) prefix[prefix_size++] = '-';
                else if (plus) prefix[prefix_size++] = '+';
                else if (space) prefix[prefix_size++] = ' ';
            }
            else if (pointer || (alternate && nonzero && base == 16))
            {
                prefix[prefix_size++] = '0';
                prefix[prefix_size++] = conversion == 'X' ? 'X' : 'x';
            }
            uint32_t zeros = precision > static_cast<int>(count) ? precision - count : 0;
            if (alternate && base == 8 && zeros == 0 &&
                (count == 0 || reverse[count - 1] != '0')) zeros = 1;
            const uint32_t size = prefix_size + zeros + count;
            uint32_t padding = width > size ? width - size : 0;
            if (zero && !left && precision < 0) { zeros += padding; padding = 0; }
            if (!left) out.repeat(' ', padding);
            for (uint32_t i = 0; i < prefix_size; ++i) out.put(prefix[i]);
            out.repeat('0', zeros);
            while (count != 0) out.put(reverse[--count]);
            if (left) out.repeat(' ', padding);
        }
        out.flush();
        return out.error != 0 ? out.error : out.total;
    }

    int32_t print(const char *format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        const int32_t result = vprint(user::stdout_descriptor, format, arguments);
        va_end(arguments);
        return result;
    }

    int32_t eprint(const char *format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        const int32_t result = vprint(user::stderr_descriptor, format, arguments);
        va_end(arguments);
        return result;
    }

    int32_t fprint(int descriptor, const char *format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        const int32_t result = vprint(descriptor, format, arguments);
        va_end(arguments);
        return result;
    }
}
