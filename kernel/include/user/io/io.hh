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
int32_t write_all(int descriptor, const void *buffer, uint32_t length);

    int32_t vprint(int descriptor, const char *format, va_list arguments);
    int32_t print(const char *format, ...) __attribute__((format(printf, 1, 2)));
    int32_t eprint(const char *format, ...) __attribute__((format(printf, 1, 2)));
    int32_t fprint(int descriptor, const char *format, ...) __attribute__((format(printf, 2, 3)));

    // Veri yokken bekler; EOF için 0, hata için negatif değer döndürür.
int32_t read(void *buffer, uint32_t capacity);
}

using user::io::print;
using user::io::eprint;
using user::io::fprint;

#endif
