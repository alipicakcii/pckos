#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
"""Build a PCKFS v1 disk image from a host directory."""

import argparse
import math
import os
from pathlib import Path
import struct


SECTOR_SIZE = 512
BLOCK_SIZE = 4096
SECTORS_PER_BLOCK = BLOCK_SIZE // SECTOR_SIZE
INODE_COUNT = 256
INODE_SIZE = 64
IMAGE_SIZE_MIB = 32
END_OF_CHAIN = 0xFFFFFFFF
DEFAULT_DIRECTORIES = (
    "boot", "dev", "etc", "home", "mnt", "tmp", "var"
)


def fail(message):
    raise SystemExit(f"mkpckfs: {message}")


def add_host_tree(source, nodes):
    def walk(directory, parent):
        try:
            children = sorted(directory.iterdir(), key=lambda item: item.name.casefold())
        except OSError as error:
            fail(f"cannot read {directory}: {error}")
        for child in children:
            if child.is_symlink():
                fail(f"symbolic links are not supported: {child}")
            relative = parent + (child.name,)
            if len(child.name.encode("utf-8")) > 47:
                fail(f"name is longer than 47 UTF-8 bytes: {child}")
            if child.is_dir():
                existing = nodes.get(relative)
                if existing is False:
                    fail(f"a file conflicts with directory {child}")
                nodes[relative] = True
                walk(child, relative)
            elif child.is_file():
                existing = nodes.get(relative)
                if existing is True:
                    fail(f"a directory conflicts with file {child}")
                nodes[relative] = False
            else:
                fail(f"unsupported host file type: {child}")

    walk(source, ())


def build_image(source, output, size_mib, logo):
    source.mkdir(parents=True, exist_ok=True)
    total_bytes = size_mib * 1024 * 1024
    if total_bytes % SECTOR_SIZE:
        fail("image size must be a multiple of 512 bytes")
    total_sectors = total_bytes // SECTOR_SIZE

    nodes = {(): True}
    for directory in DEFAULT_DIRECTORIES:
        nodes[(directory,)] = True
    extra_files = {}
    if logo.is_file():
        nodes[("system",)] = True
        nodes[("system", "logo.bmp")] = False
        extra_files[("system", "logo.bmp")] = logo
    add_host_tree(source, nodes)

    ordered_paths = sorted((path for path in nodes if path),
                           key=lambda path: (len(path), tuple(part.casefold() for part in path)))
    if len(ordered_paths) + 1 > INODE_COUNT:
        fail(f"too many files and directories (maximum {INODE_COUNT - 1})")

    inode_by_path = {(): 0}
    for index, path in enumerate(ordered_paths, start=1):
        inode_by_path[path] = index

    inode_table_sectors = math.ceil(INODE_COUNT * INODE_SIZE / SECTOR_SIZE)
    inode_start_sector = 1
    fat_start_sector = inode_start_sector + inode_table_sectors
    fat_sector_count = 1
    while True:
        data_start_sector = fat_start_sector + fat_sector_count
        data_block_count = (total_sectors - data_start_sector) // SECTORS_PER_BLOCK
        next_fat_sector_count = math.ceil((data_block_count + 1) * 4 / SECTOR_SIZE)
        if next_fat_sector_count == fat_sector_count:
            break
        fat_sector_count = next_fat_sector_count
    if data_block_count < 1:
        fail("image is too small")

    inode_table = bytearray(INODE_COUNT * INODE_SIZE)
    fat = [0] * (data_block_count + 1)
    inode_table[0:INODE_SIZE] = struct.pack(
        "<48sIIHBBI", b"/", 0, 0xFFFFFFFF, 0xFFFF, 2, 1, 0
    )
    file_data = []
    next_block = 1

    for path in ordered_paths:
        index = inode_by_path[path]
        parent = inode_by_path[path[:-1]]
        name = path[-1].encode("utf-8")
        if nodes[path]:
            size = 0
            first_block = 0xFFFFFFFF
            node_type = 2
        else:
            host_path = extra_files.get(path, source.joinpath(*path))
            try:
                data = host_path.read_bytes()
            except OSError as error:
                fail(f"cannot read {host_path}: {error}")
            if len(data) > 0xFFFFFFFF:
                fail(f"file is too large for PCKFS v1: {host_path}")
            size = len(data)
            node_type = 1
            block_count = math.ceil(size / BLOCK_SIZE)
            if block_count:
                if next_block + block_count - 1 > data_block_count:
                    fail(f"disk image is full while adding {host_path}")
                first_block = next_block
                blocks = list(range(next_block, next_block + block_count))
                for position, block in enumerate(blocks):
                    fat[block] = blocks[position + 1] if position + 1 < len(blocks) else END_OF_CHAIN
                file_data.append((blocks, data, host_path))
                next_block += block_count
            else:
                first_block = 0xFFFFFFFF

        record = struct.pack("<48sIIHBBI", name, size, first_block,
                             parent, node_type, 1, 0)
        start = index * INODE_SIZE
        inode_table[start:start + INODE_SIZE] = record

    superblock = struct.pack(
        "<8s13I", b"PCKFS1\0\0", 1, SECTOR_SIZE, BLOCK_SIZE,
        total_sectors, INODE_COUNT, INODE_SIZE, inode_start_sector,
        inode_table_sectors, fat_start_sector, fat_sector_count,
        data_start_sector, data_block_count, 0
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with output.open("wb") as image:
            image.truncate(total_bytes)
            image.seek(0)
            image.write(superblock)
            image.seek(inode_start_sector * SECTOR_SIZE)
            image.write(inode_table)
            image.seek(fat_start_sector * SECTOR_SIZE)
            image.write(struct.pack(f"<{len(fat)}I", *fat))
            for blocks, data, host_path in file_data:
                cursor = 0
                for block in blocks:
                    sector = data_start_sector + (block - 1) * SECTORS_PER_BLOCK
                    image.seek(sector * SECTOR_SIZE)
                    chunk = data[cursor:cursor + BLOCK_SIZE]
                    image.write(chunk)
                    cursor += len(chunk)
        os.chmod(output, 0o644)
    except OSError as error:
        fail(f"cannot write {output}: {error}")

    print(f"PCKFS hazir: {output}")
    print(f"Boyut: {size_mib} MiB | Dosya/klasor: {len(ordered_paths)}")
    print(f"Host klasoru: {source}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("host-share"))
    parser.add_argument("--output", type=Path, default=Path("pckfs.img"))
    parser.add_argument("--logo", type=Path, default=Path("assets/logo.bmp"))
    parser.add_argument("--size-mib", type=int, default=IMAGE_SIZE_MIB)
    args = parser.parse_args()
    if args.size_mib < 1 or args.size_mib > 512:
        fail("image size must be between 1 and 512 MiB")
    build_image(args.source, args.output, args.size_mib, args.logo)


if __name__ == "__main__":
    main()
