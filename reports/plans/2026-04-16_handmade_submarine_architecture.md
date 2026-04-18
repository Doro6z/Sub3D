# Sub3D - Handmade Submarine Architecture

Date: 2026-04-16
Scope: define the runtime and authoring architecture for the handmade Craniata submarine.

Related documents:
- `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`
- `C:\Dev\Sub3D\reports\plans\2026-04-16_spec_bridge_replaces_generator.md`

Workspace: `C:\Dev\Sub3D`

---

## 0. Executive decision

For the Craniata submarine:

1. Blender is the spatial source of truth.
2. `USubmarineDefinition` is the runtime source of truth.
3. The submarine generator is secondary and legacy for this submarine.
4. Stations, hardpoints, and turrets are authored in Unreal Editor.
5. Door actors are spawned from `GeneratedDefinition->Connections`.
6. The airlock remains an explicit compartment in the definition.
7. The visible hull and interior come from imported Blender meshes.
8. Flooding is derived from Blender gameplay proxies or UE-authored proxies, not from the raw render mesh.

This is the target architecture.
It replaces the generator-centered interpretation for the handmade submarine.

---

## 1. Code-verified facts

These facts are verified in the current repository.

### 1.1 Runtime already accepts a pre-authored definition

`ASubmarineBase::BeginPlay()` only calls the generator when:

- `GeneratedDefinition == null`
- and `GeneratorSpec != null`

If `GeneratedDefinition` is assigned directly, the generator is skipped.

### 1.2 Runtime systems already consume the definition, not the generator

The following systems read the definition structs and do not care who produced them:

- `USubFloodComponent`
- `UFloodWaterVisualsComponent`
- door spawn in `ASubmarineBase`
- compartment resolution in `ASubCrewCharacter`
- spawn point lookup in `ASubmarineBase`

### 1.3 Stations already support a manual UE path

`USubmarineStationManagerComponent::DiscoverAttachedStations()` runs before spawn-from-definition.
If stations are already attached to the submarine actor, the runtime can use them directly.

### 1.4 Current crew walking still depends on generated walkable collision

`ASubmarineBase::GetInteriorWalkableComponents()` currently returns walkable floor collision from `USubmarineGeneratedGeometryComponent`.

This means the project is not fully decoupled from generated geometry yet.
A manual walkable path is required for the handmade submarine.

### 1.5 Current hull breach logic does not read the visible hull mesh directly

`USubHullComponent` works from `FStructuralSheetDef` data or fallback sheets.
The visible Blender hull mesh is not currently the breach truth by itself.

---

## 2. Architecture boundaries

This submarine needs three different truths. They must stay separate.

### 2.1 Blender truth

Blender owns:

- visible hull shape
- visible interior shell
- visible bulkheads
- visible airlock geometry
- spatial anchors and gameplay proxy geometry when provided

Blender is the spatial source of truth.
It is not the runtime state container.

### 2.2 Runtime truth

`USubmarineDefinition` owns:

- compartments
- compartment ids
- flood graph
- connections
- door ids
- spawn points
- optional station slots

`USubmarineDefinition` is the runtime source of truth.
All gameplay systems read stable structs from this asset.

### 2.3 UE authoring truth

Unreal Editor owns:

- station actors
- turret hardpoints
- turret actors
- interaction actors
- manual tuning of walkable proxies
- manual tuning of structural sheet proxies
- final Blueprint composition of the submarine actor

UE is the final gameplay authoring surface.

---

## 3. What Blender should drive

Blender should drive:

- hull silhouette
- exterior dimensions
- bulkhead locations
- airlock location and shape
- door anchor locations
- deck and floor extents
- optional gameplay proxy volumes

Blender should not directly drive at runtime:

- flood state
- replicated door state
- active station occupancy
- repair state
- breach state
- gameplay interaction state

Reason:
The runtime needs stable ids, stable structs, replication-safe state, and explicit validation.
A raw imported render mesh does not provide that.

---

## 4. What `USubmarineDefinition` must contain for the handmade submarine

For Craniata, the definition must explicitly contain:

1. Compartments
2. Connections
3. Flood graph
4. Spawn points
5. Optional station slots

The airlock must exist as:

- one compartment with semantic type `Airlock`
- one interior connection to the rest of the submarine
- one exterior connection of type `ExteriorHatch`

This is required even if the full SAS geometry is already modeled in Blender.

Reason:
The gameplay loop needs the SAS to flood independently from the main interior.
The visual mesh alone cannot express that logic.

---

## 5. Visible geometry path

For Craniata:

- the visible hull is an imported static mesh from Blender
- the visible interior is imported static mesh content from Blender
- the visible SAS geometry is imported static mesh content from Blender
- the procedural mesh builder is not the visual path for this submarine

This does not prohibit keeping the generator code in the repository.
It only removes it from the active Craniata runtime path.

---

## 6. Flooding path

Flooding must not be derived from the raw render mesh triangles.
It must be derived from explicit gameplay proxies.

Accepted sources for those proxies:

1. Blender proxy objects exported into helper data
2. UE-authored proxy components and volumes
3. a mix of both, with UE as the final validation surface

Required proxy data:

- compartment volumes
- connection anchors
- exterior hatch anchor
- walkable floor references
- structural hull sheet references for breaches

The flooding runtime continues to read `USubmarineDefinition`.
The proxies exist to author and validate that definition.

---

## 7. Walkable surfaces

This is a critical architecture point.

The current project still assumes generated floor collision for crew traversal.
That is not acceptable as the long-term path for the handmade submarine.

Craniata needs an explicit manual walkable path.

Accepted implementation paths:

1. static mesh floor collision components with the correct walkable collision profile
2. explicit box or mesh proxy components attached to the submarine Blueprint
3. imported floor proxy meshes from Blender with a dedicated collision profile

Required rule:
`ASubmarineBase::GetInteriorWalkableComponents()` must support the manual Craniata path.
It must not depend only on `GeneratedGeometry`.

---

## 8. Hull damage and breaches

The visible Blender hull mesh is not enough by itself for runtime breach logic.

For gameplay, the submarine needs explicit structural sheet data.
That data can come from:

1. Blender hull breach proxies
2. UE-authored structural sheet proxies
3. temporary coarse sheets for First Playable, then refined later

Required rule:
The breach system reads explicit structural sheet data.
It does not read triangle topology from the visible hull mesh at runtime.

This keeps hull damage deterministic and repairable.

---

## 9. Doors

Door actors remain runtime actors.
They are not baked into the Blender mesh as the gameplay truth.

Required rule:

- interior and exterior door actors spawn from `GeneratedDefinition->Connections`
- each connection has a stable id
- the outer SAS door uses `ExteriorHatch`
- the interior SAS door uses a normal interior connection type

The Blender mesh provides the visible frame and opening.
The runtime actor provides the state, replication, interaction, and flood closure behavior.

---

## 10. Stations, hardpoints, and turrets

For First Playable, these are authored in UE.

That includes:

- `HelmStation`
- `EngineStation`
- `BallastStation`
- `TurretStation`
- turret hardpoint transforms
- the manually controlled FPS-only turret path

The current runtime already supports this direction.
Attached stations are discovered before spawn-from-definition is used.

The definition may still carry station slot data later, but that is not required for the Craniata FP path.

---

## 11. Role of helper scripts

Helper scripts are valid and useful.
They are not the architecture center.

Helper scripts may:

- extract Blender proxy data
- emit JSON for compartment and connection setup
- validate dimensions and ids
- generate debug overlays or reports

Helper scripts must not replace explicit runtime data validation.
The final runtime contract remains `USubmarineDefinition`.

---

## 12. Recommended authoring path for First Playable

For First Playable, use this path:

1. Import the visible Blender meshes into UE.
2. Build `BP_Submarine_Craniata` from those imported meshes.
3. Create or fill `DA_SubDef_Craniata` as the runtime definition asset.
4. Author stations, hardpoints, and turret actors in UE.
5. Author walkable proxies in UE or import dedicated walkable proxies from Blender.
6. Author structural sheet proxies in UE or import them from Blender.
7. Spawn door actors from definition connections.
8. Validate flooding, breaches, and repair in PIE.

This path is compatible with adding Blender export/import helpers later.
It does not require the generator.

---

## 13. What this architecture rejects

This architecture rejects the following as the main Craniata path:

1. generator-driven hull authoring
2. generator-driven airlock generation
3. procedural mesh as the visible hull path
4. flooding derived from raw render geometry
5. station layout derived automatically from the mesh import
6. treating the Blender mesh alone as sufficient gameplay truth

---

## 14. First Playable priority

The First Playable goal is not a perfect import pipeline.
The First Playable goal is a playable loop:

- interior flooding
- hull damage
- hull breach creation
- repair of breaches
- crew traversal
- station access
- turret access
- SAS isolation through two doors

Any tooling decision that delays this loop is wrong for the current phase.

---

## 15. Practical conclusion

The correct Craniata architecture is:

- Blender for spatial truth
- `USubmarineDefinition` for runtime truth
- UE for gameplay authoring and tuning
- generator as secondary legacy code

The immediate technical consequence is clear:

The project must close the manual Craniata path for:

- walkable surfaces
- structural hull sheets
- doors from connections
- SAS compartment definition
- manual station and hardpoint authoring

That is the shortest path to a playable handmade submarine.
