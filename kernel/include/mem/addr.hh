// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef MEM_VIRTUAL_ADDRES_PCKOS_MEM
#define MEM_VIRTUAL_ADDRES_PCKOS_MEM

#include <stdint.h>
#include <library/kprint.hh>

namespace mem
{
	class VirtualAddress;
	
	class PhysicalAddress
	{
	private:
		// bunun anlamı bazen unutup
		// PhysicalAddress nesnelerinii _va_args gibi
		// printf fonsksiyonlara  gönderirken sanki get() etkisi
		// gibi adresi yazdırıyor _p_addr sınıfı
		// ilk elemanı olduğu için unutulup sorun
		// var sanabildiğim icin sınıfın ilk elemanını 
		// 4 bytelik bir 0 ile dolduruyoruz
		int __zero = 0x0;
		void *_p_addr;

	public:
		PhysicalAddress(void *addr);
		PhysicalAddress(uint32_t addr);
		VirtualAddress get_vaddr();
		uint32_t get()
		{
			return static_cast<uint32_t>((uint32_t)this->_p_addr);
		}
		bool operator<=(PhysicalAddress const &other) const
		{
			return this->_p_addr <= other._p_addr;
		}
		bool operator>=(PhysicalAddress const &other) const
		{
			return this->_p_addr >= other._p_addr;
		}
		bool operator>(PhysicalAddress const &other) const
		{
			return this->_p_addr > other._p_addr;
		}
		bool operator<(PhysicalAddress const &other) const
		{
			return this->_p_addr < other._p_addr;
		}
		bool operator==(PhysicalAddress const &other) const
		{
			return this->_p_addr == other._p_addr;
		}
		bool operator!=(PhysicalAddress const &other) const
		{
			return this->_p_addr != other._p_addr;
		}
	};

	class VirtualAddress
	{
	public:

		VirtualAddress(void *v_addr);
		VirtualAddress(uint32_t v_addr);
		~VirtualAddress() = default;
	
		PhysicalAddress physical_addr();
		void print_segments();
		uint32_t get()
		{
			return static_cast<uint32_t>((uint32_t)this->_v_addr);
		}

		uint32_t pd_index()
		{
			return this->_pd_index;
		}
		uint32_t pt_index()
		{
			return this->_pt_index;
		}
		uint32_t offset()
		{
			return this->_offset;
		}
		bool operator<=(VirtualAddress const &other) const
		{
			return this->_v_addr <= other._v_addr;
		}
		bool operator>=(VirtualAddress const &other) const
		{
			return this->_v_addr >= other._v_addr;
		}
		bool operator>(VirtualAddress const &other) const
		{
			return this->_v_addr > other._v_addr;
		}
		bool operator<(VirtualAddress const &other) const
		{
			return this->_v_addr < other._v_addr;
		}
		bool operator==(VirtualAddress const &other) const
		{
			return this->_v_addr == other._v_addr;
		}
		bool operator!=(VirtualAddress const &other) const
		{
			return this->_v_addr != other._v_addr;
		}

	private:
		void *_v_addr;
		void segment_patch();
		uint32_t _pd_index;
		uint32_t _pt_index;
		uint32_t _offset;
	};

	inline VirtualAddress operator-(VirtualAddress &a, VirtualAddress &b)
	{

		VirtualAddress r = (void *)(a.get() - b.get());
		if (static_cast<int>(r.get()) <= 0)return VirtualAddress((uint32_t)0x0);
		return VirtualAddress(reinterpret_cast<void *>(r.get()));
	}
	inline PhysicalAddress operator-(PhysicalAddress &a, PhysicalAddress &b)
	{

		PhysicalAddress r = (void *)(a.get() - b.get());
		if (static_cast<int>(r.get()) <= 0)return PhysicalAddress((uint32_t)0x0);
		return PhysicalAddress(reinterpret_cast<void *>(r.get()));
	}
}

#endif
