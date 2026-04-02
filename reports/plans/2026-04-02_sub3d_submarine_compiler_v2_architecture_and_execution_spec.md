# Sub3D - SubmarineCompiler V2 Architecture And Execution Spec

Date: 2026-04-02
Status: Proposed replacement architecture
Scope: Full structural replacement of the current submarine compiler geometry path
Owner: Compiler / hull / breach pipeline

---

## 1. Purpose

This document defines the replacement architecture for `SubmarineCompiler`.

It is not a small corrective pass.
It is a clean rebuild plan for the compiler path because the current system has reached a structural limit:

- the interior and exterior hull do not read as one compiled structural object
- seams, overlaps, and open gaps are still visible after multiple local fixes
- breach ownership is still attached to a fragile geometry contract
- the actor, compiler, mesh generation, editor preview, and runtime assembly are too tightly coupled

The objective of V2 is:

- one geometric truth for the pressure hull
- one ring sequence shared by every derived mesh
- deterministic compiled ownership for breach and patch
- clean separation between pure compilation and actor assembly

---

## 2. Canon And Scope Defense

Use this priority order:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-04-01_sub3d_hull_breach_full_playable_implementation_plan.md`
3. `reports/plans/2026-04-01_sub3d_envelope_geometry_rework_spec.md`
4. this document

Scope rule:

- this V2 replaces the current geometry compiler path
- it does not replace movement, flood runtime, repair runtime, or the functional graph
- it does not introduce a freeform hull editor
- it does not introduce runtime global boolean remeshing

Hard limits for this phase:

- keep a straight X spine
- keep `USubmarineEnvelopeDef` as the geometric source of truth
- keep `USubmarineLayoutSolver` as the layout source of truth in Wave 0 and Wave 1
- do not redesign the first playable around a new gameplay model

---

## 3. Failure Analysis Of The Current System

### 3.1 Compartment-Driven Mesh Generation Is The Wrong Source Of Truth

Current situation:

- interior walls are generated per compartment
- floors are generated per compartment
- bulkheads are generated separately
- caps are generated separately

Impact:

- the visible hull is segmented by logic boundaries
- seams and overlaps are expected, not accidental
- the player reads "assembled pieces" instead of "one hull"

### 3.2 The Current Interior Is Not Geometrically Water-Tight

Current situation:

- each compartment produces its own wall surface
- closures are attached afterward
- bulkheads are separate meshes, not faces of the same manifold

Impact:

- light, background black, and occlusion gaps are visible
- front and rear section views are not credible
- the interior cannot be treated as one coherent shell

### 3.3 The Geometry Contract Still Depends On Local Builder Assumptions

Current situation:

- multiple generation paths still resolve shape independently
- the ownership bridge in the current implementation relies on expected ranges rather than on a true topology source

Impact:

- changing generation density or topology risks invalidating bindings
- breach and patch are attached to generated buffers, not to stable surface ownership

### 3.4 The Actor Owns Too Much

Current situation:

- `ASubmarineCompilerActor` mixes:
  - compile orchestration
  - preview persistence
  - PMC creation
  - collision proxy creation
  - station and door spawning
  - editor buttons

Impact:

- difficult to test
- difficult to reason about
- difficult to migrate without dragging legacy assumptions into the new system

### 3.5 Legacy Fixes Improved Symptoms But Did Not Remove The Structural Cause

What already improved:

- wall and floor separation
- material separation
- collision cleanup
- explicit binding tables
- reduced triangle degeneracy

What did not change:

- hull truth is still not compiled as one shared pressure hull surface

Conclusion:

- the current system is not a good base for breach-ready visual coherence
- local fixes should stop here except for blocking editor/runtime safety issues

---

## 4. V2 Core Principle

The V2 compiler has one core rule:

`One cross-section evaluator. One ring sequence. One topology source.`

Everything must derive from:

`EnvelopeDef + LayoutSolution`
-> `Section Stack`
-> `Ring Sequence`
-> `Unified Hull Topology`
-> `Compiled Hull Asset`
-> `Editor / Runtime Assembly`

There is no per-compartment hull generation in V2.

Compartments remain a logical concept.
The pressure hull remains a geometric concept.

---

## 5. V2 System Architecture

### 5.1 Layer A - Envelope Truth

Owner:

- `USubmarineEnvelopeDef`

Responsibilities:

- longitudinal radius evaluation
- bow and stern taper evaluation
- section point evaluation
- section width evaluation at a given height
- wall thickness authoring value

Rules:

- keep this class
- do not fork section math into other systems
- every ring evaluation path must call into this truth

### 5.2 Layer B - Layout Truth

Owner:

- `USubmarineLayoutSolver`

Responsibilities:

- compartment placement
- bulkhead placement
- station placement
- logical floor target
- logical door placement

Rules:

- solver remains mesh-agnostic
- solver does not emit geometry buffers
- solver may remain unchanged in early waves, but its geometric assumptions must be aligned later with shared section truth

### 5.3 Layer C - Pressure Hull Section Stack

Owner:

- `SubmarinePressureHullCompiler`

Responsibilities:

- evaluate a longitudinal sequence of hull samples
- insert mandatory samples at:
  - compartment starts and ends
  - bulkhead front and rear faces
  - bow and stern control points
  - taper transition zones
- compute interior and exterior section dimensions from one shared contract

This stack is not a render mesh.
It is the geometric intermediate truth.

### 5.4 Layer D - Ring Sequence

Owner:

- `SubmarinePressureHullCompiler`

Responsibilities:

- expand each section sample into a fully evaluated ring
- each ring contains the exact exterior and interior arc points used by later sweep steps
- floor intersection data is stored on the ring

This ring sequence is the only source used by:

- exterior shell generation
- interior shell generation
- floor generation
- bulkhead contour generation
- end closure generation
- patch ownership generation

### 5.5 Layer E - Unified Hull Topology

Owner:

- `SubmarinePressureHullCompiler`

Responsibilities:

- stitch consecutive rings into continuous surfaces
- create:
  - one continuous exterior shell
  - one continuous interior shell
  - floor bands
  - bulkhead meshes
  - end closures

Rules:

- no wall mesh per compartment
- no cap fan detached from the ring sequence
- no ad hoc closure primitives

### 5.6 Layer F - Compiled Hull Asset

Owner:

- `SubmarinePressureHullCompiler`

Responsibilities:

- output the compiled geometry and ownership data required by runtime

This asset is the new bridge between compiler and actor.

### 5.7 Layer G - Mesh Assembler

Owner:

- `SubmarineHullMeshAssembler`

Responsibilities:

- consume the compiled asset
- instantiate PMCs or future mesh components
- apply materials
- assign collision profiles
- expose debug views if needed

Rules:

- assembler does not solve layout
- assembler does not compute topology

### 5.8 Layer H - Compiler Actor V2

Owner:

- `ASubmarineCompilerActorV2`

Responsibilities:

- editor entry point
- preview values
- compile and rebuild actions
- world assembly orchestration
- collision proxy refresh
- door and station spawn

Rules:

- actor does not contain geometry algorithms
- actor does not contain patch ownership logic

---

## 6. Data Model

### 6.1 Required New Types

```cpp
USTRUCT(BlueprintType)
struct FPressureHullSectionSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float SpineX = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    int32 CompartmentIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float ExteriorHalfHeight = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float ExteriorHalfWidth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float InteriorHalfHeight = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float InteriorHalfWidth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float SectionExponent = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float WallThicknessCm = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float FloorZ = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float FloorHalfWidth = 0.f;
};
```

```cpp
USTRUCT(BlueprintType)
struct FHullProfileRing
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    int32 RingIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    float SpineX = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    int32 CompartmentIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    FPressureHullSectionSample Sample;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FVector> ExteriorPoints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FVector> InteriorPoints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FVector> ExteriorNormals;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FVector> InteriorNormals;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    int32 FloorArcStartIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    int32 FloorArcEndIndex = INDEX_NONE;
};
```

```cpp
USTRUCT(BlueprintType)
struct FCompiledHullPatchRange
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FName PatchId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FName SheetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    ESheetSide Side = ESheetSide::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 RingStart = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 RingEnd = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 VertexStart = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 VertexCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 TriangleStart = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    int32 TriangleCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FVector LocalNormal = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FBox LocalBounds = FBox(EForceInit::ForceInit);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FVector2D ChartMin = FVector2D(0.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    FVector2D ChartMax = FVector2D(1.f, 1.f);
};
```

```cpp
USTRUCT(BlueprintType)
struct FCompiledPressureHullAsset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FHullProfileRing> Rings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    FSubmarineMeshSectionData ExteriorShell;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    FSubmarineMeshSectionData InteriorShell;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FSubmarineMeshSectionData> FloorSections;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
    TArray<FSubmarineBulkheadMeshData> Bulkheads;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    TArray<FCompiledHullPatchRange> ExteriorPatches;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull|Patch")
    TArray<FCompiledHullPatchRange> InteriorPatches;
};
```

### 6.2 Existing Types That Remain Valid

Keep:

- `USubmarineEnvelopeDef`
- `USubmarineFunctionalGraph`
- `FSubmarineLayoutSolution`
- `FCompartmentPlacement`
- `FBulkheadPlacement`
- `FStructuralSheetDef`
- `FCompiledHullRegionRange`
- `FStructuralSheetCompiledBinding`

Change in interpretation:

- `FCompiledHullRegionRange` and `FStructuralSheetCompiledBinding` become outputs of patch generation
- they are no longer inferred from assumed buffer layout after the fact

---

## 7. API Contracts

### 7.1 Envelope Contracts

The following envelope functions are required and must remain the only geometric truth:

```cpp
FVector EvaluateSectionPoint(
    float NormalizedLongitudinalAlpha,
    float NormalizedArcAlpha) const;

float EvaluateSectionHalfWidth(
    float NormalizedLongitudinalAlpha,
    float LocalZCm,
    float RadiusCm) const;
```

If missing or incomplete, introduce explicit helpers on `USubmarineEnvelopeDef`:

```cpp
bool EvaluateInteriorSectionIntersection(
    float SpineX,
    float FloorZ,
    float WallThicknessCm,
    float& OutFloorHalfWidth,
    int32& OutArcStartIndex,
    int32& OutArcEndIndex) const;
```

Rule:

- no geometry system may re-derive section math independently

### 7.2 Pressure Hull Compiler API

```cpp
class SUB3D_API USubmarinePressureHullCompiler : public UObject
{
    GENERATED_BODY()

public:
    bool BuildSectionStack(
        const USubmarineEnvelopeDef& Envelope,
        const FSubmarineLayoutSolution& Solution,
        TArray<FPressureHullSectionSample>& OutSections,
        TArray<FLayoutValidationMessage>& OutMessages) const;

    bool BuildRingSequence(
        const USubmarineEnvelopeDef& Envelope,
        const FSubmarineLayoutSolution& Solution,
        const TArray<FPressureHullSectionSample>& Sections,
        TArray<FHullProfileRing>& OutRings,
        TArray<FLayoutValidationMessage>& OutMessages) const;

    bool CompilePressureHull(
        const USubmarineEnvelopeDef& Envelope,
        const FSubmarineLayoutSolution& Solution,
        FCompiledPressureHullAsset& OutAsset,
        TArray<FLayoutValidationMessage>& OutMessages) const;
};
```

Contract:

- `Envelope` is required
- `Solution` is required
- output asset must be deterministic for identical inputs

### 7.3 Mesh Assembler API

```cpp
class SUB3D_API USubmarineHullMeshAssembler : public UObject
{
    GENERATED_BODY()

public:
    bool BuildHullMeshes(
        AActor& ParentActor,
        const FCompiledPressureHullAsset& Asset,
        UMaterialInterface* ExteriorMaterial,
        UMaterialInterface* InteriorWallMaterial,
        UMaterialInterface* FloorMaterial,
        bool bEnableCollision,
        TArray<UProceduralMeshComponent*>& OutBuiltComponents) const;
};
```

Contract:

- no layout solve
- no ring generation
- no ownership decisions

### 7.4 Compiler Actor V2 API

```cpp
UCLASS(Blueprintable)
class SUB3D_API ASubmarineCompilerActorV2 : public ASubmarineBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "SubCompilerV2")
    bool CompileCurrentDefinitionsV2();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SubCompilerV2")
    bool CompileAndBuildV2();

    UFUNCTION(CallInEditor, Category = "SubCompilerV2", meta = (DisplayName = "Compile And Build V2"))
    void CompileAndBuildV2InEditor();

    UFUNCTION(BlueprintPure, Category = "SubCompilerV2")
    const FCompiledPressureHullAsset& GetCompiledPressureHullAsset() const;
};
```

Contract:

- V2 actor is the only editor entry point for V2
- V1 actor remains intact during coexistence

---

## 8. Mesh Generation Rules

### 8.1 Exterior Shell

Rules:

- one continuous sweep across all rings
- no per-compartment segmentation
- no duplicated ring emission
- no detached cap generation

Expected output:

- one mesh section or one component for the full outer shell

### 8.2 Interior Shell

Rules:

- one continuous sweep across all rings
- use only the interior arc between floor intersection points
- no per-compartment wall PMCs

Expected output:

- one mesh section or one component for the full inner shell

### 8.3 Floor

Rules:

- generated from ring floor edges
- width can vary per ring
- floor is not a flat rectangle by compartment

Expected output:

- floor sections may remain compartment-grouped for gameplay/editor clarity
- floor topology still comes from the ring sequence

### 8.4 Bulkheads

Rules:

- derive contour from the ring at the bulkhead location
- door cutout is subtracted in polygon space before triangulation
- triangulation uses ear clipping
- no triangle fan bulkhead fill

Expected output:

- one separate mesh per bulkhead is acceptable

### 8.5 End Closures

Rules:

- derived from terminal rings
- not built as ad hoc fans disconnected from the ring sequence
- if the last ring does not converge enough, build an explicit terminal closure from the same profile contour

---

## 9. Ownership And Breach Contract

Ownership must be produced during topology generation.

Rules:

1. Each structural sheet maps to one or more compiled patch ranges.
2. Patch ranges know:
   - side
   - ring interval
   - vertex interval
   - triangle interval
   - chart range
3. Interior and exterior ownership are parallel outputs of the same compilation pass.
4. `SubHullVisualDamageComponent` consumes these bindings directly.
5. No second ownership system may be introduced in runtime.

Consequences:

- breach hit -> sheet -> patch range -> ext and int footprint
- patch -> same patch range closure

---

## 10. Tests

### 10.1 Unit Tests

Create:

- `Sub3D.SubCompilerV2.SectionStack.Deterministic`
- `Sub3D.SubCompilerV2.RingSequence.Deterministic`
- `Sub3D.SubCompilerV2.RingSequence.FloorIntersectionValid`
- `Sub3D.SubCompilerV2.ExteriorShell.NoDegenerateTriangles`
- `Sub3D.SubCompilerV2.InteriorShell.NoDegenerateTriangles`
- `Sub3D.SubCompilerV2.Bulkhead.EarClippingValid`
- `Sub3D.SubCompilerV2.PatchOwnership.ValidRanges`
- `Sub3D.SubCompilerV2.PatchOwnership.ExtIntCoherent`

### 10.2 Geometry Validation Tests

Create:

- no visible section gap at bulkhead boundaries
- interior shell starts and ends on expected bulkhead or closure faces
- floor edges remain inside interior wall silhouette
- exterior shell remains continuous across all compartment intervals

### 10.3 Editor Tests

Create:

- `CompileAndBuildV2` creates expected component set
- material assignment is correct
- collision profiles are correct:
  - exterior shell visual or proxy as configured
  - interior shell `SubInteriorVisual`
  - floor `SubInteriorWalkable`

### 10.4 Runtime/Breach Tests

Create:

- breach on a flank resolves one exterior patch and one interior patch
- breach near taper zone still resolves a valid patch
- patch closes the same ownership region

---

## 11. Legacy / V2 Coexistence Strategy

Do not replace the existing compiler in one destructive move.

### 11.1 Legacy Freeze

Current `SubmarineCompiler` becomes legacy.

Rules:

- only blocking bug fixes
- no new feature work
- no new ownership model

### 11.2 Parallel V2 Entry Point

Introduce:

- `ASubmarineCompilerActorV2`
- `USubmarinePressureHullCompiler`
- `USubmarineHullMeshAssembler`

Rules:

- V2 code lives beside legacy
- no hidden switch inside legacy actor
- editor chooses explicitly between V1 and V2 actors

### 11.3 Migration Bridge

Bridge steps:

1. V2 compiles geometry only
2. V2 assembles geometry in editor
3. V2 exports compiled bindings
4. V2 hooks breach visuals
5. V2 becomes the default compiler path
6. V1 is deprecated

### 11.4 Removal Condition For Legacy

Legacy can be retired only when:

- V2 geometry compiles successfully in editor
- V2 build passes all compiler tests
- V2 breach ownership passes runtime validation
- first playable uses V2 submarine without fallback

---

## 12. Execution Waves

### Wave 0 - Contracts And Skeleton

Deliver:

- new types
- new compiler class
- new assembler class
- new V2 actor shell
- no runtime switch yet

Exit:

- build passes
- empty compile path compiles without mesh generation

### Wave 1 - Ring Sequence

Deliver:

- section stack builder
- ring sequence builder
- deterministic tests

Exit:

- ring sequence is stable and inspectable

### Wave 2 - Exterior Shell

Deliver:

- continuous exterior sweep
- no detached caps
- ownership ranges for exterior

Exit:

- one continuous exterior shell compiles

### Wave 3 - Interior Shell And Floor

Deliver:

- continuous interior shell
- floor sweep from ring floor edges

Exit:

- no compartment seams in interior wall shell

### Wave 4 - Bulkheads And End Closures

Deliver:

- ear-clipped bulkheads
- terminal closure path

Exit:

- no silhouette break
- no visible holes at front, rear, or bulkhead boundaries

### Wave 5 - Ownership Bridge

Deliver:

- ext and int patch ownership
- `FStructuralSheetCompiledBinding` export from V2

Exit:

- breach system can target V2 ranges without heuristic mapping

### Wave 6 - Editor And Runtime Integration

Deliver:

- V2 actor assembly
- material setup
- collision setup
- debug views

Exit:

- editor preview usable
- first playable submarine can be instantiated with V2

### Wave 7 - Breach Hookup

Deliver:

- `SubHullVisualDamageComponent` consumption of V2 bindings
- ext/int coherent breach footprint

Exit:

- breach is readable as traversing one hull

---

## 13. Done Definition

V2 is considered successful only when all of the following are true:

1. Exterior shell is continuous.
2. Interior shell is continuous.
3. No visible gap exists between wall shell and bulkheads.
4. No visible overlap exists between compartments because compartments do not define shell topology.
5. End closures and bulkheads remain inside intended silhouette.
6. Structural sheet ownership is produced during compilation.
7. A breach can open the same owned region on the exterior and interior sides.
8. The first playable submarine can run on V2 without legacy fallback.

---

## 14. Immediate Next Action

The next correct implementation step is:

1. create `SubmarinePressureHullTypes.*`
2. create `SubmarinePressureHullCompiler.*`
3. implement Wave 0 and Wave 1 before any more legacy geometry fixes

This is the shortest path to stop paying for the current topology debt.
