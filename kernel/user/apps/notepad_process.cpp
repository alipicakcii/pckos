// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/fs/filesystem.hh>
#include <user/gui/graphics.hh>
#include <user/input/events.hh>
#include <user/io/stream.hh>
#include <user/task/process.hh>
#include <user/task/thread.hh>
#include <user/gui/drawing.hh>
#include <user/gui/window.hh>

namespace
{
    class Notepad
    {
    public:
        Notepad() : path_input_(path_, sizeof(path_)), length_(0), cursor_(0),
                    scroll_(0), path_focused_(true), editing_(false),
                    truncated_(false),
                    status_("Dosya yolunu yazip Enter veya Ac tusuna bas")
        {
            path_input_.set_text("/home/");
        }

        void open_file()
        {
            const int32_t descriptor = user::open(path_input_.value());
            if (descriptor < 0)
            {
                status_ = "Dosya acilamadi";
                return;
            }
            length_ = 0;
            truncated_ = false;
            for (;;)
            {
                const uint32_t remaining = sizeof(text_) - 1 - length_;
                const uint32_t request = remaining < 4096 ? remaining : 4096;
                const int32_t count = user::read(descriptor, text_ + length_, request);
                if (count < 0)
                {
                    status_ = "Okuma hatasi";
                    length_ = 0;
                    break;
                }
                if (count == 0) { status_ = "Dosya acildi | duzenlenebilir"; break; }
                length_ += static_cast<uint32_t>(count);
                if (length_ == sizeof(text_) - 1)
                {
                    status_ = "Ilk 8191 bayt gosteriliyor";
                    truncated_ = true;
                    break;
                }
            }
            (void)user::close(descriptor);
            text_[length_] = '\0';
            cursor_ = length_;
            scroll_ = 0;
            path_focused_ = false;
            editing_ = true;
        }

        void save_file()
        {
            if (!editing_)
            {
                status_ = "Once duzenlemek icin dosya acin";
                return;
            }
            if (truncated_)
            {
                status_ = "Dosya buyuk; kesilmis icerik kaydedilmedi";
                return;
            }
            const int32_t result = user::replace_file(path_, text_, length_);
            status_ = result == static_cast<int32_t>(length_)
                ? "Dosya kaydedildi" : "Kaydetme basarisiz (disk/dosya siniri)";
        }

        void new_file()
        {
            const int32_t inode = user::create_file(path_input_.value());
            if (inode < 0)
            {
                status_ = "Dosya olusturulamadi (klasor/ad/inode kontrol edin)";
                return;
            }
            length_ = 0;
            cursor_ = 0;
            scroll_ = 0;
            text_[0] = '\0';
            truncated_ = false;
            editing_ = true;
            path_focused_ = false;
            status_ = "Bos dosya olusturuldu; yazip Kaydet'e basin";
        }

        void key(int32_t key)
        {
            if (path_focused_)
            {
                if (key == '\n') { open_file(); return; }
                (void)path_input_.handle_key(key);
                return;
            }
            if (key == '\t') path_focused_ = true;
            else if (key == 0xE4 && cursor_ > 0) --cursor_;
            else if (key == 0xE5 && cursor_ < length_) ++cursor_;
            else if ((key == '\b' || key == 0x7F) && cursor_ > 0)
            {
                for (uint32_t i = cursor_ - 1; i < length_; ++i)
                    text_[i] = text_[i + 1];
                --cursor_;
                --length_;
            }
            else if (key == '\n' || (key >= 32 && key <= 126))
            {
                if (length_ + 1 >= sizeof(text_))
                {
                    status_ = "Dosya 8191 bayt sinirina ulasti";
                    return;
                }
                for (uint32_t i = length_; i > cursor_; --i)
                    text_[i] = text_[i - 1];
                text_[cursor_++] = key == '\n' ? '\n' : static_cast<char>(key);
                ++length_;
                text_[length_] = '\0';
            }
            else if (key == 0xE2 && scroll_ > 0) --scroll_;
            else if (key == 0xE3) ++scroll_;
            else if (key == 0xE6) scroll_ = scroll_ > 10 ? scroll_ - 10 : 0;
            else if (key == 0xE7) scroll_ += 10;
        }

        void click(int32_t x, int32_t y, uint32_t width)
        {
            if (y >= 5 && y < 33)
            {
                if (x >= 8 && x < static_cast<int32_t>(width - 242))
                {
                    path_focused_ = true;
                    path_input_.place_cursor_from_x(static_cast<uint32_t>(x),
                                                    13, width - 260);
                }
                else if (x >= static_cast<int32_t>(width - 236) &&
                         x < static_cast<int32_t>(width - 171))
                    new_file();
                else if (x >= static_cast<int32_t>(width - 166) &&
                         x < static_cast<int32_t>(width - 94))
                    open_file();
                else if (x >= static_cast<int32_t>(width - 86) &&
                         x < static_cast<int32_t>(width - 8))
                    save_file();
            }
            else if (y >= 62) path_focused_ = false;
        }

        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        {
            path_input_.draw(x + 8, y + 5, width - 250, 28, path_focused_);
            user::gui::button(x + width - 236, y + 5, 65, 28, "Yeni");
            user::gui::button(x + width - 166, y + 5, 70, 28, "Ac");
            user::gui::button(x + width - 86, y + 5, 78, 28, "Kaydet");
            user::gui::text(x + 8, y + 40, status_, user::gui::COLOR_TEXT);
            const uint32_t area_y = y + 62;
            const uint32_t area_height = height > 89 ? height - 89 : 1;
            user::gui::frame(x + 7, area_y, width - 14, area_height,
                             user::gui::COLOR_WHITE, true);
            user::gui::push_clip(x + 11, area_y + 4, width - 22,
                                 area_height > 8 ? area_height - 8 : 1);
            const uint32_t columns = (width - 30) / user::gui::FONT_WIDTH;
            const uint32_t rows = (area_height - 8) / user::gui::FONT_HEIGHT;
            uint32_t line_number = 0, column = 0, line_start = 0;
            char line[96];
            for (uint32_t i = 0; i <= length_; ++i)
            {
                const bool end = i == length_;
                const bool newline = !end && text_[i] == '\n';
                if (!end && text_[i] == '\r') continue;
                if (end || newline || column >= columns)
                {
                    line[column] = '\0';
                    if (line_number >= scroll_ && line_number - scroll_ < rows)
                    {
                        const uint32_t line_y = area_y + 6 +
                            (line_number - scroll_) * user::gui::FONT_HEIGHT;
                        user::gui::text(x + 13, area_y + 6 +
                            (line_number - scroll_) * user::gui::FONT_HEIGHT,
                            line, user::gui::COLOR_TEXT);
                        const bool wrapped = column >= columns && !end && !newline;
                        if (cursor_ >= line_start &&
                            (wrapped ? cursor_ < i : cursor_ <= i))
                        {
                            const uint32_t caret_column = cursor_ - line_start;
                            user::gui::fill(x + 13 + caret_column *
                                user::gui::FONT_WIDTH, line_y,
                                2, user::gui::FONT_HEIGHT, user::gui::COLOR_TEXT);
                        }
                    }
                    if (end) break;
                    ++line_number;
                    column = 0;
                    line_start = i + (newline ? 1U : 0U);
                    if (newline) continue;
                }
                if (column + 1 < sizeof(line)) line[column++] = text_[i];
            }
            user::gui::pop_clip();
            user::gui::text(x + 8, y + height - 20,
                            "Yaz: metin | Sol/Sag: imlec | Tab: yol | Ac/Kaydet | Yukari/Asagi: kaydir",
                            user::gui::COLOR_TEXT);
        }

    private:
        char path_[128];
        user::gui::TextInput path_input_;
        char text_[8192];
        uint32_t length_, cursor_, scroll_;
        bool path_focused_, editing_, truncated_;
        const char *status_;
    };
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 || screen.width < 400 || screen.height < 300)
        user::exit(-1);
    user::thread_set_name("Not Defteri");
    const uint32_t usable_height = screen.height - 48;
    const uint32_t width = screen.width > 630 ? 610 : screen.width - 20;
    const uint32_t height = usable_height > 500 ? 480 : usable_height - 10;
    uint32_t x = (screen.width - width) / 2 + 40;
    uint32_t y = (usable_height - height) / 2 + 15;
    if (x + width > screen.width) x = screen.width - width;
    if (y + height > usable_height) y = usable_height - height;
    user::gui::Window window(x, y, width, height, "PCKOS - Not Defteri");
    if (!window.valid())
        user::exit(-1);
    Notepad notepad;
    int32_t mouse_x = static_cast<int32_t>(screen.width / 2);
    int32_t mouse_y = static_cast<int32_t>(screen.height / 2);
    (void)user::graphics_get_cursor(&mouse_x, &mouse_y);
    uint32_t last_buttons = 0;
    bool dirty = true;
    for (;;)
    {
        if (user::process_is_minimized())
        {
            last_buttons = 0;
            user::InputEvent ignored{};
            (void)user::wait_event(&ignored, 0);
            dirty = true;
            continue;
        }
        x = window.x();
        y = window.y();
        const uint32_t content_x = x + 7, content_y = y + 32;
        if (dirty)
        {
            user::gui::begin_frame();
            window.draw();
            user::gui::push_clip(content_x, content_y, width - 14, height - 40);
            notepad.draw(content_x, content_y, width - 14, height - 40);
            user::gui::pop_clip();
            user::gui::end_frame();
            dirty = false;
        }

        user::InputEvent event{};
        if (user::wait_event(&event, 0) <= 0 || event.type == 3) continue;
        dirty = true;
        if (event.type == 1)
        {
            if (event.value0 == 0x1B) user::exit(0);
            notepad.key(event.value0);
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
            if (pressed && mouse_x >= static_cast<int32_t>(content_x) &&
                mouse_x < static_cast<int32_t>(content_x + width - 14) &&
                mouse_y >= static_cast<int32_t>(content_y) &&
                mouse_y < static_cast<int32_t>(content_y + height - 40))
                notepad.click(mouse_x - static_cast<int32_t>(content_x),
                              mouse_y - static_cast<int32_t>(content_y), width - 14);
            last_buttons = event.buttons;
        }
    }
}
