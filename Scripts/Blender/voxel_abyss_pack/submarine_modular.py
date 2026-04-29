"""
Sub3D - Voxel Abyss Pack - Submarine Modular Kit
================================================

Modular submarine pieces for PCG assembly.
All geometry is generated as voxels.
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
    from core import (
        MaterialDef,
        VoxelGrid,
        ensure_collection,
        ensure_palette,
        ensure_scene,
        focus_collection,
        interpolate_knots,
        spawn_asset,
    )
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


HULL = 0
PANEL = 1
WINDOW = 2
RUST = 3
LIGHT = 4
STEEL = 5

PALETTE = [
    MaterialDef("Hull", (0.14, 0.18, 0.20), metallic=0.18, roughness=0.62),
    MaterialDef("Panel", (0.24, 0.28, 0.30), metallic=0.08, roughness=0.72),
    MaterialDef("Window", (0.08, 0.24, 0.33), roughness=0.14, emission=0.45),
    MaterialDef("Rust", (0.35, 0.20, 0.10), roughness=0.85),
    MaterialDef("Light", (0.08, 0.82, 0.92), roughness=0.12, emission=4.8),
    MaterialDef("Steel", (0.46, 0.50, 0.52), metallic=0.36, roughness=0.46),
]


def _m_to_vox(meters: float, voxel_cm: float) -> int:
    return max(1, int(round((meters * 100.0) / voxel_cm)))


def _hollow_box(
    grid: VoxelGrid,
    x0: int,
    y0: int,
    z0: int,
    x1: int,
    y1: int,
    z1: int,
    shell_material: int,
    inner_margin: int = 1,
) -> None:
    grid.fill_box(x0, y0, z0, x1, y1, z1, shell_material)
    grid.carve_box(
        x0 + inner_margin,
        y0 + inner_margin,
        z0 + inner_margin,
        x1 - inner_margin,
        y1 - inner_margin,
        z1 - inner_margin,
    )


def _build_hull_module(
    *,
    voxel_cm: float,
    length_m: float,
    radius_y_m: float,
    radius_z_m: float,
    front_scale: float,
    back_scale: float,
    windows: bool,
) -> tuple[VoxelGrid, VoxelGrid]:
    detail_voxel = voxel_cm / 3.0
    fine = max(1, int(round(voxel_cm / detail_voxel)))

    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(detail_voxel)
    inner = VoxelGrid(voxel_cm)

    length = _m_to_vox(length_m, voxel_cm)
    ry = _m_to_vox(radius_y_m, voxel_cm)
    rz = _m_to_vox(radius_z_m, voxel_cm)

    knots = [
        (0, max(2, int(round(ry * front_scale))), max(2, int(round(rz * front_scale))), rz + 2),
        (int(length * 0.20), ry, rz, rz + 2),
        (int(length * 0.55), ry, rz + 1, rz + 2),
        (int(length * 0.80), max(2, ry - 1), max(2, rz - 1), rz + 1),
        (length, max(2, int(round(ry * back_scale))), max(2, int(round(rz * back_scale))), rz),
    ]

    rib_step = max(2, _m_to_vox(1.6, voxel_cm))
    for x in range(length + 1):
        half_y, half_z, cz = interpolate_knots(x, knots)
        body.fill_ellipse_x(x, 0, cz, half_y, half_z, HULL)
        if half_y > 2 and half_z > 2:
            inner.fill_ellipse_x(x, 0, cz, half_y - 2, half_z - 2, PANEL)
        if x % rib_step == 0:
            body.set(x, -half_y, cz + 1, STEEL)
            body.set(x, half_y, cz + 1, STEEL)

    for (x, y, z) in list(inner.cells.keys()):
        body.discard(x, y, z)

    deck_z = max(1, rz - 1)
    body.fill_box(max(1, _m_to_vox(1.0, voxel_cm)), -max(1, ry - 2), deck_z, length - max(1, _m_to_vox(1.0, voxel_cm)), max(1, ry - 2), deck_z, PANEL)

    connector_half = max(2, _m_to_vox(1.4, voxel_cm))
    body.carve_box(0, -connector_half, rz, 0, connector_half, rz + 2)
    body.carve_box(length, -connector_half, rz, length, connector_half, rz + 2)

    if windows:
        x0 = int(length * 0.22)
        x1 = int(length * 0.78)
        side_y0 = max(2, ry - 1)
        side_y1 = ry
        body.fill_box(x0, side_y0, rz + 1, x1, side_y1, rz + 2, WINDOW)
        body.fill_box(x0, -side_y1, rz + 1, x1, -side_y0, rz + 2, WINDOW)

    detail.fill_polyline(
        [
            (int(length * 0.08) * fine, 0, (rz + 4) * fine),
            (int(length * 0.5) * fine, 0, (rz + 6) * fine),
            (int(length * 0.92) * fine, 0, (rz + 4) * fine),
        ],
        2,
        STEEL,
    )
    return body, detail


def _build_bridge(voxel_cm: float, height_m: float = 6.0) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    h = _m_to_vox(height_m, voxel_cm)
    half = _m_to_vox(2.8, voxel_cm)

    _hollow_box(body, -half, -half, 0, half, half, h, PANEL)
    body.fill_box(-2, -half, _m_to_vox(1.2, voxel_cm), 2, -half, _m_to_vox(2.6, voxel_cm), WINDOW)
    body.fill_box(-2, half, _m_to_vox(1.2, voxel_cm), 2, half, _m_to_vox(2.6, voxel_cm), WINDOW)
    body.fill_box(-1, -1, h + 1, 1, 1, h + _m_to_vox(1.4, voxel_cm), LIGHT)
    return body, detail


def _build_engine_block(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body, detail = _build_hull_module(
        voxel_cm=voxel_cm,
        length_m=8.0,
        radius_y_m=3.0,
        radius_z_m=2.4,
        front_scale=0.9,
        back_scale=0.8,
        windows=False,
    )
    length = _m_to_vox(8.0, voxel_cm)
    body.fill_box(length - _m_to_vox(1.8, voxel_cm), -_m_to_vox(3.6, voxel_cm), _m_to_vox(1.8, voxel_cm), length + _m_to_vox(1.2, voxel_cm), -_m_to_vox(2.2, voxel_cm), _m_to_vox(2.8, voxel_cm), STEEL)
    body.fill_box(length - _m_to_vox(1.8, voxel_cm), _m_to_vox(2.2, voxel_cm), _m_to_vox(1.8, voxel_cm), length + _m_to_vox(1.2, voxel_cm), _m_to_vox(3.6, voxel_cm), _m_to_vox(2.8, voxel_cm), STEEL)
    return body, detail


def _build_ballast_pod(voxel_cm: float, starboard: bool) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(6.0, voxel_cm)
    center_y = int(round((60.0 / voxel_cm))) * (1 if starboard else -1)

    for x in range(length + 1):
        body.fill_ellipse_x(
            x,
            center_y,
            _m_to_vox(1.6, voxel_cm),
            _m_to_vox(1.4, voxel_cm),
            _m_to_vox(1.0, voxel_cm),
            HULL,
        )
    body.fill_box(_m_to_vox(1.0, voxel_cm), center_y - 1, _m_to_vox(2.1, voxel_cm), length - _m_to_vox(1.0, voxel_cm), center_y + 1, _m_to_vox(2.4, voxel_cm), PANEL)
    return body, detail


def _build_dock_collar(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(6.0, voxel_cm)
    half = _m_to_vox(3.0, voxel_cm)
    h = _m_to_vox(3.2, voxel_cm)

    _hollow_box(body, 0, -half, 0, length, half, h, STEEL)
    body.carve_box(length // 2 - 2, -2, _m_to_vox(1.0, voxel_cm), length // 2 + 2, 2, _m_to_vox(2.4, voxel_cm))
    body.fill_box(length // 2 - 1, -1, h, length // 2 + 1, 1, h + 1, LIGHT)
    return body, detail


def _build_torpedo_bay(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(8.0, voxel_cm)
    half = _m_to_vox(2.8, voxel_cm)
    h = _m_to_vox(3.2, voxel_cm)

    _hollow_box(body, 0, -half, 0, length, half, h, PANEL)
    for y in (-1, 1):
        body.carve_box(_m_to_vox(1.4, voxel_cm), y * _m_to_vox(1.8, voxel_cm), _m_to_vox(1.2, voxel_cm), _m_to_vox(6.6, voxel_cm), y * _m_to_vox(2.2, voxel_cm), _m_to_vox(2.0, voxel_cm))
        body.fill_box(_m_to_vox(1.2, voxel_cm), y * _m_to_vox(2.2, voxel_cm), _m_to_vox(1.0, voxel_cm), _m_to_vox(6.8, voxel_cm), y * _m_to_vox(2.2, voxel_cm), _m_to_vox(2.2, voxel_cm), RUST)
    return body, detail


def _build_fin_set(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    root_x = _m_to_vox(2.0, voxel_cm)
    body.fill_box(0, -_m_to_vox(1.0, voxel_cm), _m_to_vox(1.2, voxel_cm), root_x, _m_to_vox(1.0, voxel_cm), _m_to_vox(2.4, voxel_cm), STEEL)

    span = _m_to_vox(4.5, voxel_cm)
    for side in (-1, 1):
        for x in range(0, _m_to_vox(4.0, voxel_cm) + 1):
            wing = int(round((1.0 - (x / max(1, _m_to_vox(4.0, voxel_cm)))) * span))
            body.fill_box(x, side * _m_to_vox(1.0, voxel_cm), _m_to_vox(2.0, voxel_cm), x, side * (_m_to_vox(1.0, voxel_cm) + side * wing), _m_to_vox(2.6, voxel_cm), PANEL)

    for z in range(_m_to_vox(2.4, voxel_cm), _m_to_vox(5.0, voxel_cm)):
        width = max(0, _m_to_vox(2.0, voxel_cm) - (z - _m_to_vox(2.4, voxel_cm)))
        body.fill_box(_m_to_vox(1.0, voxel_cm), -width, z, _m_to_vox(2.2, voxel_cm), width, z, PANEL)
    return body, detail


def _build_spine(voxel_cm: float, length_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(length_m, voxel_cm)
    body.fill_box(0, -_m_to_vox(1.4, voxel_cm), 0, length, _m_to_vox(1.4, voxel_cm), _m_to_vox(2.0, voxel_cm), STEEL)

    step = max(2, _m_to_vox(1.8, voxel_cm))
    for x in range(step, length, step):
        arm = _m_to_vox(2.8 + (x // step) % 2, voxel_cm)
        body.fill_box(x, -arm, _m_to_vox(0.8, voxel_cm), x + 1, arm, _m_to_vox(3.2, voxel_cm), STEEL)
        body.fill_box(x - 1, -arm, _m_to_vox(2.6, voxel_cm), x + 2, arm, _m_to_vox(2.8, voxel_cm), LIGHT)
    return body, detail


def _build_interior_corridor(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(4.0, voxel_cm)
    half = _m_to_vox(1.9, voxel_cm)
    h = _m_to_vox(2.6, voxel_cm)
    _hollow_box(body, 0, -half, 0, length, half, h, PANEL)
    for x in range(_m_to_vox(0.8, voxel_cm), length, _m_to_vox(1.0, voxel_cm)):
        body.fill_box(x, -half + 1, h - 1, x, half - 1, h - 1, LIGHT)
    return body, detail


def _build_service_room(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    half = _m_to_vox(3.0, voxel_cm)
    h = _m_to_vox(3.4, voxel_cm)
    _hollow_box(body, -half, -half, 0, half, half, h, PANEL)
    body.fill_box(-half + 1, -_m_to_vox(1.0, voxel_cm), _m_to_vox(1.1, voxel_cm), -half + _m_to_vox(2.4, voxel_cm), _m_to_vox(1.0, voxel_cm), _m_to_vox(2.3, voxel_cm), STEEL)
    body.fill_box(half - _m_to_vox(2.4, voxel_cm), -_m_to_vox(1.0, voxel_cm), _m_to_vox(1.1, voxel_cm), half - 1, _m_to_vox(1.0, voxel_cm), _m_to_vox(2.3, voxel_cm), STEEL)
    body.fill_box(-1, -1, h, 1, 1, h + 1, LIGHT)
    return body, detail


def _build_prop_ring(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    inner = VoxelGrid(voxel_cm)
    radius = _m_to_vox(2.8, voxel_cm)
    for z in range(_m_to_vox(1.2, voxel_cm), _m_to_vox(2.2, voxel_cm) + 1):
        body.fill_ellipse_z(z, 0, 0, radius, radius, STEEL)
        inner.fill_ellipse_z(
            z,
            0,
            0,
            max(1, radius - _m_to_vox(0.8, voxel_cm)),
            max(1, radius - _m_to_vox(0.8, voxel_cm)),
            PANEL,
        )
    for (x, y, z) in list(inner.cells.keys()):
        body.discard(x, y, z)

    for angle in (0, 60, 120):
        rad = math.radians(angle)
        dx = int(round(math.cos(rad) * (radius - 1)))
        dy = int(round(math.sin(rad) * (radius - 1)))
        body.fill_segment((-dx, -dy, _m_to_vox(1.7, voxel_cm)), (dx, dy, _m_to_vox(1.7, voxel_cm)), 1, STEEL)
    body.fill_box(-1, -1, _m_to_vox(1.4, voxel_cm), 1, 1, _m_to_vox(2.0, voxel_cm), LIGHT)
    return body, detail


def _build_reactor_capsule(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(6.0, voxel_cm)
    for x in range(length + 1):
        body.fill_ellipse_x(
            x,
            0,
            _m_to_vox(2.0, voxel_cm),
            _m_to_vox(2.0, voxel_cm),
            _m_to_vox(1.6, voxel_cm),
            HULL,
        )
    body.carve_box(_m_to_vox(1.0, voxel_cm), -_m_to_vox(1.0, voxel_cm), _m_to_vox(1.4, voxel_cm), length - _m_to_vox(1.0, voxel_cm), _m_to_vox(1.0, voxel_cm), _m_to_vox(2.5, voxel_cm))
    body.fill_box(length // 2 - 2, -1, _m_to_vox(1.8, voxel_cm), length // 2 + 2, 1, _m_to_vox(2.2, voxel_cm), LIGHT)
    return body, detail


ASSET_DEFS = [
    {"name": "SM_VX_SubMod_Nose_8m", "kind": "nose", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_Mid_8m_A", "kind": "mid_a", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_Mid_8m_B", "kind": "mid_b", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_Stern_10m", "kind": "stern", "voxel_cm": 12.0},
    {"name": "SM_VX_SubMod_Core_12m", "kind": "core", "voxel_cm": 12.0},
    {"name": "SM_VX_SubMod_Bridge_6m", "kind": "bridge", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_Engine_8m", "kind": "engine", "voxel_cm": 12.0},
    {"name": "SM_VX_SubMod_BallastPort_6m", "kind": "ballast_port", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_BallastStbd_6m", "kind": "ballast_stbd", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_DockCollar_6m", "kind": "dock", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_TorpedoBay_8m", "kind": "torpedo", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_FinSet_A", "kind": "fin", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_Spine_10m", "kind": "spine", "voxel_cm": 10.0},
    {"name": "SM_VX_SubMod_InteriorCorridor_4m", "kind": "corridor", "voxel_cm": 8.0},
    {"name": "SM_VX_SubMod_ServiceRoom_6m", "kind": "service", "voxel_cm": 8.0},
    {"name": "SM_VX_SubMod_PropRing_A", "kind": "prop_ring", "voxel_cm": 8.0},
    {"name": "SM_VX_SubMod_ReactorCapsule_6m", "kind": "reactor", "voxel_cm": 8.0},
]


def _build_asset(spec):
    voxel_cm = float(spec["voxel_cm"])
    kind = spec["kind"]

    if kind == "nose":
        return _build_hull_module(
            voxel_cm=voxel_cm,
            length_m=8.0,
            radius_y_m=3.2,
            radius_z_m=2.4,
            front_scale=0.45,
            back_scale=0.95,
            windows=False,
        )
    if kind == "mid_a":
        return _build_hull_module(
            voxel_cm=voxel_cm,
            length_m=8.0,
            radius_y_m=3.6,
            radius_z_m=2.7,
            front_scale=0.95,
            back_scale=0.95,
            windows=True,
        )
    if kind == "mid_b":
        return _build_hull_module(
            voxel_cm=voxel_cm,
            length_m=8.0,
            radius_y_m=3.8,
            radius_z_m=2.8,
            front_scale=0.9,
            back_scale=0.9,
            windows=True,
        )
    if kind == "stern":
        return _build_hull_module(
            voxel_cm=voxel_cm,
            length_m=10.0,
            radius_y_m=3.3,
            radius_z_m=2.5,
            front_scale=1.0,
            back_scale=0.55,
            windows=False,
        )
    if kind == "core":
        return _build_hull_module(
            voxel_cm=voxel_cm,
            length_m=12.0,
            radius_y_m=4.0,
            radius_z_m=3.0,
            front_scale=0.9,
            back_scale=0.9,
            windows=True,
        )
    if kind == "bridge":
        return _build_bridge(voxel_cm, 6.0)
    if kind == "engine":
        return _build_engine_block(voxel_cm)
    if kind == "ballast_port":
        return _build_ballast_pod(voxel_cm, False)
    if kind == "ballast_stbd":
        return _build_ballast_pod(voxel_cm, True)
    if kind == "dock":
        return _build_dock_collar(voxel_cm)
    if kind == "torpedo":
        return _build_torpedo_bay(voxel_cm)
    if kind == "fin":
        return _build_fin_set(voxel_cm)
    if kind == "spine":
        return _build_spine(voxel_cm, 10.0)
    if kind == "corridor":
        return _build_interior_corridor(voxel_cm)
    if kind == "service":
        return _build_service_room(voxel_cm)
    if kind == "prop_ring":
        return _build_prop_ring(voxel_cm)
    return _build_reactor_capsule(voxel_cm)


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Submarine_Modular", parent_collection)
    materials = ensure_palette("VXSubMod", PALETTE)
    built = []

    cols = 5
    step_x = 3800.0
    step_y = 3200.0

    for index, spec in enumerate(ASSET_DEFS):
        body, detail = _build_asset(spec)
        row = index // cols
        col = index % cols
        location = (origin[0] + col * step_x, origin[1] + row * step_y, origin[2])
        spawn_asset(spec["name"], [body, detail], materials, collection, location)
        built.append(spec["name"])

    if clear_scene:
        focus_collection(collection)
        print(f"[VX_Submarine_Modular] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
