# Sub3D — Submarine Editor : Handoff Complet

**Version :** 2026-04-04
**Statut :** Clôture du projet Sub Editor — passage au gameplay

---

## 1. État de l'implémentation

### 1.1 Ce qui est livré et compilé

Tous les modules compilent sans erreur (42 actions, 0 warning bloquant, build time ~21 s).

| Module | Rôle |
|--------|------|
| `Sub3DCore` | Types de données : Hull, Rings, Bays, Decks, Floors, Compiled types, Enums |
| `Sub3DBake` | Services de bake + bake subsystem éditeur |
| `Sub3DRuntime` | Runtime actor + composants runtime |
| `Sub3DEditor` | Toolkit Slate + asset type actions |
| `Sub3DTests` | 18 tests automation UE5 |

---

### 1.2 Pipeline bake complet

```
USub3DSubmarineAuthoringAsset
    │
    ├─ FHullProfileParams ──────────────────────────────────────────┐
    │   (Myring / Series58 / Superellipse / Uniform / Manual)       │
    │                                                               │
    ├─ FSubmarineHullDef                                            │
    ├─ TArray<FControlRingDef>   ◄── auto-généré si profil ≠ Manual ┘
    ├─ TArray<FFrameRingDef>
    ├─ FOuterEnvelopeDef
    ├─ TArray<FStructuralBayDef>
    ├─ TArray<FDeckLevelDef>
    ├─ TArray<FFloorRegionDef>
    ├─ TArray<FOpeningDef>
    ├─ TArray<FConnectorDef>
    ├─ TArray<FClosureDef>
    ├─ TArray<FPressureBulkheadDef>
    └─ TArray<FInternalWallDef>
            │
            ▼  USubmarineBakeSubsystem::BakeBase()
    UCompiledSubmarineBaseAsset
    ├─ FSub3DCompiledHullData          (sections interpolées)
    ├─ FCompiledMeshSection ExteriorHull
    ├─ FCompiledMeshSection InteriorHull     ← épaisseur corrigée
    ├─ FCompiledMeshSection CollisionProxy
    ├─ FCompiledMeshSection OuterEnvelope
    ├─ TArray<FCompiledFrameRingData>
    ├─ TArray<FCompiledBayData>
    ├─ TArray<FCompiledDeckData>
    ├─ TArray<FCompiledFloorRegionData>
    │       └─ FCompiledMeshSection FloorMesh   ← sol conformé à la coque
    ├─ TArray<FCompiledPartitionData>
    │       └─ FCompiledMeshSection PartitionMesh
    ├─ TArray<FCompiledOpeningData>
    ├─ TArray<FCompiledConnectorData>
    ├─ TArray<FCompiledClosureData>
    └─ FDerivedFloodGraph
            │
            ▼  USubmarineBakeSubsystem::BakeRuntime()
    UCompiledSubmarineRuntimeAsset
    (miroir de BaseAsset, séparé pour isoler les données runtime)
            │
            ▼  ASubmarineEditorActor::SpawnOrUpdateRuntimeActor()
    ASubmarineRuntimeActor
    ├─ USubmarineHullComponent
    ├─ USubmarineBreachRuntimeComponent
    ├─ USubmarineFloodRuntimeComponent
    ├─ USubmarineDoorRuntimeComponent
    ├─ UProceduralMeshComponent[] [tag=RuntimeRender]
    └─ UProceduralMeshComponent[] [tag=RuntimeCollision]
```

---

## 2. Bugs corrigés dans cette session

### 2.1 Double-spawn du runtime actor
- **Cause :** `FullBakeAndSpawnRuntimeActor` appelait `FullBakeAssets` (qui déclenchait `SpawnOrUpdateRuntimeActor` via flag) puis appelait `SpawnOrUpdateRuntimeActor` une seconde fois.
- **Fix :** Refactoring via pattern `Impl` — chaque public `UFUNCTION` appelle `ResetMessages()` puis son `Impl` privé. `FullBakeAndSpawnRuntimeActor` orchestre `BakeBaseAssetImpl` + `BakeRuntimeAssetImpl` + `SpawnOrUpdateRuntimeActorImpl` sans double appel. Flag `bAutoSpawnOrUpdateRuntimeActorOnFullBake` mis à `false` par défaut.

### 2.2 Hull sans épaisseur
- **Cause :** La hull intérieure utilisait les mêmes positions que la hull extérieure.
- **Fix :** `EvaluateSectionXY` accepte un `InwardOffsetCm`. Pour l'intérieur : `WallOffset = Ring.WallThicknessCm`, les rayons sont réduits d'autant.

### 2.3 Sols (floors) invisibles
- **Cause :** `FCompiledFloorRegionData` ne contenait que des métadonnées — pas de géométrie.
- **Fix :**
  1. Ajout de `FCompiledMeshSection FloorMesh` dans `FCompiledFloorRegionData`.
  2. `SubmarineFloorBakeService` : helper `BuildFloorStripMesh()` qui échantillonne la largeur intérieure de la coque à la hauteur du pont (Z du deck), crée un strip mesh left/right par tranche de ~50 cm (4 à 64 échantillons selon la longueur).
  3. `SubmarineRuntimeActor` : boucle de rendu + boucle de collision pour `CompiledFloorRegions`.

### 2.4 Tests FloorBakeTests ne compilaient plus
- **Cause :** Signatures `BakeDeckLevels`, `BakeFloorRegions`, `ValidateFloorRegionWalkability` changées de `FSubmarineHullDef` vers `FSub3DCompiledHullData`.
- **Fix :** Helper `BuildHullData()` dans les tests, construit deux sections uniformes (X=0 et X=longueur) à partir des paramètres de test.

---

## 3. Fonctionnalités nouvelles dans cette session

### 3.1 Profils de coque procéduraux

**Enum `ESub3DHullLongitudinalProfile`** (dans `Sub3DCanonicalEnums.h`) :

| Valeur | Description |
|--------|-------------|
| `Manual` | Rings de contrôle placés à la main (comportement historique) |
| `Myring` | Profil torpille classique : power-law nez, cosine-power queue |
| `Series58` | Polynôme US Navy Series 58 body of revolution |
| `SuperellipseLongitudinal` | `\|x/a\|^n = 1 - \|r/b\|^n` — plus anguleux selon l'exposant |
| `Uniform` | Cylindre à calottes hémisphériques |

**Struct `FHullProfileParams`** (dans `Sub3DHullTypes.h`) — ajoutée à `FSubmarineHullDef.ProfileParams` :

| Paramètre | Profil | Plage |
|-----------|--------|-------|
| `MyringNoseExponent` | Myring | 0.5–6.0 |
| `MyringTailAngleDeg` | Myring | 5–60° |
| `MyringNoseFraction` | Myring | 0.05–0.50 |
| `MyringTailFraction` | Myring | 0.05–0.50 |
| `Series58Fineness` | Series58 | 3–15 |
| `LongitudinalExponent` | Superellipse | 1.2–8.0 |
| `ParallelMidbodyFraction` | Tous sauf Manual | 0.0–0.8 |

Tous avec `EditCondition` — un seul groupe de paramètres visible selon le profil sélectionné.

**Service `FSubmarineHullProfileService`** (Sub3DBake) :

- `EvaluateRadiusFraction(Profile, Params, NormalizedX)` — fraction de rayon [0..1] à une position normalisée le long de la spine.
- `GenerateControlRingsFromProfile(Hull, OutRings, OutErrors)` — génère 6 rings typés (Bow Tip, Bow Mid, Bow Shoulder, Stern Shoulder, Stern Mid, Stern Tip) avec les demi-largeurs et demi-hauteurs calculées depuis le profil × dimensions par défaut.

**Intégration bake :**

`USubmarineBakeSubsystem::ResolveEffectiveControlRings(Asset, OutErrors)` :
- Si `Profile != Manual` → appelle `GenerateControlRingsFromProfile`, retourne les rings générés.
- Si échec ou `Manual` → retourne `Asset->ControlRings` (comportement historique).

`ValidateAuthoringAsset` et `BakeBase` utilisent tous les deux `ResolveEffectiveControlRings`. Les assets existants avec `Profile=Manual` (défaut) fonctionnent sans modification.

---

## 4. Architecture des fichiers

### Nouveaux fichiers créés

```
Sub3DBake/
  Public/Bake/SubmarineHullProfileService.h    ← service profils longitudinaux
  Private/Bake/SubmarineHullProfileService.cpp

Sub3DEditor/
  Public/AssetTools/SubmarineAuthoringAssetTypeActions.h  ← double-clic editor
  Private/AssetTools/SubmarineAuthoringAssetTypeActions.cpp

reports/
  SubmarineEditor_OperatorGuide.md             ← guide opérateur
  SubmarineEditor_Handoff_2026_04_04.md        ← ce document
```

### Fichiers modifiés (scope strict)

```
Sub3DCore/
  Public/Types/Sub3DCanonicalEnums.h       + ESub3DHullLongitudinalProfile
  Public/Types/Sub3DHullTypes.h            + FHullProfileParams, FSubmarineHullDef.ProfileParams
  Public/Types/Sub3DFloorTypes.h           + FCompiledFloorRegionData.FloorMesh

Sub3DBake/
  Public/Bake/SubmarineBakeSubsystem.h     + ResolveEffectiveControlRings, include Sub3DHullTypes
  Public/Bake/SubmarineFloorBakeService.h  signatures → FSub3DCompiledHullData
  Private/Bake/SubmarineBakeSubsystem.cpp  + ResolveEffectiveControlRings, include profile service
  Private/Bake/SubmarineFloorBakeService.cpp (réécriture : InterpolateSectionAtX, ComputeInteriorHalfWidthAtZ, BuildFloorStripMesh)
  Private/Bake/SubmarineHullBakeService.cpp  + InwardOffsetCm dans EvaluateSectionXY (épaisseur hull)
  Private/Editor/SubmarineEditorActor.cpp    pattern Impl, fix double-spawn

Sub3DRuntime/
  Private/Actors/SubmarineRuntimeActor.cpp   rendu + collision des FloorRegions

Sub3DEditor/
  Public/Sub3DEditorModule.h               + TSharedPtr AssetTypeActions
  Private/Sub3DEditorModule.cpp            registration asset type actions
  Public/Slate/SubmarineEditorToolkit.h    + GetBakeStatsText()
  Private/Slate/SubmarineEditorToolkit.cpp + stats bake, couleur verte

Sub3DTests/
  Private/Automation/FloorBakeTests.cpp    fix BuildHullData (FSub3DCompiledHullData)
```

---

## 5. Workflow éditeur (référence rapide)

### 5.1 Chemin minimal pour voir la coque

1. Content Browser → Clic droit → Data Asset → `Sub3DSubmarineAuthoringAsset` → Sauvegarder
2. Double-clic sur l'asset → toolkit Submarine Editor s'ouvre
3. Onglet **Ring Architecture** :
   - `Hull.LengthCm` = longueur (ex. 7200)
   - `Hull.DefaultHalfWidthCm / DefaultHalfHeightCm` = dimensions max (ex. 200 / 180)
   - `Hull.DefaultWallThicknessCm` = épaisseur (ex. 12)
   - `Hull.ProfileParams.Profile` = **Myring** (ou autre — pas Manual)
   - → Les 6 rings de contrôle sont auto-générés au bake
   - **OU** `Profile = Manual` + ajouter ≥ 2 Control Rings manuellement
4. Onglet **Validate / Bake** → **Bake + Spawn Runtime Actor**
5. Le runtime actor apparaît dans le viewport avec la coque extérieure, intérieure et collision

### 5.2 Chemin complet pour un sous-marin jouable

| Ordre | Onglet | Action minimale |
|-------|--------|-----------------|
| 1 | Ring Architecture | Hull dims + profil ou rings manuels |
| 2 | Ring Architecture | Frame-Rings avec `bIsBayBoundary = true` (au moins 2) |
| 3 | Outer Envelope | Optionnel — activer le kiosk si nécessaire |
| 4 | Bays | Vérifier que les Structural Bays sont bien dérivées des Frame-Rings |
| 5 | Decks / Floors | DeckLevel par bay + FloorRegion avec StartAlpha / EndAlpha |
| 6 | Openings | Doorways et Hatches |
| 7 | Partitions | PressureBulkheads entre les bays |
| 8 | Validate / Bake | Bake + Spawn Runtime Actor |

**Contraintes bloquantes au bake :**
- ≥ 2 Control Rings (ou profil ≠ Manual)
- PositionX strictement croissant dans [0..LengthCm]
- Frame-Ring SpineAlpha dans [0..1] strictement croissant
- Deck ZOffsetCm dans la plage intérieure de la coque
- Floor headroom ≥ 180 cm, largeur walkable ≥ 60 cm au point médian

---

## 6. Diagnostic rapide

| Symptôme | Cause | Action |
|----------|-------|--------|
| "must be a saved content asset" | Asset dans /Temp/ | Sauvegarder dans le Content Browser |
| Bake échoue, "At least two Control Rings" | Profil = Manual sans rings | Ajouter ≥ 2 rings ou changer de profil |
| Hull visible mais pas d'épaisseur | WallThicknessCm = 0 | Mettre une valeur > 1 |
| Sols invisibles après bake | FloorRegion non configurée | Ajouter FloorRegion avec DeckLevel + bay valides |
| Floor rejeté "headroom < 180 cm" | Deux decks trop proches | Écarter ZOffsetCm d'au moins 180 cm |
| Floor rejeté "width < 60 cm" | Deck trop haut dans la coque (proche du bord) | Descendre ZOffsetCm |
| Double runtime actor dans le niveau | Résidu d'une session | Supprimer l'ancien actor manuellement |
| Stats bake = 0 | Bake n'a pas été relancé après modifications | Rebake |

---

## 7. Tests automation existants

Lancer dans Session Frontend → Automation → filtre `Sub3D` :

| Test ID | Fichier | Ce qui est validé |
|---------|---------|-------------------|
| `Sub3D.Wave2.RingSequenceDeterminism` | RingSequenceTests | Double bake → séquence identique |
| `Sub3D.Wave2.RingSequenceMonotonicity` | RingSequenceTests | SpineAlpha strictement croissant |
| `Sub3D.Wave2.RingSequenceIncludesControlRings` | RingSequenceTests | Tous les control rings représentés |
| `Sub3D.Wave3.FrameRingSequenceIntegration` | RingSequenceTests | Frame-rings mappés dans la séquence |
| `Sub3D.Wave2.HullBakeVertexCount` | HullBakeTests | Compte de vertices = RingCount × RadialSegments |
| `Sub3D.Wave2.HullBakeClosedMesh` | HullBakeTests | Coque extérieure watertight (manifold) |
| `Sub3D.Wave2.HullBakeDeterminism` | HullBakeTests | Double bake → positions identiques |
| `Sub3D.Wave2.CollisionProxySimplified` | HullBakeTests | Collision < rendu en vertices |
| `Sub3D.Wave2.HullOwnershipCount` | HullBakeTests | Ownership triangles = Hull triangles |
| `Sub3D.Wave3.StructuralBaySolveFromAuthoring` | BaySolveTests | Bays explicites → compilées correctement |
| `Sub3D.Wave3.StructuralBaySolveFromFrameBoundaries` | BaySolveTests | Bays dérivées des frame-rings |
| `Sub3D.Wave4.FloorHeadroomValid` | FloorBakeTests | Floor valide avec headroom suffisant |
| `Sub3D.Wave4.FloorHeadroomReject` | FloorBakeTests | Floor rejetée si headroom < 180 cm |
| `Sub3D.Wave4.FloorWidthReject` | FloorBakeTests | Floor rejetée si largeur < 60 cm |
| `Sub3D.Wave4.FloorCountPerBay` | FloorBakeTests | Rejet si decks > bay.MaxDeckLevels |
| `Sub3D.Wave9.RuntimeActorBuildsMeshComponents` | RuntimeActorTests | Actor crée ≥ 3 render + ≥ 1 collision |
| `Sub3D.Wave9.RuntimeActorInitializesRuntimeSystems` | RuntimeActorTests | HullComponent, DoorComponent, BreachComponent, FloodComponent initialisés |

---

## 8. Prochaines étapes gameplay

Le Sub Editor est clos. Les points d'entrée pour le gameplay :

| Priorité | Composant | Travail |
|----------|-----------|---------|
| 1 | `SubmarineBreachRuntimeComponent` | Brancher trigger de brèche (dégât → ouverture de breach) |
| 2 | `SubmarineFloodRuntimeComponent` | Propagation flood par connexions du FloodGraph |
| 3 | `SubmarineDoorRuntimeComponent` | Ouvrir / fermer les Closures, affecter la propagation |
| 4 | `SubmarineHullComponent` | Extraire masse, drag, inertie pour la physique |
| 5 | `SubMovementComponent` | Consommer les paramètres de HullComponent |
| 6 | Matériaux | Assigner les matériaux sur les ProceduralMeshComponents |

Voir `memory/project_gameplay_vision_2026_04_01.md` pour les décisions créatives gameplay.
