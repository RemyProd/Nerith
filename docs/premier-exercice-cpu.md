[← Index de la documentation](README.md)

# Premier exercice — CPU pédagogique

> **Réalisé le 2026-11-14** (voir la section « État réel » ci-dessous). Ce document garde la spécification d'origine.

## But
Construire un mini-processeur pédagogique pour comprendre l'état CPU, les registres, les instructions, la mémoire et la boucle d'exécution.

Ce processeur n'est **pas** un processeur Xbox 360 et n'exécute pas du PowerPC. C'est un exercice d'apprentissage.

## Spécification version 1
- Quatre registres : R0 à R3.
- Mémoire virtuelle de 256 octets.
- Compteur de programme (PC).
- État actif ou arrêté.
- Compteur d'instructions exécutées.

Instructions :

| Instruction | Effet |
|---|---|
| `LOAD` | Charger une valeur dans un registre |
| `ADD` | Additionner deux registres |
| `SUB` | Soustraire deux registres |
| `STORE` | Écrire une valeur en mémoire |
| `HALT` | Arrêter le processeur |

## Programme de test

```text
LOAD R0, 10
LOAD R1, 20
ADD R2, R0, R1
HALT
```

Résultat attendu :

```text
R0 = 10
R1 = 20
R2 = 30
R3 = 0
Instructions exécutées = 4
État = ARRÊTÉ
```

## Étapes
- [x] Créer un projet console C++.
- [x] Définir une énumération `Operation`.
- [x] Définir une structure `Instruction`. (remplacée par 3 octets en mémoire)
- [x] Définir une classe `VirtualCPU`. (remplacée par `Cpu` et `run`)
- [x] Ajouter les quatre registres.
- [x] Ajouter l'état du processeur.
- [x] Implémenter `LOAD`.
- [x] Implémenter `ADD`.
- [x] Implémenter `SUB`.
- [x] Implémenter `HALT`.
- [x] Afficher les registres.
- [x] Vérifier le résultat attendu.
- [x] Ajouter la mémoire virtuelle.
- [x] Ajouter `STORE`.
- [x] Ajouter le compteur de programme.
- [x] Gérer les instructions invalides et les accès hors limites.

## État réel
La version actuelle (`include/nerith/cpu.h`, `memory.h`, `disassembler.h`) s'écarte de la spécification d'origine :

- Mémoire paramétrable (classe `Memory`, 64 octets dans la démo), lecture et écriture 32 bits en big-endian, accès vérifiés.
- Instructions de 3 octets (opcode, registre, valeur) lues en mémoire, avec compteur de programme `pc`.
- Jeu d'instructions : `Set`, `Add`, `Sub`, `Mul`, `Inc`, `Dec`, `Load`, `Store`, `Jmp`, `Jnz`, `Halt`.
- Résultat renvoyé par `run` : `Halted`, `InvalidInstruction`, `OutOfBounds`, `StepLimit` (garde-fou contre les boucles infinies). Pas d'état « actif/arrêté » : c'est ce `Status` qui le remplace.
- Compteur d'instructions exécutées : `cpu.instructions_executed`.
- Assembleur : `assemble` transforme un programme en texte (étiquettes, commentaires, décimal et hexadécimal) en octets ; erreurs signalées avec le numéro de ligne.
- `step` exécute une seule instruction (`Status::Running` si le programme continue) ; `run` boucle sur `step` avec le garde-fou `StepLimit`. `nerith.exe --trace` s'appuie sur `step` pour afficher une trace d'exécution.
- Désassembleur : `disassemble` et `disassemble_range`.
- Tests automatiques avec CTest : `test_cpu`, `test_memory`, `test_assembler`, `test_disassembler`.
- Pas de classe `VirtualCPU` : `Cpu` est une `struct` et `run` une fonction.
- Limite connue : les adresses (sauts, `Load`, `Store`) tiennent sur 1 octet, donc 255 au maximum.

## Questions à savoir expliquer
- Qu'est-ce qu'un registre ?
- Pourquoi un processeur a-t-il un compteur de programme ?
- Quelle différence entre instruction et donnée ?
- Pourquoi utiliser une énumération ?
- Pourquoi encapsuler l'état dans une classe ?
- Que se passe-t-il si une instruction vise R4 ?
- Comment le processeur sait-il quand s'arrêter ?

## Critère de validation
Le programme fonctionne, les résultats sont vérifiés et on peut expliquer chaque partie du code.

## Améliorations ultérieures
- Charger le programme depuis un fichier.
- ~~Ajouter un mode pas-à-pas.~~ Fait (`step`).
- ~~Ajouter un désassembleur simple.~~ Fait.
- ~~Ajouter une trace d'exécution.~~ Fait (`--trace`).
- ~~Ajouter des tests unitaires.~~ Fait.
- ~~Ajouter un assembleur (texte vers octets).~~ Fait.
