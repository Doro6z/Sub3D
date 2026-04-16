# Sub3D - Submarine Blender Integration Handoff

> **SUPERSEDED 2026-04-16** by `C:\Dev\Sub3D\reports\plans\2026-04-16_spec_bridge_replaces_generator.md`.
> The generator calibration path proposed here no longer applies. The Craniata FP integration goes through a Spec Extraction Bridge that reads `main.py` and `stations.json` statically and authors `USubmarineDefinition` directly, bypassing the runtime generator.
> This document remains valid only as historical context for the three pre-existing mesh families under `Content/Submarines/*` and for the generator-era design rationale. Do not execute its step list.

Date: 2026-04-14
Scope: integrate the current Blender submarine blockout into the active Sub3D First Playable pipeline
Authority-max plan:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Supporting references:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_submarine_generator_shape_execution_plan.md`
- `C:\Dev\Sub3D\reports\2026-04-12_submarine_systems_audit.md`
- `C:\Dev\Sub3D\reports\guides\2026-04-10_fp_editor_execution_guide.md`

Workspace: `C:\Dev\Sub3D`
Blender source path: `C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt\`

---

## 0. Executive decision

For First Playable, the submarine integration path is:

1. Blender blockout is the source art and spatial reference.
2. Unreal imported static meshes are visual assets and prefab building blocks.
3. `USubmarineDefinition` remains the only runtime gameplay source of truth.
4. `USubmarineGeneratorSpec` + `USubmarineGeneratorEnvelopeDef` remain the active First Playable authoring path.
5. The airlock must be handled as a prefab actor, not as generated hull geometry.

Do not treat the imported FBX as the gameplay submarine.
Do not move First Playable back to the bake pipeline.
Do not try to derive flood, compartments, stations, or door logic directly from the imported static meshes.

---

## 1. Code-backed current state

These points are verified in the current repository code.

### 1.1 Blender pipeline already exists

`hull_blockout_gpt` already contains a complete procedural blockout pipeline:

- `main.py`
  - builds the full Craniata blockout in Blender
  - sets scene units to centimeters
  - creates hull, decks, bulkheads, doors, airlock cassette, hydroplanes, fins, propulsor, and interior props
- `uv_unwrap.py`
  - applies Smart UV Project per object family
- `assign_materials.py`
  - assigns a hull textured material and solid industrial materials to the rest
- `fix_pivots.py`
  - sets gameplay-oriented pivots for doors, rudder, hydroplanes, propulsor, and turrets
- `fix_turret_pivots.py`
  - refines turret top pivots to base-center-at-bottom
- `export_fbx.py`
  - exports all mesh objects to:
    - `C:\Dev\Sub3D\Content\Sub3D\FirstPlayableRun\Meshes\Blockout\Craniata_Blockout.fbx`

### 1.2 Current Blender model dimensions are explicit

The Blender scripts expose concrete dimensions:

- `LENGTH = 4200.0`
- `HULL_THICK = 15.0`
- `DECK_THICK = 18.0`
- `BH_THICK = 14.0`

The script metadata in `data/stations.json` also confirms:

- length: `4200 cm`
- hull thickness: `15 cm`
- multiple decks
- bulkhead template with standard passage width and height

### 1.3 The active gameplay runtime path is generator-based

`ASubmarineBase::BeginPlay()` currently does this:

1. generate `GeneratedDefinition` from `GeneratorSpec` if needed
2. initialize stations from `GeneratedDefinition`
3. initialize `SubFlood` from `GeneratedDefinition`
4. materialize PMCs through `USubmarineGeneratedGeometryComponent::BuildFromDefinition()`
5. spawn door actors from `GeneratedDefinition->Connections`

That means the active First Playable runtime path is:

`GeneratorSpec -> Generator -> USubmarineDefinition -> MeshBuilder -> GeneratedGeometry -> Flood/Stations/Doors/Crew`

### 1.4 The imported mesh is not enough for gameplay

The imported mesh does not generate any of the following automatically:

- `USubmarineDefinition`
- compartments
- flood graph
- station slots
- spawn points
- runtime door connection data
- crew walkable floor graph

### 1.5 A second authoring pipeline exists but is not the FP runtime path

The repository still contains:

- `USubmarineAuthoringAsset`
- `UCompiledSubmarineAsset`
- `USubmarineAuthoringBakeLibrary`
- `SubCompiler` / bake-time authoring code

That path is richer for multi-deck authoring, but it is not the active First Playable runtime path.
For First Playable, do not switch back to it.

### 1.6 The Blender import content is currently fragmented

The project currently contains at least three imported mesh families:

- `Content/Submarines/BlenderScript`
- `Content/Submarines/BlenderScript2`
- `Content/Submarines/FirstPlayableSub`

This is already too many parallel mesh families for one submarine.

The current `hull_blockout_gpt` script output matches `BlenderScript2` best as an existing reference family.
Evidence:

- `SM_BH_UpperAirlock_Inner`
- `SM_Deck_engine_upper`
- `SM_EngineControlCabinet`
- `SM_HardpointFairing_*`
- `SM_MooringLug_*`

These names exist in the script output and in `Content/Submarines/BlenderScript2`, not in the older families as a complete set.

### 1.7 Airlock runtime remains incomplete

The generator creates an airlock compartment and two connections, but the full visual path is still incomplete:

- `GenerateAirlock()` creates the topological airlock data
- generated interior airlock walls/floor can exist
- no `BP_Airlock` prefab exists in `Content/`
- the authority-max plan explicitly says the airlock should be a prefab actor for FP

This remains true.

---

## 2. Consolidated architecture for this submarine

Use this split and do not blur the responsibilities.

### 2.1 Source art

Authoritative source art:
- `Scripts/Blender/hull_blockout_gpt`

Purpose:
- hull and interior blockout modeling
- pivots
- UVs
- visual mesh generation
- reusable static mesh parts for prefabs

### 2.2 Imported UE static meshes

Authoritative imported mesh family for the new Craniata pass:
- `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/`

Purpose:
- visual reference for silhouette
- reusable static mesh parts
- airlock prefab pieces
- possible collision proxy reference during editor validation

Useful existing reference family:
- `Content/Submarines/BlenderScript2`

Do not use as the active family:
- `Content/Submarines/BlenderScript`
- `Content/Submarines/BlenderScript2`
- `Content/Submarines/FirstPlayableSub`

Keep them read-only reference until explicitly retired.

### 2.3 First Playable gameplay authoring

Authoritative gameplay authoring path:
- `USubmarineGeneratorEnvelopeDef`
- `USubmarineGeneratorSpec`
- `BP_Submarine_*` based on `ASubmarineBase`

Purpose:
- compartments
- bulkheads and connections
- flood graph
- station placement
- crew spawn points
- runtime PMCs
- door spawning

### 2.4 Runtime source of truth

Authoritative runtime source of truth:
- `USubmarineDefinition`

Everything gameplay reads must come from this definition or systems initialized from it.

### 2.5 Airlock stance

Authoritative FP stance:
- airlock topology stays in the generator
- airlock visual closure is done by a prefab actor built from imported meshes
- no procedural boolean union with the hull
- no attempt to make the imported Blender hull the flood topology source

---

## 3. What the Blender model should drive, and what it must not drive

### 3.1 The Blender model must drive

- overall hull length
- hull thickness target
- silhouette targets for bow, body, stern
- broad compartment rhythm
- broad deck rhythm
- airlock location intent
- asset pieces reused by visual prefabs

### 3.2 The Blender model must not directly drive in FP

- flood graph generation
- compartment runtime ids
- door runtime ids
- station runtime transforms
- crew floor snapping logic
- runtime collision source
- runtime structural breach logic

### 3.3 Current mismatch that must be accepted for FP

The Blender model already expresses:

- upper deck
- main deck
- lower deck
- many props and detailed geometry
- detailed rear upper airlock pocket/cassette

The current FP generator path does not fully express that richness.
It currently supports a simplified runtime model:

- one hull envelope
- longitudinal compartments from bulkheads
- simplified interior meshes
- simplified station placement
- topological airlock with incomplete visual closure

For FP, approximate the Blender blockout in the generator path.
Do not attempt a 1:1 translation of every deck and prop into generator logic.

---

## 4. Required active folders and naming

This is the consolidation rule for this submarine.

### 4.1 Blender

Keep using:
- `C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt\`

### 4.2 Raw export

Keep the script export target as-is for now:
- `C:\Dev\Sub3D\Content\Sub3D\FirstPlayableRun\Meshes\Blockout\Craniata_Blockout.fbx`

This is the current script reality.
Do not create another export script path for the same submarine during FP.

### 4.3 Imported assets

Use one active UE import family for the new Craniata pass:
- `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/`

Required rule:
- import the FBX into this folder
- do not import the new pass over `BlenderScript`
- do not import the new pass over `BlenderScript2`
- do not import the new pass over `FirstPlayableSub`

Reason:
- the object names are generic `SM_*`
- folder isolation is the only safe way to avoid another ambiguous mesh family

### 4.4 Gameplay assets

Create or duplicate into:
- `/Game/Sub3D/FirstPlayableRun/DA_Envelope_Craniata`
- `/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_Craniata`
- `/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata`
- `/Game/Sub3D/FirstPlayableRun/BP_Airlock_Craniata`

Do not keep using only the generic `*_FPRun` assets once Craniata becomes the active submarine.

---

## 5. Required execution order

Execute in this order.

### Step 1 - Freeze the runtime path before touching assets

Required decision:
- FP runtime stays on the generator path
- imported static meshes are support assets only

Validation:
- everyone working on the sub uses the same assumption

Failure:
- if anyone starts wiring gameplay from static meshes directly, stop and revert the editor setup

### Step 2 - Build the Blender blockout

Run in Blender in this order:

1. `main.py`
2. `uv_unwrap.py`
3. `assign_materials.py`
4. `fix_pivots.py`
5. `fix_turret_pivots.py`
6. `export_fbx.py`

Validation:
- Blender scene is in centimeters
- mesh objects are present
- pivots are corrected
- `Craniata_Blockout.fbx` is written to the export path

Failure:
- if the export file is not created, stop before opening Unreal

### Step 3 - Import the FBX into a dedicated UE folder

Required import folder:
- `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/`

Required import settings, based on `export_fbx.py` intent:

- `Auto Generate Collision`: Off
- `Generate Lightmap UVs`: Off
- `Normal Import Method`: Import Normals and Tangents
- `Material Import Method`: Do Not Create Materials

Required rule:
- this import family becomes the active visual reference family for the new Craniata pass

Failure:
- if the import lands in an older family folder, delete that import pass and re-import cleanly

### Step 4 - Duplicate the current FP assets into Craniata-specific assets

Duplicate:

- `DA_Envelope_FPRun` -> `DA_Envelope_Craniata`
- `DA_SubGenSpec_FPRun` -> `DA_SubGenSpec_Craniata`
- `BP_Submarine_FPRun` -> `BP_Submarine_Craniata`

Required rule:
- do not overwrite the current FPRun assets while calibrating Craniata

Purpose:
- keep a stable fallback working set
- make the Craniata pass reviewable and isolated

### Step 5 - Translate the Blender dimensions into the generator assets

Use these starting values.

For `DA_Envelope_Craniata`:

- `SpineLengthCm = 4200`
- `ExteriorHullOffsetCm = 15`
- `BodyLengthFraction = 0.55` as a starting value only
- `WidthToHeightRatio` must be measured against the imported hull side and front views
- `DefaultRadiusCm` must be calibrated against the imported hull

For `DA_SubGenSpec_Craniata`:

- `WallThicknessCm = 15`
- `FloorDropBiasCm = 0` for the first pass
- `AirlockPositionNormalized = 0.69` as the starting point
- `AirlockSide = Top`

Why these values:

- `4200` comes directly from the Blender script
- `15` matches the current Blender hull thickness
- `AirlockPositionNormalized ~= 0.694` is encoded in `main.py`
- the current Blender airlock is an upper rear airlock, so `Top` is the correct starting side

Important distinction:

- `WallThicknessCm` drives runtime compartment math and floor depth
- `ExteriorHullOffsetCm` drives the exterior hull surface in the mesh builder

For the current Blender blockout, start with both at `15` so the generated outer hull aligns with the visual thickness intent.

### Step 6 - Rebuild and calibrate the generator silhouette against the imported blockout

In the editor:

1. place the imported Craniata meshes in a validation level or as hidden reference
2. place `BP_Submarine_Craniata`
3. assign `DA_SubGenSpec_Craniata`
4. use `Rebuild From Spec`
5. compare generator hull against the imported blockout from:
   - side view
   - front view
   - 3/4 exterior view

Calibrate only these fields first:

- `DefaultRadiusCm`
- `WidthToHeightRatio`
- `BowProfile`
- `SternProfile`
- `BowCapLengthCm`
- `SternCapLengthCm`
- `BowSharpness`
- `SternSharpness`
- `BodyLengthFraction`

Do not touch topology during this silhouette pass.

### Step 7 - Reduce the Blender interior to the generator-supported gameplay layout

The current generator path supports a simplified longitudinal gameplay layout.
Use the Blender model to decide:

- number of FP compartments
- bulkhead positions
- door widths and heights
- whether the mid-body should read as one or two crew spaces

Then encode only that subset into:
- `BulkheadPositionsNormalized`
- `Passages`
- `RequestedStations`

Required stance:
- do not try to encode every deck and prop in the spec
- use the Blender layout to choose the right simplified FP compartment rhythm

### Step 8 - Keep station expectations realistic

The Blender blockout contains detailed props:

- helm console
- display
- engine room props
- turret stations
- lockers
- storage

The current generator station placement is coarse:

- `Helm` goes to the Helm compartment
- `Engine` goes to the Engine compartment
- other station types fall back to the first Crew compartment

Implication:
- precise station transforms from the Blender model are not preserved automatically

Required FP rule:
- request only the station types needed for FP validation
- use editor placement adjustments later only if required for readability
- do not rewrite generator placement logic just to match decorative Blender props

### Step 9 - Build the airlock as a prefab actor from imported meshes

This is required.

Current state:
- no `BP_Airlock` exists in content
- the authority-max plan requires a prefab airlock for FP

Required action:
- create `BP_Airlock_Craniata`
- build it from the imported static mesh pieces, preferably from the current Blender pass or the matching `BlenderScript2` family

Expected components:

- root scene component
- static meshes for cassette and collar pieces
- inner door visual piece
- outer hatch visual piece
- collision volume
- optional light

Required rule:
- this prefab is visual and interaction-facing
- it does not replace the generator airlock topology

### Step 10 - Wire the submarine Blueprint to the runtime path

In `BP_Submarine_Craniata`:

- assign `Generator Spec = DA_SubGenSpec_Craniata`
- leave `Generated Definition` empty
- assign `GeneratorDoorActorClass`
- once `BP_Airlock_Craniata` exists, assign the future airlock class when code support is added

Until airlock spawn code exists, the prefab remains a manual editor actor or a pending integration item.

### Step 11 - Validate the runtime path, not only the visuals

The pass is valid only if these runtime checks pass:

- `Rebuild From Spec` succeeds
- compartments are generated
- connections are generated
- flood graph initializes
- doors spawn from definition
- crew can use interior walkable floors
- no legacy fallback warning is emitted for the active submarine

The pass is not valid if only the imported mesh looks correct.

---

## 6. Recommended starting values for Craniata

These are starting values, not frozen final numbers.

### 6.1 Envelope

Start here in `DA_Envelope_Craniata`:

- `SpineLengthCm = 4200`
- `ExteriorHullOffsetCm = 15`
- `DefaultRadiusCm = 415`
- `WidthToHeightRatio = 1.0`
- `BowProfile = Blunt`
- `SternProfile = Tapered`
- `BowCapLengthCm = 240`
- `SternCapLengthCm = 320`
- `BowSharpness = 1.0`
- `SternSharpness = 1.0`
- `BodyLengthFraction = 0.55`

Why `DefaultRadiusCm = 415`:
- the Blender station data peaks around `420 cm`
- the handcrafted hull curve in `main.py` peaks around `430 cm`
- `415` is a safe starting midpoint for generator calibration

This must still be validated visually in editor.

### 6.2 Generator spec

Start here in `DA_SubGenSpec_Craniata`:

- `WallThicknessCm = 15`
- `FloorDropBiasCm = 0`
- `AirlockPositionNormalized = 0.694`
- `AirlockSide = Top`

For the first pass, keep a small and clear compartment set.
Suggested starting longitudinal rhythm:

- forward helm
- mid crew
- aft crew or systems corridor
- engine
- top airlock

This keeps the runtime topology aligned with the current FP design.

### 6.3 Requested stations

Keep the first pass minimal:

- `Helm`
- `Engine`
- `Ballast`

Add `Turret` only if the current FP loop needs it immediately.
Do not add hidden station types just because the Blender model contains related props.

---

## 7. Current hard limits that must be accepted

### 7.1 The generator cannot reproduce the full Blender interior

The current generator path does not fully encode:

- three true decks with independent walkable networks
- ladders and hatches as a authored traversal graph
- detailed equipment placement
- exact turret station transforms from Blender
- exact prop layout from Blender

### 7.2 The imported static mesh cannot replace `USubmarineDefinition`

Even if the blockout looks complete, the gameplay systems still need:

- compartment ids
- hydro bounds
- connections
- flood graph
- spawn points
- station slots

### 7.3 The airlock still needs dedicated runtime integration

The Blender model already contains strong airlock art pieces.
The runtime still needs:

- `BP_Airlock_Craniata`
- spawn code from `GeneratedDefinition`
- connection alignment with `Airlock_Inner` and `Airlock_Outer`

### 7.4 Imported mesh content is already duplicated

Do not create a fourth long-lived mesh family.
The new import pass must live in one dedicated folder and become the active family for Craniata.

---

## 8. Explicit do-not-do list

Do not do any of the following during FP integration.

1. Do not make the imported Blender hull the flood or compartment source.
2. Do not reactivate the bake pipeline as the FP runtime source.
3. Do not keep editing `DA_SubGenSpec_FPRun` once `Craniata` assets exist.
4. Do not import the new Craniata pass over `BlenderScript`, `BlenderScript2`, and `FirstPlayableSub` all at once.
5. Do not try to boolean-cut the generated hull to merge the airlock.
6. Do not try to preserve every Blender prop as runtime gameplay logic.
7. Do not add a second authoring truth for stations and compartments outside the generator assets.

---

## 9. Immediate editor execution tasks

This is the practical editor checklist for the next session.

1. Run the Blender scripts in order and produce `Craniata_Blockout.fbx`.
2. Import that FBX into `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/`.
3. Duplicate the three FP assets into `*_Craniata` variants.
4. Set the starting envelope/spec values listed above.
5. Place `BP_Submarine_Craniata` in a validation level.
6. Rebuild from spec and align the silhouette against the imported blockout.
7. Encode only the FP compartment rhythm and passages.
8. Create `BP_Airlock_Craniata` from the imported airlock meshes.
9. Validate doors, flood, stations, and crew movement from the generator path.

---

## 10. Pass criteria

The Craniata integration pass is a pass only if all of these are true:

- the Blender export completes
- the import lands in one dedicated folder
- Craniata-specific FP assets exist
- the generator silhouette is calibrated against the imported hull
- the active submarine runtime still uses `GeneratedDefinition`
- doors spawn from `GeneratedDefinition`
- crew can walk the generated interior floors
- no legacy fallback warning is emitted by the active Craniata submarine

The pass is not complete if only the imported mesh looks correct in the viewport.

---

## 11. Suggested next code step after the editor pass

After the editor pass closes the Craniata asset path, the next code step should be:

1. add prefab airlock spawn support to `ASubmarineBase`
2. bind that spawn to the generated airlock compartment and connections
3. keep the prefab path separate from generated hull geometry

This is the missing bridge between the imported Blender airlock pieces and the current runtime topology.

---

## 12. Session result template

Use this exact return format after the editor execution:

```text
Craniata Integration Session Result

Blender export:
Import folder:
Active mesh family:
Envelope asset:
Spec asset:
Submarine BP:
Airlock BP:
Level used:

Validated:
- ...
- ...

Not validated:
- ...
- ...

Blockers:
- ...

Ready for code step:
- yes / no
```
