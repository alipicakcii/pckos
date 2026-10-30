// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_FS_PCKFS_HH
#define PCKOS_FS_PCKFS_HH

#include <stdint.h>

namespace device { class AtaPioDisk; }

namespace fs
{
    enum class PckfsNodeType : uint8_t
    {
        file = 1,
        directory = 2
    };

    struct PckfsEntry
    {
        uint16_t inode;
        PckfsNodeType type;
        uint32_t size;
        char name[48];
    };

    bool pckfs_mount(device::AtaPioDisk *disk);
    bool pckfs_is_mounted();
    uint32_t pckfs_total_bytes();
    uint32_t pckfs_inode_count();
    uint16_t pckfs_root_inode();
    uint16_t pckfs_parent_inode(uint16_t inode);
    uint32_t pckfs_child_count(uint16_t directory);
    bool pckfs_get_inode(uint16_t inode, PckfsEntry *entry);
    bool pckfs_get_child(uint16_t directory, uint32_t child_index,
                         PckfsEntry *entry);
    bool pckfs_find(const char *path, PckfsEntry *entry);
    bool pckfs_get_path(uint16_t inode, char *path, uint32_t capacity);
    bool pckfs_read_file(uint16_t inode, uint8_t *buffer, uint32_t capacity,
                         uint32_t *bytes_read);
    bool pckfs_read_file_range(uint16_t inode, uint32_t offset,
                               uint8_t *buffer, uint32_t capacity,
                               uint32_t *bytes_read);
}

#endif
