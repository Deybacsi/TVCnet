; SPDX-License-Identifier: GPL-3.0-or-later
; OPEN to an echo server, SEND, NEXT until the bytes are back, CLOSE, NEXT
; The log: the statuses of OPEN, SEND, CLOSE and the last NEXT, then the bytes that came back
        DEVICE ZXSPECTRUM48
STATS   equ 6FF8h
        org 5000h
start:  di
        call log_init
        ld a,01h                ; PING
        ld b,0
        call cmd
        ld a,20h                ; OPEN
        ld hl,addr
        ld b,addrend-addr
        call cmd
        ld (STATS),a
        ld a,21h                ; SEND, also while still connecting
        ld hl,data
        ld b,dataend-data
        call cmd
        ld (STATS+1),a
        ld b,0
.next:  push bc
        ld a,11h                ; NEXT
        ld b,0
        call cmd
        call log_data
        pop bc
        ld hl,(LOGEND)
        ld de,LOG+dataend-data
        or a
        sbc hl,de
        jr nc,.close            ; all of it is back
        djnz .next
.close: ld a,12h                ; CLOSE
        ld b,0
        call cmd
        ld (STATS+2),a
        ld a,11h
        ld b,0
        call cmd
        ld (STATS+3),a          ; NONE: the job is gone
done:   jr done

addr:   db "127.0.0.1:8766"
addrend:
data:   db "TCP test",0,1,2,0FFh,0FEh,6Ah,0A5h,13,10
dataend:
        include "common.inc"
        include "tvcnet.inc"
        SAVEBIN "tcp.bin", start, $ - start
