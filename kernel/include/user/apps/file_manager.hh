// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_FILE_MANAGER_HH
#define PCKOS_USER_FILE_MANAGER_HH

#include <stdint.h>
#include <user/apps/app_thread.hh>
#include <user/syscall.hh>

namespace user::apps
{
    class FileManager
    {
    public:
        FileManager();
        void reset();
        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void key(int32_t key);
        void click(int32_t x, int32_t y, uint32_t width, uint32_t height);
        void post_key(int32_t key);
        void post_click(int32_t x, int32_t y, uint32_t width, uint32_t height);
        bool consume_dirty();
        static void thread_entry(void *argument);

    private:
        uint32_t child_count();
        void activate_selected();
        void go_parent();
        bool enter_directory(const user::DirectoryEntry &entry);
        void draw_row(uint32_t x, uint32_t y, uint32_t width,
                      uint32_t index, bool selected);

        uint32_t directory_stack_[32];
        uint32_t depth_;
        uint32_t directory_;
        uint32_t selected_;
        bool selected_valid_;
        char path_[128];
        char preview_[512];
        uint32_t preview_size_;
        uint32_t last_click_index_;
        uint32_t last_click_ticks_;
        AppEventQueue<32> events_;
        SpinLock lock_;
        volatile uint32_t dirty_;
    };
}

#endif
