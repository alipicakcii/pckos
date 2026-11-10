// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui/window.hh>
#include <user/gui/graphics.hh>
#include <user/gui/drawing.hh>
#include <user/task/process.hh>

namespace user::gui
{
    bool Widget::handle_click(int32_t x, int32_t y) const
    {
        (void)x;
        (void)y;
        return false;
    }

    Widget::Widget(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        : parent_(nullptr), x_(x), y_(y), width_(width), height_(height) {}

    Widget::Widget(Window &parent, uint32_t x, uint32_t y,
                   uint32_t width, uint32_t height)
        : parent_(&parent), x_(x), y_(y), width_(width), height_(height)
    {
        (void)parent.add_widget(this);
    }

    uint32_t Widget::screen_x() const
    {
        return parent_ != nullptr ? parent_->content_x() + x_ : x_;
    }

    uint32_t Widget::screen_y() const
    {
        return parent_ != nullptr ? parent_->content_y() + y_ : y_;
    }

    bool Widget::contains(int32_t x, int32_t y) const
    {
        const uint32_t left = screen_x();
        const uint32_t top = screen_y();
        return x >= static_cast<int32_t>(left) && y >= static_cast<int32_t>(top) &&
               x < static_cast<int32_t>(left + width_) &&
               y < static_cast<int32_t>(top + height_);
    }

    Window::Window(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                   const char *title)
        : x_(x), y_(y), width_(width), height_(height), title_(title),
          previous_x_(x), previous_y_(y), valid_(false), dragging_(false),
          grab_x_(0), grab_y_(0), widget_count_(0), widgets_{},
          close_callback_(nullptr), close_context_(nullptr)
    {
        valid_ = user::graphics_register_window(x_, y_, width_, height_) >= 0;
    }

    bool Window::add_widget(Widget *widget)
    {
        if (widget == nullptr)
            return false;
        for (uint32_t i = 0; i < widget_count_; ++i)
            if (widgets_[i] == widget)
                return true;
        if (widget_count_ >= MAX_WIDGETS)
            return false;
        widgets_[widget_count_++] = widget;
        return true;
    }

    void Window::set_close_callback(CloseCallback callback, void *context)
    {
        close_callback_ = callback;
        close_context_ = context;
    }

    void Window::draw(bool active)
    {
        if (!valid_)
            return;
        if (x_ != previous_x_ || y_ != previous_y_)
        {
            (void)user::graphics_move_window(x_, y_);
            previous_x_ = x_;
            previous_y_ = y_;
        }
        user::gui::window(x_, y_, width_, height_, title_, active);
        user::gui::push_clip(content_x(), content_y(), content_width(), content_height());
        for (uint32_t i = 0; i < widget_count_; ++i)
            widgets_[i]->draw();
        user::gui::pop_clip();
    }

    bool Window::handle_mouse(int32_t mouse_x, int32_t mouse_y,
                              uint32_t buttons, bool pressed,
                              uint32_t screen_width, uint32_t usable_height)
    {
        const bool held = (buttons & 1U) != 0;
        const int32_t right = static_cast<int32_t>(x_ + width_);
        if (pressed && mouse_y >= static_cast<int32_t>(y_ + 5) &&
            mouse_y < static_cast<int32_t>(y_ + 25))
        {
            if (mouse_x >= right - 27 && mouse_x < right - 6)
            {
                if (close_callback_ != nullptr)
                    close_callback_(close_context_);
                user::exit(0);
            }
            if (mouse_x >= right - 47 && mouse_x < right - 28)
            {
                (void)user::process_minimize();
                return true;
            }
            if (mouse_x >= static_cast<int32_t>(x_ + 4) &&
                mouse_x < right - 48)
            {
                dragging_ = true;
                grab_x_ = mouse_x - static_cast<int32_t>(x_);
                grab_y_ = mouse_y - static_cast<int32_t>(y_);
                return true;
            }
        }
        if (held && dragging_)
        {
            int32_t next_x = mouse_x - grab_x_;
            int32_t next_y = mouse_y - grab_y_;
            const int32_t max_x = screen_width > width_
                ? static_cast<int32_t>(screen_width - width_) : 0;
            const int32_t max_y = usable_height > height_
                ? static_cast<int32_t>(usable_height - height_) : 0;
            if (next_x < 0) next_x = 0;
            if (next_y < 0) next_y = 0;
            if (next_x > max_x) next_x = max_x;
            if (next_y > max_y) next_y = max_y;
            x_ = static_cast<uint32_t>(next_x);
            y_ = static_cast<uint32_t>(next_y);
            return true;
        }
        if (!held)
            dragging_ = false;
        if (pressed && mouse_x >= static_cast<int32_t>(content_x()) &&
            mouse_y >= static_cast<int32_t>(content_y()) &&
            mouse_x < static_cast<int32_t>(content_x() + content_width()) &&
            mouse_y < static_cast<int32_t>(content_y() + content_height()))
        {
            for (uint32_t i = widget_count_; i != 0; --i)
                if (widgets_[i - 1]->handle_click(mouse_x, mouse_y))
                    return true;
        }
        return false;
    }
}
