# Sub3D - Hull And Decks First Architecture Spec

Date: 2026-04-02
Status: Proposed pivot spec
Scope: Replace the compartment-first editor model with a hull-first and floors-first authoring model
Owner: Submarine authoring / bake / runtime pipeline
Related:
- `2026-04-02_sub3d_submarine_authoring_bake_master_spec.md`

---

## 1. Purpose

This document defines the next editor-facing architecture after the current authoring/bake prototype.

The current bake path is usable as a prototype, but the underlying authoring model reached a new structural limit:

- the hull can be authored
- decks can be authored
- openings and bulkheads can be baked
- runtime consumption works

But the current model still assumes that compartment structure is decided too early.

That is the wrong constraint for the intended product.

The target product is not:

- define compartments first
- derive floors and internal layout from them

The target product is:

- define one hull
- define one or more walkable floor systems
- define vertical passages
- define interior partitions and bulkheads later
- derive compartments from those interior partitions, not the other way around

This document is the pivot specification for that change.

---

## 2. Product Decision

The new editor-facing authoring order is now:

1. Define the pressure hull
2. Define decks and floor volumes
3. Define vertical openings and vertical circulation
4. Define optional internal partitions and bulkheads
5. Derive or build compartments from that interior structure
6. Bake the submarine
7. Use the baked asset at runtime

This changes one central assumption:

- compartments are no longer the primary spatial truth

Instead:

- hull and deck geometry are the primary spatial truth

Compartments become a later structure layer.

---

## 3. Why The Current Model Must Change

### 3.1 Compartments Are Being Decided Too Early

In the current model, compartments are used too early in the generation chain.

Observed impact:

- floors are constrained by compartment spans
- bulkheads appear too early as if they were fixed truth
- internal space reads as pre-cut rather than designed
- it becomes difficult to exploit full 3D interior volume

This is especially limiting for:

- multi-deck layouts
- stacked rooms
- lower ballast or machinery spaces under upper control spaces
- lateral corridors
- emergency re-partitioning
- player-driven interior design

### 3.2 The Intended Product Needs Interior Flexibility

The intended gameplay and editor direction suggests:

- the player should be able to define internal partitions later
- internal walls and bulkheads should not be locked by the initial hull authoring pass
- different room topologies should fit inside the same hull

That means the initial bake cannot assume a final compartment graph as the primary layout truth.

### 3.3 Bulkheads And Compartments Must Become Separate Concepts

These concepts are currently too coupled.

They need to be separated:

- a bulkhead is one physical partition element
- a compartment is one connected sealed or semi-sealed volume

A compartment may be derived from several partition decisions.

This is a runtime and editor structure problem, not a hull generation problem.

---

## 4. New Architecture Direction

The editor-facing architecture becomes:

`USubmarineHullDeckAuthoringAsset`
-> `Bake Base Submarine`
-> `UCompiledSubmarineBaseAsset`
-> optional `Interior Partition Authoring`
-> optional `Compartment Solve / Build`
-> `UCompiledSubmarineRuntimeAsset`
-> runtime actor

This is a two-stage model.

### Stage A - Base Submarine

The base submarine contains:

- hull
- exterior skin
- interior hull skin
- decks
- vertical openings
- vertical circulation placeholders
- structural ownership for hull breach

This stage does not require finalized gameplay compartments.

### Stage B - Interior Partitioning

The second stage contains:

- bulkheads
- internal walls
- doors
- hatches
- compartment graph
- pressure-separated volumes

This stage may be:

- editor-authored
- runtime-built
- player-built
- solver-assisted

The system must support that flexibility.

---

## 5. New Source Of Truth

### 5.1 Spatial Truth

The spatial truth becomes:

1. pressure hull
2. deck surfaces
3. vertical openings

These three define usable interior space.

### 5.2 Structural Partition Truth

Bulkheads and internal partitions are no longer part of the hull truth.

They become a separate authoring and runtime layer.

### 5.3 Compartment Truth

Compartments are derived from:

- hull bounds
- deck connectivity
- partition placement
- door and hatch states

They are not the first-order input to hull geometry generation.

---

## 6. Editor Workflow

The new daily workflow must be:

1. Create one hull/decks authoring asset
2. Author one hull
3. Add one or more floors
4. Add vertical openings and circulation anchors
5. Bake a base submarine
6. Optionally add interior partitions
7. Optionally derive compartments
8. Spawn the runtime actor

This is simpler and closer to the intended use:

- build one submarine shell
- arrange internal circulation
- decide pressure zoning later

---

## 7. New Asset Model

### 7.1 `USubmarineHullDeckAuthoringAsset`

This becomes the new primary editor asset.

It defines:

- hull shape
- deck system
- vertical openings
- circulation placeholders
- materials
- bake settings

It does not require:

- finalized compartments
- finalized bulkheads
- finalized room graph

Minimal target shape:

```cpp
UCLASS(BlueprintType)
class SUB3D_API USubmarineHullDeckAuthoringAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName SubmarineId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    FSubmarineHullAuthoring Hull;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decks")
    TArray<FSubmarineDeckAuthoring> Decks;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vertical")
    TArray<FSubmarineVerticalOpeningAuthoring> VerticalOpenings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vertical")
    TArray<FSubmarineVerticalConnectorAuthoring> VerticalConnectors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    FSubmarineMaterialSetAuthoring Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bake")
    FSubmarineBakeSettings BakeSettings;
};
```

### 7.2 `UCompiledSubmarineBaseAsset`

This is the baked result of Stage A.

It contains:

- hull render sections
- hull collision proxy
- deck render sections
- deck walkable collision
- vertical opening cutouts
- hatch placeholder geometry
- ramp geometry
- structural hull ownership bindings

It must be sufficient for:

- navigation on decks
- hull damage and breach
- editor preview
- later partition overlay

### 7.3 Interior Partition Asset

This becomes a separate asset or data layer.

Candidate names:

- `USubmarineInteriorPartitionAsset`
- `USubmarineInteriorLayoutAsset`

It defines:

- bulkheads
- internal walls
- local doors
- hatch closures
- optional equipment room boundaries

This asset references the baked base submarine.

### 7.4 Final Runtime Asset

If needed later:

- `UCompiledSubmarineRuntimeAsset`

This merges:

- base hull/decks bake
- interior partition bake
- final compartment graph
- runtime door and flood metadata

---

## 8. Hull And Decks First Data Model

### 8.1 Hull

Hull authoring remains constrained and deterministic:

- straight spine
- section profile
- taper
- wall thickness

This part remains close to the current ring-based bake core.

### 8.2 Decks

Decks become first-class authoring data.

They define:

- height
- width policy
- active span
- optional local exclusions
- optional future branching or offset segments

Decks are not derived from compartments.

### 8.3 Vertical Openings

A vertical opening is a deck cutout location.

It defines:

- deck index or deck pair
- local X
- opening width
- opening length
- opening type

Examples:

- ladder opening
- hatch opening
- maintenance shaft opening

### 8.4 Vertical Connectors

A connector uses one or more openings.

It defines:

- from deck
- to deck
- type
- local X
- width
- slope if ramp

Important split:

- opening = geometry cut in deck
- connector = movement or placeholder object using that opening

This is cleaner than the current implicit coupling.

---

## 9. Bulkheads And Partitions As A Separate Layer

### 9.1 Bulkheads Are No Longer Required For Base Bake

The base hull bake must succeed without any bulkhead.

### 9.2 Bulkheads Become Overlay Geometry

Bulkheads are built later from the interior partition layer.

They may still use:

- the same ring evaluator
- the same local section profile

But they are not part of the base hull bake contract.

### 9.3 Internal Walls Become A Different Primitive

A wall is not necessarily a pressure bulkhead.

Examples:

- light divider wall
- reinforced partial wall
- pressure bulkhead
- emergency flood stop wall

This distinction matters for gameplay and for editor flexibility.

---

## 10. Compartments Become Derived Volumes

### 10.1 Definition

A compartment is one connected pressurizable or floodable volume.

It is computed from:

- hull inner bounds
- floor topology
- partition topology
- door states
- hatch states

### 10.2 Why This Is Better

This allows:

- stacked compartments
- partial-height spaces
- one upper command deck over one lower machinery volume
- multiple connected corridors inside one same longitudinal zone
- later player-driven reconfiguration

### 10.3 Runtime Implication

Flooding and pressure no longer depend on one early static compartment solve.

They depend on:

- derived volume graph
- open or closed passage graph

This is the correct long-term runtime model.

---

## 11. Bake Pipeline After The Pivot

### 11.1 Stage A - Base Bake

Input:

- hull/decks authoring asset

Bake steps:

1. validate hull
2. validate decks
3. validate vertical openings
4. build ring sequence
5. generate exterior hull
6. generate interior hull
7. generate end closures
8. generate deck surfaces
9. cut vertical openings into decks
10. generate ramp or hatch placeholders
11. generate hull structural ownership bindings
12. bake collision
13. output `UCompiledSubmarineBaseAsset`

### 11.2 Stage B - Partition Bake

Input:

- compiled base asset
- partition authoring asset

Bake steps:

1. validate partitions against base asset
2. generate bulkheads and internal walls
3. generate door and hatch closures
4. derive sealed graph
5. derive compartments
6. bake final runtime metadata
7. output final runtime asset

---

## 12. What Remains Valid From The Current Implementation

The following parts remain useful and should be preserved:

1. ring-based hull evaluation
2. one shared hull truth for exterior and interior
3. hull structural ownership bindings built during bake
4. raw baked mesh arrays as authoritative data
5. runtime actor consuming baked data only

These are still correct.

What changes is the editor-facing spatial truth.

---

## 13. What Must Be Deprecated

The following editor assumptions should be deprecated:

1. compartments are required before deck generation
2. bulkheads are part of the first hull bake contract
3. final internal partitioning must exist before the submarine is structurally valid

These assumptions block the intended product direction.

---

## 14. Short-Term Stabilization Rule

Before full pivot implementation, the current prototype path may still be used.

But only for:

- hull validation
- deck validation
- runtime actor bake validation
- early breach ownership validation

It must not be treated as the final long-term editor model for internal layout authoring.

---

## 15. Immediate Implementation Consequences

The next architecture wave should do the following:

1. introduce `USubmarineHullDeckAuthoringAsset`
2. split current bulkhead connection data away from base hull bake requirements
3. split `vertical opening` from `vertical connector`
4. make deck cutouts first-class baked geometry
5. keep hull breach ownership on the base hull asset
6. move compartment derivation to a later layer

This is the minimum pivot set.

---

## 16. Validation Gates

### Gate A - Base Bake

Must validate:

- hull bakes without compartments
- multiple decks bake correctly
- hatch and ladder openings cut decks correctly
- ramps remain walkable where defined
- hull breach ownership remains valid

### Gate B - Base Runtime

Must validate:

- runtime actor spawns from base baked asset
- walkable decks function
- hull damage and breach debug function
- no partition data is required for hull-only operation

### Gate C - Partition Overlay

Must validate:

- bulkheads can be added after base bake
- internal walls can be added after base bake
- doors and hatches can be added after base bake
- compartments can be derived from that overlay

### Gate D - Derived Compartments

Must validate:

- upper and lower spaces can be distinct compartments
- adjacent spaces can merge when partitions are absent
- flooding and pressure respect open and closed passages

---

## 17. Final Direction

The final direction after this pivot is:

- hull first
- decks first
- circulation first
- partitions later
- compartments derived later

This is a better fit for:

- multi-deck submarines
- flexible internal layout
- player-driven interior design
- future emergency partition gameplay
- full use of interior 3D volume

This is the architecture the next spec and implementation phases should follow.
