# Sub3D Feedback Architecture

Date: 2026-03-28
Scope: shakes, impact sounds, flood alarms, future weapon/pressure feedback
Status: working reference

## 1. Purpose

This document isolates the feedback layer from hull simulation and movement simulation.

Goal:
- keep damage and flooding as gameplay truth,
- make shakes and sounds readable,
- avoid coupling camera feedback to global world-space rules that break the submarine fantasy.

This is a feedback architecture note, not a physics note.

## 2. Core Distinction

There are three different things:

1. The shake asset
- defines waveform only
- duration
- amplitude
- frequency
- blend in / blend out

2. The runtime trigger
- decides when to play the shake
- with what scale
- at what epicenter
- with what filtering rules

3. The receiver filter
- decides who actually receives the shake/sound
- embarked crew only
- same submarine only
- optional distance falloff inside the submarine
- optional attenuation outside the submarine

This means:
- the `CameraShakeBase` asset should not decide gameplay radius
- radius belongs to runtime logic, not to the shake asset

## 3. Decision: No Global World Shake For Interior Feedback

For submarine interior feedback, `PlayWorldCameraShake` is not the right long-term primitive.

Reason:
- it is world-centric
- it naturally thinks in "players near an epicenter"
- it does not express "only crew inside this submarine"
- it risks affecting players outside the submarine if they are spatially close

Decision:
- interior submarine shakes should be submarine-scoped, not world-scoped
- preferred trigger path is local-controller camera shake after filtering by submarine membership

Implication:
- `PlayWorldCameraShake` is acceptable as a temporary debug tool
- final runtime should prefer:
  - identify relevant local crew/controller
  - compute local distance to epicenter inside submarine space
  - call `ClientStartCameraShake` only for those receivers

## 4. Feedback Domains

Feedback splits into three domains.

### A. Interior-local feedback

Affects embarked crew inside the submarine only.

Examples:
- hull impact on nearby compartment
- turret firing through the hull
- pressure water burst on door open
- violent suction / breach pull
- nearby repair hammering or emergency machinery later

Rules:
- receiver must be inside the same submarine
- scale depends on distance in submarine-local space
- no propagation to players outside

### B. Exterior-world feedback

Affects actors in the world according to normal spatial rules.

Examples:
- explosion outside hull
- large external collision
- world-scale event

Rules:
- world-space attenuation is acceptable
- can use standard world audio/shake logic

### C. UI / state feedback

Non-positional feedback.

Examples:
- flood alarm loop
- compartment warning
- critical breach indicator

Rules:
- driven by gameplay state
- not tied to a precise epicenter
- should not stack uncontrollably

## 5. Minimum Runtime Model

The system should converge toward these concepts.

### Feedback Event

Each gameplay event emits a feedback request:
- event type
- owning submarine
- local epicenter
- intensity
- radius
- optional sound
- optional shake

Examples:
- `HullImpact`
- `TurretFire`
- `PressureBurst`
- `BreachSuction`
- `FloodAlarmStateChanged`

### Receiver Rule

For local player feedback:
- if player is not embarked: no interior shake
- if player is embarked in another submarine: no interior shake
- if player is embarked in this submarine:
  - compute local distance to event epicenter
  - apply falloff
  - play shake if within radius

This gives the required rule:
- players outside the submarine are not affected by interior hull shakes

## 6. Is A Feedback Director Needed?

Yes, probably, but not as a huge system immediately.

A dedicated local director becomes justified because feedback sources are already multiplying:
- hull impact
- turret firing
- pressure burst through a door
- breach suction / violent flow
- future machinery pulses if needed

Recommended direction:
- one `SubmarineFeedbackDirectorComponent`
- owned by `ASubmarineBase`
- centralizes:
  - shake dispatch
  - interior sound dispatch
  - flood alarm state
  - anti-stack / cooldown policy

Do not create a generic cross-game "Feedback System" yet.

Keep it local:
- `ASubmarineBase`
- its embarked crew
- its local events

## 6.1 Runtime Anchor Decision

The feedback runtime should live on the submarine actor, not in `GameMode` or `GameInstance`.

Reason:
- `GameMode` is server-only and cannot own client-local presentation state cleanly
- `GameInstance` is too global and does not express "this submarine, this crew"
- the submarine actor already owns:
  - hull simulation
  - movement frame
  - embarked crew relationship
  - local event epicenters in submarine space

Implementation target:
- `USubmarineFeedbackDirectorComponent`
- attached to `ASubmarineBase`
- one instance per submarine

## 6.2 Designer Authoring Model

The director should not hardcode content references.

Designer-facing authoring should live in a dedicated data asset:
- `USubmarineFeedbackProfile`

This profile owns:
- hull impact shake asset
- hull impact sound asset
- flood alarm sound asset
- interior receiver policy
- distance radii
- damage-to-intensity scaling
- alarm MetaSound parameter names and defaults

Benefits:
- designers tune one asset instead of hunting properties across runtime code
- multiple submarine classes can swap feedback style without code changes
- prototype and production profiles can coexist cleanly

Recommended workflow:
1. Create a feedback profile asset per prototype or submarine class
2. Assign it on the feedback director component of the submarine BP
3. Tune shakes/sounds/radii there
4. Validate in PIE/Standalone with debug breach and future weapon events

Alarm props should be separate spatial anchors:
- one or more `SubmarineAlarmBeacon` actors attached to the submarine BP
- each beacon owns:
  - mesh
  - red light / gyrophare-style light
  - spatialized audio component
- the feedback director only drives state and parameters

## 7. Shake Policy

### 7.1 Hull Impact

Source:
- impact on hull
- debug breach can optionally trigger a lighter version if desired

Inputs:
- damage or impulse
- local breach/impact point

Scale:
- intensity from damage or impulse
- falloff from local player distance to epicenter

Receiver:
- embarked crew inside same submarine only

### 7.2 Turret Fire

Source:
- exterior turret firing

Inputs:
- turret local position
- weapon class
- ammo class

Scale:
- base shake profile depends on turret/munition type
- strong near turret
- lightly perceptible a few meters away through hull

Receiver:
- same submarine crew only

### 7.3 Pressure Burst / Door Water Hit

Source:
- player opens a pressured door
- localized water burst event

Inputs:
- door local position
- pressure differential

Scale:
- tied to pressure differential
- much more local than hull impact

Receiver:
- crew near door only

### 7.4 Breach Suction / Violent Water Push

Source:
- strong nearby breach flow

Inputs:
- breach local position
- flow force scale

Scale:
- local, short, repeated or pulsed carefully

Receiver:
- crew inside effective flow radius only

Important:
- this should not become permanent camera sickness
- use cooldowns and thresholding

## 8. Sound Policy

### 8.1 Hull Impact Sound

Positional interior/exterior event.

Rules:
- same submarine interior listeners get the sound
- scale by damage and local distance
- no crash if sound unset

### 8.2 Turret Fire Sound

Same receiver rule as turret shake:
- same submarine crew only
- falloff from turret local position
- per-weapon content profile later if needed

## 9. Immediate Implementation Slice

The next concrete implementation should be:

1. Move current `C.5` logic out of `ASubmarineBase`
2. Create `USubmarineFeedbackDirectorComponent`
3. Create `USubmarineFeedbackProfile`
4. Route:
   - hull impact shake
   - hull impact sound
   - flood alarm loop
5. Filter receivers by:
   - embarked crew only
   - same submarine only

This gives a stable local foundation before adding:
- turret recoil feedback
- pressure burst feedback
- flow-push feedback
- cooldown layering and mix policy

Likely dual-layer later:
- external weapon report
- internal transmitted hull thump

The internal layer is what matters for crew feedback.

### 8.3 Flood Alarm

State-driven loop.

Rules:
- one loop only
- no stacking
- starts when at least one compartment crosses danger threshold
- stops when all return below threshold

### 8.4 Pressure / Water Burst Sound

Short local event near door or breach.

Rules:
- positional
- high readability
- avoid spam if repeated every frame

## 9. Radius Guidance

Because radius belongs to runtime and not to the shake asset, define reference values per event type.

Initial guidance:

- Hull impact:
  - local radius roughly `500-6000 cm` depending on damage class and desired readability
  - but filtered to same-submarine embarked crew only

- Turret fire:
  - strong zone around turret
  - soft falloff through surrounding compartments

- Pressure burst:
  - very local radius

- Breach suction:
  - local radius tied to flow field / breach severity

These values are tuning inputs, not asset properties.

## 10. Why The Asset Does Not Expose Radius

`CS_HullImpact` should only define:
- pattern shape
- timing
- amplitude limits

It should not define:
- who receives the shake
- whether players outside receive it
- whether the event is local or global

That logic belongs in code/director.

## 11. Immediate Recommendation

Short term:

1. Keep `CS_HullImpact` as waveform asset only.
2. Stop thinking in terms of "radius inside the asset".
3. Add a submarine-local feedback dispatch layer next.
4. Replace direct world-style shake triggering with:
   - embarked receiver filtering
   - same-submarine check
   - local-distance falloff

## 12. Next Implementation Step

Recommended next coding pass:

1. Create a minimal `SubmarineFeedbackDirectorComponent`
2. Move hull impact shake/sound dispatch there
3. Filter to:
   - local player controller
   - embarked on this submarine
4. Compute shake scale from:
   - event intensity
   - local distance to epicenter
5. Keep flood alarm handling there too

This would consolidate `C.5` properly before adding:
- turret feedback
- pressure burst feedback
- water push feedback

## 13. Non-Goals

Not in this document:
- full physics tuning
- buoyancy rewrite
- fluid simulation
- replication redesign
- final sound mix

This document only defines how feedback should be structured and dispatched.
