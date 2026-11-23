; SPDX-License-Identifier: GPL-3.0-or-later
; What the test programs share: a log from 7000h, its end at 6FF0h, failed requests at 6FF2h; the program
; ends in the loop at `done`.
LOG     equ 7000h
LOGEND  equ 6FF0h
FAILS   equ 6FF2h

log_init:
        ld hl,LOG
        ld (LOGEND),hl
        xor a
        ld (FAILS),a
        ret

; tn_cmd, counting a failure
cmd:    call tn_cmd
        ret nc
        push af
        ld a,(FAILS)
        inc a
        ld (FAILS),a
        pop af
        ret

; Appends the reply's status byte (A) to the log
log_status:
        ld hl,(LOGEND)
        ld (hl),a
        inc hl
        ld (LOGEND),hl
        ret

; Appends the reply's data to the log
log_data:
        ld a,(tn_reply)
        or a
        ret z
        ld c,a
        ld b,0
        ld hl,tn_reply+2
        ld de,(LOGEND)
        ldir
        ld (LOGEND),de
        ret

; About 1.6 s without traffic
wait:   ld d,3
.o:     ld bc,0
.i:     dec bc
        ld a,b
        or c
        jr nz,.i
        dec d
        jr nz,.o
        ret
