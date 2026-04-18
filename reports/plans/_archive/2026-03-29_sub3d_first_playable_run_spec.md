# Sub3D - First Playable Run Spec

Date: 2026-03-29
Status: Canonical working spec
Scope: first playable run shell, level contract, breach crisis staging, docking V1, runtime authority, implementation packets
Parent documents:
- `C:\ACC\Projects\Sub3D\Plans\sub3d\sub3d_product_north_star.md`
- `c:\Dev\Sub3D\reports\plans\2026-03-29_sub3d_product_north_star_implementation_plan.md`
- `c:\Dev\Sub3D\reports\plans\2026-03-28_sub3d_global_project_state_precise_reframing.md`

---

## 1. Purpose

This spec defines the exact implementation shell for the `First Playable` run.

It is the canonical merge of:
- the locked product contract from the North Star
- the repository audit of what actually exists today
- the previous direct implementation draft for `RunPhase`, `SubGameMode`, `SubGameState`, breach trigger, and docking

This document is:
- a product-constrained spec
- an implementation packet map
- and a scope defense document

It does not try to solve the full game.
It defines the shortest correct path to one complete playable run.

---

## 2. Product Slice To Prove

The `First Playable` must prove this exact loop:

1. spawn at or inside a docked submarine
2. briefly inhabit and read the interior
3. take control from the helm
4. depart the start dock
5. navigate a baked route using sonar as the only reliable external sense
6. suffer one deterministic breach and flooding crisis
7. react and keep the submarine viable
8. continue traversal after containment if the destination is not yet near
9. approach an end dock
10. align and confirm docking
11. receive explicit success or failure

Success criteria at product level:
- solo playable
- completable in under 15 minutes
- no campaign runtime required
- no external vision cheat replacing sonar
- breach event forces a meaningful decision

---

## 3. Strategic Decision

### 3.1 Chosen path

The first playable is built on:
- one baked traversal route
- one start dock
- one end dock
- one active submarine
- one deterministic breach event
- one run state machine

### 3.2 Explicit rejection

The first playable must not depend on:
- a full campaign runtime
- multi-segment route streaming
- save progression
- fog-of-war persistence
- route-to-route chaining as a gameplay dependency

### 3.3 Why

This is the correct path because:
- the submarine runtime is ahead of the macro-game shell
- `TraversalRouteActor` already provides route build, bake, route transforms, dock transforms, and route identity
- `CampaignWorldManager` exists, but making it critical now would reopen a second major runtime front
- proving one run is strategically better than proving one meta-shell

---

## 4. Current Repository Audit

### 4.1 Systems that already exist and can be reused

`ASubGameMode`
- exists in `Source/Sub3D/Submarine/SubGameMode.h/.cpp`
- currently inherits `AGameModeBase`
- currently handles:
  - `DefaultPawnClass`
  - `PlayerControllerClass`
  - `PostLogin`
  - `ResolveActiveSubmarine()`
  - crew spawn at submarine / helm through `EnterOnFootInSubmarine`
- does not yet own run phase, breach authority, docking state, or success/failure transitions

`ATraversalRouteActor`
- exists in `Source/Sub3D/WorldGen/TraversalRouteActor.h/.cpp`
- already provides:
  - route build pipeline
  - baked route support
  - start and end transforms
  - start and end dock transforms
  - start and end dock radii
  - `CampaignSegmentID`
  - `SonarFieldComponent`

`USonarFieldComponent`
- exists in `Source/Sub3D/WorldGen/SonarFieldComponent.h/.cpp`
- currently stores sonar occupancy data
- current gameplay query is still a stub:
  - `SampleOcclusionAlongRay()` always returns false

`ASubPlayerController`
- already routes helm, ballast, pump, turret, and door input to runtime systems
- this means helm control should be extended, not reinvented

`ASubmarineBase` and submarine runtime
- already provide:
  - movement
  - moving-frame interior traversal baseline
  - breaches
  - flood visuals
  - doors / confinement baseline
  - feedback baseline

### 4.2 Systems that are missing or not yet usable enough

For the first playable, the following are still missing or incomplete:
- authoritative run state machine
- replicated run mirror
- breach trigger actor or deterministic trigger source
- docking completion actor / logic
- sonar gameplay V1
- death baseline
- final water equalization closure

### 4.3 Existing campaign seam

The repo already exposes the future seam through:
- `CampaignSegmentID`
- route transforms
- dock transforms

This means the first playable can stay:
- single-route
- baked
- self-contained

while still preserving future campaign compatibility.

---

## 5. Scope

This spec covers:
- `ESubRunPhase`
- `ASubGameMode` extension into run authority
- `ASubGameState` replicated run mirror
- level topology contract
- breach event staging contract
- failure contract
- docking V1 contract
- source files to touch
- implementation packets `FP-1 -> FP-7`
- done criteria

This spec does not cover in detail:
- full sonar rendering implementation
- dock art or mesh realization
- campaign graph runtime
- save game / profile persistence
- advanced threat gameplay
- Track D audio/VFX polish

---

## 6. Non-Goals

| Out of scope | Reason |
|---|---|
| Full campaign runtime | Explicitly deferred |
| Multi-segment route streaming | Not required to prove one run |
| SaveGame / progression persistence | Not required for FP loop |
| More than 2 players | Not required to prove the loop |
| Randomized breach placement | Deterministic only for FP |
| Advanced creature threats | Not required for the first proof |
| Per-surface control damage | Post-FP |
| Full art pass | Not required to validate the loop |

---

## 7. Technical Prerequisite: GameMode Migration

### 7.1 Hard prerequisite

The current code inherits from `AGameModeBase`.

If the run shell introduces `ASubGameState` as a real replicated run mirror, `ASubGameMode` must migrate from:
- `AGameModeBase`

to:
- `AGameMode`

Reason:
- the first playable needs a real `GameStateClass`
- the run shell needs a replicated state mirror that exists in runtime by default

### 7.2 Concrete migration

`ASubGameMode` must:
- inherit from `AGameMode`
- set `GameStateClass = ASubGameState::StaticClass()` in the constructor
- keep the existing `PostLogin` and submarine spawn responsibilities

### 7.3 Build implications

For this migration itself, no `Sub3D.Build.cs` module change is expected.

Reason:
- `AGameMode` and `AGameState` are already in `Engine` / `GameFramework`
- `Engine` is already a dependency in [Sub3D.Build.cs](c:/Dev/Sub3D/Source/Sub3D/Sub3D.Build.cs)

Revisit `Sub3D.Build.cs` only if later packets add:
- extra automation helpers
- online/session helpers
- new plugin-level networking helpers

---

## 8. Runtime Authority Layout

### 8.1 `ASubGameMode`

`ASubGameMode` becomes the authority for one run.

It must own:
- current run phase
- active submarine reference
- active route reference
- start dock reference
- end dock reference
- breach trigger acceptance
- docking validation
- success/failure transitions

It must not own:
- sonar render state
- flood simulation
- interior traversal logic
- campaign persistence

### 8.2 `ASubGameState`

`ASubGameState` is introduced as the replicated run mirror.

It should own:
- current run phase
- breach active flag
- breached compartment id
- docking alignment flag
- optional objective token or simple objective text

It should not own:
- raw flood state
- raw route geometry
- helm control state

### 8.3 `UGameInstance`

`UGameInstance` remains intentionally thin.

Allowed now:
- optional selected run config
- optional selected route id or seed

Deferred:
- campaign slot
- long-term progression
- persistence

### 8.4 `CampaignWorldManager`

`CampaignWorldManager` remains out of the critical path for the first playable.

It may later consume the same route and docking contracts,
but it must not gate the first playable implementation.

---

## 9. Run State Machine

### 9.1 Enum declaration

New file:
- `Source/Sub3D/Submarine/SubRunPhase.h`

```cpp
UENUM(BlueprintType)
enum class ESubRunPhase : uint8
{
    Boot         UMETA(DisplayName = "Boot"),
    Boarding     UMETA(DisplayName = "Boarding"),
    Departure    UMETA(DisplayName = "Departure"),
    Traverse     UMETA(DisplayName = "Traverse"),
    BreachCrisis UMETA(DisplayName = "Breach Crisis"),
    Approach     UMETA(DisplayName = "Approach"),
    Docking      UMETA(DisplayName = "Docking"),
    Success      UMETA(DisplayName = "Success"),
    Failure      UMETA(DisplayName = "Failure"),
};
```

### 9.2 Valid transitions

```text
Boot          -> Boarding
Boarding      -> Departure
Departure     -> Traverse
Traverse      -> BreachCrisis
Traverse      -> Approach
Traverse      -> Failure
BreachCrisis  -> Traverse
BreachCrisis  -> Approach
BreachCrisis  -> Failure
Approach      -> Docking
Approach      -> Failure
Docking       -> Success
Docking       -> Failure
Failure       -> Boot      (debug / restart path only)
Success       -> Boot      (debug / restart path only)
```

Rules:
- no phase may be skipped
- invalid direct jumps are rejected and logged
- only the server may mutate the phase

### 9.3 Transition conditions

| Transition | Exact condition |
|---|---|
| `Boot -> Boarding` | `BeginPlay` completed, active submarine resolved, route resolved |
| `Boarding -> Departure` | player commits departure from helm |
| `Departure -> Traverse` | submarine clears the start departure volume |
| `Traverse -> BreachCrisis` | deterministic breach trigger accepted during traverse |
| `Traverse -> Approach` | submarine enters end approach volume and no active breach exists |
| `Traverse -> Failure` | catastrophic sink state or all crew dead |
| `BreachCrisis -> Traverse` | designated breach is stabilized and submarine is not yet in end approach volume |
| `BreachCrisis -> Approach` | designated breach is stabilized and submarine is already in end approach volume |
| `BreachCrisis -> Failure` | catastrophic sink state or all crew dead |
| `Approach -> Docking` | docking confirm accepted while aligned and within docking volume |
| `Approach -> Failure` | catastrophic sink state or all crew dead |
| `Docking -> Success` | docking lock window completes while still valid |
| `Docking -> Failure` | submarine loses docking validity during docking lock window |

### 9.4 No vague crisis exit

There is no shipping default timer-based crisis exit.

For the first playable, crisis resolution must be explicit.
The primary path is:
- the single designated breach is patched / stabilized
- `GameMode` receives a clear runtime event
- `GameMode` transitions out of `BreachCrisis`

A debug timer may exist behind an explicit debug toggle, but it must not be the default design path.

---

## 10. ASubGameMode Extension

### 10.1 Current state

`ASubGameMode` currently handles:
- `DefaultPawnClass`
- `PlayerControllerClass`
- `PostLogin`
- `ResolveActiveSubmarine()`

It already spawns the player into the submarine context correctly.

### 10.2 New responsibilities

`ASubGameMode` becomes the run authority.

Properties to add:

| Property | Type | Purpose |
|---|---|---|
| `RunPhase` | `ESubRunPhase` | Authoritative current phase |
| `ActiveRoute` | `ATraversalRouteActor*` | Route reference for this run |
| `StartDock` | `AActor*` | Start dock actor |
| `EndDock` | `AActor*` | End dock actor |
| `BreachTriggerDelay` | `float` | Optional delayed trigger path |
| `DockingAlignmentToleranceCm` | `float` | Max distance to dock target |
| `DockingAlignmentAngleDeg` | `float` | Max angle error |
| `CampaignSegmentID` | `FName` | Future campaign seam |
| `RouteStartTransform` | `FTransform` | Future campaign seam |
| `RouteEndTransform` | `FTransform` | Future campaign seam |

UFUNCTIONs to add:

```cpp
UFUNCTION(BlueprintCallable, Category = "Run")
void TriggerBreachEvent();

UFUNCTION(BlueprintCallable, Category = "Run")
void AttemptDocking();

UFUNCTION(BlueprintCallable, Category = "Run")
void TriggerRunFailure();

UFUNCTION(BlueprintCallable, Category = "Run")
void NotifyBreachStabilized(FName BreachId);

UFUNCTION(BlueprintCallable, Category = "Run")
void AdvanceRunPhase(ESubRunPhase NewPhase);
```

Rules:
- `AdvanceRunPhase()` is the only legal mutation path for `RunPhase`
- `NotifyBreachStabilized()` is the explicit shipping path out of `BreachCrisis`

### 10.3 What GameMode does not own

It does not own:
- sonar query/render state
- flood simulation internals
- crew death internals
- route generation internals

It consumes those systems through events or state reads.

---

## 11. ASubGameState

### 11.1 Purpose

`ASubGameState` mirrors run state for clients so UI and coop logic can react without querying `GameMode`.

### 11.2 Properties

```cpp
UPROPERTY(ReplicatedUsing = OnRep_RunPhase, BlueprintReadOnly, Category = "Run")
ESubRunPhase CurrentPhase = ESubRunPhase::Boot;

UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
bool bBreachActive = false;

UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
FName BreachedCompartmentId = NAME_None;

UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
bool bDockingAligned = false;
```

Optional extension:
- a Blueprint-assignable delegate on `OnRep_RunPhase`

### 11.3 What GameState does not own

It does not own:
- raw compartment flood state
- ballast targets
- sonar field data

---

## 12. Level Topology Contract

### 12.1 Required actors

| Actor | Role | Current status |
|---|---|---|
| `ASubmarineBase` or compiler variant | Active submarine | Exists |
| `ATraversalRouteActor` | Baked route, collision, dock transforms, sonar field | Exists |
| Start dock actor | Spawn and departure reference | Not yet formalized |
| End dock actor | Docking volume and alignment target | Not yet formalized |
| `ABreachTriggerVolume` | Deterministic crisis trigger | Not yet created |

### 12.2 Start dock

Minimum acceptable:
- readable geometry
- one spawn reference transform
- one departure reference volume

The start dock does not need full interaction depth for FP.

### 12.3 End dock

Minimum acceptable:
- readable geometry
- one docking transform
- one docking validation volume
- one docking completion interaction path

Default alignment values:
- `DockingAlignmentToleranceCm = 300.0f`
- `DockingAlignmentAngleDeg = 20.0f`

These remain level-tunable.

### 12.4 Breach trigger volume

New lightweight actor:
- `ABreachTriggerVolume`
- one `UBoxComponent`
- on server overlap with submarine:
  - call `GameMode->TriggerBreachEvent()`

No extra logic belongs in the volume actor.

### 12.5 Route contract

Use the existing baked route path already supported by `TraversalRouteActor`.

Required route properties:
- valid collision for exterior hull traversal
- valid start dock transform
- valid end dock transform
- sonar field data already present on the route actor

---

## 13. Breach Event Staging Contract

### 13.1 Trigger behavior

When `TriggerBreachEvent()` fires during `Traverse`:

1. `GameMode` creates or routes one deterministic breach
2. `RunPhase` advances to `BreachCrisis`
3. `GameState` sets:
   - `bBreachActive = true`
   - `BreachedCompartmentId = <id>`
4. feedback systems react to the crisis state

### 13.2 What the trigger must not do

Out of scope for the trigger:
- scripted camera cut
- dialogue
- creature spawn
- big cinematic sequence
- campaign branching

The breach is discovered through:
- alarm
- water
- feedback
- interior consequence

### 13.3 Shipping resolution path

The shipping first playable uses one designated breach objective.

That objective is considered resolved when:
- the designated scripted breach is patched or explicitly stabilized
- `GameMode->NotifyBreachStabilized(BreachId)` is called

After that:
- if the submarine is not yet in the end approach zone, phase becomes `Traverse`
- if the submarine is already in the end approach zone, phase becomes `Approach`

### 13.4 Failure path

The crisis fails the run when one of:
- submarine catastrophe
- all crew are dead
- unrecoverable sink state

This failure path must not depend on cinematic logic.

---

## 14. Failure Contract And Death Baseline

### 14.1 Failure sources for first playable

For first playable, `Failure` can be triggered by:
- catastrophic submarine sink state
- all crew dead
- docking lost during docking lock window

### 14.2 Death baseline requirement

The first playable requires a minimal but real death baseline.

It does not need full gore or advanced post-processing.
It does need:
- a player death state
- loss of control
- clear dead / alive truth
- run failure when all crew are dead

### 14.3 Minimal acceptable death presentation

Acceptable first baseline:
- control disabled
- minimal death camera handoff or collapse
- optional ragdoll if already available

Not required for first playable:
- body dismemberment
- cinematic death sequences
- advanced respawn loop

---

## 15. Docking V1 Contract

### 15.1 Approach phase

When the submarine enters the destination proximity, `GameMode` transitions to `Approach`.

During `Approach`:
- the player still pilots normally
- `GameMode` evaluates alignment
- `GameState.bDockingAligned` is replicated for UI/feedback

Suggested alignment check:
- distance to dock socket under `DockingAlignmentToleranceCm`
- angle delta under `DockingAlignmentAngleDeg`

### 15.2 Docking phase

Player presses docking confirm input.

`AttemptDocking()` checks:
- current phase is `Approach`
- `bDockingAligned == true`
- submarine is within docking volume

If all pass:
- phase `Approach -> Docking`
- short lockout / soft stop
- phase `Docking -> Success`

If docking is lost during docking lockout:
- phase `Docking -> Failure`

### 15.3 UX rules

Docking must be:
- deliberate
- readable
- manual enough to feel earned

Docking must not be:
- fully automatic
- a large standalone minigame
- cinematic-dependent

---

## 16. Sonar V1 Contract For First Playable

Sonar is mandatory for first playable closure.

### 16.1 Current code reality

The route already stores sonar field data.
However, gameplay query and readable use are not yet complete.

Therefore:
- the data seam exists
- the gameplay layer still needs implementation

### 16.2 Sonar V1 target

Sonar V1 must provide:
- one player action to emit a ping
- route geometry reveal tied to that ping
- short-lived readable result
- no permanent full outside vision replacement

### 16.3 Minimum rendering direction

The intended V1 direction is:
- diegetic helm display
- CRT-like presentation
- ping-driven point cloud or coarse wireframe relief read
- rapid decay / remanence rather than permanent map

This is enough to estimate `FP-3`.
It is not a full render spec.

### 16.4 Scope boundary

This run spec does not fully define sonar rendering or UX implementation.
That belongs in a dedicated sonar spec.

But sonar remains a hard blocker for FP closure.

---

## 17. Source Files

### 17.1 New files

| File | Content |
|---|---|
| `Source/Sub3D/Submarine/SubRunPhase.h` | `ESubRunPhase` enum |
| `Source/Sub3D/Submarine/SubGameState.h/.cpp` | Replicated run mirror |
| `Source/Sub3D/Submarine/SubBreachTriggerVolume.h/.cpp` | Lightweight trigger actor |

### 17.2 Modified files

| File | Changes |
|---|---|
| `Source/Sub3D/Submarine/SubGameMode.h` | Migrate to `AGameMode`, add run authority fields and UFUNCTIONs |
| `Source/Sub3D/Submarine/SubGameMode.cpp` | Set `GameStateClass`, implement phase transitions, breach trigger, docking, failure path |
| `Source/Sub3D/Submarine/SubPlayerController.h/.cpp` | Only if docking confirm needs explicit controller RPC path |
| `Source/Sub3D/Sub3D.Build.cs` | No change expected for `AGameMode` / `AGameState` migration itself |

### 17.3 Protected files

These should not be structurally rewritten by the run shell work:
- `SubmarineBase.h/.cpp`
- `SubMovementComponent.h/.cpp`
- `SubHullComponent.h/.cpp`
- `SubmarineCompartmentComponent.h/.cpp`
- `SubCrewCharacter.h/.cpp`

The run shell may read from these systems, but should not destabilize them.

---

## 18. Runtime Contracts

### 18.1 Authority

- `RunPhase` is authoritative on the server
- `ASubGameState::CurrentPhase` is the replicated mirror
- clients never mutate run phase directly

### 18.2 Networking

- `ASubGameState` replicates:
  - `CurrentPhase`
  - `bBreachActive`
  - `BreachedCompartmentId`
  - `bDockingAligned`
- breach trigger actor is server-authoritative only
- docking confirm should follow the existing controller-to-server pattern

### 18.3 Determinism

- breach triggers exactly once per run
- duplicate trigger overlaps are ignored after the first accepted trigger
- `AttemptDocking()` is idempotent during `Approach`

---

## 19. Future Campaign Seam

The following properties should exist on `ASubGameMode` even if lightly used in FP:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Campaign")
FName CampaignSegmentID = NAME_None;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Campaign")
FTransform RouteStartTransform;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Campaign")
FTransform RouteEndTransform;
```

These are not campaign runtime.
They are a seam to avoid painting the first playable into a corner.

---

## 20. Precise Implementation Packets

### FP-1 - Run Shell

Goal:
- create one authoritative run state machine

Deliverables:
- `ESubRunPhase`
- `AGameModeBase -> AGameMode` migration
- `AdvanceRunPhase()`
- `ASubGameState`
- `GameStateClass` binding
- phase replication
- transition logging

Done when:
- phases advance deterministically in PIE
- invalid transitions are rejected
- clients can read run phase

### FP-2 - First Playable Level Shell

Goal:
- stand up one complete map shell around existing submarine and route tech

Deliverables:
- start dock placement
- end dock placement
- route placement
- submarine placement
- deterministic spawn and departure setup

Done when:
- player can start, reach helm, depart, and traverse physically

### FP-3 - Helm + Sonar V1

Goal:
- make navigation depend on sonar

Deliverables:
- ping input
- reveal logic
- readable helm display

Done when:
- tester can navigate using sonar instead of vision cheats

### FP-4 - Breach Crisis Closure

Goal:
- the run contains one meaningful crisis

Deliverables:
- deterministic breach trigger
- authoritative `BreachCrisis` phase
- explicit crisis resolution event
- visible crisis consequence

Done when:
- breach forces a visible and playable decision
- `BreachCrisis` does not risk remaining stuck forever

### FP-4A - Death Baseline

Goal:
- give the run a real interior failure condition

Deliverables:
- character death state
- loss of control on death
- all-crew-dead run failure path

Done when:
- the run can fail from crew death, not only from submarine sink state

### FP-5 - Docking V1

Goal:
- the run ends with a precise arrival interaction

Deliverables:
- docking alignment evaluation
- docking confirm path
- success/failure transition

Done when:
- arrival feels manual enough to be intentional

### FP-6 - Solo Closure

Goal:
- the run is fully completable by one player

Requires:
- no mandatory second-player dependency
- crisis response is solo-feasible

Done when:
- full loop is completable solo in under 15 minutes

### FP-7 - Coop Closure

Goal:
- the same loop works better with 2 players

Requires:
- readable shared state
- no station deadlocks
- no run-phase replication confusion

Done when:
- 2-player run is smoother and richer, not more brittle

---

## 21. Risks And Mitigations

### Risk 1 - Sonar exists architecturally but not yet as gameplay

Likely root cause:
- `SonarFieldComponent` stores data, but gameplay query/render is unfinished

Mitigation:
- give sonar its own spec
- treat it as a hard blocker for FP closure

### Risk 2 - Submarine crisis loop is still behaviorally uneven

Likely root cause:
- water equalization and character crisis layers are still stabilizing

Mitigation:
- keep `Track A` on the critical path until the breach loop stops lying

### Risk 3 - Campaign temptation reopens scope

Likely root cause:
- route and campaign code already exist

Mitigation:
- keep campaign as seam only
- do not make it a blocker for the first playable

### Risk 4 - Docking becomes either trivial or overbuilt

Likely root cause:
- no explicit V1 contract

Mitigation:
- keep docking to alignment + confirm + success/failure

---

## 22. Validation Matrix

### 22.1 Automation targets

Target tests:
- valid run phase progression
- invalid phase transition rejection
- breach trigger ignored outside `Traverse`
- breach stabilized during early route returns to `Traverse`
- breach stabilized inside end approach zone returns to `Approach`
- docking confirm rejected when unaligned

### 22.2 PIE manual targets

- player spawns correctly on the run map
- player can walk inside submarine before departure
- player can reach helm and depart
- route traversal is physically possible
- sonar is actually needed
- breach event happens deterministically
- crisis exits to `Traverse` when resolved far from destination
- crisis exits to `Approach` when resolved near destination
- breach has visible consequence
- player can still finish if they react correctly
- player can dock manually
- success and failure states are explicit
- solo loop fits under 15 minutes

---

## 23. Done Criteria

This spec is closed when all of the following are true:

1. `ESubRunPhase` compiles with all 9 states
2. `ASubGameMode` is migrated to `AGameMode`
3. `ASubGameMode` owns run phase, route/dock refs, breach trigger, docking confirm, failure path
4. `ASubGameState` replicates run phase and basic run mirror state
5. `ABreachTriggerVolume` exists and routes overlap correctly
6. run phase automation tests pass
7. the PIE manual checklist passes solo
8. the implementation did not destabilize the protected submarine runtime systems

---

## 24. Next Specs

After this spec, in order:

1. `sub3d_first_playable_level_architecture.md`
2. `sub3d_helm_and_sonar_v1_spec.md`
3. `sub3d_docking_v1_spec.md`
4. `sub3d_trackA_inner_crisis_closure_spec.md`
5. `sub3d_trackB_campaign_runtime_stub_spec.md`

Important rule:
- the campaign runtime stub is a seam document, not the next gameplay implementation target

---

## 25. Final Recommendation

The correct next execution order remains:

1. `FP-1 Run Shell`
2. `FP-2 Level Shell`
3. `FP-3 Sonar V1`
4. `FP-4 Breach Crisis`
5. `FP-4A Death Baseline`
6. `FP-5 Docking`
7. `FP-6 Solo Closure`
8. `FP-7 Coop Closure`

And the core strategic rule remains:

- prove one run first
- prove it on one baked route
- keep `GameMode` authoritative for the run
- keep `GameInstance` thin
- preserve the campaign seam
- do not let campaign runtime become a blocker before the first playable exists
