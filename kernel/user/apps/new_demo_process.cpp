// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/syscall.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{

    constexpr size_t BATCH_SIZE = 1023;

    int **ptrs = new int *[BATCH_SIZE];
    size_t used = 0;

    for (size_t i = 0; i < 50000; ++i)
    {
        int *ptr = new int;
        *ptr = static_cast<int>(i);
        ptrs[used++] = ptr;

        print("val=%d addr=%p\n", *ptr, ptr);

        if (used == BATCH_SIZE)
        {
            for (size_t x = 0; x < used; ++x)
            {
                delete ptrs[x];
                ptrs[x] = nullptr;
            }

            used = 0;
            print("Bellek grubu temizlendi\n");
     
        }
    }

    for (size_t x = 0; x < used; ++x)
        delete ptrs[x];

    delete[] ptrs;
    user::exit(0);
}
