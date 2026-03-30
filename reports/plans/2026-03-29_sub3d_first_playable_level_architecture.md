# Sub3D - First Playable Level Architecture

Date: 2026-03-29
Status: Working level-shell spec
Scope: `FP-2` first playable level shell, traversal setup, collision validation, breach trigger placement, approach shell
Parents:
- `C:\Dev\Sub3D\reports\plans\2026-03-29_sub3d_first_playable_run_spec.md`
- `C:\ACC\Projects\Sub3D\Plans\sub3d\sub3d_product_north_star.md`

---

## 1. Purpose

This document defines the exact shell of the first playable level.

It is not a worldbuilding document.
It is a playable-level contract for:
- one submarine
- one baked traversal route
- one start shell
- one end shell
- one deterministic breach event

The level must be easy to assemble, easy to validate, and good enough to expose real runtime problems.

---

## 2. Product Goal

The level must let a tester do exactly this:

1. spawn in or immediately beside the submarine at the start dock
2. enter the interior and reach the helm
3. commit departure
4. clear the departure gate
5. physically traverse the route
6. hit one deterministic breach trigger
7. react to flooding
8. continue traversal
9. enter the end approach zone
10. stop before full docking logic if docking is not yet implemented

For `FP-2`, success is:
- the route is physically traversable
- the submarine starts in the correct place
- the level owns the right triggers and references
- `Departure -> Traverse -> BreachCrisis` can be exercised in PIE

---

## 3. What Already Exists

### Reusable runtime

`ATraversalRouteActor`
- already stores:
  - route geometry
  - route collision
  - start transform
  - end transform
  - start dock transform
  - end dock transform

`ASubGameMode`
- now owns:
  - run phase
  - breach trigger acceptance
  - crisis recovery
  - approach-zone latch
  - departure gate callback

`ASubBreachTriggerVolume`
- deterministic breach trigger volume

`ASubRunPhaseVolume`
- level shell volume actor for:
  - `DepartureGate`
  - `ApproachZone`

---

## 4. Level Composition

The first playable level must contain exactly these runtime-critical elements:

1. one `ATraversalRouteActor`
2. one active `ASubmarineBase` or compiler-derived submarine actor
3. one start shell
4. one end shell
5. one `ASubRunPhaseVolume` configured as `DepartureGate`
6. one `ASubBreachTriggerVolume`
7. one `ASubRunPhaseVolume` configured as `ApproachZone`

Optional for readability:
- one or two simple light actors at each dock
- one temporary debug sign or mesh at start/end

Not required:
- final dock art
- final end docking actor
- final sonar UI

---

## 5. Level Ownership Rules

### Route actor is authoritative for transforms

The route actor already knows:
- `GetRouteStartTransformWorld()`
- `GetRouteEndTransformWorld()`
- `GetRouteStartDockTransformWorld()`
- `GetRouteEndDockTransformWorld()`

Therefore:
- the level shell must be positioned relative to the route actor
- do not author an unrelated start/end path beside it

### Start and end shells are presentation shells

For `FP-2`, the dock shells are not authoritative gameplay systems.
They are:
- readable spaces
- alignment cues
- simple arrival/departure context

The route transforms remain the hard runtime reference.

---

## 6. Placement Contract

### 6.1 Route

Place one `ATraversalRouteActor` using a baked route that already produces valid collision.

Minimum route requirements:
- the full submarine hull can traverse without constant snagging
- route start and end are readable
- start and end dock transforms are valid

### 6.2 Submarine spawn

Place one submarine actor.

For the first level shell:
- align the submarine to `RouteStartDockTransformWorld`
- if needed, apply a small local offset only for visual fit
- do not hand-author a completely different heading than the route start dock heading

Validation:
- on BeginPlay, the submarine should visually sit inside the start shell
- the forward direction must naturally point into the route departure direction

### 6.3 Start shell

Author a simple dock shell around the route start dock transform.

Minimum acceptable shell:
- floor or cradle
- one rear wall or gate read
- one clear forward exit

Do not overbuild:
- this shell is not a final station
- it only needs to communicate "starting berth"

### 6.4 Departure gate

Place one `ASubRunPhaseVolume` and set:
- `VolumeType = DepartureGate`
- `bConsumeAfterActivation = true`

Placement rule:
- place it just outside the start shell throat
- the submarine must cross it only after a deliberate forward move
- it must not overlap the initial idle spawn position

Recommended first dimensions:
- `X = 1200`
- `Y = route dock radius * 0.9 to 1.2`
- `Z = route dock radius * 0.9 to 1.2`

Validation:
- initial spawn must not trigger it
- a normal departure line must trigger it reliably
- backing in and out after trigger must not matter because it is consumed once

### 6.5 Breach trigger

Place one `ASubBreachTriggerVolume` on the canonical traversal path.

Placement rule:
- place it after departure is clearly established
- place it before the destination approach area
- recommended initial target:
  - around 30% to 55% of route traversal distance

Do not place it:
- immediately outside the start dock
- inside the end approach shell
- in a branch or optional route until the canonical traversal read is locked

Recommended first dimensions:
- start with `500 x 500 x 500`
- enlarge only if the sub can miss it in normal traversal

Validation:
- it triggers exactly once
- it is not reachable before `Traverse`
- it does not get hit accidentally while idling at start

### 6.6 End shell

Author a simple end destination shell around `RouteEndDockTransformWorld`.

Minimum acceptable:
- visually distinct from start shell
- readable arrival funnel
- one broad final berth or gate cue

Do not build final docking mechanics into the geometry yet.

### 6.7 Approach zone

Place one `ASubRunPhaseVolume` and set:
- `VolumeType = ApproachZone`
- `bConsumeAfterActivation = false`

Placement rule:
- the volume should cover the last readable arrival corridor before the end dock
- entering it should mean "the player is now in arrival space"
- leaving it should be possible if the player backs out

Recommended first dimensions:
- long enough to be stable:
  - `X = 5000 to 9000`
- wide enough to match the final corridor:
  - `Y/Z = route end dock radius * 1.2 to 1.5`

Validation:
- entering it during normal traversal should set the approach latch
- if no breach is active, `Traverse -> Approach`
- if a breach is active, phase remains `BreachCrisis` but the latch stays true

---

## 7. Runtime Phase Contract In The Level

### 7.1 Boarding

Initial playable state:
- player is in or immediately attached to the submarine context
- player can walk inside
- player can reach the helm

### 7.2 Departure

For now, departure commitment can be triggered by:
- an explicit BP call to `SubGameMode.BeginDeparture()`
- a temporary debug input or station hook

`FP-2` does not need the full helm UX finalized.
It does need a reliable way to enter `Departure`.

### 7.3 Traverse

`Departure -> Traverse` occurs when the submarine crosses the `DepartureGate`.

This must be level-driven, not guessed from distance alone.

### 7.4 Breach crisis

`Traverse -> BreachCrisis` occurs via `ASubBreachTriggerVolume`.

The level must make it possible to:
- reach the trigger reliably
- repair the breach after it fires
- continue moving afterward

### 7.5 Approach

Approach is driven by the end `ApproachZone`.

The current behavior is:
- no active breach:
  - entering the zone transitions `Traverse -> Approach`
- active breach:
  - entering the zone only latches destination presence
  - when the breach is stabilized, `BreachCrisis -> Approach`

---

## 8. Collision Validation

The level shell is not done until collision is validated.

### Required collision checks

1. Start shell clearance
- the submarine can idle at start without immediate collision spam

2. Departure throat clearance
- the submarine can leave the start shell cleanly

3. Route traversal clearance
- the submarine can complete the canonical path without repeated snagging
- light graze is acceptable
- chronic obstruction is not

4. Trigger reliability
- the submarine can cross:
  - `DepartureGate`
  - `BreachTrigger`
  - `ApproachZone`
  during a normal run

5. End shell clearance
- arrival corridor remains navigable

### Anti-fake rule

Do not validate this shell from free camera only.
Validation must be done from actual submarine traversal in PIE or Standalone.

---

## 9. Map Assembly Procedure

Recommended assembly order:

1. place the baked `ATraversalRouteActor`
2. verify route collision visually and with a quick sub pass
3. place the submarine aligned to `RouteStartDockTransformWorld`
4. build the minimal start shell
5. place and tune the `DepartureGate`
6. place the `BreachTrigger`
7. build the minimal end shell
8. place and tune the `ApproachZone`
9. run the first full traversal pass

Do not start with art polish.
Start with spatial truth and reliable triggers.

---

## 10. Recommended Temporary Values

These are starting values, not final tuning.

### Submarine
- use the currently stable test submarine
- enable debug freeze only when testing interior-only systems
- disable freeze for traversal validation

### DepartureGate
- extent:
  - `1200, 1800, 1800` as a first pass

### BreachTrigger
- extent:
  - `500, 500, 500`

### ApproachZone
- extent:
  - `7000, 2500, 2500`

These values should be adapted to the real route radius and hull size.

---

## 11. Manual Validation Checklist

### FP-2 shell validation

1. Start PIE in the first playable map
2. Confirm player enters the submarine context correctly
3. Confirm the player can reach the helm area
4. Trigger `BeginDeparture()`
5. Move forward and confirm `DepartureGate` advances to `Traverse`
6. Continue traversal and confirm the route is physically navigable
7. Confirm `ASubBreachTriggerVolume` advances to `BreachCrisis`
8. Repair the breach and confirm the run can continue
9. Enter the `ApproachZone`
10. Confirm:
- no active breach:
  - `Traverse -> Approach`
- active breach:
  - phase stays `BreachCrisis`
  - after repair:
    - `BreachCrisis -> Approach`

### Failure conditions for FP-2

The shell is not acceptable if:
- the submarine spawns misaligned with the route
- `DepartureGate` triggers at spawn
- the breach trigger is too early or too late to read
- the route is not reliably traversable
- the approach zone is impossible to enter cleanly

---

## 12. Out Of Scope

Not part of `FP-2`:
- final sonar UI
- final docking confirm logic
- final dock meshes
- campaign chaining
- dynamic world streaming
- advanced exterior threats

---

## 13. Deliverables

`FP-2` is considered delivered when:

1. the level contains all required runtime shell actors
2. the submarine can leave start and traverse the route
3. `Departure -> Traverse` is level-driven and reliable
4. the breach trigger is placed and reliable
5. the approach zone is placed and reliable
6. the shell is good enough to expose real runtime issues in PIE

---

## 14. Immediate Next Step

After this document:

1. assemble the first playable level in the editor
2. validate:
   - traversal collision
   - departure gate
   - breach trigger
   - approach zone
3. only then continue with:
   - `FP-3 Sonar V1`
   - `FP-5 Docking V1`
