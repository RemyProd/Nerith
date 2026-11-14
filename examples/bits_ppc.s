; Decoder une instruction PowerPC... avec un programme PowerPC.
; On prend le mot 0x38600005 (li r3, 5) et on en extrait les champs, comme le fait le decodeur C++ de Nerith.
; Lancer avec : nerith --ppc --trace examples\bits_ppc.s
; Resultat attendu : r4 = 14 (opcode), r5 = 3 (rD), r6 = 0 (rA), r7 = 5 (d)
        lis r3, 0x3860          # moitie haute du mot
        ori r3, r3, 5           # moitie basse : r3 = 0x38600005
        srwi r4, r3, 26         # opcode : les 6 premiers bits       (equivaut a  mot >> 26)
        rlwinm r5, r3, 11, 27, 31   # rD : bits 21 a 25 (rotation de 11, on garde 5 bits)
        rlwinm r6, r3, 16, 27, 31   # rA : bits 16 a 20
        clrlwi r7, r3, 16       # d : les 16 derniers bits           (equivaut a  mot & 0xFFFF)
        blr
