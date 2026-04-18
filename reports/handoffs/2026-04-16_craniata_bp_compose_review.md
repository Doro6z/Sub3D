# Handoff – Craniata BP composition pipeline review

**Date:** 2026-04-16
**Project:** Sub3D (UE 5.7.4 + Blender 5.0)
**For:** ChatGPT review pass
**From:** Claude session that just iterated on this with the user

---

## What we're building

A single `BP_Submarine_Craniata` Blueprint that holds ~141 `StaticMeshComponent`s — one per Blender-authored sub piece (hull, decks, doors, hydroplanes, antennas, periscope, welds, etc.) — placed at the correct relative transforms inside the actor, with the actor's CDO wired to the runtime `DA_SubDef_Craniata` data asset.

The submarine is built procedurally in Blender by `Scripts/Blender/hull_blockout_gpt/main.py` (~3900 lines, calls `core/pivots.py`, `core/materials.py`, `core/bulkheads_doors.py`). Each `SM_*` mesh gets a gameplay-correct origin set by `core/pivots.py`:

- Doors / hatches: hinge at min Y, mid height
- Rudder: hinge line at forward-most X
- Hydroplanes: inner edge (closest to centerline)
- Turrets: yaw pivot at base center (band-slice average)
- Default fallback: bbox center

After Blender we export to UE 5.7 via two scripts and one composer.

---

## Current pipeline (3 scripts)

### 1) Blender per-mesh export

`Scripts/Blender/hull_blockout_gpt/export_per_mesh.py`

Iterates every `SM_*` mesh (filters out `*_Cutter_Manual`, `*_RectRecut`, `*_EnvCut`, `*_EnvelopeCut`) and writes one FBX per mesh under `Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/<obj_name>.fbx`.

Currently uses **NEUTRAL** export settings:

```python
bpy.ops.export_scene.fbx(
    filepath=filepath,
    use_selection=True,
    object_types={"MESH"},
    apply_scale_options="FBX_SCALE_UNITS",  # was FBX_SCALE_ALL
    apply_unit_scale=True,
    axis_forward="-Y",                       # Blender default
    axis_up="Z",                             # Blender default
    bake_space_transform=False,              # was True
    use_mesh_modifiers=True,                 # bakes Solidify
    mesh_smooth_type="FACE",
    use_tspace=True,
    use_triangles=False,
    bake_anim=False,
)
```

### 2) Blender world-transforms JSON export

`Scripts/Blender/hull_blockout_gpt/export_transforms.py`

Dumps every `SM_*` mesh's `obj.matrix_world.decompose()` to `Craniata_Transforms.json` next to the FBX folder. Schema:

```json
{
  "schema_version": 1,
  "object_count": 141,
  "objects": {
    "SM_Hull": {
      "location_cm": [2100.0, 0.0, 0.0],
      "rotation_quat_xyzw": [0, 0, 0, 1],
      "scale": [1.0, 1.0, 1.0]
    },
    ...
  }
}
```

The `location_cm` is the world position of each mesh's pivot (set by `core/pivots.py`).

### 3) UE BP composer

`Scripts/UE5/compose_craniata_bp_v2.py`

Run headlessly via `UnrealEditor-Cmd.exe -ExecutePythonScript=...`.

- Reads the JSON
- Duplicates `BP_Submarine_FPRun` -> `BP_Submarine_Craniata` (overwrites if exists)
- For every entry, looks up `/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/SM_*` and adds a `StaticMeshComponent` to the BP via `SubobjectDataSubsystem`
- Sets each component transform from JSON
- Sets BP CDO: `GeneratedDefinition = DA_SubDef_Craniata`, `GeneratorSpec = None`
- Compiles + saves the BP

#### Coordinate transform (the part to scrutinize)

`ASubmarineBase` has a C++ `USceneComponent SubmarineRoot` as its root component. We add SM components as children of that root, then apply a yaw to `SubmarineRoot` so the bow lands on UE Forward (+X).

Position transform from Blender world coords:

```python
# Submarine center origin: BP root sits at sub geometric center.
sub_local_x = loc_blender[0] - LENGTH_CM * 0.5   # LENGTH_CM = 4200
sub_local_y = loc_blender[1]
sub_local_z = loc_blender[2]

# UE applies its standard FBX import convert on the per-mesh assets:
#   Blender (x, y, z) -> asset/BP-local (-y, x, z)
# Apply the same rule to component positions to keep the relative arrangement.
ue_x = -sub_local_y
ue_y =  sub_local_x
ue_z =  sub_local_z
```

Component scale forced to (1, 1, 1) (defeats any scale baking in Blender's matrix_world).
Component rotation forced to identity (the Blender object rotations are identity for all SM_* in main.py; if a future authoring pass introduces non-identity, we'd need `R_ue = M @ R_blender @ M^-1` with M = the axis convert).

Root yaw on `SubmarineRoot`: **+90°** (so BP-local -Y axis aligns with UE world +X).

Verification math:
- Hull at Blender world (2100, 0, 0) -> sub_local (0, 0, 0) -> BP-local (0, 0, 0)
- Bow vertex at Blender object-local (-2100, 0, 0) -> asset-local (0, -2100, 0) (from the `(x, y, z) -> (-y, x, z)` UE asset convert)
- After root yaw +90°: bow world position rotates from (0, -2100, 0) to (2100, 0, 0). Yes, bow at UE +X.

---

## Background context (history of this iteration)

We tried **three** approaches before landing on the current one:

1. **First attempt: combined FBX + per-asset import (combine_meshes=False).** Worked for the JSON-import side (DA_SubDef_Craniata populated correctly) but the per-asset import dropped node transforms — every asset arrived at world (0,0,0) with vertices in object-local. Composing the BP needs world placements; we wrote `compose_craniata_bp.py` reading the JSON. Most components placed correctly but rudder/fins/propeller floated meters away from the hull.
2. **Second attempt: per-mesh FBX with `bake_space_transform=True` + `axis_forward='-X'`.** Idea: bake a 180° Yaw into vertices so each asset arrives in UE oriented bow-toward +X. In practice the bake apparently produced a 90° Yaw (not 180°) AND introduced non-unit scales (e.g. 1.06954 on Y for `SM_Airlock_Cassette`) due to interaction with the cm scene unit and `apply_scale_options='FBX_SCALE_ALL'`. Doors and hydroplanes (which have non-bbox-center pivots) ended up visibly misplaced.
3. **Third attempt (current): neutral FBX export.** Drop all the axis tricks, apply the standard UE FBX import convert in math on the composer side, force unit scale, rotate the root by +90° to compensate. This is the version we want reviewed.

### Root cause analysis of the v2 (bake_space_transform) failure

When `bake_space_transform=True` is combined with a Blender scene in cm (`scale_length=0.01`) and `apply_scale_options='FBX_SCALE_ALL'`, Blender adds compensating scale factors to the per-object transforms before writing the FBX. Some SM_* objects had their `matrix_world.scale` come out non-unit (e.g. cassette had Y=1.06954). The `compose_v2` script was reading scale from the JSON and re-applying it to the UE component, producing a double-scale visible as oversized parts.

### Open questions for the reviewer (please flag)

- Is the position transform `(x, y, z)_blender -> (-y, x, z)_asset` correct for Blender's neutral FBX export defaults reaching UE 5.7? (Some sources say UE applies a +90° around Z; others say the X/Y swap is different.)
- Is `+90°` the right root yaw for `axis_forward='-Y'` Blender FBX -> UE actor? Should it be `-90°`?
- The user reported "Le sub façait Y forward in UE" with the v2 export. If the neutral export gives a different result, the root yaw value will need adjustment. We are about to test empirically.
- The `core/pivots.py` pass in main.py runs at the very end (after all booleans, solidify modifier baking, and merges). Should it run earlier? Could that explain any object whose `matrix_world.scale` ends up non-identity?
- For doors: the hinge is set at Blender object-local `(x_center, min_y, z_center)`. After our transform chain, the hinge should land at the bulkhead's port edge. Is anything obviously wrong with this reasoning?
- We force component scale to (1,1,1) and rotation to identity. Is this safe? Or should we permute the scale axes via `(sx, sy, sz) -> (sy, sx, sz)` to mirror the position permute when JSON scale is non-unit?

---

## Files to review (in priority order)

1. `Scripts/Blender/hull_blockout_gpt/export_per_mesh.py` — current export settings
2. `Scripts/Blender/hull_blockout_gpt/export_transforms.py` — JSON dump
3. `Scripts/UE5/compose_craniata_bp_v2.py` — the composer (~250 lines)
4. `Scripts/Blender/hull_blockout_gpt/core/pivots.py` — pivot rules
5. `Scripts/Blender/hull_blockout_gpt/main.py` (relevant sections only — it's 3900 lines): `build_airlock_exit_cassette`, `build_hydroplanes`, `_build_sf_hull_welds`, `merge_mesh_objects`, the call to `fix_all_pivots()` at the end of `main()`

Plus the C++ headers we touched on 2026-04-16:
- `Source/Sub3D/Submarine/Generator/SubmarineDefinition.h` (flipped many UPROPERTY from VisibleAnywhere to EditAnywhere so Python can populate via set_editor_property)
- `Source/Sub3D/Submarine/Generator/SubmarineDefinitionTypes.h` (same)
- `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h` (same)

---

## Symptoms still observed in the v2 attempt (before today's neutral re-export)

1. `SM_Airlock_Cassette` mal placée (top of conning tower instead of rear airlock pocket area). Component scale Y = 1.06954.
2. `SM_Hydro_Bow / Stern / Fairings` mal placés.
3. `SM_Hull_Weld_Long_*` orientations inverted (the bead seams running the wrong way around the hull).
4. All `SM_Door_*` decalées (hinge appears offset from where it should sit on the bulkhead).
5. 4 assets missing from `/Game/.../Craniata_PerMesh/`: `SM_BallastTank_Fwd_Stbd`, `SM_BH_Lower_BallastAft`, `SM_BH_Lower_BallastFwd`, `SM_BH_Lower_TechPartition` (likely just not in the user's drag&drop import set).

Some of these may be pure consequences of the scale/axis mess in v2 — the user is about to re-export with the neutral settings and re-run compose; expectation is that 1, 2, 3, 4 should largely resolve. 5 needs a separate manual import.

---

## What we WANT the reviewer to do

1. Sanity-check the math in `_blender_world_to_bp_local` and the `ROOT_YAW_COMPENSATION_DEG = 90.0` in `compose_craniata_bp_v2.py` against a clean reference (UE FBX importer source, Blender FBX exporter source, or empirical test).
2. Flag any remaining axis/scale assumptions that look fragile.
3. Suggest an alternative that doesn't require re-exporting 141 FBX every iteration if you see one (e.g., would `FbxSceneImportFactory` actually work headlessly in UE 5.7 Python? We assumed no based on docs but didn't deeply test).
4. If the math is wrong, give us the corrected formula in this exact form so we can drop it in.

Thanks.
