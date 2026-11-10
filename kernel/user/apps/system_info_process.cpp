// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui/graphics.hh>
#include <user/input/events.hh>
#include <user/task/process.hh>
#include <user/system/info.hh>
#include <user/task/thread.hh>
#include <user/gui/drawing.hh>
#include <user/gui/window.hh>

namespace
{
    void append_text(char *buffer, uint32_t capacity, uint32_t *length,
                     const char *text)
    {
        while (*text != '\0' && *length + 1 < capacity)
            buffer[(*length)++] = *text++;
        buffer[*length] = '\0';
    }

    void append_number(char *buffer, uint32_t capacity, uint32_t *length,
                       uint32_t value)
    {
        char digits[10];
        uint32_t count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0 && *length + 1 < capacity)
            buffer[(*length)++] = digits[--count];
        buffer[*length] = '\0';
    }

    void draw_row(uint32_t x, uint32_t y, uint32_t width,
                  const char *label, const char *value)
    {
        user::gui::text(x, y, label, user::gui::COLOR_TEXT);
        const uint32_t value_width = user::gui::text_width(value);
        const uint32_t value_x = width > value_width ? x + width - value_width : x;
        user::gui::text(value_x, y, value, user::gui::COLOR_TEXT);
    }

    void draw_system_info(user::gui::Window &window, uint32_t x, uint32_t y,
                          uint32_t width, uint32_t height, const user::SystemInfo &info,
                          const user::GraphicsInfo &graphics)
    {
        window.draw();
        const uint32_t content_x = x + 7;
        const uint32_t content_y = y + 32;
        const uint32_t content_width = width - 14;
        const uint32_t content_height = height - 40;
        user::gui::push_clip(content_x, content_y, content_width, content_height);
        user::gui::text(content_x + 12, content_y + 10,
                        "Donanim ve sistem durumu", user::gui::COLOR_TITLE);

        char value[96];
        uint32_t length = 0;
        append_text(value, sizeof(value), &length,
                    info.cpu_brand[0] != '\0' ? info.cpu_brand : info.cpu_vendor);
        draw_row(content_x + 12, content_y + 40, content_width - 24,
                 "Islemci", value);

        length = 0;
        append_text(value, sizeof(value), &length, info.cpu_vendor);
        draw_row(content_x + 12, content_y + 64, content_width - 24,
                 "Uretici", value);

        length = 0;
        append_text(value, sizeof(value), &length, "Aile ");
        append_number(value, sizeof(value), &length, info.cpu_family);
        append_text(value, sizeof(value), &length, " / Model ");
        append_number(value, sizeof(value), &length, info.cpu_model);
        draw_row(content_x + 12, content_y + 88, content_width - 24,
                 "CPU bilgisi", value);

        length = 0;
        append_number(value, sizeof(value), &length, info.logical_cpu_count);
        draw_row(content_x + 12, content_y + 112, content_width - 24,
                 "Mantiksal islemci", value);

        length = 0;
        append_number(value, sizeof(value), &length, graphics.width);
        append_text(value, sizeof(value), &length, " x ");
        append_number(value, sizeof(value), &length, graphics.height);
        draw_row(content_x + 12, content_y + 136, content_width - 24,
                 "Goruntu alani", value);

        length = 0;
        append_number(value, sizeof(value), &length, info.total_pages / 256);
        append_text(value, sizeof(value), &length, " MiB toplam, ");
        append_number(value, sizeof(value), &length, info.free_pages / 256);
        append_text(value, sizeof(value), &length, " MiB bos");
        draw_row(content_x + 12, content_y + 160, content_width - 24,
                 "Fiziksel bellek", value);

        length = 0;
        append_number(value, sizeof(value), &length, info.kernel_heap_used);
        append_text(value, sizeof(value), &length, " bayt kullaniliyor");
        draw_row(content_x + 12, content_y + 184, content_width - 24,
                 "Cekirdek heap", value);

        length = 0;
        append_number(value, sizeof(value), &length, info.ticks / 100);
        append_text(value, sizeof(value), &length, " saniye");
        draw_row(content_x + 12, content_y + 208, content_width - 24,
                 "Calisma suresi", value);

        length = 0;
        append_text(value, sizeof(value), &length, "100 Hz | ");
        append_number(value, sizeof(value), &length, info.switch_count);
        append_text(value, sizeof(value), &length, " zamanlayici gecisi");
        draw_row(content_x + 12, content_y + 232, content_width - 24,
                 "Zamanlayici", value);

        user::gui::text(content_x + 12, content_y + content_height - 22,
                        "Bilgiler cekirdek sistem cagrilarindan alinir",
                        user::gui::COLOR_DARK);
        user::gui::pop_clip();
    }
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo graphics{};
    if (user::graphics_get_info(&graphics) < 0 ||
        graphics.width < 400 || graphics.height < 300)
        user::exit(-1);

    user::thread_set_name("Sistem bilgisi");
    const uint32_t usable_height = graphics.height - 48;
    const uint32_t width = graphics.width - 20 < 560
        ? graphics.width - 20 : 560;
    const uint32_t height = usable_height - 10 < 390
        ? usable_height - 10 : 390;
    uint32_t x = (graphics.width - width) / 2 + 30;
    uint32_t y = (usable_height - height) / 2 + 16;
    if (x + width > graphics.width) x = graphics.width - width;
    if (y + height > usable_height) y = usable_height - height;
    user::gui::Window window(x, y, width, height, "PCKOS - Sistem Bilgisi");
    if (!window.valid())
        user::exit(-1);
    int32_t mouse_x = static_cast<int32_t>(graphics.width / 2);
    int32_t mouse_y = static_cast<int32_t>(graphics.height / 2);
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

        user::SystemInfo info{};
        if (user::get_system_info(&info) < 0)
            user::exit(-1);
        user::gui::begin_frame();
        x = window.x();
        y = window.y();
        draw_system_info(window, x, y, width, height, info, graphics);
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 100) <= 0 || event.type == 3)
            continue;
        if (event.type == 1)
        {
            if (event.value0 == 0x1B)
                user::exit(0);
        }
        else if (event.type == 2)
        {
            int32_t current_x = mouse_x;
            int32_t current_y = mouse_y;
            if (user::graphics_get_cursor(&current_x, &current_y) == 0)
            {
                mouse_x = current_x;
                mouse_y = current_y;
            }
            mouse_x += event.value0;
            mouse_y -= event.value1;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= static_cast<int32_t>(graphics.width))
                mouse_x = static_cast<int32_t>(graphics.width) - 1;
            if (mouse_y >= static_cast<int32_t>(graphics.height))
                mouse_y = static_cast<int32_t>(graphics.height) - 1;
            user::graphics_cursor(mouse_x, mouse_y, true);
            const bool pressed = (event.buttons & 1U) != 0 &&
                ((last_buttons & 1U) == 0 || (event.buttons & 0x100U) != 0);
            if (window.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                                    graphics.width, usable_height))
            { last_buttons = event.buttons; continue; }
            last_buttons = event.buttons;
        }
    }
}
