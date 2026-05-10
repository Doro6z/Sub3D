# Barotrauma Water Flood Implementation — Research Notes

Source inspection of `Barotrauma/BarotraumaShared/SharedSource/Map/Hull.cs` and `Gap.cs` (open source — github.com/FakeFishGames/Barotrauma). Captured 2026-05-10 to inform Sub3D's progressive flooding redesign.

Barotrauma ≠ realistic naval engineering. It's a 2D rectangular-room shipped game heuristic. Useful as **lower bound** of "what's enough for a playable feel". Real progressive flooding (Ruponen 2007 / Braidotti 2020) goes much further.

## 1. Hull water state

**File:** `Hull.cs`

Each Hull stores `float waterVolume`, clamped to `Volume * MaxCompress` where `Volume = rect.Width * rect.Height` and `MaxCompress = 1.05f` (5% overpressure buffer to avoid equalization stalls).

Each frame the surface Y is interpolated:

```
surface = Lerp(surface, rect.Y - rect.Height + waterVolume / rect.Width, dt * 10)
```

Crucially: **`Pressure = surface`** — pressure is the absolute world-Y of the water level, not ρgh. No density, no gravity term. 2D rectangular hulls only.

**Sub3D bridge:** our `WaterHeightCm + WalkableFloorZCm` is already richer (3D voxel-derived volumes). The `Pressure = surface Y absolute` identity is conceptually what `SurfaceA - SurfaceB` is doing in the current (committed) `AdvanceFlooding`. The 1.05× overpressure trick is worth borrowing to avoid equalization stalls.

## 2. Horizontal flow (room-to-room gaps)

**File:** `Gap.cs::UpdateRoomToRoom`

```cpp
sizeModifier = (Size / 100.0f) * open * (1 - overlappingGapFlowRateReduction);
delta = ((hull2.Pressure + subOffset.Y) - hull1.Pressure) * 300.0f * sizeModifier * dt;
delta = Min(delta, Min(hull2.WaterVolume, hull2.Volume));
```

Linear in surface delta, scaled by gap width × open ratio × magic constant 300. Source-clamped against available water — this clamp is what saves stability, not the magic constant. The `subOffset.Y` term accounts for the sub's roll/pitch so flow respects world-up.

## 3. Vertical flow

```cpp
delta = Min(hull1.WaterVolume, dt * 25000f * sizeModifier);
```

**Fixed-rate kinematic drain.** Not Torricelli, not hydrostatic. Water always falls upper→lower at a constant rate proportional to gap height × open. Source-clamped against available volume.

This is a deliberate gameplay simplification. It means staircases drain instantly even at trickle-volume — natural top-down filling. No "100% gate" logic needed because the rate is geometry-driven, not pressure-driven.

## 4. Force smoothing

```cpp
flowForce.X/Y = Clamp(±MaxFlowForce);   // MaxFlowForce = 500
lerpedFlowForce = Lerp(lerpedFlowForce, flowForce, dt * 5.0f);
if (openedTimer > 0 && flowForce.Sqr > lerpedFlowForce.Sqr)
    lerpedFlowForce = flowForce;        // instant on door-open
```

Hard clamp + lerp at 5/s, one-sided (instant ramp-up on fresh openings, smooth ramp-down). This is for **VFX/audio impulse** on water jets, not the flood mass math.

## 5. Door coupling

```cpp
public Door ConnectedDoor { get; set; }
```

The door externally writes `gap.Open ∈ [0,1]` each tick. The gap doesn't query door state — door pushes its open ratio in. Partial-open door = partial flow free.

## What Sub3D should keep / borrow / discard

| Aspect | Borrow | Discard |
|---|---|---|
| `Pressure = surface Y absolute` | ✓ already done in current code | — |
| Horizontal: `delta * coeff * area * open * dt`, source-clamped | ✓ swap our `HeightResponsePerLiter` for this | — |
| Vertical: fixed-rate kinematic drain, no level gate | ✓ replaces our forced-overflow shortcut | — |
| Door `OpenRatio` float vs `bClosed` bool | ✓ enables partial flow | — |
| 1.05× overpressure soft cap | ✓ avoids stalls | — |
| Force lerp for VFX | ✓ for visual jets only | — |
| 2D rectangular hulls | — | ✗ Sub3D voxel-derived 3D capacity is correct |
| No air pressure / trapped air | — | ✗ Real progressive flooding (Ruponen, Lee) models this; Sub3D can defer post-FP |
| No mass conservation enforcement | — | ✗ Sub3D should enforce (two-pass budget, already does) |

**Bottom line:** Barotrauma's algorithm is "good enough for a shipped game with 2D rectangular rooms". For Sub3D's 3D voxel volumes + naval-grade ambition, it's a starting point but the eventual model should use Bernoulli orifice flow (Lee 2015) for through-breach inflow and equalization-driven flow between rooms (Ruponen pressure correction, simplified). See [progressive flooding design doc](../plans/2026-05-10_progressive_flooding_simulation_design.md).
