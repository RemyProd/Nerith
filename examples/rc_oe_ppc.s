# Les bits Rc (le point) et OE (le "o") : un resultat qui met a jour cr0, et un debordement signale dans XER.
# Attendu : r6 = 9223372036854775808 (0x8000000000000000), r7 = 3221225472 (0xC0000000 : OV + SO),
#           r8 = 2415919104 (0x90000000 : LT + SO), r11 = 0, r12 = 805306368 (0x30000000 : EQ + SO).

li r4, -1
srdi r4, r4, 1       # r4 = 0x7FFFFFFFFFFFFFFF : le plus grand nombre positif sur 64 bits
li r5, 1
addo. r6, r4, r5     # deborde : r6 devient negatif ; OV et SO sont poses ; cr0 = LT + SO
mfxer r7             # r7 = OV + SO
mfcr r8              # r8 = cr : seul cr0 est rempli (LT + SO)
li r9, 5
li r10, -5
add. r11, r9, r10    # 5 + (-5) = 0 : cr0 = EQ (le SO de XER reste recopie dans cr0)
mfcr r12
blr
