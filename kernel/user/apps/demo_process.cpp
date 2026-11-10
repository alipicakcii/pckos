// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/io/io.hh> 
#include <user/task/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system/info.hh>
#include <user/task/process.hh>


extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{

    int *ptr = new int;
    *ptr = 123456789;
    uint32_t addr= (uint32_t)ptr;
    print("val=%d addr=%p\n", *ptr, addr);
    delete ptr;
    print("delete=%p\n", addr);
    
   
    auto pid = user::process_spawn("/apps/hello_word.bin", "Deneme");

    
    print("pid=%d\n", pid);

    user::thread_sleep(500);

    user::exit(0);
}
