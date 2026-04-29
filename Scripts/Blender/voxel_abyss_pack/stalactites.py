"""
Sub3D - Voxel Abyss Pack - Stalactites
======================================
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
WET = 1
SHELL = 2

PALETTE = [
    MaterialDef("Rock", (0.17, 0.18, 0.19), roughness=0.95),
    MaterialDef("Wet", (0.08, 0.17, 0.22), roughness=0.18, emission=0.2),
    MaterialDef("Shell", (0.28, 0.27, 0.24), roughness=0.80),
]

ASSET_DEFS = [
    {"name": "SM_VX_Stalactite_LongA", "kind": "long_a", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalactite_LongB", "kind": "long_b", "offset": (120.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalactite_ClusterA", "kind": "cluster_a", "offset": (240.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalactite_ClusterB", "kind": "cluster_b", "offset": (380.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalactite_BrokenA", "kind": "broken_a", "offset": (540.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalactite_BrokenB", "kind": "broken_b", "offset": (680.0, 0.0, 0.0)},
    {"name": "SM_VX_Stalagmite_A", "kind": "mite_a", "offset": (0.0, 180.0, 0.0)},
    {"name": "SM_VX_Stalagmite_B", "kind": "mite_b", "offset": (120.0, 180.0, 0.0)},
    {"name": "SM_VX_RoofFang_A", "kind": "fang_a", "offset": (260.0, 180.0, 0.0)},
    {"name": "SM_VX_RoofFang_B", "kind": "fang_b", "offset": (400.0, 180.0, 0.0)},
]


def _spire(height: int, radius: int, invert: bool = True, broken: bool = False, cluster: int = 0):
    body = VoxelGrid()
    detail = VoxelGrid()
    z_range = range(0, height + 1)
    for z in z_range:
        t = z / float(max(height, 1))
        r = max(1, int(round(radius * (1.0 - t))))
        target_z = height - z if invert else z
        body.fill_ellipse_z(target_z, 0, 0, r + cluster, r, ROCK)
        if z % 5 == 0:
            body.fill_ellipse_z(target_z, 0, 0, max(1, r - 1), max(1, r - 1), WET)
    if broken:
        body.carve_box(-6, -6, height // 2 - 2, 6, 6, height // 2 + 4)
        body.fill_box(-8, -8, height // 2 - 1, 8, 8, height // 2 + 1, SHELL)
    return body, detail


def _cluster(offsets):
    body = VoxelGrid()
    detail = VoxelGrid()
    for ox, oy, height, radius in offsets:
        piece, _ = _spire(height, radius, invert=True, cluster=1)
        body.stamp(piece, (ox, oy, 0))
    return body, detail


BUILDERS = {
    "long_a": lambda: _spire(40, 9),
    "long_b": lambda: _spire(48, 8),
    "cluster_a": lambda: _cluster([(0, 0, 26, 5), (-8, -6, 20, 4), (8, 7, 18, 4)]),
    "cluster_b": lambda: _cluster([(0, 0, 30, 6), (-10, 4, 22, 4), (9, -7, 24, 5), (4, 12, 16, 3)]),
    "broken_a": lambda: _spire(34, 8, broken=True),
    "broken_b": lambda: _spire(28, 10, broken=True),
    "mite_a": lambda: _spire(26, 8, invert=False),
    "mite_b": lambda: _spire(30, 6, invert=False),
    "fang_a": lambda: _spire(20, 12),
    "fang_b": lambda: _cluster([(0, 0, 22, 5), (-6, 8, 14, 3), (7, -6, 18, 4)]),
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Stalactites", parent_collection)
    materials = ensure_palette("VXStal", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Stalactites] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

