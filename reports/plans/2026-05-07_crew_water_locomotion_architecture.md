# Crew Water Locomotion Architecture

Date: 2026-05-07
Status: architecture addendum
Authority-max plan: `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`
Related reference: `C:\Dev\Sub3D\reports\plans\2026-05-04_water_implementation_plan.md`
Animation audit reference: `C:\Dev\Sub3D\reports\2026-05-04_crew_animation_audit.md`
Animation/debug implementation reference: `C:\Dev\Sub3D\reports\plans\2026-05-08_crew_animation_procedural_debug_plan.md`
Macro execution reference: `C:\Dev\Sub3D\reports\plans\2026-05-08_crew_animation_water_macro_execution_plan.md`

## Scope

This document defines the target architecture for crew movement in water:

- exterior ocean swimming after crossing a hull boundary;
- interior walking/wading in flooded compartments;
- later interior swimming driven by actual character immersion, not a fixed compartment water height;
- explicit separation between submarine-local water movement and exterior world-space swimming;
- hull-boundary handoff rules, including later suction/ejection forces from airlocks or breaches;
- HUD and debug requirements for validating the system in PIE;
- asset list required for first playable and later polish.
- animation audit findings and quick wins that directly affect water locomotion.

This is an addendum to the authority-max plan. It does not replace the existing movement, flood, or submarine moving-frame architecture.

## Current Code Context

Existing systems to preserve:

- `ASubCrewCharacter`
  - Owns crew gameplay state.
  - Tracks `CurrentSubmarine`, `CurrentCompartment`, pressure, water height, immersion, and hull crossing.
  - Current relevant functions:
    - `HandleHullCrossing(...)`
    - `UpdateEnvironmentalEffects(...)`
    - `ApplyWaterMovementState(...)`
    - `ApplySwimmingMovementState(...)`

- `USubCrewMovementComponent`
  - Owns the moving-frame crew locomotion.
  - Replicates `GridSpaceTransform` and `ECrewEmbarkState`.
  - Owns input conversion, local rebase, saved move payloads, and locomotion frame.

- `UCrewUnderwaterPPComponent`
  - Owns underwater post-process behavior.
  - Should remain visual/audio-facing, not movement-authoritative.

- `USubCrewAnimInstance`
  - Consumes locomotion frame state.
  - Should not decide gameplay transitions.

- `Sub3DGameplayDebugger` and `Sub3DDebugSettings`
  - Existing validation surfaces for runtime state.

## Design Rule

Do not let Unreal `PhysicsVolume` be the source of truth for Sub3D water movement.

Reason:

- Unreal `MOVE_Swimming` assumes a water `PhysicsVolume`.
- Sub3D water can be inside moving compartments, can tilt with the submarine, can slosh from impacts, and can exist without a static UE water volume.
- Native Unreal fallback paths can switch `MOVE_Swimming` to `MOVE_Falling` when no water volume is present.

For first playable, `MOVE_Swimming` can still be used with explicit guards.
For the target architecture, Sub3D should move swimming to a custom movement mode.

## State Axes

Keep state axes separate.

Important rule:

- Being in water does not imply `Outside`.
- A crew member inside a flooded compartment remains `Embarked` and remains in the submarine local frame.
- Only crossing a `USubHullBoundaryComponent` changes the spatial frame from submarine-local to world-space.
- Airlock flow, breach suction, or decompression forces are handoff effects. They can push the crew through a boundary, but they are not the authority for the final frame state. The boundary crossing is.

### Spatial Frame

Existing enum:

```cpp
enum class ECrewEmbarkState : uint8
{
	Outside,
	Embarked,
	Transitioning
};
```

Meaning:

- `Outside`: crew is outside the submarine hull. World-space movement is active. The crew may still keep `CurrentSubmarine` for pressure, ocean context, and return-to-sub interactions.
- `Embarked`: crew is inside the submarine moving frame. Grid rebase is active. This remains true even if the compartment is flooded and the crew is wading or swimming.
- `Transitioning`: reserved for later multi-tick handoff smoothing.

`ECrewEmbarkState` must not encode water depth, animation state, oxygen state, or HUD state.

### Movement Medium

Water contact is a movement medium inside the current spatial frame.

Examples:

- `Embarked + Dry`: normal interior walking.
- `Embarked + ShallowWade`: local-grid walking with water slowdown.
- `Embarked + DeepWade`: local-grid walking with heavy water resistance.
- `Embarked + Swimming`: local-grid swimming inside a flooded compartment.
- `Outside + Ocean`: world-space swimming outside the submarine.

This distinction is required because interior water can move with submarine pitch, acceleration, and impacts. The crew should still be transported by the submarine frame while inside the hull.

### Water Contact

Proposed enum:

```cpp
UENUM(BlueprintType)
enum class ECrewWaterContactState : uint8
{
	Dry,
	ShallowWade,
	DeepWade,
	Swimming,
	Ocean
};
```

Meaning:

- `Dry`: no water contact.
- `ShallowWade`: feet/lower legs in water, minor walking slowdown.
- `DeepWade`: heavy walking slowdown, water affects control.
- `Swimming`: crew is buoyant and uses swim controls in the current spatial frame.
- `Ocean`: exterior water context. For current game rules this is always `Outside + Swimming`.

### Immersion Sample

Proposed runtime sample:

```cpp
USTRUCT(BlueprintType)
struct FCrewImmersionSample
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHasWater = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsOcean = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHeadUnderwater = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Immersion01 = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float WaterSurfaceWorldZ = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector WaterNormalWorld = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector WaterVelocityWorld = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector WaterVelocityLocal = FVector::ZeroVector;
};
```

This sample is data only. It must not directly change movement mode.
When `EmbarkState == Embarked`, local-space values are the authoritative gameplay values. World-space values are derived for visuals and debug.

### Animation Domain

Animation must consume resolved locomotion state, not infer gameplay state from scattered flags.

Current audit finding:

- `USubCrewAnimInstance` already reads `FCrewLocomotionFrame`.
- Procedural swim exists, but the audit found it was not a complete swim control mode.
- `bIsSwimmingByFlood` was a weak source of truth and should not be used as the main animation driver.
- Hand IK and foot IK are calculated, but final graph application needs editor validation.

Target animation inputs:

```cpp
USTRUCT(BlueprintType)
struct FCrewWaterAnimState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ECrewEmbarkState EmbarkState = ECrewEmbarkState::Embarked;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ECrewWaterContactState WaterContactState = ECrewWaterContactState::Dry;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsSwimming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsInteriorSwim = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsExteriorSwim = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Immersion01 = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector SwimInputLocal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector SwimInputWorld = FVector::ZeroVector;
};
```

Rules:

- `bIsSwimming` is derived from the resolved movement state.
- `bIsInteriorSwim` means `EmbarkState == Embarked && WaterContactState == Swimming`.
- `bIsExteriorSwim` means `EmbarkState == Outside && WaterContactState == Ocean`.
- Anim code must not decide when a player disembarks.
- Anim code must not read `CurrentCompartment` or flood state directly.
- Foot IK is disabled during swim.
- Hand IK is disabled during swim unless a later grab/brace system explicitly owns it.

### Hull Boundary Handoff

Proposed handoff context:

```cpp
USTRUCT(BlueprintType)
struct FCrewHullBoundaryHandoff
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USubHullBoundaryComponent> Boundary = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bOutgoing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector EntryVelocityLocal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector ExitVelocityWorld = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector HandoffImpulseLocal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector HandoffImpulseWorld = FVector::ZeroVector;
};
```

Purpose:

- Capture the exact boundary that changed the spatial frame.
- Convert velocity from submarine local-space to world-space on exit.
- Convert velocity from world-space to submarine local-space on entry.
- Later, apply suction/ejection impulses from airlocks, breaches, or pressure differentials.

The handoff impulse is applied during the crossing. It does not replace `ECrewEmbarkState`.

## Proposed Components

### `UCrewWaterContactComponent`

New component on `ASubCrewCharacter`.

Responsibilities:

- Compute `FCrewImmersionSample` once per tick.
- Read crew capsule dimensions.
- Read `ECrewEmbarkState`.
- Read `CurrentSubmarine`.
- Read `CurrentCompartment`.
- Read `USubFloodComponent`.
- Convert compartment water data into submarine local-space water data.
- Derive world-space water data from the submarine transform for visuals and debug.
- Later, sample tilted/sloshed water instead of flat compartment height.

Non-responsibilities:

- It does not call `SetMovementMode`.
- It does not play animation.
- It does not own post-process.
- It does not replicate movement.

Initial implementation:

- Outside:
  - `bHasWater = true`
  - `bIsOcean = true`
  - `Immersion01 = 1`
  - `bHeadUnderwater = true` for current exterior ocean assumption

- Embarked:
  - Use compartment flood state.
  - Compute capsule feet/head against the local water surface.
  - Output `Immersion01`.
  - Output local water velocity as the primary gameplay value.
  - Derive world water velocity for VFX/audio.
  - Do not trigger swimming from a fixed height threshold in the current phase.

Later implementation:

- Use dynamic water plane or local water volume.
- Include submarine pitch/roll while staying in submarine-local gameplay space.
- Include slosh offset from speed/impact.
- Include water velocity for control drag and FX.

### `ASubCrewCharacter`

Keep as orchestrator.

Add a central resolver:

```cpp
void ASubCrewCharacter::ResolveCrewLocomotionFromEnvironment(float DeltaSeconds);
```

Inputs:

- `ECrewEmbarkState`
- `ECrewWaterContactState`
- `FCrewImmersionSample`
- optional `FCrewHullBoundaryHandoff`
- ladder state
- posture/running state

Outputs:

- movement mode request;
- max walk speed;
- swim speed;
- braking/deceleration;
- gravity scale;
- replicated/debug state.

Rule:

All movement mode changes for crew water must go through this resolver or a small helper called by it.

### `USubCrewMovementComponent`

Short term:

- Keep `MOVE_Swimming` for exterior swimming.
- Maintain the invariant:
  - `EmbarkState == Outside` implies final movement mode is swimming.
- Maintain the separate invariant:
  - `EmbarkState == Embarked` implies the crew remains in submarine local-space, even if the movement medium is water.
- Block native Unreal fallback to falling when outside.
- Keep logs for every mode change.

Target:

Move swimming to a custom movement mode:

```cpp
enum ECrewCustomMovementMode : uint8
{
	CMOVE_None = 0,
	CMOVE_Sub3DSwim = 1
};
```

Target behavior:

- Use `MOVE_Custom` + `CMOVE_Sub3DSwim`.
- Implement `PhysCustom` branch for Sub3D swimming.
- Remove dependence on `PhysicsVolume`.
- Support both `Outside + CMOVE_Sub3DSwim` and `Embarked + CMOVE_Sub3DSwim`.
- For `Embarked + CMOVE_Sub3DSwim`, keep grid rebase active and integrate swim velocity in submarine local-space.
- Keep `MOVE_Walking` for inside/base walking and wading.
- Keep ladder as its own movement override or current authoritative ladder path.

### `UCrewUnderwaterPPComponent`

Consume `FCrewImmersionSample`.

Responsibilities:

- underwater PP weight;
- camera waterline transition;
- audio low-pass signal;
- droplets and surface transition events;
- outside ocean always underwater for current FP assumption.

It must not decide movement mode.

### `USubCrewAnimInstance`

Consume locomotion data:

- `bIsSwimming`
- `ECrewWaterContactState`
- `Immersion01`
- local/world velocity
- vertical swim axis
- posture
- `FCrewWaterAnimState`

It must not query compartment water directly.

Required audit follow-up:

- Open `ABP_Crew`.
- Verify whether `FootIK_R_Offset`, `FootIK_L_Offset`, `HandIK_L_Target`, and `HandIK_R_Target` are actually wired.
- Verify whether there are extra Control Rig, Layered Blend per Bone, or Transform Modify Bone nodes that fight the procedural C++ output.
- Verify whether swim/wade states are driven from `FCrewLocomotionFrame` and the new water anim state, not from deprecated flood flags.

### `FAnimNode_CrewProcedural`

Keep as the current procedural layer for first playable.

Do not expand it into gameplay logic.

Allowed responsibilities:

- procedural breathing;
- posture offsets;
- simple swim pose;
- local motion compensation;
- pelvis offset already present.

Deferred or validation-required:

- final foot IK application;
- final hand IK application;
- authored locomotion replacement;
- skeleton axis/bone roll fixes.

### Input Ownership

The animation audit identified split ownership between pawn and player controller.

Target rule:

- PlayerController owns input mapping contexts.
- `ASubCrewCharacter` exposes typed gameplay wrappers.
- `USubCrewMovementComponent` owns movement intent and final movement input.

For water locomotion:

- one planar move action feeds `ApplyCrewPlanarMoveInput`;
- one vertical swim action feeds `ApplyCrewVerticalMoveInput`;
- no Blueprint should call `SetMovementMode` directly;
- no Blueprint should decide `EmbarkState`.

Input context recommendation:

- Keep `IMC_OnFoot` as the base context for embarked walking/wading.
- Add swim vertical input to the same active character context for first playable.
- Do not introduce `IMC_Swimming` until input ownership is centralized in `PC_SubPlayerController`.

Reason:

- Current gameplay rule has two spatial states, not a separate player control mode.
- Adding `IMC_Swimming` too early can create another path that fights existing pawn/controller bindings.

## Transition Rules

### Current Phase

Required behavior:

- `EmbarkState == Outside`:
  - swimming is mandatory;
  - movement is world-space;
  - vertical input is enabled;
  - pressure/ocean post-process is enabled;
  - no dependency on UE water volume.

- `EmbarkState == Embarked`:
  - movement remains submarine-local;
  - movement remains walking/wading;
  - water can slow walking;
  - static height auto-swim is disabled;
  - interior swim is deferred.

- `USubHullBoundaryComponent` crossing:
  - outgoing crossing changes `Embarked` to `Outside`;
  - incoming crossing changes `Outside` to `Embarked`;
  - velocity is converted between local and world frames;
  - later suction/ejection impulses are applied in the handoff context.

- Ladder/climb:
  - ladder has priority over water movement while active.

### Boundary Handoff Rules

Outgoing through airlock or breach:

- Read current submarine transform.
- Convert crew local velocity to world velocity.
- Add submarine world velocity.
- Apply optional handoff impulse:
  - breach suction;
  - decompression push;
  - airlock current;
  - collision-driven ejection.
- Set `EmbarkState = Outside`.
- Final movement is world-space swimming.

Incoming through airlock or breach:

- Read current submarine transform.
- Convert crew world velocity to submarine local velocity.
- Subtract submarine transport velocity.
- Set `EmbarkState = Embarked`.
- Final movement is submarine-local walking, wading, or later interior swimming depending on immersion.

Suction/ejection model:

- Store impulse in submarine local-space first.
- Convert to world-space only during outgoing handoff.
- For incoming handoff, convert world-space impact/current into local-space before applying to the crew.
- Do not let suction/ejection directly set `EmbarkState`. It can move the capsule across a boundary; the boundary event sets the frame.

### Later Interior Swim

Interior swim should be based on `FCrewImmersionSample`, not raw compartment `WaterHeightCm`.
Interior swim does not disembark the player. It remains `Embarked`.

Use hysteresis:

```cpp
SwimEnterImmersion01 = 0.72f;
SwimExitImmersion01 = 0.55f;
```

Rules:

- Enter swim only when `Immersion01 >= SwimEnterImmersion01`.
- Exit swim only when `Immersion01 <= SwimExitImmersion01`.
- While interior swimming, keep grid rebase active.
- Swim acceleration and drag are evaluated in submarine local-space.
- Convert to world-space only for collision solving, rendering, camera, VFX, and replication outputs already expected by CMC.
- If `bHeadUnderwater`, force underwater PP and audio.
- If water velocity is high, add movement drag and possible stumble/push behavior.

Reason:

- Prevents rapid walk/swim toggling when water moves.
- Allows fun transitions from pitch, speed, collisions, and slosh.

## HUD Requirements

### Player HUD

Minimal first-playable HUD elements:

- pressure danger indicator;
- oxygen/air status indicator;
- underwater/head-submerged indicator;
- water resistance indicator when wading or swimming;
- optional depth/pressure readout in EVA.

Do not show raw debug values on player HUD.

### Debug HUD

Add to existing Sub3D debug surfaces:

- `EmbarkState`
- `MovementMode`
- `CustomMovementMode`
- spatial frame: `EmbarkedLocal` or `OutsideWorld`
- `WaterContactState`
- `Immersion01`
- `bHeadUnderwater`
- `CurrentCompartmentId`
- `WaterHeightCm`
- `WaterSurfaceWorldZ`
- `WaterNormalWorld`
- `WaterVelocityWorld`
- `WaterVelocityLocal`
- `CrewVelocityLocal`
- `CrewVelocityWorld`
- last hull boundary id/name
- last handoff direction
- last handoff impulse local/world
- last movement mode change
- last movement mode change source if available

Preferred implementation:

- Extend `Sub3DGameplayDebugger`.
- Add toggles to `Sub3DDebugSettings`.
- Keep debug state visible in PIE without requiring final UMG.
- Add one compact animation block:
  - `Stance`
  - `Gait`
  - `bIsSwimming`
  - `bIsInteriorSwim`
  - `bIsExteriorSwim`
  - `FootIK active`
  - `HandIK active`
  - current swim/wade animation state name if exposed by ABP.

### UMG Assets

Use editor-assigned widgets for persistent UI.

Proposed assets:

- `Content/Sub3D/UI/WBP_CrewStatusHUD`
  - player-facing compact HUD.

- `Content/Sub3D/UI/WBP_CrewWaterDebugPanel`
  - validation panel for water locomotion.

- `Content/Sub3D/UI/Icons/I_WaterResistance`
- `Content/Sub3D/UI/Icons/I_HeadUnderwater`
- `Content/Sub3D/UI/Icons/I_PressureWarning`
- `Content/Sub3D/UI/Icons/I_Oxygen`

First playable can use debug text before final icons are made.

## Required Assets

### Input

Existing/proposed:

- `Content/Sub3D/Input/IA_CrewMove`
- `Content/Sub3D/Input/SWIM/IA_CrewSwimVertical`
- `Content/Sub3D/Input/IMC_OnFoot`

Mappings:

- `Z`: forward
- `S`: backward
- `Q`: left
- `D`: right
- `Space`: swim up
- `Left Ctrl`: swim down

### Animation

Required for first swim pass:

- `A_Crew_Swim_Idle`
- `A_Crew_Swim_Forward`
- `A_Crew_Swim_Backward`
- `A_Crew_Swim_Strafe_L`
- `A_Crew_Swim_Strafe_R`
- `A_Crew_Swim_Ascend`
- `A_Crew_Swim_Descend`
- `A_Crew_Walk_To_Swim`
- `A_Crew_Swim_To_Walk`

Required for wading:

- `A_Crew_Wade_Shallow_Walk`
- `A_Crew_Wade_Deep_Walk`
- `A_Crew_Wade_Idle`

Animation blueprint updates:

- Add water contact state input.
- Add swim blendspace.
- Add wade blendspace.
- Keep foot IK disabled during swim.
- Keep hand IK disabled during swim unless a later grab/brace system owns it.
- Validate whether current procedural swim is acceptable as first playable placeholder.
- Do not add final authored animation dependencies before state routing is correct.

Audit-driven quick win assets:

- `BS_Crew_Swim_2D` or equivalent procedural state parameters.
- `BS_Crew_Wade_1D` or equivalent procedural tuning.
- `CR_Crew_HandIK` only if the existing ABP proves IK targets are not wired.
- `CR_Crew_FootIK` only if the existing ABP proves foot offsets are not wired.

### VFX

First playable:

- `NS_Crew_Splash_EnterWater`
- `NS_Crew_Splash_ExitWater`
- `NS_Crew_Swim_Bubbles`
- `NS_Crew_Wade_Ripples`
- `NS_Breach_Suction_Stream`
- `NS_Airlock_Water_Jet`

Later:

- `NS_Waterline_Droplets`
- `NS_HullContact_Foam`
- `NS_Crew_Swim_Trail`
- `NS_Impact_Slosh_Spray`

### Audio

First playable:

- underwater low-pass mix/state;
- swim stroke loop or one-shots;
- wade step events;
- splash enter/exit;
- airlock water rush;
- breach suction rush;
- pressure danger cue.

Later:

- water impact against hull;
- slosh in compartment;
- muffled interior audio when head underwater;
- creature/grab underwater cues.

### Materials And Post Process

Existing:

- current underwater PP material via `UCrewUnderwaterPPComponent`.

Required:

- waterline mask function for camera transition;
- droplet overlay or PP material parameter;
- foam/contact material function for hull-water contact;
- ripple normal/detail for shallow water surfaces.

## Implementation Order

The order below unifies this water locomotion plan with the animation audit findings.
The quick wins are scoped to validate state flow first. They do not require final authored animation.

Execution rule:

- Implement `reports/plans/2026-05-08_crew_animation_water_macro_execution_plan.md` Milestones 1 and 2 before deep water locomotion refactors.
- Water locomotion Phase B must feed the same `WaterContactState` and `ImmersionSample` fields used by the crew animation debug snapshot.
- Do not build a separate water debug vocabulary. The crew debug snapshot is the shared validation source.

### Phase A - Stabilize Current Behavior

Goal:

- Exterior swimming cannot become falling.
- Interior flood does not auto-trigger swim by static height.

Tasks:

- Keep `Outside => Swimming` invariant in `USubCrewMovementComponent`.
- Log all movement mode changes.
- Keep interior water as walking slowdown only.

Validation:

- Cross hull outward.
- Confirm final mode remains swimming after several ticks.
- Confirm no static-height interior swim trigger.
- Confirm `USubCrewAnimInstance::bIsSwimming` follows final movement state.

### Phase B - Add Water Contact Data

Goal:

- Isolate water sampling from movement decisions.
- Keep interior water sampling in submarine local-space.

Tasks:

- Add `ECrewWaterContactState`.
- Add `FCrewImmersionSample`.
- Add `UCrewWaterContactComponent`.
- Move current capsule immersion calculation out of `ASubCrewCharacter`.
- Store both local and world water velocity in `FCrewImmersionSample`.

Validation:

- Debug HUD reports stable immersion values.
- Outside reports ocean sample.
- Dry compartment reports no water.
- Flooded compartment reports correct rough immersion.
- Flooded compartment reports local water values while `EmbarkState` remains `Embarked`.
- Anim debug reports water contact state without reading compartment state directly.

### Phase C - Centralize Locomotion Resolution

Goal:

- Remove scattered movement mode decisions.

Tasks:

- Add `ResolveCrewLocomotionFromEnvironment`.
- Route outside swim, wading slowdown, gravity, and speed settings through it.
- Keep ladder priority explicit.
- Add `FCrewHullBoundaryHandoff`.
- Route outgoing/incoming boundary velocity conversion through one helper.
- Keep suction/ejection force application inside the handoff helper.

Validation:

- Search confirms crew water movement mode changes have one write path.
- Walking, outside swimming, interior wading, interior swimming, and ladder do not fight each other.
- Boundary handoff logs local and world velocity before/after conversion.
- Anim state reports `bIsInteriorSwim` and `bIsExteriorSwim` correctly.

### Phase D - HUD And Debug

Goal:

- Make hidden state visible in PIE.

Tasks:

- Extend `Sub3DGameplayDebugger`.
- Add debug settings toggles.
- Add `WBP_CrewWaterDebugPanel` if needed.
- Add minimal player HUD signals.

Validation:

- Debug HUD shows movement mode, embark state, water contact state, and immersion.
- Player HUD only shows player-facing indicators.
- Debug HUD shows stance, gait, swim flags, and IK active flags.

### Phase E - Custom Swim Movement

Goal:

- Remove Unreal water-volume coupling.

Tasks:

- Add `CMOVE_Sub3DSwim`.
- Implement `PhysCustom` swim path.
- Move exterior swimming from `MOVE_Swimming` to `MOVE_Custom`.
- Support `Embarked + CMOVE_Sub3DSwim` without disabling grid rebase.
- Support `Outside + CMOVE_Sub3DSwim` in world-space.
- Keep saved move serialization compatible.

Validation:

- No UE water volume needed.
- No native falling fallback possible.
- Network handoff remains stable.
- Interior swim remains local to the submarine.

### Phase F - Animation Quick Wins

Goal:

- Make the current animation system reflect water locomotion states clearly enough for first playable validation.

Tasks:

- Replace deprecated swim animation driver usage with resolved swim state.
- Add `FCrewWaterAnimState` or equivalent fields to the anim instance.
- Confirm procedural swim uses input direction and vertical axis.
- Add separate interior/exterior swim debug flags.
- Disable foot IK and hand IK while swimming unless a dedicated grab/brace state owns them.
- Validate `ABP_Crew` wiring for existing IK variables.
- If IK variables are not wired, add temporary editor-assigned Control Rig or AnimGraph nodes for validation.
- Add TODO entries for final authored swim/wade assets after locomotion state is stable.

Validation:

- PIE outside swim: anim debug shows exterior swim.
- PIE flooded interior swim later: anim debug shows interior swim while `EmbarkState` remains `Embarked`.
- Walking/wading: foot IK remains available.
- Swimming: foot IK disabled.
- Hand IK only active in non-swim brace/contact states.

### Phase G - Authored Animation Pass

Goal:

- Replace weak procedural locomotion where it visibly fails.

Tasks:

- Decide whether to keep custom Blender skeleton or align to a retarget-compatible rig.
- Import or author minimal swim and wade clips.
- Build swim and wade blendspaces.
- Keep procedural layers for breathing, sub motion, and minor additive motion.
- Keep IK validation separate from locomotion clip import.

Validation:

- No arm roll regression.
- Swim/wade states transition from runtime state, not AnimBP-only guesses.
- Procedural additives do not fight authored base pose.

## Networking Notes

Current network path:

- `FSavedMove_SubCrew` serializes `EmbarkState`.
- `USubCrewMovementComponent::MoveAutonomous` applies reported grid state on server.

Requirements:

- Any new water contact state that affects authoritative movement must be either deterministic from replicated flood/sub state or sent in saved moves.
- Do not replicate visual-only water data.
- Do not let clients invent pressure or damage state.
- Interior swim must remain deterministic in submarine local-space where possible.
- Boundary handoff must serialize the state flip and enough velocity context for server convergence.

Recommended:

- Keep `FCrewImmersionSample` local for visuals.
- Server recomputes authoritative swim/walk transition when interior swim is implemented.
- If client prediction needs water contact, include compact water contact bits in the saved move only after the deterministic path is proven insufficient.
- Keep handoff events as explicit saved move payloads.
- If suction/ejection becomes gameplay-relevant, send compact handoff impulse data or recompute it server-side from the boundary and pressure state.

## Animation Audit Cross-Reference

This section maps the animation audit priorities to this architecture.

| Audit item | Meaning for this architecture | Action |
|---|---|---|
| P1 - arms leaning right | Skeleton axis / procedural arm rest issue, separate from water locomotion | Do not block water state work. Validate ABP and arm rest tuning before authored animation pass. |
| P2 - ground locomotion weak | Procedural locomotion is acceptable for validation, not final quality | Keep procedural for first playable; add authored pass after movement states are stable. |
| P3 - IK calculated but final application uncertain | Water states must explicitly enable/disable IK | Add debug flags and editor validation for IK wiring. Disable IK during swim. |
| P4 - swimming not complete control mode | Directly addressed by state axes and water contact state | Do not add `Swimming` to `ECrewControlMode` as a spatial state. Use water contact + movement resolver. |
| P5 - input ownership split | Swim input can create another conflict if added in multiple places | Keep PC owning IMCs, pawn exposing wrappers, movement component consuming movement intent. |
| P6 - peer smoothing absent | Relevant to interior swim because it stays grid-authoritative | Do not ship multiplayer interior swim without local-grid smoothing review. |
| P7 - collision/rollback bulkhead | Boundary handoff must log and own local/world conversion | Add handoff debug and avoid direct BP state flips. |
| P8 - posture capsule grow without clearance | Wading/swimming transitions can happen in cramped compartments | Keep posture/clearance as a separate fix before final cramped-space polish. |
| P9 - O2/audio/PP/ocean swim stubs | Water locomotion needs HUD/PP/audio signals but not final systems | Add minimal PP/audio hooks and debug values first. |

Current stale audit notes:

- The audit mentions exterior EVA as `MOVE_Flying`; the current direction is explicit Sub3D swim and later custom swim movement.
- The audit mentions `bIsSwimmingByFlood` as an anim source; this architecture replaces it with resolved locomotion/water state.
- The audit did not include the local-space interior swim distinction. This document supersedes that point.

## Unified Quick Wins

These are the lowest-risk steps that advance movement, animation, debug, and editor validation together.

### Quick Win 1 - One Swim Truth For Animation

Implement:

- `bIsSwimming` comes from final resolved movement state.
- `bIsInteriorSwim` and `bIsExteriorSwim` are exposed.
- `bIsSwimmingByFlood` is not used as the animation source of truth.

Verify:

- Debug HUD and Anim Debug show the same swimming state.

### Quick Win 2 - Water Contact Debug Block

Implement:

- Add debug lines for `EmbarkState`, `WaterContactState`, `Immersion01`, local/world water velocity, and head underwater.

Verify:

- Flooded compartment shows `Embarked` and water contact.
- Exterior ocean shows `Outside` and ocean contact.

### Quick Win 3 - Boundary Handoff Logging

Implement:

- Log boundary name, direction, local velocity, world velocity, and any handoff impulse.

Verify:

- Airlock/breach crossing has exactly one state flip.
- Suction/ejection experiments can be tuned from logs.

### Quick Win 4 - ABP IK Wiring Check

Implement:

- Open `ABP_Crew`.
- Verify whether foot and hand IK variables are wired.
- Add temporary debug-visible weights if needed.

Verify:

- Foot IK active while walking/wading.
- Foot IK inactive while swimming.
- Hand IK inactive while swimming unless explicitly bracing/grabbing.

### Quick Win 5 - Input Path Audit

Implement:

- Confirm `IA_Move` feeds only `ApplyCrewPlanarMoveInput`.
- Confirm swim vertical action feeds only `ApplyCrewVerticalMoveInput`.
- Confirm no Blueprint calls `SetMovementMode` directly for crew water movement.

Verify:

- No double input path.
- No dead swim vertical path.

### Quick Win 6 - Procedural Swim Tuning Pass

Implement:

- Make procedural swim respond to local swim input direction and vertical axis.
- Keep it clearly temporary.

Verify:

- Exterior swim reads visibly different for forward, strafe, ascend, descend.
- Interior swim later can reuse the same anim state with local-space velocity.

## Debug Log Requirements

Movement transition log must include:

- previous movement mode;
- new movement mode;
- `EmbarkState`;
- role;
- current compartment;
- current submarine;
- `Immersion01`;
- `WaterContactState`;
- whether a UE physics volume is water.

This is required because native Unreal movement can change mode during physics.

## Deferred Concerns

These are not required for the current implementation path:

- final underwater combat;
- monster grab logic;
- oxygen inventory/equipment;
- complete slosh simulation;
- final HUD art;
- final animation polish;
- network anti-cheat validation.

They should not block stabilization of exterior swim and water-contact data.
