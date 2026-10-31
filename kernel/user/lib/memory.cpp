// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/lib/memory.hh>
#include <stdint.h>

extern "C"
{
    void *memcpy(void *destination, const void *source, size_t size)
    {
        auto *output = static_cast<unsigned char *>(destination);
        const auto *input = static_cast<const unsigned char *>(source);
        for (size_t index = 0; index < size; ++index)
            output[index] = input[index];
        return destination;
    }

    void *memmove(void *destination, const void *source, size_t size)
    {
        auto *output = static_cast<unsigned char *>(destination);
        const auto *input = static_cast<const unsigned char *>(source);
        if (reinterpret_cast<uintptr_t>(output) < reinterpret_cast<uintptr_t>(input))
        {
            for (size_t index = 0; index < size; ++index)
                output[index] = input[index];
        }
        else if (output != input)
        {
            for (size_t index = size; index > 0; --index)
                output[index - 1] = input[index - 1];
        }
        return destination;
    }

    void *memset(void *destination, int value, size_t size)
    {
        auto *output = static_cast<unsigned char *>(destination);
        for (size_t index = 0; index < size; ++index)
            output[index] = static_cast<unsigned char>(value);
        return destination;
    }

    int memcmp(const void *left, const void *right, size_t size)
    {
        const auto *first = static_cast<const unsigned char *>(left);
        const auto *second = static_cast<const unsigned char *>(right);
        for (size_t index = 0; index < size; ++index)
        {
            if (first[index] != second[index])
                return first[index] < second[index] ? -1 : 1;
        }
        return 0;
    }

    size_t strlen(const char *text)
    {
        size_t length = 0;
        while (text[length] != '\0') ++length;
        return length;
    }

    int strcmp(const char *left, const char *right)
    {
        while (*left != '\0' && *left == *right)
        {
            ++left;
            ++right;
        }
        const unsigned char first = static_cast<unsigned char>(*left);
        const unsigned char second = static_cast<unsigned char>(*right);
        return first == second ? 0 : (first < second ? -1 : 1);
    }

    int strncmp(const char *left, const char *right, size_t size)
    {
        for (size_t index = 0; index < size; ++index)
        {
            const unsigned char first = static_cast<unsigned char>(left[index]);
            const unsigned char second = static_cast<unsigned char>(right[index]);
            if (first != second) return first < second ? -1 : 1;
            if (first == '\0') return 0;
        }
        return 0;
    }

    char *strcpy(char *destination, const char *source)
    {
        char *result = destination;
        do { *destination++ = *source; }
        while (*source++ != '\0');
        return result;
    }

    char *strncpy(char *destination, const char *source, size_t size)
    {
        char *result = destination;
        size_t index = 0;
        while (index < size && source[index] != '\0')
        {
            destination[index] = source[index];
            ++index;
        }
        while (index < size) destination[index++] = '\0';
        return result;
    }
}
