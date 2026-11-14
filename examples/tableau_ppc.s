; Somme d'un tableau de 5 mots en memoire, avec lwzx (adresse = base + decalage).
; Attendu : r3 = 150 (10 + 20 + 30 + 40 + 50).
        li   r5, 0x100        ; adresse du tableau
        li   r6, 10
        stw  r6, 0(r5)
        li   r6, 20
        stw  r6, 4(r5)
        li   r6, 30
        stw  r6, 8(r5)
        li   r6, 40
        stw  r6, 12(r5)
        li   r6, 50
        stw  r6, 16(r5)
        li   r3, 0            ; somme
        li   r4, 0            ; decalage en octets
        li   r7, 5
        mtctr r7
boucle:
        lwzx r6, r5, r4
        add  r3, r3, r6
        addi r4, r4, 4
        bdnz boucle
        blr
