# Sub3D — Editor Handoff: Appendages + Preview (2026-04-04)

## Résumé

Session complète. Toutes les fonctions manquantes implémentées. Nouveaux types d'appendages. Asset étendu. Widgets dédiés. Build devrait passer sans modification supplémentaire.

---

## État des lieux — ce qui est implémenté

### Sub3DCore — Nouveaux types

| Fichier | Contenu |
|---|---|
| `Types/Sub3DAppendageTypes.h` | **NOUVEAU** — 6 enums + 4 structs |

Enums :
- `ESub3DSailStyle` — TallNarrow / WideSlab / FinnedAlpha / LowProfile / Retracted / SciFiPod / Custom
- `ESub3DSonarDomeType` — Hemisphere / Conformal / Extended / CylindricalFlat / SciFiPlate / None
- `ESub3DBowPlaneConfig` — None / BowPlanes / CanardFins
- `ESub3DPropulsorType` — SevenBladedSkewback / PumpJet / ContradRotating / PoddedAzimuth / MHD / DualShaft / SciFiNacelle
- `ESub3DControlSurfaceArrangement` — CrossPattern / XPattern / YPattern / SingleRudder / SciFiVectorNozzle
- `ESub3DAnechoicCoating` — None / PartialStrakes / FullBody / Conformal / SciFiActive

Structs (avec GENERATED_BODY + EditCondition) :
- `FSailDef` — kiosque/fin, fairwater planes, mâts, écoutille
- `FBowSectionDef` — dôme sonar, plans d'étrave, tubes torpilles, carène de quille
- `FSternSectionDef` — gouvernes, propulseur, antenne remorquée
- `FOuterCasingDef` — pont extérieur, revêtement anéchoïque, troncs de ventilation

---

### Sub3DBake — AuthoringAsset étendu

`SubmarineAuthoringAsset.h` — **MODIFIÉ** — catégories remontées en [A]/[B]/[C] :

```
[A] Pressure Hull    — FSubmarineHullDef, ControlRings, FrameRings
[A] Appendages       — FSailDef, FBowSectionDef, FSternSectionDef
[A] Outer Envelope   — FOuterEnvelopeDef, FOuterCasingDef, OuterEnvelopeRings[]
[A] Bake State       — bHullGeometryConfirmed, HullGeometryHash
[B] Structural Bays  — FStructuralBayDef[]
[B] Deck Levels      — FDeckLevelDef[]
[B] Floor Regions    — FFloorRegionDef[]
[B] Openings         — FOpeningDef[]
[B] Bake State       — LayoutHash
[C] Connectors       — FConnectorDef[]
[C] Closures         — FClosureDef[]
[C] Partitions       — FPressureBulkheadDef[], FInternalWallDef[]
```

Champs nouveaux :
- `FSailDef Sail`
- `FBowSectionDef BowSection`
- `FSternSectionDef SternSection`
- `FOuterCasingDef OuterCasing`
- `TArray<FControlRingDef> OuterEnvelopeRings` — chaîne de rings secondaire pour l'enveloppe
- `bool bHullGeometryConfirmed` — verrou couche A
- `uint32 HullGeometryHash` — CRC32 Hull + ControlRings
- `uint32 LayoutHash` — CRC32 StructuralBays + DeckLevels

---

### Sub3DEditor — Widgets Slate

| Fichier | Classe | Rôle |
|---|---|---|
| `Public/Slate/SSubmarineHullStatsBar.h` | `SSubmarineHullStatsBar` | Barre de stats hull (LOA, Beam, L/D, Profile, Rings) |
| `Private/Slate/SSubmarineHullStatsBar.cpp` | — | Implémentation, bindings lambda live |
| `Public/Slate/SSubmarineAppendagesPanel.h` | `SSubmarineAppendagesPanel` | Panel Appendages avec badges d'état |
| `Private/Slate/SSubmarineAppendagesPanel.cpp` | — | ✓ Sail / ○ Bow etc. en couleur |

`SSubmarineHullStatsBar` — live, se rafraîchit chaque tick Slate :
```
LOA: 7200 cm (72.0 m) | Beam: 360 cm (3.6 m) | L/D: 10.0 | Profile: Myring | Rings: 0 manual + 6 auto
```

`SSubmarineAppendagesPanel` — badges + details view intégrée :
```
[✓ Sail]  [○ Bow]  [○ Stern]  [○ Casing]
─────────────────────────────────────────
[Details View — [A] Appendages]
```

---

### Sub3DEditor — Toolkit

`SubmarineEditorToolkit.h` — **MODIFIÉ** :
- `AppendagesTabId` ajouté
- `SpawnAppendagesTab` déclarée
- `AppendagesDetailsView` membre ajouté

`SubmarineEditorToolkit.cpp` — **MODIFIÉ** :
- Layout bumped `v10 → v11` (ajout de l'onglet Appendages)
- `FindOrCreatePreviewActor()` — implémenté
- `HandlePreviewLayout()` — implémenté
- `OnPreviewPropertiesChanged()` — implémenté
- `SpawnAppendagesTab()` — implémenté
- `EnvelopeDetailsView` branché sur `OnPreviewPropertiesChanged`
- Filtres mis à jour vers catégories `[A]/[B]/[C]`
- `BuildContractWarnings` étendu : checks Sail HeightCm, BowSection/SternSection vs beam

---

## Onglets de l'éditeur — état final

| Onglet | Filtre catégorie | Widget | Notes |
|---|---|---|---|
| Drydock | Aucun (tous champs) | inline Slate | Buttons + AuthoringDetailsView complète |
| Ring Architecture | `[A] Pressure Hull` | `SSubmarineHullStatsBar` + RingDetailsView | Auto-refresh preview sur changement |
| Outer Envelope | `[A] Outer Envelope`, `Outer Envelope` | BuildPhasePanel | Inclut OuterCasing et OuterEnvelopeRings |
| **Appendages** | `[A] Appendages` | `SSubmarineAppendagesPanel` | **NOUVEAU** |
| Bays | `[B] Structural Bays` | BuildPhasePanel | Draw Structural Bays button |
| Decks / Floors | `[B] Deck Levels`, `[B] Floor Regions` | BuildPhasePanel | — |
| Openings | `[B] Openings`, `[C] Connectors`, `[C] Closures` | BuildPhasePanel | — |
| Partitions | `[C] Partitions` | BuildPhasePanel | — |
| Validate / Bake | Aucun | inline Slate | Stats + contract warnings |

---

## Preview — flux complet

```
[Preview Layout button] / [propriété ring change]
    │
    ▼
HandlePreviewLayout() / OnPreviewPropertiesChanged()
    │
    ▼
FindOrCreatePreviewActor()
    ├── Cherche ASubmarinePreviewActor dans le world
    ├── Trouve → InitializeForAsset() (re-bind au cas où)
    └── Pas trouvé → SpawnActor + InitializeForAsset()
    │
    ▼
ActivePreviewActor = found/new
    │
    ▼
RefreshPreview()
    ├── GenerateControlRingsFromProfile() si Profile != Manual
    ├── BuildRingSequence(8 rings)
    ├── BakeExteriorHull(12 segments, no collision)
    └── UploadMeshSection(section 0)
```

Auto-refresh : déclenché uniquement si `ActivePreviewActor.IsValid()`. L'actor n'est jamais spawné automatiquement — l'utilisateur clique **Preview Layout** une première fois.

---

## Assets à créer dans l'éditeur UE

| Asset | Type | Où | Contenu minimal |
|---|---|---|---|
| `DA_SubTest_Compact` | `Sub3DSubmarineAuthoringAsset` | `/Game/Submarines/Authoring/` | Hull.LengthCm=3000, DefaultHalfWidthCm=175, Profile=Myring |
| `DA_SubTest_Standard` | `Sub3DSubmarineAuthoringAsset` | `/Game/Submarines/Authoring/` | Hull.LengthCm=6500, DefaultHalfWidthCm=350, Profile=Series58 |
| `DA_SubTest_Manual` | `Sub3DSubmarineAuthoringAsset` | `/Game/Submarines/Authoring/` | Profile=Manual + 6 rings manuels |

---

## Checklist d'ouverture de l'éditeur

1. Double-clic sur un `Sub3DSubmarineAuthoringAsset` dans le Content Browser
2. L'éditeur s'ouvre sur l'onglet **Drydock**
3. Passer à **Ring Architecture** → barre de stats visible en haut
4. Cliquer **Preview Layout** → `ASubmarinePreviewActor` spawné dans le viewport
5. Modifier `Hull.ProfileParams.Profile` → le preview se rafraîchit
6. Passer à **Appendages** → badges `○ Sail / ○ Bow / ○ Stern / ○ Casing`
7. Activer `Sail.bEnabled = true` → badge passe `✓ Sail` en vert
8. Passer à **Validate / Bake** → `Contract Status: quick checks passed`
9. Cliquer **Bake Assets** → stats géométrie affichées

---

## Ce qui n'est PAS encore implémenté

| Fonctionnalité | Priorité | Notes |
|---|---|---|
| Bake des appendages (sail/bow/stern) | Moyenne | Structs présents, service de bake manquant |
| Preview appendages visuels | Moyenne | ASubmarinePreviewActor ne bake que la coque extérieure |
| Ring handle gizmos in viewport | Haute (UX) | Drag XYZ sur rings — USubmarineRingHandleComponent à créer |
| Chaîne OuterEnvelopeRings dans le preview | Moyenne | Section mesh 1 à ajouter dans ASubmarinePreviewActor |
| Presets système (bouton Apply Preset) | Basse | Tableau de presets défini dans vision doc |
| bHullGeometryConfirmed — logique de lock | Moyenne | Champ présent, aucune UI de lock/unlock |
| HullGeometryHash / LayoutHash — calcul CRC | Moyenne | Champs présents, calcul à implémenter dans BakeSubsystem |

---

## Notes de migration — assets existants

Les `USub3DSubmarineAuthoringAsset` existants ont des catégories `"Pressure Hull"`, `"Outer Envelope"` etc. dans les anciennes versions. Ces propriétés restent sérialisées (les données ne sont pas perdues) mais n'apparaîtront plus dans les onglets filtrés puisque les catégories ont changé en `"[A] Pressure Hull"` etc.

**Action requise :** Ouvrir chaque asset existant → re-sauvegarder → les catégories sont mises à jour dans les métadonnées de réflexion UE. Les valeurs sont préservées.

L'onglet **Drydock** (filtre désactivé, AuthoringDetailsView complet) affiche TOUS les champs indépendamment des catégories — aucune donnée n'est masquée.
