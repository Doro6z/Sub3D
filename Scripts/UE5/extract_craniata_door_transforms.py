"""
Sub3D - Extract local transforms of SubDoor child-actor components on
BP_Submarine_Craniata.

Targets ChildActorComponent entries whose name starts with "BP_SubDoor" (covers
the 6 BP_SubDoor instances + the BP_SubAirLockExit / BP_SubDoor_Exterior),
reads the relative transform stored in the SCS template, and dumps:

  RelativeLocation (x, y, z)  cm
  RelativeRotation (pitch, yaw, roll)  deg
  RelativeScale3D  (x, y, z)
  ChildActorClass  (the door class instantiated)

Output goes to stdout AND to a JSON file at:
  C:/Dev/Sub3D/reports/2026-05-05_craniata_door_local_transforms.json

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/extract_craniata_door_transforms.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4 -NullRHI
"""

import json
import os

import unreal


CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"
NAME_PREFIX = "BP_SubDoor"
OUTPUT_JSON = "C:/Dev/Sub3D/reports/2026-05-05_craniata_door_local_transforms.json"


def log(msg):
    unreal.log(f"[ExtractDoorXf] {msg}")


def warn(msg):
    unreal.log_warning(f"[ExtractDoorXf] {msg}")


def err(msg):
    unreal.log_error(f"[ExtractDoorXf] {msg}")


def _get_subobject_data_subsystem():
    try:
        return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        getter = getattr(unreal.SubobjectDataSubsystem, "get", None)
        if callable(getter):
            return getter()
        raise


def _vec_to_dict(v):
    return {"x": float(v.x), "y": float(v.y), "z": float(v.z)}


def _rot_to_dict(r):
    return {"pitch": float(r.pitch), "yaw": float(r.yaw), "roll": float(r.roll)}


def _read_child_actor_class(comp):
    try:
        cls = comp.get_editor_property("child_actor_class")
    except Exception:
        cls = None
    if cls is None:
        return ""
    try:
        return cls.get_path_name()
    except Exception:
        return str(cls)


def main():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(CRANIATA_BP_PATH):
        err(f"Blueprint not found: {CRANIATA_BP_PATH}")
        return

    blueprint = eal.load_asset(CRANIATA_BP_PATH)
    log(f"loaded {CRANIATA_BP_PATH}")

    sds = _get_subobject_data_subsystem()
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        err("no subobject handles for BP")
        return

    rows = []
    for h in handles:
        data = sds.k2_find_subobject_data_from_handle(h)
        if data is None:
            continue
        obj = lib.get_object(data)
        if obj is None:
            continue
        name = obj.get_name()
        if not name.startswith(NAME_PREFIX):
            continue
        if not isinstance(obj, unreal.SceneComponent):
            continue

        try:
            loc = obj.get_editor_property("relative_location")
        except Exception:
            loc = unreal.Vector(0, 0, 0)
        try:
            rot = obj.get_editor_property("relative_rotation")
        except Exception:
            rot = unreal.Rotator(0, 0, 0)
        try:
            scl = obj.get_editor_property("relative_scale3d")
        except Exception:
            scl = unreal.Vector(1, 1, 1)

        child_class = ""
        if isinstance(obj, unreal.ChildActorComponent):
            child_class = _read_child_actor_class(obj)

        row = {
            "component_name": name,
            "component_class": type(obj).__name__,
            "child_actor_class": child_class,
            "relative_location_cm": _vec_to_dict(loc),
            "relative_rotation_deg": _rot_to_dict(rot),
            "relative_scale3d": _vec_to_dict(scl),
        }
        rows.append(row)

        log(
            f"  {name}  loc=({loc.x:.3f}, {loc.y:.3f}, {loc.z:.3f})  "
            f"rot=(P={rot.pitch:.3f}, Y={rot.yaw:.3f}, R={rot.roll:.3f})  "
            f"scl=({scl.x:.3f}, {scl.y:.3f}, {scl.z:.3f})  "
            f"class={child_class}"
        )

    rows.sort(key=lambda r: r["component_name"])

    payload = {
        "blueprint_path": CRANIATA_BP_PATH,
        "name_prefix": NAME_PREFIX,
        "count": len(rows),
        "components": rows,
    }

    os.makedirs(os.path.dirname(OUTPUT_JSON), exist_ok=True)
    with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)

    log("=" * 60)
    log(f"components matched ({NAME_PREFIX}*): {len(rows)}")
    log(f"json written: {OUTPUT_JSON}")
    log("=" * 60)


main()
