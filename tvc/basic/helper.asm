; SPDX-License-Identifier: GPL-3.0-or-later
; TVCnet for TVC BASIC: Z=USR(6656) sends the request at RQ (command, length, data) and returns its status,
; or -1 without a good reply; the reply (LEN, CTRL, data, CHK) is at tn_reply. make_bas.py turns it into DATA lines.
        DEVICE ZXSPECTRUM48
        org 1A00h
start:  di
        ld a,(RQ+1)
        ld b,a
        ld hl,RQ+2
        ld a,(RQ)
        call tn_cmd
        ei
        ld h,0
        ld l,a
        ret nc
        ld hl,0FFFFh
        ret

        include "../tvcnet.inc"
RQ      equ tn_reply+260
        SAVEBIN "helper.bin", start, tn_reply - start
