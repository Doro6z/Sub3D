# Sub3D - Character Pipeline Editor Handoff

Date: 2026-04-14
Scope: editor execution handoff for the current crew character pipeline
Authority-max plan:
- `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

Supporting references:
- `reports/plans/2026-04-10_firstplayablerun_editor_session_contract.md`
- `reports/plans/character_pipeline_architecture.md`

Workspace: `C:\Dev\Sub3D`
Build target validated: `Sub3DEditor Win64 Development`

---

## 0. Why this handoff exists

The character pipeline exists in code, but it is not fully closed in editor.

The main gap is not "missing C++ only".
The main gap is mixed:
- some runtime behavior is implemented in C++
- some Blueprint wiring is still required
- some planned features are described in the architecture doc but are not actually closed in editor
- some comments and summaries from the previous session overstated the completion state

This handoff is the execution document for the editor phase.
It does not replace the authority-max plan.

---

## 1. Current code state

### 1.1 Validated in C++

The following points are implemented and compiled:

- posture scalar on `USubCrewMovementComponent`
- run state on `USubCrewMovementComponent`
- posture-driven capsule and FPS camera height interpolation
- run gating by posture
- run gating by swim state
- relative locomotion speed based on submarine-local movement, not world drift
- flood-driven water immersion and swim movement mode
- procedural anim input feed in `USubCrewAnimInstance`
- swim cycle in `USubCrewAnimInstance`
- hand probe data generation in `USubCrewMovementComponent`
- foot IK trace data generation in `USubCrewMovementComponent`
- FPS local head hide in `ASubCrewCharacter`
- console debug commands `Anim` and `AnimList`
- server bridge for run and posture requests

### 1.2 Not closed in editor

The following points still require editor work or remain blocked:

- `BP_SubmarineCrew` input wiring for `IA_Run`
- `BP_SubmarineCrew` input wiring for `IA_PostureScroll`
- actual Anim Blueprint asset validation on the placed crew mesh
- actual AnimGraph wiring of `Crew Procedural Animation` node
- actual hand IK application in AnimGraph or Control Rig
- actual foot IK application in AnimGraph or Control Rig
- runtime debug widget instantiation
- TPS camera implementation and toggle path

### 1.3 Not implemented yet

The following points are still absent in the current implementation path:

- real TPS camera runtime path in C++
- local camera mode bool and toggle API in `ASubCrewCharacter`
- hard clearance test before growing the capsule upward
- proper crawl-specific locomotion state
- proper crouch-specific locomotion state
- proper prone-specific locomotion state
- robust hand IK disable when the player is at a station or at helm
- runtime creation or binding of `CrewAnimDebugWidget`

---

## 2. Build result

Validated on 2026-04-14:

```text
Build.bat Sub3DEditor Win64 Development C:\Dev\Sub3D\Sub3D.uproject -WaitMutex -NoHotReloadFromIDE
Result: Succeeded
```

This confirms:
- current C++ compiles
- current module links
- the editor can be launched on the resulting binaries

This does not confirm:
- Blueprint asset wiring
- AnimBP graph correctness
- Control Rig correctness
- PIE runtime validation of every character feature

---

## 3. Blocking truths before opening the editor

These points must be treated as facts during the session:

### 3.1 The character pipeline is not "done"

It is in a mixed state:
- mechanics partly implemented
- editor wiring partly missing
- procedural layers present, but not all validated

### 3.2 The posture system is mechanical first, not animation-complete

Current posture does this:
- scales capsule height
- scales FPS camera Z
- scales walk speed
- bends the spine chain procedurally

Current posture does not yet provide:
- a true crouch locomotion pose set
- a true crawl locomotion pose set
- a true prone body support pose

### 3.3 The hand IK system currently exposes data, not a proven final pose result

In code, probes exist and targets/weights are exposed.
That is not the same thing as a validated final arm pose in editor.

### 3.4 TPS is still design guidance, not implementation

`IA_ToggleCamera` exists as an asset in content, but there is no complete C++ runtime path for:
- second camera component
- spring arm
- local camera mode state
- local camera toggle function

Do not report TPS as implemented.

---

## 4. Exact editor execution order

Execute in this order.

### Step 1 - Restart editor on the fresh build

Action:
1. Close Unreal Editor completely if it is already open.
2. Launch `Sub3D.uproject`.

Validation:
- project opens without module rebuild prompt
- Output Log shows normal module load, no `Sub3D` load failure

Failure:
- if UE asks to rebuild modules again, stop and verify you launched the same workspace

### Step 2 - Verify the player Blueprint parentage

Target asset:
- `Content/Sub3D/Blueprint/PlayerBP/BP_SubmarineCrew`

Action:
1. Open `BP_SubmarineCrew`.
2. In Class Settings, verify Parent Class.

Required result:
- parent must be `SubCrewCharacter` or a direct subclass that preserves the current C++ path

Failure:
- if parent is not on the `ASubCrewCharacter` path, stop and do not continue the handoff as-is

### Step 3 - Verify the mesh uses the correct Anim Instance path

Action:
1. In `BP_SubmarineCrew`, select the inherited skeletal mesh component.
2. In Details, check:
   - Skeletal Mesh asset assigned
   - Animation Mode
   - Anim Class

Required result:
- Animation Mode must use an Anim Blueprint
- that Anim Blueprint must be based on `USubCrewAnimInstance`

Validation:
1. Open the Anim Blueprint.
2. In Class Settings, verify Parent Class is `SubCrewAnimInstance`.

Failure:
- if the Anim Blueprint is not derived from `USubCrewAnimInstance`, the current procedural code path will not drive the pose

### Step 4 - Verify the AnimGraph structure

The current code exposes a custom anim graph node named:
- `Crew Procedural Animation`

It is backed by:
- `FAnimNode_CrewProcedural`
- `UAnimGraphNode_CrewProcedural`

Action:
1. In the crew Anim Blueprint, open the AnimGraph.
2. Verify there is a `Crew Procedural Animation` node.
3. Verify it is actually in the active graph path leading to the final output pose.

Required minimal graph:

```text
Reference Pose -> Crew Procedural Animation -> Output Pose
```

Acceptable upgraded graph:

```text
Base pose / locomotion pose -> Crew Procedural Animation -> optional IK / Control Rig -> Output Pose
```

Validation:
- compile the Anim Blueprint
- no broken node pins
- no missing node class

Failure:
- if the AnimGraph does not include `Crew Procedural Animation`, the C++ anim work is not visible in runtime

### Step 5 - Verify skeleton compatibility

The procedural node resolves these bone names:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `spine_04`
- `spine_05`
- `neck_01`
- `head`
- `thigh_r`
- `thigh_l`
- `calf_r`
- `calf_l`
- `foot_r`
- `foot_l`
- `upperarm_r`
- `upperarm_l`
- `lowerarm_r`
- `lowerarm_l`

Action:
1. Open the skeleton used by the crew mesh.
2. Verify these bone names exist exactly.

Required result:
- exact or compatible naming for the procedural node

Failure:
- if several of these bones are absent, the procedural node will silently affect fewer bones than intended

Note:
- `HideBoneByName("head")` in FPS also depends on the `head` bone name being present

### Step 6 - Verify the On Foot input mapping contains the character inputs

Target assets:
- `Content/Sub3D/Input/IMC_OnFoot`
- `Content/Sub3D/Input/ONFOOT/IA_Run`
- `Content/Sub3D/Input/IA_PostureScroll`
- `Content/Sub3D/Input/IA_ToggleCamera`

Action:
1. Open `IMC_OnFoot`.
2. Verify `IA_Run` is present.
3. Verify `IA_PostureScroll` is present.
4. Verify `IA_ToggleCamera` if it is already part of the mapping set.

Required result:
- `IA_Run` and `IA_PostureScroll` must be present

Note:
- `IA_ToggleCamera` may exist as an asset but should remain untrusted until the runtime camera path exists

### Step 7 - Wire run in BP_SubmarineCrew

Action:
1. In `BP_SubmarineCrew`, go to Event Graph.
2. Locate or add the Enhanced Input event for `IA_Run`.
3. Wire:
   - `Started` -> `GetCrewMovement` -> `RequestRunStart`
   - `Completed` -> `GetCrewMovement` -> `RequestRunStop`
   - `Canceled` -> `GetCrewMovement` -> `RequestRunStop`

Required result:
- all three execution pins are wired

Why `Canceled` is required:
- it prevents a stuck run state when input focus changes or an action is interrupted

Failure:
- if only `Started` is wired, run will latch on

### Step 8 - Wire posture scroll in BP_SubmarineCrew

Action:
1. Add or find the Enhanced Input event for `IA_PostureScroll`.
2. Use the float axis value from the action.
3. Wire:
   - `Triggered` -> `GetCrewMovement` -> `AddPostureDelta`
4. Multiply the action value by `0.1` before `AddPostureDelta`.

Required result:
- scroll changes the posture target in controlled increments

Validation:
- no direct write to `PostureAlpha`
- use the movement component API only

Failure:
- if BP writes posture state directly on the pawn or anim instance, remove that path

### Step 9 - Do not wire TPS toggle as a shipping path

Current rule:
- do not present `IA_ToggleCamera` as implemented gameplay

Reason:
- there is no complete camera mode runtime path in current C++

Allowed editor action:
- leave the input asset present but unused

Blocked action:
- do not claim TPS is working based only on an input asset existing

### Step 10 - Validate local FPS visibility

Action:
1. PIE as the local player.
2. Confirm the head is hidden in first person.
3. If possible in multiplayer PIE, confirm remote players still see the head.

Required result:
- local player: head hidden
- remote player: head visible

Failure:
- if the local head is still visible, inspect the actual skeleton bone name and mesh assignment

### Step 11 - Validate run behavior

Action:
1. PIE in a dry compartment.
2. Stand fully upright.
3. Hold the run input.
4. Release the run input.
5. Lower posture below upright and try to run again.

Required result:
- upright dry movement speed increases
- release returns to normal speed
- low posture prevents run

Expected source of truth:
- run speed is now derived from movement component posture/run state, then modulated by water state

Failure:
- if speed changes only visually and not physically, inspect Blueprint for stale speed writes
- if run still works while crouched or prone, inspect input wiring and posture state flow

### Step 12 - Validate posture behavior

Action:
1. PIE and scroll downward from standing to low posture.
2. Scroll back upward.

Required result:
- capsule height changes smoothly
- camera height changes smoothly
- movement speed reduces at lower posture
- run is canceled when posture drops below standing threshold

Failure:
- if the camera moves but collision does not, inspect the inherited capsule in the pawn
- if the capsule grows through a ceiling, record it as the expected remaining blocker

### Step 13 - Validate swim behavior

Action:
1. Enter PIE in a submarine level where flood cheats work.
2. Use:

```text
DevCheat_ListCompartments
DevCheat_SetFloodLevel <compartment_name> 0.9
```

3. Move in the flooded compartment.

Required result:
- movement mode enters swimming
- run is disabled
- swim procedural motion becomes active
- hand IK weights decay toward zero
- foot IK offsets are zero while swimming

Failure:
- if the pawn remains in walking mode at high immersion, inspect the current compartment resolution path

### Step 14 - Validate inertial brace behavior

Action:
1. Put the crew in a compartment while the submarine is rotating or rocking.
2. Open console and use:

```text
AnimList
```

3. Inspect movement and pose response.

Required result:
- hand probe behavior should only become relevant when:
  - support quality is poor
  - or submarine angular velocity exceeds the threshold

Expected code condition:
- `SupportQuality01 < 0.8`
- or `LocalSubAngularVelocityDegrees.GetAbsMax() > 2`

Failure:
- if hands appear to brace constantly at rest, inspect the AnimGraph/Control Rig, not just the movement component

### Step 15 - Validate debug tuning path

The controller exposes runtime anim tuning commands:

```text
AnimList
Anim armrestaxis 0
Anim armrestaxis 1
Anim armrestaxis 2
Anim negarm 1
Anim debug 1
```

Action:
1. PIE as local player.
2. Run `AnimList`.
3. Confirm there is a `SubCrewAnimInstance`.
4. Use the tuning commands above to validate axis alignment.

Required result:
- values print successfully
- axis changes affect the live pose

Truth to keep in mind:
- `CrewAnimDebugWidget` exists in code, but it is not proven to be instantiated anywhere in runtime
- the stale comment `ToggleAnimDebug` is not proof of a working console command

---

## 5. AnimGraph and IK closing guidance

This section is execution guidance, not a claim that the assets are already finished.

### 5.1 Minimum acceptable AnimGraph for this phase

For the current first playable phase, the minimum acceptable graph is:

```text
Reference Pose
-> Crew Procedural Animation
-> Output Pose
```

This is ugly but valid for mechanical validation.

### 5.2 Better graph for current phase

Better graph for the same phase:

```text
Reference Pose or base locomotion pose
-> Crew Procedural Animation
-> hand IK layer
-> foot IK layer
-> Output Pose
```

### 5.3 Hand IK source data already available

`USubCrewAnimInstance` already exposes:
- `HandIK_L_Target`
- `HandIK_R_Target`
- `HandIK_L_Weight`
- `HandIK_R_Weight`

Editor closing task:
- verify these variables are actually consumed by either:
  - a Control Rig
  - or AnimGraph IK nodes

If they are not consumed, the hand IK system is still only a data feed.

### 5.4 Foot IK source data already available

`USubCrewAnimInstance` already exposes:
- `FootIK_R_Offset`
- `FootIK_L_Offset`

Editor closing task:
- verify these offsets are actually applied to feet or pelvis in AnimGraph or Control Rig

If they are not consumed, foot IK is still only trace output.

---

## 6. Design decision for posture states

The current scalar posture implementation is acceptable as a transport layer.
It is not sufficient as a final animation state model.

For the next implementation step, use these states:

### 6.1 Standing

Properties:
- full biped locomotion
- run allowed
- upper body aim fully enabled

### 6.2 Crouched

Properties:
- no run
- pelvis lowered
- knees and hips visibly bent
- reduced upper body yaw freedom

### 6.3 Crawl

Properties:
- no run
- dedicated forward crawl cycle
- stronger arm contribution
- lower camera
- stronger front contact support logic

### 6.4 Prone or forced-down

Only add this as a separate state if gameplay distinguishes:
- deliberate low traversal
- versus a temporary knocked-down or forced-flat state

Do not try to solve these with spine bend alone.

---

## 7. Camera recommendation

### 7.1 Recommended design choice

For Sub3D, keep FPS as the primary gameplay camera.

Reason:
- the game is interior-heavy
- the space is narrow
- embodiment inside the submarine is the core read
- first playable criteria do not require TPS

### 7.2 TPS recommendation

TPS should be treated as:
- a local comfort mode
- or a validation mode

Not as the reference gameplay camera for the current phase.

### 7.3 TPS implementation prerequisites

Do not wire TPS as "done" until these exist:

- `USpringArmComponent` on the character
- second camera component
- local-only camera toggle function
- collision test on the spring arm against the interior channel
- camera offsets per posture

### 7.4 Interim rule

For the current editor session:
- validate FPS only
- leave `IA_ToggleCamera` as backlog unless the full runtime path is added in code

---

## 8. Known obvious issues to record during the session

Record these explicitly if observed.

### 8.1 Likely root cause

- posture upscaling into low ceilings is still unsafe because there is no ceiling clearance test before enlarging the capsule

### 8.2 Possible contributor

- if the procedural pose looks wrong, the first suspect is skeleton bone naming or axis mismatch, not the movement component

### 8.3 Possible contributor

- if run feels inconsistent, inspect Blueprint for stale speed writes or duplicated movement logic

### 8.4 Deferred concern

- `CrewAnimDebugWidget` exists in code but is not yet a validated runtime tool path

### 8.5 Deferred concern

- the comment mentioning `ToggleAnimDebug` is stale until proven by actual runtime wiring

### 8.6 Deferred concern

- foot traces currently use simple actor-relative offsets, not exact foot socket positions

---

## 9. Session pass criteria

The editor session is a pass only if all of these are true:

- `BP_SubmarineCrew` is confirmed on the `ASubCrewCharacter` path
- run input is wired
- posture scroll input is wired
- active Anim Blueprint is confirmed on the `USubCrewAnimInstance` path
- active AnimGraph includes `Crew Procedural Animation`
- FPS head hide works locally
- run works only when upright and dry
- posture affects camera, capsule, and movement
- swim switches movement mode and disables run
- `AnimList` works in PIE

Optional for this pass:
- hand IK final pose
- foot IK final pose
- TPS camera

Those remain desirable, but they must not be reported as done unless explicitly validated in editor.

---

## 10. Report format after the editor session

Use this exact structure:

```text
Character Pipeline Editor Session Result

Pawn BP:
Anim BP:
Skeleton:
Level used:

Build used:
PIE result:

Validated:
- ...
- ...

Not validated:
- ...
- ...

Blockers:
- ...

Ready for next code step:
- yes / no
```

---

## 11. Immediate next code step after a successful editor pass

Only after the editor session validates the active path:

1. add ceiling clearance before posture upscaling
2. implement a real posture state machine: standing / crouched / crawl
3. wire actual hand IK consumption if still missing
4. wire actual foot IK consumption if still missing
5. implement TPS only if design still wants it after FPS validation

Until then:
- do not report the character pipeline as complete
- do not report TPS as implemented
- do not collapse crouch and crawl into the current scalar bend alone
