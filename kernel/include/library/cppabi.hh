// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_CPPABI_HH
#define PCKOS_CPPABI_HH

extern "C"
{
    void _init();
    int __cxa_atexit(void (*destructor)(void *), void *object, void *dso);
    [[noreturn]] void __cxa_pure_virtual();
}

#endif
