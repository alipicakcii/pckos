// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/graphics.hh>
#include <user/input.hh>
#include <user/process.hh>
#include <user/thread.hh>
#include <user/gui.hh>
#include <user/lib/window.hh>
#include <user/lib/widgets.hh>

namespace
{
    uint32_t parse_value(const char *text)
    {
        uint32_t value = 0;
        for (uint32_t i = 0; text[i] != '\0'; ++i)
        {
            const uint32_t digit = static_cast<uint32_t>(text[i] - '0');
            if (value > (UINT32_MAX - digit) / 10)
                return UINT32_MAX;
            value = value * 10 + digit;
        }
        return value;
    }

    bool input_overflow(const char *text)
    {
        uint32_t value = 0;
        for (uint32_t i = 0; text[i] != '\0'; ++i)
        {
            const uint32_t digit = static_cast<uint32_t>(text[i] - '0');
            if (value > (UINT32_MAX - digit) / 10)
                return true;
            value = value * 10 + digit;
        }
        return false;
    }

    void number(uint32_t value, char *text)
    {
        char reverse[10];
        uint32_t count = 0;
        do
        {
            reverse[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0 && count < sizeof(reverse));
        uint32_t at = 0;
        while (count != 0) text[at++] = reverse[--count];
        text[at] = '\0';
    }

    void scaled_value(uint32_t bytes, uint32_t divisor, char *text)
    {
        number(bytes / divisor, text);
        uint32_t at = 0;
        while (text[at] != '\0') ++at;
        text[at++] = '.';
        const uint32_t remainder = bytes % divisor;
        uint32_t fraction = divisor > UINT32_MAX / 100
            ? remainder / (divisor / 100)
            : remainder * 100 / divisor;
        if (fraction > 99) fraction = 99;
        text[at++] = static_cast<char>('0' + fraction / 10);
        text[at++] = static_cast<char>('0' + fraction % 10);
        text[at] = '\0';
    }

    void draw_converter(user::gui::Window &window, uint32_t x, uint32_t y,
                        uint32_t width,
                        user::gui::TextInput &input, uint32_t unit,
                        bool overflow)
    {
        window.draw();
        const uint32_t left = x + 7, top = y + 32;
        user::gui::push_clip(left, top, width - 14, 290);
        user::gui::text(left + 16, top + 14, "Bellek miktarini girin",
                        user::gui::COLOR_TITLE);
        user::gui::text(left + 16, top + 134,
                        "Sonuc (ikilik birimler)", user::gui::COLOR_TITLE);

        if (overflow)
            user::gui::text(left + 16, top + 164, "Deger tasiyor",
                            user::gui::COLOR_TEXT);
        else
        {
            const uint32_t value = input.value()[0] == '\0'
                ? 0 : parse_value(input.value());
            const uint32_t multiplier = unit == 0 ? 1
                : unit == 1 ? 1024 : 1024 * 1024;
            const uint32_t bytes = value * multiplier;
            char result[32];
            number(bytes, result);
            user::gui::text(left + 16, top + 164, "Bayt", user::gui::COLOR_TEXT);
            user::gui::text(left + 170, top + 164, result, user::gui::COLOR_TEXT);
            scaled_value(bytes, 1024, result);
            user::gui::text(left + 16, top + 190, "KiB", user::gui::COLOR_TEXT);
            user::gui::text(left + 170, top + 190, result, user::gui::COLOR_TEXT);
            scaled_value(bytes, 1024 * 1024, result);
            user::gui::text(left + 16, top + 216, "MiB", user::gui::COLOR_TEXT);
            user::gui::text(left + 170, top + 216, result, user::gui::COLOR_TEXT);
            scaled_value(bytes, 1024 * 1024 * 1024U, result);
            user::gui::text(left + 16, top + 242, "GiB", user::gui::COLOR_TEXT);
            user::gui::text(left + 170, top + 242, result, user::gui::COLOR_TEXT);
        }
        user::gui::text(left + 16, top + 270,
                        "Klavye: sayi gir | Backspace sil | C temizle",
                        user::gui::COLOR_DARK);
        user::gui::pop_clip();
    }

    void unit_button_click(void *context, uint32_t id)
    {
        *static_cast<uint32_t *>(context) = id;
    }
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 ||
        screen.width < 400 || screen.height < 380)
        user::exit(-1);
    user::thread_set_name("Birim donusturucu");

    const uint32_t width = 340, height = 330;
    const uint32_t usable_height = screen.height - 48;
    uint32_t x = (screen.width - width) / 2;
    uint32_t y = (usable_height - height) / 2;
    user::gui::Window window(x, y, width, height, "PCKOS - Birim Donusturucu");
    if (!window.valid())
        user::exit(-1);
    char input_buffer[11] = {};
    user::gui::TextInput input(input_buffer, sizeof(input_buffer));
    input.set_text("0");
    uint32_t unit = 0;
    user::gui::ClickListener unit_listener(unit_button_click, &unit);
    user::gui::Button byte_button(window, 0, 16, 88, 86, 32,
                                  "Bayt", &unit_listener);
    user::gui::Button kib_button(window, 1, 112, 88, 86, 32,
                                 "KiB", &unit_listener);
    user::gui::Button mib_button(window, 2, 208, 88, 86, 32,
                                 "MiB", &unit_listener);
    user::gui::TextInputWidget amount_input(
        window, 16, 42, width - 46, 34, input);
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
        uint32_t value = parse_value(input.value());
        const uint32_t multiplier = unit == 0 ? 1
            : unit == 1 ? 1024 : 1024 * 1024;
        const bool overflow = input_overflow(input.value()) ||
                              value > UINT32_MAX / multiplier;
        byte_button.set_pressed(unit == 0);
        kib_button.set_pressed(unit == 1);
        mib_button.set_pressed(unit == 2);
        user::gui::begin_frame();
        x = window.x();
        y = window.y();
        draw_converter(window, x, y, width, input, unit, overflow);
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 10) <= 0 || event.type == 3)
            continue;
        if (event.type == 1)
        {
            if (event.value0 == 0x1B) user::exit(0);
            if (event.value0 == 'c' || event.value0 == 'C')
                input.set_text("0");
            else if (event.value0 >= '0' && event.value0 <= '9')
            {
                if (input.length() == 1 && input.value()[0] == '0')
                    input.clear();
                (void)input.handle_key(event.value0);
            }
            else if (event.value0 == '\b' || event.value0 == 0xE0 ||
                     event.value0 == 0xE1 || event.value0 == 0xE4 ||
                     event.value0 == 0xE5 || event.value0 == 0xE9)
            {
                (void)input.handle_key(event.value0);
                if (input.length() == 0) input.set_text("0");
            }
        }
        else if (event.type == 2)
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
            if (window.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                                    screen.width, usable_height))
            { last_buttons = event.buttons; continue; }
            last_buttons = event.buttons;
        }
    }
}
