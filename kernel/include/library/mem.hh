// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef LIB_MEM_FUNCTION_STRING_HH
#define LIB_MEM_FUNCTION_STRING_HH

#include <stddef.h>
#include <stdint.h>


#define EOF (-1)

extern "C"
{
    int memcmp(const void *, const void *, size_t);
    void *memcpy(void *, const void *, size_t);
    void *memmove(void *, const void *, size_t);
    void *memset(void *, int, size_t);
    size_t strlen(const char *);
    bool streq(const char *a, const char *b);
}
#endif
