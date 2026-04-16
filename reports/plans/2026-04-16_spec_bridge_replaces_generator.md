# Sub3D - Spec Bridge Replaces Generator

Date: 2026-04-16
Scope: lock the new First Playable submarine integration path around a Spec Extraction Bridge that reads the existing Blender authoring data and produces `USubmarineDefinition` directly, bypassing the runtime generator.

Supersedes:
- `C:\Dev\Sub3D\reports\guides\2026-04-14_submarine_blender_integration_handoff.md`

Upstream authority (unchanged for FP scope):
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Workspace: `C:\Dev\Sub3D`
Blender source: `C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt\`
UE runtime root: `C:\Dev\Sub3D\Source\Sub3D\`

---

## 0. Executive decision

For First Playable:

1. The Blender authoring script `hull_blockout_gpt` and its data files are the authoritative submarine spec.
2. The submarine model in UE is driven by a `USubmarineDefinition` data asset authored offline from that spec.
3. The runtime generator (`USubmarineGenerator` + `USubmarineGeneratorSpec`) is not called for the Craniata submarine. It remains as dormant legacy code during FP.
4. The visible hull and interior meshes are imported static meshes placed as components on the submarine Blueprint. The procedural mesh builder is not used for Craniata.
5. The airlock is an authored prefab actor built from imported Blender meshes, with two door actors (inner and outer).

Do not treat the runtime generator as part of the FP path.
Do not edit the Blender model or re-run `main.py` to extract the spec.
Do not hand-edit the submarine definition asset in the UE editor once the bridge can regenerate it.

---

## 1. Why this replaces the 2026-04-14 handoff

The 14 April handoff assumed the FP runtime stayed on the generator path and that the Blender model was silhouette reference only. Two facts make that assumption obsolete:

1. The user has edited the Blender model after the last script run. Re-running `main.py` would destroy those edits. The generator calibration loop proposed on 14 April required iteratively regenerating to align the procedural hull with the Blender model. This is no longer safe.
2. The Blender script already expresses the submarine topology as structured Python data plus a pure JSON data file. Deriving `USubmarineDefinition` from that spec is less work than calibrating procedural parameters to approximate it.

The 14 April handoff is not discarded. It remains valid as historical context for the three mesh families that already exist in `Content/Submarines/*` and the generator-era decisions. Its execution steps, starting values, and generator calibration sections are not applicable.

---

## 2. Hard constraints

These are non-negotiable for FP.

### 2.1 Source spec is frozen

- Do not modify `Scripts/Blender/hull_blockout_gpt/main.py`.
- Do not modify any file inside `Scripts/Blender/hull_blockout_gpt/core/`.
- Do not modify `Scripts/Blender/hull_blockout_gpt/data/stations.json`.
- Do not re-run `main.py` against the current Blender scene.

Reason: the Blender model is the current art reference and contains manual edits made after the last script execution. Re-running `main.py` would discard those edits. Modifying the script would drift the bridge source from the current art.

### 2.2 Generator stays dormant

- Do not call `USubmarineGenerator::Generate()` for the Craniata submarine.
- Do not create or edit `USubmarineGeneratorSpec` assets for Craniata.
- Do not delete the generator code during FP.

Reason: the existing BeginPlay path already skips the generator when a pre-authored `USubmarineDefinition` is assigned. Leaving the generator in place avoids breaking other in-flight assets that may still depend on it. Deletion is a post-FP task.

### 2.3 Procedural mesh builder stays dormant

- Do not call `USubmarineGeneratedGeometryComponent::BuildFromDefinition()` for Craniata.
- Do not populate procedural mesh arrays (`ExteriorHullMesh`, `InteriorMeshes`, `BulkheadMeshes`) on the Craniata definition.

Reason: the visible geometry comes from imported static meshes attached to `BP_Submarine_Craniata`. The procedural builder is only needed when the definition produces its own mesh data. Feeding static mesh vertex data through the procedural pipeline adds a conversion step with no benefit.

### 2.4 Single source of truth

- The Spec Extraction Bridge is the only path that writes to `DA_SubDef_Craniata.uasset`.
- Manual edits to that asset inside the UE editor are disallowed, except for transient debugging.
- If a value needs to change permanently, the bridge or its source data must change.

Reason: any manual override on the definition is erased on the next bridge run. The single-source-of-truth rule keeps the spec and the runtime aligned.

---

## 3. Architecture

### 3.1 Data flow

```
Scripts/Blender/hull_blockout_gpt/main.py                   (frozen authoring script)
Scripts/Blender/hull_blockout_gpt/data/stations.json        (frozen hull profile data)
        |
        | static read (AST parse, no execution, no Blender)
        v
Scripts/Blender/hull_blockout_gpt/export_sub_definition.py  (new, standalone Python 3)
        |
        | writes
        v
Content/Sub3D/FirstPlayableRun/Craniata_Definition.json     (bridge output, versioned)
        |
        | read by UE editor command
        v
Source/scripts/import_sub_definition.py                      (new, runs under UnrealEditor-Cmd Python)
        |
        | fills
        v
Content/Sub3D/FirstPlayableRun/DA_SubDef_Craniata.uasset     (USubmarineDefinition, authored)
        |
        | assigned to
        v
Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata         (ASubmarineBase subclass)
        |
        | at BeginPlay: GeneratedDefinition already set, generator is skipped
        v
Runtime: USubFloodComponent, USubmarineStationManagerComponent, door spawn, crew
```

### 3.2 Responsibilities

- `main.py` and `stations.json`: authoritative submarine spec. Read-only.
- `export_sub_definition.py`: extract spec from the frozen source, compute derived topology, emit a single JSON with a stable schema.
- `Craniata_Definition.json`: versioned bridge output. Checked into git. Human-readable.
- `import_sub_definition.py`: UE Python editor utility that reads the JSON and populates `DA_SubDef_Craniata.uasset`.
- `DA_SubDef_Craniata.uasset`: runtime source of truth for the Craniata submarine topology. Not hand-edited.
- `BP_Submarine_Craniata`: Blueprint subclass of `ASubmarineBase`. Holds imported static mesh components for visual hull and interior, and references the definition asset.
- `BP_Airlock_Craniata`: Blueprint actor containing the airlock mesh assembly and two door actors.

### 3.3 What the bridge does not do

- The bridge does not touch the generator code.
- The bridge does not create mesh data. Meshes stay as imported FBX static meshes in UE.
- The bridge does not place meshes in the scene. That is manual Blueprint construction work done once.
- The bridge does not set up navigation or crew walkable surfaces. Those come from collision on the imported meshes and standard UE nav generation.

---

## 4. Code-verified facts

The following are verified against the current repository.

### 4.1 `USubmarineDefinition` is authorable

`USubmarineDefinition` inherits from `UDataAsset`. Its topology properties (`Compartments`, `Connections`, `StationSlots`, `SpawnPoints`, `FloodGraph`) are plain UPROPERTY structs. They can be populated from C++, from Blueprint, or from a Python editor script. The engine does not require them to come from the generator.

### 4.2 `ASubmarineBase::BeginPlay()` accepts a pre-authored definition

The existing branch is:

```
if (!GeneratedDefinition && GeneratorSpec) {
    USubmarineGenerator* Generator = NewObject<USubmarineGenerator>(this);
    GeneratedDefinition = Generator->Generate(GeneratorSpec);
}
```

When `GeneratedDefinition` is assigned, the generator is skipped. No code change required for the bridge path.

### 4.3 Downstream systems are origin-agnostic

- `USubFloodComponent::InitializeFromDefinition` reads `FloodGraph`, `Compartments`, `Connections`.
- `USubmarineStationManagerComponent::SpawnStationsFromDefinition` reads `StationSlots`.
- Door spawning reads `Connections`.

None of these systems check who produced the definition.

### 4.4 Spec source is statically extractable

- `data/stations.json` is pure JSON. Usable as-is with `json.load`.
- `main.py` top-level constants are literals or simple expressions: dict literals, list literals, arithmetic `BinOp`, `Subscript` into previously-defined dicts, references to previously-defined names.
- These can be evaluated by a small safe evaluator driven by `ast.parse`, with no execution of imports, no `bpy`, no Blender runtime.

### 4.5 Runtime content already has a starting submarine BP

`Content/Sub3D/FirstPlayableRun/BP_Submarine_FPRun.uasset` exists and subclasses `ASubmarineBase`. It is the template to duplicate for `BP_Submarine_Craniata`.

### 4.6 Mesh family candidates are imported

`Content/Submarines/BlenderScript/` already contains a prior import of the same pipeline family (airlock collar, ballast, decks, bulkheads, turret hardpoints, doors). This family is a viable reuse target for `BP_Submarine_Craniata` and `BP_Airlock_Craniata` components, pending a single consolidation pass.

---

## 5. Required folder and naming

### 5.1 Scripts

Keep in place, do not move:
- `Scripts/Blender/hull_blockout_gpt/main.py`
- `Scripts/Blender/hull_blockout_gpt/core/`
- `Scripts/Blender/hull_blockout_gpt/data/stations.json`

Add, in the same folder:
- `Scripts/Blender/hull_blockout_gpt/export_sub_definition.py`

The export script lives next to `main.py` because the spec source is the Blender authoring tree. It must not be imported from `Scripts/Blender/hull_blockout_gpt/core/`.

### 5.2 UE import utility

Add, under the existing UE scripts folder:
- `Source/scripts/import_sub_definition.py`

This is a UE Python editor utility invoked from `UnrealEditor-Cmd.exe`.

### 5.3 Bridge output

Output lives under content, committed to git:
- `Content/Sub3D/FirstPlayableRun/Craniata_Definition.json`

### 5.4 Runtime assets

- `Content/Sub3D/FirstPlayableRun/DA_SubDef_Craniata.uasset`  (USubmarineDefinition)
- `Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata.uasset`  (ASubmarineBase subclass)
- `Content/Sub3D/FirstPlayableRun/BP_Airlock_Craniata.uasset`  (airlock prefab actor)

Do not create:
- any `DA_Envelope_Craniata`
- any `DA_SubGenSpec_Craniata`

Those exist in the superseded 14 April handoff. They are no longer part of the FP path.

### 5.5 Mesh family policy

Pick one mesh family for Craniata and use only that. The two viable options are:

- reuse `Content/Submarines/BlenderScript/` as-is
- import a fresh pass into `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/`

Do not blend both families in `BP_Submarine_Craniata`.
Do not reimport the same FBX over `Content/Submarines/BlenderScript/` or `Content/Submarines/BlenderScript2/`.

The choice is deferred to the first editor session. The bridge JSON does not encode which mesh family is used; that decision lives in `BP_Submarine_Craniata` component references.

---

## 6. Bridge JSON schema

The bridge output is a single JSON file. This is the contract between the Blender-side export script and the UE-side import utility.

### 6.1 Top-level keys

```
{
  "schema_version": 1,
  "source_hashes": { "main_py": "...", "stations_json": "..." },
  "meta": { "sub_name": "Craniata", "length_cm": 4200.0, ... },
  "hull": { "profile_samples": [ ... ] },
  "decks": [ ... ],
  "bulkheads": [ ... ],
  "compartments": [ ... ],
  "connections": [ ... ],
  "flood_graph": { "nodes": [...], "edges": [...] },
  "station_slots": [ ... ],
  "spawn_points": [ ... ],
  "airlock": { ... },
  "turret_hardpoints": [ ... ]
}
```

### 6.2 Key contents

- `meta` — copied from `main.py` scalar constants (`LENGTH`, `HULL_THICK`, `DECK_THICK`, `BH_THICK`, `SUB_NAME`).
- `hull.profile_samples` — copied from `data/stations.json` station list. Retains 16 cross sections.
- `decks` — copied from `data/stations.json` decks list. Three entries (`upper`, `main`, `lower`).
- `bulkheads` — union of `MAIN_BULKHEAD_SPECS`, `LOWER_BULKHEAD_SPECS`, `UPPER_AIRLOCK_EXIT_BULKHEAD`, `UPPER_AIRLOCK_INNER_BULKHEAD`. Each entry carries `name`, `x_norm`, `door_w`, `door_h`, `deck`.
- `compartments` — derived topology. Rules are in section 7.
- `connections` — derived topology. Rules are in section 7.
- `flood_graph` — derived topology. Rules are in section 7.
- `station_slots` — authored in the bridge script for FP stations (Helm, Engine, Ballast). Positions are expressed as `x_norm` + `deck` + `y_offset_cm` + `yaw_deg`.
- `spawn_points` — one per compartment. Authored in the bridge script with a simple centered-on-compartment rule.
- `airlock` — inner and outer x_norm + connection ids pointing into `connections`.
- `turret_hardpoints` — copied from `TURRET_HARDPOINTS` in `main.py`.

### 6.3 Source hashes

The bridge computes SHA-256 of `main.py` and `data/stations.json` and records them in `source_hashes`. The UE import utility refuses to run if both hashes differ from what the bridge recorded, unless invoked with `--force`.

Reason: catch accidental runs against a modified script before they corrupt the runtime asset.

---

## 7. Topology derivation rules

These rules are fixed so the output is reproducible.

### 7.1 Compartments

A compartment is a 3D volume bounded by two adjacent bulkheads on a given deck.

Per-deck compartment set = sorted bulkhead x_norm list for that deck, plus implicit x_norm=0 (bow) and x_norm=1 (stern).

- Main deck bulkheads: `MAIN_BULKHEAD_SPECS` + `UPPER_AIRLOCK_INNER_BULKHEAD` + `UPPER_AIRLOCK_EXIT_BULKHEAD`.
- Lower deck bulkheads: `LOWER_BULKHEAD_SPECS`.
- Upper deck bulkheads: same as main deck at this FP iteration. If divergence is needed later, it becomes a bridge input.

For each deck, sorted bulkhead x_norm list `[x_0, x_1, ..., x_n]` with `x_0 = 0.0` and `x_n = 1.0`:

- Compartment `i` occupies `[x_{i}, x_{i+1}]` on that deck.
- Compartment id: `C_<deck>_<index>` (for example `C_main_02`).
- `min_x_cm = x_{i} * LENGTH`, `max_x_cm = x_{i+1} * LENGTH`.
- `length_cm = max_x_cm - min_x_cm`.

### 7.2 Semantic compartment names

Named compartments derive from FP design intent and are fixed in the bridge script:

- Main deck: `Bow`, `Fwd`, `Control`, `Aft`, `AirlockInner`, `Airlock`, `Stern`.
- Lower deck: `Bow`, `BallastFwd`, `Hub`, `BallastAft`, `Stern`.
- Upper deck: mirrors main deck footprint for FP; no independent semantic split.

The mapping from `(deck, index)` to semantic name is declared as a table in the bridge script, not guessed at import time.

### 7.3 Connections

A connection is a walkable or flood-conductive link between two compartments.

- Each bulkhead produces one connection between the two compartments it divides on its deck.
- Connection id: `N_<deck>_<bh_name>` (for example `N_main_Fwd`).
- `door_w_cm`, `door_h_cm`: from the bulkhead spec.
- `sill_z_cm`: from deck z + `DOOR_THRESHOLD`.
- Connection type: `WatertightDoor` by default, `Airlock` for the two airlock bulkheads.

Vertical access between decks comes from `MAIN_DECK_CUTOUTS`, `UPPER_DECK_CUTOUTS`, `LOWER_DECK_CUTOUTS`:

- Each cutout produces a vertical connection between the two decks it crosses at that x_norm.
- Connection id: `N_vert_<cutout_name>` (for example `N_vert_LowerAccess`).
- Connection type: `Hatch` or `Ladder`.

### 7.4 Flood graph

- Nodes: one per compartment.
- Edges: one per connection.
- Edge capacity: `door_w_cm * door_h_cm` in mm² for watertight doors; cutout area for vertical connections.
- The edge weight and ordering are deterministic and stable across bridge runs.

### 7.5 Airlock

- Inner connection: `N_main_UpperAirlock_Inner`. Endpoints: `Aft` and `Airlock`.
- Outer connection: `N_main_UpperAirlock_Exit`. Endpoints: `Airlock` and `Exterior` (special sentinel node `EXT`).
- The airlock prefab actor in UE reads these two connection ids and spawns one door actor per side.

---

## 8. Extraction method for `main.py`

The bridge reads `main.py` without executing it.

### 8.1 Why not import

- `main.py` imports `bpy` at the top. Importing requires Blender's Python.
- `main.py` also runs side effects on import (scene setup, modifier evaluation).
- Stubbing `bpy` is brittle and invites drift.

### 8.2 AST walk

Use `ast.parse(source)` with `mode="exec"`. Walk only top-level `Assign` nodes. Resolve targets whose names are in the extraction whitelist.

Whitelist (initial):

- Scalars: `LENGTH`, `SUB_NAME`, `HULL_THICK`, `DECK_THICK`, `BH_THICK`, `DOOR_W`, `DOOR_H`, `DOOR_LOWER_W`, `DOOR_LOWER_H`, `DOOR_THRESHOLD`, `SAS_LENGTH_CM`, `DECK_MAIN_Z`, `DECK_LOWER_Z`, `HYBRID_DECK_Z`, `LOWER_HUB_START`, `LOWER_HUB_END`.
- Containers: `MAIN_BULKHEAD_SPECS`, `LOWER_BULKHEAD_SPECS`, `UPPER_AIRLOCK_EXIT_BULKHEAD`, `UPPER_AIRLOCK_INNER_BULKHEAD`, `MAIN_DECK_CUTOUTS`, `UPPER_DECK_CUTOUTS`, `LOWER_DECK_CUTOUTS`, `BALLAST_PAIRS`, `TURRET_HARDPOINTS`.

Evaluator supported node kinds:

- `Constant` (numbers, strings, booleans)
- `Name` (look up in the local namespace built so far)
- `List`, `Tuple`, `Dict`, `Set`
- `UnaryOp` with `USub`, `UAdd`
- `BinOp` with `Add`, `Sub`, `Mult`, `Div`
- `Subscript` with constant or evaluated index

Unsupported node kinds fail loud with a clear message naming the offending assignment.

### 8.3 Failure behavior

If a whitelisted name does not exist in the source, or uses an unsupported node kind, the bridge aborts with a non-zero exit code and prints the failing assignment with its line number. It does not write a partial JSON.

This keeps the bridge honest: a drift between source and extractor is surfaced before any UE-side work runs.

---

## 9. Execution order

### Step 1 - Lock the constraints

Required action:
- confirm `main.py` will not be modified or re-run
- confirm the generator is out of the FP execution path
- confirm the procedural mesh builder is out of the FP execution path

Validation:
- this plan is committed
- the 2026-04-14 handoff is marked superseded in the `reports/guides/` directory (comment at top or filename suffix)

### Step 2 - Implement the Spec Extraction Bridge

File: `Scripts/Blender/hull_blockout_gpt/export_sub_definition.py`

Requirements:
- Python 3 standard library only.
- Reads `main.py` via AST parse.
- Reads `data/stations.json` via `json.load`.
- Emits `Content/Sub3D/FirstPlayableRun/Craniata_Definition.json` with the schema in section 6.
- Hash both source files with SHA-256 and record them in the output.
- Exit non-zero on any extraction failure.

Validation:
- running the script from the workspace root produces the JSON.
- running it twice produces byte-identical output.
- editing any whitelisted constant and re-running changes the JSON in the expected field.

### Step 3 - Implement the UE import utility

File: `Source/scripts/import_sub_definition.py`

Requirements:
- Runs under `UnrealEditor-Cmd.exe` with the Python plugin enabled.
- Reads `Content/Sub3D/FirstPlayableRun/Craniata_Definition.json`.
- Verifies `source_hashes` match the current `main.py` and `stations.json` on disk. Fail loud unless `--force` is passed.
- Creates `DA_SubDef_Craniata.uasset` at `/Game/Sub3D/FirstPlayableRun/` if missing.
- Populates `Compartments`, `Connections`, `StationSlots`, `SpawnPoints`, `FloodGraph`.
- Does not populate mesh arrays.
- Saves the asset.

Validation:
- the produced asset opens in the editor.
- `Compartments.Num()` matches the expected count from the JSON.
- `Connections.Num()` matches.
- `StationSlots.Num()` equals the requested FP station count.
- the asset survives editor restart.

### Step 4 - Duplicate the submarine Blueprint

- Duplicate `BP_Submarine_FPRun` to `BP_Submarine_Craniata`.
- Do not modify `BP_Submarine_FPRun`.
- Assign `GeneratedDefinition = DA_SubDef_Craniata` on `BP_Submarine_Craniata`.
- Clear `GeneratorSpec` on `BP_Submarine_Craniata` so nothing can silently regenerate.
- Attach imported static meshes from the chosen mesh family as child components under a single `Meshes_Visual` scene root.

Validation:
- placing the Blueprint in a test level and hitting Play does not call the generator (log grep for generator entry message).
- doors spawn at the expected compartment boundaries.
- crew can board and walk the interior.

### Step 5 - Build the airlock prefab

- Create `BP_Airlock_Craniata` at `/Game/Sub3D/FirstPlayableRun/`.
- Compose it from the airlock meshes in the chosen family (`Airlock_Collar`, `Airlock_Pocket_Port`, `Airlock_Pocket_Stbd`, `Airlock_FlatExit`, `Airlock_Door_*`).
- Add two door actor children, one for the inner airlock bulkhead and one for the outer.
- Expose two connection id properties on the Blueprint so it can bind to the two airlock connections from the definition.

Validation:
- the prefab can be placed independently in a level and interacted with.
- when attached to the submarine, both doors obey the same open/close logic as the generator-era doors.

### Step 6 - Validate runtime

Required runtime checks:

- BeginPlay completes without calling the generator.
- Flood simulation produces compartment-to-compartment flow that matches the connections.
- Doors spawn at the connection positions from the definition.
- Station interactions (Helm, Engine, Ballast) work against the authored station slots.
- Crew movement between compartments works across all doors and cutouts.

Optional, useful to keep on:
- the generator code still compiles.
- `BP_Submarine_FPRun` still works as a fallback with its legacy generator path.

---

## 10. What the Blender script keeps driving, and what it does not

### 10.1 Still drives

- Hull dimensions (`LENGTH`, `HULL_THICK`, etc.).
- Bulkhead positions (per `MAIN_BULKHEAD_SPECS` and `LOWER_BULKHEAD_SPECS`).
- Airlock bulkhead positions.
- Deck z positions.
- Door dimensions.
- Ballast compartment positions.
- Turret hardpoint positions.
- Hull cross-section profile (from `stations.json`).
- Deck vertical cutouts (ladder positions).

These flow through the bridge into the runtime definition.

### 10.2 No longer drives anything

- The procedural hull mesh generator (`USubmarineMeshBuilder` hull math).
- The procedural interior mesh generator.
- The envelope/spec calibration loop from the 14 April handoff.

### 10.3 Does not drive yet

- Station transforms precise enough to replace authored in-Blueprint placement. The bridge exports coarse station anchors; fine placement remains a one-time Blueprint pass.
- Crew walkable floor topology. Nav generation handles this through imported mesh collision.

---

## 11. Hard limits we accept for FP

### 11.1 Manual edits on the Blender model are not captured by the bridge

The bridge reads `main.py` and `stations.json`, not the current state of the Blender scene. If the user moved a deck by hand in Blender after the last script run, that edit is invisible to the bridge. The bridge output still reflects the pre-edit spec.

For FP this is acceptable: the manual edits are cosmetic, and the runtime topology matches the script intent. If a manual edit moves a bulkhead far enough to shift gameplay meaning, the script constants must be updated first, then the bridge re-run, then the model re-baked from the script. That is a post-FP workflow.

### 11.2 Single submarine

The bridge targets Craniata only. Supporting multiple submarines means multiple bridge inputs or a configurable source path. This is out of scope for FP.

### 11.3 Dormant generator

The generator compiles and sits unused. Any test that exercises the generator path still passes. Removing the generator is a post-FP cleanup task and is not covered by this plan.

### 11.4 Manual Blueprint work remains

The bridge does not place mesh components in `BP_Submarine_Craniata`. Attaching the imported static meshes is a one-time editor operation.

---

## 12. Do-not-do list

1. Do not modify `main.py`, `core/*.py`, or `data/stations.json`.
2. Do not re-run `main.py` against the current scene.
3. Do not call `USubmarineGenerator::Generate()` for Craniata.
4. Do not populate procedural mesh arrays on `DA_SubDef_Craniata`.
5. Do not hand-edit `DA_SubDef_Craniata.uasset` in the editor beyond debug inspection.
6. Do not create `DA_Envelope_Craniata` or `DA_SubGenSpec_Craniata`.
7. Do not reimport the new FBX over `Content/Submarines/BlenderScript` or `Content/Submarines/BlenderScript2`.
8. Do not skip the hash check in the UE import utility without `--force` and a clear reason.
9. Do not expand the bridge to handle multiple submarines during FP.
10. Do not start deleting generator code during FP.

---

## 13. Immediate tasks

Ordered, no parallelism assumed.

1. Commit this plan.
2. Mark `2026-04-14_submarine_blender_integration_handoff.md` as superseded (header note pointing to this plan).
3. Write `export_sub_definition.py`.
4. Run the bridge once and commit `Craniata_Definition.json`.
5. Write `import_sub_definition.py`.
6. Produce `DA_SubDef_Craniata.uasset` from the JSON.
7. Duplicate `BP_Submarine_FPRun` to `BP_Submarine_Craniata`, wire the definition and mesh components.
8. Build `BP_Airlock_Craniata`.
9. Validate runtime per section 9 step 6.

---

## 14. Pass criteria

The Craniata integration is a pass only if all of the following hold:

- `export_sub_definition.py` runs from the command line and produces a deterministic JSON.
- `import_sub_definition.py` runs under UnrealEditor-Cmd and produces a saved `DA_SubDef_Craniata.uasset`.
- `BP_Submarine_Craniata` has `GeneratedDefinition` assigned and `GeneratorSpec` empty.
- At runtime, no generator log line fires for the Craniata submarine.
- Flood, doors, stations, crew movement all work using the authored definition.
- `BP_Airlock_Craniata` is a self-contained prefab with two working door actors bound to the airlock connections.
- `main.py`, `core/`, and `data/stations.json` are unchanged on disk.
- The output JSON commits reflect only bridge-driven changes, never hand edits.

The integration is not a pass if only the imported meshes look correct in the viewport, or if runtime systems still depend on generator output.

---

## 15. Follow-up work, out of FP scope

Listed for continuity. Do not start any of these until FP closes.

- Remove `USubmarineGeneratorSpec` and `USubmarineGenerator` from the runtime module.
- Remove or isolate the procedural mesh builder paths unused by Craniata.
- Migrate the authored-definition path from a Python editor utility to a C++ `UFactory` if multiple submarines are planned.
- Capture the current Blender scene edits into `main.py` so re-running the script produces a faithful model again.
