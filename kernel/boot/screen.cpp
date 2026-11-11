// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/boot/screen.hh>
#include <device/video/frame_buffer.hh>
#include <library/kernel_log.hh>

namespace kernel::boot
{
    namespace
    {
        constexpr uint32_t LOADER_BACKGROUND = 0xFF06110B;
        constexpr uint32_t LOADER_PANEL = 0xFF0D1D13;
        constexpr uint32_t LOADER_BORDER = 0xFF31583A;
        constexpr uint32_t LOADER_TRACK = 0xFF192D1F;
        constexpr uint32_t LOADER_ACCENT = 0xFF9DE85B;
        constexpr uint32_t LOADER_TEXT = 0xFFE8F2E3;
        constexpr uint32_t LOADER_MUTED = 0xFF9CAE9A;
        bool loader_drawn = false;
        uint32_t loader_x = 0, loader_y = 0, loader_width = 0;

        void fill_rectangle(device::FrameBuffer &framebuffer, uint32_t x,
                            uint32_t y, uint32_t width, uint32_t height,
                            uint32_t color)
        {
            const uint32_t right = x + width < framebuffer.get_width()
                ? x + width : framebuffer.get_width();
            const uint32_t bottom = y + height < framebuffer.get_height()
                ? y + height : framebuffer.get_height();
            for (uint32_t row = y; row < bottom; ++row)
                for (uint32_t column = x; column < right; ++column)
                    framebuffer.draw_pixel(column, row, color);
        }

        void draw_centered(device::FrameBuffer &framebuffer, uint32_t y,
                           const char *text, uint32_t color)
        {
            uint32_t length = 0;
            while (text[length] != '\0') ++length;
            const uint32_t text_width = length * 8;
            const uint32_t x = text_width < framebuffer.get_width()
                ? (framebuffer.get_width() - text_width) / 2 : 0;
            framebuffer.draw_string(x, y, text, color);
        }

        void draw_number(device::FrameBuffer &framebuffer, uint32_t x,
                         uint32_t y, uint32_t number, uint32_t color)
        {
            char digits[11];
            uint32_t count = 0;
            do
            {
                digits[count++] = static_cast<char>('0' + number % 10);
                number /= 10;
            } while (number != 0 && count < sizeof(digits));
            while (count != 0)
                framebuffer.draw_char(x, y, digits[--count], color), x += 8;
        }
    }

    void show_loading_screen(device::FrameBuffer &framebuffer,
                             const char *message, uint32_t progress,
                             uint32_t ready, uint32_t total)
    {
        const uint32_t screen_width = framebuffer.get_width();
        const uint32_t screen_height = framebuffer.get_height();
        if (screen_width < 240 || screen_height < 160)
            return;
        framebuffer.select_process(1);
        const bool frame_started = framebuffer.begin_frame();
        framebuffer.reset_clip();

        if (!loader_drawn)
        {
            loader_width = screen_width > 700 ? 620 : screen_width - 32;
            const uint32_t panel_height = 224;
            loader_x = (screen_width - loader_width) / 2;
            loader_y = (screen_height - panel_height) / 2;
            framebuffer.clear(LOADER_BACKGROUND);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           loader_width, panel_height, LOADER_PANEL);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           loader_width, 2, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x, loader_y + panel_height - 2,
                           loader_width, 2, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           2, panel_height, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + loader_width - 2, loader_y,
                           2, panel_height, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + 28, loader_y + 28,
                           48, 48, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + 32, loader_y + 32,
                           40, 40, LOADER_BACKGROUND);
            framebuffer.draw_char_scaled(loader_x + 42, loader_y + 36,
                                          'P', LOADER_TEXT, 2);
            fill_rectangle(framebuffer, loader_x + 64, loader_y + 62,
                           7, 7, LOADER_ACCENT);
            framebuffer.draw_string(loader_x + 92, loader_y + 32,
                                    "PCKOS", LOADER_TEXT);
            framebuffer.draw_string(loader_x + 92, loader_y + 52,
                                    "SISTEM BASLATILIYOR", LOADER_MUTED);
            fill_rectangle(framebuffer, loader_x + 28, loader_y + 94,
                           loader_width - 56, 1, LOADER_BORDER);
            draw_centered(framebuffer, loader_y + 112,
                          "PCKOS loading...", LOADER_ACCENT);
            fill_rectangle(framebuffer, loader_x + 32, loader_y + 158,
                           loader_width - 64, 14, LOADER_TRACK);
            loader_drawn = true;
        }

        const uint32_t bar_x = loader_x + 32;
        const uint32_t bar_y = loader_y + 158;
        const uint32_t bar_width = loader_width - 64;
        fill_rectangle(framebuffer, loader_x + 28, loader_y + 132,
                       loader_width - 56, 18, LOADER_PANEL);
        framebuffer.draw_string(loader_x + 32, loader_y + 132,
                                message, LOADER_TEXT);
        fill_rectangle(framebuffer, bar_x, bar_y, bar_width, 14, LOADER_TRACK);
        if (progress > 100) progress = 100;
        fill_rectangle(framebuffer, bar_x, bar_y,
                       (bar_width * progress) / 100, 14, LOADER_ACCENT);
        fill_rectangle(framebuffer, loader_x + 28, loader_y + 184,
                       loader_width - 56, 18, LOADER_PANEL);
        if (total != 0)
        {
            framebuffer.draw_string(loader_x + 32, loader_y + 184,
                                    "Islemciler:", LOADER_MUTED);
            draw_number(framebuffer, loader_x + 112, loader_y + 184,
                        ready, LOADER_TEXT);
            framebuffer.draw_char(loader_x + 128, loader_y + 184, '/', LOADER_MUTED);
            draw_number(framebuffer, loader_x + 144, loader_y + 184,
                        total, LOADER_MUTED);
        }
        else
            framebuffer.draw_string(loader_x + 32, loader_y + 184,
                                    "Lutfen bekleyin", LOADER_MUTED);
        if (frame_started)
            framebuffer.end_frame();
    }

    void show_smp_progress(uint32_t ready, uint32_t total, void *context)
    {
        auto *framebuffer = static_cast<device::FrameBuffer *>(context);
        const uint32_t safe_total = total == 0 ? 1 : total;
        const uint32_t progress = 24 + (ready * 66) / safe_total;
        show_loading_screen(*framebuffer, "SMP islemcileri baslatiliyor...",
                            progress, ready, total);
    }

    void show_console(device::FrameBuffer &framebuffer)
    {
        char log[lib::KERNEL_LOG_CAPACITY];
        const uint32_t size = lib::kernel_log_snapshot(log, sizeof(log));
        framebuffer.select_process(1);
        framebuffer.begin_frame();
        framebuffer.clear(0xFF101820);
        uint32_t x = 8, y = 8;
        for (uint32_t i = 0; i < size; ++i)
        {
            if (log[i] == '\r') continue;
            if (log[i] == '\n' || x + 8 > framebuffer.get_width())
            {
                x = 8;
                y += 18;
                if (y + 16 > framebuffer.get_height())
                {
                    framebuffer.clear(0xFF101820);
                    y = 8;
                }
                if (log[i] == '\n') continue;
            }
            framebuffer.draw_char(x, y, log[i], 0xFFC0FFC0);
            x += 8;
        }
        framebuffer.end_frame();
    }
}
