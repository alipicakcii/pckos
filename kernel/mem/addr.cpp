// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <mem/addr.hh>



namespace mem
{

	PhysicalAddress::PhysicalAddress(void *addr)
		: _p_addr{addr}
	{

	}
	PhysicalAddress::PhysicalAddress(uint32_t addr)
		: _p_addr{reinterpret_cast<void*>(addr)}
	{

	}
	VirtualAddress PhysicalAddress::get_vaddr()
	{
		VirtualAddress fake_vaddr =  (uint32_t)0x0;
		return fake_vaddr;
	}
	VirtualAddress::VirtualAddress(void * v_addr)
		:_v_addr{v_addr}
	{
		this->segment_patch();
	}
	
	VirtualAddress::VirtualAddress(uint32_t  v_addr)
		:_v_addr{reinterpret_cast<void*>(v_addr)}
	{
		this->segment_patch();
	}

	void VirtualAddress::print_segments()
	{

		_kprintf("pd_index=0x%x pt_index=0x%x offset=0x%x\n",
				this->_pd_index, this->_pt_index, this->_offset);
	}
	void VirtualAddress::segment_patch()
	{

		this->_pd_index = this->get() >> 22;
		this->_pt_index = (this->get() >> 12) & 0x3FF;
		this->_offset = (this->get() & 0xFFF);
	}
	

	PhysicalAddress  VirtualAddress::physical_addr()
	{


		uint32_t v_addr = static_cast<uint32_t>((uint32_t)this->_v_addr);
		uint32_t *pd  = (uint32_t*)0xFFFFF000; 
		if ((uint32_t)v_addr <= 0x00C00000)
		{
			uint32_t m_pd_index = ((uint32_t)v_addr>>22);
			uint32_t offset = ((uint32_t)v_addr & 0x3FFFFF);
			PhysicalAddress p_addr_ret = reinterpret_cast<void*>((pd[m_pd_index] & ~0xFFF)+offset);
			return p_addr_ret;

		}

		uint32_t pd_index = (((uint32_t)v_addr) >> 22);
		uint32_t pt_index = (((uint32_t)v_addr) >> 12) & 0x03FF;
		uint32_t *pt = ((uint32_t *)0xFFC00000) + (0x400 * pd_index);

		uint32_t p_addr = (pt[pt_index]&~0xFFF) + ((uint32_t)v_addr & 0xFFF)  ;
		PhysicalAddress p_addr_ret  = p_addr;
		return p_addr_ret;

	}






}
