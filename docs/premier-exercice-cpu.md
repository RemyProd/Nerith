[← Index de la documentation](README.md)

# Premier exercice — CPU pédagogique

> À réaliser après la préparation de l'environnement et les premiers fondamentaux C++.

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
- [ ] Créer un projet console C++.
- [ ] Définir une énumération `Operation`.
- [ ] Définir une structure `Instruction`.
- [ ] Définir une classe `VirtualCPU`.
- [ ] Ajouter les quatre registres.
- [ ] Ajouter l'état du processeur.
- [ ] Implémenter `LOAD`.
- [ ] Implémenter `ADD`.
- [ ] Implémenter `SUB`.
- [ ] Implémenter `HALT`.
- [ ] Afficher les registres.
- [ ] Vérifier le résultat attendu.
- [ ] Ajouter la mémoire virtuelle.
- [ ] Ajouter `STORE`.
- [ ] Ajouter le compteur de programme.
- [ ] Gérer les instructions invalides et les accès hors limites.

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
- Ajouter un mode pas-à-pas.
- Ajouter un désassembleur simple.
- Ajouter une trace d'exécution.
- Ajouter des tests unitaires.
