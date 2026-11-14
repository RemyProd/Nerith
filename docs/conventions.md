[← Index de la documentation](README.md)

# Conventions de code

Ces règles décrivent ce que le code de Nerith fait déjà. Elles servent à rester cohérent quand le projet grandit. Elles peuvent changer : la décision est notée dans [Décisions techniques](decisions.md) (D-010).

## Organisation des fichiers

| Dossier | Contenu |
|---|---|
| `include/nerith/` | En-têtes publics (`.h`) : ce que les autres parties du projet ont le droit d'utiliser |
| `src/` | Code (`.cpp`) et programme principal (`main.cpp`) |
| `tests/` | Un fichier `test_<sujet>.cpp` par sujet |
| `examples/` | Programmes d'exemple (`.asm`) |
| `docs/` | Documentation |

- Un sujet = un en-tête + un fichier source du même nom (`memory.h` et `memory.cpp`).
- Tout le code du projet est dans le namespace `nerith`. Les petites fonctions qui ne servent qu'à un fichier vont dans un namespace anonyme `namespace { ... }` en haut du `.cpp`.

## Noms

| Élément | Style | Exemple |
|---|---|---|
| Fichiers | `snake_case` | `disassembler.cpp` |
| Types (classes, structures, enums) | `PascalCase` | `Memory`, `Status`, `LogLevel` |
| Valeurs d'enum | `PascalCase` | `Status::Halted` |
| Fonctions et variables | `snake_case` | `read_text_file`, `max_steps` |
| Membres privés d'une classe | `snake_case` avec `_` à la fin | `data_` |
| Constantes globales | `kPascalCase` | `kDemoProgram` |

Les noms sont en anglais dans le code, les commentaires en français.

## En-têtes

- `#pragma once` en première ligne de chaque `.h`.
- Dans un `.cpp` : d'abord son propre en-tête, puis la bibliothèque standard, puis les autres en-têtes du projet.
- Un en-tête n'inclut que ce dont il a besoin lui-même.
- Chaque fonction publique a un commentaire dans l'en-tête qui dit ce qu'elle fait, ce qu'elle renvoie et quelles erreurs elle peut signaler.

## Types

- Tailles exactes pour tout ce qui ressemble au matériel : `std::uint8_t`, `std::uint32_t`, etc.
- `std::size_t` pour les adresses et les tailles de l'émulateur.
- `enum class` plutôt que des nombres ou des `enum` simples.
- `const` dès que possible, références `const&` pour passer des objets sans les copier.

## Erreurs

- Un accès impossible à la mémoire émulée lève `std::out_of_range` (classe `Memory`). Une erreur dans un programme source lève `AssemblyError`. Un fichier introuvable lève `std::runtime_error`.
- Le CPU **ne laisse pas passer d'exception** : `step` et `run` les transforment en `Status` (`OutOfBounds`, `InvalidInstruction`, `StepLimit`).
- Une opération qui peut échouer en plein milieu ne doit rien modifier à moitié (exemple : `Memory::write32` vérifie les 4 octets avant d'écrire).

## Logs

- On utilise `log_debug`, `log_info`, `log_warning` et `log_error` de `nerith/log.h`, pas `std::cerr` directement.
- Pas de log dans la boucle d'exécution du CPU (`step`) : elle doit rester rapide. Pour suivre l'exécution, on utilise `--trace`.
- Niveau par défaut : `Warning`. `--verbose` passe au niveau `Debug`.

## Tests

- Chaque nouveau comportement a un test dans `tests/`. Pour une nouvelle instruction, écrire le test d'abord.
- Un test = un bloc `{ ... }` avec un nom lisible de la forme « sujet : résultat attendu ».
- Les tests sont enregistrés dans CMake (`add_test`) et se lancent avec `ctest`.

## Compilation

- Standard C++20, sans extensions du compilateur.
- Avertissements au maximum (`/W4` avec MSVC, `-Wall -Wextra` avec les autres) : **zéro avertissement** attendu.
- La bibliothèque standard seulement, pour l'instant. Toute dépendance externe doit être notée dans [Décisions techniques](decisions.md).

## Texte et encodage

- Dans le code (chaînes et commentaires) : **ASCII seulement**, sans accents, pour éviter les problèmes d'encodage avec MSVC.
- Dans `docs/` et le README : français avec accents, en UTF-8.
- Fins de ligne `LF`, indentation de 4 espaces, ligne vide à la fin du fichier (réglé par `.editorconfig`).

## Git

- Messages de commit courts, en français, à l'impératif : « Ajoute l'assembleur ».
- Un commit = un changement compréhensible. Les tests doivent passer avant de pousser.
- Jamais de jeu, BIOS, firmware, clé ni fichier propriétaire dans le dépôt (voir `.gitignore`).
- Pas de code copié dont on ne comprend pas le fonctionnement, et pas de code sous licence incompatible (pas de GPL).
