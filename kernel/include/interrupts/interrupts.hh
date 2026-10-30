// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef ARCH_I386_INTERRUPTS_TOOL_H
#define ARCH_I386_INTERRUPTS_TOOL_H

namespace cpu
{
    extern "C" void enable_interrupt();
    extern "C" void disable_interrupt();
    extern "C" void cpu_halt();
} // namespace kernel

#endif