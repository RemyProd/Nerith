; Ecrit un nombre en memoire, le relit, puis le double.
; Resultat attendu : r2 = 84, et le mot a l'adresse 100 vaut 42

        Set   r0, 42
        Store r0, [100]     ; memoire[100] = 42
        Load  r1, [100]     ; r1 = memoire[100]
        Set   r2, 0
        Add   r2, r1
        Add   r2, r1        ; r2 = 42 + 42
        Halt
