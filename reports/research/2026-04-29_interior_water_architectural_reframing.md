---
title: Eau intérieure Sub3D — Reframing architectural et axes de R&D
date: 2026-04-29
status: open / in research
related:
  - reports/research/2026-04-29_interior_water_approaches_matrix.md
  - reports/guides/2026-04-24_water_cap_authoring_and_material_functions.md
  - memory/project_flood_containment_decision_2026_04_24.md
  - memory/project_water_material_post_fp.md
---

# Eau intérieure Sub3D — Reframing architectural et axes de R&D

## 0. Statut

Document de R&D. **Aucune solution arbitrée**. Aucun plan d'implémentation. Objectif : poser un diagnostic précis du système actuel, séparer proprement les couches qui ont été confondues, et lister les axes que tout candidat de remplacement devra arbitrer. La matrice de comparaison des approches vit dans le doc associé.

---

## 1. Vision globale

### 1.1 Le résultat visé

Le rendu d'eau intérieure de Sub3D est :

> **N volumes d'eau locaux**, chacun rattaché à un compartiment, exprimés dans le repère local du sous-marin, dont **le niveau est piloté par `USubFloodComponent`**, dont les **transitions visuelles aux portes et aux brèches sont des événements VFX explicites**, et dont la **dynamique de surface (vagues, pitch/roll, impacts) est un comportement géré au niveau du mesh ou du heightfield**, pas un problème de topologie.

Cette formulation est l'invariant. Toute architecture candidate s'évalue à sa capacité à la respecter sans triches structurelles.

### 1.2 Le contexte gameplay qui contraint

- Sub3D est un coop 16 joueurs. Le rendu peut être **100 % client-side** : les niveaux d'eau (`WaterLevelNormalized`, `WaterHeightCm`) sont déjà répliqués par `USubFloodComponent`.
- Pas de contrainte anti-cheat sur le visuel. Désynchronisations légères tolérées.
- L'objectif esthétique est **l'ambiance**, pas la précision physique. L'eau qui monte = game over imminent, ressenti viscéral.
- La Craniata est le sous-marin de FP. 4 à 6 compartiments cloisonnés, hauteur d'eau max ~200 cm.
- Le sous-marin est un `APawn` non-Chaos qui se déplace via `USubMovementComponent` (math pur, 60 Hz). Tout système d'eau qui suppose un acteur Chaos ou un monde statique est éliminé d'office.

---

## 2. Le bug fondamental n'est pas dans le shader

### 2.1 L'erreur architecturale

> Le système actuel demande au shader de **deviner à runtime** une **topologie qui est en réalité du contenu authored**.
>
> Le résultat est un système qui combat la résolution des Distance Fields, qui dépend de la qualité d'un raymarch, et qui est non-déterministe selon les conditions de scène.

### 2.2 Conséquence pratique

Tant que la topologie reste un problème shader, **aucune amélioration du shader ne corrigera durablement les fuites**. Augmenter la résolution des DF, ajouter des AABB, multiplier les tests de visibilité — tout cela patch des symptômes. Le bug n'est pas dans le shader ; il est dans la décision d'avoir mis la topologie dans le shader.

### 2.3 Symptômes observés

| Symptôme | Cause superficielle | Cause profonde |
|---|---|---|
| L'eau bave à travers les cloisons (mais pas tout le temps) | Résolution insuffisante du Global Distance Field pour des murs fins | Le shader essaie de reconstruire une géométrie qui existe déjà dans la coque |
| L'eau passe par la porte fermée | Le ray-march rate la cloison | La fermeture de porte n'est connue que dans `USubFloodComponent`, jamais propagée au rendu |
| La direction de la fuite varie selon l'angle de la caméra | Conditions de raymarch dépendantes de la scène | Bug structurel : le shader infère, ne lit pas |
| Bug Row2/Row3 dans `M_CompartmentWater` | Param mismatch C++/material | L'OBB n'aurait jamais été nécessaire si la géométrie portait la silhouette |
| Plan générique 80 m × 80 m | `PlaneWorldSizeCm = 8000.f` par défaut | Le plan ne correspond à aucune réalité spatiale, son existence force le shader à clipper |

---

## 3. Les quatre couches confondues aujourd'hui

| Couche | Question | Bonne réponse | Mauvaise réponse actuelle |
|---|---|---|---|
| **Simulation** | Combien d'eau dans chaque compartiment ? | `USubFloodComponent` (déjà en place, OK) | — |
| **Topologie** | Où sont les murs, portes, brèches ? | Données authoring + gameplay state | Distance Fields globaux + raymarching shader |
| **Représentation** | À quoi ressemble la surface ? | Mesh + matériau d'eau | Plan infini masqué par shader |
| **Événements** | Splash, jet de brèche, courant ? | VFX/Niagara + impulsions | Inexistant ou inclus dans le shader |

Le reframing de ce tableau est l'angle mort des itérations précédentes : on a traité les quatre lignes comme un seul système entrelacé, alors qu'elles devraient être quatre couches indépendantes communiquant par contrat.

### 3.1 Ce que le reframing dissout

- La **topologie** sortie du shader → plus de raymarch, plus de clipping inféré, plus de Row2/Row3.
- Les **événements** explicités → la fermeture de porte n'est plus un problème de shader mais un trigger VFX.
- La **représentation** assumée → on accepte qu'un mesh plat ne suffit pas, et on choisit en connaissance.
- La **simulation** intouchée → `USubFloodComponent` est l'autorité, point. Toute couche en aval lit, ne pousse pas.

### 3.2 Ce que le reframing impose

Chaque couche a un contrat clair vers les autres. La topologie doit être lisible **comme donnée** par la représentation et les événements (pas inférée). Les événements lisent la simulation pour savoir quand se déclencher (jet de brèche au moment où `BreachInflowLitersPerSec > 0`, ripple à l'ouverture de porte).

---

## 4. État du code Sub3D mappé aux 4 couches

### 4.1 Couche Simulation — solide

`USubFloodComponent` est server-authoritative et complet pour les besoins FP :

- `FFloodCompartmentState` par compartiment : `CompartmentId`, `CapacityLiters`, `CurrentWaterLiters`, `WaterLevelNormalized` (0..1), `WaterHeightCm`, `FloodRateIn/Out`, `bPumpActive`, `BreachInflowLitersPerSec`.
- `FFloodEdgeState` par porte/connexion : `ClosureId`, `VolumeA`, `VolumeB`, `bClosed`, `PassageAreaCm2`, `bExteriorEdge`.
- Réplication `OnRep_CompartmentStates` ; chaque client a la même vue logique du flood.
- Wiring porte→sim **déjà fait** : `ASubDoorActor::SetDoorClosed` appelle `OwningSubmarine->SubFlood->SetDoorState(DoorId, bClosed)` (cf. `SubDoorActor.cpp:202`).

**Aucune intervention requise sur cette couche pour résoudre le rendu.**

### 4.2 Couche Topologie — partiellement modélisée, mal exploitée

Ce qui existe :

- `UCompartmentVolumeComponent` : `BoxComponent` placé en BP, porte `CompartmentId` et `WaterPlaneMeshOverride`. Sa box donne la footprint XY approximative du compartiment.
- La coque (`HullMesh`) et les meshes interieurs (cloisons, planchers, plafonds) sont des `UStaticMeshComponent` enfants du sub. Ils portent leur géométrie réelle.
- Les portes (`ASubDoorActor`) connaissent `CompartmentA` et `CompartmentB`.

Ce qui manque pour que la topologie soit lisible par le rendu :

- **Pas de tag par compartiment sur la coque.** La coque ne sait pas qu'elle est l'enveloppe d'un compartiment plutôt que d'un autre. Custom Stencil par compartiment, par exemple, n'est pas exposé.
- **Pas de représentation de l'enveloppe XY+Z d'un compartiment** au-delà d'une box. Or les compartiments ont des sections qui varient avec Z (coque courbe, plafond incliné, équipements protrudants) — voir memory `project_flood_containment_decision_2026_04_24.md`.
- **Les portes ne portent aucun marqueur visuel** indiquant "ici, l'eau peut être continue" ou "ici, c'est une cloison étanche".
- **Les brèches ne sont localisées que par un point** (`BreachLocalCenter` dans `FCompartmentBreachState`) — pas de plan de coupe authored ni de hit-zone marquée.

### 4.3 Couche Représentation — la couche cassée

`UFloodWaterPlaneComponent` :

- Spawné dynamiquement par `ASubmarineBase::BeginPlay` (un par `CompartmentId`), parenté au sub.
- Lit `WaterLevelNormalized` et `WaterHeightCm` de son `SourceVolume` chaque tick.
- Crée un `UStaticMeshComponent` enfant à `BeginPlay` :
  - Si `SourceVolume->WaterPlaneMeshOverride` existe → utilisé à scale (1,1,1). Path "cap mesh".
  - Sinon → plan engine `BasicShapes/Plane` scalé à `PlaneWorldSizeCm = 8000.f` (80 m). **Path actuellement actif sur Craniata, et c'est lui qui fuit.**
- Pousse au MID : `WaterLevel01`, `WaterHeightCm`, `CV_Center_WS`, `CV_HalfExtent`, `CV_W2L_Row0..3`. Le matériau utilise ces valeurs pour tenter le clip OBB + le ray-march GDF.

`UFloodWaterVisualsComponent` (legacy) :

- Path alternatif, pas utilisé par Craniata. Faisait quelque chose de meilleur sur la stabilité : interpolation `CurrentLocalZ → TargetLocalZ` en repère local sub, padding `+50 cm` pour rendre à travers les portes (et masquait ailleurs avec la même logique GDF).
- À considérer comme **archive** : son code de gestion local-Z est une référence utile, mais sa logique de masking est la même impasse.

`M_CompartmentWater` + `MF_CompartmentWater_Containment` :

- Le matériau et sa material function font le ray-march GDF + OBB.
- Bug actif : param `CV_W2L_Row3` dans le matériau alors que le C++ pousse `CV_W2L_Row2` ; l'axe Z de l'OBB n'est pas réellement testé. Workaround temporaire : `MakeRow(2)` est dupliqué dans Row3 (`FloodWaterPlaneComponent.cpp:216`), mais c'est un patch.
- Quel que soit le futur choix de représentation, **toute cette logique de containment shader est candidate à la suppression**.

### 4.4 Couche Événements — quasi inexistante

- `BP_OnWaterLevelChanged` et `BP_OnVisibilityChanged` existent comme événements BP sur `UFloodWaterPlaneComponent` mais ne sont câblés à aucun VFX dans Craniata.
- `UBreachVfxManagerComponent` et `UDoorFloodVfxComponent` existent comme composants legacy (Proto02) mais ne sont **pas activés** sur Craniata.
- Aucun feedback explicite à l'ouverture/fermeture de porte côté visuel : la cloison apparaît étanche ou ne l'est pas, sans transition.
- Aucun jet de brèche : le breach se traduit aujourd'hui par une augmentation continue de `WaterHeightCm`, sans trace visuelle au point d'entrée.

C'est la couche **la plus pauvre** et pourtant celle qui porterait le plus l'ambiance dans le résultat final.

---

## 5. Les six axes à arbitrer

Une fois la topologie sortie du shader, le système se décompose en six décisions indépendantes (mais couplées). Chacune admet plusieurs réponses ; chacune a son propre tableau de candidats dans le doc matrice.

### Axe 1 — Représentation du volume d'eau

**Question.** Quelle géométrie/structure représente le volume d'eau d'un compartiment ?

**Famille de candidats (R&D).**
- Plan plat (cap mesh per-room)
- Volume mesh fermé suivant la coque
- Cube/box translucide englobante
- Heightfield 2D Niagara (UE 5.6+)
- Heightfield 2D compute custom
- Heightfield baked (Houdini) + playback paramétrique
- Render Target heightmap peint à la main
- Particle-based (FLIP/SPH local)

**Couplage critique.** Cet axe contraint fortement les axes 3 (dynamique de surface) et 4 (continuité aux portes). Un heightfield connecté entre compartiments réduit le besoin de raccord aux portes ; un volume mesh impose un VFX pour la transition.

### Axe 2 — Repère

**Question.** Dans quel repère la géométrie/le clip s'exprime-t-il ?

**Candidats.**
- Repère local sub (mesh parenté, clip en local Z)
- Repère monde (mesh parenté ou non, clip en world Z gravity-aligned)
- Hybride (mesh parenté en local, clip en world)

**Couplage.** Un repère local ne gère pas naturellement le pitch/roll du sub (la surface tilte avec le sub, ce qui est physiquement faux). Un repère monde le gère gratuitement mais peut introduire des problèmes de volume conservation si la valeur `WaterHeightCm` n'est pas recalculée par compartiment incliné. Le code actuel utilise un hybride : mesh parenté local, mais world Z pris depuis `GetWaterSurfaceWorldLocation()` — sans toutefois en tirer parti correctement (le plan ne prend que le Yaw du sub, pas le Pitch/Roll).

### Axe 3 — Dynamique de surface

**Question.** Comment la surface bouge-t-elle ?

**Candidats.**
- Statique (rien)
- Sine simple (oscillation temporelle uniforme)
- Gerstner waves vertex shader (faible amplitude pour intérieur)
- Slosh paramétrique 1D (ressort amorti excité par accélération sub) — réf. From The Depths
- Slosh paramétrique 2D (offset Z + tilt)
- Heightfield simulé (Niagara Fluids 2D ou compute) avec ondes émergentes
- Gerstner FFT (overkill ici)

**Couplage.** La dynamique doit être cohérente avec le repère. Slosh paramétrique a besoin de l'accélération sub-locale ; heightfield a besoin de conditions aux bords stables.

### Axe 4 — Continuité visuelle aux portes

**Question.** Quand deux compartiments adjacents sont connectés par une porte ouverte, comment le rendu réagit-il ?

**Candidats.**
- Aucune continuité (chaque compartiment ferme sa lame d'eau au plan de porte) — le plus simple, ressemble à un bug
- Stencil portal (la porte ouverte écrit un stencil partagé qui autorise la lame voisine à rendre)
- VFX explicite (cascade Niagara à l'ouverture, transition visuelle stylisée)
- Heightfield connecté topologiquement (surface unique sur deux compartiments)
- Raccord stylisé par seuil de porte (cap mesh étendu de quelques cm à travers la porte, masqué par le mesh de porte)

**Couplage.** Cet axe consomme directement la donnée gameplay `FFloodEdgeState.bClosed`. Il dépend aussi de l'axe 1 : un heightfield Niagara connecté est un choix structurellement différent de N volumes meshes indépendants.

### Axe 5 — Brèches

**Question.** Comment le visuel d'une brèche se déclenche-t-il et perdure-t-il ?

**Candidats.**
- Source ponctuelle Niagara (jet d'eau émis depuis `BreachLocalCenter`)
- Déformation locale du heightfield (impulsion à la position de la brèche)
- Burst FLIP/SPH les premières secondes, fade vers représentation steady
- VFX-only sans modification de la représentation principale (jet + ripples sur le plan)
- Mix : jet Niagara au point d'entrée + niveau d'eau qui monte via la simulation, sans coupling shader

**Couplage.** L'événement de brèche existe dans la simulation (`USubFloodComponent::CreateBreach`). La couche événement doit s'y abonner — mécanisme à concevoir, n'existe pas aujourd'hui pour Craniata.

### Axe 6 — Vue sous l'eau

**Question.** Que voit le joueur quand sa caméra est immergée ?

**Candidats.**
- Post-process volume confiné par compartiment (`APostProcessVolume` enfant du sub, top clippé à `WaterHeightCm`)
- Post-process fullscreen + masking par stencil
- Substrate slab d'eau (épaisseur perçue selon profondeur traversée)
- Volumetric fog densifié dans le compartiment (densité ↗ avec niveau d'eau)
- Decals caustique projetés depuis la waterline
- Audio-only (LowPass + reverb humide, pas d'effet visuel) — cf. Iron Lung principle

**Couplage.** Cet axe est largement orthogonal aux cinq autres. Un post-process volume ne dépend pas de la représentation choisie pour la surface ; il dépend uniquement de la donnée `WaterHeightCm` et de la position du compartiment dans le sub. Conséquence : cet axe peut être implémenté indépendamment, et son retour sur ambiance est très élevé pour un coût d'implémentation réduit.

---

## 6. Couplages entre axes

Tous les axes ne sont pas indépendants. Voici les couplages forts à connaître pour ne pas faire de choix incohérents.

| Axe contraint | Par | Comment |
|---|---|---|
| Axe 3 (dynamique) | Axe 1 (représentation) | Heightfield → ondulations émergentes "pour rien" ; volume mesh fermé → dynamique uniquement par offset/tilt du clip ; plan plat → dynamique par vertex displacement |
| Axe 4 (portes) | Axe 1 (représentation) | Heightfield connecté topologiquement résout le problème par construction ; meshes per-room nécessitent un raccord explicite |
| Axe 4 (portes) | Axe 5 (brèches) | Si un VFX explicite gère les portes, le même type de VFX gère naturellement les brèches (source d'événements similaire) |
| Axe 2 (repère) | Axe 3 (dynamique) | Slosh paramétrique a besoin de l'accélération du sub, donc lit le repère local ; gravity-alignement fonctionne en world |
| Axe 6 (sous l'eau) | Aucun fort | Découplable, peut être prototype indépendamment |

---

## 7. Critères d'évaluation transverses

Pour chaque candidat de chaque axe, le doc matrice évalue selon :

1. **Coût GPU steady-state** — le sub étant possiblement à distance, multiplié par N subs visibles
2. **Coût GPU transitoire** — au moment d'un événement (breach, porte qui claque)
3. **Fidélité visuelle steady** — eau au repos, niveau stable
4. **Fidélité visuelle dynamique** — eau qui monte, qui balance, qui réagit
5. **Complexité d'implémentation initiale** — heures/jours à un premier prototype fonctionnel
6. **Complexité d'authoring** — coût récurrent côté Blender/data quand un compartiment change
7. **Robustesse au repère mobile** — sub qui bouge à 60 Hz
8. **Robustesse aux mouvements brusques** — collision, descente rapide, pitch/roll extrême
9. **Compatibilité avec la simulation existante** — lecture de `WaterHeightCm` / `bClosed`
10. **Maintenabilité long-terme** — qui s'occupera de ce module dans 6 mois
11. **Rollback-ability** — si l'approche échoue, combien de temps pour revenir en arrière
12. **Capacité ambiance "game over imminent"** — l'objectif esthétique réel

---

## 8. Hors scope de ce reframing

Ce document **ne** traite **pas** :

- La simulation de flooding elle-même (`USubFloodComponent` est l'autorité, intouchée).
- Le mouvement du sous-marin et le repère du crew (axes orthogonaux, déjà résolus dans `2026-04-21_local_grid_space_authority_architecture.md`).
- Les performances réseau (le rendu peut être 100 % client-local).
- Les futures features post-FP (oxygen, life support, multi-sub) qui pourraient consommer la couche eau plus tard.

---

## 9. Annexe A — Bugs identifiés sur le système actuel

Pour traçabilité, à supprimer si on quitte ce système.

| Bug | Fichier | Description |
|---|---|---|
| Plan générique 80 m × 80 m | `FloodWaterPlaneComponent.h:51` | `PlaneWorldSizeCm = 8000.f` — sert de fallback quand `WaterPlaneMeshOverride` n'est pas assigné (cas Craniata actuel). Ne correspond à aucune réalité physique du compartiment. |
| Bug de mismatch Row2/Row3 | `FloodWaterPlaneComponent.cpp:216`, `M_CompartmentWater` | Le matériau attend `CV_W2L_Row3`, le C++ pousse `CV_W2L_Row2` ; l'axe Z de l'OBB n'est pas réellement testé. Workaround actif : duplication. |
| Ray-march GDF non fiable pour cloisons fines | `MF_CompartmentWater_Containment` (Custom HLSL) | `GetDistanceToNearestSurfaceGlobal` a une résolution minimale (~4 cm) et un seuil (~5 cm) inadaptés aux cloisons typiques d'un sous-marin. |
| Aucune transmission gameplay→rendu pour l'état des portes | absence | `bClosed` côté `FFloodEdgeState` n'est jamais consommé visuellement. |
| Aucun feedback de brèche | absence (ou Proto02 désactivé) | Une brèche n'a aucune signature visuelle au point d'entrée. |
| `UFloodWaterVisualsComponent` legacy actif sur la classe | `SubmarineBase.h:89` | Composant pas désactivé sur Craniata, peut prêter à confusion lors de la lecture. |

## 10. Annexe B — Contraintes Sub3D rappelées

Pour quiconque arrive sur ce dossier sans contexte préalable :

- UE 5.7 Windows uniquement.
- Sous-marin = `APawn` non-Chaos, math 60 Hz.
- 4-6 compartiments par sub Craniata, hauteur d'eau max ~200 cm.
- Coop 16 joueurs, rendu peut être client-local.
- Dépendances `USubFloodComponent` server-authoritative, déjà répliqué.
- BP de production = `BP_Submarine_Craniata`, handmade, pas de pipeline runtime generator actif.
- Wiring porte→flood déjà fait (`ASubDoorActor::SetDoorClosed`).
- Wiring breach→flood déjà fait (`USubFloodComponent::CreateBreach`).

---

## 11. Conclusion provisoire

Ce reframing pose le diagnostic. Le système échoue parce qu'il a confondu quatre couches qui auraient dû rester indépendantes ; le shader est le symptôme, pas la cause. La sortie de la topologie hors du shader rend le problème adressable.

Les six axes définis ci-dessus sont les vraies décisions à prendre. Le doc associé `2026-04-29_interior_water_approaches_matrix.md` recense les candidats par axe avec leur évaluation multi-critères, sans recommandation finale.

L'arbitrage final reste à faire, en R&D ouverte.
