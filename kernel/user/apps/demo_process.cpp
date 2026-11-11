// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#include <user/io/io.hh> 
#include <user/task/thread.hh>
#include <user/lib/rnd.hh>
#include <user/system/info.hh>
#include <user/task/process.hh>
#include <user/io/serial_dev.hh>


extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{

    auto result =user::io::serialdev::write("deneme deneme deneme deneme deneme deneme pckos pckospckos");
    if (result < 0){
        print("serial porta yazılamadı\n");
    }else print("basari ile seri porta yazildi\n");

    user::exit(0);
}
