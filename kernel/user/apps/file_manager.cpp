// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
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

    FileManager::FileManager()
    {
        dirty_ = 0;
        reset();
    }

    void FileManager::reset()
    {
        LockGuard guard(lock_);
        depth_ = 0;
        const int32_t root = user::root_inode();
        directory_ = root < 0 ? 0 : static_cast<uint32_t>(root);
        selected_ = 0;
        selected_valid_ = false;
        path_[0] = '/';
        path_[1] = '\0';
        preview_size_ = 0;
        last_click_index_ = UINT32_MAX;
        last_click_ticks_ = 0;
        dirty_ = 1;
    }

    uint32_t FileManager::child_count()
    {
        user::DirectoryEntry entry{};
        uint32_t count = 0;
        while (count < 256 && user::read_directory(directory_, count, &entry) == 1)
            ++count;
        return count;
    }

    void FileManager::draw_row(uint32_t x, uint32_t y, uint32_t width,
                               uint32_t index, bool selected)
    {
        user::DirectoryEntry entry{};
        if (user::read_directory(directory_, index, &entry) != 1)
            return;
        const uint32_t background = selected ? user::gui::COLOR_SELECTION
                                              : user::gui::COLOR_WHITE;
        const uint32_t foreground = selected ? user::gui::COLOR_WHITE
                                              : user::gui::COLOR_TEXT;
        user::gui::push_clip(x, y, width, ROW_HEIGHT);
        user::gui::fill(x, y, width, ROW_HEIGHT, background);
        small_file_icon(x + 3, y + 2, entry.type == user::directory_type);
        const bool show_size = width >= 220;
        const uint32_t size_column = show_size ? width - 88 : width - 4;
        const uint32_t name_chars = (size_column - 29) / user::gui::FONT_WIDTH;
        draw_name(x + 25, y + 3, entry.name, name_chars, foreground);
        if (entry.type != user::directory_type && show_size)
        {
            char size[11];
            number(entry.size, size);
            user::gui::text(x + size_column, y + 3, size,
                            selected ? user::gui::COLOR_WHITE : 0xFF505050);
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
        user::gui::frame(x + 76, y + 6, width - 84, 26,
                         user::gui::COLOR_WHITE, true);
        user::gui::push_clip(x + 82, y + 9, width - 96, 20);
        const uint32_t path_columns = (width - 100) / user::gui::FONT_WIDTH;
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
        if (area.list_width - 4 >= 220)
            user::gui::text(list_x + area.list_width - 90, list_y + 6,
                             "Boyut", user::gui::COLOR_TEXT);
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
            user::gui::fill(pane_x + 7, list_y + 29, pane_width - 14, 1,
                            0xFF808080);
            const uint32_t row_height = user::gui::FONT_HEIGHT + 2;
            const uint32_t max_lines = area.list_height > 43
                ? (area.list_height - 43) / row_height : 0;
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
                user::gui::text(pane_x + 10, list_y + 39 + line * row_height,
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
        selected_ = 0;
        selected_valid_ = false;
        preview_size_ = 0;
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
        preview_size_ = 0;
        last_click_index_ = UINT32_MAX;
    }

    void FileManager::activate_selected()
    {
        if (!selected_valid_)
            return;
        user::DirectoryEntry entry{};
        if (user::read_directory(directory_, selected_, &entry) != 1)
            return;
        if (entry.type == user::directory_type)
        {
            enter_directory(entry);
            return;
        }

        char file_path[128];
        copy(file_path, path_, sizeof(file_path));
        if (!append(file_path, sizeof(file_path), entry.name))
            return;
        const int32_t descriptor = user::open(file_path);
        if (descriptor < 0)
        {
            preview_size_ = 0;
            return;
        }
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

    void FileManager::key(int32_t key_value)
    {
        LockGuard guard(lock_);
        dirty_ = 1;
        const uint32_t count = child_count();
        last_click_index_ = UINT32_MAX;
        if (key_value == 0xE2 && count != 0)
        {
            if (selected_ != 0)
                --selected_;
            selected_valid_ = true;
            preview_size_ = 0;
        }
        else if (key_value == 0xE3 && count != 0)
        {
            if (selected_valid_ && selected_ + 1 < count)
                ++selected_;
            selected_valid_ = true;
            preview_size_ = 0;
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
            go_parent();
            return;
        }

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
            preview_size_ = 0;
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
