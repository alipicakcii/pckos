// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/filesystem.hh>
#include <user/io.hh>
#include <user/system.hh>
#include <user/thread.hh>
#include <user/apps/file_manager.hh>
#include <user/gui.hh>

namespace user::apps
{
    namespace
    {
        constexpr uint32_t ROW_HEIGHT = 22;

        struct Layout
        {
            uint32_t list_width;
            uint32_t list_height;
            uint32_t visible_rows;
            bool preview;
        };

        Layout layout(uint32_t width, uint32_t height)
        {
            const bool preview = width >= 420;
            const uint32_t list_width = preview ? width * 58 / 100 : width - 14;
            const uint32_t list_height = height - 72;
            return {list_width, list_height, (list_height - 28) / ROW_HEIGHT,
                    preview};
        }

        uint32_t length(const char *text)
        {
            uint32_t result = 0;
            while (text[result] != '\0')
                ++result;
            return result;
        }

        char lower_ascii(char value)
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a') : value;
        }

        bool is_png(const char *name)
        {
            const uint32_t size = length(name);
            return size >= 4 && name[size - 4] == '.' &&
                lower_ascii(name[size - 3]) == 'p' &&
                lower_ascii(name[size - 2]) == 'n' &&
                lower_ascii(name[size - 1]) == 'g';
        }

        const char *entry_type(const user::DirectoryEntry &entry)
        {
            if (entry.type == user::directory_type)
                return "Klasor";
            const char *extension = nullptr;
            for (const char *at = entry.name; *at != '\0'; ++at)
                if (*at == '.') extension = at + 1;
            if (extension == nullptr) return "Dosya";
            const uint32_t extension_length = length(extension);
            if (extension_length == 3 && (lower_ascii(extension[0]) == 't') &&
                (lower_ascii(extension[1]) == 'x') &&
                (lower_ascii(extension[2]) == 't'))
                return "TXT";
            if (extension_length == 3 && (lower_ascii(extension[0]) == 'b') &&
                (lower_ascii(extension[1]) == 'm') &&
                (lower_ascii(extension[2]) == 'p'))
                return "BMP";
            if (extension_length == 3 && (lower_ascii(extension[0]) == 'p') &&
                (lower_ascii(extension[1]) == 'n') &&
                (lower_ascii(extension[2]) == 'g'))
                return "PNG";
            if (extension_length == 3 && (lower_ascii(extension[0]) == 'b') &&
                (lower_ascii(extension[1]) == 'i') &&
                (lower_ascii(extension[2]) == 'n'))
                return "BIN";
            if (extension_length == 3 && (lower_ascii(extension[0]) == 'e') &&
                (lower_ascii(extension[1]) == 'l') &&
                (lower_ascii(extension[2]) == 'f'))
                return "ELF";
            return "Dosya";
        }

        void copy(char *destination, const char *source, uint32_t capacity)
        {
            if (capacity == 0)
                return;
            uint32_t i = 0;
            while (i + 1 < capacity && source[i] != '\0')
            {
                destination[i] = source[i];
                ++i;
            }
            destination[i] = '\0';
        }

        void append_text(char *destination, uint32_t capacity, const char *text)
        {
            uint32_t at = length(destination);
            while (text[0] != '\0' && at + 1 < capacity)
                destination[at++] = *text++;
            destination[at] = '\0';
        }

        bool append(char *path, uint32_t capacity, const char *name)
        {
            uint32_t path_length = length(path);
            const uint32_t name_length = length(name);
            const bool separator = path_length > 1;
            if (path_length + (separator ? 1 : 0) + name_length + 1 > capacity)
                return false;
            if (separator)
                path[path_length++] = '/';
            for (uint32_t i = 0; i < name_length; ++i)
                path[path_length++] = name[i];
            path[path_length] = '\0';
            return true;
        }

        void number(uint32_t value, char *output)
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

        void small_file_icon(uint32_t x, uint32_t y, bool directory)
        {
            if (directory)
            {
                user::gui::fill(x + 1, y + 5, 17, 12, 0xFF805000);
                user::gui::fill(x + 2, y + 3, 8, 4, 0xFFFFD040);
                user::gui::fill(x, y + 6, 18, 9, 0xFFFFC030);
                user::gui::fill(x + 2, y + 8, 14, 5, 0xFFFFE080);
            }
            else
            {
                user::gui::fill(x + 3, y + 1, 12, 16, 0xFFFFFFFF);
                user::gui::fill(x + 3, y + 1, 12, 1, 0xFF808080);
                user::gui::fill(x + 3, y + 1, 1, 16, 0xFF808080);
                user::gui::fill(x + 6, y + 6, 7, 1, 0xFF606060);
                user::gui::fill(x + 6, y + 9, 7, 1, 0xFF606060);
                user::gui::fill(x + 6, y + 12, 5, 1, 0xFF606060);
            }
        }

        void draw_name(uint32_t x, uint32_t y, const char *name,
                       uint32_t max_chars, uint32_t color)
        {
            char clipped[128];
            uint32_t i = 0;
            while (i < max_chars && i + 1 < sizeof(clipped) && name[i] != '\0')
            {
                clipped[i] = name[i];
                ++i;
            }
            clipped[i] = '\0';
            if (name[i] != '\0' && i >= 3)
            {
                clipped[i - 1] = '.';
                clipped[i - 2] = '.';
                clipped[i - 3] = '.';
            }
            user::gui::text(x, y, clipped, color);
        }
    }

    FileManager::FileManager() : search_input_(search_, sizeof(search_))
    {
        dirty_ = 0;
        reset();
    }

    void FileManager::reset()
    {
        LockGuard guard(lock_);
        clear_preview();
        depth_ = 0;
        const int32_t root = user::root_inode();
        directory_ = root < 0 ? 0 : static_cast<uint32_t>(root);
        selected_ = 0;
        selected_valid_ = false;
        search_input_.clear();
        search_focused_ = false;
        path_[0] = '/';
        path_[1] = '\0';
        last_click_index_ = UINT32_MAX;
        last_click_ticks_ = 0;
        dirty_ = 1;
    }

    uint32_t FileManager::child_count()
    {
        uint32_t count = 0;
        for (uint32_t index = 0; index < 256; ++index)
        {
            user::DirectoryEntry entry{};
            if (user::read_directory(directory_, index, &entry) != 1)
                break;
            if (matches_search(entry.name))
                ++count;
        }
        return count;
    }

    bool FileManager::matches_search(const char *name) const
    {
        if (search_input_.length() == 0)
            return true;
        for (uint32_t start = 0; name[start] != '\0'; ++start)
        {
            uint32_t offset = 0;
            while (offset < search_input_.length() && name[start + offset] != '\0' &&
                   lower_ascii(name[start + offset]) == lower_ascii(search_[offset]))
                ++offset;
            if (offset == search_input_.length())
                return true;
        }
        return false;
    }

    bool FileManager::filtered_entry(uint32_t index, user::DirectoryEntry *result)
    {
        if (result == nullptr)
            return false;
        uint32_t visible_index = 0;
        for (uint32_t disk_index = 0; disk_index < 256; ++disk_index)
        {
            user::DirectoryEntry entry{};
            if (user::read_directory(directory_, disk_index, &entry) != 1)
                return false;
            if (!matches_search(entry.name))
                continue;
            if (visible_index++ == index)
            {
                *result = entry;
                return true;
            }
        }
        return false;
    }

    void FileManager::draw_row(uint32_t x, uint32_t y, uint32_t width,
                               uint32_t index, bool selected)
    {
        user::DirectoryEntry entry{};
        if (!filtered_entry(index, &entry))
            return;
        const uint32_t background = selected ? user::gui::COLOR_SELECTION
                                              : user::gui::COLOR_WHITE;
        const uint32_t foreground = selected ? user::gui::COLOR_WHITE
                                              : user::gui::COLOR_TEXT;
        user::gui::push_clip(x, y, width, ROW_HEIGHT);
        user::gui::fill(x, y, width, ROW_HEIGHT, background);
        small_file_icon(x + 3, y + 2, entry.type == user::directory_type);
        const bool show_details = width >= 300;
        const uint32_t type_column = show_details ? width - 124 : width - 4;
        const uint32_t name_chars = (type_column - 29) / user::gui::FONT_WIDTH;
        draw_name(x + 25, y + 3, entry.name, name_chars, foreground);
        if (show_details)
        {
            user::gui::text(x + type_column, y + 3, entry_type(entry),
                            selected ? user::gui::COLOR_WHITE : 0xFF505050);
            if (entry.type != user::directory_type)
            {
                char size[11];
                number(entry.size, size);
                user::gui::text(x + width - 55, y + 3, size,
                                selected ? user::gui::COLOR_WHITE : 0xFF505050);
            }
        }
        user::gui::pop_clip();
    }

    void FileManager::draw(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height)
    {
        LockGuard guard(lock_);
        if (width < 180 || height < 122)
            return;
        const Layout area = layout(width, height);
        user::gui::push_clip(x, y, width, height);
        user::gui::fill(x, y, width, height, user::gui::COLOR_FACE);
        user::gui::button(x + 7, y + 6, 62, 26, "Ust");
        const bool search_enabled = width >= 500;
        const uint32_t path_width = search_enabled ? width - 260 : width - 84;
        user::gui::frame(x + 76, y + 6, path_width, 26,
                         user::gui::COLOR_WHITE, true);
        user::gui::push_clip(x + 82, y + 9, path_width - 12, 20);
        const uint32_t path_columns = (path_width - 20) / user::gui::FONT_WIDTH;
        const uint32_t path_length = length(path_);
        if (path_length > path_columns && path_columns > 3)
        {
            user::gui::text(x + 84, y + 11, "...", user::gui::COLOR_TEXT);
            user::gui::text(x + 84 + 3 * user::gui::FONT_WIDTH, y + 11,
                path_ + path_length - (path_columns - 3), user::gui::COLOR_TEXT);
        }
        else
            user::gui::text(x + 84, y + 11, path_, user::gui::COLOR_TEXT);
        user::gui::pop_clip();
        if (search_enabled)
        {
            const uint32_t search_x = x + width - 174;
            user::gui::frame(search_x, y + 6, 167, 26,
                             user::gui::COLOR_WHITE, true);
            user::gui::text(search_x + 5, y + 11, "Ara:",
                            user::gui::COLOR_DARK);
            user::gui::push_clip(search_x + 38, y + 9, 123, 20);
            search_input_.draw_inline(search_x + 40, y + 11, 123,
                search_focused_, user::gui::COLOR_TEXT);
            user::gui::pop_clip();
        }

        const uint32_t list_x = x + 7;
        const uint32_t list_y = y + 40;
        const uint32_t count = child_count();
        if (selected_ >= count && count != 0)
            selected_ = count - 1;
        const uint32_t first = selected_ >= area.visible_rows
            ? selected_ - area.visible_rows + 1 : 0;

        user::gui::frame(list_x, list_y, area.list_width, area.list_height,
                         user::gui::COLOR_WHITE, true);
        user::gui::push_clip(list_x + 2, list_y + 2,
                             area.list_width - 4, area.list_height - 4);
        user::gui::fill(list_x + 2, list_y + 2, area.list_width - 4, 24,
                        user::gui::COLOR_FACE);
        user::gui::text(list_x + 8, list_y + 6, "Ad", user::gui::COLOR_TEXT);
        if (area.list_width - 4 >= 300)
        {
            user::gui::text(list_x + area.list_width - 126, list_y + 6,
                            "Tur", user::gui::COLOR_TEXT);
            user::gui::text(list_x + area.list_width - 58, list_y + 6,
                            "Boyut", user::gui::COLOR_TEXT);
        }
        for (uint32_t row = 0; row < area.visible_rows && first + row < count; ++row)
            draw_row(list_x + 2, list_y + 26 + row * ROW_HEIGHT,
                     area.list_width - 4, first + row,
                     selected_valid_ && first + row == selected_);
        if (count == 0)
            draw_name(list_x + 10, list_y + 32, "Bu klasor bos",
                       (area.list_width - 20) / user::gui::FONT_WIDTH, 0xFF606060);
        user::gui::pop_clip();

        if (area.preview)
        {
            const uint32_t pane_x = list_x + area.list_width + 7;
            const uint32_t pane_width = width - area.list_width - 21;
            user::gui::frame(pane_x, list_y, pane_width, area.list_height,
                             user::gui::COLOR_WHITE, true);
            user::gui::push_clip(pane_x + 2, list_y + 2, pane_width - 4,
                                 area.list_height - 4);
            user::gui::text(pane_x + 10, list_y + 8, "Onizleme",
                            user::gui::COLOR_TITLE);
            char metadata[64] = "Dosya secin";
            if (selected_valid_)
            {
                user::DirectoryEntry entry{};
                if (filtered_entry(selected_, &entry) &&
                    entry.type != user::directory_type)
                {
                    metadata[0] = '\0';
                    append_text(metadata, sizeof(metadata), entry_type(entry));
                    if (preview_image_.pixels != nullptr)
                    {
                        append_text(metadata, sizeof(metadata), " ");
                        char dimension[11];
                        number(preview_image_.source_width, dimension);
                        append_text(metadata, sizeof(metadata), dimension);
                        append_text(metadata, sizeof(metadata), "x");
                        number(preview_image_.source_height, dimension);
                        append_text(metadata, sizeof(metadata), dimension);
                    }
                    else
                    {
                        append_text(metadata, sizeof(metadata), " | ");
                        char size[11];
                        number(entry.size, size);
                        append_text(metadata, sizeof(metadata), size);
                        append_text(metadata, sizeof(metadata), " bayt");
                    }
                }
            }
            const bool has_preview = preview_size_ != 0 ||
                preview_image_.pixels != nullptr;
            user::gui::text(pane_x + 10, list_y + 32, metadata,
                            has_preview ? user::gui::COLOR_TEXT : 0xFF606060);
            user::gui::fill(pane_x + 7, list_y + 53, pane_width - 14, 1,
                            0xFF808080);
            const uint32_t row_height = user::gui::FONT_HEIGHT + 2;
            if (preview_image_.pixels != nullptr)
            {
                user::gui::bitmap(pane_x + 10, list_y + 63,
                    preview_image_.pixels, preview_image_.width,
                    preview_image_.height);
            }
            else
            {
                const uint32_t max_lines = area.list_height > 67
                    ? (area.list_height - 67) / row_height : 0;
                const uint32_t max_columns = (pane_width - 20) / user::gui::FONT_WIDTH;
                const char *text = preview_size_ == 0 ? "Metin dosyasi secin" : preview_;
                const uint32_t text_size = preview_size_ == 0 ? 18 : preview_size_;
                uint32_t offset = 0;
                uint32_t line = 0;
                while (offset < text_size && line < max_lines)
                {
                    char segment[128];
                    uint32_t columns = 0;
                    while (offset < text_size && text[offset] != '\n' &&
                           text[offset] != '\r' && columns < max_columns &&
                           columns + 1 < sizeof(segment))
                        segment[columns++] = text[offset++];
                    segment[columns] = '\0';
                    user::gui::text(pane_x + 10, list_y + 63 + line * row_height,
                                     segment, preview_size_ == 0 ? 0xFF606060
                                                                : user::gui::COLOR_TEXT);
                    if (offset < text_size && text[offset] == '\r')
                    {
                        ++offset;
                        if (offset < text_size && text[offset] == '\n')
                            ++offset;
                    }
                    else if (offset < text_size && text[offset] == '\n')
                        ++offset;
                    ++line;
                }
            }
            user::gui::pop_clip();
        }

        char status[16];
        number(count, status);
        user::gui::text(x + 8, y + height - 21, status, 0xFF505050);
        user::gui::text(x + 8 + user::gui::text_width(status) + user::gui::FONT_WIDTH,
                         y + height - 21, "oge", 0xFF505050);
        user::gui::pop_clip();
    }

    bool FileManager::enter_directory(const user::DirectoryEntry &entry)
    {
        if (entry.type != user::directory_type || depth_ >= 31)
            return false;
        char next[128];
        copy(next, path_, sizeof(next));
        if (!append(next, sizeof(next), entry.name))
            return false;
        directory_stack_[depth_++] = directory_;
        directory_ = entry.inode;
        copy(path_, next, sizeof(path_));
        search_input_.clear();
        search_focused_ = false;
        selected_ = 0;
        selected_valid_ = false;
        clear_preview();
        last_click_index_ = UINT32_MAX;
        return true;
    }

    void FileManager::go_parent()
    {
        if (depth_ == 0)
            return;
        directory_ = directory_stack_[--depth_];
        uint32_t size = length(path_);
        while (size > 1 && path_[size - 1] != '/')
            --size;
        if (size > 1)
            path_[size - 1] = '\0';
        else
            path_[1] = '\0';
        selected_ = 0;
        selected_valid_ = false;
        clear_preview();
        last_click_index_ = UINT32_MAX;
    }

    void FileManager::activate_selected()
    {
        if (!selected_valid_)
            return;
        user::DirectoryEntry entry{};
        if (!filtered_entry(selected_, &entry))
            return;
        if (entry.type == user::directory_type)
        {
            enter_directory(entry);
            return;
        }

        load_preview(entry);
    }

    void FileManager::load_preview(const user::DirectoryEntry &entry)
    {
        clear_preview();
        if (entry.type == user::directory_type)
            return;

        char file_path[128];
        copy(file_path, path_, sizeof(file_path));
        if (!append(file_path, sizeof(file_path), entry.name))
            return;
        if (is_png(entry.name))
        {
            (void)user::image::load_png(file_path, 160, 170, &preview_image_);
            return;
        }
        const int32_t descriptor = user::open(file_path);
        if (descriptor < 0)
            return;
        const int32_t bytes = user::read(descriptor, preview_, sizeof(preview_));
        user::close(descriptor);
        preview_size_ = bytes > 0 ? static_cast<uint32_t>(bytes) : 0;
        for (uint32_t i = 0; i < preview_size_; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(preview_[i]);
            if (c < 32 && c != '\n' && c != '\r')
                preview_[i] = ' ';
            else if (c > 126)
                preview_[i] = '?';
        }
    }

    void FileManager::clear_preview()
    {
        user::image::release(&preview_image_);
        preview_size_ = 0;
    }

    void FileManager::key(int32_t key_value)
    {
        LockGuard guard(lock_);
        dirty_ = 1;
        if (search_focused_)
        {
            last_click_index_ = UINT32_MAX;
            if (key_value == '\n')
                search_focused_ = false;
            else
                (void)search_input_.handle_key(key_value);
            selected_ = 0;
            selected_valid_ = false;
            clear_preview();
            return;
        }
        const uint32_t count = child_count();
        last_click_index_ = UINT32_MAX;
        if (key_value == 0xE2 && count != 0)
        {
            if (selected_ != 0)
                --selected_;
            selected_valid_ = true;
            clear_preview();
        }
        else if (key_value == 0xE3 && count != 0)
        {
            if (selected_valid_ && selected_ + 1 < count)
                ++selected_;
            selected_valid_ = true;
            clear_preview();
        }
        else if (key_value == '\b')
            go_parent();
        else if (key_value == '\n')
            activate_selected();
    }

    void FileManager::click(int32_t x, int32_t y, uint32_t width,
                            uint32_t height)
    {
        LockGuard guard(lock_);
        dirty_ = 1;
        if (width < 180 || height < 122)
            return;
        const Layout area = layout(width, height);
        if (y >= 6 && y < 32 && x >= 7 && x < 69)
        {
            search_focused_ = false;
            go_parent();
            return;
        }
        if (width >= 500 && y >= 6 && y < 32 && x >= static_cast<int32_t>(width - 174))
        {
            search_focused_ = true;
            search_input_.place_cursor_from_x(static_cast<uint32_t>(x),
                                              width - 134, 123);
            return;
        }

        search_focused_ = false;
        if (x < 9 || x >= static_cast<int32_t>(5 + area.list_width) || y < 66)
            return;
        const uint32_t row = static_cast<uint32_t>(y - 66) / ROW_HEIGHT;
        if (row >= area.visible_rows)
            return;
        const uint32_t first = selected_ >= area.visible_rows
            ? selected_ - area.visible_rows + 1 : 0;
        const uint32_t index = first + row;
        if (index >= child_count())
            return;

        const uint32_t now = static_cast<uint32_t>(user::get_ticks());
        if (last_click_index_ == index && now - last_click_ticks_ <= 45)
        {
            selected_ = index;
            selected_valid_ = true;
            activate_selected();
            last_click_index_ = UINT32_MAX;
        }
        else
        {
            selected_ = index;
            selected_valid_ = true;
            clear_preview();
            user::DirectoryEntry entry{};
            if (filtered_entry(selected_, &entry) &&
                entry.type != user::directory_type)
                load_preview(entry);
            last_click_index_ = index;
            last_click_ticks_ = now;
        }
    }

    void FileManager::post_key(int32_t key_value)
    {
        events_.push({1, key_value, 0, 0, 0});
    }

    void FileManager::post_click(int32_t x, int32_t y, uint32_t width,
                                 uint32_t height)
    {
        events_.push({2, x, y, width, height});
    }

    bool FileManager::consume_dirty()
    {
        LockGuard guard(lock_);
        const bool dirty = dirty_ != 0;
        dirty_ = 0;
        return dirty;
    }

    void FileManager::thread_entry(void *argument)
    {
        auto *manager = static_cast<FileManager *>(argument);
        if (manager == nullptr)
            user::thread_exit();
        for (;;)
        {
            AppEvent event{};
            if (!manager->events_.pop(&event))
            {
                user::thread_sleep(1);
                continue;
            }
            if (event.type == 1)
                manager->key(event.value0);
            else if (event.type == 2)
                manager->click(event.value0, event.value1,
                               event.value2, event.value3);
        }
    }
}
