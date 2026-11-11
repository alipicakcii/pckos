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
        volatile uint32_t pckfs_lock = 0;

        class PckfsGuard
        {
        public:
            PckfsGuard()
            {
                asm volatile("pushfl; popl %0; cli" : "=r"(flags_) :: "memory");
                uint32_t previous = 1;
                while (previous != 0)
                {
                    previous = 1;
                    asm volatile("xchgl %0, %1"
                        : "+r"(previous), "+m"(pckfs_lock) :: "memory");
                    if (previous != 0)
                        asm volatile("pause");
                }
            }

            ~PckfsGuard()
            {
                asm volatile("" ::: "memory");
                pckfs_lock = 0;
                if ((flags_ & 0x200U) != 0)
                    asm volatile("sti" ::: "memory");
            }

        private:
            uint32_t flags_;
        };

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
        PckfsGuard guard;
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

    bool pckfs_replace_file(uint16_t inode, const uint8_t *buffer,
                            uint32_t length)
    {
        constexpr uint32_t MAX_REPLACE_SIZE = 8192;
        constexpr uint32_t MAX_REPLACE_BLOCKS =
            (MAX_REPLACE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        PckfsGuard guard;
        if (!mounted || !valid_inode(inode) ||
            inodes[inode].type != static_cast<uint8_t>(PckfsNodeType::file) ||
            length > MAX_REPLACE_SIZE || (length != 0 && buffer == nullptr))
            return false;

        const uint32_t block_count = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
        uint32_t new_blocks[MAX_REPLACE_BLOCKS]{};
        uint32_t found = 0;
        for (uint32_t block = 1; block <= superblock.data_block_count &&
             found < block_count; ++block)
        {
            if (fat[block] == 0)
                new_blocks[found++] = block;
        }
        if (found != block_count)
            return false;

        uint8_t sector[SECTOR_SIZE];
        for (uint32_t i = 0; i < block_count; ++i)
        {
            for (uint32_t s = 0; s < SECTORS_PER_BLOCK; ++s)
            {
                for (uint32_t byte = 0; byte < SECTOR_SIZE; ++byte)
                {
                    const uint32_t source_offset =
                        i * BLOCK_SIZE + s * SECTOR_SIZE + byte;
                    sector[byte] = source_offset < length
                        ? buffer[source_offset] : 0;
                }
                const uint32_t lba = superblock.data_start_sector +
                    (new_blocks[i] - 1) * SECTORS_PER_BLOCK + s;
                if (!system_disk->write_sector(lba, sector))
                    return false;
            }
        }

        for (uint32_t i = 0; i < block_count; ++i)
        {
            const uint32_t next = i + 1 < block_count
                ? new_blocks[i + 1] : END_OF_CHAIN;
            const uint32_t fat_offset = new_blocks[i] * sizeof(uint32_t);
            const uint32_t fat_lba = superblock.fat_start_sector +
                fat_offset / SECTOR_SIZE;
            if (!system_disk->read_sector(fat_lba, sector))
                return false;
            const uint32_t offset = fat_offset % SECTOR_SIZE;
            sector[offset] = static_cast<uint8_t>(next);
            sector[offset + 1] = static_cast<uint8_t>(next >> 8);
            sector[offset + 2] = static_cast<uint8_t>(next >> 16);
            sector[offset + 3] = static_cast<uint8_t>(next >> 24);
            if (!system_disk->write_sector(fat_lba, sector))
                return false;
            fat[new_blocks[i]] = next;
        }

        const uint32_t old_first_block = inodes[inode].first_block;
        PckfsDiskInode updated = inodes[inode];
        updated.size = length;
        updated.first_block = block_count == 0
            ? END_OF_CHAIN : new_blocks[0];
        const uint32_t inode_offset = inode * sizeof(PckfsDiskInode);
        const uint32_t inode_lba = superblock.inode_start_sector +
            inode_offset / SECTOR_SIZE;
        if (!system_disk->read_sector(inode_lba, sector))
            return false;
        const uint32_t record_offset = inode_offset % SECTOR_SIZE;
        *reinterpret_cast<PckfsDiskInode *>(sector + record_offset) = updated;
        if (!system_disk->write_sector(inode_lba, sector))
            return false;
        inodes[inode] = updated;

        uint32_t old_block = old_first_block;
        uint32_t walked = 0;
        while (old_block != END_OF_CHAIN && old_block != 0 &&
               old_block <= superblock.data_block_count &&
               walked++ < superblock.data_block_count)
        {
            const uint32_t next_old = fat[old_block];
            const uint32_t fat_offset = old_block * sizeof(uint32_t);
            const uint32_t fat_lba = superblock.fat_start_sector +
                fat_offset / SECTOR_SIZE;
            if (next_old == 0 ||
                (next_old != END_OF_CHAIN &&
                 next_old > superblock.data_block_count))
                return false;

            if (!system_disk->read_sector(fat_lba, sector))
                return false;
            const uint32_t offset = fat_offset % SECTOR_SIZE;
            sector[offset] = 0;
            sector[offset + 1] = 0;
            sector[offset + 2] = 0;
            sector[offset + 3] = 0;
            if (!system_disk->write_sector(fat_lba, sector))
                return false;
            fat[old_block] = 0;
            if (next_old == END_OF_CHAIN)
                break;
            old_block = next_old;
        }
        return true;
    }

    bool pckfs_create_file(const char *path, uint16_t *created_inode)
    {
        if (path == nullptr || created_inode == nullptr)
            return false;
        PckfsGuard guard;
        if (!mounted)
            return false;

        char parent_path[128]{};
        char name[48]{};
        uint32_t path_length = 0;
        while (path[path_length] != '\0')
        {
            if (path_length + 1 >= sizeof(parent_path))
                return false;
            ++path_length;
        }
        if (path_length == 0)
            return false;

        uint32_t name_start = path_length;
        while (name_start > 0 && path[name_start - 1] != '/')
            --name_start;
        const uint32_t name_length = path_length - name_start;
        if (name_length == 0 || name_length >= sizeof(name))
            return false;
        for (uint32_t i = 0; i < name_length; ++i)
            name[i] = path[name_start + i];
        if (string_equals(name, ".") || string_equals(name, ".."))
            return false;

        if (name_start == 0)
            parent_path[0] = '/';
        else if (name_start == 1 && path[0] == '/')
            parent_path[0] = '/';
        else
        {
            const uint32_t parent_length = name_start - 1;
            if (parent_length == 0)
                parent_path[0] = '/';
            else
            {
                for (uint32_t i = 0; i < parent_length; ++i)
                    parent_path[i] = path[i];
                parent_path[parent_length] = '\0';
            }
        }

        PckfsEntry parent_entry{};
        if (!pckfs_find(parent_path, &parent_entry) ||
            parent_entry.type != PckfsNodeType::directory)
            return false;
        uint16_t existing = 0;
        if (find_child(parent_entry.inode, name, &existing))
            return false;

        uint16_t free_inode = UINT16_MAX;
        for (uint16_t i = 0; i < superblock.inode_count; ++i)
        {
            if (!valid_inode(i))
            {
                free_inode = i;
                break;
            }
        }
        if (free_inode == UINT16_MAX)
            return false;

        PckfsDiskInode created{};
        for (uint32_t i = 0; i < sizeof(created.name); ++i)
            created.name[i] = name[i];
        created.size = 0;
        created.first_block = END_OF_CHAIN;
        created.parent = parent_entry.inode;
        created.type = static_cast<uint8_t>(PckfsNodeType::file);
        created.flags = NODE_USED;

        const uint32_t inode_offset = free_inode * sizeof(PckfsDiskInode);
        const uint32_t inode_lba = superblock.inode_start_sector +
            inode_offset / SECTOR_SIZE;
        uint8_t sector[SECTOR_SIZE];
        if (!system_disk->read_sector(inode_lba, sector))
            return false;
        *reinterpret_cast<PckfsDiskInode *>(sector + inode_offset % SECTOR_SIZE) = created;
        if (!system_disk->write_sector(inode_lba, sector))
            return false;

        inodes[free_inode] = created;
        *created_inode = free_inode;
        return true;
    }

}
