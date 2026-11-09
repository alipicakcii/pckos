// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/rnd.hh>

extern "C" bool rdseed_generate(uint32_t *result);
extern "C" bool is_rdseed();

namespace lib
{
    uint32_t rand(uint32_t max)
    {
        if (max == 0 || !is_rdseed())
            return 0;

        // 2^32'nin max'a bölümünden kalanı dışlayarak modulo yanlılığını önler.
        const uint32_t threshold = (uint32_t(0) - max) % max;
        for (uint32_t attempt = 0; attempt < 100; ++attempt)
        {
            uint32_t random = 0;
            if (!rdseed_generate(&random))
                return 0;
            if (random >= threshold)
                return (random % max) + 1;
        }
        return 0;
    }

}
