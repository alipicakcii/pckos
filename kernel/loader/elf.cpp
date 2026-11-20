// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/loader/elf.hh>

namespace kernel::loader::elf32
{
    namespace
    {
        constexpr uint32_t PROGRAM_HEADER_LIMIT = 128;
    }

    bool inspect(const uint8_t *bytes, uint32_t size, uint16_t expected_type,
                 ImageView *view)
    {
        if (bytes == nullptr || view == nullptr || size < sizeof(Header))
            return false;

        const auto *header = reinterpret_cast<const Header *>(bytes);
        if (header->identification[0] != 0x7F ||
            header->identification[1] != 'E' ||
            header->identification[2] != 'L' ||
            header->identification[3] != 'F' ||
            header->identification[4] != 1 ||
            header->identification[5] != 1 ||
            header->identification[6] != 1 ||
            header->type != expected_type || header->machine != MACHINE_I386 ||
            header->version != 1 || header->header_size != sizeof(Header) ||
            header->program_header_size != sizeof(ProgramHeader) ||
            header->program_header_count == 0 ||
            header->program_header_count > PROGRAM_HEADER_LIMIT)
            return false;

        const uint64_t headers_end =
            static_cast<uint64_t>(header->program_header_offset) +
            static_cast<uint64_t>(header->program_header_count) *
                header->program_header_size;
        if (headers_end > size)
            return false;

        view->bytes = bytes;
        view->size = size;
        view->header = header;
        view->program_headers = reinterpret_cast<const ProgramHeader *>(
            bytes + header->program_header_offset);
        return true;
    }

    bool virtual_to_file_offset(const ImageView &view, uint32_t address,
                                uint32_t length, uint32_t *offset)
    {
        if (offset == nullptr)
            return false;
        const uint64_t requested_end = static_cast<uint64_t>(address) + length;
        if (requested_end > UINT32_MAX + 1ULL)
            return false;

        for (uint32_t index = 0;
             index < view.header->program_header_count; ++index)
        {
            const ProgramHeader &segment = view.program_headers[index];
            if (segment.type != PROGRAM_LOAD)
                continue;
            const uint64_t segment_begin = segment.virtual_address;
            const uint64_t segment_file_end = segment_begin + segment.file_size;
            if (address < segment_begin || requested_end > segment_file_end)
                continue;
            const uint64_t file_offset = segment.offset + (address - segment_begin);
            if (file_offset + length > view.size)
                return false;
            *offset = static_cast<uint32_t>(file_offset);
            return true;
        }
        return false;
    }
}
