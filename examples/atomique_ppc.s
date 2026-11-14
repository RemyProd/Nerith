; Compteur atomique : lwarx lit en posant une reservation, stwcx. n'ecrit que si personne n'a touche a la case.
; Ici, un seul processeur : la reservation tient toujours et la boucle ne tourne qu'une fois.
; Attendu : r5 = 42 (41 + 1), et la case a l'adresse 0x80 contient 42.
        li     r6, 41
        li     r4, 0x80
        stw    r6, 0(r4)        ; compteur = 41
retry:  lwarx  r5, 0, r4        ; lit 41 et reserve la case
        addi   r5, r5, 1
        stwcx. r5, 0, r4        ; ecrit 42 si la reservation tient ; sinon EQ vaut 0 et on recommence
        bne    retry
        lwz    r7, 0(r4)
        blr
