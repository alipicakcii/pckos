// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_WINDOW_DRAG_HH
#define PCKOS_USER_WINDOW_DRAG_HH

#include <stdint.h>
#include <user/syscall.hh>

namespace user::gui
{
    class WindowDrag
    {
    public:
        WindowDrag(uint32_t x, uint32_t y)
            : previous_x_(x), previous_y_(y), dragging_(false),
              grab_x_(0), grab_y_(0) {}

        void restore_old_position(uint32_t x, uint32_t y)
        {
            if (x != previous_x_ || y != previous_y_)
                (void)user::graphics_move_window(x, y);
            previous_x_ = x;
            previous_y_ = y;
        }

        void handle_mouse(int32_t mouse_x, int32_t mouse_y, uint32_t buttons,
                          bool pressed, uint32_t *x, uint32_t *y,
                          uint32_t width, uint32_t height,
                          uint32_t screen_width, uint32_t usable_height)
        {
            if (x == nullptr || y == nullptr)
                return;
            const bool held = (buttons & 1U) != 0;
            if (pressed && mouse_x >= static_cast<int32_t>(*x + 4) &&
                mouse_x < static_cast<int32_t>(*x + width - 48) &&
                mouse_y >= static_cast<int32_t>(*y + 4) &&
                mouse_y < static_cast<int32_t>(*y + 28))
            {
                dragging_ = true;
                grab_x_ = mouse_x - static_cast<int32_t>(*x);
                grab_y_ = mouse_y - static_cast<int32_t>(*y);
            }
            if (held && dragging_)
            {
                int32_t next_x = mouse_x - grab_x_;
                int32_t next_y = mouse_y - grab_y_;
                const int32_t maximum_x = screen_width > width
                    ? static_cast<int32_t>(screen_width - width) : 0;
                const int32_t maximum_y = usable_height > height
                    ? static_cast<int32_t>(usable_height - height) : 0;
                if (next_x < 0) next_x = 0;
                if (next_y < 0) next_y = 0;
                if (next_x > maximum_x) next_x = maximum_x;
                if (next_y > maximum_y) next_y = maximum_y;
                *x = static_cast<uint32_t>(next_x);
                *y = static_cast<uint32_t>(next_y);
            }
            if (!held)
                dragging_ = false;
        }

    private:
        uint32_t previous_x_;
        uint32_t previous_y_;
        bool dragging_;
        int32_t grab_x_;
        int32_t grab_y_;
    };
}

#endif
