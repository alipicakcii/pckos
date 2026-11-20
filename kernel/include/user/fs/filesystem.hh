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

int32_t open(const char *path, uint32_t flags = 0);

int32_t close(int descriptor);

int32_t read_directory(uint32_t directory, uint32_t index,
                                  DirectoryEntry *entry);

int32_t root_inode();

int32_t replace_file(const char *path, const void *buffer,
                                uint32_t length);

int32_t create_file(const char *path);

int32_t create_directory(const char *path);
}

#endif
