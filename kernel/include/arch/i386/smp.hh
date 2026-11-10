// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_SMP_HH
#define PCKOS_KERNEL_SMP_HH

#include <stdint.h>

namespace mem { class Vmm; }
namespace kernel { struct Registers; }

namespace kernel::smp
{
    constexpr uint32_t MAX_CPUS = 32;
    constexpr uint32_t TIMER_VECTOR = 0xD0;
    constexpr uint32_t TLB_VECTOR = 0xE1;
    constexpr uint32_t PARK_VECTOR = 0xF2;
    constexpr uint32_t SPURIOUS_VECTOR = 0xFF;

    struct CpuTopology
    {
        uint32_t count;
        uint32_t apic_ids[MAX_CPUS];
        uint32_t local_apic_address;
        bool acpi_found;
        bool local_apic_found;
    };

    CpuTopology discover_cpus();
    uint32_t initialize(const CpuTopology &topology, mem::Vmm &memory);
    void release_processors();
    uint32_t current_cpu();
    uint32_t online_count();
    void end_interrupt();

    // İlk SMP sürümünde çekirdek verileri ortak kilitle korunur; kullanıcı kodu paralel çalışır.
    void kernel_enter();
    void kernel_leave();
    bool kernel_owned();
    bool kernel_waiting();
    uint32_t suspend_kernel();
    void resume_kernel(uint32_t depth);
    void invalidate_remote_tlb();
    // Yalnızca yeni eşlemelerde, çekirdek kilidi bırakılmadan kullanılmalıdır.
    class NewMappingBatch
    {
    public:
        NewMappingBatch();
        ~NewMappingBatch();
        NewMappingBatch(const NewMappingBatch &) = delete;
        NewMappingBatch &operator=(const NewMappingBatch &) = delete;
    };
    void handle_tlb_interrupt();
    Registers *handle_park_interrupt(Registers *registers);
    void pause_other_cpus();
    void resume_other_cpus();

    class CpuRendezvous
    {
    public:
        CpuRendezvous() { pause_other_cpus(); }
        ~CpuRendezvous() { resume_other_cpus(); }
        CpuRendezvous(const CpuRendezvous &) = delete;
        CpuRendezvous &operator=(const CpuRendezvous &) = delete;
    };
}

extern "C" void smp_kernel_leave();
extern "C" void smp_invalidate_remote_tlb();
#endif
