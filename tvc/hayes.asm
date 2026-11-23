; SPDX-License-Identifier: GPL-3.0-or-later
; A Hayes session through TERM, run from a script; the log is everything TVCnet printed
; Ops: S len text (send), P n (n empty polls), U st (poll until that status), L n (poll until the log has n bytes
; more than before the last S),
; W (about 1.6 s of silence), E (end)
        DEVICE ZXSPECTRUM48
        org 5000h
start:  di
        call log_init
        ld a,01h                ; PING first
        ld b,0
        call cmd
        ld hl,script
step:   ld a,(hl)
        inc hl
        cp 'S'
        jr z,op_s
        cp 'P'
        jr z,op_p
        cp 'U'
        jr z,op_u
        cp 'L'
        jr z,op_l
        cp 'W'
        jr z,op_w
done:   jr done

op_s:   ld b,(hl)
        inc hl
        push hl
        ld e,b
        ld d,0
        add hl,de
        ex (sp),hl              ; the next op on the stack, the text in HL
        ld de,(LOGEND)
        ld (mark),de
        call term
        pop hl
        jp step

op_p:   ld b,(hl)
        inc hl
.p:     push bc
        push hl
        call poll
        pop hl
        pop bc
        djnz .p
        jp step

op_u:   ld a,(hl)
        inc hl
        ld (want),a
        ld b,250
.u:     push bc
        push hl
        call poll
        pop hl
        pop bc
        ld a,(status)
        ld c,a
        ld a,(want)
        cp c
        jp z,step
        djnz .u
        jp step

op_l:   ld e,(hl)
        inc hl
        push hl
        ld hl,(mark)
        ld d,0
        add hl,de
        ld (target),hl
        ld b,250
.l:     push bc
        call poll
        pop bc
        ld hl,(LOGEND)
        ld de,(target)
        or a
        sbc hl,de
        jr nc,.got
        djnz .l
.got:   pop hl
        jp step

op_w:   push hl
        call wait
        pop hl
        jp step

poll:   ld b,0
; TERM with HL, B bytes; the reply's data goes to the log
term:   ld a,30h
        call cmd
        ld (status),a
        jp log_data

want:   db 0
status: db 0
target: dw 0
mark:   dw 0

        MACRO SEND text
        db 'S', .e - .t
.t:     db text
.e:
        ENDM

script:
        SEND "ATE0\r"
        SEND "ATI4\r"
        SEND "ATDT\"127.0.0.1:8766\"\r"
        db 'U', 03h             ; online: CONNECT
        SEND "hello, TVCnet\r"
        db 'L', 14              ; the echo server sends it back
        db 'W'
        SEND "+++"
        db 'W'
        db 'P', 1               ; OK: command mode, still connected
        SEND "ATO\r"            ; CONNECT
        SEND "again\r"
        db 'L', 6
        db 'W'
        SEND "+++"
        db 'W'
        db 'P', 1
        SEND "ATH\r"
        SEND "ATO\r"            ; NO CARRIER: hung up
        SEND "AT&G\"http://127.0.0.1:8765/hello.txt\"\r"
        db 'U', 00h             ; the body, then OK
        SEND "ATD\"127.0.0.1:1\"\r"
        db 'U', 00h             ; refused: NO CARRIER
        SEND "ATV0\r"
        SEND "ATX\r"            ; 4: ERROR
        db 'E'

        include "common.inc"
        include "tvcnet.inc"
        SAVEBIN "hayes.bin", start, $ - start
