; Calcul avec des nombres a virgule : (3/4)^2 + 3/4 = 1.3125, puis conversion en entier.
; Il n'y a pas encore de donnees dans le programme : on ecrit les entiers 3 et 4 en memoire, on les relit
; dans des registres flottants et fcfid les transforme en 3.0 et 4.0.
; Attendu : f3 = 0.75, f4 = 0.5625, f5 = 1.3125, et r5 = 0x3FF50000 (les 32 premiers bits de 1.3125 en memoire).
        li     r4, 0x100        ; zone de travail en memoire
        li     r3, 3
        std    r3, 0(r4)
        lfd    f1, 0(r4)
        fcfid  f1, f1           ; f1 = 3.0
        li     r3, 4
        std    r3, 8(r4)
        lfd    f2, 8(r4)
        fcfid  f2, f2           ; f2 = 4.0
        fdiv   f3, f1, f2       ; 0.75
        fmul   f4, f3, f3       ; 0.5625
        fadd   f5, f4, f3       ; 1.3125
        stfd   f5, 16(r4)
        lwz    r5, 16(r4)       ; mot de poids fort de 1.3125
        blr
