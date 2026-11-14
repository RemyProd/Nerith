# Nerith

Projet de recherche et de développement d'un émulateur Xbox 360 indépendant pour PC (Windows), écrit en C++.

> **Statut : phase 0, documentation et préparation. Aucun code n'est encore écrit, et rien ne fonctionne pour l'instant.**

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

La documentation du projet (vision, périmètre, architecture, décisions techniques) est dans le dossier [docs/](docs/README.md).

## Environnement de développement

- Windows 11 (x64)
- Visual Studio Community (MSVC, charge de travail « Développement Desktop en C++ »)
- CMake
- Git

Les instructions de compilation seront ajoutées avec le premier code.

## Licence

Ce projet est distribué sous licence **BSD 3-Clause**. Voir le fichier [LICENSE](LICENSE).

Copyright (c) 2026, RemyProd
