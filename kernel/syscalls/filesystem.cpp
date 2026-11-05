// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/filesystem.hh>
#include <fs/pckfs.hh>

namespace kernel::syscalls
{
    namespace
    {
        constexpr uint32_t MAX_OPEN_FILES = 8;
        struct OpenFile
        {
            uint16_t inode;
            uint32_t offset;
            bool used;
        };
        OpenFile open_files[MAX_OPEN_FILES]{};
    }

    static int32_t open_file(const Registers *registers)
    {
        if (!fs::pckfs_is_mounted())
            return ERROR_NO_DEVICE;
        if (registers->ecx != 0)
            return ERROR_INVALID_ARGUMENT;

        char path[MAX_USER_PATH];
        if (!copy_user_string(registers->ebx, path, sizeof(path)))
            return ERROR_BAD_ADDRESS;

        fs::PckfsEntry entry;
        if (!fs::pckfs_find(path, &entry))
            return ERROR_NO_ENTRY;
        if (entry.type == fs::PckfsNodeType::directory)
            return ERROR_IS_DIRECTORY;

        for (uint32_t i = 0; i < MAX_OPEN_FILES; ++i)
        {
            if (open_files[i].used)
                continue;
            open_files[i].inode = entry.inode;
            open_files[i].offset = 0;
            open_files[i].used = true;
            return static_cast<int32_t>(i + 3);
        }
        return ERROR_TOO_MANY_FILES;
    }

    int32_t read_file(uint32_t descriptor, uint32_t user_buffer,
                      uint32_t length)
    {
        const uint32_t slot = descriptor - 3;
        if (slot >= MAX_OPEN_FILES || !open_files[slot].used)
            return ERROR_BAD_DESCRIPTOR;
        if (length > MAX_SYSCALL_TRANSFER)
            return ERROR_INVALID_ARGUMENT;
        if (length != 0 && !active_virtual_memory->is_user_range(
                user_buffer, length, true))
            return ERROR_BAD_ADDRESS;

        uint32_t bytes_read = 0;
        uint8_t *buffer = reinterpret_cast<uint8_t *>(user_buffer);
        if (!fs::pckfs_read_file_range(open_files[slot].inode,
                open_files[slot].offset, buffer, length, &bytes_read))
            return ERROR_IO;
        open_files[slot].offset += bytes_read;
        return static_cast<int32_t>(bytes_read);
    }

    static int32_t read_directory(const Registers *registers)
    {
        if (!fs::pckfs_is_mounted())
            return ERROR_NO_DEVICE;
        if (registers->ebx > UINT16_MAX)
            return ERROR_INVALID_ARGUMENT;
        if (!active_virtual_memory->is_user_range(registers->edx,
                sizeof(abi::DirectoryEntry), true))
            return ERROR_BAD_ADDRESS;

        fs::PckfsEntry entry;
        if (!fs::pckfs_get_child(static_cast<uint16_t>(registers->ebx),
                                 registers->ecx, &entry))
            return 0;

        abi::DirectoryEntry output{};
        output.inode = entry.inode;
        output.type = static_cast<uint32_t>(entry.type);
        output.size = entry.size;
        for (uint32_t i = 0; i < sizeof(output.name); ++i)
            output.name[i] = entry.name[i];

        const auto *source = reinterpret_cast<const uint8_t *>(&output);
        auto *destination = reinterpret_cast<uint8_t *>(registers->edx);
        for (uint32_t i = 0; i < sizeof(output); ++i)
            destination[i] = source[i];
        return 1;
    }

    void reset_files()
    {
        for (uint32_t i = 0; i < MAX_OPEN_FILES; ++i)
            open_files[i] = {};
    }

    void handle_filesystem(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_open:
                registers->eax = static_cast<uint32_t>(open_file(registers));
                return;

            case abi::syscall_close:
            {
                if (registers->ebx < 3 || registers->ebx >= 3 + MAX_OPEN_FILES ||
                    !open_files[registers->ebx - 3].used)
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                    return;
                }
                open_files[registers->ebx - 3] = {};
                registers->eax = 0;
                return;
            }

            case abi::syscall_readdir:
                registers->eax = static_cast<uint32_t>(read_directory(registers));
                return;

            case abi::syscall_get_root:
                registers->eax = fs::pckfs_is_mounted()
                    ? fs::pckfs_root_inode()
                    : static_cast<uint32_t>(ERROR_NO_DEVICE);
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
