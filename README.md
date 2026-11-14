# Nerith

Projet de recherche et de développement d'un émulateur Xbox 360 indépendant pour PC (Windows), écrit en C++.

> **Statut : phase 4 commencée. Un CPU pédagogique (11 instructions) et un début d'interpréteur PowerPC (environ 105 formes d'instructions entières : arithmétique, logique, décalages, rotations, comparaisons, mémoire de 8 à 64 bits, sauts, appels de fonction avec pile) avec assembleur, désassembleur et tests automatiques sont fonctionnels. Ce n'est pas encore un émulateur : il ne lit aucun exécutable Xbox 360 et ne lance aucun jeu.**

*Nerith est un nom de travail provisoire : il peut changer avant la première version publique.*

## Pourquoi ce projet

Nerith est un projet d'apprentissage mené à ciel ouvert. Il part d'un hobby (la Xbox 360) et sert à apprendre sérieusement le C++, l'architecture des ordinateurs et l'émulation, étape par étape et avec des jalons vérifiables.

L'objectif n'est pas de rivaliser avec les projets existants, comme [Xenia](https://github.com/xenia-project/xenia), mais de comprendre comment fonctionne une console, en construisant les briques soi-même et en documentant la démarche.

## Objectifs

- Émuler le processeur Xenon (PowerPC) de la console.
- Construire une architecture modulaire : CPU, mémoire, GPU (Xenos), audio, noyau système, périphériques.
- Progresser par petits jalons testables, en commençant par un interpréteur simple.
- Documenter chaque décision technique.

## Ce que le projet n'est pas

- Un projet de piratage. Nerith n'inclut et n'incluera **aucun jeu, aucun BIOS, aucun firmware, aucune clé ni aucun fichier propriétaire de Microsoft**.
- Un projet affilié à Microsoft. « Xbox » et « Xbox 360 » sont des marques de Microsoft Corporation. Nerith n'est ni approuvé, ni sponsorisé, ni associé à Microsoft.
- Une promesse de compatibilité avec les jeux commerciaux.

## Feuille de route

| Phase | Contenu |
|---|---|
| 0 | Préparation, documentation, outils, dépôt |
| 1 | Bases du C++ |
| 2 | Premier exercice : un CPU pédagogique |
| 3 | Architecture du projet |
| 4 | CPU PowerPC / Xenon |
| 5 | Mémoire |
| 6 | GPU (Xenos) et audio |
| 7 | Chargement et noyau système |
| 8 | Interface console |
| 9 | Périphériques |
| 10 | Compatibilité |

Le détail évolue au fil du travail : voir [docs/feuille-de-route.md](docs/feuille-de-route.md).

## Documentation

La documentation du projet (vision, périmètre, architecture, conventions de code, décisions techniques) est dans le dossier [docs/](docs/README.md).

## Environnement de développement

- Windows 11 (x64)
- Visual Studio Community (MSVC, charge de travail « Développement Desktop en C++ »)
- CMake
- Git

## Compiler et tester

Depuis un terminal où CMake et Visual Studio sont disponibles (par exemple « Developer PowerShell for VS ») :

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\nerith.exe
```

Pour lancer un programme écrit en texte :

```powershell
.\build\Debug\nerith.exe examples\somme.asm
```

Avec `--trace`, il affiche chaque instruction exécutée et l'état des registres après elle :

```powershell
.\build\Debug\nerith.exe --trace examples\somme.asm
```

Pour lancer un programme PowerPC écrit en texte (option `--ppc`, qui se combine avec `--trace`) :

```powershell
.\build\Debug\nerith.exe --ppc --trace examples\somme_ppc.s
.\build\Debug\nerith.exe --ppc examples\factorielle_recursive_ppc.s
.\build\Debug\nerith.exe --ppc examples\crc32_ppc.s
```

Pour afficher l'en-tête d'un fichier `.xex` (Xbox 360) que tu possèdes, sans le déchiffrer ni le décompresser :

```powershell
.\build\Debug\nerith.exe --xex chemin\vers\fichier.xex
```

Avec `--verbose`, il affiche aussi des messages de diagnostic (lecture du fichier, assemblage, fin de l'exécution). Les deux options se combinent. Sans argument, `nerith.exe` exécute un programme de démonstration. Le dossier `build/` est ignoré par Git. Pour l'instant, le programme assemble un programme écrit en texte, par exemple une boucle qui additionne 5 + 4 + 3 + 2 + 1, l'affiche en clair, l'exécute sur un CPU pédagogique à 4 registres et affiche le résultat. Ça sert d'exercice avant le vrai CPU PowerPC.

## Structure du dépôt

```
include/nerith/   en-têtes publics, un sous-dossier par module :
                    util/    fichiers, logs
                    memory/  mémoire de l'émulateur
                    toy/     CPU pédagogique, assembleur, désassembleur
                    ppc/     CPU PowerPC : décodeur, désassembleur, assembleur, interpréteur
                    xex/     lecture de l'en-tête des fichiers XEX (exécutables Xbox 360)
src/              code des modules (même découpage) et programme principal (main.cpp)
tests/            tests automatiques
examples/         programmes d'exemple : .asm (CPU pédagogique) et .s (PowerPC)
docs/             documentation du projet
```

## Licence

Ce projet est distribué sous licence **BSD 3-Clause**. Voir le fichier [LICENSE](LICENSE).

Copyright (c) 2026, RemyProd
