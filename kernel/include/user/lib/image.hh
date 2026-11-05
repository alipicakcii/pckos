// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_IMAGE_HH
#define PCKOS_USER_IMAGE_HH

#include <stdint.h>

namespace user::image
{
    struct Image
    {
        uint32_t width;
        uint32_t height;
        uint32_t source_width;
        uint32_t source_height;
        uint32_t *pixels;
    };

    bool load_png(const char *path, uint32_t max_width, uint32_t max_height,
                  Image *image, bool cover = false);
    void release(Image *image);
}

#endif
