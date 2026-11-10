#include <firmware/smbios.hh>


namespace firmware
{
	

	size_t SMBIOS::smbios_struct_len(SMBIOSHeader *hd)
	{
		size_t i;
		const char *strtab = (char *)hd + hd->len;
		for (i = 1; strtab[i - 1] != '\0' || strtab[i] != '\0'; i++)
			;
		return hd->len + i + 1;
	}

	SMBIOS::SMBIOS()
	{

		this->_eps = this->find_eps();
		this->_str_start = nullptr;
		this->_str_end   = nullptr;
		this->_type_next_number =  0;
		
	}

	void SMBIOS::print_string()
	{
		if (this->_str_start == nullptr ||this->_str_end == nullptr  )return;


		while((uint32_t) this->_str_start < (uint32_t) this->_str_end)
		{
			
			if (this->_str_start[0] == '\0' && this->_str_start[1] == '\0') break;

			while(*this->_str_start == '\0')
			{
				//kprintf(" ");
				this->_str_start++;

			}
	
			char c = *this->_str_start++;
			//_kprintf("%c", c);
		}
	}

	SmBiosStructure * SMBIOS::next_structure()
	{
		if (this->_eps == nullptr) return nullptr;

		SmBiosStructure  * type = reinterpret_cast<SmBiosStructure *>(this->_eps->structure_table_address);


		char *ptr;
		char  len;
		for (int i  = 0; i < this->_type_next_number; i++)
		{
			len = this->smbios_struct_len(& type->header);
			ptr     = ((char*) type + len);
			type    =  reinterpret_cast<SmBiosStructure*>(ptr);
		}


		char * str_start = (char*)type + type->header.len;
		char * str_end   = (char*) str_start + 64; 
		this->_str_start = str_start;
		this->_str_end   = str_end;
		this->_type_next_number++;
		if (
				type->header.type >= 127 
				|| 
				type->header.len <=0
		   )
		{
			return nullptr;
		}
		return type;
	}

	char * SMBIOS::string_start()
	{
		return this->_str_start;

	}
	char * SMBIOS::string_end()
	{
		return this->_str_end;
	}
	
	Eps * SMBIOS::find_eps()
	{
		char *_start = (char*)START_ADDR;
		char *_end = (char*)END_ADDR;
		bool checksum = false;
		while((uint32_t)_start < (uint32_t)_end)
		{
			if (
					_start[0] == '_' &&
					_start[1] == 'S' &&
					_start[2] == 'M' &&
					_start[3] == '_' 

			   )
			{
				checksum = true;
				break;
			}
			_start++;
		}
		if (!checksum)return nullptr;
		return reinterpret_cast<Eps*>(_start);
	}
	Eps * SMBIOS::get_eps()
	{
		
		return this->_eps;
	}


    SmBiosStructure * SMBIOS::find_strtucte(SmBiosType  table_type)
    {

            if (this->_eps == nullptr) return nullptr;

            SmBiosStructure  * type = reinterpret_cast<SmBiosStructure *>(this->_eps->structure_table_address);
            char *ptr;
            char  len;


            while(true)
            {

                    if (type->header.type ==  table_type) break;
                    if (type == nullptr) break;
                    len = this->smbios_struct_len(& type->header);
                    ptr     = ((char*) type + len);
                    type    =  reinterpret_cast<SmBiosStructure*>(ptr);
                    if (type->header.type >= 126)  break;

            }

            char * str_start = (char*)type + type->header.len;
            char * str_end   = (char*) str_start + 64; 
            this->_str_start = str_start;
            this->_str_end   = str_end;
			
			

            return type;
    
    }

    BiosInformation * SMBIOS::get_bios_information()
    {
            auto ptr = this->find_strtucte(SmBiosType::BIOS_Information);
            return static_cast<BiosInformation*>(ptr);
    }
    
    SystemInformation  * SMBIOS::get_system_information()
    {
            auto ptr = this->find_strtucte(SmBiosType::System_Information);
            return static_cast<SystemInformation*>(ptr);
    }
    SystemEnclosure  * SMBIOS::get_enclosure_information()
    {
            auto ptr = this->find_strtucte(SmBiosType::System_Enclosure);
            return static_cast<SystemEnclosure*>(ptr);
    }
    ProcInformation  * SMBIOS::get_cpu_information()
    {
            auto ptr = this->find_strtucte(SmBiosType::Processor_Information);
            return static_cast<ProcInformation*>(ptr);
    }
}
