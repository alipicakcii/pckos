// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/graphics.hh>
#include <user/input.hh>
#include <user/process.hh>
#include <user/system.hh>
#include <user/thread.hh>
#include <user/gui.hh>
#include <user/window_drag.hh>

namespace
{
    const char *month_name(uint32_t month)
    {
        static const char *names[] = {"", "Ocak", "Subat", "Mart", "Nisan",
            "Mayis", "Haziran", "Temmuz", "Agustos", "Eylul", "Ekim",
            "Kasim", "Aralik"};
        return month <= 12 ? names[month] : "Bilinmiyor";
    }

    uint32_t days_in_month(uint32_t year, uint32_t month)
    {
        if (month == 2)
            return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
                ? 29 : 28;
        return month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31;
    }

    uint32_t monday_first_weekday(uint32_t year, uint32_t month)
    {
        int32_t y = static_cast<int32_t>(year);
        int32_t m = static_cast<int32_t>(month);
        if (m < 3) { --y; m += 12; }
        const int32_t zeller = (1 + (13 * (m + 1)) / 5 + y + y / 4 -
                                y / 100 + y / 400) % 7;
        return static_cast<uint32_t>((zeller + 5) % 7);
    }

    void draw_clock(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                    const user::DateTime &now, bool valid)
    {
        user::gui::window(x, y, width, height, "PCKOS - Saat ve Takvim", true);
        const uint32_t left = x + 7, top = y + 32;
        user::gui::push_clip(left, top, width - 14, height - 40);
        user::gui::text(left + 18, top + 14, "SAAT", user::gui::COLOR_TITLE);

        if (valid)
        {
            char time[9] = {
                static_cast<char>('0' + now.hour / 10),
                static_cast<char>('0' + now.hour % 10), ':',
                static_cast<char>('0' + now.minute / 10),
                static_cast<char>('0' + now.minute % 10), ':',
                static_cast<char>('0' + now.second / 10),
                static_cast<char>('0' + now.second % 10), '\0'
            };
            const uint32_t time_width = user::gui::text_width(time);
            user::gui::frame(left + 18, top + 40, width - 50, 56,
                             user::gui::COLOR_WHITE, true);
            user::gui::text(left + (width - time_width) / 2, top + 60,
                            time, user::gui::COLOR_TEXT);

            char date[64];
            uint32_t at = 0;
            date[at++] = static_cast<char>('0' + now.day / 10);
            date[at++] = static_cast<char>('0' + now.day % 10);
            date[at++] = ' ';
            const char *month = month_name(now.month);
            while (*month != '\0' && at + 1 < sizeof(date)) date[at++] = *month++;
            date[at++] = ' ';
            date[at++] = static_cast<char>('0' + now.year / 1000 % 10);
            date[at++] = static_cast<char>('0' + now.year / 100 % 10);
            date[at++] = static_cast<char>('0' + now.year / 10 % 10);
            date[at++] = static_cast<char>('0' + now.year % 10);
            date[at] = '\0';
            user::gui::text(left + 18, top + 112, date, user::gui::COLOR_TEXT);

            const char *weekdays[7] = {"Pzt", "Sal", "Car", "Per", "Cum", "Cmt", "Paz"};
            const uint32_t grid_x = left + 18;
            const uint32_t grid_y = top + 148;
            const uint32_t cell_width = (width - 50) / 7;
            for (uint32_t day = 0; day < 7; ++day)
                user::gui::text(grid_x + day * cell_width, grid_y,
                                weekdays[day], user::gui::COLOR_TITLE);

            const uint32_t first_day = monday_first_weekday(now.year, now.month);
            const uint32_t month_days = days_in_month(now.year, now.month);
            for (uint32_t day = 1; day <= month_days; ++day)
            {
                const uint32_t cell = first_day + day - 1;
                char number[3] = {
                    day >= 10 ? static_cast<char>('0' + day / 10) : ' ',
                    static_cast<char>('0' + day % 10), '\0'
                };
                const uint32_t day_x = grid_x + (cell % 7) * cell_width;
                const uint32_t day_y = grid_y + 22 + (cell / 7) * 22;
                if (day == now.day)
                    user::gui::button(day_x - 3, day_y - 3, cell_width - 2,
                                      20, number, true);
                else
                    user::gui::text(day_x, day_y, number, user::gui::COLOR_TEXT);
            }
        }
        else
            user::gui::text(left + 18, top + 54, "RTC okunamadi",
                            user::gui::COLOR_TEXT);

        user::gui::pop_clip();
    }
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 ||
        screen.width < 400 || screen.height < 300)
        user::exit(-1);
    user::thread_set_name("Saat ve Takvim");

    const uint32_t usable_height = screen.height - 48;
    const uint32_t width = screen.width - 20 < 420 ? screen.width - 20 : 420;
    const uint32_t height = usable_height - 10 < 350 ? usable_height - 10 : 350;
    uint32_t x = (screen.width - width) / 2;
    uint32_t y = (usable_height - height) / 2;
    user::gui::WindowDrag drag(x, y);
    if (user::graphics_register_window(x, y, width, height) < 0)
        user::exit(-1);
    int32_t mouse_x = static_cast<int32_t>(screen.width / 2);
    int32_t mouse_y = static_cast<int32_t>(screen.height / 2);
    (void)user::graphics_get_cursor(&mouse_x, &mouse_y);
    uint32_t last_buttons = 0;

    for (;;)
    {
        if (user::process_is_minimized())
        {
            last_buttons = 0;
            user::InputEvent ignored{};
            (void)user::wait_event(&ignored, 10);
            continue;
        }
        user::DateTime now{};
        const bool valid = user::get_datetime(&now) == 0;
        user::gui::begin_frame();
        drag.restore_old_position(x, y);
        draw_clock(x, y, width, height, now, valid);
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 100) <= 0 || event.type == 3)
            continue;
        if (event.type == 1 && event.value0 == 0x1B)
            user::exit(0);
        if (event.type == 2)
        {
            int32_t current_x = mouse_x, current_y = mouse_y;
            if (user::graphics_get_cursor(&current_x, &current_y) == 0)
            { mouse_x = current_x; mouse_y = current_y; }
            mouse_x += event.value0;
            mouse_y -= event.value1;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= static_cast<int32_t>(screen.width)) mouse_x = screen.width - 1;
            if (mouse_y >= static_cast<int32_t>(screen.height)) mouse_y = screen.height - 1;
            user::graphics_cursor(mouse_x, mouse_y, true);
            const bool pressed = (event.buttons & 1U) != 0 &&
                ((last_buttons & 1U) == 0 || (event.buttons & 0x100U) != 0);
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 47) &&
                mouse_x < static_cast<int32_t>(x + width - 28) &&
                mouse_y >= static_cast<int32_t>(y + 5) &&
                mouse_y < static_cast<int32_t>(y + 25))
            { (void)user::process_minimize(); last_buttons = event.buttons; continue; }
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 27) &&
                mouse_x < static_cast<int32_t>(x + width - 6) &&
                mouse_y >= static_cast<int32_t>(y + 5) &&
                mouse_y < static_cast<int32_t>(y + 25))
                user::exit(0);
            drag.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                              &x, &y, width, height, screen.width, usable_height);
            last_buttons = event.buttons;
        }
    }
}
