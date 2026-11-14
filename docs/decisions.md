[← Index de la documentation](README.md)

# Décisions techniques

Conserver les décisions importantes et leur justification. Ne pas effacer une ancienne décision : la marquer comme remplacée et créer une nouvelle entrée.

## Registre

| ID | Sujet | État | Décision |
|---|---|---|---|
| D-001 | Plateforme initiale | Retenue | Windows |
| D-002 | Langage principal | Retenue | C++ |
| D-003 | Build | Proposée | CMake |
| D-004 | Dépôt | Retenue | GitHub public |
| D-005 | Nom | Provisoire | Nerith (remplace Project Xenon) |
| D-006 | Architecture | À définir | Modules indépendants |
| D-007 | Kernel cible | À étudier | Architecture multi-version ; le dernier kernel ne garantit pas tous les jeux |
| D-008 | API graphique | À étudier | Décider après recherche et prototype |
| D-009 | Licence | Provisoire | BSD-3-Clause (copyright : RemyProd) |

## Décisions détaillées

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
