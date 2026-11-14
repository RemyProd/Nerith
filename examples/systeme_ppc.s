# Instructions systeme : dcbz (mise a zero d'un bloc de cache) et la base de temps.
# Attendu : r5 = 0x77 (119), r6 = 0x77 (l'octet ecrit), r7 = 0 (efface par dcbz), r8 = 6 (instructions executees avant mftb).

li r4, 0x200        # adresse de travail (alignee sur 32 octets)
li r5, 0x77
stb r5, 0x10(r4)    # ecrit un octet au milieu du bloc
lbz r6, 0x10(r4)    # relit : 0x77
dcbz 0, r4          # met a zero tout le bloc de 32 octets a partir de 0x200
lbz r7, 0x10(r4)    # relit : 0
mftb r8             # la base de temps : 1 tic par instruction executee
blr
