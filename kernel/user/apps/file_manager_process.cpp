// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/apps/file_manager.hh>
#include <user/gui.hh>
#include <user/syscall.hh>
#include <user/window_drag.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 || screen.width < 400 || screen.height < 240)
        user::exit(-1);
    user::thread_set_name("Dosya yoneticisi");

    user::apps::FileManager manager;
    const uint32_t width = screen.width > 28
        ? (screen.width - 28 < 650 ? screen.width - 28 : 650) : screen.width;
    const uint32_t usable_height = screen.height > 60 ? screen.height - 60 : screen.height;
    const uint32_t height = usable_height < 430 ? usable_height : 430;
    const uint32_t candidate_x = (screen.width - width) / 2 + 25;
    uint32_t x = candidate_x + width > screen.width
        ? screen.width - width : candidate_x;
    const uint32_t candidate_y = (usable_height > height
        ? (usable_height - height) / 2 : 0) + 24;
    uint32_t y = candidate_y + height > usable_height
        ? usable_height - height : candidate_y;
    int32_t mouse_x = static_cast<int32_t>(screen.width / 2);
    int32_t mouse_y = static_cast<int32_t>(screen.height / 2);
    (void)user::graphics_get_cursor(&mouse_x, &mouse_y);
    uint32_t last_buttons = 0;
    user::gui::WindowDrag drag(x, y);
    if (user::graphics_register_window(x, y, width, height) < 0)
        user::exit(-1);
    user::graphics_cursor(mouse_x, mouse_y, true);

    for (;;)
    {
        if (user::process_is_minimized())
        {
            last_buttons = 0;
            user::InputEvent event{};
            (void)user::wait_event(&event, 10);
            continue;
        }
        user::gui::begin_frame();
        drag.restore_old_position(x, y);
        user::gui::window(x, y, width, height, "PCKOS - Dosya Yonetimi", true);
        const uint32_t content_x = x + 7, content_y = y + 32;
        const uint32_t content_width = width - 14, content_height = height - 40;
        user::gui::push_clip(content_x, content_y, content_width, content_height);
        manager.draw(content_x, content_y, content_width, content_height);
        user::gui::pop_clip();
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 10) <= 0 || event.type == 3)
            continue;
        if (event.type == 1)
        {
            if (event.value0 == 0x1B)
                user::exit(0);
            manager.key(event.value0);
        }
        else if (event.type == 2)
        {
            int32_t cursor_x = mouse_x;
            int32_t cursor_y = mouse_y;
            if (user::graphics_get_cursor(&cursor_x, &cursor_y) == 0)
            {
                mouse_x = cursor_x;
                mouse_y = cursor_y;
            }
            mouse_x += event.value0;
            mouse_y -= event.value1;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= static_cast<int32_t>(screen.width))
                mouse_x = static_cast<int32_t>(screen.width) - 1;
            if (mouse_y >= static_cast<int32_t>(screen.height))
                mouse_y = static_cast<int32_t>(screen.height) - 1;
            user::graphics_cursor(mouse_x, mouse_y, true);
            const bool pressed = (event.buttons & 1U) != 0 &&
                ((last_buttons & 1U) == 0 || (event.buttons & 0x100U) != 0);
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 47) &&
                mouse_x < static_cast<int32_t>(x + width - 28) &&
                mouse_y >= static_cast<int32_t>(y + 5) &&
                mouse_y < static_cast<int32_t>(y + 25))
            {
                (void)user::process_minimize();
                last_buttons = event.buttons;
                continue;
            }
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 27) &&
                mouse_x < static_cast<int32_t>(x + width - 6) &&
                mouse_y >= static_cast<int32_t>(y + 5) &&
                mouse_y < static_cast<int32_t>(y + 25))
                user::exit(0);
            if (pressed && mouse_x >= static_cast<int32_t>(content_x) &&
                mouse_x < static_cast<int32_t>(content_x + content_width) &&
                mouse_y >= static_cast<int32_t>(content_y) &&
                mouse_y < static_cast<int32_t>(content_y + content_height))
                manager.click(mouse_x - static_cast<int32_t>(content_x),
                              mouse_y - static_cast<int32_t>(content_y),
                              content_width, content_height);
            drag.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                              &x, &y, width, height, screen.width, usable_height);
            last_buttons = event.buttons;
        }
    }
}
