// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/input/ps2_keyboard.hh>
#include <device/input/keymap.hh>
#include <cpu/io.hh>
#include <interrupts/isr.hh>

namespace device
{
    namespace
    {
        constexpr uint16_t DATA_PORT = 0x60;
        constexpr uint16_t STATUS_PORT = 0x64;
        constexpr uint8_t OUTPUT_FULL = 0x01;
        constexpr uint8_t QUEUE_MASK = 0x7F;

        volatile uint8_t key_queue[QUEUE_MASK + 1];
        volatile uint8_t queue_read = 0;
        volatile uint8_t queue_write = 0;
        bool shift_pressed = false;
        bool extended_scancode = false;

        void queue_character(uint8_t character)
        {
            const uint8_t next = static_cast<uint8_t>((queue_write + 1) & QUEUE_MASK);
            if (next == queue_read)
                return;
            key_queue[queue_write] = character;
            queue_write = next;
        }

        void keyboard_irq(kernel::Registers *registers)
        {
            (void)registers;
            if ((cpu::inb(STATUS_PORT) & OUTPUT_FULL) == 0)
                return;

            const uint8_t scancode = cpu::inb(DATA_PORT);
            if (scancode == 0xE0 || scancode == 0xE1)
            {
                extended_scancode = true;
                return;
            }

            const bool released = (scancode & 0x80) != 0;
            const uint8_t code = scancode & 0x7F;
            if (code == 0x2A || code == 0x36)
            {
                shift_pressed = !released;
                extended_scancode = false;
                return;
            }
            if (released)
            {
                extended_scancode = false;
                return;
            }

            if (extended_scancode)
            {
                extended_scancode = false;
                const uint8_t character = keyboard_translate_scancode(code, shift_pressed);
                if (character != 0)
                    queue_character(character);
                return;
            }

            const uint8_t character = keyboard_translate_scancode(code, shift_pressed);
            if (character != 0)
                queue_character(character);
        }
    }

    void keyboard_initialize()
    {
        kernel::interrupt_register(IRQ1, keyboard_irq);
        for (uint32_t count = 0; count < 32; ++count)
        {
            if ((cpu::inb(STATUS_PORT) & OUTPUT_FULL) == 0)
                break;
            (void)cpu::inb(DATA_PORT);
        }

        const uint8_t master_mask = cpu::inb(0x21);
        cpu::outb(0x21, static_cast<uint8_t>(master_mask & ~0x02));
    }

    int keyboard_read_char()
    {
        if (queue_read == queue_write)
            return -1;
        const uint8_t character = key_queue[queue_read];
        queue_read = static_cast<uint8_t>((queue_read + 1) & QUEUE_MASK);
        return character;
    }
}
