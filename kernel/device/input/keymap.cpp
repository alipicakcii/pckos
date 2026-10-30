// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/input/keymap.hh>

namespace device
{
    namespace
    {
        constexpr uint8_t KEY_HOME = 0xE0;
        constexpr uint8_t KEY_END = 0xE1;
        constexpr uint8_t KEY_UP = 0xE2;
        constexpr uint8_t KEY_DOWN = 0xE3;
        constexpr uint8_t KEY_LEFT = 0xE4;
        constexpr uint8_t KEY_RIGHT = 0xE5;
        constexpr uint8_t KEY_PAGEUP = 0xE6;
        constexpr uint8_t KEY_PAGEDOWN = 0xE7;
        constexpr uint8_t KEY_INSERT = 0xE8;
        constexpr uint8_t KEY_DELETE = 0xE9;

        constexpr uint8_t normal_map[128] = {
            0, 0x1B, '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '-', '=', '\b', '\t',
            'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
            'o', 'p', '[', ']', '\n', 0, 'a', 's',
            'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
            '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
            'b', 'n', 'm', ',', '.', '/', 0, '*',
            0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, KEY_HOME,
            KEY_UP, KEY_PAGEUP, '-', KEY_LEFT, '5', KEY_RIGHT, '+', KEY_END,
            KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, 0
        };

        constexpr uint8_t shifted_map[128] = {
            0, 0x1B, '!', '@', '#', '$', '%', '^',
            '&', '*', '(', ')', '_', '+', '\b', '\t',
            'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
            'O', 'P', '{', '}', '\n', 0, 'A', 'S',
            'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
            '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
            'B', 'N', 'M', '<', '>', '?', 0, '*',
            0, ' ', 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, KEY_HOME,
            KEY_UP, KEY_PAGEUP, '-', KEY_LEFT, '5', KEY_RIGHT, '+', KEY_END,
            KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, 0
        };
    }

    uint8_t keyboard_translate_scancode(uint8_t scancode, bool shift_pressed)
    {
        if (scancode >= sizeof(normal_map))
            return 0;
        return shift_pressed ? shifted_map[scancode] : normal_map[scancode];
    }
}
