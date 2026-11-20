// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/fs/filesystem.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t open(const char *path, uint32_t flags)
    {
        return call(syscall_open, reinterpret_cast<uint32_t>(path), flags);
    }

    int32_t close(int descriptor)
    {
        return call(syscall_close, static_cast<uint32_t>(descriptor));
    }

    int32_t read_directory(uint32_t directory, uint32_t index,
                           DirectoryEntry *entry)
    {
        return call(syscall_readdir, directory, index,
                    reinterpret_cast<uint32_t>(entry));
    }

    int32_t root_inode()
    {
        return call(syscall_get_root);
    }

    int32_t replace_file(const char *path, const void *buffer,
                         uint32_t length)
    {
        return call(syscall_file_replace,
                    reinterpret_cast<uint32_t>(path),
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    int32_t create_file(const char *path)
    {
        return call(syscall_file_create, reinterpret_cast<uint32_t>(path));
    }

    int32_t create_directory(const char *path)
    {
        return call(syscall_directory_create, reinterpret_cast<uint32_t>(path));
    }
}
