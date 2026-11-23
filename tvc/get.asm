; SPDX-License-Identifier: GPL-3.0-or-later
; GET a URL, then NEXT until the end; the log is the body, the last status goes to 6FF4h
        DEVICE ZXSPECTRUM48
LAST    equ 6FF4h
        org 5000h
start:  di
        call log_init
        ld a,10h                ; GET
        ld hl,url
        ld b,urlend-url
        call cmd
        ld (LAST),a
        or a
        jr nz,done
        ld b,0
.next:  push bc
        ld a,11h                ; NEXT
        ld b,0
        call cmd
        ld (LAST),a
        call log_data
        pop bc
        ld a,(LAST)
        cp 3
        jr z,.next              ; DATA
        cp 2
        jr nz,done              ; END or an error
        djnz .next              ; BUSY: ask again
done:   jr done

url:    db "http://127.0.0.1:8765/hello.txt"
urlend:
        include "common.inc"
        include "tvcnet.inc"
        SAVEBIN "get.bin", start, $ - start
