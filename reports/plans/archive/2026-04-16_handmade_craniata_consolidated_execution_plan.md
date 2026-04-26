# Sub3D - Handmade Craniata Consolidated Execution Plan

Date: 2026-04-16
Scope: execute the handmade Craniata submarine path to a playable First Playable loop.

Primary architecture reference:
- `C:\Dev\Sub3D\reports\plans\2026-04-16_handmade_submarine_architecture.md`

Compared document:
- `C:\Dev\Sub3D\reports\plans\2026-04-16_spec_bridge_replaces_generator.md`

Upstream gameplay scope:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Workspace: `C:\Dev\Sub3D`

---

## 0. Goal

Close the Craniata First Playable loop with the handmade submarine path:

- crew can move inside the submarine
- stations are accessible
- interior flooding works
- hull damage creates breaches
- breaches can be repaired
- the SAS works as an isolated compartment with two doors
- the visible submarine comes from Blender meshes, not procedural hull generation

---

## 1. Consolidated executive decision

For Craniata:

1. Do not use the generator as the main authoring path.
2. Do not use the procedural mesh builder as the visible hull path.
3. Use imported Blender meshes for visible geometry.
4. Use `USubmarineDefinition` for all runtime topology and flood logic.
5. Author stations, hardpoints, and turrets in Unreal Editor.
6. Spawn door actors from definition connections.
7. Keep the SAS as an explicit compartment in the definition.
8. Build flooding from explicit proxy data, not from the raw render mesh.
9. Close the manual UE path first. Add Blender helpers only after the runtime path is working.

This is the execution order.

---

## 2. Comparison with `2026-04-16_spec_bridge_replaces_generator.md`

### 2.1 Keep

These parts remain correct and are kept:

1. `GeneratedDefinition` should be assigned directly for Craniata.
2. The generator should stay dormant for Craniata.
3. The procedural mesh builder should stay out of the visible hull path for Craniata.
4. The runtime systems already consume `USubmarineDefinition` and do not care who produced it.
5. The imported Blender meshes should be used as visible geometry.

### 2.2 Reject

These parts are rejected as the main path:

1. `main.py` and `stations.json` as the authoritative current submarine spec.
2. AST parsing of old Blender scripts as the required first step.
3. a separate `BP_Airlock_Craniata` as the default architecture.
4. the assumption that standard imported mesh collision or nav alone is enough for crew traversal.
5. the assumption that the bridge must be the only writer of the definition before First Playable runtime is closed.

Reason:
The current handmade submarine truth lives in the edited Blender model and in the runtime requirements, not in the older generator-oriented script source alone.

### 2.3 Add

These missing pieces must be added to the plan:

1. explicit walkable surface authoring for Craniata
2. explicit structural sheet authoring for hull damage and repair
3. a direct UE authoring path for `USubmarineDefinition` during the First Playable push
4. exterior door support distinct from interior door visuals
5. a later helper pipeline from Blender proxies to runtime data, after the manual path works

---

## 3. Non-negotiable rules

### 3.1 Runtime truth

`USubmarineDefinition` is the runtime truth.
All flood, door, compartment, and spawn logic must read from it.

### 3.2 Visible truth

Visible hull and interior geometry come from imported Blender meshes attached to the submarine Blueprint.

### 3.3 SAS truth

The SAS is part of the visible Blender geometry and also a distinct gameplay compartment.
It is not a separate generator product.

### 3.4 Station truth

Stations and hardpoints are authored in UE for Craniata.
They are not automatically inferred from imported meshes.

### 3.5 Flood truth

Flooding must use explicit compartment and connection data.
It must not attempt to infer runtime topology from render triangles.

---

## 4. Required runtime path for Craniata

This is the target runtime composition.

### 4.1 `BP_Submarine_Craniata`

`BP_Submarine_Craniata` must contain:

- imported visible hull mesh components
- imported visible interior mesh components
- imported visible SAS geometry
- manually authored walkable proxy components
- manually authored collision setup for the hull
- manually placed station actors or attached station actors
- assigned `GeneratedDefinition = DA_SubDef_Craniata`
- no active `GeneratorSpec` for the Craniata runtime path

### 4.2 `DA_SubDef_Craniata`

The definition must contain at minimum:

- compartments including the SAS compartment
- connections including interior doors and the exterior hatch
- flood graph matching those compartments and connections
- spawn points
- optional station slots

### 4.3 Door actors

Door actors must spawn from `GeneratedDefinition->Connections`.

Required classes:

- interior submarine door actor class
- exterior hatch door actor class or visual variant

### 4.4 Hull damage

Hull damage must read explicit structural sheet data.
Do not rely on fallback bounds-based sheets as the intended Craniata path.

---

## 5. First Playable execution order

Execute in this order.

### Step 1 - Freeze the Craniata runtime path

Required state:

- `BP_Submarine_Craniata` uses `GeneratedDefinition`
- `GeneratorSpec` is not part of the active Craniata setup
- visible geometry comes from imported Blender meshes
- procedural generated geometry is not the visible path

Validation:

- the submarine appears visually from static meshes only
- the game does not rely on `RebuildFromSpec` for Craniata runtime

### Step 2 - Create the manual walkable path

This is the first technical blocker.

Required work:

- provide explicit walkable components for Craniata
- update the submarine path so crew floor queries can read those components

Accepted implementation:

- dedicated static mesh proxies
- dedicated box proxies
- imported floor proxy meshes with the correct collision profile

Required validation:

- `GetInteriorWalkableComponents()` returns valid components for Craniata without `GeneratedGeometry`
- crew can embark, floor-snap, and traverse the imported submarine interior

### Step 3 - Author the runtime definition

Create and fill `DA_SubDef_Craniata` with:

- main compartments
- SAS compartment
- interior connections
- one `ExteriorHatch` connection for the outer SAS door
- flood graph edges
- spawn points

Required rule:

The First Playable push may author this definition directly in UE if that is faster.
A helper importer is not a prerequisite for closing the runtime loop.

### Step 4 - Spawn doors from connections

Required work:

- interior and exterior doors must spawn from definition connections
- each door must propagate closed/open state to `SubFlood`

Required validation:

- opening and closing the interior SAS door updates flood closure state
- opening and closing the exterior SAS door updates flood closure state
- both doors have stable connection ids

### Step 5 - Close the SAS gameplay path

Required state:

- SAS geometry visible from Blender import
- SAS compartment exists in the definition
- inner door is an interior closure
- outer door is an exterior closure

Required validation:

- the SAS can flood independently of the main sub interior
- the two doors behave as two separate closures

### Step 6 - Place stations and hardpoints in UE

Required First Playable set:

- `HelmStation`
- `EngineStation`
- `BallastStation` placeholders
- `TurretStation` actors for the two accessible turret stations
- one manually controlled FPS-only turret path

Required validation:

- stations are discovered as attached stations
- no generator station placement is required for Craniata

### Step 7 - Author structural sheets for hull damage

This is the second technical blocker.

Required work:

- define structural sheet coverage for the handmade hull
- connect that data to `USubHullComponent`

Accepted First Playable quality:

- coarse but explicit sheets covering the main hull bands
- enough precision to create breaches in meaningful zones

Required validation:

- weapon or debug damage creates breach clusters
- breach clusters map to the correct compartment inflow
- repair can clear those breaches

### Step 8 - Close the repair loop

Required work:

- expose a direct repair action at or near breaches
- keep the repair loop simple

Required validation:

- a breach can be created
- water enters the correct compartment
- the player can repair it
- inflow stops after repair

### Step 9 - Add debug and validation helpers

Required debug helpers:

- compartment volume debug draw
- connection and door anchor debug draw
- walkable proxy debug draw
- structural sheet debug draw
- breach debug creation
- flood level debug commands

Reason:
This submarine is handmade. Tuning must be observable.

### Step 10 - Only after runtime closes, add Blender helpers

Only after steps 1 through 9 work in PIE:

- add Blender proxy export helpers
- add JSON export if it removes repeated manual setup
- add UE import helpers if they remove repeated manual setup

These helpers are optimization work.
They are not the critical path to First Playable runtime closure.

---

## 6. Recommended authoring split for First Playable

### 6.1 In Blender

Author or keep:

- visible hull
- visible interior
- visible SAS geometry
- optional proxy objects for compartments, doors, floors, and structural sheets

### 6.2 In Unreal Editor

Author:

- `BP_Submarine_Craniata`
- `DA_SubDef_Craniata`
- walkable proxies
- station placement
- turret hardpoints
- structural sheet proxies or bindings

### 6.3 In runtime code

Support:

- door spawn from definition
- manual walkable component discovery
- structural sheet initialization for the handmade submarine
- debug visualization and cheats

---

## 7. Required technical corrections implied by this plan

These are the code and asset changes this plan implies.

### 7.1 Manual walkable discovery

Current state:
`ASubmarineBase::GetInteriorWalkableComponents()` only exposes generated floor collision.

Required change:
Expose and use manual walkable components for Craniata.

### 7.2 Manual hull sheet initialization

Current state:
`USubHullComponent` uses layout or fallback sheet logic.

Required change:
Provide an explicit handmade hull sheet path for Craniata.

### 7.3 Exterior door visual/runtime path

Current state:
all door-like connections use the same door actor class path.

Required change:
Support the Craniata outer hatch path without treating the SAS as a separate prefab architecture.

### 7.4 Definition-first runtime validation

Current state:
parts of the project still retain legacy fallback language and generator-era assumptions.

Required change:
Validate the Craniata runtime through the direct definition path.

---

## 8. Do-not-do list

Do not do any of the following before the playable loop is closed:

1. Do not rebuild the Craniata path around the generator.
2. Do not make the AST bridge the first mandatory milestone.
3. Do not derive flooding directly from the raw visible mesh triangles.
4. Do not block runtime progress on a perfect Blender import pipeline.
5. Do not create a separate airlock actor architecture unless the code path proves it is required.
6. Do not assume navmesh alone solves interior traversal for the crew.
7. Do not keep fallback bounds-based hull sheets as the intended Craniata solution.

---

## 9. Pass criteria

The Craniata First Playable path is valid only if all of these are true:

- Craniata uses a directly assigned `GeneratedDefinition`
- visible geometry comes from imported Blender meshes
- crew can walk the handmade interior without generated floor geometry
- stations are accessible in the handmade submarine
- the SAS is a distinct compartment
- the inner and outer SAS doors are separate runtime closures
- flooding works across the definition graph
- hull damage creates breaches
- breaches can be repaired

---

## 10. Immediate execution guidance

Start with these three tasks and do them in this order:

1. close the manual walkable path
2. close the direct `DA_SubDef_Craniata` authoring path with SAS compartment and connections
3. close the handmade structural sheet path for breaches and repair

Do not start with the Blender AST bridge.
Do not start with generator cleanup.
Do not start with a separate airlock actor implementation.

---

## 11. Practical conclusion

The consolidated plan is:

- use the handmade submarine directly
- keep Blender as spatial truth
- keep `USubmarineDefinition` as runtime truth
- author gameplay-critical proxies explicitly
- close the playable runtime path in UE first
- add helper automation only after the path works

This is the shortest path to a playable Craniata submarine.
