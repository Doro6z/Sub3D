# Sub3D — Editor Handoff : Gizmos + Appendages Bake + Hashes + OuterEnvelopeRings
**Date:** 2026-04-04
**Session:** Continuation — complète la liste "Ce qui reste" du handoff précédent

---

## Ce qui a été implémenté dans cette session

### 1. Ring Handle Gizmos — `USubmarineRingHandlesComponent` + `FSubmarineRingHandleVisualizer`

#### Nouveaux fichiers

| Fichier | Module | Rôle |
|---|---|---|
| `Sub3DBake/Public/Editor/SubmarineRingHandlesComponent.h` | Sub3DBake | UActorComponent data carrier (asset ref + selected index) |
| `Sub3DBake/Private/Editor/SubmarineRingHandlesComponent.cpp` | Sub3DBake | Ctor + BindToAsset() |
| `Sub3DEditor/Public/Preview/SubmarineRingHandleVisualizer.h` | Sub3DEditor | FComponentVisualizer déclaration |
| `Sub3DEditor/Private/Preview/SubmarineRingHandleVisualizer.cpp` | Sub3DEditor | Implémentation complète |

#### Architecture

```
ASubmarinePreviewActor
  └── USubmarineRingHandlesComponent  (data: AssetRef + SelectedRingIndex)
         ▲
         │  registered by class name
FSubmarineRingHandleVisualizer          (draws + handles input)
  ├── DrawVisualization()
  │     For each ControlRingDef:
  │     - PDI->SetHitProxy(HSubmarineRingHandleProxy(i))
  │     - DrawWireSphere at (PositionX, 0, 0) — yellow if selected, blue otherwise
  │     - Draw ellipse outline at ring cross-section (HalfWidthCm × HalfHeightCm)
  │     - PDI->SetHitProxy(nullptr)
  │
  ├── VisProxyHandleClick()  → sets EditedComponent + SelectedRingIndex
  │
  ├── HandleInputDelta()
  │     GEditor->BeginTransaction on first delta
  │     Ring.PositionX  += Drag.X  (clamped [0..LengthCm])
  │     Ring.HalfWidthCm  += Drag.Y  (min 1)
  │     Ring.HalfHeightCm += Drag.Z  (min 1)
  │     PreviewActor->RefreshPreview()
  │
  ├── GetWidgetLocation()  → world pos of selected ring (X, 0, 0) in actor space
  │
  └── EndEditing()  → GEditor->EndTransaction + SelectedRingIndex = INDEX_NONE
```

#### Registration (Sub3DEditorModule.cpp)

```cpp
GUnrealEd->RegisterComponentVisualizer(
    USubmarineRingHandlesComponent::StaticClass()->GetFName(),
    MakeShared<FSubmarineRingHandleVisualizer>());
// Unregister in ShutdownModule()
```

#### Interaction utilisateur

1. Ouvrir l'asset → Preview Layout → `ASubmarinePreviewActor` spawné
2. Cliquer sur un handle bleu → anneau sélectionné (jaune) + gizmo de transform UE apparaît
3. Glisser l'axe X → déplace le ring sur la spine
4. Glisser l'axe Y → élargit/rétrécit HalfWidthCm
5. Glisser l'axe Z → ajuste HalfHeightCm
6. Relâcher → transaction Ctrl+Z fermée, preview rafraîchi
7. Cliquer en dehors → désélection

---

### 2. Appendage Bake Service — `FSubmarineAppendageBakeService`

#### Nouveaux fichiers

| Fichier | Rôle |
|---|---|
| `Sub3DBake/Public/Bake/SubmarineAppendageBakeService.h` | Interface namespace Sub3DWave9 |
| `Sub3DBake/Private/Bake/SubmarineAppendageBakeService.cpp` | Générateurs analytiques |

#### Méthodes

| Méthode | Géométrie | Déclencheur |
|---|---|---|
| `BakeSail(Hull, RingSeq, FSailDef, OutSection)` | Box trapézoïdale 6 faces, swept | `Sail.bEnabled == true` |
| `BakeBowDome(Hull, RingSeq, FBowSectionDef, OutSection)` | Demi-ellipsoïde 7 latitudes × 12 longitudes | `BowSection.bEnabled && SonarDomeType != None` |
| `BakeSternFairing(Hull, RingSeq, FSternSectionDef, OutSection)` | Cône tronqué 12 segments + disque de fermeture | `SternSection.bEnabled && SternFairingLengthCm > 0` |

**Détail sail (8 sommets, 12 triangles) :**
```
Base corners at  (X ± BaseChord/2,  ±Beam/2,  HullTopZ)
Top  corners at  (X ± TopChord/2 + sweep,  ±Beam/2,  HullTopZ + HeightCm)
Faces: bottom, top, fwd, aft, port, starboard
```

**Détail bow dome :**
```
Half-ellipsoid: center at X=0 (bow), extends to X = -SonarDomeLengthCm
Radius = SonarDomeDiamCm/2 in Y and Z
φ ∈ [0, π/2] — triangle fan at tip, quads on lateral rings
```

**Détail stern fairing :**
```
Base ring at X=LengthCm, radius=PropulsorDiamCm/2, 12 segments
Tip at X=LengthCm + SternFairingLengthCm
Triangle fan + base disk cap
```

---

### 3. OuterEnvelopeRings dans le preview — Section 1

`ASubmarinePreviewActor::RefreshPreview()` — **MODIFIÉ** :

```
Section 0 — Exterior hull           (toujours baked)
Section 1 — Outer envelope rings    (si Asset->OuterEnvelopeRings.Num() >= 2)
Section 2 — Sail                    (si FSailDef.bEnabled)
Section 3 — Bow dome                (si FBowSectionDef.bEnabled)
Section 4 — Stern fairing           (si FSternSectionDef.bEnabled)
```

La section 1 réutilise `FSubmarineRingSequenceBuilder::BuildRingSequence()` + `BakeExteriorHull()` avec les `OuterEnvelopeRings` plutôt que les `ControlRings`.

---

### 4. HullGeometryHash / LayoutHash — BakeSubsystem

`SubmarineBakeSubsystem.cpp` — **MODIFIÉ** — après `LastBakedBaseAsset = BakedAsset` :

```cpp
Asset->Modify();
Asset->HullGeometryHash = ComputeHullGeometryHash(Asset);
Asset->LayoutHash       = ComputeLayoutHash(Asset);
```

**`ComputeHullGeometryHash`** — CRC32 cumulatif de :
- `Hull.LengthCm`, `DefaultHalfWidthCm`, `DefaultHalfHeightCm`, `DefaultWallThicknessCm`
- Tous les champs de `ProfileParams` (Profile enum + 7 floats)
- Pour chaque `ControlRingDef` : PositionX, HalfWidthCm, HalfHeightCm, WallThicknessCm, SectionProfile, SectionRoundness

**`ComputeLayoutHash`** — CRC32 de :
- Pour chaque `FStructuralBayDef` : StartAlpha, EndAlpha
- Pour chaque `FDeckLevelDef` : ZOffsetCm

Ces hashes permettent de détecter si un asset Layer B/C est devenu stale après modification du hull.

---

## État global après cette session — tous les items "reste" traités

| Item | État |
|---|---|
| Ring handle gizmos (drag viewport) | ✅ Implémenté |
| Bake des appendages (sail/bow/stern) | ✅ Implémenté (preview) |
| HullGeometryHash / LayoutHash CRC | ✅ Implémenté |
| OuterEnvelopeRings dans le preview | ✅ Implémenté |

---

## Ce qui reste (hors scope de cette session)

| Fonctionnalité | Effort | Notes |
|---|---|---|
| bHullGeometryConfirmed — UI de lock/unlock | Petit | Champ présent, pas de bouton "Confirm" dans le toolkit |
| Hash stale warning dans le toolkit | Petit | Comparer `HullGeometryHash` au hash live dans `GetContractStatusText` |
| Appendage bake full (CompiledSubmarineBaseAsset) | Moyen | Ajouter `SailMesh`, `BowMesh`, `SternMesh` dans le compiled asset |
| Sort automatique des ControlRings après drag | Petit | Après drag X, re-trier `ControlRings` par PositionX |
| Preset system (Apply Preset button) | Bas | Tableau défini dans le vision doc |

---

## Checklist de validation

```
□ Build solution complète sans erreur
□ Ouvrir un AuthoringAsset dans l'éditeur → Preview Layout
□ Rings visibles en bleu dans le viewport
□ Cliquer un ring → jaune + ellipse outline + gizmo transform
□ Glisser axe X → ring se déplace, preview rafraîchi
□ Ctrl+Z → ring revient à sa position
□ Activer Sail.bEnabled → section 2 visible dans le preview
□ Activer BowSection.bEnabled → demi-ellipsoïde visible en proue
□ Activer SternSection.bEnabled → cône en poupe
□ Ajouter >= 2 OuterEnvelopeRings → section 1 visible
□ Bake Assets → HullGeometryHash != 0 dans l'asset
```

---

## Fichiers modifiés dans cette session

| Fichier | Type |
|---|---|
| `Sub3DBake/Public/Editor/SubmarinePreviewActor.h` | Modifié — `RingHandlesComponent` + section consts |
| `Sub3DBake/Private/Editor/SubmarinePreviewActor.cpp` | Réécrit — 5 sections + appendages |
| `Sub3DBake/Private/Bake/SubmarineBakeSubsystem.cpp` | Modifié — hash helpers + appel post-bake |
| `Sub3DEditor/Public/Sub3DEditorModule.h` | Modifié — RingHandleVisualizer member |
| `Sub3DEditor/Private/Sub3DEditorModule.cpp` | Réécrit — register/unregister visualizer |

### Nouveaux fichiers (6)

| Fichier |
|---|
| `Sub3DBake/Public/Bake/SubmarineAppendageBakeService.h` |
| `Sub3DBake/Private/Bake/SubmarineAppendageBakeService.cpp` |
| `Sub3DBake/Public/Editor/SubmarineRingHandlesComponent.h` |
| `Sub3DBake/Private/Editor/SubmarineRingHandlesComponent.cpp` |
| `Sub3DEditor/Public/Preview/SubmarineRingHandleVisualizer.h` |
| `Sub3DEditor/Private/Preview/SubmarineRingHandleVisualizer.cpp` |
