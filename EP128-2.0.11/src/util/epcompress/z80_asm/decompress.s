
; border effects are disabled if this is set to any non-zero value
NO_BORDER_FX            equ     0

decompressData:
        push  hl
        exx
        pop   hl                        ; HL' = compressed data read address
        dec   l
        exx
        ld    hl, 00000h
        add   hl, sp
        ld    ixl, decodeTableEnd + 6
        ld    sp, ix
        in    a, (0b2h)                 ; save memory paging,
        push  af
        in    a, (0b1h)
        push  af
        push  hl                        ; and stack pointer
        ld    sp, hl
        call  allocateSegment           ; get first output segment
        dec   de
        exx
        ld    e, 080h                   ; initialize shift register
        exx
        call  read8Bits                 ; skip checksum byte
ldd_01: call  decompressDataBlock       ; decompress all blocks
        jr    z, ldd_01
        inc   de
        defb  0feh                      ; = CP nn

memoryError:
        xor   a

decompressDone:
        ld    c, a                      ; save error flag: 0: error, 1: success
        ld    ixl, decodeTableEnd
        ld    sp, ix
        pop   hl                        ; restore stack pointer,
        pop   af                        ; memory paging,
        out   (0b1h), a
        pop   af
        out   (0b2h), a
    if NO_BORDER_FX == 0
        xor   a                         ; and border color
        out   (081h), a
    endif
        ld    sp, hl
        exx
        inc   l
        push  hl
        exx
        pop   hl
        ld    a, c                      ; on success: return A=0, Z=1, C=0
        sub   1                         ; on error: return A=0FFh, Z=0, C=1
        ret

writeBlock:
        inc   d
        bit   6, d
        ret   z
        inc   iy

allocateSegment:
        set   7, d                      ; write decompressed data to page 2
        res   6, d
        push  af
        push  bc
        push  de
        ld    a, (iy)
        cp    1
        jr    z, memoryError
        jr    nc, las_01                ; use pre-allocated segment ?
        rst   030h                      ; no, allocate new segment
        defb  24
        cp    0f5h                      ; NOTE: this ignores shared segments
        jr    z, memoryError
        ld    a, c
        ld    (iy), a                   ; save segment number
las_01: out   (0b2h), a
        pop   de
        pop   bc
        pop   af
        ret

; -----------------------------------------------------------------------------

readBlock:
        push  af
        push  bc
        push  de

        pop   de
        pop   bc
        pop   af
        ret

; -----------------------------------------------------------------------------

; BC': symbols (literal byte or match code) remaining
; D':  prefix size for LZ77 matches with length >= 3 bytes
; E':  shift register
; HL': compressed data read address
; A:   temp. register
; BC:  temp. register (number of literal/LZ77 bytes to copy)
; DE:  decompressed data write address
; HL:  temp. register (literal/LZ77 data source address)
; IXH: decode table upper byte
; IY:  segment table pointer

nLengthSlots            equ 8
nOffs1Slots             equ 4
nOffs2Slots             equ 8
maxOffs3Slots           equ 32
totalSlots              equ nLengthSlots+nOffs1Slots+nOffs2Slots+maxOffs3Slots
; NOTE: the upper byte of the address of all table elements must be the same
slotBitsTable           equ 00000h
;slotBaseLowTable       equ slotBitsTable + totalSlots
;slotBaseHighTable      equ slotBaseLowTable + totalSlots
slotBitsTableL          equ slotBitsTable
;slotBaseLowTableL      equ slotBaseLowTable
;slotBaseHighTableL     equ slotBaseHighTable
slotBitsTableO1         equ slotBitsTableL + nLengthSlots
;slotBaseLowTableO1     equ slotBaseLowTableL + nLengthSlots
;slotBaseHighTableO1    equ slotBaseHighTableL + nLengthSlots
slotBitsTableO2         equ slotBitsTableO1 + nOffs1Slots
;slotBaseLowTableO2     equ slotBaseLowTableO1 + nOffs1Slots
;slotBaseHighTableO2    equ slotBaseHighTableO1 + nOffs1Slots
slotBitsTableO3         equ slotBitsTableO2 + nOffs2Slots
;slotBaseLowTableO3     equ slotBaseLowTableO2 + nOffs2Slots
;slotBaseHighTableO3    equ slotBaseHighTableO2 + nOffs2Slots
decodeTableEnd          equ slotBitsTable + (totalSlots * 3)

decompressDataBlock:
        call  read8Bits                 ; read number of symbols - 1 (BC)
        ld    c, a                      ; NOTE: MSB is in C, and LSB is in B
        call  read8Bits
        ld    b, a
        inc   b
        inc   c
        ld    a, 040h
        call  readBits                  ; read flag bits
        srl   a
        push  af                        ; save last block flag (A=1,Z=0)
        jr    c, lddb01                 ; is compression enabled ?
        exx                             ; no, copy uncompressed literal data
        ld    bc, 00101h
        jr    lddb12
lddb01: push  bc                        ; compression enabled:
        ld    a, 040h
        call  readBits                  ; get prefix size for length >= 3 bytes
        exx
        ld    b, a
        inc   b
        ld    a, 002h
        ld    d, 080h
lddb02: add   a, a
        srl   d                         ; D' = prefix size code for readBits
        djnz  lddb02
        pop   bc                        ; store the number of symbols in BC'
        exx
        add   a, nLengthSlots + nOffs1Slots + nOffs2Slots - 3
        ld    c, a                      ; store total table size - 3 in C
        push  de                        ; save decompressed data write address
        ld    ixl, low slotBitsTable    ; initialize decode tables
lddb03: sbc   hl, hl                    ; set initial base value (carry is 0)
lddb04: ld    (ix + totalSlots), l        ; store base value LSB
        ld    (ix + (totalSlots * 2)), h  ; store base value MSB
        ld    a, 010h
        call  readBits
        ld    (ix), a                   ; store the number of bits to read
        ex    de, hl
        ld    hl, 00001h                ; calculate 1 << nBits
        jr    z, lddb06
        ld    b, a
lddb05: add   hl, hl
        djnz  lddb05
lddb06: add   hl, de                    ; calculate new base value
        inc   ixl
        ld    a, ixl
        cp    low slotBitsTableO1
        jr    z, lddb03                 ; end of length decode table ?
        cp    low slotBitsTableO2
        jr    z, lddb03                 ; end of offset table for length=1 ?
        cp    low slotBitsTableO3
        jr    z, lddb03                 ; end of offset table for length=2 ?
        dec   c
        jr    nz, lddb04                ; continue until all tables are read
        pop   de                        ; DE = decompressed data write address
        exx
lddb07: sla   e
        jp    nz, lddb08
        inc   l
        call  z, readBlock
        ld    e, (hl)
        rl    e
lddb08: jr    nc, lddb13                ; literal byte ?
        ld    a, 0f8h
lddb09: sla   e
        jp    nz, lddb10
        inc   l
        call  z, readBlock
        ld    e, (hl)
        rl    e
lddb10: jr    nc, copyLZMatch           ; LZ77 match ?
        inc   a
        jr    nz, lddb09
        exx
        ld    c, a
        call  read8Bits                 ; get literal sequence length - 17
        add   a, 16
        ld    b, a
        rl    c
        inc   b                         ; B: (length - 1) LSB + 1
        inc   c                         ; C: (length - 1) MSB + 1
lddb11: exx                             ; copy literal sequence
lddb12: inc   l
        call  z, readBlock
        ld    a, (hl)
        exx
        inc   e
        call  z, writeBlock
        ld    (de), a
        djnz  lddb11
        dec   c
        jr    nz, lddb11
        jr    lddb14
lddb13: inc   l                         ; copy literal byte
        call  z, readBlock
        ld    a, (hl)
        exx
        inc   e
        call  z, writeBlock
        ld    (de), a
lddb14: exx
        djnz  lddb07
        dec   c
        jr    nz, lddb07
        exx
        pop   af                        ; return with last block flag
        ret                             ; (A=1,Z=0 if last block)

copyLZMatch:
        exx
        add   a, low (slotBitsTableL + 8)
        call  readEncodedValue          ; decode match length - 1
        ld    l, b                      ; save length (L: LSB, C: MSB)
        ld    c, a
        or    b
        jr    z, lclm02                 ; length == 1 byte ?
        dec   a
        or    c
        jr    nz, lclm01                ; length >= 3 bytes ?
        ld    a, 020h                   ; length == 2 bytes, read 3 prefix bits
        ld    b, low slotBitsTableO2
        jr    lclm03
lclm01: exx                             ; length >= 3 bytes,
        ld    a, d                      ; variable prefix size
        exx
        ld    b, low slotBitsTableO3
        jr    lclm03
lclm02: ld    a, 040h                   ; length == 1 byte, read 2 prefix bits
        ld    b, low slotBitsTableO1
lclm03: call  readBits                  ; read offset prefix bits
        add   a, b
        call  readEncodedValue          ; decode match offset
        ld    h, a
        ld    a, e                      ; calculate LZ77 match read address
    if NO_BORDER_FX == 0
        out   (081h), a
    endif
        sub   b
        ld    b, l                      ; length LSB
        ld    l, a
        ld    a, d
        sbc   a, h
        ld    h, a
        jr    c, lclm15                 ; set up memory paging
        jp    p, lclm13
        in    a, (0b2h)
lclm04: res   7, h                      ; read from page 1
lclm05: set   6, h
lclm06: out   (0b1h), a
        inc   b                         ; B: (length - 1) LSB + 1
        inc   c                         ; C: (length - 1) MSB + 1
lclm07: inc   e                         ; copy match data
        jr    z, lclm10
lclm08: ld    a, (hl)
        ld    (de), a
        inc   l
        jr    z, lclm11
lclm09: djnz  lclm07
        dec   c
        jr    z, lddb14                 ; return to main decompress loop
        jr    lclm07
lclm10: call  writeBlock
        jr    lclm08
lclm11: inc   h
        jp    p, lclm09
        ld    h, 040h
        push  iy
        in    a, (0b1h)
lclm12: cp    (iy)
        dec   iy
        jr    nz, lclm12
        ld    a, (iy + 2)
        out   (0b1h), a                 ; read next segment
        pop   iy
        jr    lclm09
lclm13: add   a, a
        jp    p, lclm14
        ld    a, (iy - 1)
        jr    lclm06
lclm14: ld    a, (iy - 2)
        jr    lclm05
lclm15: add   a, a
        jp    p, lclm16
        ld    a, (iy - 3)
        jr    lclm04
lclm16: ld    a, (iy - 4)
        jr    lclm04

read8Bits:
        ld    a, 001h

readBits:
        exx
lrb_01: sla   e
        jr    z, lrb_03
lrb_02: adc   a, a
        jp    nc, lrb_01
        exx
        ret
lrb_03: inc   l
        call  z, readBlock
        ld    e, (hl)
        rl    e
        jp    lrb_02

readEncodedValue:
        ld    ixl, a
        xor   a
        ld    h, a
        ld    b, (ix)
        cp    b
        jr    z, lrev03
lrev01: exx
        sla   e
        jr    z, lrev04
lrev02: exx
        adc   a, a
        rl    h
        djnz  lrev01
lrev03: add   a, (ix + totalSlots)
        ld    b, a
        ld    a, h
        adc   a, (ix + (totalSlots * 2))
        ret                             ; return LSB in B, MSB in A
lrev04: inc   l
        call  z, readBlock
        ld    e, (hl)
        rl    e
        jp    lrev02

