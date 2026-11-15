# Vecteurs (AltiVec / VMX128) : un produit scalaire de 4 flottants et une permutation de mots.
# Attendu : r12 = 1114636288 (0x42700000 = 60.0f : 1*2 + 2*4 + 3*6 + 4*8), r13 = 1082130432 (0x40800000 = 4.0f).

lis r3, 0x3F80       # 1.0f
lis r4, 0x4000       # 2.0f
lis r5, 0x4040       # 3.0f
lis r6, 0x4080       # 4.0f
li r10, 0x100        # un bloc de 16 octets aligne
stw r3, 0(r10)
stw r4, 4(r10)
stw r5, 8(r10)
stw r6, 12(r10)      # la memoire contient (1, 2, 3, 4)
lvx128 v100, 0, r10  # v100 = (1, 2, 3, 4) : les registres vont jusqu'a v127 avec VMX128
vaddfp128 v101, v100, v100   # (2, 4, 6, 8)
vmsum4fp128 v102, v100, v101 # produit scalaire, copie dans les 4 mots : 60
li r11, 0x110
stvx128 v102, 0, r11
lwz r12, 0(r11)
vpermwi128 v103, v100, 0xE4  # ordre des mots inverse : (4, 3, 2, 1)
li r14, 0x120
stvx128 v103, 0, r14
lwz r13, 0(r14)
blr
