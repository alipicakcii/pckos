// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/fs/filesystem.hh>
#include <user/memory/heap.hh>
#include <user/io/stream.hh>
#include <user/task/process.hh>
#include <user/system/info.hh>
#include <user/task/thread.hh>
#include <user/apps/terminal.hh>
#include <user/gui/drawing.hh>

namespace user::apps
{
    namespace
    {
        uint32_t length(const char *text)
        {
            uint32_t result = 0;
            while (text[result] != '\0')
                ++result;
            return result;
        }

        bool equal(const char *left, const char *right)
        {
            while (*left && *right)
            {
                char a = *left++;
                char b = *right++;
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b)
                    return false;
            }
            return *left == '\0' && *right == '\0';
        }

    }

    Terminal::Terminal() : command_input_(command_, sizeof(command_))
    {
        dirty_ = 0;
        log_cursor_ = 0;
        history_count_ = 0;
        history_index_ = 0;
        scroll_offset_ = 0;
        scrollbar_x_ = 0;
        scrollbar_y_ = 0;
        scrollbar_height_ = 0;
        scrollbar_thumb_y_ = 0;
        scrollbar_thumb_height_ = 0;
        maximum_scroll_ = 0;
        scrollbar_grab_offset_ = 0;
        scrollbar_dragging_ = false;
        working_directory_[0] = '/';
        working_directory_[1] = '\0';
        previous_directory_[0] = '\0';
        reset();
    }

    void Terminal::reset()
    {
        LockGuard guard(lock_);
        for (uint32_t row = 0; row < MAX_LINES; ++row)
            for (uint32_t column = 0; column < MAX_COLUMNS; ++column)
                lines_[row][column] = '\0';
        line_count_ = 0;
        foreground_process_ = 0;
        output_open_ = false;
        command_length_ = 0;
        history_count_ = 0;
        history_index_ = 0;
        scroll_offset_ = 0;
        scrollbar_dragging_ = false;
        history_draft_[0] = '\0';
        command_input_.clear();
        print_line("PCKOS komut kabugu");
        print_line("Komutlar icin help yazin.");
        dirty_ = 1;
    }

    void Terminal::scroll_lines()
    {
        for (uint32_t row = 1; row < MAX_LINES; ++row)
            for (uint32_t column = 0; column < MAX_COLUMNS; ++column)
                lines_[row - 1][column] = lines_[row][column];
        for (uint32_t column = 0; column < MAX_COLUMNS; ++column)
            lines_[MAX_LINES - 1][column] = '\0';
        line_count_ = MAX_LINES - 1;
    }

    void Terminal::remember_command()
    {
        if (command_length_ == 0)
            return;
        if (history_count_ != 0 &&
            equal(command_history_[history_count_ - 1], command_))
        {
            history_index_ = history_count_;
            return;
        }
        if (history_count_ == COMMAND_HISTORY_SIZE)
        {
            for (uint32_t i = 1; i < COMMAND_HISTORY_SIZE; ++i)
            {
                uint32_t j = 0;
                while (j < MAX_COMMAND && command_history_[i][j] != '\0')
                {
                    command_history_[i - 1][j] = command_history_[i][j];
                    ++j;
                }
                command_history_[i - 1][j] = '\0';
            }
            --history_count_;
        }
        uint32_t i = 0;
        while (i < command_length_ && i < MAX_COMMAND)
        {
            command_history_[history_count_][i] = command_[i];
            ++i;
        }
        command_history_[history_count_][i] = '\0';
        ++history_count_;
        history_index_ = history_count_;
    }

    void Terminal::recall_previous_command()
    {
        if (history_count_ == 0 || history_index_ == 0)
            return;
        if (history_index_ == history_count_)
        {
            uint32_t i = 0;
            while (i < command_length_ && i < MAX_COMMAND)
            {
                history_draft_[i] = command_[i];
                ++i;
            }
            history_draft_[i] = '\0';
        }
        --history_index_;
        command_input_.set_text(command_history_[history_index_]);
        command_length_ = command_input_.length();
    }

    void Terminal::recall_next_command()
    {
        if (history_index_ >= history_count_)
            return;
        ++history_index_;
        command_input_.set_text(history_index_ == history_count_
            ? history_draft_ : command_history_[history_index_]);
        command_length_ = command_input_.length();
    }

    void Terminal::print_line(const char *value)
    {
        output_open_ = false;
        // Bir komut geçmiş satırından uzun olsa bile çıktının tamamını koru.
        do
        {
            if (line_count_ >= MAX_LINES)
                scroll_lines();
            uint32_t column = 0;
            while (*value != '\0' && column + 1 < MAX_COLUMNS)
            {
                const unsigned char c = static_cast<unsigned char>(*value++);
                lines_[line_count_][column++] = c >= 32 && c <= 126
                    ? static_cast<char>(c) : ' ';
            }
            lines_[line_count_++][column] = '\0';
        } while (*value != '\0');
    }

    void Terminal::print_bytes(const char *value, uint32_t length)
    {
        char line[MAX_COLUMNS];
        uint32_t column = 0;
        for (uint32_t i = 0; i < length; ++i)
        {
            char c = value[i];
            if (c == '\r')
                continue;
            if (c == '\n' || column + 1 >= MAX_COLUMNS)
            {
                line[column] = '\0';
                print_line(line);
                column = 0;
                if (c == '\n')
                    continue;
            }
            if (c < 32)
                c = ' ';
            line[column++] = c;
        }
        if (column != 0)
        {
            line[column] = '\0';
            print_line(line);
        }
    }

    void Terminal::append_output(const char *text, uint32_t size)
    {
        for (uint32_t i = 0; i < size; ++i)
        {
            char c = text[i];
            if (c == '\r')
            {
                if (output_open_) lines_[line_count_ - 1][0] = '\0';
                continue;
            }
            if (!output_open_)
            {
                print_line("");
                output_open_ = true;
            }
            if (c == '\n') { output_open_ = false; continue; }
            uint32_t column = length(lines_[line_count_ - 1]);
            if (c == '\b')
            {
                if (column != 0) lines_[line_count_ - 1][column - 1] = '\0';
                continue;
            }
            if (column + 1 >= MAX_COLUMNS)
            {
                print_line("");
                output_open_ = true;
                column = 0;
            }
            lines_[line_count_ - 1][column++] = c >= 32 && c <= 126 ? c : ' ';
            lines_[line_count_ - 1][column] = '\0';
        }
        if (size != 0) dirty_ = 1;
    }

    void Terminal::poll_kernel_log()
    {
        LockGuard guard(lock_);
        char buffer[512];
        for (uint32_t i = 0; i < 8; ++i)
        {
            const int32_t count = user::read_kernel_log(buffer, sizeof(buffer), &log_cursor_);
            if (count <= 0) break;
            append_output(buffer, static_cast<uint32_t>(count));
        }
    }

    void Terminal::poll_output()
    {
        LockGuard guard(lock_);
        char buffer[512];
        for (uint32_t i = 0; i < 16; ++i)
        {
            const int32_t count = user::terminal_read(buffer, sizeof(buffer));
            if (count <= 0) break;
            append_output(buffer, static_cast<uint32_t>(count));
        }
        if (foreground_process_ != 0)
        {
            bool running = false;
            for (uint32_t i = 0; i < user::process_count(); ++i)
            {
                user::ProcessInfo info{};
                if (user::process_get_info(i, &info) == 0 && info.id == foreground_process_)
                    running = true;
            }
            if (!running)
            {
                foreground_process_ = 0;
                command_length_ = 0;
                command_input_.clear();
                dirty_ = 1;
            }
        }
    }

    void Terminal::draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        LockGuard guard(lock_);
        if (width < 100 || height < 70)
            return;
        user::gui::push_clip(x, y, width, height);
        user::gui::frame(x, y, width, height, user::gui::COLOR_BLACK, true);
        const uint32_t text_x = x + 8;
        const uint32_t text_y = y + 6;
        const uint32_t columns = (width - 28) / user::gui::FONT_WIDTH;
        const uint32_t row_height = user::gui::FONT_HEIGHT + 2;
        const uint32_t rows = (height - 12) / row_height;
        const uint32_t visible_output_rows = rows - 1;
        scrollbar_x_ = width - 13;
        scrollbar_y_ = 6;
        scrollbar_height_ = rows * row_height;
        user::gui::push_clip(text_x, text_y, columns * user::gui::FONT_WIDTH,
                             rows * row_height);

        // Geçmiş, mantıksal satırları saklar. En yeni tam ekran dolusu satırı koruyarak
        // bunları geçerli pencereye veya tam ekran konsola göre yeniden kaydır.
        uint32_t total_rows = 0;
        for (uint32_t line = 0; line < line_count_; ++line)
        {
            const uint32_t size = length(lines_[line]);
            total_rows += size == 0 ? 1 : (size + columns - 1) / columns;
        }
        const uint32_t maximum_scroll = total_rows > visible_output_rows
            ? total_rows - visible_output_rows : 0;
        maximum_scroll_ = maximum_scroll;
        if (scroll_offset_ > maximum_scroll)
            scroll_offset_ = maximum_scroll;
        uint32_t skip_rows = maximum_scroll - scroll_offset_;
        uint32_t drawn_rows = 0;
        for (uint32_t line = 0; line < line_count_; ++line)
        {
            const uint32_t size = length(lines_[line]);
            const uint32_t wrapped_rows = size == 0 ? 1
                : (size + columns - 1) / columns;
            for (uint32_t row = 0; row < wrapped_rows; ++row)
            {
                if (skip_rows != 0)
                {
                    --skip_rows;
                    continue;
                }
                char segment[MAX_COLUMNS];
                const uint32_t offset = row * columns;
                uint32_t count = 0;
                while (count < columns && offset + count < size)
                {
                    segment[count] = lines_[line][offset + count];
                    ++count;
                }
                segment[count] = '\0';
                user::gui::text(text_x, text_y + drawn_rows * row_height,
                                 segment, 0xFFC0FFC0);
                ++drawn_rows;
            }
        }

        const uint32_t thumb_height = total_rows <= visible_output_rows || total_rows == 0
            ? scrollbar_height_
            : (scrollbar_height_ * visible_output_rows) / total_rows;
        scrollbar_thumb_height_ = thumb_height < 12 ? 12 : thumb_height;
        if (scrollbar_thumb_height_ > scrollbar_height_)
            scrollbar_thumb_height_ = scrollbar_height_;
        const uint32_t thumb_travel = scrollbar_height_ - scrollbar_thumb_height_;
        const uint32_t thumb_offset = maximum_scroll == 0 || thumb_travel == 0 ? 0
            : (scroll_offset_ * thumb_travel) / maximum_scroll;
        scrollbar_thumb_y_ = scrollbar_y_ + thumb_travel - thumb_offset;
        const uint32_t prompt_y = text_y + drawn_rows * row_height;
        char prompt[MAX_PATH + 10];
        make_prompt(prompt);
        const uint32_t prompt_size = length(prompt);
        const uint32_t prompt_limit = columns / 2;
        const char *visible_prompt = prompt_size > prompt_limit
            ? prompt + prompt_size - prompt_limit : prompt;
        const uint32_t prompt_columns = foreground_process_ == 0 ? length(visible_prompt) : 0;
        if (foreground_process_ == 0 && scroll_offset_ == 0)
            user::gui::text(text_x, prompt_y, visible_prompt, 0xFFC0FFC0);
        const uint32_t input_columns = columns - prompt_columns;
        uint32_t input_x = text_x + prompt_columns * user::gui::FONT_WIDTH;
        if (scroll_offset_ == 0)
            command_input_.draw_inline(input_x, prompt_y,
                input_columns * user::gui::FONT_WIDTH, true, 0xFFC0FFC0,
                0xFFC0FFC0, true);
        user::gui::pop_clip();
        user::gui::fill(x + scrollbar_x_, y + scrollbar_y_, 8,
                        scrollbar_height_, 0xFF203526);
        user::gui::frame(x + scrollbar_x_, y + scrollbar_thumb_y_, 8,
                         scrollbar_thumb_height_, user::gui::COLOR_FACE, false);
        user::gui::pop_clip();
    }

    void Terminal::key(int32_t key_value)
    {
        LockGuard guard(lock_);
        dirty_ = 1;
        if (key_value == 3)
        {
            if (foreground_process_ != 0)
            {
                const int32_t result = user::terminal_interrupt(foreground_process_);
                if (result < 0 && result != -2)
                {
                    print_line("Uygulama durdurulamadi.");
                    return;
                }
                foreground_process_ = 0;
                char pending[512];
                for (uint32_t i = 0; i < 16; ++i)
                {
                    const int32_t bytes = user::terminal_read(pending, sizeof(pending));
                    if (bytes <= 0) break;
                    append_output(pending, static_cast<uint32_t>(bytes));
                }
            }
            print_line("^C");
            command_input_.clear();
            command_length_ = 0;
            history_index_ = history_count_;
            history_draft_[0] = '\0';
            scroll_offset_ = 0;
        }
        else if (key_value == 12)
            clear_output();
        else if (foreground_process_ == 0 && key_value == 0xE2)
            recall_previous_command();
        else if (foreground_process_ == 0 && key_value == 0xE3)
            recall_next_command();
        else if (key_value == 0xE6)
        {
            scroll_offset_ += 8;
        }
        else if (key_value == 0xE7)
        {
            scroll_offset_ = scroll_offset_ > 8 ? scroll_offset_ - 8 : 0;
        }
        else if (key_value == 4 && foreground_process_ != 0)
        {
            const int32_t result = user::terminal_send(foreground_process_, command_,
                                                       command_length_);
            if (result >= 0 && command_length_ != 0)
            {
                print_line(command_);
                command_length_ = 0;
                command_input_.clear();
            }
        }
        else if (key_value == '\n' && foreground_process_ != 0)
        {
            command_[command_length_] = '\n';
            const int32_t result = user::terminal_send(foreground_process_, command_,
                                                       command_length_ + 1);
            command_[command_length_] = '\0';
            if (result >= 0)
            {
                print_line(command_);
                command_length_ = 0;
                command_input_.clear();
            }
        }
        else if (key_value == '\n')
            execute();
        else if (command_input_.handle_key(key_value))
        {
            command_length_ = command_input_.length();
        }
    }

    void Terminal::mouse(int32_t x, int32_t y, uint32_t buttons, bool pressed)
    {
        LockGuard guard(lock_);
        const bool held = (buttons & 1U) != 0;
        const int32_t track_top = static_cast<int32_t>(scrollbar_y_);
        const int32_t track_bottom = track_top + static_cast<int32_t>(scrollbar_height_);
        const int32_t thumb_top = static_cast<int32_t>(scrollbar_thumb_y_);
        const int32_t thumb_bottom = thumb_top +
            static_cast<int32_t>(scrollbar_thumb_height_);
        const int32_t thumb_travel = static_cast<int32_t>(scrollbar_height_ -
            scrollbar_thumb_height_);
        bool changed = false;

        if (pressed && x >= static_cast<int32_t>(scrollbar_x_ - 2) &&
            x < static_cast<int32_t>(scrollbar_x_ + 11) &&
            y >= track_top && y < track_bottom && maximum_scroll_ != 0)
        {
            if (y >= thumb_top && y < thumb_bottom)
            {
                scrollbar_dragging_ = true;
                scrollbar_grab_offset_ = y - thumb_top;
            }
            else if (thumb_travel > 0)
            {
                int32_t next_top = y - track_top -
                    static_cast<int32_t>(scrollbar_thumb_height_ / 2);
                if (next_top < 0) next_top = 0;
                if (next_top > thumb_travel) next_top = thumb_travel;
                scroll_offset_ = maximum_scroll_ - static_cast<uint32_t>(
                    (static_cast<uint32_t>(next_top) * maximum_scroll_) / thumb_travel);
                changed = true;
            }
        }

        if (held && scrollbar_dragging_ && thumb_travel > 0)
        {
            int32_t next_top = y - track_top - scrollbar_grab_offset_;
            if (next_top < 0) next_top = 0;
            if (next_top > thumb_travel) next_top = thumb_travel;
            const uint32_t next_offset = maximum_scroll_ - static_cast<uint32_t>(
                (static_cast<uint32_t>(next_top) * maximum_scroll_) / thumb_travel);
            if (next_offset != scroll_offset_)
            {
                scroll_offset_ = next_offset;
                changed = true;
            }
        }
        if (!held)
            scrollbar_dragging_ = false;
        if (changed)
            dirty_ = 1;
    }

    void Terminal::post_key(int32_t key_value)
    {
        events_.push({1, key_value, 0, 0, 0});
    }

    bool Terminal::consume_dirty()
    {
        LockGuard guard(lock_);
        const bool dirty = dirty_ != 0;
        dirty_ = 0;
        return dirty;
    }

    void Terminal::thread_entry(void *argument)
    {
        auto *terminal = static_cast<Terminal *>(argument);
        if (terminal == nullptr)
            user::thread_exit();
        for (;;)
        {
            terminal->poll_output();
            AppEvent event{};
            if (!terminal->events_.pop(&event))
            {
                user::thread_sleep(1);
                continue;
            }
            if (event.type == 1)
                terminal->key(event.value0);
        }
    }
}
