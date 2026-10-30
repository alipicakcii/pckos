// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/gui.hh>
#include <user/syscall.hh>

namespace user::gui
{
    void begin_frame()
    {
        (void)user::graphics_begin_frame();
    }

    void end_frame()
    {
        (void)user::graphics_end_frame();
    }

    void push_clip(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        (void)user::graphics_push_clip(x, y, width, height);
    }

    void pop_clip()
    {
        (void)user::graphics_pop_clip();
    }

    uint32_t text_width(const char *value)
    {
        if (value == nullptr)
            return 0;
        uint32_t columns = 0;
        uint32_t longest = 0;
        for (; *value != '\0'; ++value)
        {
            if (*value == '\n')
            {
                if (columns > longest) longest = columns;
                columns = 0;
            }
            else
                ++columns;
        }
        return (columns > longest ? columns : longest) * FONT_WIDTH;
    }

    void fill(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
              uint32_t color)
    {
        if (width != 0 && height != 0)
            (void)user::graphics_rect(x, y, width, height, color);
    }

    void text(uint32_t x, uint32_t y, const char *value, uint32_t color)
    {
        if (value == nullptr)
            return;
        uint32_t length = 0;
        while (value[length] != '\0') ++length;
        (void)user::graphics_text(x, y, value, length, color);
    }

    void bitmap(uint32_t x, uint32_t y, const uint32_t *argb_pixels,
                uint32_t width, uint32_t height)
    {
        (void)user::graphics_bitmap(x, y, argb_pixels, width, height);
    }

    void frame(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               uint32_t fill_color, bool sunken)
    {
        if (width < 2 || height < 2)
            return;
        fill(x, y, width, height, fill_color);
        const uint32_t top_left = sunken ? 0xFF808080 : COLOR_LIGHT;
        const uint32_t bottom_right = sunken ? COLOR_LIGHT : COLOR_DARK;
        fill(x, y, width - 1, 1, top_left);
        fill(x, y, 1, height - 1, top_left);
        fill(x, y + height - 1, width, 1, bottom_right);
        fill(x + width - 1, y, 1, height, bottom_right);
        if (width > 3 && height > 3)
        {
            fill(x + 1, y + 1, width - 3, 1,
                 sunken ? COLOR_DARK : 0xFFDFDFDF);
            fill(x + 1, y + 1, 1, height - 3,
                 sunken ? COLOR_DARK : 0xFFDFDFDF);
            fill(x + 1, y + height - 2, width - 2, 1,
                 sunken ? 0xFFDFDFDF : 0xFF808080);
            fill(x + width - 2, y + 1, 1, height - 2,
                 sunken ? 0xFFDFDFDF : 0xFF808080);
        }
    }

    void button(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                const char *label, bool pressed)
    {
        if (width < 6 || height < 6)
            return;
        frame(x, y, width, height, COLOR_FACE, pressed);
        const uint32_t label_width = text_width(label);
        const uint32_t label_x = width > label_width + 6
            ? x + (width - label_width) / 2 : x + 3;
        const uint32_t label_y = height > FONT_HEIGHT
            ? y + (height - FONT_HEIGHT) / 2 : y;
        push_clip(x + 3, y + 3, width - 6, height - 6);
        text(label_x + (pressed ? 1 : 0), label_y + (pressed ? 1 : 0),
             label, COLOR_TEXT);
        pop_clip();
    }

    void window(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                const char *title, bool active)
    {
        if (width < 64 || height < 36)
            return;
        frame(x, y, width, height, COLOR_FACE);
        const uint32_t title_color = active ? COLOR_TITLE : COLOR_TITLE_INACTIVE;
        fill(x + 4, y + 4, width - 8, 22, title_color);
        push_clip(x + 9, y + 6, width - 58, 18);
        text(x + 9, y + 7, title, COLOR_WHITE);
        pop_clip();

        const uint32_t button_y = y + 6;
        const uint32_t close_x = x + width - 25;
        frame(close_x, button_y, 17, 17, COLOR_FACE);
        frame(close_x - 20, button_y, 17, 17, COLOR_FACE);
        // Başlık çubuğu simgelerini, tam boy yazı glifini 17 piksellik denetime
        // kırpmak yerine düğmelerin içine çiz.
        for (uint32_t i = 0; i < 7; ++i)
        {
            fill(close_x + 5 + i, button_y + 5 + i, 2, 1, COLOR_BLACK);
            fill(close_x + 11 - i, button_y + 5 + i, 2, 1, COLOR_BLACK);
        }
        fill(close_x - 15, button_y + 11, 8, 2, COLOR_BLACK);
        frame(x + 4, y + 28, width - 8, height - 32, COLOR_FACE, true);
    }

    void pckos_mark(uint32_t x, uint32_t y, uint32_t size)
    {
        if (size < 12)
            return;
        const uint32_t ink = 0xFF123050;
        fill(x, y, size, size, 0xFF45D6B0);
        // Üst kısmı kapalı kavis ve uzun gövde, 24 pikselde bile P harfini belirgin kılar.
        fill(x + size * 5 / 24, y + size * 4 / 24,
             size * 3 / 24, size * 17 / 24, ink);
        fill(x + size * 5 / 24, y + size * 4 / 24,
             size * 13 / 24, size * 3 / 24, ink);
        fill(x + size * 15 / 24, y + size * 4 / 24,
             size * 3 / 24, size * 10 / 24, ink);
        fill(x + size * 5 / 24, y + size * 11 / 24,
             size * 13 / 24, size * 3 / 24, ink);
    }

    void icon(uint32_t x, uint32_t y, uint32_t kind)
    {
        if (kind == 0)
        {
            frame(x + 4, y + 3, 36, 36, 0xFF123050);
            pckos_mark(x + 8, y + 7, 28);
        }
        else if (kind == 1)
        {
            fill(x + 5, y + 11, 34, 23, 0xFF805000);
            fill(x + 7, y + 7, 16, 8, 0xFFFFD040);
            fill(x + 5, y + 12, 34, 19, 0xFFFFC030);
            fill(x + 7, y + 15, 30, 13, 0xFFFFE080);
            fill(x + 7, y + 15, 30, 2, COLOR_WHITE);
        }
        else if (kind == 2)
        {
            frame(x + 5, y + 4, 34, 28, 0xFF101010, true);
            fill(x + 8, y + 7, 28, 2, 0xFF808080);
            fill(x + 10, y + 13, 3, 3, 0xFF00FF40);
            fill(x + 13, y + 16, 3, 3, 0xFF00FF40);
            fill(x + 16, y + 19, 9, 2, 0xFF00FF40);
            fill(x + 13, y + 34, 18, 2, COLOR_LIGHT);
            fill(x + 16, y + 32, 12, 2, COLOR_DARK);
        }
        else
        {
            frame(x + 5, y + 5, 34, 30, COLOR_FACE);
            fill(x + 10, y + 10, 24, 3, COLOR_TITLE);
            fill(x + 10, y + 17, 18, 2, COLOR_DARK);
            fill(x + 10, y + 22, 22, 2, COLOR_DARK);
            fill(x + 10, y + 27, 14, 2, COLOR_DARK);
        }
    }
}
