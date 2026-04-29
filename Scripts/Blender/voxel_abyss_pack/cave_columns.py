"""
Sub3D - Voxel Abyss Pack - Cave Columns and Arches
==================================================
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
SHELL = 2
WET = 3

PALETTE = [
    MaterialDef("Rock", (0.16, 0.18, 0.20), roughness=0.94),
    MaterialDef("Moss", (0.08, 0.18, 0.16), roughness=0.90),
    MaterialDef("Shell", (0.32, 0.30, 0.26), roughness=0.82),
    MaterialDef("Wet", (0.06, 0.16, 0.22), roughness=0.20, emission=0.25),
]

ASSET_DEFS = [
    {"name": "SM_VX_CaveArch_Tight", "kind": "tight", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_CaveArch_Wide", "kind": "wide", "offset": (240.0, 0.0, 0.0)},
    {"name": "SM_VX_CaveArch_SplitPillar", "kind": "split", "offset": (560.0, 0.0, 0.0)},
    {"name": "SM_VX_CaveArch_ToothGate", "kind": "tooth", "offset": (840.0, 0.0, 0.0)},
    {"name": "SM_VX_CaveArch_RibTunnel", "kind": "rib", "offset": (0.0, 320.0, 0.0)},
    {"name": "SM_VX_CaveArch_Buttress", "kind": "buttress", "offset": (280.0, 320.0, 0.0)},
    {"name": "SM_VX_CaveArch_ShelfSpine", "kind": "shelf", "offset": (560.0, 320.0, 0.0)},
    {"name": "SM_VX_CaveArch_CathedralColumn", "kind": "cathedral", "offset": (860.0, 320.0, 0.0)},
]


def _arch(span: int, height: int, thickness: int, fang: bool = False):
    body = VoxelGrid()
    detail = VoxelGrid()
    for y in range(-span, span + 1):
        t = abs(y) / float(max(span, 1))
        z_top = int(round(height * math.cos(t * math.pi * 0.5)))
        x_half = max(4, thickness + int((1.0 - t) * thickness))
        body.fill_box(-x_half, y, 0, x_half, y, z_top, ROCK)
        if y % 4 == 0:
            body.fill_box(-x_half, y, z_top, x_half, y, z_top + 1, MOSS)
        if fang and z_top > height * 0.65:
            body.fill_box(-1, y, z_top - 8, 1, y, z_top - 1, SHELL)
    return body, detail


def _split():
    body, detail = _arch(22, 28, 6)
    body.carve_box(-4, -2, 0, 4, 2, 24)
    body.fill_box(-10, -1, 14, -4, 1, 18, WET)
    body.fill_box(4, -1, 14, 10, 1, 18, WET)
    return body, detail


def _rib():
    body = VoxelGrid()
    detail = VoxelGrid()
    for segment in range(6):
        base = segment * 10
        for y in range(-10, 11):
            z_top = int(round(18 * math.cos(abs(y) / 10.0 * math.pi * 0.5)))
            body.fill_box(base, y, 0, base + 2, y, z_top, ROCK)
    return body, detail


def _buttress():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-10, -10, 0, 10, 10, 28, ROCK)
    body.fill_box(-18, -6, 0, -10, 6, 20, ROCK)
    body.fill_box(10, -6, 0, 18, 6, 20, ROCK)
    body.fill_box(-4, -4, 18, 4, 4, 34, MOSS)
    return body, detail


def _shelf():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-12, -8, 0, 12, 8, 14, ROCK)
    body.fill_box(-22, -5, 12, 22, 5, 20, ROCK)
    body.fill_box(-28, -3, 18, 28, 3, 22, MOSS)
    return body, detail


def _cathedral():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-12, -12, 0, 12, 12, 36, ROCK)
    body.fill_box(-18, -6, 24, 18, 6, 30, ROCK)
    for z in range(6, 37, 6):
        body.fill_box(-13, -13, z, 13, 13, z + 1, SHELL if z > 24 else MOSS)
    return body, detail


BUILDERS = {
    "tight": lambda: _arch(16, 26, 5),
    "wide": lambda: _arch(28, 24, 6),
    "split": _split,
    "tooth": lambda: _arch(20, 30, 6, fang=True),
    "rib": _rib,
    "buttress": _buttress,
    "shelf": _shelf,
    "cathedral": _cathedral,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Cave_Columns", parent_collection)
    materials = ensure_palette("VXCave", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Cave_Columns] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

