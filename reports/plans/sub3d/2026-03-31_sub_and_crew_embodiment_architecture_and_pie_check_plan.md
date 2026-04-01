# Sub3D - Character Embodiment Architecture and PIE Check Plan

Date: 2026-03-31
Scope: Crew embodiment, inertial reactions, bracing, contact hands, narrow-space posture, stagger/fall logic
Status: Architecture lock only, no implementation yet

## 0. Decision

Yes, this topic should be included now at architecture level.

No, it should not be implemented as a large feature block before the support/runtime truth is stable.

Reason:

- leaning, bracing, hand placement, stagger, and falls all depend on stable support truth
- they also depend on a canonical inertial signal from the submarine
- they also depend on reliable local environment queries:
  - floor support
  - nearby walls
  - rails / consoles / supports
  - narrow-space clearance

If implemented too early, these features will hide core support bugs instead of solving them.

## 1. Current Code Reality

From repository inspection:

- `ASubCrewCharacter` is still a mostly gameplay/probe character with capsule + FPS camera + interaction component.
- `USubCrewMovementComponent` currently keeps stock `CharacterMovementComponent` based movement.
- the codebase currently exposes no C++ animation-runtime contract for:
  - inertial pose offsets
  - procedural leaning
  - foot IK
  - hand IK
  - bracing states
  - stagger/fall states
  - physical animation / ragdoll blend
- `SubPlayerController` contains a specific sonar lean input flow, but that is UI/station-specific, not a general embodied locomotion system.

Implication:

The embodiment system should be introduced as a new runtime layer on top of stable movement/support, not by overloading the current movement component with animation logic.

### 1.1 Runtime exports now available

The current code now exposes a first runtime contract for embodiment-oriented systems.

From `USubInteriorFrameComponent`:

- local linear velocity
- local linear acceleration
- local angular velocity
- local angular acceleration

From `USubCrewMovementComponent`:

- valid floor state
- accepted/canonical base state
- recovery-needed state
- support quality scalar
- nearby brace support state
- nearby brace support location/normal/distance

This is enough to start a BP/AnimBP contract without making animation graphs guess from raw actor transforms alone.

## 2. Recommended Architecture

### 2.1 Separate three layers

Keep these responsibilities distinct:

1. locomotion / support layer
   - authoritative floor support
   - movement base
   - fall/stability decisions

2. embodiment state layer
   - stable / brace / stagger / fall / recover
   - hand contact requests
   - narrow-space posture requests
   - procedural response parameters

3. animation presentation layer
   - additive lean
   - aim offsets / upper body overlays
   - hand IK
   - foot stabilization
   - fall / recover montages or state machine clips

Do not collapse all three into `USubCrewMovementComponent`.

### 2.2 New truth to add: inertial reaction signal

The crew needs a canonical per-frame signal derived from submarine movement.

That signal should not be guessed from camera shake or animation velocity.

Target runtime signal:

- local linear acceleration of the submarine frame
- local angular acceleration or angular velocity delta
- current support quality
- current base validity
- optional impact impulse events

Recommended producer:

- `USubInteriorFrameComponent` or a sibling runtime component

Recommended consumer:

- a future crew embodiment component, not the raw AnimBP alone

### 2.3 New truth to add: support affordance query

The embodiment layer needs to know more than "is walking".

It needs:

- is there stable floor support
- how far is the nearest brace surface
- is there a wall within hand-reach left/right/front
- is there a valid support object:
  - wall
  - console
  - railing
  - bulkhead edge
- is the corridor narrow enough to justify shoulder/arm contact

This should be a canonical gameplay query layer, not arbitrary traces from animation blueprint graphs.

### 2.4 Embodiment state model

Recommended first-pass state model:

1. Stable
   - normal locomotion
   - mild additive inertial lean only

2. Bracing
   - strong inertial event predicted or ongoing
   - nearby support exists
   - locomotion slowed
   - hand/arm contact may engage

3. Staggering
   - support degraded or inertial threshold exceeded
   - feet are trying to recover
   - root or capsule stays gameplay-authoritative

4. Falling
   - support lost and no valid nearby brace support
   - violent acceleration / impact / turn threshold exceeded
   - locomotion transitions to fall / knock / grounded imbalance state

5. Recovering
   - after fall or large stagger
   - reacquire support
   - blend back to stable

6. NarrowSpaceContact
   - optional overlay state
   - close walls / cramped areas
   - hand-on-wall / shoulder constraint / reduced arm swing

These are gameplay states first, animation states second.

## 3. What Should Be Implemented First

### 3.1 Before animation features

Add runtime observability and data contracts:

1. expose inertial metrics from submarine frame
2. expose support quality / base validity as explicit crew runtime state
3. add support-affordance queries near the crew capsule
4. add debug drawing and logging for these queries

Without these, procedural animation will be guesswork.

### 3.2 Before hand placement

Define support categories.

Minimum tags or categories recommended:

- `BraceWall`
- `BraceRail`
- `BraceConsole`
- `BraceDoorFrame`
- `NarrowPassageSurface`

These can later map to sockets, surfaces, or trace filters.

### 3.3 Before falls

Define deterministic fall triggers.

A fall should not be "animation decided it looked right".

A fall should come from gameplay thresholds combining:

- support validity
- local acceleration
- local angular impulse
- current crew locomotion speed
- nearby support availability
- optional impact event

## 4. What Must Stay Out of Scope For Now

Do not do these before support stabilization:

- full-body ragdoll or physics-driven crew
- large custom locomotion rewrite
- animation-first wall tracing in AnimBP as source of truth
- montage soup to mask unstable floor/base behavior
- "feel" tuning before the state transitions are trustworthy

## 5. Proposed Implementation Order

### Step A - Runtime contracts

Primary code targets:

- `Source/Sub3D/Submarine/SubInteriorFrameComponent.*`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.*`
- `Source/Sub3D/Submarine/SubCrewCharacter.*`

Deliverables:

- explicit inertial metrics
- explicit support validity
- explicit base validity
- explicit recovery attempts

### Step B - Embodiment component

Primary code target:

- new component recommended, for example `USubCrewEmbodimentComponent`

Responsibilities:

- convert runtime metrics into embodiment state
- publish animation parameters
- request brace contact / stagger / fall

Reason:

This avoids turning `USubCrewMovementComponent` into a mixed movement + animation brain.

### Step C - Animation integration

Primary content/runtime targets:

- skeletal mesh / AnimBP layer
- additive lean poses
- hand placement IK
- fall/recover clips
- locomotion overlays

This should consume embodiment state, not invent it.

### Step D - Tuning

Only after state transitions are proven:

- thresholds
- blend timings
- arm swing suppression
- narrow-space contact bias
- bracing aggressiveness

## 6. Editor Check Plan

These checks are intended before full PIE traversal tests.

### Editor checks

1. Verify the crew actor has a clear runtime owner for embodiment.
   - if using a skeletal mesh, confirm where the mesh and AnimBP are bound
   - if not, document that embodiment implementation requires that binding first

2. Verify support affordance surfaces can be identified.
   - walls
   - rails
   - consoles
   - door frames

3. Verify collision/profile taxonomy is usable for support queries.
   - `SubInteriorWalkable`
   - `SubInteriorVisual`
   - any future brace/support profile or tags

4. Verify debug hooks exist or are planned for:
   - support base
   - inertial vectors
   - brace candidates
   - fall trigger reason

## 7. PIE Runtime Check Plan

These are the checks to run once Phase 3 support stabilization is accepted.

### PIE pass 1 - Stable support baseline

1. Spawn crew in submarine at rest.
2. Confirm:
   - valid floor
   - valid movement base
   - no unexpected lean
   - no stagger state

GO:

- stable state only

### PIE pass 2 - Mild submarine motion

1. Apply mild forward movement and soft yaw.
2. Confirm:
   - crew remains stably grounded
   - only mild additive lean or posture reaction
   - no brace spam

GO:

- embodiment reacts without destabilizing gameplay support

### PIE pass 3 - Violent turn with nearby support

1. Place crew near wall / console / rail.
2. Force hard yaw or sudden acceleration.
3. Confirm:
   - brace state can trigger
   - contact target is plausible
   - no fall if support exists and thresholds permit bracing

GO:

- brace path wins over fall when support is available

### PIE pass 4 - Violent turn without nearby support

1. Place crew in open area with no close brace surface.
2. Force the same hard motion.
3. Confirm:
   - brace does not falsely trigger
   - stagger or fall can trigger deterministically
   - recover state reacquires support afterward

GO:

- fall logic is deterministic and explainable

### PIE pass 5 - Narrow corridor contact

1. Move crew through tight compartments and doorways.
2. Confirm:
   - optional hand/wall contact overlay can appear
   - locomotion remains readable
   - no collision fighting from animation offsets

GO:

- contact is a presentation assist, not a gameplay collision hack

### PIE pass 6 - Impact event

1. Simulate a large submarine hit or breach-adjacent disturbance.
2. Confirm:
   - inertial event is propagated
   - crew transitions through brace / stagger / fall according to context
   - transitions are consistent across repeated runs

GO:

- impact-driven embodiment is keyed from runtime truth, not randomness

## 8. Concrete Recommendation

Ask for this architecture review now.

Do not ask for full implementation of leaning, hand placement, or falls in the same batch as support stabilization.

Recommended next execution split:

1. finish Phase 3 support/base stabilization
2. add embodiment runtime contracts and debug
3. run PIE checks for support + inertial signals
4. only then implement procedural/basic animation reactions

## 9. Next Patch Suggestion

The next useful patch is not "all character ergonomics".

The next useful patch is:

- add explicit crew support validity state
- add inertial signal exposure from submarine frame
- add debug support/bracing queries
- add a first embodiment contract document for BP/AnimBP integration

That is the correct technical bridge between the current codebase and the richer embodied behavior you described.
