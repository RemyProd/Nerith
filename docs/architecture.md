[← Index de la documentation](README.md)

# Architecture technique

## Objectif
Séparer les responsabilités afin de développer, tester et remplacer les composants indépendamment.

## Modules envisagés

### Application
Démarrage, configuration, journaux et interface utilisateur.

### CPU
État du processeur émulé, registres, décodage et exécution des instructions PowerPC/Xenon. Commencer par un interpréteur vérifiable ; étudier un JIT plus tard.

### Memory
Modèle mémoire, régions adressables, permissions et accès mémoire. Le modèle exact devra être établi à partir de documentation et de tests.

### GPU
Traduction des commandes graphiques et comportement du GPU Xenos vers le matériel PC. L'architecture Xbox 360 ne se réduit pas à un simple appel Direct3D.

### Audio
Formats, traitement et sortie audio.

### Kernel et services système
Appels système, threads, événements, chargement des programmes et services nécessaires.

### Input et périphériques
Abstraction des manettes, entrées utilisateur et périphériques spécialisés.

### Storage
Périphériques de stockage virtuels et formats pris en charge.

### Dashboard et interface
Expérience utilisateur. Ce module ne doit pas contenir le cœur de l'émulation.

## Technologies envisagées

| Domaine | Choix envisagé | Remarque |
|---|---|---|
| Langage | C++ | Standard à fixer |
| Build | CMake | Génération du projet |
| Compilateur | MSVC | Windows |
| Interface | À décider | Après prototype |
| Graphismes | À étudier | Selon les besoins |
| Audio | À étudier | Après analyse |
| Entrées | À étudier | HID, XInput, USB |
| Tests | GoogleTest envisagé | À intégrer au bon moment |
| Versionnement | Git | GitHub |

## Contraintes
- L'émulation CPU et GPU demande une compréhension précise du matériel.
- Le système doit être reproduit assez fidèlement pour les logiciels ciblés.
- Une couche d'abstraction permet de tester sans dépendre immédiatement du GPU réel.
- La compatibilité dépend de l'ensemble des composants.
- Le multithreading doit être introduit prudemment et mesuré.
- Le JIT exige une gestion rigoureuse de la mémoire exécutable et de la sécurité.

## Organisation actuelle du code

Le code existant est rangé en cinq modules. Chaque module est une bibliothèque CMake distincte, ce qui force à déclarer les dépendances.

```text
include/nerith/<module>/   en-têtes du module
src/<module>/              code du module
```

| Module | Contenu | Dépend de |
|---|---|---|
| `util` | Lecture de fichiers (`file.h`), logs (`log.h`) | rien |
| `memory` | Classe `Memory` : octets, big-endian, accès vérifiés | rien |
| `toy` | CPU pédagogique (`cpu.h`), assembleur, désassembleur | `memory` |
| `ppc` | CPU PowerPC : décodeur, désassembleur, assembleur, interpréteur, table d'instructions (`nerith::ppc`) | `memory` |
| `xex` | Lecture de l'en-tête d'un fichier XEX (exécutable Xbox 360), sans déchiffrement (`nerith::xex`) | rien |

L'exécutable `nerith` (`src/main.cpp`) utilise `toy`, `ppc`, `xex` et `util` (option `--ppc` pour le PowerPC, option `--xex` pour lire un en-tête XEX). Chaque test n'est lié qu'au module qu'il vérifie.

Dans `ppc`, la plupart des instructions sont décrites une seule fois dans une table (`optable.h` et `optable.cpp` : code opération, nom, place des champs). Le décodeur, l'assembleur et l'affichage la lisent tous les trois ; ce que fait chaque instruction reste écrit dans `cpu.cpp`. Décision D-015 dans `docs/decisions.md`.

Règles :
- Un module ne dépend que des modules plus bas dans le tableau. Pas de dépendance circulaire.
- On inclut un en-tête avec son module : `#include "nerith/memory/memory.h"`.
- `Memory` est partagée : `toy` et `ppc` l'utilisent, sans dépendre l'un de l'autre.
- Le namespace reste `nerith` pour tous les modules. `ppc` est dans `nerith::ppc`, car ses noms (`Cpu`, `step`, `run`...) sont les mêmes que ceux de `toy` ; `toy` pourra passer dans `nerith::toy` plus tard.

Décisions : D-012, D-013, D-014 et D-015 dans [Décisions techniques](decisions.md).

## Arborescence cible du dépôt

```text
Nerith/
├── .github/
│   └── ISSUE_TEMPLATE/
├── docs/
├── include/
│   └── nerith/
│       ├── cpu/        (futur ; pour l'instant le CPU PowerPC est dans ppc/ et le CPU pédagogique dans toy/)
│       ├── memory/
│       ├── gpu/
│       ├── audio/
│       ├── kernel/
│       └── input/
├── src/
│   ├── app/
│   ├── cpu/
│   ├── memory/
│   ├── gpu/
│   ├── audio/
│   ├── kernel/
│   ├── input/
│   └── system/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── test_programs/
├── tools/
├── third_party/
├── CMakeLists.txt
├── .gitignore
├── LICENSE
└── README.md
```

Cette arborescence est une cible, pas une obligation immédiate : créer uniquement les dossiers utiles au premier jalon. Aucun jeu, firmware, clé ni fichier système propriétaire ne doit être ajouté au dépôt.

## Questions ouvertes
- Standard C++.
- Représentation et stratégie d'interpréteur CPU.
- Modèle mémoire.
- Threads et synchronisation.
- API graphique.
- Recompilation dynamique.
- Versions système à supporter.
- Stratégie de tests.

Les décisions prises sont consignées dans [Décisions techniques](decisions.md).
