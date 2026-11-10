// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui/widgets.hh>
#include <user/gui/drawing.hh>

namespace user::gui
{
    ClickListener::ClickListener(Callback callback, void *context)
        : callback_(callback), context_(context) {}

    void ClickListener::dispatch(uint32_t id) const
    {
        if (callback_ != nullptr)
            callback_(context_, id);
    }

    Button::Button(uint32_t id, uint32_t x, uint32_t y,
                   uint32_t width, uint32_t height, const char *label,
                   const ClickListener *listener, const uint32_t *icon_pixels,
                   uint32_t icon_width, uint32_t icon_height)
        : Widget(x, y, width, height), id_(id), label_(label),
          listener_(listener), icon_pixels_(icon_pixels),
          icon_width_(icon_width), icon_height_(icon_height), pressed_(false) {}

    Button::Button(Window &parent, uint32_t id, uint32_t x, uint32_t y,
                   uint32_t width, uint32_t height, const char *label,
                   const ClickListener *listener, const uint32_t *icon_pixels,
                   uint32_t icon_width, uint32_t icon_height)
        : Widget(parent, x, y, width, height), id_(id), label_(label),
          listener_(listener), icon_pixels_(icon_pixels),
          icon_width_(icon_width), icon_height_(icon_height), pressed_(false) {}

    void Button::draw(bool pressed) const
    {
        const uint32_t x = screen_x();
        const uint32_t y = screen_y();
        const uint32_t button_width = width();
        const uint32_t button_height = height();
        if (icon_pixels_ == nullptr || icon_width_ == 0 || icon_height_ == 0)
        {
            button(x, y, button_width, button_height, label_, pressed);
            return;
        }
        frame(x, y, button_width, button_height, COLOR_FACE, pressed);
        const uint32_t icon_y = y + (button_height > icon_height_
            ? (button_height - icon_height_) / 2 : 0);
        bitmap(x + 5 + (pressed ? 1 : 0), icon_y + (pressed ? 1 : 0),
               icon_pixels_, icon_width_, icon_height_);
        const uint32_t text_x = x + icon_width_ + 11;
        const uint32_t text_y = y + (button_height > FONT_HEIGHT
            ? (button_height - FONT_HEIGHT) / 2 : 0);
        push_clip(text_x, y + 2, button_width > icon_width_ + 14
            ? button_width - icon_width_ - 14 : 1, button_height - 4);
        text(text_x + (pressed ? 1 : 0), text_y + (pressed ? 1 : 0),
             label_, COLOR_TEXT);
        pop_clip();
    }

    void Button::draw() const
    {
        draw(pressed_);
    }

    void Button::set_pressed(bool pressed)
    {
        pressed_ = pressed;
    }

    bool Button::handle_click(int32_t x, int32_t y) const
    {
        if (!contains(x, y))
            return false;
        if (listener_ != nullptr)
            listener_->dispatch(id_);
        return true;
    }

    Label::Label(Window &parent, uint32_t x, uint32_t y,
                 uint32_t width, uint32_t height, const char *text_value,
                 uint32_t color)
        : Widget(parent, x, y, width, height), text_(text_value), color_(color) {}

    void Label::draw() const
    {
        text(screen_x(), screen_y(), text_, color_);
    }

    TextInputWidget::TextInputWidget(Window &parent, uint32_t x, uint32_t y,
                                     uint32_t width, uint32_t height,
                                     TextInput &input)
        : Widget(parent, x, y, width, height), input_(&input) {}

    void TextInputWidget::draw() const
    {
        input_->draw(screen_x(), screen_y(), width(), height());
    }

    bool TextInputWidget::handle_click(int32_t x, int32_t y) const
    {
        if (!contains(x, y))
            return false;
        input_->place_cursor_from_x(static_cast<uint32_t>(x), screen_x() + 5,
                                    width() > 10 ? width() - 10 : 1);
        return true;
    }
}
