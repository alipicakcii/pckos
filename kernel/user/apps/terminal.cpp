// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/apps/terminal.hh>
#include <user/gui.hh>
#include <user/syscall.hh>

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

        bool starts_with(const char *value, const char *prefix)
        {
            while (*prefix)
            {
                char a = *value++;
                char b = *prefix++;
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b)
                    return false;
            }
            return true;
        }

        void to_number(uint32_t value, char *output)
        {
            char reverse[10];
            uint32_t count = 0;
            do
            {
                reverse[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && count < sizeof(reverse));
            uint32_t at = 0;
            while (count != 0)
                output[at++] = reverse[--count];
            output[at] = '\0';
        }
    }

    Terminal::Terminal()
    {
        dirty_ = 0;
        reset();
    }

    void Terminal::reset()
    {
        LockGuard guard(lock_);
        for (uint32_t row = 0; row < MAX_LINES; ++row)
            for (uint32_t column = 0; column < MAX_COLUMNS; ++column)
                lines_[row][column] = '\0';
        line_count_ = 0;
        command_length_ = 0;
        command_[0] = '\0';
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

    void Terminal::print_line(const char *value)
    {
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

    void Terminal::execute()
    {
        char echoed[MAX_COMMAND + 10];
        const char *prefix = "pckos> ";
        uint32_t at = 0;
        while (*prefix)
            echoed[at++] = *prefix++;
        for (uint32_t i = 0; i < command_length_ && at + 1 < sizeof(echoed); ++i)
            echoed[at++] = command_[i];
        echoed[at] = '\0';
        print_line(echoed);

        if (equal(command_, "help"))
        {
            print_line("Komutlar: help, clear, about, ticks, mem, threads, dmesg, ls, pwd, cat, echo, run");
            print_line("cat, PCKFS icindeki metin dosyalarini okur.");
            print_line("run /apps/new_demo.bin: uygulamayi ayri surecte baslatir.");
        }
        else if (equal(command_, "clear"))
        {
            for (uint32_t row = 0; row < MAX_LINES; ++row)
                for (uint32_t column = 0; column < MAX_COLUMNS; ++column)
                    lines_[row][column] = '\0';
            line_count_ = 0;
        }
        else if (equal(command_, "about"))
            print_line("PCKOS i386 | Kullanici alani terminali | PCKFS");
        else if (equal(command_, "ticks"))
        {
            char value[11];
            char result[48] = "PIT tik sayisi: ";
            to_number(static_cast<uint32_t>(user::get_ticks()), value);
            uint32_t i = length(result);
            for (uint32_t j = 0; value[j] != '\0' && i + 1 < sizeof(result); ++j)
                result[i++] = value[j];
            result[i] = '\0';
            print_line(result);
        }
        else if (equal(command_, "mem"))
        {
            char value[11];
            char result[48] = "Kernel heap bayt: ";
            to_number(static_cast<uint32_t>(user::get_heap_usage()), value);
            uint32_t i = length(result);
            for (uint32_t j = 0; value[j] != '\0' && i + 1 < sizeof(result); ++j)
                result[i++] = value[j];
            result[i] = '\0';
            print_line(result);
        }
        else if (equal(command_, "threads"))
        {
            user::thread_yield();
            user::thread_sleep(2);
            char value[11];
            char result[112] = "Thread id=";
            uint32_t at = length(result);
            to_number(user::thread_id(), value);
            for (uint32_t i = 0; value[i] != '\0'; ++i) result[at++] = value[i];
            const char *active = " aktif=";
            while (*active) result[at++] = *active++;
            to_number(user::thread_count(), value);
            for (uint32_t i = 0; value[i] != '\0'; ++i) result[at++] = value[i];
            const char *switches = " gecis=";
            while (*switches) result[at++] = *switches++;
            to_number(user::thread_switch_count(), value);
            for (uint32_t i = 0; value[i] != '\0' && at + 1 < sizeof(result); ++i)
                result[at++] = value[i];
            result[at] = '\0';
            print_line(result);
        }
        else if (equal(command_, "dmesg"))
        {
            char log[4096];
            const int32_t length = user::get_kernel_log(log, sizeof(log));
            if (length > 0)
                print_bytes(log, static_cast<uint32_t>(length));
            else
                print_line("Kernel gunlugu bos.");
        }
        else if (equal(command_, "pwd"))
            print_line("/");
        else if (equal(command_, "ls"))
        {
            const int32_t root = user::root_inode();
            user::DirectoryEntry entry{};
            uint32_t index = 0;
            bool any = false;
            while (root >= 0 && index < 256 &&
                   user::read_directory(static_cast<uint32_t>(root), index,
                                        &entry) == 1)
            {
                char name[56];
                uint32_t i = 0;
                while (i + 2 < sizeof(name) && entry.name[i] != '\0')
                {
                    name[i] = entry.name[i];
                    ++i;
                }
                if (entry.type == user::directory_type)
                    name[i++] = '/';
                name[i] = '\0';
                print_line(name);
                ++index;
                any = true;
            }
            if (!any)
                print_line("PCKFS bagli degil veya dizin bos.");
        }
        else if (starts_with(command_, "run "))
        {
            const char *path = command_ + 4;
            while (*path == ' ') ++path;
            if (*path != '/')
                print_line("Kullanim: run /apps/uygulama.bin");
            else
            {
                const int32_t pid = user::process_spawn(path, "Uygulama");
                char number[12];
                if (pid < 0)
                {
                    print_line("Uygulama baslatilamadi. Hata kodu (mutlak):");
                    to_number(static_cast<uint32_t>(-pid), number);
                }
                else
                {
                    print_line("Uygulama baslatildi. PID:");
                    to_number(static_cast<uint32_t>(pid), number);
                }
                print_line(number);
            }
        }
        else if (starts_with(command_, "echo "))
            print_line(command_ + 5);
        else if (starts_with(command_, "cat "))
        {
            const char *path = command_ + 4;
            char absolute_path[128];
            uint32_t path_length = 0;
            if (*path != '/')
                absolute_path[path_length++] = '/';
            while (path_length + 1 < sizeof(absolute_path) && *path != '\0')
                absolute_path[path_length++] = *path++;
            absolute_path[path_length] = '\0';

            const int32_t descriptor = user::open(absolute_path);
            if (descriptor < 0)
                print_line("Dosya bulunamadi.");
            else
            {
                char data[512];
                const int32_t bytes = user::read(descriptor, data, sizeof(data));
                user::close(descriptor);
                if (bytes > 0)
                    print_bytes(data, static_cast<uint32_t>(bytes));
                else
                    print_line("Dosya bos veya okunamadi.");
            }
        }
        else if (command_length_ != 0)
            print_line("Bilinmeyen komut. help yazin.");

        command_length_ = 0;
        command_[0] = '\0';
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
        const uint32_t columns = (width - 16) / user::gui::FONT_WIDTH;
        const uint32_t row_height = user::gui::FONT_HEIGHT + 2;
        const uint32_t rows = (height - 12) / row_height;
        const uint32_t visible_output_rows = rows - 1;
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
        uint32_t skip_rows = total_rows > visible_output_rows
            ? total_rows - visible_output_rows : 0;
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

        const uint32_t prompt_y = text_y + drawn_rows * row_height;
        user::gui::text(text_x, prompt_y, "pckos> ", 0xFFC0FFC0);
        // Girdi sınırında bile dolu imleç için bir karakter hücresini boş bırak.
        const uint32_t input_columns = columns - 7 - 1;
        const uint32_t start = command_length_ > input_columns
            ? command_length_ - input_columns : 0;
        uint32_t input_x = text_x + 7 * user::gui::FONT_WIDTH;
        user::gui::text(input_x, prompt_y, command_ + start, 0xFFC0FFC0);
        input_x += (command_length_ - start) * user::gui::FONT_WIDTH;
        user::gui::fill(input_x, prompt_y, user::gui::FONT_WIDTH,
                         user::gui::FONT_HEIGHT, 0xFFC0FFC0);
        user::gui::pop_clip();
        user::gui::pop_clip();
    }

    void Terminal::key(int32_t key_value)
    {
        LockGuard guard(lock_);
        dirty_ = 1;
        if (key_value == '\n')
            execute();
        else if (key_value == '\b')
        {
            if (command_length_ != 0)
                command_[--command_length_] = '\0';
        }
        else if (key_value >= 32 && key_value <= 126 &&
                 command_length_ < MAX_COMMAND)
        {
            command_[command_length_++] = static_cast<char>(key_value);
            command_[command_length_] = '\0';
        }
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
