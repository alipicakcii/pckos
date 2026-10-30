// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/video/frame_buffer.hh>
#include <device/video/font8x16.hh>
#include <library/kprint.hh>
#include <mem/alloc.hh>

namespace device
{
    namespace
    {
        // Two 16 MiB RAM images at most; enough for 2560x1440 without allowing
        // malformed boot metadata to request an unbounded kernel allocation.
        constexpr uint32_t MAX_BUFFER_PIXELS = 4 * 1024 * 1024;
        constexpr uint32_t FONT_WIDTH = 8;
        constexpr uint32_t FONT_HEIGHT = 16;
    }

    FrameBuffer::FrameBuffer(mem::multiboot_info_t* mbi)
        : fb_address(nullptr), width(0), height(0), pitch(0), bpp(0),
          back_buffer(nullptr), presented_buffer(nullptr), background_buffer(nullptr),
          window_layers{}, selected_layer(nullptr), selected_process_id(0),
          next_z(0), composing(false), drawing_frame(false),
          mapped(false), clip_left(0), clip_top(0), clip_right(0), clip_bottom(0),
          dirty_left(0), dirty_top(0), dirty_right(0), dirty_bottom(0)
    {
        if (mbi == nullptr ||
            (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) == 0)
            return;

        const uint64_t pixels = static_cast<uint64_t>(mbi->framebuffer_width) *
                                mbi->framebuffer_height;
        const uint64_t size = static_cast<uint64_t>(mbi->framebuffer_pitch) *
                              mbi->framebuffer_height;
        // Drawing and cursor colors use packed 0xAARRGGBB throughout the ABI.
        if (mbi->framebuffer_type != 1 || mbi->framebuffer_bpp != 32 ||
            mbi->framebuffer_red_field_position != 16 ||
            mbi->framebuffer_green_field_position != 8 ||
            mbi->framebuffer_blue_field_position != 0 ||
            mbi->framebuffer_red_mask_size != 8 ||
            mbi->framebuffer_green_mask_size != 8 ||
            mbi->framebuffer_blue_mask_size != 8 ||
            pixels == 0 || pixels > MAX_BUFFER_PIXELS ||
            mbi->framebuffer_pitch < static_cast<uint64_t>(mbi->framebuffer_width) * 4 ||
            size > UINT32_MAX || mbi->framebuffer_addr > UINT32_MAX ||
            mbi->framebuffer_addr + size > 0xFFC00000ULL)
            return;

        fb_address = reinterpret_cast<uint32_t*>(
            static_cast<uintptr_t>(mbi->framebuffer_addr));
        width = mbi->framebuffer_width;
        height = mbi->framebuffer_height;
        pitch = mbi->framebuffer_pitch;
        bpp = mbi->framebuffer_bpp;
        reset_clip();
    }

    void FrameBuffer::initialize(mem::Vmm &vmm)
    {
        if (width == 0 || height == 0)
        {
            _kprintf("[FrameBuffer] Unsupported framebuffer; RGB 32 bpp required.\n");
            return;
        }
        if (mapped)
            return;

        const uint32_t address = reinterpret_cast<uintptr_t>(fb_address);
        const uint32_t page_base = address & ~(PAGESIZE_4K - 1U);
        const uint32_t page_end = (address + pitch * height + PAGESIZE_4K - 1U) &
                                  ~(PAGESIZE_4K - 1U);
        for (uint32_t page = page_base; page < page_end; page += PAGESIZE_4K)
        {
            mem::VirtualAddress virtual_address(page);
            mem::PhysicalAddress physical_address(page);
            if (!vmm.add_page_to_pd(virtual_address, physical_address,
                                    PG_PRESENT | PG_WRITE | 0x10))
            {
                _kprintf("[FrameBuffer] Mapping failed at 0x%x.\n", page);
                width = height = 0;
                reset_clip();
                return;
            }
        }
        mapped = true;

        const uint32_t bytes = width * height * sizeof(uint32_t);
        mem::MemAlloc allocator;
        back_buffer = static_cast<uint32_t*>(allocator.kmalloc(bytes));
        presented_buffer = static_cast<uint32_t*>(allocator.kmalloc(bytes));
        background_buffer = static_cast<uint32_t*>(allocator.kmalloc(bytes));
        if (back_buffer == nullptr || presented_buffer == nullptr)
        {
            allocator.kfree(back_buffer);
            allocator.kfree(presented_buffer);
            allocator.kfree(background_buffer);
            back_buffer = presented_buffer = nullptr;
            background_buffer = nullptr;
            _kprintf("[FrameBuffer] RAM buffers unavailable.\n");
        }
        if (background_buffer == nullptr)
            _kprintf("[FrameBuffer] Window background buffer unavailable.\n");

        // Initialize without reading undefined allocation contents.
        const uint32_t background = 0xFF1E1E1E;
        for (uint32_t y = 0; y < height; ++y)
        {
            auto *video = reinterpret_cast<volatile uint32_t*>(
                reinterpret_cast<uint8_t*>(fb_address) + y * pitch);
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t index = y * width + x;
                if (back_buffer != nullptr)
                {
                    back_buffer[index] = background;
                    presented_buffer[index] = background;
                }
                if (background_buffer != nullptr)
                    background_buffer[index] = background;
                video[x] = background;
            }
        }
        _kprintf("[FrameBuffer] %d x %d, RAM composition: %s.\n", width, height,
                 back_buffer != nullptr ? "ready" : "unavailable");
    }

    uint32_t* FrameBuffer::get_buffer_address()
    {
        return fb_address;
    }

    uint32_t FrameBuffer::get_pitch()
    {
        return pitch;
    }

    void FrameBuffer::set_clip(uint32_t x, uint32_t y,
                              uint32_t clip_width, uint32_t clip_height)
    {
        clip_left = x < width ? x : width;
        clip_top = y < height ? y : height;
        // Subtraction before addition avoids wraparound for user coordinates.
        clip_right = clip_left + (clip_width < width - clip_left
            ? clip_width : width - clip_left);
        clip_bottom = clip_top + (clip_height < height - clip_top
            ? clip_height : height - clip_top);
    }

    void FrameBuffer::reset_clip()
    {
        set_clip(0, 0, width, height);
    }

    void FrameBuffer::mark_dirty(uint32_t left, uint32_t top,
                                 uint32_t right, uint32_t bottom)
    {
        if (left < dirty_left) dirty_left = left;
        if (top < dirty_top) dirty_top = top;
        if (right > dirty_right) dirty_right = right;
        if (bottom > dirty_bottom) dirty_bottom = bottom;
    }

    FrameBuffer::WindowLayer *FrameBuffer::find_window(uint32_t process_id)
    {
        for (uint32_t i = 0; i < MAX_WINDOW_LAYERS; ++i)
            if (window_layers[i].process_id == process_id)
                return &window_layers[i];
        return nullptr;
    }

    void FrameBuffer::select_process(uint32_t process_id)
    {
        selected_process_id = process_id;
        selected_layer = process_id > 1 ? find_window(process_id) : nullptr;
    }

    void FrameBuffer::compose_dirty()
    {
        if (background_buffer == nullptr || presented_buffer == nullptr ||
            dirty_left >= dirty_right || dirty_top >= dirty_bottom)
            return;
        composing = true;
        for (uint32_t y = dirty_top; y < dirty_bottom; ++y)
        {
            auto *video = reinterpret_cast<volatile uint32_t*>(
                reinterpret_cast<uint8_t*>(fb_address) + y * pitch);
            for (uint32_t x = dirty_left; x < dirty_right; ++x)
            {
                uint32_t color = background_buffer[y * width + x];
                uint32_t top_z = 0;
                for (uint32_t i = 0; i < MAX_WINDOW_LAYERS; ++i)
                {
                    const WindowLayer &layer = window_layers[i];
                    if (layer.process_id == 0 || !layer.visible ||
                        x < layer.x || y < layer.y ||
                        x - layer.x >= layer.width ||
                        y - layer.y >= layer.height || layer.z < top_z)
                        continue;
                    const uint32_t candidate = layer.pixels[
                        (y - layer.y) * layer.width + (x - layer.x)];
                    if ((candidate >> 24) == 0)
                        continue;
                    color = candidate;
                    top_z = layer.z;
                }
                const uint32_t index = y * width + x;
                back_buffer[index] = color;
                if (presented_buffer[index] != color)
                {
                    video[x] = color;
                    presented_buffer[index] = color;
                }
            }
        }
        composing = false;
        dirty_left = width;
        dirty_top = height;
        dirty_right = dirty_bottom = 0;
    }

    bool FrameBuffer::register_window(uint32_t process_id, uint32_t x, uint32_t y,
                                      uint32_t window_width, uint32_t window_height)
    {
        if (process_id <= 1 || find_window(process_id) != nullptr ||
            window_width == 0 || window_height == 0 ||
            x >= width || y >= height || window_width > width - x ||
            window_height > height - y)
            return false;
        WindowLayer *slot = nullptr;
        for (uint32_t i = 0; i < MAX_WINDOW_LAYERS; ++i)
            if (window_layers[i].process_id == 0)
            {
                slot = &window_layers[i];
                break;
            }
        if (slot == nullptr)
            return false;
        mem::MemAlloc allocator;
        uint32_t *pixels = static_cast<uint32_t*>(allocator.kmalloc(
            window_width * window_height * sizeof(uint32_t)));
        if (pixels == nullptr)
            return false;
        for (uint32_t i = 0; i < window_width * window_height; ++i)
            pixels[i] = 0;
        *slot = {process_id, x, y, window_width, window_height,
                 ++next_z, pixels, true};
        if (selected_process_id == process_id)
            selected_layer = slot;
        return true;
    }

    bool FrameBuffer::move_window(uint32_t process_id, uint32_t x, uint32_t y)
    {
        WindowLayer *layer = find_window(process_id);
        if (layer == nullptr || x >= width || y >= height ||
            layer->width > width - x || layer->height > height - y)
            return false;
        if (layer->x == x && layer->y == y)
            return true;
        if (!drawing_frame)
        {
            dirty_left = width;
            dirty_top = height;
            dirty_right = dirty_bottom = 0;
        }
        mark_dirty(layer->x, layer->y, layer->x + layer->width,
                   layer->y + layer->height);
        layer->x = x;
        layer->y = y;
        mark_dirty(x, y, x + layer->width, y + layer->height);
        if (!drawing_frame) compose_dirty();
        return true;
    }

    bool FrameBuffer::set_window_visible(uint32_t process_id, bool visible)
    {
        WindowLayer *layer = find_window(process_id);
        if (layer == nullptr) return false;
        if (layer->visible == visible) return true;
        dirty_left = width;
        dirty_top = height;
        dirty_right = dirty_bottom = 0;
        layer->visible = visible;
        mark_dirty(layer->x, layer->y, layer->x + layer->width,
                   layer->y + layer->height);
        compose_dirty();
        return true;
    }

    bool FrameBuffer::raise_window(uint32_t process_id)
    {
        WindowLayer *layer = find_window(process_id);
        if (layer == nullptr) return false;
        dirty_left = width;
        dirty_top = height;
        dirty_right = dirty_bottom = 0;
        layer->z = ++next_z;
        mark_dirty(layer->x, layer->y, layer->x + layer->width,
                   layer->y + layer->height);
        compose_dirty();
        return true;
    }

    void FrameBuffer::unregister_window(uint32_t process_id)
    {
        WindowLayer *layer = find_window(process_id);
        if (layer == nullptr) return;
        const uint32_t x = layer->x, y = layer->y;
        const uint32_t window_width = layer->width;
        const uint32_t window_height = layer->height;
        mem::MemAlloc allocator;
        allocator.kfree(layer->pixels);
        if (selected_layer == layer)
        {
            selected_layer = nullptr;
            selected_process_id = 0;
        }
        *layer = {};
        dirty_left = width;
        dirty_top = height;
        dirty_right = dirty_bottom = 0;
        mark_dirty(x, y, x + window_width, y + window_height);
        compose_dirty();
    }

    uint32_t FrameBuffer::window_at(uint32_t x, uint32_t y) const
    {
        uint32_t process_id = 1;
        uint32_t top_z = 0;
        for (uint32_t i = 0; i < MAX_WINDOW_LAYERS; ++i)
        {
            const WindowLayer &layer = window_layers[i];
            if (layer.process_id != 0 && layer.visible && layer.z > top_z &&
                x >= layer.x && y >= layer.y &&
                x - layer.x < layer.width && y - layer.y < layer.height)
            {
                process_id = layer.process_id;
                top_z = layer.z;
            }
        }
        return process_id;
    }

    void FrameBuffer::abort_frame()
    {
        drawing_frame = false;
        reset_clip();
    }

    bool FrameBuffer::begin_frame()
    {
        if (drawing_frame || composing || back_buffer == nullptr ||
            presented_buffer == nullptr || background_buffer == nullptr)
            return false;
        dirty_left = width;
        dirty_top = height;
        dirty_right = dirty_bottom = 0;
        drawing_frame = true;
        return true;
    }

    void FrameBuffer::end_frame()
    {
        if (!drawing_frame)
            return;
        compose_dirty();
        drawing_frame = false;
    }

    void FrameBuffer::draw_hardware_pixel(uint32_t x, uint32_t y, uint32_t color)
    {
        if (!mapped || x >= width || y >= height)
            return;
        auto *pixel = reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<uint8_t*>(fb_address) + y * pitch + x * 4);
        *pixel = color;
    }

    uint32_t FrameBuffer::get_presented_pixel(uint32_t x, uint32_t y) const
    {
        if (!mapped || x >= width || y >= height)
            return 0;
        if (presented_buffer != nullptr)
            return presented_buffer[y * width + x];
        const auto *pixel = reinterpret_cast<const volatile uint32_t*>(
            reinterpret_cast<const uint8_t*>(fb_address) + y * pitch + x * 4);
        return *pixel;
    }

    void FrameBuffer::draw_pixel(uint32_t x, uint32_t y, uint32_t color)
    {
        if (!mapped || x < clip_left || y < clip_top ||
            x >= clip_right || y >= clip_bottom)
            return;
        if (selected_process_id == 1 && background_buffer != nullptr)
        {
            const uint32_t index = y * width + x;
            if (background_buffer[index] == color) return;
            background_buffer[index] = color;
            if (drawing_frame) mark_dirty(x, y, x + 1, y + 1);
            return;
        }
        if (selected_process_id > 1)
        {
            if (selected_layer == nullptr || x < selected_layer->x ||
                y < selected_layer->y ||
                x - selected_layer->x >= selected_layer->width ||
                y - selected_layer->y >= selected_layer->height)
                return;
            const uint32_t index = (y - selected_layer->y) * selected_layer->width +
                                   (x - selected_layer->x);
            if (selected_layer->pixels[index] == color) return;
            selected_layer->pixels[index] = color;
            if (drawing_frame) mark_dirty(x, y, x + 1, y + 1);
            return;
        }
        const uint32_t index = y * width + x;
        if (back_buffer != nullptr)
        {
            if (back_buffer[index] == color)
                return;
            back_buffer[index] = color;
            if (drawing_frame)
            {
                mark_dirty(x, y, x + 1, y + 1);
                return;
            }
            presented_buffer[index] = color;
        }
        draw_hardware_pixel(x, y, color);
    }

    uint32_t FrameBuffer::get_pixel(uint32_t x, uint32_t y) const
    {
        if (!mapped || x >= width || y >= height)
            return 0;
        if (selected_process_id == 1 && background_buffer != nullptr)
            return background_buffer[y * width + x];
        if (selected_layer != nullptr && x >= selected_layer->x &&
            y >= selected_layer->y &&
            x - selected_layer->x < selected_layer->width &&
            y - selected_layer->y < selected_layer->height)
            return selected_layer->pixels[(y - selected_layer->y) *
                selected_layer->width + (x - selected_layer->x)];
        if (back_buffer != nullptr)
            return back_buffer[y * width + x];
        return get_presented_pixel(x, y);
    }

    void FrameBuffer::clear(uint32_t color)
    {
        if (!mapped || clip_left >= clip_right || clip_top >= clip_bottom)
            return;
        if (selected_process_id == 1 && background_buffer != nullptr)
        {
            for (uint32_t y = clip_top; y < clip_bottom; ++y)
                for (uint32_t x = clip_left; x < clip_right; ++x)
                    background_buffer[y * width + x] = color;
            if (drawing_frame)
                mark_dirty(clip_left, clip_top, clip_right, clip_bottom);
            return;
        }
        if (selected_process_id > 1 && selected_layer != nullptr)
        {
            const uint32_t left = clip_left > selected_layer->x
                ? clip_left : selected_layer->x;
            const uint32_t top = clip_top > selected_layer->y
                ? clip_top : selected_layer->y;
            const uint32_t right = clip_right < selected_layer->x + selected_layer->width
                ? clip_right : selected_layer->x + selected_layer->width;
            const uint32_t bottom = clip_bottom < selected_layer->y + selected_layer->height
                ? clip_bottom : selected_layer->y + selected_layer->height;
            for (uint32_t y = top; y < bottom; ++y)
                for (uint32_t x = left; x < right; ++x)
                    selected_layer->pixels[(y - selected_layer->y) *
                        selected_layer->width + (x - selected_layer->x)] = color;
            if (drawing_frame && left < right && top < bottom)
                mark_dirty(left, top, right, bottom);
            return;
        }
        if (drawing_frame)
        {
            for (uint32_t y = clip_top; y < clip_bottom; ++y)
                for (uint32_t x = clip_left; x < clip_right; ++x)
                    back_buffer[y * width + x] = color;
            mark_dirty(clip_left, clip_top, clip_right, clip_bottom);
            return;
        }
        for (uint32_t y = clip_top; y < clip_bottom; ++y)
        {
            auto *video = reinterpret_cast<volatile uint32_t*>(
                reinterpret_cast<uint8_t*>(fb_address) + y * pitch);
            for (uint32_t x = clip_left; x < clip_right; ++x)
            {
                const uint32_t index = y * width + x;
                if (back_buffer != nullptr)
                {
                    back_buffer[index] = color;
                    presented_buffer[index] = color;
                }
                video[x] = color;
            }
        }
    }

    void FrameBuffer::draw_char(uint32_t x, uint32_t y, char c, uint32_t color)
    {
        if (x >= width || y >= height)
            return;
        const uint8_t character = static_cast<uint8_t>(c);
        const uint8_t* glyph = font8x16[character < 128 ? character : '?'];
        const uint32_t rows = height - y < FONT_HEIGHT ? height - y : FONT_HEIGHT;
        const uint32_t columns = width - x < FONT_WIDTH ? width - x : FONT_WIDTH;
        for (uint32_t row = 0; row < rows; ++row)
            for (uint32_t column = 0; column < columns; ++column)
                if (glyph[row] & (1U << (7 - column)))
                    draw_pixel(x + column, y + row, color);
    }

    void FrameBuffer::draw_char_scaled(uint32_t x, uint32_t y, char c,
                                       uint32_t color, uint32_t scale)
    {
        if (scale == 0 || scale > 16 || x >= width || y >= height)
            return;
        const uint8_t character = static_cast<uint8_t>(c);
        const uint8_t* glyph = font8x16[character < 128 ? character : '?'];
        const uint32_t rows = height - y < FONT_HEIGHT * scale
            ? height - y : FONT_HEIGHT * scale;
        const uint32_t columns = width - x < FONT_WIDTH * scale
            ? width - x : FONT_WIDTH * scale;
        for (uint32_t row = 0; row < rows; ++row)
            for (uint32_t column = 0; column < columns; ++column)
                if (glyph[row / scale] & (1U << (7 - column / scale)))
                    draw_pixel(x + column, y + row, color);
    }

    void FrameBuffer::draw_string(uint32_t x, uint32_t y,
                                  const char* str, uint32_t color)
    {
        if (str == nullptr)
            return;
        uint32_t current_x = x;
        uint32_t current_y = y;
        while (*str && current_y < height)
        {
            if (*str == '\n')
            {
                if (current_y > UINT32_MAX - 18)
                    break;
                current_y += 18;
                current_x = x;
            }
            else
            {
                draw_char(current_x, current_y, *str, color);
                if (current_x > UINT32_MAX - FONT_WIDTH)
                    break;
                current_x += FONT_WIDTH;
            }
            ++str;
        }
    }
}
