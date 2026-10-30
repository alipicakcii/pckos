; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
section .text

global _enable_paging
global change_pd
global invalidate

; Bu fonsksiyon işlemcinin sayfalama unitesini etkinleştirir
; 4KB sayfa desteğini etkinleştiri
; 4M sayfa desteğini etkimşeitir. Pekiyi işletim sistemi 4M sayfamı olarak çalışacak? 
; Hayır sadece belleğin tepesi (0x0...) yani çekirdek kodları 4M kolaylık açısından
_enable_paging:
        push   ebp
        mov    ebp,esp
        mov    eax, [esp + 8]
        mov    cr3,eax
      	mov    eax,cr4
       	or     eax,0x10
      	mov    cr4,eax
        mov    eax,cr0
        or     eax,0x80010000 ; paging + supervisor write protection
       	mov    cr0,eax
        nop
        pop    ebp
        ret

change_pd:
	push ebp
	mov ebp, esp
	mov eax, [esp+8]
	mov cr3, eax	
	pop ebp
	ret

invalidate:
	push ebp
	mov ebp, esp
	mov eax, [esp+8]
	invlpg [eax] 
	pop ebp
	ret

