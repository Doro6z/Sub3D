# Sub3D — Unified Client Presentation Chain Refactor (Path 2)

**Date**: 2026-04-27
**Authority-max reference**: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
**Predecessor plans**:
- `reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md` (Phase 1, completed)
- `reports/plans/2026-04-27_motion_conditional_jitter_root_cause_audit.md` (audit, ruled out sub-only causes)

**Scope**: refactor the entire client-side presentation chain — sub playback, interior
inertia, crew rebase, grid replication smoothing, replicated input visual decoupling — so
that every visual reading the submarine state samples a single coherent presentation
state at the same render-time anchor.

---

## 1. Purpose

The audit dated 2026-04-27 demonstrated that sub motion is the necessary trigger for the
visible jitter, but five iterations on `USubMovementComponent` non-authority playback
produced no visible change. The dominant remaining hypotheses (audit §5.1–§5.5) point at
**incoherent sampling between consumers of sub motion**:

- Sub world transform is now smoothed (Phase 1 buffered playback).
- `USubInteriorFrameComponent` still derives inertia from `SubMovement->Velocity`, which
  is the latest-snapshot replicated velocity — not the per-frame derivative of the
  smoothed playback transform.
- `OnRep_GridSpaceTransform` applies peer-crew grid pose with no smoothing.
- Rudder / dive-plane / thrust replicated inputs drive mesh rotation visually with their
  own replication cadence, independent of the sub body playback.
- Compartment volume debug / water plane components ride the sub transform directly.

Each consumer reads sub motion at a different cadence and from a different source. The
result is a presentation chain where the sub body is smooth but everything around it
(inertia-driven camera sway, peer crew on the sub, visual mesh rotations, water visuals)
is sampled at a different beat. The eye groups the whole assembly as "sub jitter".

This refactor fuses every presentation-side reader onto **one coherent client
presentation state**, sampled at one render-time anchor each tick.

---

## 2. Refactor Goal

Define a single per-frame "what does the sub visually look like right now" state that
all client-side presentation consumers read from, derived from the same buffered
playback timeline:

```
ClientPresentationState (computed once per render frame, non-authority only)
├── Pose (location, rotation) — already smoothed by Phase 1 buffered playback
├── LinearVelocity, AngularVelocity — DERIVED from buffer at render time, not from snapshot.LinearVelocity
├── LinearAcceleration, AngularAcceleration — DERIVED from frame-to-frame deltas of the above
├── ReplicatedInputs (Rudder, DivePlane, Thrust) — buffered + smoothed for visual lerp
└── PeerCrewGridSmoothState — applied in USubCrewMovementComponent::OnRep on SimProxy peers
```

Every consumer that currently reads either `Owner->GetActorTransform()` mid-frame or
`SubMovement->Velocity` directly must instead read from getters on
`USubMovementComponent` that all return the same render-time-anchored state.

---

## 3. Hard Constraints

1. The authoritative simulation path in `USubMovementComponent` remains unchanged.
2. `FSubmarineNetState` payload remains unchanged. No new replicated fields on the sub
   state.
3. Tick prereq order remains `SubFlood → SubMovement → InteriorFrame → CrewMovement`.
4. The five working scenarios MUST continue to work:
   - spawn
   - helm-only piloting (sub moves, no crew on foot)
   - local crew standing still on a moving sub
   - frozen sub + crew walking any surface
   - EVA hull crossing
5. No new `UActorComponent` is introduced. The presentation state lives as private
   state inside `USubMovementComponent`, with getters consumed by `USubInteriorFrameComponent`
   and `USubCrewMovementComponent`.
6. No BP-asset wiring changes for the submarine actor (`BP_Submarine_Craniata`). The
   refactor must not require re-saving BP assets to take effect on existing scenes.
7. The Phase 1 buffered playback structure (`FBufferedClientSubSnapshot`,
   `FClientPlaybackSample`, `EvaluateClientPlaybackPose`, `ApplyClientPlaybackPose`,
   `QueueClientSnapshot`, `ResetClientPlayback`) is preserved as the substrate. This
   refactor extends it, not replaces it.

---

## 4. Target Internal Design

### 4.1 New presentation state on `USubMovementComponent`

Add a new private struct populated each non-authority tick after
`EvaluateClientPlaybackPose` runs:

```cpp
struct FClientPresentationState
{
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector LinearVelocity = FVector::ZeroVector;       // cm/s, world space, derived
    FVector AngularVelocity = FVector::ZeroVector;      // deg/s, world space, derived
    FVector LinearAcceleration = FVector::ZeroVector;   // cm/s^2
    FVector AngularAcceleration = FVector::ZeroVector;  // deg/s^2
    float RudderInputSmoothed = 0.f;
    float DivePlaneInputSmoothed = 0.f;
    float ThrustInputSmoothed = 0.f;
    double SampleWorldTime = 0.0;
    bool bIsValid = false;
};
FClientPresentationState ClientPresentationState;
FClientPresentationState ClientPresentationStatePrev;
```

`ClientPresentationStatePrev` keeps the previous frame's state for derivative
computation.

### 4.2 Per-frame computation order, non-authority path

1. `EvaluateClientPlaybackPose(WorldNow, Sample)` — produces Location + Rotation.
2. Build `ClientPresentationState`:
   - `Location`, `Rotation` ← `Sample.Location`, `Sample.Rotation`
   - `LinearVelocity` ← `(Location - Prev.Location) / DeltaTime` (NOT from
     snapshot velocity; that is the authoritative-instantaneous velocity, not the
     render-time velocity)
   - `AngularVelocity` ← `(Rotation - Prev.Rotation).GetNormalized() / DeltaTime`
   - `LinearAcceleration` ← `(LinearVelocity - Prev.LinearVelocity) / DeltaTime`
   - `AngularAcceleration` ← `(AngularVelocity - Prev.AngularVelocity) / DeltaTime`
   - `RudderInputSmoothed` ← `FMath::FInterpTo(Prev.RudderInputSmoothed, RudderInput,
     DeltaTime, kInputSmoothingHz)` (same for DivePlane, Thrust)
   - `bIsValid` ← `true`
3. `ApplyClientPlaybackPose(Location, Rotation)` — writes Owner.Transform.
4. `ClientPresentationStatePrev = ClientPresentationState`.

### 4.3 New public getters on `USubMovementComponent`

```cpp
// All return the current frame's presentation values. On authority, they fall through to
// the existing instantaneous fields (Velocity, etc.) for backward compatibility.
const FClientPresentationState& GetClientPresentationState() const;
FVector GetPresentedLinearVelocity() const;
FVector GetPresentedAngularVelocityDeg() const;
FVector GetPresentedLinearAcceleration() const;
FVector GetPresentedAngularAccelerationDeg() const;
float GetPresentedRudderInput() const;
float GetPresentedDivePlaneInput() const;
float GetPresentedThrustInput() const;
```

### 4.4 `USubInteriorFrameComponent` migration

Currently `TickComponent` does:

```cpp
const USubMovementComponent* SubMov = ...;
WorldLinearVelocity = SubMov->Velocity;  // authoritative-instantaneous, snapshot-rate
LocalLinearVelocity = ActorTransform.InverseTransformVector(WorldLinearVelocity);
LocalLinearAcceleration = computed only on SimFrameCounter advance, divided by SimDt;
```

After refactor:

```cpp
WorldLinearVelocity = SubMov->GetPresentedLinearVelocity();
LocalLinearVelocity = ActorTransform.InverseTransformVector(WorldLinearVelocity);
LocalLinearAcceleration = ActorTransform.InverseTransformVector(SubMov->GetPresentedLinearAcceleration());
LocalAngularVelocityDegrees = ActorTransform.InverseTransformVector(SubMov->GetPresentedAngularVelocityDeg());
LocalAngularAccelerationDegrees = ActorTransform.InverseTransformVector(SubMov->GetPresentedAngularAccelerationDeg());
```

The fallback path (no SubMov, finite-difference on Owner location) is removed: the
SubMovementComponent is the single source of truth.

The `LastSeenSubSimFrame` gating logic for acceleration-only-on-sim-step is removed.
Acceleration is now derived per render frame from the smoothed velocity, which is
already coherent.

### 4.5 `USubCrewMovementComponent` peer-crew grid smoothing

`OnRep_GridSpaceTransform` currently applies the replicated value directly and only
logs.

After refactor — only on SimProxy (peer), not on the locally-controlled AutonomousProxy
crew:

```cpp
void USubCrewMovementComponent::OnRep_GridSpaceTransform()
{
    if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
    {
        // Push replicated value into a grid playback buffer; the rebase in
        // TickComponent will sample the buffer at WorldNow - GridPlaybackDelaySeconds.
        QueueGridSnapshot(GridSpaceTransform);
    }
    // ... existing logging ...
}
```

Add a parallel grid-snapshot buffer on `USubCrewMovementComponent`, sized small (e.g.,
4 entries), with the same delay-buffered playback structure as Phase 1. Rebase in
`TickComponent` uses the buffered grid pose on SimProxy, the locally-computed grid pose
on AutonomousProxy and Authority. The locally-controlled crew sees zero change.

### 4.6 Replicated input visual decoupling

`RudderInput`, `DivePlaneInput`, `ThrustInput` continue to replicate as-is. Visual
consumers (rudder mesh rotation, dive-plane mesh, thrust gauge) read the **smoothed**
values from the new getters. BP graphs that read these via `GetRudderInput()` etc.
must switch to `GetPresentedRudderInput()` etc.

`USubMovementComponent::GetRudderInput()` and friends can be marked deprecated or kept
returning the raw value for code that genuinely needs the authoritative input (e.g.,
helm widget displaying current target).

### 4.7 `ASubmarineBase::RefreshRepState`

Untouched. Authority-only. The presentation state is a CLIENT-SIDE construct.

### 4.8 `UCompartmentVolumeComponent`

Untouched in this phase. The component's world transform is computed from
`GetComponentTransform()`, which is `OwnerActor.Transform * RelativeTransform`. With the
sub Owner.Transform now being the smoothed presentation pose (set by
`ApplyClientPlaybackPose`), the volume transform inherits the smoothing without code
change.

If audit hypothesis 5.5 (cached component transform) proves real, that becomes a Tier-3
follow-up after this refactor lands.

---

## 5. Migration Path

Phase A — extend without breaking:

1. Add `FClientPresentationState`, getters, derivation. Authority path returns the
   existing instantaneous fields via the getters.
2. Build, run all 5 stable scenarios. Confirm no regression.

Phase B — migrate consumers:

3. `USubInteriorFrameComponent` reads from getters. Remove the SimFrame-gated
   acceleration logic and the no-SubMov fallback.
4. Build, run all 5 stable scenarios + the 3 jitter scenarios. Capture before/after
   logs.

Phase C — peer-crew grid smoothing:

5. Add grid-playback buffer to `USubCrewMovementComponent`. Wire `OnRep_GridSpaceTransform`
   on SimProxy only.
6. Build, run scenario C specifically. Confirm scenario C resolves.

Phase D — replicated input smoothing:

7. Add smoothed input fields to presentation state. Update BP rudder / dive-plane mesh
   bindings (or expose new BlueprintPure getters and document the migration).
8. Build, run helm-only scenario. Confirm rudder visual is smooth.

Each phase is an atomic commit. Phase B alone may resolve the dominant scenario A and B
jitter; if so, Phases C and D can be assessed independently.

---

## 6. Logging Requirements

Add structured logs gated by existing toggles, no new toggle:

1. `Presentation sample` (per-render-frame, gated by `bLogSubInterpPacing`)
   - frame
   - presented location and rotation
   - presented linear velocity (cm/s magnitude)
   - presented linear acceleration (cm/s² magnitude)
   - delta vs previous frame
2. `Grid replication received` (gated by `bLogCrewJitter`)
   - role
   - new grid local delta
   - buffer size
3. `Grid playback sample` (gated by `bLogCrewJitter`)
   - role
   - render time
   - segment duration
   - alpha
   - rebased world position

These are diagnostic, not always-on. Production builds run with all toggles off.

---

## 7. Execution Order

Strict order; each step builds and tests:

1. Read this doc, the audit, and the companion Phase 1 plan.
2. Add `FClientPresentationState` struct + getters + derivation in
   `USubMovementComponent.h/.cpp`. Authority path returns instantaneous fallback.
3. Build. Run all 5 stable scenarios. Commit if green.
4. Migrate `USubInteriorFrameComponent` to read from getters. Remove fallback and
   sim-frame gating.
5. Build. Run all 5 stable + 3 jitter scenarios. Capture log.
6. If scenario A or B resolves: commit. If not: capture log and audit before continuing.
7. Add grid-playback buffer to `USubCrewMovementComponent`. Wire `OnRep_GridSpaceTransform`
   on SimProxy.
8. Build. Run scenario C specifically. Capture log.
9. Commit if scenario C resolves.
10. Add replicated-input smoothing in presentation state.
11. Build. Run helm-only. Confirm rudder mesh visual smooth.
12. Commit.

Each commit must be independently revertable.

---

## 8. Acceptance Criteria

The refactor is accepted only if all of the following hold in PIE:

1. The five working scenarios continue to work without regression.
2. Scenario A (breach symmetric jitter) is resolved or measurably reduced.
3. Scenario B (stair while sub moves, large local spikes) is resolved.
4. Scenario C (peer J2 walking, asymmetric jitter) is resolved.
5. The `Presentation sample` log shows a presented linear velocity that varies
   smoothly across frames with magnitude consistent with authority sub speed (within ±10%
   instead of the ±30% observed in the audit logs).
6. Helm input feels responsive — no perceptible input-to-rudder lag introduced by the
   smoothing.
7. EVA hull crossing still hands off correctly between Embarked and Outside.

If any of 1–7 fails, the offending phase commit is reverted and re-audited before
continuing.

---

## 9. Risks and Rollback Strategy

### 9.1 Risk: Phase B introduces velocity discontinuities at frame boundaries

If `(Location - Prev.Location) / DeltaTime` produces noisy velocity (e.g., when
DeltaTime is small or zero on a hitched frame), the InteriorFrame inertia signal becomes
spiky and Camera Sway / IK jerks.

Mitigation: clamp DeltaTime ≥ 1e-3 in derivation. Apply a single-pole low-pass filter
(time constant ≈ 50 ms) on `LinearVelocity` and `AngularVelocity` if pure
finite-difference proves too noisy.

Rollback: revert Phase B commit; InteriorFrame returns to reading
`SubMovement->Velocity` directly.

### 9.2 Risk: Phase C introduces visible peer-crew lag

Adding playback delay on grid pose introduces a 100 ms lag on peer-crew rendering. May
feel laggy when peers are doing fast actions. Tunable via grid playback delay.

Mitigation: separate grid playback delay (default 60–100 ms) from sub playback delay so
both can be tuned independently. Document the trade-off in the component header.

Rollback: revert Phase C commit; `OnRep_GridSpaceTransform` returns to direct apply.

### 9.3 Risk: Phase D requires BP changes to consume smoothed input getters

If BP rudder mesh logic is widespread, migrating consumers is a content-touching task.

Mitigation: Phase D is independent and can be deferred. Phases A–C deliver the bulk of
the visual improvement.

Rollback: revert Phase D commit.

### 9.4 Risk: Helm-only scenario regresses

Helm-only piloting means sub moves while crew is at a station. Crew rebase still runs
when embarked, so any change in InteriorFrame inertia signal could perturb camera or
station-bound visuals.

Mitigation: explicit helm-only test after Phase B. If regression, audit camera /
station bindings.

Rollback: revert offending phase.

---

## 10. Out Of Scope

- Authority-side simulation changes.
- `FSubmarineNetState` payload extensions.
- Rewriting `USubMovementComponent` as multiple `UActorComponent`s.
- Replacing `USubMovementComponent` with a parallel V2.
- Camera component replacement.
- IK system replacement.
- Asset-side stair → ramp replacement (existing TODO in CLAUDE.md, separate concern).

---

## 11. Files Touched

Expected:

- `Source/Sub3D/Submarine/SubMovementComponent.h`
- `Source/Sub3D/Submarine/SubMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubInteriorFrameComponent.h`
- `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`

Not touched in this phase:

- `Source/Sub3D/Submarine/SubmarineBase.h/.cpp`
- `Source/Sub3D/Submarine/CompartmentVolumeComponent.h/.cpp`
- `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h`
- Any BP asset, except optionally rudder/dive-plane mesh BP graphs in Phase D
  (deferred).

---

## 12. Acceptance Of This Plan

This document is correct only if executed phase-by-phase, each commit independently
revertable, with the 5 working scenarios tested between phases. If a phase fails its
acceptance criterion, the next session must revert that phase and re-audit before
proceeding to the next one.
