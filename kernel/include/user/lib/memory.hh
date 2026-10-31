// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_MEMORY_HH
#define PCKOS_USER_MEMORY_HH

#include <stddef.h>

extern "C"
{
    void *memcpy(void *destination, const void *source, size_t size);
    void *memmove(void *destination, const void *source, size_t size);
    void *memset(void *destination, int value, size_t size);
    int memcmp(const void *left, const void *right, size_t size);
    size_t strlen(const char *text);
    int strcmp(const char *left, const char *right);
    int strncmp(const char *left, const char *right, size_t size);
    char *strcpy(char *destination, const char *source);
    char *strncpy(char *destination, const char *source, size_t size);
}

namespace user::memory
{
    inline void *copy(void *destination, const void *source, size_t size)
    { return ::memcpy(destination, source, size); }

    inline void *move(void *destination, const void *source, size_t size)
    { return ::memmove(destination, source, size); }

    inline void *fill(void *destination, int value, size_t size)
    { return ::memset(destination, value, size); }

    inline int compare(const void *left, const void *right, size_t size)
    { return ::memcmp(left, right, size); }

    inline size_t length(const char *text)
    { return ::strlen(text); }

    inline bool equal(const char *left, const char *right)
    { return ::strcmp(left, right) == 0; }
}

#endif
