; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
	

global is_rdseed
global rand_genaration

; işlemci rnd desteği olup olmadığınu
; kontrol eden bu fonskiyon sistem cagrilarinda ABI sorunu yaşattı kontrol edilecek
; şimdilik kullanmiyıoruz
is_rdseed:
	mov eax, 7  
	mov ecx, 0    
	cpuid
	shr ebx, 18    
	and ebx, 1 
	mov eax, ebx
	ret
    
	;rastgele sayı oluştur
rand_genaration:
	mov ecx, 100
	.retry:
	rdseed eax
	jc .done    
	loop .retry
	.fail:
	; akış buraya hiç gelmemeli 
	; gelirse de rastgele sayı oluşturulamadı demek
	; bununla ilgili bir işlem yapılacak mı?
	.done:
	ret