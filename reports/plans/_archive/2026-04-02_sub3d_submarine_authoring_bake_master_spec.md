# Sub3D - Submarine Authoring Bake Master Spec

Date: 2026-04-02
Status: Master spec
Scope: Final editor-facing and runtime-facing architecture for submarine authoring, bake, and runtime consumption
Owner: Submarine authoring / bake / runtime pipeline
Supersedes:
- `2026-04-02_sub3d_submarine_authoring_bake_architecture_spec.md`
- `2026-04-02_sub3d_submarine_authoring_bake_architecture_decision_addendum.md`

---

## 1. Purpose

This document is the single reference for the new submarine authoring and bake architecture.

It replaces the current editor-facing workflow built around:

- `USubmarineEnvelopeDef`
- `USubmarineFunctionalGraph`
- `USubmarineLayoutSolver`
- `ASubmarineCompilerActor`

as the daily authoring surface.

The current system reached a structural limit:

- the geometry model still reads as compartment-driven
- the editor workflow exposes too many technical concepts
- authoring, preview, compilation, and runtime assembly are not cleanly separated
- breach ownership needs one baked structural truth

This master spec keeps the correct pressure hull and ring-sequence logic internally, but moves the editor workflow to a simpler asset-centric model.

---

## 2. Final Product Workflow

The workflow is fixed:

1. Create or duplicate one `USubmarineAuthoringAsset`
2. Configure one specific submarine in that asset
3. Preview and validate it in editor
4. Bake one `UCompiledSubmarineAsset`
5. Use that baked asset in one runtime actor or one runtime blueprint

Normal daily use must not require the user to think in terms of:

- envelope assets
- ring buffers
- topology builders
- compiler actors
- patch arrays
- low-level mesh range bookkeeping

Those remain internal implementation details.

---

## 3. Failure Analysis Of The Current System

### 3.1 Wrong Editor-Facing Abstractions

The current workflow forces the user to manage:

- envelope definition
- functional graph
- layout solve
- compiler actor
- preview overrides

This is acceptable for a technical prototype.
It is not acceptable for daily creation of one submarine class.

Observed impact:

- too many assets and moving parts for one result
- weak separation between authoring truth and generated output
- high friction to build one specific submarine cleanly

### 3.2 Geometry Still Reads As Compartment-Driven

The current hull path still carries a compartment-first mindset.

Observed impact:

- visible seams between generated sections
- interior and exterior do not read as faces of one same hull
- bulkheads and closures can still read as separate pieces
- the hull is not reliably water-tight as compiled geometry

### 3.3 Bake Boundary Is Unclear

The current path mixes:

- authoring data
- preview state
- compilation
- mesh creation
- actor assembly

Observed impact:

- difficult to reason about persistent versus temporary data
- difficult to guarantee deterministic bake output
- difficult to migrate away from the legacy path cleanly

### 3.4 Ownership Is Not The Primary Product Concept

Breach needs:

- one structural surface truth
- one stable compiled ownership table
- one exact region shared by outer face and inner face

The current system improved ownership technically, but the exposed workflow still does not present a clear bake boundary around it.

---

## 4. Architecture Decision

The architecture is now:

`USubmarineAuthoringAsset`
-> `Validate`
-> `Bake`
-> `UCompiledSubmarineAsset`
-> `ASubmarineRuntimeActor` or a dedicated runtime blueprint

Optional editor helper:

`ASubmarineAuthoringPreviewActor`

This means:

- one asset for daily editing
- one explicit bake output
- one runtime consumer

The pressure hull ring-based compiler remains internal.
It is not the editor-facing architecture.

---

## 5. Scope And Constraints

### 5.1 In Scope

- one authoring asset per submarine class or variant
- deterministic bake to one compiled asset
- pressure hull compiled from one ring sequence
- explicit compiled ownership for breach
- runtime actor consuming baked data only
- editor preview and validation
- multi-deck authoring with ramps in the current phase

### 5.2 Out Of Scope

- freeform hull sculpting
- runtime global boolean remeshing
- curved centerline submarines
- replacing movement, flooding, repair, or station gameplay in this phase
- using artist-authored exterior meshes as the primary structural truth

### 5.3 Hard Constraints

- straight X spine
- deterministic build
- one pressure hull truth
- no second ownership system for damage
- no parallel geometry math paths for the same section
- no shipping bake path that delegates geometry generation to the old `SubmarineGeometryBuilder`

---

## 6. Core Internal Principles

These principles remain mandatory.

### 6.1 One Geometric Truth

There is one pressure hull truth.

Exterior face and interior face are derived from the same structure.
Damage ownership is attached to that same structure.

### 6.2 One Ring Sequence

The bake compiler internally uses one longitudinal ring sequence.

All of the following are derived from that same sequence:

- exterior shell
- interior shell
- deck surfaces
- bulkheads
- end closures
- ownership regions

### 6.3 Ownership Is Produced During Bake

Structural ownership is built during compilation, not inferred later.

This means:

- one structural sheet owns one compiled region
- the compiled region knows its outer and inner mesh ranges
- breach and patch use that exact compiled region

### 6.4 No Freeform Hull Editor

The new workflow must be simpler, but still constrained.

The hull remains:

- straight-spine
- deterministic
- profile-driven
- section-driven

---

## 7. Editor-Facing Asset Model

### 7.1 `USubmarineAuthoringAsset`

This is the only required daily authoring asset.

It defines one specific submarine.

Required top-level blocks:

1. `Identity`
2. `Hull`
3. `Decks`
4. `Compartments`
5. `BulkheadConnections`
6. `VerticalConnectors`
7. `Structure`
8. `Materials`
9. `BakeSettings`

Minimal target:

```cpp
UCLASS(BlueprintType)
class SUB3D_API USubmarineAuthoringAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName SubmarineId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    FSubmarineHullAuthoring Hull;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decks")
    TArray<FSubmarineDeckAuthoring> Decks;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compartments")
    TArray<FSubmarineCompartmentAuthoring> Compartments;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connections")
    TArray<FSubmarineBulkheadConnectionAuthoring> BulkheadConnections;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connections")
    TArray<FSubmarineVerticalConnectorAuthoring> VerticalConnectors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structure")
    FSubmarineStructureAuthoring Structure;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    FSubmarineMaterialSetAuthoring Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bake")
    FSubmarineBakeSettings BakeSettings;
};
```

### 7.2 `FSubmarineHullAuthoring`

This block must be intuitive to manipulate.

It defines:

- total length
- maximum outer diameter
- wall thickness
- bow and stern profile
- section profile
- section roundness
- width-to-height ratio
- optional longitudinal radius curve
- optional width-to-height curve

Implementation rule:

- prefer presets and curves first
- allow detailed numeric control in advanced fields
- do not force users to edit a separate envelope asset

Enum migration rule:

- prefer reusing `EBowSternProfile` in the first implementation wave
- if more editor-readable enum names are introduced later, add explicit migration

### 7.3 Decks

Deck authoring is multi-deck from the start.

Decision:

- support multi-deck authoring in the base architecture
- use ramps for implemented vertical traversal in the current phase
- support ladder anchor data as placement data, without implementing climb behavior yet

Implications:

- the asset stores `TArray<FSubmarineDeckAuthoring> Decks`
- the asset stores `TArray<FSubmarineVerticalConnectorAuthoring> VerticalConnectors`
- deck surfaces are baked explicitly

### 7.4 Compartments

Compartments remain explicit because they are gameplay structure.

They define:

- stable id
- type
- target length or weight
- minimum length
- optional order constraints
- connection policy at boundaries

Compartments do not generate their own hull meshes.
They only define structure, segmentation, and placement intent.

Compartment placement decision:

- placement is solved by an internal deterministic solver
- manual fixed X ranges are not the primary authoring mode
- resolved X spans are stored in the baked asset

### 7.5 Bulkhead Connections

Door and passage data live on bulkhead connection authoring, not as free standalone placement.

Horizontal traversal between compartments is defined here.

Minimal target:

```cpp
USTRUCT(BlueprintType)
struct FSubmarineBulkheadConnectionAuthoring
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName BoundaryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CompartmentA;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CompartmentB;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasDoor = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D DoorSizeCm = FVector2D(90.f, 190.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DoorSillZCm = 0.f;
};
```

### 7.6 Vertical Connectors

Vertical traversal data is explicit authoring data.

Minimal target:

```cpp
UENUM(BlueprintType)
enum class ESubmarineVerticalConnectorType : uint8
{
    Ramp,
    LadderAnchor
};

USTRUCT(BlueprintType)
struct FSubmarineVerticalConnectorAuthoring
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ConnectorId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESubmarineVerticalConnectorType Type = ESubmarineVerticalConnectorType::Ramp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 FromDeckIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ToDeckIndex = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float LocalX = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float WidthCm = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float SlopeDegrees = 18.f;
};
```

Current phase rule:

- `Ramp` is implemented
- `LadderAnchor` is stored and baked as placement data only

### 7.7 Structure

Structural authoring defines how breach-capable regions are partitioned.

It should define:

- segmentation policy along X
- segmentation policy around circumference
- which structural sheets are breach-capable
- optional special zones near bow or stern

This remains simple at authoring level.
Exact mesh ranges stay internal to the bake output.

### 7.8 Materials

Material slot definitions live in both authoring and baked asset.

Authoring stores the intended material set.
Baked data stores resolved material slot names and slot indices.

Minimum required slots:

1. `ExteriorHull`
2. `InteriorHull`
3. `Deck`
4. `Bulkhead`
5. `DoorFrame`
6. `Ramp`
7. `BreachRim`

Minimal target:

```cpp
USTRUCT(BlueprintType)
struct FSubmarineMaterialSetAuthoring
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> ExteriorHull;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> InteriorHull;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> Deck;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> Bulkhead;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> DoorFrame;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> Ramp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> BreachRim;
};
```

---

## 8. Baked Runtime Asset Model

### 8.1 `UCompiledSubmarineAsset`

This is the bake result and the only runtime geometry source of truth.

The runtime actor must not regenerate hull geometry from authoring data.

Minimal target:

```cpp
UCLASS(BlueprintType)
class SUB3D_API UCompiledSubmarineAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FCompiledSubmarineMeshSection> RenderSections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    FCompiledSubmarineCollisionData Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FCompiledSubmarineCompartmentData> Compartments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FCompiledSubmarineBulkheadConnectionData> BulkheadConnections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FCompiledSubmarineVerticalConnectorData> VerticalConnectors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FStructuralSheetCompiledBinding> StructuralBindings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compiled")
    TArray<FCompiledSubmarineMaterialSlot> MaterialSlots;
};
```

### 8.2 Baked Mesh Format

Decision:

- the baked asset stores raw compiled mesh arrays as the authoritative format
- `UStaticMesh` is not the authoritative baked representation in this phase

Reason:

- breach ownership needs exact vertex and triangle range control
- outer and inner faces must preserve stable compiled bindings
- runtime and debug paths need deterministic access to compiled buffers

Minimal target:

```cpp
USTRUCT(BlueprintType)
struct FCompiledSubmarineMeshSection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName SectionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MaterialSlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector3f> Positions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector3f> Normals;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector4f> Tangents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector2f> UV0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<int32> Indices;
};
```

Later, derived `UStaticMesh` generation is allowed as an optimization.
It is not the primary format in this phase.

### 8.3 Baked Collision Format

Collision is baked separately from render mesh usage.

Decision:

- collision is derived during bake
- render geometry does not define gameplay collision policy by itself

Minimal target:

```cpp
USTRUCT(BlueprintType)
struct FCompiledSubmarineCollisionData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FCompiledSubmarineMeshSection ExteriorProxy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FCompiledSubmarineMeshSection> WalkableDeckSections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FCompiledSubmarineMeshSection> BulkheadBlockerSections;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FCompiledSubmarineMeshSection> RampSections;
};
```

Collision policy:

- exterior proxy:
  - closed baked proxy
  - used for submarine movement and external hit queries
- walkable deck sections:
  - baked per deck
  - `QueryOnly` in current phase
- bulkhead blocker sections:
  - baked blockers at sealed boundaries
  - openings carved where doors exist
- ramp sections:
  - baked walkable collision for ramps
- interior wall render geometry:
  - not the primary gameplay blocking source

### 8.4 Ownership Contract

The baked asset must include the structural ownership contract used by breach and repair.

The contract must expose:

- stable sheet id
- compartment relation
- outer mesh range
- inner mesh range
- local chart or projection frame
- local bounds

This is the baked form of ring-sequence ownership.

---

## 9. Preview And Runtime Actors

### 9.1 `ASubmarineAuthoringPreviewActor`

Optional editor helper only.

Responsibilities:

- load one authoring asset
- run validation
- trigger bake
- display temporary preview meshes
- show debug overlays

It is not the gameplay runtime actor.

### 9.2 `ASubmarineRuntimeActor`

Runtime actor responsibilities:

- load one baked asset
- create render components
- create collision components
- initialize breach and flood systems from baked bindings
- initialize stations and door runtime data from baked placement data

It must not rebuild hull geometry from authoring data.

If preferred, a runtime blueprint may wrap the baked asset.
The important rule is unchanged: runtime consumes baked data only.

---

## 10. Section Evaluation API

Do not keep a separate editor-facing `USubmarineEnvelopeDef` asset in the normal workflow.

Instead:

- store hull parameters in authoring structs
- keep reusable math in one C++ evaluator
- expose Blueprint access through a `UBlueprintFunctionLibrary`

Minimal target:

```cpp
UCLASS()
class SUB3D_API USubmarineHullEvaluationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Submarine|Hull")
    static FVector EvaluateSectionPoint(const FSubmarineHullAuthoring& Hull, float SpineAlpha, float ArcAlpha);

    UFUNCTION(BlueprintPure, Category="Submarine|Hull")
    static float EvaluateBowSternTaper(const FSubmarineHullAuthoring& Hull, float SpineAlpha);

    UFUNCTION(BlueprintPure, Category="Submarine|Hull")
    static float GetSectionArcLengthEstimate(const FSubmarineHullAuthoring& Hull, float SpineAlpha, int32 NumSamples);
};
```

This replaces the old BlueprintPure `UFUNCTION` pattern previously attached to `USubmarineEnvelopeDef`.

---

## 11. Bake Pipeline

### 11.1 Input

The bake input is exactly one `USubmarineAuthoringAsset`.

No second authoring asset should be required for the normal workflow.

### 11.2 Bake Steps

The bake executes in this order:

1. Validate authoring data
2. Solve compartment placement
3. Resolve derived hull and deck parameters
4. Build one ring sequence along the spine
5. Generate one continuous exterior hull mesh
6. Generate one continuous interior hull mesh
7. Generate multi-deck walkable surfaces from the same ring sequence
8. Generate ramps from deck and connector authoring
9. Generate bulkheads from ring-derived interior outlines
10. Apply bulkhead openings from connection data
11. Generate end closures from the same hull truth
12. Generate collision proxy data
13. Generate structural ownership bindings
14. Generate compiled connection and placement metadata
15. Save or update one `UCompiledSubmarineAsset`

### 11.3 Bake Validation

Bake must fail on:

- wall thickness greater than local hull half-height
- invalid compartment overlap
- invalid solve result
- bulkhead positions outside hull span
- door opening outside the local bulkhead polygon
- connector outside local deck or hull clearance
- degenerate ring sequence
- invalid ownership ranges

Bake may warn on:

- extreme taper reducing walkable width
- deck too close to hull
- breach-capable region becoming too small near bow or stern

### 11.4 Performance Gate

Add an editor bake performance validation for the reference submarine.

Required gate:

- bake of the reference submarine at default bake resolution completes in under `2.0` seconds on the target development machine

Grading:

- under `1.0` second: good
- `1.0` to `2.0` seconds: acceptable
- above `2.0` seconds: fail for this phase

---

## 12. Migration Strategy

### 12.1 Rule

Do not migrate by exposing more V2 internal types.
Migrate by inserting the new authoring asset above the new ring-based bake core.

### 12.2 No Transitional Geometry Bridge

The new bake path must implement the ring-based geometry core directly.

Not allowed:

- a shipping bake path that delegates generation to the old `SubmarineGeometryBuilder`

Allowed legacy usage:

- import tools
- migration helpers
- validation comparison only

### 12.3 Phases

#### Phase 1

- create `USubmarineAuthoringAsset`
- create authoring structs for hull, decks, compartments, connections, materials, and bake settings
- create `UCompiledSubmarineAsset`
- create `USubmarineHullEvaluationLibrary`

#### Phase 2

- implement the new internal ring-based bake core directly
- generate raw compiled mesh arrays
- generate ownership data

#### Phase 3

- implement collision bake
- implement multi-deck surfaces and ramp bake
- implement bulkhead opening bake

#### Phase 4

- implement preview actor and bake actions
- implement validation UI and debug views

#### Phase 5

- move runtime actor to baked asset only
- move breach component to baked ownership only

#### Phase 6

- add import and migration helpers if still needed
- deprecate the old authoring path

---

## 13. Test Plan

### 13.1 Authoring Validation Tests

Add automated tests for:

- invalid hull thickness
- invalid compartment ordering
- invalid door placement
- invalid deck width
- invalid connector clearance

### 13.2 Bake Determinism Tests

Add automated tests for:

- same authoring asset -> same structural binding count
- same authoring asset -> same triangle counts
- same authoring asset -> same compartment spans

### 13.3 Ownership Tests

Add automated tests for:

- every breach-capable sheet has valid compiled bindings
- outer and inner ranges exist for the same compiled region
- local chart data remains valid near bow and stern taper

### 13.4 Geometry Integrity Tests

Add automated tests for:

- no negative area bulkhead polygons
- no degenerate adjacent ring sweeps
- no open seam between adjacent ring spans

### 13.5 Runtime Consumption Tests

Add automated tests for:

- runtime actor initializes from baked asset only
- breach component reads baked ownership only
- authoring asset is not required at runtime

---

## 14. Done Definition

This architecture is successfully implemented only when all of the following are true:

1. A designer can define one submarine through one authoring asset.
2. The editor workflow reads as create, preview, validate, bake.
3. The baked asset is sufficient for runtime spawning.
4. The internal compiler uses one ring sequence.
5. Interior and exterior derive from one pressure hull truth.
6. Ownership is produced during bake and consumed directly by breach.
7. The runtime actor no longer depends on the old compiler actor workflow.
8. Multi-deck support exists with ramps in the current phase.
9. The current multi-asset authoring friction is removed from normal use.

---

## 15. Final Direction

The direction is final:

- one authoring asset
- one baked asset
- one runtime consumption path
- multi-deck supported
- compartment spans solved internally
- door and vertical access data authored as explicit structural connections
- raw mesh arrays are the baked mesh truth
- collision is baked separately from render geometry
- material slots exist in authoring and baked asset
- section evaluation moves to a blueprint function library
- no geometry bridge back to the old builder

This is the architecture to implement.
