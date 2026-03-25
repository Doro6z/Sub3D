# Sub3D Proto 03 - Structural Hull Damage Handoff

Date: 2026-03-20  
Project: `C:/Dev/Sub3D`  
Status: `Implemented in C++ (runtime core) + Build OK + Editor launch OK`

Companion document for art direction, spatiality, and asset realization:
`C:/Dev/Sub3D/reports/plans/20260320-proto03-da-asset-realization-plan.md`

## 1. Scope Locked (Proto 03)

- Single hull wall logic (no dual internal/external shell simulation).
- Dynamic localized damage anywhere on hull or internal sheets.
- No pre-cut meshes, no baked panel destruction dependency.
- Runtime math field drives:
  - local durability
  - leak/open transitions
  - breach clustering
  - suction/flow fields
  - flooding by compartment
- Visual hole rendering is decoupled (material/VFX pass can be added later without changing core logic).

Important confirmation: **Sheets are purely logical simulation surfaces**.  
They do not force visual panel cuts. Mesh rendering can stay independent.

## 2. Implemented C++ Architecture

### Core classes

- `USubHullComponent`
Path: `C:/Dev/Sub3D/Source/Sub3D/Submarine/SubHullComponent.h`
- `USubmarineLayoutAsset`
Path: `C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineLayoutAsset.h`
- Shared structs/enums
Path: `C:/Dev/Sub3D/Source/Sub3D/Submarine/StructuralHullTypes.h`

### Owner integration

- `ASubmarineBase` now owns `USubHullComponent` and forwards hit damage to it from `OnHullHit`.
Path: `C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp`

## 3. Runtime Data Model

### Authoring data (static)

- `FSubCompartmentDef`
  - `CompartmentId`
  - `CapacityLiters`
- `FStructuralSheetDef`
  - local plane: `LocalOrigin`, `LocalNormal`, `LocalTangentX`, `LocalTangentY`
  - size: `SizeCm`
  - grid: `GridResolutionX/Y`
  - material params: `ThicknessCm`, `MaterialStrength`
  - topology role: `ParentCompartmentId`, `AdjacentCompartmentId`, `bCanOpenToExterior`

### Runtime state (replicated)

- `FStructuralSheetRuntimeState` (per-cell `Damage01`, `ThicknessRemaining`, `bLeaking`, `bOpen`)
- `FBreachClusterState` (connected open cells merged into dynamic holes)
- `FBreachFlowField` (suction/projection cloud per breach cluster)
- `FCompartmentRuntimeState` (flood liters, water level, pumps)

## 4. Server Tick Flow

```text
Physics Hit (HullMesh OnComponentHit)
    -> Convert Hit point to Local space
    -> Project impact on nearest logical sheet (UV + plane distance)
    -> Radial damage on sheet cells (falloff)
    -> Cell state transitions (intact -> leak -> open)
    -> Rebuild open-cell clusters (BFS connected components)
    -> Build flow fields per cluster (radius/force/passage state)
    -> Integrate flooding per parent compartment
    -> Replicate runtime arrays to clients
```

Passage state is data-driven from cluster radius:
- `LeakOnly`
- `StrongSuction`
- `ActorEjectable`
- `CreatureEnterable`

## 5. Dynamic Interaction (No Mesh Dependency)

`USubHullComponent` exposes:
- `SampleSuctionAtWorldLocation(...)`
  - returns suction direction + force scale + strongest passage state at a world point.
- `GetLargestOpenRadiusCm()`
  - quick debug/telemetry for breach severity.

This supports dynamic actor/fauna behavior without hardcoded `CanPass` panels.

## 6. Fallback Behavior (No Layout Asset)

If no `USubmarineLayoutAsset` is assigned:
- component auto-generates 4 logical exterior sheets from hull mesh bounds:
  - port, starboard, top, bottom
- creates one default compartment `HullMain`

This makes the prototype immediately testable on existing submarine actors.

## 7. Editor Handoff - Exact Setup

1. Open `Sub3D.uproject`.
2. Ensure submarine actor class is `ASubmarineBase`.
3. Select submarine actor (or BP subclass).
4. In `SubHull` component:
   - optionally assign `Layout Asset` (`USubmarineLayoutAsset`)
   - set `bDrawDebug = true`
   - tune:
     - `LeakThreshold` (start with `0.35`)
     - `OpenThreshold` (start with `1.0`)
     - `BaseLeakFlowLitersPerSec` (start with `120`)
     - `BaseSuctionRadiusCm` (start with `100`)
5. In actor:
   - set `HullImpactDamageScale` (start with `0.0001`)
   - set `HullImpactRadiusCm` (start with `18`)
6. PIE and collide/impact hull.
7. Validate debug:
   - cyan spheres: breach clusters
   - orange arrows + green radius: suction flow fields
8. Verify flooding by reading `CompartmentStates` in details/debug.

## 8. 3D Artist / Asset Authoring Requirements

For Proto 03, visual meshes are independent from simulation sheets.  
Authoring should still respect these constraints for coherence:

- Use one solid hull mesh (single-wall gameplay, real thickness visual mesh preferred).
- Keep transforms clean: forward `+X`, up `+Z`, scale `1,1,1`.
- Reasonable pivot near submarine root center.
- Exterior hull should be continuous and readable for impact VFX placement.
- Optional future layer: interior wall masks/materials for damage decal projection.

Minimum required assets now:
- Submarine exterior mesh (already present/imported)
- Optional simple interior collision shell
- Breach Niagara prototype FX (small jet, medium jet, heavy jet)
- One master hull material instance with damage mask hooks (future visual phase)

## 9. AI-Assisted Asset Pipeline (Fast Proto)

- Generate first-pass variants with Meshy/Tripo/Rodin.
- Cleanup in Blender:
  - normals
  - pivot/scale
  - non-manifold fixes
- Import to UE5 with Nanite where needed.
- Keep destruction logic in C++ sheets; do not encode gameplay holes in mesh topology.

## 10. Verification Done

- C++ build command:
  - `Build.bat Sub3DEditor Win64 Development C:/Dev/Sub3D/Sub3D.uproject -WaitMutex -NoHotReloadFromIDE`
- Result: `Succeeded` (latest run).
- Editor launch:
  - `UnrealEditor.exe C:/Dev/Sub3D/Sub3D.uproject`
- Result: process responsive, window title `Unreal Editor - Sub3D`.

## 11. Next Implementation Slice (Recommended)

1. Hook `SampleSuctionAtWorldLocation` into crew movement and fauna locomotion.
2. Add deterministic visual damage layer:
   - render target stamps by impact UV
   - leak/hole material blending by replicated cell state
3. Add internal water visual prototype per compartment:
   - client-side heightfield / render target driven surface
   - server remains authoritative on `FloodLevel` and transfer events
   - inject motion bias from submarine acceleration / roll / pitch
   - inject local surge events from door openings and breaches
4. Spawn/drive Niagara jets from `FlowFields`.
5. Add repair gameplay:
   - weld action reduces local cell damage
   - foam patch temporarily clamps flow
6. Add structured QA map with scripted impacts at roof/corners/floor.
