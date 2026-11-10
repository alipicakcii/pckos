// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui/graphics.hh>
#include <user/input/events.hh>
#include <user/task/process.hh>
#include <user/task/thread.hh>
#include <user/gui/drawing.hh>
#include <user/gui/widgets.hh>
#include <user/gui/window.hh>

namespace
{
    constexpr int32_t LIMIT = 999999999;

    class Calculator
    {
    public:
        Calculator() : input_(display_, sizeof(display_)), value_(0), pending_(0),
                       operation_(0), entering_(false), error_(false)
        { input_.set_text("0"); }

        const char *display() const { return error_ ? "Hata" : input_.value(); }
        user::gui::TextInput &input() { return input_; }

        void key(int32_t key)
        {
            if (key == 'c' || key == 'C') { clear(); return; }
            if (key == '\b')
            {
                if (error_) { clear(); return; }
                (void)input_.handle_key(key);
                if (input_.length() == 0) input_.set_text("0");
                value_ = parse();
                entering_ = true;
                return;
            }
            if (key == 0xE0 || key == 0xE1 || key == 0xE4 ||
                key == 0xE5 || key == 0xE9)
            {
                (void)input_.handle_key(key);
                return;
            }
            if (error_) return;
            if (key >= '0' && key <= '9')
            {
                if (!entering_)
                {
                    input_.set_text("0");
                    entering_ = true;
                }
                if (input_.length() == 1 && input_.value()[0] == '0')
                    input_.clear();
                if (input_.length() < 9)
                    (void)input_.handle_key(key);
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
            input_.set_text("0");
        }

        int32_t parse() const
        {
            int32_t value = 0;
            const char *text = input_.value();
            const bool negative = text[0] == '-';
            for (uint32_t i = negative ? 1 : 0; text[i] != '\0'; ++i)
                value = value * 10 + text[i] - '0';
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
            char formatted[16];
            uint32_t at = 0;
            if (negative) formatted[at++] = '-';
            while (count != 0) formatted[at++] = reverse[--count];
            formatted[at] = '\0';
            input_.set_text(formatted);
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

        char display_[16];
        user::gui::TextInput input_;
        int32_t value_, pending_;
        char operation_;
        bool entering_, error_;
    };

    void calculator_button_click(void *context, uint32_t id)
    {
        static_cast<Calculator *>(context)->key(static_cast<int32_t>(id));
    }
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
    user::gui::Window window(x, y, width, height, "PCKOS - Hesap Makinesi");
    if (!window.valid()) user::exit(-1);
    Calculator calculator;
    user::gui::ClickListener button_listener(&calculator_button_click, &calculator);
    user::gui::Button key_7(window, '7', 11, 84, 70, 54, "7", &button_listener);
    user::gui::Button key_8(window, '8', 89, 84, 70, 54, "8", &button_listener);
    user::gui::Button key_9(window, '9', 167, 84, 70, 54, "9", &button_listener);
    user::gui::Button key_divide(window, '/', 245, 84, 70, 54, "/", &button_listener);
    user::gui::Button key_4(window, '4', 11, 146, 70, 54, "4", &button_listener);
    user::gui::Button key_5(window, '5', 89, 146, 70, 54, "5", &button_listener);
    user::gui::Button key_6(window, '6', 167, 146, 70, 54, "6", &button_listener);
    user::gui::Button key_multiply(window, '*', 245, 146, 70, 54, "*", &button_listener);
    user::gui::Button key_1(window, '1', 11, 208, 70, 54, "1", &button_listener);
    user::gui::Button key_2(window, '2', 89, 208, 70, 54, "2", &button_listener);
    user::gui::Button key_3(window, '3', 167, 208, 70, 54, "3", &button_listener);
    user::gui::Button key_subtract(window, '-', 245, 208, 70, 54, "-", &button_listener);
    user::gui::Button key_clear(window, 'C', 11, 270, 70, 54, "C", &button_listener);
    user::gui::Button key_0(window, '0', 89, 270, 70, 54, "0", &button_listener);
    user::gui::Button key_equals(window, '=', 167, 270, 70, 54, "=", &button_listener);
    user::gui::Button key_add(window, '+', 245, 270, 70, 54, "+", &button_listener);
    user::gui::TextInputWidget display_input(
        window, 11, 13, 304, 56, calculator.input());
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
        user::gui::begin_frame();
        x = window.x();
        y = window.y();
        window.draw();
        if (calculator.display()[0] == 'H')
        {
            user::gui::frame(x + 18, y + 45, width - 36, 56,
                             user::gui::COLOR_WHITE, true);
            user::gui::text(x + width - 28 - user::gui::text_width("Hata"),
                            y + 63, "Hata", user::gui::COLOR_TEXT);
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
            if (window.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                                    screen.width, usable_height))
            { last_buttons = event.buttons; continue; }
            last_buttons = event.buttons;
        }
    }
}
