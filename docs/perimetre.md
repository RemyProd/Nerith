[← Index de la documentation](README.md)

# Périmètre et principes

## Première plateforme
- Développement initial : Windows 10/11 64 bits.
- Matériel : PC de bureau (voir [Environnement](environnement.md)).
- Langage principal : C++.
- Build : CMake.
- Versionnement : Git et GitHub (dépôt public).

## Périmètre fonctionnel

### Émulation matérielle
- CPU Xenon / PowerPC.
- GPU Xenos.
- Mémoire et mappage d'adresses.
- Audio.
- Interruptions, timers et périphériques.
- Contrôleurs et entrées/sorties.

### Système
- Chargement des exécutables.
- Appels et services système.
- Stockage virtuel.
- Profils et données utilisateur.
- Versions du système et compatibilité.

### Expérience utilisateur
- Configuration de l'émulateur.
- Bibliothèque de contenus locaux.
- Dashboard et Xbox Guide à terme.
- Notifications, succès et profils.
- Gestion des manettes et accessoires.

## Principes d'ingénierie
1. Comprendre avant d'intégrer.
2. Séparer les composants en modules.
3. Commencer par des tests contrôlés.
4. Documenter décisions et limites.
5. Ne pas confondre interface ressemblante et émulation réelle.
6. Ne pas annoncer la compatibilité d'un jeu sans test.
7. Garder le projet compilable à chaque jalon.

## Recherche et propriété intellectuelle
- Lire les licences avant toute réutilisation de code.
- Attribuer correctement les travaux et références.
- Ne pas copier du code sous licence incompatible.
- Ne pas distribuer BIOS, firmware, clés, jeux, fichiers système ou ressources propriétaires Microsoft.
- Utiliser des contenus de test que l'on est autorisé à posséder et utiliser.
- Privilégier protocoles documentés et tests avec son propre matériel.

## Hors périmètre initial
- Compatibilité commerciale immédiate.
- Tous les accessoires dès la première version.
- Services Xbox Live officiels.
- Optimisation prématurée.
- Interface finale avant les fondations.
