[← Index de la documentation](README.md)

# Feuille de route

Les durées ne sont pas garanties. Pour une personne seule débutant en C++, ce projet peut s'étendre sur plusieurs années.

## Phase 0 — Préparation (sans coder)
- [ ] Choisir le nom définitif (provisoire : Nerith, voir [Décisions techniques](decisions.md)).
- [x] Préparer l'environnement de développement ([Environnement](environnement.md)).
- [x] Installer les outils (Visual Studio Community, CMake, Git).
- [x] Créer le dépôt Git et GitHub (licence BSD-3-Clause, README, `.gitignore`).
- [x] Documenter le projet (dossier `docs/`).

**Livrable :** environnement prêt et projet documenté.

## Phase 1 — Bases C++
Détail du parcours : [Apprentissage C++](apprentissage-cpp.md).
- [ ] Types, variables et opérateurs.
- [ ] Conditions et boucles.
- [ ] Fonctions.
- [ ] Tableaux, `std::array`, `std::vector`.
- [ ] Références et pointeurs.
- [ ] Structures et classes.
- [ ] Durée de vie et gestion mémoire.
- [ ] Fichiers et flux.
- [ ] Débogueur Visual Studio.
- [ ] Compilation et warnings.

**Livrable :** petits programmes compris et maîtrisés.

## Phase 2 — CPU pédagogique
Spécification : [Premier exercice](premier-exercice-cpu.md).
- [ ] Registres.
- [ ] Mémoire virtuelle bornée.
- [ ] Jeu d'instructions minimal.
- [ ] Compteur de programme.
- [ ] Boucle d'exécution.
- [ ] Arrêt et instructions invalides.
- [ ] Tests automatisés.

**Livrable :** petit CPU pédagogique fonctionnel.

## Phase 3 — Architecture du projet réel
Voir [Architecture](architecture.md).
- [ ] Interfaces entre modules.
- [ ] CMake.
- [ ] Tests.
- [ ] Logs et diagnostics.
- [ ] Conventions de code.
- [ ] Documentation des décisions.

## Phase 4 — CPU Xbox 360
- [ ] Étudier PowerPC/Xenon.
- [ ] Étudier registres et modes d'exécution.
- [ ] Étudier les instructions nécessaires.
- [ ] Concevoir un interpréteur de référence.
- [ ] Tester chaque instruction.
- [ ] Étudier les extensions vectorielles.
- [ ] Évaluer ultérieurement un JIT.

## Phase 5 — Mémoire et périphériques
- [ ] Modéliser les espaces d'adressage.
- [ ] Implémenter les accès mémoire vérifiés.
- [ ] Étudier interruptions et timers.
- [ ] Définir les interfaces de périphériques.

## Phase 6 — GPU et audio
- [ ] Étudier ATI Xenos.
- [ ] Comprendre commandes graphiques et shaders.
- [ ] Étudier mémoire vidéo et eDRAM.
- [ ] Construire des tests graphiques.
- [ ] Étudier formats et chemins audio.

## Phase 7 — Chargement et système
- [ ] Étudier les formats exécutables Xbox 360.
- [ ] Définir un chargeur de programmes de test.
- [ ] Étudier appels et services système.
- [ ] Documenter les versions de kernel.
- [ ] Définir le stockage virtuel.

## Phase 8 — Interface console
- [ ] Construire une interface de diagnostic.
- [ ] Créer la configuration.
- [ ] Ajouter les profils locaux.
- [ ] Étudier la reproduction du Dashboard.
- [ ] Ajouter la navigation manette.

## Phase 9 — Accessoires
- [ ] Définir une couche d'abstraction des entrées.
- [ ] Prendre en charge les manettes standard.
- [ ] Étudier les récepteurs sans fil.
- [ ] Étudier les périphériques USB spécialisés.
- [ ] Évaluer séparément Kinect, casques, volants et accessoires musicaux.

## Phase 10 — Compatibilité et optimisation
- [ ] Construire une suite de tests.
- [ ] Définir une fiche de test reproductible par jeu.
- [ ] Corriger les écarts.
- [ ] Mesurer les performances.
- [ ] Optimiser après profilage.
- [ ] Publier des résultats reproductibles.

## Critère de passage
Ne pas passer à la phase suivante simplement parce que le code compile. Il faut comprendre le fonctionnement, reproduire les tests et documenter les limites.
