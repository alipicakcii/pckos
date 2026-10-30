// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <mem/new.hh>
#include <library/kprint.hh>


void *kernel_malloc(size_t size)
{
	mem::MemAlloc alloc;
	auto *ptr = alloc.kmalloc(size);
	return ptr;
}
void kernel_free(void *v_addr)
{
	mem::MemAlloc alloc;
	alloc.kfree(v_addr);
}
void *operator new(size_t size)
{
	return kernel_malloc(size);
}
void *operator new[](size_t size)
{		
	return kernel_malloc(size);
}

void *operator new (size_t size, void *addr)
{
        (void)(size);
        return addr;
}

void operator delete(void *addr)
{
	kernel_free(addr);
}

void operator delete[](void *addr, long unsigned int)
{
	kernel_free(addr);
}

void operator delete[](void *addr)
{
	kernel_free(addr);
}

void operator delete(void *addr, unsigned long)
{
	kernel_free(addr);
}


