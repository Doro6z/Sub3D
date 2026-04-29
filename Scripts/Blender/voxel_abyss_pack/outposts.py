"""
Sub3D - Voxel Abyss Pack - Modular Outposts
===========================================

Large modular structures for PCG assembly.
Target scale:
- tower modules: 10-20 m
- empty interiors (shell geometry)
- consistent corridor connectors
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
PANEL = 1
WINDOW = 2
RUST = 3
LIGHT = 4
PIPE = 5

PALETTE = [
    MaterialDef("Steel", (0.18, 0.23, 0.25), metallic=0.22, roughness=0.56),
    MaterialDef("Panel", (0.25, 0.30, 0.32), metallic=0.10, roughness=0.68),
    MaterialDef("Window", (0.06, 0.20, 0.29), roughness=0.14, emission=0.4),
    MaterialDef("Rust", (0.34, 0.20, 0.12), roughness=0.84),
    MaterialDef("Light", (0.07, 0.80, 0.92), roughness=0.12, emission=4.2),
    MaterialDef("Pipe", (0.44, 0.47, 0.48), metallic=0.30, roughness=0.44),
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


def _tower(voxel_cm: float, height_m: float, width_m: float, ring_step_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    h = _m_to_vox(height_m, voxel_cm)
    half = _m_to_vox(width_m * 0.5, voxel_cm)
    _hollow_box(body, -half, -half, 0, half, half, h, STEEL)

    door_w = max(2, half // 3)
    body.carve_box(-door_w, -half, 1, door_w, -half + 1, _m_to_vox(3.0, voxel_cm))

    floor_step = max(2, _m_to_vox(3.0, voxel_cm))
    for z in range(floor_step, h, floor_step):
        body.fill_box(-half + 1, -half + 1, z, half - 1, half - 1, z, PANEL)
        body.carve_box(-2, -2, z, 2, 2, z)

    ring_step = max(2, _m_to_vox(ring_step_m, voxel_cm))
    for z in range(ring_step, h, ring_step):
        body.fill_box(-half - 1, -half - 1, z, half + 1, half + 1, z + 1, PIPE)

    window_z0 = _m_to_vox(4.0, voxel_cm)
    window_step = max(2, _m_to_vox(2.5, voxel_cm))
    for z in range(window_z0, h - 2, window_step):
        body.fill_box(-2, -half, z, 2, -half, z + 1, WINDOW)
        body.fill_box(-2, half, z, 2, half, z + 1, WINDOW)
        body.fill_box(-half, -2, z, -half, 2, z + 1, WINDOW)
        body.fill_box(half, -2, z, half, 2, z + 1, WINDOW)

    body.fill_box(-2, -2, h + 1, 2, 2, h + _m_to_vox(2.0, voxel_cm), LIGHT)
    return body, detail


def _hub(voxel_cm: float, size_m: float, floors: int) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    half = _m_to_vox(size_m * 0.5, voxel_cm)
    height = _m_to_vox(3.0 * floors + 1.0, voxel_cm)
    _hollow_box(body, -half, -half, 0, half, half, height, PANEL)

    for z in range(_m_to_vox(3.0, voxel_cm), height, _m_to_vox(3.0, voxel_cm)):
        body.fill_box(-half + 1, -half + 1, z, half - 1, half - 1, z, STEEL)
        body.carve_box(-2, -2, z, 2, 2, z)

    connector_half = _m_to_vox(1.5, voxel_cm)
    for side in (-1, 1):
        body.carve_box(-connector_half, side * half, _m_to_vox(1.0, voxel_cm), connector_half, side * half, _m_to_vox(2.4, voxel_cm))
        body.carve_box(side * half, -connector_half, _m_to_vox(1.0, voxel_cm), side * half, connector_half, _m_to_vox(2.4, voxel_cm))

    body.fill_box(-1, -1, height + 1, 1, 1, height + _m_to_vox(1.5, voxel_cm), LIGHT)
    return body, detail


def _corridor(voxel_cm: float, length_m: float, width_m: float = 4.0, height_m: float = 3.0) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    length = _m_to_vox(length_m, voxel_cm)
    half_w = _m_to_vox(width_m * 0.5, voxel_cm)
    h = _m_to_vox(height_m, voxel_cm)
    _hollow_box(body, 0, -half_w, 0, length, half_w, h, STEEL)
    body.carve_box(0, -1, _m_to_vox(1.0, voxel_cm), 0, 1, _m_to_vox(2.4, voxel_cm))
    body.carve_box(length, -1, _m_to_vox(1.0, voxel_cm), length, 1, _m_to_vox(2.4, voxel_cm))

    for x in range(_m_to_vox(1.5, voxel_cm), length, _m_to_vox(2.0, voxel_cm)):
        body.fill_box(x, -half_w + 1, h - 1, x, half_w - 1, h - 1, WINDOW)
    return body, detail


def _junction_elbow(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body, detail = _corridor(voxel_cm, 8.0)
    leg, _ = _corridor(voxel_cm, 8.0)
    body.stamp(leg, (0, _m_to_vox(4.0, voxel_cm), 0))
    return body, detail


def _junction_t(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body, detail = _corridor(voxel_cm, 10.0)
    branch, _ = _corridor(voxel_cm, 6.0)
    body.stamp(branch, (_m_to_vox(4.0, voxel_cm), _m_to_vox(4.0, voxel_cm), 0))
    body.stamp(branch, (_m_to_vox(4.0, voxel_cm), -_m_to_vox(10.0, voxel_cm), 0))
    return body, detail


def _junction_cross(voxel_cm: float) -> tuple[VoxelGrid, VoxelGrid]:
    body, detail = _corridor(voxel_cm, 10.0)
    cross, _ = _corridor(voxel_cm, 10.0)
    body.stamp(cross, (0, -_m_to_vox(5.0, voxel_cm), 0))
    body.stamp(cross, (_m_to_vox(5.0, voxel_cm), -_m_to_vox(5.0, voxel_cm), 0))
    return body, detail


def _dome(voxel_cm: float, diameter_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    radius = _m_to_vox(diameter_m * 0.5, voxel_cm)
    height = max(2, int(radius * 1.2))

    for z in range(0, height + 1):
        t = z / float(max(height, 1))
        ring = max(2, int(round(radius * math.sqrt(max(0.0, 1.0 - t * t)))))
        body.fill_ellipse_z(z, 0, 0, ring, ring, PANEL if z > 2 else STEEL)

    inner = VoxelGrid(voxel_cm)
    for z in range(1, height):
        t = z / float(max(height, 1))
        ring = max(1, int(round((radius - 1) * math.sqrt(max(0.0, 1.0 - t * t)))))
        inner.fill_ellipse_z(z, 0, 0, ring, ring, PANEL)
    for (x, y, z) in list(inner.cells.keys()):
        body.discard(x, y, z)

    neck = _m_to_vox(2.0, voxel_cm)
    body.fill_box(-neck, -neck, 0, neck, neck, _m_to_vox(2.5, voxel_cm), STEEL)
    body.carve_box(-1, -1, 1, 1, 1, _m_to_vox(2.2, voxel_cm))
    return body, detail


def _dock(voxel_cm: float, length_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)

    length = _m_to_vox(length_m, voxel_cm)
    half_w = _m_to_vox(4.0, voxel_cm)
    h = _m_to_vox(4.0, voxel_cm)

    body.fill_box(0, -half_w, 0, length, half_w, h, STEEL)
    body.carve_box(_m_to_vox(1.0, voxel_cm), -half_w + 1, 1, length - 1, half_w - 1, h - 1)
    body.fill_box(length - _m_to_vox(2.0, voxel_cm), -half_w - 2, 1, length + _m_to_vox(1.5, voxel_cm), -half_w + 2, h + 2, PIPE)
    body.fill_box(length - _m_to_vox(2.0, voxel_cm), half_w - 2, 1, length + _m_to_vox(1.5, voxel_cm), half_w + 2, h + 2, PIPE)
    return body, detail


def _airlock(voxel_cm: float, length_m: float = 6.0) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(length_m, voxel_cm)
    half_w = _m_to_vox(2.5, voxel_cm)
    h = _m_to_vox(3.5, voxel_cm)
    _hollow_box(body, 0, -half_w, 0, length, half_w, h, STEEL)
    door_x = _m_to_vox(1.0, voxel_cm)
    body.fill_box(door_x, -half_w + 1, 1, door_x + 1, half_w - 1, h - 1, RUST)
    body.fill_box(length - door_x - 1, -half_w + 1, 1, length - door_x, half_w - 1, h - 1, RUST)
    body.fill_box(length // 2 - 1, -1, h - 1, length // 2 + 1, 1, h, LIGHT)
    return body, detail


def _stairs(voxel_cm: float, height_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    h = _m_to_vox(height_m, voxel_cm)
    body.fill_box(-_m_to_vox(3.0, voxel_cm), -_m_to_vox(3.0, voxel_cm), 0, _m_to_vox(3.0, voxel_cm), _m_to_vox(3.0, voxel_cm), h, STEEL)
    body.carve_box(-_m_to_vox(2.0, voxel_cm), -_m_to_vox(2.0, voxel_cm), 1, _m_to_vox(2.0, voxel_cm), _m_to_vox(2.0, voxel_cm), h - 1)
    step_h = max(1, _m_to_vox(0.5, voxel_cm))
    step_d = max(1, _m_to_vox(0.75, voxel_cm))
    for z in range(1, h - 1, step_h):
        x = -_m_to_vox(2.0, voxel_cm) + ((z // step_h) % max(2, _m_to_vox(4.0, voxel_cm)))
        body.fill_box(x, -_m_to_vox(2.0, voxel_cm), z, x + step_d, _m_to_vox(2.0, voxel_cm), z, PANEL)
    return body, detail


def _walkway(voxel_cm: float, length_m: float) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(length_m, voxel_cm)
    half_w = _m_to_vox(2.0, voxel_cm)
    body.fill_box(0, -half_w, 0, length, half_w, 1, PANEL)
    for x in range(0, length + 1, max(1, _m_to_vox(2.0, voxel_cm))):
        body.fill_box(x, -half_w, 1, x, -half_w, _m_to_vox(2.0, voxel_cm), PIPE)
        body.fill_box(x, half_w, 1, x, half_w, _m_to_vox(2.0, voxel_cm), PIPE)
    return body, detail


def _service_spine(voxel_cm: float, length_m: float, branches: int) -> tuple[VoxelGrid, VoxelGrid]:
    body = VoxelGrid(voxel_cm)
    detail = VoxelGrid(voxel_cm / 3.0)
    length = _m_to_vox(length_m, voxel_cm)
    body.fill_box(0, -_m_to_vox(1.5, voxel_cm), 0, length, _m_to_vox(1.5, voxel_cm), _m_to_vox(2.0, voxel_cm), PIPE)
    step = max(2, length // max(1, branches))
    for index, x in enumerate(range(step, length, step)):
        span = _m_to_vox(3.0 + (index % 2), voxel_cm)
        body.fill_box(x, -span, _m_to_vox(1.0, voxel_cm), x + 1, span, _m_to_vox(4.0, voxel_cm), PIPE)
        body.fill_box(x - 1, -span, _m_to_vox(3.0, voxel_cm), x + 2, span, _m_to_vox(3.0, voxel_cm), LIGHT)
    return body, detail


ASSET_DEFS = [
    {"name": "SM_VX_OutpostTower_10m_A", "builder": "tower", "params": {"height_m": 10.0, "width_m": 8.0, "ring_step_m": 2.5}},
    {"name": "SM_VX_OutpostTower_15m_A", "builder": "tower", "params": {"height_m": 15.0, "width_m": 9.0, "ring_step_m": 3.0}},
    {"name": "SM_VX_OutpostTower_20m_A", "builder": "tower", "params": {"height_m": 20.0, "width_m": 10.0, "ring_step_m": 3.5}},
    {"name": "SM_VX_OutpostHub_2F_A", "builder": "hub", "params": {"size_m": 12.0, "floors": 2}},
    {"name": "SM_VX_OutpostHub_3F_A", "builder": "hub", "params": {"size_m": 14.0, "floors": 3}},
    {"name": "SM_VX_OutpostCorridor_6m_Straight", "builder": "corridor", "params": {"length_m": 6.0}},
    {"name": "SM_VX_OutpostCorridor_12m_Straight", "builder": "corridor", "params": {"length_m": 12.0}},
    {"name": "SM_VX_OutpostCorridor_18m_Straight", "builder": "corridor", "params": {"length_m": 18.0}},
    {"name": "SM_VX_OutpostCorridor_Elbow_A", "builder": "elbow", "params": {}},
    {"name": "SM_VX_OutpostCorridor_T_A", "builder": "t", "params": {}},
    {"name": "SM_VX_OutpostCorridor_Cross_A", "builder": "cross", "params": {}},
    {"name": "SM_VX_OutpostDockClamp_10m_A", "builder": "dock", "params": {"length_m": 10.0}},
    {"name": "SM_VX_OutpostDockClamp_16m_A", "builder": "dock", "params": {"length_m": 16.0}},
    {"name": "SM_VX_OutpostPressureDome_8m_A", "builder": "dome", "params": {"diameter_m": 8.0}},
    {"name": "SM_VX_OutpostPressureDome_12m_A", "builder": "dome", "params": {"diameter_m": 12.0}},
    {"name": "SM_VX_OutpostAirlock_6m_A", "builder": "airlock", "params": {"length_m": 6.0}},
    {"name": "SM_VX_OutpostAirlock_8m_A", "builder": "airlock", "params": {"length_m": 8.0}},
    {"name": "SM_VX_OutpostStairs_10m_A", "builder": "stairs", "params": {"height_m": 10.0}},
    {"name": "SM_VX_OutpostLift_15m_A", "builder": "stairs", "params": {"height_m": 15.0}},
    {"name": "SM_VX_OutpostWalkway_10m_A", "builder": "walkway", "params": {"length_m": 10.0}},
    {"name": "SM_VX_OutpostWalkway_20m_A", "builder": "walkway", "params": {"length_m": 20.0}},
    {"name": "SM_VX_OutpostServiceSpine_14m_A", "builder": "spine", "params": {"length_m": 14.0, "branches": 3}},
    {"name": "SM_VX_OutpostServiceSpine_22m_A", "builder": "spine", "params": {"length_m": 22.0, "branches": 5}},
]


def _build_asset(spec) -> tuple[VoxelGrid, VoxelGrid]:
    voxel_cm = 25.0
    builder = spec["builder"]
    params = dict(spec["params"])

    if builder == "tower":
        return _tower(voxel_cm, **params)
    if builder == "hub":
        return _hub(voxel_cm, **params)
    if builder == "corridor":
        return _corridor(voxel_cm, **params)
    if builder == "elbow":
        return _junction_elbow(voxel_cm)
    if builder == "t":
        return _junction_t(voxel_cm)
    if builder == "cross":
        return _junction_cross(voxel_cm)
    if builder == "dock":
        return _dock(voxel_cm, **params)
    if builder == "dome":
        return _dome(voxel_cm, **params)
    if builder == "airlock":
        return _airlock(voxel_cm, **params)
    if builder == "stairs":
        return _stairs(voxel_cm, **params)
    if builder == "walkway":
        return _walkway(voxel_cm, **params)
    return _service_spine(voxel_cm, **params)


def build_assets(parent_collection=None, clear_scene: bool = True, origin=(0.0, 0.0, 0.0)):
    if clear_scene:
        ensure_scene(clear_scene=True)

    collection = ensure_collection("VX_Outposts", parent_collection)
    materials = ensure_palette("VXOutpost", PALETTE)
    built = []

    cols = 5
    step_x = 3200.0
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
        print(f"[VX_Outposts] built {len(built)} assets")
    return built


def main():
    build_assets(clear_scene=True)


if __name__ == "__main__":
    main()
