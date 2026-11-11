// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/apps/terminal.hh>
#include <user/io/stream.hh>
#include <user/memory/heap.hh>
#include <user/task/process.hh>
#include <user/system/info.hh>
#include <user/task/thread.hh>
namespace user::apps
{
    namespace
    {
        void copy(char *out, const char *in) { while ((*out++ = *in++)) {} }
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

        bool make_absolute_path(const char *argument, const char *working_directory,
                                char *path, uint32_t capacity)
        {
            if (argument == nullptr || *argument == '\0' || capacity == 0)
                return false;
            uint32_t argument_length = length(argument);
            while (argument_length != 0 && argument[argument_length - 1] == ' ')
                --argument_length;
            if (argument_length == 0)
                return false;

            uint32_t output = 0;
            if (*argument != '/')
            {
                const uint32_t directory_length = length(working_directory);
                if (directory_length + 1 >= capacity)
                    return false;
                for (uint32_t i = 0; i < directory_length; ++i)
                    path[output++] = working_directory[i];
                if (!(directory_length == 1 && working_directory[0] == '/'))
                    path[output++] = '/';
            }
            if (output + argument_length >= capacity)
                return false;
            for (uint32_t i = 0; i < argument_length; ++i)
                path[output++] = argument[i];
            path[output] = '\0';
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


    void Terminal::clear_output()
    {
        for (auto &row : lines_) row[0] = '\0';
        line_count_ = 0;
        output_open_ = false;
        scroll_offset_ = 0;
    }

    void Terminal::make_prompt(char *output) const
    {
        copy(output, "pckos:");
        copy(output + 6, working_directory_);
        copy(output + length(output), "> ");
    }

    void Terminal::print_entry(const user::DirectoryEntry &entry)
    {
        char row[80];
        copy(row, entry.name);
        if (entry.type == user::directory_type) copy(row + length(row), "/");
        print_line(row);
    }

    bool Terminal::resolve_path(const char *path, char *absolute, user::DirectoryEntry *entry)
    {
        char combined[MAX_PATH * 2];
        uint32_t used = 0;
        if (*path != '/')
        {
            copy(combined, working_directory_);
            used = length(combined);
            combined[used++] = '/';
        }
        while (*path && used + 1 < sizeof(combined)) combined[used++] = *path++;
        if (*path) return false;
        combined[used] = 0;
        const int32_t root = user::root_inode();
        if (root < 0) return false;
        uint32_t parents[MAX_PATH / 2]{};
        uint32_t depth = 0;
        parents[0] = static_cast<uint32_t>(root);
        *entry = {};
        entry->inode = parents[0];
        entry->type = user::directory_type;
        copy(absolute, "/");
        const char *cursor = combined;
        while (*cursor)
        {
            while (*cursor == '/') ++cursor;
            if (!*cursor) break;
            if (entry->type != user::directory_type) return false;
            char part[48];
            uint32_t size = 0;
            while (*cursor && *cursor != '/')
            {
                if (size + 1 >= sizeof(part)) return false;
                part[size++] = *cursor++;
            }
            part[size] = 0;
            if (equal(part, ".")) continue;
            if (equal(part, ".."))
            {
                if (depth) --depth;
                uint32_t end = length(absolute);
                while (end > 1 && absolute[end - 1] != '/') --end;
                if (end > 1) --end;
                absolute[end] = 0;
                entry->inode = parents[depth];
                entry->type = user::directory_type;
                continue;
            }
            user::DirectoryEntry child{};
            bool found = false;
            for (uint32_t index = 0; user::read_directory(entry->inode, index, &child) == 1; ++index)
            {
                // PCKFS dosya adlarında büyük/küçük harf ayrımını koru.
                uint32_t i = 0;
                while (part[i] && part[i] == child.name[i]) ++i;
                if (!part[i] && !child.name[i]) { found = true; break; }
            }
            if (!found) return false;
            uint32_t end = length(absolute);
            if (end + size + 2 > MAX_PATH || depth + 1 >= MAX_PATH / 2) return false;
            if (end > 1) absolute[end++] = '/';
            copy(absolute + end, part);
            *entry = child;
            parents[++depth] = child.inode;
        }
        return true;
    }
    void Terminal::execute()
    {
        remember_command();
        history_index_ = history_count_;
        history_draft_[0] = '\0';
        scroll_offset_ = 0;
        char echoed[MAX_COMMAND + MAX_PATH + 10];
        char prompt[MAX_PATH + 10];
        make_prompt(prompt);
        const char *prefix = prompt;
        uint32_t at = 0;
        while (*prefix)
            echoed[at++] = *prefix++;
        for (uint32_t i = 0; i < command_length_ && at + 1 < sizeof(echoed); ++i)
            echoed[at++] = command_[i];
        echoed[at] = '\0';
        print_line(echoed);

        if (equal(command_, "help"))
        {
            print_line("DOSYA VE DIZIN");
            print_line("  pwd          Bulunulan dizin");
            print_line("  cd [yol]     Dizin degistir (.., /, -)");
            print_line("  ls [yol]     Dizin icerigini listele");
            print_line("  cat <yol>    Metin dosyasini oku");
            print_line("  mkdir <yol>  Dizin olustur");
            print_line("  touch <yol>  Bos dosya olustur");
            print_line("");
            print_line("UYGULAMA VE KABUK");
            print_line("  run <yol>    Uygulama baslat");
            print_line("  ps           Surecleri listele");
            print_line("  echo <metin> Metni yazdir");
            print_line("  history      Komut gecmisi");
            print_line("  clear        Ekrani temizle");
            print_line("  exit         Terminali kapat");
            print_line("");
            print_line("SISTEM");
            print_line("  about        Sistem tanitimi");
            print_line("  date         Tarih ve saat");
            print_line("  uptime       Calisma suresi (saniye)");
            print_line("  mem          Cekirdek heap kullanimi");
            print_line("  threads      Thread bilgileri");
            print_line("  ticks        PIT sayaci");
            print_line("  dmesg        Cekirdek gunlugu");
            print_line("");
            print_line("KISAYOLLAR");
            print_line("  Ctrl+C       Uygulamayi / satiri iptal et");
            print_line("  Ctrl+L       Ekrani temizle");
            print_line("  Ctrl+D       Bos satirda girdiyi bitir");
            print_line("  Yukari/Asagi Komut gecmisi");
            print_line("  PgUp/PgDn    Ciktiyi kaydir");
        }
        else if (equal(command_, "clear"))
            clear_output();
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
            print_line(working_directory_);
        else if (equal(command_, "exit"))
            user::exit(0);
        else if (equal(command_, "history"))
        {
            for (uint32_t i = 0; i < history_count_; ++i)
                print_line(command_history_[i]);
        }
        else if (equal(command_, "ps"))
        {
            print_line("PID      Uygulama");
            for (uint32_t i = 0; i < user::process_count(); ++i)
            {
                user::ProcessInfo info{};
                if (user::process_get_info(i, &info) < 0) continue;
                char row[64]{};
                to_number(info.id, row);
                uint32_t pos = length(row);
                while (pos < 9) row[pos++] = ' ';
                copy(row + pos, info.name);
                print_line(row);
            }
        }
        else if (equal(command_, "date"))
        {
            user::DateTime now{};
            if (user::get_datetime(&now) < 0) print_line("Saat okunamadi.");
            else
            {
                char row[64]{};
                uint32_t values[] = {now.year, now.month, now.day, now.hour, now.minute, now.second};
                uint32_t pos = 0;
                for (uint32_t i = 0; i < 6; ++i)
                {
                    if (i && values[i] < 10) row[pos++] = '0';
                    to_number(values[i], row + pos);
                    pos = length(row);
                    if (i < 5) row[pos++] = i < 2 ? '-' : i == 2 ? ' ' : ':';
                }
                row[pos] = 0;
                print_line(row);
            }
        }
        else if (equal(command_, "uptime"))
        {
            char value[12];
            to_number(static_cast<uint32_t>(user::get_ticks()) / 100, value);
            print_line(value);
        }
        else if (equal(command_, "echo")) print_line("");
        else if (starts_with(command_, "echo ")) print_line(command_ + 5);
        else if (starts_with(command_, "mkdir ") || starts_with(command_, "touch "))
        {
            const bool make_directory = starts_with(command_, "mkdir ");
            const char *argument = command_ + 6;
            while (*argument == ' ') ++argument;
            char path[MAX_PATH];
            if (!make_absolute_path(argument, working_directory_, path, sizeof(path)))
                print_line(make_directory ? "Kullanim: mkdir <yol>" : "Kullanim: touch <yol>");
            else
            {
                const int32_t result = make_directory
                    ? user::create_directory(path) : user::create_file(path);
                if (result < 0)
                    print_line(make_directory ? "Dizin olusturulamadi." : "Dosya olusturulamadi.");
                else
                    print_line(make_directory ? "Dizin olusturuldu." : "Bos dosya olusturuldu.");
            }
        }
        else if (equal(command_, "cd") || starts_with(command_, "cd ") ||
                 equal(command_, "ls") || starts_with(command_, "ls ") ||
                 starts_with(command_, "cat ") || starts_with(command_, "run "))
        {
            const bool change = command_[0] == 'c' && command_[1] == 'd';
            const bool list = command_[0] == 'l';
            const bool run = command_[0] == 'r';
            const char *argument = command_ + (change || list ? 2 : 3);
            while (*argument == ' ') ++argument;
            if (!*argument) argument = change ? "/" : ".";
            if (change && equal(argument, "-")) argument = previous_directory_;
            char absolute[MAX_PATH];
            user::DirectoryEntry entry{};
            if (!*argument || !resolve_path(argument, absolute, &entry))
                print_line("Yol bulunamadi veya cok uzun.");
            else if (change)
            {
                if (entry.type != user::directory_type) print_line("Bu yol bir dizin degil.");
                else
                {
                    copy(previous_directory_, working_directory_);
                    copy(working_directory_, absolute);
                }
            }
            else if (list)
            {
                if (entry.type != user::directory_type) print_entry(entry);
                else
                {
                    user::DirectoryEntry child{};
                    for (uint32_t i = 0; user::read_directory(entry.inode, i, &child) == 1; ++i)
                        print_entry(child);
                }
            }
            else if (entry.type == user::directory_type) print_line("Bu yol bir dosya degil.");
            else if (run)
            {
                const char *name = absolute;
                for (const char *cursor = absolute; *cursor; ++cursor)
                    if (*cursor == '/') name = cursor + 1;
                const int32_t attached = user::terminal_attach();
                const int32_t pid = attached < 0 ? attached : user::process_spawn(absolute, name);
                if (pid < 0) print_line("Uygulama baslatilamadi.");
                else foreground_process_ = static_cast<uint32_t>(pid);
            }
            else
            {
                const int32_t descriptor = user::open(absolute);
                if (descriptor < 0) print_line("Dosya acilamadi.");
                else
                {
                    char data[512];
                    int32_t bytes;
                    while ((bytes = user::read(descriptor, data, sizeof(data))) > 0)
                        append_output(data, static_cast<uint32_t>(bytes));
                    user::close(descriptor);
                    output_open_ = false;
                    if (bytes < 0) print_line("Dosya okunamadi.");
                }
            }
        }
        else if (command_length_ != 0)
            print_line("Bilinmeyen komut. help yazin.");

        command_length_ = 0;
        command_input_.clear();
    }

}
