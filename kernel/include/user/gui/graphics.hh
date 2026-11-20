// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_GRAPHICS_HH
#define PCKOS_USER_GRAPHICS_HH

#include <abi/graphics.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::GraphicsInfo;

int32_t graphics_clear(uint32_t color);

int32_t graphics_rect(uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t color);

int32_t graphics_text(uint32_t x, uint32_t y, const char *text,
                                 uint32_t length, uint32_t color);

int32_t graphics_get_info(GraphicsInfo *info);

int32_t graphics_cursor(uint32_t x, uint32_t y, bool visible);

int32_t graphics_get_cursor(int32_t *x, int32_t *y);

int32_t graphics_register_window(uint32_t x, uint32_t y,
                                            uint32_t width, uint32_t height);

int32_t graphics_move_window(uint32_t x, uint32_t y);

int32_t graphics_bitmap(uint32_t x, uint32_t y,
                                   const uint32_t *argb_pixels,
                                   uint32_t width, uint32_t height);

    // Kare yalnızca end_frame sonrasında görünür olur; kırpma durumu process başına iç içe tutulur.
int32_t graphics_begin_frame();

int32_t graphics_end_frame();

int32_t graphics_push_clip(uint32_t x, uint32_t y,
                                      uint32_t width, uint32_t height);

int32_t graphics_pop_clip();
}

#endif
