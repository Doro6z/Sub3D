# Sub3D - Editor Handoff Hull/Breach Current State

Date: 2026-04-01
Owner: Runtime / gameplay handoff
Status: Current implemented state after hull collision damage A4 hardening and pump-out closure
Scope: What already exists today for `damage -> breach -> flood -> repair -> dewater`

---

## 1. Purpose

This document is the current editor handoff for the hull damage and flooding loop that already exists in the repository.

It covers:

- what is already implemented in C++
- what can be configured in editor now
- what can be validated in PIE now
- what is still missing before the full robust breach gameplay solution

This is not the final handoff for the complete hull gameplay track.
It is the current-state handoff only.

---

## 2. Canon

Keep this priority order:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-03-29_sub3d_product_north_star_implementation_plan.md`
3. `reports/plans/2026-03-31_sub3d_gameplay_vision_questions.md`
4. this document

If conflict exists:

- the first playable run spec wins on scope and run flow
- the gameplay vision answers win on intended submarine feel
- this document only describes what is already implemented and how to use it in editor

---

## 3. What Is Already Implemented

## 3.1 Structural hull truth

`USubHullComponent` is already the structural truth for hull damage and flooding.

It already owns:

- structural sheets
- per-cell damage state
- breach cluster rebuild
- breach flow fields
- compartment flood state
- world/local sampling helpers
- repair APIs
- pump APIs

Current source files:

- `Source/Sub3D/Submarine/SubHullComponent.h`
- `Source/Sub3D/Submarine/SubHullComponent.cpp`
- `Source/Sub3D/Submarine/StructuralHullTypes.h`

## 3.2 Collision damage A4 behavior

`ASubmarineBase::OnHullHit()` now applies collision damage using speed-dependent gating.

Current behavior:

- low-speed scraping below threshold does not damage the hull
- high-speed approach applies damage
- the damage ramp is curved between minimum speed and catastrophic speed
- the old impulse-based scale remains as a fallback
- self or attached-actor hits are ignored

Current tuning properties on the submarine actor:

- `HullImpactDamageScale`
- `HullImpactRadiusCm`
- `HullCollisionDamageMinSpeedCmS`
- `HullCollisionCatastrophicSpeedCmS`
- `HullCollisionDamageAtCatastrophicSpeed`
- `HullCollisionDamageExponent`
- `bDebugLogHullCollisions`

Current source files:

- `Source/Sub3D/Submarine/SubmarineBase.h`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`

## 3.3 Breach and flood runtime

The current runtime already supports:

- hull impact projected to structural sheet
- local damage applied to cells
- open cells grouped into breach clusters
- exterior inflow from breach open area
- internal equalization through open doors
- compartment water accumulation
- mirrored compartment export for UI and gameplay

Existing linked systems:

- `USubmarineCompartmentComponent`
- `UFloodWaterVisualsComponent`
- `UBreachVfxManagerComponent`
- `USubmarineFeedbackDirectorComponent`

## 3.4 Repair path

Repair is already routable in runtime.

Current path:

- `USubInteractionComponent::TryRepairFocusedTarget()`
- server RPC to `ServerTryRepairTarget()`
- world-space repair point sent to `USubHullComponent::RepairAtWorldPoint()`

Current repair effect:

- reduces per-cell damage on the impacted structural sheet
- can close breaches if enough repair strength is applied
- rebuilds breach clusters after repair

This means "close the hole" already exists logically.

## 3.5 Pump-out path

The pump path now works logically after repair.

Current path:

- `USubmarineSystemsComponent` pushes pump state to `USubHullComponent`
- `USubHullComponent::SetAllPumpsActive()` stores target compartment pump-out rate
- `USubHullComponent::AdvanceFlooding()` now actually converts `PumpRateOut` into `FloodRateOut`

Current result:

- after sealing a breach, existing water can now be removed from the compartment
- repair no longer leaves the player with a permanently flooded compartment and a fake active pump

## 3.6 Stabilization and helm support

The helm command pipeline already includes:

- auto-speed
- auto-depth
- auto-pitch
- manual override suspend timers

This matters for breach gameplay because the submarine can now be held in a more stable state while the player responds to damage.

Current source files:

- `Source/Sub3D/Submarine/SubmarineSystemsComponent.h`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp`

---

## 4. What Is Not Implemented Yet

The following are not closed yet:

1. Visible hull opening on the exterior mesh.
2. Visual growth of the opening when repeated impacts enlarge the breach.
3. Visible repair patch state on the hull surface.
4. Real pressure gameplay. The pressure fields exist in runtime types, but current derived pressure stays at nominal values.
5. A durable repair tool loop with hold-to-repair, player-facing progress, and clear repair state presentation.
6. Explicit run-shell integration of "designated breach resolved" as a shipped gameplay objective.
7. Final sink / catastrophe criteria tied to the hull state for first playable closure.

Important:

- `BreachVfxManager` and `FloodWaterVisuals` exist.
- They are not the same thing as a visual hole in the hull mesh.

---

## 5. Editor Preconditions

Open:

- `c:\Dev\Sub3D\Sub3D.uproject`

Recommended map:

- `Content/Maps/L_FP01_RunShell.umap`

Recommended actor type:

- compiled submarine actor or `BP_Submarine_Compiler` instance with valid `SubHull`

Required runtime components on the active submarine:

- `SubMovement`
- `SubHull`
- `Systems`
- `Compartments`
- `BreachVfxManager`
- `FloodWaterVisuals`
- `FeedbackManager`

If using the compiler actor path:

1. compile the submarine
2. ensure the generated layout populates structural sheets
3. ensure at least one sheet can open to exterior

---

## 6. Editor Settings To Inspect Now

## 6.1 On the submarine actor

Damage section:

- `HullImpactDamageScale`
- `HullImpactRadiusCm`
- `HullCollisionDamageMinSpeedCmS`
- `HullCollisionCatastrophicSpeedCmS`
- `HullCollisionDamageAtCatastrophicSpeed`
- `HullCollisionDamageExponent`
- `bDebugLogHullCollisions`

Debug section:

- `bFreezeMovementForTesting`

Useful current default reading:

- low-speed contact should be survivable
- high-speed impact should be dangerous

## 6.2 On `SubHull`

Inspect:

- `DamageToThicknessScale`
- `LeakThreshold`
- `OpenThreshold`
- `BaseLeakFlowLitersPerSec`
- `MaxExteriorFloodInLitersPerSec`
- `MaxInternalConnectionFlowLitersPerSec`
- `PumpPressurePenaltyStartKPa`
- `PumpPressurePenaltyEndKPa`
- `MinPumpEfficiency01`
- `bDrawDebug`
- `bLogWaterLevels`

Current runtime debug helpers:

- breach spheres and flow field lines if `bDrawDebug = true`
- compartment water log output if `bLogWaterLevels = true`

## 6.3 On `Systems`

Inspect:

- `DefaultPumpCompartmentId`
- `BasePumpRateLitersPerSec`
- stabilization gains if you want a stable repair scenario in PIE

---

## 7. Current PIE Validation Protocol

## 7.1 Debug breach path

Fastest path for current editor validation:

1. start PIE with one player
2. force a breach using `CreateDebugBreachOnFirstExteriorSheet`
3. observe:
   - breach VFX if configured
   - water rise in flood visuals
   - compartment flood state update
4. repair the same point
5. activate pumps
6. verify water level decreases

This is the most deterministic path today.

## 7.2 Collision path

Gameplay path to validate A4:

1. drive the submarine at low speed into a wall
2. verify no real hull damage is applied
3. drive at high speed into the same wall
4. verify hull damage is applied
5. verify breach/flood can follow if the damage is high enough

## 7.3 Repair path

Current repair path assumptions:

- the player has a repair-capable interaction route
- the repair trace hits the submarine actor or one of its owned actors/components
- the impact point is on or near the damaged sheet

Expected current result:

- repair can close the breach logically
- pump can remove remaining water

---

## 8. Current Expected Results

You should be able to observe all of the following today:

1. point damage creates damaged hull cells
2. enough damage creates at least one breach cluster
3. flooding adds water to the owning compartment
4. repair can remove the breach
5. pump can reduce flooded water after repair
6. low-speed contact does not damage the hull
7. high-speed collision can damage the hull

If one of these does not happen, the current state is no longer in sync with the code handoff.

---

## 9. Known Current Limits

1. Pressure gameplay is still structurally present but not behaviorally closed.
2. Exterior hull visual rupture is not implemented.
3. Repair visuals are not implemented.
4. A single robust gameplay objective for breach resolution is not yet wired into the run shell.
5. The current flood loop is systemically valid, but not yet fully player-readable from the hull surface.

---

## 10. Immediate Editor Use Rule

Use the current system for:

- breach logic
- flooding logic
- repair logic
- pump-out logic
- feedback baseline

Do not treat the current exterior hull visuals as final.
They do not yet show the real opening state of the hull.

---

## 11. Canonical Files For This Current State

Primary files:

- `Source/Sub3D/Submarine/SubmarineBase.h`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`
- `Source/Sub3D/Submarine/SubHullComponent.h`
- `Source/Sub3D/Submarine/SubHullComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.h`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp`
- `Source/Sub3D/Submarine/SubInteractionComponent.h`
- `Source/Sub3D/Submarine/SubInteractionComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineCompartmentComponent.h`
- `Source/Sub3D/Submarine/SubmarineCompartmentComponent.cpp`

Supporting runtime files:

- `Source/Sub3D/Submarine/BreachVfxManagerComponent.h`
- `Source/Sub3D/Submarine/BreachVfxManagerComponent.cpp`
- `Source/Sub3D/Submarine/FloodWaterVisualsComponent.h`
- `Source/Sub3D/Submarine/FloodWaterVisualsComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineFeedbackDirectorComponent.h`
- `Source/Sub3D/Submarine/SubmarineFeedbackDirectorComponent.cpp`

---

## 12. Bottom Line

The logical hull gameplay loop now exists in runtime:

- collision damage can create hull damage
- hull damage can create breaches
- breaches can flood compartments
- repair can close breaches
- pumps can now remove the remaining water

The missing work is no longer the logical hull loop itself.
The missing work is the full player-facing solution around it:

- visible hull rupture
- visible repair state
- explicit crisis objective
- final robust first playable integration
