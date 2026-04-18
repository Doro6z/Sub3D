# Submarine Physics Revision — 2026-04-17

Status: **implemented, awaiting build + PIE validation**. Intended to be
reviewed step-by-step with a second Agent before merging to `main`. Every
change is contained to the files listed in §8 so a reviewer can diff them
without grep-spelunking.

---

## 1. Intent

Five gameplay changes, one design constraint:

1. **Command state persists on the submarine.** Thrust, rudder, dive plane,
   ballast target, autopilot flags — all stay set after the pilot leaves
   the helm. Pilot B arriving at a running helm sees the same values
   pilot A left with.
2. **Thrust behaves as a throttle, not a punch.** 100% thrust = max
   acceleration and the sub stabilises toward its Vmax. Pitch modulates
   Vmax (+10% descending, −10% climbing) so gravity has felt influence.
3. **Pitch + rudder gain inertia.** Time constant doubled (0.5s → 1.0s
   approx) with steady-state rates preserved — same max output, twice as
   long to get there.
4. **Ballast effect on depth doubled.** The deviation-from-neutral is
   amplified by `BallastEffectScale = 2.0`. Neutral fill still produces
   zero net force. Pumping water is now a real depth control, not a nudge.
5. **Submarine hull collides with the Traversal Route.** The sub was
   phasing through route meshes because the movement sweep used the
   actor's `USceneComponent` root (no shape). Movement now sweeps through
   `ASubmarineBase::GetMovementCollisionComponent()` (`HullMesh` by
   default), which has a real collision shape.

Constraint: **BaseMass, MaxSpeed, MaxThrust are authored on the submarine**
(via `USubmarineDefinition`). Not hardcoded on the movement component.
Per-sub tuning. User preference "ca doit probablement être lié à
l'enveloppe", implemented as optional fields on `USubmarineDefinition`.

---

## 2. Command persistence (§1)

**Before:** [SubCrewCharacter.cpp](../../Source/Sub3D/Submarine/SubCrewCharacter.cpp):436 —
`Server_ReleaseHelm_Implementation` explicitly zeroed `SubMovement`
inputs and `Systems` helm commands on pilot exit.

**After:** Only `ClearPilot()` remains. Command state lives on
`USubmarineSystemsComponent::CommandState` (RepNotify-replicated, line
123 of [SubmarineSystemsComponent.h](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.h))
and flows into `USubMovementComponent` each sim step via
`ApplyCommandState` — unchanged. Since that struct is server-authoritative
and replicated, persistence is automatic once we stop clearing it.

**Caveats worth validating in PIE:**
- A disconnecting pilot: nothing changes here (the sub keeps running).
  That is the intended behaviour but it means a runaway sub if the
  single player quits. Out of scope for this pass.
- Autopilot targets (`TargetSpeedCmS`, `TargetDepthMeters`,
  `TargetPitchDeg`) already persist — they were never zeroed.

---

## 3. Throttle model & pitch-dependent Vmax (§2)

The existing model already behaves as a throttle: thrust produces force
`SpooledPower * MaxThrust`, and terminal speed is reached when quadratic
drag cancels thrust. That wasn't changed. The addition is a Vmax cap that
varies with pitch so the player feels gravity.

**Implementation** (ApplyPhysics, [SubMovementComponent.cpp](../../Source/Sub3D/Submarine/SubMovementComponent.cpp):~325):
```cpp
const float PitchFactor = clamp(1 - sin(pitch) * PitchVmaxInfluence,
                                1 - PitchVmaxInfluence,
                                1 + PitchVmaxInfluence);
EffectiveMaxForward = MaxForwardSpeed * PitchFactor;
EffectiveMaxReverse = MaxReverseSpeed * PitchFactor;
```

New param `PitchVmaxInfluence = 0.2` (clamped 0..0.5). At max pitch
(±30°), gives ±10% swing. Set to 0 to disable the coupling. Not adding
gravity-along-forward acceleration: preserves predictability, no risk of
runaway or stall. A physics-correct gravity coupling can be revisited
later if the feel is flat.

---

## 4. Pitch + rudder inertia (§3)

Both rudder and pitch use a first-order low-pass: `d(rate)/dt = Input -
rate * Damping`. Steady-state is `Input / Damping`, time constant is
`1 / Damping`.

To double τ without changing steady state, **halve both the input
coefficient and the damping**:

| Param | Before | After | Effect |
|---|---|---|---|
| `RudderTurnRate`          | 30.0 | 15.0 | input halved |
| `YawRateDamping`          | 2.0  | 1.0  | damping halved |
| `PitchFromHydroplaneAccel`| 25.0 | 12.5 | input halved |
| `PitchRateDamping`        | 2.2  | 1.1  | damping halved |
| `BallastTrimPitchRate`    | 8.0  | 4.0  | trim input halved (to match pitch damping halving) |

Full-input equilibrium unchanged (15/1 = 15°/s yaw, same as 30/2).
Time constant doubled (0.5s → 1.0s for yaw, ~0.45s → ~0.9s for pitch).
BG restoration (`PitchRestorationDamping = 3.0`) left untouched — it's a
spring-like term, not velocity damping, so the 2× τ doesn't compound on it.

---

## 5. Ballast effect (§4)

The existing model does ballast through `ComputeTotalMass()` — water
fill raises total mass, which shifts the `Buoyancy - Gravity` net
vertical force. At BaseMass=200t and Volume=10m³ per tank × 2 tanks,
full-fill vs empty gives a ~100 kN swing ≈ 5% of gravity. Not enough.

New param `BallastEffectScale = 2.0`. In `ComputeTotalMass`, the
water-mass **deviation from neutral** is multiplied by this scale.
Neutral fill still gives zero deviation, so neutral buoyancy is
preserved. Scale = 1.0 reproduces the old behaviour.

At scale 2.0, full-fill vs empty ≈ 200 kN ≈ 10% of gravity. Ballast now
competes with hydroplanes as a depth control. User explicitly asked for
iteration in PIE; this is the starting point.

---

## 6. Hull ↔ Traversal Route collision (§5)

**Root cause:** `USubMovementComponent::ApplyPhysics` called
`Owner->AddActorWorldOffset(Delta, /*bSweep=*/true, ...)`. The actor's
root is a `USceneComponent` (`SubmarineRoot`), and
`USceneComponent::MoveComponent` silently ignores the sweep flag — only
`UPrimitiveComponent::MoveComponent` does sweeps. Result: no sweep,
submarine passes through everything.

**Fix:** Replaced the `MoveWithSlide` lambda with a manual
`World->ComponentSweepMulti` against
`ASubmarineBase::GetMovementCollisionComponent()` (resolves to
`HullMesh` by default, can be overridden by subclasses via virtual).
Sweep happens against the actual hull shape at its current world
transform. On hit, the actor translates by the adjusted delta,
depenetrates, then slides along the plane. No change to the rotational
update or the horizontal/vertical split.

**Prerequisite for Craniata** (user action): `HullMesh` component in
`BP_Submarine_Craniata` must have `SM_Hull` assigned as its StaticMesh.
The convex-decomposition collision we added on `SM_Hull` earlier today
is what the sweep will use. If `HullMesh` is empty the sweep falls back
silently (bCanSweep=false), translate-without-sweep, same as before
this change. A log warning could be added in a later pass if we want to
flag the missing hull.

**Debug toggle:** `bDebugLogCollisionSweeps` already existed on the
component. It now logs each sweep hit with component + normal + hit time.

---

## 7. Authored performance profile (§1 constraint)

New fields on [USubmarineDefinition](../../Source/Sub3D/Submarine/Generator/SubmarineDefinition.h) (Category = Performance):

```cpp
float MaxForwardSpeedCmS  = 0.f;  // 0 = keep component default
float MaxReverseSpeedCmS  = 0.f;
float MaxVerticalSpeedCmS = 0.f;
float MaxThrustN          = 0.f;
```

`BaseMassKg` already existed (populated by the generator from envelope
volume). Now also copied onto `USubMovementComponent::BaseMass` at
BeginPlay. When mass changes, `InitializeNeutralBuoyancy()` is
re-invoked to adjust `SubmergedVolume` so the sub still floats neutral
at `NeutralBuoyancyFill01`.

New public method
`USubMovementComponent::ApplyPerformanceProfileFromDefinition(const
USubmarineDefinition*)`. Called from
`ASubmarineBase::BeginPlay` right after the Definition is resolved
(generator path or handmade path, same site).

**User action for Craniata:** open `DA_SubDef_Craniata` and set
`BaseMassKg`, `MaxForwardSpeedCmS`, etc. to the desired values. Zero
fields keep the movement component's fallback defaults.

---

## 7bis. Auto Depth Hold → zero-vertical-velocity controller

**Before:** `UpdateStabilization` drove ballast toward
`0.5 + (TargetDepthMeters - CurrentDepth) * AutoDepthBallastGain`. User
had to set a TargetDepthMeters and the sub homed onto that depth.

**After:** targets **zero vertical velocity** (`Movement->Velocity.Z = 0`),
not a depth. Wherever the sub happens to be, auto-depth holds it there:

```cpp
const float VerticalSpeedCmS = Movement->Velocity.Z;
const float NeutralFill = Movement->NeutralBuoyancyFill01;
const float TargetBallast = clamp(NeutralFill + VerticalSpeedCmS * AutoDepthVelocityGain, 0, 1);
CommandState.GlobalBallastTarget01 = FInterpTo(..., TargetBallast, dt, AutoDepthResponseRate);
```

New params on `USubmarineSystemsComponent`:

| Param | Default | Purpose |
|---|---|---|
| `AutoDepthVelocityGain` | 0.004 | Ballast offset per cm/s of vertical velocity |
| `AutoDepthResponseRate` | 2.0 | FInterpTo rate toward the commanded ballast |

Legacy `AutoDepthBallastGain` kept with `DeprecatedProperty` marker; no
longer read by the stabiliser.

**Feedback:** `GlobalBallastTarget01` is already replicated, so the
slider visibly moves on all clients while the controller adjusts. That
is the "slider obuge seul" the user asked for — no extra UI plumbing.

**Override / release:** already handled. `SetGlobalBallastTarget` and
`SetBallastTargetByIndex` call `NotifyManualInput(Depth)` → bumps
`DepthSuspendTimer = SuspendDurationSeconds (3s)` → `IsAutoDepthActive`
returns false for 3s after every slider touch. Release → timer drains
→ controller resumes. Zero code change, pattern already there.

---

## 7quater. Helm widget audit (Bug A, Bug B, Bug C)

Three independent issues surfaced during PIE testing, all fixed in this
pass.

### Bug A — "AUTO DEPTH HOLD checkbox does nothing"

Root cause: not the click chain (which works), but a UX dependency.
`IsAutoDepthActive()` requires `bStabilizationMasterEnabled` AND
`bAutoDepthEnabled` AND `bBallastsActive`
([SubmarineSystemsComponent.cpp:237-243](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:237)).
Default for the master flag was `false`, so toggling AutoDepth alone did
nothing — player had to enable MASTER ENABLED first.

**Fix:** [SubmarineRuntimeTypes.h:30](../../Source/Sub3D/Submarine/SubmarineRuntimeTypes.h:30) — default
`bStabilizationMasterEnabled = true`. Master becomes a kill switch
("OFF disables all auto") instead of a prerequisite. Per-axis toggles
now work standalone, which matches what the player expects.

### Bug B — "Spacebar (sonar ping) doesn't fire when widget has focus"

Root cause: zero `SetInputMode(...)` calls in the project. Default
behavior when a widget is shown: it captures keyboard focus and the
player's IA_SonarPing (and IA_Move, IA_Look, etc.) never fires.

**Fix:** [SubHelmWidget.cpp::NativeConstruct](../../Source/Sub3D/Submarine/SubHelmWidget.cpp:60) sets
`FInputModeGameAndUI` with `LockMouseToViewportBehavior=DoNotLock` and
`HideCursorDuringCapture=false`. Game inputs continue to flow while
mouse clicks reach UI. `NativeDestruct` restores `FInputModeGameOnly`
so the helm doesn't leak GameAndUI behavior into on-foot or other
station contexts.

### Bug C (smell, not user-reported) — RepNotify doesn't refresh widget

Verified: `CommandState` is `ReplicatedUsing=OnRep_CommandState` but the
widget polls every tick via `RefreshFromHelmData`. If the server changes
state while `bRefreshingFromRuntime` is gating the next tick, the UI
shows stale state for one frame. Not critical, no fix in this pass.
Flag for future cleanup if the player observes lag on auto-toggle
checkbox visual feedback.

---

## 7ter. Throttle / Rudder / Dive Plane ramp (Z / S / A / D / W / X)

**Before:** IA_Thrust / IA_Rudder / IA_DivePlane → `ServerRoute…(value)` →
absolute set. Key press = ±100% instantly. Unplayable for a sub sim.

**After:** Each axis gets a parallel ramp RPC + intent state +
configurable rate. Original absolute RPCs preserved for slider widgets.

| Axis | Ramp RPC | Intent setter | Rate param (default) |
|---|---|---|---|
| Throttle | `ServerRouteHelmThrottleRamp(Intent)` | `SetThrottleRampIntent` | `ThrottleRampRate = 0.35` (~2.9s 0→100%) |
| Rudder | `ServerRouteHelmRudderRamp(Intent)` | `SetRudderRampIntent` | `RudderRampRate = 0.6` (~1.7s 0→±1) |
| Dive plane | `ServerRouteHelmDivePlaneRamp(Intent)` | `SetDivePlaneRampIntent` | `DivePlaneRampRate = 0.6` |

Each `UpdateStabilization` tick walks
`Helm{Throttle,Yaw,Trim}Cmd` by `Intent * Rate * DeltaTime`, clamped
to ±1. Intent=0 freezes the ramp; current value persists (matches
"thrust must stay at 30%" behavior).

**Priority chain per axis:**

- **Throttle:** AutoSpeed wins → else ramp → else hold (current value).
- **Dive plane:** AutoPitch wins → else ramp → else (no `bPlaneHoldEnabled`)
  recenter at `PlaneReturnRate`.
- **Rudder:** ramp wins → else (no `bRudderHoldEnabled`) recenter at
  `RudderReturnRate`. No AutoYaw axis exists yet; if added, it goes on
  top of the ramp.

**Slider widgets** still call the original absolute RPCs
(`ServerRouteHelm{Thrust,Steer,Dive}`). Those setters now also clear
the corresponding ramp intent so the ramp doesn't fight a slider drag
on the next tick. Slam slider to ±1 = instant hard input.

**AutoSpeed / AutoPitch interaction:** ramp blocks only run when the
corresponding auto isn't active. Key press calls
`NotifyManualInput(axis)` → auto suspends for 3s → ramp wins during
the suspend window, auto resumes after the timer drains. Pure
suspend-on-touch pattern, identical to slider behavior.

**Files:**
- [SubPlayerController.h](../../Source/Sub3D/Submarine/SubPlayerController.h) — new RPC declaration
- [SubPlayerController.cpp](../../Source/Sub3D/Submarine/SubPlayerController.cpp) — RPC implementation (routes to Systems)
- [SubmarineSystemsComponent.h](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.h) — `SetThrottleRampIntent`, `ThrottleRampRate`, `ThrottleRampIntent`
- [SubmarineSystemsComponent.cpp](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp) — ramp tick in `UpdateStabilization`; slider clears intent

**User BP action after build** (`PC_SubPlayerController`):

| IA event | Before | After |
|---|---|---|
| `IA_Thrust Triggered` | `ServerRouteHelmThrust(Value)` | `ServerRouteHelmThrottleRamp(Value)` |
| `IA_Thrust Completed` | (optional) `ServerRouteHelmThrust(0)` | `ServerRouteHelmThrottleRamp(0.0)` (required — stops the ramp) |
| `IA_Rudder Triggered` | `ServerRouteHelmSteer(Value)` | `ServerRouteHelmRudderRamp(Value)` |
| `IA_Rudder Completed` | (optional) `ServerRouteHelmSteer(0)` | `ServerRouteHelmRudderRamp(0.0)` (required) |
| `IA_DivePlane Triggered` | `ServerRouteHelmDive(Value)` | `ServerRouteHelmDivePlaneRamp(Value)` |
| `IA_DivePlane Completed` | (optional) `ServerRouteHelmDive(0)` | `ServerRouteHelmDivePlaneRamp(0.0)` (required) |
| Helm widget sliders | `ServerRouteHelm{Thrust,Steer,Dive}` | **unchanged** (absolute path preserved) |

Without rewiring the BP, keys still jump instantly because the original
RPCs are still wired. Rewiring is the only user-facing step needed.

---

## 8. Files modified (review targets)

| File | Reason |
|---|---|
| [Config/DefaultEngine.ini](../../Config/DefaultEngine.ini) | Pawn=Block on SubmarineHull profile (earlier session, kept here for completeness) |
| [Source/Sub3D/Submarine/SubCrewCharacter.cpp](../../Source/Sub3D/Submarine/SubCrewCharacter.cpp) | Capsule blocks Submarine channel; remove zero-reset on helm exit |
| [Source/Sub3D/Submarine/SubmarineBase.cpp](../../Source/Sub3D/Submarine/SubmarineBase.cpp) | HullMesh Pawn=Block in ApplyHullCollisionDefaults; call ApplyPerformanceProfileFromDefinition |
| [Source/Sub3D/Submarine/SubMovementComponent.h](../../Source/Sub3D/Submarine/SubMovementComponent.h) | Tuning defaults (rudder/pitch halved); new params BallastEffectScale, PitchVmaxInfluence; new public method |
| [Source/Sub3D/Submarine/SubMovementComponent.cpp](../../Source/Sub3D/Submarine/SubMovementComponent.cpp) | ComputeTotalMass scaled deviation; pitch-modulated Vmax clamp; component-sweep movement; ApplyPerformanceProfileFromDefinition impl |
| [Source/Sub3D/Submarine/Generator/SubmarineDefinition.h](../../Source/Sub3D/Submarine/Generator/SubmarineDefinition.h) | Four new Performance fields |
| [Source/Sub3D/Submarine/SubmarineSystemsComponent.h](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.h) | `SetThrottleRampIntent`, `AutoDepthVelocityGain`, `AutoDepthResponseRate`, `ThrottleRampRate`, `ThrottleRampIntent` state; `AutoDepthBallastGain` deprecated |
| [Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp](../../Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp) | Auto Depth = zero-vertical-velocity controller; throttle ramp tick; slider clears ramp intent |
| [Source/Sub3D/Submarine/SubPlayerController.h](../../Source/Sub3D/Submarine/SubPlayerController.h) | `ServerRouteHelmThrottleRamp`, `ServerRouteHelmRudderRamp`, `ServerRouteHelmDivePlaneRamp` RPCs |
| [Source/Sub3D/Submarine/SubPlayerController.cpp](../../Source/Sub3D/Submarine/SubPlayerController.cpp) | RPC implementations for the three ramps |
| [Source/Sub3D/Submarine/SubmarineRuntimeTypes.h](../../Source/Sub3D/Submarine/SubmarineRuntimeTypes.h) | `bStabilizationMasterEnabled` default flipped to `true` (Bug A) |
| [Source/Sub3D/Submarine/SubHelmWidget.cpp](../../Source/Sub3D/Submarine/SubHelmWidget.cpp) | `SetInputMode(GameAndUI)` in NativeConstruct, restore `GameOnly` in NativeDestruct (Bug B) |

Earlier-session companions (unchanged by this doc but part of the same
effort — listed for reviewer context):

- `Scripts/UE5/setup_craniata_collisions.py` — generates convex / AABB
  collision primitives on the 141 Craniata meshes.
- Removed `Scripts/UE5/harvest_level_to_bp.py` — it forced
  `NoCollision` overrides on every BP SMC, which was the root cause of
  the crew falling through earlier.

---

## 9. Tuning guide (what the user can iterate on)

Everything below is `EditAnywhere` — BP editor or Details panel, no
recompile needed after the initial build:

| What you want | Param | File |
|---|---|---|
| Sub accelerates slower / faster to top speed | `MaxThrust` (via Definition `MaxThrustN`) | DA_SubDef_* |
| Top speed | `MaxForwardSpeed` (via Definition) | DA_SubDef_* |
| Descent gain / climb penalty strength | `PitchVmaxInfluence` | `USubMovementComponent` |
| Rudder less sluggish | `YawRateDamping` ↑ and `RudderTurnRate` ↑ (keep ratio for same eq) | `USubMovementComponent` |
| Pitch less sluggish | `PitchRateDamping` ↑ and `PitchFromHydroplaneAccel` ↑ (same ratio) | `USubMovementComponent` |
| Ballast more / less dominant | `BallastEffectScale` | `USubMovementComponent` |
| BG self-righting stronger | `PitchRestorationDamping` or `BG_DistanceCm` | `USubMovementComponent` |
| Auto-depth reacts harder to vertical drift | `AutoDepthVelocityGain` ↑ | `USubmarineSystemsComponent` |
| Auto-depth slider visibly moves faster | `AutoDepthResponseRate` ↑ | `USubmarineSystemsComponent` |
| Throttle Z/S ramp faster / slower | `ThrottleRampRate` | `USubmarineSystemsComponent` |
| Rudder A/D ramp faster / slower | `RudderRampRate` | `USubmarineSystemsComponent` |
| Dive plane W/X ramp faster / slower | `DivePlaneRampRate` | `USubmarineSystemsComponent` |
| Auto-systems stay suspended longer after user touch | `SuspendDurationSeconds` | `USubmarineSystemsComponent` |

---

## 10. Things this revision does NOT change

- `FSubmarineNetState` layout / replication frequency.
- Flood simulation.
- Station management.
- Client prediction / interpolation logic (`HandleReplicatedNetState`,
  `InterpolateClient`).
- Rudder / hydroplane visual mesh rotation (still uses replicated
  `RudderInput` / `DivePlaneInput`).
- Turret, sonar, UI.

Any deviation in these areas during PIE should point to an unrelated
regression.

---

## 11. Validation checklist for review

- [ ] Build C++ succeeds (Win64 Development Editor).
- [ ] PIE: pilot sets thrust 30%, leaves helm, values persist on widget
      when pilot B sits down.
- [ ] PIE: full thrust + nose-down pitch → forward speed exceeds
      steady-state cruise by ~10%.
- [ ] PIE: rudder + pitch response feel slower than before (qualitative).
- [ ] PIE: drain ballast fully → sub rises faster than before; fill
      fully → sub sinks faster than before.
- [ ] PIE: drive sub at a Traversal Route wall → sub stops / slides,
      does not phase through.
- [ ] PIE: toggle Auto Depth ON → sub drifts, ballast slider visibly
      moves on its own, vertical velocity trends toward 0.
- [ ] PIE: with Auto Depth ON, drag ballast slider → auto suspends,
      release → 3s later controller resumes and re-trims to zero drift.
- [ ] PIE: hold Z → thrust ramps up smoothly instead of snapping to 100%.
      Hold S → ramps down. Release Z/S → thrust holds the current value.
      Slam slider to -1 → full reverse instantly.
- [ ] PIE: hold A / D → rudder ramps to side, release → recenters at
      `RudderReturnRate` unless `bRudderHoldEnabled = true`.
- [ ] PIE: hold W / X → dive plane ramps, release → recenters unless
      `bPlaneHoldEnabled = true`. AutoPitch ON overrides the ramp.
- [ ] PIE: open helm widget, click anywhere on the panel, then press
      Space → sonar ping fires (input focus fix).
- [ ] PIE: enable AUTO DEPTH HOLD on its own (without touching MASTER) →
      slider visibly trims toward neutralizing vertical velocity.
- [ ] No regression on Proto03 map (legacy generator path still works —
      the `ApplyPerformanceProfileFromDefinition` call is a no-op when
      fields are left at 0).
- [ ] Existing automation: `Sub3D.Submarine.*` tests still pass.

---

## 12. Known caveats / follow-ups

- Pitch-modulated Vmax is a game-side cap, not real gravity-on-forward
  coupling. If the feel is too "synthetic" we can replace with a true
  `ForwardDir · (0,0,−g)` acceleration term and remove the cap.
- `BallastEffectScale = 2` is the starting point. Tune in PIE. If 2 is
  clearly too much, 1.5 is a natural fallback.
- Sweep via `HullMesh` only works if `HullMesh` has a static mesh
  assigned. Craniata needs `SM_Hull` dropped in. For generator subs
  (Proto03), `GeneratedGeometry->GetExteriorHullCollisionComponents()`
  is preferred (already wired via `GetMovementCollisionComponent`).
- Command persistence means a single-player "Quit to menu" leaves the
  sub driving itself until another pilot arrives. Cosmetic issue only
  since the map tears down with the PIE session.
- Runaway-ballast edge case: with `BallastEffectScale = 2`, setting
  both tanks to 0% at a shallow depth may surface the sub faster than
  `MaxVerticalSpeed` allows — the velocity clamp still bounds it, but
  the acceleration burst will be noticeable. Expected and wanted.
