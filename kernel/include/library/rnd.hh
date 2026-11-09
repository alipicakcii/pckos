// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_LIBRARY_RND_HH
#define PCKOS_LIBRARY_RND_HH

#include <stdint.h>

namespace lib
{
    // 1...max arasında sayı üretir. max sıfırsa, destek yoksa veya üretim
    // denemeleri başarısız olursa 0 döner.
    uint32_t rand(uint32_t max);
}

#endif
