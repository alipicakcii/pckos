// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/io/io.hh>
#include <user/task/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system/info.hh>
#include <user/task/process.hh>
#include <user/io/serial_dev.hh>
#include <user/device/audio.hh>


// çekirdek ring1 yetkileri ile 
// cr3 ookuma gibi kodlarda ring3 dofgru tepki veriyormu güvenlik olarak
// test etmek icin
void work(void*)
{
   uint64_t cr3;

    // Ring 3'te #GP(0) hatası oluşturur.
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
        );
}




extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{
   

    print("start\n");
    user::io::serialdev::write("merhaba serial port");
 
    print("end\n");

    user::exit(0);
}
