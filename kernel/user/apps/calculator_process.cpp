// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui.hh>
#include <user/syscall.hh>
#include <user/window_drag.hh>

namespace
{
    constexpr int32_t LIMIT = 999999999;

    class Calculator
    {
    public:
        Calculator() : value_(0), pending_(0), operation_(0), entering_(false),
                       error_(false) { display_[0] = '0'; display_[1] = '\0'; }

        const char *display() const { return error_ ? "Hata" : display_; }

        void key(int32_t key)
        {
            if (key == 'c' || key == 'C') { clear(); return; }
            if (key == '\b')
            {
                if (error_) { clear(); return; }
                uint32_t length = length_();
                if (length > 1) display_[length - 1] = '\0';
                else { display_[0] = '0'; display_[1] = '\0'; }
                value_ = parse();
                entering_ = true;
                return;
            }
            if (error_) return;
            if (key >= '0' && key <= '9')
            {
                if (!entering_) { display_[0] = '0'; display_[1] = '\0'; entering_ = true; }
                const uint32_t length = length_();
                if (length == 1 && display_[0] == '0') display_[0] = static_cast<char>(key);
                else if (length < 9) { display_[length] = static_cast<char>(key); display_[length + 1] = '\0'; }
                value_ = parse();
                return;
            }
            if (key == '\n') key = '=';
            if (key != '+' && key != '-' && key != '*' && key != '/' && key != '=') return;
            if (operation_ != 0 && entering_)
            {
                int32_t next = 0;
                if (!compute(pending_, value_, operation_, &next))
                { error_ = true; return; }
                value_ = next;
                format(value_);
            }
            pending_ = value_;
            operation_ = key == '=' ? 0 : static_cast<char>(key);
            entering_ = false;
        }

    private:
        void clear()
        {
            value_ = pending_ = 0;
            operation_ = 0;
            entering_ = error_ = false;
            display_[0] = '0'; display_[1] = '\0';
        }

        uint32_t length_() const
        {
            uint32_t length = 0;
            while (display_[length] != '\0') ++length;
            return length;
        }

        int32_t parse() const
        {
            int32_t value = 0;
            const bool negative = display_[0] == '-';
            for (uint32_t i = negative ? 1 : 0; display_[i] != '\0'; ++i)
                value = value * 10 + display_[i] - '0';
            return negative ? -value : value;
        }

        void format(int32_t value)
        {
            char reverse[12];
            uint32_t count = 0;
            const bool negative = value < 0;
            uint32_t magnitude = static_cast<uint32_t>(negative ? -value : value);
            do { reverse[count++] = static_cast<char>('0' + magnitude % 10); magnitude /= 10; }
            while (magnitude != 0);
            uint32_t at = 0;
            if (negative) display_[at++] = '-';
            while (count != 0) display_[at++] = reverse[--count];
            display_[at] = '\0';
        }

        bool compute(int32_t a, int32_t b, char op, int32_t *out)
        {
            if (op == '/' && b == 0) return false;
            if (op == '*')
            {
                const uint32_t x = static_cast<uint32_t>(a < 0 ? -a : a);
                const uint32_t y = static_cast<uint32_t>(b < 0 ? -b : b);
                if (y != 0 && x > static_cast<uint32_t>(LIMIT) / y) return false;
                *out = a * b;
            }
            else if (op == '+')
            {
                if ((b > 0 && a > LIMIT - b) || (b < 0 && a < -LIMIT - b)) return false;
                *out = a + b;
            }
            else if (op == '-')
            {
                if ((b < 0 && a > LIMIT + b) || (b > 0 && a < -LIMIT + b)) return false;
                *out = a - b;
            }
            else *out = a / b;
            return true;
        }

        int32_t value_, pending_;
        char operation_;
        bool entering_, error_;
        char display_[16];
    };
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 || screen.width < 400 || screen.height < 300)
        user::exit(-1);
    user::thread_set_name("Hesap makinesi");
    const uint32_t width = 340, height = 385;
    const uint32_t usable_height = screen.height - 48;
    uint32_t x = (screen.width - width) / 2 + 85;
    uint32_t y = (usable_height - height) / 2 + 20;
    if (x + width > screen.width) x = screen.width - width;
    if (y + height > usable_height) y = usable_height - height;
    if (user::graphics_register_window(x, y, width, height) < 0) user::exit(-1);
    user::gui::WindowDrag drag(x, y);
    Calculator calculator;
    int32_t mouse_x = static_cast<int32_t>(screen.width / 2);
    int32_t mouse_y = static_cast<int32_t>(screen.height / 2);
    (void)user::graphics_get_cursor(&mouse_x, &mouse_y);
    uint32_t last_buttons = 0;
    const char keys[4][5] = {"789/", "456*", "123-", "C0=+"};
    for (;;)
    {
        if (user::process_is_minimized())
        {
            last_buttons = 0;
            user::InputEvent ignored{};
            (void)user::wait_event(&ignored, 10);
            continue;
        }
        user::gui::begin_frame();
        drag.restore_old_position(x, y);
        user::gui::window(x, y, width, height, "PCKOS - Hesap Makinesi", true);
        user::gui::frame(x + 18, y + 45, width - 36, 56, user::gui::COLOR_WHITE, true);
        user::gui::push_clip(x + 22, y + 49, width - 44, 48);
        const char *shown = calculator.display();
        const uint32_t text_width = user::gui::text_width(shown);
        user::gui::text(x + width - 28 - (text_width < width - 55 ? text_width : 0),
                        y + 63, shown, user::gui::COLOR_TEXT);
        user::gui::pop_clip();
        for (uint32_t row = 0; row < 4; ++row)
            for (uint32_t column = 0; column < 4; ++column)
            {
                char label[2] = {keys[row][column], '\0'};
                user::gui::button(x + 18 + column * 78, y + 116 + row * 62,
                                  70, 54, label);
            }
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 10) <= 0 || event.type == 3) continue;
        if (event.type == 1)
        {
            if (event.value0 == 0x1B) user::exit(0);
            calculator.key(event.value0);
        }
        else if (event.type == 2)
        {
            int32_t cursor_x = mouse_x, cursor_y = mouse_y;
            if (user::graphics_get_cursor(&cursor_x, &cursor_y) == 0)
            { mouse_x = cursor_x; mouse_y = cursor_y; }
            mouse_x += event.value0;
            mouse_y -= event.value1;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= static_cast<int32_t>(screen.width)) mouse_x = screen.width - 1;
            if (mouse_y >= static_cast<int32_t>(screen.height)) mouse_y = screen.height - 1;
            user::graphics_cursor(mouse_x, mouse_y, true);
            const bool pressed = (event.buttons & 1U) &&
                (!(last_buttons & 1U) || (event.buttons & 0x100U));
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 47) &&
                mouse_x < static_cast<int32_t>(x + width - 28) &&
                mouse_y >= static_cast<int32_t>(y + 5) && mouse_y < static_cast<int32_t>(y + 25))
            { (void)user::process_minimize(); last_buttons = event.buttons; continue; }
            if (pressed && mouse_x >= static_cast<int32_t>(x + width - 27) &&
                mouse_x < static_cast<int32_t>(x + width - 6) &&
                mouse_y >= static_cast<int32_t>(y + 5) && mouse_y < static_cast<int32_t>(y + 25))
                user::exit(0);
            if (pressed && mouse_x >= static_cast<int32_t>(x + 18) &&
                mouse_x < static_cast<int32_t>(x + 18 + 4 * 78) &&
                mouse_y >= static_cast<int32_t>(y + 116) &&
                mouse_y < static_cast<int32_t>(y + 116 + 4 * 62))
            {
                const uint32_t column = (mouse_x - static_cast<int32_t>(x + 18)) / 78;
                const uint32_t row = (mouse_y - static_cast<int32_t>(y + 116)) / 62;
                if ((mouse_x - static_cast<int32_t>(x + 18)) % 78 < 70 &&
                    (mouse_y - static_cast<int32_t>(y + 116)) % 62 < 54)
                    calculator.key(keys[row][column]);
            }
            drag.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                              &x, &y, width, height, screen.width, usable_height);
            last_buttons = event.buttons;
        }
    }
}
