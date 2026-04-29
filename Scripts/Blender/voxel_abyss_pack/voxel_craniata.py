"""
Sub3D - Voxel Abyss Pack - Craniata Reconstruction
==================================================

Craniata-focused voxel family:
- full rebuild variants
- modular hull sections
- reusable subsystem modules
"""

from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path


def _resolve_this_dir() -> Path:
    candidates = []
    file_value = globals().get("__file__")
    if file_value:
        candidates.append(Path(file_value).resolve().parent)

    try:
        import bpy

        for text in (
            getattr(bpy.context, "edit_text", None),
            getattr(getattr(bpy.context, "space_data", None), "text", None),
        ):
            filepath = getattr(text, "filepath", "")
            if filepath:
                candidates.append(Path(bpy.path.abspath(filepath)).resolve().parent)
    except Exception:
        pass

    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path.cwd()


THIS_DIR = _resolve_this_dir()
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))


def _load_core():
    module_path = THIS_DIR / "core.py"
    existing = sys.modules.get("core")
    if existing is not None:
        existing_file = getattr(existing, "__file__", None)
        if existing_file:
            try:
                if Path(existing_file).resolve() == module_path.resolve():
                    return existing
            except Exception:
                pass
        sys.modules.pop("core", None)

    spec = importlib.util.spec_from_file_location("core", module_path)
    if spec is None or spec.loader is None:
        raise ModuleNotFoundError(f"Unable to load local module 'core' from '{module_path}'")

    module = importlib.util.module_from_spec(spec)
    sys.modules["core"] = module
    spec.loader.exec_module(module)
    return module


try:
    from core import MaterialDef, VoxelGrid, ensure_collection, ensure_palette, ensure_scene, focus_collection, interpolate_knots, spawn_asset
except (ModuleNotFoundError, ImportError):
    core = _load_core()
    MaterialDef = core.MaterialDef
    VoxelGrid = core.VoxelGrid
    ensure_collection = core.ensure_collection
    ensure_palette = core.ensure_palette
    ensure_scene = core.ensure_scene
    focus_collection = core.focus_collection
    interpolate_knots = core.interpolate_knots
    spawn_asset = core.spawn_asset


HULL = 0
PANEL = 1
WINDOW = 2
RUST = 3
GLOW = 4
STEEL = 5

PALETTE = [
    MaterialDef("Hull", (0.16, 0.20, 0.22), metallic=0.18, roughness=0.60),
    MaterialDef("Panel", (0.24, 0.28, 0.30), metallic=0.10, roughness=0.70),
    MaterialDef("Window", (0.07, 0.22, 0.32), roughness=0.16, emission=0.4),
    MaterialDef("Rust", (0.32, 0.18, 0.10), roughness=0.82),
    MaterialDef("Glow", (0.08, 0.78, 0.92), roughness=0.14, emission=4.6),
    MaterialDef("Steel", (0.43, 0.47, 0.49), metallic=0.35, roughness=0.48),
]


def _m_to_vox(meters: float, voxel_cm: float) -> int:
    return max(1, int(round((meters * 100.0) / voxel_cm)))


def _build_hull_segment(
    *,
    voxel_cm: float,
    length_m: float,
    radius_y_m: float,
    radius_z_m: float,
    broken: bool = False,
    windows: bool = True,
) -> tuple[VoxelGrid, VoxelGrid]:
    detail_voxel = voxel_cm / 3.0
    fine = max(1, int(round(voxel_cm / detail_voxel)))

    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(detail_voxel)

    length = _m_to_vox(length_m, voxel_cm)
    ry = _m_to_vox(radius_y_m, voxel_cm)
    rz = _m_to_vox(radius_z_m, voxel_cm)

    knots = [
        (0, max(2, ry // 2), max(2, rz // 2), rz + 2),
        (int(length * 0.2), ry, rz, rz + 2),
        (int(length * 0.5), ry, rz + 1, rz + 2),
        (int(length * 0.8), max(2, ry - 1), max(2, rz - 1), rz + 1),
        (length, max(2, ry // 2), max(2, rz // 2), rz),
    ]

    for x in range(length + 1):
        half_y, half_z, cz = interpolate_knots(x, knots)
        body.fill_ellipse_x(x, 0, cz, half_y, half_z, HULL)
        body.fill_ellipse_x(x, 0, cz, max(0, half_y - 2), max(0, half_z - 2), PANEL)
        if 4 < x < length - 4:
            body.carve_box(x, -1, cz - 2, x, 1, cz + 2)
        if x % max(4, length // 16) == 0:
            body.set(x, -half_y, cz + 1, STEEL)
            body.set(x, half_y, cz + 1, STEEL)

    if windows:
        for side in (-1, 1):
            x0 = int(length * 0.25)
            x1 = int(length * 0.75)
            body.fill_box(x0, side * max(2, ry - 2), rz + 2, x1, side * max(2, ry - 1), rz + 3, WINDOW)

    detail.fill_polyline([(int(length * 0.1) * fine, 0, (rz + 4) * fine), (int(length * 0.5) * fine, 0, (rz + 6) * fine), (int(length * 0.9) * fine, 0, (rz + 4) * fine)], 2, STEEL)

    if broken:
        bx0 = int(length * 0.4)
        bx1 = int(length * 0.65)
        body.carve_box(bx0, -ry - 1, max(1, rz - 3), bx1, ry + 1, rz + 5)
        for x in range(bx0 - 2, bx1 + 2, 3):
            body.fill_box(x, -ry - 2, max(1, rz - 3), x + 1, ry + 2, max(1, rz - 2), RUST)
        detail.fill_polyline([(bx0 * fine, -ry * fine, (rz + 2) * fine), (bx1 * fine, -int(ry * 1.8) * fine, rz * fine)], 2, RUST)
        detail.fill_polyline([(bx0 * fine, ry * fine, (rz + 2) * fine), (bx1 * fine, int(ry * 1.8) * fine, rz * fine)], 2, STEEL)

    return body, detail


def _build_tower_module(voxel_cm: float, height_m: float = 6.0) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    h = _m_to_vox(height_m, voxel_cm)
    half = _m_to_vox(2.8, voxel_cm)
    body.fill_box(-half, -half, 0, half, half, h, PANEL)
    body.carve_box(-half + 1, -half + 1, 1, half - 1, half - 1, h - 1)
    body.fill_box(-1, -half, _m_to_vox(1.0, voxel_cm), 1, -half, _m_to_vox(2.2, voxel_cm), WINDOW)
    body.fill_box(-1, half, _m_to_vox(1.0, voxel_cm), 1, half, _m_to_vox(2.2, voxel_cm), WINDOW)
    body.fill_box(-1, -1, h + 1, 1, 1, h + _m_to_vox(1.2, voxel_cm), GLOW)
    return body, detail


def _build_airlock_module(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    length = _m_to_vox(7.0, voxel_cm)
    half = _m_to_vox(2.2, voxel_cm)
    h = _m_to_vox(3.2, voxel_cm)
    body.fill_box(0, -half, 0, length, half, h, STEEL)
    body.carve_box(1, -half + 1, 1, length - 1, half - 1, h - 1)
    body.fill_box(_m_to_vox(1.0, voxel_cm), -half + 1, 1, _m_to_vox(1.5, voxel_cm), half - 1, h - 1, RUST)
    body.fill_box(length - _m_to_vox(1.5, voxel_cm), -half + 1, 1, length - _m_to_vox(1.0, voxel_cm), half - 1, h - 1, RUST)
    return body, detail


def _build_ballast_pod(voxel_cm: float, starboard: bool) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    length = _m_to_vox(8.0, voxel_cm)
    for x in range(length + 1):
        body.fill_ellipse_x(x, 0, _m_to_vox(1.8, voxel_cm), _m_to_vox(1.4, voxel_cm), _m_to_vox(1.0, voxel_cm), HULL)
    body.fill_box(2, -1, _m_to_vox(2.2, voxel_cm), length - 2, 1, _m_to_vox(2.6, voxel_cm), PANEL)

    if starboard:
        body.stamp(VoxelGrid(voxel_cm), (0, _m_to_vox(0.5, voxel_cm), 0))
    else:
        body.stamp(VoxelGrid(voxel_cm), (0, -_m_to_vox(0.5, voxel_cm), 0))
    return body, detail


def _build_prop_cluster(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    shaft = _m_to_vox(6.0, voxel_cm)
    body.fill_box(0, -1, _m_to_vox(1.8, voxel_cm), shaft, 1, _m_to_vox(2.2, voxel_cm), STEEL)
    hub_x = shaft + _m_to_vox(1.0, voxel_cm)
    body.fill_box(hub_x - 1, -1, _m_to_vox(1.5, voxel_cm), hub_x + 1, 1, _m_to_vox(2.5, voxel_cm), STEEL)
    for side in (-1, 1):
        body.fill_box(hub_x, side * 1, _m_to_vox(1.8, voxel_cm), hub_x + _m_to_vox(2.5, voxel_cm), side * _m_to_vox(2.5, voxel_cm), _m_to_vox(2.4, voxel_cm), STEEL)
    return body, detail


def _build_craniata_asset(spec) -> tuple[VoxelGrid, VoxelGrid]:
    voxel_cm = float(spec.get("voxel_cm", 20.0))
    kind = spec["kind"]

    if kind == "full":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=42.0, radius_y_m=4.0, radius_z_m=3.2, broken=False, windows=True)
    if kind == "full_derelict":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=42.0, radius_y_m=4.0, radius_z_m=3.2, broken=True, windows=True)
    if kind == "nose":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=10.0, radius_y_m=3.6, radius_z_m=2.8, windows=False)
    if kind == "mid_a":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=8.0, radius_y_m=4.0, radius_z_m=3.0, windows=True)
    if kind == "mid_b":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=8.0, radius_y_m=4.2, radius_z_m=3.2, windows=True)
    if kind == "engine":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=8.0, radius_y_m=4.4, radius_z_m=3.2, windows=False)
    if kind == "tail":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=6.0, radius_y_m=3.4, radius_z_m=2.6, windows=False)
    if kind == "tower":
        return _build_tower_module(voxel_cm, 6.0)
    if kind == "airlock":
        return _build_airlock_module(voxel_cm)
    if kind == "pod_port":
        return _build_ballast_pod(voxel_cm, False)
    if kind == "pod_stbd":
        return _build_ballast_pod(voxel_cm, True)
    if kind == "prop":
        return _build_prop_cluster(voxel_cm)
    if kind == "corridor":
        return _build_hull_segment(voxel_cm=voxel_cm, length_m=6.0, radius_y_m=2.0, radius_z_m=1.8, windows=False)
    return _build_hull_segment(voxel_cm=voxel_cm, length_m=4.0, radius_y_m=1.8, radius_z_m=1.6, windows=False)


ASSET_DEFS = [
    {"name": "SM_VX_Craniata_FullRebuild_A", "kind": "full", "voxel_cm": 20.0},
    {"name": "SM_VX_Craniata_FullRebuild_Derelict", "kind": "full_derelict", "voxel_cm": 20.0},
    {"name": "SM_VX_Craniata_Nose_10m", "kind": "nose", "voxel_cm": 15.0},
    {"name": "SM_VX_Craniata_Mid_8m_A", "kind": "mid_a", "voxel_cm": 15.0},
    {"name": "SM_VX_Craniata_Mid_8m_B", "kind": "mid_b", "voxel_cm": 15.0},
    {"name": "SM_VX_Craniata_Engine_8m", "kind": "engine", "voxel_cm": 15.0},
    {"name": "SM_VX_Craniata_Tail_6m", "kind": "tail", "voxel_cm": 15.0},
    {"name": "SM_VX_Craniata_Tower_Module", "kind": "tower", "voxel_cm": 12.0},
    {"name": "SM_VX_Craniata_Airlock_Module", "kind": "airlock", "voxel_cm": 12.0},
    {"name": "SM_VX_Craniata_BallastPod_Port", "kind": "pod_port", "voxel_cm": 12.0},
    {"name": "SM_VX_Craniata_BallastPod_Stbd", "kind": "pod_stbd", "voxel_cm": 12.0},
    {"name": "SM_VX_Craniata_PropCluster", "kind": "prop", "voxel_cm": 10.0},
    {"name": "SM_VX_Craniata_InteriorCorridor_6m", "kind": "corridor", "voxel_cm": 10.0},
    {"name": "SM_VX_Craniata_UtilityCapsule_4m", "kind": "utility", "voxel_cm": 10.0},
]


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Craniata_Voxel", parent_collection)
    materials = ensure_palette("VXCraniata", PALETTE)
    built = []

    cols = 4
    step_x = 4600.0
    step_y = 3800.0

    for index, spec in enumerate(ASSET_DEFS):
        body, detail = _build_craniata_asset(spec)
        row = index // cols
        col = index % cols
        location = (origin[0] + col * step_x, origin[1] + row * step_y, origin[2])
        spawn_asset(spec["name"], [body, detail], materials, collection, location)
        built.append(spec["name"])

    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Craniata_Voxel] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
