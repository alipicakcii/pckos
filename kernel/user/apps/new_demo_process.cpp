// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/syscall.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    int *ptr = new int;

    char message[] = "[new-demo] ptr = 0x00000000\n";
    const char *digits = "0123456789ABCDEF";
    const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    for (uint32_t i = 0; i < 8; ++i)
        message[19 + i] = digits[(address >> ((7 - i) * 4)) & 0xF];
    user::write(user::stdout_descriptor, message, sizeof(message) - 1);

    delete ptr;
    constexpr char done[] = "[new-demo] delete tamamlandi\n";
    user::write(user::stdout_descriptor, done, sizeof(done) - 1);
    user::exit(0);
}
