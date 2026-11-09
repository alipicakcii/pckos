; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32
section .text

global is_rdseed
global rdseed_generate

; CPUID, C++ çağrı sözleşmesinde korunması gereken EBX'i değiştirir.
is_rdseed:
    push ebx
    xor eax, eax
    cpuid
    cmp eax, 7
    jb .unsupported
    mov eax, 7
    xor ecx, ecx
    cpuid
    mov eax, ebx
    shr eax, 18
    and eax, 1
    pop ebx
    ret
.unsupported:
    xor eax, eax
    pop ebx
    ret

; Destek kontrolünden sonra çağrılır. Başarıda *result yazılır ve true döner.
; Sıfır da geçerli bir donanım çıktısıdır; başarı bilgisi ayrı döndürülür.
rdseed_generate:
    mov edx, [esp + 4]
    test edx, edx
    jz .failed
    mov ecx, 100
.retry:
    rdseed eax
    jc .success
    pause
    loop .retry
.failed:
    xor eax, eax
    ret
.success:
    mov [edx], eax
    mov eax, 1
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
