; Factorielle RECURSIVE de 5 en PowerPC, avec une vraie pile.
; Resultat : r3 = 120. Lancer avec : nerith --ppc --trace examples\factorielle_recursive_ppc.s
;
; Convention utilisee ici (inspiree de l'ABI PowerPC) :
;   - r1 = pointeur de pile, qui descend ; r3 = argument puis resultat
;   - un appel "bl fact" met l'adresse de retour dans lr ; "blr" y retourne
;   - une fonction qui en appelle une autre sauvegarde lr (et ce dont elle a besoin) sur la pile

main:   mflr r12            # sauvegarde lr (vaut 0 au depart) pour pouvoir finir proprement
        li r1, 0x800        # pile en haut de la memoire utilisee (2048)
        li r3, 5            # argument : n = 5
        bl fact             # r3 = fact(r3)
        mtlr r12            # remet lr a 0 : le blr suivant termine le programme
        blr

fact:   cmpwi r3, 1
        ble base            # si n <= 1, on retourne 1
        mflr r0             # lr est ecrase par le bl suivant : on le garde
        stwu r1, -16(r1)    # reserve 16 octets sur la pile (r1 = r1 - 16) et ecrit le mot a cette adresse
        stw r0, 20(r1)      # sauvegarde lr dans l'espace de l'appelant
        stw r3, 8(r1)       # sauvegarde n
        addi r3, r3, -1
        bl fact             # r3 = fact(n - 1)
        lwz r4, 8(r1)       # relit n
        mullw r3, r3, r4    # resultat = fact(n - 1) * n
        lwz r0, 20(r1)      # relit lr
        addi r1, r1, 16     # libere la pile
        mtlr r0
        blr
base:   li r3, 1
        blr
