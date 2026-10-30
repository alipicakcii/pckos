// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <mem/alloc.hh>
#include <library/kernel_panic.hh>

#define KMALLOC_MINSIZE 16U

namespace mem
{
	static Vmm vm;
	static char *kernel_heap = nullptr;
	static uint32_t kernel_heap_used = 0;

	void MemAlloc::initialize()
	{
		kernel_heap = reinterpret_cast<char *>(KERN_HEAP_BEGIN);
		kernel_heap_used = 0;
		MemAlloc allocator;
		if (allocator.ksbrk_page(1) == reinterpret_cast<void *>(-1))
			lib::panic("MemAlloc::initialize(): could not map initial heap page");
	}

	uint32_t *MemAlloc::get_kernel_heap()
	{
		return reinterpret_cast<uint32_t *>(kernel_heap);
	}

	void *MemAlloc::ksbrk_page(int n)
	{
		if (n <= 0 || kernel_heap == nullptr)
			return reinterpret_cast<void *>(-1);

		const uint32_t heap_end = reinterpret_cast<uint32_t>(kernel_heap);
		const uint32_t remaining = KERN_HEAP_END - heap_end;
		if (static_cast<uint32_t>(n) > remaining / PAGESIZE_4K)
			return reinterpret_cast<void *>(-1);

		MallocHeader *chunk = reinterpret_cast<MallocHeader *>(kernel_heap);
		for (int i = 0; i < n; ++i)
		{
			const uint32_t p_addr = vm.acquire_page_frame_4k();
			if (p_addr == static_cast<uint32_t>(-1))
				return reinterpret_cast<void *>(-1);

			VirtualAddress vaddr = kernel_heap;
			PhysicalAddress paddr = p_addr;
			if (!vm.add_page_to_pd(vaddr, paddr, PG_WRITE))
			{
				lib::panic("ksbrk_page(): virtual page is already mapped");
				return reinterpret_cast<void *>(-1);
			}
			kernel_heap += PAGESIZE_4K;
		}

		chunk->size = PAGESIZE_4K * static_cast<uint32_t>(n);
		chunk->used = 0;
		chunk->alignment_padding = 0;
		return chunk;
	}

	void *MemAlloc::kmalloc(uint32_t size)
	{
		if (size == 0 || kernel_heap == nullptr)
			return nullptr;

		if (size > UINT32_MAX - sizeof(MallocHeader) - 7)
			return nullptr;
		uint32_t required = static_cast<uint32_t>(sizeof(MallocHeader)) + size;
		required = (required + 7) & ~7U;
		if (required < KMALLOC_MINSIZE)
			required = KMALLOC_MINSIZE;

		MallocHeader *chunk = reinterpret_cast<MallocHeader *>(KERN_HEAP_BEGIN);
		for (;;)
		{
			const uint32_t heap_end = reinterpret_cast<uint32_t>(kernel_heap);
			uint32_t address = reinterpret_cast<uint32_t>(chunk);
			if (address == heap_end)
			{
				const uint32_t pages = (required + PAGESIZE_4K - 1) / PAGESIZE_4K;
				if (pages > static_cast<uint32_t>(KERN_HEAP_END - heap_end) / PAGESIZE_4K ||
				    ksbrk_page(static_cast<int>(pages)) == reinterpret_cast<void *>(-1))
				{
					lib::panic("kmalloc(): no memory left for kernel heap");
					return nullptr;
				}
				continue;
			}

			if (address > heap_end || heap_end - address < sizeof(MallocHeader) ||
			    chunk->size < sizeof(MallocHeader) || chunk->size > heap_end - address)
			{
				lib::panic("kmalloc(): corrupt heap block");
				return nullptr;
			}

			if (!chunk->used && chunk->size >= required)
				break;
			chunk = reinterpret_cast<MallocHeader *>(address + chunk->size);
		}

		if (chunk->size - required >= KMALLOC_MINSIZE)
		{
			MallocHeader *remainder = reinterpret_cast<MallocHeader *>(
				reinterpret_cast<uint32_t>(chunk) + required);
			remainder->size = chunk->size - required;
			remainder->used = 0;
			remainder->alignment_padding = 0;
			chunk->size = required;
		}
		chunk->used = 1;
		kernel_heap_used += chunk->size;
		return reinterpret_cast<char *>(chunk) + sizeof(MallocHeader);
	}

	void MemAlloc::kfree(void *v_addr)
	{
		if (v_addr == nullptr)
			return;

		const uint32_t target = reinterpret_cast<uint32_t>(v_addr);
		const uint32_t heap_end = reinterpret_cast<uint32_t>(kernel_heap);
		MallocHeader *chunk = reinterpret_cast<MallocHeader *>(KERN_HEAP_BEGIN);
		while (reinterpret_cast<uint32_t>(chunk) < heap_end)
		{
			const uint32_t address = reinterpret_cast<uint32_t>(chunk);
			if (chunk->size < sizeof(MallocHeader) || chunk->size > heap_end - address)
			{
				lib::panic("kfree(): corrupt heap block");
				return;
			}
			if (target == address + sizeof(MallocHeader))
			{
				if (!chunk->used)
				{
					lib::panic("kfree(): block was already freed");
					return;
				}
				chunk->used = 0;
				kernel_heap_used -= chunk->size;
				while (address + chunk->size < heap_end)
				{
					MallocHeader *next = reinterpret_cast<MallocHeader *>(address + chunk->size);
					if (next->size < sizeof(MallocHeader) ||
					    next->size > heap_end - reinterpret_cast<uint32_t>(next))
					{
						lib::panic("kfree(): corrupt adjacent heap block");
						return;
					}
					if (next->used)
						break;
					chunk->size += next->size;
				}
				return;
			}
			chunk = reinterpret_cast<MallocHeader *>(address + chunk->size);
		}
		lib::panic("kfree(): pointer is not an allocation");
	}

	uint32_t MemAlloc::get_kernel_heap_used()
	{
		return kernel_heap_used;
	}
}
