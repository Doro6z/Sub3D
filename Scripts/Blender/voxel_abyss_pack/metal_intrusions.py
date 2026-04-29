"""
Sub3D - Voxel Abyss Pack - Metal Intrusions
===========================================
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
    from core import DETAIL_VOXEL, MaterialDef, VoxelGrid, ensure_collection, ensure_palette, ensure_scene, focus_collection, spawn_asset
except (ModuleNotFoundError, ImportError):
    core = _load_core()
    DETAIL_VOXEL = core.DETAIL_VOXEL
    MaterialDef = core.MaterialDef
    VoxelGrid = core.VoxelGrid
    ensure_collection = core.ensure_collection
    ensure_palette = core.ensure_palette
    ensure_scene = core.ensure_scene
    focus_collection = core.focus_collection
    spawn_asset = core.spawn_asset

STEEL = 0
RUST = 1
GLOW = 2
WIRE = 3

PALETTE = [
    MaterialDef("Steel", (0.32, 0.36, 0.39), metallic=0.42, roughness=0.40),
    MaterialDef("Rust", (0.35, 0.18, 0.10), roughness=0.88),
    MaterialDef("Glow", (0.07, 0.72, 0.85), roughness=0.12, emission=3.8),
    MaterialDef("Wire", (0.48, 0.50, 0.52), metallic=0.54, roughness=0.35),
]

ASSET_DEFS = [
    {"name": "SM_VX_MetalLance_Straight", "kind": "straight", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_MetalLance_Twisted", "kind": "twisted", "offset": (140.0, 0.0, 0.0)},
    {"name": "SM_VX_MetalLance_Crossbrace", "kind": "cross", "offset": (320.0, 0.0, 0.0)},
    {"name": "SM_VX_MetalLance_BuriedTruss", "kind": "truss", "offset": (520.0, 0.0, 0.0)},
    {"name": "SM_VX_MetalLance_RoofSpears", "kind": "roof", "offset": (0.0, 220.0, 0.0)},
    {"name": "SM_VX_MetalLance_JaggedPile", "kind": "pile", "offset": (220.0, 220.0, 0.0)},
]


def _lance(length: int, width: int, twist: bool = False):
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for x in range(length + 1):
        y = 0 if not twist else int(round((x / max(length, 1)) * 6))
        body.fill_box(x, y - width, 0, x, y + width, 4, STEEL if x < length - 4 else RUST)
    detail.fill_polyline([(0, 0, 12), (length * 3, 0 if not twist else 18, 18)], 2, WIRE)
    return body, detail


def _cross():
    body, detail = _lance(34, 3)
    brace, _ = _lance(22, 2)
    body.stamp(brace, (8, -8, 6))
    body.stamp(brace, (8, 8, 6))
    return body, detail


def _truss():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(0, -4, 0, 30, 4, 6, STEEL)
    body.fill_box(6, -8, 6, 24, 8, 10, RUST)
    for x in range(0, 31, 6):
        body.fill_box(x, -6, 6, x + 1, 6, 16, STEEL)
    detail.fill_polyline([(0, -12, 9), (45, 0, 27), (90, 12, 39)], 2, WIRE)
    detail.fill_polyline([(0, 12, 9), (45, 0, 27), (90, -12, 39)], 2, WIRE)
    return body, detail


def _roof():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for index, x in enumerate(range(0, 28, 6)):
        length = 22 + index * 4
        spear, _ = _lance(length, 2 + index % 2)
        body.stamp(spear, (x, index * 3, 0))
    return body, detail


def _pile():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for index, offset in enumerate([(0, 0, 0), (8, -6, 4), (12, 8, 2), (18, -2, 6)]):
        spear, _ = _lance(24 + index * 4, 2 + index % 2, twist=index % 2 == 1)
        body.stamp(spear, offset)
    detail.fill_polyline([(24, -18, 18), (50, -6, 26), (84, 6, 31)], 2, GLOW)
    return body, detail


BUILDERS = {
    "straight": lambda: _lance(44, 3),
    "twisted": lambda: _lance(40, 3, twist=True),
    "cross": _cross,
    "truss": _truss,
    "roof": _roof,
    "pile": _pile,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Metal_Intrusions", parent_collection)
    materials = ensure_palette("VXMetal", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Metal_Intrusions] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

