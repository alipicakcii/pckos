// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <fs/pckfs.hh>
#include <device/ata_pio.hh>

namespace fs
{
    namespace
    {
        constexpr uint32_t SECTOR_SIZE = 512;
        constexpr uint32_t BLOCK_SIZE = 4096;
        constexpr uint32_t SECTORS_PER_BLOCK = BLOCK_SIZE / SECTOR_SIZE;
        constexpr uint32_t MAX_INODES = 256;
        constexpr uint32_t MAX_BLOCKS = 32768;
        constexpr uint32_t END_OF_CHAIN = 0xFFFFFFFF;
        constexpr uint8_t NODE_USED = 1;

        struct __attribute__((packed)) PckfsSuperblock
        {
            char magic[8];
            uint32_t version;
            uint32_t sector_size;
            uint32_t block_size;
            uint32_t total_sectors;
            uint32_t inode_count;
            uint32_t inode_size;
            uint32_t inode_start_sector;
            uint32_t inode_sector_count;
            uint32_t fat_start_sector;
            uint32_t fat_sector_count;
            uint32_t data_start_sector;
            uint32_t data_block_count;
            uint32_t root_inode;
            uint8_t reserved[SECTOR_SIZE - 60];
        };

        struct __attribute__((packed)) PckfsDiskInode
        {
            char name[48];
            uint32_t size;
            uint32_t first_block;
            uint16_t parent;
            uint8_t type;
            uint8_t flags;
            uint32_t reserved;
        };

        static_assert(sizeof(PckfsSuperblock) == SECTOR_SIZE,
                      "PCKFS superblock must occupy one sector");
        static_assert(sizeof(PckfsDiskInode) == 64,
                      "PCKFS inode layout changed");

        device::AtaPioDisk *system_disk = nullptr;
        PckfsSuperblock superblock{};
        PckfsDiskInode inodes[MAX_INODES]{};
        uint32_t fat[MAX_BLOCKS + 1]{};
        bool mounted = false;

        bool string_equals(const char *left, const char *right)
        {
            while (*left && *right && *left == *right)
            {
                ++left;
                ++right;
            }
            return *left == '\0' && *right == '\0';
        }

        bool valid_inode(uint16_t index)
        {
            return index < superblock.inode_count &&
                   (inodes[index].flags & NODE_USED) != 0;
        }

        void copy_entry(uint16_t index, PckfsEntry *entry)
        {
            entry->inode = index;
            entry->type = static_cast<PckfsNodeType>(inodes[index].type);
            entry->size = inodes[index].size;
            for (uint32_t i = 0; i < sizeof(entry->name); ++i)
                entry->name[i] = inodes[index].name[i];
        }

        bool find_child(uint16_t directory, const char *name, uint16_t *index)
        {
            if (!valid_inode(directory) ||
                inodes[directory].type != static_cast<uint8_t>(PckfsNodeType::directory))
                return false;
            for (uint16_t i = 0; i < superblock.inode_count; ++i)
            {
                if (!valid_inode(i) || i == superblock.root_inode ||
                    inodes[i].parent != directory)
                    continue;
                if (string_equals(inodes[i].name, name))
                {
                    *index = i;
                    return true;
                }
            }
            return false;
        }
    }

    bool pckfs_mount(device::AtaPioDisk *disk)
    {
        mounted = false;
        system_disk = nullptr;
        if (disk == nullptr || !disk->is_ready())
            return false;

        uint8_t sector[SECTOR_SIZE];
        if (!disk->read_sector(0, sector))
            return false;
        const auto *read_super = reinterpret_cast<const PckfsSuperblock *>(sector);
        const char expected_magic[8] = {'P', 'C', 'K', 'F', 'S', '1', '\0', '\0'};
        for (uint32_t i = 0; i < sizeof(expected_magic); ++i)
            if (read_super->magic[i] != expected_magic[i])
                return false;
        if (read_super->version != 1 || read_super->sector_size != SECTOR_SIZE ||
            read_super->block_size != BLOCK_SIZE || read_super->inode_size != 64 ||
            read_super->inode_count == 0 || read_super->inode_count > MAX_INODES ||
            read_super->root_inode >= read_super->inode_count ||
            read_super->data_block_count == 0 || read_super->data_block_count > MAX_BLOCKS ||
            read_super->total_sectors > disk->sector_count() ||
            read_super->inode_start_sector != 1 ||
            read_super->inode_sector_count <
                (read_super->inode_count * 64 + SECTOR_SIZE - 1) / SECTOR_SIZE ||
            read_super->fat_start_sector <
                read_super->inode_start_sector + read_super->inode_sector_count ||
            read_super->fat_sector_count <
                ((read_super->data_block_count + 1) * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE ||
            read_super->data_start_sector <
                read_super->fat_start_sector + read_super->fat_sector_count ||
            read_super->data_start_sector +
                read_super->data_block_count * SECTORS_PER_BLOCK > read_super->total_sectors)
            return false;

        superblock = *read_super;
        system_disk = disk;

        uint32_t loaded_inodes = 0;
        for (uint32_t s = 0; s < superblock.inode_sector_count &&
             loaded_inodes < superblock.inode_count; ++s)
        {
            if (!system_disk->read_sector(superblock.inode_start_sector + s, sector))
                return false;
            for (uint32_t offset = 0; offset + sizeof(PckfsDiskInode) <= SECTOR_SIZE &&
                 loaded_inodes < superblock.inode_count;
                 offset += sizeof(PckfsDiskInode), ++loaded_inodes)
                inodes[loaded_inodes] =
                    *reinterpret_cast<const PckfsDiskInode *>(sector + offset);
        }

        uint32_t loaded_fat_words = 0;
        const uint32_t required_fat_words = superblock.data_block_count + 1;
        for (uint32_t s = 0; s < superblock.fat_sector_count &&
             loaded_fat_words < required_fat_words; ++s)
        {
            if (!system_disk->read_sector(superblock.fat_start_sector + s, sector))
                return false;
            for (uint32_t offset = 0; offset + 4 <= SECTOR_SIZE &&
                 loaded_fat_words < required_fat_words; offset += 4)
            {
                fat[loaded_fat_words++] = static_cast<uint32_t>(sector[offset]) |
                    (static_cast<uint32_t>(sector[offset + 1]) << 8) |
                    (static_cast<uint32_t>(sector[offset + 2]) << 16) |
                    (static_cast<uint32_t>(sector[offset + 3]) << 24);
            }
        }

        if (loaded_inodes != superblock.inode_count ||
            loaded_fat_words != required_fat_words ||
            !valid_inode(static_cast<uint16_t>(superblock.root_inode)) ||
            inodes[superblock.root_inode].type !=
                static_cast<uint8_t>(PckfsNodeType::directory))
        {
            system_disk = nullptr;
            return false;
        }

        for (uint16_t i = 0; i < superblock.inode_count; ++i)
        {
            if (!valid_inode(i))
                continue;
            if (inodes[i].name[sizeof(inodes[i].name) - 1] != '\0')
            {
                system_disk = nullptr;
                return false;
            }
            if (i != superblock.root_inode &&
                (!valid_inode(inodes[i].parent) ||
                 inodes[inodes[i].parent].type !=
                    static_cast<uint8_t>(PckfsNodeType::directory)))
            {
                system_disk = nullptr;
                return false;
            }
        }

        mounted = true;
        return true;
    }

    bool pckfs_is_mounted()
    {
        return mounted;
    }

    uint32_t pckfs_total_bytes()
    {
        return mounted ? superblock.total_sectors * SECTOR_SIZE : 0;
    }

    uint32_t pckfs_inode_count()
    {
        return mounted ? superblock.inode_count : 0;
    }

    uint16_t pckfs_root_inode()
    {
        return mounted ? static_cast<uint16_t>(superblock.root_inode) : 0;
    }

    uint16_t pckfs_parent_inode(uint16_t inode)
    {
        if (!mounted || !valid_inode(inode) || inode == pckfs_root_inode())
            return pckfs_root_inode();
        return inodes[inode].parent;
    }

    uint32_t pckfs_child_count(uint16_t directory)
    {
        if (!mounted || !valid_inode(directory) ||
            inodes[directory].type != static_cast<uint8_t>(PckfsNodeType::directory))
            return 0;
        uint32_t count = 0;
        for (uint16_t i = 0; i < superblock.inode_count; ++i)
            if (valid_inode(i) && i != superblock.root_inode &&
                inodes[i].parent == directory)
                ++count;
        return count;
    }

    bool pckfs_get_inode(uint16_t inode, PckfsEntry *entry)
    {
        if (!mounted || entry == nullptr || !valid_inode(inode))
            return false;
        copy_entry(inode, entry);
        return true;
    }

    bool pckfs_get_child(uint16_t directory, uint32_t child_index,
                         PckfsEntry *entry)
    {
        if (!mounted || entry == nullptr || !valid_inode(directory) ||
            inodes[directory].type != static_cast<uint8_t>(PckfsNodeType::directory))
            return false;
        for (uint16_t i = 0; i < superblock.inode_count; ++i)
        {
            if (!valid_inode(i) || i == superblock.root_inode ||
                inodes[i].parent != directory)
                continue;
            if (child_index-- == 0)
            {
                copy_entry(i, entry);
                return true;
            }
        }
        return false;
    }

    bool pckfs_find(const char *path, PckfsEntry *entry)
    {
        if (!mounted || path == nullptr || entry == nullptr)
            return false;
        while (*path == '/')
            ++path;

        uint16_t current = pckfs_root_inode();
        while (*path)
        {
            char component[48];
            uint32_t length = 0;
            while (*path && *path != '/')
            {
                if (length + 1 >= sizeof(component))
                    return false;
                component[length++] = *path++;
            }
            component[length] = '\0';
            while (*path == '/')
                ++path;
            if (length == 0 || string_equals(component, "."))
                continue;
            if (string_equals(component, ".."))
            {
                if (current != pckfs_root_inode())
                    current = inodes[current].parent;
                continue;
            }
            uint16_t child;
            if (!find_child(current, component, &child))
                return false;
            current = child;
            if (*path && inodes[current].type !=
                static_cast<uint8_t>(PckfsNodeType::directory))
                return false;
        }
        copy_entry(current, entry);
        return true;
    }

    bool pckfs_get_path(uint16_t inode, char *path, uint32_t capacity)
    {
        if (!mounted || path == nullptr || capacity < 2 || !valid_inode(inode))
            return false;

        uint16_t ancestry[32];
        uint32_t depth = 0;
        uint16_t current = inode;
        while (current != pckfs_root_inode())
        {
            if (depth >= sizeof(ancestry) / sizeof(ancestry[0]))
                return false;
            ancestry[depth++] = current;
            current = inodes[current].parent;
            if (!valid_inode(current))
                return false;
        }

        uint32_t output = 0;
        path[output++] = '/';
        while (depth != 0)
        {
            const char *name = inodes[ancestry[--depth]].name;
            while (*name)
            {
                if (output + 1 >= capacity)
                    return false;
                path[output++] = *name++;
            }
            if (depth != 0)
            {
                if (output + 1 >= capacity)
                    return false;
                path[output++] = '/';
            }
        }
        path[output] = '\0';
        return true;
    }

    bool pckfs_read_file(uint16_t inode, uint8_t *buffer, uint32_t capacity,
                         uint32_t *bytes_read)
    {
        if (!mounted || !valid_inode(inode) || buffer == nullptr || bytes_read == nullptr ||
            inodes[inode].type != static_cast<uint8_t>(PckfsNodeType::file) ||
            inodes[inode].size > capacity)
            return false;

        return pckfs_read_file_range(inode, 0, buffer, capacity, bytes_read);
    }

    bool pckfs_read_file_range(uint16_t inode, uint32_t offset,
                               uint8_t *buffer, uint32_t capacity,
                               uint32_t *bytes_read)
    {
        if (!mounted || !valid_inode(inode) || bytes_read == nullptr ||
            (capacity != 0 && buffer == nullptr) ||
            inodes[inode].type != static_cast<uint8_t>(PckfsNodeType::file) ||
            offset > inodes[inode].size)
            return false;

        *bytes_read = 0;
        uint32_t remaining = inodes[inode].size - offset;
        if (remaining > capacity)
            remaining = capacity;
        if (remaining == 0)
            return true;

        uint32_t block = inodes[inode].first_block;
        uint32_t blocks_to_skip = offset / BLOCK_SIZE;
        uint32_t offset_in_block = offset % BLOCK_SIZE;
        uint32_t walked = 0;
        uint8_t sector[SECTOR_SIZE];
        while (remaining != 0 && walked < superblock.data_block_count)
        {
            if (block == 0 || block > superblock.data_block_count)
                return false;

            if (blocks_to_skip != 0)
            {
                --blocks_to_skip;
                ++walked;
                block = fat[block];
                continue;
            }

            const uint32_t first_sector = superblock.data_start_sector +
                (block - 1) * SECTORS_PER_BLOCK;
            for (uint32_t s = 0; s < SECTORS_PER_BLOCK && remaining != 0; ++s)
            {
                if (offset_in_block >= SECTOR_SIZE)
                {
                    offset_in_block -= SECTOR_SIZE;
                    continue;
                }
                if (!system_disk->read_sector(first_sector + s, sector))
                    return false;
                const uint32_t sector_available = SECTOR_SIZE - offset_in_block;
                const uint32_t copy_size = remaining < sector_available
                    ? remaining : sector_available;
                for (uint32_t i = 0; i < copy_size; ++i)
                    buffer[*bytes_read + i] = sector[offset_in_block + i];
                *bytes_read += copy_size;
                remaining -= copy_size;
                offset_in_block = 0;
            }
            ++walked;
            if (remaining != 0)
            {
                if (fat[block] == END_OF_CHAIN)
                    return false;
                block = fat[block];
            }
        }
        return remaining == 0;
    }

}
