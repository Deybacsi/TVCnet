; SPDX-License-Identifier: GPL-3.0-or-later
; TVCnet from the TVC side (PROTOKOLL.md), for sjasmplus: TVCnet in CH12 (05h b7), 2400 baud, T = 250 us.
; Interrupts must be off while tn_cmd runs (the timing is counted in loops at 3.125 MHz).
;   tn_cmd: A = command code, HL = data, B = its length (0-255)
;           out: A = status, CY = no good reply after 8 tries; the reply in tn_reply (LEN, CTRL, data, CHK)

TN_PORT05 equ 0B12h             ; the ROM's copy of port 05h
TN_THRESH equ 26                ; halfbit loops: fewer is T (1), more is 2T (0)
TN_BITN   equ 77                ; bitdelay loops for one 2400-baud bit

tn_cmd:
        ld (tn_code),a
        ld (tn_data),hl
        ld a,b
        ld (tn_len),a
        ld a,8
        ld (tn_tries),a
.again: ld a,(tn_bit)
        ld c,a
        ld a,(tn_code)
        or c
        ld (tn_ctrl),a
        call tn_send
        ld hl,tn_reply
        call tn_recv
        ld a,(tn_timeout)
        or a
        jr nz,.retry
        ld hl,tn_ctrl
        ld a,(tn_reply+1)
        xor (hl)
        and 80h
        jr nz,.retry            ; the answer to another request
        ld a,(tn_bit)
        xor 80h
        ld (tn_bit),a
        ld a,(tn_reply+1)
        and 7Fh
        ret                     ; and clears CY
.retry: ld hl,tn_tries
        dec (hl)
        jr nz,.again            ; same bit: TVCnet repeats its reply
        scf
        ret

; The request in tn_ctrl, tn_len, tn_data
tn_send:
        ld a,(tn_len)
        ld b,a
        ld c,a
        ld a,(tn_ctrl)
        add a,c
        ld c,a                  ; checksum: LEN + CTRL + data
        ld hl,(tn_data)
        ld a,b
        or a
        jr z,.sum
.s:     ld a,(hl)
        add a,c
        ld c,a
        inc hl
        djnz .s
.sum:   ld a,c
        ld (tn_chk),a
        ld a,0A5h
        call tn_byte
        ld a,(tn_len)
        call tn_byte
        ld a,(tn_ctrl)
        call tn_byte
        ld a,(tn_len)
        or a
        jr z,.chk
        ld b,a
        ld hl,(tn_data)
.d:     ld a,(hl)
        inc hl
        push bc
        push hl
        call tn_byte
        pop hl
        pop bc
        djnz .d
.chk:   ld a,(tn_chk)
        jp tn_byte

; One 8N1 character on the motor line: a set bit pulls it low, so a start bit is 1 and a stop bit 0
tn_byte:
        ld c,a
        ld a,(TN_PORT05)
        and 3Fh
        ld d,a                  ; line high
        or 80h
        ld e,a                  ; line low
        ld a,e
        out (05h),a             ; start bit
        call .delay
        ld b,8
.bit:   rr c
        ld a,d
        jr c,.one
        ld a,e
.one:   out (05h),a
        call .delay
        djnz .bit
        ld a,d
        out (05h),a             ; stop bit
.delay: ld h,TN_BITN
.l:     dec h
        jr nz,.l
        ret

; A reply frame from 59h b5 to HL (LEN, CTRL, data, CHK); tn_timeout: 0 good, 1 none, 2 lost edge, 3 checksum
tn_recv:
        xor a
        ld (tn_timeout),a
        in a,(59h)
        and 20h
        ld b,a
        ld de,0
.w0:    in a,(59h)
        and 20h
        cp b
        jr nz,.first
        dec de
        ld a,d
        or e
        jr nz,.w0
        ld a,1
        ld (tn_timeout),a
        ret
.first: ld b,a
        ld c,0
.sync:  call tn_half
        rr c
        ld a,(tn_timeout)
        or a
        ret nz
        ld a,c
        cp 6Ah
        jr nz,.sync
        call tn_get             ; LEN
        ld (hl),a
        inc hl
        ld (tn_sum),a
        push af
        call tn_get             ; CTRL
        ld (hl),a
        inc hl
        call tn_add
        pop af
        or a
        jr z,.chk
        ld d,a
.data:  push de
        call tn_get
        pop de
        ld (hl),a
        inc hl
        call tn_add
        dec d
        jr nz,.data
.chk:   call tn_get             ; CHK
        ld (hl),a
        ld e,a
        ld a,(tn_timeout)
        or a
        ret nz
        ld a,(tn_sum)
        cp e
        ret z
        ld a,3
        ld (tn_timeout),a
        ret

tn_add: push bc
        ld c,a
        ld a,(tn_sum)
        add a,c
        ld (tn_sum),a
        pop bc
        ret

tn_get: ld e,8
.b:     push de
        call tn_half
        pop de
        rr c
        dec e
        jr nz,.b
        ld a,c
        ret

; The time to the next edge: CY = 1 for a short half-period (a 1 bit)
tn_half:
        ld e,0
.w:     inc e
        jr z,.to
        in a,(59h)
        and 20h
        cp b
        jr z,.w
        ld b,a
        ld a,e
        cp TN_THRESH
        ret
.to:    ld a,2
        ld (tn_timeout),a
        or a
        ret

tn_bit:     db 0                ; the alternating bit of the next request
tn_code:    db 0
tn_ctrl:    db 0
tn_len:     db 0
tn_chk:     db 0
tn_data:    dw 0
tn_tries:   db 0
tn_timeout: db 0
tn_sum:     db 0
tn_reply:   ds 260
