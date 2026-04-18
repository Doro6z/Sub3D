# Sub3D - Master Plan vers un First Playable

Date: 2026-04-10
Source research: `C:\Users\coren\Downloads\deep-research-report submarine.md`
Status: Strategic master plan

This document starts a new global plan.
It does not cancel the current shape execution work.
It defines the next project-level order after the runtime spine has been built.

Priority order:
1. Existing authoritaire shape plan remains active
2. This master plan defines the next global sequencing
3. Any future implementation plan must align with both

Related documents:
- `C:\Dev\Sub3D\reports\plans\2026-04-08_submarine_generator_architecture_unified.md`
- `C:\Dev\Sub3D\reports\plans\2026-04-10_submarine_generator_shape_execution_plan.md`
- `C:\Dev\Sub3D\reports\plans\2026-04-10_firstplayablerun_editor_session_contract.md`

---

## 0. Project state at the start of this plan

The runtime spine is already in place.

Confirmed base:
- `USubmarineGeneratorSpec`
- `USubmarineGenerator`
- `USubmarineDefinition`
- `USubmarineMeshBuilder`
- `USubmarineGeneratedGeometryComponent`
- `SubFlood` as the runtime flood path
- `SubHull` as damage-only
- FirstPlayableRun editor setup started

What is still not closed:
- exterior shape quality
- readable interior geometry
- clean render/collision separation in practice
- playable FP validation in PIE without `[LEGACY]`
- minimal editor tooling for submarine authoring

This plan assumes:
- the runtime architecture is no longer the main problem
- the main problem is now closing a playable submarine creation loop

---

## 1. Core project decision

The project focus shifts from migration to creation.

This means:
- no new runtime architecture phase
- no broad legacy cleanup before the FP is pass
- no advanced editor mode before the FP is pass
- no backend mesh lock-in inside gameplay logic

The main target is now:

`Spec -> Generator -> Definition -> Runtime Geometry -> Gameplay Runtime -> First Playable validation`

The next global success condition is not "more systems exist".
It is:
- one playable generated submarine
- in one dedicated FP level
- validated in PIE
- with no normal-path legacy fallback

---

## 2. Non-negotiable principles

### 2.1 Definition stays the truth

`USubmarineDefinition` is the only canonical runtime truth for:
- topology
- compartments
- connections
- flood graph
- stations
- spawns
- generated mesh data

Gameplay systems must read the definition or data derived from it.
They must not depend on the render backend.

### 2.2 Render and collision are separate by design

The project must keep a hard distinction between:
- render geometry
- gameplay collision

The FP target is:
- exterior collision by convex hulls / slices
- interior walkable collision by simple dedicated collision volumes
- no dependence on triangle-level collision as the main gameplay support

### 2.3 Shape before tooling

No advanced editor tooling starts before the generated submarine is:
- visible
- walkable
- collidable
- readable
- usable in PIE

### 2.4 Airlock is solved topologically first

For FP:
- airlock is an explicit compartment
- inner door is explicit
- outer hatch is explicit
- flood topology is explicit

Visual blend is secondary.

### 2.5 No runtime regression to legacy

Legacy code may stay present temporarily.
It must not become the runtime source of truth again.

---

## 3. Binary definition of FP pass

The FP is pass only if all checks below are true.

### 3.1 Scene

- one dedicated FP level
- one intended submarine FP actor
- one valid `GeneratorSpec`
- no hidden dependency on a proto level

### 3.2 Geometry

- generated geometry appears in PIE
- exterior hull is visible from outside
- floor is readable and walkable
- bulkheads are readable
- airlock exists as usable gameplay space

### 3.3 Collision

- exterior collision is stable for movement and impacts
- interior collision is stable for walking and traces
- no visible technical collision geometry in normal gameplay view

### 3.4 Flood gameplay

- breach raises water as expected
- doors and hatches modulate transfers as expected
- airlock behaves as an explicit flood topology element

### 3.5 Runtime hygiene

- stations are present and plausibly usable
- no `[LEGACY]` warning appears on the normal FP submarine path

If any of these fail, the FP is not pass.

---

## 4. Global workstreams

This master plan is split into 5 workstreams.

### Workstream A - Shape and playability closure

Purpose:
- make the generated submarine visually and physically usable

Scope:
- hull
- caps
- floors
- bulkheads
- doors
- render/collision separation
- interior readability

Current status:
- active now
- already delegated through:
  - `2026-04-08_submarine_generator_architecture_unified.md`
  - `2026-04-10_submarine_generator_shape_execution_plan.md`

Gate out:
- the generated submarine is playable enough to validate the runtime loop

### Workstream B - FirstPlayableRun validation

Purpose:
- prove that the generated path is the normal path in practice

Scope:
- dedicated FP level
- FP submarine actor
- generator asset wiring
- PIE validation
- log validation

Gate out:
- FP pass definition satisfied

### Workstream C - Minimal internal authoring tool

Purpose:
- accelerate iteration without building a full editor mode

Scope:
- Editor Utility Widget
- Details view
- rebuild button
- preview actor
- cheap handles only

Explicitly allowed:
- Details + EUW + component visualizer or standard gizmo

Explicitly blocked:
- full editor mode
- Scriptable Tools based mode
- complex custom viewport framework

Gate out:
- faster iteration than raw asset editing
- no explosion in tool complexity

### Workstream D - Backend mesh decoupling

Purpose:
- keep the gameplay truth independent from the mesh backend

Scope:
- preserve `USubmarineDefinition` as backend-agnostic truth
- keep geometry materialization replaceable
- allow future backend change without gameplay rewrite

FP position:
- this is a constraint, not a rewrite project
- do not switch backend unless the current backend blocks FP closure

### Workstream E - Post-FP tooling and quality expansion

Purpose:
- only after FP pass

Possible topics:
- advanced GeneratorEditor
- preview/final bake split
- Dynamic Mesh preview backend
- richer control rings
- improved visual airlock blending
- optional static mesh bake pipeline

This workstream is explicitly blocked until FP pass.

---

## 5. Recommended technical direction

### 5.1 Geometry backend stance

The gameplay must not depend on the current runtime mesh backend.

Current practical rule:
- keep `USubmarineDefinition` backend-independent
- keep `USubmarineGeneratedGeometryComponent` as a replaceable materialization layer

For FP:
- use the current backend if it can close the FP quickly
- do not begin a backend rewrite preemptively

For later:
- preview/editor workflows may use a different mesh backend than the packaged runtime, as long as the definition remains canonical

### 5.2 Collision stance

FP collision target:
- exterior hull: convex hulls / slices
- floors: dedicated simple walkable collision
- bulkheads: simple blocking collision
- airlock: simple collision volumes

Do not chase:
- perfect triangle-level collision
- visual precision beyond what FP requires

### 5.3 Airlock stance

FP airlock solution:
- explicit compartment
- explicit connections
- explicit collision
- acceptable visual blend

Do not require:
- runtime boolean union between hull and airlock

Recommended visual compromise:
- simple airlock volume
- local collar / trim geometry at the junction
- dedicated interior opening logic

### 5.4 Tooling stance

The first internal tool must be minimal.

Required minimum:
- preview submarine actor
- Details-driven editing
- rebuild preview button
- filtered validation output

Optional cheap interaction:
- standard transform gizmo
- component visualizer

Blocked:
- full editor mode before FP pass

---

## 6. Sequencing

This is the required global order.

### Phase 1 - Finish Workstream A

Close the shape and playability blockers first.

This phase is already in progress.
It must be finished before any editor tooling expansion.

### Phase 2 - Close Workstream B

Run the dedicated FirstPlayableRun validation loop.

Required result:
- the FP submarine works through the generated path in PIE
- no `[LEGACY]` warning on the normal path

### Phase 3 - Start Workstream C

Only after FP is playable enough in PIE.

Build the smallest useful authoring surface:
- Details
- rebuild
- preview
- cheap handles

### Phase 4 - Re-evaluate backend needs

Only after:
- shape is stable
- FP pass is proven
- minimal authoring loop is usable

Decision:
- keep current geometry materialization backend
- or add an alternate preview path

### Phase 5 - Post-FP expansion

Only then:
- advanced GeneratorEditor
- dynamic preview improvements
- better visual blend systems
- wider content authoring

---

## 7. Immediate implications for current work

### 7.1 What continues now

`Opus` continues the shape execution plan.

That work is not replaced by this master plan.
It is Workstream A of this master plan.

### 7.2 What must not start now

Do not start:
- full GeneratorEditor implementation
- full editor mode
- backend rewrite
- broad cleanup of legacy systems
- generalized airlock boolean solution

### 7.3 What can start in parallel

Planning work may start now for:
- FP pass criteria tracking
- minimal EUW structure
- preview actor contract
- asset validation rules

But implementation must wait until Workstream A is sufficiently closed.

---

## 8. Red flags

If any of the following happens, the project is drifting.

1. Editor Mode work starts before FP pass.
2. New shape parameters are added before floors, doors, caps, and interior readability are stable.
3. Gameplay code starts reading rendered geometry instead of reading canonical definition data.
4. The FP appears to work only because a legacy fallback is still active.
5. A runtime boolean union becomes required just to make the airlock acceptable.
6. Collision becomes triangle-dependent for normal gameplay instead of using simple dedicated gameplay collision.

Any of these is a stop-and-realign signal.

---

## 9. Definition of done for this master plan stage

This master plan reaches its first major milestone when:

- Workstream A is closed
- Workstream B is closed
- the FP is pass according to Section 3

At that point:
- the project has one generated submarine that is visually usable, physically usable, and gameplay-valid
- the generated path is proven in PIE
- tooling expansion can begin from a stable base

Until that point:
- shape and FP validation remain the only priority

