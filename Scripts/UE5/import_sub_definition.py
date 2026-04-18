"""
Sub3D - Import Craniata Submarine Definition into UE5
======================================================

Reads Content/Sub3D/FirstPlayableRun/Craniata_Definition.json (produced by the
Spec Extraction Bridge) and writes DA_SubDef_Craniata as a USubmarineDefinition
DataAsset. Runtime uses this directly via ASubmarineBase::GeneratedDefinition,
which skips the generator.

Run in UE5 Editor:
    Tools > Python > open Python console, then:
        exec(open(r"C:/Dev/Sub3D/Scripts/UE5/import_sub_definition.py").read())

Run from command line (headless editor):
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/import_sub_definition.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4

Related plan:
    reports/plans/2026-04-16_handmade_craniata_consolidated_execution_plan.md
"""

import hashlib
import json
from pathlib import Path

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(r"C:/Dev/Sub3D")
JSON_PATH = REPO_ROOT / "Content" / "Sub3D" / "FirstPlayableRun" / "Craniata_Definition.json"
MAIN_PY_PATH = REPO_ROOT / "Scripts" / "Blender" / "hull_blockout_gpt" / "main.py"
STATIONS_JSON_PATH = REPO_ROOT / "Scripts" / "Blender" / "hull_blockout_gpt" / "data" / "stations.json"

ASSET_FOLDER = "/Game/Sub3D/FirstPlayableRun"
ASSET_NAME = "DA_SubDef_Craniata"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"

# BP_Submarine_Craniata origin is expected to sit at the submarine geometric center.
# JSON X values run 0 (bow) to LENGTH (stern); subtract LENGTH/2 to translate into
# submarine local space centered on the origin. Set to False if the BP keeps X=0 at bow.
CENTER_ORIGIN = True

# Half-width and per-deck height used for FP-quality HydroBounds. Refine later.
FP_HALF_BEAM_CM = 200.0
FP_DECK_HEIGHT_CM = 200.0


# ---------------------------------------------------------------------------
# Connection type mapping
# ---------------------------------------------------------------------------

CONNECTION_TYPE_MAP = {
    "WatertightDoor": unreal.ConnectionType.DOOR,
    "Airlock": unreal.ConnectionType.DOOR,
    "AirlockExterior": unreal.ConnectionType.EXTERIOR_HATCH,
    "Hatch": unreal.ConnectionType.HATCH,
    "Ladder": unreal.ConnectionType.OPEN,
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log_info(msg: str) -> None:
    unreal.log(f"[ImportSubDef] {msg}")


def log_warn(msg: str) -> None:
    unreal.log_warning(f"[ImportSubDef] {msg}")


def log_error(msg: str) -> None:
    unreal.log_error(f"[ImportSubDef] {msg}")


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def offset_x(x_cm: float, length_cm: float) -> float:
    return x_cm - 0.5 * length_cm if CENTER_ORIGIN else x_cm


def make_yaw_rotator(yaw_deg: float) -> "unreal.Rotator":
    return unreal.Rotator(pitch=0.0, yaw=float(yaw_deg), roll=0.0)


def semantic_to_enum(semantic: str) -> "unreal.SubCompartmentType":
    mapping = {
        "Airlock": unreal.SubCompartmentType.AIRLOCK,
        "UpperAirlock": unreal.SubCompartmentType.AIRLOCK,
    }
    return mapping.get(semantic, unreal.SubCompartmentType.GENERIC)


def load_or_create_definition():
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(ASSET_PATH):
        asset = eal.load_asset(ASSET_PATH)
        log_info(f"loaded existing asset {ASSET_PATH}")
        return asset

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = None

    try:
        factory = unreal.DataAssetFactory()
        try:
            factory.set_editor_property("data_asset_class", unreal.SubmarineDefinition)
        except Exception:
            for attr in ("DataAssetClass", "asset_class", "AssetClass"):
                try:
                    factory.set_editor_property(attr, unreal.SubmarineDefinition)
                    break
                except Exception:
                    continue
        asset = asset_tools.create_asset(
            asset_name=ASSET_NAME,
            package_path=ASSET_FOLDER,
            asset_class=unreal.SubmarineDefinition,
            factory=factory,
        )
    except Exception as factory_err:
        log_warn(f"DataAssetFactory path failed ({factory_err}); retrying with no factory")
        asset = asset_tools.create_asset(
            asset_name=ASSET_NAME,
            package_path=ASSET_FOLDER,
            asset_class=unreal.SubmarineDefinition,
            factory=None,
        )

    if asset is None:
        raise RuntimeError(f"Failed to create DataAsset at {ASSET_PATH}")
    log_info(f"created new asset {ASSET_PATH}")
    return asset


def verify_source_hashes(data: dict) -> None:
    expected = data.get("source_hashes", {})
    problems = []
    if MAIN_PY_PATH.is_file():
        actual = sha256_file(MAIN_PY_PATH)
        if expected.get("main_py") != actual:
            problems.append(f"main.py hash mismatch (expected {expected.get('main_py')[:12]}..., got {actual[:12]}...)")
    if STATIONS_JSON_PATH.is_file():
        actual = sha256_file(STATIONS_JSON_PATH)
        if expected.get("stations_json") != actual:
            problems.append(
                f"stations.json hash mismatch (expected {expected.get('stations_json')[:12]}..., got {actual[:12]}...)"
            )
    if problems:
        log_warn("source files drifted from bridge output:")
        for problem in problems:
            log_warn(f"  - {problem}")
        log_warn("proceeding anyway - re-run export_sub_definition.py to refresh the JSON")


# ---------------------------------------------------------------------------
# Struct builders
# ---------------------------------------------------------------------------

def build_compartment(c: dict, length_cm: float) -> "unreal.GeneratedCompartmentDef":
    out = unreal.GeneratedCompartmentDef()

    min_x = offset_x(c["min_x_cm"], length_cm)
    max_x = offset_x(c["max_x_cm"], length_cm)
    floor_z = c["deck_z_cm"]
    ceil_z = floor_z + FP_DECK_HEIGHT_CM

    bounds_min = unreal.Vector(min_x, -FP_HALF_BEAM_CM, floor_z)
    bounds_max = unreal.Vector(max_x, FP_HALF_BEAM_CM, ceil_z)

    length_along_x = max_x - min_x
    capacity_liters = (length_along_x * (2.0 * FP_HALF_BEAM_CM) * FP_DECK_HEIGHT_CM) / 1000.0

    out.set_editor_property("compartment_id", unreal.Name(c["id"]))
    out.set_editor_property("display_name", unreal.Text(c["semantic"]))
    out.set_editor_property("semantic_type", semantic_to_enum(c["semantic"]))
    out.set_editor_property("capacity_liters", capacity_liters)
    out.set_editor_property("hydro_bounds_min", bounds_min)
    out.set_editor_property("hydro_bounds_max", bounds_max)
    out.set_editor_property("max_water_height_cm", FP_DECK_HEIGHT_CM)
    out.set_editor_property("walkable_floor_z_cm", floor_z)
    return out


def build_connection(n: dict, length_cm: float) -> "unreal.GeneratedConnectionDef":
    out = unreal.GeneratedConnectionDef()
    from_id = n.get("from") or "NAME_None"
    to_id = n.get("to") or ""
    is_exterior = to_id == "EXT"

    conn_type = CONNECTION_TYPE_MAP.get(n["type"], unreal.ConnectionType.DOOR)

    door_w = float(n.get("door_w_cm") or 0.0)
    door_h = float(n.get("door_h_cm") or 0.0)

    flow_area = 0.0
    if door_w > 0.0 and door_h > 0.0:
        flow_area = door_w * door_h
    else:
        shape = n.get("shape")
        if shape == "round" and n.get("radius_cm"):
            import math

            flow_area = math.pi * float(n["radius_cm"]) ** 2
        elif shape == "rect" and n.get("half_length_cm") and n.get("half_width_cm"):
            flow_area = 4.0 * float(n["half_length_cm"]) * float(n["half_width_cm"])

    x_cm = offset_x(float(n.get("x_cm", 0.0)), length_cm)
    transform = unreal.Transform(
        location=unreal.Vector(x_cm, 0.0, float(n.get("sill_z_cm", 0.0))),
        rotation=make_yaw_rotator(0.0),
        scale=unreal.Vector(1.0, 1.0, 1.0),
    )

    out.set_editor_property("connection_id", unreal.Name(n["id"]))
    out.set_editor_property("compartment_a", unreal.Name(from_id))
    out.set_editor_property("compartment_b", unreal.Name("None") if is_exterior else unreal.Name(to_id))
    out.set_editor_property("flow_area_cm2", flow_area)
    out.set_editor_property("local_transform", transform)
    out.set_editor_property("connection_type", conn_type)
    out.set_editor_property("starts_closed", True)
    out.set_editor_property("door_width_cm", door_w if door_w > 0.0 else 90.0)
    out.set_editor_property("door_height_cm", door_h if door_h > 0.0 else 180.0)
    return out


def build_flood_graph(compartments: list, connections: list) -> "unreal.CompiledFloodGraph":
    graph = unreal.CompiledFloodGraph()

    volumes = []
    for c in compartments:
        volume = unreal.DerivedFloodVolume()
        volume.set_editor_property("volume_id", unreal.Name(c["id"]))
        volume.set_editor_property(
            "capacity_liters",
            float(c["length_cm"]) * (2.0 * FP_HALF_BEAM_CM) * FP_DECK_HEIGHT_CM / 1000.0,
        )
        volumes.append(volume)

    edges = []
    for n in connections:
        edge = unreal.FloodGraphEdge()
        edge.set_editor_property("volume_a", unreal.Name(n["from"]))
        to_id = n.get("to") or ""
        is_exterior = to_id == "EXT"
        edge.set_editor_property("volume_b", unreal.Name("None") if is_exterior else unreal.Name(to_id))
        edge.set_editor_property("closure_id", unreal.Name(n["id"]))
        edge.set_editor_property(
            "passage_area_cm2",
            float(n.get("door_w_cm") or 0.0) * float(n.get("door_h_cm") or 0.0),
        )
        edge.set_editor_property("exterior_edge", is_exterior)
        edges.append(edge)

    graph.set_editor_property("volumes", volumes)
    graph.set_editor_property("edges", edges)
    return graph


def build_spawn_point(sp: dict, length_cm: float) -> "unreal.GeneratedSpawnPointDef":
    out = unreal.GeneratedSpawnPointDef()
    transform = unreal.Transform(
        location=unreal.Vector(offset_x(float(sp["x_cm"]), length_cm), float(sp["y_cm"]), float(sp["z_cm"])),
        rotation=make_yaw_rotator(float(sp.get("yaw_deg", 0.0))),
        scale=unreal.Vector(1.0, 1.0, 1.0),
    )
    out.set_editor_property("spawn_id", unreal.Name(sp["id"]))
    out.set_editor_property("local_transform", transform)
    out.set_editor_property("role", unreal.SpawnRole.CREW)
    return out


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    if not JSON_PATH.is_file():
        log_error(f"JSON not found at {JSON_PATH}. Run export_sub_definition.py first.")
        return

    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    verify_source_hashes(data)

    meta = data["meta"]
    length_cm = float(meta["length_cm"])

    asset = load_or_create_definition()

    asset.set_editor_property("hull_length_cm", length_cm)
    asset.set_editor_property("hull_beam_cm", 2.0 * FP_HALF_BEAM_CM)
    asset.set_editor_property("hull_height_cm", 3.0 * FP_DECK_HEIGHT_CM)
    asset.set_editor_property("wall_thickness_cm", float(meta.get("hull_thick_cm", 15.0)))

    compartments = [build_compartment(c, length_cm) for c in data["compartments"]]
    asset.set_editor_property("compartments", compartments)

    connections = [build_connection(n, length_cm) for n in data["connections"]]
    asset.set_editor_property("connections", connections)

    flood_graph = build_flood_graph(data["compartments"], data["connections"])
    asset.set_editor_property("flood_graph", flood_graph)

    asset.set_editor_property("station_slots", [])

    spawns = [build_spawn_point(sp, length_cm) for sp in data["spawn_points"]]
    asset.set_editor_property("spawn_points", spawns)

    unreal.EditorAssetLibrary.save_loaded_asset(asset)

    log_info("=" * 60)
    log_info(f"asset saved: {ASSET_PATH}")
    log_info(f"  hull length: {length_cm} cm")
    log_info(f"  compartments: {len(compartments)}")
    log_info(f"  connections:  {len(connections)}")
    log_info(f"  spawn points: {len(spawns)}")
    log_info(f"  center origin: {CENTER_ORIGIN} (half-length offset = {0.5 * length_cm if CENTER_ORIGIN else 0.0} cm)")
    log_info("  mesh arrays:  empty (Craniata ships imported static meshes)")
    log_info("  station slots: empty (author in BP_Submarine_Craniata)")
    log_info("=" * 60)


if __name__ == "__main__":
    main()
