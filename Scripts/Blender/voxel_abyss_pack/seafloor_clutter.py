"""
Sub3D - Voxel Abyss Pack - Seafloor Clutter
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

STEEL = 0
RUST = 1
SHELL = 2
MOSS = 3
GLOW = 4

PALETTE = [
    MaterialDef("Steel", (0.28, 0.31, 0.34), metallic=0.28, roughness=0.46),
    MaterialDef("Rust", (0.35, 0.18, 0.11), roughness=0.86),
    MaterialDef("Shell", (0.31, 0.31, 0.28), roughness=0.82),
    MaterialDef("Moss", (0.08, 0.16, 0.12), roughness=0.92),
    MaterialDef("Glow", (0.09, 0.78, 0.90), roughness=0.14, emission=4.3),
]

ASSET_DEFS = [
    {"name": "SM_VX_WreckPlate", "kind": "plate", "offset": (0.0, 0.0, 0.0)},
    {"name": "SM_VX_ChainNest", "kind": "chain", "offset": (180.0, 0.0, 0.0)},
    {"name": "SM_VX_AnchorBone", "kind": "anchor", "offset": (340.0, 0.0, 0.0)},
    {"name": "SM_VX_PipeDebris", "kind": "pipe", "offset": (520.0, 0.0, 0.0)},
    {"name": "SM_VX_EggCluster", "kind": "eggs", "offset": (0.0, 160.0, 0.0)},
    {"name": "SM_VX_ShellPile", "kind": "shells", "offset": (160.0, 160.0, 0.0)},
    {"name": "SM_VX_CrateRemains", "kind": "crate", "offset": (340.0, 160.0, 0.0)},
    {"name": "SM_VX_RustedPanelGarden", "kind": "garden", "offset": (540.0, 160.0, 0.0)},
]


def _plate():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-18, -10, 0, 18, 10, 3, STEEL)
    body.fill_box(-12, -6, 3, 12, 6, 5, RUST)
    body.fill_box(-4, -2, 5, 4, 2, 6, GLOW)
    return body, detail


def _chain():
    body = VoxelGrid()
    detail = VoxelGrid()
    for x in range(-12, 13, 6):
        body.fill_box(x, -3, 0, x + 4, 3, 4, STEEL)
    body.fill_box(14, -3, 0, 18, 9, 4, RUST)
    return body, detail


def _anchor():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-2, -2, 0, 2, 2, 22, SHELL)
    body.fill_box(-12, -2, 4, 12, 2, 8, SHELL)
    body.fill_box(-12, -2, 0, -8, 2, 12, SHELL)
    body.fill_box(8, -2, 0, 12, 2, 12, SHELL)
    return body, detail


def _pipe():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-16, -4, 0, 16, 4, 6, STEEL)
    body.carve_box(-10, -2, 1, -2, 2, 5)
    body.carve_box(4, -2, 1, 12, 2, 5)
    body.fill_box(-16, -4, 0, -6, 4, 6, RUST)
    body.fill_box(8, -4, 0, 16, 4, 6, RUST)
    return body, detail


def _eggs():
    body = VoxelGrid()
    detail = VoxelGrid()
    for offset in [(-8, -4, 6), (-2, 2, 7), (4, -1, 8), (10, 4, 6), (0, -8, 5)]:
        body.fill_sphere(offset[0], offset[1], offset[2], 4, 4, 4, GLOW)
    return body, detail


def _shells():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-12, -8, 0, 12, 8, 2, MOSS)
    for offset in [(-10, -4, 2), (-2, 3, 2), (6, -3, 2), (10, 4, 2)]:
        body.fill_box(offset[0], offset[1], offset[2], offset[0] + 6, offset[1] + 4, offset[2] + 3, SHELL)
    return body, detail


def _crate():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-10, -10, 0, 10, 10, 10, STEEL)
    body.carve_box(-6, -6, 2, 6, 6, 8)
    body.fill_box(-10, -10, 0, -8, 10, 10, RUST)
    body.fill_box(8, -10, 0, 10, 10, 10, RUST)
    return body, detail


def _garden():
    body = VoxelGrid()
    detail = VoxelGrid()
    body.fill_box(-16, -10, 0, 16, 10, 3, RUST)
    for x in range(-12, 13, 6):
        body.fill_box(x, -2, 3, x + 2, 2, 11, MOSS)
        body.fill_box(x, -1, 11, x + 2, 1, 13, GLOW)
    return body, detail


BUILDERS = {
    "plate": _plate,
    "chain": _chain,
    "anchor": _anchor,
    "pipe": _pipe,
    "eggs": _eggs,
    "shells": _shells,
    "crate": _crate,
    "garden": _garden,
}


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)
    collection = ensure_collection("VX_Seafloor_Clutter", parent_collection)
    materials = ensure_palette("VXClutter", PALETTE)
    built = []
    for spec in ASSET_DEFS:
        coarse, detail = BUILDERS[spec["kind"]]()
        ox, oy, oz = spec["offset"]
        location = (origin[0] + ox, origin[1] + oy, origin[2] + oz)
        spawn_asset(spec["name"], [coarse, detail], materials, collection, location)
        built.append(spec["name"])
    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Seafloor_Clutter] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()

