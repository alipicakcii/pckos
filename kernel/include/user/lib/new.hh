// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_NEW_HH
#define PCKOS_USER_NEW_HH

#include <stddef.h>

// Kullanıcı SDK'sının minimal new arayüzü; tam standart kütüphane değildir.
namespace std
{
    struct nothrow_t { explicit constexpr nothrow_t() = default; };
    constexpr nothrow_t nothrow{};
#if defined(__cpp_aligned_new)
    enum class align_val_t : size_t {};
#endif
}

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *address) noexcept;
void operator delete[](void *address) noexcept;
void operator delete(void *address, size_t size) noexcept;
void operator delete[](void *address, size_t size) noexcept;
void *operator new(size_t size, const std::nothrow_t &) noexcept;
void *operator new[](size_t size, const std::nothrow_t &) noexcept;
void operator delete(void *address, const std::nothrow_t &) noexcept;
void operator delete[](void *address, const std::nothrow_t &) noexcept;

inline void *operator new(size_t, void *address) noexcept { return address; }
inline void *operator new[](size_t, void *address) noexcept { return address; }
inline void operator delete(void *, void *) noexcept {}
inline void operator delete[](void *, void *) noexcept {}

#if defined(__cpp_aligned_new)
void *operator new(size_t size, std::align_val_t alignment);
void *operator new[](size_t size, std::align_val_t alignment);
void operator delete(void *address, std::align_val_t) noexcept;
void operator delete[](void *address, std::align_val_t) noexcept;
void operator delete(void *address, size_t, std::align_val_t) noexcept;
void operator delete[](void *address, size_t, std::align_val_t) noexcept;
void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept;
void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept;
void operator delete(void *address, std::align_val_t, const std::nothrow_t &) noexcept;
void operator delete[](void *address, std::align_val_t, const std::nothrow_t &) noexcept;
#endif

#endif
