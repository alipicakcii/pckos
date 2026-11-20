// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/syscall/call.hh>

namespace user
{
    int32_t call(uint32_t number, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2, uint32_t arg3, uint32_t arg4)
    {
        register uint32_t result __asm__("eax") = number;
        register uint32_t first __asm__("ebx") = arg0;
        register uint32_t second __asm__("ecx") = arg1;
        register uint32_t third __asm__("edx") = arg2;
        register uint32_t fourth __asm__("esi") = arg3;
        register uint32_t fifth __asm__("edi") = arg4;
        __asm__ volatile("int $0x80"
                         : "+a"(result)
                         : "b"(first), "c"(second), "d"(third),
                           "S"(fourth), "D"(fifth)
                         : "memory", "cc");
        return static_cast<int32_t>(result);
    }
}
