// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/input.hh>
#include <device/input/ps2_keyboard.hh>
#include <device/input/ps2_mouse.hh>
#include <kernel/task/thread.hh>
#include <kernel/task/user_mode.hh>
#include <kernel/syscalls/graphics.hh>
#include <interrupts/interrupts.hh>

namespace kernel::syscalls
{
    namespace
    {
        device::MouseEvent pending_mouse{};
        uint32_t pending_mouse_process = 0;
        uint32_t pending_mouse_buttons = 0;
        uint8_t last_mouse_buttons = 0;
    }

    int32_t read_input(uint32_t user_buffer, uint32_t length)
    {
        if (length > MAX_SYSCALL_TRANSFER)
            return ERROR_INVALID_ARGUMENT;
        if (length != 0 && !active_virtual_memory->is_user_range(
                user_buffer, length, true))
            return ERROR_BAD_ADDRESS;

        char *buffer = reinterpret_cast<char *>(user_buffer);
        if (user_mode_has_terminal())
            return user_mode_stream_read(buffer, length);
        if (process_input_focus() != thread_current_process_id())
            return length == 0 ? 0 : -11;
        uint32_t read_count = 0;
        while (read_count < length)
        {
            const int character = device::keyboard_read_char();
            if (character < 0)
                break;
            buffer[read_count++] = static_cast<char>(character);
        }
        return static_cast<int32_t>(read_count);
    }

    static int32_t wait_for_input(uint32_t user_event, uint32_t timeout_ticks)
    {
        if (!active_virtual_memory->is_user_range(user_event,
                sizeof(abi::InputEvent), true))
            return ERROR_BAD_ADDRESS;

        auto *event = reinterpret_cast<abi::InputEvent *>(user_event);
        const uint32_t started = timer_get_ticks();
        for (;;)
        {
            const uint32_t current_process = thread_current_process_id();
            if (pending_mouse_process == current_process)
            {
                event->type = 2;
                event->value0 = pending_mouse.delta_x;
                event->value1 = pending_mouse.delta_y;
                event->buttons = pending_mouse_buttons;
                pending_mouse_process = 0;
                return 1;
            }
            const bool input_focused = thread_current_process_id() ==
                                       process_input_focus();
            // Ctrl+C, odak uygulamada olsa bile bağlı terminal tarafından işlenir.
            const uint32_t terminal = user_mode_terminal_owner(process_input_focus());
            const bool terminal_interrupt = terminal != 0 &&
                device::keyboard_peek_char() == 3;
            const bool read_key = terminal_interrupt
                ? current_process == terminal : input_focused;
            const int character = read_key ? device::keyboard_read_char() : -1;
            if (character >= 0)
            {
                event->type = 1;
                event->value0 = character;
                event->value1 = 0;
                event->buttons = 0;
                return 1;
            }

            device::MouseEvent mouse;
            if (input_focused && device::mouse_read_event(&mouse))
            {
                const bool pressed = (mouse.buttons & 1U) != 0 &&
                                     (last_mouse_buttons & 1U) == 0;
                last_mouse_buttons = mouse.buttons;
                if (pressed)
                {
                    const uint32_t target = graphics_mouse_target(mouse.delta_x, mouse.delta_y);
                    if (target != current_process)
                    {
                        pending_mouse = mouse;
                        pending_mouse_process = target;
                        pending_mouse_buttons = mouse.buttons | 0x100U;
                        process_set_input_focus(target);
                        syscall_window_raise(target);
                        continue;
                    }
                }
                event->type = 2;
                event->value0 = mouse.delta_x;
                event->value1 = mouse.delta_y;
                event->buttons = mouse.buttons | (pressed ? 0x100U : 0U);
                return 1;
            }

            if (timeout_ticks != 0 && timer_get_ticks() - started >= timeout_ticks)
            {
                *event = {3, 0, 0, 0};
                return 1;
            }
            thread_set_waiting_for_input(true);
            thread_sleep(1);
            thread_set_waiting_for_input(false);
        }
    }

    void handle_input(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_wait_event:
                registers->eax = static_cast<uint32_t>(wait_for_input(registers->ebx, registers->ecx));
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
