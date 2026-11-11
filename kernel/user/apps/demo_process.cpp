// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/io/io.hh>
#include <user/task/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system/info.hh>
#include <user/task/process.hh>
#include <user/io/serial_dev.hh>

void work(void *)
{
    print("tid=%d\n", user::thread_id());
     user::thread_sleep(100);
}

extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{

    for (int i = 0; i < 100; i++)
    {
        const int32_t a = user::thread_create(work, nullptr, "Sayac1");
        const int32_t b = user::thread_create(work, nullptr, "Sayac2");
       user::thread_sleep(50);
        print("id=%d siradaki=%d\n", user::thread_id(),i);
    }

    user::exit(0);
}
