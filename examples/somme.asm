; Somme de 5 + 4 + 3 + 2 + 1 avec une boucle.
; Resultat attendu : r1 = 15

        Set r0, 5       ; compteur
        Set r1, 0       ; somme
loop:   Add r1, r0
        Dec r0
        Jnz r0, loop    ; tant que r0 != 0, on recommence
        Halt
