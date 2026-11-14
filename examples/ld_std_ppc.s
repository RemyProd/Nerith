; Mots de 64 bits : ld, std, cmpdi, et la difference avec cmpwi.
; On fabrique 2^32 en memoire (octets 00 00 00 01 00 00 00 00, big-endian), puis on le relit en 64 bits.
; Attendu : r5 = 0x100000000, r4 = 1 (cmpdi voit un nombre different de 0), r8 = 0 (cmpwi ne voit que les 32 bits de poids faible : 0).
        li    r6, 0x200
        li    r7, 1
        stw   r7, 0(r6)        ; mot de poids fort = 1
        li    r7, 0
        stw   r7, 4(r6)        ; mot de poids faible = 0
        ld    r5, 0(r6)        ; r5 = 2^32
        std   r5, 8(r6)        ; recopie dans les 8 octets suivants
        li    r4, 0
        cmpdi r5, 0
        beq   suite
        li    r4, 1            ; 64 bits : different de 0
suite:
        li    r8, 0
        cmpwi r5, 0
        beq   fin
        li    r8, 1            ; ne doit pas arriver
fin:
        blr
