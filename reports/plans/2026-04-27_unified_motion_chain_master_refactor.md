# Sub3D — Unified Motion Chain Master Refactor

**Date**: 2026-04-27
**Status**: PROPOSED, awaiting approval
**Authority-max reference**: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
**Predecessor docs (read these first)**:

1. `reports/plans/2026-04-27_motion_conditional_jitter_root_cause_audit.md`
2. `reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md`
3. `reports/plans/2026-04-21_local_grid_space_authority_architecture.md`
4. Memory notes: `feedback_avoid_surengineering`, `feedback_legacy_after_refactor`, `feedback_iteration_pace`, `project_embarked_refactor_rolled_back_2026_04_20`

---

## 0. TL;DR

Three fundamentals are wrong in the current architecture:

1. **The client samples sub motion from multiple sources at different cadences.** Sub
   pose is smoothed (Phase 1 buffered playback). Inertia is read from raw replicated
   velocity. Replicated inputs (rudder/dive-plane/thrust) replicate independently.
   Peer-crew grid pose is unsmoothed. The eye groups all of these as "sub jitter".
2. **`USubInteriorFrameComponent` is a middleman that adds no value.** It re-derives
   inertia from `SubMov->Velocity` (snapshot velocity), then crew copies it back into
   its own field. The data lives in SubMov; the derivation should too.
3. **Peer-crew `OnRep_GridSpaceTransform` applies the replicated grid pose immediately
   with zero smoothing.** This is the asymmetric scenario C jitter on its own.

The perfect refactor produces ONE coherent client presentation state, sampled at ONE
render-time anchor per frame, consumed by every downstream visual subsystem. It
absorbs the InteriorFrame middleman into `USubMovementComponent`. It applies the same
buffered-playback discipline to peer-crew grid pose. It consolidates replicated input
streams into the existing `FSubmarineNetState` to eliminate independent-replication
beat mismatch.

This is a 4-phase refactor, ~7-12 hours of disciplined work, each phase atomic and
revertable, with an explicit cleanup ticket queued at the end.

---

## 1. Lessons From Past Failures

These are the dataset. The plan below is constructed to *not* re-create them.

### 1.1 Five iterations on SubMov non-auth playback alone produced no visible change

Iterations attempted (chronologically, this session and prior):
1. Hermite cubic with linear-velocity tangents on a single Prev/Target pair.
2. NetUpdateFrequency raised 30→60Hz + robust-shift Prev anchoring on current pose.
3. SmoothCorrection no-op in grid mode (kept — necessary).
4. Buffered ring with receive-time-based segments.
5. Buffered ring with server-time-axis (added then reverted `ServerTimeSec` field).
6. Plan-conformant buffered playback with authority-time axis derived from `SimFrame`.

→ **Lesson**: the visible jitter is not in the math of the sub interp itself. The
fix must address every consumer that reads sub motion, not just the sub's own
transform.

### 1.2 Two embarked architectures rolled back (2026-04-20)

- Stock CMC + MovementBase produced jitter (replication delta carried by CMC's
  base-relative transform fought with sub interp).
- Custom bypass tunneling crew through floors when sub moved fast.

→ **Lesson**: don't fight CMC. The current LGA (rebase pre-CMC, extract post-CMC,
disable base-rotation carry, no MovementBase transport) is the third attempt and the
one that works for static-on-moving-sub. Preserve it. Build on it.

### 1.3 InteriorFrame is dead architecture

Originally introduced to provide a stable "passenger frame" abstraction for any
consumer. In practice today it is only:
- A passthrough to `Owner->GetActorTransform()`.
- A re-derivation of inertia from `SubMov->Velocity`.

→ **Lesson**: dead layers must die. Hunt them BEFORE the refactor (memory note
`feedback_legacy_after_refactor`).

### 1.4 Replication independence creates beat mismatch

`USubMovementComponent` has THREE separate `UPROPERTY(Replicated)` floats
(`ThrustInput`, `RudderInput`, `DivePlaneInput`) that drive visual mesh rotation
INDEPENDENTLY from `FSubmarineNetState`. Three independent replication beats means
visual rudder reacts at one timing, sub body at another. Eye reads it as jitter on
the assembly.

→ **Lesson**: every motion-coupled visual signal must replicate IN the same payload
that carries the pose, so they sample together.

### 1.5 Buffered playback alone solves nothing if downstream readers are unsmoothed

Phase 1 buffered the sub pose but left InteriorFrame inertia and peer-crew grid pose
both unsmoothed. The presentation chain stayed jittery overall.

→ **Lesson**: smoothing must reach the LAST visual consumer, not just the first.

### 1.6 Plan-first discipline saves cycles

The companion plan dated 2026-04-27 produced a clean refactor that conformed to
spec, even though it didn't kill the visible jitter. The spec was clear, scope was
bounded, acceptance criteria were explicit. Same discipline applies here.

→ **Lesson**: write the plan, get alignment, only then code. No iteration coding
without plan approval.

### 1.7 Memory: prefer ship coherent defaults + iterate in PIE

`feedback_iteration_pace`: ship sane numbers, tune in PIE, mark all tunables
EditAnywhere. Applies here to playback delays, smoothing time constants, buffer
sizes.

### 1.8 Memory: accept stock UE flaws + work around

`feedback_avoid_surengineering`: don't rebuild what UE provides if a workaround
suffices. Applies here: keep `UCharacterMovementComponent` as substrate, keep math
sub physics, keep replication subsystem; only change the contracts at the boundaries
of the components we own.

### 1.9 In/out-of-sub determination is already deterministic via hull boundaries

The crew environment axis (`reports/plans/2026-04-22_crew_environment_axis.md`) already
implements a deterministic boundary model:

- `USubHullBoundaryComponent` placed at airlock + spawned at breaches detects capsule
  crossings.
- `ASubCrewCharacter::HandleHullCrossing` flips `EmbarkState`
  (`Embarked` ↔ `Outside`) on crossing events.
- `IsGridAuthoritative()` = `EmbarkState == Embarked`.

→ **Lesson**: the "passenger frame" abstraction `USubInteriorFrameComponent` was
introduced to answer "where is this crew relative to the sub?" — a question the
boundary model already answers, atomically and deterministically. InteriorFrame can
be deleted; the source of truth for in/out is the hull boundary system.

### 1.10 Working features must be preserved bit-exact

The user reports the current build delivers these features WORKING and they must NOT
regress through this refactor:

- Helm controls (thrust, rudder, dive plane) — input feel and authority response.
- Sub sinks under flood load (gravity + buoyancy + flood mass).
- Sub takes damage via breach activation (dev cheat).
- Crew exits sub via airlock (EVA hull crossing).
- Crew exits sub via breach (EVA hull crossing).
- Crew changes deck via stair traversal (stair micro-jitter is a documented asset
  issue, not addressed here).
- Replicated rudder/dive-plane mesh visuals match server input.

→ **Lesson**: this refactor changes ONLY the client presentation chain. Authority
sim, flood, breach, EVA boundary, and helm all stay byte-equivalent on the server
side.

---

## 2. Final Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  AUTHORITY  (server only)                                       │
│                                                                 │
│  USubMovementComponent.SimulateStep@60Hz                        │
│    → writes Owner.Transform                                     │
│    → updates Velocity, AngVel, AngAccel                         │
│    → reads ThrustInput / RudderInput / DivePlaneInput           │
│                                                                 │
│  ASubmarineBase.RefreshRepState (once per server tick)          │
│    → captures Pose, Velocity, AngVel, AngAccel, Inputs,         │
│      DepthMeters, FloodedMassKg, Ballast, SimFrame              │
│    → replicates as FSubmarineNetState                           │
└──────────────────────────────┬──────────────────────────────────┘
                               │  OnRep_RepState
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  CLIENT PRESENTATION  (non-authority only, every render frame)  │
│                                                                 │
│  USubMovementComponent.HandleReplicatedNetState                 │
│    → QueueClientSnapshot (existing Phase 1)                     │
│                                                                 │
│  USubMovementComponent.TickComponent (non-auth path)            │
│    → EvaluateClientPlaybackPose                                 │
│    → Build FSubPresentationState:                               │
│        Pose         ← interpolated from buffer (Phase 1)        │
│        LinearVel    ← interpolated from buffer (snap-replicated)│
│        AngularVel   ← interpolated from buffer                  │
│        LinearAccel  ← interpolated from buffer                  │
│        AngularAccel ← interpolated from buffer (NEW)            │
│        Inputs       ← interpolated + lightly low-passed         │
│    → Apply pose to Owner.Transform                              │
│    → Cache state for getters                                    │
└──────────────────────────────┬──────────────────────────────────┘
                               │  Single source of truth via getters
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  CONSUMERS                                                      │
│                                                                 │
│  USubCrewMovementComponent (rebase + extract)                   │
│    → SubXf  ← SubMov->GetPresentedTransform()                   │
│    → Inertia ← SubMov->GetPresented*() (replaces InteriorFrame  │
│      middleman; fields LocalSubXxx populated directly here)     │
│    → SimProxy peer: smoothed grid playback (NEW)                │
│                                                                 │
│  USubCrewAnimInstance.NativeUpdateAnimation                     │
│    → reads CMC->LocalSubXxx (unchanged interface)               │
│                                                                 │
│  ASubCrewCharacter (Camera Sway)                                │
│    → reads CMC->LocalSubXxx (unchanged interface)               │
│                                                                 │
│  Mesh BPs (rudder, dive-plane, thrust visual)                   │
│    → read SubMov->GetPresentedRudderInput etc.                  │
│                                                                 │
│  UCompartmentVolumeComponent debug visuals                      │
│    → inherit Owner.Transform smoothing automatically            │
│                                                                 │
│  USubInteriorFrameComponent                                     │
│    → DROPPED FROM ACTIVE CHAIN. Stub remains for BP             │
│      compatibility; deletion is a cleanup ticket.               │
└─────────────────────────────────────────────────────────────────┘
```

Single-source-of-truth principle: any client subsystem that needs sub motion calls a
getter on `USubMovementComponent`. The getter returns the value AT THE CURRENT RENDER
FRAME, which is identical for all consumers within that frame.

---

## 3. Hard Invariants

The refactor MUST preserve these through every phase:

| # | Invariant | Why |
|---|---|---|
| 1 | Authority simulation untouched | Past sim changes broke physics ergonomics |
| 2 | LGA architecture preserved (rebase pre-CMC, extract post-CMC, no MovementBase transport) | Two prior architectures rolled back |
| 3 | Tick prereq order: SubFlood → SubMov → CrewMov | InteriorFrame removed from chain (no functional impact) |
| 4 | No new `UActorComponent` introduced | Avoids unnecessary BP wiring expansion |
| 5 | `BP_Submarine_Craniata` re-save IS authorized as part of Phase C (InteriorFrame component removed from the actor's class layout) | Deterministic in/out lives in `USubHullBoundaryComponent`; InteriorFrame is fully deleted in this refactor (§1.9) |
| 6 | Working features (§1.10) remain green: spawn, helm-only piloting, local crew static on moving sub, frozen sub + crew walking any surface, EVA hull crossing via airlock, EVA via breach, stair deck-change traversal, sub sinks under flood load, breach damage activation, replicated mesh visuals (rudder, dive plane) match server input | Re-tested between every phase |
| 7 | Crew on AutonomousProxy of own pawn keeps zero perceived input lag | Smoothing applies only on SimProxy peers and visual-only signals |
| 8 | Helm responsiveness unchanged on the helmsman's view | Input smoothing time constant ≤ 30 ms, applied to visual mesh only — not to the sim itself |

---

## 4. Replication Contract Changes

`FSubmarineNetState` extension (necessary, justified, bandwidth-trivial):

| Field | Current | Refactored | Rationale |
|---|---|---|---|
| WorldLocation | yes | yes | unchanged |
| QuantizedRotation | yes | yes | unchanged |
| LinearVelocity | yes | yes | unchanged |
| AngularVelocity | yes | yes | unchanged |
| **AngularAccelerationDeg** | NO | **YES (NetQuantize10)** | InteriorFrame's role goes here |
| **LinearAccel** | NO | **YES (NetQuantize10)** | Same |
| ForwardSpeed, VerticalSpeed | yes | yes | unchanged |
| DepthMeters, FloodedMassKg, BallastGlobal01, MainTrim01, bPumpActive | yes | yes | unchanged |
| SimFrame | yes | yes | unchanged |
| **RudderInput** | separate `UPROPERTY(Replicated)` on SubMov | **MOVED INTO NetState** | Beat coherence |
| **DivePlaneInput** | separate | **MOVED INTO NetState** | Beat coherence |
| **ThrustInput** | separate | **MOVED INTO NetState** | Beat coherence |

Bandwidth delta: +3 NetQuantize10 vectors (+ ~30 bytes) + 3 floats consolidated (no net
change, just relocated). Net cost ≈ 30 bytes per snapshot × 30 snapshots/sec × 16
players (max from project_gameplay_vision_2026_04_01) = ~14.4 KB/s. Trivial.

The three separate `Replicated` fields on `USubMovementComponent` are removed. Their
setters still exist (server-side authoritative writes), they just write into the field
that gets folded into `RepState` next snapshot.

---

## 5. Component Design (Final Contracts)

### 5.1 `USubMovementComponent`

New private state:

```cpp
struct FSubPresentationState
{
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector LinearVelocity = FVector::ZeroVector;
    FVector AngularVelocityDeg = FVector::ZeroVector;
    FVector LinearAcceleration = FVector::ZeroVector;
    FVector AngularAccelerationDeg = FVector::ZeroVector;
    float RudderInputSmoothed = 0.f;
    float DivePlaneInputSmoothed = 0.f;
    float ThrustInputSmoothed = 0.f;
    double SampleWorldTime = 0.0;
    bool bIsValid = false;
};
FSubPresentationState ClientPresentation;
```

Tunables (UPROPERTY EditAnywhere on the component, defaults coherent per memory
`feedback_iteration_pace`):

```cpp
double ClientPlaybackDelaySeconds       = 0.10;   // Phase 1 (existing)
double ClientMaxBufferHistorySeconds    = 0.50;   // Phase 1 (existing)
double ClientStallResetSeconds          = 0.20;   // Phase 1 (existing)
float  ClientInputVisualSmoothingHz     = 30.f;   // FInterpTo speed for input visual
```

Public getters (read by all consumers):

```cpp
const FVector& GetPresentedLocation() const;
const FRotator& GetPresentedRotation() const;
FTransform GetPresentedTransform() const;          // composed pose
const FVector& GetPresentedLinearVelocity() const;
const FVector& GetPresentedAngularVelocityDeg() const;
const FVector& GetPresentedLinearAcceleration() const;
const FVector& GetPresentedAngularAccelerationDeg() const;
float GetPresentedRudderInput() const;
float GetPresentedDivePlaneInput() const;
float GetPresentedThrustInput() const;
```

Authority-side fallbacks: getters return the existing instantaneous fields
(`Velocity`, `RudderInput`, etc.) when called on Authority, so server-side code that
already reads via the new getters continues to work.

Per non-authority tick computation:

1. `EvaluateClientPlaybackPose(WorldNow, Sample)` — Phase 1 logic.
2. Interpolate replicated kinematic fields between bracketing snapshots using the same
   alpha — this gives the vel/accel state that BELONGS to the rendered pose, not the
   latest snapshot.
3. Smooth input visuals: `Rudder = FInterpTo(prev, snapshotRudder, dt, kHz)` etc.
4. Fill `ClientPresentation`.
5. `ApplyClientPlaybackPose(Pose)` writes Owner.Transform.

### 5.2 `USubCrewMovementComponent`

Inertial state migration: replace `Frame->GetLocalLinearVelocity()` etc. with
`SubMov->GetPresentedLinearVelocity()` etc., then transform into local sub frame
inside CMC. The `LocalSubXxx` fields on CMC remain unchanged in name and type — only
their populator changes. Animation and Camera Sway code paths are unaffected.

Peer-crew grid playback (NEW, structural):

```cpp
struct FBufferedPeerGridSnapshot
{
    FTransform GridPose;
    double WorldReceiveTime = 0.0;
    double RealReceiveTime = 0.0;
};
TArray<FBufferedPeerGridSnapshot, TInlineAllocator<6>> PeerGridBuffer;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Playback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
double PeerGridPlaybackDelaySeconds = 0.06;       // shorter than sub: peer crew should feel responsive
```

`OnRep_GridSpaceTransform` on SimProxy enqueues. AutonomousProxy and Authority paths
unchanged. The rebase in `TickComponent`, when `IsGridAuthoritative()` and crew is
`SimulatedProxy`, uses `EvaluatePeerGridSample(WorldNow)` to read a smoothed grid
pose; otherwise uses the locally-computed grid as today.

`UpdateInertialState` becomes:

```cpp
const USubMovementComponent* SubMov = ...;
LocalSubLinearVelocity        = ActorXf.InverseTransformVectorNoScale(SubMov->GetPresentedLinearVelocity());
LocalSubLinearAcceleration    = ActorXf.InverseTransformVectorNoScale(SubMov->GetPresentedLinearAcceleration());
LocalSubAngularVelocityDegrees = ActorXf.InverseTransformVectorNoScale(SubMov->GetPresentedAngularVelocityDeg());
LocalSubAngularAccelerationDegrees = ActorXf.InverseTransformVectorNoScale(SubMov->GetPresentedAngularAccelerationDeg());
```

`USubCrewMovementComponent::GetInteriorFrame()` and the InteriorFrame tick prereq are
removed. The function is preserved as a stub returning nullptr ONLY if the BP still
references it; otherwise deleted.

### 5.3 `USubInteriorFrameComponent` — DELETED in Phase C

The component is fully removed in Phase C. Source of truth for in/out is the hull
boundary system (§1.9). Source of truth for inertia is `USubMovementComponent`
presentation state (§5.1).

Phase C deletion sequence (one commit):

1. Migrate every caller off InteriorFrame:
   - `USubCrewMovementComponent` reads inertia from `SubMov->GetPresentedXxx`,
     transform from `SubMov->GetPresentedTransform()`.
   - `ASubCrewCharacter:307,312` `Sub->InteriorFrame->GetSubTransform()` →
     `Sub->SubMovement->GetPresentedTransform()`.
   - `USubRelativeFrameExpectedTransformProvider` (diagnostic) reads SubMov
     directly.
2. Remove `InteriorFrame` UPROPERTY from `ASubmarineBase`.
3. Remove `bLogInteriorFrame` and `bDrawInteriorFrame` from `Sub3DDebugSettings`.
4. Delete `Source/Sub3D/Submarine/SubInteriorFrameComponent.h/.cpp`.
5. Re-save `BP_Submarine_Craniata` (open + save in editor — clears the now-orphan
   component reference).

No InteriorFrame stub kept. The class is gone.

### 5.4 `ASubmarineBase`

`RefreshRepState` extended to populate the new fields:

```cpp
RepState.LinearVelocity            = SubMovement->Velocity;
RepState.AngularVelocity           = FVector(0.f, SubMovement->GetPitchRateDegPerSec(), SubMovement->GetYawRateDegPerSec());
RepState.LinearAcceleration        = SubMovement->GetAuthorityLinearAcceleration();   // NEW getter on SubMov
RepState.AngularAccelerationDeg    = SubMovement->GetAuthorityAngularAccelerationDeg();  // NEW
RepState.RudderInput               = SubMovement->GetRudderInputAuthoritative();
RepState.DivePlaneInput            = SubMovement->GetDivePlaneInputAuthoritative();
RepState.ThrustInput               = SubMovement->GetThrustInputAuthoritative();
```

`SubMovementComponent` exposes authoritative-side accessors for accel (computed in
`SimulateStep` from velocity delta) so RefreshRepState doesn't need to read internals.

### 5.5 `SubmarineRuntimeTypes.h`

`FSubmarineNetState` gains:

```cpp
UPROPERTY(BlueprintReadOnly, Category = "Net")
FVector_NetQuantize10 LinearAcceleration = FVector::ZeroVector;

UPROPERTY(BlueprintReadOnly, Category = "Net")
FVector_NetQuantize10 AngularAccelerationDeg = FVector::ZeroVector;

UPROPERTY(BlueprintReadOnly, Category = "Net")
float RudderInput = 0.f;

UPROPERTY(BlueprintReadOnly, Category = "Net")
float DivePlaneInput = 0.f;

UPROPERTY(BlueprintReadOnly, Category = "Net")
float ThrustInput = 0.f;
```

The three separate `UPROPERTY(Replicated)` floats on `USubMovementComponent` are
removed (and `GetLifetimeReplicatedProps` lines too).

---

## 6. Migration Phases

Each phase is one commit, atomic, revertable, with a dedicated test pass.

### Phase A — Replication contract extension

1. Add `LinearAcceleration`, `AngularAccelerationDeg`, `RudderInput`, `DivePlaneInput`,
   `ThrustInput` to `FSubmarineNetState`.
2. Add `GetAuthorityLinearAcceleration` / `GetAuthorityAngularAccelerationDeg` getters
   to `USubMovementComponent` (compute from Velocity delta in SimulateStep).
3. Update `RefreshRepState` to populate the new fields.
4. Remove `Replicated` from `RudderInput`/`DivePlaneInput`/`ThrustInput` UPROPERTYs on
   `USubMovementComponent`. Keep the field, only the replication is removed.
5. `Get*Input()` BlueprintPure helpers continue to return the field; on non-authority
   the field is now populated from the replicated `RepState` snapshot in
   `HandleReplicatedNetState`.

**Build, test the 5 stable scenarios. Confirm no regression. Commit.**

### Phase B — Presentation state in `USubMovementComponent`

1. Add `FSubPresentationState`, getters, derivation per §5.1.
2. Authority path: getters return instantaneous fields (fallback).
3. Non-auth path: derivation fills `ClientPresentation` from interpolated bracketing
   snapshots (same alpha as pose interpolation).

**Build, test 5 stable scenarios. Confirm no regression. Commit.**

### Phase C — Migrate consumers off InteriorFrame + DELETE the component

1. `USubCrewMovementComponent::UpdateInertialState` reads from
   `SubMov->GetPresentedXxx()`. All `Frame->*` calls removed; `GetInteriorFrame()`
   helper removed.
2. `USubCrewMovementComponent` calls to `Frame->GetSubTransform()` →
   `SubMov->GetPresentedTransform()`. Audit every site (search `Frame->`).
3. `USubCrewMovementComponent::InitializeForSubmarine` removes the InteriorFrame tick
   prereq.
4. `ASubCrewCharacter:307,312` `Sub->InteriorFrame->GetSubTransform()` →
   `Sub->SubMovement->GetPresentedTransform()`.
5. `USubRelativeFrameExpectedTransformProvider` (diagnostic, in `Source/Sub3D/Diagnostics/`)
   redirected to `SubMovement` directly.
6. `ASubmarineBase` removes the `InteriorFrame` UPROPERTY field and any constructor
   wiring of it.
7. `Sub3DDebugSettings` removes `bLogInteriorFrame`, `bDrawInteriorFrame`.
8. Delete `Source/Sub3D/Submarine/SubInteriorFrameComponent.h` and `.cpp`.
9. Open `BP_Submarine_Craniata` in editor, re-save (clears orphan component
   reference).

**Build, test all working features (§1.10) + the 3 jitter scenarios. Capture log.
Commit if A and B visibly improve and §1.10 features are intact.**

This phase is the largest single commit of the refactor and includes the asset
re-save. Revert is straightforward: `git revert` the commit, re-save BP again to
restore the InteriorFrame slot (or restore from version control directly).

### Phase D — Peer-crew grid smoothing

1. Add `FBufferedPeerGridSnapshot`, `PeerGridBuffer`,
   `PeerGridPlaybackDelaySeconds` to `USubCrewMovementComponent`.
2. `OnRep_GridSpaceTransform` on SimProxy enqueues into buffer.
3. `TickComponent` rebase, when SimProxy + IsGridAuthoritative, samples buffered grid.
4. AutonomousProxy and Authority paths unchanged.

**Build, test scenario C specifically + helm-only scenario. Commit if scenario C
resolves.**

### Phase E (optional, depends on Phase B/C results) — Input visual smoothing tightening

If Phase B/C resolves all jitter, Phase E may not be needed. If rudder visual still
jitters, tune `ClientInputVisualSmoothingHz` or apply heavier smoothing. No code
structure change beyond the tunable.

---

## 7. Logging Requirements

Single new toggle `bLogPresentationChain` in `Sub3DDebugSettings` (one toggle, four
log lines):

1. `Presentation built` — non-authority tick, Pose / LinearVel mag / LinearAccel mag /
   AngularVel mag / RudderInput.
2. `Presentation consumer read` — once per tick from CrewMov, the values it received.
3. `Peer grid sample` — SimProxy crew tick, render time / segment / alpha / world
   pose result.
4. `Peer grid queued` — `OnRep_GridSpaceTransform` SimProxy, queue size / receive
   gap.

All gated, OFF by default. Production build runs none of them.

---

## 8. Acceptance Criteria

The refactor is accepted only if all of these hold in PIE:

| # | Criterion | Test |
|---|---|---|
| 1 | All 5 stable scenarios still green | spawn, helm-only, static crew on moving sub, frozen sub + walk, EVA hull crossing |
| 2 | Scenario A (breach) jitter resolved or measurably reduced | Visual judgement + `Presentation built` log shows smooth progression |
| 3 | Scenario B (stair sub mvt) large local spikes resolved | Visual + log; residual stair micro-jitter is asset issue, separate ticket |
| 4 | Scenario C (peer J2 walk) resolved | J1 watching J2 walk = smooth |
| 5 | `Presentation built` LinearVelocity magnitude varies smoothly within ±10% of authority (down from ±30% in audit) | Log inspection |
| 6 | Helm input feels responsive on helmsman's view | Subjective + helm widget shows immediate target value (raw, not smoothed) |
| 7 | EVA hull crossing still hands off Embarked ↔ Outside correctly | Crew exits sub through airlock, returns through breach |
| 8 | No CMC base-rotation regression on stair walk | Crew yaw stays aligned with sub yaw during stair traversal |

---

## 9. Risk Register

| Risk | Likelihood | Mitigation |
|---|---|---|
| Phase A bandwidth bump objected to | Low | +30 B/snapshot is trivial; documented in §4 |
| Phase B interpolated velocity is noisier than snapshot velocity | Medium | Snapshot-replicated values are the AUTHORITY values at server emit; interpolating between two snapshot values for the SAME render time produces a smoother time-aligned signal than using the latest. If noisy, apply low-pass FInterpTo with kHz=30. |
| Phase C breaks the existing `GetSubTransform()` callers | Medium | Audit all callers (see §1.3 grep results) before commit. Most call SubInteriorFrame; redirect to SubMov. |
| Phase D introduces visible peer-crew lag | Low | 60 ms grid playback delay is half the sub playback delay; tunable; less than typical input-to-pixel latency anyway |
| Removing replicated input UPROPERTYs breaks an external BP reader | Medium | Search all BP graphs for `Get Rudder Input`, `Get Dive Plane Input`, `Get Thrust Input` before commit |
| Helm-widget reads RudderInput/etc. for the spool meter | Low | Helm widget reads `GetSpooledPower()` (different field). Other inputs read for visual feedback should be re-pointed at `GetPresented*Input()` when that's the visual purpose. |
| Phase B regresses authority-side server because getters now have a virtual call | Negligible | Inline getters; auth path returns instantaneous fields directly |
| InteriorFrame stub stays in the build but does nothing — dead-code smell | Low | Cleanup ticket queued explicitly (§11) |

---

## 10. Files Touched (Definitive List)

Phase A:
- `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h` — extend `FSubmarineNetState`
- `Source/Sub3D/Submarine/SubMovementComponent.h` — remove `Replicated` on
  RudderInput/DivePlaneInput/ThrustInput; add Authority accel getters
- `Source/Sub3D/Submarine/SubMovementComponent.cpp` — populate accel members in
  `SimulateStep`; rewire `Get*Input` to read from internal field; on non-auth, the
  field is set from `RepState` in `HandleReplicatedNetState`
- `Source/Sub3D/Submarine/SubmarineBase.cpp` — `RefreshRepState` populates new fields

Phase B:
- `Source/Sub3D/Submarine/SubMovementComponent.h` — add `FSubPresentationState`,
  getters, tunables
- `Source/Sub3D/Submarine/SubMovementComponent.cpp` — derivation in non-auth tick

Phase C (largest commit, includes asset diff):
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h` — remove `GetInteriorFrame()` decl
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp` — rewire `UpdateInertialState`,
  remove InteriorFrame tick prereq, all `Frame->` callers go to SubMov getters
- `Source/Sub3D/Submarine/SubCrewCharacter.cpp:307,312` — `Sub->InteriorFrame` →
  `Sub->SubMovement`
- `Source/Sub3D/Submarine/SubmarineBase.h/.cpp` — remove `InteriorFrame` UPROPERTY +
  constructor wiring
- `Source/Sub3D/Diagnostics/SubRelativeFrameExpectedTransformProvider.h/.cpp` — read
  SubMov directly
- `Source/Sub3D/Debug/Sub3DDebugSettings.h` — remove `bLogInteriorFrame`,
  `bDrawInteriorFrame`
- **DELETE**: `Source/Sub3D/Submarine/SubInteriorFrameComponent.h`
- **DELETE**: `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp`
- **ASSET DIFF**: `Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata.uasset` (re-save)

Phase D:
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h` — peer grid buffer + tunable
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp` — `OnRep_GridSpaceTransform`
  enqueue + `EvaluatePeerGridSample` + rebase fork on SimProxy

Logging:
- `Source/Sub3D/Debug/Sub3DDebugSettings.h` — `bLogPresentationChain` toggle

Untouched in this refactor:
- Any BP asset.
- `ASubGameMode`, `ASubPlayerController`.
- Helm widgets, debug panels.
- `USubFloodComponent`.
- `ASubDoorActor`.
- Anything in `Plugins/`.

---

## 11. Cleanup — Folded Into Phase C

InteriorFrame deletion was originally proposed as a follow-up ticket. Per user
direction it is folded into Phase C (§6) so the refactor commits the legacy removal
in the same atomic change as its functional replacement. This avoids the
`feedback_legacy_after_refactor` failure mode (orphan legacy running silently while
the new path is being tested).

The single remaining post-acceptance task is documentation:

- Update memory note `feedback_legacy_after_refactor` with this InteriorFrame case
  study after acceptance.

---

## 12. Decisions — RESOLVED

All decisions resolved by the maintainer; recording here for traceability.

1. **Bandwidth (Phase A: +30 B/snapshot)** — **APPROVED**.
   At 30 snap/s × 16 players = ~14 KB/s server upload. Standard home upstream
   (5–50 Mbps = 625–6250 KB/s) absorbs this at ~0.2% of capacity. Local PIE: trivial.

2. **Rudder visual smoothing default** — **`ClientInputVisualSmoothingHz = 60.f`**.
   17 ms time constant. Below typical render-frame perceptual threshold; preserves
   the "sub marche parfaitement pour les inputs" feel. Tunable EditAnywhere; can
   be raised to 120 (8 ms) if any operator perceives lag.

3. **Peer grid playback delay** — **`PeerGridPlaybackDelaySeconds = 0.05`**.
   50 ms = ~1.5 server frames at 30 Hz nominal. At 300 cm/s walk speed, that's 15 cm
   peer position lag — well within visual tolerance for cooperative gameplay. Half
   the sub playback delay (100 ms). Tunable.

4. **InteriorFrame deletion** — **FOLDED INTO PHASE C**.
   Per maintainer direction (§1.9): in/out determination is owned by
   `USubHullBoundaryComponent`. InteriorFrame has no remaining responsibility once
   inertia derivation moves into SubMov. Deleting in the same commit as its
   functional replacement avoids the `feedback_legacy_after_refactor` orphan-legacy
   pitfall. BP `BP_Submarine_Craniata` re-save is authorized.

5. **Phase E (input visual smoothing tightening)** — **DEFERRED**.
   Activation decided after Phase B/C results. If rudder visual jitters after C, tune
   `ClientInputVisualSmoothingHz` only — no structural change.

---

## 13. Authority Of This Plan

This plan supersedes the earlier
`reports/plans/2026-04-27_unified_client_presentation_chain_refactor.md` (which was
the first draft of Path 2). The earlier doc remains in the repo for traceability;
this one is the executable spec.

If a phase fails its acceptance criterion, REVERT THAT PHASE'S COMMIT and re-audit
before continuing. No iteration coding without re-aligned spec.

The plan is correct only if executed phase-by-phase, each commit independently
revertable, with the 5 working scenarios tested between phases.
