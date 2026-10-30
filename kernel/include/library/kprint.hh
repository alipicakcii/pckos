// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef LIB_KERNEL_PCKOS_KPRINT_HH
#define LIB_KERNEL_PCKOS_KPRINT_HH

extern "C" int __kprintf(const char *format, ...);

// Kernel messages go to the serial port and the in-memory system log.
template <typename... Args>
void _kprintf(const char *format, Args&&... args)
{
    __kprintf(format, static_cast<Args&&>(args)...);
}

#endif
