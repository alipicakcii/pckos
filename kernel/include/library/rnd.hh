// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef ARCH_I386_RND_H
#define ARCH_I386_RND_H

#include <stdint.h>

namespace lib
{
    /*
     * max 32 bit unsigned bir determinist rastgele sayı oluşturur
     * @param
     * max: maximum oluşturabilecek rastegeke sayı sınırı
     */

    uint32_t rand(uint32_t max);
} // namespace kernel

#endif