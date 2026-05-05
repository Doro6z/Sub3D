# Sub3D Water Implementation Plan

**Date** : 2026-05-04
**Auteur** : Claude (analyse code-en-main, post-conversation externe)
**Statut** : décisions D1-D7 résolues, contrat de porte intégré, prêt pour exécution
**Documents source** :
- `reports/research/Sub3D_Water_Conversation_Notes (2).md` (proposition externe)
- `reports/research/Sub3D_Water_Architecture (1).md` (proposition externe)
- `reports/research/deep-research-report water.md` (panorama d'approches)
- `reports/research/2026-04-29_interior_water_approaches_matrix.md`
- `reports/research/2026-04-29_interior_water_architectural_reframing.md`
- `reports/2026-05-02_water_proto_audit.md` (audit du proto)

**Tests proto associés** : `reports/tests/2026-05-04_water_proto_minitests.md` (mini-tests à exécuter dans le proto avant portage Phase 3-4)

---

## 1. Cadre conceptuel et analyse

### 1.1 Constat fondamental — la proposition externe est un quasi-doublon

L'agent externe a proposé `USubmarineWaterManager`, `UCompartmentWaterRenderer`, `USubmarineWaterDef`. **Ces trois composants existent déjà** sous une forme proche dans le code Sub3D, mais l'agent externe ne le savait pas (pas d'accès au repo).

| Proposition externe | Réalité dans le code | Référence |
|---|---|---|
| `UCompartmentWaterRenderer` (per-compartment renderer) | `UFloodWaterPlaneComponent` (per-compartment, dynamic spawn) | [FloodWaterPlaneComponent.h:27](../../Source/Sub3D/Submarine/FloodWaterPlaneComponent.h#L27), spawn à [SubmarineBase.cpp:426-444](../../Source/Sub3D/Submarine/SubmarineBase.cpp#L426-L444) |
| `USubmarineWaterManager` (orchestrator central) | `UFloodWaterVisualsComponent` (legacy doublon) + `ASubmarineBase::BeginPlay` orchestre déjà | [SubmarineBase.cpp:217](../../Source/Sub3D/Submarine/SubmarineBase.cpp#L217), [FloodWaterVisualsComponent.h](../../Source/Sub3D/Submarine/FloodWaterVisualsComponent.h) |
| `USubmarineWaterDef` (UDataAsset topologie) | `USubmarineDefinition` (DA complet, validé Craniata) | [SubmarineDefinition.h:15-121](../../Source/Sub3D/Submarine/Generator/SubmarineDefinition.h#L15-L121) |
| `M_CompartmentWater` (à créer) | `M_CompartmentWater` + `MI_CompartmentWater` + 3 `MF_*` (existent) | `Content/Sub3D/Material/M_CompartmentWater.uasset` |

**Conclusion** : créer ces composants serait du churn. La bonne approche est **d'étendre l'existant et supprimer le doublon legacy**.

### 1.2 État réel découvert

1. **Double rendering simultané** : `UFloodWaterVisualsComponent` (legacy) ET `UFloodWaterPlaneComponent` (moderne) tournent en parallèle aujourd'hui sur Craniata. Source probable de Z-fighting et du sentiment "matériau pas utilisé" ([FloodWaterPlaneComponent.cpp:138](../../Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp#L138) warning).
2. **Init flood path inversé par rapport à la vision** : à [SubmarineBase.cpp:295-331](../../Source/Sub3D/Submarine/SubmarineBase.cpp#L295-L331), priorité = volumes BP > DA. Vision humaine = "DA source unique".
3. **Plan plat sans dynamique** : `UFloodWaterPlaneComponent` rend un mesh plat, pas d'ondes, pas de slosh, pas de continuité aux portes. **Le scénario A↔B des fondamentaux est intenable** avec ça.
4. **`USubFloodComponent` calcule déjà les flow rates** par edge dans `AdvanceFlooding` ([SubFloodComponent.cpp:619-641](../../Source/Sub3D/Submarine/SubFloodComponent.cpp#L619-L641)) mais ne les expose pas.
5. **`USubHullComponent::OnBreachesUpdated`** ([SubHullComponent.h:120-127](../../Source/Sub3D/Submarine/SubHullComponent.h#L120-L127)) est répliqué et prêt — c'est le hook propre pour hull breaking futur.
6. **Proto chargé en runtime par défaut** ([Sub3D.uproject:43-46](../../Sub3D.uproject#L43-L46)).

### 1.3 Direction validée

Étendre `UFloodWaterPlaneComponent` (déjà la prod) avec heightfield CPU + slosh modal + cap mesh authored (issus du proto). Supprimer `UFloodWaterVisualsComponent`. Ajouter `USubmarineDoorWaterCoordinator` pour la sync au bord aux portes horizontales. Le DA `USubmarineDefinition` reste source de vérité topologie, étendu avec `TMap<FName, UCompartmentWaterBake*> WaterBakes`. Hull breaking branche sur `OnBreachesUpdated` sans modification de l'archi.

### 1.4 Position sur les 14 fondamentaux

Tous tenus (cf. table v1 du plan, conservée). Les points 5, 6, 7, 8, 9, 10, 11 contraignent directement l'archi. Les points 1-4 contraignent le rendu. Aucun n'est sacrifié.

---

## 2. Contrat géométrique de porte (NEW — DA-level invariant)

### 2.1 Statut : convention recommandée, pas correctness requirement

**Le contrat est une optimisation de qualité, pas une obligation de correction.** Le système de bake et le coordinator runtime fonctionnent sur **n'importe quelle géométrie de porte**, mais sont :
- **Plus rapides et plus précis** sur portes conformes (bijection 1:1 triviale)
- **Plus coûteux et avec pairing approximatif** sur portes non-conformes (algorithme général)

Cf. P4.1 pour les deux algorithmes (rapide-conforme et général-fallback). Le contrat est validé en éditeur via `IsValid()` avec warning, jamais blocking.

### 2.2 Énoncé

Une porte (`FGeneratedConnectionDef` avec `ConnectionType==Door`) **devrait** respecter le contrat suivant :

1. **Volume de porte contenu dans l'épaisseur du mur** : `DoorWidthCm × DoorHeightCm × DoorDepthCm` ⊂ wall footprint.
   - Exemple : door 8cm de large × 200cm de haut × 80cm de profondeur dans un mur 15cm d'épaisseur (X) × 240cm de haut (Z) × 100cm de longueur (Y).
2. **Clearance de chaque face du mur** : ~3cm minimum entre la face de la porte et la face du mur (intérieur ET extérieur du mur).
3. **Axe de la porte perpendiculaire au plan du mur** : `LocalTransform.GetUpVector()` ou un de ses axes locaux est aligné avec la normale du mur (tolerance ±5°).
4. **Centre de la porte sur le plan du mur** : `LocalTransform.GetLocation()` est sur le plan séparant `Compartments[A].HydroBoundsMin/Max` et `Compartments[B].HydroBoundsMin/Max` (tolerance = wall thickness / 2).
5. **Wall thickness > door depth** : invariant pour garantir clearance.

### 2.3 Conséquences architecturales (porte conforme — fast path)

- **Bake trivial des cellules frontière** : cell_A[i] (côté compartiment A, adjacente au plan du mur, sous le footprint de porte) et cell_B[i] (mirror côté B) sont en bijection 1:1. Pas de logique segment-intersection skew, pas de raycast, juste projection rectangulaire sur la grille heightfield.
- **Sync runtime O(N_DoorCells) sans spatial query** : indices pré-calculés au bake, simple loop linéaire au tick.
- **Validation DA-time** : `USubmarineDefinition::IsValid()` détecte une porte non-conforme avec warning (pas blocking).
- **Authoring discipliné** : si l'humain place une porte qui déborde du mur, l'éditeur le signale visuellement avant le bake.

### 2.4 Conséquences architecturales (porte non-conforme — fallback path)

Si une porte ne respecte pas le contrat (forme libre, asymétrique, traversante diagonale, etc.) :

- **Bake en mode général** :
  - Pour chaque cellule de la grille du compartiment A : si centre dans rayon `MaxDoorExtent/2` autour de `LocalTransform.GetLocation()` ET du côté de A par rapport au plan du mur → ajout à `CellIndicesA`
  - Idem côté B
  - Pairing : pour chaque cellule de A, trouver la cellule de B la plus proche (projection sur le plan du mur). Si `Num(A) ≠ Num(B)` : pairing nearest-neighbor avec poids inversement proportionnel à la distance. Stockage de la liste de paires dans `FDoorBoundaryCells`.
  - Coût : O(NumCellsA × NumCellsB) au bake — toujours négligeable (<100 cells par porte).
- **Sync runtime identique** : le coordinator parcourt les paires pré-baked sans savoir si la porte est conforme ou pas. Pas de coût runtime supplémentaire.

**Garantie de fonctionnement** : que la porte soit un rectangle propre, un trou organique, ou même un breach runtime, l'algorithme fournit toujours un pairing valide. Cette robustesse est la raison pour laquelle le contrat est un *recommandé*, pas un *requis*.

### 2.5 Implications sur les structs existants

Modification de `FGeneratedConnectionDef` ([SubmarineDefinitionTypes.h:98-133](../../Source/Sub3D/Submarine/Generator/SubmarineDefinitionTypes.h#L98-L133)) :

- Ajout `float DoorDepthCm = 8.f` (épaisseur de la porte dans la direction perpendiculaire au mur). Default 8cm.
- Documentation mise à jour : `DoorWidthCm` = largeur dans le plan du mur (perpendiculaire à `DoorDepthCm`), `DoorHeightCm` = hauteur (Z local).
- `LocalTransform.GetForwardVector()` = normale du mur (par convention).

Modification de `USubmarineDefinition` :

- Ajout `float WallThicknessCm = 15.f` au niveau DA (déjà existant à [SubmarineDefinition.h:42](../../Source/Sub3D/Submarine/Generator/SubmarineDefinition.h#L42)). Confirmer la valeur.
- Ajout dans `IsValid()` : pour chaque connection `Door`, valider le contrat (1)-(5). Warning si non-conforme.

### 2.6 Cas spéciaux et exclusions

- `ConnectionType==Hatch` (verticale entre decks) : **pas soumise au contrat porte**. La sync verticale n'est pas implémentée (fondamental #6). Le bake calcule un opening vertical comme dans le proto actuel.
- `ConnectionType==ExteriorHatch` (airlock vers ocean) : pas soumise au contrat (pas de sync inter-compartiment).
- `ConnectionType==Open` (toujours ouverte, pas de porte physique) : si entre deux compartiments avec mur, **soumise au contrat** mais avec `bStartsClosed=false` permanent — le mur est traité comme une porte toujours ouverte.

### 2.5 Outil d'audit

Editor Utility : `EUW_AuditSubmarineDoorContract`
- Itère toutes les `Door` connections d'une `USubmarineDefinition`
- Pour chaque, dessine en debug viewport :
  - Wall plane (blue) avec normal
  - Door extent box (green si conforme, red si non)
  - Clearance arrows (yellow if < threshold)
- Liste textuelle des violations
- Bouton "Auto-fix offset" (clamp door position au plan du mur, redimensionne si déborde)

Phase d'apparition : Phase 2 (avec le baker). Pas critique pour Phase 1.

---

## 3. Architecture recommandée

### 3.1 Vue d'ensemble

```
USubmarineDefinition (DA — source de vérité topologie)
   │
   ├─ Compartments[].HydroBoundsMin/Max + WalkableFloorZCm + MaxWaterHeightCm
   ├─ Connections[].LocalTransform + DoorWidthCm + DoorHeightCm + [NEW] DoorDepthCm
   ├─ FloodGraph.Edges[].PassageAreaCm2
   ├─ [NEW] WaterBakes : TMap<FName CompartmentId, TObjectPtr<UCompartmentWaterBake>>
   └─ IsValid() — [NEW] valide le contrat de porte
        │
        ▼
USubFloodComponent (existant, server-auth flood sim)
   │   FCompartmentState[]      replicated (niveaux)
   │   FFloodEdgeState[]        replicated (bClosed, area, [NEW] CurrentFlowRateLitersPerSec)
   │   OnFloodStateUpdated      delegate (server tick beat)
   │
   ▼
ASubmarineBase::BeginPlay
   │   1. InitializeFromDefinition (priorité 1, fallback volumes BP) [Phase 1]
   │   2. EnsureCompartmentVolumesFromDefinition() [NEW — Phase 1]
   │   3. SpawnWaterRenderingFromDefinition() [refactor de l'existant — Phase 2]
   │
   ▼
UFloodWaterPlaneComponent (existant — ÉTENDU)
   │   Per compartiment :
   │   ├─ CapMesh = WaterBakes[CompartmentId]->CapMesh [NEW Phase 2]
   │   ├─ CPU heightfield 2D + R32F texture push [NEW Phase 3]
   │   ├─ Slosh modal 2D (offset Z + tilt) excité par SubMovement->Acceleration [NEW Phase 3]
   │   ├─ MID (MI_CompartmentWater) — params : OBB clip (existant), WaterHeightTex (NEW), Gerstner WPO (NEW)
   │   └─ InjectAt(WorldPoint, Strength, Radius) [NEW Phase 3]
   │
USubmarineDoorWaterCoordinator (NOUVEAU — Phase 4)
   │   Tick prereq : SubFlood
   │   Pour chaque Door connection ouverte (et runtime breach edge — Phase 5) :
   │   └─ Sync cellules frontière A↔B (Heights forte, Velocities faible)
   │
USubHullComponent (existant)
   │   BreachClusters[] replicated
   │   OnBreachesUpdated delegate
   │   ↓ (Phase 5)
   ▼
ASubmarineBase::OnBreachesUpdated [NEW handler]
   │   Pour chaque breach :
   │   ├─ Find compartiment touché (LocalCenter ∈ HydroBounds)
   │   ├─ Renderer génère skirt runtime + injection heightfield continue
   │   └─ Si breach inter-compartiment : ajout à la liste runtime edges du Coordinator
```

### 3.2 Composants — tableau de décisions

| Composant | Décision | Rationale |
|---|---|---|
| `UFloodWaterPlaneComponent` | **ÉTENDRE** : heightfield + slosh + cap mesh | Déjà la prod. |
| `UFloodWaterVisualsComponent` | **SUPPRIMER** | Doublon legacy. |
| `UDoorFloodVfxComponent` | **GARDER + alimenter par flow rate exposé** | Cascade Niagara existante OK. |
| `UBreachVfxManagerComponent` | **GARDER + ajout injection heightfield** | Jet Niagara existant OK. |
| `USubHullBoundaryComponent` | **PAS RÉUTILISÉ pour eau** | Crew EVA seulement. |
| `USubmarineDoorWaterCoordinator` | **NOUVEAU** | Sync au bord, ~150 lignes. |
| `URoomWaterRenderer` | **PORTER LA LOGIQUE dans `UFloodWaterPlaneComponent`** | Pas de nouveau composant. |
| `URoomWaterBakerLibrary` | **PORTER + RENOMMER `USubmarineWaterBakerLibrary`** | Editor-only, dans nouveau module. |
| `URoomWaterBakedData` | **PORTER + RENOMMER `UCompartmentWaterBake`** | Per-compartment bake DA. |
| `UDoorWaterBridge` | **JETER** | Logique redistribuée. |
| `ARoomActor` | **JETER** | Test container proto. |
| `M_Phase0_Test`, `M_WaterProto_Skirt` | **GARDER tant que proto activable A/B**, retirer du runtime au commit final | Pas la prod. |
| `M_CompartmentWater` | **ÉTENDRE** : sample texture R32F + WPO + Gerstner | Bon point de départ. |
| `M_FloodWater_DF` (Proto04C) | **JETER** | Anti-pattern Distance Field. |

### 3.3 Pourquoi sync au bord et pas heightfield-connected pur

Heightfield-connected pur exige bijection topologique entre les grilles de A et B. Sur Craniata avec le contrat de porte, c'est faisable mais fragile (changement de cell size = recalcul). Le sync au bord par moyenne pondérée est :

- **Suffisant pour le ressenti fluide unique** (point 1 des fondamentaux) : à l'équilibre, les deux surfaces convergent au même Z, ondes traversent la frontière par moyenne.
- **Robuste à des changements de grille indépendants** (compartiment A à 25cm, compartiment B à 30cm = OK, mismatch absorbé par la moyenne).
- **Compatible runtime topology delta** (breach inter-compartiment se branche au coordinator avec les mêmes structures).

### 3.4 Hull breaking — comment l'archi l'accueille

Détails dans Phase 5. Le principe : breach = "porte runtime additionnelle". L'archi traite Door DA et breach runtime via la **même boucle** dans `USubmarineDoorWaterCoordinator`. Le découplage `OnBreachesUpdated` → renderer fournit la notification et le delta de topologie.

---

## 4. Plan d'implémentation détaillé par phase

**Convention de tâches** : `P<phase>.<task>` (ex: P2.3, P4.5). Chaque tâche a un fichier cible, une action précise, et un critère d'acceptation. Estimations en jours-équivalent (single dev, contexte repo Sub3D).

---

### Phase 0 — Préparation (0.5 jour)

**Objectif** : tag stable + nettoyage dette + index memory + baseline tests.

**Pré-requis** : mini-tests `P-T3` et `P-T4` du doc compagnon **complétés** sur la branche `water-proto`. Les mini-tests doivent être faits **avant** Phase 0 car ils ont besoin du proto chargé runtime (PIE sur `L_WaterProto_TwoRooms`). Le plan principal opère sur `main` (ou la branche de portage), pas sur `water-proto`.

> **Note ordering** : la désactivation runtime du proto a été déplacée en Phase 5 (P5.5) pour éviter le conflit logique avec les mini-tests. Phase 0 ne touche pas au `.uproject`. Le proto reste chargé pendant toutes les phases 0-4 (sans actor placé en niveau actif), permettant un retour A/B à tout moment.

#### Tâches

**P0.1 — Tag stable pré-water-merge**
- Action : `git tag stable-pre-water-2026-05-04 <current-HEAD-of-main>`
- Critère : tag visible dans `git tag --list`.

**P0.2 — Référencer ce plan dans `MEMORY.md`**
- Fichier : `C:/Users/coren/.claude/projects/c--Dev-Sub3D/memory/MEMORY.md`
- Action : ajouter une ligne dans l'index :
  ```
  - [project_water_implementation_plan_2026_05_04.md](project_water_implementation_plan_2026_05_04.md) — Plan d'implémentation eau intérieure (Phase 0-5), contrat de porte, mini-tests pré-Phase 3/4
  ```
- Créer le fichier memory `project_water_implementation_plan_2026_05_04.md` (frontmatter + body court qui pointe vers `reports/plans/2026-05-04_water_implementation_plan.md` comme source canonique).
- Critère : prochaine session démarre avec le plan visible dans l'index memory.

**P0.3 — Nettoyage dette `FDerivedFloodVolume.BoundsMin/Max`**
- Fichier : `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h` lignes ~15-20
- Action : `grep -r "BoundsMin\|BoundsMax" Source/` pour confirmer aucun read. Si confirmé : retirer les deux UPROPERTY.
- Critère : build passe.

**P0.4 — Pre-commit baseline**
- Action : tests automation `RunTests Sub3D`. Capture log baseline pour comparaison post-water-merge.
- Critère : tous les tests existants passent. Log archivé.

**Validation Phase 0** : build vert + tests verts + tag posé + plan référencé dans MEMORY.md. Commit : `chore(water): phase 0 — baseline + memory index + dette cleanup`.

---

### Phase 1 — Sortie du legacy + DA priorité 1 (1-1.5 jour)

**Objectif** : un seul système de rendu eau ; init flood priorité DA.

**Pré-requis** : Phase 0 terminée.

#### Tâches

**P1.1 — Audit BP pour références à `UFloodWaterVisualsComponent`**
- Outil : `mcp__unrealclaude__unreal_blueprint_query` ou T3D dump des BP
- Cibles à chercher : `BP_Submarine_Craniata`, `BP_Submarine_*`, anciens protos
- Si référencé en BP : supprimer manuellement le composant de chaque BP avant suppression code.
- Critère : aucune référence BP au composant.

**P1.2 — Suppression de `UFloodWaterVisualsComponent`**
- Fichiers à supprimer :
  - `Source/Sub3D/Submarine/FloodWaterVisualsComponent.h`
  - `Source/Sub3D/Submarine/FloodWaterVisualsComponent.cpp`
- Fichier à modifier : `Source/Sub3D/Submarine/SubmarineBase.cpp` ligne 217 (CreateDefaultSubobject) et `SubmarineBase.h` (UPROPERTY `FloodWaterVisuals`).
- Action : retirer construction + UPROPERTY + include.
- Critère : build passe sans warning unresolved-reference.

**P1.3 — Inversion priorité init flood**
- Fichier : `Source/Sub3D/Submarine/SubmarineBase.cpp` lignes 295-331
- Action : refactor le bloc d'init :
  ```
  // AVANT (priorité volumes BP)
  if (Volumes.Num() > 0) → InitializeFromCompartmentVolumes
  else if (GeneratedDefinition) → InitializeFromDefinition
  else if (SubHull->LayoutAsset) → InitializeFromLayout
  else → warning

  // APRÈS (priorité DA)
  if (GeneratedDefinition) → InitializeFromDefinition
  else if (Volumes.Num() > 0) → InitializeFromCompartmentVolumes (warning : "fallback BP volumes — DA missing")
  else if (SubHull->LayoutAsset) → InitializeFromLayout (warning : "fallback legacy layout")
  else → warning
  ```
- Critère : Craniata se charge correctement, log montre `[Flood] Initialized from definition (Craniata)`.

**P1.4 — Helper `EnsureCompartmentVolumesFromDefinition()`**
- Fichier : nouveau dans `Source/Sub3D/Submarine/SubmarineBase.{h,cpp}`
- Signature : `void EnsureCompartmentVolumesFromDefinition();` private
- Comportement :
  ```
  Pour chaque Compartment dans Definition->Compartments :
      Si aucun UCompartmentVolumeComponent existant n'a CompartmentId == Compartment.CompartmentId :
          Spawn UCompartmentVolumeComponent en NewObject
          Set CompartmentId, BoxExtent depuis HydroBoundsMax-Min, RelativeLocation = (Min+Max)/2
          AttachToComponent(SubmarineRoot)
          RegisterComponent()
  ```
- Appelé dans `BeginPlay` **après** `InitializeFromDefinition`.
- Critère : Craniata avec ses volumes BP existants → pas de double-création (idempotent). Sub futur sans volumes BP → création auto, plans visibles à PIE.

**P1.5 — Test PIE Craniata état actuel**
- Action : ouvrir Craniata en PIE, déclencher un breach via `SubFloodComponent` console (ex: `SubFlood.CreateBreach C_main_Bow 200`).
- Critère :
  - 9 plans plats visibles (un par compartiment)
  - Niveau monte progressivement dans C_main_Bow
  - Aucun warning legacy "FloodWaterVisuals" dans log
  - Pas de Z-fighting visuel à la surface (validation suppression doublon)

**Validation Phase 1** : 1 seul système rendu, init DA-first, Craniata fonctionne. Commit : `refactor(water): phase 1 — drop legacy visuals, DA-first init`.

---

### Phase 2 — Bake offline + cap mesh per-compartiment (2-3 jours)

**Objectif** : remplacer le mesh plat par un cap mesh authored par compartiment via bake éditeur. Surface lit la silhouette réelle du compartiment.

**Pré-requis** : Phase 1 terminée.

#### Tâches

**P2.1 — Création module `Sub3DWaterBake` (Editor-only)**
- Fichiers à créer :
  - `Source/Sub3DWaterBake/Sub3DWaterBake.Build.cs` (Type=Editor)
  - `Source/Sub3DWaterBake/Public/Sub3DWaterBake.h`
  - `Source/Sub3DWaterBake/Private/Sub3DWaterBake.cpp` (FOutputDeviceFile log dédié)
- Build.cs deps : `Core, CoreUObject, Engine, Sub3D, Sub3DCore, ProceduralMeshComponent, GeometryCore, GeometryAlgorithms, UnrealEd, EditorScriptingUtilities`
- Ajouter au `.uproject` :
  ```json
  {
      "Name": "Sub3DWaterBake",
      "Type": "Editor",
      "LoadingPhase": "Default"
  }
  ```
- Critère : module compile, log `LogWaterBake: Module loaded` apparaît dans editor.

**P2.2 — Création UDataAsset `UCompartmentWaterBake`**
- Fichier : `Source/Sub3DWaterBake/Public/CompartmentWaterBake.h` et `.cpp`
- Hérite de `UDataAsset`
- Champs (tous `UPROPERTY(VisibleAnywhere, Category="Water Bake")`) :
  ```
  FName CompartmentId
  TArray<FCompartmentSlice> Slices                    // SDF + ContourPolygon
  TArray<FCachedWaterMesh> CapMeshesPerSlice          // ring tessellation
  TObjectPtr<UStaticMesh> CapMeshAsset                // mesh static authored (Marching Squares output)
  FVector LocalBoundsMin, LocalBoundsMax
  TArray<FOpeningSegment> OpeningSegments             // bordures portes/hatches
  TMap<FName ConnectionId, FDoorBoundaryCells> DoorCellMap   // [Phase 4 — pre-calc]
  int32 GridResolutionX, GridResolutionY
  float CellSizeCm
  uint32 BakeContentHash                              // DA topology hash for invalidation
  ```
- Struct `FDoorBoundaryCells { TArray<int32> CellIndicesA; TArray<int32> CellIndicesB; TArray<float> CellPairWeights; bool bUsedFastPath; }` — populée Phase 4 (cf. P4.1 dual-path).
- Critère : asset créable manuellement dans l'éditeur, sérialisable.

**P2.3 — Port de `URoomWaterBakerLibrary` → `USubmarineWaterBakerLibrary`**
- Fichier : `Source/Sub3DWaterBake/Public/SubmarineWaterBakerLibrary.h` et `.cpp`
- Hérite de `UBlueprintFunctionLibrary`
- API publique :
  ```cpp
  UFUNCTION(BlueprintCallable, CallInEditor)
  static UCompartmentWaterBake* BakeCompartment(
      USubmarineDefinition* Definition,
      FName CompartmentId,
      AActor* HullSourceActor,
      const FBakeParams& Params);

  UFUNCTION(BlueprintCallable, CallInEditor)
  static int32 BakeAllCompartments(
      USubmarineDefinition* Definition,
      AActor* HullSourceActor,
      const FBakeParams& Params);
  ```
- Logique portée depuis `RoomWaterBakerLibrary.cpp:581-826` (proto) :
  - Voxelisation parity raycast 6-direction (inside/outside)
  - SDF par 8 raycasts XY
  - Marching Squares interpolated par raycast réel sur arête
  - ChainSegmentsIntoPolygon, ResamplePolygonUniform (N=64)
  - Tessellation rings concentriques (R=3)
- Adaptation : prend une `USubmarineDefinition` + `CompartmentId`, lit `HydroBoundsMin/Max` au lieu d'un `UBoxComponent`. Source géométrie = `HullSourceActor` (typiquement `BP_Submarine_Craniata` placé en niveau).
- Sortie : populate `UCompartmentWaterBake` + génère `UStaticMesh` à partir des cap meshes (use `MeshDescription` API), sauvegarde dans `Content/Submarines/<SubName>/Water/`.
- Critère : `BakeCompartment` retourne un asset valide pour 1 compartiment de Craniata.

**P2.4 — Editor Utility Widget `EUW_BakeSubmarineWater`**
- Fichier : `Content/Sub3D/EditorUtility/EUW_BakeSubmarineWater.uasset` (BP)
- UI :
  - Picker `USubmarineDefinition`
  - Picker `AActor` (hull source)
  - Picker `BakeParams` struct (ou expose params individuels : CellSize, NumSlices, BakeRingsCount)
  - Liste compartiments avec checkbox individuel + bouton "Bake Selected"
  - Bouton "Bake All"
  - Progress bar
  - Log textuel par compartiment (success/warning/fail)
- Critère : utilisable manuellement, bake les 9 compartiments de Craniata en ~3-5 min total.

**P2.5 — Extension `USubmarineDefinition.WaterBakes`**
- Fichier : `Source/Sub3D/Submarine/Generator/SubmarineDefinition.h`
- Ajout :
  ```cpp
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water")
  TMap<FName, TObjectPtr<UCompartmentWaterBake>> WaterBakes;
  ```
- Renseigné par le baker au moment de la sauvegarde (le baker met à jour la map du DA).
- Critère : après bake Craniata, `DA_SubDef_Craniata.WaterBakes.Num() == 9`.

**P2.6 — Modification `UFloodWaterPlaneComponent::EnsurePlaneMesh()` pour résoudre le cap mesh**
- Fichier : `Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp` lignes 81-126
- Ordre de résolution :
  1. Lookup `OwnerSubmarine->GeneratedDefinition->WaterBakes[CompartmentId]->CapMeshAsset` → utiliser si présent (priorité 1)
  2. `SourceVolume->WaterPlaneMeshOverride` (existant — priorité 2)
  3. `PlaneMesh` engine fallback (existant — priorité 3)
- Si cap mesh authored : `SetWorldScale3D(FVector::OneVector)` (mesh authored à l'échelle correcte).
- Critère : Craniata après bake → 9 cap meshes correspondant aux silhouettes des compartiments visibles en PIE. Plus de plan plat débordant.

**P2.7 — Validation contrat de porte dans `USubmarineDefinition::IsValid()`**
- Fichier : `Source/Sub3D/Submarine/Generator/SubmarineDefinition.cpp`
- Action : ajouter une boucle de validation pour chaque Door connection :
  - `DoorWidthCm + 2*ClearanceMin < WallExtentInDoorPlane` ?
  - `DoorDepthCm + 2*ClearanceMin < WallThicknessCm` ?
  - LocalTransform forward axis perpendicular to wall plane (dot product check) ?
  - Center of door on wall plane (distance check) ?
- Si violation : log Warning, retourne false (mode strict) ou true avec liste d'avertissements (mode permissif). Préférer mode permissif Phase 2, pour ne pas bloquer Craniata existant. Mode strict activable par flag.
- Critère : Craniata audit log liste les portes non-conformes (à corriger en Phase 2 ou laisser pour audit ultérieur).

**P2.8 — Bake Craniata + audit visuel**
- Action :
  1. Lancer `EUW_BakeSubmarineWater` sur `DA_SubDef_Craniata`
  2. Observer 9 `UCompartmentWaterBake` créés dans `Content/Submarines/Craniata/Water/`
  3. PIE Craniata : observer les 9 plans avec silhouette correcte
  4. Comparer visuellement avant/après (plan plat 80m vs cap mesh authored)
- Critère : aucune fuite voxel (cellules "inside" en dehors du compartiment). Aucun gap visible aux portes/hatches au niveau d'eau standard.

**Validation Phase 2** : cap meshes authored visibles sur Craniata, plus de plan plat. Commit : `feat(water): phase 2 — offline bake + per-compartment cap meshes`.

**Risques Phase 2** : voxelisation parity raycast peut échouer sur Craniata handmade si géométrie hull a des trous fins. Mitigation : enable "Use Simple Collision" sur HullMesh, ou utiliser `MovementCollisionProxy` comme source géométrie. Audit P2.8 doit détecter ces cas.

---

### Phase 3 — Heightfield CPU + slosh modal + couplage flood (2-3 jours)

**Objectif** : surface vivante (ondes, slosh, ambient). Couplage flow rate exposé.

**Pré-requis** : Phase 2 terminée. Mini-test proto P-T3 (heightfield port standalone) recommandé avant — voir doc tests.

#### Tâches

**P3.1 — Audit `M_CompartmentWater` et préparation du shader**
- Action : ouvrir le matériau dans l'éditeur, lister les params + functions actuelles.
- Vérifier compatibilité Substrate / SLW (Single Layer Water) ou translucent classique.
- Documenter l'arbre matériau dans un commentaire technique.
- Critère : compréhension claire avant modifications.

**P3.2 — Extension matériau : sample texture R32F WPO**
- Asset : `M_CompartmentWater` (ou sa branche Substrate)
- Modification : ajouter input `WaterHeightTex` (Texture2D), sample en UV = local XY normalisé du compartiment, output → World Position Offset Z.
- Magnitude WPO : `(SampledValue - 0.5) * HeightfieldScaleCm` (default ~10cm pour ondes visibles mais subtiles).
- Test : matériau test minimaliste avec texture statique (gradient) → vérifier que WPO se voit en preview.
- Critère : matériau compile, preview montre déformation du plan basée sur la texture.

**P3.3 — Extension matériau : Gerstner ambient (port HLSL Custom node du proto)**
- Source : Custom HLSL node de `M_Phase0_Test` du proto
- Action : copier le node dans `M_CompartmentWater`, params : 3 ondes (Wavelength, Direction, Steepness, Speed)
- Stack : WPO total = HeightfieldWPO + GerstnerWPO
- Critère : preview animé ; ondes ambient subtiles + déformation heightfield visibles.

**P3.4 — Port heightfield CPU 2D dans `UFloodWaterPlaneComponent`**
- Fichier : `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h` et `.cpp`
- Inspirer de `RoomWaterRenderer.cpp:466-491` (proto)

> ⚠️ **CRITICAL — leçon mini-test P-T4** : le wave equation **DOIT traiter toutes les cellules** (`x=0..W-1`, `y=0..H-1`), pas juste les internes. Utiliser **Neumann BC** sur les bords (out-of-grid neighbor = mirror du centre). Bug invisible dans le proto initial (boucle `x=1..W-2`) parce que les bords n'étaient touchés par rien — mais Phase 4 sync au bord écrit dans ces cellules, et sans BC les valeurs s'accrochent forever sans damping. Cf. mini-test `reports/tests/2026-05-04_water_proto_minitests.md` § P-T4 résultats.
- Ajouter au composant :
  ```cpp
  // Heightfield state
  TArray<float> Heights;          // grid resolution from CompartmentWaterBake
  TArray<float> Velocities;
  int32 GridX, GridY;
  float CellSizeCm;

  // Texture
  UPROPERTY(Transient) UTexture2D* HeightfieldTex;
  TArray<float> TexBuffer;        // R32F push buffer

  // Tunables (EditAnywhere)
  float WaveSpeed = 12.0f;
  float WaveDamping = 0.985f;
  float HeightfieldUpdateHz = 60.f;

  // API publique
  void InjectAt(FVector WorldPoint, float Strength, float RadiusCm);
  void InjectAtLocalXY(FVector2D LocalXY, float Strength, float RadiusCm);
  ```
- Tick logic :
  ```
  Accumulate dt → fixed step 1/HeightfieldUpdateHz
  Per step :
      Wave equation update : Heights += Velocities, Velocities += LaplacianHeights * WaveSpeed * Damping
      Damp Velocities *= WaveDamping
  Push Heights → TexBuffer (normalize to 0..1) → UpdateTextureRegions
  Set MID param "WaterHeightTex" = HeightfieldTex
  Set MID param "HeightfieldScaleCm" = HeightfieldScale
  ```
- Init : `BeginPlay` lit la résolution depuis `OwnerSubmarine->GeneratedDefinition->WaterBakes[CompartmentId]`. Crée Texture2D R32F dynamique.
- Critère : ondes simulées CPU + visibles sur le plan via WPO matériau.

**P3.5 — `InjectAt` et hooks**
- Hook : appelé depuis `OnFloodStateUpdated` quand un breach inflow est détecté (injection radiale au point du breach localCenter).
- Hook : appelé manuellement via console command `FloodWater.InjectAt <CompartmentId> <X> <Y> <Strength>` pour test.
- Critère : breach trigger → onde radiale visible.

**P3.6 — Slosh modal 2D parametrique**
- Fichier : `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h/.cpp`
- Ajouter struct interne :
  ```cpp
  struct FSloshState
  {
      float OffsetZ = 0.f;        // current displacement
      float OffsetVelZ = 0.f;
      FVector2D Tilt = FVector2D::ZeroVector;       // current pitch/roll equiv
      FVector2D TiltVel = FVector2D::ZeroVector;
      float NaturalFreq = 0.8f;   // Hz
      float DampingRatio = 0.15f;
  };
  ```
- Tick :
  - Lire `OwnerSubmarine->SubMovement->GetVelocityCmS()` delta entre ticks → injection dans `OffsetVelZ` et `TiltVel`
  - Spring-damper update : `OffsetVelZ += (-OffsetZ * Stiffness - OffsetVelZ * Damping) * dt`
  - Idem pour Tilt
  - Output : ajustement Z du `PlaneMeshComponent` (`+= OffsetZ`) et tilt dans la rotation (clamp ±2°)
- Critère : sub vire à droite → léger slosh visible (offset latéral + tilt).

**P3.7 — Exposition flow rate par edge**
- Fichier : `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h`
- Modification de `FFloodEdgeState` :
  ```cpp
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  float CurrentFlowRateLitersPerSec = 0.f;       // [NEW] replicated
  ```
- Replication : ajouter au DOREPLIFETIME équivalent dans `SubFloodComponent`.
- Fichier : `Source/Sub3D/Submarine/SubFloodComponent.cpp` lignes ~619-641
- Action : dans `AdvanceFlooding`, après le calcul de `DesiredRateLps`, set `Edge.CurrentFlowRateLitersPerSec = DesiredRateLps` (avec sign si nécessaire pour direction).
- Ajouter API publique : `float GetFlowRateForEdge(FName ConnectionId) const;`
- **Bande passante** : Craniata 9 connections × 4 octets × ~10 Hz ≈ 360 octets/sec/client. Négligeable en FP. Sub futur 30+ connections = toujours sous 1.5 KB/sec/client.
- **Optimisation post-FP (NON FAIT FP)** : delta-based replication (ne replier que si `|FlowRate - LastReplicatedFlowRate| > Threshold`). À considérer si l'on dépasse ~50 connections par sub. Pas un blocker FP.
- Critère : log montre flow rate non-nul à l'ouverture de porte avec delta.

**P3.8 — Hook flow rate dans `UDoorFloodVfxComponent`**
- Fichier : `Source/Sub3D/Submarine/DoorFloodVfxComponent.cpp`
- Action : dans `RefreshFromCurrentFloodState`, remplacer le calcul `FlowIntensity01 = HeightDelta / MaxHeightDelta` par `FlowIntensity01 = abs(GetFlowRateForEdge) / MaxFlowRate`.
- Critère : cascade Niagara module à travers ouverture de porte avec intensité plus fidèle au flow réel.

**P3.9 — Test PIE complet Phase 3 + budget perf**
- Scénarios :
  1. Click sur un compartiment → onde radiale visible
  2. Sub vire brutalement → slosh visible (offset Z + tilt)
  3. Ouvre une porte avec delta de niveau → cascade Niagara avec intensité proportionnelle
  4. Trigger breach → niveau monte + onde radiale + jet Niagara (BreachVfx existant)
- **Budget perf cible** : <0.2ms par compartiment **actif** côté CPU (non 0.5ms comme initialement proposé — 4.5ms total sur Craniata 9 compartiments serait 28% du frame budget, trop large).
- **Stratégie LOD obligatoire pour tenir le budget** :
  - Désactivation heightfield update si :
    - Compartiment hors view frustum (skip wave equation, freeze Heights/Velocities)
    - Compartiment à niveau d'eau < 5% (rien à animer)
    - Aucun crew dans le compartiment ET pas de breach actif
  - Compartiments dormants : push texture R32F gardée (1 frame statique), pas de tick
  - Réveil immédiat si breach créé / crew entre / niveau monte au-delà du seuil
- **Estimation Craniata avec LOD** : ~3 compartiments actifs simultanés × 0.2ms = 0.6ms (~4% du budget 16ms@60Hz). Acceptable.
- Profile GPU/CPU : capture frame trace `stat unit` + `stat game`, valider l'estimation.
- Critère : tous scénarios visuels OK, budget tenu (< 1ms total system heightfield sur Craniata avec usage typique), pas de stutter à 60fps fixed-rate.

**Validation Phase 3** : surface vivante avec ondes + slosh, flow rate exposé, VFX Niagara améliorées. Commit : `feat(water): phase 3 — heightfield CPU + slosh + flow rate exposure`.

---

### Phase 4 — Continuité aux portes horizontales (1.5-2 jours)

**Objectif** : scénario A↔B des fondamentaux (continuité visuelle + propagation des vagues à travers porte ouverte).

**Pré-requis** : Phase 3 terminée. Mini-test proto P-T4 (boundary sync standalone) recommandé avant — voir doc tests.

#### Tâches

**P4.1 — Calcul des cellules frontière au bake (dual-path : fast/general)**
- Fichier : `Source/Sub3DWaterBake/Private/SubmarineWaterBakerLibrary.cpp`
- Action : après tessellation de chaque compartiment, pour chaque connection `Door|Open` impliquant ce compartiment, exécuter d'abord la validation contrat (P2.7), puis :

**Fast path (porte conforme au contrat)** :
- Wall plane connue (perpendiculaire à `LocalTransform.GetForwardVector()` au point `LocalTransform.GetLocation()`)
- Cellules frontière côté A = cellules de la grille du compartiment A dont le centre est :
  - À distance ≤ 1 cell de la wall plane
  - Sous la projection rectangulaire du door footprint sur le plan de la grille
- Idem côté B (mirror sur wall plane)
- Bijection 1:1 : `CellIndicesA[i]` et `CellIndicesB[i]` correspondent à la même position dans le plan de porte (mirror).

**General fallback path (porte non-conforme)** :
- Récupérer le footprint de la porte (rectangle ou polygone arbitraire) projeté sur le plan du mur
- Pour chaque cellule de la grille du compartiment A :
  - Si centre dans rayon `MaxDoorExtent/2` autour de `LocalTransform.GetLocation()` ET du côté de A par rapport au plan du mur → ajouter à `CellIndicesA`
- Idem côté B
- **Pairing nearest-neighbor** :
  - Pour chaque cellule de A : trouver la cellule de B dont la projection sur le plan du mur est la plus proche
  - Si `Num(A) ≠ Num(B)` : autoriser `N:1` ou `1:N` mappings via `CellPairWeights` (poids inversement proportionnels à la distance projetée)
- Coût : O(NumCellsA × NumCellsB) — toujours négligeable (<100 cells/porte).

**Stockage commun** :
```cpp
FDoorBoundaryCells {
    TArray<int32> CellIndicesA;       // dans la grille du compartiment A
    TArray<int32> CellIndicesB;       // dans la grille du compartiment B
    TArray<float> CellPairWeights;    // 1.0 par défaut (fast path), variable (general fallback)
    bool bUsedFastPath;               // diagnostic
}
```

- Critère sur Craniata : chaque Door connection a un `DoorCellMap[ID]` valide. Log informe de `bUsedFastPath` par porte. Visuel sync (Phase 4 P4.5) doit fonctionner indépendamment du path utilisé.

**P4.2 — Création `USubmarineDoorWaterCoordinator`**
- Fichier : `Source/Sub3D/Submarine/SubmarineDoorWaterCoordinator.h` et `.cpp`
- Hérite de `UActorComponent`
- Sous-composant ajouté au constructeur de `ASubmarineBase` (ou attaché dynamiquement) :
  ```cpp
  // SubmarineBase.cpp constructor
  DoorWaterCoordinator = CreateDefaultSubobject<USubmarineDoorWaterCoordinator>(TEXT("DoorWaterCoordinator"));
  ```
- Tick prereq : `AddTickPrerequisiteComponent(SubFlood)` dans `BeginPlay`.

**P4.3 — Logique tick du coordinator**
- Fichier : `SubmarineDoorWaterCoordinator.cpp`
- Tick :
  ```
  Pour chaque Connection dans Definition->Connections :
      Si ConnectionType ∉ {Door, Open} : skip
      Si EdgeStates[i].bClosed && ConnectionType==Door : skip

      RendererA = trouver UFloodWaterPlaneComponent du compartiment A
      RendererB = trouver UFloodWaterPlaneComponent du compartiment B
      Si RendererA == nullptr || RendererB == nullptr : skip

      DoorCells = WaterBakes[A].DoorCellMap[ConnectionId]
      Si DoorCells.IsEmpty : skip

      SyncFactor = clamp(PassageAreaCm2 / RefAreaCm2, 0, 1)
      // RefAreaCm2 = DoorWidthCm * DoorHeightCm typique = 20000 cm² (ajustable)

      // Heights : sync forte
      HeightSyncStrength = SyncFactor * SyncStrengthHeights      // tunable, default 0.5
      // Velocities : sync faible (anti-resonance — D-4)
      VelocitySyncStrength = SyncFactor * SyncStrengthVelocities  // tunable, default 0.1

      Pour i = 0 .. DoorCells.CellIndicesA.Num() :
          idxA = DoorCells.CellIndicesA[i]
          idxB = DoorCells.CellIndicesB[i]
          PairWeight = DoorCells.CellPairWeights.IsValidIndex(i) ? DoorCells.CellPairWeights[i] : 1.0f

          // Pondération par PairWeight (1.0 fast path, variable general fallback)
          EffectiveHeightSync = HeightSyncStrength * PairWeight
          EffectiveVelSync = VelocitySyncStrength * PairWeight

          // Average pondéré des heights
          AvgHeight = lerp(RendererA->Heights[idxA], RendererB->Heights[idxB], 0.5f)
          RendererA->Heights[idxA] = lerp(RendererA->Heights[idxA], AvgHeight, EffectiveHeightSync)
          RendererB->Heights[idxB] = lerp(RendererB->Heights[idxB], AvgHeight, EffectiveHeightSync)

          // Velocities sync plus faible
          AvgVel = lerp(RendererA->Velocities[idxA], RendererB->Velocities[idxB], 0.5f)
          RendererA->Velocities[idxA] = lerp(RendererA->Velocities[idxA], AvgVel, EffectiveVelSync)
          RendererB->Velocities[idxB] = lerp(RendererB->Velocities[idxB], AvgVel, EffectiveVelSync)
  ```
- Tunables EditAnywhere (**valeurs validées par mini-test P-T4 — voir `reports/tests/2026-05-04_water_proto_minitests.md`**) :
  - `SyncStrengthHeights = 0.5` (continuité visuelle nette aux bords, pas de step Z visible à l'équilibre)
  - `SyncStrengthVelocities = 0.1` (onde traverse la frontière, pas de résonance R-6)
  - `VelocitiesPostSyncDamping = 0.95` (amortissement progressif, pas d'oscillation parasite)
  - `RefAreaCm2 = 20000` (default, à tuner selon largeur de porte typique)
- Critère : compile, tick s'exécute après SubFlood et avant les renderers (vérifier l'ordre via `AddTickPrerequisiteComponent`).
- **Tick prereq** : les renderers doivent ticker APRÈS le coordinator pour que la wave equation propage les valeurs synchronisées dans le frame courant. Validé par mini-test (sans prereq, le sync écrit dans des cellules potentiellement déjà processées).

**P4.4 — Cohérence multi-portes**
- Pas de logique additionnelle requise. Si compartiment A a 2 portes (vers B et C) :
  - Passe 1 (sync A↔B) modifie Heights[bord A vers B]
  - Passe 2 (sync A↔C) modifie Heights[bord A vers C]
  - Le heightfield interne du compartiment A propage ensuite naturellement les deux contributions
- Critère : test scénario avec 3 compartiments en chaîne (B fermé, A↔C ouvert via B-bypass) : pas applicable directement, mais 2 portes ouvertes simultanément doivent fonctionner.

**P4.5 — Test scénario A↔B (validation des fondamentaux 1, 2, 6)**
- Setup : 2 compartiments adjacents avec porte horizontale (Craniata : C_main_Bow ↔ C_main_Fwd typiquement).
- Console commands :
  ```
  SubFlood.SetLevel C_main_Bow 0.5
  SubFlood.SetLevel C_main_Fwd 0
  SubFlood.SetDoorState <DoorID> false   // ouvrir
  ```
- Observations attendues (les 7 points du scénario fondamental) :
  1. Cascade Niagara visible dès l'ouverture (déjà OK Phase 3)
  2. Cascade s'amplifie avec l'augmentation de delta
  3. Niveau Bow baisse, niveau Fwd monte → équilibrage à ~0.25
  4. Surface visuellement continue à travers la porte (sync au bord)
  5. Vagues générées en Fwd se propagent via la porte vers Bow
  6. À l'équilibre, surface strictement continue (pas de discontinuité Z)
- Critère : observation visuelle qualitative satisfaisante. Si oscillations résonantes : ajuster `SyncStrengthVelocities` à la baisse (D-4).

**P4.6 — Test multi-portes**
- Setup : chaîne A→B→C avec 2 portes ouvertes simultanément.
- Action : remplir A à 60%, B et C vides, ouvrir les 2 portes.
- Critère : eau se redistribue dans les 3, surface continue visuellement à chaque porte.

**Validation Phase 4** : continuité visuelle aux portes confirmée, pas de patch, pas de bridge mesh. Commit : `feat(water): phase 4 — door boundary sync coordinator`.

**Risques Phase 4** :
- Oscillations résonantes (R-6) : mitigation = SyncStrengthVelocities faible.
- Cellules frontière mal calculées sur portes obliques : audit visuel + commande debug `WaterCoord.DrawDoorCells <ConnectionId>`.

---

### Phase 5 — Hull breaking ready + cleanup proto (1 jour)

**Objectif** : archi accueille hull breaking sans refonte. Proto bordé. Doc à jour.

**Pré-requis** : Phase 4 terminée et validée.

#### Tâches

**P5.1 — Hook `OnBreachesUpdated` dans `ASubmarineBase`**
- Fichier : `Source/Sub3D/Submarine/SubmarineBase.cpp`
- Action :
  ```cpp
  // BeginPlay
  if (SubHull) {
      SubHull->OnBreachesUpdated.AddDynamic(this, &ASubmarineBase::HandleBreachesUpdatedForWater);
  }
  ```
- Nouvelle méthode `HandleBreachesUpdatedForWater()` :
  - Récupérer la liste actuelle des `FBreachClusterState` depuis `SubHull->BreachClusters`
  - Pour chaque breach :
    - Trouver le compartiment touché par `LocalCenter` ∈ `Compartments[i].HydroBoundsMin/Max`
    - Récupérer `UFloodWaterPlaneComponent` du compartiment
    - Appeler `Renderer->OnBreachAdded/Updated/Removed(BreachState)`
  - Détecter breaches inter-compartiments (cas hull breaking entre 2 compartiments) :
    - Si `LocalCenter` est sur la frontière entre 2 compartiments :
      - Notifier `DoorWaterCoordinator->RegisterRuntimeEdge(BreachId, CompA, CompB, FlowAreaCm2)`
- Critère : test avec console `SubHull.AddBreach C_main_Bow 100 0 50 (radius 30)` → renderer notifié, jet Niagara visible.

**P5.2 — Renderer breach handling**
- Fichier : `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h` et `.cpp`
- Nouvelle API :
  ```cpp
  void OnBreachAdded(const FBreachClusterState& Breach);
  void OnBreachUpdated(const FBreachClusterState& Breach);
  void OnBreachRemoved(FName BreachId);
  ```
- Logique :
  - Maintien d'une `TMap<FName, FActiveBreachWaterEffect>` des breaches actifs
  - Sur Add/Update : `InjectAt(BreachWorldPoint, Strength * InflowRate, BreachInscribedRadiusCm * 2)` continu
  - Sur Remove : nettoie l'entrée
- Critère : breach trigger → onde continue + jet Niagara (existant).

**P5.3 — `RegisterRuntimeEdge` dans `DoorWaterCoordinator`**
- Fichier : `Source/Sub3D/Submarine/SubmarineDoorWaterCoordinator.h` et `.cpp`
- Refactor : la liste des connexions itérée au tick devient l'union de :
  - `Definition->Connections[]` filtrée sur Door/Open ouvertes
  - `RuntimeEdges[]` ajoutées par `RegisterRuntimeEdge` (breaches inter-compartiment)
- API :
  ```cpp
  void RegisterRuntimeEdge(FName EdgeId, FName CompA, FName CompB, float FlowAreaCm2);
  void UnregisterRuntimeEdge(FName EdgeId);
  ```
- Pour les runtime edges, `DoorCellMap` n'est pas pré-baké → calcul à la volée la première fois (cache), basé sur la position du breach.
- **Pas obligatoire FP** : peut rester un stub avec log warning si breach inter-compartiment détecté. Implémentation complète à faire avec hull breaking lui-même.
- Critère : structure prête, log informatif si stub.

**P5.4 — Console command "fake breach trigger"**
- Fichier : `Source/Sub3D/Submarine/SubFloodComponent.cpp` (ou nouveau cpp pour debug commands)
- Commande : `SubFlood.SimulateBreach <CompartmentId> <LocalX> <LocalY> <LocalZ> <RadiusCm>`
- Action : crée un breach via `USubHullComponent::AddBreachAt(...)` (ou stub direct vers le renderer si SubHull pas prêt).
- Critère : commande exécutable, déclenche la chaîne de notifications.

**P5.5 — Cleanup proto + désactivation runtime**
- Action :
  1. Lister actors `ARoomActor` placés dans niveaux : `grep "ARoomActor\|/Sub3DWaterProto/" Content/Maps/*.umap`
  2. Si trouvés dans niveaux FP : retirer ou déplacer dans `Content/_archive/`
  3. `L_WaterProto_TwoRooms.umap` : ne pas inclure dans le pack final FP, mais conserver dans `Content/_archive/`
  4. `BD_Room_01.uasset`, `BD_Room_02.uasset` : déplacer dans `Content/_archive/Sub3DWaterProto/BakedData/`
  5. **Retirer `Sub3DWaterProto` du `.uproject` runtime** (déplacé depuis P0.2 pour permettre les mini-tests pré-portage) :
     - Fichier : `C:/Dev/Sub3D/Sub3D.uproject` lignes 43-46
     - Supprimer le bloc JSON :
       ```json
       {
           "Name": "Sub3DWaterProto",
           "Type": "Runtime",
           "LoadingPhase": "Default"
       }
       ```
     - Le code reste sur disque dans `Source/Sub3DWaterProto/` (compilable si réintégré pour A/B post-FP).
- Critère : packaging FP n'inclut plus les actors proto, module non chargé runtime, code reste accessible pour A/B.

**P5.6 — Documentation**
- Fichier `CLAUDE.md` :
  - Ajouter section "Water rendering pipeline" décrivant l'archi finale
  - Retirer/marquer comme legacy toute référence au proto `Sub3DWaterProto`
- Memory : créer `project_water_pipeline_2026_05_XX.md` avec :
  - Architecture finale
  - Composants clefs : `UFloodWaterPlaneComponent`, `USubmarineDoorWaterCoordinator`, `UCompartmentWaterBake`
  - Pipeline de bake : `EUW_BakeSubmarineWater`
  - Hull breaking hook : `SubHull->OnBreachesUpdated`
- Memory : marquer `project_water_proto_pivot_2026_04_29.md` comme superseded.
- Memory : marquer `project_water_material_post_fp.md` comme partiellement superseded (single-slab Fresnel-driven approach n'est plus pertinent puisqu'on a un cap mesh authored).
- Critère : doc à jour, prochaine session démarre avec la bonne mental model.

**Validation Phase 5** : breach trigger → réaction visuelle complète. Doc à jour. Commit : `feat(water): phase 5 — hull breaking hooks + proto cleanup + docs`.

---

### Phase 6 (optionnelle, post-FP) — Beta heightfield Niagara / shallow-water

Si le heightfield CPU s'avère insuffisant en performance ou en qualité visuelle (decision based on profile multiplayer 8+ joueurs) :

- Migration vers Niagara Fluids 2D (shallow-water solver natif Epic)
- Le matériau lit déjà une texture R32F → swap source CPU vs Niagara transparent
- Coût estimé : 3-5 jours additional

**Pas un commitment FP**.

---

### Récapitulatif estimations

| Phase | Estimation | Cumulé | Confiance |
|---|---|---|---|
| 0 | 0.5j | 0.5j | haute |
| 1 | 1-1.5j | 2j | haute |
| 2 | 2-3j | 5j | moyenne (bake éditeur authoring + voxel Craniata handmade) |
| 3 | 2-3j | 8j | moyenne (audit matériau peut révéler des contraintes Substrate) |
| 4 | 1.5-2j | 10j | basse (sync au bord — cas tordus possibles malgré contrat porte) |
| 5 | 1j | 11j | moyenne (RuntimeEdge complet est un demi-stub, vrai cost arrive avec hull breaking) |

**Fourchette totale** : 8-11 jours-équivalent solo dev. Plus large que l'estimation externe (6-8j) car Phase 2 et Phase 4 sont systématiquement sous-estimées dans les plans de rendu eau.

---

## 5. Décisions résolues (D1-D7)

| ID | Décision | Choix | Justification |
|---|---|---|---|
| **D1** | Module pour le baker | **A — nouveau `Sub3DWaterBake` (Editor-only)** | Symétrie avec `Sub3DBake` legacy, isolation propre. |
| **D2** | Format de stockage du bake | **A — `TMap<FName, UCompartmentWaterBake*>` dans `USubmarineDefinition`** | Référence explicite via DA = source unique cohérent avec fondamental #8. Rebake sélectif possible. |
| **D3** | Désactivation runtime du proto | **A — retirer du `.uproject`** | Zéro coût runtime, séparation claire, restauration simple par re-ajout JSON. |
| **D4** | Pondération sync au bord | **Sync forte sur Heights, faible sur Velocities** | Anti-résonance R-6. À tuner Phase 4 PIE. Default Heights 0.5, Velocities 0.1, ajustable. |
| **D5** | `UFloodWaterVisualsComponent` | **Suppression complète** | Doublon legacy. Vérification grep BP avant suppression (P1.1). |
| **D6** | Authoring Craniata | **Garder les deux paths (DA priorité, fallback BP)** | Évite churn. Si DA insuffisant → placement BP manuel possible. À ajuster post-tests. |
| **D7** | Multi-deck | **Heightfield par compartiment, point** | Pas de notion de "deck" au niveau eau. Hatches verticales narratives. Raffinement post-FP. |

**Décision additionnelle (issue de l'input utilisateur 2026-05-04)** :
- **D8 — Contrat de porte obligatoire** : ✅ adopté. Voir Section 2. Implications : validation DA-time, simplification baker, sync 1:1.

---

## 6. Fichiers impactés

### Nouveaux

| Fichier | Phase | Rôle |
|---|---|---|
| `Source/Sub3DWaterBake/Sub3DWaterBake.Build.cs` | P2.1 | Nouveau module éditeur |
| `Source/Sub3DWaterBake/Public/Sub3DWaterBake.h` | P2.1 | Module include |
| `Source/Sub3DWaterBake/Private/Sub3DWaterBake.cpp` | P2.1 | Log dédié |
| `Source/Sub3DWaterBake/Public/CompartmentWaterBake.h` | P2.2 | UDataAsset per compartiment |
| `Source/Sub3DWaterBake/Private/CompartmentWaterBake.cpp` | P2.2 | (vide / POD) |
| `Source/Sub3DWaterBake/Public/SubmarineWaterBakerLibrary.h` | P2.3 | API bake |
| `Source/Sub3DWaterBake/Private/SubmarineWaterBakerLibrary.cpp` | P2.3 | Pipeline bake (port proto + adaptation DA) |
| `Source/Sub3DWaterBake/Public/SubmarineWaterBakerTypes.h` | P2.2 | `FCompartmentSlice`, `FCachedWaterMesh`, `FOpeningSegment`, `FDoorBoundaryCells`, `FBakeParams` |
| `Content/Sub3D/EditorUtility/EUW_BakeSubmarineWater.uasset` | P2.4 | UI bake éditeur |
| `Content/Sub3D/EditorUtility/EUW_AuditDoorContract.uasset` | P2.7 (option) | Audit contrat porte |
| `Source/Sub3D/Submarine/SubmarineDoorWaterCoordinator.h` | P4.2 | Coordinator sync au bord |
| `Source/Sub3D/Submarine/SubmarineDoorWaterCoordinator.cpp` | P4.3 | Impl |
| `Content/Submarines/Craniata/Water/CWB_*.uasset` (×9) | P2.8 | Bakes per compartiment |

### Modifiés

| Fichier | Phase | Nature |
|---|---|---|
| `Sub3D.uproject` | P0.2, P2.1 | Retirer Sub3DWaterProto, ajouter Sub3DWaterBake |
| `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h` | P0.3, P3.7 | Cleanup BoundsMin/Max ; ajout `CurrentFlowRateLitersPerSec` |
| `Source/Sub3D/Submarine/SubmarineBase.h/.cpp` | P1.2-P1.4, P4.2, P5.1 | Drop FloodWaterVisuals ; inversion priorité ; auto-spawn volumes ; DoorWaterCoordinator ; OnBreachesUpdated hook |
| `Source/Sub3D/Submarine/FloodWaterVisualsComponent.{h,cpp}` | P1.2 | **Supprimés** |
| `Source/Sub3D/Submarine/Generator/SubmarineDefinition.h/.cpp` | P2.5, P2.7 | `WaterBakes` map ; validation contrat porte |
| `Source/Sub3D/Submarine/Generator/SubmarineDefinitionTypes.h` | P2.7 | Ajout `DoorDepthCm` à `FGeneratedConnectionDef` |
| `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h/.cpp` | P2.6, P3.4-P3.6, P5.2 | Cap mesh resolution ; heightfield CPU ; slosh modal ; breach handling |
| `Source/Sub3D/Submarine/SubFloodComponent.h/.cpp` | P3.7, P5.4 | Expose flow rate ; SimulateBreach console |
| `Source/Sub3D/Submarine/DoorFloodVfxComponent.cpp` | P3.8 | Use exposed flow rate |
| `Content/Sub3D/Material/M_CompartmentWater.uasset` | P3.2-P3.3 | Sample R32F WPO + Gerstner ambient |
| `Content/Sub3D/Material/MI_CompartmentWater.uasset` | P3.2-P3.3 | Tuning des nouveaux params |
| `CLAUDE.md` | P5.6 | Section "Water rendering pipeline" |

### Déplacés / archivés

| Fichier | Phase | Destination |
|---|---|---|
| `Content/Sub3DWaterProto/BakedData/BD_Room_*.uasset` | P5.5 | `Content/_archive/Sub3DWaterProto/` |
| `Content/Sub3DWaterProto/Materials/M_Phase0_Test.uasset` | P5.5 | Reste en place (proto reactivable A/B) |
| `Content/Maps/L_WaterProto_TwoRooms.umap` | P5.5 | `Content/_archive/Maps/` |
| `Source/Sub3DWaterProto/` (code) | P0.2 | Reste en place, pas chargé runtime |

### Memory / docs

| Fichier | Phase | Action |
|---|---|---|
| `memory/project_water_pipeline_2026_05_XX.md` | P5.6 | Créer |
| `memory/project_water_proto_pivot_2026_04_29.md` | P5.6 | Marquer superseded |
| `memory/project_water_material_post_fp.md` | P5.6 | Marquer partiellement superseded |
| `memory/MEMORY.md` | P5.6 | Update index |

---

## 7. Risques identifiés

### R-1 : Voxelisation parity raycast échoue sur Craniata handmade (HAUT)

**Symptôme attendu** : cellules "inside" en dehors du compartiment (silhouette qui bave) ou cellules "outside" à l'intérieur (trous dans le cap).

**Causes possibles** : doubles surfaces, normales inversées, gaps fins entre meshes, MovementCollisionProxy incomplet.

**Mitigation** :
- P2.8 audit visuel obligatoire sur Craniata
- Toggle "use simple collision" sur HullMesh dans static mesh editor
- Fallback : si bake échoue sur un compartiment, le baker génère un cap mesh = box `HydroBoundsMin/Max`. Plan B documenté.

### R-2 : Performance heightfield 60 Hz × N compartiments × N clients (MOYEN)

**Symptôme attendu** : stutter en multiplayer 8+ joueurs.

**Mesure** : Phase 3 P3.9 frame trace. Budget cible : <0.5ms/compartiment CPU.

**Mitigation** :
- LOD : freeze update si compartment hors view frustum + niveau < 5%
- Désactivation conditionnelle si vide
- Phase 6 (Niagara) si CPU vraiment saturé

### R-3 : Dérive client desync visuelle (FAIBLE)

**Symptôme** : surface ondulante diffère légèrement entre clients.

**Acceptable selon fondamentaux** (visuel suit niveaux répliqués, détails locaux OK). Pas de mitigation FP.

### R-4 : Substrate + WPO + ProcMesh (MOYEN)

**Symptôme attendu** : matériau test minimaliste P3.2 ne montre pas de WPO ou erreurs compilation.

**Mitigation** :
- P3.1 audit matériau avant modifs
- Test minimaliste avant investissement (P3.2 d'abord avec texture statique)
- Fallback : SLW (Single Layer Water) traditionnel si Substrate récalcitrant

### R-5 : Hull breaking pas testé en Phase 5 — incompatibilité tardive (MOYEN)

**Mitigation** : P5.4 fake breach trigger valide les hooks sans implémenter hull breaking complet. Si hull breaking arrive plus tard, l'archi est validée.

### R-6 : Sync au bord trop fort = oscillations résonantes (MOYEN)

**Symptôme attendu** : Phase 4 — quand 2 compartiments connectés, leurs surfaces se "tirent" mutuellement, bouncing.

**Mitigation** :
- D-4 résolu : Heights sync forte (cohérence visuelle), Velocities sync faible (anti-bounce)
- Damping additionnel après sync : `Velocities[idx] *= 0.95` après merge
- Test PIE Phase 4 dédié (P4.5) capture le problème

### R-7 : Init order BeginPlay sans DA ni volumes (FAIBLE)

**Mitigation** : conserver fallback `InitializeFromLayout` jusqu'à Phase 5 cleanup. Sub futur SANS DA reste functional avec warning.

### R-8 : Contrat de porte non respecté par Craniata existant (FAIBLE — depuis revision)

**Symptôme attendu** : P2.7 audit log liste portes non-conformes sur Craniata.

**Mitigation par design** : le contrat est un *recommandé*, pas un *requis* (cf. Section 2.1). Le bake et le coordinator runtime fonctionnent **toujours** — fast path bijection 1:1 sur portes conformes, fallback nearest-neighbor pairing sur portes non-conformes (cf. Section 2.4 et P4.1). Aucune porte n'est "non-fonctionnelle" — au pire, elle a un pairing approximatif et un coût bake légèrement plus élevé (négligeable, <100 cells/porte).

**Conséquences en pratique** :
- Une porte de Craniata violant le contrat ne casse pas la sync — elle utilise le fallback path automatiquement
- L'audit visuel `EUW_AuditDoorContract` reste utile pour identifier les portes où le pairing approximatif pourrait dégrader la qualité visuelle (asymétrie de cellules, sync moins fluide)
- Correction du DA Craniata recommandée seulement si dégradation visible
- Mode strict ne sera jamais activé — le système est robuste par design

### R-9 : Conflit nommage `Sub3DBake` legacy vs `Sub3DWaterBake` nouveau (FAIBLE)

**Mitigation** : namespacing log `LogWaterBake` distinct de `LogSub3DBake`. Doc CLAUDE.md différencie clairement.

---

## 8. Ce que cette proposition NE fait PAS (et c'est intentionnel)

- **Pas de Niagara Fluids / shallow-water 2D** en système central. Phase 6 optionnelle.
- **Pas de FLIP/SPH 3D**.
- **Pas de postprocess underwater dédié**. Ré-utilise `UCrewUnderwaterPPComponent` existant.
- **Pas de caustiques avancées**.
- **Pas de stencil portal** pour la continuité.
- **Pas de redesign de `USubFloodComponent`**. Juste exposition.
- **Pas de modal slosh 3D complet**. Slosh 2D parametrique suffit.
- **Pas de hull breaking implementation**. Juste les hooks.

---

## 9. Tests proto associés — RÉSULTATS (exécution 2026-05-05)

Doc complet : `reports/tests/2026-05-04_water_proto_minitests.md`. Branche éphémère : `water-proto-minitest-pt4` (référence, **pas mergée**).

### Verdict global

✅ **P-T3 validé** — matériau Substrate + WPO + R32F portable dans `M_CompartmentWater` Sub3D. **R-4 dérisqué**.
✅ **P-T4 validé** — algorithme de boundary sync fonctionne (pairing nearest-neighbor world-space, géré Yaw arbitraire). **R-6 dérisqué** avec les valeurs ci-dessus.
⏭ **P-T5 skippé** — direct Phase 3.6 du plan principal.

### Bug critique trouvé en cours (DOIT être appliqué Phase 3)

Le wave equation du proto ne traitait QUE les cellules **internes** (`x=1..W-2`, `y=1..H-2`). Les cellules de bord n'étaient jamais updatées par le wave equation → toute valeur écrite par un système externe (sync au bord Phase 4) **restait accrochée pour toujours**, sans Laplacian, sans damping. Bug invisible dans le proto initial parce que rien n'écrivait dans les bords — mais Phase 4 le fait. **Sans fix, Phase 4 échoue silencieusement de la même manière.**

**Fix obligatoire Phase 3 (P3.4)** : le wave equation porté dans `UFloodWaterPlaneComponent` doit :
- Traiter **toutes les cellules** dès l'écriture (boucle `x=0..W-1`, `y=0..H-1`)
- Utiliser **Neumann reflective BC** sur les bords (out-of-grid neighbor = mirror du centre)

Référence implémentation validée : diff sur `RoomWaterRenderer.cpp:466-491` dans la branche `water-proto-minitest-pt4`.

### Valeurs validées (à utiliser comme défauts Phase 4)

`SyncStrengthHeights=0.5`, `SyncStrengthVelocities=0.1`, `VelocitiesPostSyncDamping=0.95`. Voir P4.3.

### Note de cadrage

Le sync au bord ne synchronise QUE le heightfield (`Heights[]` = ondes/deltas). Il ne synchronise **PAS** les niveaux absolus (`CurrentWaterLevelLocalZ`). Cohérent avec fondamental #4 (visuel suit FloodComponent qui pilote les niveaux gameplay). Le sync transfère uniquement l'activité ondulatoire, ce qui suffit pour la continuité visuelle des ondes au bord.

---

**Fin du plan. Prêt pour exécution Phase 0 sur green light humain.**
