// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <mem/vmm.hh>
#include <interrupts/isr.hh>
#include <interrupts/interrupts.hh>
#include <library/mem.hh>
#include <mem/alloc.hh>
#include <cpu/get_cpu.hh>
#include <library/kernel_panic.hh>

namespace mem
{
	uint32_t g_total_page_count = 0;
	uint32_t g_free_page_count = 0;
	class PageFrame
	{
	private:
		uint8_t bitmap[RAM_AS_4K_PAGES / 8];

	public:
		void set_frame(int i, uint8_t v)
		{
			this->bitmap[i] = v;
		}
		void set_frame_used(int page_index)
		{
			this->bitmap[((uint32_t)page_index) / 8] |= (1 << (((uint32_t)page_index) % 8));
		}
		bool clear_frame(int page_index)
		{
			const uint32_t byte = static_cast<uint32_t>(page_index) / 8;
			const uint8_t mask = static_cast<uint8_t>(1U << (static_cast<uint32_t>(page_index) % 8));
			if ((bitmap[byte] & mask) == 0)
				return false;
			bitmap[byte] &= static_cast<uint8_t>(~mask);
			return true;
		}
		uint8_t &operator[](int i)
		{
			return this->bitmap[i];
		}
	};



	PageFrame p_frame;
	void hander_no_page(kernel::Registers *registers)
	{
		uint32_t fault_address = 0;
		uint32_t page_directory = 0;
		asm volatile("mov %%cr2, %0" : "=r"(fault_address));
		asm volatile("mov %%cr3, %0" : "=r"(page_directory));
		if (registers != nullptr)
			_kprintf("[PageFault] addr=0x%x eip=0x%x error=0x%x cs=0x%x cr3=0x%x\n",
			         fault_address, registers->eip, registers->errorCode,
			         registers->cs, page_directory);
		cpu::disable_interrupt();
		lib::panic("Paged Error");
		cpu::cpu_halt();
	}

	uint32_t *Vmm::get_kernel_page_directory()
	{
		return (uint32_t *)KERN_PAGE_DIRECTORY;
	}

	// bitmapda yer ayırma
	uint32_t Vmm::acquire_page_frame_4k()
	{
		uint32_t page = -1;

		// int pid = -1;
		// Thread* thread = thread_get_current();
		// if (thread)
		//{
		//	Process* process = thread->owner;
		//	if (process)
		//	{
		//  		pid = process->pid;
		//	}
		//}
		for (int byte = 0; byte < RAM_AS_4K_PAGES / 8; byte++)
		{

			if (p_frame[byte] != 0xFF)
			{

				for (int bit = 0; bit < 8; bit++)
				{
					// bitleri kaydır, kaydırma yapilirken
					// hangi bit deysek kaydırıyoruz ornegin
					// 00110011 bunu (1<<4) kaydırsak işlem
					// 00110011
					// 00010000
					// olur ve & operatoru ile denk gelen bit 1 ise 1 0 ise 0 olur
					// doğal olarak ya 0 olur yada 1 yada decimalda cok daha büyük bir değer olabilir
					// ve bu hat dolu demekdir 0 ı gördüğü anda sayfa çervesinde doldurmak için döngüden cıkıyoruz
					int v = (p_frame[byte] & (1 << bit));
					if (!v)
					{
						page = 8 * byte + bit;
						p_frame.set_frame_used(page);
						if (g_free_page_count != 0)
							--g_free_page_count;
						return (page * PAGESIZE_4K);
					}
				}
			}
			
		}

		return -1;
	}

	bool Vmm::release_page_frame_4k(uint32_t physical_address)
	{
		if ((physical_address & (PAGESIZE_4K - 1)) != 0 ||
		    physical_address < RESERVED_AREA)
			return false;

		const uint32_t page_index = physical_address / PAGESIZE_4K;
		if (page_index >= g_total_page_count)
			return false;
		const bool released = p_frame.clear_frame(static_cast<int>(page_index));
		if (released)
			++g_free_page_count;
		return released;
	}

#if defined(__is_DEBUG)
	// bitmap haritasısnı serial porta yaz
	void Vmm::print_page_frame_bitmap(bool if_bit)
	{
		for (size_t i = 0; i < (RAM_AS_4K_PAGES / 8); i++)
		{
			_kprintf("index=%d byte=0x%x bit_value=", i, p_frame[i]);
			if (if_bit)
			{
				for (int bit = 7; bit > -1; bit--)
				{
					_kprintf("%d", (p_frame[i] >> bit) & 1);
				}
			}
			else
			{
				_kprintf("--------");
			}
			_kprintf("\n");
		}

	}


#endif

	// Bu işlevi çağırırken:
	// Çekirdek belleği tahsis edilmesi amaçlanıyorsa v_addr < KERN_HEAP_END olmalıdır.
	// Kullanıcı belleği tahsis edilmesi amaçlanıyorsa v_addr > KERN_HEAP_END olmalıdır.
	// Aktif Sayfa Dizini için çalışır!
	bool Vmm::add_page_to_pd(VirtualAddress &virtual_adress,PhysicalAddress & p_addr, int flags)
	{
		char * v_addr = reinterpret_cast<char*>(virtual_adress.get());
		int pd_index = (((uint32_t)v_addr) >> 22);
		int pt_index = (((uint32_t)v_addr) >> 12) & 0x03FF;

		uint32_t *pd = (uint32_t *)0xFFFFF000;
		// 0x400 * N
		// örnek bir adres 0x02000000
		// adresin sayfa dizini  index  = 1023
		// adresin sayfa tablosu index  = 8
		// adresin sayfa ofsett         = 0
		// o zaman bu 0xFFC00000 adresinin sayfa tablosu indexide 8 cunku (0x400*pd_index)
		// bu durumda pd [1023] de  yine pd var
		// pd[1023][8] anlamına gelir
		// eğer 0xFFFFF000 ile pd ye bir pt yerleştiriksek
		// yani bir sayfa tablosu gösterirse
		// o zaman
		// pt[1023][8] de işlemci pd[8] de olan sayfa tablosu fiziksel adresini bulur
		// o zaman da biz bir pt[pt_index]  yani aslında
		// pd[1023][8][pt_index] e bir fiziksel 4KB sayfa yerleştirebiliriz
		uint32_t *pt = ((uint32_t *)0xFFC00000) + (0x400 * pd_index);

		// Kernel heap page tables must be created in the kernel directory.
		// User processes share those tables after sync_all_from_kernel().
		const bool kernel_mapping = virtual_adress.get() < KERN_HEAP_END;
		const uint32_t previous_cr3 = cpu::read_cr3();
		const bool switched = kernel_mapping && previous_cr3 != KERN_PAGE_DIRECTORY;
		if (switched)
			change_pd(KERN_PAGE_DIRECTORY);
		// sayfa dizinnde bir fiziksel sayfa yoksa
		if ((pd[pd_index] & PG_PRESENT) != PG_PRESENT)
		{
			// 4KB fiziksel sayfa 
			uint32_t tablePhysical = this->acquire_page_frame_4k();
			if (tablePhysical == static_cast<uint32_t>(-1))
			{
				if (switched)
					change_pd(previous_cr3);
				return false;
			}
			// 4KB bir fiziksel sayfa tablosu girişi oluşturduk
			pd[pd_index] = (tablePhysical) | (flags & 0xFFF) |
			               PG_PRESENT | PG_WRITE;
			// TBL buffer temizle
			invalidate((uint32_t)v_addr);

			// pde nin gösterdiği yerdeki sayfa
			// sayfa tablosunu 0x0 olarak hazır tur
			// kullanılmaz sekilde önceki pis veriler varsa temiz
			for (int i = 0; i < 1024; i++)
			{
				pt[i] = 0x0;
			}
		}
		else if ((pd[pd_index] & PG_4MB) != 0)
		{
			if (switched)
				change_pd(previous_cr3);
			return false;
		}
		else
		{
			if ((flags & PG_USER) != 0)
				pd[pd_index] |= PG_USER;
			if ((flags & PG_WRITE) != 0)
				pd[pd_index] |= PG_WRITE;
		}

		// sayfa tablosunda fiziksel sayfa varsa
		// bir 4kb sayfa oluşturmaya gerek yok zaten var
		if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
		{
			if (switched)
				change_pd(previous_cr3);
			return false;
		}

		// 4KB sayfa 
		pt[pt_index] = (p_addr.get()) | (flags & 0xFFF) | PG_PRESENT;
		invalidate((uint32_t)v_addr);

		if (kernel_mapping)
		{
			// Copy any new kernel PDE into every process before returning to its CR3.
			sync_all_from_kernel();
		}
		if (switched)
			change_pd(previous_cr3);
		return true;
	}

	bool Vmm::remove_page_from_pd(VirtualAddress &virtual_address,
	                              uint32_t *physical_address)
	{
		const uint32_t address = virtual_address.get();
		const uint32_t pd_index = address >> 22;
		const uint32_t pt_index = (address >> 12) & 0x3FF;
		const uint32_t previous_cr3 = cpu::read_cr3();
		const bool switched = address < KERN_HEAP_END &&
		                      previous_cr3 != KERN_PAGE_DIRECTORY;
		if (switched)
			change_pd(KERN_PAGE_DIRECTORY);

		uint32_t *pd = reinterpret_cast<uint32_t *>(0xFFFFF000);
		bool removed = false;
		if ((pd[pd_index] & PG_PRESENT) != 0 && (pd[pd_index] & PG_4MB) == 0)
		{
			uint32_t *pt = reinterpret_cast<uint32_t *>(0xFFC00000) + (0x400 * pd_index);
			if ((pt[pt_index] & PG_PRESENT) != 0)
			{
				if (physical_address != nullptr)
					*physical_address = pt[pt_index] & 0xFFFFF000;
				pt[pt_index] = 0;
				invalidate(address);
				removed = true;
			}
		}

		if (switched)
			change_pd(previous_cr3);
		return removed;
	}

	bool Vmm::set_page_permissions(VirtualAddress &virtual_address,
	                               bool user_access, bool write_access)
	{
		const uint32_t address = virtual_address.get();
		const uint32_t pd_index = address >> 22;
		const uint32_t pt_index = (address >> 12) & 0x3FF;
		uint32_t *pd = reinterpret_cast<uint32_t *>(0xFFFFF000);
		if ((pd[pd_index] & PG_PRESENT) == 0 || (pd[pd_index] & PG_4MB) != 0)
			return false;

		uint32_t *pt = reinterpret_cast<uint32_t *>(0xFFC00000) + (0x400 * pd_index);
		if ((pt[pt_index] & PG_PRESENT) == 0)
			return false;

		pt[pt_index] &= ~(PG_USER | PG_WRITE);
		if (user_access)
		{
			pt[pt_index] |= PG_USER;
			pd[pd_index] |= PG_USER;
		}
		if (write_access)
		{
			pt[pt_index] |= PG_WRITE;
			pd[pd_index] |= PG_WRITE;
		}
		invalidate(address);
		return true;
	}

	bool Vmm::is_user_range(uint32_t address, size_t length, bool require_write) const
	{
		if (length == 0)
			return address >= USER_OFFSET && address < 0xFFC00000;

		const uint64_t end = static_cast<uint64_t>(address) + length;
		if (address < USER_OFFSET || end > 0xFFC00000 || end <= address)
			return false;

		const uint32_t *pd = reinterpret_cast<const uint32_t *>(0xFFFFF000);
		uint32_t page = address & ~(PAGESIZE_4K - 1);
		while (static_cast<uint64_t>(page) < end)
		{
			const uint32_t pd_index = page >> 22;
			const uint32_t pde = pd[pd_index];
			if ((pde & (PG_PRESENT | PG_USER)) != (PG_PRESENT | PG_USER))
				return false;
			if (require_write && (pde & PG_WRITE) == 0)
				return false;

			if ((pde & PG_4MB) == 0)
			{
				const uint32_t pt_index = (page >> 12) & 0x3FF;
				const uint32_t *pt = reinterpret_cast<const uint32_t *>(0xFFC00000) +
				                     (0x400 * pd_index);
				const uint32_t pte = pt[pt_index];
				if ((pte & (PG_PRESENT | PG_USER)) != (PG_PRESENT | PG_USER) ||
				    (require_write && (pte & PG_WRITE) == 0))
					return false;
			}
			page += PAGESIZE_4K;
		}
		return true;
	}

	void Vmm::sync_all_from_kernel()
	{
		uint32_t address = KERN_PD_AREA_BEGIN;
		for (; address < KERN_PD_AREA_END; address += PAGESIZE_4K)
		{
			uint32_t *pd = (uint32_t *)address;

			if (*pd != 0)
			{
				// Found an in-use page directory

				for (int i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
				{
					pd[i] = this->get_kernel_page_directory()[i] & ~PG_OWNED;
				}
			}
		}
	}

	uint32_t Vmm::get_total_page_count()
	{
		return g_total_page_count;
	}

	uint32_t Vmm::get_free_page_count()
	{
		return g_free_page_count;
	}

	uint32_t Vmm::initialize_paging(const uint32_t &high_mem)
	{
		uint32_t pg;
		size_t i;
		kernel::interrupt_register(14, (kernel::IsrFunction)hander_no_page);

		// Multiboot mem_upper excludes the first MiB and is reported in KiB.
		g_total_page_count = high_mem / 4 + 256;
		if (g_total_page_count > RAM_AS_4K_PAGES)
			g_total_page_count = RAM_AS_4K_PAGES;

		// Begin with all frames unavailable, then free only pages that exist.
		for (pg = 0; pg < (RAM_AS_4K_PAGES / 8); ++pg)
			p_frame.set_frame(pg, 0xFF);
		for (pg = 0; pg < g_total_page_count; ++pg)
		{
			const uint32_t byte = pg / 8;
			const uint8_t mask = static_cast<uint8_t>(1U << (pg % 8));
			p_frame.set_frame(byte, static_cast<uint8_t>(p_frame[byte] & ~mask));
		}

		// Çekirdek için ayrılan sayfaları kullanıldığı gibi işaretle
		for (pg = PAGE_INDEX_4K(0x0); pg < (int)(PAGE_INDEX_4K(RESERVED_AREA)); ++pg)
		{
			p_frame.set_frame_used(pg);
		}
		const uint32_t reserved_pages = g_total_page_count < PAGE_INDEX_4K(RESERVED_AREA)
			? g_total_page_count : PAGE_INDEX_4K(RESERVED_AREA);
		g_free_page_count = g_total_page_count - reserved_pages;

		// İlk 16MB için kimlik haritası
		// İlk kimlik sayfaları kolaylık olması açısından 4MB boyutundadır
		for (i = 0; i < 4; ++i)
		{

			this->get_kernel_page_directory()[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB));
			
		}
		// sayfa dizini geri kalanlarınişaretle
		for (i = 4; i < 1024; ++i)
		{
			this->get_kernel_page_directory()[i] = 0;
		}

		// Özyinelemeli sayfa dizini stratejisi 0xFFFFF000 sanal
		// adresi direk olarak sayfa dizinini 0xFFFFF000 sanal adresinin
		// sayfa tablosuda yine direk olarak 1023 de olan sayfa dizinini gösterecekdir
		this->get_kernel_page_directory()[1023] = (uint32_t)this->get_kernel_page_directory() | PG_PRESENT | PG_WRITE;

		memset((uint8_t *)KERN_PD_AREA_BEGIN, 0x0, KERN_PD_AREA_END - KERN_PD_AREA_BEGIN);

		_enable_paging(this->get_kernel_page_directory());

		MemAlloc::initialize();
		return (uint32_t)this->get_kernel_page_directory();
	}

} // namespace mem
