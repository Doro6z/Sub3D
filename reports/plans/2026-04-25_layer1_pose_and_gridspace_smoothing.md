# Layer 1 pose source + GridSpaceTransform smoothing — jitter resolution

**Date** : 2026-04-25
**Status** : Implemented (S1 + S2). S3 = asset work, deferred.
**Context** : Residual jitter under breach activation, ladder transit, and peer movement, despite the prior fixes (Hermite client interp, NetUpdateFrequency 60Hz, robust shift, real-time gap snap).

## Symptoms

| # | Trigger | Visible jitter | Asymmetric |
|---|---|---|---|
| A | Breach activated by `DevCheat_CreateBreach` | Sub jitters on all clients ; water/refraction visually unstable | symmetric |
| B | Crew transits the ladder/stair while sub is moving | Sub jitters locally → unplayable spikes | local to crew transit |
| C | J2 walks (no breach, sub idle) | J1 sees J2 jitter ; J2 sees J1 fluid | **asymmetric** |

Conditions that reproduce **fluid** behaviour:
- Spawn → fluid
- Helm alone (thrust/pitch/rudder) → fluid
- Local crew static on moving sub → fluid

Conditions that reproduce **jitter**:
- Mass change in motion (breach)
- MovementBase change while sub moves (stair)
- Peer crew motion (J2 walks → J1 sees jitter)

## Root cause inventory

The crew side is **clean** : 23/23 `IsGridAuthoritative()` guards properly neutralize all pre-LGA paths (`UpdateBasedMovement`, `UpdateBasedRotation`, `SmoothCorrection`, `ServerCheckClientError`, floor recovery). `ApplyYawCompensation`, crew tether, and `WorldSnap` are deleted. No legacy code is firing.

The jitter is **two distinct problems** that share an amplifier:

### Cause #1 — Layer 1 visual interp writes lerped pose on the server actor

[SubMovementComponent.cpp:235-241](../Source/Sub3D/Submarine/SubMovementComponent.cpp:235) on the AUTHORITY path:

```cpp
const float Alpha = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
const FVector InterpLocation = FMath::Lerp(PrevSimLocation, CurrSimLocation, Alpha);
Owner->SetActorLocationAndRotation(InterpLocation, ...);  // lerped pose written to actor
```

After Layer 1 runs, the sub's `GetActorLocation()` returns the **lerped** pose, not the authoritative `CurrSim` pose. The interior frame caches this when its tick runs after SubMovement (per prereq), and the crew rebase reads this lerped pose via `Frame->GetSubTransform()`.

For 1 substep per frame at steady frame rate, lerp alpha is consistent and the pose offset is constant → no perceptible artefact.

For **2+ substeps per frame** (frame rate < sim rate, which happens under breach load) :
- `PrevSim` is captured BEFORE each substep, so after the loop `PrevSim = pose-before-LAST-substep`
- `CurrSim = pose-after-all-substeps`
- The lerp covers ONLY the last substep visually
- Earlier substeps are "compressed" into a single visible step
- Frame-to-frame variance in alpha → visible jitter on whatever reads the actor pose

Everything that reads `Sub->GetActorTransform()` on the authority side is affected: `InteriorFrame::GetSubTransform()`, the crew rebase, attached components (debug volumes, water plane positioning, breach boundaries).

### Cause #2 — `GridSpaceTransform` replication is not smoothed for SimulatedProxy peers

[SubCrewMovementComponent.h:60-61](../Source/Sub3D/Submarine/SubCrewMovementComponent.h:60) :

```cpp
UPROPERTY(BlueprintReadOnly, Replicated, Category = "...")
FTransform GridSpaceTransform = FTransform::Identity;
```

Plain `Replicated`. No `ReplicatedUsing`. No callback. The value updates silently when a new replication packet arrives.

On a SimulatedProxy peer (J2 character on J1 client) :
- The server replicates `GridSpaceTransform` at NetUpdateFrequency
- Updates arrive in discrete steps
- The crew's rebase tick uses the current (just-received) value directly
- Result : the peer's world position steps stepwise on each replication update

When the peer moves locally (J2 walks), J2's `GridSpaceTransform` updates each AutoProxy client tick. The server stores it (via `MoveAutonomous`) and replicates to J1. J1 sees a sequence of stepwise positions on the peer.

This is **inherently asymmetric** : the autonomous proxy on its owning client moves smoothly via local CMC, while the simulated proxy on the other client steps through replicated values without smoothing.

### Amplifier — server frame rate variance

The two causes above are mostly invisible at steady server frame rate. They become visible when frame rate is variable:

- **Breach** : `USubFloodComponent::AdvanceFlooding` becomes more expensive as more compartments have inflow ; spawning `USubHullBoundaryComponent` ; water plane material refraction sampling. Server tick time spikes → frame rate dips → 2+ substeps per frame → Cause #1 visible.
- **Ladder** : CMC `FindFloor` on the stair's complex collision (CLAUDE.md known asset debt) → CPU spike during transit → frame rate dips → Cause #1 visible.
- **Peer movement** : autoproxy's CMC processing on the owning client + replication burst on the peer client → Cause #2 visible.

## Fixes

### S1 — Crew rebase reads authoritative pose, not lerped pose

The Layer 1 visual interp is preserved (the sub's MESH still smoothes at variable render rates) but the rebase queries the authoritative `CurrSim` pose instead of the actor's current (lerped) pose.

Implementation :
1. Add `USubMovementComponent::GetAuthoritativeTransform()` returning a transform built from `CurrSimLocation` / `CurrSimRotation` (already cached members).
2. On the authority path, `USubInteriorFrameComponent::GetSubTransform()` returns `SubMov->GetAuthoritativeTransform()` if available, falls back to `Owner->GetActorTransform()` otherwise.
3. On non-authority, `GetSubTransform()` returns `Owner->GetActorTransform()` (which IS the Hermite-smoothed pose) — unchanged.

Effect : the crew rebase, all `WorldToLocal`/`LocalToWorld` calls via the InteriorFrame, and any consumer of `GetSubTransform()` reads a stable authoritative value on the server. The mesh visual lerp is unaffected (cosmetic only on the server local view).

### S2 — Smooth `GridSpaceTransform` on SimulatedProxy peer

The replicated `GridSpaceTransform` becomes `ReplicatedUsing = OnRep_GridSpaceTransform`. The OnRep captures the previous "rendered" transform as the new lerp anchor, records the receive time, and the rebase step on a SimulatedProxy uses a smoothed value lerped over the receive interval.

Implementation :
1. Switch the UPROPERTY to `ReplicatedUsing = OnRep_GridSpaceTransform`.
2. Add `RenderedGridSpaceTransform`, `PrevRenderedGridSpaceTransform`, `GridSpaceLastReplicatedTime`, `GridSpacePrevReplicatedTime` private members.
3. Add `OnRep_GridSpaceTransform()` that captures the previous rendered as new prev anchor and records the receive timestamp.
4. Add `GetEffectiveGridSpaceTransform()` that returns the smoothed value on SimulatedProxy and the raw `GridSpaceTransform` on Authority/AutonomousProxy.
5. The rebase uses `GetEffectiveGridSpaceTransform()` instead of `GridSpaceTransform` directly.

Effect : SimulatedProxy peer crew lerps between received GridSpaceTransform values rather than stepping. Owning client and authority paths unchanged.

### S3 — Stairs → Ramps (asset work, NOT in this commit)

CLAUDE.md known debt. `SM_Stair_*` meshes use complex collision. Replace with ramp meshes (single planar slope, simple collision). Until done, the ladder transit will continue to cause CPU spikes during `FindFloor`, amplifying any residual Cause #1 jitter.

This is authoring work, not code. Tracked in CLAUDE.md.

## Validation procedure

After applying S1 + S2, validate :

1. **Test A (breach)** : `DevCheat_CreateBreach MainDeckID 50000`. Sub should sink smoothly without spikes on all clients.
2. **Test B (ladder)** : crew transits the stair while sub is moving. Sub-side jitter should be reduced (residual from S3 only).
3. **Test C (peer movement)** : J2 walks, J1 watches J2. J1 should see smooth motion.
4. **Test D (asymmetry)** : reciprocal — J1 walks, J2 watches J1. Should also be smooth.
5. **Validation thrust alone** : sub thrust/pitch alone should remain smooth (no regression on the working baseline).

Add `stat unit` and `bLogSubMovement = true` on PIE if any residual jitter persists, capture log around breach event for post-mortem.

## Architecture summary post-fix

The 3-layer smoothing pipeline becomes :

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Layer 1 — Authority sub-frame visual interp                            │
│  Lerp(PrevSim, CurrSim) on the actor pose for render smoothness when    │
│  render rate ≠ sim rate. Cosmetic only — does NOT contaminate the       │
│  rebase pipeline because S1 routes the rebase to CurrSim directly.      │
└─────────────────────────────────────────────────────────────────────────┘
                              ↓ (replication via FSubmarineNetState)
┌─────────────────────────────────────────────────────────────────────────┐
│  Layer 2 — Client snapshot interp (Hermite cubic + real-time gap snap)  │
│  Hermite cubic between PrevSnap and TargetSnap with LinearVelocity      │
│  tangents. C¹ continuous. Real-time-based throttle/focus-loss snap.     │
└─────────────────────────────────────────────────────────────────────────┘
                              ↓ (rebase)
┌─────────────────────────────────────────────────────────────────────────┐
│  Layer 3 — Local Grid Authority + GridSpaceTransform smoothing (S2)     │
│  - Authority + AutoProxy : compute GridSpaceTransform locally per tick. │
│  - SimulatedProxy peer : OnRep captures, lerp toward latest received    │
│    over the inter-replication interval. Eliminates stepwise peer motion │
│    when a peer is walking on the sub.                                   │
└─────────────────────────────────────────────────────────────────────────┘
                              +
┌─────────────────────────────────────────────────────────────────────────┐
│  CMC mesh smoothing (override)                                          │
│  SmoothCorrection no-op while grid-authoritative — prevents             │
│  MeshTranslationOffset oscillation against the rebase.                  │
└─────────────────────────────────────────────────────────────────────────┘
```

## Lessons recorded

- A "smoothing layer" that writes the actor pose silently contaminates everyone reading the actor transform. If the smoothing is meant to be cosmetic only, it must apply to the visual mesh component, not the actor root, OR the read path must explicitly query the authoritative pose.
- Replicated state used for per-tick rendering needs explicit smoothing if the receive cadence differs from the consumer tick rate. `Replicated` without `ReplicatedUsing` + smoothing logic produces stepwise visuals.
- The asymmetric jitter (J1 sees J2 stepping, J2 doesn't see J1) was a strong diagnostic signal pointing at SimProxy-only paths. Always check what differs between owning client and peer client for asymmetric symptoms.
