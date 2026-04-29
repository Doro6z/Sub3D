"""
Sub3D - Voxel Abyss Pack - Giant Mobs
=====================================

Large fauna and leviathans.
Includes mega sea-worm / serpent variants around 10 m, 20 m, 30 m.
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


BODY = 0
BELLY = 1
PLATE = 2
EYE = 3
GLOW = 4
BONE = 5

PALETTE = [
    MaterialDef("Body", (0.05, 0.10, 0.12), roughness=0.86),
    MaterialDef("Belly", (0.19, 0.23, 0.22), roughness=0.92),
    MaterialDef("Plate", (0.16, 0.18, 0.19), roughness=0.66, metallic=0.10),
    MaterialDef("Eye", (0.88, 0.93, 0.88), roughness=0.10, emission=0.8),
    MaterialDef("Glow", (0.05, 0.78, 0.90), roughness=0.14, emission=4.8),
    MaterialDef("Bone", (0.70, 0.68, 0.58), roughness=0.58),
]


def _m_to_vox(length_m: float, voxel_cm: float) -> int:
    return max(20, int(round((length_m * 100.0) / voxel_cm)))


ASSET_DEFS = [
    {"name": "SM_VX_GigaVer_6m", "kind": "worm", "length_m": 6.0, "voxel_cm": 10.0},
    {"name": "SM_VX_GigaVer_12m", "kind": "worm", "length_m": 12.0, "voxel_cm": 15.0},
    {"name": "SM_VX_GigaVer_24m", "kind": "worm", "length_m": 24.0, "voxel_cm": 20.0},
    {"name": "SM_VX_MegaSerpent_10m", "kind": "serpent", "length_m": 10.0, "voxel_cm": 10.0},
    {"name": "SM_VX_MegaSerpent_20m", "kind": "serpent", "length_m": 20.0, "voxel_cm": 20.0},
    {"name": "SM_VX_MegaSerpent_30m", "kind": "serpent", "length_m": 30.0, "voxel_cm": 25.0},
    {"name": "SM_VX_SeaWorm_15m", "kind": "worm", "length_m": 15.0, "voxel_cm": 15.0},
    {"name": "SM_VX_TrenchTitan_8m", "kind": "walker", "length_m": 8.0, "voxel_cm": 12.0},
    {"name": "SM_VX_TrenchTitan_14m", "kind": "walker", "length_m": 14.0, "voxel_cm": 15.0},
    {"name": "SM_VX_ChoirWhale_12m", "kind": "whale", "length_m": 12.0, "voxel_cm": 15.0},
    {"name": "SM_VX_ChoirWhale_20m", "kind": "whale", "length_m": 20.0, "voxel_cm": 20.0},
    {"name": "SM_VX_RookLeviathan_10m", "kind": "angler", "length_m": 10.0, "voxel_cm": 12.0},
    {"name": "SM_VX_RookLeviathan_18m", "kind": "angler", "length_m": 18.0, "voxel_cm": 20.0},
    {"name": "SM_VX_SpineCathedral_9m", "kind": "seraph", "length_m": 9.0, "voxel_cm": 12.0},
    {"name": "SM_VX_SpineCathedral_16m", "kind": "seraph", "length_m": 16.0, "voxel_cm": 16.0},
    {"name": "SM_VX_MirrorKraken_11m", "kind": "kraken", "length_m": 11.0, "voxel_cm": 14.0},
    {"name": "SM_VX_MirrorKraken_19m", "kind": "kraken", "length_m": 19.0, "voxel_cm": 20.0},
]


def _build_giant(spec):
    voxel_cm = float(spec["voxel_cm"])
    detail_voxel = voxel_cm / 3.0
    fine = max(1, int(round(voxel_cm / detail_voxel)))

    length = _m_to_vox(spec["length_m"], voxel_cm)
    width = max(6, int(round(length * 0.12)))
    height = max(6, int(round(length * 0.09)))

    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(detail_voxel)

    knots = [
        (0, max(3, width // 3), max(3, height // 3), 8),
        (int(length * 0.12), width // 2, height // 2, 9),
        (int(length * 0.40), width, height, 10),
        (int(length * 0.72), max(4, width - 1), max(4, height - 2), 9),
        (length, 2, 2, 7),
    ]

    if spec["kind"] in ("worm", "serpent"):
        knots[1] = (int(length * 0.15), width, height, 8)
        knots[3] = (int(length * 0.82), width, max(4, height - 1), 8)
    elif spec["kind"] == "whale":
        knots[2] = (int(length * 0.52), width + 2, height + 1, 11)
    elif spec["kind"] == "seraph":
        knots[2] = (int(length * 0.48), width - 1, height + 3, 13)

    for x in range(length + 1):
        ry, rz, zc = interpolate_knots(x, knots)
        zc += int(round(math.sin((x / max(length, 1)) * math.pi) * 2.0))
        body.fill_ellipse_x(x, 0, zc, ry, rz, BODY)
        if ry > 2:
            body.fill_ellipse_x(x, 0, zc - rz, max(1, ry - 2), 1, BELLY)
        if x % 5 == 0:
            body.set(x, 0, zc + rz + 1, PLATE)

    if spec["kind"] in ("worm", "serpent"):
        for x in range(8, length - 6, max(5, int(length * 0.04))):
            for side in (-1, 1):
                detail.fill_polyline([(x * fine, side * (width * fine), 18 * fine), ((x + 4) * fine, side * ((width + 6) * fine), 12 * fine)], 2, GLOW)
        for x in range(length - 18, length + 1):
            ring = (x - (length - 18)) % 4
            body.fill_box(x, -width - 1, 7, x, width + 1, 7 + ring, BONE)
        body.carve_box(length - 6, -4, 8, length, 4, 12)
        body.fill_box(length - 4, -5, 7, length, 5, 8, GLOW)

    if spec["kind"] == "walker":
        for side in (-1, 1):
            stride = max(8, int(length * 0.12))
            for x in range(8, length - 10, stride):
                y = side * (width + 1)
                y2 = side * (width + 8 + (x // stride) % 3)
                detail.fill_polyline([(x * fine, y * fine, 24 * fine), ((x + 3) * fine, y2 * fine, 15 * fine), ((x + 5) * fine, (y2 + side * 4) * fine, 6 * fine)], 3, PLATE)
        body.fill_box(length - 14, -5, 10, length - 4, 5, 18, BONE)

    if spec["kind"] == "whale":
        for x in range(int(length * 0.25), int(length * 0.65)):
            span = 4 + int(8 * math.sin((x - length * 0.25) / max(length * 0.4, 1.0) * math.pi))
            body.fill_box(x, -width - span, 8, x, -width, 12 + span // 2, PLATE)
            body.fill_box(x, width, 8, x, width + span, 12 + span // 2, PLATE)
        for x in range(length - 10, length + 1):
            tail = x - (length - 10)
            body.fill_box(x, -3 - tail, 7 - tail // 2, x, 3 + tail, 7 + tail // 2, PLATE)
        for side in (-1, 1):
            body.fill_box(length - 18, side * width, 11, length - 10, side * (width + 2), 14, EYE)

    if spec["kind"] == "angler":
        body.fill_box(length - 22, -6, 9, length - 6, 6, 18, PLATE)
        detail.fill_polyline([((length - 16) * fine, 0, 45 * fine), ((length - 6) * fine, 0, 60 * fine), ((length + 8) * fine, 0, 72 * fine)], 3, BONE)
        detail.fill_segment(((length + 8) * fine, 0, 72 * fine), ((length + 14) * fine, 0, 75 * fine), 4, GLOW)
        for x in range(18, length - 8, max(6, int(length * 0.08))):
            body.set(x, width, 11, GLOW)
            body.set(x, -width, 11, GLOW)

    if spec["kind"] == "seraph":
        for x in range(16, length - 10, max(6, int(length * 0.1))):
            height_extra = 4 + (x % 3)
            body.fill_box(x, -1, 18, x + 1, 1, 18 + height_extra, BONE)
        for side in (-1, 1):
            for wing_x in range(int(length * 0.2), int(length * 0.7)):
                reach = 6 + int(7 * math.sin((wing_x - length * 0.2) / max(length * 0.5, 1.0) * math.pi))
                body.fill_box(wing_x, side * width, 12, wing_x, side * (width + reach), 15 + reach // 2, PLATE)

    if spec["kind"] == "kraken":
        body.fill_box(18, -8, 8, 44, 8, 19, PLATE)
        for side in (-1, 1):
            for index in range(4):
                base_y = side * (8 - index * 2)
                detail.fill_polyline(
                    [
                        (22 * fine, base_y * fine, 27 * fine),
                        ((12 - index * 2) * fine, (base_y + side * (8 + index * 3)) * fine, 18 * fine),
                        (0, (base_y + side * (16 + index * 4)) * fine, 8 * fine),
                    ],
                    3,
                    GLOW if index % 2 == 0 else BONE,
                )
        for side in (-1, 1):
            body.fill_box(40, side * 7, 16, 44, side * 9, 18, EYE)

    if spec["kind"] not in ("whale", "kraken"):
        eye_x = length - max(8, length // 5)
        for side in (-1, 1):
            body.fill_box(eye_x, side * max(3, width - 2), 11, eye_x + 2, side * max(4, width - 1), 13, EYE)

    return body, detail


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Mobs_Giants", parent_collection)
    materials = ensure_palette("VXGiant", PALETTE)
    built = []

    cols = 4
    step_x = 4200.0
    step_y = 4200.0

    for index, spec in enumerate(ASSET_DEFS):
        body, detail = _build_giant(spec)
        row = index // cols
        col = index % cols
        location = (origin[0] + col * step_x, origin[1] + row * step_y, origin[2])
        spawn_asset(spec["name"], [body, detail], materials, collection, location)
        built.append(spec["name"])

    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Mobs_Giants] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
