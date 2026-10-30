// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/mem.hh>
#include <library/kprint.hh>
#include <library/convert.hh>

extern "C"
{


bool streq(const char *a, const char *b) 
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

       


void *memcpy(void *dstptr, const void *srcptr, size_t size)
{
        unsigned char *dst = (unsigned char *)dstptr;
        const unsigned char *src = (const unsigned char *)srcptr;
        for (size_t i = 0; i < size; i++)
                dst[i] = src[i];
        return dstptr;
}

void *memmove(void *dstptr, const void *srcptr, size_t size)
{
        unsigned char *dst = (unsigned char *)dstptr;
        const unsigned char *src = (const unsigned char *)srcptr;
        if (dst < src)
        {
                for (size_t i = 0; i < size; i++)
                        dst[i] = src[i];
        }
        else
        {
                for (size_t i = size; i != 0; i--)
                        dst[i - 1] = src[i - 1];
        }
        return dstptr;
}

void *memset(void *bufptr, int value, size_t size)
{
        unsigned char *buf = (unsigned char *)bufptr;
        for (size_t i = 0; i < size; i++)
                buf[i] = (unsigned char)value;
        return bufptr;
}

size_t strlen(const char *str)
{
        size_t len = 0;
        while (str[len])
                len++;
        return len;
}
}
