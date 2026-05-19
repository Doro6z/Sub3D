# Crew Procedural Swim Execution Plan - 2026-05-19

Authority plan: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`.

Scope for this pass:
- C++ gameplay and animation contracts for crew swimming.
- Procedural swim parameters exposed through C++ and the procedural animation profile.
- Local-grid/reference-frame coherence for interior swimming.
- No Blueprint asset edits.
- No compilation in this pass.

## 1. Current State Reviewed

Files inspected:
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`
- `Source/Sub3D/Submarine/SubCrewCharacter.h`
- `Source/Sub3D/Submarine/SubCrewCharacter.cpp`
- `Source/Sub3D/Submarine/SubCrewAnimInstance.h`
- `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp`
- `Source/Sub3D/Submarine/CrewLocomotionTypes.h`
- `Source/Sub3D/Submarine/CrewProceduralAnimProfile.h`
- `Source/Sub3D/Submarine/SubPlayerController.cpp`
- `reports/plans/2026-05-12_crew_animation_system_v2_plan.md`
- `reports/plans/2026-05-13_crew_procedural_animation_lga_audit.md`

Observed implementation:
- `USubCrewMovementComponent` owns the authoritative crew movement inputs and produces `FCrewMoveIntent`.
- `ASubPlayerController` already forwards planar, vertical, run, water sprint, and jump requests to the crew/movement component path.
- `USubCrewMovementComponent::BuildWorldMoveInput` keeps walking planar, while swimming can use camera pitch plus vertical input.
- `USubCrewMovementComponent::UpdateLocomotionFrame` produces the frame consumed by animation.
- `USubCrewAnimInstance` consumes `FCrewLocomotionFrame` and `FCrewMoveIntent`, not raw CMC velocity, which matches the LGA audit direction.
- Procedural grounded locomotion now uses `FCrewProceduralGroundedTuning` and `FCrewProceduralRigAxisProfile`.
- Swim was still using legacy scalar values directly in the AnimInstance.

## 2. Research Questions

### Q1 - Should swimming use the submarine reference frame inside the submarine?

Answer: yes.

Evidence:
- `USubCrewMovementComponent::UpdateLocomotionFrame` uses `RelativeLinearVelocity` when grid authoritative.
- The animation frame exposes `LocalVelocity`, `DirectionDeg`, `BodyLocalYawDeg`, sub tilt, sub acceleration, and sub angular velocity.
- `BuildMoveIntent` computes `LocalMoveDirection` by unrotating world input by the submarine yaw when a current submarine exists.

Decision:
- Keep animation driven by `FCrewLocomotionFrame`.
- Do not read raw world-space velocity in swim animation.
- Keep axes and directional values local when grid-authoritative.

### Q2 - Should water sprint be visible to animation?

Answer: yes.

Reason:
- Swim speed alone is not enough because sprint should change stroke rate, body pitch, kick amplitude, and arm effort immediately.
- Animation should not query input state or controller state directly for sprint.

Applied:
- Added `FCrewLocomotionFrame::bIsWaterSprinting`.
- `USubCrewMovementComponent::UpdateLocomotionFrame` writes it.
- `USubCrewAnimInstance` reads it and exposes `bIsWaterSprinting`.

### Q3 - Should swim parameters live in the profile?

Answer: yes.

Reason:
- Grounded gait already lives in `UCrewProceduralAnimProfile`.
- Swim tuning needs the same editor workflow: one DataAsset can carry rig axes and locomotion tuning together.
- This avoids hard-coded swim constants in the AnimInstance.

Applied:
- Added `FCrewProceduralSwimTuning`.
- Added `UCrewProceduralAnimProfile::Swim`.
- Added `USubCrewAnimInstance::SwimTuning` fallback when no profile is assigned.

### Q4 - What axis policy should the swim implementation follow?

Answer:
- Use `FCrewProceduralRigAxisProfile` only.
- Axis mapping remains `0 = Roll/local X`, `1 = Pitch/local Y`, `2 = Yaw/local Z`.
- Swim must use the same axis profile entries as grounded locomotion:
  - thighs: `Axes.ThighSwing`
  - knees: `Axes.KneeBend`
  - feet: `Axes.FootPitch`
  - shoulder swing: `Axes.ShoulderSwing`
  - shoulder lower/scull: `Axes.ShoulderLower`
  - elbows: `Axes.ElbowBend`
  - spine bend: `Axes.SpineBend`
  - spine roll/twist response: `Axes.SpineTwist`

Applied:
- `ComputeSwimCycle` uses only `GetRigAxes(*this)` axis entries.
- No new raw axis enum was added.

### Q5 - Should FPS and third-person share exactly the same arm amplitude?

Answer: no.

Reason:
- Third-person needs readable shoulder/arm swimming.
- FPS camera can become noisy if upper arms swing at full amplitude near the camera.

Applied:
- Added `FCrewProceduralSwimTuning::FirstPersonArmScale`.
- `ComputeSwimCycle` scales arm stroke when `ASubCrewCharacter::IsFirstPersonMode()` is true.

## 3. Implementation Applied

### Data Contract

`FCrewLocomotionFrame` now carries:
- `bIsSwimming`
- `bIsWaterSprinting`
- `bIsRunning`

This lets AnimBP/AnimInstance distinguish normal swim from sprint swim without reading input bindings.

### Water Sprint Defaults

Changed defaults:
- `USubCrewMovementComponent::MaxSwimSpeed`: `640`
- `WaterSprintMaxHoldSeconds`: `5.0`
- `WaterSprintRecoverySeconds`: `2.5`
- `WaterSprintMinStartEnergy01`: `0.04`
- `WaterSprintRequiredImmersion01`: `0.35`
- `ASubCrewCharacter::SwimSpeedMultiplier`: `0.7`
- `ASubCrewCharacter::WaterSprintSpeedMultiplier`: `2.0`

Result:
- Interior flood swim is no longer extremely slow.
- Water sprint can arm before the movement mode has fully flipped into swimming.
- Sprint is easier to trigger and lasts longer.

### Procedural Swim

New swim tuning fields:
- speed reference and stroke rates
- smoothing
- body pitch
- vertical input pitch
- lateral roll
- body wave
- pelvis bob
- kick thigh/knee/foot amplitudes
- shoulder stroke/scull/elbow amplitudes
- first-person arm scale

`ComputeSwimCycle` now separates:
- phase and stroke rate
- sprint smoothing
- leg kick
- arm pull/recovery/scull
- body pitch/roll/wave

## 4. Remaining Editor Validation

Open the crew procedural profile asset used by the crew AnimInstance.

Check:
- `RigAxes.ThighSwing`, `KneeBend`, `FootPitch`
- `RigAxes.ShoulderSwing`, `ShoulderLower`, `ElbowBend`
- `RigAxes.SpineBend`, `SpineTwist`
- new `Swim` section values

In AnimBP preview:
- enable preview swim
- set preview speed around `250`, `500`, `650`
- toggle preview water sprint
- scrub stride phase from `0` to `1`

Runtime PIE checks:
- flood a compartment, enter water, confirm crew remains in submarine reference frame.
- hold water sprint and verify boost, energy drain, stroke rate increase.
- release water sprint and verify recovery.
- test FPS and third-person arm readability.

Debug commands/tools:
- `ShowDebug Animation`
- animation blueprint preview panel for exposed variables
- procedural debug draw in `USubCrewAnimInstance`

## 5. Risks Not Closed Without Editor

- Whether the current AnimGraph applies every procedural output bone in the expected transform space.
- Whether the current skeletal mesh rig axes match the profile defaults for shoulders and elbows.
- Whether `FirstPersonArmScale = 0.55` is enough to keep arms readable without camera noise.
- Whether the water sprint input action is already bound in every test pawn/controller asset.

No C++ build was run in this pass by request.
