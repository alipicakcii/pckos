// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/audio.hh>
#include <user/process.hh>
#include <user/thread.hh>
#include <user/lib/new.hh>
#include <user/lib/io.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

// farklı thread sanal adresleriini deneme
// aynı sanal adreslere işaret etmesi önemli deil
// cr3'leri farklı sayfa dizinlerine işaret edior:D


void work(void *)
{
    int *b =  new int;
    print("b_ptr=%p tid=%d\n", b, user::thread_id());
  
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{

    int *a =  new int;
    print("a_ptr=%p tid=%d\n", a, user::thread_id());

    auto id =user::thread_create(&work, nullptr, "Test Thread");

    user::thread_sleep(50);


    
    
    user::exit(0);
}
