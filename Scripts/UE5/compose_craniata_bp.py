"""
Sub3D - Compose BP_Submarine_Craniata from imported SM_* assets + transforms JSON
=================================================================================

Pipeline:
  1. Read Craniata_Transforms.json (produced by Blender export_transforms.py).
  2. Duplicate /Game/.../BP_Submarine_FPRun -> /Game/.../BP_Submarine_Craniata.
     If BP_Submarine_Craniata already exists, it is replaced.
  3. For every SM_* in the JSON, locate the matching imported asset under
     /Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/ and add a
     StaticMeshComponent to the duplicated BP at the JSON-recorded transform.
  4. Set CDO properties: GeneratedDefinition = DA_SubDef_Craniata,
     GeneratorSpec = None.
  5. Compile + save the Blueprint.
  6. Report counts and CDO state in the log for verification.

This script is the headless companion to the user's "Import Into Level +
Harvest Components" interactive workflow. It avoids the FbxSceneImportFactory
which is unreliable in commandlet mode.

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/compose_craniata_bp.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4

Run from the editor Python console:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/compose_craniata_bp.py").read())
"""

import json
from pathlib import Path

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(r"C:/Dev/Sub3D")
TRANSFORMS_JSON = REPO_ROOT / "Content" / "Sub3D" / "FirstPlayableRun" / "Meshes" / "Blockout" / "Craniata_Transforms.json"

SM_FOLDER = "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata"
FPRUN_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_FPRun"
CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"
DEF_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata"


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):   unreal.log(f"[ComposeCraniata] {msg}")
def warn(msg):  unreal.log_warning(f"[ComposeCraniata] {msg}")
def err(msg):   unreal.log_error(f"[ComposeCraniata] {msg}")


# ---------------------------------------------------------------------------
# Step 1: Load + duplicate
# ---------------------------------------------------------------------------

def _ensure_craniata_bp() -> "unreal.Object":
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
# Step 2: Add components via SubobjectDataSubsystem
# ---------------------------------------------------------------------------

def _root_handle(sds, blueprint):
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        raise RuntimeError("k2_gather_subobject_data_for_blueprint returned no handles")
    # Index 0 is the BP class root subobject; component default root is the
    # first child (or itself if the BP root is already a SceneComponent).
    return handles[0]


def _sanitize_component_name(raw_name: str) -> str:
    return raw_name.replace(".", "_")


def _get_subobject_data_subsystem():
    # UE 5.7 dropped SubobjectDataSubsystem.get(); use the engine-subsystem
    # accessor instead. Fall back to the legacy classmethod for older builds.
    try:
        return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        getter = getattr(unreal.SubobjectDataSubsystem, "get", None)
        if callable(getter):
            return getter()
        raise


def _add_static_mesh_components(blueprint, transforms_data):
    sds = _get_subobject_data_subsystem()
    if sds is None:
        raise RuntimeError("SubobjectDataSubsystem unavailable")
    root = _root_handle(sds, blueprint)
    eal = unreal.EditorAssetLibrary

    added = 0
    skipped_missing = 0
    skipped_failed = 0
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
        # FText -> str via str() (UE 5.7 dropped the .to_string() method).
        fail_str = str(fail_reason) if fail_reason else ""
        if fail_str:
            err(f"add_new_subobject failed for {obj_name}: {fail_str}")
            skipped_failed += 1
            continue

        try:
            sds.rename_subobject(handle=new_handle, new_name=unreal.Text(_sanitize_component_name(obj_name)))
        except Exception as e:
            warn(f"rename_subobject failed for {obj_name}: {e}")

        data_obj = sds.k2_find_subobject_data_from_handle(new_handle)
        if data_obj is None:
            err(f"k2_find_subobject_data_from_handle returned None for {obj_name}")
            skipped_failed += 1
            continue

        # UE 5.7: SubobjectData has no `.get_object()` instance method; use the
        # static library calls instead.
        comp_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data_obj)
        comp = comp_obj if isinstance(comp_obj, unreal.StaticMeshComponent) else None

        if comp is None:
            err(f"could not cast to StaticMeshComponent for {obj_name}: got {type(comp_obj).__name__}")
            skipped_failed += 1
            continue

        comp.set_static_mesh(sm_asset)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

        loc = t["location_cm"]
        rot_q = t["rotation_quat_xyzw"]
        scl = t["scale"]
        location = unreal.Vector(float(loc[0]), float(loc[1]), float(loc[2]))
        quat = unreal.Quat(float(rot_q[0]), float(rot_q[1]), float(rot_q[2]), float(rot_q[3]))
        scale = unreal.Vector(float(scl[0]), float(scl[1]), float(scl[2]))
        transform = unreal.Transform(location=location, rotation=quat.rotator(), scale=scale)
        comp.set_relative_transform(transform, sweep=False, teleport=True)

        added += 1

    return added, skipped_missing, skipped_failed


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

    # Read back to confirm.
    actual_def = cdo.get_editor_property("generated_definition")
    actual_spec = cdo.get_editor_property("generator_spec")
    return actual_def, actual_spec


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

    blueprint = _ensure_craniata_bp()

    added, skipped_missing, skipped_failed = _add_static_mesh_components(blueprint, data)
    log(f"components added: {added}")
    if skipped_missing:
        warn(f"  skipped (asset missing in /Game): {skipped_missing}")
    if skipped_failed:
        warn(f"  skipped (subobject API failure): {skipped_failed}")

    actual_def, actual_spec = _set_cdo_properties(blueprint)
    def_path = actual_def.get_path_name() if actual_def is not None else "None"
    spec_path = actual_spec.get_path_name() if actual_spec is not None else "None"

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    log("=" * 60)
    log(f"BP saved: {CRANIATA_BP_PATH}")
    log(f"  components added:       {added}")
    log(f"  CDO GeneratedDefinition: {def_path}")
    log(f"  CDO GeneratorSpec:       {spec_path}")
    log("=" * 60)


main()
