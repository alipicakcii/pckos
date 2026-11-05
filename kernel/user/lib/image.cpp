// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/image.hh>
#include <user/lib/new.hh>
#include <user/lib/memory.hh>
#include <user/syscall.hh>
#include <limits.h>

namespace
{
    void *decoder_allocate(size_t size)
    {
        return size <= UINT32_MAX ? user::heap_alloc(static_cast<uint32_t>(size))
                                  : nullptr;
    }

    void *decoder_reallocate(void *pointer, size_t old_size, size_t new_size)
    {
        if (pointer == nullptr)
            return decoder_allocate(new_size);
        if (new_size == 0)
        {
            (void)user::heap_free(pointer);
            return nullptr;
        }
        void *replacement = decoder_allocate(new_size);
        if (replacement == nullptr)
            return nullptr;
        const size_t copy_size = old_size < new_size ? old_size : new_size;
        (void)::memcpy(replacement, pointer, copy_size);
        (void)user::heap_free(pointer);
        return replacement;
    }

    void decoder_release(void *pointer)
    {
        if (pointer != nullptr)
            (void)user::heap_free(pointer);
    }
}

#define STBI_NO_STDIO
// Kullanıcı çalışma zamanı henüz TLS sağlamıyor.
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_STD_LIBC
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STBI_ASSERT(expression) ((void)0)
#define STBI_MALLOC(size) decoder_allocate(size)
#define STBI_REALLOC_SIZED(pointer, old_size, new_size) \
    decoder_reallocate(pointer, old_size, new_size)
#define STBI_FREE(pointer) decoder_release(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace user::image
{
    namespace
    {
        constexpr uint32_t MAX_ENCODED_SIZE = 8 * 1024 * 1024;
        constexpr uint32_t MAX_SOURCE_PIXELS = 4 * 1024 * 1024;

        bool read_image_file(const char *path, uint8_t **data, uint32_t *size)
        {
            if (path == nullptr || data == nullptr || size == nullptr)
                return false;
            *data = nullptr;
            *size = 0;
            const int32_t descriptor = user::open(path);
            if (descriptor < 0)
                return false;

            uint32_t capacity = 65536;
            auto *buffer = static_cast<uint8_t *>(user::heap_alloc(capacity));
            if (buffer == nullptr)
            {
                user::close(descriptor);
                return false;
            }
            uint32_t used = 0;
            bool valid = true;
            for (;;)
            {
                if (used == capacity)
                {
                    if (capacity >= MAX_ENCODED_SIZE)
                    {
                        valid = false;
                        break;
                    }
                    const uint32_t next_capacity = capacity * 2;
                    auto *next = static_cast<uint8_t *>(
                        user::heap_alloc(next_capacity));
                    if (next == nullptr)
                    {
                        valid = false;
                        break;
                    }
                    (void)::memcpy(next, buffer, used);
                    (void)user::heap_free(buffer);
                    buffer = next;
                    capacity = next_capacity;
                }
                const int32_t count = user::read(descriptor, buffer + used,
                    capacity - used < 4096 ? capacity - used : 4096);
                if (count < 0)
                {
                    valid = false;
                    break;
                }
                if (count == 0)
                    break;
                used += static_cast<uint32_t>(count);
            }
            user::close(descriptor);
            if (!valid || used == 0)
            {
                user::heap_free(buffer);
                return false;
            }
            *data = buffer;
            *size = used;
            return true;
        }
    }

    bool load_png(const char *path, uint32_t max_width, uint32_t max_height,
                  Image *image, bool cover)
    {
        if (image == nullptr || max_width == 0 || max_height == 0)
            return false;
        *image = {};

        uint8_t *encoded = nullptr;
        uint32_t encoded_size = 0;
        if (!read_image_file(path, &encoded, &encoded_size) ||
            encoded_size > static_cast<uint32_t>(INT_MAX))
            return false;

        int width = 0;
        int height = 0;
        int channels = 0;
        bool valid = stbi_info_from_memory(encoded, static_cast<int>(encoded_size),
                                           &width, &height, &channels) != 0 &&
            width > 0 && width <= 8192 && height > 0 && height <= 8192 &&
            static_cast<uint64_t>(width) * static_cast<uint32_t>(height) <=
                MAX_SOURCE_PIXELS;
        if (!valid)
        {
            user::heap_free(encoded);
            return false;
        }

        int decoded_width = 0;
        int decoded_height = 0;
        int decoded_channels = 0;
        auto *rgba = stbi_load_from_memory(encoded, static_cast<int>(encoded_size),
            &decoded_width, &decoded_height, &decoded_channels, 4);
        user::heap_free(encoded);
        if (rgba == nullptr || decoded_width <= 0 || decoded_height <= 0)
        {
            if (rgba != nullptr)
                stbi_image_free(rgba);
            return false;
        }

        const uint32_t source_width = static_cast<uint32_t>(decoded_width);
        const uint32_t source_height = static_cast<uint32_t>(decoded_height);
        if (static_cast<uint64_t>(max_width) * max_height > 1024 * 1024)
        {
            stbi_image_free(rgba);
            return false;
        }
        uint32_t output_width = source_width;
        uint32_t output_height = source_height;
        uint32_t crop_x = 0;
        uint32_t crop_y = 0;
        uint32_t crop_width = source_width;
        uint32_t crop_height = source_height;
        if (cover)
        {
            output_width = max_width;
            output_height = max_height;
            if (source_width * max_height > source_height * max_width)
            {
                crop_width = source_height * max_width / max_height;
                crop_x = (source_width - crop_width) / 2;
            }
            else
            {
                crop_height = source_width * max_height / max_width;
                crop_y = (source_height - crop_height) / 2;
            }
        }
        else if (output_width > max_width || output_height > max_height)
        {
            if (source_width * max_height > source_height * max_width)
            {
                output_width = max_width;
                output_height = source_height * max_width / source_width;
            }
            else
            {
                output_height = max_height;
                output_width = source_width * max_height / source_height;
            }
            if (output_width == 0) output_width = 1;
            if (output_height == 0) output_height = 1;
        }

        auto *pixels = new (std::nothrow) uint32_t[output_width * output_height];
        if (pixels == nullptr)
        {
            stbi_image_free(rgba);
            return false;
        }
        for (uint32_t y = 0; y < output_height; ++y)
        {
            const uint32_t source_y = crop_y + y * crop_height / output_height;
            for (uint32_t x = 0; x < output_width; ++x)
            {
                const uint32_t source_x = crop_x + x * crop_width / output_width;
                const uint32_t source = (source_y * source_width + source_x) * 4;
                const uint8_t red = rgba[source];
                const uint8_t green = rgba[source + 1];
                const uint8_t blue = rgba[source + 2];
                const uint8_t alpha = rgba[source + 3];
                pixels[y * output_width + x] =
                    (static_cast<uint32_t>(alpha) << 24) |
                    (static_cast<uint32_t>(red) << 16) |
                    (static_cast<uint32_t>(green) << 8) | blue;
            }
        }
        stbi_image_free(rgba);
        *image = {output_width, output_height, source_width, source_height, pixels};
        return true;
    }

    void release(Image *image)
    {
        if (image == nullptr)
            return;
        delete[] image->pixels;
        *image = {};
    }
}
