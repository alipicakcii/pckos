; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
section .text

global _enable_paging
global change_pd
global invalidate

; Bu fonsksiyon işlemcinin sayfalama unitesini etkinleştirir
; çekirdeğin ilk 16MB kısmı 4M sayfalama olarak eşlenmiştir
; geri kalan kısım 4KB dir kullanıcı alanıda dahil
; 4M eşleşme ilk 16MB için sadece kolaylık içindir
_enable_paging:
        push   ebp
        mov    ebp,esp
        mov    eax, [esp + 8]
        mov    cr3,eax
      	mov    eax,cr4
       	or     eax,0x10
      	mov    cr4,eax
        mov    eax,cr0
        or     eax,0x80010000 ; sayfalama + denetleyici yazma koruması
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

