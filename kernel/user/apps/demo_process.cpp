// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/io/io.hh>
#include <user/task/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system/info.hh>
#include <user/task/process.hh>
#include <user/io/serial_dev.hh>



void work(void*)
{
    print("work_tid=%d\n", user::thread_id());
        user::thread_sleep(8000);
}




extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{

    for (int i = 0; i < 500; i++)
    {
        auto tid = user::thread_create(work, nullptr, "Deneme");
        print("p_tid=%d tid=%d next=%d\n", user::thread_id(), tid, i);
    }

    user::exit(0);
}
