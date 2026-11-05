// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/syscall.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    static constexpr uint32_t melody[] = {
        523, 587, 659, 698, 784, 698, 659, 587, 523};

    for (uint32_t i = 0; i < sizeof(melody) / sizeof(melody[0]); ++i)
    {
        const int32_t result = user::pc_speaker_play(melody[i], 140);
        if (result < 0)
        {
            print("PC speaker cagrisi basarisiz: %d\n", static_cast<int>(result));
            user::exit(1);
        }
        user::thread_sleep(2);
    }

    print("PCKOS ses denemesi tamamlandi.\n");

    user::exit(0);
}
