# Architecture : Procedural Crew Animation and Execution Plan

**Date** : 2026-04-23
**Status** : Proposed architecture and execution plan
**Authority-max reference** : `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`
**Related documents** :
- [2026-04-21_local_grid_space_authority_architecture.md](2026-04-21_local_grid_space_authority_architecture.md)
- [2026-04-22_crew_environment_axis.md](2026-04-22_crew_environment_axis.md)
- [character_pipeline_architecture.md](character_pipeline_architecture.md)

> [!IMPORTANT]
> This document covers only the crew procedural animation and embodiment axis.
> It extends the existing runtime stack:
> - `USubCrewMovementComponent`
> - `USubCrewAnimInstance`
> - `FAnimNode_CrewProcedural`
>
> It does not replace the local grid authority plan or the crew environment axis plan.
> Those plans remain the authority for movement rebase, environment ownership, and EVA handoff.

---

## 1. Goal

Build a production-safe procedural animation stack for crew traversal that:

- supports interior grounded locomotion
- supports interior flood swim and exterior swim
- supports stance changes driven by scroll wheel
- supports localized limb damage
- supports carried mass and hand IK equipment constraints
- remains thread-safe in the animation pipeline
- scales to multiple crew entities without introducing game thread animation work

The target style is fully procedural or minimally hybrid. Gameplay authority remains in runtime code. Animation is presentation driven by a compact runtime snapshot.

---

## 2. Current Code Reality

### 2.1 Existing strengths

The repository already has a viable base:

- `USubCrewMovementComponent` already owns:
  - local-grid rebase and extract
  - support quality
  - brace detection
  - hand probes
  - foot traces
  - posture interpolation
  - running state
- `ASubCrewCharacter` already owns:
  - water immersion
  - flood-triggered swim switching
  - hull crossing handoff
  - compartment context
- `USubCrewAnimInstance` already computes:
  - walk cycle
  - crawl cycle
  - posture offsets
  - inertial lean
  - hand IK weights
  - foot IK offsets
  - swim cycle
- `FAnimNode_CrewProcedural` already applies the final bone transforms in one custom node instead of a long stack of `ModifyBone` nodes

### 2.2 Current weaknesses

The current stack is not yet safe enough for long-term production:

1. `IsEmbarked()` is semantically ambiguous.
   - It currently means "the crew still has a submarine frame".
   - It does not reliably mean "the crew is currently using interior locomotion logic".

2. The animation node still reads `USubCrewAnimInstance` directly inside `Evaluate_AnyThread`.
   - This is the main thread-safety issue in the current architecture.

3. Swim logic is not semantically separated.
   - There is still a difference between flood swim, EVA stub, and movement mode transitions.
   - The current swim pose is not a true breaststroke sequence.

4. Public posture truth is still scalar-first.
   - The gameplay-facing contract is still `PostureTarget` / `PostureAlpha`.
   - The desired control contract is discrete posture state, with continuous animation execution.

5. Surface semantics for bracing are still weak.
   - Collision filtering exists.
   - Surface type meaning is not yet explicit enough for a durable brace/contact system.

---

## 3. Design Rules

### 3.1 Strict owner split

The runtime stack must stay split into 3 layers:

1. locomotion and support layer
2. embodiment snapshot layer
3. animation solve and pose output layer

Do not collapse these layers into a single class.

### 3.2 Gameplay truth stays outside the AnimNode

The animation node does not:

- query actors
- trace the world
- read `UObject` state directly
- own gameplay decisions
- change movement state

The animation node only:

- reads a snapshot copied before animation evaluation
- advances phase accumulators
- blends key poses
- solves limb placement
- writes pose transforms

### 3.3 One authoritative semantic per axis

The animation stack must not infer state from mixed raw variables when an explicit runtime state can be published.

Required explicit runtime states:

- reference frame state
- traversal domain
- gait state
- posture state
- limb damage mask
- support state
- equipment IK state

---

## 4. Canonical State Model

### 4.1 Reference frame state

Replace the ambiguous mental model around `IsEmbarked()` with a dedicated transport state.

```cpp
UENUM(BlueprintType)
enum class ESubReferenceSpaceKind : uint8
{
    World,
    SubmarineLocal
};

USTRUCT(BlueprintType)
struct FCrewReferenceFrameState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ESubReferenceSpaceKind Space = ESubReferenceSpaceKind::World;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<ASubmarineBase> Submarine = nullptr;
};
```

This state answers only:

- which frame owns the locomotion transform
- which transform conversion path must be used

It does not answer:

- whether the crew is on a floor
- whether the crew is swimming
- whether the crew is in the submarine interior

### 4.2 Traversal domain

Traversal domain is the animation and locomotion domain.

```cpp
UENUM(BlueprintType)
enum class ECrewTraversalDomain : uint8
{
    InteriorFloor,
    InteriorFloodSwim,
    ExteriorWater,
    ExteriorDrift
};
```

Rules:

- `InteriorFloor` = grounded interior solve
- `InteriorFloodSwim` = interior swim solve
- `ExteriorWater` = world swim solve
- `ExteriorDrift` = optional no-input floating state for later use

`Airlock` is not a traversal domain. Airlock is a compartment semantic, not a locomotion algorithm.

### 4.3 Gait state

```cpp
UENUM(BlueprintType)
enum class ECrewGaitState : uint8
{
    Idle,
    Walk,
    Run,
    Crawl,
    Breaststroke
};
```

Rules:

- `Run` is valid only in upright states and only when support and clearance allow it.
- `Crawl` is the grounded prone gait.
- `Breaststroke` is the swim gait.

### 4.4 Posture state

Gameplay truth must become an enum:

```cpp
UENUM(BlueprintType)
enum class ECrewPostureState : uint8
{
    Standing,
    Crouched,
    Prone
};
```

Runtime execution still uses a float alpha, but the public gameplay contract becomes the enum.

### 4.5 Limb damage mask

```cpp
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsElements))
enum class ESubLimbDamage : uint8
{
    None     = 0,
    LeftArm  = 1 << 0,
    RightArm = 1 << 1,
    LeftLeg  = 1 << 2,
    RightLeg = 1 << 3
};
ENUM_CLASS_FLAGS(ESubLimbDamage);
```

---

## 5. Customization Profile

The locomotion profile is presentation-only.

```cpp
USTRUCT(BlueprintType)
struct FSubAnimLocomotionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
    float ArmSpreadOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
    float ArmSwingMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
    float StepHeightMultiplier = 1.0f;
};
```

Implementation rule:

- these values only scale procedural pose equations and IK targets
- they never change:
  - `MaxWalkSpeed`
  - acceleration
  - braking
  - stamina drain
  - collision size

Gameplay stays in `USubCrewMovementComponent`. Cosmetic signature stays in the profile.

---

## 6. Runtime Snapshot Contract

The animation stack needs a single compact snapshot copied before parallel animation evaluation.

```cpp
USTRUCT()
struct FSubCrewAnimInputSnapshot
{
    GENERATED_BODY()

    FCrewReferenceFrameState ReferenceFrame;

    ECrewTraversalDomain TraversalDomain = ECrewTraversalDomain::InteriorFloor;
    ECrewGaitState GaitState = ECrewGaitState::Idle;
    ECrewPostureState PostureState = ECrewPostureState::Standing;
    ESubLimbDamage LimbDamageMask = ESubLimbDamage::None;

    FVector LocalPlanarVelocity = FVector::ZeroVector;
    FVector LocalLinearVelocity = FVector::ZeroVector;
    FVector LocalLinearAcceleration = FVector::ZeroVector;
    FVector LocalAngularVelocityDeg = FVector::ZeroVector;

    float CurrentStanceAlpha = 1.0f;
    float TargetStanceAlpha = 1.0f;
    float SupportQuality01 = 1.0f;
    float WaterImmersion01 = 0.0f;
    float CarryWeightAlpha = 0.0f;

    bool bHasBrace = false;
    FVector BraceLocationWS = FVector::ZeroVector;
    FVector BraceNormalWS = FVector::ZeroVector;

    bool bLeftHandLocked = false;
    bool bRightHandLocked = false;
    FTransform LeftHandTargetCS = FTransform::Identity;
    FTransform RightHandTargetCS = FTransform::Identity;

    FSubAnimLocomotionProfile LocomotionProfile;
};
```

Rules:

- POD-style fields only
- no world lookups during solve
- no dynamic allocation in the solve path
- no raw actor dependency in the node

---

## 7. Thread-Safe Animation Pipeline

### 7.1 Required pipeline

1. Game Thread
   - movement updates support, probes, posture, gait, transport frame
   - character updates environment and traversal domain
   - health updates limb damage mask
   - equipment updates hand targets and carry state

2. Anim update
   - `USubCrewAnimInstance` copies runtime state into `FSubCrewAnimInputSnapshot`
   - snapshot is stored on a custom anim proxy

3. Worker thread
   - custom node reads only proxy snapshot
   - computes phase state
   - computes IK targets and local bone rotations
   - writes final transforms into `FPoseContext`

### 7.2 Required architectural change

The current direct cast in `FAnimNode_CrewProcedural::Evaluate_AnyThread` must be removed.

Current pattern:

```cpp
const USubCrewAnimInstance* AnimInst = Cast<USubCrewAnimInstance>(
    Output.AnimInstanceProxy->GetAnimInstanceObject());
```

Target pattern:

- custom proxy class derived from `FAnimInstanceProxy`
- snapshot copied onto the proxy
- node reads `const FSubCrewAnimInputSnapshot&`

This is mandatory before adding new procedural states.

---

## 8. Grounded Locomotion Solver

### 8.1 Phase accumulator

Grounded locomotion must be distance-driven, not time-driven.

Formula:

```cpp
DeltaPhase = (SpeedXY * DeltaTime / StrideLengthCm) * TWO_PI;
```

Rules:

- use local planar velocity
- do not use world absolute time
- do not use `sin(Time)` as the primary stride driver

Benefits:

- avoids foot sliding
- keeps stride phase tied to actual motion
- remains valid on a moving submarine frame

### 8.2 Stride parameters

Stride length is gameplay-driven and solve-driven, not cosmetic-profile-driven.

Stride length may vary by:

- gait state
- stance state
- carry weight state
- injury state

Stride length must not vary by:

- cosmetic locomotion profile

### 8.3 Foot swing shape

Foot elevation should not be a pure sinus across the entire cycle.

Recommended grounded foot rule:

- stance phase:
  - foot Z target = 0
  - foot remains locked to floor frame
- swing phase:
  - foot Z target follows a parabolic or cycloidal arc

Minimal implementation:

```cpp
if (SinPhase > 0.0f)
{
    FootLiftZ = MaxStepHeightCm * SinPhase;
}
else
{
    FootLiftZ = 0.0f;
}
```

Production refinement:

- replace this with a dedicated curve or piecewise parabola

### 8.4 Voxel snap

Voxel stylization is applied only at the final IK target stage.

Rule:

- snap the final IK target position
- do not snap pelvis
- do not snap root world position
- do not snap support traces

This keeps stylization visible without destabilizing locomotion.

---

## 9. Stance System

### 9.1 Public contract

Scroll wheel changes discrete posture state:

- `Standing`
- `Crouched`
- `Prone`

### 9.2 Runtime execution

Movement component owns:

- `TargetPostureState`
- `TargetStanceAlpha`
- `CurrentStanceAlpha`

Rules:

- movement state is discrete
- collision and speed use interpolated float execution
- animation uses the same interpolated float

### 9.3 Gameplay effects

`USubCrewMovementComponent` adjusts:

- capsule half-height
- camera offset
- maximum walk speed multiplier

### 9.4 Animation effects

The animation solver uses `CurrentStanceAlpha` for:

- pelvis Z offset
- spine bend
- thigh and calf fold
- crawl gait entry
- prone root pitch
- foot IK target lowering

### 9.5 Required migration

Current public scalar posture flow should be migrated to enum-first control.

Do not keep both long-term.

Short migration path:

1. add `ECrewPostureState TargetPostureState`
2. keep internal alpha during migration
3. update input to step enum state
4. remove direct public float-driven posture input after validation

---

## 10. Swim Solver

### 10.1 Swim trigger

Swimming is triggered by physical water state, not by visual triggers.

Rule:

- when water level rises above chest threshold, movement switches to swim domain

The codebase already has most of this logic in environment state.

### 10.2 Required semantic change

Current swim bool is too weak.

Replace animation-facing use of `bIsSwimmingByFlood` with `TraversalDomain`.

### 10.3 Breaststroke implementation

Breaststroke must be implemented as a phase sequence, not as a single sinusoidal cycle.

Internal phases:

1. `Glide`
2. `CatchPull`
3. `RecoveryTuck`
4. `WhipKick`
5. `ReturnToGlide`

### 10.4 Time mapping

Example fixed-duration cycle:

- `0.0 - 0.4` : Glide
- `0.4 - 0.7` : CatchPull
- `0.7 - 1.0` : RecoveryTuck
- `1.0 - 1.2` : WhipKick
- `1.2 - 1.5` : ReturnToGlide

### 10.5 Pose model

Each phase defines local-space target poses relative to the torso:

- wrist targets
- ankle targets
- torso pitch
- pelvis offset

Animation node behavior:

- interpolate between local key poses
- modulate total cycle duration by input acceleration or swim intent
- if no input:
  - lock on glide
  - add low-amplitude float noise

### 10.6 Required correction

The current swim implementation must be replaced because it still mixes a breaststroke upper body idea with a flutter kick lower body pattern.

---

## 11. Localized Limb Damage

### 11.1 Ownership

Damage ownership belongs to health or damage runtime code, not the animation instance.

Animation reads only the damage bitmask in the snapshot.

### 11.2 Solve rules

If left or right arm is damaged:

- disable standard arm swing on that side
- disable default free-hand brace reach on that side
- if destroyed:
  - allow hidden or collapsed presentation path
- if disabled but present:
  - use passive drag or pendulum behavior

If left or right leg is damaged:

- opposite leg gets longer effective stance support
- damaged leg loses normal step lift
- damaged leg target drags near floor plane
- run gait is disallowed

### 11.3 Scope rule

Damage alters the solve. It does not rewrite locomotion ownership.

---

## 12. Equipment, Carried Mass, and Hand Locks

### 12.1 Carried mass

Inventory or equipment code notifies runtime movement.

Movement owns:

- endurance drain
- walk speed cap
- run eligibility

Animation reads only:

- carry weight alpha
- hand lock targets

### 12.2 One-hand and two-hand states

One-hand item:

- one arm locked to item transform
- other arm remains procedural unless brace or other lock overrides it

Two-hand item:

- both arms lock to equipment targets
- torso yaw aligns toward control or aim direction
- free brace logic is disabled while both hands are locked

### 12.3 Rule

Do not solve equipment by layering visual offsets on top of a full free-swing upper body.
The lock state must alter the procedural solve at the source.

---

## 13. Environment Contact and Bracing

### 13.1 Runtime sensing

Bracing remains a Game Thread responsibility.

Recommended sensing rate:

- reduced-frequency sphere or sweep traces, for example 10 Hz

Do not trace every animation frame in the animation node.

### 13.2 Surface meaning

The brace system needs explicit surface semantics.

Recommended categories:

- `BraceWall`
- `BraceConsole`
- `BraceHandrail`
- `InteriorFloor`

### 13.3 Preferred implementation path

For this project, start with tags or explicit component categories before introducing physical material complexity on generated PMCs.

Reason:

- generated geometry is already section-aware
- tag and component-category wiring is more explicit
- lower maintenance cost for this phase

### 13.4 Snapshot handoff

Movement publishes:

- `bHasBrace`
- `BraceLocationWS`
- `BraceNormalWS`

Animation consumes:

- free hand target blends toward brace pose
- hand orientation aligns to normal

---

## 14. Files Impacted

### 14.1 Core runtime

- `Source/Sub3D/Submarine/SubCrewMovementComponent.h`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubCrewCharacter.h`
- `Source/Sub3D/Submarine/SubCrewCharacter.cpp`

### 14.2 Animation runtime

- `Source/Sub3D/Submarine/SubCrewAnimInstance.h`
- `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp`
- `Source/Sub3D/Submarine/AnimNode_CrewProcedural.h`
- `Source/Sub3D/Submarine/AnimNode_CrewProcedural.cpp`

### 14.3 New recommended runtime pieces

- `Source/Sub3D/Submarine/SubCrewAnimTypes.h`
- `Source/Sub3D/Submarine/SubCrewAnimProxy.h`
- `Source/Sub3D/Submarine/SubCrewAnimProxy.cpp`
- `Source/Sub3D/Submarine/SubHealthComponent.h`
- `Source/Sub3D/Submarine/SubHealthComponent.cpp`

### 14.4 Spatial semantics

- `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`

---

## 15. Execution Procedure

Implementation must happen in narrow patches.

### Phase 0 : Baseline capture

Goal:

- record current behavior before refactor

Validation:

- interior walk
- current posture scroll behavior
- hull crossing
- flood swim

### Phase 1 : Semantic split only

Changes:

- add reference frame state
- add traversal domain
- add gait state
- add enum posture state
- keep current visual output unchanged

Pass criteria:

- no visible regression in current procedural animation
- hull crossing still works
- flood swim still works

### Phase 2 : Snapshot and proxy only

Changes:

- add `FSubCrewAnimInputSnapshot`
- add custom anim proxy
- copy snapshot from anim instance

Pass criteria:

- compile
- no visual change
- snapshot values visible in debug

### Phase 3 : Node reads proxy only

Changes:

- remove direct `USubCrewAnimInstance` read in `Evaluate_AnyThread`
- read snapshot from proxy only

Pass criteria:

- compile
- no visual change
- no direct UObject dependency left in solve path

### Phase 4 : Posture enum migration

Changes:

- input steps `ECrewPostureState`
- movement interpolates `CurrentStanceAlpha`
- animation consumes stance alpha

Pass criteria:

- three-state scroll wheel behavior works
- capsule and camera update correctly
- prone grounded locomotion remains stable

### Phase 5 : Grounded gait solver replacement

Changes:

- add distance-driven stride phase
- replace time-driven grounded walk logic
- keep swim and damage disabled in this phase

Pass criteria:

- visible foot sliding reduced or eliminated
- walk, run, and crawl remain readable
- no collision fighting from IK

### Phase 6 : Swim solver replacement

Changes:

- add breaststroke key-pose sequence
- branch solve by `TraversalDomain`

Pass criteria:

- flood swim uses breaststroke sequence
- no-input swim rests in glide
- exterior swim path remains coherent

### Phase 7 : Damage solve

Changes:

- add limb damage snapshot input
- apply arm and leg compensation rules

Pass criteria:

- damaged arm no longer swings normally
- damaged leg drags and run is disabled

### Phase 8 : Brace semantics and equipment integration

Changes:

- explicit brace surface categories
- one-hand and two-hand arm locking

Pass criteria:

- free hand can brace
- locked hands do not fight brace solve
- heavy carry changes gait presentation through runtime state only

---

## 16. Validation Matrix

### Grounded

1. Walk forward in the submarine while the submarine moves and turns.
2. Run in open interior space.
3. Crouch walk through a constrained interior path.
4. Prone crawl in a low-clearance test path.

Pass:

- no visible foot sliding
- no major pelvis jitter
- no hand IK popping under normal motion

### Swim

1. Flood a compartment above chest level.
2. Enter swim domain from grounded interior.
3. Release input and observe glide hold.
4. Cross hull boundary into exterior water.

Pass:

- domain changes correctly
- swim solver changes correctly
- no brace solve while in exterior water

### Damage

1. Apply left arm damage.
2. Apply right leg damage.
3. Attempt to run.
4. Attempt one-hand and two-hand equipment use.

Pass:

- damaged limbs change solve behavior
- invalid gait modes are suppressed

### Performance

1. Spawn several crew entities.
2. Keep all in locomotion or swim.
3. Watch frame time and animation stability.

Pass:

- no solve-time allocations
- no game-thread traces from animation
- no direct object reads in any-thread solve

---

## 17. Out of Scope

Not part of this implementation path:

- ragdoll
- physical animation blend
- full montage-based animation graph rewrite
- AI-specific locomotion planner
- procedural navmesh work
- authored cinematic transitions
- final ocean buoyancy simulation
- advanced finger pose solve

---

## 18. Final Implementation Rules

1. Do not merge semantic refactor and solve replacement in one patch.
2. Do not add new animation states before the proxy snapshot is in place.
3. Do not keep `IsEmbarked()` as a control concept once the semantic split lands.
4. Do not read `USubCrewAnimInstance` directly in `Evaluate_AnyThread`.
5. Do not trace the world from the animation node.
6. Do not let cosmetic locomotion profiles modify gameplay movement.
7. Do not use `Airlock` as an animation domain.
8. Do not add a second parallel locomotion architecture.

This system must extend the existing stack, not compete with it.
