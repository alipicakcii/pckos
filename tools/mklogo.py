#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
"""Create the 32-bit top-down BMP expected by the PCKOS desktop loader."""

from pathlib import Path
import argparse
import struct

from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=Path("assets/pckos-icon.png"))
    parser.add_argument("--output", type=Path, default=Path("assets/logo.bmp"))
    args = parser.parse_args()

    with Image.open(args.input) as source:
        icon = source.convert("RGBA").resize((128, 128), Image.Resampling.LANCZOS)
    pixels = icon.tobytes("raw", "BGRA")
    dib = bytearray(124)
    struct.pack_into("<IiiHHIIiiII", dib, 0,
                     124, 128, -128, 1, 32, 3, len(pixels), 2835, 2835, 0, 0)
    struct.pack_into("<IIII", dib, 40,
                     0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header = b"BM" + struct.pack("<IHHI", 14 + len(dib) + len(pixels), 0, 0, 138)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + dib + pixels)


if __name__ == "__main__":
    main()
