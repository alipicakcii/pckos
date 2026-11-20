// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_THREAD_HH
#define PCKOS_USER_THREAD_HH

#include <abi/thread.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::ThreadInfo;
    using abi::ThreadEntry;

void thread_yield();

void thread_sleep(uint32_t ticks);

uint32_t thread_id();

uint32_t thread_count();

uint32_t thread_capacity();

uint32_t thread_switch_count();

[[noreturn]] void thread_start_trampoline(ThreadEntry entry,
                                                      void *argument);

int32_t thread_create(ThreadEntry entry, void *argument,
                                 const char *name);

[[noreturn]] void thread_exit();

int32_t thread_get_info(uint32_t id, ThreadInfo *info);

int32_t thread_set_name(const char *name);
}

#endif
