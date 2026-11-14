; Factorielle de 5 en PowerPC : 5 * 4 * 3 * 2 * 1 = 120.
; Lancer avec : nerith --ppc --trace examples\factorielle_ppc.s
        li r3, 5            # compteur
        li r4, 1            # resultat
loop:   mullw r4, r4, r3    # resultat = resultat * compteur
        addi r3, r3, -1     # compteur = compteur - 1
        cmpwi r3, 0
        bne loop            # tant que le compteur n'est pas 0
        mr r5, r4           # copie le resultat dans r5
        blr                 # fin du programme
