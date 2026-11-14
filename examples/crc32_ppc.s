; CRC32 (le CRC de zip, png...) calcule a la main, octet par octet, bit par bit.
; On ecrit "abc" en memoire, puis on calcule son CRC32. Attendu : r3 = 0x352441C2 (= 891568578).
        li    r4, 0x100        ; adresse des donnees
        li    r7, 0x61
        stb   r7, 0(r4)        ; 'a'
        li    r7, 0x62
        stb   r7, 1(r4)        ; 'b'
        li    r7, 0x63
        stb   r7, 2(r4)        ; 'c'
        li    r5, 3            ; longueur

        lis   r6, 0xEDB8
        ori   r6, r6, 0x8320   ; polynome 0xEDB88320
        clrldi r6, r6, 32      ; lis a rempli le haut de r6 avec des 1 : on garde 32 bits
        li    r3, -1
        clrldi r3, r3, 32      ; crc = 0xFFFFFFFF
        mtctr r5               ; une tour de boucle par octet
octet:
        lbz   r7, 0(r4)
        addi  r4, r4, 1
        xor   r3, r3, r7       ; crc ^= octet
        li    r8, 8            ; 8 bits par octet
bit:
        andi. r9, r3, 1        ; le bit de poids faible vaut-il 0 ? (met a jour cr0)
        srwi  r3, r3, 1        ; crc >>= 1
        beq   suite
        xor   r3, r3, r6       ; bit a 1 : crc ^= polynome
suite:
        addi  r8, r8, -1
        cmpwi r8, 0
        bne   bit
        bdnz  octet
        not   r3, r3           ; crc = ~crc
        clrldi r3, r3, 32
        blr
