// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_FILESYSTEM_HH
#define PCKOS_ABI_FILESYSTEM_HH

#include <stdint.h>

namespace abi
{
    constexpr uint32_t directory_type = 2;

    struct DirectoryEntry
    {
        uint32_t inode;
        uint32_t type;
        uint32_t size;
        char name[48];
    };

    static_assert(sizeof(DirectoryEntry) == 60);
}

#endif
