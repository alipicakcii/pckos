// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/rnd.hh>
#include <user/syscall/call.hh>

namespace user::lib::random
{
    Random::Random(uint32_t seed_value)
        : lock_(), state_(seed_value == 0 ? 0x6D2B79F5U : seed_value)
    {
    }

    // Tohumlama->yazılımla üretilecek sayı dizisinin nereden başlayacağını belirler.
    void Random::seed(uint32_t value)
    {
        // Birden fazla iş parçacığı aynı Random nesnesini kullanıyorsa durumu korur.
        LockGuard guard(lock_);
        // Xorshift algoritmasında tohum 0 olursa üretilen her değer 0 kalır;
        // bu sabit sıfır olmayan yedek tohumu sağlar.
        /// https://en.wikipedia.org/wiki/Xorshift
        state_ = value == 0 ? 0x6D2B79F5U : value;
    }

    // İstenen kaynağı seçer. Aralık 1..max'tir; sınır geçersizse veya kaynak hata verirse 0 döner.
    // işlemci veya yazılıma rastgele sayı üretmek için Source enumu belirteçdir
    uint32_t Random::next(uint32_t max, Source source)
    {
        if (max == 0)
            return 0;
        switch (source)
        {
        case Source::hardware:
            return next_hardware(max);
        case Source::software:
            return next_software(max);
        default:
            return 0;
        }
    }

    uint32_t Random::next_hardware(uint32_t max)
    {
      
        if (max > 0x7FFFFFFFU)
            return 0;
        // Çekirdek, RDSEED destekliyorsa CPU'dan sayı ister; destek yoksa 0 döndürür.
        const int32_t value = user::call(user::syscall_cpu_rnd, max);
        // Hata kodları negatiftir; geçerli sayılar 1..max aralığındadır.
        return value > 0 ? static_cast<uint32_t>(value) : 0;
    }

    uint32_t Random::next_software(uint32_t max)
    {
        // 2^32 olası değeri max'a böldüğümüzde artan küçük aralığı hesaplar.
        // Bu aralıktaki değerleri reddetmek, kalan alma işleminin bazı sayıları daha sık seçmesini
        // önler.
        const uint32_t threshold = static_cast<uint32_t>(-max) % max;
        // Tohum veya sayı dizisi başka bir iş parçacığı tarafından aynı anda değiştirilmesin.
        LockGuard guard(lock_);
        uint32_t value;
        do
        {
            // Xorshift32, önceki durumu bit kaydırma ve XOR işlemleriyle yeni duruma dönüştürür.
            value = state_;
            value ^= value << 13;
            value ^= value >> 17;
            value ^= value << 5;
            state_ = value;
            // Eşit dağılım için eşik değerinin altındaki sayıları yeniden üretir.
        } while (value < threshold);
        // Kalan alma işlemiyle sonucu 0..max-1 aralığına indirip 1..max aralığına taşır.
        return (value % max) + 1;
    }
}
