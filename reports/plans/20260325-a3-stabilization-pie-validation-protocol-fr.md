# Sub3D Proto03 - A3 Stabilization Pass - PIE Validation Protocol

Date: 2026-03-25
Status: Ready to Execute
Scope: Runtime validation in Unreal Editor / PIE after the A3 stabilization pass

## 0. Current Test Harness Reality

This protocol is written against the current runtime flow actually present in code.

Current spawn / entry flow:

- `ASubGameMode::PostLogin()`
- resolves the active submarine
- reads `HelmSocket` transform when available
- calls `ASubCrewCharacter::EnterOnFootInSubmarine(Submarine, SpawnXform)`
- sets the player to `OnFoot`

Implications:

- there is currently no real door / embark / disembark gameplay loop to validate
- "boarding" validation in this document means: login spawn into the submarine through `EnterOnFootInSubmarine()`
- `BoardSubmarine()` and `DisembarkSubmarine()` are not primary runtime coverage for this pass unless you trigger them manually from some existing debug path

Important scope rule:

- do not stop A3 validation because there is no door yet
- validate A3 against the current authoritative entry path first
- treat "clearer test harness with better spawn / door / disembark flow" as a possible follow-up task, not as a prerequisite to start PIE

## 1. Validation Goals

This protocol validates exactly these A3 claims:

1. the custom pre-tick compensation is gone and no longer fights CMC
2. stock `UpdateBasedMovement()` and `UpdateBasedRotation()` are active again
3. crew remains stable on the submarine floor during translation / yaw / pitch
4. local controller yaw follow is good enough when the submarine rotates
5. `InteriorFrame` remains structurally present and observable
6. submarine replication / interpolation / snap behavior is visible in logs
7. local and remote crew behavior are close enough to continue with stock CMC as the stabilization base

## 2. Test Preconditions

Before starting PIE, confirm:

1. the project compiles
2. the map contains one valid submarine actor
3. `HelmSocket` exists on that submarine actor or the fallback spawn transform is acceptable
4. the interior collision floor is present and walkable
5. the Output Log is open
6. you can see both the submarine actor and, when PIE starts, the spawned crew pawn in the World Outliner

If these are not true, stop and fix the environment before using the results.

## 3. Exact Editor Preparation

### 3.1 Windows and panels

Open:

1. `Window > Output Log`
2. `World Outliner`
3. `Details`
4. optionally a second editor viewport if you want an external observer camera during PIE

### 3.2 Output Log setup

Prepare the Output Log to watch these categories / strings:

- `LogSubCrewMovement`
- `LogSubInteriorFrame`
- `LogSubMovement`
- `LogSubCrew`

Primary strings to watch:

- `EnterOnFootInSubmarine`
- `InitializeForSubmarine`
- `Post-init`
- `Base acquired`
- `Base lost!`
- `Base changed`
- `YawCompensation`
- `CrewState |`
- `RelativeState |`
- `InteriorFrame initialized`
- `InteriorFrame delta`
- `InteriorFrame |`
- `Snapshot received`
- `SUBMARINE SNAP`
- `Interp |`

### 3.3 Actor identification

Before PIE:

1. identify the submarine instance in the `World Outliner`
2. note which component names appear on it:
   - `SubMovement`
   - `InteriorFrame`
   - `HelmSocket`
3. if the submarine is spawned dynamically rather than placed, note which Blueprint / defaults own those component values

### 3.4 Toggle placement rule

Use this rule consistently:

1. if the crew pawn is spawned dynamically, set crew toggles on the pawn class defaults before PIE
2. if the submarine is placed in the map, set submarine toggles on the placed instance
3. if the submarine is spawned dynamically, set submarine toggles on the spawning class defaults

## 4. Debug Toggle Matrix

### 4.1 Baseline solo pass

Crew movement component:

- `bDebugLogCrewMovement = true`
- `bDebugDrawCrewMovement = false`

Interior frame component:

- `bDebugLogFrame = true`
- `bDebugDrawFrame = false`

Sub movement component:

- `bDebugLogSubMovement = false`

### 4.2 Visual solo pass

Crew movement component:

- `bDebugLogCrewMovement = true`
- `bDebugDrawCrewMovement = true`

Interior frame component:

- `bDebugLogFrame = true`
- `bDebugDrawFrame = true`

Sub movement component:

- `bDebugLogSubMovement = false`

### 4.3 Network pass

Crew movement component:

- `bDebugLogCrewMovement = true`
- `bDebugDrawCrewMovement = optional`

Interior frame component:

- `bDebugLogFrame = true`
- `bDebugDrawFrame = false`

Sub movement component:

- `bDebugLogSubMovement = true`

Note:

- `Snapshot received`, `Interp`, and `SUBMARINE SNAP` matter primarily on the non-authority client copy of the submarine
- on a listen server, the host window will not be the best place to judge replicated snapshot behavior

## 5. PIE Configuration

### 5.1 Solo

Use:

1. `Number of Players = 1`
2. normal PIE, no network emulation
3. start in the map that contains the submarine under test

### 5.2 Two players

Use:

1. `Number of Players = 2`
2. `Net Mode = Play As Listen Server`
3. `Run Under One Process = false` recommended
4. no network emulation for the first network pass

Optional later pass:

1. keep `2 Players`
2. keep `Play As Listen Server`
3. enable network emulation only after the clean baseline network pass is understood

## 6. Solo PIE Test Order

## 6.1 Solo Test S0 - Spawn and init sanity

Steps:

1. launch PIE in 1-player mode
2. let the player spawn through the normal `PostLogin -> EnterOnFootInSubmarine()` path
3. do not move for 5 seconds
4. rotate the view only

Expected logs:

- `EnterOnFootInSubmarine | Sub=...`
- `InitializeForSubmarine | Sub=... | FrameValid=1`
- `Post-init | ... | IsEmbarked=1`
- `InteriorFrame initialized | ... | FrameValid=1`
- `Tick prerequisite set on SubMovementComponent: true`
- one `Base acquired: ...`

Failure indicators:

- no `InitializeForSubmarine`
- `FrameValid=0`
- no `Base acquired`
- immediate `Base lost!` while stationary

## 6.2 Solo Test S1 - Stationary floor stability

Steps:

1. with the submarine stationary, walk forward, backward, strafe left, strafe right
2. stop between each move
3. repeat for 20 to 30 seconds

Observe:

- no visible vertical micro-jitter
- no repeated slide corrections
- no fall state
- periodic `CrewState | Embarked=1`

Success:

- `Base acquired` happens once and stays stable
- no spam of `Base lost!`
- movement feels like normal walking on a stable platform

Failure:

- repeated visible bobbing
- floor snap every few frames
- base is repeatedly lost during normal walking

## 6.3 Solo Test S2 - Translation stability

Steps:

1. start forward thrust
2. keep the submarine in mostly straight translation
3. while the submarine moves, walk around
4. stop moving the crew and observe for 10 seconds while the submarine keeps moving

Observe in logs:

- `InteriorFrame delta | Loc=...`
- `CrewState | ... | Base=...`
- no recurring `Base lost!`

Observe visually:

- the crew stays attached to the floor
- no clear fight between gravity and moving frame
- no periodic downward snapping

Success:

- floor contact looks stable
- red debug error line stays short during the visual pass

Failure:

- crew drifts behind the submarine motion
- repeated correction bursts
- floor support appears to drop and return rhythmically

## 6.4 Solo Test S3 - Yaw stability

Steps:

1. apply strong rudder input
2. first stand still and let the submarine yaw
3. then walk while the submarine is still yawing

Observe in logs:

- `YawCompensation | DeltaYaw=...`
- `InteriorFrame delta | Rot=...`

Observe visually:

- the camera follows the interior turn coherently
- no view snap every frame
- no obvious mismatch between world turn and player view

Success:

- yaw follow feels coherent
- no oscillation between controller and base rotation

Failure:

- camera does not follow the submarine turn
- camera catches up in visible jerks
- crew visually twists or desynchronizes from the deck during yaw

## 6.5 Solo Test S4 - Pitch / dive stability

Steps:

1. use dive plane / trim to pitch the submarine
2. while pitched, walk uphill and downhill on the deck
3. stop and observe for 10 seconds

Observe:

- continued base support while deck angle changes
- no transition into obvious falling unless geometry genuinely causes it

Success:

- crew remains supported on the interior floor through pitch changes

Failure:

- crew falls or base is repeatedly lost during ordinary pitch changes

## 6.6 Solo Test S5 - Visual debug pass

Enable:

- `bDebugDrawCrewMovement = true`
- `bDebugDrawFrame = true`

Repeat:

- S2
- S3

Interpretation:

- green sphere: frame origin
- cyan sphere: expected world position from crew relative state
- yellow sphere: actual crew world position
- red line: error between expected and actual
- blue arrow: frame forward axis
- frame debug axes show current frame orientation and motion delta

Success:

- cyan and yellow stay close most of the time
- red line remains short in smooth motion

Failure:

- cyan / yellow separation grows and stays large
- red line pulses strongly during normal motion, not only during abrupt corrections

## 7. Two-Player PIE Test Order

## 7.1 Network Test N0 - Spawn and separation sanity

Steps:

1. launch PIE with 2 players as listen server
2. confirm both players spawn into the submarine through `EnterOnFootInSubmarine()`
3. if both players overlap at the `HelmSocket`, separate them immediately before judging movement quality

Important:

- do not judge initial collision noise from two overlapping players as an A3 movement failure
- start judging only after the players are physically separated

Expected logs:

- one `EnterOnFootInSubmarine` per player
- one `InitializeForSubmarine` per player
- one `Post-init` per player

## 7.2 Network Test N1 - Baseline movement parity

Steps:

1. on the server window, move the submarine in straight translation
2. keep player 1 standing, then walking
3. observe player 2 from the client window
4. switch focus and compare both views

Observe:

- local player behavior on server
- remote representation on client
- relative similarity between the two

Success:

- no large behavioral asymmetry
- both appear grounded and stable

Failure:

- one side is stable while the other jitters, slides, or falls

## 7.3 Network Test N2 - Yaw parity

Steps:

1. on the server, apply strong yaw
2. observe player 1 locally
3. observe player 2 on the client
4. then swap who is moving and who is observing if you can

Observe:

- local camera coherence
- remote crew coherence on the deck
- whether yaw creates larger local/remote divergence than translation does

Success:

- both remain readable and grounded
- no severe yaw-only regression

Failure:

- local looks acceptable but remote visibly skates or snaps
- yaw is the primary divergence trigger

## 7.4 Network Test N3 - Pitch parity

Steps:

1. on the server, pitch the submarine
2. walk with both players during pitch change
3. stop and observe both windows

Success:

- both remain on the interior floor within acceptable network tolerance

Failure:

- remote loses base much more often than local

## 7.5 Network Test N4 - Forced correction / snap

Goal:

- provoke a correction large enough to validate the `SUBMARINE SNAP` instrumentation

Preferred method in editor:

1. keep PIE running in 2-player listen server mode
2. go to the server PIE world
3. in the `World Outliner`, select the submarine PIE instance
4. in `Details > Transform`, move the submarine instantly by more than `600 cm`
5. return focus to the client window immediately

Observe on client:

- `Snapshot received | ...`
- `SUBMARINE SNAP | Distance=...`
- possibly `Interp | ...` before / after depending on timing

Observe visually:

- the submarine corrects
- crew should not enter a prolonged uncontrolled fall
- base should remain or be reacquired quickly

Failure:

- snap occurs and crew becomes unusable afterward
- long floor loss after correction
- yaw/camera becomes incoherent after correction

Fallback if editor transform move is not practical:

1. enable PIE network emulation
2. do an aggressive maneuver change with large server-side position delta
3. only use this as a secondary method because it is less deterministic

## 8. Exact Log Interpretation Guide

### 8.1 Crew init logs

Good:

- `EnterOnFootInSubmarine`
- `InitializeForSubmarine ... FrameValid=1`
- `Post-init ... IsEmbarked=1`

Bad:

- missing init logs
- `FrameValid=0`
- `IsEmbarked=0`

### 8.2 Base logs

Good:

- one `Base acquired`
- maybe rare base changes only when something real changes

Bad:

- repeated `Base lost!` during ordinary translation / yaw / pitch
- `Base acquired` / `Base lost!` oscillation loop

### 8.3 Crew periodic state logs

Good:

- `Embarked=1`
- valid base names
- stable relative state while motion remains smooth

Bad:

- no valid base during normal traversal
- state jumps that match visible jitter

### 8.4 Yaw logs

Good:

- `YawCompensation` appears when the submarine yaws

Bad:

- no yaw compensation logs during obvious yaw
- large visible camera instability despite those logs

### 8.5 Frame logs

Good:

- `InteriorFrame delta` tracks actual submarine motion
- `Tick prerequisite set on SubMovementComponent: true`

Bad:

- no frame delta while the submarine visibly moves
- frame logs show erratic spikes that match presentation artifacts

### 8.6 Replication logs

Good:

- client receives `Snapshot received`
- `Interp` behaves as expected
- `SUBMARINE SNAP` only appears when you intentionally force a large correction

Bad:

- frequent snap logs during ordinary motion
- client correction quality degrades crew stability badly

## 9. Success Criteria

The A3 pass is considered technically successful enough to continue if:

1. stationary floor contact is stable
2. translation no longer shows the old compensation-vs-CMC fight
3. yaw is visually coherent for the local player
4. pitch remains traversable
5. base stability is mostly maintained during normal submarine motion
6. local and remote behavior are acceptably similar
7. snap events are observable and recoverable
8. debug hooks now make failures diagnosable rather than hidden

## 10. Failure / Escalation Criteria

Escalate beyond A3 if one or more of these remain true after repeated PIE runs:

1. the crew still cannot maintain a stable movement base on interior geometry during normal submarine motion
2. local and remote proxies still diverge structurally even with stock based movement re-enabled
3. yaw follow remains visibly unstable because frame rotation deltas inherit raw actor correction artifacts
4. snap recovery produces prolonged floor loss or visible unusable jitter
5. the only apparent path back to stability would be to reintroduce custom pre-tick compensation

If these appear, the likely next architectural direction is no longer "tune A3 more", but:

- stronger relative-space-first crew traversal
- more explicit embarked movement ownership
- possible decoupling of `InteriorFrame` from raw presented actor transform

## 11. Out-of-Scope For This Validation Pass

Do not block A3 sign-off on these items today:

1. no real door / hatch interaction
2. no polished embark / disembark gameplay flow
3. no final interior traversal mode replacement
4. no advanced network prediction / rollback

These are valid future tasks, but they are not prerequisites for judging whether A3 removed the active hybrid conflict and restored enough stability to continue.

## 12. Recommended Execution Order Summary

Run in this exact order:

1. S0 spawn and init sanity
2. S1 stationary floor stability
3. S2 translation stability
4. S3 yaw stability
5. S4 pitch stability
6. S5 visual debug pass
7. N0 two-player spawn sanity
8. N1 network baseline movement parity
9. N2 network yaw parity
10. N3 network pitch parity
11. N4 forced correction / snap

Do not interpret later tests until earlier ones are understood.

If S0 or S1 already fail, stop and analyze before continuing.
