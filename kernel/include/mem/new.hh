// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef KERNEL_MEM_ALLOC_NEW_AND_DELETE_HH
#define KERNEL_MEM_ALLOC_NEW_AND_DELETE_HH

#include <mem/alloc.hh>
#include <stdint.h>


void *operator new(size_t size);
void *operator new[](size_t size);
void* operator new(size_t size, void* ptr);
void operator delete(void *addr);
void operator delete[](void *addr, long unsigned int);
void operator delete[](void *addr);
void operator delete(void *addr, unsigned long);


#endif
