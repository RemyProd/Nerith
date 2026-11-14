; Le registre CTR : une boucle avec bdnz, puis un appel de fonction par adresse (bctrl).
; Lancer avec : nerith --ppc --trace examples\ctr_ppc.s
; Resultat attendu : r4 = 10 (5 tours de +2) et r3 = 20 (la fonction double r4).
        mflr r12            # sauvegarde lr (vaut 0 au depart)
        li r3, 5
        mtctr r3            # CTR = 5 : nombre de tours
        li r4, 0
loop:   addi r4, r4, 2
        bdnz loop           # CTR = CTR - 1, puis saute tant que CTR n'est pas 0
        li r5, 44           # adresse de la fonction "double" : c'est la 12e instruction, donc 11 * 4 = 44
        mtctr r5
        bctrl               # appel indirect : saute a l'adresse dans CTR, lr = retour
        mtlr r12            # remet lr a 0 : le blr suivant termine le programme
        blr
double: add r3, r4, r4      # r3 = r4 + r4
        blr
