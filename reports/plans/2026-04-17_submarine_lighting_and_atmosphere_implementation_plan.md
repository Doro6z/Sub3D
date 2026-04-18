# Sub3D - Submarine Lighting and Atmosphere Implementation Plan

Date: 2026-04-17
Scope: implement a fast, prototype-scope, scalable lighting and atmosphere stack for the handmade Craniata submarine.

Authority-max plan:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Related local references:
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubLightBase.h`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubLightBase.cpp`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubmarineAlarmBeacon.h`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubmarineAlarmBeacon.cpp`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubmarineFeedbackDirectorComponent.cpp`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\DepthPostProcessActor.cpp`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\DoorFloodVfxComponent.cpp`
- `C:\Dev\Sub3D\Source\Sub3D\Submarine\BreachVfxManagerComponent.cpp`
- `C:\Dev\Sub3D\Config\DefaultEngine.ini`

---

## 0. Executive decision

The Craniata lighting path must use a simple runtime stack with strong editor control.

Use this split:

1. `ASubLightBase` and its Blueprint children are the practical light actors.
2. `ASubmarineAlarmBeacon` remains the alarm-specific light and audio actor.
3. `USubmarineFeedbackDirectorComponent` remains the single place that drives alarm state from gameplay.
4. `ExponentialHeightFog` and a small number of `Local Fog Volume` actors create visible light beams and air density.
5. Niagara provides only two localized atmosphere effects for this phase:
   - dust motes in light beams
   - light haze / steam near technical or damaged areas
6. Post process is used to control exposure, bloom, contrast, and color response inside the submarine.
7. Decals are limited to functional warning, grime, leak, and rust overlays.

Do not attempt a full cinematic relight pass.
Do not add many unique systems per room.
Do not build a second alarm system in parallel.

---

## 1. Current verified project state

These facts are verified in the repository.

### 1.1 Practical light base already exists

`ASubLightBase` now exists and already supports:

- steady light
- blink
- faulty light behavior
- gyro spin
- spot light
- point light
- emissive fixture driving

This is enough to make practical light Blueprints the main reusable light actor type.

### 1.2 Alarm path already exists

The project already contains:

- `ASubmarineAlarmBeacon`
- `USubmarineFeedbackDirectorComponent`
- alarm MetaSound assets under `Content/Sub3D/Audio/MetaSound/Alarm/`
- `BP_SubAlarmBeacon`

This means flood alarm visuals and audio already have a concrete runtime path.

### 1.3 Visual feedback systems already use Niagara

The project already contains Niagara-driven runtime components for:

- door flood cascades
- breach and leak effects

That is enough to justify a small Niagara extension for atmosphere work instead of adding another VFX stack.

### 1.4 Post-process and fog support already exist in the project

The repository already contains:

- `ADepthPostProcessActor`
- config support for light functions and volumetric fog in `DefaultEngine.ini`

This means the engine-side rendering path is already compatible with the proposed atmosphere plan.

### 1.5 Light Blueprints already exist in content

The content tree already contains:

- `BP_SubLight`
- `BP_SubLight_Warm`
- `BP_SubLight_Cold`
- `BP_SubLight_Faulty`
- `BP_SubLight_Alarm`

The next work is therefore not to invent a new light family, but to finish and standardize this one.

---

## 2. Visual target for this phase

The submarine should read as:

- cramped
- mechanical
- humid
- worn
- readable in gameplay
- controllable in editor

The image goal is not perfect realism.
The image goal is a strong playable atmosphere with low setup cost and a clean path to future polish.

For this phase, the player must see:

- practical lights with distinct warm, cold, faulty, and alarm families
- visible beams in selected locations only
- dust or haze when lights cut through the air
- localized grime and warnings on surfaces
- stronger alarm mood when flood state escalates

---

## 3. Scope boundaries

### 3.1 In scope

- practical light presets
- light shaping with IES and light functions
- light beam visibility through fog
- very light atmosphere particles
- small post-process pass for interior lighting response
- alarm-ready light actor usage
- decals for warning, grime, leak, rust

### 3.2 Out of scope

- full cinematic grade master lookdev
- many unique Niagara systems per room
- heavy volumetric smoke everywhere
- expensive dynamic shadowing on every light
- separate player-facing alarm UI implementation
- hard coupling of alarm visuals to helm UI before the runtime entry point is cleaned up

---

## 4. Architecture

### 4.1 Practical lights

Use `ASubLightBase` as the one reusable actor family for practical lighting.

Required Blueprint children:

- `BP_SubLight_Warm`
- `BP_SubLight_Cold`
- `BP_SubLight_Faulty`
- `BP_SubLight_Alarm`

Required light uses:

- `Warm`: crew areas, corridors, access points
- `Cold`: engine, ballast, maintenance, technical bays
- `Faulty`: sparse accent only, never the dominant light family
- `Alarm`: local warning accent, not the whole alarm system by itself

Required additions to the practical light setup:

- optional IES profile slot
- optional light function slot
- exposed volumetric scattering intensity
- exposed cast shadow toggle
- exposed lighting channels
- optional preset enum for warm, cold, faulty, alarm default values

Reason:
This keeps the practical light path reusable, editor-assigned, and future-proof.

### 4.2 Alarm visuals

Do not replace `ASubmarineAlarmBeacon` with generic `BP_SubLight_Alarm`.

Use this split:

- `ASubLightBase` for practical lights and non-critical alarm accent lights
- `ASubmarineAlarmBeacon` for synchronized alarm light and alarm audio behavior

Future manual alarm entry must go through the same feedback director path that already dispatches flood alarm state.

Required future alarm entry point:

- one explicit gameplay call on the submarine feedback path
- not a parallel ad hoc alarm Blueprint graph

Target shape for that future step:

- flood state can trigger alarm
- a red alarm button can trigger alarm
- helm station can trigger alarm
- all three paths converge into one runtime state

The implementation document does not require that code now, but it does require that future work use one alarm authority path.

### 4.3 Fog and beam visibility

Use one global interior fog setup plus a few local fog areas.

Required rule:

- one `ExponentialHeightFog` actor for the playable sub interior atmosphere baseline
- a small number of `Local Fog Volume` actors only in selected hero zones

Recommended hero zones:

- engine room
- airlock
- one forward corridor or command path
- one breach-prone maintenance zone

Do not place fog volumes in every room.

Reason:
The atmosphere must read as heavy and believable, not as stage smoke.

### 4.4 Niagara atmosphere

Use only two prototype Niagara systems.

Required systems:

- `NS_Sub_DustMotes_LightBeam`
- `NS_Sub_SteamLeak_LightHaze`

Rules:

- dust motes are subtle and broad
- steam haze is local and purposeful
- neither system should dominate the image
- both systems should be scalable down or disabled easily

Do not create per-light Niagara systems for every practical.

### 4.5 Decals

Use decals as readable storytelling overlays, not as the base material solution.

Required decal families:

- grime
- leak streaks
- rust
- hazard / warning signage
- direction or service labels only where gameplay readability improves

Decals must stay sparse and readable.
The submarine should not become visually noisy.

### 4.6 Post-process

Use one interior post-process volume to lock the playable look.

Required control areas:

- exposure range
- local exposure
- bloom
- vignette
- light color response
- contrast
- mild color grading

Do not use strong lens distortion, strong chromatic aberration, or heavy film effects in the first playable path.

---

## 5. Recommended feature stack

This is the fastest stack that gives a strong result.

### 5.1 IES profiles

Use IES only on selected lights.

Best use cases:

- caged lamps
- narrow technical practicals
- inspection lamps
- local alarm heads

Do not put IES on every point light.

Reason:
IES improves beam shape and fixture character without requiring custom geometry or expensive shadowing everywhere.

### 5.2 Light functions

Use light functions for:

- dirty flicker modulation
- grille or bar breakup
- narrow spill pattern
- rotating alarm sweep when needed

Do not use them to change light color.
The light color must remain on the light itself.

### 5.3 Local fog volumes

Use local fog volumes to make beams visible only where it matters.

Good zones:

- around engine machinery
- around airlock practicals
- near steam or pipe clusters
- around leaks or breaches

### 5.4 Dust motes

Use a single reusable Niagara system for slow motes.

Good behavior:

- very low opacity
- low spawn count
- visible when crossing a beam
- broad but subtle motion

### 5.5 Steam / light haze

Use a single Niagara system for technical haze.

Good behavior:

- local
- attached near pipes, valves, or damage
- short-lived or looped very lightly

### 5.6 Contact shadows and hero shadows

Use shadow quality selectively.

Required rule:

- only hero lights cast full shadows
- smaller practicals use no shadows or very limited shadow settings
- contact shadows can be used only on a few hero local lights if the image needs it

### 5.7 Lighting channels

Use lighting channels only when the image or readability requires it.

Valid use cases:

- keep an alarm accent off non-critical surfaces
- isolate a hero beam from clutter
- shape a local practical around an interaction station

Do not turn lighting channels into a global dependency.

---

## 6. Concrete asset strategy

### 6.1 Required local assets to author

Create or fill these project assets:

- `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_GridBars`
- `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_GridBars_Soft`
- `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_DirtyFlicker`
- `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_DirtyFlicker_Warm`
- `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_DirtyFlicker_Cold`
- `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_AlarmSweep`
- `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_AlarmSweep_Red`
- `Content/Sub3D/VFX/Lighting/Niagara/NS_Sub_DustMotes_LightBeam`
- `Content/Sub3D/VFX/Lighting/Niagara/NS_Sub_SteamLeak_LightHaze`
- `Content/Sub3D/Materials/Decals/Submarine/M_Decal_Sub_GrimeMaster`
- `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Leak_Dark`
- `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Rust_Orange`
- `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Warning_Yellow`
- one interior `PostProcessVolume` asset setup in the level
- one global `ExponentialHeightFog` actor setup in the level

### 6.2 External assets that are actually useful for this phase

Use external assets only when they save time immediately.

Recommended priority:

1. `Starter Content`
   - reason: gives a known smoke material path used by Epic Niagara smoke documentation
   - useful for: first pass dust / haze sprites

2. `Quixel Bridge / Megascans`
   - reason: free with Unreal Engine, good source for rough metal, grime, leak, dust, and rust textures used to build decal materials and light function breakup masks
   - useful for: decal masks, grime overlays, roughness breakup

3. Optional free Fab VFX donor pack
   - `Basic VFX Pack (Free)`
   - use only as donor for sprites, gradients, and quick Niagara material references, not as a final look package

4. Optional free Fab warning decal donor
   - `Warning signs decals Vol. 1`
   - use only if the team wants a fast pass of hazard labeling without making signs manually

Do not pull a large industrial environment pack just to get a few decals.

---

## 7. Alarm scalability requirement

The lighting plan must support future manual alarm triggering without rework.

Required design rule:

- alarm visual logic must remain compatible with flood alarm
- manual alarm must not create a second incompatible visual path

Future entry points expected later:

- red alarm button actor
- helm station action
- flood threshold from feedback director

Target runtime shape:

- all alarm requests resolve into one alarm authority state
- that authority state drives:
  - `ASubmarineAlarmBeacon`
  - optional `BP_SubLight_Alarm` accents
  - alarm audio
  - future UI / widget state

This is the correct place to scale the feature.

---

## 8. Phase plan

### Phase 1 - Close the prototype base

Required work:

- finish practical light defaults and presets
- add IES slot support
- add light function slot support
- expose volumetric scattering and lighting channels
- author three light function materials
- author the two Niagara systems
- place one global fog actor and a small set of local fog volumes
- place one interior post-process volume

Pass criteria:

- warm, cold, faulty, and alarm light families all read differently
- at least one corridor, one technical area, and one airlock area look strong in PIE
- dust or haze is visible but controlled

### Phase 2 - Add surface storytelling

Required work:

- create grime, leak, rust, and warning decal material instances
- place decals only where they help navigation, mood, or wear storytelling

Pass criteria:

- environment reads older, used, and technical without clutter

### Phase 3 - Tie lighting to feedback states

Required work:

- feed flood alarm into alarm lights cleanly
- reserve one future runtime entry for manual alarm activation
- keep practical light presets reusable

Pass criteria:

- alarm mode changes the sub mood fast and clearly
- no duplicate alarm control path exists

---

## 9. Default values to start with

### 9.1 Practical light families

`Warm`
- color: `FLinearColor(1.00, 0.93, 0.84, 1.0)`
- primary use: crew and circulation
- spot intensity: medium
- point fill: low
- volumetric scattering: low to medium

`Cold`
- color: `FLinearColor(0.78, 0.86, 1.00, 1.0)`
- primary use: technical zones
- spot intensity: medium
- point fill: low to medium
- volumetric scattering: medium in engine room only

`Faulty`
- color: `FLinearColor(0.86, 0.93, 1.00, 1.0)`
- behavior: `Faulty`
- use count: sparse
- light function: dirty flicker optional

`Alarm`
- color: `FLinearColor(1.00, 0.62, 0.30, 1.0)` for amber accents
- beacon color: stronger red on dedicated alarm beacons
- use count: limited and intentional

### 9.2 Post-process

Start conservatively:

- fixed or tightly clamped auto exposure
- bloom low
- vignette low to medium
- no strong chromatic aberration
- no aggressive film grain
- mild contrast boost

### 9.3 Fog

Start conservatively:

- global fog density low
- local fog volumes only in hero zones
- avoid thick global mist

---

## 10. Risks and controls

### Likely root cause of bad submarine lighting

Too much of the look is often pushed into light color and bloom alone.
That produces flat surfaces, unreadable space, and poor atmosphere.

### Possible contributors

- no IES or beam shaping
- no fog medium for beams
- too many uniform practicals
- overuse of shadow casting lights
- too many decals too early
- exposure not locked

### Deferred concerns

- full hero lighting per room
- more advanced volumetric smoke
- cinematic alarm sweep materials
- camera-dependent post-process for cutscenes

---

## 11. Practical conclusion

The correct prototype path is:

1. practical light families first
2. beam shaping second
3. local fog third
4. two small Niagara systems fourth
5. decals after the lighting is readable
6. alarm integration through the existing feedback authority path

This gives a visually strong submarine with low system count, fast editor iteration, and a clean future path.

---

## 12. External references used

Official Epic documentation:

- Lighting overview
  - https://dev.epicgames.com/documentation/unreal-engine/lighting-the-environment-in-unreal-engine
- Light Functions
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/light-functions?application_version=4.27&trk=public_post_comment-text
- Volumetric Fog
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine?application_version=5.6
- Physical Lighting Units
  - https://dev.epicgames.com/documentation/de-de/unreal-engine/using-physical-lighting-units-in-unreal-engine
- Lighting Channels
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/using-lighting-channels-in-unreal-engine?application_version=5.6
- IES brightness usage
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/ULightComponent/bUseIESBrightness
- Niagara smoke tutorial using Starter Content material
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-smoke-effect-using-sprite-particles-in-niagara-for-unreal-engine?application_version=5.6
- Quixel Bridge and Megascans free access for Unreal users
  - https://dev.epicgames.com/documentation/ar-ar/unreal-engine/quixel-bridge-plugin-for-unreal-engine
- UE 5.5 release notes mentioning Local Fog Volumes as production ready
  - https://dev.epicgames.com/documentation/de-de/unreal-engine/unreal-engine-5-5-release-notes

Optional external asset references checked on 2026-04-17:

- Basic VFX Pack (Free)
  - https://www.fab.com/listings/75698e52-edfc-4f76-a86c-b4f26fcf5a29
- Warning signs decals Vol. 1
  - https://www.fab.com/listings/8064dbb6-85f3-4ec1-8390-7c8eb8f4cd96
