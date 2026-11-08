// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/lib/io.hh> 
#include <user/thread.hh>

extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{
    for (int i = 0; i < 100; i++)
    {
        int *ptr = new int;
        *ptr = i;
        print("val=%d addr=%p\n", *ptr, ptr);
        delete ptr;
        user::thread_sleep(10);
    }
    
}
