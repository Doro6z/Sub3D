"""
Sub3D - Voxel Abyss Pack - Geological Formations
================================================
"""

from __future__ import annotations

import importlib.util
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
    if "core" in sys.modules:
        return sys.modules["core"]

    module_path = THIS_DIR / "core.py"
    spec = importlib.util.spec_from_file_location("core", module_path)
    if spec is None or spec.loader is None:
        raise ModuleNotFoundError(f"Unable to load local module 'core' from '{module_path}'")

    module = importlib.util.module_from_spec(spec)
    sys.modules["core"] = module
    spec.loader.exec_module(module)
    return module


try:
    from core import MaterialDef, VoxelGrid, ensure_collection, ensure_palette, ensure_scene, focus_collection, spawn_asset
except (ModuleNotFoundError, ImportError):
    core = _load_core()
    MaterialDef = core.MaterialDef
    VoxelGrid = core.VoxelGrid
    ensure_collection = core.ensure_collection
    ensure_palette = core.ensure_palette
    ensure_scene = core.ensure_scene
    focus_collection = core.focus_collection
    spawn_asset = core.spawn_asset

ROCK = 0
MOSS = 1
WET = 2

PALETTE = [
    MaterialDef("Rock", (0.15, 0.17, 0.18), roughness=0.96),
    MaterialDef("Moss", (0.10, 0.18, 0.16), roughness=0.90),
    MaterialDef("Wet", (0.07, 0.17, 0.22), roughness=0.16, emission=0.22),
]

ASSET_DEFS = [
    {"name": "SM_VX_Rock_BoulderA", "kind": "boulder_a", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_Rock_BoulderB", "kind": "boulder_b", "offset": (140.0, 0.0, 0.0)},
    {"name": "SM_VX_Rock_ShelfA", "kind": "shelf_a", "offset": (320.0, 0.0, 0.0)},
    {"name": "SM_VX_Rock_ShelfB", "kind": "shelf_b", "offset": (560.0, 0.0, 0.0)},
    {"name": "SM_VX_Rock_NeedleA", "kind": "needle_a", "offset": (0.0, 220.0, 0.0)},
    {"name": "SM_VX_Rock_NeedleB", "kind": "needle_b", "offset": (160.0, 220.0, 0.0)},
    {"name": "SM_VX_Rock_WallChunkA", "kind": "wall_a", "offset": (320.0, 220.0, 0.0)},
    {"name": "SM_VX_Rock_FumaroleA", "kind": "fumarole", "offset": (620.0, 220.0, 0.0)},
]


def _blob(offsets):
    body = VoxelGrid()
    detail = VoxelGrid()
    for ox, oy, oz, rx, ry, rz in offsets:
        body.fill_sphere(ox, oy, oz, rx, ry, rz, ROCK)
    return body, detail


def _shelf(wide: bool = False):
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-14, -10, 0, 14, 10, 12, ROCK)
    body.fill_box(-24 if wide else -18, -6, 10, 24 if wide else 18, 6, 18, ROCK)
    body.fill_box(-28 if wide else -20, -4, 16, 28 if wide else 20, 4, 20, MOSS)
    return body, detail


def _needle(height: int):
    body = VoxelGrid()
    detail = VoxelGrid()
    for z in range(height + 1):
        radius = max(1, int(round(6 * (1.0 - z / float(max(height, 1))))))
        body.fill_ellipse_z(z, 0, 0, radius + 1, radius, ROCK)
    return body, detail


def _wall():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-18, -6, 0, 18, 6, 30, ROCK)
    body.fill_box(-8, -8, 10, 8, 8, 22, MOSS)
    body.fill_box(-2, -2, 12, 2, 2, 18, WET)
    return body, detail


def _fumarole():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-10, -10, 0, 10, 10, 12, ROCK)
    body.carve_box(-3, -3, 8, 3, 3, 12)
    body.fill_box(-2, -2, 6, 2, 2, 8, WET)
    body.fill_box(-12, -2, 4, -2, 2, 9, MOSS)
    body.fill_box(2, -2, 4, 12, 2, 9, MOSS)
    return body, detail


BUILDERS = {
    "boulder_a": lambda: _blob([(0, 0, 7, 8, 6, 7), (6, -2, 10, 5, 4, 5), (-6, 3, 9, 4, 4, 4)]),
    "boulder_b": lambda: _blob([(0, 0, 8, 10, 7, 6), (7, 4, 10, 4, 4, 4), (-7, -4, 9, 4, 3, 4)]),
    "shelf_a": _shelf,
    "shelf_b": lambda: _shelf(True),
    "needle_a": lambda: _needle(26),
    "needle_b": lambda: _needle(34),
    "wall_a": _wall,
    "fumarole": _fumarole,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Rock_Formations", parent_collection)
    materials = ensure_palette("VXRock", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Rock_Formations] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

