# Sub3D - Submarine Generator Shape Execution Plan

Date: 2026-04-10
Input: `2026-04-08_submarine_generator_architecture_unified.md`
Status: Actionable execution plan for the shape phase only.

This document is not a parallel architecture plan.
It is the execution document for Sections 3 to 6 of:
`C:\Dev\Sub3D\reports\plans\2026-04-08_submarine_generator_architecture_unified.md`

If this document conflicts with the authoritaire document, the authoritaire document wins.

---

## 0. Scope freeze

This phase changes only submarine shape generation and the visibility split between render and collision.

Allowed:
- `SubmarineGeneratorEnvelopeDef`
- `SubmarineMeshBuilder`
- `SubmarineGeneratedGeometryComponent`
- minimal debug exposure on `ASubmarineBase` only if the component route is insufficient
- validation assets and PIE checks for `L_FP_GeneratorRun`

Not allowed:
- no flood changes
- no topology changes in `USubmarineGenerator`
- no station logic changes
- no spawn logic changes
- no airlock behavior changes
- no return to `SubCompiler`, `Sub3DBake`, or `SubmarineBakedRuntimeActor` as runtime source of truth

Objective:
- keep the current runtime spine intact
- improve exterior shape quality
- make interior geometry readable
- remove visible technical geometry from gameplay views

---

## 1. Current code baseline

These facts are verified in code and drive the execution order.

1. `USubmarineGeneratorEnvelopeDef` still contains `RadiusProfile`.
   - File: `Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.h`
   - File: `Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.cpp`

2. `USubmarineMeshBuilder` is the only mesh data producer for the new pipeline.
   - `BuildMeshData`
   - `BuildExteriorHull`
   - `BuildInteriorCompartments`
   - `BuildBulkheads`
   - File: `Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp`

3. `USubmarineGeneratedGeometryComponent` already materializes generated mesh data at runtime.
   - File: `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.cpp`

4. `ASubmarineBase` already builds geometry from `GeneratedDefinition`.
   - File: `Source/Sub3D/Submarine/SubmarineBase.cpp`

5. `USubmarineGenerator` already generates topology, compartments, airlock, flood graph, stations, and spawns.
   - This phase does not change that contract.

---

## 2. Files allowed in this execution plan

### Primary code files

- `Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.h`
- `Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.cpp`
- `Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.h`
- `Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp`
- `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.h`
- `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.cpp`

### Conditional code files

- `Source/Sub3D/Submarine/SubmarineBase.h`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`

Only touch `SubmarineBase` if debug isolation cannot be exposed cleanly from `USubmarineGeneratedGeometryComponent`.

### Validation assets

- `/Game/Sub3D/FirstPlayableRun/DA_Envelope_FPRun`
- `/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun`
- `/Game/Sub3D/FirstPlayableRun/BP_Submarine_FpRun`
- `/Game/Sub3D/FirstPlayableRun/L_FP_GeneratorRun`

---

## 3. Execution order

No batch commit.
Each step is completed, compiled, and validated before the next one starts.

### Step 1 - Instrumentation and section isolation

Purpose:
- identify the real cause of interior visual pollution before rewriting geometry

Code changes:
- `SubmarineMeshBuilder.cpp`
  - add logs after each sub-step in `BuildMeshData`
  - log vertex count and triangle count for:
    - exterior hull
    - interior meshes
    - bulkhead meshes
    - airlock geometry if emitted
- `SubmarineGeneratedGeometryComponent.h/.cpp`
  - add validation-only toggles:
    - `bBuildExteriorHull`
    - `bBuildInterior`
    - `bBuildBulkheads`
    - `bBuildAirlock`
  - default all to `true`
  - use them only to skip materialization of sections during validation

Rules:
- no geometry algorithm changes in this step
- no topology changes

Verification:
- build clean
- PIE in `L_FP_GeneratorRun`
- isolate each section visually
- record whether the interior pollution comes from:
  - visible collision
  - duplicate render geometry
  - bad normals/winding
  - airlock double emission

Exit condition:
- one root cause identified and written down

---

### Step 2 - Envelope contract cleanup

Purpose:
- make the shape contract match the authoritaire document

Code changes:
- `SubmarineGeneratorEnvelopeDef.h`
  - remove `RadiusProfile`
  - add:
    - `BowCapLengthCm`
    - `SternCapLengthCm`
    - `BowSharpness`
    - `SternSharpness`
    - `BodyLengthFraction`
    - `ControlRingCount`
- `SubmarineGeneratorEnvelopeDef.cpp`
  - rewrite `EvaluateRadius`
  - rewrite `EvaluateBowSternTaper`

Required formula:
- `RemainingFraction = max(0, 1 - BodyLengthFraction)`
- `WeightSum = max(0.001, BowTaperFraction + SternTaperFraction)`
- `BowShapeFraction = RemainingFraction * BowTaperFraction / WeightSum`
- `SternShapeFraction = RemainingFraction * SternTaperFraction / WeightSum`
- `BodyStart = BowShapeFraction`
- `BodyEnd = 1 - SternShapeFraction`
- `DefaultRadiusCm` on `[BodyStart, BodyEnd]`
- taper logic on `[0, BodyStart]` and `[BodyEnd, 1]`

Rules:
- `BowCapLengthCm` and `SternCapLengthCm` do not affect `EvaluateRadius`
- they affect only cap geometry in `BuildExteriorHull`

Verification:
- build clean
- create/update `DA_Envelope_FPRun`
- confirm editor details panel contains only the new shape fields

Exit condition:
- envelope asset is editor-clear and no longer curve-driven

---

### Step 3 - Exterior hull quality pass

Purpose:
- replace the current hardcoded cap shape with controllable bow/stern geometry

Code changes:
- `SubmarineMeshBuilder.cpp`
  - `BuildExteriorHull`
    - remove hardcoded cap length logic
    - use `BowCapLengthCm` / `SternCapLengthCm`
    - use `BowProfile` / `SternProfile`
    - use `BowSharpness` / `SternSharpness`
    - audit and fix winding consistency between body and caps
    - remove or gate radius easing that breaks control ring fidelity

Required constraints:
- no import from legacy bake services
- no new appendage logic
- no change to compartment topology

Verification:
- build clean
- PIE with at least 3 envelope variants:
  1. default values
  2. longer bow, short stern
  3. short bow, longer stern
- confirm silhouette responds to parameters

Exit condition:
- exterior hull reads as a coherent submarine shape from outside

---

### Step 4 - Bulkhead door fix

Purpose:
- stop bulkhead doors from turning into solid panels when floor rises

Code changes:
- `SubmarineMeshBuilder.cpp`
  - `BuildBulkheads`
  - replace the current fan-from-pivot cutout logic
  - use the retained fallback from the authoritaire document:
    - 3-band construction
    - left band
    - right band
    - upper band

Rules:
- no polygon-star triangulation
- no broad rewrite of unrelated bulkhead behavior
- door opening remains a shape-only cutout, not a gameplay change

Verification:
- build clean
- PIE with:
  - `FloorDropBiasCm = 0`
  - `FloorDropBiasCm = 40`
  - `FloorDropBiasCm = 80`
- confirm the opening remains visible in all three cases

Exit condition:
- no filled door where a passage is expected

---

### Step 5 - Floor readability and material-side validation

Purpose:
- fix the current "visible only from below" floor issue
- confirm whether two-sided material solves the visible symptom cleanly

Code changes:
- `SubmarineMeshBuilder.cpp`
  - keep the floor mesh simple
  - do not duplicate triangles unless material-side validation fails

Editor validation:
- assign validation materials that are explicitly two-sided for:
  - exterior hull render
  - interior render
  - floor render

Rules:
- this step does not introduce a new material pipeline
- material assignment stays validation-only and editor-assigned

Verification:
- PIE from inside and outside
- confirm floor is visible from gameplay camera
- confirm hull is no longer transparent because of one-sided material only

Exit condition:
- floor visibility issue is resolved with the smallest stable fix

---

### Step 6 - Interior pollution fix

Purpose:
- remove visible technical geometry from the interior without destabilizing the runtime path

Decision gate:
- use the root cause identified in Step 1

Path A - collision visibility confirmed:
- `SubmarineGeneratedGeometryComponent.cpp`
  - keep render and collision creation separate
  - ensure collision PMCs are hidden and remain hidden in validation view
  - verify collision settings do not force unwanted visible output in editor modes

Path B - duplicate render geometry confirmed:
- `SubmarineMeshBuilder.cpp`
  - remove duplicate emission between:
    - interior walls
    - bulkheads
    - caps
    - airlock-related geometry

Path C - normals/winding confirmed:
- `SubmarineMeshBuilder.cpp`
  - fix only the affected interior section winding

Rules:
- apply exactly one targeted fix path first
- do not mix multiple rewrites in the same commit unless verification proves they are the same root cause

Verification:
- build clean
- PIE in normal lit view and wireframe
- compare before/after screenshots from the same camera positions

Exit condition:
- interior is readable as playable volume
- no large technical planes or solid triangulated volumes remain visible

---

### Step 7 - Final validation pass

Purpose:
- lock the shape phase before any GeneratorEditor work

Validation scene:
- `L_FP_GeneratorRun`
- `BP_Submarine_FpRun`
- `DA_Envelope_FPRun`
- `DA_SubGenSpec_FPRun`

Validation list:
1. exterior silhouette is controllable by envelope parameters
2. hull is visible from outside
3. floor is visible from player-relevant views
4. doors remain open as cutouts with non-zero floor bias
5. interior is readable and not filled with technical geometry
6. stations still spawn
7. no `[LEGACY]` warning is introduced by this phase
8. flood runtime path remains untouched

Exit condition:
- all 8 checks pass in PIE

---

## 4. Commit structure

Use this exact order.

1. `shape-step1-instrumentation-and-isolation`
2. `shape-step2-envelope-contract`
3. `shape-step3-exterior-hull-quality`
4. `shape-step4-bulkhead-door-fix`
5. `shape-step5-floor-and-material-validation`
6. `shape-step6-interior-pollution-fix`
7. `shape-step7-final-pie-validation`

No squash before the phase is accepted.

---

## 5. Required screenshots per step

For review, capture and archive:

- exterior 3/4 view
- exterior side view
- front cap view
- interior centerline view
- interior low-angle floor view
- wireframe interior view

Use the same camera positions before and after Step 6.

---

## 6. Explicit non-goals

Do not do any of the following in this phase:

- GeneratorEditor implementation
- player-facing submarine editor
- save/load work
- flood refactor
- station refactor
- spawn refactor
- airlock logic redesign
- appendages
- naval profile naming
- curve-based authoring
- legacy bake runtime reintroduction

---

## 7. Exit gate for the next global phase

The shape phase is complete only when:

- the authoritaire document Sections 3 to 6 are satisfied
- the execution steps above all pass
- the FPRun submarine reads as a coherent submarine visually
- the new runtime pipeline remains the only runtime source of truth

Only after that:
- a dedicated `GeneratorEditor` implementation plan may start

