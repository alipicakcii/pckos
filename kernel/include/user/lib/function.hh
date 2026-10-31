
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef KLIB_FUNCTION_KERNEL_HH
#define KLIB_FUNCTION_KERNEL_HH
#include <user/lib/templates.hh>



namespace lib
{
        // Bu function sınıfı yalnızca function pointer kabul eder.
        // Şimdilik lambda tanımlarında [=], [&] gibi capture (kapanım) kullanan ifadeler desteklenmez.
        // Çünkü capture'lı lambdalar function pointer'a çevrilemez.
        //
        // Capture'sız bir lambda tanımını function pointer'a çevirmek için
        // derleyiciye küçük bir ipucu vermek gerekir.
        //
        // Örnek:
        // []()->int { return 0x455; };      // ❌ derlenmez (lambda objesi)
        // +[]()->int { return 0x455; };     // ✅ derlenir (function pointer dönüşümü)

        template<typename Signature>
        class function;

        template<typename R, typename... Args>
        class function<R(Args...)>
        {
        public:
                R (*func)(Args...); 

                function() : func(nullptr)
                {

                }

                function(R(*_func)(Args...)) : func(_func)
                {

                }

                R operator()(Args... args)
                {
                        if (func)
                                return func(args...);
                        return R();
                }
        };
}


#endif
