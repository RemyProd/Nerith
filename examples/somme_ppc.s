; Somme de 5 + 4 + 3 + 2 + 1 en PowerPC.
; Lancer avec : nerith --ppc --trace examples\somme_ppc.s
        li r3, 5            # compteur
        li r4, 0            # somme
loop:   add r4, r4, r3      # somme = somme + compteur
        addi r3, r3, -1     # compteur = compteur - 1
        cmpwi r3, 0         # compare le compteur a 0 (resultat dans CR0)
        bne loop            # si different de 0, on recommence
        stw r4, 256(r0)     # range le resultat en memoire a l'adresse 256
        lwz r5, 256(r0)     # et le relit dans r5
        blr                 # lr vaut 0 : fin du programme
