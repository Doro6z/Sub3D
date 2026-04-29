# Sub3D - Non-Authority Sub Playback Refactor And Audit

**Date**: 2026-04-27  
**Authority-max reference**: `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`  
**Scope**: targeted runtime refactor of the non-authority playback path in `USubMovementComponent` only

---

## 1. Purpose

This document formalizes the targeted refactor of the non-authority submarine playback path in:

- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubMovementComponent.h`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubMovementComponent.cpp`

This document also audits whether that refactor resolves the currently observed jitter cases.

This document does **not** authorize:

- a full rewrite of `USubMovementComponent`
- a split into several Unreal `ActorComponent`s
- any change to the authoritative simulation path
- any change to flood simulation, ballast simulation, or crew movement logic in this phase

---

## 2. Observed Problems

Three scenarios currently produce a visually similar jitter:

1. **Scenario A**
   Trigger: breach activated by dev cheat  
   Visible jitter: submarine on all clients, water plane, refraction, debug boxes  
   Symmetry: symmetric

2. **Scenario B**
   Trigger: crew crosses the stair while the submarine is moving  
   Visible jitter: large local spikes on the submarine and all visuals reading the submarine transform  
   Symmetry: local to the crew performing the transition

3. **Scenario C**
   Trigger: J2 walks while the submarine is idle and no breach is active  
   Visible jitter: J1 sees J2 jitter, J2 does not see J1 jitter  
   Symmetry: asymmetric

Confirmed stable cases:

1. Spawn is stable
2. Helm only, including thrust, pitch, and rudder, is stable
3. Local crew standing still on a moving submarine is stable

---

## 3. Findings Already Proven

The latest movement logs prove the following:

1. The non-authority submarine playback currently uses the last receive gap as the interpolation segment duration.
2. The non-authority playback frequently reaches `Alpha=1.00` and then waits for the next snapshot.
3. Snapshot spacing is variable in PIE. The current log shows repeated `dt` values around `0.018s`, `0.035s`, `0.040s`, and `0.045s`.
4. Snapshot step distance varies accordingly. The current log shows repeated `StepCm` values near `12.50`, `25.00`, and `37.50`.
5. Rendered submarine movement inherits that variability. The current log shows repeated `RenderStepCm` values near `11-21cm`, with larger jumps when a short segment follows a larger snapshot delta.
6. In the stair repro, the crew can remain locally stable while the world still jitters:
   - `RelDeltaCm=0.00`
   - `Mode=Walking`
   - `Falling=0`
   - large `ClientPlayback` render steps continue in the same frames
7. `Base changed` during the stair repro is often valid:
   - `Accepted=1`
   - `FloorWalkable=1`
   - `FloorDist=2.15`
   - `Falling=0`

This proves that the dominant issue in the current stair and breach logs is the **rendered submarine transform on non-authority clients**, not a fundamental loss of walking support.

---

## 4. Root Cause Classification

**Likely root cause**

- Non-authority submarine playback is tied to raw snapshot receive cadence instead of a fixed playback time.

**Possible contributor**

- PIE server frame pacing and replication cadence are irregular, which amplifies the problem.

**Deferred concern**

- Remote crew smoothing in local-grid space is still a separate concern for scenario C.

---

## 5. Refactor Goal

Refactor the non-authority path so the submarine is rendered at:

- `RenderTime = WorldNow - PlaybackDelay`

instead of:

- `RenderTime = function(last receive gap)`

The rendered submarine must no longer depend directly on the timing of the latest received packet.

---

## 6. Hard Constraints

1. `USubMovementComponent` remains the single owner of the rendered submarine transform.
2. No additional Unreal `ActorComponent` is introduced in this phase.
3. The authoritative simulation path remains unchanged.
4. The replicated `FSubmarineNetState` payload remains unchanged in this phase.
5. Crew, water plane, refraction, and debug visuals continue to read the same rendered submarine transform as before.

---

## 7. Target Internal Design

### 7.1 New Buffered Snapshot State

Add an internal buffered snapshot type in `USubMovementComponent`:

```cpp
struct FBufferedClientSubSnapshot
{
	FSubmarineNetState State;
	double WorldReceiveTime = 0.0;
	double RealReceiveTime = 0.0;
};
```

Add these members:

```cpp
TArray<FBufferedClientSubSnapshot, TInlineAllocator<8>> ClientSnapshotBuffer;
double ClientPlaybackDelaySeconds = 0.10;
double ClientMaxBufferHistorySeconds = 0.50;
double ClientStallResetSeconds = 0.20;
```

The current two-snapshot model must stop being the source of truth for playback:

- `ClientPrevSnapshot`
- `ClientTargetSnapshot`
- `ClientPrevSnapshotTime`
- `ClientTargetSnapshotTime`

They can be removed or retained only as temporary migration scaffolding during implementation. They must not remain the active playback model after the refactor is complete.

### 7.2 New Internal Responsibilities

Split the current non-authority logic into these private methods inside `USubMovementComponent`:

1. `QueueClientSnapshot`
   - Input: `const FSubmarineNetState& NewState`
   - Responsibility: append the snapshot to the client buffer with world and real receive times
   - Responsibility: update non-positional sampled state such as velocity, yaw rate, pitch rate, depth, flooded mass, and forward speed
   - Must not write the rendered transform

2. `ResetClientPlayback`
   - Responsibility: clear the buffer and snap once to a known valid pose
   - Allowed only on:
     - first snapshot
     - real stall greater than `ClientStallResetSeconds`
     - invalid or severely out-of-order snapshot sequence

3. `EvaluateClientPlaybackPose`
   - Input: `double WorldNow`
   - Responsibility: compute the rendered submarine pose at `WorldNow - ClientPlaybackDelaySeconds`
   - Responsibility: find the two buffered snapshots that surround the target render time
   - Responsibility: evaluate translation and rotation for that render time

4. `ApplyClientPlaybackPose`
   - Responsibility: apply the evaluated pose to the non-authority actor
   - Must not contain receive or buffer management logic

### 7.3 Playback Evaluation Rules

For each non-authority tick:

1. Compute `RenderTime = WorldNow - ClientPlaybackDelaySeconds`
2. Remove snapshots older than `RenderTime - ClientMaxBufferHistorySeconds`
3. Find buffered snapshot `A` and `B` such that:
   - `A.WorldReceiveTime <= RenderTime <= B.WorldReceiveTime`
4. Compute:
   - `Alpha = (RenderTime - A.WorldReceiveTime) / (B.WorldReceiveTime - A.WorldReceiveTime)`
5. Evaluate translation using Hermite:
   - `P0 = A.State.WorldLocation`
   - `P1 = B.State.WorldLocation`
   - `T0 = A.State.LinearVelocity * SegmentDuration`
   - `T1 = B.State.LinearVelocity * SegmentDuration`
6. Evaluate rotation with the current linear rotation interpolation in this phase
7. Apply the resulting transform

### 7.4 Strict Rules

These current behaviors must stop:

1. The playback segment duration must not come from the last receive gap.
2. `ClientPrevSnapshot.WorldLocation = Owner->GetActorLocation()` must not be used in normal packet processing.
3. The client must not spend long periods visually pinned at `Alpha=1.00` while waiting for the next packet, except during true buffer underrun.
4. The non-authority path must not write the actor transform during snapshot enqueue except during an explicit reset.

---

## 8. Logging Requirements

Keep logging focused and runtime-meaningful:

1. `Snapshot queued`
   - frame
   - snapshot location
   - queue size
   - world gap
   - real gap

2. `Playback sample`
   - render location before and after
   - snapshot A and B
   - alpha
   - segment duration
   - render step distance
   - segment distance

3. `Playback reset`
   - reason
   - real gap
   - frame

4. `Buffer underrun`
   - queue size
   - render time
   - newest snapshot time

The current `Snapshot received` and `ClientPlayback` logs can be adapted to this structure instead of being duplicated.

---

## 9. Execution Order

1. Add the buffered client snapshot state to `USubMovementComponent`
2. Add `QueueClientSnapshot`
3. Add `ResetClientPlayback`
4. Add `EvaluateClientPlaybackPose`
5. Add `ApplyClientPlaybackPose`
6. Replace the current non-authority tick path with the buffered playback path
7. Remove the old two-snapshot playback logic as the active path
8. Rebuild
9. Re-run the current jitter repro matrix

---

## 10. Acceptance Criteria

The refactor is accepted only if all of the following are true in PIE:

1. The submarine no longer alternates between visible freeze and visible catch-up during normal movement.
2. `ClientPlayback` movement becomes more regular than `Snapshot received` movement.
3. `RenderStepCm` no longer regularly reaches full snapshot step distance on normal frames.
4. Stair traversal while the submarine is moving no longer produces the current large local submarine spikes.
5. Breach activation no longer produces visible jitter on the submarine, water plane, refraction, and debug boxes due to non-authority submarine playback.

---

## 11. Audit Against The Observed Scenarios

### 11.1 Scenario A - Breach Activated By Dev Cheat

**Expected result after this refactor**: high confidence resolved

Reason:

1. The observed breach jitter is symmetric.
2. Water plane, refraction, and debug boxes all read the submarine transform.
3. The current logs already prove that non-authority submarine playback is stepping in visible chunks.

If the rendered submarine transform is stabilized, this scenario should stop showing the current family of visual jitter.

### 11.2 Scenario B - Crew Crosses The Stair While The Submarine Moves

**Expected result after this refactor**: high confidence on the dominant symptom, not a full guarantee on every residual issue

Reason:

1. Current logs prove the crew can remain locally stable while the world still jitters because the submarine render transform is stepping.
2. Current `Base changed` logs remain valid and do not prove floor loss in the repro segment.
3. The stair acts as a visual amplifier and timing trigger, not as the dominant root cause in the captured log.

Residual risk:

- If a smaller residual jitter remains after this refactor, that residual should be audited separately as a stair support or crew smoothing issue.

### 11.3 Scenario C - J2 Walks While Submarine Is Idle, J1 Sees J2 Jitter

**Expected result after this refactor**: not resolved by scope

Reason:

1. In this scenario the submarine is idle.
2. A non-authority submarine playback fix cannot explain an asymmetric peer-only crew jitter while the submarine itself is not moving.
3. This scenario belongs to the remote crew presentation path, not to the submarine playback path.

This requires a second targeted runtime change after the submarine playback refactor:

- buffered or explicit smoothing for non-owner crew local-grid presentation

### 11.4 Working Cases

This refactor must preserve:

1. stable spawn
2. stable helm-only movement
3. stable local static crew on a moving submarine

---

## 12. Audit Result

This refactor is **necessary** and **correctly scoped** for the dominant submarine playback problem.

This refactor is **not sufficient** to claim closure of all observed jitter problems.

Specifically:

1. It should resolve scenario A.
2. It should resolve the dominant submarine-driven part of scenario B.
3. It should not be presented as a fix for scenario C.

Therefore the correct interpretation is:

- **Phase 1**: refactor non-authority submarine playback in `USubMovementComponent`
- **Phase 2**: audit and refactor remote crew non-owner smoothing for local-grid presentation

This document is correct only if it is executed with that scope discipline.
