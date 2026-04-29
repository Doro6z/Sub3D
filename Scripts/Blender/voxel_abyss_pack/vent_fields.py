"""
Sub3D - Voxel Abyss Pack - Vent Fields
======================================
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
SULFUR = 1
SMOKE = 2
GLOW = 3
WET = 4

PALETTE = [
    MaterialDef("Rock", (0.16, 0.17, 0.18), roughness=0.96),
    MaterialDef("Sulfur", (0.58, 0.50, 0.12), roughness=0.72),
    MaterialDef("Smoke", (0.34, 0.34, 0.35), roughness=0.92),
    MaterialDef("Glow", (0.08, 0.82, 0.90), roughness=0.12, emission=4.6),
    MaterialDef("Wet", (0.08, 0.18, 0.24), roughness=0.16),
]

ASSET_DEFS = [
    {"name": "SM_VX_Vent_BlackSmoker", "kind": "black", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_Vent_WhiteSmoker", "kind": "white", "offset": (160.0, 0.0, 0.0)},
    {"name": "SM_VX_Vent_ColdSeep", "kind": "seep", "offset": (320.0, 0.0, 0.0)},
    {"name": "SM_VX_Vent_SulfurMound", "kind": "mound", "offset": (500.0, 0.0, 0.0)},
    {"name": "SM_VX_Vent_BubbleSpire", "kind": "bubble", "offset": (680.0, 0.0, 0.0)},
    {"name": "SM_VX_Vent_ChimneyBroken", "kind": "broken", "offset": (0.0, 220.0, 0.0)},
    {"name": "SM_VX_Vent_ChimneyRing", "kind": "ring", "offset": (180.0, 220.0, 0.0)},
    {"name": "SM_VX_Vent_ChemMat", "kind": "mat", "offset": (360.0, 220.0, 0.0)},
]


def _chimney(height: int, sulfur: bool = False, broken: bool = False):
    body = VoxelGrid()
    detail = VoxelGrid()
    for z in range(height + 1):
        radius = max(2, int(round(7 * (1.0 - z / float(max(height, 1))))))
        body.fill_ellipse_z(z, 0, 0, radius, radius, ROCK if not sulfur else SULFUR)
    body.fill_box(-2, -2, height, 2, 2, height + 10, SMOKE)
    if broken:
        body.carve_box(-4, -4, height // 2, 4, 4, height // 2 + 6)
        body.fill_box(-6, -6, height // 2 - 2, 6, 6, height // 2, SMOKE)
    return body, detail


def _seep():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-12, -12, 0, 12, 12, 6, ROCK)
    body.fill_box(-6, -6, 4, 6, 6, 8, WET)
    body.fill_box(-4, -4, 8, 4, 4, 12, GLOW)
    return body, detail


def _mound():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_sphere(0, 0, 8, 10, 10, 6, SULFUR)
    body.fill_box(-2, -2, 6, 2, 2, 14, GLOW)
    return body, detail


def _bubble():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-4, -4, 0, 4, 4, 24, ROCK)
    body.fill_box(-2, -2, 24, 2, 2, 30, GLOW)
    body.fill_box(-8, -8, 0, 8, 8, 4, WET)
    return body, detail


def _ring():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-14, -14, 0, 14, 14, 4, ROCK)
    body.carve_box(-8, -8, 0, 8, 8, 4)
    body.fill_box(-10, -10, 4, 10, 10, 8, SULFUR)
    body.carve_box(-6, -6, 4, 6, 6, 8)
    return body, detail


def _mat():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-18, -12, 0, 18, 12, 2, GLOW)
    body.fill_box(-10, -8, 2, 10, 8, 4, WET)
    return body, detail


BUILDERS = {
    "black": lambda: _chimney(30),
    "white": lambda: _chimney(24, sulfur=True),
    "seep": _seep,
    "mound": _mound,
    "bubble": _bubble,
    "broken": lambda: _chimney(26, broken=True),
    "ring": _ring,
    "mat": _mat,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Vent_Fields", parent_collection)
    materials = ensure_palette("VXVent", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Vent_Fields] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

