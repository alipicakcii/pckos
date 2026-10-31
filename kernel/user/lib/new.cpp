// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/new.hh>
#include <user/syscall.hh>

namespace
{
    void *allocate(size_t size, size_t alignment)
    {
        void *address = user::heap_alloc(size, alignment);
        if (address != nullptr) return address;
        // İstisnalar kapalıdır; normal new başarısızlığında süreç sonlandırılır.
        constexpr char message[] = "new: kullanici bellegi ayrilamadi\n";
        user::write(user::stderr_descriptor, message, sizeof(message) - 1);
        user::exit(-12);
    }
}

void *operator new(size_t size) { return allocate(size, 16); }
void *operator new[](size_t size) { return allocate(size, 16); }
void operator delete(void *address) noexcept { (void)user::heap_free(address); }
void operator delete[](void *address) noexcept { (void)user::heap_free(address); }
void operator delete(void *address, size_t) noexcept { (void)user::heap_free(address); }
void operator delete[](void *address, size_t) noexcept { (void)user::heap_free(address); }
void *operator new(size_t size, const std::nothrow_t &) noexcept
{ return user::heap_alloc(size); }
void *operator new[](size_t size, const std::nothrow_t &) noexcept
{ return user::heap_alloc(size); }
void operator delete(void *address, const std::nothrow_t &) noexcept
{ (void)user::heap_free(address); }
void operator delete[](void *address, const std::nothrow_t &) noexcept
{ (void)user::heap_free(address); }

#if defined(__cpp_aligned_new)
void *operator new(size_t size, std::align_val_t alignment)
{ return allocate(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment)
{ return allocate(size, static_cast<size_t>(alignment)); }
void operator delete(void *address, std::align_val_t) noexcept
{ (void)user::heap_free(address); }
void operator delete[](void *address, std::align_val_t) noexcept
{ (void)user::heap_free(address); }
void operator delete(void *address, size_t, std::align_val_t) noexcept
{ (void)user::heap_free(address); }
void operator delete[](void *address, size_t, std::align_val_t) noexcept
{ (void)user::heap_free(address); }
void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{ return user::heap_alloc(size, static_cast<size_t>(alignment)); }
void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{ return user::heap_alloc(size, static_cast<size_t>(alignment)); }
void operator delete(void *address, std::align_val_t, const std::nothrow_t &) noexcept
{ (void)user::heap_free(address); }
void operator delete[](void *address, std::align_val_t, const std::nothrow_t &) noexcept
{ (void)user::heap_free(address); }
#endif
