"""
Sub3D - Compose BP_Submarine_Craniata v2 (per-mesh assets + axis mirror)
===========================================================================

Compose the handmade Craniata submarine Blueprint from one imported StaticMesh
asset per Blender-authored SM_* object.

Pipeline:
- `export_per_mesh.py` writes one neutral FBX per SM_* mesh into
  `Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/`
  using `axis_forward='-Y'`, `axis_up='Z'`, `bake_space_transform=False`.
- `export_transforms.py` writes `Craniata_Transforms.json` containing each
  object's Blender world transform at export time.
- This script duplicates `BP_Submarine_FPRun`, adds one StaticMeshComponent per
  imported per-mesh asset, applies the matching BP-local transform, assigns
  `DA_SubDef_Craniata`, applies optional material overrides, and saves the
  result as `BP_Submarine_Craniata`.

Coordinate contract:
- Blender world positions are centered onto the submarine geometric center by
  subtracting `LENGTH_CM / 2` from X.
- Interchange per-mesh import keeps each static mesh around its authored pivot
  and already aligns the local mesh orientation for the handmade Craniata path.
- Component placement therefore maps centered Blender positions directly into
  BP-local UE space with the bow on +X:
      Blender centered (x, y, z) -> BP-local (-x, -y, z)
- With actor yaw = 0, the bow lands on BP-local +X.

The compose path intentionally ignores JSON scale and JSON rotation by default:
- JSON scale is exported for diagnostics only. Some handmade Blender edits still
  leave a few meshes with non-unit `matrix_world.scale`; reapplying that on the
  UE component would reintroduce the double-scale class of bugs.
- JSON rotation is currently ignored by this path. A global local-space 180 deg
  correction is applied on each component pivot for the current Craniata pass.

Material override contract:
- Per-family material overrides are resolved from explicit UE asset paths.
- If a configured material asset does not exist, the mesh keeps its imported
  default material assignment and the missing asset is logged.
- This keeps the compose path deterministic without re-enabling FBX material
  import or inventing runtime material generation.

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/compose_craniata_bp_v2.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4
"""

import json
import math
from pathlib import Path

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(r"C:/Dev/Sub3D")
TRANSFORMS_JSON = REPO_ROOT / "Content" / "Sub3D" / "FirstPlayableRun" / "Meshes" / "Blockout" / "Craniata_Transforms.json"

SM_FOLDER = "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh"
FPRUN_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_FPRun"
CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"
DEF_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata"
MATERIAL_FOLDER = "/Game/Sub3D/FirstPlayableRun/Materials/Craniata"

# Submarine length in cm (Blender X spans 0..LENGTH_CM); BP root sits at the
# geometric center, i.e. Blender X = LENGTH_CM / 2.
LENGTH_CM = 4200.0

# Direct position mapping for the Interchange per-mesh import path.
APPLY_FBX_AXIS_MIRROR = False
ROOT_YAW_COMPENSATION_DEG = 0.0
GLOBAL_COMPONENT_YAW_DEG = 180.0
FORCE_UNIT_SCALE = True
ANOMALY_TOLERANCE = 1e-4
MAX_LOGGED_ANOMALIES = 16
MAX_LOGGED_MISSING_MATERIALS = 12
ENABLE_COMPONENT_COLLISION = True
COMPONENT_COLLISION_PROFILE = "BlockAll"

# Material override assets expected for the handmade Craniata path.
# Create these assets in the editor and re-run the compose script.
MATERIAL_PATHS = {
    "hull":        f"{MATERIAL_FOLDER}/MI_Craniata_Hull.MI_Craniata_Hull",
    "deck":        f"{MATERIAL_FOLDER}/MI_Craniata_Deck.MI_Craniata_Deck",
    "deck_upper":  f"{MATERIAL_FOLDER}/MI_Craniata_DeckUpper.MI_Craniata_DeckUpper",
    "bulkhead":    f"{MATERIAL_FOLDER}/MI_Craniata_Bulkhead.MI_Craniata_Bulkhead",
    "bulkhead_lwr":f"{MATERIAL_FOLDER}/MI_Craniata_BulkheadLower.MI_Craniata_BulkheadLower",
    "door":        f"{MATERIAL_FOLDER}/MI_Craniata_Door.MI_Craniata_Door",
    "door_frame":  f"{MATERIAL_FOLDER}/MI_Craniata_DoorFrame.MI_Craniata_DoorFrame",
    "hatch":       f"{MATERIAL_FOLDER}/MI_Craniata_Hatch.MI_Craniata_Hatch",
    "fin":         f"{MATERIAL_FOLDER}/MI_Craniata_Fin.MI_Craniata_Fin",
    "propulsor":   f"{MATERIAL_FOLDER}/MI_Craniata_Propulsor.MI_Craniata_Propulsor",
    "duct":        f"{MATERIAL_FOLDER}/MI_Craniata_Duct.MI_Craniata_Duct",
    "pipe_water":  f"{MATERIAL_FOLDER}/MI_Craniata_PipeWater.MI_Craniata_PipeWater",
    "pipe_hyd":    f"{MATERIAL_FOLDER}/MI_Craniata_PipeHyd.MI_Craniata_PipeHyd",
    "pipe_reactor":f"{MATERIAL_FOLDER}/MI_Craniata_PipeReactor.MI_Craniata_PipeReactor",
    "pipe":        f"{MATERIAL_FOLDER}/MI_Craniata_Pipe.MI_Craniata_Pipe",
    "valve":       f"{MATERIAL_FOLDER}/MI_Craniata_Valve.MI_Craniata_Valve",
    "junction":    f"{MATERIAL_FOLDER}/MI_Craniata_Junction.MI_Craniata_Junction",
    "periscope":   f"{MATERIAL_FOLDER}/MI_Craniata_Periscope.MI_Craniata_Periscope",
    "antenna":     f"{MATERIAL_FOLDER}/MI_Craniata_Antenna.MI_Craniata_Antenna",
    "flag":        f"{MATERIAL_FOLDER}/MI_Craniata_Flag.MI_Craniata_Flag",
    "catwalk":     f"{MATERIAL_FOLDER}/MI_Craniata_Catwalk.MI_Craniata_Catwalk",
    "ladder":      f"{MATERIAL_FOLDER}/MI_Craniata_Ladder.MI_Craniata_Ladder",
    "turret":      f"{MATERIAL_FOLDER}/MI_Craniata_Turret.MI_Craniata_Turret",
    "mount":       f"{MATERIAL_FOLDER}/MI_Craniata_Mount.MI_Craniata_Mount",
    "engine":      f"{MATERIAL_FOLDER}/MI_Craniata_Engine.MI_Craniata_Engine",
    "reactor":     f"{MATERIAL_FOLDER}/MI_Craniata_Reactor.MI_Craniata_Reactor",
    "storage":     f"{MATERIAL_FOLDER}/MI_Craniata_Storage.MI_Craniata_Storage",
    "seal":        f"{MATERIAL_FOLDER}/MI_Craniata_Seal.MI_Craniata_Seal",
    "ui":          f"{MATERIAL_FOLDER}/MI_Craniata_UI.MI_Craniata_UI",
    "default":     f"{MATERIAL_FOLDER}/MI_Craniata_Default.MI_Craniata_Default",
}

# Ordered by specificity: longest prefix first wins.
MATERIAL_PATTERNS = {
    "SM_Hull_Weld_":             "hull",
    "SM_HardpointFairing_":      "fin",
    "SM_SAS_HullCutter":         "default",
    "SM_Hull":                   "hull",
    "SM_Pipe_Water":             "pipe_water",
    "SM_Pipe_Hyd":               "pipe_hyd",
    "SM_Pipe_Reactor":           "pipe_reactor",
    "SM_Pipe_":                  "pipe",
    "SM_Deck_engine_upper":      "deck",
    "SM_Deck_engine_lower":      "deck",
    "SM_Deck_lower_main":        "deck",
    "SM_Deck_upper":             "deck_upper",
    "SM_Deck_main":              "deck",
    "SM_BH_Lower":               "bulkhead_lwr",
    "SM_BH_Main":                "bulkhead",
    "SM_BH_Upper":               "bulkhead",
    "SM_Airlock_Seal_Center":    "seal",
    "SM_Airlock_Door":           "door",
    "SM_Airlock_Cassette":       "door_frame",
    "SM_Door_":                  "door",
    "SM_HatchDoor_":             "hatch",
    "SM_Hatch_":                 "hatch",
    "SM_Battant_":               "door",
    "SM_FinAssembly":            "fin",
    "SM_Fin_":                   "fin",
    "SM_Hydro_":                 "fin",
    "SM_RudderAssembly":         "fin",
    "SM_Rudder":                 "fin",
    "SM_Skeg":                   "fin",
    "SM_Propeller":              "propulsor",
    "SM_Propulsor_Duct":         "duct",
    "SM_Duct":                   "duct",
    "SM_Valve":                  "valve",
    "SM_Junction":               "junction",
    "SM_Periscope":              "periscope",
    "SM_Flag_":                  "flag",
    "SM_Antenna":                "antenna",
    "SM_Ladder_":                "ladder",
    "SM_Stair_":                 "ladder",
    "SM_StairRail_":             "ladder",
    "SM_StairRailPost_":         "ladder",
    "SM_StairSupport_":          "ladder",
    "SM_StairStringer_":         "ladder",
    "SM_StairHanger_":           "ladder",
    "SM_Ballast":                "mount",
    "SM_Catwalk":                "catwalk",
    "SM_TurretStation_":         "mount",
    "SM_Turret_":                "turret",
    "SM_Hardpoint_":             "mount",
    "SM_TurretSocket_":          "mount",
    "SM_HelmDisplay":            "ui",
    "SM_HelmConsole":            "mount",
    "SM_CrewLocker_":            "storage",
    "SM_CrewProp_":              "storage",
    "SM_UpperStorage":           "storage",
    "SM_Engine":                 "engine",
    "SM_Reactor":                "reactor",
}


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):
    unreal.log(f"[ComposeCraniataV2] {msg}")


def warn(msg):
    unreal.log_warning(f"[ComposeCraniataV2] {msg}")


def err(msg):
    unreal.log_error(f"[ComposeCraniataV2] {msg}")


# ---------------------------------------------------------------------------
# Coordinate transform
# ---------------------------------------------------------------------------

def _blender_world_to_bp_local(loc_blender):
    """Translate Blender world coordinates to BP-local UE coordinates.

    Two adjustments:
      1. Center origin: subtract LENGTH/2 from Blender X so the Blueprint root
         sits at the submarine geometric center.
      2. Map the centered handmade Craniata coordinates so the bow points to
         UE +X and lateral offset stays stable in Blueprint local space.
    """
    sub_local_x = float(loc_blender[0]) - LENGTH_CM * 0.5
    sub_local_y = float(loc_blender[1])
    sub_local_z = float(loc_blender[2])

    if APPLY_FBX_AXIS_MIRROR:
        ue_x = -sub_local_y
        ue_y = sub_local_x
        ue_z = sub_local_z
    else:
        ue_x = -sub_local_x
        ue_y = -sub_local_y
        ue_z = sub_local_z

    return ue_x, ue_y, ue_z


def _blender_quat_to_ue_quat(_quat_xyzw):
    """Return the component-space rotation to apply in UE.

    For the current handmade Craniata path, the imported per-mesh assets need a
    uniform local-space 180 deg yaw correction around their authored pivots.
    JSON-authored object rotations remain ignored for this pass.
    """
    if abs(GLOBAL_COMPONENT_YAW_DEG) < 1e-3:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)

    half_radians = 0.5 * GLOBAL_COMPONENT_YAW_DEG * math.pi / 180.0
    return unreal.Quat(0.0, 0.0, float(math.sin(half_radians)), float(math.cos(half_radians)))


# ---------------------------------------------------------------------------
# Material classification
# ---------------------------------------------------------------------------

def _classify_material_key(obj_name):
    for pattern, key in sorted(MATERIAL_PATTERNS.items(), key=lambda item: -len(item[0])):
        if obj_name.startswith(pattern):
            return key
    return "default"


def _load_material_override(material_key, material_cache, missing_material_paths):
    material_path = MATERIAL_PATHS.get(material_key)
    if not material_path:
        return None

    if material_path in material_cache:
        return material_cache[material_path]

    material_asset = unreal.EditorAssetLibrary.load_asset(material_path)
    if material_asset is None:
        missing_material_paths.add(material_path)
        return None

    material_cache[material_path] = material_asset
    return material_asset


def _apply_material_override(comp, sm_asset, obj_name, material_cache, missing_material_paths):
    material_key = _classify_material_key(obj_name)
    material_asset = _load_material_override(material_key, material_cache, missing_material_paths)
    if material_asset is None:
        return False

    static_materials = sm_asset.get_editor_property("static_materials")
    slot_count = max(len(static_materials), 1)
    for slot_index in range(slot_count):
        comp.set_material(slot_index, material_asset)
    return True


# ---------------------------------------------------------------------------
# Transform diagnostics
# ---------------------------------------------------------------------------

def _collect_transform_anomalies(transforms_data):
    non_unit_scale = []
    non_identity_rotation = []

    for obj_name, t in transforms_data.get("objects", {}).items():
        sx, sy, sz = [float(v) for v in t.get("scale", (1.0, 1.0, 1.0))]
        if (
            abs(sx - 1.0) > ANOMALY_TOLERANCE
            or abs(sy - 1.0) > ANOMALY_TOLERANCE
            or abs(sz - 1.0) > ANOMALY_TOLERANCE
        ):
            non_unit_scale.append((obj_name, sx, sy, sz))

        qx, qy, qz, qw = [float(v) for v in t.get("rotation_quat_xyzw", (0.0, 0.0, 0.0, 1.0))]
        if (
            abs(qx) > ANOMALY_TOLERANCE
            or abs(qy) > ANOMALY_TOLERANCE
            or abs(qz) > ANOMALY_TOLERANCE
            or abs(qw - 1.0) > ANOMALY_TOLERANCE
        ):
            non_identity_rotation.append((obj_name, qx, qy, qz, qw))

    return non_unit_scale, non_identity_rotation


def _log_transform_anomalies(transforms_data):
    non_unit_scale, non_identity_rotation = _collect_transform_anomalies(transforms_data)

    if non_unit_scale:
        warn(
            f"JSON contains {len(non_unit_scale)} non-unit scales. "
            f"FORCE_UNIT_SCALE={FORCE_UNIT_SCALE}."
        )
        for row in non_unit_scale[:MAX_LOGGED_ANOMALIES]:
            warn(
                "  non-unit scale: "
                f"{row[0]} -> ({row[1]:.6f}, {row[2]:.6f}, {row[3]:.6f})"
            )
        if len(non_unit_scale) > MAX_LOGGED_ANOMALIES:
            warn(f"  ... and {len(non_unit_scale) - MAX_LOGGED_ANOMALIES} more")
    else:
        log("JSON scales are unit-length for all objects")

    if non_identity_rotation:
        warn(f"JSON contains {len(non_identity_rotation)} non-identity object rotations")
        for row in non_identity_rotation[:MAX_LOGGED_ANOMALIES]:
            warn(
                "  non-identity rotation: "
                f"{row[0]} -> quat({row[1]:.6f}, {row[2]:.6f}, {row[3]:.6f}, {row[4]:.6f})"
            )
        if len(non_identity_rotation) > MAX_LOGGED_ANOMALIES:
            warn(f"  ... and {len(non_identity_rotation) - MAX_LOGGED_ANOMALIES} more")
    else:
        log("JSON rotations are identity for all objects")


def _validate_imported_asset_space():
    """Detect per-mesh assets imported with baked world transforms.

    The neutral Craniata per-mesh path expects SM_Hull to be imported around
    its authored local pivot. If the imported static mesh bounds origin is far
    from zero, UE baked scene transforms into the mesh vertices and BP
    recomposition will apply placement twice.
    """
    hull_asset = unreal.EditorAssetLibrary.load_asset(f"{SM_FOLDER}/SM_Hull.SM_Hull")
    if hull_asset is None:
        raise RuntimeError(f"Required mesh asset not found: {SM_FOLDER}/SM_Hull.SM_Hull")

    bounds = hull_asset.get_bounds()
    origin = bounds.origin
    threshold = 10.0
    if abs(origin.x) > threshold or abs(origin.y) > threshold or abs(origin.z) > threshold:
        raise RuntimeError(
            "Imported per-mesh assets are not in local pivot space. "
            f"SM_Hull bounds origin is ({origin.x:.3f}, {origin.y:.3f}, {origin.z:.3f}). "
            "Re-import with Scripts/UE5/import_craniata_per_meshes.py before composing."
        )


# ---------------------------------------------------------------------------
# Subobject helpers
# ---------------------------------------------------------------------------

def _get_subobject_data_subsystem():
    try:
        return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        getter = getattr(unreal.SubobjectDataSubsystem, "get", None)
        if callable(getter):
            return getter()
        raise


def _root_handle(sds, blueprint):
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        raise RuntimeError("k2_gather_subobject_data_for_blueprint returned no handles")
    return handles[0]


def _sanitize_component_name(raw_name: str) -> str:
    return raw_name.replace(".", "_")


# ---------------------------------------------------------------------------
# Step 1: Load + duplicate
# ---------------------------------------------------------------------------

def _ensure_craniata_bp():
    eal = unreal.EditorAssetLibrary

    if eal.does_asset_exist(CRANIATA_BP_PATH):
        warn(f"{CRANIATA_BP_PATH} already exists - deleting and recreating")
        eal.delete_asset(CRANIATA_BP_PATH)

    if not eal.does_asset_exist(FPRUN_BP_PATH):
        raise RuntimeError(f"Source BP not found: {FPRUN_BP_PATH}")

    duplicated = eal.duplicate_asset(FPRUN_BP_PATH, CRANIATA_BP_PATH)
    if duplicated is None:
        raise RuntimeError(f"Failed to duplicate {FPRUN_BP_PATH} -> {CRANIATA_BP_PATH}")

    log(f"Duplicated {FPRUN_BP_PATH} -> {CRANIATA_BP_PATH}")
    return eal.load_asset(CRANIATA_BP_PATH)


# ---------------------------------------------------------------------------
# Step 2: Add components
# ---------------------------------------------------------------------------

def _add_static_mesh_components(blueprint, transforms_data):
    sds = _get_subobject_data_subsystem()
    if sds is None:
        raise RuntimeError("SubobjectDataSubsystem unavailable")
    root = _root_handle(sds, blueprint)
    eal = unreal.EditorAssetLibrary
    material_cache = {}
    missing_material_paths = set()

    added = 0
    skipped_missing = 0
    skipped_failed = 0
    material_overrides_applied = 0
    objects = transforms_data["objects"]

    for obj_name, t in objects.items():
        asset_obj_path = f"{SM_FOLDER}/{obj_name}.{obj_name}"
        sm_asset = eal.load_asset(asset_obj_path)
        if sm_asset is None:
            warn(f"asset not found, skipped: {asset_obj_path}")
            skipped_missing += 1
            continue

        params = unreal.AddNewSubobjectParams()
        params.parent_handle = root
        params.new_class = unreal.StaticMeshComponent
        params.blueprint_context = blueprint

        new_handle, fail_reason = sds.add_new_subobject(params)
        fail_str = str(fail_reason) if fail_reason else ""
        if fail_str:
            err(f"add_new_subobject failed for {obj_name}: {fail_str}")
            skipped_failed += 1
            continue

        try:
            sds.rename_subobject(handle=new_handle, new_name=unreal.Text(_sanitize_component_name(obj_name)))
        except Exception as exc:
            warn(f"rename_subobject failed for {obj_name}: {exc}")

        data_obj = sds.k2_find_subobject_data_from_handle(new_handle)
        if data_obj is None:
            err(f"k2_find_subobject_data_from_handle returned None for {obj_name}")
            skipped_failed += 1
            continue

        comp_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data_obj)
        comp = comp_obj if isinstance(comp_obj, unreal.StaticMeshComponent) else None
        if comp is None:
            err(f"could not cast to StaticMeshComponent for {obj_name}: got {type(comp_obj).__name__}")
            skipped_failed += 1
            continue

        comp.set_static_mesh(sm_asset)
        if ENABLE_COMPONENT_COLLISION:
            comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
            try:
                comp.set_collision_profile_name(COMPONENT_COLLISION_PROFILE)
            except Exception:
                try:
                    comp.set_editor_property("collision_profile_name", COMPONENT_COLLISION_PROFILE)
                except Exception as exc:
                    warn(f"failed to set collision profile for {obj_name}: {exc}")
            try:
                comp.set_generate_overlap_events(False)
            except Exception:
                pass
        else:
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

        if _apply_material_override(comp, sm_asset, obj_name, material_cache, missing_material_paths):
            material_overrides_applied += 1

        ue_loc = _blender_world_to_bp_local(t["location_cm"])
        ue_quat = _blender_quat_to_ue_quat(t["rotation_quat_xyzw"])

        if FORCE_UNIT_SCALE:
            scale = unreal.Vector(1.0, 1.0, 1.0)
        else:
            scl = t["scale"]
            scale = unreal.Vector(float(scl[0]), float(scl[1]), float(scl[2]))

        location = unreal.Vector(float(ue_loc[0]), float(ue_loc[1]), float(ue_loc[2]))
        transform = unreal.Transform(location=location, rotation=ue_quat.rotator(), scale=scale)
        comp.set_relative_transform(transform, sweep=False, teleport=True)

        added += 1

    return added, skipped_missing, skipped_failed, material_overrides_applied, sorted(missing_material_paths)


# ---------------------------------------------------------------------------
# Step 3: Set CDO properties
# ---------------------------------------------------------------------------

def _set_cdo_properties(blueprint):
    eal = unreal.EditorAssetLibrary
    def_asset = eal.load_asset(DEF_PATH)
    if def_asset is None:
        raise RuntimeError(f"Definition asset not found: {DEF_PATH}")

    gen_class = blueprint.generated_class()
    cdo = unreal.get_default_object(gen_class)

    cdo.set_editor_property("generated_definition", def_asset)
    cdo.set_editor_property("generator_spec", None)

    actual_def = cdo.get_editor_property("generated_definition")
    actual_spec = cdo.get_editor_property("generator_spec")
    return actual_def, actual_spec


def _find_root_scene_component(blueprint):
    """Locate the actor root SceneComponent in the Blueprint subobject tree."""
    sds = _get_subobject_data_subsystem()
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    for handle in handles:
        data = sds.k2_find_subobject_data_from_handle(handle)
        if data is None:
            continue
        if lib.is_root_component(data):
            return lib.get_object(data)

    for handle in handles:
        data = sds.k2_find_subobject_data_from_handle(handle)
        if data is None:
            continue
        obj = lib.get_object(data)
        if isinstance(obj, unreal.SceneComponent) and obj.get_name() == "SubmarineRoot":
            return obj

    return None


def _apply_root_yaw_compensation(blueprint, yaw_deg):
    """Rotate the Blueprint root so the bow lands on UE world +X."""
    if abs(yaw_deg) < 1e-3:
        return None

    root_obj = _find_root_scene_component(blueprint)
    if root_obj is None:
        warn("root yaw compensation skipped: no SceneComponent root found")
        return None

    try:
        root_obj.set_editor_property(
            "relative_rotation",
            unreal.Rotator(pitch=0.0, yaw=float(yaw_deg), roll=0.0),
        )
    except Exception as exc:
        warn(f"root yaw compensation failed: {exc}")
        return None

    log(f"root yaw set on {root_obj.get_name()} -> {yaw_deg} deg")
    return yaw_deg


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if not TRANSFORMS_JSON.is_file():
        err(f"transforms JSON not found at {TRANSFORMS_JSON}")
        err("Run Blender export_transforms.py first.")
        return

    data = json.loads(TRANSFORMS_JSON.read_text(encoding="utf-8"))
    log(f"loaded {data['object_count']} object transforms from JSON")
    log(f"axis mirror applied: {APPLY_FBX_AXIS_MIRROR}")
    _log_transform_anomalies(data)
    _validate_imported_asset_space()

    blueprint = _ensure_craniata_bp()

    added, skipped_missing, skipped_failed, material_overrides_applied, missing_material_paths = _add_static_mesh_components(blueprint, data)
    log(f"components added: {added}")
    log(f"material overrides applied: {material_overrides_applied}")
    if skipped_missing:
        warn(f"  skipped (asset missing in /Game): {skipped_missing}")
    if skipped_failed:
        warn(f"  skipped (subobject API failure): {skipped_failed}")
    if missing_material_paths:
        warn(f"missing material assets: {len(missing_material_paths)}")
        for material_path in missing_material_paths[:MAX_LOGGED_MISSING_MATERIALS]:
            warn(f"  missing material: {material_path}")
        if len(missing_material_paths) > MAX_LOGGED_MISSING_MATERIALS:
            warn(f"  ... and {len(missing_material_paths) - MAX_LOGGED_MISSING_MATERIALS} more")

    actual_def, actual_spec = _set_cdo_properties(blueprint)
    def_path = actual_def.get_path_name() if actual_def is not None else "None"
    spec_path = actual_spec.get_path_name() if actual_spec is not None else "None"

    applied_yaw = _apply_root_yaw_compensation(blueprint, ROOT_YAW_COMPENSATION_DEG)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    log("=" * 60)
    log(f"BP saved: {CRANIATA_BP_PATH}")
    log(f"  components added:          {added}")
    log(f"  material overrides:       {material_overrides_applied}")
    log(f"  CDO GeneratedDefinition:   {def_path}")
    log(f"  CDO GeneratorSpec:         {spec_path}")
    log(f"  axis mirror applied:       {APPLY_FBX_AXIS_MIRROR}")
    log(f"  root yaw compensation:     {applied_yaw if applied_yaw is not None else 'skipped'} deg")
    log(f"  component collision:       {'enabled' if ENABLE_COMPONENT_COLLISION else 'disabled'}")
    log(f"  collision profile:         {COMPONENT_COLLISION_PROFILE if ENABLE_COMPONENT_COLLISION else 'None'}")
    log(f"  source folder:             {SM_FOLDER}")
    log("=" * 60)


main()
