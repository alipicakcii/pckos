// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_IO_HH
#define PCKOS_USER_IO_HH

#include <abi/io.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::stdin_descriptor;
    using abi::stdout_descriptor;
    using abi::stderr_descriptor;

int32_t write(int descriptor, const void *buffer, uint32_t length);

int32_t read(int descriptor, void *buffer, uint32_t length);

int32_t terminal_attach();

int32_t terminal_read(void *buffer, uint32_t length);

    // Yalnızca bu terminale bağlı uygulamayı ve alt süreçlerini sonlandırır.
int32_t terminal_interrupt(uint32_t process_id);

int32_t terminal_send(uint32_t process_id, const void *buffer, uint32_t length);
}

#endif
