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

   
   
    const int32_t deneme_pid = user::process_spawn("/apps/calculator.bin", "Deneme");

    
    print("Deneme PID=%d\n", deneme_pid);



    user::exit(0);
}
