// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_GUI_HH
#define PCKOS_USER_GUI_HH

#include <stdint.h>

namespace user::gui
{
    constexpr uint32_t FONT_WIDTH = 8;
    constexpr uint32_t FONT_HEIGHT = 16;
    constexpr uint32_t COLOR_DESKTOP = 0xFF202B23;
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
    // Kırpma alanları iç içe kesişimlerdir. Her push için eşleşen bir pop gerekir.
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
    void window(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                const char *title, bool active);
    void pckos_mark(uint32_t x, uint32_t y, uint32_t size);
    void icon(uint32_t x, uint32_t y, uint32_t kind);
}

#endif
