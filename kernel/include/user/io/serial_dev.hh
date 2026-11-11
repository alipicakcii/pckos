// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SERIAL_DEV_HH
#define PCKOS_USER_SERIAL_DEV_HH

#include <user/syscall/call.hh>

namespace user::io::serialdev
{
   

    inline int32_t write(const char *string)
    {
        return call(sysycall_serial_dev_write,  reinterpret_cast<uint32_t>(string));
    }

    
}

#endif
