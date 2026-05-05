# Sub3D — Jeux à analyser pour inspiration design

**Date** : 2026-05-05
**Contexte** : suite à l'analyse Barotrauma, le user veut récupérer Subnautica et veut des recos d'autres jeux à inspecter

## Méthode d'analyse

Pour chaque jeu : ce qu'on cherche, où le trouver, format des données accessibles, ROI estimé.

Critère d'éligibilité Sub3D : sub/sous-marin OU compartiments fluides OU coop multi-joueurs OU sim de pression/profondeur OU breach/damage.

## Tier 1 — analyse à faire (haut ROI)

### Subnautica / Subnautica Below Zero (le user récupère)

**Engine** : Unity (C# DLL, déchiffrable via dnSpy/ILSpy mais 2GB de code).
**Pertinence** : sous-marin (Cyclops, Seamoth), bases sous-marines avec compartiments, breach/flood en interne, pression à profondeur.

À chercher :
- **Cyclops flood mechanic** : comment le sub se remplit quand breach, gameplay damage control crew solo
- **Base building** : comment les compartments sont définis (rect-based comme Baro ou volumes 3D ?)
- **Hull integrity vs depth** : comment la pression endommage la structure
- **Water rendering** : Subnautica a une eau techniquement excellente (refraction, caustics) — pas notre cible visuelle (trop AAA) mais source d'inspiration shader
- **Echo/sonar** : différent de notre sonar mais worth checking pour comparaison

ROI : **HAUT**. Le plus proche de Sub3D côté setting. Si on trouve leur algorithme flood Cyclops c'est précieux.

Path attendu : `Steam/steamapps/common/Subnautica/Subnautica_Data/Managed/Assembly-CSharp.dll` → décompiler.
Easier inspection : `BuildingTiles/`, `recipes/`, `ScriptableObjects/`. Les `.asset` Unity sont semi-lisibles.

### Sea of Thieves (à acheter / fetch via mods communautaires)

**Engine** : UE4 (custom). Code propriétaire, mais GDC talks publiques.
**Pertinence** : **multiplayer coop avec ship flooding au cœur du gameplay**. Joueurs réparent breaches, écopent l'eau. Probablement la référence #1 sur la cible coop "fluide unique" qu'on cherche.

À chercher :
- **GDC talks** : "The Tech Behind The Water in Sea of Thieves" (cherchable). Algorithmes water rendering, propagation au-dessus du pont, dynamique avec mouvement du bateau.
- **Damage propagation** : per-deck flooding rate, breach repair mechanic.
- **Visual juice** : l'eau qui monte à l'intérieur du hull est lisible et juicy malgré la simplicité.

ROI : **TRÈS HAUT** pour la vision produit (coop + flood gameplay). Sources : YouTube GDC + articles dev blog.

Pas de fichiers à analyser localement (jeu propriétaire UE4 cooked) mais ça mérite 1h de visionnage GDC + lecture articles.

### Space Engineers

**Engine** : custom C#. **Code partiellement open-source** (`SpaceEngineers/Sources/` sur GitHub officiel Keen).
**Pertinence** : ship building avec pressurized rooms, oxygen sim, breach/damage interne, compartments détectés runtime depuis blocs 3D.

À chercher :
- **Room detection algorithm** : comment les compartments sont identifiés à partir d'une grille de blocs (flood-fill sur bloc connectivity ?). Très pertinent pour notre Phase 2 bake voxelisation — leur algo est probablement lisible directement.
- **Air/oxygen sim** : per-room air quantity, transfer through open doors/breaches
- **Hull breach** : quand un bloc est détruit, la pièce devient unsealed. Mécanisme proche de notre `OnBreachesUpdated`.

ROI : **HAUT** côté algorithm. Code open. Une session de 2-3h à lire leur source pourrait nous donner des idées concrètes pour le baker Phase 2.

GitHub : `https://github.com/KeenSoftwareHouse/SpaceEngineers` (vérifier la dernière version dispo, certains modules peuvent être fermés).

## Tier 2 — analyse opportuniste (ROI moyen)

### Stationeers

**Engine** : Unity.
**Pertinence** : atmospheric sim détaillée (pressure, temperature, gas mix), room-based.
**ROI moyen** : moins focus sub mais leur sim atmospherique est très détaillée. Si on veut un jour une vraie sim O2 + temperature pour Sub3D post-FP, c'est la référence.
Inspection : Unity ScriptableObjects décompilables.

### From the Depths

**Engine** : Unity.
**Pertinence** : **ship/sub building avec hull buoyancy précise + flooding compartments**. Joueurs construisent des bateaux avec des "blocks" puis ils se battent dans un sandbox.
**ROI moyen** : hull breach mechanics validées sur 10 ans. Voxel-based block placement, flood fill par compartment.
Inspection : Unity .NET decompile.

### UBOAT

**Engine** : Unity.
**Pertinence** : sub WW2 sim, **damage control gameplay très détaillé**, crew management.
**ROI moyen** : crew AI behavior dans compartments flooded, animation/comportement dans water = utile pour notre future procedural anim.
Inspection : Unity decompile.

### Oxygen Not Included

**Engine** : Unity.
**Pertinence** : 2D mais **leur fluid sim est la gold standard** pour propagation gas/liquid avec pressure. Algorithmes documentés dans GDC + articles dev.
**ROI moyen** : pas direct (2D vs 3D, fluid sim cellulaire) mais les concepts (pressure, mixing, equilibrium) sont educatifs. Si on bascule un jour de heightfield à shallow-water solver Phase 6, leur approche pourrait inspirer.
Bonne ressource : Klei a publié plusieurs articles tech.

## Tier 3 — Référence visuelle uniquement (ROI faible mais cool)

- **Iron Lung** : single-room sub horror. Inspirant pour UX, cockpit pressure feel, stations limitées. Pas de tech à reprendre.
- **We Need to Go Deeper** : 2D coop sub multijoueur. Cute, simple, exact même fantasy. À regarder pour le ressenti coop, pas la tech.
- **Cold Waters / Silent Hunter** : sims sub combat réalistes. Pertinent pour helm/sonar UX, pas pour flood.
- **Diluvion** : exploration sub 2.5D avec crewable ship. Inspirant pour composition d'écran de jeu (crew vs cockpit views).

## Tier 4 — Adjacent (à ignorer sauf curiosité)

- **Half-Life 2 / Source water** : tech ancienne, dépassée par UE5 native
- **Tomb Raider 2013** : flooding cinématique scripted, pas systémique
- **HITMAN** : interior fluid pas pertinent
- **The Witness** : water rendering esthétique pure, pas de gameplay

## Recommandation par priorité d'analyse

1. **Sea of Thieves GDC talks** (1-2h, gratuit) — probablement plus de signal que tout le reste cumulé pour la vision produit "coop water gameplay"
2. **Space Engineers source GitHub** (2-3h) — code open, room detection algorithm directement applicable à notre Phase 2 bake
3. **Subnautica decompile** (le user le récupère, 2-3h analysis) — Cyclops flood mechanic vaut le coup
4. **Oxygen Not Included articles** (1h lecture) — fluid sim concepts, optionnel
5. **Stationeers/UBOAT/From the Depths** : à faire si on a vraiment du temps post-FP

## À NE PAS faire

- Decompiler les binaires propriétaires non open (Sea of Thieves, Subnautica au-delà du raisonnable) → ROI/effort défavorable, watch GDC à la place
- Multiplier les analyses tier 4 → diminishing returns
- Confondre "inspiration design" et "audit complet" : on cherche des patterns conceptuels, pas à reproduire le code

## Quand re-faire ce genre de passe ?

- Avant chaque phase majeure du plan eau (Phase 2, 3, 4) si on doute de notre approche
- Avant d'attaquer la breach reaction loop post-FP (regarder Repairable Barotrauma + damage control UBOAT)
- Avant procedural crew animation (regarder UBOAT + Subnautica crew)
- Avant le système électrique post-FP (regarder Stationeers + Space Engineers + Barotrauma wired)
