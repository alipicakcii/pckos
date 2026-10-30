// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_TERMINAL_HH
#define PCKOS_USER_TERMINAL_HH

#include <stdint.h>
#include <user/apps/app_thread.hh>

namespace user::apps
{
    class Terminal
    {
    public:
        Terminal();
        void reset();
        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void key(int32_t key);
        void post_key(int32_t key);
        bool consume_dirty();
        static void thread_entry(void *argument);

    private:
        void print_line(const char *text);
        void print_bytes(const char *text, uint32_t length);
        void execute();
        void scroll_lines();

        static constexpr uint32_t MAX_LINES = 48;
        static constexpr uint32_t MAX_COLUMNS = 120;
        static constexpr uint32_t MAX_COMMAND = 127;
        char lines_[MAX_LINES][MAX_COLUMNS];
        uint32_t line_count_;
        char command_[MAX_COMMAND + 1];
        uint32_t command_length_;
        AppEventQueue<32> events_;
        SpinLock lock_;
        volatile uint32_t dirty_;
    };
}

#endif
