# Crew animation and procedural debug implementation plan

Date: 2026-05-08
Workspace: `C:\Dev\Sub3D`
Status: implementation plan, no code change in this document.

Authority-max plan:

- `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

Related Sub3D references:

- `reports/2026-05-04_crew_animation_audit.md`
- `reports/plans/2026-04-23_procedural_crew_animation_architecture_and_execution_plan.md`
- `reports/plans/2026-05-04_water_implementation_plan.md`
- `reports/plans/2026-05-07_crew_water_locomotion_architecture.md`
- `reports/plans/2026-05-08_crew_animation_water_macro_execution_plan.md`

External local references inspected for design inspiration only:

- `G:\Steam\steamapps\common\Barotrauma\Content`
- `C:\Games\Subnautica\Subnautica_Data\StreamingAssets\SNUnmanagedData`

Important legal and technical rule:

- Do not copy Barotrauma or Subnautica code, data tables, assets, names, values, or behaviors directly into Sub3D.
- Use them only as reference for debug coverage, gameplay categories, and player feedback patterns.
- Subnautica compiled assemblies were not decompiled for this plan.

---

## 1. Goal

Build a complete runtime debug system for the crew animation and procedural movement stack.

The debug system must let us see, in PIE:

- what input was received;
- which movement frame was built;
- whether the crew is `Embarked`, `Outside`, walking, swimming, falling, or in an invalid mixed state;
- which water/pressure/oxygen values are affecting the crew;
- which procedural animation branch ran: walk, crawl, swim, breathing, posture, submarine motion, upper-body aim, IK, rest pose;
- which final per-bone rotations and pelvis translation are sent to `FAnimNode_CrewProcedural`;
- whether hand IK and foot IK were computed, weighted, and actually wired in the AnimBP;
- which warnings explain bad poses, missing wiring, or state conflicts.

This is a validation tool for gameplay and animation during First Playable. It is not final player UI.

---

## 2. Current state in code

### 2.1 Existing usable pieces

The current code already has these foundations:

| Area | Current file | Current behavior |
|---|---|---|
| Animation instance | `Source/Sub3D/Submarine/SubCrewAnimInstance.h/.cpp` | Computes walk, crawl, swim, breathing, posture, sub motion, upper-body aim, hand IK, foot IK, and final procedural bone values. |
| Procedural anim node | `Source/Sub3D/Submarine/AnimNode_CrewProcedural.h/.cpp` | Copies final values from `USubCrewAnimInstance` during `PreUpdate`, then applies 18 bone rotations and pelvis translation in `Evaluate_AnyThread`. |
| Locomotion structs | `Source/Sub3D/Submarine/CrewLocomotionTypes.h` | Defines `FCrewMoveIntent`, `FCrewLocomotionFrame`, IK states, stance, gait, and anim locomotion state. |
| Movement source | `Source/Sub3D/Submarine/SubCrewMovementComponent.h/.cpp` | Owns local grid movement, `EmbarkState`, swim movement, input intent, locomotion frame, hand probes, and foot IK traces. |
| Crew environment | `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` | Owns compartment, pressure, water height, immersion, oxygen, swimming state helpers, and hull crossing. |
| Debug settings | `Source/Sub3D/Debug/Sub3DDebugSettings.h` | Central project debug toggles for crew, flood, hull, movement, and related systems. |
| Gameplay debugger | `Source/Sub3D/Debug/Sub3DGameplayDebugger.cpp` | Shows context, submarine state, crew state, embark state, movement mode, control mode, compartment. |
| Anim console tuning | `Source/Sub3D/Submarine/SubPlayerController.cpp` | `Anim <param> <value>` and `AnimList` can inspect and edit current anim parameters. |
| Anim debug widget | `Source/Sub3D/Submarine/CrewAnimDebugWidget.h/.cpp` | Runtime tuner for procedural anim parameters, built dynamically in C++. |

### 2.2 Main gaps

The existing pieces do not yet answer the most important debug questions:

| Gap | Impact |
|---|---|
| No single per-frame debug snapshot | Input, movement, water, anim and IK must be read from separate files/logs. |
| No contribution breakdown | We can see final bone values, but not whether a value came from swim, posture, sub motion, upper body aim, or rest pose. |
| No conflict detector | Invalid combinations like `EmbarkState=Outside` and `MovementMode=Falling` are not elevated as first-class warnings in animation debug. |
| Existing widget is a tuner, not a diagnostic panel | It changes values but shows very little of the runtime state. |
| Existing widget builds its UI tree in C++ | Project rules prefer editor-assigned UMG widgets with explicit names and stable layout. |
| Anim node debug is minimal | `GatherDebugData` reports bone count and snapshot presence only. |
| IK application is still unclear | Movement and AnimInstance compute IK data, but the final AnimBP wiring must be validated in editor. |
| No trace recording | Hard to compare a bad frame before and after a tuning change. |
| No reference-frame view | Local sub frame, world frame, water frame, and camera direction are not shown together. |

---

## 3. Reference takeaways

### 3.1 Barotrauma reference takeaways

Inspected local content shows useful structural patterns:

- Characters are data-driven. A species declares ragdolls and animations through content files.
- Human animation configs expose walk, swim, cycle speed, movement speed, IK strength, torque, head/torso angles, and hand/foot motion categories.
- Ragdoll configs separate collider, limbs, joints, health indexes, attack priorities, damage modifiers, and sprite/body deformation.
- Creature files separate AI target priorities from attack definitions.
- Creature attacks are context-aware: inside, outside, water, ground, any.
- Some creatures support latch/attach behavior on walls, submarines, or characters.
- Swarm behavior uses simple cohesion and distance constraints.

Sub3D debug implication:

- The debug system should expose the crew as a data graph: body state, movement state, environment state, animation state, IK state, and final pose state.
- For future creatures, attach/latch behavior should be defined as data and debugged like movement: target, attach surface, limb/socket, detach speed, damage, cooldown, and current state.

### 3.2 Subnautica reference takeaways

Inspected local data and language/config files show useful feedback patterns:

- Player-facing survival warnings are simple and direct: oxygen, depth, crush depth, swim-to-surface, vehicle crush depth.
- Equipment can modify underwater traversal: fins, rebreather, oxygen source, vehicle systems.
- Environment feedback is grouped around survivability: depth, oxygen, pressure, vehicle safety, habitat oxygen, power.
- Local data includes player, oxygen, swim, creature, biome, signal, and streaming categories.

Sub3D debug implication:

- The debug panel should not only show raw C++ values. It should show the same state a player will eventually understand: inside/outside, water contact, oxygen, pressure, movement limitation, and swim authority.
- The debug panel should make wrong state transitions obvious before final HUD/audio/PP feedback is implemented.

### 3.3 Compatibility with the animation and water architecture plans

This debug plan wraps the current code types, but its field names must anticipate the target model in the related plans.

Current code:

- `ECrewEmbarkState`
- native `EMovementMode`
- `ECrewLocomotionStance`
- `ECrewLocomotionGait`
- raw water values on `ASubCrewCharacter`

Target architecture references:

- `reports/plans/2026-04-23_procedural_crew_animation_architecture_and_execution_plan.md`
- `reports/plans/2026-05-07_crew_water_locomotion_architecture.md`

Required naming rule:

- Use `TraversalDomain` in debug snapshots as the gameplay-facing locomotion domain. For the first pass, derive it from `EmbarkState + MovementMode`.
- Use `ReferenceFrameState` in debug snapshots as the spatial frame. For the first pass, derive it from `EmbarkState`.
- Use `WaterContactState` in debug snapshots. For the first pass, derive it from `CurrentWaterImmersion01`, `CurrentCompartmentId`, and `EmbarkState`.
- Use `ImmersionSample` in debug snapshots. For the first pass, build it from the current water height, immersion value, and source compartment/ocean context.

This prevents a rename pass when the canonical traversal and water states land later.

---

## 4. Architecture target

### 4.1 One debug source of truth

Add one runtime snapshot owned by the crew:

```text
ASubCrewCharacter
  UCrewAnimDebugComponent
    FCrewAnimDebugSnapshot CurrentSnapshot
    TCircularBuffer<FCrewAnimDebugSnapshot> RecentSnapshots
```

The component reads from existing systems. It does not own gameplay state.

Data flow:

```text
USubCrewMovementComponent
  LastMoveIntent
  LastLocomotionFrame
  EmbarkState
  MovementMode
  HandProbes
  FootIK states
  Last handoff / movement mode change

ASubCrewCharacter
  CurrentCompartmentId
  CurrentWaterHeightCm
  CurrentWaterImmersion01
  WaterContactState derived for debug
  ImmersionSample derived for debug
  CurrentAmbientPressureKPa
  PressureExposureSeconds
  HasOxygen()
  IsInWater()
  IsCrewSwimming()

USubCrewAnimInstance
  Input vars
  procedural phase vars
  final bone outputs
  IK targets / weights
  contribution breakdown when debug is enabled

UCrewAnimDebugComponent
  builds CurrentSnapshot once per debug tick
  renders panel data
  draws 3D debug
  records trace buffers
```

### 4.2 Thread safety rule

Do not read or write UObject state from `FAnimNode_CrewProcedural::Evaluate_AnyThread`.

Allowed:

- `USubCrewAnimInstance::NativeUpdateAnimation` writes game-thread debug data.
- `FAnimNode_CrewProcedural::PreUpdate` copies POD pose data from the AnimInstance.
- `FAnimNode_CrewProcedural::GatherDebugData` reports limited node debug text.

Deferred:

- Per-bone post-evaluation capture from worker thread. It is not required for the first debug pass.

### 4.3 Debug ownership

| Owner | Responsibility |
|---|---|
| `USubCrewMovementComponent` | Movement and IK source values. |
| `ASubCrewCharacter` | Environment and water source values. |
| `USubCrewAnimInstance` | Animation input, procedural branch output, final bone values. |
| `FAnimNode_CrewProcedural` | Node health: bone resolution, snapshot presence, applied bone count. |
| `UCrewAnimDebugComponent` | Snapshot aggregation, warnings, draw debug, recording. |
| `ASubPlayerController` | Console commands and local panel visibility. |
| `USub3DDebugSettings` | Persistent project debug toggles. |
| `FSub3DGameplayDebuggerCategory` | Non-UMG diagnostic summary. |

---

## 5. Snapshot data contract

Add `Source/Sub3D/Submarine/CrewAnimDebugTypes.h`.

### 5.1 `FCrewAnimDebugSnapshot`

Required fields:

| Field | Purpose |
|---|---|
| `FrameCounter` | Correlate logs, panel, and trace records. |
| `WorldTimeSeconds` | Time axis for traces. |
| `OwnerName` | Which crew actor produced the snapshot. |
| `NetRole` | Authority/autonomous/simulated. |
| `bLocallyControlled` | Distinguish local validation from peer display. |
| `TraversalDomain` | Gameplay-facing locomotion domain derived from current state now, mapped to the canonical state model later. |
| `ReferenceFrameState` | Spatial frame: submarine-local, world-space, or transition. Derived from `EmbarkState` for the first pass. |
| `MovementMode` | Raw native CMC mode. Keep this for diagnosing Unreal fallback behavior. |
| `EmbarkState` | Raw current frame state. Keep this until the canonical reference-frame state replaces it. |
| `ControlMode` | OnFoot/HelmDriving/StationUI. |
| `CurrentSubmarineName` | Current sub binding. |
| `CurrentCompartmentId` | Current compartment or none. |
| `WaterContactState` | Debug-facing water contact state: dry, shallow wade, deep wade, swimming, ocean. |
| `ImmersionSample` | Water source, water height, immersion, and sampling validity. |
| `bInWater` | Crew environment water state. |
| `bHasOxygen` | Oxygen availability. |
| `WaterHeightCm` | Current compartment water height. |
| `WaterImmersion01` | Current crew immersion. |
| `AmbientPressureKPa` | Current pressure. |
| `PressureExposureSeconds` | Pressure timer. |
| `MoveIntent` | Last move intent. |
| `LocomotionFrame` | Last locomotion frame. |
| `WorldVelocity` | Current movement velocity. |
| `LocalVelocity` | Current sub-local velocity when available. |
| `RelativeLinearVelocity` | Local grid velocity. |
| `ControlRotation` | Camera/controller direction. |
| `MeshWorldRotation` | Mesh orientation check. |
| `bAnimInstanceValid` | Anim graph has valid parent instance. |
| `bProceduralNodeSeen` | Node health flag from AnimInstance/node debug. |
| `AnimInputs` | Speed, direction, stance, gait, swim, run, posture, support. |
| `AnimPhases` | Walk phase, swim phase, breath phase if exposed. |
| `RequestedFinalPose` | Per-bone procedural rotations and pelvis offset requested by `USubCrewAnimInstance`. |
| `AppliedBoneTransforms` | Bone transforms read from the skeletal mesh after evaluation. Used to distinguish requested pose from applied pose. |
| `ContributionBreakdown` | Optional per-branch bone deltas. |
| `HandIK` | Left/right targets, weights, selected probe source. |
| `FootIK` | Left/right traces, hit, offset, plant alpha, weight. |
| `Warnings` | Computed state conflicts and missing wiring warnings. |

### 5.2 Warnings

Warnings are first-class data, not only log text.

Initial warnings:

| Warning id | Condition |
|---|---|
| `OutsideNotSwimming` | `EmbarkState == Outside` and `MovementMode != MOVE_Swimming`. |
| `SwimmingButEmbarkedDry` | `MovementMode == MOVE_Swimming`, `ReferenceFrameState == SubmarineLocal`, and `WaterContactState == Dry`. |
| `AnimSwimMismatch` | Anim `bIsSwimming` differs from movement component swimming state. |
| `InvalidCompartmentOutside` | `EmbarkState == Outside` but `CurrentCompartmentId` is set. |
| `NoAnimInstance` | Crew mesh has no anim instance or has an anim instance that is not `USubCrewAnimInstance`. |
| `NoProceduralSnapshot` | Anim node reports no copied pose snapshot. |
| `NoResolvedBones` | Procedural node resolved zero bones. |
| `IKComputedButNoAnimGraphUse` | IK states have non-zero weights but the editor validation flag says the ABP is not wired. |
| `HandIKActiveDuringSwim` | Hand IK weight remains above threshold while `WaterContactState == Swimming` or `WaterContactState == Ocean`. |
| `FootIKActiveDuringSwim` | Foot IK weight remains above threshold while `WaterContactState == Swimming` or `WaterContactState == Ocean`. |
| `RestPoseDominatesArm` | Arm rest rotation magnitude is above a configured threshold and movement contribution is small. |
| `GridVelocitySpike` | Relative velocity exceeds `CrewJitterWarnVelocityCmPerSec`. |

### 5.3 Contribution breakdown

Add optional capture in `USubCrewAnimInstance`.

When enabled:

```text
Reset
ReadInputState
WalkOrCrawlOrSwim
Breathing
Posture
SubMotion
UpperBodyAim
HandIK
FootIK
RestPose
Final
```

Implementation method:

- Store final procedural bone array after each branch only when debug capture is enabled.
- Guard contribution capture with `#if !UE_BUILD_SHIPPING`.
- Early-out before the first pose copy when `bCaptureCrewAnimContributions` is false.
- Compute differences in debug code, not in shipping path.
- Do not log every frame by default.

This makes the arm problem directly visible:

```text
upperarm_r final roll = -85
  RestPose = -85
  Walk = 0
  Aim = 0
  SubMotion = 0
```

### 5.4 Requested pose vs applied pose

The debug system must distinguish two things:

| Data | Source | Meaning |
|---|---|---|
| `RequestedFinalPose` | `USubCrewAnimInstance` | What the procedural code requested for each tracked bone. |
| `AppliedBoneTransforms` | `USkeletalMeshComponent::GetBoneTransform` after animation evaluation | What the skeletal mesh is actually showing at runtime. |

This distinction is required for IK and AnimBP validation.

Examples:

- If foot IK is computed but no final foot bone or IK node changes, the likely issue is AnimGraph wiring.
- If requested arm rotation is already wrong, the likely issue is procedural contribution, rest pose, or bone axis mapping.
- If requested pose looks correct but applied pose is wrong, the likely issue is AnimGraph order, additive order, mesh import, or another node after the procedural node.

---

## 6. Code changes by file

### 6.1 New files

| File | Purpose |
|---|---|
| `Source/Sub3D/Submarine/CrewAnimDebugTypes.h` | Snapshot structs, warning enum, pose arrays, helper names. |
| `Source/Sub3D/Submarine/CrewAnimDebugComponent.h` | Runtime component public API. |
| `Source/Sub3D/Submarine/CrewAnimDebugComponent.cpp` | Snapshot aggregation, warning build, 3D draw, trace buffer, CSV dump. |
| `Source/Sub3D/Submarine/CrewAnimDebugPanelWidget.h` | Editor-bound diagnostic panel class. |
| `Source/Sub3D/Submarine/CrewAnimDebugPanelWidget.cpp` | Panel data binding and display update code. |

### 6.2 Modified files

| File | Change |
|---|---|
| `Source/Sub3D/Submarine/SubCrewCharacter.h` | Add `UCrewAnimDebugComponent* CrewAnimDebugComponent`; add editor-assigned widget class only if panel spawning stays on character. |
| `Source/Sub3D/Submarine/SubCrewCharacter.cpp` | Create default debug component in constructor; expose environment sample helper. |
| `Source/Sub3D/Submarine/SubCrewMovementComponent.h` | Add debug getters for hand probes, last movement mode transition, last handoff, and swim/mode context. |
| `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp` | Populate last transition data in `SetMovementMode`, `OnMovementModeChanged`, `SetEmbarkState`, and hull handoff packet handling. |
| `Source/Sub3D/Submarine/SubCrewAnimInstance.h` | Add debug snapshot getters, pose output helper, optional contribution capture storage. |
| `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp` | Fill anim debug output after each compute branch when debug is enabled. |
| `Source/Sub3D/Submarine/AnimNode_CrewProcedural.h` | Add lightweight node debug state: resolved bone count, snapshot copied, missing bone names if needed. |
| `Source/Sub3D/Submarine/AnimNode_CrewProcedural.cpp` | Update node debug state in `PreUpdate`, `ResolveBones`, and `GatherDebugData`. |
| `Source/Sub3D/Submarine/CrewAnimDebugWidget.h/.cpp` | Keep as temporary legacy tuner only. Do not convert it during the first diagnostic panel pass. |
| `Source/Sub3D/Submarine/CrewAnimDebugPanelWidget.h/.cpp` | New editor-bound diagnostic panel controller. |
| `Source/Sub3D/Submarine/SubPlayerController.h/.cpp` | Add debug exec commands and local panel spawn/toggle. |
| `Source/Sub3D/Debug/Sub3DDebugSettings.h` | Add persistent debug toggles and thresholds. |
| `Source/Sub3D/Debug/Sub3DGameplayDebugger.cpp` | Add an `ANIM` section to the existing crew block. |

---

## 7. Debug settings

Add settings under `USub3DDebugSettings`.

```cpp
UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bDrawCrewAnimDebug = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bShowCrewAnimDebugPanel = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bLogCrewAnimWarnings = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bCaptureCrewAnimContributions = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bDrawCrewAnimBones = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bDrawCrewAnimIK = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation")
bool bDrawCrewAnimFrameAxes = false;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation", meta = (ClampMin = "0.02"))
float CrewAnimDebugSampleIntervalSeconds = 0.1f;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation", meta = (ClampMin = "1"))
int32 CrewAnimDebugRingBufferFrames = 600;

UPROPERTY(Config, EditAnywhere, Category = "Crew|Animation", meta = (ClampMin = "0.0"))
float CrewAnimArmRestWarningDegrees = 60.f;
```

Rules:

- Settings are persistent.
- Console commands can toggle runtime values.
- Per-frame logging remains off by default.
- Debug code must be compiled out or inert in Shipping where practical.

---

## 8. Console commands

Extend `ASubPlayerController`.

Existing:

- `Anim <param> <value>`
- `AnimList`

New commands:

| Command | Behavior |
|---|---|
| `CrewAnimDebug 0/1` | Toggle the whole local crew animation debug system. |
| `CrewAnimPanel 0/1` | Show/hide diagnostic panel. |
| `CrewAnimDraw 0/1` | Master draw toggle for 3D debug. |
| `CrewAnimDrawBones 0/1` | Draw selected bone axes and pose labels. |
| `CrewAnimDrawIK 0/1` | Draw hand probes, hand targets, foot traces, foot offsets. |
| `CrewAnimDrawFrames 0/1` | Draw world/sub/camera/move direction axes. |
| `CrewAnimDump` | Dump current snapshot to log. |
| `CrewAnimDumpRecent <seconds>` | Dump recent ring buffer to CSV. |
| `CrewAnimTraceStart` | Start recording snapshots to memory. |
| `CrewAnimTraceStop` | Stop recording and write CSV. |
| `CrewAnimFreeze 0/1` | Freeze displayed snapshot while gameplay continues. |
| `CrewAnimSelectBone <name>` | Select one bone for detailed panel and 3D axis draw. |
| `CrewAnimResetParams` | Reset runtime anim tuning parameters to class defaults. |

Do not remove `Anim` and `AnimList`. They remain useful for fast tuning.

Add `AnimList` categories for:

- axes;
- rest pose;
- walk/run;
- crouch/crawl;
- swim;
- body/aim;
- IK;
- current live state.

---

## 9. UMG panel

### 9.1 Asset list

Create editor-assigned assets:

| Asset | Class | Purpose |
|---|---|---|
| `Content/Sub3D/UI/Debug/WBP_CrewAnimDebugPanel.uasset` | `UCrewAnimDebugPanelWidget` | Main diagnostic panel. |
| `Content/Sub3D/UI/Debug/WBP_CrewAnimDebugRow.uasset` | `UUserWidget` | Reusable name/value/warning row. |
| `Content/Sub3D/UI/Debug/WBP_CrewAnimBoneList.uasset` | `UUserWidget` | Bone output table. |
| `Content/Sub3D/UI/Debug/WBP_CrewAnimTimeline.uasset` | `UUserWidget` | Recent warning/state timeline. |
| `Content/Sub3D/UI/Debug/WBP_CrewAnimTuningPanel.uasset` | `UUserWidget` | Existing sliders migrated to editor-bound widgets. |

Optional data assets:

| Asset | Purpose |
|---|---|
| `DA_CrewAnimDebugProfile` | Default sample interval, shown tabs, selected bones, warning thresholds. |
| `DA_CrewBoneDebugMap` | Stable bone display names and grouping. |
| `DA_CrewProceduralParamCatalog` | Parameter names, ranges, default values, console aliases. |

### 9.2 Panel tabs

Use tabs or a compact segmented control:

| Tab | Required fields |
|---|---|
| Summary | Warnings, role, mode, embark state, swim state, compartment, water, oxygen, pressure. |
| Input/Move | Move axis, vertical axis, camera direction, move direction, speed, local/world velocity, yaw targets. |
| Anim State | Speed, direction, stance, gait, posture, run, swim, phases. |
| Procedural | Final bone outputs, contribution breakdown, selected bone detail. |
| IK | Hand probes, hand targets, foot traces, weights, plant alphas. |
| Water/EVA | `ReferenceFrameState`, `TraversalDomain`, `WaterContactState`, hull handoff, immersion sample, pressure, oxygen, movement protection. |
| Network | net role, local control, grid transform, replicated state, last server move handoff. |
| Tuning | Existing runtime sliders and reset button. |

### 9.3 UI implementation rule

Do not build the final debug panel by constructing all widget children in C++.

Use editor-assigned `UPROPERTY(meta=(BindWidget))` members for:

- text fields;
- tab buttons;
- scroll boxes;
- warning rows;
- bone list containers;
- tuning controls.

The current `UCrewAnimDebugWidget` can remain temporarily as a legacy tuner only if the new diagnostic panel is introduced separately.

Decision for this implementation:

- Add a new `UCrewAnimDebugPanelWidget` for the diagnostic panel.
- Keep `UCrewAnimDebugWidget` as a temporary legacy tuner.
- Mark the legacy tuner with `// TODO: post-FP` when code is touched.
- Add the cleanup entry to `reports/backlog/post_fp_debt.md`.
- Do not migrate the tuner during the first diagnostic implementation pass.

---

## 10. 3D debug visualization

Draw only for the locally controlled crew unless explicitly toggled for all crews.

### 10.1 Frame axes

Draw:

- world axes at crew capsule;
- sub-local axes at `GridSpaceTransform` rebased into world;
- camera forward;
- desired movement direction;
- current velocity direction;
- hull handoff normal when recently crossed.

### 10.2 Bone debug

Draw for selected bones:

- current bone world transform axes from `USkeletalMeshComponent::GetBoneTransform`;
- final procedural rotation values as text;
- contribution bars in panel only, not 3D text spam.

Default selected bones:

- `pelvis`
- `spine_03`
- `spine_05`
- `upperarm_r`
- `upperarm_l`
- `lowerarm_r`
- `lowerarm_l`
- `thigh_r`
- `thigh_l`
- `foot_r`
- `foot_l`

### 10.3 IK debug

Hand IK:

- draw six hand probe rays;
- draw hit points;
- draw selected left/right target;
- label weight and hit distance.

Foot IK:

- draw foot trace start/end;
- draw impact point and normal;
- draw target offset direction;
- label weight and plant alpha.

### 10.4 Water and environment debug

Draw:

- current compartment water surface sample near the crew;
- current compartment bounds if already enabled by flood debug;
- current immersion threshold line on the capsule;
- outside ocean swim marker when `EmbarkState == Outside`.

This must reuse existing flood/compartment debug settings where possible.

---

## 11. Gameplay Debugger integration

Extend `FSub3DGameplayDebuggerCategory`.

Add under each crew:

```text
ANIM
  Anim=valid Node=valid ResolvedBones=18 Snapshot=1
  Speed=123 Dir=-12 Stance=Standing Gait=Walk Swim=0 Run=0 Posture=1.00
  Phase walk=0.42 swim=0.00 Support=0.86
  IK Hand L/R=0.00/0.00 Foot L/R=0.75/0.80
  Warnings: none
```

If warnings exist, display them before normal values.

Do not print per-bone tables in Gameplay Debugger. Use the UMG panel and trace export for that.

---

## 12. Trace recording

### 12.1 CSV first

First implementation should write CSV, not JSON.

Output:

```text
Saved/Sub3D/CrewAnimDebug/CrewAnimTrace_YYYYMMDD_HHMMSS.csv
```

Initial columns:

```text
Time,Frame,Crew,Role,Local,TraversalDomain,ReferenceFrame,Embark,MoveMode,
ControlMode,Sub,Comp,WaterContact,WaterImmersion,Oxygen,Pressure,
MoveX,MoveY,MoveZ,Speed,Direction,Stance,Gait,AnimSwim,CMCswim,
Posture,Support,WalkPhase,SwimPhase,HandLWeight,HandRWeight,
FootLWeight,FootRWeight,Warnings
```

Add per-bone export as a second CSV when needed:

```text
CrewAnimTrace_YYYYMMDD_HHMMSS_Bones.csv
```

Columns:

```text
Time,Frame,Crew,Bone,FinalPitch,FinalYaw,FinalRoll,Walk,Swim,Breathing,Posture,SubMotion,Aim,Rest
```

### 12.2 Ring buffer

The component keeps a ring buffer even when not actively recording.

Default:

- 600 frames at 0.1 second interval = 60 seconds of recent state.

Command:

- `CrewAnimDumpRecent 10`

This writes the last 10 seconds.

---

## 13. Implementation phases

### Phase A - Baseline and types

Files:

- `CrewAnimDebugTypes.h`
- `Sub3DDebugSettings.h`

Tasks:

- Add snapshot structs.
- Add warning enum.
- Add debug settings.
- Add helper string conversion functions for movement mode, embark state, stance, gait, warnings.

Exit criteria:

- Project compiles.
- No gameplay behavior changed.
- Snapshot structs are BlueprintType only when needed by UMG.

### Phase B - Debug component

Files:

- `CrewAnimDebugComponent.h/.cpp`
- `SubCrewCharacter.h/.cpp`

Tasks:

- Add `UCrewAnimDebugComponent`.
- Create default subobject in `ASubCrewCharacter`.
- In `BeginPlay`, call `AddTickPrerequisiteComponent` for the crew movement component and the skeletal mesh component.
- Treat missing tick prerequisites as a warning in logs, not as a silent fallback.
- Build base snapshot from crew, movement, and environment.
- Build warning list.
- Add ring buffer.

Exit criteria:

- `CrewAnimDump` logs a complete snapshot for local crew.
- No crash if crew has no sub, no compartment, no mesh, or no anim instance.
- Snapshot timing is not one frame behind movement or skeletal mesh evaluation in the standard `BP_SubmarineCrew` setup.

### Phase C - Movement debug hooks

Files:

- `SubCrewMovementComponent.h/.cpp`

Tasks:

- Add getters for hand probes.
- Add last movement mode change data.
- Add last embark transition data.
- Add last handoff kind/time.
- Add swim context fields: outside swim, physics volume water, fallback blocked, custom swim path active.

Exit criteria:

- Snapshot shows why a crew is swimming, walking, falling, or blocked from falling.
- Warnings identify `OutsideNotSwimming` deterministically.

### Phase D - AnimInstance debug hooks

Files:

- `SubCrewAnimInstance.h/.cpp`

Tasks:

- Add function to copy all final procedural bone rotations into a debug array.
- Expose final pelvis offset.
- Expose walk/swim/breath phases.
- Add optional contribution capture around compute branches.
- Track whether hand/foot IK values were calculated.

Exit criteria:

- Panel and dump show final per-bone values.
- Contribution capture can prove which branch is producing a bad arm/spine/leg pose.
- Debug disabled path has minimal overhead.

### Phase E - Procedural node health

Files:

- `AnimNode_CrewProcedural.h/.cpp`

Tasks:

- Track resolved bone count.
- Track missing expected bones.
- Track whether `PreUpdate` copied a snapshot.
- Expand `GatherDebugData` with missing bone names only when relevant.

Exit criteria:

- `ShowDebug Animation` reports node health.
- Central snapshot can show node health if exposed through AnimInstance.

### Phase F - Editor validation checklist

Documents:

- Update this plan with validation results.
- Optionally add `reports/guides/2026-05-08_crew_anim_debug_editor_handoff.md` after implementation.

Editor checks:

- Open `ABP_Crew`.
- Confirm parent class is `USubCrewAnimInstance`.
- Confirm `AnimGraphNode_CrewProcedural` is present in the final pose path.
- Confirm whether foot IK variables are wired.
- Confirm whether hand IK variables are wired.
- Confirm the diagnostic panel has explicit fields for `computed`, `wired`, and `applied`.
- Confirm `SK_Crew_Basic` preview pose and bone axes for arms.
- Confirm `BP_SubmarineCrew` uses the expected mesh and `ABP_Crew`.

Exit criteria:

- The debug panel can distinguish "IK not computed" from "IK computed but not applied".
- The arm rest / bone axis issue has a repeatable editor validation path.
- UMG panel requirements are clear before the panel is built.

### Phase G - UMG diagnostic panel

Files:

- `CrewAnimDebugPanelWidget.h/.cpp`.
- `SubPlayerController.h/.cpp`

Assets:

- `WBP_CrewAnimDebugPanel`
- `WBP_CrewAnimDebugRow`
- `WBP_CrewAnimBoneList`
- `WBP_CrewAnimTimeline`
- `WBP_CrewAnimTuningPanel`

Tasks:

- Create editor-assigned WBP.
- Add widget class property to PlayerController or Character.
- Add panel toggle command.
- Populate summary/input/anim/procedural/IK/water/network/tuning tabs.
- Keep text small and stable. This is a debug tool, not a marketing UI.

Exit criteria:

- The panel opens in PIE.
- It updates without reallocating the full widget tree every frame.
- It shows warnings first.
- Existing tuning commands still work.

### Phase H - 3D draw debug

Files:

- `CrewAnimDebugComponent.cpp`

Tasks:

- Draw frame axes.
- Draw move and camera vectors.
- Draw hand probe rays.
- Draw foot trace rays.
- Draw selected bone axes.
- Draw water/immersion markers using existing compartment/flood data.

Exit criteria:

- A bad swim/walk transition can be diagnosed visually without reading logs.
- Draw debug respects settings and local-only defaults.

### Phase I - Trace recording

Files:

- `CrewAnimDebugComponent.cpp`
- `SubPlayerController.h/.cpp`

Tasks:

- Add start/stop trace commands.
- Add dump recent command.
- Write CSV under `Saved/Sub3D/CrewAnimDebug`.
- Include warnings as stable ids.

Exit criteria:

- A 10 second repro can be saved and compared after a code or tuning change.
- Trace export does not require editor assets.

### Phase J - Gameplay Debugger summary

Files:

- `Sub3DGameplayDebugger.cpp`

Tasks:

- Add compact anim block per crew.
- Show warnings.
- Show mode/embark/swim/stance/gait/IK summary.

Exit criteria:

- Pressing Gameplay Debugger key shows enough anim state to diagnose a remote peer or non-local crew.

---

## 14. Expected quick wins after implementation

### 14.1 Arm pose diagnosis

The contribution breakdown should immediately answer:

- Is the right arm leaning because of `ArmRestR`?
- Is the left/right mirror sign wrong?
- Is upper-body aim adding twist when it should not?
- Are clavicles missing from the procedural node and leaving the arm chain unbalanced?
- Are bone axes wrong for one side only?

### 14.2 Swim diagnosis

The debug snapshot should immediately answer:

- Is CMC in `MOVE_Swimming`?
- Is `EmbarkState` `Outside` or `Embarked`?
- Did `SetMovementMode(MOVE_Falling)` get converted to swimming?
- Is `IsInWater()` returning true because of outside state or a water physics volume?
- Is the anim instance reading `bIsSwimming` from movement or from stale flood state?

### 14.3 IK diagnosis

The panel should immediately answer:

- Did foot traces hit?
- Is the plant alpha zero because the crew is moving/swimming?
- Are foot offsets computed but never applied in AnimBP?
- Are hand probes finding braces?
- Are hand weights disabled during swim?

### 14.4 Water/EVA diagnosis

The panel should immediately answer:

- Is the crew in submarine local frame or world frame?
- Is current water immersion coming from compartment state or outside ocean?
- Is oxygen/pressure feedback consistent with inside/outside?
- Did a hull boundary crossing fire recently?

---

## 15. Future gameplay extensions unlocked by this debug base

These are not part of the first implementation pass. They show why the debug architecture should expose attach, water, IK, and frame data clearly.

### 15.1 Latching creatures

Inspired by Barotrauma content patterns, Sub3D can later add creatures that attach to:

- exterior hull;
- breach edges;
- airlock frames;
- pipes/rails inside flooded compartments;
- crew character sockets.

Needed debug fields later:

- attach target actor/component;
- attach surface normal;
- attach socket/bone;
- attach state: searching, approaching, attached, damaging, detaching, stunned;
- detach condition: speed, damage, timer, tool hit, pressure pulse;
- damage target: hull, breach, item, crew.

### 15.2 Creature behavior quick wins

Good First Playable-friendly behaviors:

- small hull parasite that reduces battery/power while attached;
- breach biter that expands a breach until removed;
- pipe crawler that blocks a corridor during flooding;
- sonar-attracted predator that approaches only when active pinging is used;
- swarm of small creatures that flee lights but attack damaged hull;
- interior flooded-compartment ambusher that uses water volume as navigation permission.

These should be implemented later through data assets, not hardcoded per creature class.

### 15.3 Player feedback quick wins

Inspired by Subnautica-style readability:

- oxygen warning states;
- pressure warning states;
- depth/crush warning states for sub and crew;
- swim equipment modifiers;
- water contact audio/PP feedback;
- clear transition feedback when leaving/entering the submarine frame.

The animation debug snapshot should already expose the data required by those future HUD/audio/PP systems.

---

## 16. Verification plan

### 16.1 Static verification

Run after implementation:

```text
git diff --check
```

Compile check should be run by the user or by the agent when explicitly allowed.

### 16.2 PIE scenarios

Required scenarios:

| Scenario | Expected debug result |
|---|---|
| Standing idle inside sub | `Embarked`, `Walking`, no swim warning, idle gait. |
| Walking inside sub | move intent, local velocity, walk phase, foot IK visible. |
| Sprinting inside sub | run flag, run gait, increased procedural amplitudes. |
| Crouch/prone | posture alpha and stance/gait update; capsule state visible separately. |
| Enter water inside compartment | `Embarked` remains true; water/immersion changes; no outside warning. |
| Exit through hull boundary | `Outside`, `Swimming`, current compartment none, outside swim context. |
| Re-enter through boundary | `Embarked`, walking restored, grid transform seeded. |
| Force bad Falling case | warning `OutsideNotSwimming` or movement fallback conversion visible. |
| Hand brace near wall | hand probe hit and weight visible. |
| Foot IK on uneven floor | foot hit, offset, plant alpha visible. |
| Swim | foot/hand IK disabled, swim phase visible, swim procedural branch visible. |
| Remote peer | Gameplay Debugger shows replicated movement/anim summary without local-only panel. |

### 16.3 Failure conditions

Implementation is not accepted if:

- debug off changes gameplay behavior;
- panel crashes when no anim instance exists;
- per-frame logs are emitted by default;
- UMG panel rebuilds its full tree every tick;
- `Evaluate_AnyThread` touches UObjects;
- outside swim can silently display as falling without a warning;
- IK computed/not-applied remains indistinguishable.

---

## 17. Implementation order recommendation

Recommended order:

1. Add snapshot structs and debug settings.
2. Add `UCrewAnimDebugComponent` and `CrewAnimDump`.
3. Add movement/environment snapshot fields, including `TraversalDomain`, `ReferenceFrameState`, `WaterContactState`, and `ImmersionSample`.
4. Add AnimInstance requested pose snapshot.
5. Add applied skeletal mesh pose sampling.
6. Add contribution breakdown.
7. Add warnings.
8. Validate ABP/IK wiring in editor before building the panel.
9. Add UMG diagnostic panel through `UCrewAnimDebugPanelWidget`.
10. Add 3D draw debug.
11. Add CSV trace recording.
12. Add Gameplay Debugger anim block.

Reason:

- The first useful result should be a deterministic text snapshot.
- The panel and draw debug should consume the same snapshot, not create their own reads.
- The panel requirements depend on the ABP/IK validation result, so editor validation comes before panel implementation.
- Trace recording should come after warning ids are stable.

---

## 18. Non-goals for this pass

Do not implement in this debug pass:

- authored animation replacement;
- Control Rig migration;
- IK Rig retargeting;
- creature AI;
- latch/attach gameplay;
- final player HUD;
- final oxygen or pressure gameplay balancing;
- network smoothing refactor;
- large movement rewrite.

Those are downstream decisions. This plan gives us the debug visibility needed to implement them safely later.
