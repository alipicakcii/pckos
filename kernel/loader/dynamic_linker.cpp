// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/loader/elf.hh>

namespace kernel::loader
{
    namespace
    {
        constexpr uint32_t OBJECT_LIMIT = 8;
        constexpr uint32_t NEEDED_LIMIT = 8;
        constexpr uint32_t SONAME_CAPACITY = 64;

        constexpr int32_t DYNAMIC_NULL = 0;
        constexpr int32_t DYNAMIC_NEEDED = 1;
        constexpr int32_t DYNAMIC_PLT_RELOCATION_SIZE = 2;
        constexpr int32_t DYNAMIC_HASH = 4;
        constexpr int32_t DYNAMIC_STRING_TABLE = 5;
        constexpr int32_t DYNAMIC_SYMBOL_TABLE = 6;
        constexpr int32_t DYNAMIC_RELA_SIZE = 8;
        constexpr int32_t DYNAMIC_STRING_SIZE = 10;
        constexpr int32_t DYNAMIC_SYMBOL_SIZE = 11;
        constexpr int32_t DYNAMIC_REL = 17;
        constexpr int32_t DYNAMIC_REL_SIZE = 18;
        constexpr int32_t DYNAMIC_REL_ENTRY_SIZE = 19;
        constexpr int32_t DYNAMIC_PLT_REL_TYPE = 20;
        constexpr int32_t DYNAMIC_JMP_REL = 23;

        constexpr uint32_t RELOCATION_NONE = 0;
        constexpr uint32_t RELOCATION_386_32 = 1;
        constexpr uint32_t RELOCATION_386_PC32 = 2;
        constexpr uint32_t RELOCATION_386_GLOB_DAT = 6;
        constexpr uint32_t RELOCATION_386_JMP_SLOT = 7;
        constexpr uint32_t RELOCATION_386_RELATIVE = 8;

        constexpr uint8_t SYMBOL_BINDING_GLOBAL = 1;
        constexpr uint8_t SYMBOL_BINDING_WEAK = 2;
        constexpr uint8_t SYMBOL_VISIBILITY_DEFAULT = 0;
        constexpr uint8_t SYMBOL_VISIBILITY_PROTECTED = 3;

        struct DynamicInfo
        {
            uint32_t string_table;
            uint32_t string_size;
            uint32_t symbol_table;
            uint32_t symbol_size;
            uint32_t hash_table;
            uint32_t rel_table;
            uint32_t rel_size;
            uint32_t rel_entry_size;
            uint32_t plt_rel_table;
            uint32_t plt_rel_size;
            uint32_t plt_rel_type;
            uint32_t needed[NEEDED_LIMIT];
            uint32_t needed_count;
            bool present;
        };

        struct LoadedObject
        {
            elf32::ImageView view;
            DynamicInfo dynamic;
            uint32_t bias;
            uint32_t mapped_begin;
            uint32_t mapped_end;
            char soname[SONAME_CAPACITY];
        };

        struct AddressSpan
        {
            uint32_t first_page;
            uint32_t end_page;
            bool has_load_segment;
            bool entry_is_executable;
        };

        uint32_t align_down(uint32_t value, uint32_t alignment)
        {
            return value & ~(alignment - 1U);
        }

        uint64_t align_up(uint64_t value, uint32_t alignment)
        {
            return (value + alignment - 1U) &
                   ~static_cast<uint64_t>(alignment - 1U);
        }

        bool image_span(const elf32::ImageView &view, uint32_t bias,
                        uint32_t page_size, AddressSpan *span)
        {
            if (span == nullptr || page_size == 0 ||
                (page_size & (page_size - 1U)) != 0)
                return false;

            span->first_page = UINT32_MAX;
            span->end_page = 0;
            span->has_load_segment = false;
            span->entry_is_executable = false;
            for (uint32_t i = 0; i < view.header->program_header_count; ++i)
            {
                const elf32::ProgramHeader &segment = view.program_headers[i];
                if (segment.type == elf32::PROGRAM_INTERPRETER ||
                    segment.type == elf32::PROGRAM_TLS)
                    return false;
                if (segment.type != elf32::PROGRAM_LOAD)
                    continue;

                const uint64_t file_end =
                    static_cast<uint64_t>(segment.offset) + segment.file_size;
                const uint64_t raw_end =
                    static_cast<uint64_t>(segment.virtual_address) +
                    segment.memory_size;
                const uint64_t runtime_begin =
                    static_cast<uint64_t>(bias) + segment.virtual_address;
                const uint64_t runtime_end = runtime_begin + segment.memory_size;
                if (segment.file_size > segment.memory_size ||
                    file_end > view.size || raw_end > UINT32_MAX + 1ULL ||
                    runtime_end > UINT32_MAX + 1ULL)
                    return false;
                if (segment.alignment > 1 &&
                    ((segment.alignment & (segment.alignment - 1U)) != 0 ||
                     (segment.virtual_address & (segment.alignment - 1U)) !=
                         (segment.offset & (segment.alignment - 1U))))
                    return false;
                if (segment.memory_size == 0)
                    continue;

                const uint32_t first = align_down(
                    static_cast<uint32_t>(runtime_begin), page_size);
                const uint64_t end = align_up(runtime_end, page_size);
                if (end > UINT32_MAX)
                    return false;
                if (first < span->first_page) span->first_page = first;
                if (end > span->end_page) span->end_page = static_cast<uint32_t>(end);
                span->has_load_segment = true;

                if (view.header->type == elf32::TYPE_EXECUTABLE &&
                    (segment.flags & elf32::FLAG_EXECUTE) != 0 &&
                    view.header->entry >= segment.virtual_address &&
                    static_cast<uint64_t>(view.header->entry) < raw_end)
                    span->entry_is_executable = true;
            }
            return span->has_load_segment && span->end_page > span->first_page;
        }

        bool copy_load_segments(const LoadedObject &object,
                                const Elf32LoadOperations &operations)
        {
            for (uint32_t i = 0; i < object.view.header->program_header_count; ++i)
            {
                const elf32::ProgramHeader &segment = object.view.program_headers[i];
                if (segment.type != elf32::PROGRAM_LOAD || segment.file_size == 0)
                    continue;
                const uint64_t address =
                    static_cast<uint64_t>(object.bias) + segment.virtual_address;
                if (address > UINT32_MAX ||
                    !operations.copy_to_user(operations.context,
                        static_cast<uint32_t>(address),
                        object.view.bytes + segment.offset, segment.file_size))
                    return false;
            }
            return true;
        }

        bool parse_dynamic(LoadedObject &object)
        {
            const elf32::ProgramHeader *dynamic_segment = nullptr;
            for (uint32_t i = 0; i < object.view.header->program_header_count; ++i)
                if (object.view.program_headers[i].type == elf32::PROGRAM_DYNAMIC)
                {
                    if (dynamic_segment != nullptr)
                        return false;
                    dynamic_segment = &object.view.program_headers[i];
                }
            if (dynamic_segment == nullptr)
                return true;
            if (dynamic_segment->file_size > dynamic_segment->memory_size ||
                dynamic_segment->file_size % sizeof(elf32::Dynamic) != 0 ||
                static_cast<uint64_t>(dynamic_segment->offset) +
                    dynamic_segment->file_size > object.view.size)
                return false;

            object.dynamic.present = true;
            const auto *entries = reinterpret_cast<const elf32::Dynamic *>(
                object.view.bytes + dynamic_segment->offset);
            const uint32_t count = dynamic_segment->file_size /
                                   sizeof(elf32::Dynamic);
            bool terminated = false;
            for (uint32_t i = 0; i < count; ++i)
            {
                const elf32::Dynamic &entry = entries[i];
                if (entry.tag == DYNAMIC_NULL)
                {
                    terminated = true;
                    break;
                }
                switch (entry.tag)
                {
                case DYNAMIC_NEEDED:
                    if (object.dynamic.needed_count == NEEDED_LIMIT)
                        return false;
                    object.dynamic.needed[object.dynamic.needed_count++] =
                        entry.value;
                    break;
                case DYNAMIC_HASH: object.dynamic.hash_table = entry.value; break;
                case DYNAMIC_STRING_TABLE:
                    object.dynamic.string_table = entry.value;
                    break;
                case DYNAMIC_STRING_SIZE:
                    object.dynamic.string_size = entry.value;
                    break;
                case DYNAMIC_SYMBOL_TABLE:
                    object.dynamic.symbol_table = entry.value;
                    break;
                case DYNAMIC_SYMBOL_SIZE:
                    object.dynamic.symbol_size = entry.value;
                    break;
                case DYNAMIC_REL: object.dynamic.rel_table = entry.value; break;
                case DYNAMIC_REL_SIZE: object.dynamic.rel_size = entry.value; break;
                case DYNAMIC_REL_ENTRY_SIZE:
                    object.dynamic.rel_entry_size = entry.value;
                    break;
                case DYNAMIC_JMP_REL:
                    object.dynamic.plt_rel_table = entry.value;
                    break;
                case DYNAMIC_PLT_RELOCATION_SIZE:
                    object.dynamic.plt_rel_size = entry.value;
                    break;
                case DYNAMIC_PLT_REL_TYPE:
                    object.dynamic.plt_rel_type = entry.value;
                    break;
                case DYNAMIC_RELA_SIZE:
                    if (entry.value != 0) return false;
                    break;
                default: break;
                }
            }
            if (!terminated)
                return false;
            if (object.dynamic.needed_count != 0 &&
                (object.dynamic.string_table == 0 ||
                 object.dynamic.string_size == 0))
                return false;
            if ((object.dynamic.rel_size != 0 ||
                 object.dynamic.plt_rel_size != 0) &&
                (object.dynamic.symbol_table == 0 ||
                 object.dynamic.string_table == 0 ||
                 object.dynamic.hash_table == 0 ||
                 object.dynamic.symbol_size != sizeof(elf32::Symbol)))
                return false;
            return true;
        }

        bool dynamic_string(const LoadedObject &object, uint32_t string_offset,
                            char *destination, uint32_t capacity)
        {
            if (destination == nullptr || capacity == 0 ||
                string_offset >= object.dynamic.string_size)
                return false;
            const uint32_t remaining = object.dynamic.string_size - string_offset;
            const uint64_t string_address =
                static_cast<uint64_t>(object.dynamic.string_table) + string_offset;
            if (string_address > UINT32_MAX)
                return false;
            uint32_t offset = 0;
            if (!elf32::virtual_to_file_offset(object.view,
                    static_cast<uint32_t>(string_address),
                    remaining, &offset))
                return false;
            uint32_t length = 0;
            while (length < remaining && object.view.bytes[offset + length] != 0)
                ++length;
            if (length == remaining || length + 1 > capacity)
                return false;
            for (uint32_t i = 0; i <= length; ++i)
                destination[i] = static_cast<char>(object.view.bytes[offset + i]);
            return true;
        }

        bool same_string(const char *left, const char *right)
        {
            if (left == nullptr || right == nullptr)
                return false;
            while (*left != 0 && *right != 0)
            {
                if (*left++ != *right++) return false;
            }
            return *left == *right;
        }

        bool load_object_segments(LoadedObject &object, uint32_t load_base,
                                  uint32_t page_size, uint32_t area_end,
                                  const Elf32LoadOperations &operations,
                                  uint32_t *next_load_base)
        {
            AddressSpan relative_span{};
            if (!image_span(object.view, 0, page_size, &relative_span))
                return false;
            const uint32_t relative_begin = relative_span.first_page;
            const uint64_t relative_span_size =
                static_cast<uint64_t>(relative_span.end_page) - relative_begin;
            if (relative_span_size == 0 ||
                static_cast<uint64_t>(load_base) + relative_span_size > area_end ||
                relative_begin > load_base)
                return false;

            object.bias = load_base - relative_begin;
            object.mapped_begin = load_base;
            object.mapped_end = load_base +
                                static_cast<uint32_t>(relative_span_size);
            for (uint32_t address = object.mapped_begin;
                 address < object.mapped_end; address += page_size)
                if (!operations.map_page(operations.context, address))
                    return false;
            if (!copy_load_segments(object, operations) || !parse_dynamic(object))
                return false;

            *next_load_base = object.mapped_end;
            return true;
        }

        bool load_needed_objects(LoadedObject *objects, uint32_t *object_count,
                                 uint32_t page_size, uint32_t area_begin,
                                 uint32_t area_end,
                                 const Elf32LoadOperations &operations)
        {
            uint32_t next_load_base = area_begin;
            for (uint32_t owner_index = 0; owner_index < *object_count;
                 ++owner_index)
            {
                LoadedObject &owner = objects[owner_index];
                for (uint32_t needed_index = 0;
                     needed_index < owner.dynamic.needed_count; ++needed_index)
                {
                    char name[SONAME_CAPACITY]{};
                    if (!dynamic_string(owner, owner.dynamic.needed[needed_index],
                                        name, sizeof(name)))
                        return false;
                    bool already_loaded = false;
                    for (uint32_t loaded = 1; loaded < *object_count; ++loaded)
                        if (same_string(objects[loaded].soname, name))
                        {
                            already_loaded = true;
                            break;
                        }
                    if (already_loaded)
                        continue;
                    if (*object_count == OBJECT_LIMIT)
                        return false;

                    const uint8_t *library_image = nullptr;
                    uint32_t library_size = 0;
                    if (!operations.load_library(operations.context, name,
                                                 &library_image, &library_size) ||
                        library_image == nullptr || library_size == 0)
                        return false;

                    LoadedObject &library = objects[*object_count];
                    library.view = {};
                    library.view.bytes = library_image;
                    library.dynamic = {};
                    library.bias = 0;
                    library.mapped_begin = 0;
                    library.mapped_end = 0;
                    for (uint32_t i = 0; i < sizeof(library.soname); ++i)
                        library.soname[i] = name[i];
                    ++*object_count;

                    if (!elf32::inspect(library_image, library_size,
                                        elf32::TYPE_SHARED, &library.view) ||
                        !load_object_segments(library, next_load_base, page_size,
                                              area_end, operations,
                                              &next_load_base))
                        return false;
                }
            }
            return true;
        }

        bool symbol_count(const LoadedObject &object, uint32_t *count)
        {
            if (count == nullptr || object.dynamic.hash_table == 0)
                return false;
            uint32_t offset = 0;
            if (!elf32::virtual_to_file_offset(object.view,
                    object.dynamic.hash_table, 8, &offset))
                return false;
            const auto *hash = reinterpret_cast<const uint32_t *>(
                object.view.bytes + offset);
            if (hash[1] == 0 || hash[1] > 65536)
                return false;
            *count = hash[1];
            return true;
        }

        bool get_symbol(const LoadedObject &object, uint32_t index,
                        elf32::Symbol *symbol)
        {
            uint32_t count = 0;
            if (symbol == nullptr || !symbol_count(object, &count) || index >= count)
                return false;
            const uint64_t address =
                static_cast<uint64_t>(object.dynamic.symbol_table) +
                static_cast<uint64_t>(index) * object.dynamic.symbol_size;
            if (address > UINT32_MAX)
                return false;
            uint32_t offset = 0;
            if (!elf32::virtual_to_file_offset(object.view,
                    static_cast<uint32_t>(address), sizeof(*symbol), &offset))
                return false;
            *symbol = *reinterpret_cast<const elf32::Symbol *>(
                object.view.bytes + offset);
            return true;
        }

        bool symbol_name(const LoadedObject &object, const elf32::Symbol &symbol,
                         const char **name)
        {
            if (name == nullptr || symbol.name >= object.dynamic.string_size)
                return false;
            uint32_t offset = 0;
            const uint32_t remaining =
                object.dynamic.string_size - symbol.name;
            const uint64_t string_address =
                static_cast<uint64_t>(object.dynamic.string_table) + symbol.name;
            if (string_address > UINT32_MAX ||
                !elf32::virtual_to_file_offset(object.view,
                    static_cast<uint32_t>(string_address),
                    remaining, &offset))
                return false;
            bool terminated = false;
            for (uint32_t i = 0; i < remaining; ++i)
                if (object.view.bytes[offset + i] == 0)
                {
                    terminated = true;
                    break;
                }
            if (!terminated)
                return false;
            *name = reinterpret_cast<const char *>(object.view.bytes + offset);
            return true;
        }

        bool resolve_global_symbol(const LoadedObject *objects,
                                   uint32_t object_count, const char *name,
                                   uint32_t *address, bool *found)
        {
            *found = false;
            for (uint32_t object_index = 0; object_index < object_count;
                 ++object_index)
            {
                uint32_t count = 0;
                if (!symbol_count(objects[object_index], &count))
                    continue;
                for (uint32_t index = 1; index < count; ++index)
                {
                    elf32::Symbol symbol{};
                    if (!get_symbol(objects[object_index], index, &symbol))
                        return false;
                    const uint8_t binding = symbol.info >> 4;
                    const uint8_t visibility = symbol.other & 0x3U;
                    if (symbol.section_index == elf32::SECTION_UNDEFINED ||
                        (binding != SYMBOL_BINDING_GLOBAL &&
                         binding != SYMBOL_BINDING_WEAK) ||
                        (visibility != SYMBOL_VISIBILITY_DEFAULT &&
                         visibility != SYMBOL_VISIBILITY_PROTECTED))
                        continue;
                    const char *candidate = nullptr;
                    if (!symbol_name(objects[object_index], symbol, &candidate))
                        return false;
                    if (!same_string(candidate, name))
                        continue;
                    const uint64_t value = symbol.section_index ==
                            elf32::SECTION_ABSOLUTE
                        ? symbol.value
                        : static_cast<uint64_t>(objects[object_index].bias) +
                              symbol.value;
                    if (value > UINT32_MAX)
                        return false;
                    *address = static_cast<uint32_t>(value);
                    *found = true;
                    return true;
                }
            }
            return true;
        }

        bool resolve_relocation_symbol(const LoadedObject *objects,
                                       uint32_t object_count,
                                       const LoadedObject &requester,
                                       uint32_t symbol_index,
                                       uint32_t *address)
        {
            elf32::Symbol symbol{};
            if (!get_symbol(requester, symbol_index, &symbol))
                return false;
            if (symbol.section_index != elf32::SECTION_UNDEFINED)
            {
                if ((symbol.info >> 4) == 0)
                {
                    const uint64_t value = symbol.section_index ==
                            elf32::SECTION_ABSOLUTE
                        ? symbol.value
                        : static_cast<uint64_t>(requester.bias) + symbol.value;
                    if (value > UINT32_MAX) return false;
                    *address = static_cast<uint32_t>(value);
                    return true;
                }
                const char *name = nullptr;
                bool found = false;
                if (!symbol_name(requester, symbol, &name) ||
                    !resolve_global_symbol(objects, object_count, name,
                                           address, &found))
                    return false;
                if (found) return true;
                const uint8_t binding = symbol.info >> 4;
                if (binding == SYMBOL_BINDING_WEAK)
                {
                    *address = 0;
                    return true;
                }
                return false;
            }

            const char *name = nullptr;
            bool found = false;
            if (!symbol_name(requester, symbol, &name) ||
                !resolve_global_symbol(objects, object_count, name,
                                       address, &found))
                return false;
            if (found) return true;
            if ((symbol.info >> 4) == SYMBOL_BINDING_WEAK)
            {
                *address = 0;
                return true;
            }
            return false;
        }

        bool relocate_table(const LoadedObject *objects, uint32_t object_count,
                            const LoadedObject &object, uint32_t table_address,
                            uint32_t table_size,
                            const Elf32LoadOperations &operations)
        {
            if (table_size == 0)
                return true;
            if (table_address == 0 || table_size % sizeof(elf32::Relocation) != 0)
                return false;
            uint32_t offset = 0;
            if (!elf32::virtual_to_file_offset(object.view, table_address,
                                               table_size, &offset))
                return false;
            const auto *entries = reinterpret_cast<const elf32::Relocation *>(
                object.view.bytes + offset);
            const uint32_t count = table_size / sizeof(elf32::Relocation);
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t type = entries[i].info & 0xFFU;
                const uint32_t symbol_index = entries[i].info >> 8;
                if (type == RELOCATION_NONE)
                    continue;
                const uint64_t place_wide =
                    static_cast<uint64_t>(object.bias) + entries[i].offset;
                if (place_wide > UINT32_MAX)
                    return false;
                const uint32_t place = static_cast<uint32_t>(place_wide);
                uint32_t addend = 0;
                if (!operations.read_user_word(operations.context, place, &addend))
                    return false;

                uint32_t result = 0;
                if (type == RELOCATION_386_RELATIVE)
                {
                    result = object.bias + addend;
                }
                else
                {
                    uint32_t symbol_address = 0;
                    if (!resolve_relocation_symbol(objects, object_count, object,
                                                   symbol_index,
                                                   &symbol_address))
                        return false;
                    if (type == RELOCATION_386_32)
                        result = symbol_address + addend;
                    else if (type == RELOCATION_386_PC32)
                        result = symbol_address + addend - place;
                    else if (type == RELOCATION_386_GLOB_DAT ||
                             type == RELOCATION_386_JMP_SLOT)
                        result = symbol_address;
                    else
                        return false;
                }
                if (!operations.write_user_word(operations.context, place, result))
                    return false;
            }
            return true;
        }

        bool relocate_objects(const LoadedObject *objects, uint32_t object_count,
                              const Elf32LoadOperations &operations)
        {
            for (uint32_t i = 0; i < object_count; ++i)
            {
                const LoadedObject &object = objects[i];
                if (object.dynamic.rel_entry_size != 0 &&
                    object.dynamic.rel_entry_size != sizeof(elf32::Relocation))
                    return false;
                if (object.dynamic.plt_rel_size != 0 &&
                    object.dynamic.plt_rel_type != DYNAMIC_REL)
                    return false;
                if (!relocate_table(objects, object_count, object,
                        object.dynamic.rel_table, object.dynamic.rel_size,
                        operations))
                    return false;

                bool plt_already_in_rel = false;
                if (object.dynamic.plt_rel_size != 0 &&
                    object.dynamic.rel_size != 0)
                {
                    const uint64_t rel_end =
                        static_cast<uint64_t>(object.dynamic.rel_table) +
                        object.dynamic.rel_size;
                    const uint64_t plt_end =
                        static_cast<uint64_t>(object.dynamic.plt_rel_table) +
                        object.dynamic.plt_rel_size;
                    plt_already_in_rel =
                        object.dynamic.plt_rel_table >= object.dynamic.rel_table &&
                        plt_end <= rel_end;
                }
                if (!plt_already_in_rel &&
                    !relocate_table(objects, object_count, object,
                        object.dynamic.plt_rel_table,
                        object.dynamic.plt_rel_size, operations))
                    return false;
            }
            return true;
        }

        bool protect_object_pages(const LoadedObject &object, uint32_t page_size,
                                  const Elf32LoadOperations &operations)
        {
            for (uint32_t address = object.mapped_begin;
                 address < object.mapped_end; address += page_size)
            {
                const uint64_t page_end =
                    static_cast<uint64_t>(address) + page_size;
                bool writable = false;
                for (uint32_t i = 0;
                     i < object.view.header->program_header_count; ++i)
                {
                    const elf32::ProgramHeader &segment =
                        object.view.program_headers[i];
                    if (segment.type != elf32::PROGRAM_LOAD ||
                        segment.memory_size == 0 ||
                        (segment.flags & elf32::FLAG_WRITE) == 0)
                        continue;
                    const uint64_t begin =
                        static_cast<uint64_t>(object.bias) +
                        segment.virtual_address;
                    const uint64_t end = begin + segment.memory_size;
                    if (begin < page_end && end > address)
                    {
                        writable = true;
                        break;
                    }
                }
                if (!operations.set_page_writable(operations.context,
                                                   address, writable))
                    return false;
            }
            return true;
        }

        void release_objects(LoadedObject *objects, uint32_t object_count,
                             const Elf32LoadOperations &operations)
        {
            for (uint32_t i = 1; i < object_count; ++i)
                if (objects[i].view.bytes != nullptr)
                    operations.release_library(operations.context,
                                               objects[i].view.bytes);
        }

    }

    DynamicLinker::DynamicLinker(const Elf32LoadRequest &request)
        : request_(request)
    {
    }

    bool DynamicLinker::load()
    {
        const Elf32LoadRequest &request = request_;
        if (request.image == nullptr || request.entry_address == nullptr ||
            request.operations == nullptr || request.image_size == 0 ||
            request.main_image_limit == 0 || request.library_area_size == 0 ||
            request.page_size == 0 ||
            (request.page_size & (request.page_size - 1U)) != 0 ||
            (request.main_image_base & (request.page_size - 1U)) != 0 ||
            (request.library_area_base & (request.page_size - 1U)) != 0 ||
            (request.library_area_size & (request.page_size - 1U)) != 0)
            return false;

        const Elf32LoadOperations &operations = *request.operations;
        if (operations.prepare == nullptr || operations.map_page == nullptr ||
            operations.begin_access == nullptr ||
            operations.copy_to_user == nullptr ||
            operations.read_user_word == nullptr ||
            operations.write_user_word == nullptr ||
            operations.set_page_writable == nullptr ||
            operations.validate_stack == nullptr ||
            operations.load_library == nullptr ||
            operations.release_library == nullptr ||
            operations.end_access == nullptr)
            return false;

        const uint64_t main_limit =
            static_cast<uint64_t>(request.main_image_base) +
            request.main_image_limit;
        const uint64_t library_area_end =
            static_cast<uint64_t>(request.library_area_base) +
            request.library_area_size;
        LoadedObject objects[OBJECT_LIMIT]{};
        uint32_t object_count = 1;
        if (main_limit > UINT32_MAX ||
            library_area_end > UINT32_MAX ||
            !elf32::inspect(request.image, request.image_size,
                            elf32::TYPE_EXECUTABLE, &objects[0].view))
            return false;

        objects[0].bias = 0;
        objects[0].mapped_begin = request.main_image_base;
        if (objects[0].view.header->entry < request.main_image_base)
            return false;
        AddressSpan main_span{};
        if (!image_span(objects[0].view, 0, request.page_size, &main_span) ||
            !main_span.entry_is_executable ||
            main_span.first_page != request.main_image_base ||
            main_span.end_page > main_limit)
            return false;
        objects[0].mapped_end = main_span.end_page;
        const uint32_t main_pages =
            (objects[0].mapped_end - objects[0].mapped_begin) /
            request.page_size;
        if (main_pages == 0 || !parse_dynamic(objects[0]) ||
            !operations.prepare(operations.context, main_pages))
            return false;

        for (uint32_t address = objects[0].mapped_begin;
             address < objects[0].mapped_end; address += request.page_size)
            if (!operations.map_page(operations.context, address))
                return false;

        if (!operations.begin_access(operations.context))
            return false;
        bool valid = copy_load_segments(objects[0], operations) &&
            operations.validate_stack(operations.context);
        if (valid)
            valid = load_needed_objects(objects, &object_count,
                request.page_size, request.library_area_base,
                static_cast<uint32_t>(library_area_end), operations);
        if (valid)
            valid = relocate_objects(objects, object_count, operations);
        for (uint32_t i = 0; valid && i < object_count; ++i)
            valid = protect_object_pages(objects[i], request.page_size,
                                         operations);

        operations.end_access(operations.context);
        release_objects(objects, object_count, operations);
        if (!valid)
            return false;
        *request.entry_address = objects[0].view.header->entry;
        return true;
    }
}
