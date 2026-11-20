// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_LOADER_ELF_HH
#define PCKOS_KERNEL_LOADER_ELF_HH

#include <stdint.h>

namespace kernel::loader
{
    namespace elf32
    {
        constexpr uint16_t TYPE_EXECUTABLE = 2;
        constexpr uint16_t TYPE_SHARED = 3;
        constexpr uint16_t MACHINE_I386 = 3;
        constexpr uint32_t PROGRAM_LOAD = 1;
        constexpr uint32_t PROGRAM_DYNAMIC = 2;
        constexpr uint32_t PROGRAM_INTERPRETER = 3;
        constexpr uint32_t PROGRAM_TLS = 7;
        constexpr uint32_t FLAG_EXECUTE = 1;
        constexpr uint32_t FLAG_WRITE = 2;
        constexpr uint16_t SECTION_UNDEFINED = 0;
        constexpr uint16_t SECTION_ABSOLUTE = 0xFFF1;

        struct __attribute__((packed)) Header
        {
            uint8_t identification[16];
            uint16_t type;
            uint16_t machine;
            uint32_t version;
            uint32_t entry;
            uint32_t program_header_offset;
            uint32_t section_header_offset;
            uint32_t flags;
            uint16_t header_size;
            uint16_t program_header_size;
            uint16_t program_header_count;
            uint16_t section_header_size;
            uint16_t section_header_count;
            uint16_t section_name_index;
        };

        struct __attribute__((packed)) ProgramHeader
        {
            uint32_t type;
            uint32_t offset;
            uint32_t virtual_address;
            uint32_t physical_address;
            uint32_t file_size;
            uint32_t memory_size;
            uint32_t flags;
            uint32_t alignment;
        };

        struct __attribute__((packed)) Dynamic
        {
            int32_t tag;
            uint32_t value;
        };

        struct __attribute__((packed)) Symbol
        {
            uint32_t name;
            uint32_t value;
            uint32_t size;
            uint8_t info;
            uint8_t other;
            uint16_t section_index;
        };

        struct __attribute__((packed)) Relocation
        {
            uint32_t offset;
            uint32_t info;
        };

        struct ImageView
        {
            const uint8_t *bytes;
            uint32_t size;
            const Header *header;
            const ProgramHeader *program_headers;
        };

        bool inspect(const uint8_t *bytes, uint32_t size, uint16_t expected_type,
                     ImageView *view);
        bool virtual_to_file_offset(const ImageView &view, uint32_t address,
                                    uint32_t length, uint32_t *offset);
    }

    struct Elf32LoadOperations
    {
        void *context;
        bool (*prepare)(void *context, uint32_t main_image_pages);
        bool (*map_page)(void *context, uint32_t address);
        bool (*begin_access)(void *context);
        bool (*copy_to_user)(void *context, uint32_t address,
                             const uint8_t *source, uint32_t size);
        bool (*read_user_word)(void *context, uint32_t address, uint32_t *value);
        bool (*write_user_word)(void *context, uint32_t address, uint32_t value);
        bool (*set_page_writable)(void *context, uint32_t address, bool writable);
        bool (*validate_stack)(void *context);
        bool (*load_library)(void *context, const char *soname,
                             const uint8_t **image, uint32_t *image_size);
        void (*release_library)(void *context, const uint8_t *image);
        void (*end_access)(void *context);
    };

    struct Elf32LoadRequest
    {
        const uint8_t *image;
        uint32_t image_size;
        uint32_t main_image_base;
        uint32_t main_image_limit;
        uint32_t library_area_base;
        uint32_t library_area_size;
        uint32_t page_size;
        const Elf32LoadOperations *operations;
        uint32_t *entry_address;
    };

    class DynamicLinker
    {
    public:
        explicit DynamicLinker(const Elf32LoadRequest &request);
        bool load();

    private:
        const Elf32LoadRequest &request_;
    };
}

#endif
