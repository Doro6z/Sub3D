# Review — Sub3D Water Phase 2 (commit `50f8070`)

**Reviewer:** Antigravity (independent, no chat history)
**Date:** 2026-05-05
**Branch:** `water-proto`
**Commit:** `50f80706a3ff640fc13876d97487a8f73bd1b576`
**Build:** Green (UBT Sub3DEditor Win64 Development, per commit message)

---

## 1. Summary

Build is green, module dependency graph is a valid DAG, all editor-only
API calls (`SavePackage`, `AssetRegistryModule`) are properly gated with
`#if WITH_EDITOR`. Three notable issues: (1) a first-frame PMC visibility
problem where the bake cap mesh starts visible when water is empty, (2)
`IsLocalPosInsideAnyVolume` uses AABB in sub-local space that is only
correct for axis-aligned volumes — no comment flags this assumption, (3)
`IsValid()` door width check compares against the **Y span** of compartments
regardless of door orientation, which is semantically wrong for
X-aligned doors. Six observations at P1/P2 level. No P0 blockers found.
Phase 3 readiness is good with one caveat on slice clamping.

---

## 2. P0 Issues — Must fix before bake is used

**None found.**

All five P0 checks passed:

### P0.1 — Module dependency DAG ✅

- `Sub3DCore` deps: `Core`, `CoreUObject`, `Engine`. No Sub3D dep.
- `Sub3D` deps: includes `Sub3DCore` as `PublicDependencyModuleNames`
  (`Sub3D.Build.cs` line 22). `UCompartmentWaterBake` is used as a forward
  declaration in `SubmarineDefinition.h:84` (`class UCompartmentWaterBake`)
  and included fully only in `FloodWaterPlaneComponent.cpp:16`
  (`#include "Types/CompartmentWaterBake.h"`). That include path resolves via
  Sub3D → Sub3DCore dep. ✅
- `Sub3DWaterBake` deps: `Sub3D` + `Sub3DCore` + engine modules. No
  reverse dep from Sub3DCore or Sub3D back to Sub3DWaterBake. ✅
- DAG: `Sub3DCore ← Sub3D ← Sub3DWaterBake`. No cycle.

### P0.2 — `#if WITH_EDITOR` gating ✅

`SubmarineWaterBakerLibrary.cpp` has the following `#if WITH_EDITOR` blocks:
- Lines 14–19: includes for `AssetRegistryModule`, `PackageName`,
  `Package`, `SavePackage`.
- Lines 429–466: the entire `CreateOrLoadBakeAsset()` and `SaveAsset()`
  functions in the anonymous namespace.
- Lines 550–556: the `BakeCompartment` asset creation/load path (shipping
  fallback uses `NewObject<>(GetTransientPackage())` — runtime-only, no save).
- Lines 705–713 and 711–713: `SaveAsset(Bake)` and
  `Definition->MarkPackageDirty()` calls inside `BakeCompartment`.
- Lines 746–748: `Definition->MarkPackageDirty()` in `BakeAllCompartments`.

All editor-only APIs are correctly gated. Shipping builds see only the
voxelisation + tessellation math, which has no editor dependencies.

### P0.3 — UPROPERTY on `WaterBakes` TMap ✅

`SubmarineDefinition.h:83–84`:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Bakes")
TMap<FName, TObjectPtr<class UCompartmentWaterBake>> WaterBakes;
```
UPROPERTY present. `TObjectPtr` is the UE5-idiomatic GC-tracked pointer.
The GC will keep each `UCompartmentWaterBake*` alive as long as the
`USubmarineDefinition` DA is alive. ✅

### P0.4 — Coordinate system in `ComputeUnionBoundsLocal` ✅

`SubmarineWaterBakerLibrary.cpp:355–401`. The function:
1. Takes `InvSub = SubmarineWorldXf.Inverse()`.
2. For each `UCompartmentVolumeComponent`, gets its **world** transform via
   `Vol->GetComponentTransform()` — correct, handles arbitrary attachment
   hierarchies.
3. Projects all 8 corners (`CornerVolLocal → CornerWorld → CornerSubLocal`)
   through the volume's own world transform then through `InvSub`.

Manual trace for a volume with `RelativeTransform = (T=(-500,0,0),
R=(0,0,0), S=(1,1,1))` attached to the submarine root at world origin:

- `VolWorldXf.TransformPosition({-HX, -HY, -HZ})` → world corner at
  `(-500-HX, -HY, -HZ)`.
- `InvSub.TransformPosition(world)` → sub-local `(-500-HX, -HY, -HZ)`
  (sub at world origin, no rotation). Correct.

For a volume with a non-zero parent component (e.g. attached to a mesh
socket): `GetComponentTransform()` returns the **full world** transform
including all parent transforms, so this is safe. ✅

### P0.5 — `bBakeResolveAttempted` retry ✅ (intended, not a bug)

`FloodWaterPlaneComponent.cpp:131–138`: once `bBakeResolveAttempted = true`
and `CachedBake = nullptr`, the function returns early forever. If the
bake is run while PIE is live, the component never picks it up. This is
**intentional**: the comment says "Stay on legacy SMC path forever (until BP
changes WaterBakes)". The component's `CachedBake` is `Transient`, so a PIE
restart naturally re-resolves. The workflow contract is: bake first, then
PIE. This is documented in the commit message workflow steps 1–6. Acceptable
for FP scope; could be improved post-FP by adding a `ForceRebake()` callable.

---

## 3. P1 Issues — Should fix soon

### P1.1 — First-frame PMC visibility race

**File:** `FloodWaterPlaneComponent.cpp:131–162` (first-time bake setup) and
`FloodWaterPlaneComponent.h:118` (`bLastVisible = false`).

**Problem:**

When `bBakeResolveAttempted = false` and a bake is found for the first time
(i.e., first call to `RefreshBakeCapMesh`), the code:
1. Allocates `BakeCapMeshComp` via `NewObject<UProceduralMeshComponent>`.
2. Calls `RegisterComponent()`.

`UProceduralMeshComponent` inherits `UPrimitiveComponent::bVisible = true`
by default (the engine default). No explicit `SetVisibility(false)` is
called on `BakeCapMeshComp` at creation time.

Back in `RefreshFromFlood` (line 73):
```cpp
if (CachedBake && BakeCapMeshComp && BakeCapMeshComp->GetNumSections() > 0)
```
On this very first frame, `GetNumSections() == 0` (no mesh uploaded yet),
so the bake path block is **skipped**. The `BakeCapMeshComp` remains
registered and visible (engine default) for one frame with no geometry.
Actually no geometry means nothing is rendered — this is likely benign in
practice, but it is also technically a state leak.

The real exposure is the **next frame**: sections are still 0 until
`RefreshBakeCapMesh` is called again (tick 2). At tick 2, if water is empty
(`Level01 < VisibilityThreshold01`), then:
- `bShouldBeVisible = false`
- `bLastVisible = false` (initialized in `.h`)
- Delta check `bShouldBeVisible != bLastVisible` → **false** → `SetVisibility`
  is **not called**.
- `BakeCapMeshComp` remains at engine-default visibility (`true`).

If the first uploaded mesh happens to be an empty/degenerate slice (no
vertices), this is still invisible in practice. But if the first uploaded
slice has geometry and water is empty, the cap mesh shows at Z=0 for one
tick.

**Suggested fix (one-liner):**

In `RefreshBakeCapMesh` at line 143, after `RegisterComponent()`:
```cpp
BakeCapMeshComp->SetVisibility(false, true);  // hidden until first RefreshFromFlood delta
```

Then force the initial visibility sync by setting `bLastVisible = true`
before calling `SetVisibility(false)` in `EnsurePlaneMesh` equivalent, so
the delta fires on first real tick. Or more simply: initialize
`bLastVisible = true` in the header so the delta fires on the first frame
regardless.

### P1.2 — Slice picking when water height exceeds `MaxWaterHeightCm`

**File:** `FloodWaterPlaneComponent.cpp:172`.

```cpp
const float WaterZLocal = CachedBake->LocalBoundsMin.Z + NewWaterHeightLocalCm;
```

`NewWaterHeightLocalCm` is passed directly from
`Source->GetWaterHeightCm()`. If `MaxWaterHeightCm` (the compartment's
water capacity ceiling) is smaller than the bake's Z extent
(`LocalBoundsMax.Z - LocalBoundsMin.Z`) — which is the **known** "L=1 →
50% visual" misconfiguration from user feedback — then
`WaterZLocal` can exceed `LocalBoundsMax.Z`.

In that case, the nearest-slice search will correctly pick the **topmost**
slice (the for-loop clamps naturally by minimum difference). This is the
correct fallback — the water surface is at or above the top bake slice, so
showing the topmost slice is reasonable. No clamp is needed; the slice
search is already monotone.

**However**, `WaterZLocal` passed to `SetRelativeLocation` line 188/213 will
be above `LocalBoundsMax.Z`, positioning the PMC above the baked geometry.
This produces a visible gap between the cap mesh silhouette and the actual
water height. The root fix is to clamp `NewWaterHeightLocalCm` to
`CachedBake->LocalBoundsMax.Z - CachedBake->LocalBoundsMin.Z` before the
slice search and `SetRelativeLocation`. This is low-risk and would eliminate
the floating-PMC artifact without touching the bake.

**Suggested fix:**
```cpp
const float ClampedHeightCm = FMath::Clamp(NewWaterHeightLocalCm,
    0.f, CachedBake->LocalBoundsMax.Z - CachedBake->LocalBoundsMin.Z);
const float WaterZLocal = CachedBake->LocalBoundsMin.Z + ClampedHeightCm;
```

---

## 4. P2 Issues — Nice to have

### P2.1 — `IsLocalPosInsideAnyVolume`: OBB assumption undocumented

**File:** `SubmarineWaterBakerLibrary.cpp:403–416`.

```cpp
bool IsLocalPosInsideAnyVolume(const FVector& LocalPos,
    const TArray<FVolumeBoundsLocal>& Volumes)
```

`FVolumeBoundsLocal` stores a sub-local `Center` + `HalfExtent`, and the
test is an AABB check. `FVolumeBoundsLocal` is computed by projecting the 8
corners of the volume (in volume-local space) through the **volume's world
transform → sub inverse**, then taking `ComponentMin/Max`. This is the AABB
of the OBB in sub-local space. For a rotated volume, the AABB is a
conservative superset: cells that pass the AABB test but are outside the
true OBB are **incorrectly marked as inside a volume**, and combined with
the parity raycast (which may say "inside hull"), they produce spurious
inside cells.

For axis-aligned compartments — the expected case for Craniata — this is
correct. The code comment at line 338–343 explains the compound room
rationale but does not mention the axis-aligned assumption.

**Suggested fix:** Add one comment line at line 403:
```cpp
// NOTE: stored bounds are sub-local AABB of the OBB. Accurate only for
// axis-aligned volumes. Rotated volumes will include false positives
// (cells in the AABB outside the true OBB). Acceptable for Craniata
// which has axis-aligned compartments only.
```

### P2.2 — Door contract validation: Y-span heuristic wrong for X-aligned doors

**File:** `SubmarineDefinition.cpp:264–272`.

The check computes:
```cpp
const float SmallestYSpan = FMath::Min(
    CompA->HydroBoundsMax.Y - CompA->HydroBoundsMin.Y,
    CompB->HydroBoundsMax.Y - CompB->HydroBoundsMin.Y);
if (Conn.DoorWidthCm > SmallestYSpan) { UE_LOG Warning; }
```

The plan's door contract (Section 2 of
`reports/plans/2026-05-04_water_implementation_plan.md`) defines the
contract as: *the door must be narrower than the connecting wall span*, where
the wall span is on the axis **perpendicular to the corridor direction**.
For a Y-aligned corridor (two compartments side-by-side in Y), the
separating wall is in the XZ plane and the relevant span is the X width.
For an X-aligned corridor (fore-aft), the relevant span is Y. The code
always uses Y, which is only correct for fore-aft doors.

Craniata is a fore-aft layout, so Y is correct for the current sub. This is
a semantic mismatch that won't fire incorrect warnings today but could for
future subs. Since this is warnings-only and the comment already says
"crude approximation", this is P2.

**No immediate fix required** — add a comment noting the assumption:
```cpp
// Heuristic: uses Y span (correct for fore-aft corridors, wrong for beam-wise doors).
// Craniata is fore-aft only — acceptable for this phase.
```

### P2.3 — Magic constants undocumented

**File:** `SubmarineWaterBakerLibrary.cpp:582–583`.

```cpp
constexpr float SDF_MaxDistanceCm = 1000.f;
constexpr float SDF_ParityRayLength = 100000.f;
```

`SDF_MaxDistanceCm = 1000.f` (10 m) caps the SDF magnitude. The largest
Craniata compartment dimension is ~1200 cm (estimated from HullLengthCm in
the DA). For a cell near the center of a wide compartment, the nearest wall
could be >1000 cm — the SDF magnitude would be clamped and the MarchingSquares
interpolation would use the clamped value. This is harmless for contour
extraction (sign matters, not magnitude), but could affect any future consumer
of `SignedDistance` values directly (Phase 3 heightfield). Suggest renaming
or documenting: `SDF_MaxDistanceCm` is a magnitude cap for performance, not
a spatial limit.

`SDF_ParityRayLength = 100000.f` (1 km) is used for parity rays. Correct —
it must exceed the largest possible sub dimension. OK as-is; add one comment.

### P2.4 — `BakeAllCompartments` continues on failure ✅ (by design)

**File:** `SubmarineWaterBakerLibrary.cpp:735–740`.

```cpp
UCompartmentWaterBake* Result = BakeCompartment(...);
if (Result) ++Successful;
```

On a per-compartment failure, iteration continues. This is correct — a
missing `UCompartmentVolumeComponent` for one compartment should not abort
the whole bake run. The final log at line 742 reports the ratio. ✅

### P2.5 — `LogTemp` instead of `LogSub3D` in `IsValid()` door warnings

**File:** `SubmarineDefinition.cpp:269, 288, 294`.

```cpp
UE_LOG(LogTemp, Warning, TEXT("[DA Door Contract] ..."));
```

All other Sub3D logs use dedicated categories (`LogFloodWaterPlane`,
`LogWaterBake`, etc.). `LogTemp` makes these harder to filter. Low priority
— no functional impact.

### P2.6 — `SavePackage` API signature

**File:** `SubmarineWaterBakerLibrary.cpp:464`.

```cpp
UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
```

This is the UE5 four-argument `FSavePackageArgs` overload:
`SavePackage(UPackage*, UObject*, const TCHAR*, const FSavePackageArgs&)`.
This overload is present in UE5.1+ and is the documented current API.
`FSavePackageArgs` with `TopLevelFlags` and `SaveFlags` is the correct
pattern for DA assets. ✅ (verified against UE5 public API — no issue.)

---

## 5. Architecture Commentary

The three-module layout is sound and matches the stated dependency claim.
`Sub3DCore` as a zero-dependency data module for `UCompartmentWaterBake`
is the right call — it allows `Sub3D` (runtime) to reference bake data
without pulling in the baker library, and `Sub3DWaterBake` to write into
`Sub3D` structures without creating a cycle.

The split between offline-bake (physics raycast voxelisation, Marching
Squares in `Sub3DWaterBake`) and runtime consumption (PMC slice-swap in
`FloodWaterPlaneComponent`) is clean and respects the architecture contract.
The PMC is a good pragmatic choice for FP: it defers UE static mesh
authoring while still giving renderable geometry per compartment.

One mild over-engineering note: `FDoorBoundaryCells` and `DoorCellMap` in
`UCompartmentWaterBake` are Phase 4 structures stored in every bake asset
now, adding serialisation weight for zero Phase 2 value. They are empty by
default so they don't break anything, but they will confuse anyone opening
the DA and wondering why Phase 2 assets have door cell maps. A comment is
present in the header (`// Phase 4 prep`) — sufficient.

The anonymous namespace in `SubmarineWaterBakerLibrary.cpp` for the
geometry helpers is correct C++ practice. The `ChainSegmentsIntoPolygon`
O(N²) greedy search could become slow for very large contours (>1000
segments), but for compartment-scale geometry at 25 cm cells this is fine.

---

## 6. Confidence on Phase 3 Readiness

**Overall: high**, with one caveat.

The `FCompartmentBakeSlice` struct stores the full `SignedDistance` TArray
per slice, which is exactly what a CPU heightfield port (P3.4–P3.6) would
consume. The `HeightfieldResolutionX/Y` fields are already on the DA.
`LocalBoundsMin/Max` give the spatial mapping. The data contract between
Phase 2 bake output and Phase 3 consumer is clean.

**Caveat:** P1.2 above. If Phase 3 reads `WaterZLocal` directly from a
`GetWaterHeightCm()` that exceeds `MaxWaterHeightCm` and maps it into the
heightfield grid without clamping, the grid lookup will go out of bounds.
The fix in P1.2 (`ClampedHeightCm`) should be applied before Phase 3 work
starts — it's a one-liner in `FloodWaterPlaneComponent.cpp:172` and removes
a latent Phase 3 array-bounds risk.

**No structural changes to Phase 2 are needed** before Phase 3 begins.
The bake pipeline, DA schema, and runtime consumption path are all solid.

---

## Appendix: Files Reviewed

| File | Status |
|---|---|
| `Source/Sub3DWaterBake/Sub3DWaterBake.Build.cs` | ✅ |
| `Source/Sub3DWaterBake/Private/SubmarineWaterBakerLibrary.cpp` | ✅ (P2.1, P2.3) |
| `Source/Sub3DWaterBake/Public/SubmarineWaterBakerLibrary.h` | ✅ |
| `Source/Sub3DCore/Public/Types/CompartmentWaterBake.h` | ✅ |
| `Source/Sub3DCore/Sub3DCore.Build.cs` | ✅ |
| `Source/Sub3D/Sub3D.Build.cs` | ✅ |
| `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h` | ✅ (P1.1) |
| `Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp` | ✅ (P1.1, P1.2) |
| `Source/Sub3D/Submarine/Generator/SubmarineDefinition.h` | ✅ |
| `Source/Sub3D/Submarine/Generator/SubmarineDefinition.cpp` | ✅ (P2.2, P2.5) |
| `Source/Sub3D/Submarine/Generator/SubmarineDefinitionTypes.h` | ✅ |
