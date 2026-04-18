"""
Sub3D - Wire doors on BP_Submarine_Craniata
===========================================

Two jobs in one pass:

1. Remove static door mesh components from the Blueprint that were carried in
   during the per-mesh harvest:
     SM_Door_*, SM_Airlock_Door_*, SM_HatchDoor_*
   (Frames like SM_Door_*_Frame are kept — they are the fixed bulkhead cutouts.)
   Doors are instead spawned at runtime by
   `ASubmarineBase::SpawnDoorsFromDefinition()` from each connection in
   `GeneratedDefinition->Connections`.

2. Set `GeneratorDoorActorClass = BP_SubDoor` on the Blueprint CDO so the
   runtime spawn has a class to instantiate.

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/wire_craniata_doors.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4
"""

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"
DOOR_BP_PATH = "/Game/Sub3D/Blueprint/SubBP/BP_SubDoor"

# A component is pruned if its name starts with any of these prefixes AND does
# not contain any of the protected substrings. Prefixes are matched against
# the sanitized subobject name (spaces / dots already replaced by '_').
REMOVE_PREFIXES = (
    "SM_Door_",
    "SM_Airlock_Door",
    "SM_HatchDoor_",
)
PROTECT_SUBSTRINGS = (
    "_Frame",          # SM_Door_*_Frame are the static bulkhead frames
    "_Cassette",       # SM_Airlock_Cassette (not a door)
)


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):   unreal.log(f"[WireDoors] {msg}")
def warn(msg):  unreal.log_warning(f"[WireDoors] {msg}")
def err(msg):   unreal.log_error(f"[WireDoors] {msg}")


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


def _should_remove(name):
    if not any(name.startswith(p) for p in REMOVE_PREFIXES):
        return False
    for protect in PROTECT_SUBSTRINGS:
        if protect in name:
            return False
    return True


# ---------------------------------------------------------------------------
# Step 1: prune door components
# ---------------------------------------------------------------------------

def _prune_door_components(blueprint):
    sds = _get_subobject_data_subsystem()
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        err("no subobject handles for BP")
        return 0

    to_delete = []
    for h in handles:
        data = sds.k2_find_subobject_data_from_handle(h)
        if data is None:
            continue
        obj = lib.get_object(data)
        if obj is None:
            continue
        name = obj.get_name()
        if not isinstance(obj, unreal.StaticMeshComponent):
            continue
        if _should_remove(name):
            to_delete.append((h, name))

    removed = 0
    for h, name in to_delete:
        try:
            # UE 5.7: delete_subobject wants the blueprint context + the handle.
            # Different engine versions expose this under different names; try
            # the common variants in order.
            deleted = False
            for fn_name in ("delete_subobject", "delete_subobjects", "delete_subobject_from_instance"):
                fn = getattr(sds, fn_name, None)
                if fn is None:
                    continue
                try:
                    if fn_name == "delete_subobjects":
                        fn(context_object=blueprint, subobjects_to_delete=[h], blueprint_context=blueprint)
                    else:
                        fn(context_object=blueprint, subobject_to_delete=h, blueprint_context=blueprint)
                    deleted = True
                    break
                except Exception:
                    continue
            if not deleted:
                warn(f"could not delete {name} (no compatible API)")
                continue
            log(f"  removed component: {name}")
            removed += 1
        except Exception as e:
            warn(f"  delete failed for {name}: {e}")

    return removed


# ---------------------------------------------------------------------------
# Step 2: set GeneratorDoorActorClass
# ---------------------------------------------------------------------------

def _set_generator_door_actor_class(blueprint):
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(DOOR_BP_PATH):
        raise RuntimeError(f"door BP not found at {DOOR_BP_PATH}")
    door_bp = eal.load_asset(DOOR_BP_PATH)
    if door_bp is None:
        raise RuntimeError(f"failed to load {DOOR_BP_PATH}")

    door_class = door_bp.generated_class()
    if door_class is None:
        raise RuntimeError(f"no generated class on {DOOR_BP_PATH}")

    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("generator_door_actor_class", door_class)

    actual = cdo.get_editor_property("generator_door_actor_class")
    return actual


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(CRANIATA_BP_PATH):
        err(f"Blueprint not found: {CRANIATA_BP_PATH}")
        return

    blueprint = eal.load_asset(CRANIATA_BP_PATH)
    log(f"loaded {CRANIATA_BP_PATH}")

    removed = _prune_door_components(blueprint)
    log(f"components removed: {removed}")

    actual_class = _set_generator_door_actor_class(blueprint)
    actual_class_name = actual_class.get_path_name() if actual_class is not None else "None"
    log(f"CDO GeneratorDoorActorClass: {actual_class_name}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    eal.save_loaded_asset(blueprint)

    log("=" * 60)
    log(f"BP saved: {CRANIATA_BP_PATH}")
    log(f"  door components removed:      {removed}")
    log(f"  GeneratorDoorActorClass:      {actual_class_name}")
    log("=" * 60)


main()
