; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK

global enable_interrupt
global disable_interrupt
global cpu_halt
extern handle_isr
extern handle_irq
extern handle_timer_irq

enable_interrupt:
    sti
    ret
disable_interrupt:
    cli
    ret
cpu_halt:
    hlt
    ret

; NASM makrosu
%macro ISR_ERROR_CODE 1
  global isr%1
  isr%1:
    push dword %1         ; kesme numarasını itin
    jmp handle_isr_common
%endmacro

; NASM makrosu
%macro ISR_NO_ERROR_CODE 1
  global isr%1
  isr%1:
    push dword 0          ; yapı kayıtlarını yukarıdaki makroyla aynı tutmak için sahte bir hata kodu girin
    push dword %1         ; kesme numarasını itin
    jmp handle_isr_common
%endmacro


; NASM makrosu
%macro IRQ 2
  global irq%1
  irq%1:
    push dword 0          ; yapı kayıtlarını tekrar aynı tutmak için sahte bir hata kodu itin
    push dword %2
    jmp handle_irq_common
%endmacro
        
ISR_NO_ERROR_CODE  0
ISR_NO_ERROR_CODE  1
ISR_NO_ERROR_CODE  2
ISR_NO_ERROR_CODE  3
ISR_NO_ERROR_CODE  4
ISR_NO_ERROR_CODE  5
ISR_NO_ERROR_CODE  6
ISR_NO_ERROR_CODE  7
ISR_ERROR_CODE     8
ISR_NO_ERROR_CODE  9
ISR_ERROR_CODE     10
ISR_ERROR_CODE     11
ISR_ERROR_CODE     12
ISR_ERROR_CODE     13
ISR_ERROR_CODE     14
ISR_NO_ERROR_CODE  15
ISR_NO_ERROR_CODE  16
ISR_NO_ERROR_CODE  17
ISR_NO_ERROR_CODE  18
ISR_NO_ERROR_CODE  19
ISR_NO_ERROR_CODE  20
ISR_NO_ERROR_CODE  21
ISR_NO_ERROR_CODE  22
ISR_NO_ERROR_CODE  23
ISR_NO_ERROR_CODE  24
ISR_NO_ERROR_CODE  25
ISR_NO_ERROR_CODE  26
ISR_NO_ERROR_CODE  27
ISR_NO_ERROR_CODE  28
ISR_NO_ERROR_CODE  29
ISR_NO_ERROR_CODE  30
ISR_NO_ERROR_CODE  31
ISR_NO_ERROR_CODE  128
ISR_NO_ERROR_CODE  129

IRQ   0,    32
IRQ   1,    33
IRQ   2,    34
IRQ   3,    35
IRQ   4,    36
IRQ   5,    37
IRQ   6,    38
IRQ   7,    39
IRQ   8,    40
IRQ   9,    41
IRQ  10,    42
IRQ  11,    43
IRQ  12,    44
IRQ  13,    45
IRQ  14,    46
IRQ  15,    47

%macro	SAVE_REGS 0
        pushad
        push ds           ;bu kayıtlar 16 bittir ancak burada 32 bit olarak itilirler
        push es
        push fs
        push gs

        push ebx
        mov bx, 0x10      ; çekirdek veri segmenti tanımlayıcısını yükleyin
        mov ds, bx
        mov es, bx
        mov fs, bx
        mov gs, bx
        pop ebx
%endmacro

%macro	RESTORE_REGS 0
        pop gs
        pop fs
        pop es
        pop ds
        popad
%endmacro




handle_isr_common:
    SAVE_REGS
    mov eax, esp
    push eax
    call handle_isr
    add esp, 4
    mov esp, eax
    RESTORE_REGS
    add esp, 8
    iret


handle_irq_common:
    SAVE_REGS
    mov eax, esp
    push eax
    call handle_irq
    add esp, 4
    mov esp, eax
    RESTORE_REGS
    add esp, 8
    iret
