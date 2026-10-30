// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef KERNEL_VIRTUAL_MEMEORY_MANAGER_H
#define KERNEL_VIRTUAL_MEMEORY_MANAGER_H
#include <stdint.h>
#include <stddef.h>
#include <library/kprint.hh>
#include <mem/addr.hh>


#define KERN_PAGE_DIRECTORY 0x00001000

// 16M aşağıdaki gibi kimlik eşlemesidir.
// Çekirdek ve GRUB tarafindan yuklenen acilis verileri bu alanda bulunur.
// 4M, 4K sayfa dizinleri için ayrılmıştır.
#define RESERVED_AREA      0x01000000	  // 16 mb
#define KERN_PD_AREA_BEGIN 0x00C00000     // 12 mb
#define KERN_PD_AREA_END   0x01000000	  // 16 mb

#define GFX_MEMORY 0x01000000 // 16 mb

#define KERN_HEAP_BEGIN 0x02000000 // 32 mb
#define KERN_HEAP_END 0x40000000   // 1 gb

#define PAGING_FLAG 0x80000000 // CR0 - bit 31
#define PSE_FLAG 0x00000010	   // CR4 - bit 4 //4M sayfa desteği için.
#define PG_PRESENT 0x00000001  // page directory / table
#define PG_WRITE 0x00000002
#define PG_USER 0x00000004
#define PG_4MB 0x00000080
#define PG_OWNED 0x00000200 // Sahip olunan sayfaların defter tutulması için 9. biti kullanırız (İşletim Sistemi için 9-11. bitler mevcuttur)
#define PAGESIZE_4K 0x00001000
#define PAGESIZE_4M 0x00400000
#define RAM_AS_4K_PAGES 0x100000
#define RAM_AS_4M_PAGES 1024
#define PAGE_COUNT(bytes) (((bytes - 1) / PAGESIZE_4K) + 1)
#define PAGE_INDEX_4K(addr) ((addr) >> 12)
#define PAGE_INDEX_4M(addr) ((addr) >> 22)

#define KERNELMEMORY_PAGE_COUNT 256 // İlk 1GB çekirdek alanı (sayfa dizinindeki ilk 256 giriş)
#define KERN_STACK_SIZE PAGESIZE_4K

// KERN_HEAP_END ends and this one starts
#define USER_OFFSET 0x40000000
#define USER_MMAP_START 0x80000000 // Bu sadece haritalama içindir, sbrk ile çakışmamak için vmem'i buradan aramaya başlar. Sbrk kullanılmazsa USER_OFFSET'ten başlayabilir!
#define MEMORY_END 0xFFC00000	   // After this address is not usable. Because Page Tables sit there!

#define SIZE_2MB 0x200000 // 2MB

#define USER_STACK 0xF0000000

namespace mem
{

	extern "C" void change_pd(uint32_t);
	extern "C" uint32_t  invalidate(uint32_t);
	extern "C" void _enable_paging(uint32_t *);

	

	

	class Vmm
	{

	private:
	public:
		// sayfalamyı kur
		uint32_t initialize_paging(const uint32_t &high_mem);
		// sauyfa dizinne bir sayfa tablosu ekle
		// eğer sayfa dizini sanal adresin gösterdiği
		// pd de pde bulumkuyorsa pd icin bir pde oluştur
		bool add_page_to_pd(VirtualAddress &v_addr, PhysicalAddress  &p_addr, int flags);
		// toplam kullanabilir sayfa adeti
		uint32_t get_total_page_count();
		uint32_t get_free_page_count();
		// çekirdeğin sayfa dizini adresini döner
		uint32_t *get_kernel_page_directory();
		// 4K  sayfa oluştur boşta olan ilk
		// sayfayı oluştur
		uint32_t acquire_page_frame_4k();
		bool release_page_frame_4k(uint32_t physical_address);
		bool remove_page_from_pd(VirtualAddress &virtual_address,
		                         uint32_t *physical_address = nullptr);
		bool set_page_permissions(VirtualAddress &virtual_address,
		                          bool user_access, bool write_access);
		bool is_user_range(uint32_t address, size_t length,
		                   bool require_write = false) const;
		//Bu çekirdek sayfası diziniyse, ilk 1 GB için diğerlerini senkronize edin
		void sync_all_from_kernel();
		// sanal adresin fiziksel adresini verir
#if defined(__is_DEBUG)
		void print_page_frame_bitmap(bool if_bit = false);
	
#endif
	};

} // namespace mem

#endif
