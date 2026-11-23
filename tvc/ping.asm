; SPDX-License-Identifier: GPL-3.0-or-later
; PING and ECHO; the log: each reply's status, length and data
        DEVICE ZXSPECTRUM48
        org 5000h
start:  di
        call log_init
        ld a,01h                ; PING
        ld b,0
        call one
        ld a,02h                ; ECHO
        ld hl,text
        ld b,textend-text
        call one
done:   jr done

one:    call cmd
        call log_status
        ld a,(tn_reply)
        call log_status
        jp log_data

text:   db "HELLO",0,0FFh,6Ah
textend:
        include "common.inc"
        include "tvcnet.inc"
        SAVEBIN "ping.bin", start, $ - start
