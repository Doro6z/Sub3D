# Sub3D - Sub & Crew Patch State and Execution Contract

Date: 2026-03-31
Scope: Sub movement collision truth, interior walkable truth, crew embark/floor support, structural truth
Status: Patch 1 verified in code, Patch 2 partially wired into gameplay/runtime, build succeeded

## 0. Decision

Do not start by rewriting `USubCrewMovementComponent` or inventing a custom movement stack.

The correct order remains:

1. lock the exterior movement collision truth
2. lock the interior walkable truth
3. lock the crew floor-support and embark contract
4. only then tune FPS feel

This document is the implementation contract for continuing after Patch 1 and Patch 2.

## 1. Current Verified State

### 1.1 Patch 1 is present in code

Verified in:

- `Source/Sub3D/Submarine/SubMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineBase.h`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.h`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`
- `Config/DefaultEngine.ini`

Observed state:

- `USubMovementComponent` now uses an `IsInternalHit` guard inside `MoveWithSlide()`.
- Internal hits are ignored for both the primary sweep and slide sweep.
- The guard correctly treats same-owner, owned child actor, attached actor, deep attachment, and same-owner component as internal.
- `ASubmarineBase` now exposes `GetInteriorWalkableComponents()` and `GetCrewEmbarkTransform()`.
- `ASubmarineCompilerActor::GetMovementCollisionComponent()` prefers `ExteriorCollisionProxy`, then generated exterior mesh, before falling back to base behavior.
- `ASubmarineCompilerActor::GetInteriorWalkableComponents()` returns generated interior components using the `SubInteriorWalkable` collision profile.
- `DefaultEngine.ini` already defines `SubmarineHull` and `SubInteriorWalkable`.

Result:

- The submarine movement path is now explicitly separated from attached internal actors/components at the sweep stage.
- The submarine runtime facade now has distinct APIs for movement collision and crew support.

### 1.2 Patch 2 is present in code, but not fully consumed end-to-end

Verified in:

- `Source/Sub3D/Submarine/SubmarineLayoutAsset.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.cpp`

Observed state:

- `USubmarineLayoutAsset` now contains `FWalkableSurfaceDef` and `WalkableSurfaces`.
- `USubmarineGeometryBuilder::BuildInteriorMeshes()` now creates:
  - visual interior PMCs using `SubInteriorVisual`
  - walkable floor-only PMCs using `SubInteriorWalkable`
- `USubmarineBuildCompiler` still writes `WalkableFloorZCm` per compartment into `FSubCompartmentDef`.

Result:

- A dedicated walkable collision layer now exists at geometry generation time.
- The layout asset has a place to carry walkable truth data.
- However, the asset-level walkable truth is not yet the runtime source of truth for gameplay.

## 2. The Four Truths in the Current Architecture

### 2.1 Exterior movement truth

Current canonical direction is correct:

- submarine movement sweeps against a single exterior movement component
- compiled submarines prefer `ExteriorCollisionProxy`
- internal hits are filtered out in movement code

This is the correct base for Sub movement.

### 2.2 Interior walkable truth

Current direction is also correct:

- walkable floor geometry is now separated from interior visual geometry
- walkable floor uses `SubInteriorWalkable`
- `ASubmarineCompilerActor` can enumerate walkable components

This is the correct gameplay direction, but not yet fully consumed by embark/bootstrap logic.

### 2.3 Structural / flooding truth

`USubHullComponent` remains the structural truth:

- initialized from `USubmarineLayoutAsset`
- owns structural sheets, breaches, flow fields, compartment runtime state
- samples compartment state at crew world location

This should remain separate from crew floor support.

### 2.4 Crew support truth

Current direction:

- `ASubCrewCharacter::EnterOnFootInSubmarine()` teleports, stops movement, forces walking, initializes crew movement, refreshes flooring, and applies a vertical fallback trace if floor is not found
- `USubCrewMovementComponent` keeps stock `UpdateBasedMovement()` and `UpdateBasedRotation()`
- tick prerequisites are set on `SubMovement` and `InteriorFrame`
- `ASubGameMode` already has a bootstrap pipeline and validates basic crew embark state

This is a viable base, but the support contract is still only partially canonical.

## 3. Verified Gaps and Risks

These are the important deltas between the intended architecture and the current code.

### 3.1 `GetCrewEmbarkTransform()` exists but is not used by bootstrap

Verified in:

- `Source/Sub3D/Submarine/SubmarineBase.cpp`
- `Source/Sub3D/Submarine/SubGameMode.cpp`

Observed state:

- `ASubmarineBase::GetCrewEmbarkTransform()` currently delegates to `GetPrimaryCrewSpawnTransform()`
- `ASubGameMode::ResolveCrewSpawnTransform()` still calls `GetPrimaryCrewSpawnTransform()`, not `GetCrewEmbarkTransform()`

Impact:

- the new canonical embark API exists but gameplay/bootstrap is still bypassing it

### 3.2 `WalkableSurfaces` exists in the layout asset but is not populated or consumed

Verified in:

- `Source/Sub3D/Submarine/SubmarineLayoutAsset.h`
- `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.cpp`

Observed state:

- `WalkableSurfaces` is declared on the asset
- `USubmarineBuildCompiler` resets `Compartments`, `StructuralSheets`, `Doors`, and `StationSlots`
- no code currently populates `WalkableSurfaces`
- no gameplay/runtime code currently consumes `WalkableSurfaces`

Impact:

- Patch 2 introduced the data shape, but not the compiled data pipeline
- current walkable truth is still geometry-derived, not asset-driven

### 3.3 `SubInteriorVisual` is used in code but not declared in collision config

Verified in:

- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `Config/DefaultEngine.ini`

Observed state:

- interior visual PMCs and bulkheads are assigned `SubInteriorVisual`
- `DefaultEngine.ini` contains `SubmarineHull` and `SubInteriorWalkable`
- `DefaultEngine.ini` does not currently declare `SubInteriorVisual`

Impact:

- runtime/editor collision behavior for interior visual meshes is not fully explicit
- this weakens the collision contract and may depend on fallback engine behavior

### 3.4 `BuildGeneratedGeometry()` still validates against the old interior mesh count

Verified in:

- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`

Observed state:

- Patch 2 now generates two interior components per compartment in normal cases:
  - one visual PMC
  - one walkable PMC
- bulkheads are still generated separately
- `BuildGeneratedGeometry()` still returns success only if:
  - `GeneratedInteriorMeshes.Num() == Compartments + Bulkheads`

Impact:

- this predicate is stale relative to the new generation model
- compile/build may be reported as failed even when geometry generation actually succeeded
- this is a likely runtime/editor blocker for Patch 2 adoption

### 3.5 Crew embark still relies on channel trace fallback, not the new walkable API

Verified in:

- `Source/Sub3D/Submarine/SubCrewCharacter.cpp`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`

Observed state:

- `EnterOnFootInSubmarine()` still uses a downward line trace on `ECC_GameTraceChannel2` as fallback
- `RefreshEmbarkedFlooring()` still relies on generic `FindFloor()` and `SetBaseFromFloor()`
- neither path explicitly uses `GetInteriorWalkableComponents()`

Impact:

- the code is still relying on channel-level convention rather than an explicit canonical support set
- support is improved, but not yet fully locked to the new interior walkable truth

### 3.6 Crew bootstrap validation is still minimal

Verified in:

- `Source/Sub3D/Submarine/SubGameMode.cpp`

Observed state:

- `ValidateCrewBootstrap()` only checks:
  - `Crew->CurrentSubmarine == ActiveSubmarine`
  - `Crew->GetMovementBase() != nullptr`
  - `MovementMode == MOVE_Walking`

Impact:

- bootstrap does not verify that the base belongs to the submarine
- bootstrap does not verify that the floor is walkable
- bootstrap does not verify that the base is one of the canonical interior walkable components

## 4. Target Objective From Here

The next execution target is not "better feel" yet.

The next correct target is:

1. finish wiring Patch 2 into runtime truth
2. formalize the crew embark contract
3. stabilize movement base acquisition and recovery
4. only then tune interior FPS feel

## 5. Exact Collision Contract To Keep

### 5.1 `SubmarineHull`

Use for:

- exterior movement collision
- submarine sweep truth
- world collision authority

Must:

- ignore `Pawn` for internal crew capsule noise
- ignore `SubInterior`
- never be used as the interior crew floor truth

### 5.2 `SubInteriorWalkable`

Use for:

- crew floor support
- walkable floor traces
- movement base acquisition

Must:

- be ignored by submarine movement sweep
- block crew floor resolution
- remain dedicated to support, not visual authority

### 5.3 `SubInteriorVisual`

Use for:

- interior walls, ceilings, bulkheads, render-facing geometry

Must:

- be explicitly declared in config
- not become the authoritative support or movement truth

### 5.4 Crew capsule

Must:

- walk on `SubInteriorWalkable`
- not perturb submarine movement
- remain a crew-only probe inside the submarine frame

## 6. Implementation Order After This Document

### Phase 2.5 - Finish the Patch 2 glue

Small structural corrections before Patch 3:

1. declare `SubInteriorVisual` in `DefaultEngine.ini`
2. fix `ASubmarineCompilerActor::BuildGeneratedGeometry()` success criteria for the new mesh split
3. switch `ASubGameMode::ResolveCrewSpawnTransform()` to `GetCrewEmbarkTransform()`
4. decide whether `WalkableSurfaces` is now required as a compiled asset output or stays deferred

### Phase 3 - Crew embark and floor support stabilization

Primary files:

- `Source/Sub3D/Submarine/SubCrewCharacter.h`
- `Source/Sub3D/Submarine/SubCrewCharacter.cpp`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubGameMode.cpp`

Execution goals:

1. make embark sequence canonical
2. validate floor support explicitly
3. validate movement base explicitly
4. add recovery path when base is unexpectedly lost while embarked
5. harden bootstrap validation against false-positive success

### Phase 3.5 - Character embodiment architecture lock

Do this now at architecture level, before implementation-heavy animation work:

1. define the inertial signal contract exposed by submarine/runtime
2. define the crew embodiment state model:
   - stable
   - bracing
   - staggering
   - falling
   - recovering
   - narrow-space contact
3. define the support and proximity queries required by animation/procedural layers
4. define Editor and PIE checks before implementing leaning, hand placement, or fall reactions

Reference document:

- `reports/plans/sub3d/2026-03-31_sub_and_crew_embodiment_architecture_and_pie_check_plan.md`

### Phase 4 - Envelope cleanup

Primary files:

- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`
- `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.cpp`
- `Source/Sub3D/Submarine/SubmarineLayoutAsset.h`

Execution goals:

1. clean outer hull continuity
2. clean bulkhead transitions
3. keep walkable, visual, and structural outputs distinct

### Phase 5 - FPS feel tuning

Only after support is stable:

- walk speed
- acceleration
- braking/friction
- camera feel
- on-foot readability while submarine is moving
- embodied motion tuning:
  - leaning
  - bracing
  - contact hands
  - stagger/fall thresholds

## 7. GO / NO-GO

### GO

- submarine sweep no longer reacts to internal geometry
- compiled sub exposes one exterior movement truth
- crew spawn resolves onto a valid interior support surface
- crew base remains stable while the sub moves
- flooding logic remains on `USubHullComponent`

### NO-GO

- `USubCrewMovementComponent` is rewritten before support truth is locked
- visual interior geometry is treated as the crew floor truth
- bootstrap keeps bypassing `GetCrewEmbarkTransform()`
- Patch 2 geometry split stays half-wired and unvalidated
- FPS feel tuning starts before support/base stability is verified

## 8. Validation Checklist

### Code-level validation

- build succeeds in `Sub3DEditor Win64 Development`
- `BuildGeneratedGeometry()` returns success under the new interior mesh split
- `GetMovementCollisionComponent()` resolves to the exterior proxy on compiled subs
- `GetInteriorWalkableComponents()` returns walkable PMCs only

### Runtime validation

1. submarine departs without self-collision against interior geometry
2. crew spawn lands above a valid interior floor
3. crew movement base is acquired on embark
4. crew movement base remains stable during submarine motion
5. crew can move across compartments without major jitter or absurd gaps
6. breach/flooding logic still samples from `USubHullComponent`

## 9. Build Verification

Build status: succeeded
Command:

- `Build.bat Sub3DEditor Win64 Development -Project=C:\Dev\Sub3D\Sub3D.uproject -WaitMutex -NoHotReloadFromIDE`

Notes:

- Build succeeded on 2026-03-31 after fixing one local syntax error in `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`.
- The compile fix was structural only: removal of one extra closing brace before the bulkhead generation loop.
- This build validates C++ compilation only.
- It does not invalidate the runtime risks listed above, especially the stale `BuildGeneratedGeometry()` success predicate and the incomplete gameplay consumption of Patch 2 walkable truth.
