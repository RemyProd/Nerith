[← Index de la documentation](README.md)

# Vision et objectifs

## Vision
Développer un émulateur Xbox 360 indépendant pour PC Windows. Le logiciel devra, à terme, démarrer un environnement système Xbox 360, exécuter des logiciels et jeux compatibles et proposer une expérience proche de la console.

Le cœur du projet est l'émulation matérielle et logicielle, pas seulement la reproduction du Dashboard.

## Objectifs à long terme
- Émuler le processeur Xenon (PowerPC).
- Émuler le GPU ATI Xenos et ses particularités.
- Reproduire les comportements mémoire et périphériques nécessaires.
- Implémenter les interfaces et services du système Xbox 360.
- Charger et exécuter des programmes compatibles.
- Gérer plusieurs versions système/kernel si nécessaire.
- Recréer progressivement Dashboard et Xbox Guide.
- Gérer profils locaux, avatars, Gamerscore, succès et sauvegardes.
- Prendre en charge manettes et accessoires compatibles.
- Construire une suite de tests et une base de compatibilité reproductible.

## Ce que signifie « complet »
1. Démarrer l'application.
2. Initialiser les composants virtuels.
3. Exécuter des programmes de test.
4. Démarrer un environnement système.
5. Afficher un Dashboard fonctionnel.
6. Exécuter des jeux.
7. Améliorer compatibilité et performances.
8. Gérer les périphériques et accessoires.

Chaque niveau dépend des précédents et peut demander un effort important.

## Compatibilité des kernels
Ne pas supposer que le dernier kernel rend automatiquement tous les jeux compatibles. Prévoir une architecture permettant de documenter les versions système, les exigences des titres et leurs différences. Voir [Décisions techniques](decisions.md) (D-007).

## Critères de réussite
- Code compréhensible et maintenable.
- Comportements testés.
- Progrès mesurables et documentés.
- Aucun fichier propriétaire redistribué.
- Compatibilité annoncée uniquement après tests.
- Architecture évolutive.
