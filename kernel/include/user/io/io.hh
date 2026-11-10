// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_IO_PRINT_HH
#define PCKOS_USER_IO_PRINT_HH

#include <user/io/stream.hh>
#include <user/task/thread.hh>
#include <stdarg.h>

namespace user::io
{
    // Kısa yazmaları tamamlar; kuyruk doluysa zamanlayıcıya fırsat verir.
    inline int32_t write_all(int descriptor, const void *buffer, uint32_t length)
    {
        const char *bytes = static_cast<const char *>(buffer);
        uint32_t sent = 0;
        while (sent < length)
        {
            const uint32_t chunk = length - sent > 4096 ? 4096 : length - sent;
            const int32_t result = user::write(descriptor, bytes + sent, chunk);
            if (result == -11) { user::thread_sleep(1); continue; }
            if (result <= 0) return sent != 0 ? static_cast<int32_t>(sent) : result;
            sent += static_cast<uint32_t>(result);
        }
        return static_cast<int32_t>(sent);
    }

    int32_t vprint(int descriptor, const char *format, va_list arguments);
    int32_t print(const char *format, ...) __attribute__((format(printf, 1, 2)));
    int32_t eprint(const char *format, ...) __attribute__((format(printf, 1, 2)));
    int32_t fprint(int descriptor, const char *format, ...) __attribute__((format(printf, 2, 3)));

    // Veri yokken bekler; EOF için 0, hata için negatif değer döndürür.
    inline int32_t read(void *buffer, uint32_t capacity)
    {
        for (;;)
        {
            const int32_t result = user::read(user::stdin_descriptor, buffer, capacity);
            if (result != -11) return result;
            user::thread_sleep(1);
        }
    }
}

using user::io::print;
using user::io::eprint;
using user::io::fprint;

#endif
