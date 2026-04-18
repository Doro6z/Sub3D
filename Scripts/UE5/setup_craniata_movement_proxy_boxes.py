"""
Sub3D - Build manual box collision for the Craniata movement proxy
==================================================================

This script writes a controlled simple-collision layout onto a single Static
Mesh asset intended for ASubmarineBase.MovementCollisionProxy.

Why this exists:
- The moving submarine hull cannot rely on UseComplexAsSimple for sweep queries.
- Auto Convex on SM_Hull is too irregular for stable movement and impact points.
- Manual boxes must form a continuous solid envelope, not a thin shell with gaps.

This script follows the Blender blockout style:
- the hull body is defined by longitudinal sections along X
- each adjacent section pair becomes one box
- the sail / dorsal structure is defined as an explicit extra box

The result is:
- deterministic
- easy to tweak in source control
- safe to re-run

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/scripts/UE5/setup_craniata_movement_proxy_boxes.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4

Run from the editor Python console:
    exec(open(r"C:/Dev/Sub3D/scripts/UE5/setup_craniata_movement_proxy_boxes.py").read())
"""

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

TARGET_ASSET = (
    "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/"
    "SM_Hull_MovementProxy.SM_Hull_MovementProxy"
)

DRY_RUN = False
COLLISION_PROFILE = "SubmarineHull"
SECTION_OVERLAP_CM = 8.0

# Longitudinal hull body.
# Format: (name, x_alpha, half_width_alpha, half_height_alpha)
# x_alpha is mapped to local mesh bounds in [-1, +1] along X.
# half_width_alpha / half_height_alpha are fractions of the mesh local Y/Z extents.
#
# Adjacent pairs define one solid box. Keep this list ordered by x_alpha.
HULL_SECTIONS = [
    ("stern_tip",  -1.00, 0.16, 0.18),
    ("aft_body",   -0.82, 0.40, 0.44),
    ("aft_mid",    -0.45, 0.56, 0.60),
    ("center",      0.00, 0.60, 0.66),
    ("bow_mid",     0.42, 0.52, 0.56),
    ("bow_body",    0.74, 0.34, 0.38),
    ("bow_tip",     1.00, 0.12, 0.14),
]

# Explicit dorsal / sail / upper access box.
# Format: (name, x0_alpha, x1_alpha, y0_alpha, y1_alpha, z0_alpha, z1_alpha)
DORSAL_BOXES = [
    ("sail", -0.22, 0.08, -0.34, 0.34, 0.42, 1.02),
]


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):
    unreal.log(f"[CraniataMovementProxy] {msg}")


def warn(msg):
    unreal.log_warning(f"[CraniataMovementProxy] {msg}")


def err(msg):
    unreal.log_error(f"[CraniataMovementProxy] {msg}")


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _sorted_pair(a, b):
    return (a, b) if a <= b else (b, a)


def _alpha_to_x(origin_x, extent_x, alpha):
    return origin_x + extent_x * float(alpha)


def _alpha_to_y(origin_y, extent_y, alpha):
    return origin_y + extent_y * float(alpha)


def _alpha_to_z(origin_z, extent_z, alpha):
    return origin_z + extent_z * float(alpha)


def _make_box_elem(name, x0, x1, y0, y1, z0, z1):
    x0, x1 = _sorted_pair(x0, x1)
    y0, y1 = _sorted_pair(y0, y1)
    z0, z1 = _sorted_pair(z0, z1)

    elem = unreal.KBoxElem()
    elem.set_editor_property("name", name)
    elem.set_editor_property(
        "center",
        unreal.Vector(
            0.5 * (x0 + x1),
            0.5 * (y0 + y1),
            0.5 * (z0 + z1),
        ),
    )
    elem.set_editor_property("rotation", unreal.Rotator(0.0, 0.0, 0.0))
    elem.set_editor_property("x", abs(x1 - x0))
    elem.set_editor_property("y", abs(y1 - y0))
    elem.set_editor_property("z", abs(z1 - z0))
    elem.set_editor_property("collision_enabled", unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    elem.set_editor_property("contribute_to_mass", True)
    elem.set_editor_property("is_generated", False)
    elem.set_editor_property("rest_offset", 0.0)
    return elem


def _build_section_boxes(bounds):
    origin = bounds.origin
    extent = bounds.box_extent
    boxes = []

    for index in range(len(HULL_SECTIONS) - 1):
        name0, x0_alpha, half_w0, half_h0 = HULL_SECTIONS[index]
        name1, x1_alpha, half_w1, half_h1 = HULL_SECTIONS[index + 1]

        x0 = _alpha_to_x(origin.x, extent.x, x0_alpha)
        x1 = _alpha_to_x(origin.x, extent.x, x1_alpha)
        if x1 < x0:
            x0, x1 = x1, x0

        # Slight overlap prevents narrow sweep gaps between adjacent boxes.
        x0 -= SECTION_OVERLAP_CM
        x1 += SECTION_OVERLAP_CM

        half_w = max(float(half_w0), float(half_w1)) * extent.y
        half_h = max(float(half_h0), float(half_h1)) * extent.z

        y0 = origin.y - half_w
        y1 = origin.y + half_w
        z0 = origin.z - half_h
        z1 = origin.z + half_h

        boxes.append(
            _make_box_elem(
                f"Body_{index:02d}_{name0}_to_{name1}",
                x0,
                x1,
                y0,
                y1,
                z0,
                z1,
            )
        )

    return boxes


def _build_dorsal_boxes(bounds):
    origin = bounds.origin
    extent = bounds.box_extent
    boxes = []

    for index, (name, x0a, x1a, y0a, y1a, z0a, z1a) in enumerate(DORSAL_BOXES):
        boxes.append(
            _make_box_elem(
                f"Dorsal_{index:02d}_{name}",
                _alpha_to_x(origin.x, extent.x, x0a),
                _alpha_to_x(origin.x, extent.x, x1a),
                _alpha_to_y(origin.y, extent.y, y0a),
                _alpha_to_y(origin.y, extent.y, y1a),
                _alpha_to_z(origin.z, extent.z, z0a),
                _alpha_to_z(origin.z, extent.z, z1a),
            )
        )

    return boxes


# ---------------------------------------------------------------------------
# Asset mutation
# ---------------------------------------------------------------------------

def _clear_agg_array(agg_geom, property_name):
    try:
        agg_geom.set_editor_property(property_name, [])
    except Exception:
        warn(f"agg_geom property not exposed in Python: {property_name}")


def _elem_name_text(elem):
    try:
        return str(elem.get_editor_property("name"))
    except Exception:
        return "<unnamed>"


def _load_static_mesh(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"asset not found: {path}")
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError(f"asset is not a StaticMesh: {path}")
    return asset


def _try_call(obj, method_name, *args):
    try:
        method = getattr(obj, method_name, None)
        if method is None:
            return False
        method(*args)
        return True
    except Exception as exc:
        warn(f"{obj.__class__.__name__}.{method_name} failed: {exc}")
        return False


def _apply_collision(asset):
    body_setup = asset.get_editor_property("body_setup")
    if body_setup is None:
        raise RuntimeError(f"{asset.get_name()} has no body_setup")

    bounds = asset.get_bounds()
    section_boxes = _build_section_boxes(bounds)
    dorsal_boxes = _build_dorsal_boxes(bounds)
    all_boxes = section_boxes + dorsal_boxes

    log(f"target: {asset.get_path_name()}")
    log(
        "bounds | "
        f"origin=({bounds.origin.x:.2f}, {bounds.origin.y:.2f}, {bounds.origin.z:.2f}) | "
        f"extent=({bounds.box_extent.x:.2f}, {bounds.box_extent.y:.2f}, {bounds.box_extent.z:.2f})"
    )
    log(f"generated {len(all_boxes)} box element(s)")

    for elem in all_boxes:
        center = elem.get_editor_property("center")
        elem_name = _elem_name_text(elem)
        log(
            f"  [BOX] {elem_name:<28} "
            f"center=({center.x:.1f}, {center.y:.1f}, {center.z:.1f}) "
            f"size=({elem.get_editor_property('x'):.1f}, "
            f"{elem.get_editor_property('y'):.1f}, "
            f"{elem.get_editor_property('z'):.1f})"
        )

    if DRY_RUN:
        log("DRY RUN: asset not modified")
        return

    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if mesh_editor is None:
        raise RuntimeError("StaticMeshEditorSubsystem not available")

    asset.modify()
    body_setup.modify()

    # Clear existing simple collision first so the script is deterministic.
    try:
        mesh_editor.remove_collisions_with_notification(asset, False)
    except Exception:
        mesh_editor.remove_collisions(asset)

    agg_geom = body_setup.get_editor_property("agg_geom")
    _clear_agg_array(agg_geom, "sphere_elems")
    _clear_agg_array(agg_geom, "sphyl_elems")
    _clear_agg_array(agg_geom, "convex_elems")
    _clear_agg_array(agg_geom, "tapered_capsule_elems")
    _clear_agg_array(agg_geom, "level_set_elems")
    _clear_agg_array(agg_geom, "skinned_level_set_elems")
    _clear_agg_array(agg_geom, "ml_level_set_elems")
    _clear_agg_array(agg_geom, "skinned_triangle_mesh_elems")
    agg_geom.set_editor_property("box_elems", all_boxes)
    body_setup.set_editor_property("agg_geom", agg_geom)

    default_instance = body_setup.get_editor_property("default_instance")
    default_instance.set_editor_property("collision_profile_name", COLLISION_PROFILE)
    body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)

    # Python exposure differs by engine version. Try the editor notifications that exist.
    _try_call(body_setup, "invalidate_physics_data")
    _try_call(body_setup, "create_physics_meshes")
    _try_call(body_setup, "post_edit_change")
    _try_call(asset, "build", False)
    _try_call(asset, "post_edit_change")
    _try_call(asset, "mark_package_dirty")
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False)

    log("asset updated and saved")


def main():
    try:
        asset = _load_static_mesh(TARGET_ASSET)
        _apply_collision(asset)
    except Exception as exc:
        err(str(exc))
        raise


main()
