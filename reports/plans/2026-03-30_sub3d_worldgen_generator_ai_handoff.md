# Sub3D - WorldGen Generator Handoff For External AI

Date: 2026-03-30  
Status: Working handoff  
Goal: give another AI the minimum canonical context to understand the route generator and propose safe extensions.

---

## 1) Canonical file pack (10 files max)

Give these exact files first, in this order:

1. `C:\Dev\Sub3D\reports\plans\2026-03-29_sub3d_first_playable_run_spec.md`  
   Product/runtime contract and scope guard.

2. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\TraversalRouteActor.cpp`  
   Real pipeline orchestration (`RunPipeline`, bake flow, spawn flow, endpoint updates, logs).

3. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\TraversalRouteActor.h`  
   Public API, runtime outputs, build/bake entry points, exposed properties.

4. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\WorldGenTypes.h`  
   Shared data model (spec, seeds, topology/skeleton/field/semantic/validation structs and enums).

5. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\TraversalTopologyGenerator.cpp`  
   Topology graph generation logic.

6. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\SkeletonResolver.cpp`  
   C5 conversion from topology nodes to Bezier skeleton segments.

7. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\NavigableVolumeGenerator.cpp`  
   C6 guaranteed volume and voxel field rasterization.

8. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\RouteValidator.cpp`  
   C9 constraints/clearance/connectivity validation.

9. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\RouteMeshBuilder.cpp`  
   C10 marching-cubes extraction and mesh chunk generation.

10. `C:\Dev\Sub3D\Source\Sub3D\WorldGen\RouteArchetypeDataAsset.h`  
    Authoring schema that drives topology/shape/rhythm/complexity.

If context budget is smaller, use files 1-7 only.

---

## 2) Generator flow summary (ground truth from code)

Entry points:
- `ATraversalRouteActor::BuildRouteFromSpec(...)`
- `ATraversalRouteActor::RebakeInEditor()`
- `ATraversalRouteActor::BakeCurrentRouteToStaticMeshAsset()`

Upstream route recipe:
- `UCampaignRouteCompiler` derives `FRouteGenSpec` + `FRouteSeedCascade`.

Pipeline order in `TraversalRouteActor`:
- C4 Topology (`UTraversalTopologyGenerator`)
- C5 Skeleton (`USkeletonResolver`)
- C6 Volume (`UNavigableVolumeGenerator`)
- C7 Organic deformation (`UOrganicDeformationGenerator`)
- C8 Semantics (`URouteSemanticGenerator`)
- C9 Validation (`URouteValidator`)
- C10 Mesh build (`URouteMeshBuilder`)
- Sonar field init (`USonarFieldComponent::InitializeFromField`)
- Runtime mesh spawn (PMC sections) or baked static mesh resolution

Important runtime outputs already present on `ATraversalRouteActor`:
- `RouteStartTransform`, `RouteEndTransform`
- `RouteStartDockTransform`, `RouteEndDockTransform`
- corresponding radii
- `LastValidationReport`
- `RouteNetSpec` (spec+seeds+build hash, replicated)

Important design/runtime fact:
- Mesh is not replicated directly; clients rebuild from replicated recipe (`RouteNetSpec`).

---

## 3) What your external AI should treat as canonical

Canonical for product/run behavior:
- `2026-03-29_sub3d_first_playable_run_spec.md`

Canonical for generator runtime truth:
- `TraversalRouteActor.cpp`
- `WorldGenTypes.h`
- stage generator `.cpp` files listed above

Do not treat old staging docs or screenshots as authority when they conflict with code.

---

## 4) Safe extension target for your Helm Navigation Suite

Your idea (Front Cross-Section + Forward Anticipation + Tactical Graph) is compatible with current generator if implemented as data export from existing pipeline artifacts.

Recommended extension seam:
- add a new export asset generated alongside route build:
  - `UTunnelNavDataAsset` (new)

Recommended generation moments:
1. After C5 (skeleton ready): seed edge centerline samples.
2. After C6 (field ready): compute local cross-section/clearance metrics per sample.
3. After C9 (validation ready): stamp restrictions/warnings and route-quality metadata.
4. Before/after C10: persist stable IDs and optional render-assist metadata.

This avoids coupling helm UI to transient mesh triangles.

---

## 5) Minimum schema for `UTunnelNavDataAsset` (pragmatic MVP)

Keep MVP compact and deterministic:

- Route identity:
  - `BuildHash`
  - `CampaignSegmentID`
  - `RouteID`

- Graph:
  - `Nodes[]` (Hub/Branch/Checkpoint tags, world position)
  - `Edges[]` (from/to, length, canonical vs optional)

- Edge samples (uniform step, e.g. every 200-500 cm):
  - `DistanceAlongEdge`
  - `Center`, `Forward`, `Right`, `Up`
  - `InscribedRadius`
  - `MinClearanceLeft/Right/Up/Down`
  - `Curvature`, `Slope`
  - `VisibilityAheadDistance`
  - `RecommendedMaxSpeedByClass`
  - `bSupportsTurnaroundClassS/M/L`

This is enough to power:
- Screen A (cross-section + instantaneous clearance)
- Screen B (lookahead profile + narrowing/turn warnings)
- Screen C (local tactical graph)

---

## 6) Implementation guardrails for external AI

- Do not replace current route generation; extend via export step.
- Do not derive nav data from rendered triangles only; prefer skeleton + field model.
- Keep deterministic generation bound to `BuildHash`.
- Keep authoritative generation on server/editor build path, not per-client UI runtime.
- Do not break existing bake/runtime options (`bUseBakedStaticMeshAtRuntime`, mesh replacement flags).

---

## 7) One-line prompt to give your external AI

"Read the 10-file pack in order, treat `TraversalRouteActor.cpp` and `WorldGenTypes.h` as runtime truth, then propose and implement `UTunnelNavDataAsset` export at C5/C6/C9 without changing existing route behavior."

