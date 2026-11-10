// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <firmware/smp.hh>

namespace kernel::smp
{
    namespace
    {
        constexpr uint32_t PHYSICAL_LIMIT = 0xFFFFF000;
        constexpr uint32_t BIOS_SCAN_BEGIN = 0x000E0000;
        constexpr uint32_t BIOS_SCAN_END = 0x00100000;

        struct SdtHeader
        {
            char signature[4];
            uint32_t length;
            uint8_t revision;
            uint8_t checksum;
            char oem_id[6];
            char oem_table_id[8];
            uint32_t oem_revision;
            uint32_t creator_id;
            uint32_t creator_revision;
        } __attribute__((packed));

        bool matches(const char *left, const char *right, uint32_t length)
        {
            for (uint32_t i = 0; i < length; ++i)
                if (left[i] != right[i])
                    return false;
            return true;
        }

        bool checksum_valid(const uint8_t *bytes, uint32_t length)
        {
            uint8_t sum = 0;
            for (uint32_t i = 0; i < length; ++i)
                sum = static_cast<uint8_t>(sum + bytes[i]);
            return sum == 0;
        }

        bool readable_table(uint32_t address, uint32_t minimum_size)
        {
            if (address == 0 || address > PHYSICAL_LIMIT - sizeof(SdtHeader))
                return false;
            const auto *header = reinterpret_cast<const SdtHeader *>(address);
            return header->length >= minimum_size &&
                   header->length <= 0x100000 && header->length <= PHYSICAL_LIMIT - address;
        }

        uint32_t find_rsdp()
        {
            const uint16_t ebda_segment = *reinterpret_cast<volatile uint16_t *>(0x40E);
            const uint32_t ebda = static_cast<uint32_t>(ebda_segment) << 4;
            const uint32_t ebda_end = ebda + 1024;
            for (uint32_t address = ebda; address < ebda_end && address < BIOS_SCAN_BEGIN;
                 address += 16)
            {
                const auto *signature = reinterpret_cast<const char *>(address);
                if (matches(signature, "RSD PTR ", 8) &&
                    checksum_valid(reinterpret_cast<const uint8_t *>(address), 20))
                    return address;
            }

            for (uint32_t address = BIOS_SCAN_BEGIN; address < BIOS_SCAN_END; address += 16)
            {
                const auto *signature = reinterpret_cast<const char *>(address);
                if (matches(signature, "RSD PTR ", 8) &&
                    checksum_valid(reinterpret_cast<const uint8_t *>(address), 20))
                    return address;
            }
            return 0;
        }

        uint32_t find_madt(uint32_t rsdp_address)
        {
            if (rsdp_address == 0)
                return 0;
            const auto *rsdp = reinterpret_cast<const uint8_t *>(rsdp_address);
            const uint32_t rsdt_address = *reinterpret_cast<const uint32_t *>(rsdp + 16);
            if (!readable_table(rsdt_address, sizeof(SdtHeader)))
                return 0;

            const auto *rsdt = reinterpret_cast<const SdtHeader *>(rsdt_address);
            if (!matches(rsdt->signature, "RSDT", 4) ||
                (rsdt->length - sizeof(SdtHeader)) % sizeof(uint32_t) != 0 ||
                !checksum_valid(reinterpret_cast<const uint8_t *>(rsdt), rsdt->length))
                return 0;

            const auto *entries = reinterpret_cast<const uint32_t *>(rsdt + 1);
            const uint32_t entry_count =
                (rsdt->length - sizeof(SdtHeader)) / sizeof(uint32_t);
            for (uint32_t i = 0; i < entry_count; ++i)
            {
                const uint32_t address = entries[i];
                if (!readable_table(address, sizeof(SdtHeader) + 8))
                    continue;
                const auto *header = reinterpret_cast<const SdtHeader *>(address);
                if (matches(header->signature, "APIC", 4) &&
                    checksum_valid(reinterpret_cast<const uint8_t *>(header), header->length))
                    return address;
            }
            return 0;
        }

        void add_cpu(CpuTopology &topology, uint32_t apic_id)
        {
            if (topology.count >= MAX_CPUS)
                return;
            for (uint32_t i = 0; i < topology.count; ++i)
                if (topology.apic_ids[i] == apic_id)
                    return;
            topology.apic_ids[topology.count++] = apic_id;
        }

        void parse_madt(CpuTopology &topology, uint32_t address)
        {
            const auto *header = reinterpret_cast<const SdtHeader *>(address);
            topology.local_apic_address = *reinterpret_cast<const uint32_t *>(
                reinterpret_cast<const uint8_t *>(header) + sizeof(SdtHeader));
            topology.local_apic_found = topology.local_apic_address != 0;
            const uint8_t *cursor = reinterpret_cast<const uint8_t *>(header) + sizeof(SdtHeader);
            const uint8_t *end = reinterpret_cast<const uint8_t *>(header) + header->length;
            if (end - cursor < 8)
                return;
            cursor += 8;

            while (cursor + 2 <= end)
            {
                const uint8_t type = cursor[0];
                const uint8_t length = cursor[1];
                if (length < 2 || cursor + length > end)
                    return;

                if (type == 0 && length >= 8)
                {
                    const uint32_t flags = *reinterpret_cast<const uint32_t *>(cursor + 4);
                    if ((flags & 1U) != 0)
                        add_cpu(topology, cursor[3]);
                }
                else if (type == 9 && length >= 16)
                {
                    const uint32_t apic_id = *reinterpret_cast<const uint32_t *>(cursor + 4);
                    const uint32_t flags = *reinterpret_cast<const uint32_t *>(cursor + 8);
                    if ((flags & 1U) != 0)
                        add_cpu(topology, apic_id);
                }
                cursor += length;
            }
        }
    }

    CpuTopology discover_cpus()
    {
        CpuTopology topology{};
        const uint32_t rsdp = find_rsdp();
        const uint32_t madt = find_madt(rsdp);
        if (madt != 0)
        {
            topology.acpi_found = true;
            parse_madt(topology, madt);
        }

        if (topology.count == 0)
        {
            uint32_t eax, ebx, ecx, edx;
            asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(1), "c"(0));
            (void)eax;
            (void)ecx;
            (void)edx;
            add_cpu(topology, (ebx >> 24) & 0xFFU);
        }
        return topology;
    }
}
