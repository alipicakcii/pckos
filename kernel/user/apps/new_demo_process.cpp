// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/syscall.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    // thread id ve sleep demosu
    for (size_t i = 0; i < 50; i++)
    {
        auto id = user::thread_id();
        print("val=%d id=%d\n",i++, id);
        user::thread_sleep(5);
    }
    
    user::exit(0);
}
