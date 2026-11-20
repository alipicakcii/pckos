// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SERIAL_DEV_HH
#define PCKOS_USER_SERIAL_DEV_HH

#include <user/syscall/call.hh>

namespace user::io::serialdev
{
   

int32_t write(const char *string);

    
}

#endif
