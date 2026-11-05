// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/syscall.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{

   
    int *ptr = new int;
    *ptr = 0x12345678;


    print("value=0x%x addr=%p\n", *ptr, ptr);
    user::exit(1);
}
