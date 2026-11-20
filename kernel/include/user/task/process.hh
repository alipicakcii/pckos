// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_PROCESS_HH
#define PCKOS_USER_PROCESS_HH

#include <abi/process.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::ProcessInfo;

uint32_t process_count();

int32_t process_get_info(uint32_t index, ProcessInfo *info);

int32_t process_spawn(const char *path, const char *name);

int32_t process_minimize();

    // Çağıran process’i sorgulamak için 0 gönderin.
bool process_is_minimized(uint32_t process_id = 0);

int32_t process_focus(uint32_t process_id);

uint32_t process_focused_id();

[[noreturn]] void exit(int32_t status);
}

#endif
