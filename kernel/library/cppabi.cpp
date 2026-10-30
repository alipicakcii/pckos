// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/cppabi.hh>
#include <library/kernel_panic.hh>

// Çekirdek nesneleri sistem kapanana kadar yaşar; burada process sonlandırma işlemi yapılmaz.
extern "C" int __cxa_atexit(void (*)(void *), void *, void *)
{
    return 0;
}

extern "C" [[noreturn]] void __cxa_pure_virtual()
{
    lib::panic("Pure virtual function called");
    __builtin_unreachable();
}
