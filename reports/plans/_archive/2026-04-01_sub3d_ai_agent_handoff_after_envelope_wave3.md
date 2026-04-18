# Sub3D - AI Agent Handoff After Envelope Wave 3

Date: 2026-04-01
Owner: Runtime / planning handoff
Status: Current operational handoff after envelope/compiler Waves 1-3
Scope: What the next agent should assume, what must not be redone, and what should happen next

---

## 1. Purpose

This is the current handoff for the next AI agent working on the first playable submarine path.

It is not a broad project summary.
It is an execution handoff from the current code and planning state.

Use it to avoid:
- reopening already-closed envelope work
- restarting plan fragmentation
- spending time on non-blocking polish while the breach gameplay track is still open

---

## 2. Canon

Use this priority order:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-03-29_sub3d_product_north_star_implementation_plan.md`
3. `reports/plans/2026-04-01_sub3d_hull_breach_full_playable_implementation_plan.md`
4. `reports/plans/2026-04-01_sub3d_envelope_geometry_rework_spec.md`
5. `reports/plans/2026-04-01_sub3d_first_playable_global_execution_plan_from_current_state.md`
6. this handoff

If conflict exists:
- the run spec wins on playable product scope
- the North Star plan wins on scope defense
- the hull breach plan wins on the robust hull gameplay target
- the updated global execution plan wins on current order
- this handoff only tells you how to continue from the present state

---

## 3. Current State You Should Assume

### 3.1 Runtime baseline already exists

Assume these are already in place and should be built on, not rewritten:

- submarine movement baseline
- helm stabilization baseline
- collision-gated hull damage
- breach clustering
- flooding and equalization
- repair entry path
- pump-out closure
- current structural hull runtime truth

### 3.2 Envelope/compiler Waves 1-3 are considered done enough for Proto03

Treat these as already completed:

- `E0`
- `E1`
- `E2`
- `E3`
- `E4`
- `E5`
- `E7`
- `E10`

Repository spot checks match the reported state on key items:

- `ExteriorHullOffsetCm` is exposed on the envelope
- `InteriorArcSegments` is exposed and threaded
- the compiler actor now uses `UCapsuleComponent` for the movement proxy
- the floor triangle validation now expects 6 indices
- sealed bulkheads use `SubInteriorWalkable`
- superellipse code paths are present in the geometry builder

Build status:

- the latest execution report states that three builds passed without errors
- this handoff turn did not rerun those builds

### 3.3 Remaining envelope/compiler items

Still open:

- `E6` explicit tangents
- `E8` sheet-to-mesh ownership data
- `E9` exterior/interior breach coherence contract support

Classification:

- `E6` is non-blocking polish for Proto03 unless shading becomes a gameplay readability blocker
- `E8/E9` are the actual prerequisite for resuming visible hull rupture correctly

---

## 4. What Must Not Be Redone

Do not spend time restarting or redesigning:

- the broad envelope geometry rework from zero
- the movement and rudder model from zero
- the hull damage baseline from zero
- the stabilization system from zero
- a universal hull authoring tool
- a free 3D spline hull pipeline

Do not treat these as current blockers unless new evidence appears:

- tangent polish by itself
- campaign runtime expansion
- coop-first support
- broad UI redesign outside the packet you are actively closing

---

## 5. Immediate Next Task

The immediate next task is:

1. implement `E8`
2. confirm the repository now satisfies `E9`
3. then resume the hull gameplay packets at `H2`

This is the shortest correct next move.

Reason:

- the project already has logical breach runtime
- the project already has stabilized enough compiled hull geometry
- the missing bridge is ownership of compiled mesh regions by structural sheets

---

## 6. Ordered Next Execution

Use this order unless a direct blocker is discovered.

### Step 1 - `E8` Sheet-To-Mesh Ownership

Target files:

- `Source/Sub3D/Submarine/StructuralHullTypes.h`
- `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.cpp`
- `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.cpp`

Goal:

- each structural sheet must know which compiled geometry range it owns

Minimum acceptable result:

- stable ownership data for exterior geometry
- interior ownership when the mapping is stable enough to support later breach masking

### Step 2 - Confirm `E9` Contract

Target files:

- likely the same files as `E8`
- then the current visual damage runtime files:
  - `Source/Sub3D/Submarine/SubHullVisualDamageComponent.h`
  - `Source/Sub3D/Submarine/SubHullVisualDamageComponent.cpp`

Goal:

- prove that one breach can later drive both the exterior visual hole and the corresponding interior wall opening without inventing another ownership system

You do not need to finish all final visuals here.
You do need to leave a contract that `H4` can use directly.

### Step 3 - `H2` Run Objective Integration

Goal:

- make the hull crisis a real first playable objective rather than a local technical event

Likely files:

- `Source/Sub3D/GameModes/SubGameMode.*`
- `Source/Sub3D/GameModes/SubGameState.*`
- hull gameplay files that expose designated breach completion state

### Step 4 - `H4` Hull Visual Damage Component

Goal:

- turn a logical breach into a visible exterior and interior damage state

Existing foundations already present:

- structural sheet visual rupture fields
- current `USubHullVisualDamageComponent` base
- breach cluster runtime state

Do not restart this packet from nothing.
Continue from the current groundwork.

### Step 5 - `H5` Repair Tool Gameplay

Goal:

- the player must close the designated breach through a real repair action and see the patched state

### Step 6 - `H6/H7/H8`

Goal:

- close pressure and containment logic
- expose readability and alarms
- add explicit catastrophe and failure closure

### Step 7 - `H9/H10`

Goal:

- make the content path authorable in editor
- balance the crisis for the first playable time budget

---

## 7. Scope Rules For The Next Agent

Follow these rules strictly:

1. Stay inside the current packet.
2. Prefer the smallest correct patch that preserves the existing architecture.
3. Inspect the real code path before editing.
4. Do not invent new ownership systems if `E8` can extend the current structural sheet path cleanly.
5. Do not broaden into UI unless the current packet directly requires a runtime hook.
6. Do not let `E6` tangent polish delay `E8/H2/H4`.

---

## 8. Verification Expectations

For the next implementation turn:

- verify compile/build after code changes
- state clearly what was code-verified and what remains only planned
- if runtime behavior is central, add observability instead of guessing
- if a visual packet is only partially completed, state exactly what the player can and cannot see yet

---

## 9. External Reference

The previous detailed walkthrough referenced by the user is:

`C:\Users\coren\.gemini\antigravity\brain\2f841c6c-d07a-405d-9636-e907af76a8c4\walkthrough.md.resolved`

Treat it as supplemental context only.
Repository code and the canon documents remain the source of truth.

---

## 10. Practical Summary For The Next Agent

Assume this:

- the envelope/compiler foundation is good enough to stop being the main track
- `E8/E9` are now the bridge back into the hull gameplay plan
- the real next milestone is not more hull shape work
- the real next milestone is a visible, repairable, first-playable breach crisis

If you need one-line direction:

- finish ownership
- resume hull gameplay closure
- do not reopen broad geometry scope
