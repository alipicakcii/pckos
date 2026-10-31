// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/lib/memory.hh>
#include <user/syscall.hh>

namespace
{
    void log(const char *text)
    {
        uint32_t length = 0;
        while (text[length]) ++length;
        user::write(1, text, length);
    }

    void check(bool value, const char *message)
    {
        if (value) return;
        log("HEAP_FAIL: "); log(message); log("\n");
        user::exit(1);
    }

#if HEAP_TEST_ROLE == 0
    uint32_t free_pages()
    {
        user::SystemInfo info{};
        check(user::get_system_info(&info) == 0, "system info");
        return info.free_pages;
    }

    struct Item
    {
        uint32_t value = 37;
        uint32_t *destructors = nullptr;
        ~Item() { if (destructors) ++*destructors; }
    };

    struct alignas(8192) Aligned { uint32_t value = 42; };
    struct Worker { volatile uint32_t done; volatile uint32_t errors; };

    void worker(void *argument)
    {
        auto *state = static_cast<Worker *>(argument);
        for (uint32_t i = 0; i < 100; ++i)
        {
            auto *data = new (std::nothrow) uint32_t[1100];
            if (!data) { ++state->errors; break; }
            data[0] = i; data[1099] = i ^ 0x1234;
            user::thread_yield();
            if (data[0] != i || data[1099] != (i ^ 0x1234)) ++state->errors;
            delete[] data;
        }
        state->done = 1;
    }

    void run_child(const char *path)
    {
        check(user::process_spawn(path, "Heap test") > 0, "spawn");
        const uint32_t deadline = static_cast<uint32_t>(user::get_ticks()) + 500;
        while (user::process_count() > 1)
        {
            check(static_cast<uint32_t>(user::get_ticks()) < deadline, "child timeout");
            user::thread_sleep(1);
        }
    }
#endif
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
#if HEAP_TEST_ROLE == 1
    auto *data = new uint8_t[3 * 1024 * 1024];
    data[0] = 0xAB; data[3 * 1024 * 1024 - 1] = 0xCD;
    check(data[0] == 0xAB, "child write");
    log("HEAP_CHILD_PASS\n");
    // Süreç çıkışında delete yapılmamış tahsislerin de geri verilmesini deniyoruz.
    user::exit(0);
#elif HEAP_TEST_ROLE == 2
    (void)::operator new(4 * 1024 * 1024);
    check(false, "normal new returned on OOM");
#else
    log("HEAP_BEGIN\n");
    unsigned char memory[16]{};
    const unsigned char source[] = {1, 2, 3, 4, 5, 0xFF};
    check(user::memory::fill(memory, 0xAA, sizeof(memory)) == memory &&
          memory[15] == 0xAA, "memset/fill");
    check(user::memory::copy(memory, source, sizeof(source)) == memory &&
          user::memory::compare(memory, source, sizeof(source)) == 0, "memcpy/compare");
    user::memory::move(memory + 1, memory, 5);
    check(memory[1] == 1 && memory[5] == 5, "overlapping memmove");
    check(user::memory::length("PCKOS") == 5 &&
          user::memory::equal("memory", "memory") &&
          !user::memory::equal("PCK", "PCKOS") &&
          strncmp("kernel", "keymap", 2) == 0, "string functions");
    char copied_text[8];
    check(strcpy(copied_text, "PCKOS") == copied_text &&
          strcmp(copied_text, "PCKOS") == 0, "strcpy/strcmp");
    char bounded_text[8];
    strncpy(bounded_text, "PCKOS", sizeof(bounded_text));
    check(bounded_text[5] == '\0' && bounded_text[7] == '\0', "strncpy padding");

    const uint32_t initial_pages = free_pages();
    int *number = new int(123);
    check(*number == 123 && (reinterpret_cast<uintptr_t>(number) & 15) == 0, "new int");
    delete number;
    uint32_t destructors = 0;
    Item *items = new Item[70];
    for (uint32_t i = 0; i < 70; ++i)
    {
        check(items[i].value == 37, "array constructor");
        items[i].destructors = &destructors;
    }
    delete[] items;
    check(destructors == 70, "array destructor");
    auto *aligned = new Aligned;
    check((reinterpret_cast<uintptr_t>(aligned) & 8191) == 0 && aligned->value == 42,
          "aligned new");
    delete aligned;
    auto *aligned_array = new (std::nothrow) Aligned[2];
    check(aligned_array && (reinterpret_cast<uintptr_t>(aligned_array) & 8191) == 0,
          "aligned array");
    delete[] aligned_array;
    alignas(Item) uint8_t storage[sizeof(Item)];
    Item *placed = new (storage) Item;
    placed->destructors = &destructors;
    placed->~Item();
    check(destructors == 71, "placement new");

    auto *a = new uint8_t[32];
    auto *b = new uint8_t[32];
    b[0] = 77;
    check(free_pages() + 1 == initial_pages, "small allocations share page");
    delete[] a;
    check(b[0] == 77, "neighbor survives free");
    auto *c = new uint8_t[32];
    check(c == a, "reuse hole");
    check(user::heap_free(c + 1) < 0, "reject interior pointer");
    delete[] c;
    check(user::heap_free(c) < 0, "reject double free");
    delete[] b;
    check(user::heap_free(nullptr) == 0, "free null");
    check(user::heap_free(reinterpret_cast<void *>(0x100000)) < 0, "reject kernel address");
    check(user::heap_alloc(16, 3) == nullptr, "reject alignment");
    check(new (std::nothrow) uint8_t[4 * 1024 * 1024] == nullptr, "nothrow OOM");
    check(user::heap_alloc(0xFFFFFFFF) == nullptr, "overflow");
    void *zero = ::operator new(0);
    check(zero != nullptr, "zero size");
    ::operator delete(zero);

    void *slots[1024];
    for (uint32_t i = 0; i < 1024; ++i)
    {
        slots[i] = user::heap_alloc(1);
        check(slots[i] != nullptr, "allocation slots");
    }
    check(user::heap_alloc(1) == nullptr, "slot limit");
    for (uint32_t i = 0; i < 1024; ++i) check(user::heap_free(slots[i]) == 0, "free slots");
    auto *large = new uint8_t[3 * 1024 * 1024];
    for (uint32_t i = 0; i < 3 * 1024 * 1024; i += 4096) large[i] = 91;
    check(large[3 * 1024 * 1024 - 4096] == 91, "large allocation");
    delete[] large;
    check(free_pages() == initial_pages, "physical pages reclaimed");
    log("HEAP_ALLOC_PASS\n");

    Worker workers[3]{};
    for (auto &state : workers)
        check(user::thread_create(worker, &state, "Heap worker") >= 0, "thread create");
    const uint32_t deadline = static_cast<uint32_t>(user::get_ticks()) + 500;
    for (;;)
    {
        bool done = true;
        for (const auto &state : workers) done = done && state.done;
        if (done) break;
        check(static_cast<uint32_t>(user::get_ticks()) < deadline, "thread timeout");
        user::thread_sleep(1);
    }
    for (const auto &state : workers) check(state.errors == 0, "thread heap corruption");
    user::thread_sleep(3);
    log("HEAP_THREADS_PASS\n");

    auto *parent_data = new uint32_t(0x12345678);
    // İlk süreç açılışı zamanlayıcı için ek çekirdek belleği ayırabilir.
    run_child("/apps/heap_child.bin");
    const uint32_t baseline = free_pages();
    for (uint32_t i = 0; i < 8; ++i)
    {
        run_child("/apps/heap_child.bin");
        check(*parent_data == 0x12345678, "process isolation");
        check(free_pages() == baseline, "process heap leak");
    }
    delete parent_data;
    run_child("/apps/heap_oom.bin");
    run_child("/apps/new_demo.bin");
    log("HEAP_ALL_PASS\n");
#endif
    user::exit(0);
}
