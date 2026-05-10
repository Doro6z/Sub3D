# Crew Swimming - Editor Setup Handoff

Date: 2026-05-07
Status: implementation handoff for editor wiring

## Scope

This handoff covers the editor-side setup for the crew swimming mode. For the current phase, swimming is used outside the submarine after crossing a `USubHullBoundaryComponent`.

Code now exposes:

- `ASubCrewCharacter::ApplyCrewPlanarMoveInput(FVector2D MoveAxis)`
- `ASubCrewCharacter::ApplyCrewVerticalMoveInput(float Axis)`
- `ASubCrewCharacter::IsCrewSwimming()`
- `USubCrewMovementComponent::SwimVerticalInputScale`
- `USubCrewMovementComponent::Sub3DExteriorSwimFluidFriction`
- `ASubCrewCharacter::ExteriorSwimSpeedMultiplier`
- `ASubCrewCharacter::SwimBrakingDeceleration`
- `ASubCrewCharacter::SwimGravityScale`

## Input Actions

Keep the existing planar crew movement action if it already drives `ApplyCrewPlanarMoveInput`.

Add one new input action:

| Asset | Value Type | Purpose |
|---|---:|---|
| `IA_CrewSwimVertical` | Axis1D | Space/Ctrl vertical swim input |

Suggested IMC mappings:

| Key | Action | Scale |
|---|---|---:|
| `Space Bar` | `IA_CrewSwimVertical` | `+1.0` |
| `Left Ctrl` | `IA_CrewSwimVertical` | `-1.0` |

Existing planar mapping should remain:

| Key | Axis |
|---|---|
| `Z` / forward | `MoveAxis.X = +1` |
| `S` / back | `MoveAxis.X = -1` |
| `D` / right | `MoveAxis.Y = +1` |
| `Q` / left | `MoveAxis.Y = -1` |

## Blueprint Wiring

In `BP_SubCrewCharacter` or the existing crew input BP:

1. Existing planar move action:
   - Triggered: call `ApplyCrewPlanarMoveInput(Value as Vector2D)`.
   - Completed/Canceled: call `ApplyCrewPlanarMoveInput(0, 0)`.

2. New vertical swim action:
   - Triggered: call `ApplyCrewVerticalMoveInput(Value as float)`.
   - Completed/Canceled: call `ApplyCrewVerticalMoveInput(0)`.

No extra branch is required in BP. The C++ method ignores vertical input while not swimming.

## Runtime Behavior

When the crew crosses a hull boundary outward:

- `EmbarkState = Outside`
- `CurrentCompartment = null`
- `CurrentWaterImmersion01 = 1.0`
- movement switches to `MOVE_Swimming`
- gravity scale uses `SwimGravityScale`

When the crew crosses inward:

- `EmbarkState = Embarked`
- movement switches to `MOVE_Walking`
- gravity scale returns to `1.0`

There are only two gameplay locomotion modes for this phase:

- inside/base mode: embarked walking or the existing default interior state;
- swimming mode: outside the submarine after an outward hull-boundary crossing.

Interior water currently slows walking but does not switch the crew to swimming. The previous static `SwimThreshold01` auto-trigger is disabled in code. A later implementation should switch walking/swimming from actual character immersion against the current water shape, so pitch, speed, and impacts can produce useful transitions.

## Tuning Defaults

Recommended first-pass values:

| Setting | Default | Notes |
|---|---:|---|
| `ShallowWadeThreshold01` | `0.20` | Low water starts slowing walking. |
| `DeepWadeThreshold01` | `0.50` | Heavy walking slowdown. |
| `NearSwimSpeedMultiplier` | `0.40` | Maximum walking slowdown at full static immersion. |
| `SwimSpeedMultiplier` | existing value | Reserved for future interior swim. Not used by the current static-height path. |
| `ExteriorSwimSpeedMultiplier` | `1.00` | Exterior swim speed uses `DefaultSwimSpeed * this`. |
| `SwimBrakingDeceleration` | `900` | Water drag while swimming. |
| `SwimGravityScale` | `0.0` | Keeps swim neutrally buoyant for FP. |
| `SwimVerticalInputScale` | `1.0` | Space/Ctrl vertical input strength. |
| `Sub3DExteriorSwimFluidFriction` | `0.5` | Drag used by exterior swimming when there is no UE water PhysicsVolume. |

## Validation Checklist

1. Start PIE inside the submarine.
2. Walk to a hull boundary / airlock boundary.
3. Cross outward.
4. Confirm debugger/log shows `EmbarkState=Outside` and `Mode=Swimming`.
5. Confirm `Z/S` swim along camera forward/back.
6. Confirm `Q/D` strafe left/right.
7. Confirm `Space` rises and `Ctrl` descends.
8. Cross inward.
9. Confirm `EmbarkState=Embarked`, `Mode=Walking`, and gravity/walking controls are restored.

## Known Follow-Up

Interior swim should be driven by actual character immersion against the current water surface/volume. This is intentionally deferred because compartment water can move with submarine pitch, speed, and impacts.
