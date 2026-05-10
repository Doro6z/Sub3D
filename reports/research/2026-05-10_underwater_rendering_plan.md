# Underwater Rendering & Surface Transitions — Sub3D Plan

Captured 2026-05-10 from research agent. Companion to the [progressive flooding design doc](../plans/2026-05-10_progressive_flooding_simulation_design.md). Foundation for FP visual polish on top of the §6 sim.

## 0. Anchoring on existing systems

Sub3D already owns the right scaffolding:

- `UFloodWaterPlaneComponent` ([FloodWaterPlaneComponent.h](../../Source/Sub3D/Submarine/FloodWaterPlaneComponent.h)) — cap mesh + R32F heightfield + slosh modal + `InjectAt` / `InjectAtWorldPoint` API for FX hooks.
- `UCrewUnderwaterPPComponent` ([CrewUnderwaterPPComponent.h](../../Source/Sub3D/Submarine/CrewUnderwaterPPComponent.h)) — already fires `BP_OnEnterWater`, `BP_OnExitWater`, `BP_OnWaterlineProximity(float)` and feeds `UnderwaterAlpha` + `DistanceToSurfaceCm` to a PP MID. **Ship the visuals through this; do not write a sibling component.**
- `UCompartmentVolumeComponent::GetWaterSurfaceWorldLocation()` — single source of truth for surface Z. Already consumed by the PP component.

## 1. Surface transition: emerged ↔ submerged

Three regimes driven by the existing `LastDistanceCm` from `UCrewUnderwaterPPComponent`:

| Regime | Trigger (signed cm) | Stack |
|---|---|---|
| Above | > +`WaterlineCrossThresholdCm` | normal interior PP |
| Waterline (split-screen) | within ±`WaterlineCrossThresholdCm` | PP MID switches to "split" branch (mask by per-pixel water depth) |
| Below | < −`WaterlineCrossThresholdCm` | full underwater PP — tint, fog, refraction, audio LPF |

Reference: **Subnautica's simulationView shader** (revisited at GDC 2018 "The Art of Subnautica"). Scene rendered normally then a screen-space underwater pass with mask computed from `(WaterPlaneZ - PixelWorldZ)` reconstructed from `SceneDepth`. Sea of Thieves uses the same family (Sharpe & Roberts, GDC 2018). For Sub3D's interior case, **per-pixel waterline reconstructed from SceneDepth + heightfield sample** is the correct base — handles stairs, props, large compartments where a single-line waterline would shear.

`BP_OnEnterWater` / `BP_OnExitWater` are splash trigger / droplet-on-lens trigger respectively — already wired.

## 2. Half-submerged shader (camera near surface)

| Approach | Cost | Wave correctness | Verdict |
|---|---|---|---|
| **Two-layer composite** (scene render + underwater volume rebuild from SceneDepth + heightfield sample) | Medium | Yes — sub-pixel waves slosh in/out per pixel | **Ship for FP** |
| Per-pixel raymarched waterline | High | Yes | Post-FP only |
| Screen-space split (single horizontal mask) | Low | No — flat line | Reject — looks plastic |
| Depth-clip bobble | Low | Partial | Reject — artifact-prone |

Implementation in `M_Crew_Underwater_PP` (post-process domain material):

- Sample `SceneDepth` → reconstruct `PixelWorldPos`.
- Sample the same R32F heightfield the cap mesh uses (`HeightFieldTex`, `LocalBoundsMin/Max` already on `BakeCapMID`).
- `WaterMask = saturate((WaterZ + Heightfield*Amplitude) - PixelWorldPos.Z)`.
- Lerp tinted/fogged scene over original by `WaterMask`.

Sloshing waterline "for free" because the heightfield is already CPU-simulated and pushed each tick.

## 3. Post-process effects under water — FP priority

Required for FP (must ship in `M_Crew_Underwater_PP`):

1. **Color tint** — `lerp(SceneColor, SceneColor*tint, alpha)` driven by `UnderwaterAlpha`. Tint authored via curve atlas keyed by ambient O2/light, not depth (no depth gradient inside a sub).
2. **Volumetric fog falloff** — exponential attenuation on `SceneDepth`, not the engine's `ExponentialHeightFog` (global). Cheap: 1 mad per pixel. Critical for "you can't see across the compartment".
3. **Refraction distortion** — UV scroll of normal map sampled from heightfield gradient (two heightfield taps). Reuses existing texture.
4. **Vignette** — built-in PP `VignetteIntensity`, ramp via `UnderwaterAlpha`.
5. **Audio low-pass** — `UAudioComponent` on camera rig + Submix Effect Chain (`USoundSubmixEffectFilter`, `EFilter::LowPass`) toggled on enter/exit. `LinkedAudioVolume` already on `UCompartmentVolumeComponent` — wire on the same events.
6. **Subaquatic ambience** — second `UAudioComponent` on crew, fade-in on enter.

Post-FP polish: caustics (3-channel projected noise), bloom boost, chromatic aberration on waterline only, lens drops decal on emerge.

## 4. FX — drips, falls, splashes, breaches

| Effect | System | Driver | Niagara modules |
|---|---|---|---|
| Drips on emerge | Niagara on camera socket + decal-on-lens material | `BP_OnExitWater` lifetime ~3s | Spawn Burst, Gravity, Drag, Collision |
| Falling water through hatch | Niagara on `ASubDoorActor` when upper compartment level > door bottom Z | `UFloodWaterPlaneComponent::InjectAt` on lower side at splash point | Cone emit + Gravity + Light Disable Lifetime; expose `FlowRateLPS` user param |
| Player falls into water | Niagara one-shot at impact XY | New `BP_OnCharacterImpactWater` event added to `UCrewUnderwaterPPComponent` (~10 lines C++) | Radial mesh splash + droplet ribbon + sound cue; call `InjectAtWorldPoint` so heightfield ripples |
| Breach cascade | already wired (`BreachWaterImpactVfx`, `BreachInjectForce`, `BreachRecurringInjectHz`) | already wired | leave as-is, just author asset |

Expose on every Niagara: `Intensity` (0..1, drives spawn rate), `WaterTint` (linear color), `FlowRateLPS` where physical. Wire `Intensity` from `USubFloodComponent::CurrentFlowRateLitersPerSec` so a small leak vs hull rupture differ without separate assets.

## 5. Door / wall gap fix

Visible Z-step at door thresholds. Recommended FP combination:

1. **Skirt overshoot in the bake** (primary) — extend cap polygon outward by `BakeOvershootCm` (default 5cm) at perimeter edges touching a connector. Computed in `Sub3DBuilder` bake, not at runtime. Adjacent caps overlap by 10cm; depth-test resolves.
2. **Material edge alpha-fade** (secondary, hides overlap z-fight) — vertex color alpha 0 on the outer 10cm of skirt, premultiplied alpha in `M_CompartmentWater_v2`. Use **dithered translucency** to keep depth writes for the underwater PP mask.
3. **Hide diff behind door frame** — closed door mesh occludes the seam anyway. Open door = Z-step is *gameplay-visible info* (water pours from one side to other) and should not be hidden — instead spawn Niagara cascade between the two surface Zs (item 4 row 2).

**Do not** generate runtime triangle strip between caps — adds CPU cost during a frame already busy with heightfield + slosh, and duplicates info the bake should encode once.

## 6. Water skirts (vertical curtains)

Vertical strips dropped from cap perimeter to flood floor, sharing cap material. Needed when camera is underwater looking at wall — without skirts, cap renders as thin disc with wall behind not occluded by water.

For Sub3D: **needed, ship in FP**, cheap version. ~50–100 tris extra per compartment, one extra draw call. Implementation: in `Sub3DBuilder` water bake, after generating cap polygon, emit second triangle ring `(perimeter_v, perimeter_v_below_at_floor)` with same material slot. Material samples same heightfield horizontally → vertical edges ripple in sync.

Two gotchas:

- Skirt triangles must face **inward** (normal pointing into compartment). Underwater PP mask reads `SceneDepth`; back-facing skirts won't write depth. Toggle on skirt sub-mesh.
- Skirts must clip against `UCompartmentVolumeComponent` box (or use bake's contour) so they don't poke through walls when bake's Z-slice is approximate. Use Material Pixel Depth Offset trick: discard pixels where `WorldPos` projected onto compartment local frame is outside `LocalBoundsMin/Max` (already exposed).

## Shipping order

**FP-blocking (must ship):**

- Wire `M_Crew_Underwater_PP` two-layer composite (item 2). Uses existing `UCrewUnderwaterPPComponent` BP events, no C++ change.
- Tint, fog falloff, vignette, refraction, audio LPF + ambience (item 3 #1–6).
- Skirts (item 6) — bake-time addition to `Sub3DBuilder`.
- Skirt overshoot + alpha-fade for door seams (item 5 #1–2).
- Splash Niagara on player water entry (item 4 row 3) — adds `BP_OnCharacterImpactWater` event to `UCrewUnderwaterPPComponent`.

**Post-FP polish:**

- Caustics, bloom kick, chromatic aberration at waterline, lens drops decals.
- Niagara cascade between adjacent compartments at open doors with delta water Z (item 4 row 2).
- Per-pixel raymarched waterline upgrade (item 2 best-quality variant).

**Reject:**

- Engine `WaterPlugin` (open-ocean, wrong context).
- Runtime-generated boundary triangle strips between caps.
- Screen-space single-line waterline mask.

## Files to touch

- [CrewUnderwaterPPComponent.h](../../Source/Sub3D/Submarine/CrewUnderwaterPPComponent.h) / `.cpp` — add `BP_OnCharacterImpactWater` event, wire audio submix.
- [FloodWaterPlaneComponent.h](../../Source/Sub3D/Submarine/FloodWaterPlaneComponent.h) / `.cpp` — skirt path uses existing `BakeCapMID` / `LocalBoundsMin/Max`.
- [Sub3DBuilder/](../../Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp) bake side for skirt geometry + perimeter overshoot.
- New material `M_Crew_Underwater_PP` (post-process domain).
