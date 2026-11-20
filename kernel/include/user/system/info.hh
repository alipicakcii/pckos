// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SYSTEM_HH
#define PCKOS_USER_SYSTEM_HH

#include <abi/system.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::SystemInfo;
    using abi::DateTime;

int32_t get_ticks();

int32_t get_datetime(DateTime *value);

int32_t get_system_info(SystemInfo *info);

int32_t get_kernel_log(char *buffer, uint32_t capacity);

int32_t read_kernel_log(char *buffer, uint32_t capacity, uint32_t *cursor);

}

#endif
