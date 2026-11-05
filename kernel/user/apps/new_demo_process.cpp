// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/audio.hh>
#include <user/process.hh>
#include <user/thread.hh>
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{

    // pc_speaker sürücü testi
    // QEMU'da sorunsuz calısdı
    // virtualbox da denenmedi 
    
    uint32_t r =     user::pc_speaker_play(1000, 30);
    print("PCKOS ses denemesi tamamlandi=%d.\n", r);

    user::exit(0);
}
