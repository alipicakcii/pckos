// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_GUI_HH
#define PCKOS_USER_GUI_HH

#include <stdint.h>

namespace user::gui
{
    constexpr uint32_t FONT_WIDTH = 8;
    constexpr uint32_t FONT_HEIGHT = 16;
    constexpr uint32_t COLOR_DESKTOP = 0xFF012A11;
    constexpr uint32_t COLOR_FACE = 0xFFC0C0C0;
    constexpr uint32_t COLOR_LIGHT = 0xFFFFFFFF;
    constexpr uint32_t COLOR_DARK = 0xFF404040;
    constexpr uint32_t COLOR_BLACK = 0xFF000000;
    constexpr uint32_t COLOR_TEXT = 0xFF101010;
    constexpr uint32_t COLOR_TITLE = 0xFF35543A;
    constexpr uint32_t COLOR_TITLE_INACTIVE = 0xFF808080;
    constexpr uint32_t COLOR_WHITE = 0xFFFFFFFF;
    constexpr uint32_t COLOR_SELECTION = 0xFF000080;

    // Kare ekran dışında birleştirilir ve yalnızca tamamlandığında gösterilir.
    void begin_frame();
    void end_frame();
    // Kırpma alanları iç içe kesişimlerdir. Her push için eşleşen bir pop gerekir...
    void push_clip(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void pop_clip();
    uint32_t text_width(const char *text);
    void fill(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
              uint32_t color);
    void text(uint32_t x, uint32_t y, const char *value, uint32_t color);
    void bitmap(uint32_t x, uint32_t y, const uint32_t *argb_pixels,
                uint32_t width, uint32_t height);
    void frame(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               uint32_t fill_color, bool sunken = false);
    void button(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                const char *label, bool pressed = false);
    class TextInput
    {
    public:
        TextInput(char *buffer, uint32_t capacity);
        const char *value() const;
        uint32_t length() const;
        uint32_t cursor() const;
        void clear();
        void set_text(const char *value);
        bool handle_key(int32_t key);
        void place_cursor_from_x(uint32_t pixel_x, uint32_t x,
                                 uint32_t width);
        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                  bool focused = true) const;
        void draw_inline(uint32_t x, uint32_t y, uint32_t width,
                         bool focused, uint32_t color,
                         uint32_t cursor_color = COLOR_TITLE,
                         bool block_cursor = false) const;

    private:
        uint32_t visible_start(uint32_t columns) const;
        char *buffer_;
        uint32_t capacity_;
        uint32_t length_;
        uint32_t cursor_;
    };
    void window(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                const char *title, bool active);
    void pckos_mark(uint32_t x, uint32_t y, uint32_t size);
    void icon(uint32_t x, uint32_t y, uint32_t kind);
}

#endif
