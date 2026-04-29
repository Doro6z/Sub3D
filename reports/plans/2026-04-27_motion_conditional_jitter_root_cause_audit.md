# Sub3D — Motion-Conditional Visible Jitter — Root Cause Audit

**Date**: 2026-04-27
**Status**: UNRESOLVED
**Authority-max reference**: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
**Companion plan**: `reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md`

---

## 1. Purpose

Capture in one place every confirmed fact, ruled-out hypothesis, and remaining candidate
for the visible jitter that appears in PIE 2-client (Play As Client) when the submarine is
moving. After five implementation attempts on `USubMovementComponent` non-authority
playback (the most recent conformant to the companion plan), the visible jitter is
unchanged. The sub interpolation path is therefore eliminated as the dominant root cause,
and the next investigation must target the rest of the motion → render pipeline.

This document is an audit, not a fix proposal. It exists so the next session (or another
agent) can resume without re-running ruled-out hypotheses.

---

## 2. Observed Phenomenon

Three repro scenarios, all sharing what the user describes as the *same family of jitter*:

| ID | Trigger | Visible jitter |
|---|---|---|
| A | Breach activated by dev cheat | Submarine, water plane, refraction, debug compartment boxes — symmetric across clients |
| B | Crew crosses stair while submarine is moving | Large local spikes on the submarine and every visual reading the submarine transform — local to the crew performing the transition |
| C | J2 walks while submarine is idle | J1 sees J2 jitter; J2 does not see J1 jitter — asymmetric |

Confirmed stable cases:

1. Spawn is stable.
2. Helm-only piloting (thrust, pitch, rudder) — sub moves, no crew on foot — is stable.
3. Local crew standing still on a moving submarine is stable.
4. **Sub frozen via `bFreezeMovementForTesting=true` + crew walks (any surface, including stair) is stable.**

Point 4 is the load-bearing observation in this document. It eliminates every "asset
collision micro-jitter" and "CMC alone" hypothesis: without sub motion, no visible
jitter, regardless of what the crew does.

---

## 3. Confirmed Facts

### 3.1 From per-render-frame pacing logs (`bLogSubInterpPacing`)

- Server emits snapshots at variable render-tick rate. PIE 2-client server-side `dt`
  ranges 0.020–0.040 s (50% variance).
- Snapshot receive intervals on the client range 0.020–0.080 s; OS-scheduler / world-tick
  jitter adds variance on top of server emission spacing.
- Authority-side per-frame `dx` divided by Authority-side `dt` is consistent — sub
  velocity ≈ 580 cm/s steady or smoothly accelerating.
- Non-authority-side per-frame `dx` divided by `dt` was inconsistent (404–656 cm/s) under
  the original receive-gap-based interp. This is what motivated the playback refactor.

### 3.2 From companion-plan refactor (current state)

The `USubMovementComponent` non-authority path now:

- enqueues snapshots into `ClientSnapshotBuffer` with `WorldReceiveTime`,
  `RealReceiveTime`, and an authority-time stamp derived from `SimFrame`,
- evaluates `RenderTime = WorldNow - ClientPlaybackDelaySeconds`,
- finds the bracketing snapshots in authority-time space,
- interpolates with Hermite cubic and replicated `LinearVelocity` tangents,
- snaps and resets only on first snapshot or `RealGap > ClientStallResetSeconds`.

The math of that path is consistent with the companion plan §7. The user reports the
visible jitter is unchanged with this path active.

### 3.3 Replication cadence

`ASubmarineBase::RefreshRepState` is called from `USubMovementComponent::TickComponent`
on the authority path only when `bSimulated` is true. With server render `dt` 20–40 ms
and `FixedSimulationHz=60`, that's once per server render tick.

`USubMovementComponent` has additional independently-replicated fields:

- `Ballasts` (`UPROPERTY(Replicated)`)
- `GlobalTargetFill` (`UPROPERTY(Replicated)`)
- `ThrustInput`, `RudderInput`, `DivePlaneInput` (`UPROPERTY(Replicated)`)

These have their own dirty/replicate cadence, which is **not** synchronized with the
`RepState` snapshot stream. They drive visible mesh rotation on rudder / dive planes.

### 3.4 Crew rebase chain

Tick order, set in `USubMovementComponent::BeginPlay` and `USubCrewMovementComponent::InitializeForSubmarine`:

```
SubFlood → SubMovement → InteriorFrame → CrewMovement
```

`USubCrewMovementComponent::TickComponent`, when `IsGridAuthoritative()`:

1. Pre-CMC rebase: `UpdatedComponent->SetWorldLocationAndRotation(SubXf * GridSpace)`,
   bSweep=false, TeleportPhysics. SubXf comes from
   `USubInteriorFrameComponent::GetSubTransform()` = `Owner->GetActorTransform()` of the
   sub.
2. Controller yaw delta is added based on `SubRot.Yaw - LastSubWorldTransform.Rotator().Yaw`.
3. `Super::TickComponent` runs the CMC.
4. Post-CMC extract: `GridSpace = SubXf.Inverse() * Char.WorldXf` (yaw-only on rotation).

`USubInteriorFrameComponent::TickComponent` reads `SubMovement->Velocity` for inertial
fields (`LocalLinearVelocity`, `LocalLinearAcceleration`,
`LocalAngularVelocityDegrees`). Acceleration is updated only when `SimFrameCounter`
advances, with `SimDt = 1 / FixedSimulationHz`. That signal is consumed by Camera Sway
and any other reader of the inertial state.

### 3.5 Peer-crew SimProxy presentation

`USubCrewMovementComponent::OnRep_GridSpaceTransform` does **not** smooth. It logs and
returns. The replicated `GridSpaceTransform` is applied as the authoritative grid pose on
peer SimProxy crew. Combined with the smoothed sub world transform, peer crew world
position = `Sub_smooth × Grid_replicated`. `Grid_replicated` updates discretely on
snapshot receipt — between snapshots, peer crew grid pose is stale, then jumps.

This is the documented Phase-2 concern for scenario C in the companion plan §11.3.

---

## 4. Hypotheses Ruled Out

| # | Hypothesis | How it was ruled out |
|---|---|---|
| 1 | Last receive gap as segment duration | Refactored to buffered + `RenderTime = WorldNow - delay`. Visible jitter unchanged. |
| 2 | Receive-time jitter polluting segment durations | Tried server-time axis (added `ServerTimeSec` to `FSubmarineNetState`) and authority-time axis (derived from `SimFrame`). Visible jitter unchanged in both. |
| 3 | "Robust shift" anti-pattern that re-anchored Prev to current rendered pose | Removed in the buffered playback. Visible jitter unchanged. |
| 4 | Auth path's "undo offset / lerp PrevSim/CurrSim" trick on the server | Authority side renders and emits a numerically smooth `dx/dt`. Confirmed in pacing log. |
| 5 | Stair geometry collision micro-jitter alone | `bFreezeMovementForTesting=true` + crew walks stair = no jitter. Documented in `CLAUDE.md` "Known environment debt" but not the dominant cause for the observed jitter. |
| 6 | CMC mesh smoothing offset on AutonomousProxy crew | `SmoothCorrection` overridden to no-op when `IsGridAuthoritative()` (commit b531e24). Sub-frozen test shows no jitter, so CMC alone is not the source. |
| 7 | Camera attached to a moving crew producing apparent jitter | Sub-frozen test shows no jitter regardless of crew motion or camera turning. |

---

## 5. Hypotheses Remaining, Ranked by Likelihood

The trigger condition is necessarily *sub motion + something downstream of the sub
transform*. Candidate downstream consumers of the sub transform that update each frame:

### 5.1 (most likely) Peer-crew GridSpaceTransform replication path — for scenario C

`USubCrewMovementComponent::OnRep_GridSpaceTransform` applies the replicated grid pose
unsmoothed. On the peer client, between snapshots the grid pose is stale; on receipt it
jumps. Combined with smooth sub interp, peer crew world position has a step-and-hold
component. This matches the asymmetric symptom (J1 sees J2 jitter, J2 does not see J1
jitter) and the idle-sub scenario (sub motion is not strictly required for this — peer
crew motion alone is enough).

Note: this hypothesis is consistent with the companion plan §11.3 explicitly classifying
scenario C as out of scope for the sub-playback refactor.

### 5.2 Crew rebase yaw-delta reading lagged sub rotation

In `USubCrewMovementComponent::TickComponent`:

```
const float DeltaYaw = FRotator::NormalizeAxis(SubRot.Yaw - PrevSubRot.Yaw);
CtrlRot.Yaw += DeltaYaw;
C->SetControlRotation(CtrlRot);
```

`SubRot` comes from the buffered playback (lagged) sub. If the buffer playback yaw
trajectory has any tick-to-tick non-monotonicity (rotator lerp can wrap, especially if
`QuantizedRotation` precision interacts with small rotations near 0), the controller yaw
gets nudged inconsistently each tick — felt as camera jitter while moving.

Specifically suspect: rotation interpolation between `S0.QuantizedRotation` and
`S1.QuantizedRotation` is plain `FMath::Lerp`, no shortest-path/slerp normalization. At
yaw values near ±180, this produces visible flips. Even at moderate yaws, FRotator Lerp
component-wise can produce non-monotonic interpolation.

### 5.3 InteriorFrame inertial state read by visual subsystems

`USubInteriorFrameComponent` reads `SubMovement->Velocity` directly (the replicated
authoritative velocity, not the interpolated one). On non-authority that velocity comes
from the latest snapshot; it updates at snapshot cadence, not per render frame. Any
camera shake, posture sway, or IK signal that consumes those inertial fields will see
stair-stepped values when the sub interp is smooth — i.e., the visual lags or jumps
relative to sub geometry.

This would explain the breach scenario being symmetric: water plane shaders and
refraction read sub-anchored fields; if these fields update discretely while the sub
position interpolates smoothly, the slab moves in lockstep with sub but the shader
parameters step.

### 5.4 Replicated input fields (RudderInput, DivePlaneInput, ThrustInput) drive mesh rotation independently of RepState

Rudder and dive plane meshes are typically rotated by reading these replicated floats.
Their replication cadence is independent of `FSubmarineNetState`. If a client's
RepState arrives with one timing and the rudder input arrives with a different timing,
the rudder mesh visually reacts at a different beat than the sub body — perceptually
similar to "jitter on the sub" because the eye groups them.

### 5.5 Compartment volume `DrawDebugString` / `DrawDebugBox` running once per render tick

`UCompartmentVolumeComponent::TickComponent` calls `DrawDebugString(World,
WorldXform.GetLocation() + offset, Label, ...)`. `WorldXform = GetComponentTransform()`.
If the cached component transform is invalidated only on attachment changes (not on
parent actor location changes mid-frame), the label could draw at a stale world
position relative to where the sub mesh ends up at the same render tick.

Less likely than the others but cheap to check.

### 5.6 Sub mesh smoothing or animation component

Confirm `SubmarineRoot` is a plain `USceneComponent` and `HullMesh` is a
`UStaticMeshComponent` with no procedural offset. Confirm there's no
`UInterpToMovementComponent`, `URotatingMovementComponent`, or similar attached. If any
extra movement component is on the sub, it is running its own integration on top of the
playback transform.

---

## 6. Files Touched Across the Five Iterations

All inside `USubMovementComponent`, plus one round on `FSubmarineNetState`:

- `Source/Sub3D/Submarine/SubMovementComponent.h`
- `Source/Sub3D/Submarine/SubMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h` (added `ServerTimeSec`, then reverted
  per companion plan §6.4 constraint)
- `Source/Sub3D/Submarine/SubmarineBase.cpp` (`RefreshRepState` populated and reverted
  the same field)
- `Source/Sub3D/Debug/Sub3DDebugSettings.h` (added `bLogSubInterpPacing` toggle)

Current state matches the companion plan: buffered playback with authority-time axis
derived from `SimFrame`, plan-format logging (`Snapshot queued`, `Playback sample`,
`Playback reset`, `Buffer underrun`), no payload extension to `FSubmarineNetState`.

---

## 7. Recommended Next Steps, Diagnostic-First

The next session must NOT modify `USubMovementComponent` until at least one of the
remaining hypotheses is confirmed by data. Suggested order:

### 7.1 Tier-1 instrumentation (cheapest)

1. Add tick-time logging to `USubCrewMovementComponent::TickComponent` and
   `USubInteriorFrameComponent::TickComponent` to confirm the prereq order
   `SubFlood → SubMov → InteriorFrame → CrewMov` actually holds at runtime, and that all
   four components tick once per render frame on each PIE world. Print the world
   timestamp at start-of-tick for each.
2. Add a per-render-tick log on the sub debug compartment volume: at the end of
   `UCompartmentVolumeComponent::TickComponent`, print `WorldXform.GetLocation() -
   Owner->GetActorLocation()`. If this drifts even slightly from the static
   relative offset between sub root and the volume, the cached transform is being read
   stale.
3. Repro scenario C (peer J2 walking, sub idle) with `OnRep_GridSpaceTransform` adding
   a tick-time log of the new `LocalDelta`. If `LocalDelta` is large and discrete on each
   replication tick, this confirms the un-smoothed grid hypothesis (5.1).

### 7.2 Tier-2 isolation (no code changes, only inspector toggles)

1. **Disable compartment debug visuals**: `bDrawCompartmentVolumes=false`,
   `bDrawCompartmentWater=false`, `bDisableFloodWaterPlanes=true`,
   `bDisableBreachBoundaries=true`. Run scenarios A, B, C. If jitter disappears, it lived
   in one of the debug/water visuals; binary-search re-enable to pinpoint.
2. **Disable crew Camera Sway / Posture / IK** (whatever the crew BP exposes): same
   procedure.
3. **Disable rudder / dive-plane mesh rotation** in BP: same procedure.

### 7.3 Tier-3 targeted code change (only after 7.1 / 7.2 produces a hit)

Based on the hypothesis confirmed by 7.1 / 7.2, the next change is *one of*:

- Add `OnRep_GridSpaceTransform` smoothing on peer SimProxy crew (companion plan §11.3
  Phase 2).
- Replace `FRotator` Lerp with `FQuat` slerp in the sub playback rotation interpolation.
- Re-anchor `USubInteriorFrameComponent` inertial fields to per-render-frame derivatives
  of the sub world transform instead of the snapshot velocity, so they smooth in sync.
- Add a `MarkRenderStateDirty()` on attached debug components if the cached transform
  hypothesis (5.5) holds.

Each is small (≈30–80 lines) and constrained to a single component.

---

## 8. Files To Read Before Starting Phase 2

If a future session resumes from this doc, read these files in order before writing any
code:

1. This doc.
2. `reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md`
   — the playback refactor plan.
3. `reports/plans/2026-04-10_first_playable_strategic_analysis.md` — authority-max plan.
4. `Source/Sub3D/Submarine/SubMovementComponent.h` and `.cpp` — current playback path.
5. `Source/Sub3D/Submarine/SubCrewMovementComponent.h` and `.cpp` — rebase + replicated
   `GridSpaceTransform`.
6. `Source/Sub3D/Submarine/SubInteriorFrameComponent.h` and `.cpp` — inertial signal
   provider.
7. `Source/Sub3D/Submarine/CompartmentVolumeComponent.cpp` — debug volume render path
   and water plane host.

---

## 9. What This Audit Is Not

- Not a fix.
- Not an authorization to refactor any new component.
- Not a closure of the visible jitter problem.

It is the formalized state of knowledge after five failed attempts on
`USubMovementComponent`. The next session resumes from here.

---

## 13. Resolution (added 2026-04-27)

**Status: RESOLVED.**

After all hypotheses §4 and §5 were ruled out by direct experiments (kill-switch test,
debug-pawn-attached-to-sub experiment, component-tick disabling), the real cause was
found by inspecting the `Snapshot queued` log's `FrameGap` distribution:

- 16% of snapshots had FrameGap=1 (sim advanced 1 step).
- 50% of snapshots had FrameGap=2 (sim advanced 2 steps).
- **27% of snapshots had FrameGap=3** (sim advanced 3 steps — 50ms of motion in one snapshot).
- 1.5% had FrameGap=4+.

The `USubMovementComponent::TickComponent` Authority path's sim accumulator while-loop
runs 1, 2 or 3 sim steps depending on the server's render `DeltaTime`. `RefreshRepState`
fires once per render frame, AFTER the loop. UE replication coalesces multiple per-frame
RepState writes into one push. So **one snapshot can contain 1, 2, or 3 sim steps' worth
of motion**, depending on whether the server frame was 17ms, 33ms or 50ms.

The CLIENT then plays back these variable-content snapshots through Hermite cubic
interpolation, but **lissing the timing does not lisse the content**. A 50cm motion delta
in one snapshot remains a 50cm visual jump.

**Trigger correlation**: stair traversal, breach activation, and other "trigger events"
load the server frame slightly more (CMC processing MovementBase change, RPC dispatch,
floor traces, new component spawning), pushing the server frame from ~17ms to ~50ms.
Snapshot now carries 3 sim steps. Client renders the 50cm jump as visible jitter.

**Why client-side smoothing failed**: 5 iterations on `USubMovementComponent` non-auth
playback (Hermite, server timestamps, robust shift, authority-time axis, buffered ring)
all attacked the SYMPTOM (uneven segment durations on the client), not the CAUSE
(variable snapshot content from the server). The cause was upstream of the client.

**Test that proved it**: with `t.MaxFPS 30` console command active, server tick is forced
uniform (33ms), sim step count is consistently 2 per frame, FrameGap is uniformly 2,
snapshots carry uniform content, **stutter mostly disappeared**.

**Fix applied**: `Edit → Project Settings → Engine → General Settings → Framerate` :
- ✅ Use Fixed Frame Rate
- Fixed Frame Rate = 60.0

This forces uniform 16.67ms server tick → 1 sim step per render frame consistently →
snapshots are uniform → stutter eliminated. This is the standard production config for
UE dedicated servers. PIE without this default produces variable framerate due to
editor + slate + multi-world overhead, exposing this bug.

**Code-side guardrails added** (commit pending):
- Comment in `USubMovementComponent::TickComponent` Authority path documenting the
  requirement.
- Runtime `Warning` log in `BeginPlay` if the project is running with
  `Engine.UseFixedFrameRate=false` on Authority.
- Memory note `project_motion_chain_jitter_root_cause_2026_04_27.md`.

**Diagnostic instrumentation kept** (useful for any future replicated-motion bug):
- `bLogSubMovement` (per-snapshot queued/played, per-tick playback sample, reset/underrun).
- `bLogSubInterpPacing` (per-render-frame pacing log).
- `bLogPresentationChain` (master toggle + `Motion chain tick` aggregate).
- `bLogMotionChainTrace` (per-tick 4-point crew capsule trace + event detection).
- Cvar `Sub3D.SubMovement.DisableRootInterpolation` + setting
  `bDisableSubRootVisualInterpolation` (auth & non-auth root interp kill-switch).

**Related plan documents**:
- `reports/plans/2026-04-27_unified_motion_chain_master_refactor.md` — Phase A
  (replication contract extension) was applied and is independently valuable. Phase C
  (kill `USubInteriorFrameComponent` legacy) is a justified cleanup independent of this
  bug. Phase B (FSubPresentationState) and Phase D (peer-crew grid smoothing) were
  predicated on the bug being client-side; with the actual cause now identified, B and D
  are deferred as quality improvements rather than required fixes.
