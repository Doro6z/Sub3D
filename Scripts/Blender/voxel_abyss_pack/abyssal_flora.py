"""
Sub3D - Voxel Abyss Pack - Abyssal Flora
========================================
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

STEM = 0
TIP = 1
SHELL = 2
GLOW = 3
MOSS = 4

PALETTE = [
    MaterialDef("Stem", (0.12, 0.22, 0.18), roughness=0.88),
    MaterialDef("Tip", (0.65, 0.16, 0.12), roughness=0.74),
    MaterialDef("Shell", (0.34, 0.34, 0.30), roughness=0.80),
    MaterialDef("Glow", (0.08, 0.84, 0.92), roughness=0.16, emission=4.8),
    MaterialDef("Moss", (0.08, 0.18, 0.14), roughness=0.92),
]

ASSET_DEFS = [
    {"name": "SM_VX_Flora_TubewormPatch", "kind": "tubeworms", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_Flora_FanCoral", "kind": "fan", "offset": (160.0, 0.0, 0.0)},
    {"name": "SM_VX_Flora_BambooCoral", "kind": "bamboo", "offset": (340.0, 0.0, 0.0)},
    {"name": "SM_VX_Flora_SeaPen", "kind": "pen", "offset": (500.0, 0.0, 0.0)},
    {"name": "SM_VX_Flora_VentReeds", "kind": "reeds", "offset": (640.0, 0.0, 0.0)},
    {"name": "SM_VX_Flora_SporePalm", "kind": "palm", "offset": (0.0, 220.0, 0.0)},
    {"name": "SM_VX_Flora_BulbAnemone", "kind": "anemone", "offset": (180.0, 220.0, 0.0)},
    {"name": "SM_VX_Flora_LanternKelp", "kind": "kelp", "offset": (360.0, 220.0, 0.0)},
    {"name": "SM_VX_Flora_BoneMoss", "kind": "moss", "offset": (540.0, 220.0, 0.0)},
    {"name": "SM_VX_Flora_BlackCoralShrub", "kind": "shrub", "offset": (700.0, 220.0, 0.0)},
]


def _tubeworms():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for x, y, h in [(-6, -4, 18), (-2, 2, 22), (4, -1, 20), (7, 5, 16), (-8, 5, 14)]:
        body.fill_box(x, y, 0, x + 1, y + 1, h, STEM)
        body.fill_box(x, y, h, x + 1, y + 1, h + 3, TIP)
    return body, detail


def _fan():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(0, -1, 0, 2, 1, 12, STEM)
    for z in range(10, 30):
        reach = max(2, int((z - 8) * 0.7))
        body.fill_box(0, -reach, z, 0, reach, z, SHELL)
    return body, detail


def _bamboo():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for z in range(0, 28):
        body.fill_box(-1, -1, z, 1, 1, z, STEM)
        if z % 6 == 0:
            body.fill_box(-2, -2, z, 2, 2, z + 1, SHELL)
    for branch in [10, 18, 24]:
        detail.fill_polyline([(0, 0, branch * 3), (18, 9, (branch + 6) * 3), (30, 16, (branch + 8) * 3)], 2, GLOW)
        detail.fill_polyline([(0, 0, branch * 3), (-16, -10, (branch + 5) * 3), (-26, -16, (branch + 7) * 3)], 2, GLOW)
    return body, detail


def _pen():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(-1, -1, 0, 1, 1, 18, STEM)
    for z in range(8, 24, 2):
        width = max(2, (24 - z) // 2)
        body.fill_box(-width, -1, z, width, 1, z, TIP)
    return body, detail


def _reeds():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    for x, y, h in [(-7, -2, 24), (-4, 4, 22), (0, 0, 28), (5, -3, 20), (8, 5, 26)]:
        detail.fill_polyline([(x * 3, y * 3, 0), (x * 3, y * 3, h * 2), ((x + 2) * 3, (y + 2) * 3, h * 3)], 2, STEM)
        detail.fill_segment(((x + 2) * 3, (y + 2) * 3, h * 3), ((x + 3) * 3, (y + 3) * 3, h * 3 + 4), 2, GLOW)
    return body, detail


def _palm():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(-2, -2, 0, 2, 2, 18, STEM)
    for angle, vec in [(1, (18, 0)), (2, (12, 14)), (3, (-6, 16)), (4, (-16, -2)), (5, (0, -18))]:
        detail.fill_polyline([(0, 0, 54), (vec[0], vec[1], 66), (vec[0] + 12, vec[1], 69)], 2, GLOW)
    return body, detail


def _anemone():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_sphere(0, 0, 8, 8, 8, 6, SHELL)
    for vec in [(18, 0), (12, 12), (0, 18), (-12, 12), (-18, 0), (-12, -12), (0, -18), (12, -12)]:
        detail.fill_polyline([(0, 0, 24), (vec[0], vec[1], 36), (vec[0] + vec[0] // 2, vec[1] + vec[1] // 2, 42)], 3, GLOW)
    return body, detail


def _kelp():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    detail.fill_polyline([(0, 0, 0), (0, 0, 32), (6, 3, 60), (12, 6, 78)], 3, STEM)
    for z in [18, 34, 50, 66]:
        body.fill_box(-6, -2, z, 6, 2, z + 6, MOSS)
    return body, detail


def _moss():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(-14, -10, 0, 14, 10, 4, MOSS)
    for x in range(-12, 13, 4):
        for y in range(-8, 9, 4):
            body.fill_box(x, y, 4, x + 1, y + 1, 8, SHELL)
    return body, detail


def _shrub():
    body = VoxelGrid()
    detail = VoxelGrid(DETAIL_VOXEL)
    body.fill_box(-2, -2, 0, 2, 2, 14, STEM)
    for branch in [8, 12, 16]:
        detail.fill_polyline([(0, 0, branch * 3), (12, 9, branch * 3 + 12), (18, 15, branch * 3 + 18)], 2, SHELL)
        detail.fill_polyline([(0, 0, branch * 3), (-10, -8, branch * 3 + 10), (-18, -14, branch * 3 + 16)], 2, SHELL)
    body.fill_box(-6, -6, 14, 6, 6, 18, GLOW)
    return body, detail


BUILDERS = {
    "tubeworms": _tubeworms,
    "fan": _fan,
    "bamboo": _bamboo,
    "pen": _pen,
    "reeds": _reeds,
    "palm": _palm,
    "anemone": _anemone,
    "kelp": _kelp,
    "moss": _moss,
    "shrub": _shrub,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Abyssal_Flora", parent_collection)
    materials = ensure_palette("VXFlora", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Abyssal_Flora] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

