// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_FILESYSTEM_HH
#define PCKOS_USER_FILESYSTEM_HH

#include <abi/filesystem.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::DirectoryEntry;
    using abi::directory_type;

    inline int32_t open(const char *path, uint32_t flags = 0)
    {
        return call(syscall_open, reinterpret_cast<uint32_t>(path), flags);
    }

    inline int32_t close(int descriptor)
    {
        return call(syscall_close, static_cast<uint32_t>(descriptor));
    }

    inline int32_t read_directory(uint32_t directory, uint32_t index,
                                  DirectoryEntry *entry)
    {
        return call(syscall_readdir, directory, index,
                    reinterpret_cast<uint32_t>(entry));
    }

    inline int32_t root_inode()
    {
        return call(syscall_get_root);
    }
}

#endif
