// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <firmware/smp.hh>
#include <kernel/task/thread.hh>
#include <kernel/syscalls/graphics.hh>
#include <interrupts/descriptortables.hh>
#include <interrupts/interrupts.hh>
#include <cpu/get_cpu.hh>
#include <cpu/io.hh>
#include <library/mem.hh>
#include <library/kprint.hh>
#include <kernel/core/panic.hh>
#include <mem/vmm.hh>

extern "C" const uint8_t smp_trampoline_start[], smp_trampoline_end[];

namespace kernel::smp
{
    namespace
    {
        constexpr uint32_t NO_OWNER = UINT32_MAX;
        constexpr uint32_t LAPIC_VIRTUAL_ADDRESS = 0x01FFF000;
        constexpr uint32_t AP_STACK_SIZE = 16384;
        constexpr uint32_t WAIT_LIMIT = 100000000;
        alignas(64) uint32_t kernel_lock = 0;
        uint32_t lock_owner = NO_OWNER;
        uint32_t lock_depth[MAX_CPUS]{};
        uint32_t waiting_for_kernel[MAX_CPUS]{};
        uint32_t apic_ids[MAX_CPUS]{};
        uint32_t apic_to_cpu[256]{};
        uint32_t online[MAX_CPUS]{};
        uint32_t cpu_count = 1;
        uint32_t processors_released = 0;
        uint32_t timer_count = 0;
        uint32_t tlb_generation = 0;
        uint32_t tlb_ack[MAX_CPUS]{};
        uint32_t mapping_batch_depth[MAX_CPUS]{};
        bool mapping_batch_pending[MAX_CPUS]{};
        uint32_t park_requested = 0;
        uint32_t parked[MAX_CPUS]{};
        uint32_t rendezvous_depth[MAX_CPUS]{};
        volatile uint32_t *lapic = nullptr;
        alignas(16) uint8_t ap_stacks[MAX_CPUS][AP_STACK_SIZE];

        uint32_t load(const uint32_t &value)
        {
            return __atomic_load_n(&value, __ATOMIC_ACQUIRE);
        }

        void store(uint32_t &destination, uint32_t value)
        {
            __atomic_store_n(&destination, value, __ATOMIC_RELEASE);
        }

        uint32_t local_apic_id()
        {
            uint32_t a, b, c, d;
            asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "a"(1), "c"(0));
            return b >> 24;
        }

        uint32_t read_apic(uint32_t offset) { return lapic[offset / 4]; }
        void write_apic(uint32_t offset, uint32_t value)
        {
            lapic[offset / 4] = value;
            (void)lapic[0x20 / 4];
        }

        bool wait_delivery()
        {
            for (uint32_t i = 0; i < WAIT_LIMIT; ++i)
            {
                if ((read_apic(0x300) & (1U << 12)) == 0) return true;
                asm volatile("pause");
            }
            return false;
        }

        bool send_ipi(uint32_t id, uint32_t command)
        {
            if (!wait_delivery()) return false;
            write_apic(0x310, id << 24);
            write_apic(0x300, command);
            return wait_delivery();
        }

        void enable_local_apic(bool bootstrap)
        {
            uint32_t low, high;
            asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1BU));
            low |= 1U << 11;
            asm volatile("wrmsr" :: "a"(low), "d"(high), "c"(0x1BU));
            write_apic(0x80, 0);
            write_apic(0x320, 1U << 16);
            const uint32_t max_lvt = (read_apic(0x30) >> 16) & 0xFF;
            if (max_lvt >= 4) write_apic(0x340, 1U << 16);
            if (max_lvt >= 5) write_apic(0x330, 1U << 16);
            write_apic(0x350, bootstrap ? 0x700 : 1U << 16);
            write_apic(0x360, bootstrap ? 0x400 : 1U << 16);
            write_apic(0x370, 1U << 16);
            write_apic(0xF0, 0x100 | SPURIOUS_VECTOR);
        }

        // PIT kanal 2 ile beklerken sistemin 100 Hz'lik kanal 0 ayarına dokunulmaz.
        bool pit_delay(uint32_t microseconds)
        {
            const uint32_t count = (1193182ULL * microseconds) / 1000000;
            const uint8_t previous = cpu::inb(0x61);
            cpu::outb(0x61, previous & ~3U);
            cpu::outb(0x43, 0xB0);
            cpu::outb(0x42, static_cast<uint8_t>(count));
            cpu::outb(0x42, static_cast<uint8_t>(count >> 8));
            cpu::outb(0x61, (previous & ~2U) | 1U);
            bool completed = false;
            for (uint32_t i = 0; i < WAIT_LIMIT; ++i)
            {
                if ((cpu::inb(0x61) & 0x20) != 0) { completed = true; break; }
                asm volatile("pause");
            }
            cpu::outb(0x61, previous);
            return completed;
        }

        bool calibrate_timer()
        {
            write_apic(0x3E0, 3);
            write_apic(0x380, UINT32_MAX);
            if (!pit_delay(10000)) return false;
            timer_count = UINT32_MAX - read_apic(0x390);
            write_apic(0x380, 0);
            return timer_count > 0;
        }

        void service_tlb()
        {
            const uint32_t index = current_cpu();
            const uint32_t generation = load(tlb_generation);
            if (load(tlb_ack[index]) == generation) return;
            const uint32_t directory = cpu::read_cr3();
            asm volatile("mov %0, %%cr3" :: "r"(directory) : "memory");
            store(tlb_ack[index], generation);
        }

        void wait_ack(uint32_t *values, uint32_t expected)
        {
            for (uint32_t index = 0; index < MAX_CPUS; ++index)
            {
                if (index == current_cpu() || load(online[index]) == 0) continue;
                bool complete = false;
                for (uint32_t spin = 0; spin < WAIT_LIMIT; ++spin)
                {
                    if (load(values[index]) == expected) { complete = true; break; }
                    asm volatile("pause");
                }
                if (!complete) lib::panic("SMP: processor did not acknowledge IPI");
            }
        }
    }

    uint32_t current_cpu() { return apic_to_cpu[local_apic_id()]; }
    uint32_t online_count() { return load(cpu_count); }
    bool kernel_owned() { return load(lock_owner) == current_cpu(); }
    bool kernel_waiting() { return load(waiting_for_kernel[current_cpu()]) != 0; }

    void kernel_enter()
    {
        uint32_t flags;
        asm volatile("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
        const uint32_t index = current_cpu();
        if (load(lock_owner) == index)
        {
            ++lock_depth[index];
            if ((flags & 0x200U) != 0) cpu::enable_interrupt();
            return;
        }
        store(waiting_for_kernel[index], 1);
        while (__atomic_exchange_n(&kernel_lock, 1U, __ATOMIC_ACQUIRE) != 0)
        {
            // Bekleyen işlemci TLB/durdurma IPI'lerini alabilmelidir.
            asm volatile("sti; pause; cli" ::: "memory");
        }
        store(waiting_for_kernel[index], 0);
        store(lock_owner, index);
        lock_depth[index] = 1;
        if ((flags & 0x200U) != 0) cpu::enable_interrupt();
    }

    void kernel_leave()
    {
        const uint32_t index = current_cpu();
        if (load(lock_owner) != index || lock_depth[index] == 0) return;
        if (--lock_depth[index] != 0) return;
        // Ortak çizim karesi, begin_frame ile end_frame arasında tek işlemciye aittir.
        if (syscall_graphics_frame_active()) return;
        store(lock_owner, NO_OWNER);
        __atomic_store_n(&kernel_lock, 0U, __ATOMIC_RELEASE);
    }

    uint32_t suspend_kernel()
    {
        const uint32_t index = current_cpu();
        if (load(lock_owner) != index) return 0;
        const uint32_t depth = lock_depth[index];
        lock_depth[index] = 0;
        store(lock_owner, NO_OWNER);
        __atomic_store_n(&kernel_lock, 0U, __ATOMIC_RELEASE);
        return depth;
    }

    void resume_kernel(uint32_t depth)
    {
        if (depth == 0) return;
        kernel_enter();
        lock_depth[current_cpu()] = depth;
    }

    void end_interrupt()
    {
        if (lapic != nullptr) write_apic(0xB0, 0);
    }

    void invalidate_remote_tlb()
    {
        if (load(processors_released) == 0 || online_count() < 2) return;
        if (mapping_batch_depth[current_cpu()] != 0)
        {
            mapping_batch_pending[current_cpu()] = true;
            return;
        }
        const uint32_t generation = __atomic_add_fetch(&tlb_generation, 1U, __ATOMIC_RELEASE);
        store(tlb_ack[current_cpu()], generation);
        for (uint32_t index = 0; index < MAX_CPUS; ++index)
            if (index != current_cpu() && load(online[index]) != 0 && load(parked[index]) == 0)
                if (!send_ipi(apic_ids[index], TLB_VECTOR))
                    lib::panic("SMP: TLB IPI delivery failed");
        wait_ack(tlb_ack, generation);
    }

    NewMappingBatch::NewMappingBatch()
    {
        if (load(processors_released) != 0 && !kernel_owned())
            lib::panic("SMP: mapping batch requires kernel lock");
        ++mapping_batch_depth[current_cpu()];
    }

    NewMappingBatch::~NewMappingBatch()
    {
        const uint32_t index = current_cpu();
        if (--mapping_batch_depth[index] == 0 && mapping_batch_pending[index])
        {
            mapping_batch_pending[index] = false;
            // Yeni bellek çağırana verilmeden önce bütün işlemcilerin onayını bekle.
            invalidate_remote_tlb();
        }
    }

    void handle_tlb_interrupt()
    {
        service_tlb();
        end_interrupt();
    }

    void pause_other_cpus()
    {
        if (load(processors_released) == 0 || online_count() < 2) return;
        if (rendezvous_depth[current_cpu()]++ != 0) return;
        store(park_requested, 1);
        for (uint32_t index = 0; index < MAX_CPUS; ++index)
            if (index != current_cpu() && load(online[index]) != 0)
                if (!send_ipi(apic_ids[index], PARK_VECTOR))
                    lib::panic("SMP: stop IPI delivery failed");
        wait_ack(parked, 1);
    }

    void resume_other_cpus()
    {
        if (load(processors_released) == 0 || online_count() < 2) return;
        if (--rendezvous_depth[current_cpu()] != 0) return;
        store(park_requested, 0);
        wait_ack(parked, 0);
    }

    Registers *handle_park_interrupt(Registers *registers)
    {
        const uint32_t index = current_cpu();
        const uint32_t directory = cpu::read_cr3();
        mem::change_pd(KERN_PAGE_DIRECTORY);
        end_interrupt();
        store(parked[index], 1);
        while (load(park_requested) != 0)
        {
            service_tlb();
            asm volatile("pause");
        }
        service_tlb();
        store(parked[index], 0);
        // Kapanan sürecin temizlenmiş sayfa dizinine geri dönülmez.
        const bool alive = thread_current_is_alive();
        if (alive) mem::change_pd(directory);
        if (alive && (registers->cs & 3U) == 0) return registers;
        kernel_enter();
        return thread_schedule(registers, false);
    }

    extern "C" [[noreturn]] void smp_ap_entry(uint32_t index)
    {
        cpu::disable_interrupt();
        descriptor_tables_initialize_ap();
        enable_local_apic(false);
        kernel_enter();
        if (!thread_attach_cpu(reinterpret_cast<uint32_t>(ap_stacks[index]) + AP_STACK_SIZE))
            for (;;) cpu::cpu_halt();
        kernel_leave();
        store(online[index], 1);
        // Açılış bitene kadar boş döngüyle ana işlemcinin kaynaklarını tüketme.
        // Kontrol ile HLT arasında kesme alınmaması, uyandırmanın kaybolmasını önler.
        while (load(processors_released) == 0)
            asm volatile("sti; hlt; cli" ::: "memory");
        write_apic(0x3E0, 3);
        write_apic(0x320, (1U << 17) | TIMER_VECTOR);
        write_apic(0x380, timer_count);
        cpu::enable_interrupt();
        thread_yield();
        for (;;) cpu::cpu_halt();
    }

    uint32_t initialize(const CpuTopology &topology, mem::Vmm &memory,
                        InitProgressCallback progress, void *context)
    {
        const uint32_t bootstrap_id = local_apic_id();
        apic_ids[0] = bootstrap_id;
        apic_to_cpu[bootstrap_id] = 0;
        store(online[0], 1);
        if (progress != nullptr)
            progress(1, topology.count == 0 ? 1 : topology.count, context);
        if (topology.count < 2 || !topology.local_apic_found) return 1;
        uint32_t a, b, c, d;
        asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
        if ((d & (1U << 9)) == 0 || (d & (1U << 5)) == 0) return 1;
        uint32_t msr_low, msr_high;
        asm volatile("rdmsr" : "=a"(msr_low), "=d"(msr_high) : "c"(0x1BU));
        if ((msr_low & (1U << 10)) != 0 || msr_high != 0) return 1;
        mem::VirtualAddress virtual_address(LAPIC_VIRTUAL_ADDRESS);
        mem::PhysicalAddress physical_address(msr_low & 0xFFFFF000U);
        if (!memory.add_page_to_pd(virtual_address, physical_address, PG_WRITE | 0x10)) return 1;
        lapic = reinterpret_cast<volatile uint32_t *>(LAPIC_VIRTUAL_ADDRESS);
        enable_local_apic(true);
        if (!calibrate_timer()) return 1;
        const uint32_t size = smp_trampoline_end - smp_trampoline_start;
        if (size > 4096) return 1;
        memcpy(reinterpret_cast<void *>(0x8000), smp_trampoline_start, size);
        uint32_t index = 1;
        for (uint32_t entry = 0; entry < topology.count && index < MAX_CPUS; ++entry)
        {
            const uint32_t id = topology.apic_ids[entry];
            if (id == bootstrap_id || id > 255) continue;
            apic_ids[index] = id;
            apic_to_cpu[id] = index;
            auto *mailbox = reinterpret_cast<volatile uint32_t *>(0x9000);
            mailbox[0] = KERN_PAGE_DIRECTORY;
            mailbox[1] = reinterpret_cast<uint32_t>(ap_stacks[index]) + AP_STACK_SIZE;
            mailbox[2] = reinterpret_cast<uint32_t>(smp_ap_entry);
            mailbox[3] = index;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            bool started = send_ipi(id, 0xC500) && pit_delay(10000) &&
                           send_ipi(id, 0x8500) && pit_delay(200) &&
                           send_ipi(id, 0x600 | 8) && pit_delay(200);
            if (started && load(online[index]) == 0)
                started = send_ipi(id, 0x600 | 8);
            for (uint32_t wait = 0; started && wait < 100 && load(online[index]) == 0; ++wait)
                if (!pit_delay(1000)) break;
            if (load(online[index]) == 0)
            {
                _kprintf("[SMP] APIC %u baslatilamadi; diger islemcilerle devam\n", id);
                // Posta kutusunu geç gelen bir işlemciyle paylaşmamak için yeni AP başlatma:D
                if (!send_ipi(id, 0xC500) || !pit_delay(10000))
                    lib::panic("SMP: failed processor could not be reset");
                store(online[index], 0);
                break;
            }
            __atomic_add_fetch(&cpu_count, 1U, __ATOMIC_RELEASE);
            _kprintf("[SMP] CPU %u hazir | APIC %u\n", index, id);
            ++index;
            if (progress != nullptr)
                progress(index, topology.count, context);
        }
        return online_count();
    }

    void release_processors()
    {
        store(processors_released, 1);
        // TLB işleyicisi kilit almadan döner; burada bekleyen işlemciyi de uyandırır.
        for (uint32_t index = 1; index < MAX_CPUS; ++index)
            if (load(online[index]) != 0)
                if (!send_ipi(apic_ids[index], TLB_VECTOR))
                    lib::panic("SMP: processor wake IPI delivery failed");
    }
}

extern "C" void smp_kernel_leave() { kernel::smp::kernel_leave(); }
extern "C" void smp_invalidate_remote_tlb() { kernel::smp::invalidate_remote_tlb(); }
