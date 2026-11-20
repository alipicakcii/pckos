// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_HEAP_HH
#define PCKOS_USER_HEAP_HH

#include <user/syscall/call.hh>

namespace user
{
int32_t get_heap_usage();

void *heap_alloc(uint32_t size, uint32_t alignment = 16);

int32_t heap_free(void *address);
}

#endif
