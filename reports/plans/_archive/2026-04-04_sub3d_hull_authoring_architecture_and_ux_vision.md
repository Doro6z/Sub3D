# Sub3D — Hull Authoring Architecture & UX Vision
**Date:** 2026-04-04
**Status:** Design reference — dev-only asset, single AuthoringAsset for now
**Scope:** Full submarine envelope definition, editor UX layout, layered modification model, size matrix, sci-fi extension paths

---

## 1. Design Axes

### 1.1 Realistic Submarines — the reference corpus

| Class | Nation | LOA (m) | Beam (m) | L/D | Notes |
|---|---|---|---|---|---|
| Suffren (Barracuda) | France | 99.5 | 8.8 | 11.3 | Modern nuclear attack, teardrop hull |
| A26 (Blekinge) | Sweden | 66 | 6.5 | 10.2 | AIP, conventional |
| Virginia Block V | USA | 115 | 10.4 | 11.1 | Large SSN, enlarged midbody |
| Dolphin II | Israel | 68 | 6.8 | 10.0 | Export Type 212 variant |
| Type 212 | Germany | 57 | 7.0 | 8.1 | Compact, AIP, air-independent |
| Kilo (Varshavyanka) | Russia | 73.8 | 9.9 | 7.5 | Wide, quiet diesel |
| Ohio (SSBN) | USA | 170.7 | 12.8 | 13.3 | Very long, large parallel midbody |
| Narwhal (sci-fi compact) | — | 30 | 4.0 | 7.5 | Fictional compact, crew of 4 |
| Leviathan (sci-fi dreadnought) | — | 250 | 22.0 | 11.4 | Fictional capital sub |

**Key proportions to reproduce:**
- Bow: short power-law taper (Myring n=2–3.5), sonar dome prominent
- Midbody: long parallel section 30–55% of total length
- Stern: longer than bow, concave tail, propulsor fairing
- Casing: flat-deck casing above pressure hull for walkway + mast fairings
- Fin/Sail: tall and narrow (attack), wide-base slab (early cold-war), retracted (AIP)

### 1.2 Sci-Fi / Abstract Extension Paths

The system must not force realism. Sci-fi / abstract forms come from:
- Extreme L/D ratios (< 5 or > 16)
- Non-circular cross-section all along (rectangular, triangular, star)
- Multiple pressure hulls (catamaran, SWATH)
- External drive nacelles (not on axis)
- Integrated weapon blisters, organic protrusions
- Entirely custom longitudinal profile via Manual control rings

The `ESub3DSectionProfile::Superellipse` + high `SectionRoundness` → box cross-section.
The `ESub3DHullLongitudinalProfile::Manual` → any silhouette, no mathematical constraint.

---

## 2. Editor UX Layout

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Submarine Hull Authoring — [Asset Name]                               [? Help] │
├─────────────────┬──────────────────────────────────────┬────────────────────────┤
│  LEFT PANEL     │         3D VIEWPORT                  │  RIGHT PANEL           │
│  ─────────────  │                                      │  ─────────────────     │
│  PRESET         │  ┌──────────────────────────────┐    │  SELECTED RING         │
│  [Myring 7:1 ▼] │  │                              │    │  ─────────────────     │
│  [Series 58 ▼]  │  │   ○ ── ──────────────── ── ○ │    │  ID: Ring_03           │
│  [Manual ▼]     │  │  ╱                            ╲   │  X: 2400 cm            │
│                 │  │ ╱    ← ring handles (◆)       ╲  │  HalfW: 350 cm         │
│  HULL SHAPE     │  │╱                                ╲ │  HalfH: 280 cm         │
│  Profile: Myring│  │  parallel midbody               │ │  Profile: Superellipse │
│  L/D: ──●── 9.0 │  │                                  │ │  Roundness: ──●── 0.8  │
│  Midbody: 40%   │  │  ◆ ◆  ◆──────────◆  ◆ ◆         │ │  WallThick: 14 cm      │
│  NoseExp: 2.5   │  │                                  │ │                        │
│  TailAngle: 24° │  └──────────────────────────────┘    │  [+ Add Ring]          │
│                 │                                      │  [× Delete Ring]       │
│  ─────────────  │  View: [Exterior] [Wireframe] [X-Ray]│  [↑↓ Reorder]         │
│  APPENDAGES     │  Rings: [Show Handles] [Lock Scale]  │                        │
│  [+] Sail       │                                      │  ─────────────────     │
│  [+] Bow Form   │  Stats: L=7200cm  MaxR=360cm  L/D=10 │  VIEW FILTERS          │
│  [+] Stern Form │  Rings: 8  Segs: 24 (preview)        │  ☑ Pressure Hull       │
│  [+] Outer Casing│                                     │  ☑ Wall Thickness      │
│                 │                                      │  ☑ Frame-Rings         │
│  ─────────────  │                                      │  ☐ Outer Casing        │
│  SIZE PRESETS   │                                      │  ☐ Sail                │
│  [Compact 30m]  │                                      │  ☐ Bow Section         │
│  [Standard 70m] │                                      │  ☐ Stern Section       │
│  [Large 100m]   │                                      │                        │
│  [Dreadnought]  │                                      │  BAKE PATHS            │
│  [Custom]       │                                      │  [Validate]            │
│                 │                                      │  [Bake Preview]        │
│                 │                                      │  [Bake & Spawn]        │
└─────────────────┴──────────────────────────────────────┴────────────────────────┘
```

**Interaction model:**
- Ring handles in viewport are draggable (GizmoComponents, not FEdMode)
- Dragging X: moves ring along spine, auto-sorts by PositionX
- Dragging Y/Z: adjusts HalfWidth/HalfHeight
- Property panel updates live; viewport refreshes via `ASubmarinePreviewActor::RefreshPreview()`
- Preset dropdown → populates ProfileParams + sensible defaults → refreshes

---

## 3. Layered Modification Model

| Layer | Content | Who edits | Lock condition |
|---|---|---|---|
| **A — Hull Geometry** | Pressure hull, control rings, frame-rings, outer envelope | Dev editor only, major refit | bHullGeometryConfirmed = true |
| **B — Structure** | Structural bays, deck levels, floor regions, openings | Shipyard (constrained edit) | Depends on Layer A hash |
| **C — Fit-Out** | Bulkheads, internal walls, doors, equipment, room tags | Interior refit | Depends on Layer B hash |
| **D — Runtime State** | Open/closed, breach, flood, mission states | Runtime / game logic | Never locked at edit time |

### 3.1 Hash Invalidation Chain

```
FSubmarineHullDef  →  HullGeometryHash  →  invalidates Layer B compiled data
FStructuralBayDef  →  LayoutHash        →  invalidates Layer C compiled data
```

```cpp
// In AuthoringAsset:
UPROPERTY(VisibleAnywhere, Category="Bake State")
bool bHullGeometryConfirmed = false;

UPROPERTY(VisibleAnywhere, Category="Bake State")
uint32 HullGeometryHash = 0;          // CRC32 of Hull + ControlRings

UPROPERTY(VisibleAnywhere, Category="Bake State")
uint32 LayoutHash = 0;                // CRC32 of StructuralBays + DeckLevels
```

---

## 4. Hull Definition — Complete Struct Set

### 4.1 FSubmarineHullDef (existing, Sub3DCore)

```cpp
USTRUCT(BlueprintType)
struct FSubmarineHullDef
{
    FName    HullId;
    float    LengthCm             = 7200.0f;
    float    DefaultHalfWidthCm   = 180.0f;
    float    DefaultHalfHeightCm  = 180.0f;
    ESub3DSectionProfile  DefaultSectionProfile  = ESub3DSectionProfile::Ellipse;
    float    DefaultSectionRoundness = 0.5f;
    float    DefaultWallThicknessCm  = 12.0f;
    FHullProfileParams ProfileParams;
};
```

### 4.2 FHullProfileParams (existing, Sub3DCore)

```cpp
USTRUCT(BlueprintType)
struct FHullProfileParams
{
    ESub3DHullLongitudinalProfile Profile = ESub3DHullLongitudinalProfile::Manual;

    // Myring — power-law nose + cosine-power tail
    float MyringNoseExponent  = 2.0f;   // 1–4, higher = blunter
    float MyringTailAngleDeg  = 25.0f;  // 15–35°
    float MyringNoseFraction  = 0.20f;  // 0.05–0.5
    float MyringTailFraction  = 0.25f;  // 0.05–0.5

    // Series 58 — US Navy polynomial
    float Series58Fineness    = 7.0f;   // L/D 3–15

    // Superellipse longitudinal
    float LongitudinalExponent = 2.5f;  // 1.2–8, 2.0 = ellipse, 8 = box

    // Shared
    float ParallelMidbodyFraction = 0.4f; // 0–0.8
};
```

### 4.3 FSailDef (to add — Sub3DCore or Sub3DBake)

```cpp
UENUM(BlueprintType)
enum class ESub3DSailStyle : uint8
{
    TallNarrow,       // Modern SSN — Barracuda, Virginia
    WideSlab,         // Cold-war Soviet — Kilo, Victor
    FinnedAlpha,      // ALPHA/Lira fairing
    LowProfile,       // AIP / shallow-water — Type 212
    Retracted,        // Fully retracted, hull-flush
    SciFiPod,         // Detached pod, linked by pylons
    Custom            // Manual rings define sail shape
};

USTRUCT(BlueprintType)
struct FSailDef
{
    GENERATED_BODY()

    bool  bEnabled            = false;

    // Position
    float SpineAlpha          = 0.45f;  // 0..1 along hull spine
    float LateralOffsetCm     = 0.0f;   // for twin-sail or offset configs
    float BaseHeightOffsetCm  = 0.0f;   // adjustment above hull surface

    // Shape
    ESub3DSailStyle Style     = ESub3DSailStyle::TallNarrow;
    float HeightCm            = 600.0f; // total sail height above hull top
    float BaseChordCm         = 220.0f; // fore-aft extent at base
    float TopChordCm          = 140.0f; // fore-aft extent at top
    float BeamCm              = 85.0f;  // max lateral width

    // Leading / trailing edge sweep
    float LeadingEdgeSweepDeg  = 15.0f; // aft-sweep of leading edge
    float TrailingEdgeSweepDeg = 8.0f;

    // Fairwater planes
    bool  bHasFairwaterPlanes  = true;
    float FairwaterSpanCm      = 350.0f;
    float FairwaterChordCm     = 120.0f;
    float FairwaterDihedralDeg = 0.0f;  // +ve = anhedral

    // Masts (conceptual count — detail at runtime/C level)
    int32 MastCount            = 4;

    // Hatch at sail top
    bool  bHasSailHatch        = true;
    float SailHatchDiamCm      = 65.0f;
};
```

### 4.4 FBowSectionDef (to add)

```cpp
UENUM(BlueprintType)
enum class ESub3DSonarDomeType : uint8
{
    Hemisphere,    // Classic sphere — Kilo, early SSNs
    Conformal,     // Low-drag teardrop — Virginia, Barracuda
    Extended,      // Elongated cylinder — Skipjack style
    CylindricalFlat, // Flat-faced cylinder — Type 212 / AIP compact
    SciFiPlate,    // Large forward sensor plate
    None           // No dome — raw bow
};

UENUM(BlueprintType)
enum class ESub3DBowPlaneConfig : uint8
{
    None,
    BowPlanes,        // Forward horizontal planes
    RetractableSail,  // Planes on sail (counted here for completeness)
    CanardFins        // Sci-fi forward control fins
};

USTRUCT(BlueprintType)
struct FBowSectionDef
{
    GENERATED_BODY()

    bool bEnabled = false;

    // Sonar dome
    ESub3DSonarDomeType  SonarDomeType    = ESub3DSonarDomeType::Conformal;
    float                SonarDomeLengthCm = 180.0f;
    float                SonarDomeDiamCm   = 340.0f; // often ≈ hull beam at bow

    // Bow planes
    ESub3DBowPlaneConfig BowPlaneConfig   = ESub3DBowPlaneConfig::None;
    float                BowPlaneSpanCm   = 250.0f;
    float                BowPlaneChordCm  = 80.0f;
    float                BowPlaneSpineAlpha = 0.06f; // position along hull

    // Torpedo tubes (Visual only at this layer — gameplay at C level)
    int32  TorpedoTubeCount   = 4;
    float  TorpedoTubeDiamCm  = 53.3f;   // 533mm standard
    bool   bTubesAngled       = false;    // angled outward like Type 212
    float  TubeAngleDeg       = 8.0f;

    // Minelaying keel opening
    bool   bHasKeel           = false;
};
```

### 4.5 FSternSectionDef (to add)

```cpp
UENUM(BlueprintType)
enum class ESub3DPropulsorType : uint8
{
    SevenBladedSkewback,  // Standard SSN/SSBN
    PumpJet,              // Barracuda, Astute, Virginia
    ContradRotating,      // Alfa/Lira style
    PoddedAzimuth,        // Some AIP / export
    MHD,                  // Magnetohydrodynamic (sci-fi / Seawolf quiet)
    DualShaft,            // Twin screws — Ohio early, Type 209
    SciFiNacelle          // External drive pod
};

UENUM(BlueprintType)
enum class ESub3DControlSurfaceArrangement : uint8
{
    CrossPattern,      // + shape — most submarines
    XPattern,          // × shape — Type 212, Collins
    YPattern,          // Y stern — ALFA class
    SingleRudder,      // Simple vertical only
    SciFiVectorNozzle  // Articulated thrust vectoring
};

USTRUCT(BlueprintType)
struct FSternSectionDef
{
    GENERATED_BODY()

    bool bEnabled = false;

    // Control surfaces
    ESub3DControlSurfaceArrangement ControlArrangement = ESub3DControlSurfaceArrangement::CrossPattern;
    float  RudderSpanCm    = 260.0f;
    float  RudderChordCm   = 120.0f;
    float  ElevatorSpanCm  = 260.0f;
    float  ElevatorChordCm = 120.0f;

    // Propulsor
    ESub3DPropulsorType  PropulsorType     = ESub3DPropulsorType::SevenBladedSkewback;
    float                PropulsorDiamCm   = 380.0f;
    float                SternFairingLengthCm = 220.0f;
    bool                 bHasPropGuard     = false;

    // Towed array
    bool   bHasTowedArrayFairing = true;
    float  TowedArraySpineAlpha  = 0.88f;  // where fairing starts
    float  TowedArrayLengthCm    = 240.0f;
    float  TowedArrayDiamCm      = 14.0f;
};
```

### 4.6 FOuterCasingDef (to add — extends FOuterEnvelopeDef)

```cpp
UENUM(BlueprintType)
enum class ESub3DAnechoicCoating : uint8
{
    None,
    PartialStrakes,   // Panels on flank/bottom only
    FullBody,         // Entire casing tiled
    Conformal,        // Flush-mount, minimal visible seams
    SciFiActive       // Animated / mission-programmable
};

USTRUCT(BlueprintType)
struct FOuterCasingDef
{
    GENERATED_BODY()

    bool bEnabled = false;

    // Deck casing (the flat upper walkway structure)
    bool  bHasDeckCasing     = true;
    float DeckCasingWidthCm  = 260.0f;  // typically 60–80% of max beam
    float DeckCasingHeightCm = 40.0f;   // thickness above hull top
    float DeckCasingFwdAlpha = 0.12f;   // where casing starts
    float DeckCasingAftAlpha = 0.92f;   // where casing ends

    // Free-flood limber holes — visual only
    int32 LimberHoleCountPerSide = 6;

    // Anechoic coating
    ESub3DAnechoicCoating AnechoicType = ESub3DAnechoicCoating::None;

    // Outer envelope (re-exposed from FOuterEnvelopeDef intent)
    // NOTE: Outer envelope is a second small ring chain around the sail base
    // It is authored via its own control rings in OuterEnvelopeRings[] on the asset.

    // Ballast tank vent structures (visual bumps on deck — simple boxes)
    bool  bHasMainVentTrunks = false;
    int32 VentTrunkCount     = 4;
};
```

---

## 5. Full AuthoringAsset — Extended Vision

```cpp
UCLASS(BlueprintType)
class USub3DSubmarineAuthoringAsset : public UPrimaryDataAsset
{
    // ─── [A] Hull Geometry — Dev editor / major refit only ─────────────────────
    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    FSubmarineHullDef Hull;

    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    TArray<FControlRingDef> ControlRings;

    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    TArray<FFrameRingDef> FrameRings;

    // Appendages — also Level A (define the external geometry)
    UPROPERTY(EditAnywhere, Category="[A] Appendages")
    FSailDef Sail;

    UPROPERTY(EditAnywhere, Category="[A] Appendages")
    FBowSectionDef BowSection;

    UPROPERTY(EditAnywhere, Category="[A] Appendages")
    FSternSectionDef SternSection;

    UPROPERTY(EditAnywhere, Category="[A] Outer Envelope")
    FOuterEnvelopeDef OuterEnvelope;

    UPROPERTY(EditAnywhere, Category="[A] Outer Envelope")
    FOuterCasingDef OuterCasing;

    UPROPERTY(EditAnywhere, Category="[A] Outer Envelope")
    TArray<FControlRingDef> OuterEnvelopeRings; // second ring chain for envelope

    // Bake state
    UPROPERTY(VisibleAnywhere, Category="[A] Bake State")
    bool bHullGeometryConfirmed = false;

    UPROPERTY(VisibleAnywhere, Category="[A] Bake State")
    uint32 HullGeometryHash = 0;

    // ─── [B] Structure — Shipyard editable ─────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="[B] Structural Bays")
    TArray<FStructuralBayDef> StructuralBays;

    UPROPERTY(EditAnywhere, Category="[B] Deck Levels")
    TArray<FDeckLevelDef> DeckLevels;

    UPROPERTY(EditAnywhere, Category="[B] Floor Regions")
    TArray<FFloorRegionDef> FloorRegions;

    UPROPERTY(EditAnywhere, Category="[B] Openings")
    TArray<FOpeningDef> Openings;

    UPROPERTY(VisibleAnywhere, Category="[B] Bake State")
    uint32 LayoutHash = 0;

    // ─── [C] Fit-Out — Interior refit ──────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="[C] Partitions")
    TArray<FPressureBulkheadDef> PressureBulkheads;

    UPROPERTY(EditAnywhere, Category="[C] Partitions")
    TArray<FInternalWallDef> InternalWalls;

    UPROPERTY(EditAnywhere, Category="[C] Connectors")
    TArray<FConnectorDef> Connectors;

    UPROPERTY(EditAnywhere, Category="[C] Closures")
    TArray<FClosureDef> Closures;
};
```

---

## 6. Size Matrix

| Archetype | LOA | Beam | L/D | DefaultHalfWidthCm | LengthCm | Crew | Profile hint |
|---|---|---|---|---|---|---|---|
| Compact scout | 30 m | 4 m | 7.5 | 200 | 3000 | 4–6 | Myring, nose=2.5 |
| Standard patrol | 60–70 m | 7 m | 9.0 | 350 | 6500 | 25–40 | Myring/Series58 |
| Nuclear attack | 95–110 m | 10 m | 10.5 | 500 | 10000 | 70–100 | Series58, midbody=45% |
| Ballistic missile | 150–175 m | 13 m | 12.5 | 650 | 16000 | 150 | Myring, midbody=55% |
| Sci-fi compact | 20 m | 3.5 m | 5.7 | 175 | 2000 | 2–4 | Manual or Superellipse |
| Sci-fi dreadnought | 220–260 m | 20 m | 12.0 | 1000 | 24000 | 200+ | Series58 or Manual |

**Relation to WallThicknessCm:**
- Scale thumb-rule: `WallThicknessCm ≈ MaxBeam × 0.025` (roughly 2.5% of beam)
- Compact 4m beam → ~10 cm
- Standard 7m beam → ~14 cm
- Large 10m beam → ~18–22 cm
- Sci-fi / exotic → free, no constraint (can be 50+ cm for armored hull)

---

## 7. Hull Longitudinal Profile — Mathematical Reference

### 7.1 Myring Profile (standard implementation)

```
Nose section (0 ≤ t ≤ NoseFraction):
    x_norm = t / NoseFraction
    r(t) = 1 - (1 - x_norm)^n            where n = MyringNoseExponent

Parallel midbody (NoseFraction ≤ t ≤ 1 - TailFraction - ParallelMidbodyFraction):
    r(t) = 1.0

Tail section (tail_start ≤ t ≤ 1.0):
    x_norm = (t - tail_start) / TailFraction
    r(t) = cos(θ × x_norm)^(2/3)         where θ = TailAngleDeg in radians
```

### 7.2 Series 58 Polynomial

```
For x ∈ [-0.5, 0.5] normalized:
    r(x) = 1 - (2|x|)^2 × (a₂ + a₄(2|x|)^2 + a₆(2|x|)^4)
    a₂ = f(L/D),  a₄ = f(L/D),  a₆ = f(L/D)   [table lookup or polynomial fit]
```

### 7.3 Superellipse Longitudinal

```
|x_norm|^n + |r|^n = 1   (n = LongitudinalExponent)
→ r(x) = (1 - |x_norm|^n)^(1/n)
n=2 → ellipse, n=6 → near-rectangular with rounded ends
```

### 7.4 Uniform (hemispherical caps)

```
Cap zone: |x_norm| > (1 - ParallelMidbodyFraction)/2
    r(x) = sin(arccos(x_norm_cap))   [hemisphere]
Midbody: r = 1.0
```

---

## 8. Preview Architecture

```
FSubmarineEditorToolkit
    │
    ├── ASubmarinePreviewActor   (in Level viewport, editor-only)
    │       ├── UProceduralMeshComponent   (exterior hull)
    │       └── [future] UProceduralMeshComponent  (outer envelope)
    │
    ├── FSubmarineHullProfileService::GenerateControlRingsFromProfile()
    │       → 6 rings: BowTip, BowMid, BowShoulder, SternShoulder, SternMid, SternTip
    │
    ├── FSubmarineRingSequenceBuilder::BuildRingSequence()
    │       → N=8 rings at preview quality
    │
    └── FSubmarineHullBakeService::BakeExteriorHull()
            → 12 radial segments, no collision
            → Upload to mesh section 0

On property change → OnPreviewPropertiesChanged() → RefreshPreview()
```

**Future gizmo layer (Phase 2):**
- `USubmarineRingHandleComponent` per ring: capsule collider + drag callback
- Drag X → update `ControlRingDef.PositionX`, re-sort, refresh
- Drag Y/Z scale → update `HalfWidthCm` / `HalfHeightCm`, refresh
- Outer envelope rings: separate handle components, distinct colour

---

## 9. Preset Table

| Preset Name | Profile | L/D | NoseExp | TailAngle | Midbody | Notes |
|---|---|---|---|---|---|---|
| Torpedo Body | Myring | 10.0 | 2.0 | 25° | 40% | Classic SSN shape |
| Modern Attack | Series58 | 10.5 | — | — | 45% | Virginia-like |
| Compact AIP | Myring | 8.0 | 3.0 | 30° | 30% | Type 212 / A26 |
| Ballistic Boat | Myring | 12.5 | 2.5 | 20° | 55% | Ohio-like, long midbody |
| Fat Soviet | Series58 | 7.5 | — | — | 35% | Kilo-like, wide beam |
| Cylinder Scout | Uniform | 6.0 | — | — | 60% | Simple, sci-fi compact |
| Blade Runner | Superellipse | 14.0 | — | — | 50% | n=5, sharp ends |
| Organic Creature | Manual | — | — | — | — | Free-form, no constraints |
| Dreadnought | Myring | 12.0 | 2.0 | 18° | 55% | Very long, large parallel |

---

## 10. Implementation Sequence (recommended)

1. **Now (done):** `FHullProfileParams` + `ESub3DHullLongitudinalProfile` + `FSubmarineHullProfileService` — generates 6 control rings from math profile.
2. **Next:** Add `FSailDef`, `FBowSectionDef`, `FSternSectionDef`, `FOuterCasingDef` to `Sub3DCore` types. Wire into `USub3DSubmarineAuthoringAsset`.
3. **Viewport handles:** `USubmarineRingHandleComponent` — drag to adjust rings in-viewport.
4. **Outer envelope ring chain:** second `FControlRingDef[]` + separate preview mesh section.
5. **Sail / appendage bake:** separate mesh sections on `ASubmarinePreviewActor`.
6. **Phase wizard:** Ring Layout tab → Bay Definition tab → Final Bake tab (already present in toolkit skeleton).

---

## 11. What Must NOT be Constrained

- Cross-section can be any `ESub3DSectionProfile` value including `Superellipse` with extreme roundness
- `ESub3DHullLongitudinalProfile::Manual` must always be available — sci-fi / organic forms bypass all math
- `LengthCm` has no upper bound — support 250m+ without special-casing
- `DefaultHalfWidthCm` / `DefaultHalfHeightCm` are independent — allow very wide, shallow designs
- `FSailDef.bEnabled = false` + `FBowSectionDef.bEnabled = false` → valid hull, appendage-free
- Multiple `OuterEnvelopeRings` can produce asymmetric bump topologies (e.g. twin nacelles)
