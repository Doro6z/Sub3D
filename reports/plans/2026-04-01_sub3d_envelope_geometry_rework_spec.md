# Sub3D - Envelope Geometry Rework Spec

Date: 2026-04-01
Status: Consolidated execution spec
Canon:
- `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
- `reports/plans/2026-04-01_sub3d_hull_breach_full_playable_implementation_plan.md`
- `reports/plans/2026-03-31_sub3d_submarine_stabilization_architecture_audit.md`
- `reports/plans/2026-03-31_sub3d_gameplay_vision_questions.md`

---

## 1. Position In The Global Plan


Current global situation:
- movement, inertia, helm stabilization, and hull collision damage baseline have already been pushed forward enough to support the first playable loop
- logical hull damage, breach, flooding, repair, and pump-out are already present in runtime form
- visual breach opening, visible patching, and full crisis closure for the player are not complete yet
- the current blocker before finishing that hull gameplay track is the submarine envelope and compiler output

What this document does not cover:
- final player-facing repair UI
- final material setup in editor
- full hull visual rupture implementation
- full repair tool gameplay

---

## 2. Decision Record

### 2.1 Envelope And Compiler Before Visible Rupture

Decision:
- finish the envelope and compiler corrections before continuing `H4` and `H5`

Reason:
- the visual breach system needs a stable relation between structural sheets, exterior geometry, interior geometry, and collision
- without that relation, a visible hole or patch is attached to moving technical debt instead of a stable runtime contract

### 2.2 Keep A Straight X Spine For First Playable

Decision:
- keep the submarine body organized around a straight longitudinal X axis for this phase

Reason:
- the current pipeline already assumes a straight spine in the layout solver, geometry builder, and compiled actor setup
- forcing a curved centerline now would reopen too much of the compiler and movement pipeline

### 2.3 Use Deterministic Curve-Driven Shape Control, Not A Free 3D Hull Spline

Decision:
- for this phase, shape variation must stay curve-driven and deterministic
- spline-like authoring is acceptable only as an editor input method for longitudinal curves, not as a free spatial hull source of truth

Reason:
- current envelope parameters already support the first meaningful shape controls:
  - `RadiusProfile`
  - `FloorDropBiasCm`
  - `SectionExponent`
  - `WidthToHeightRatio`
  - `BowProfile`
  - `SternProfile`
  - `BowTaperFraction`
  - `SternTaperFraction`
- these are enough to support several submarine families without breaking the compiler contract

### 2.4 First Playable Scope Defense

Decision:
- do not turn this track into a universal submarine authoring tool

Reason:
- the first playable needs one credible, readable, compilable submarine pipeline
- it does not need arbitrary freeform hull authoring

### 2.5 Solver And Geometry Must Share The Same Envelope Truth

Decision:
- the layout solver is now explicitly in scope for this track

Reason:
- the current solver derives floor width from a circle in `Source/Sub3D/SubCompiler/SubmarineLayoutSolver.cpp`
- the geometry builder derives interior shape from a superellipse in `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- this mismatch makes floor width, headroom, and usable interior volume inconsistent

This is now the first blocker in the execution order.

---

## 3. Current Technical Diagnosis

### 3.1 Solver And Geometry Do Not Use The Same Cross-Section Truth

Current code:
- `USubmarineEnvelopeDef` already exposes section shape controls in `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h`
- `USubmarineGeometryBuilder::GenerateCompartmentInteriorMeshData()` uses a superellipse in `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `FSubmarineLayoutSolver::ApplyEnvelopeToCompartments()` still computes floor half-width with a circular formula in `Source/Sub3D/SubCompiler/SubmarineLayoutSolver.cpp`

Impact:
- a floor can be declared valid by the solver but not match the generated wall shape
- headroom and walkable width are not derived from the same geometry that the player sees
- future floor authoring and shape variations will become unreliable if this is not fixed first

Root cause:
- one part of the pipeline uses `sqrt(r^2 - z^2)`
- another part uses the superellipse exponent and width-height ratio

### 3.2 Double Hull Visibility And Exterior Offset Are Not Stable Enough

Current code:
- the exterior offset is still effectively controlled from geometry generation, not from the envelope contract

Impact:
- the player can perceive the exterior hull and interior visual wall as two separate shells
- the offset is not treated as a real authoring parameter

### 3.3 The Exterior Collision Proxy Is Still Too Coarse

Current code:
- `ExteriorCollisionProxy` is still a box around a curved hull in `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`

Impact:
- the submarine feels like a box during sweeps
- tunnels and wall glances are less credible than the rendered hull

### 3.4 Bulkheads Are Rectangular While The Hull Is Curved

Current code:
- bulkheads and some interior closing geometry are still created from `AppendBoxPrism()` in `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Impact:
- geometry protrudes outside the hull silhouette
- the compiled result looks wrong from the outside and wrong in section views

### 3.5 Bow And Stern Caps Still Produce Hard Lighting Artifacts

Current code:
- exterior caps are still simple triangle fans converging to a center point

Impact:
- hard lighting transitions remain visible on the nose and stern

### 3.6 The Interior Floor Still Carries Unnecessary Back Faces

Current code:
- floor triangles are still emitted as a double-sided slab in the interior builder path

Impact:
- unnecessary geometry
- internal z-fight risk

### 3.7 Tangents And Lighting Quality Are Still Underdefined

Current code:
- mesh sections rely on weak or absent tangent data

Impact:
- lighting quality is unstable and depends too much on fallback generation

### 3.8 Sheet-To-Mesh Ownership Is Still Missing

Current code:
- structural sheets exist
- visual rupture support fields exist
- but the compiled geometry still does not provide enough ownership data to tell which mesh range belongs to which structural sheet

Impact:
- visible hull rupture cannot be attached robustly to the compiled exterior and interior walls

### 3.9 Bow And Stern Caps Are Not Structural Sheets

Impact:
- an impact on a cap is not represented as a structural sheet breach

Decision for first playable:
- keep this as a known limitation
- do not expand the structural model here unless it becomes a blocker during tuning

### 3.10 Bulkhead Collision Profile Still Needs To Match Gameplay

Impact:
- a closed bulkhead must block crew movement
- if collision profiles stay visual-only, the compiled level lies to traversal

---

## 4. Execution Plan - Blockers For First Playable

This is the required order for the current phase.

### B0 - Unify Envelope Truth Across Solver And Geometry

Goal:
- ensure that the layout solver and the geometry builder derive walkable width and interior wall shape from the same envelope definition

Required result:
- floor width, headroom, and interior silhouette are all based on the same section exponent and width-height ratio

Implementation direction:
- add a shared helper for cross-section width evaluation at a given floor offset
- use that shared truth in both the solver and geometry builder

Why it is first:
- every later feature depends on usable width being truthful
- this is the base for floor authoring, multi-floor work, and breach visual ownership

### B1 - Immediate Geometry And Collision Corrections

Goal:
- remove the most obvious credibility breaks with the smallest safe changes

Includes:
- one-sided interior floor
- envelope-owned exterior hull offset
- closed bulkheads using the correct collision profile
- capsule collision proxy instead of box proxy

Why now:
- these are high value fixes with low structural risk

### B2 - Fix Geometry That Breaks The Hull Silhouette

Goal:
- ensure that generated interior closures do not protrude outside the hull

Includes:
- superellipse-conforming bulkheads
- superellipse-conforming interior caps or their removal if redundant
- progressive exterior bow and stern caps

Why now:
- the exterior and section views must be credible before holes, patches, or visible damage are attached to them

### B3 - Expose Minimum Geometry Resolution Controls

Goal:
- move critical geometry quality knobs out of hardcoded builder constants

Includes:
- interior arc segment count
- exterior offset if not already completed in `B1`

Why now:
- first playable tuning must happen from authoring data, not builder internals

### B4 - Add Sheet-To-Mesh Ownership Data

Goal:
- make each structural sheet addressable in compiled geometry

Includes:
- exterior vertex and triangle ownership ranges
- interior wall ownership ranges where available

Why now:
- this is the bridge into `H4 Hull Visual Damage Component`

### B5 - Stabilize Lighting Quality

Goal:
- ensure compiled geometry has predictable shading

Includes:
- explicit tangents where needed
- improved UV continuity only if still required after tangent work

Why now:
- this is required before editor handoff of the compiled hull path

### Exit Condition For The Blocking Phase

This phase is complete when all of the following are true:
- the solver and geometry builder agree on usable interior width
- the submarine no longer reads as a box in sweep collision
- no bulkhead or cap geometry protrudes outside the intended hull silhouette
- the interior floor is clean and single-sided
- the exterior and interior geometry no longer reveal an obvious double shell during normal play views
- each structural sheet has enough ownership data for later visible breach work

At that point, the project can resume the hull plan at:
- `H4 Hull Visual Damage Component`
- `H5 Repair Tool Gameplay`
- then the rest of the first playable breach closure path

---

## 5. Detailed Implementation Specs

### SPEC-E0 - Shared Cross-Section Truth For Solver And Geometry

Files:
- `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h`
- `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.cpp`
- `Source/Sub3D/SubCompiler/SubmarineLayoutSolver.cpp`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Add a shared evaluation path for cross-section half-width at a given normalized vertical position or floor offset.
2. That shared path must honor:
   - `SectionExponent`
   - `WidthToHeightRatio`
   - the current evaluated hull radius at longitudinal position
3. Replace the circular floor-width computation in the solver with that shared evaluation.
4. Keep the builder on the same truth instead of duplicating another independent formula if possible.

Validation:
- a compartment that passes floor fit in the solver produces interior geometry with the same usable floor width in the builder

### SPEC-E1 - Fix Floor Double-Sided Geometry

File:
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Remove the unnecessary back-face floor triangles.
2. Update any validation count that still assumes a double-sided floor index count.

Validation:
- the floor is visible from above
- the floor does not z-fight with itself

### SPEC-E2 - Make Exterior Hull Offset A Real Envelope Parameter

Files:
- `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`

Action:
1. Add an envelope-authored exterior hull offset property.
2. Remove the hardcoded builder-only constant.
3. Ensure preview and compiled actor paths read the same value.

Validation:
- exterior offset can be changed from envelope data
- normal views do not reveal obvious double hull overlap

### SPEC-E3 - Replace Exterior Collision Box With Capsule

Files:
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.h`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`

Action:
1. Replace the exterior collision proxy box with a capsule.
2. Align the capsule to the submarine longitudinal axis.
3. Update spawn validation and debug logging to use capsule data.
4. Reduce padding to a value appropriate for a capsule approximation.

Validation:
- the submarine no longer catches corners like a box
- spawn validation uses the same collision proxy shape

### SPEC-E4 - Generate Bulkheads That Follow The Hull Section

Files:
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Replace rectangular bulkhead generation with a section-conforming shape based on the same envelope section truth.
2. Apply the same rule to interior closing caps if they remain part of the mesh set.
3. Keep door cutouts compatible with the new profile.

Validation:
- no generated bulkhead protrudes outside the hull silhouette

### SPEC-E5 - Use Progressive Exterior Caps

File:
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Replace simple bow and stern fan caps with a small progressive cap construction.
2. Smooth the normal transition from ring normals to axial center normals.

Validation:
- bow and stern do not show a harsh lighting spike at the center

### SPEC-E6 - Provide Tangents For Compiled Mesh Sections

File:
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Provide explicit tangents where mesh sections currently rely on empty tangent arrays or weak fallback generation.
2. Only use UV adjustments as a secondary correction if tangent data is still not sufficient.

Validation:
- shading remains coherent under directional lighting across the full submarine body

### SPEC-E7 - Expose Interior Arc Resolution

Files:
- `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Move interior arc resolution out of a hardcoded constant into envelope-controlled data.
2. Keep clamped limits suitable for first playable compilation cost.

Validation:
- interior wall smoothness can be tuned without editing builder code

### SPEC-E8 - Add Sheet-To-Mesh Ownership Data

Files:
- `Source/Sub3D/Submarine/StructuralHullTypes.h`
- `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.cpp`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. Extend structural sheet data with compiled geometry ownership fields.
2. Record exterior ownership ranges for the geometry produced for each compartment side.
3. Record interior wall ownership ranges where the mapping is available and stable.
4. Populate those fields during compile output.

Validation:
- each structural sheet can identify the compiled geometry region it owns

### SPEC-E9 - Maintain Exterior And Interior Hole Coherence Contract

Scope note:
- this spec defines the contract needed by the hull visual damage track
- it is not implemented entirely in this document's execution set

Contract:
1. a later hull visual damage component must be able to apply the same breach mask to the exterior hull and corresponding interior wall
2. the geometry output of this track must provide enough ownership data to support that

Validation:
- later `H4` work can address one breach location without inventing a new ownership system

### SPEC-E10 - Use The Correct Collision Profile For Closed Bulkheads

File:
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`

Action:
1. closed or solid bulkhead geometry must use the collision profile that blocks crew traversal
2. purely visual geometry may keep a visual-only profile

Validation:
- the crew cannot walk through a closed bulkhead

---

## 6. Ordered Delivery Waves

### Wave 1 - First Playable Blockers

Required contents:
- `SPEC-E0`
- `SPEC-E1`
- `SPEC-E2`
- `SPEC-E3`
- `SPEC-E10`

Result:
- solver truth is coherent
- collision is already more credible
- the most visible geometry lies are removed

### Wave 2 - Hull Silhouette Integrity

Required contents:
- `SPEC-E4`
- `SPEC-E5`
- interior cap correction if still needed

Result:
- the compiled hull reads as one clean form

### Wave 3 - Authoring And Lighting Readiness

Required contents:
- `SPEC-E7`
- `SPEC-E6`

Result:
- geometry quality is tunable
- lighting is stable enough for editor handoff

### Wave 4 - Breach Visual Prerequisite Data

Required contents:
- `SPEC-E8`
- confirm `SPEC-E9` contract is satisfiable

Result:
- the project can return to the hull plan and resume visible rupture work

---

## 7. Post-Stabilization Shape Flexibility

This section is intentionally post-blocker. It must not delay the first playable.

### P1 - Improve Longitudinal Shape Control

Goal:
- support more hull families through stronger curve-driven control of radius and taper distribution

Allowed direction:
- richer curve authoring for longitudinal radius behavior
- richer bow and stern taper behavior

Not allowed in this phase:
- arbitrary 3D freeform hull editing

### P2 - Improve Cross-Section Family Control

Goal:
- support more credible cross-section families without breaking the deterministic compiler

Allowed direction:
- better control of width-height behavior
- controlled changes of section family along the hull

### P3 - Improve Floor Authoring Flexibility

Goal:
- support single-floor and multi-floor authoring with the same geometric truth

Required rule:
- every floor rule must still derive from the same cross-section truth used by solver and geometry

### P4 - Add Preset Families

Goal:
- expose repeatable hull families such as cigar, teardrop, flatter naval forms, and wider utility forms

Required rule:
- presets are parameter sets over the same deterministic envelope contract

### P5 - Improve Editor Validation

Goal:
- make it easier to validate headroom, floor fit, crew clearance, and silhouette quality before runtime

### P6 - Reevaluate A Free Spatial Spine Later

Goal:
- only after the current pipeline is stable, decide whether a truly curved submarine centerline is worth the additional compiler and movement cost

Current decision:
- not part of first playable

---

## 8. Explicit No-Go Rules

1. Do not turn this phase into a full freeform submarine editor.
2. Do not start visible breach rendering work before `SPEC-E8` exists.
3. Do not keep the solver outside scope. The solver mismatch is now a known blocker.
4. Do not collapse visual geometry, walkable geometry, structural truth, and movement collision into one mesh just to move faster.
5. Do not rely on editor-only material tricks to hide geometry problems that should be fixed in compiled output.

---

## 9. Done Criteria For This Document

This document is complete in implementation terms when:
- solver and geometry share the same envelope truth for interior width
- the compiled submarine no longer exposes obvious double hull overlap in normal views
- collision sweep no longer behaves like a box around the submarine
- bulkheads and closing geometry follow the intended hull silhouette
- bow and stern cap shading is acceptable under normal lighting
- interior floor geometry is clean
- critical geometry quality parameters are authorable
- structural sheets can identify their compiled geometry ownership ranges

At that point:
- this envelope/compiler track is no longer the current blocker
- the project returns to the hull plan at visible rupture and repair completion

---

## 10. Summary

North Star for this phase:
- one deterministic compiled submarine
- one coherent envelope truth from solver to geometry
- one addressable hull surface for later visible breach work

First playable meaning:
- do the minimum complete work that makes the submarine credible, compilable, traversable, and ready for the remaining breach gameplay packets
