; Division par 4 avec arrondi vers zero, comme le font les compilateurs : srawi puis addze.
; srawi decale avec le signe et pose la retenue (CA) si des bits 1 sont partis ; addze l'ajoute.
; Attendu : r3 = -7 / 4 = -1, r5 = 7 / 4 = 1, r7 = -8 / 4 = -2.
; (Nerith affiche les registres sans signe : -1 s'ecrit 18446744073709551615 et -2 s'ecrit 18446744073709551614.)
        li     r4, -7
        srawi  r3, r4, 2       ; -7 >> 2 = -2, avec retenue (des bits 1 sont partis)
        addze  r3, r3          ; -2 + CA = -1

        li     r6, 7
        srawi  r5, r6, 2       ; 7 >> 2 = 1, pas de retenue
        addze  r5, r5

        li     r8, -8
        srawi  r7, r8, 2       ; -8 >> 2 = -2, pas de retenue (que des 0 sont partis)
        addze  r7, r7
        blr
