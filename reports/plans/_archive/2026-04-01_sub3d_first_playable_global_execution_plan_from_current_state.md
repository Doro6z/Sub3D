# Sub3D - First Playable Global Execution Plan From Current State

Date: 2026-04-01
Status: Updated execution order from the current repository state
Scope: Shortest correct path from the current runtime baseline to a playable submarine run

---

## 1. Purpose

This document updates the global execution order from the current repository state.

It does not replace the first playable run spec.
It translates the current state of the codebase into the next correct order of implementation.

This document exists because the project is no longer at the same point as the original planning baseline:
- hull movement and stabilization are already advanced enough to support gameplay tuning
- hull damage, breach, flood, repair, and pump-out logic already exist in runtime form
- envelope and compiler Waves 1-3 have now been executed far enough that they are no longer the main blocker for Proto03

---

## 2. Canon

Priority order:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-03-29_sub3d_product_north_star_implementation_plan.md`
3. `reports/plans/2026-04-01_sub3d_hull_breach_full_playable_implementation_plan.md`
4. `reports/plans/2026-04-01_sub3d_envelope_geometry_rework_spec.md`
5. this document

If conflict exists:
- the first playable run spec wins on product scope and run flow
- the North Star plan wins on scope defense
- the hull breach plan wins on the complete hull gameplay target
- the envelope rework spec wins on the geometry/compiler sequence
- this document only updates execution order from the current code state

---

## 3. Current Situation In The Global Plan

The project is now in this state:

- the submarine movement baseline is credible enough to continue gameplay closure
- the helm command and stabilization baseline exists
- the structural hull gameplay loop already exists in logical runtime form
- the compiled submarine geometry is much closer to a usable first playable baseline
- the remaining blocker before a fully visible and finishable breach crisis is no longer the broad envelope/compiler track

What changed materially:

- the envelope/compiler rework is no longer a full blocker for Proto03 on shape and collision basics
- the remaining envelope/compiler items are now split into:
  - `E6` explicit tangents: visual polish, not currently blocking Proto03
  - `E8/E9` sheet-to-mesh ownership and double-coat hole coherence contract: direct prerequisite for the next robust hull gameplay packet

This means the project should now return to the hull gameplay track rather than continue broad envelope work.

---

## 4. Current Repository State Summary

The current state should be treated as:

### 4.1 Already advanced enough to build on

- submarine movement and inertia
- rudder behavior moved away from direct arcade yaw
- helm stabilization and dampener runtime
- collision-driven hull damage gating
- breach clustering and flooding logic
- runtime repair entry point
- pump-out closure
- structural sheet visual rupture foundation

### 4.2 Envelope/compiler baseline now sufficiently stabilized for Proto03

The following envelope rework items are treated as completed for execution planning:

- `E0` shared solver/builder envelope truth
- `E1` single-sided floor
- `E2` envelope-owned exterior hull offset
- `E3` capsule collision proxy
- `E4` superellipse bulkheads
- `E5` progressive exterior caps
- `E7` exposed interior arc resolution
- `E10` sealed bulkhead traversal blocking

These items were reported as executed and build-verified before this handoff.
This turn did not rerun the full editor build, but spot checks in the repository match the reported state for the main code paths.

### 4.3 Explicitly deferred for now

- `E6` explicit tangents
- `E8` sheet-to-mesh ownership data
- `E9` exterior/interior hole coherence contract implementation support

Assessment:

- `E6` is deferred polish unless shading becomes a readability blocker in PIE
- `E8/E9` are not optional if the project wants to resume `H4 Hull Visual Damage Component` correctly

---

## 5. Global Packet Status

This section is the practical map from the current code state to the first playable packets.

### 5.1 FP-1 Run Shell

Treat as still open until explicitly re-verified.

Needs:
- authoritative run state machine
- explicit crisis state transitions
- proper first playable flow ownership in `GameMode` and `GameState`

### 5.2 FP-2 First Playable Level Shell

Treat as still open until explicitly re-verified.

Needs:
- level topology contract in editor
- breach trigger placement
- route start/end closure
- playable traversal validation

### 5.3 FP-3 Helm + Sonar V1

Runtime foundations are advanced.
Player-facing closure should still be treated as open until the final station path is validated in PIE.

Needs:
- final panel hookup
- stable gameplay read path in the first playable map
- manual validation under navigation pressure

### 5.4 FP-4 Breach Crisis Closure

This is the current main gameplay track.

Logical runtime baseline exists.
Full first playable closure is still open because the player-facing crisis is not fully visible, not yet fully tied to run objective closure, and not yet fully finished through catastrophe/failure handling.

### 5.5 FP-4A Death Baseline

Treat as open until explicitly re-verified.

### 5.6 FP-5 Docking V1

Treat as open until explicitly re-verified.

### 5.7 FP-6 Solo Closure

Open.
This remains the main product target after the breach track is completed enough to support a full run.

### 5.8 FP-7 Coop Closure

Keep after solo closure.
Do not let it reopen scope before the solo loop is actually complete.

---

## 6. Updated Execution Order From Current State

This is the recommended order now.

### Phase 1 - Finish The Remaining Hull Geometry Prerequisite

Goal:
- make the compiled hull addressable enough for visible breach work

Execute:
1. `E8` sheet-to-mesh ownership data
2. confirm `E9` contract is satisfied by the data model and compiled output

Notes:
- do not restart broad envelope work
- do not spend this phase on `E6` unless current shading blocks gameplay readability

### Phase 2 - Resume The Robust Hull Gameplay Track

Goal:
- turn the existing logical hull crisis into a visible, repairable, finishable first playable event

Execute in this order:
1. `H2` run objective integration
2. `H3` structural sheet visual authoring top-up only where needed
3. `H4` hull visual damage component
4. `H5` repair tool gameplay
5. `H6` pressure and containment closure
6. `H7` readability and alarms
7. `H8` catastrophe and failure closure
8. `H9` editor authoring and content contract
9. `H10` first playable balance pass

Important rule:
- `H3` should not be restarted as a fresh design track
- use the current structural sheet visual groundwork and only complete what is missing for `H4`

### Phase 3 - Close The First Playable Runtime Shell Around The Crisis

Goal:
- make the full run structurally playable, not only the breach loop in isolation

Execute:
1. close `FP-1` run shell if still incomplete
2. close `FP-2` first playable level shell
3. close `FP-3` helm + sonar V1 player path
4. close `FP-5` docking V1
5. close `FP-4A` death baseline

Reason for this order:
- the breach crisis must live inside a real run shell
- docking and failure only matter once the breach event is truly part of the run

### Phase 4 - First Full Solo Closure

Goal:
- prove one complete solo run from start to finish

Execute:
1. run full solo validation pass
2. tune timings and readability
3. remove any remaining ambiguity in crisis resolution conditions
4. close `FP-6`

### Phase 5 - Coop Follow-Through

Goal:
- extend the same run proof to the coop baseline without reopening product scope

Execute:
1. authority review
2. replication review on the crisis path
3. station concurrency review
4. close `FP-7`

---

## 7. What Is Not The Current Priority

Do not make these the main track right now:

- a universal freeform hull authoring tool
- tangent polish as a primary milestone
- campaign runtime expansion
- broad coop features before solo closure
- cosmetic UI expansion that is disconnected from first playable closure

---

## 8. Current Main Blocker

The current main blocker is:

- resuming the hull gameplay track at the correct level of dependency

In concrete terms:

- `E8/E9` are the next technical prerequisite
- then the project should move directly into `H2 -> H4 -> H5 -> H6 -> H7 -> H8 -> H9 -> H10`

That is now the shortest correct path back to the first playable crisis closure.

---

## 9. Done Definition For This Updated Plan

This updated execution plan is considered fulfilled when:

- the remaining hull geometry prerequisite is closed
- the breach crisis is visible, readable, repairable, and objective-bound
- the player can succeed or fail a complete first playable run
- the solo path is proven before coop expansion becomes a blocker

---

## 10. Summary

The project is past the stage where envelope/compiler cleanup is the main problem.

The correct next move is:
- finish `E8/E9`
- return immediately to the robust hull gameplay packets
- then close the first playable run shell around that crisis

That is the current shortest correct path to a playable submarine run.
