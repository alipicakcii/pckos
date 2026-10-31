// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/io.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    int *ptr = new int;
    print("[new-demo] ptr = %p\n", static_cast<void *>(ptr));
    delete ptr;
    print("[new-demo] delete tamamlandi\n");
    user::exit(0);
}
