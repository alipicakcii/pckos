// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_TERMINAL_HH
#define PCKOS_USER_TERMINAL_HH

#include <stdint.h>
#include <user/apps/app_thread.hh>
#include <user/gui.hh>
#include <user/filesystem.hh>

namespace user::apps
{
    class Terminal
    {
    public:
        Terminal();
        void reset();
        void poll_output();
        void poll_kernel_log();
        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void key(int32_t key);
        void mouse(int32_t x, int32_t y, uint32_t buttons, bool pressed);
        void post_key(int32_t key);
        bool consume_dirty();
        static void thread_entry(void *argument);

    private:
        void print_line(const char *text);
        void print_bytes(const char *text, uint32_t length);
        void execute();
        void clear_output();
        void make_prompt(char *output) const;
        bool resolve_path(const char *path, char *absolute, user::DirectoryEntry *entry);
        void print_entry(const user::DirectoryEntry &entry);
        void append_output(const char *text, uint32_t length);
        void scroll_lines();
        void remember_command();
        void recall_previous_command();
        void recall_next_command();

        static constexpr uint32_t MAX_LINES = 128;
        static constexpr uint32_t MAX_COLUMNS = 120;
        static constexpr uint32_t MAX_COMMAND = 127;
        static constexpr uint32_t COMMAND_HISTORY_SIZE = 16;
        static constexpr uint32_t MAX_PATH = 128;
        char working_directory_[MAX_PATH];
        char previous_directory_[MAX_PATH];
        char lines_[MAX_LINES][MAX_COLUMNS];
        char command_history_[COMMAND_HISTORY_SIZE][MAX_COMMAND + 1];
        char history_draft_[MAX_COMMAND + 1];
        uint32_t line_count_;
        uint32_t history_count_;
        uint32_t history_index_;
        uint32_t scroll_offset_;
        uint32_t scrollbar_x_;
        uint32_t scrollbar_y_;
        uint32_t scrollbar_height_;
        uint32_t scrollbar_thumb_y_;
        uint32_t scrollbar_thumb_height_;
        uint32_t maximum_scroll_;
        int32_t scrollbar_grab_offset_;
        bool scrollbar_dragging_;
        uint32_t foreground_process_;
        uint32_t log_cursor_;
        bool output_open_;
        char command_[MAX_COMMAND + 1];
        user::gui::TextInput command_input_;
        uint32_t command_length_;
        AppEventQueue<32> events_;
        SpinLock lock_;
        volatile uint32_t dirty_;
    };
}

#endif
