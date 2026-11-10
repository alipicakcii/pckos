// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/lib/io.hh> 
#include <user/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system.hh>
#include <user/process.hh>


extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{

    user::lib::random::Random random;
    for (int i = 0; i < 50; i++)
    {
        uint32_t rnd = random.next(100, user::lib::random::Source::software);
        print("sira=%d rnd=%u\n", i, static_cast<unsigned int>(rnd));
       // user::thread_sleep(5);
        
    }

    
       
    user::exit(0);
}
