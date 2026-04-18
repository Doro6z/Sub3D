# Sub3D - Submarine Lighting and Atmosphere Editor Handoff

Date: 2026-04-17
Scope: editor execution handoff for the Craniata submarine lighting, atmosphere, alarm-readiness, and surface storytelling pass.

Authority-max plan:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Implementation reference:
- `C:\Dev\Sub3D\reports\plans\2026-04-17_submarine_lighting_and_atmosphere_implementation_plan.md`

---

## 0. Session objective

Close a first strong visual pass for the Craniata interior with:

- practical lights that read by family
- localized visible beams
- subtle airborne dust and haze
- readable alarm mood escalation
- restrained grime and warning decals

This pass is for playable atmosphere and validation in PIE.
It is not a final art pass.

---

## 1. Assets to fill first

Do these first.
These are the minimum assets that unblock the whole lighting pass.

### 1.1 Required existing local assets to verify

These already exist in the project and must be checked before creating anything new.

- `Content/Sub3D/Blueprint/SubBP/BP_SubLight.uasset`
- `Content/Sub3D/Blueprint/SubBP/BP_SubLight_Warm.uasset`
- `Content/Sub3D/Blueprint/SubBP/BP_SubLight_Cold.uasset`
- `Content/Sub3D/Blueprint/SubBP/BP_SubLight_Faulty.uasset`
- `Content/Sub3D/Blueprint/SubBP/BP_SubLight_Alarm.uasset`
- `Content/Sub3D/Blueprint/SubBP/BP_SubAlarmBeacon.uasset`
- `Content/Sub3D/Audio/MetaSound/Alarm/MS_Alarm_Flood.uasset`

Required check:

- confirm these assets open cleanly
- confirm the practical light BPs still derive from the intended base light class
- confirm the alarm beacon BP is still the alarm-specific actor

### 1.2 Required new assets to create immediately

Create these first, in this order.

1. `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_GridBars`
2. `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_GridBars_Soft`
3. `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_DirtyFlicker`
4. `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_DirtyFlicker_Warm`
5. `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_DirtyFlicker_Cold`
6. `Content/Sub3D/VFX/Lighting/LightFunctions/M_LF_Sub_AlarmSweep`
7. `Content/Sub3D/VFX/Lighting/LightFunctions/MI_LF_Sub_AlarmSweep_Red`
8. `Content/Sub3D/VFX/Lighting/Niagara/NS_Sub_DustMotes_LightBeam`
9. `Content/Sub3D/VFX/Lighting/Niagara/NS_Sub_SteamLeak_LightHaze`
10. `Content/Sub3D/Materials/Decals/Submarine/M_Decal_Sub_GrimeMaster`
11. `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Leak_Dark`
12. `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Rust_Orange`
13. `Content/Sub3D/Materials/Decals/Submarine/MI_Decal_Sub_Warning_Yellow`

Reason:
This is the smallest asset set that gives immediate mood, readability, and a clean future path.

---

## 2. Assets to fetch only if needed

Keep external downloads minimal.
The project can already close a good prototype with built-in and local assets.

### 2.1 Recommended optional free assets

These are the only external assets worth pulling for this phase.

1. `Starter Content`
- purpose: gives `M_smoke_subUV` and other basic donor materials useful for the first Niagara haze pass
- source: Epic documentation references Starter Content directly in the Niagara smoke tutorial
- source link:
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-smoke-effect-using-sprite-particles-in-niagara-for-unreal-engine?application_version=5.6

2. `Quixel Bridge / Megascans`
- purpose: free source of grime, rust, rough metal, leak, and dust textures for decal and breakup masks
- use it for texture donors only, not for pulling large environment kits
- source link:
  - https://dev.epicgames.com/documentation/ar-ar/unreal-engine/quixel-bridge-plugin-for-unreal-engine

3. `Basic VFX Pack (Free)`
- purpose: optional donor for simple Niagara sprites and reference materials if the team wants to move faster on dust / haze
- use it as donor content, not as a final look package
- checked free on 2026-04-17
- source link:
  - https://www.fab.com/listings/75698e52-edfc-4f76-a86c-b4f26fcf5a29

4. `Warning signs decals Vol. 1`
- purpose: optional fast path for hazard signage and technical warning labels
- checked free on 2026-04-17
- source link:
  - https://www.fab.com/listings/8064dbb6-85f3-4ec1-8390-7c8eb8f4cd96

### 2.2 Do not fetch for this phase

Do not pull these categories now:

- large industrial prop packs
- large decal libraries with many unrelated themes
- expensive cinematic smoke packs
- heavy environment kits used only for donor textures
- plugins that solve editor convenience but do not ship gameplay value

---

## 3. Editor setup order

Execute in this order.

### Step 1 - Verify the existing light Blueprint family

Open:

- `BP_SubLight`
- `BP_SubLight_Warm`
- `BP_SubLight_Cold`
- `BP_SubLight_Faulty`
- `BP_SubLight_Alarm`
- `BP_SubAlarmBeacon`

Confirm:

- the family is intact
- the fixture mesh slot exists
- the light pivot is usable
- the alarm beacon remains separate from generic practical lights

Failure condition:

- if the existing BPs are not clean or are not using the expected base class path, stop and fix this first

### Step 2 - Create the light function materials

Create three master materials or direct masters with instances.

Required functions:

1. `GridBars`
- purpose: subtle cage or grille breakup on selected practicals
- output: intensity breakup only
- use: warm corridor and a few technical lights

2. `DirtyFlicker`
- purpose: slight dirty breakup and flicker modulation for faulty or old lights
- output: intensity breakup only
- use: faulty practicals only, and a few maintenance lights

3. `AlarmSweep`
- purpose: sweep or rotating alarm modulation for hero alarm moments
- output: intensity breakup only
- use: alarm accents only, not on every alarm source

Required rule:

- keep all three cheap
- avoid overly detailed textures
- keep function scale readable at gameplay distance

### Step 3 - Create the two Niagara systems

Create:

1. `NS_Sub_DustMotes_LightBeam`
- sprites only
- very low opacity
- slow movement
- broad life range
- should read only when a beam crosses the camera view or geometry

2. `NS_Sub_SteamLeak_LightHaze`
- sprites or very light ribbon-less smoke
- local use only
- attach only near pipes, valves, airlock, or breach-adjacent technical zones

Required rule:

- these systems are atmosphere support, not hero VFX
- keep counts and overdraw low

### Step 4 - Create the decal material family

Create:

- one master grime/leak/rust decal material
- three first useful instances:
  - leak
  - rust
  - warning yellow

Use cases:

- leak streaks under valves or along seams
- rust around wet metal edges and service zones
- warning labels or hazard edge markings

Do not place decorative clutter decals everywhere.

### Step 5 - Place global fog and post-process

In the validation level or active submarine level:

1. add one `ExponentialHeightFog`
2. enable `Volumetric Fog`
3. add one interior `PostProcessVolume`
4. make the PPV unbound only if the level is dedicated to the sub interior
5. otherwise keep it local to the playable sub region

Start values:

- fog density: low
- volumetric fog: on
- bloom: low
- exposure range: tight
- vignette: low to medium
- color grading: mild only

### Step 6 - Add local fog volumes

Place a small number of local fog areas.

Place them only in:

- engine room
- airlock
- one command or central corridor zone
- one maintenance or leak-prone zone

Required rule:

- 3 to 6 local fog volumes max for the first pass

### Step 7 - Assign practical light families

Use the Blueprint family intentionally.

Assign:

- `BP_SubLight_Warm` to crew and main corridor practicals
- `BP_SubLight_Cold` to technical and ballast zones
- `BP_SubLight_Faulty` to isolated broken or unstable fixtures only
- `BP_SubLight_Alarm` only as alarm accent lights where needed
- `BP_SubAlarmBeacon` for actual synchronized alarm heads

Required rule:

- the alarm beacon actor remains the real alarm light+audio path
- the generic alarm light BP is secondary and visual only

### Step 8 - Place Niagara atmosphere only in hero zones

Attach or place:

- dust motes in beams that matter
- steam haze where pipes, valves, or technical heat sources justify it

Do not attach a Niagara actor to every practical light.

### Step 9 - Add decals last

Only after the lighting and atmosphere read well:

- add leak decals
- add rust decals
- add warning decals

Use decals to support the lighting pass, not to rescue it.

---

## 4. Initial default values

### 4.1 Practical lights

`Warm`
- color: `FLinearColor(1.00, 0.93, 0.84, 1.0)`
- main use: corridors, crew spaces, access points
- use mostly spot plus a small point fill if the fixture reads better that way

`Cold`
- color: `FLinearColor(0.78, 0.86, 1.00, 1.0)`
- main use: engine, ballast, utility
- tighter beams and slightly harsher feel

`Faulty`
- color: `FLinearColor(0.86, 0.93, 1.00, 1.0)`
- use `Faulty` behavior
- keep count low

`Alarm accent`
- color: `FLinearColor(1.00, 0.62, 0.30, 1.0)` for local warm alarm accents
- dedicated alarm beacons can push further toward red

### 4.2 Post-process

Start here:

- bloom: low
- local exposure: enabled if available in project settings
- min/max exposure: tight, not wide-open
- no heavy chromatic aberration
- no strong lens flares
- no strong film grain

### 4.3 Fog

Start here:

- global fog low
- local fog moderate only in hero zones
- keep view readability intact

---

## 5. Placement rules

### 5.1 Warm lights

Place in:

- main circulation path
- crew rest or access zones
- general interior readability path

Do not make them orange.
They must stay readable and practical.

### 5.2 Cold lights

Place in:

- engine room
- ballast access
- machinery zones
- service areas

Do not make them deeply blue.
They should read technical, not sci-fi neon.

### 5.3 Faulty lights

Place only where failure adds tension.

Good locations:

- one maintenance corridor
- one secondary service access
- one technical zone edge

Required rule:

- 1 or 2 faulty lights per zone maximum

### 5.4 Alarm beacons

Place where alarm escalation must read immediately.

Good locations:

- command path
- airlock entry
- engine access
- one or two corridor junctions

---

## 6. Alarm-ready editor requirements

The sub must be laid out so future manual alarm triggering is easy.

Required placement rule:

- use `BP_SubAlarmBeacon` as the real alarm node where audio and true alarm state must exist
- reserve `BP_SubLight_Alarm` for additional visual accents only

Future manual alarm sources expected:

- red wall button
- helm station command
- flood threshold from feedback director

Required editor consequence now:

- do not hide alarm logic in random practical light graphs
- keep beacon placement explicit and inspectable

---

## 7. Validation checklist

Validate in PIE from first-person inside the submarine.

### Visual checks

- warm and cold zones are clearly distinct
- faulty lights are readable but not distracting everywhere
- alarm mood reads immediately in beacon zones
- at least one visible beam exists in a corridor
- at least one visible beam exists in a technical room
- airlock has a distinct mood from the main corridor

### Atmosphere checks

- dust motes are visible only when useful
- local haze does not fog the entire room
- fog does not destroy readability of doors, stations, or walk paths

### Surface checks

- decals support wear and navigation
- warning decals do not look pasted everywhere
- leak and rust placement matches wet or structural logic

### Gameplay readability checks

- helm area remains readable
- doors remain readable
- stations remain readable
- crew traversal path is not hidden by fog or glare

### Alarm checks

- existing flood alarm path still triggers beacon visuals and audio
- added alarm accents do not conflict with beacon readability

---

## 8. Pass / fail criteria

### Pass

The pass is valid only if all are true:

- practical light families read clearly
- selected beams are visible
- atmosphere remains controlled
- alarm beacons are still the authoritative visible alarm actors
- decals improve the look without clutter
- the level remains readable in gameplay

### Fail

The pass fails if any of these happen:

- everything is foggy all the time
- all lights feel the same except color tint
- faulty lights are overused
- alarm accent lights compete with or obscure alarm beacons
- decals become visual noise
- the player cannot read interaction zones cleanly

---

## 9. Recommended first editor session

Do this in one focused pass.

1. Verify the existing light and alarm BPs.
2. Create the three light function materials.
3. Create the two Niagara atmosphere systems.
4. Create the first three decal instances.
5. Place one global fog actor and one PPV.
6. Add 3 to 6 local fog volumes.
7. Replace or tune practicals by light family.
8. Place a few dust or haze systems in hero zones.
9. Add decals only after the lighting reads correctly.
10. Validate alarm readability using the existing flood alarm path.

---

## 10. Session result template

Use this exact format after the editor pass:

```text
Craniata Lighting Session Result

Level used:
Global fog actor:
PostProcessVolume:
Local fog volume count:
Warm light count:
Cold light count:
Faulty light count:
Alarm beacon count:
Alarm accent light count:
Dust mote systems placed:
Steam haze systems placed:
Decal instances placed:
External assets pulled:

Validated:
- ...
- ...

Not validated:
- ...
- ...

Blockers:
- ...

Ready for next implementation step:
- yes / no
```

---

## 11. External references used

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
