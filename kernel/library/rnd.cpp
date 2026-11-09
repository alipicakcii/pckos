// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/rnd.hh>

/**
 * cpu tarafından bir rastgele
 * sayı oluşturulur max uzunluk yine 32 bit işaretsiz sayıdır
 */
extern "C" uint32_t rand_genaration();
/**
 * x68 mimaride işlemcinin rdseed desteğini dönderir
 */
extern "C" bool is_rdseed();

namespace lib
{

    uint32_t rand(uint32_t max)
    {
        // is_rdseed() sistem cagrilarinda abiyiş bozdu
        //TODO: is_rdseed() assemblu fonskiyonu ABI kontrol edilecek
        //if (max == 0 || !is_rdseed()) 
    
        if (max == 0)
            return 0;

        uint32_t random = rand_genaration();

        return (random % max) + 1; // 1...max arasında sayı üretir
    }

} // namespace cpu
