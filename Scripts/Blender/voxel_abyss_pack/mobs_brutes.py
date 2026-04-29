"""
Sub3D - Voxel Abyss Pack - Brute Mobs
=====================================

Mid-size hostile fauna.
Includes Juvenile / Adult / Alpha variants.
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
ARMOR = 2
EYE = 3
GLOW = 4
BONE = 5

PALETTE = [
    MaterialDef("Body", (0.09, 0.14, 0.16), roughness=0.82),
    MaterialDef("Belly", (0.24, 0.27, 0.23), roughness=0.90),
    MaterialDef("Armor", (0.21, 0.24, 0.25), roughness=0.68, metallic=0.12),
    MaterialDef("Eye", (0.90, 0.92, 0.84), roughness=0.10, emission=0.6),
    MaterialDef("Glow", (0.05, 0.65, 0.78), roughness=0.15, emission=4.0),
    MaterialDef("Bone", (0.68, 0.66, 0.57), roughness=0.62),
]

BASE_FORMS = [
    {"base_name": "MawDrifter", "kind": "drifter", "length": 56, "width": 10, "height": 7},
    {"base_name": "BarnacleBoar", "kind": "boar", "length": 48, "width": 11, "height": 8},
    {"base_name": "ShellbackRam", "kind": "boar", "length": 52, "width": 12, "height": 8},
    {"base_name": "AnchorJaw", "kind": "anchor", "length": 60, "width": 10, "height": 9},
    {"base_name": "SlugBrute", "kind": "slug", "length": 54, "width": 11, "height": 7},
    {"base_name": "VentStalker", "kind": "stalker", "length": 50, "width": 9, "height": 9},
    {"base_name": "MudGorger", "kind": "drifter", "length": 58, "width": 11, "height": 8},
    {"base_name": "ReefCrusher", "kind": "anchor", "length": 56, "width": 12, "height": 9},
]

SIZE_VARIANTS = [
    {"label": "Juvenile", "scale": 0.65, "voxel_cm": 3.0},
    {"label": "Adult", "scale": 1.00, "voxel_cm": 4.0},
    {"label": "Alpha", "scale": 1.45, "voxel_cm": 5.0},
]


def _build_asset_defs():
    assets = []
    seed = 400
    for base in BASE_FORMS:
        for variant in SIZE_VARIANTS:
            assets.append(
                {
                    "name": f"SM_VX_{base['base_name']}_{variant['label']}",
                    "kind": base["kind"],
                    "seed": seed,
                    "length": max(20, int(round(base["length"] * variant["scale"]))),
                    "width": max(4, int(round(base["width"] * variant["scale"]))),
                    "height": max(4, int(round(base["height"] * variant["scale"]))),
                    "voxel_cm": variant["voxel_cm"],
                }
            )
            seed += 17
    return assets


ASSET_DEFS = _build_asset_defs()


def _build_brute(spec):
    voxel_cm = float(spec["voxel_cm"])
    detail_voxel = voxel_cm / 3.0
    fine = max(1, int(round(voxel_cm / detail_voxel)))

    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(detail_voxel)
    length = spec["length"]
    width = spec["width"]
    height = spec["height"]

    knots = [
        (0, max(2, width // 2), max(2, height // 2), 6),
        (int(length * 0.18), width, height, 7),
        (int(length * 0.46), width + 1, height + (1 if spec["kind"] != "slug" else 0), 8),
        (int(length * 0.78), max(3, width - 1), height, 7),
        (length, 2, max(2, height // 2), 5),
    ]

    for x in range(length + 1):
        ry, rz, zc = interpolate_knots(x, knots)
        zc += int(round(math.sin((x / max(length, 1)) * math.pi) * 1.5))
        body.fill_ellipse_x(x, 0, zc, ry, rz, BODY)
        if ry > 2:
            body.fill_ellipse_x(x, 0, zc - rz, max(1, ry - 1), 1, BELLY)

        if x % 4 == 0 and 4 < x < length - 2:
            body.set(x, 0, zc + rz + 1, ARMOR)

    if spec["kind"] in ("boar", "anchor"):
        for x in range(int(length * 0.16), int(length * 0.74), 4):
            body.fill_box(x, -width - 1, 5, x + 1, -width + 1, 8, ARMOR)
            body.fill_box(x, width - 1, 5, x + 1, width + 1, 8, ARMOR)
        for side in (-1, 1):
            detail.fill_polyline(
                [(length * fine - 6 * fine, side * 9 * fine, 21 * fine), (length * fine, side * 16 * fine, 17 * fine), (length * fine + 4 * fine, side * 22 * fine, 12 * fine)],
                2,
                BONE,
            )

    if spec["kind"] == "slug":
        body.carve_box(length - 8, -2, 5, length, 2, 7)
        for x in range(6, length - 3, 5):
            body.fill_box(x, -width - 1, 3, x + 1, -width, 4, ARMOR)
            body.fill_box(x, width, 3, x + 1, width + 1, 4, ARMOR)
        for side in (-1, 1):
            detail.fill_polyline([(12 * fine, side * 18 * fine, 15 * fine), (30 * fine, side * 28 * fine, 10 * fine), (50 * fine, side * 34 * fine, 7 * fine)], 2, GLOW)

    if spec["kind"] == "drifter":
        for x in range(int(length * 0.25), int(length * 0.6)):
            span = 3 + int(3 * math.sin((x - length * 0.25) / max(length * 0.35, 1.0) * math.pi))
            body.fill_box(x, -width - span, 5, x, -width, 7 + span // 2, ARMOR)
            body.fill_box(x, width, 5, x, width + span, 7 + span // 2, ARMOR)

    if spec["kind"] == "stalker":
        for side in (-1, 1):
            for leg_index, x in enumerate(range(5, length - 5, 5)):
                y0 = side * (width + 1)
                y1 = side * (width + 6 + (leg_index % 2))
                detail.fill_polyline([(x * fine, y0 * fine, 15 * fine), ((x + 2) * fine, y1 * fine, 9 * fine), ((x + 3) * fine, (y1 + side * 3) * fine, 5 * fine)], 2, ARMOR)
            for spike_x in range(8, length - 4, 6):
                body.fill_box(spike_x, -1, 10, spike_x + 1, 1, 13 + (spike_x % 3), BONE)

    eye_x = length - max(5, length // 4)
    for side in (-1, 1):
        body.fill_box(eye_x, side * (width - 1), 8, eye_x + 2, side * width, 10, EYE)

    for side in (-1, 1):
        detail.fill_polyline(
            [((length - 4) * fine, side * 9 * fine, 24 * fine), ((length + 1) * fine, side * 18 * fine, 18 * fine), ((length + 6) * fine, side * 24 * fine, 12 * fine)],
            2,
            GLOW if spec["kind"] in ("anchor", "drifter") else BONE,
        )

    maw_x = length - 2
    body.fill_box(maw_x - 5, -2, 4, maw_x, 2, 7, BONE if spec["kind"] == "anchor" else GLOW)
    return body, detail


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Mobs_Brutes", parent_collection)
    materials = ensure_palette("VXBrute", PALETTE)
    built = []

    cols = 6
    step_x = 2400.0
    step_y = 2200.0

    for index, spec in enumerate(ASSET_DEFS):
        body, detail = _build_brute(spec)
        row = index // cols
        col = index % cols
        location = (origin[0] + col * step_x, origin[1] + row * step_y, origin[2])
        spawn_asset(spec["name"], [body, detail], materials, collection, location)
        built.append(spec["name"])

    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Mobs_Brutes] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
