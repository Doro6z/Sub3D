"""
Sub3D - Voxel Abyss Pack - Scout Mobs
=====================================

Small fauna focus.
Includes baby/small/adult/large variants for each base archetype.
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
SHELL = 2
EYE = 3
GLOW = 4
TOOTH = 5

PALETTE = [
    MaterialDef("Body", (0.08, 0.17, 0.20), roughness=0.78),
    MaterialDef("Belly", (0.30, 0.34, 0.30), roughness=0.92),
    MaterialDef("Shell", (0.22, 0.27, 0.26), roughness=0.68),
    MaterialDef("Eye", (0.85, 0.88, 0.82), roughness=0.15, emission=0.8),
    MaterialDef("Glow", (0.07, 0.72, 0.86), roughness=0.16, emission=4.2),
    MaterialDef("Tooth", (0.74, 0.76, 0.68), roughness=0.55),
]

BASE_FORMS = [
    {"base_name": "SiltLeech", "archetype": "eel", "length": 30, "width": 2, "height": 2, "fin": 0, "glow": True},
    {"base_name": "PincherMite", "archetype": "crawler", "length": 20, "width": 3, "height": 2, "fin": 0, "glow": False},
    {"base_name": "GlimmerSkate", "archetype": "fish", "length": 26, "width": 5, "height": 2, "fin": 4, "glow": True},
    {"base_name": "RiftEel", "archetype": "eel", "length": 36, "width": 2, "height": 3, "fin": 1, "glow": True},
    {"base_name": "BlindCrawler", "archetype": "crawler", "length": 24, "width": 4, "height": 3, "fin": 0, "glow": False},
    {"base_name": "NeedleLamprey", "archetype": "eel", "length": 22, "width": 2, "height": 2, "fin": 0, "glow": False},
    {"base_name": "SnoutCrab", "archetype": "crab", "length": 18, "width": 4, "height": 3, "fin": 0, "glow": True},
    {"base_name": "PulseMinnow", "archetype": "fish", "length": 22, "width": 3, "height": 2, "fin": 3, "glow": True},
]

SIZE_VARIANTS = [
    {"label": "Baby", "scale": 0.45, "voxel_cm": 2.0},
    {"label": "Small", "scale": 0.75, "voxel_cm": 2.0},
    {"label": "Adult", "scale": 1.00, "voxel_cm": 2.0},
    {"label": "Large", "scale": 1.35, "voxel_cm": 3.0},
]


def _build_asset_defs():
    assets = []
    seed = 100
    for base in BASE_FORMS:
        for variant in SIZE_VARIANTS:
            length = max(10, int(round(base["length"] * variant["scale"])))
            width = max(1, int(round(base["width"] * variant["scale"])))
            height = max(1, int(round(base["height"] * variant["scale"])))
            fin = max(0, int(round(base["fin"] * variant["scale"])))
            assets.append(
                {
                    "name": f"SM_VX_{base['base_name']}_{variant['label']}",
                    "archetype": base["archetype"],
                    "seed": seed,
                    "length": length,
                    "width": width,
                    "height": height,
                    "fin": fin,
                    "glow": base["glow"],
                    "voxel_cm": variant["voxel_cm"],
                }
            )
            seed += 13
    return assets


ASSET_DEFS = _build_asset_defs()


def _make_body(spec) -> tuple[VoxelGrid, VoxelGrid]:
    voxel_cm = float(spec["voxel_cm"])
    detail_voxel = voxel_cm / 3.0
    fine = max(1, int(round(voxel_cm / detail_voxel)))

    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(detail_voxel)

    length = spec["length"]
    width = spec["width"]
    height = spec["height"]
    knots = [
        (0, max(1, width // 2), max(1, height // 2), 5),
        (int(length * 0.22), width, height, 6),
        (int(length * 0.55), width + (1 if spec["archetype"] != "eel" else 0), height + (1 if spec["archetype"] == "crawler" else 0), 6),
        (int(length * 0.82), max(1, width - 1), max(1, height - 1), 5),
        (length, 1, 1, 4),
    ]

    for x in range(length + 1):
        ry, rz, zc = interpolate_knots(x, knots)
        zc += int(round(math.sin((x / max(length, 1)) * math.pi) * 1.5))
        body.fill_ellipse_x(x, 0, zc, ry, rz, BODY)
        if ry > 1:
            body.fill_ellipse_x(x, 0, zc - rz, max(1, ry - 1), 1, BELLY)

        if spec["glow"] and 5 < x < length - 3 and x % 5 == 0:
            body.set(x, ry, zc, GLOW)
            body.set(x, -ry, zc - 1, GLOW)

        if spec["archetype"] in ("fish", "crawler") and x % 6 == 0 and 4 < x < length - 3:
            body.set(x, 0, zc + rz + 1, SHELL)

    if spec["archetype"] == "fish":
        for x in range(int(length * 0.28), int(length * 0.60)):
            span = 1 + int(3 * math.sin((x - length * 0.28) / max(length * 0.32, 1.0) * math.pi))
            body.fill_box(x, -span - spec["fin"], 5, x, -span, 6 + span // 2, SHELL)
            body.fill_box(x, span, 5, x, span + spec["fin"], 6 + span // 2, SHELL)
        for tail_x in range(length - 3, length + 1):
            tail_h = 1 + (tail_x - (length - 3))
            body.fill_box(tail_x, -1, 4 - tail_h, tail_x, 1, 4 + tail_h, SHELL)

    if spec["archetype"] == "eel":
        for x in range(length - 8, length + 1):
            tail = x - (length - 8)
            body.fill_box(x, -1 - tail // 3, 4 - tail // 2, x, 1 + tail // 3, 4 + tail // 2, GLOW if spec["glow"] else SHELL)

    if spec["archetype"] == "crawler":
        for side in (-1, 1):
            for leg_index, x in enumerate(range(5, length - 2, 4)):
                y = side * (width + 1 + (leg_index % 2))
                body.fill_box(x - 1, y, 3, x, y + side * 2, 4, SHELL)
                detail.fill_polyline(
                    [
                        (x * fine, (y + side * 2) * fine, 11 * fine),
                        ((x + 2) * fine, (y + side * 5) * fine, 9 * fine),
                        ((x + 3) * fine, (y + side * 6) * fine, 7 * fine),
                    ],
                    1,
                    SHELL,
                )

    if spec["archetype"] == "crab":
        body.fill_box(4, -5, 4, 11, 5, 8, SHELL)
        body.carve_box(6, -1, 4, 8, 1, 8)
        for side in (-1, 1):
            for leg in range(4):
                base_x = 3 + leg * 3
                detail.fill_polyline(
                    [
                        (base_x * fine, side * 18 * fine, 15 * fine),
                        ((base_x + 2) * fine, side * 24 * fine, 12 * fine),
                        ((base_x + 4) * fine, side * 28 * fine, 8 * fine),
                    ],
                    1,
                    SHELL,
                )
            detail.fill_polyline(
                [(7 * fine, side * 16 * fine, 20 * fine), (10 * fine, side * 28 * fine, 22 * fine), (13 * fine, side * 34 * fine, 18 * fine)],
                2,
                TOOTH,
            )

    eye_x = length - max(4, length // 5)
    for side in (-1, 1):
        body.fill_box(eye_x, side * width, 6, eye_x + 1, side * (width + 1), 7, EYE)

    if spec["archetype"] in ("eel", "fish"):
        jaw_x = length - 2
        body.fill_box(jaw_x - 2, -1, 4, jaw_x, 1, 5, TOOTH)
        for side in (-1, 1):
            detail.fill_polyline(
                [
                    ((jaw_x - 1) * fine, side * 3 * fine, 15 * fine),
                    ((jaw_x + 2) * fine, side * 7 * fine, 12 * fine),
                    ((jaw_x + 4) * fine, side * 11 * fine, 10 * fine),
                ],
                1,
                GLOW if spec["glow"] else TOOTH,
            )

    return body, detail


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Mobs_Scouts", parent_collection)
    materials = ensure_palette("VXScout", PALETTE)
    built = []

    cols = 8
    step_x = 1600.0
    step_y = 1400.0

    for index, spec in enumerate(ASSET_DEFS):
        body, detail = _make_body(spec)
        row = index // cols
        col = index % cols
        location = (origin[0] + col * step_x, origin[1] + row * step_y, origin[2])
        spawn_asset(spec["name"], [body, detail], materials, collection, location)
        built.append(spec["name"])

    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Mobs_Scouts] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
