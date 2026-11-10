// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_LIB_WIDGETS_HH
#define PCKOS_USER_LIB_WIDGETS_HH

#include <stdint.h>
#include <user/gui/drawing.hh>
#include <user/gui/window.hh>

namespace user::gui
{
    class ClickListener
    {
    public:
        using Callback = void (*)(void *context, uint32_t id);

        ClickListener(Callback callback = nullptr, void *context = nullptr);
        void dispatch(uint32_t id) const;

    private:
        Callback callback_;
        void *context_;
    };

    class Button : public Widget
    {
    public:
        Button(uint32_t id, uint32_t x, uint32_t y, uint32_t width,
               uint32_t height, const char *label,
               const ClickListener *listener = nullptr,
               const uint32_t *icon_pixels = nullptr,
               uint32_t icon_width = 0, uint32_t icon_height = 0);
        Button(Window &parent, uint32_t id, uint32_t x, uint32_t y,
               uint32_t width, uint32_t height, const char *label,
               const ClickListener *listener = nullptr,
               const uint32_t *icon_pixels = nullptr,
               uint32_t icon_width = 0, uint32_t icon_height = 0);

        void draw(bool pressed) const;
        void draw() const override;
        void set_pressed(bool pressed);
        bool handle_click(int32_t x, int32_t y) const override;

    private:
        uint32_t id_;
        const char *label_;
        const ClickListener *listener_;
        const uint32_t *icon_pixels_;
        uint32_t icon_width_;
        uint32_t icon_height_;
        bool pressed_;
    };

    class Label : public Widget
    {
    public:
        Label(Window &parent, uint32_t x, uint32_t y, uint32_t width,
              uint32_t height, const char *text, uint32_t color = COLOR_TEXT);
        void draw() const override;

    private:
        const char *text_;
        uint32_t color_;
    };

    class TextInputWidget : public Widget
    {
    public:
        TextInputWidget(Window &parent, uint32_t x, uint32_t y,
                        uint32_t width, uint32_t height, TextInput &input);
        void draw() const override;
        bool handle_click(int32_t x, int32_t y) const override;

    private:
        TextInput *input_;
    };
}

#endif
