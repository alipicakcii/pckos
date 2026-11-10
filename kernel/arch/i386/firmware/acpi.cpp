// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <firmware/acpi.hh>
#include <firmware/smp.hh>
#include <interrupts/interrupts.hh>
#include <stdint.h>

namespace firmware
{
    namespace
    {
        constexpr uint32_t BIOS_BEGIN = 0x000E0000;
        constexpr uint32_t BIOS_END = 0x00100000;
        constexpr uint32_t ADDRESS_LIMIT = 0xFFFFF000;
        constexpr uint32_t MAX_TABLE_LENGTH = 0x100000;
        constexpr uint32_t PM1_SLP_TYP = 7U << 10;
        constexpr uint32_t PM1_SLP_EN = 1U << 13;
        uint16_t power_control_a = 0;
        uint16_t power_control_b = 0;
        uint32_t sleep_type_a = 0;
        uint32_t sleep_type_b = 0;
        bool power_info_ready = false;

        struct Rsdp
        {
            char signature[8];
            uint8_t checksum;
            char oem_id[6];
            uint8_t revision;
            uint32_t rsdt_address;
        } __attribute__((packed));

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
                if (left[i] != right[i]) return false;
            return true;
        }

        bool checksum_valid(const uint8_t *bytes, uint32_t length)
        {
            uint8_t sum = 0;
            for (uint32_t i = 0; i < length; ++i)
                sum = static_cast<uint8_t>(sum + bytes[i]);
            return sum == 0;
        }

        const SdtHeader *table_at(uint32_t address, uint32_t minimum_length)
        {
            if (address == 0 || address > ADDRESS_LIMIT - sizeof(SdtHeader))
                return nullptr;
            const auto *header = reinterpret_cast<const SdtHeader *>(address);
            if (header->length < minimum_length ||
                header->length > MAX_TABLE_LENGTH ||
                header->length > ADDRESS_LIMIT - address ||
                !checksum_valid(reinterpret_cast<const uint8_t *>(header), header->length))
                return nullptr;
            return header;
        }

        uint32_t find_rsdp()
        {
            const uint16_t ebda_segment = *reinterpret_cast<volatile uint16_t *>(0x40E);
            const uint32_t ebda = static_cast<uint32_t>(ebda_segment) << 4;
            const uint32_t ebda_end = ebda + 1024;
            for (uint32_t address = ebda; address < ebda_end && address < BIOS_BEGIN;
                 address += 16)
            {
                const auto *candidate = reinterpret_cast<const Rsdp *>(address);
                if (matches(candidate->signature, "RSD PTR ", 8) &&
                    checksum_valid(reinterpret_cast<const uint8_t *>(candidate), 20))
                    return address;
            }
            for (uint32_t address = BIOS_BEGIN; address < BIOS_END; address += 16)
            {
                const auto *candidate = reinterpret_cast<const Rsdp *>(address);
                if (matches(candidate->signature, "RSD PTR ", 8) &&
                    checksum_valid(reinterpret_cast<const uint8_t *>(candidate), 20))
                    return address;
            }
            return 0;
        }

        const SdtHeader *find_fadt()
        {
            const uint32_t rsdp_address = find_rsdp();
            if (rsdp_address == 0) return nullptr;
            const auto *rsdp = reinterpret_cast<const Rsdp *>(rsdp_address);
            const SdtHeader *rsdt = table_at(rsdp->rsdt_address, sizeof(SdtHeader));
            if (rsdt == nullptr || !matches(rsdt->signature, "RSDT", 4) ||
                (rsdt->length - sizeof(SdtHeader)) % sizeof(uint32_t) != 0)
                return nullptr;

            const uint32_t *entries = reinterpret_cast<const uint32_t *>(rsdt + 1);
            const uint32_t count =
                (rsdt->length - sizeof(SdtHeader)) / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
            {
                const SdtHeader *table = table_at(entries[i], sizeof(SdtHeader));
                if (table != nullptr && matches(table->signature, "FACP", 4) &&
                    table->length >= 90)
                    return table;
            }
            return nullptr;
        }

        bool read_aml_integer(const uint8_t *&cursor, const uint8_t *end,
                              uint32_t &value)
        {
            if (cursor >= end) return false;
            const uint8_t opcode = *cursor++;
            if (opcode == 0x00) { value = 0; return true; }
            if (opcode == 0x01) { value = 1; return true; }
            if (opcode == 0x0A && end - cursor >= 1)
            {
                value = cursor[0];
                cursor += 1;
                return true;
            }
            if (opcode == 0x0B && end - cursor >= 2)
            {
                value = static_cast<uint32_t>(cursor[0]) |
                    (static_cast<uint32_t>(cursor[1]) << 8);
                cursor += 2;
                return true;
            }
            if (opcode == 0x0C && end - cursor >= 4)
            {
                value = static_cast<uint32_t>(cursor[0]) |
                    (static_cast<uint32_t>(cursor[1]) << 8) |
                    (static_cast<uint32_t>(cursor[2]) << 16) |
                    (static_cast<uint32_t>(cursor[3]) << 24);
                cursor += 4;
                return true;
            }
            return false;
        }

        bool read_sleep_types(uint32_t dsdt_address, uint32_t &type_a,
                              uint32_t &type_b)
        {
            const SdtHeader *dsdt = table_at(dsdt_address, sizeof(SdtHeader));
            if (dsdt == nullptr || !matches(dsdt->signature, "DSDT", 4))
                return false;
            const uint8_t *cursor = reinterpret_cast<const uint8_t *>(dsdt) +
                sizeof(SdtHeader);
            const uint8_t *end = reinterpret_cast<const uint8_t *>(dsdt) + dsdt->length;

            while (cursor + 7 <= end)
            {
                if (cursor[0] != 0x08)
                {
                    ++cursor;
                    continue;
                }
                const uint8_t *name = cursor + 1;
                if (name < end && *name == 0x5C) ++name;
                if (end - name < 5 || !matches(reinterpret_cast<const char *>(name),
                                                "_S5_", 4) || name[4] != 0x12)
                {
                    ++cursor;
                    continue;
                }

                const uint8_t *package_length = name + 5;
                if (package_length >= end) return false;
                const uint8_t follow_bytes = *package_length >> 6;
                if (follow_bytes > 3 || end - package_length <= follow_bytes)
                    return false;
                const uint8_t *elements = package_length + 1 + follow_bytes;
                if (elements >= end || *elements < 2) return false;
                ++elements;
                return read_aml_integer(elements, end, type_a) &&
                       read_aml_integer(elements, end, type_b) &&
                       type_a <= 7 && type_b <= 7;
            }
            return false;
        }

        uint16_t input_word(uint16_t port)
        {
            uint16_t value;
            asm volatile("inw %1, %0" : "=a"(value) : "dN"(port));
            return value;
        }

        void output_word(uint16_t port, uint16_t value)
        {
            asm volatile("outw %0, %1" :: "a"(value), "dN"(port));
        }
    }

    bool initialize_power_off()
    {
        const SdtHeader *fadt = find_fadt();
        if (fadt == nullptr) return false;
        const auto *bytes = reinterpret_cast<const uint8_t *>(fadt);
        const uint32_t dsdt = *reinterpret_cast<const uint32_t *>(bytes + 40);
        const uint32_t pm1a_control = *reinterpret_cast<const uint32_t *>(bytes + 64);
        const uint32_t pm1b_control = *reinterpret_cast<const uint32_t *>(bytes + 68);
        if (pm1a_control == 0 || pm1a_control > 0xFFFF || bytes[89] < 2)
            return false;
        uint32_t type_a = 0, type_b = 0;
        if (!read_sleep_types(dsdt, type_a, type_b)) return false;

        power_control_a = static_cast<uint16_t>(pm1a_control);
        power_control_b = pm1b_control != 0 && pm1b_control <= 0xFFFF
            ? static_cast<uint16_t>(pm1b_control) : 0;
        sleep_type_a = type_a;
        sleep_type_b = type_b;
        power_info_ready = true;
        return true;
    }

    bool power_off()
    {
        if (!power_info_ready) return false;

        // AP’leri durdurup S5 güç durumu yazılana kadar çekirdek durumunu sabit tut.
        kernel::smp::pause_other_cpus();
        cpu::disable_interrupt();
        uint16_t value = input_word(power_control_a);
        value = static_cast<uint16_t>((value & ~PM1_SLP_TYP) |
            (sleep_type_a << 10) | PM1_SLP_EN);
        output_word(power_control_a, value);
        if (power_control_b != 0)
        {
            value = input_word(power_control_b);
            value = static_cast<uint16_t>((value & ~PM1_SLP_TYP) |
                (sleep_type_b << 10) | PM1_SLP_EN);
            output_word(power_control_b, value);
        }
        for (;;) asm volatile("hlt");
    }
}
