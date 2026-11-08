// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/graphics.hh>
#include <user/input.hh>
#include <user/process.hh>
#include <user/thread.hh>
#include <user/apps/task_manager.hh>
#include <user/gui.hh>
#include <user/lib/window.hh>

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 || screen.width < 400 || screen.height < 240)
        user::exit(-1);
    user::thread_set_name("Gorev Yoneticisi");

    user::apps::TaskManager manager;
    (void)user::thread_create(&user::apps::TaskManager::thread_entry,
                              &manager, "Gorev bilgisi");
    const uint32_t width = screen.width > 28
        ? (screen.width - 28 < 720 ? screen.width - 28 : 720) : screen.width;
    const uint32_t usable_height = screen.height > 60 ? screen.height - 60 : screen.height;
    const uint32_t height = usable_height < 470 ? usable_height : 470;
    const uint32_t candidate_x = (screen.width - width) / 2 + 75;
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
    user::gui::Window window(x, y, width, height, "PCKOS - Gorev Yoneticisi");
    if (!window.valid())
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
        x = window.x();
        y = window.y();
        window.draw();
        const uint32_t content_x = x + 7, content_y = y + 32;
        const uint32_t content_width = width - 14, content_height = height - 40;
        user::gui::push_clip(content_x, content_y, content_width, content_height);
        manager.draw(content_x, content_y, content_width, content_height);
        user::gui::pop_clip();
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 10) <= 0 || event.type == 3)
        {
            (void)manager.consume_dirty();
            continue;
        }
        if (event.type == 1 && event.value0 == 0x1B)
            user::exit(0);
        if (event.type == 2)
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
            if (window.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                                    screen.width, usable_height))
            { last_buttons = event.buttons; continue; }
            last_buttons = event.buttons;
        }
    }
}
