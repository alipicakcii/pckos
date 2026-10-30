// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/input/ps2_mouse.hh>
#include <cpu/io.hh>
#include <interrupts/isr.hh>

namespace device
{
    namespace
    {
        constexpr uint16_t DATA_PORT = 0x60;
        constexpr uint16_t STATUS_PORT = 0x64;
        constexpr uint8_t STATUS_OUTPUT_FULL = 0x01;
        constexpr uint8_t STATUS_INPUT_FULL = 0x02;
        constexpr uint8_t MOUSE_ACK = 0xFA;
        constexpr uint8_t QUEUE_SIZE = 16;

        volatile MouseEvent event_queue[QUEUE_SIZE];
        volatile uint8_t queue_read = 0;
        volatile uint8_t queue_write = 0;
        uint8_t packet[3];
        uint8_t packet_index = 0;
        bool mouse_ready = false;
        uint8_t delivered_buttons = 0;

        bool wait_input_empty()
        {
            for (uint32_t timeout = 0; timeout < 100000; ++timeout)
                if ((cpu::inb(STATUS_PORT) & STATUS_INPUT_FULL) == 0)
                    return true;
            return false;
        }

        bool wait_output_full()
        {
            for (uint32_t timeout = 0; timeout < 100000; ++timeout)
                if (cpu::inb(STATUS_PORT) & STATUS_OUTPUT_FULL)
                    return true;
            return false;
        }

        bool controller_command(uint8_t command)
        {
            if (!wait_input_empty())
                return false;
            cpu::outb(STATUS_PORT, command);
            return true;
        }

        bool controller_data(uint8_t data)
        {
            if (!wait_input_empty())
                return false;
            cpu::outb(DATA_PORT, data);
            return true;
        }

        bool send_mouse_command(uint8_t command)
        {
            if (!controller_command(0xD4) || !controller_data(command) || !wait_output_full())
                return false;
            return cpu::inb(DATA_PORT) == MOUSE_ACK;
        }

        void mouse_irq(kernel::Registers *registers)
        {
            (void)registers;
            const uint8_t value = cpu::inb(DATA_PORT);

            if (packet_index == 0 && (value & 0x08) == 0)
                return;
            packet[packet_index++] = value;
            if (packet_index != 3)
                return;
            packet_index = 0;

            const uint8_t next = static_cast<uint8_t>((queue_write + 1) & (QUEUE_SIZE - 1));
            if (next == queue_read)
                return;

            MouseEvent event;
            event.delta_x = (packet[0] & 0x40) ? 0 : static_cast<int32_t>(packet[1]) - ((packet[0] & 0x10) ? 256 : 0);
            event.delta_y = (packet[0] & 0x80) ? 0 : static_cast<int32_t>(packet[2]) - ((packet[0] & 0x20) ? 256 : 0);
            event.buttons = packet[0] & 0x07;
            event_queue[queue_write].delta_x = event.delta_x;
            event_queue[queue_write].delta_y = event.delta_y;
            event_queue[queue_write].buttons = event.buttons;
            queue_write = next;
        }
    }

    bool mouse_read_event(MouseEvent *event)
    {
        if (event == nullptr || queue_read == queue_write)
            return false;

        event->delta_x = event_queue[queue_read].delta_x;
        event->delta_y = event_queue[queue_read].delta_y;
        event->buttons = event_queue[queue_read].buttons;
        queue_read = static_cast<uint8_t>((queue_read + 1) & (QUEUE_SIZE - 1));
        // Coalesce only motion with identical button state. Keep press/release
        // edges queued so a fast drag cannot turn into a missed click.
        const bool button_edge = event->buttons != delivered_buttons;
        delivered_buttons = event->buttons;
        while (!button_edge && queue_read != queue_write &&
               event_queue[queue_read].buttons == event->buttons)
        {
            event->delta_x += event_queue[queue_read].delta_x;
            event->delta_y += event_queue[queue_read].delta_y;
            queue_read = static_cast<uint8_t>((queue_read + 1) & (QUEUE_SIZE - 1));
        }
        return true;
    }

    bool mouse_is_ready()
    {
        return mouse_ready;
    }

    bool mouse_initialize()
    {
        // Drain stale controller bytes before enabling the auxiliary port.
        for (uint32_t count = 0; count < 32; ++count)
        {
            if ((cpu::inb(STATUS_PORT) & STATUS_OUTPUT_FULL) == 0)
                break;
            (void)cpu::inb(DATA_PORT);
        }

        if (!controller_command(0xA8) || !controller_command(0x20) || !wait_output_full())
            return false;

        uint8_t command_byte = cpu::inb(DATA_PORT);
        command_byte = static_cast<uint8_t>((command_byte | 0x02) & ~0x20);
        if (!controller_command(0x60) || !controller_data(command_byte))
            return false;

        if (!send_mouse_command(0xF6) || !send_mouse_command(0xF4))
            return false;

        kernel::interrupt_register(IRQ12, mouse_irq);
        uint8_t slave_mask = cpu::inb(0xA1);
        cpu::outb(0xA1, static_cast<uint8_t>(slave_mask & ~0x10));
        uint8_t master_mask = cpu::inb(0x21);
        cpu::outb(0x21, static_cast<uint8_t>(master_mask & ~0x04));
        mouse_ready = true;
        return true;
    }
}
