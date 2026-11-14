[← Index de la documentation](README.md)

# Décisions techniques

Conserver les décisions importantes et leur justification. Ne pas effacer une ancienne décision : la marquer comme remplacée et créer une nouvelle entrée.

## Registre

| ID | Sujet | État | Décision |
|---|---|---|---|
| D-001 | Plateforme initiale | Retenue | Windows |
| D-002 | Langage principal | Retenue | C++ |
| D-003 | Build | Retenue | CMake |
| D-004 | Dépôt | Retenue | GitHub public |
| D-005 | Nom | Provisoire | Nerith (remplace Project Xenon) |
| D-006 | Architecture | En cours | Modules indépendants ; découpage en D-012, D-013 et D-014 |
| D-007 | Kernel cible | À étudier | Architecture multi-version ; le dernier kernel ne garantit pas tous les jeux |
| D-008 | API graphique | À étudier | Décider après recherche et prototype |
| D-009 | Licence | Provisoire | BSD-3-Clause (copyright : RemyProd) |
| D-010 | Conventions de code | Retenue | Conventions écrites (`docs/conventions.md`) et `.editorconfig`, sans formateur automatique |
| D-011 | Logs | Retenue | Petit module maison (`nerith/util/log.h`), sans dépendance |
| D-012 | Organisation du code | Retenue | Modules `util`, `memory`, `toy` : un sous-dossier et une bibliothèque CMake chacun |
| D-013 | Module PowerPC | Retenue | Module `ppc` (décodeur, désassembleur, assembleur, interpréteur) dans `nerith::ppc`, qui dépend de `memory` |
| D-014 | Lecture des fichiers XEX | Retenue | Module `xex` (`nerith::xex`, sans dépendance) : en-tête seulement, aucun déchiffrement ni décompression |
| D-015 | Table d'instructions et vérification avec llvm | Retenue | Les nouvelles instructions sont décrites une fois dans `optable` ; chaque mot est comparé à llvm-mc (et le texte, sauf pour les sauts) |

## Décisions détaillées

### D-003 — Outil de build
- **Date :** 2026-11-14
- **État :** retenue (était « proposée »)
- **Décision :** CMake, avec Visual Studio (MSVC) comme compilateur principal.
- **Justification :** en usage depuis le premier code ; génère la solution Visual Studio, fonctionne aussi avec g++ (utile pour vérifier les avertissements avec `-Werror`).

### D-005 — Nom du projet
- **Date :** 2026-11-14
- **État :** provisoire
- **Contexte :** « Project Xenon » entre en conflit avec un émulateur Xbox 360 existant (Xenon). Éviter aussi « Xbox » et « 360 » (marques de Microsoft).
- **Options étudiées :** Krypton, Orvia, Zenth, Vanta, Kiro, Velkor, Taleon, Quorin, Ixira, Celer, Halvyn, Kaelith, Tavren, Soravin, Veloria, Ondrel, Nerith.
- **Décision :** Nerith, à titre provisoire.
- **Justification :** aucun émulateur ni logiciel notable du même nom ; les dépôts GitHub trouvés n'ont aucune étoile ; le compte `Nerith` est personnel et sans rapport. Ondrel est le meilleur candidat de rechange.
- **Conséquences :** nom de dépôt `Nerith` sous le compte GitHub `RemyProd` (l'organisation `Nerith` n'est pas disponible) ; la mention « CPU Xenon » reste valable car elle désigne le processeur de la console.
- **Vérifié le 2026-11-14 :** `Baca-Z-Lesa/Nerith` est un jeu sans rapport (pas de conflit) ; noms de domaine vérifiés disponibles.
- **Marques vérifiées (TMview, 2026-11-14) :** trois dépôts brésiliens « NERITH » (classes 37, 39, 42) par Nerith Engenharia e Consultoria Ambiental SPE Ltda, déposés le 2025-07-01 : location de matériel, collecte de déchets, ingénierie ; aucun rapport avec le logiciel. NERITHÉE (USPTO, n° 99608470) : produits capillaires (classe 3), sans rapport. Aucun conflit identifié. Ce n'est pas un avis juridique.
- **Reste à faire :** aucun point bloquant ; garder Ondrel en rechange si un conflit apparaît.
- **Sources :** recherches web et GitHub du 2026-11-13 et 2026-11-14. Ce n'est pas un avis juridique.

### D-009 — Licence du dépôt
- **Date :** 2026-11-14
- **État :** provisoire
- **Contexte :** projet d'émulateur public sur GitHub, développé seul au départ, avec l'envie de pouvoir s'inspirer de projets existants comme Xenia (BSD-3-Clause).
- **Options étudiées :** MIT, Apache-2.0, BSD-3-Clause, GPL-3.0-or-later.
- **Décision :** BSD-3-Clause, à titre provisoire.
- **Justification :** licence permissive, compatible avec la réutilisation de code de Xenia, identique à celle du projet de référence ; changement possible plus tard tant que l'auteur est seul (par exemple vers la GPL si un risque de forks fermés apparaît).
- **Conséquences :** fichier `LICENSE` à la racine du dépôt avec le copyright « RemyProd » ; ne pas copier de code GPL dans le dépôt ; garder les notices de copyright pour tout code tiers repris ; engagement possible dans le README (jamais vendu, jamais fermé).
- **Limite :** ce n'est pas un avis juridique. La licence ne règle pas la question des droits Microsoft : aucun BIOS, firmware, clé ou jeu dans le dépôt.

### D-010 — Conventions de code
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** le code existant (CPU, mémoire, assembleur, fichiers) suit déjà des habitudes. Il vaut mieux les écrire avant que le projet grandisse.
- **Options étudiées :** (1) conventions écrites + `.editorconfig` ; (2) en plus, un formateur automatique (`clang-format`) ; (3) rien de formel.
- **Décision :** option 1. Voir `docs/conventions.md`.
- **Justification :** un outil de moins à apprendre pendant la phase d'apprentissage ; les conventions décrivent ce qui est déjà fait. Un formateur automatique pourra être ajouté plus tard.
- **Conséquences :** le code et les commentaires restent en ASCII (problèmes d'encodage MSVC évités) ; zéro avertissement au niveau maximum ; chaque comportement nouveau a un test.

### D-011 — Logs
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** besoin de messages de diagnostic (erreurs, étapes, détails) distincts de la sortie normale du programme.
- **Options étudiées :** (1) petit module maison ; (2) bibliothèque externe (par exemple spdlog) ; (3) `std::cerr` partout.
- **Décision :** option 1 : `nerith/util/log.h`, quatre niveaux (`Debug`, `Info`, `Warning`, `Error`) plus `Off`, flux configurable (utile pour les tests), niveau par défaut `Warning`, option `--verbose` pour passer à `Debug`.
- **Justification :** aucune dépendance, une centaine de lignes, testable, suffisant pour un seul thread.
- **Limites :** **pas sûr pour plusieurs threads** (variables globales sans protection). À revoir quand l'émulation multi-thread (6 threads matériels sur Xenon) arrivera, soit en protégeant le module, soit en passant à une bibliothèque. Pas de log dans la boucle chaude du CPU.
- **Conséquences :** `main` utilise `log_info`/`log_error` ; les résultats du programme restent sur la sortie standard.

### D-012 — Organisation du code en modules
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** le code existant (mémoire, CPU pédagogique, assembleur, fichiers, logs) était à plat dans `include/nerith/` et `src/`. Un futur CPU PowerPC va s'y ajouter et doit pouvoir réutiliser `Memory` sans mélanger ses fichiers avec ceux du CPU pédagogique.
- **Options étudiées :** (A) ne rien déplacer ; (B) un sous-dossier et une bibliothèque CMake par module ; (C) comme B, avec en plus un namespace par module.
- **Décision :** option B. Modules `util` (fichiers, logs), `memory` (`Memory`), `toy` (CPU pédagogique, assembleur, désassembleur). Dépendances : `toy` → `memory` ; `util` et `memory` ne dépendent de rien. Le namespace reste `nerith`.
- **Justification :** les dépendances sont déclarées dans CMake et vérifiées à la compilation ; `Memory` est clairement partagée ; le déplacement est mécanique et les 6 séries de tests vérifient qu'il n'a rien cassé.
- **Conséquences :** en-têtes inclus avec leur module (`nerith/memory/memory.h`) ; un test n'est lié qu'au module qu'il vérifie ; le futur module `ppc` s'ajoutera à côté de `toy`. L'option C sera reconsidérée quand deux modules auront des noms identiques.


### D-013 — Module `ppc` et namespace `nerith::ppc`
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** le CPU pédagogique (`toy`) et le futur CPU PowerPC ont des noms identiques (`Cpu`, `step`, `run`, `Status`, `AssemblyError`, `disassemble_range`). D-012 prévoyait de passer à un namespace par module à ce moment-là.
- **Options étudiées :** (A) tout dans `nerith` et préfixer les noms (`PpcCpu`...) ; (B) un namespace pour `ppc` seulement ; (C) un namespace par module, y compris `toy`.
- **Décision :** option B. Nouveau module `ppc` (décodeur, désassembleur, assembleur, interpréteur), dépendant de `memory`, dans le namespace `nerith::ppc`. `toy` reste dans `nerith` pour ne rien casser.
- **Règles de départ :** un mot inconnu ou une forme pas encore gérée (`Rc = 1`, `OE = 1`, 64 bits...) donne `Unknown` / `InvalidInstruction`, jamais une exécution approximative ; une instruction qui échoue ne modifie rien ; `blr` avec `lr == 0` termine le programme (convention de test, pas le comportement de la console) ; `rA = 0` vaut le nombre 0 dans `addi`, `addis`, `lwz`, `stw`, `lbz`, `stb`.
- **Vérification :** les encodages sont comparés à `llvm-mc` (assembleur LLVM) pendant le développement ; `llvm-mc` n'est pas une dépendance du dépôt. Le comportement (et pas seulement l'encodage) reste à vérifier avec le manuel PowerPC.
- **Conséquences :** le test de chaque instruction se fait à trois niveaux (décodage, assemblage, exécution) ; `toy` pourra passer dans `nerith::toy` plus tard ; D-006 reste « en cours ».


### D-014 — Module `xex` : lire l'en-tête seulement
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** un jeu Xbox 360 est un fichier XEX : un conteneur autour d'une image PE PowerPC, en général chiffré et compressé. Le dépôt ne contient et ne doit contenir ni jeu, ni clé, ni fichier propriétaire (`docs/conventions.md`).
- **Options étudiées :** (A) ne rien faire avant d'avoir tranché la question du chiffrement ; (B) lire seulement l'en-tête et la liste des en-têtes optionnels, sans rien déchiffrer ni décompresser ; (C) viser tout de suite le chargement complet.
- **Décision :** option B. Module `xex` dans le namespace `nerith::xex`, sans dépendance : il lit un tampon d'octets. Seul XEX2 est géré. Les tests utilisent des octets fabriqués à la main. L'option `nerith --xex fichier.xex` affiche l'en-tête d'un fichier donné par l'utilisateur et ne lit que les 64 premiers Kio.
- **Ce que la décision ne tranche pas :** le chiffrement et la compression du contenu. Cette question de fond (comment travailler sans distribuer de matériel protégé) reste ouverte et doit être décidée avant tout code qui touche au contenu.
- **Justification :** l'en-tête est la seule partie testable sans fichier de jeu ni clé ; elle suffit à répondre à plusieurs points « à vérifier » de la note de lecture (en-tête lisible sans clé ? tailles des en-têtes optionnels ?).
- **Points à vérifier :** valeur directe pour les identifiants dont l'octet de poids faible vaut 0x00 (hypothèse retenue pour le point d'entrée) ; sens exact du champ à 0x10 (« informations de sécurité »).
- **Conséquences :** `value_kind` et les noms des identifiants viennent de Free60 (source auto-qualifiée de spéculative) ; à recouper avant de s'y fier pour charger un programme.

### D-015 — Table d'instructions et vérification croisée avec llvm
- **Date :** 2026-11-14
- **État :** retenue, révisable
- **Contexte :** une centaine d'instructions entières à gérer. Écrire à la main, pour chacune, un cas dans le décodeur, un dans l'assembleur et un dans l'affichage répète beaucoup de code, et chaque répétition est une occasion de faute.
- **Options étudiées :** (A) continuer à la main ; (B) une table unique (code opération, nom, place des champs) lue par le décodeur, l'assembleur et l'affichage, la sémantique restant dans `cpu.cpp` ; (C) générer le code à partir d'un fichier de description avec un outil séparé.
- **Décision :** option B pour toutes les nouvelles instructions (`src/ppc/optable.cpp`) ; les anciennes restent écrites à la main et pourront migrer plus tard. Les rotations, les comparaisons sans signe et quelques formes spéciales restent à la main dans le décodeur.
- **Vérification :** chaque ligne de test est un cas assemblé par llvm-mc (mot de 32 bits) et désassemblé par llvm-mc (texte). llvm-mc sert d'outil de référence hors dépôt ; il n'est pas une dépendance du projet. Le comportement (ce que fait l'instruction) n'est pas vérifiable ainsi : les tests sont calculés à la main, et un programme complet (CRC32) est comparé aux valeurs connues.
- **Choix de comportement :** les formes avec point (`Rc = 1`) et avec débordement (`OE = 1`) restent refusées. Division par zéro et `INT_MIN / -1` : le manuel dit « indéfini », Nerith met 0. Résultats de 32 bits signés (`mulhw`, `divw`) : signe étendu sur 64 bits ; non signés (`mulhwu`, `divwu`) : complétés par des 0. Le décodeur tolère un bit réservé non nul dans `cmpwi` et `cmplw`, comme le matériel.
- **Retenue (XER) :** ajoutée dans un deuxième temps le même soir (voir le journal). XER garde les bits SO, OV, CA et le compteur d'octets ; seul CA est calculé pour l'instant, SO et OV ne changent que par `mtxer`, car les formes à débordement (`OE = 1`) ne sont pas gérées.
- **Registres et instructions ajoutés ensuite (le même soir) :** le registre de conditions (`mcrf`, `mfcr`, `mtcrf`, `cror`, `crxor`...), tous les sauts conditionnels (`bc` avec n'importe quel BO, `bclr`, `bcctr`, et leurs noms `beqlr`, `bdnzt`...), les verrous atomiques (`lwarx`, `ldarx`, `stwcx.`, `stdcx.`), `sc`, les barrières (`sync`, `lwsync`, `eieio`, `isync`) et les nombres à virgule flottante (32 `fN` de 64 bits, `FPSCR`, chargements, écritures, calculs en simple et double précision, conversions, comparaisons). Pour les flottants, la table a reçu de nouvelles dispositions (`FA3`, `FA4`, `FMemD`...) et un masque de « bits réservés » par disposition : un mot dont un champ inutilisé n'est pas à 0 est refusé.
- **Choix de comportement (flottants) :** un seul coeur et une mémoire immédiate, donc les barrières ne font rien et la réservation est un simple (adresse, vrai/faux) ; elle disparaît après `stwcx.`. `sc` rend la main avec `Status::SystemCall`, `pc` déjà sur l'instruction suivante. Les NaN : si une entrée est un NaN, il est propagé (rendu silencieux) ; sinon une opération invalide donne `0x7FF8000000000000`, comme PowerPC (et pas `0xFFF8...` comme x86). `fres` et `frsqrte` donnent le résultat exact au lieu de l'estimation du matériel. `fctiw` et `mffs` mettent `0xFFF80000` dans les 32 bits de poids fort (indéfinis d'après le manuel ; même convention que Dolphin). `fctiw` et `fctid` suivent le mode d'arrondi de FPSCR (bits 30-31).
- **Limites connues (flottants) :** les drapeaux d'exception de FPSCR (débordement, division par zéro, invalide, inexact, `FPRF`...) ne sont PAS calculés par les opérations, seulement le champ de comparaison (`FPCC`) après `fcmpu`/`fcmpo` ; `FEX` et `VX` ne sont jamais écrits. Les formes avec point (`fadd.`, qui écrit CR1) sont refusées. Pas de VMX128 (vecteurs), de `fcfids`, `frim`... ni de formes `OE = 1`.
- **Sauts :** `bclr` pris vers LR = 0 arrête le programme (convention des tests) seulement si ce n'est pas un appel (`LK = 0`) ; dans ce cas CTR et CR restent inchangés. Les 2 bits de poids faible de LR et CTR sont ignorés pour les sauts conditionnels (comme le matériel). Pour les noms, Nerith écrit `beq`, `bne`... là où llvm écrit `bt`, `bf` ; les mots sont identiques.
- **Caches et registres système (ajout du même soir) :** `dcbt`, `dcbtst`, `dcbst`, `dcbf`, `icbi` sont acceptés et ne font rien (pas de vrai cache) ; `dcbz` met à zéro un bloc de 32 octets aligné, et `dcbzl` (le `dcbz128` du Xenon, même mot) un bloc de 128 octets — tout ou rien si le bloc sort de la mémoire. `mfmsr`/`mtmsr`/`mtmsrd` avec le bit L (L = 1 : seuls EE et RI changent ; `mtmsr` sans L ne touche que les 32 bits de poids faible) ; le MSR est gardé mais n'a encore aucun effet sur l'exécution. SPR acceptés en plus de XER/LR/CTR : DEC (22), SRR0/1 (26/27), SPRG0-3 (272-275), PVR (287, lecture seule, valeur par défaut 0 — la valeur du vrai Xenon reste à trouver) et la base de temps (268/269 et `mftb`/`mftbu`, lecture seule). Horloge : la base de temps monte de 1 et DEC descend de 1 à chaque instruction exécutée (pas d'interruption du décompteur). Tout autre numéro de SPR est refusé, ainsi que `dcbt` avec indication TH et `dcbf` avec L non nul.
- **Points à vérifier :** le résultat réel de la console pour une division par zéro (peu probable dans du code sain) ; les valeurs exactes de `fctiw`/`mffs` dans les 32 bits de poids fort sur le vrai Xenon ; la valeur du PVR du Xenon.

## Format d'une décision

### D-XXX — Titre
- **Date :**
- **État :** proposée / retenue / remplacée
- **Contexte :**
- **Options étudiées :**
- **Décision :**
- **Justification :**
- **Conséquences :**
- **Sources :**

## Questions ouvertes
- Nom définitif : Nerith est provisoire, mais les vérifications (GitHub, domaines, marques) n'ont révélé aucun conflit ; Ondrel reste en rechange.
- Standard C++.
- Licence : BSD-3-Clause provisoire (D-009) ; à confirmer avant la première version publique.
- Langue du dépôt public : français pour le moment, anglais envisagé plus tard.
- API graphique.
- Compatibilité des versions système.
- Stratégie d'émulation des accessoires.
