// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <stdint.h>
#include <mem/multiboot.hh>
#include <mem/vmm.hh>

namespace device
{

class FrameBuffer
{
private:
        uint32_t* fb_address;
        uint32_t  width;
        uint32_t  height;
        uint32_t  pitch;
        uint32_t  bpp; // Piksel başına bit sayısı
        uint32_t* back_buffer;
        uint32_t* presented_buffer;
        uint32_t* background_buffer;
        static constexpr uint32_t MAX_WINDOW_LAYERS = 7;
        struct WindowLayer
        {
            uint32_t process_id;
            uint32_t x, y, width, height;
            uint32_t z;
            uint32_t *pixels;
            bool visible;
        };
        WindowLayer window_layers[MAX_WINDOW_LAYERS];
        WindowLayer *selected_layer;
        uint32_t selected_process_id;
        uint32_t next_z;
        bool composing;
        bool drawing_frame;
        bool mapped;
        uint32_t clip_left, clip_top, clip_right, clip_bottom;
        uint32_t dirty_left, dirty_top, dirty_right, dirty_bottom;

        void mark_dirty(uint32_t left, uint32_t top,
                        uint32_t right, uint32_t bottom);
        void compose_dirty();
        WindowLayer *find_window(uint32_t process_id);

public:
        explicit FrameBuffer(mem::multiboot_info_t* mbi);
        ~FrameBuffer() = default;

        void initialize(mem::Vmm &vmm);

        // Özel çizim fonksiyonları
        void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
        uint32_t get_pixel(uint32_t x, uint32_t y) const;
        void clear(uint32_t color);

        // Kare RAM üzerinde birleştirilir. VRAM’e yalnızca son görüntüde değişen pikseller aktarılır.
        bool begin_frame();
        void end_frame();
        bool frame_active() const { return drawing_frame || composing; }
        void set_clip(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void reset_clip();
        void select_process(uint32_t process_id);
        bool register_window(uint32_t process_id, uint32_t x, uint32_t y,
                             uint32_t window_width, uint32_t window_height);
        bool move_window(uint32_t process_id, uint32_t x, uint32_t y);
        bool set_window_visible(uint32_t process_id, bool visible);
        bool raise_window(uint32_t process_id);
        void unregister_window(uint32_t process_id);
        uint32_t window_at(uint32_t x, uint32_t y) const;
        void abort_frame();

        // Yazılımsal imleç bir katmandır; RAM görüntülerinin hiçbirine dahil değildir.
        void draw_hardware_pixel(uint32_t x, uint32_t y, uint32_t color);
        uint32_t get_presented_pixel(uint32_t x, uint32_t y) const;

        uint32_t get_width() const { return width; }
        uint32_t get_height() const { return height; }

        uint32_t* get_buffer_address();
        uint32_t get_pitch();


        void draw_char(uint32_t x, uint32_t y, char c, uint32_t color);
        void draw_char_scaled(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale);
        void draw_string(uint32_t x, uint32_t y, const char* str, uint32_t color);

        
};

} // namespace device

#endif // FRAME_BUFFER_H
