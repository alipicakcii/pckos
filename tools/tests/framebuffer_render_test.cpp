// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
// PCKOS’un kullandığı framebuffer kodu için ana makinede gerileme kontrolleri.
// run_framebuffer_tests.sh ile derlenir; QEMU veya fiziksel bellek eşlemesi kullanılmaz.
#define private public
#include <device/video/frame_buffer.hh>
#undef private
#include <assert.h>
#include <stdio.h>
#include <vector>

int main()
{
    constexpr uint32_t width = 31, height = 25, stride = width + 3;
    constexpr uint32_t background = 0xFF112233, white = 0xFFFFFFFF;
    std::vector<uint32_t> video(stride * height, background);
    std::vector<uint32_t> back(width * height, background);
    std::vector<uint32_t> presented(width * height, background);
    device::FrameBuffer fb(nullptr);
    fb.fb_address = video.data();
    fb.width = width;
    fb.height = height;
    fb.pitch = stride * 4;
    fb.bpp = 32;
    fb.back_buffer = back.data();
    fb.presented_buffer = presented.data();
    fb.mapped = true;
    fb.reset_clip();

    assert(fb.begin_frame());
    assert(!fb.begin_frame());
    fb.clear(0xFF000000);
    assert(video[0] == background); // Ara temizleme işlemleri donanıma aktarılmaz.
    fb.draw_pixel(5, 5, white);
    assert(fb.get_pixel(5, 5) == white);
    assert(fb.get_presented_pixel(5, 5) == background);
    fb.end_frame();
    assert(!fb.frame_active());
    assert(video[5 * stride + 5] == white);
    assert(presented[5 * width + 5] == white);
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = width; x < stride; ++x)
            assert(video[y * stride + x] == background); // VRAM satır aralığını dikkate al.

    // İmleç katmanı, saklanan sahneyi veya alfa kaynağını değiştirmez.
    fb.draw_hardware_pixel(5, 5, 0xFF00FF00);
    assert(fb.get_pixel(5, 5) == white);
    assert(fb.get_presented_pixel(5, 5) == white);
    assert(fb.begin_frame());
    fb.draw_pixel(5, 5, 0xFF000000);
    fb.draw_pixel(5, 5, white);
    fb.end_frame();
    assert(video[5 * stride + 5] == 0xFF00FF00); // Son görüntüde değişmeyen piksellere dokunulmaz.
    fb.draw_hardware_pixel(5, 5, fb.get_presented_pixel(5, 5));

    // Kırpma; temizleme, pikseller ve glifler üzerinde, taşan koordinatlar dahil etkili olur.
    const auto original = video;
    fb.set_clip(7, 6, 9, 7);
    assert(fb.begin_frame());
    fb.clear(0xFF123456);
    fb.draw_char(2, 2, 'W', white);
    fb.draw_char_scaled(2, 2, 'A', white, 2);
    fb.draw_pixel(UINT32_MAX, UINT32_MAX, white);
    fb.end_frame();
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            if (x < 7 || x >= 16 || y < 6 || y >= 13)
                assert(video[y * stride + x] == original[y * stride + x]);
    const auto clipped = video;
    fb.set_clip(UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX);
    fb.clear(white);
    assert(video == clipped);
    fb.set_clip(width - 1, height - 1, UINT32_MAX, UINT32_MAX);
    fb.clear(white);
    assert(video[(height - 1) * stride + width - 1] == white);
    fb.reset_clip();

    // Tanınmayan işaretli baytlar güvenli biçimde varsayılana döner; scale=1 normal yazı tipiyle eşleşir.
    fb.clear(0);
    fb.draw_char(1, 1, '?', white);
    const auto question = back;
    fb.clear(0);
    fb.draw_char(1, 1, static_cast<char>(255), white);
    assert(back == question);
    fb.clear(0);
    fb.draw_char_scaled(1, 1, '?', white, 1);
    assert(back == question);
    fb.draw_char_scaled(UINT32_MAX, UINT32_MAX, 'A', white, UINT32_MAX);
    assert(back == question);
    puts("framebuffer: deferred presentation, diff, pitch, cursor isolation, clipping and glyph checks passed");
}
