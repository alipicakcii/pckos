// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_LIB_WINDOW_HH
#define PCKOS_USER_LIB_WINDOW_HH

#include <stdint.h>

namespace user::gui
{
    class Window;

    class Widget
    {
    public:
        virtual void draw() const = 0;
        virtual bool handle_click(int32_t x, int32_t y) const;

    protected:
        Widget(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        Widget(Window &parent, uint32_t x, uint32_t y,
               uint32_t width, uint32_t height);

        uint32_t screen_x() const;
        uint32_t screen_y() const;
        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }
        bool contains(int32_t x, int32_t y) const;

    private:
        friend class Window;
        Window *parent_;
        uint32_t x_;
        uint32_t y_;
        uint32_t width_;
        uint32_t height_;
    };

    class Window
    {
    public:
        using CloseCallback = void (*)(void *context);

        Window(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               const char *title);

        bool valid() const { return valid_; }
        uint32_t x() const { return x_; }
        uint32_t y() const { return y_; }
        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }
        uint32_t content_x() const { return x_ + 7; }
        uint32_t content_y() const { return y_ + 32; }
        uint32_t content_width() const { return width_ > 14 ? width_ - 14 : 1; }
        uint32_t content_height() const { return height_ > 40 ? height_ - 40 : 1; }

        bool add_widget(Widget *widget);
        void set_close_callback(CloseCallback callback, void *context = nullptr);
        void draw(bool active = true);
        bool handle_mouse(int32_t mouse_x, int32_t mouse_y, uint32_t buttons,
                          bool pressed, uint32_t screen_width,
                          uint32_t usable_height);

    private:
        static constexpr uint32_t MAX_WIDGETS = 64;
        uint32_t x_;
        uint32_t y_;
        uint32_t width_;
        uint32_t height_;
        const char *title_;
        uint32_t previous_x_;
        uint32_t previous_y_;
        bool valid_;
        bool dragging_;
        int32_t grab_x_;
        int32_t grab_y_;
        uint32_t widget_count_;
        Widget *widgets_[MAX_WIDGETS];
        CloseCallback close_callback_;
        void *close_context_;
    };
}

#endif
