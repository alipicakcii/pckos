// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef KERNEL_MEM_ALLOC_HH
#define KERNEL_MEM_ALLOC_HH

#include <stdint.h>
#include <mem/vmm.hh>
#include <mem/addr.hh>
#include <stddef.h>


namespace mem
{
        struct MallocHeader
        {
                uint32_t size:31;
                uint32_t used:1;
                uint32_t alignment_padding;

        } __attribute__ ((packed));



    class MemAlloc
    {
	    public:
		    MemAlloc() = default ;
		    ~MemAlloc() = default;
		    static void initialize();
		    // çekirdek için bir heap 
		    // bölgesi uzayı oluştur
		    void *ksbrk_page(int n);
		    // tahsis edilmiş bloğu kaldırı
		    void kfree(void *v_addr);
		    // çekirdeğin heap göstericinin
		    // nerede olduğunu tutar
		    // her 4KB bellek tahsisinde
		    // şu an için 0x02000000 dan başlayan gösterici
		    // 0x0200000 + 0x1000 kadar artar
		    static uint32_t *get_kernel_heap();
		    // çekirdek için bir bellek bloğu
		    // tahsi eder
		    void* kmalloc(uint32_t size);
		    // çekirdek için anlık kullanılan
		    // bellek miktar
		    static uint32_t get_kernel_heap_used();
		    
	   private:
	 
    
   
    };  
} // namespace mem


#endif
