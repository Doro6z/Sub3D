"""
Sub3D - gameplay-first hull blockout (Blender 5.0 safe)
=======================================================
Hull, superstructure, decks, bulkheads, doors, hatches, ballast relief,
control surfaces, and rear airlock blockout.

Run: Blender > Scripting > Open > Alt+P
"""

import bpy
import math
import os
import sys
import importlib.util
import importlib

def _resolve_script_dirs():
    dirs = []

    if "__file__" in globals():
        dirs.append(os.path.dirname(os.path.abspath(__file__)))

    # Blender text editor execution often has no __file__; use the active text filepath.
    try:
        text = bpy.context.space_data.text if bpy.context and bpy.context.space_data else None
        if text and text.filepath:
            dirs.append(os.path.dirname(os.path.abspath(text.filepath)))
    except Exception:
        pass

    # Stable project fallbacks for this repository.
    dirs.append(r"C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt")
    dirs.append(r"C:\Dev\Sub3D\Source\scripts\hull_blockout_gpt")
    dirs.append(os.getcwd())

    # De-duplicate while preserving order.
    unique = []
    for path in dirs:
        if path and path not in unique:
            unique.append(path)
    return unique


def _install_import_paths():
    for base in _resolve_script_dirs():
        core_dir = os.path.join(base, "core")
        if os.path.isdir(base) and base not in sys.path:
            sys.path.append(base)
        if os.path.isdir(core_dir) and core_dir not in sys.path:
            sys.path.append(core_dir)


def _load_core_submodule(filename, module_name, missing_msg):
    """Generic loader: same strategy as the legacy bulkheads loader, reused for
    pivots.py and materials.py so main.py can run from Blender's text editor
    where __file__ is not defined."""
    _install_import_paths()

    for base in _resolve_script_dirs():
        candidate = os.path.join(base, "core", filename)
        if not os.path.isfile(candidate):
            continue
        sys.modules.pop(module_name, None)
        spec = importlib.util.spec_from_file_location(module_name, candidate)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module

    importlib.invalidate_caches()
    bare_name = filename[:-3] if filename.endswith(".py") else filename
    sys.modules.pop(bare_name, None)
    try:
        return importlib.import_module(bare_name)
    except ModuleNotFoundError:
        pass

    raise ModuleNotFoundError(missing_msg)


def _load_bulkheads_module():
    return _load_core_submodule(
        "bulkheads_doors.py",
        "hull_blockout_gpt_bulkheads_doors",
        "Unable to load bulkheads_doors.py from hull_blockout_gpt/core",
    )


def _load_pivots_module():
    return _load_core_submodule(
        "pivots.py",
        "hull_blockout_gpt_pivots",
        "Unable to load pivots.py from hull_blockout_gpt/core",
    )


def _load_materials_module():
    return _load_core_submodule(
        "materials.py",
        "hull_blockout_gpt_materials",
        "Unable to load materials.py from hull_blockout_gpt/core",
    )


_bulkheads_doors = _load_bulkheads_module()
_pivots = _load_pivots_module()
_materials = _load_materials_module()
build_compartment_bulkhead = getattr(_bulkheads_doors, "build_compartment_bulkhead", _bulkheads_doors.build_standard_bulkhead)
build_standard_pressure_door = getattr(_bulkheads_doors, "build_standard_pressure_door", _bulkheads_doors.build_standard_watertight_door)
build_standard_bulkhead = _bulkheads_doors.build_standard_bulkhead
build_sliding_split_door = _bulkheads_doors.build_sliding_split_door
build_standard_watertight_door = _bulkheads_doors.build_standard_watertight_door
compute_bulkhead_top_z = _bulkheads_doors.compute_bulkhead_top_z
place_bulkhead_with_clearance = _bulkheads_doors.place_bulkhead_with_clearance
fix_all_pivots = _pivots.fix_all_pivots
assign_materials = _materials.assign_materials

LENGTH = 4200.0
SUB_NAME = "Craniata"
HULL_THICK = 15.0
DECK_THICK = 18.0
BH_THICK = 14.0
DECK_CONTACT_OVERLAP = 1.0
BULKHEAD_CONTACT_OVERLAP = 1.5
UPPER_ROOM_SIDE_PAD = 10.0
UPPER_ROOM_ROOF_START = 0.58
RADIAL = 64
N_RINGS = 300

# Hull curve: keep the gameplay-first mass, but recover the fuller bow.
HULL_CURVE = [
    (0.000, 50),
    (0.010, 140),
    (0.025, 240),
    (0.050, 330),
    (0.080, 380),
    (0.110, 400),
    (0.150, 415),
    (0.200, 425),
    (0.280, 430),
    (0.380, 430),
    (0.480, 425),
    (0.560, 415),
    (0.640, 400),
    (0.720, 375),
    (0.780, 345),
    (0.830, 320),
    (0.870, 285),
    (0.910, 240),
    (0.940, 185),
    (0.960, 145),
    (0.980, 102),
    (0.995, 65),
    (1.000, 48),
]

DECK_MAIN_Z = -20.0
DECK_LOWER_Z = -240.0

SUPER_X_START = 0.16
SUPER_X_END = 0.72
SUPER_WIDTH = 480.0
SUPER_HEIGHT = 200.0
SUPER_FRONT_BLEND = 0.24
SUPER_REAR_BLEND = 0.095
SUPER_FRONT_WIDTH_SCALE = 0.86
SUPER_REAR_WIDTH_SCALE = 0.98
SUPER_FRONT_HEIGHT_SCALE = 0.78
SUPER_REAR_HEIGHT_SCALE = 1.02
SUPER_SHOULDER_START = 0.48
SUPER_SHOULDER_END = 0.45

BOW_BELUGA_END = 0.22
BOW_CENTER_DROP = 0.08
BOW_TOP_STRETCH = 0.30
BOW_BOTTOM_SQUASH = 0.10

DOOR_W = 100.0
DOOR_H = 200.0
DOOR_LOWER_W = DOOR_W
DOOR_LOWER_H = DOOR_H
DOOR_FRAME_MARGIN = 12.0
DOOR_FRAME_DEPTH = 12.0
DOOR_LEAF_DEPTH = 8.0
DOOR_THRESHOLD = 10.0
DOOR_ROUGH_MARGIN_X = 6.0
DOOR_ROUGH_MARGIN_Z = 7.0
DOOR_LEAF_OVERLAP = 2.0
UPPER_ARMORY_DOOR_Y_OFFSET = -131.0
LADDER_Y_OFFSET = -38.48

MAIN_BULKHEAD_SPECS = [
    {"name": "Fwd", "x_norm": 0.224, "door_w": DOOR_W, "door_h": DOOR_H, "full_height": True},
    {"name": "Control", "x_norm": 0.628, "door_w": DOOR_W, "door_h": DOOR_H, "full_height": False},
]

LOWER_BULKHEAD_SPECS = [
    {"name": "BallastFwd", "x_norm": 0.335, "door_w": DOOR_LOWER_W, "door_h": DOOR_LOWER_H},
    {"name": "BallastAft", "x_norm": 0.515, "door_w": DOOR_LOWER_W, "door_h": DOOR_LOWER_H},
]

SAS_LENGTH_CM = 220.0
UPPER_AIRLOCK_EXIT_BULKHEAD = {"name": "UpperAirlock_Exit", "x_norm": 0.694, "door_w": DOOR_W, "door_h": DOOR_H}
UPPER_AIRLOCK_INNER_BULKHEAD = {
    "name": "UpperAirlock_Inner",
    "x_norm": UPPER_AIRLOCK_EXIT_BULKHEAD["x_norm"] - (SAS_LENGTH_CM / LENGTH),
    "door_w": DOOR_W,
    "door_h": DOOR_H,
}
UPPER_ARMORY_PARTITION_X_NORM = 0.275
AIRLOCK_TERRACE_START_OFFSET = 90.0
AIRLOCK_TERRACE_FLAT_OFFSET = 90.0
AIRLOCK_TERRACE_RECOVER_OFFSET = 240.0
AIRLOCK_TERRACE_RECOVER_MID_OFFSET = 160.0
AIRLOCK_CASSETTE_DEPTH = 24.0
LOWER_HUB_CENTER_NORM = 0.425

BH_MAIN_FWD_NAME = "Fwd"
DOOR_CLEARANCE_H = 200.0
UPPER_DECK_AFT_NORM = SUPER_X_END - 0.003

MAIN_DECK_CUTOUTS = [
    {
        "name": "LowerAccess",
        "x_norm": LOWER_HUB_CENTER_NORM,
        "y_center": 0.0,
        "shape": "round",
        "radius": 56.0,
        "coaming_height": 48.0,
    },
]

UPPER_DECK_CUTOUTS = [
    # Staircase hole geometry.
    # The previous values (x_norm=0.334, half_length=188, half_width=68) made
    # the hole too large and positioned 188 cm FORWARD of the stair arrival,
    # so the player's head bumped the upper deck during climb and the EXACT
    # boolean sometimes failed silently, leaving only the coaming wireframe.
    #
    # New sizing: the stair spans x in [1129.2, 1335.3] cm and rises from
    # z=-9 to z=267. A 180 cm character's head clears the upper-deck underside
    # at x~=1205 during climb, and the last step ends at x=1335.3. The hole
    # needs to cover that full range plus a small landing aft.
    #
    # Hole = [1201, 1361] in X (half_length=80, cx=1281 -> x_norm=0.305),
    # y = +/-44 (stair+rails fit within 84 cm, 4 cm margin).
    {
        "name": "UpperAccess",
        "x_norm": 0.305,
        "y_center": 0.0,
        "shape": "rect",
        "half_length": 80.0,
        "half_width": 44.0,
        "coaming_height": 48.0,
    },
]

LOWER_DECK_CUTOUTS = [
    {
        "name": "FondAccess",
        "x_norm": LOWER_HUB_CENTER_NORM,
        "y_center": 0.0,
        "shape": "round",
        "radius": 46.0,
        "coaming_height": 24.0,
    },
]

BALLAST_PAIRS = [
    {
        "name": "Fwd",
        "center_norm": 0.18,
        "length_norm": 0.44,
        "y_center": 174.0,
        "z_center": -244.0,
        "half_width": 84.0,
        "half_height": 62.0,
    },
    {
        "name": "Aft",
        "center_norm": 0.60,
        "length_norm": 0.26,
        "y_center": 172.0,
        "z_center": -248.0,
        "half_width": 80.0,
        "half_height": 60.0,
    },
]

BALLAST_BULGE_STRENGTH = 0.190
BALLAST_BULGE_DROP = 0.092
BALLAST_SERVICE_Z_OFFSET = -34.0
BALLAST_CATWALK_THICK = 3.5
BALLAST_COMPARTMENT_MARGIN = 24.0
BALLAST_CATWALK_END_MARGIN = 2.0
LOWER_WALKWAY_START = 0.22
LOWER_WALKWAY_END = 0.74
LOWER_WALKWAY_HALF_WIDTH = 64.0
LOWER_WALKWAY_Z = DECK_LOWER_Z
HYBRID_DECK_Z = -125.0
LOWER_TECH_PARTITION_X = 0.72
LOWER_HUB_START = 0.345
LOWER_HUB_END = 0.505
LOWER_BH_Z_MIN = -430.0

AFT_UPPER_SHOULDER_START = 0.56
AFT_UPPER_SHOULDER_END = 0.80
AFT_UPPER_SHOULDER_STRENGTH = 0.08
AFT_DORSAL_START = 0.52
AFT_DORSAL_END = 0.73
AFT_DORSAL_STRENGTH = 0.10

TURRET_HARDPOINTS = [
    {"name": "FwdTop", "x_norm": 0.098, "mount": "top", "length": 56.0, "width": 74.0, "height": 14.0},
    {"name": "AftTop", "x_norm": 0.756, "mount": "top", "length": 52.0, "width": 66.0, "height": 10.0},
    {"name": "FwdBottom", "x_norm": 0.108, "mount": "bottom", "length": 52.0, "width": 72.0, "height": 14.0},
]


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def blend_window(t, fade_in, fade_out):
    t = max(0.0, min(1.0, t))
    if t <= 0.0 or t >= 1.0:
        return 0.0
    if t < fade_in:
        return smoothstep(t / max(fade_in, 1e-6))
    if t > 1.0 - fade_out:
        return smoothstep((1.0 - t) / max(fade_out, 1e-6))
    return 1.0


def _build_cubic_spline(points):
    n = len(points) - 1
    x = [p[0] for p in points]
    y = [p[1] for p in points]

    h = [x[i + 1] - x[i] for i in range(n)]
    alpha = [0.0] * (n + 1)
    for i in range(1, n):
        alpha[i] = (3 / h[i]) * (y[i + 1] - y[i]) - (3 / h[i - 1]) * (y[i] - y[i - 1])

    l = [1.0] + [0.0] * n
    mu = [0.0] * (n + 1)
    z = [0.0] * (n + 1)

    for i in range(1, n):
        l[i] = 2 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1]
        mu[i] = h[i] / l[i]
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i]

    l[n] = 1.0
    z[n] = 0.0

    b = [0.0] * n
    c = [0.0] * (n + 1)
    d = [0.0] * n

    for j in range(n - 1, -1, -1):
        c[j] = z[j] - mu[j] * c[j + 1]
        b[j] = (y[j + 1] - y[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3
        d[j] = (c[j + 1] - c[j]) / (3 * h[j])

    return x, y, b, c, d


_SPLINE = _build_cubic_spline(HULL_CURVE)


def sample_curve(nx):
    nx = max(0.0, min(1.0, nx))
    sx, sy, sb, sc, sd = _SPLINE
    n = len(sx) - 1

    seg = 0
    for j in range(n):
        if sx[j] <= nx <= sx[j + 1]:
            seg = j
            break
        seg = j

    dx = nx - sx[seg]
    result = sy[seg] + sb[seg] * dx + sc[seg] * dx * dx + sd[seg] * dx * dx * dx
    return max(0.0, result)


def superstructure_state(nx):
    if nx < SUPER_X_START or nx > SUPER_X_END:
        return 0.0, SUPER_WIDTH, SUPER_HEIGHT, SUPER_SHOULDER_START, 0.0

    t = (nx - SUPER_X_START) / (SUPER_X_END - SUPER_X_START)
    sb = blend_window(t, SUPER_FRONT_BLEND, SUPER_REAR_BLEND)

    front_fill = smoothstep(min(1.0, t / 0.32))
    aft_airlock = smoothstep(max(0.0, (t - 0.88) / 0.12))

    width_scale = (
        SUPER_FRONT_WIDTH_SCALE
        + (1.0 - SUPER_FRONT_WIDTH_SCALE) * front_fill
        - (1.0 - SUPER_REAR_WIDTH_SCALE) * aft_airlock * 0.6
    )
    height_scale = (
        SUPER_FRONT_HEIGHT_SCALE
        + (1.0 - SUPER_FRONT_HEIGHT_SCALE) * front_fill
        + (SUPER_REAR_HEIGHT_SCALE - 1.0) * aft_airlock * 0.55
    )
    shoulder_norm = (
        SUPER_SHOULDER_START
        + (SUPER_SHOULDER_END - SUPER_SHOULDER_START) * front_fill
    )

    return (
        sb,
        SUPER_WIDTH * width_scale,
        SUPER_HEIGHT * height_scale,
        shoulder_norm,
        aft_airlock,
    )


def apply_bow_profile(nx, by, bz, r):
    if nx >= BOW_BELUGA_END:
        return by, bz

    bow_t = 1.0 - nx / BOW_BELUGA_END
    z_offset = -r * BOW_CENTER_DROP * bow_t
    if bz >= 0.0:
        z_scaled = bz * (1.0 + BOW_TOP_STRETCH * bow_t)
    else:
        z_scaled = bz * (1.0 - BOW_BOTTOM_SQUASH * bow_t)
    return by, z_scaled + z_offset


def interior_hw(x_cm, z):
    nx = x_cm / LENGTH
    r = max(0.0, sample_curve(nx) - HULL_THICK - 5)
    main_r = sample_curve(nx)
    sb, super_w_local, super_h_local, shoulder_norm, _ = superstructure_state(nx)

    if sb > 0.0 and z >= main_r * shoulder_norm:
        sw = max(0.0, super_w_local / 2 - HULL_THICK - 5) * sb
        shoulder_z = main_r * shoulder_norm
        if z <= main_r:
            bt = (z - shoulder_z) / max(1.0, main_r - shoulder_z)
            hh = math.sqrt(max(0.0, r * r - z * z)) if abs(z) < r else 0.0
            return max(hh, hh + (sw - hh) * smoothstep(bt))

        dz = z - main_r
        sh = super_h_local * sb
        if sh > 0.0 and dz < sh:
            return sw * math.sqrt(max(0.0, 1.0 - (dz / sh) ** 2))
        return 0.0

    if r <= 0.0 or abs(z) >= r:
        return 0.0
    return math.sqrt(max(0.0, r * r - z * z))


def deck_bottom_z(deck_center_z):
    return deck_center_z - DECK_THICK * 0.5 - 0.5


def upper_room_floor_z():
    return sample_curve(0.40) - 150.0


def _inner_half_width_with_offset(x_cm, z, offset_cm):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    r = max(0.0, main_r - max(0.0, HULL_THICK - offset_cm))
    sb, super_w_local, super_h_local, shoulder_norm, _ = superstructure_state(nx)

    if sb > 0.0 and z >= main_r * shoulder_norm:
        super_hw = max(0.0, super_w_local * 0.5 - max(0.0, HULL_THICK - offset_cm)) * sb
        shoulder_z = main_r * shoulder_norm
        if z <= main_r:
            bt = (z - shoulder_z) / max(1.0, main_r - shoulder_z)
            hh = math.sqrt(max(0.0, r * r - z * z)) if abs(z) < r else 0.0
            return max(hh, hh + (super_hw - hh) * smoothstep(bt))

        dz = z - main_r
        sh = max(1.0, super_h_local * sb - max(0.0, HULL_THICK - offset_cm))
        if dz < sh:
            return super_hw * math.sqrt(max(0.0, 1.0 - (dz / sh) ** 2))
        return 0.0

    if r <= 0.0 or abs(z) >= r:
        return 0.0
    return math.sqrt(max(0.0, r * r - z * z))


def upper_ceiling_z(x_cm, margin=10.0):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    sb, _, super_h_local, _, _ = superstructure_state(nx)
    if sb > 0.02:
        return main_r + super_h_local * sb - HULL_THICK - margin
    return main_r - HULL_THICK - margin


def interior_hw_superfilled(x_cm, z):
    hw = interior_hw(x_cm, z)
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    sb, super_w_local, _, shoulder_norm, _ = superstructure_state(nx)
    if sb <= 0.02:
        return hw

    shoulder_z = main_r * shoulder_norm - 12.0
    blend_start_z = shoulder_z - 46.0
    if z <= blend_start_z:
        return hw

    ceiling_z = upper_ceiling_z(x_cm, margin=10.0)
    super_hw = max(0.0, super_w_local * 0.5 - HULL_THICK - 10.0)
    if ceiling_z <= blend_start_z + 1.0:
        return max(hw, super_hw)

    t = smoothstep((z - blend_start_z) / max(1.0, ceiling_z - blend_start_z))
    return max(hw, hw + (super_hw - hw) * t)


def interior_contact_hw(x_cm, z):
    return _inner_half_width_with_offset(x_cm, z, BULKHEAD_CONTACT_OVERLAP)


def upper_inner_apex_z(x_cm, margin=1.5):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    sb, _, super_h_local, _, _ = superstructure_state(nx)
    if sb > 0.02:
        return main_r + super_h_local * sb - margin
    return main_r - HULL_THICK - margin


def upper_deck_hw(x_cm, z):
    return upper_superstructure_contact_hw(x_cm, z, DECK_CONTACT_OVERLAP)


def upper_bulkhead_hw(x_cm, z):
    return upper_superstructure_contact_hw(x_cm, z, BULKHEAD_CONTACT_OVERLAP)


def main_deck_hw(x_cm, z):
    return _inner_half_width_with_offset(x_cm, z, DECK_CONTACT_OVERLAP)


def lower_deck_hw(x_cm, z):
    return _inner_half_width_with_offset(x_cm, z, DECK_CONTACT_OVERLAP)


def main_bulkhead_hw(x_cm, z):
    return _inner_half_width_with_offset(x_cm, z, BULKHEAD_CONTACT_OVERLAP)


def lower_bulkhead_hw(x_cm, z):
    return _inner_half_width_with_offset(x_cm, z, BULKHEAD_CONTACT_OVERLAP)


def upper_room_apex_z(x_cm, overlap_cm=BULKHEAD_CONTACT_OVERLAP):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    sb, _, super_h_local, _, _ = superstructure_state(nx)
    if sb <= 0.02:
        return 0.0
    return main_r + max(0.0, super_h_local * sb - max(0.0, HULL_THICK - overlap_cm - 2.0))


def upper_superstructure_contact_hw(x_cm, z, overlap_cm=BULKHEAD_CONTACT_OVERLAP):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    sb, super_w_local, _, _, _ = superstructure_state(nx)
    if sb <= 0.02:
        return 0.0

    floor_z = upper_room_floor_z()
    ceiling_z = upper_room_apex_z(x_cm, overlap_cm)
    if z >= ceiling_z:
        return 0.0

    super_hw = max(0.0, super_w_local * 0.5 - HULL_THICK + overlap_cm + UPPER_ROOM_SIDE_PAD)
    if super_hw <= 0.0:
        return 0.0

    if z <= floor_z + 6.0:
        return super_hw

    roof_start_z = floor_z + max(1.0, (ceiling_z - floor_z) * UPPER_ROOM_ROOF_START)
    if z <= roof_start_z:
        return super_hw

    roof_t = smoothstep((z - roof_start_z) / max(1.0, ceiling_z - roof_start_z))
    return super_hw * math.cos(roof_t * math.pi * 0.5)


def deck_cutouts_for(name):
    if name == "main":
        return MAIN_DECK_CUTOUTS
    if name == "lower":
        return LOWER_DECK_CUTOUTS
    if name == "upper":
        return UPPER_DECK_CUTOUTS
    return []


def bulkhead_x_norm(name):
    for spec in MAIN_BULKHEAD_SPECS:
        if spec["name"] == name:
            return spec["x_norm"]
    return MAIN_BULKHEAD_SPECS[0]["x_norm"]


def hull_half_width_at(x_cm, z_cm):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    r = sample_curve(nx)
    if abs(z_cm) >= r:
        return 0.0
    return math.sqrt(max(0.0, r * r - z_cm * z_cm))


def point_in_cutout(x, y, cutout):
    cx = cutout["x_norm"] * LENGTH
    cy = cutout.get("y_center", 0.0)
    if cutout.get("shape") == "round":
        radius = cutout.get("radius", 40.0)
        dx = x - cx
        dy = y - cy
        return dx * dx + dy * dy <= radius * radius
    return (
        abs(x - cx) <= cutout["half_length"]
        and abs(y - cy) <= cutout["half_width"]
    )


def clear():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for block in list(bpy.data.meshes):
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in list(bpy.data.materials):
        if block.users == 0:
            bpy.data.materials.remove(block)


def simple_mat(name, r, g, b):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = 0.7
    return mat


# The stylized material system and the legacy classifier that used to live here
# have been moved to core/materials.py. See that module for the full palette
# (solid-PBR, no textures, lowpoly/voxel friendly) and classification patterns.
# The call site at the bottom of main() now uses:
#     from core.materials import assign_materials as _assign_materials
#     _assign_materials()


def make(name, verts, faces, r, g, b, smooth=True):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(simple_mat(name, r, g, b))
    if smooth:
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        try:
            bpy.ops.object.shade_auto_smooth()
        except Exception:
            for poly in obj.data.polygons:
                poly.use_smooth = True
        obj.select_set(False)
    return obj


def add_solidify(obj, thick, offset=-1):
    mod = obj.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = thick
    mod.offset = offset
    return mod


def apply_object_modifier(obj, modifier_name):
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = obj.evaluated_get(depsgraph)
    baked_mesh = bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers=True, depsgraph=depsgraph)
    old_mesh = obj.data
    obj.modifiers.clear()
    obj.data = baked_mesh
    if old_mesh and old_mesh.users == 0:
        bpy.data.meshes.remove(old_mesh)


def _cleanup_mesh_for_boolean(obj):
    """Dissolve degenerate edges + recalc normals before a boolean operation.

    Blender's EXACT solver silently fails on non-manifold inputs, leaving the
    target mesh intact or with ghost topology. Running a degenerate-dissolve and
    a normal-recalc before the cut drops the failure rate on lofted and
    intersected decks to near zero.
    """
    if not obj or obj.type != "MESH":
        return
    prev_active = bpy.context.view_layer.objects.active
    prev_selection = [o for o in bpy.context.view_layer.objects if o.select_get()]
    try:
        if bpy.context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.remove_doubles(threshold=0.0005)
        bpy.ops.mesh.dissolve_degenerate(threshold=0.0005)
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.object.mode_set(mode="OBJECT")
    except RuntimeError as exc:
        print(f"[WARN] _cleanup_mesh_for_boolean failed on {obj.name}: {exc}")
    finally:
        obj.select_set(False)
        for o in prev_selection:
            try:
                o.select_set(True)
            except ReferenceError:
                pass
        if prev_active and prev_active.name in bpy.data.objects:
            bpy.context.view_layer.objects.active = prev_active


_BOOLEAN_SOLVER_PREFERENCE = {
    # Logical intent -> ordered list of enum names to try.
    # Newer Blender exposes ('FLOAT', 'EXACT', 'MANIFOLD'); older exposes ('FAST', 'EXACT').
    "EXACT": ("EXACT",),
    "FAST":  ("FLOAT", "FAST", "MANIFOLD"),
}


def _assign_boolean_solver(mod, intent):
    for candidate in _BOOLEAN_SOLVER_PREFERENCE.get(intent, (intent,)):
        try:
            mod.solver = candidate
            return candidate
        except TypeError:
            continue
    # Leave the default Blender solver in place if nothing matched.
    return mod.solver


def apply_boolean_difference(target_obj, cutter_obj, modifier_name, solver="EXACT"):
    if solver == "FAST":
        _cleanup_mesh_for_boolean(target_obj)
    mod = target_obj.modifiers.new(modifier_name, "BOOLEAN")
    mod.operation = "DIFFERENCE"
    _assign_boolean_solver(mod, solver)
    mod.object = cutter_obj

    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = target_obj.evaluated_get(depsgraph)
    baked_mesh = bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers=True, depsgraph=depsgraph)
    old_mesh = target_obj.data
    target_obj.modifiers.clear()
    target_obj.data = baked_mesh
    if old_mesh and old_mesh.users == 0:
        bpy.data.meshes.remove(old_mesh)

    bpy.data.objects.remove(cutter_obj, do_unlink=True)


def apply_boolean_intersect(target_obj, cutter_obj, modifier_name):
    mod = target_obj.modifiers.new(modifier_name, "BOOLEAN")
    mod.operation = "INTERSECT"
    _assign_boolean_solver(mod, "EXACT")
    mod.object = cutter_obj

    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = target_obj.evaluated_get(depsgraph)
    baked_mesh = bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers=True, depsgraph=depsgraph)
    old_mesh = target_obj.data
    target_obj.modifiers.clear()
    target_obj.data = baked_mesh
    if old_mesh and old_mesh.users == 0:
        bpy.data.meshes.remove(old_mesh)

    bpy.data.objects.remove(cutter_obj, do_unlink=True)


def bake_object_modifiers(obj):
    if not obj or obj.type != "MESH":
        return
    if len(obj.modifiers) == 0:
        return
    apply_object_modifier(obj, obj.modifiers[0].name)


def merge_mesh_objects(new_name, objs):
    mesh_objs = []
    seen = set()
    for obj in objs:
        if not obj or obj.type != "MESH":
            continue
        if obj.name in seen:
            continue
        seen.add(obj.name)
        mesh_objs.append(obj)

    if not mesh_objs:
        return None

    for obj in mesh_objs:
        bake_object_modifiers(obj)

    if len(mesh_objs) == 1:
        merged = mesh_objs[0]
        merged.name = new_name
        merged.data.name = f"{new_name}_Mesh"
        return merged

    try:
        view_layer = bpy.context.view_layer
        for obj in bpy.data.objects:
            if obj.select_get():
                obj.select_set(False)

        for obj in mesh_objs:
            obj.select_set(True)
        view_layer.objects.active = mesh_objs[0]

        active = view_layer.objects.active
        if active and active.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")

        bpy.ops.object.join()
        merged = view_layer.objects.active
        merged.name = new_name
        merged.data.name = f"{new_name}_Mesh"
        merged.select_set(False)
        return merged
    except RuntimeError as exc:
        print(f"[WARN] merge_mesh_objects failed for {new_name}: {exc}")
        fallback = mesh_objs[0]
        fallback.name = new_name
        fallback.data.name = f"{new_name}_Mesh"
        return fallback


def append_box(verts, faces, x0, x1, y0, y1, z0, z1):
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z0, z1 = sorted((z0, z1))

    base = len(verts)
    verts.extend(
        [
            (x0, y0, z0),
            (x0, y1, z0),
            (x0, y1, z1),
            (x0, y0, z1),
            (x1, y0, z0),
            (x1, y1, z0),
            (x1, y1, z1),
            (x1, y0, z1),
        ]
    )
    faces.extend(
        [
            (base + 0, base + 1, base + 2, base + 3),
            (base + 4, base + 7, base + 6, base + 5),
            (base + 0, base + 4, base + 5, base + 1),
            (base + 1, base + 5, base + 6, base + 2),
            (base + 2, base + 6, base + 7, base + 3),
            (base + 3, base + 7, base + 4, base + 0),
        ]
    )


def append_extruded_profile_x(verts, faces, x0, x1, profile):
    n = len(profile)
    base = len(verts)

    for y, z in profile:
        verts.append((x0, y, z))
    for y, z in profile:
        verts.append((x1, y, z))

    faces.append(tuple(base + i for i in range(n)))
    faces.append(tuple(base + n + i for i in reversed(range(n))))
    for i in range(n):
        ni = (i + 1) % n
        faces.append((base + i, base + ni, base + n + ni, base + n + i))


def append_loft_profiles_x(verts, faces, sections):
    if len(sections) < 2:
        return

    profile_count = len(sections[0][1])
    base = len(verts)

    for x, profile in sections:
        if len(profile) != profile_count:
            raise ValueError("All loft profiles must have the same vertex count")
        for y, z in profile:
            verts.append((x, y, z))

    for section_index in range(len(sections) - 1):
        start0 = base + section_index * profile_count
        start1 = start0 + profile_count
        for i in range(profile_count):
            ni = (i + 1) % profile_count
            faces.append((start0 + i, start0 + ni, start1 + ni, start1 + i))

    faces.append(tuple(base + i for i in range(profile_count)))
    last = base + (len(sections) - 1) * profile_count
    faces.append(tuple(last + i for i in reversed(range(profile_count))))


def build_box_object(name, x0, x1, y0, y1, z0, z1, r, g, b):
    verts = []
    faces = []
    append_box(verts, faces, x0, x1, y0, y1, z0, z1)
    return make(name, verts, faces, r, g, b, smooth=False)


def cut_box_opening(target_obj, name, x0, x1, y0, y1, z0, z1):
    verts = []
    faces = []
    append_box(verts, faces, x0, x1, y0, y1, z0, z1)
    cutter = make(name, verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(target_obj, cutter, f"{name}_Cut")


def cut_round_opening_x(target_obj, name, x0, x1, y_center, z_center, radius):
    verts = []
    faces = []
    radial = 28
    profile = []
    for i in range(radial):
        a = 2.0 * math.pi * i / radial
        profile.append((y_center + radius * math.cos(a), z_center + radius * math.sin(a)))
    append_extruded_profile_x(verts, faces, x0, x1, profile)
    cutter = make(name, verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(target_obj, cutter, f"{name}_Cut")


def cut_round_opening_z(target_obj, name, z0, z1, x_center, y_center, radius, segments=28):
    verts = []
    faces = []

    for ring_z in (z0, z1):
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            verts.append(
                (
                    x_center + radius * math.cos(a),
                    y_center + radius * math.sin(a),
                    ring_z,
                )
            )

    for i in range(segments):
        ni = (i + 1) % segments
        faces.append((i, ni, segments + ni, segments + i))

    faces.append(tuple(range(segments)))
    faces.append(tuple(segments + i for i in reversed(range(segments))))

    cutter = make(name, verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(target_obj, cutter, f"{name}_Cut")


def build_ballast_armature_frame(name, x_center, y_center, z_center, half_width, half_height):
    foot_z0 = z_center - half_height - 42.0
    foot_z1 = foot_z0 + 8.0
    cradle_z0 = z_center - half_height - 16.0
    cradle_z1 = cradle_z0 + 10.0
    post_z0 = foot_z1
    post_z1 = cradle_z0
    x0 = x_center - 14.0
    x1 = x_center + 14.0
    support_hw = max(34.0, interior_hw(x_center, foot_z1 + 8.0) - 34.0)
    cradle_half_span = min(half_width * 0.70, support_hw)
    leg_offset = min(half_width * 0.56, max(30.0, support_hw - 10.0))
    foot_half_span = 18.0

    build_box_object(
        f"{name}_Cradle",
        x0,
        x1,
        y_center - cradle_half_span,
        y_center + cradle_half_span,
        cradle_z0,
        cradle_z1,
        0.20,
        0.21,
        0.20,
    )
    build_box_object(
        f"{name}_Keel",
        x0,
        x1,
        y_center - 18.0,
        y_center + 18.0,
        foot_z1,
        cradle_z0,
        0.20,
        0.21,
        0.20,
    )
    for suffix, side in (("Port", -1.0), ("Stbd", 1.0)):
        ly = y_center + side * leg_offset
        build_box_object(
            f"{name}_{suffix}Foot",
            x0,
            x1,
            ly - foot_half_span,
            ly + foot_half_span,
            foot_z0,
            foot_z1 + 2.0,
            0.20,
            0.21,
            0.20,
        )
        build_box_object(
            f"{name}_{suffix}Post",
            x0,
            x1,
            ly - 5.0,
            ly + 5.0,
            post_z0,
            post_z1,
            0.20,
            0.21,
            0.20,
        )
        brace_y0 = y_center + side * (leg_offset * 0.38)
        brace_y1 = y_center + side * (leg_offset - 4.0)
        by0, by1 = sorted((brace_y0, brace_y1))
        build_box_object(
            f"{name}_{suffix}Brace",
            x0,
            x1,
            by0,
            by1,
            foot_z1 + 8.0,
            cradle_z0 + 2.0,
            0.20,
            0.21,
            0.20,
        )
    build_box_object(
        f"{name}_MidBand",
        x0,
        x1,
        y_center - cradle_half_span * 0.58,
        y_center + cradle_half_span * 0.58,
        foot_z1 + 14.0,
        foot_z1 + 22.0,
        0.20,
        0.21,
        0.20,
    )


def build_ramp_x_object(name, x0, x1, y0, y1, z0, z1, thickness, r, g, b):
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z_min = min(z0, z1)
    z_max = max(z0, z1)
    t = max(1.0, thickness)

    verts = [
        (x0, y0, z_max),
        (x0, y1, z_max),
        (x1, y1, z_min),
        (x1, y0, z_min),
        (x0, y0, z_max - t),
        (x0, y1, z_max - t),
        (x1, y1, z_min - t),
        (x1, y0, z_min - t),
    ]
    faces = [
        (0, 1, 2, 3),
        (4, 7, 6, 5),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    ]
    return make(name, verts, faces, r, g, b, smooth=False)


def build_ramp_x_oriented(name, x0, x1, y0, y1, z_at_x0, z_at_x1, thickness, r, g, b):
    y0, y1 = sorted((y0, y1))
    t = max(1.0, thickness)
    verts = [
        (x0, y0, z_at_x0),
        (x0, y1, z_at_x0),
        (x1, y1, z_at_x1),
        (x1, y0, z_at_x1),
        (x0, y0, z_at_x0 - t),
        (x0, y1, z_at_x0 - t),
        (x1, y1, z_at_x1 - t),
        (x1, y0, z_at_x1 - t),
    ]
    faces = [
        (0, 1, 2, 3),
        (4, 7, 6, 5),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    ]
    return make(name, verts, faces, r, g, b, smooth=False)


def build_cylinder_x(name, x0, x1, radius, y_center, z_center, r, g, b, segments=18):
    verts = []
    faces = []

    for ring_x in (x0, x1):
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            verts.append(
                (
                    ring_x,
                    y_center + radius * math.cos(a),
                    z_center + radius * math.sin(a),
                )
            )

    for i in range(segments):
        ni = (i + 1) % segments
        faces.append((i, ni, segments + ni, segments + i))

    front = len(verts)
    verts.append((x0, y_center, z_center))
    for i in range(segments):
        faces.append((front, (i + 1) % segments, i))

    back = len(verts)
    verts.append((x1, y_center, z_center))
    for i in range(segments):
        faces.append((back, segments + i, segments + (i + 1) % segments))

    return make(name, verts, faces, r, g, b)


def build_cylinder_z(name, z0, z1, radius, x_center, y_center, r, g, b, segments=20):
    verts = []
    faces = []

    for ring_z in (z0, z1):
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            verts.append(
                (
                    x_center + radius * math.cos(a),
                    y_center + radius * math.sin(a),
                    ring_z,
                )
            )

    for i in range(segments):
        ni = (i + 1) % segments
        faces.append((i, ni, segments + ni, segments + i))

    bottom = len(verts)
    verts.append((x_center, y_center, z0))
    for i in range(segments):
        faces.append((bottom, i, (i + 1) % segments))

    top = len(verts)
    verts.append((x_center, y_center, z1))
    for i in range(segments):
        faces.append((top, segments + (i + 1) % segments, segments + i))

    return make(name, verts, faces, r, g, b)


def build_internal_ballast_tank(name, x0, x1, y_center, z_center, half_width, half_height, r, g, b):
    ring_count = 22
    radial = 20
    verts = []
    faces = []
    cap_ratio = 0.16

    for i in range(ring_count + 1):
        t = i / ring_count
        if t < cap_ratio:
            end_t = t / cap_ratio
            blend = 0.68 + 0.32 * math.sin(end_t * math.pi * 0.5)
        elif t > 1.0 - cap_ratio:
            end_t = (1.0 - t) / cap_ratio
            blend = 0.68 + 0.32 * math.sin(end_t * math.pi * 0.5)
        else:
            blend = 1.0
        x = x0 + (x1 - x0) * t
        hw = max(2.0, half_width * blend)
        hh = max(2.0, half_height * blend)

        for s in range(radial):
            a = 2 * math.pi * s / radial
            ca = math.cos(a)
            sa = math.sin(a)
            # Slight top flattening keeps these readable as tanks, not perfect pills.
            top_flat = 0.90 if sa > 0.0 else 1.0
            y = y_center + hw * ca
            z = z_center + hh * sa * top_flat
            verts.append((x, y, z))

    for i in range(ring_count):
        ring0 = i * radial
        ring1 = (i + 1) * radial
        for s in range(radial):
            ns = (s + 1) % radial
            faces.append((ring0 + s, ring0 + ns, ring1 + ns, ring1 + s))

    faces.append(tuple(i for i in range(radial)))
    last = ring_count * radial
    faces.append(tuple(last + i for i in reversed(range(radial))))

    return make(name, verts, faces, r, g, b)


def build_long_pod(name, x_start_norm, x_end_norm, y_center, z_offset, half_width, half_height, r, g, b):
    verts = []
    faces = []
    ring_count = 40
    radial = 18

    for i in range(ring_count + 1):
        t = i / ring_count
        nx = x_start_norm + (x_end_norm - x_start_norm) * t
        x = nx * LENGTH
        blend = blend_window(t, 0.16, 0.16)
        width = max(1.0, half_width * blend)
        height = max(1.0, half_height * blend)
        z_center = -sample_curve(nx) + z_offset

        for s in range(radial):
            a = 2 * math.pi * s / radial
            verts.append(
                (
                    x,
                    y_center + width * math.cos(a),
                    z_center + height * math.sin(a),
                )
            )

    for i in range(ring_count):
        ring0 = i * radial
        ring1 = (i + 1) * radial
        for s in range(radial):
            ns = (s + 1) % radial
            faces.append((ring0 + s, ring0 + ns, ring1 + ns, ring1 + s))

    faces.append(tuple(i for i in range(radial)))
    last_base = ring_count * radial
    faces.append(tuple(last_base + i for i in reversed(range(radial))))

    return make(name, verts, faces, r, g, b)


def hull_ring(nx):
    r = sample_curve(nx)
    if r < 0.3:
        r = 0.3

    sb, super_w_local, super_h_local, shoulder_norm, _ = superstructure_state(nx)
    super_hw = super_w_local / 2
    shoulder_z = r * shoulder_norm
    shoulder_span = max(1.0, r - shoulder_z)

    ballast_blend = 0.0
    for pair in BALLAST_PAIRS:
        half_len = pair["length_norm"] * 0.5
        start = pair["center_norm"] - half_len
        if pair["length_norm"] <= 0.0:
            continue
        t = (nx - start) / pair["length_norm"]
        ballast_blend = max(ballast_blend, blend_window(t, 0.42, 0.42))

    aft_shoulder_blend = 0.0
    if AFT_UPPER_SHOULDER_END > AFT_UPPER_SHOULDER_START:
        at = (nx - AFT_UPPER_SHOULDER_START) / (AFT_UPPER_SHOULDER_END - AFT_UPPER_SHOULDER_START)
        aft_shoulder_blend = blend_window(at, 0.35, 0.35)

    aft_dorsal_blend = 0.0
    if AFT_DORSAL_END > AFT_DORSAL_START:
        dt = (nx - AFT_DORSAL_START) / (AFT_DORSAL_END - AFT_DORSAL_START)
        aft_dorsal_blend = blend_window(dt, 0.30, 0.45)

    pts = []
    for s in range(RADIAL):
        a = 2 * math.pi * s / RADIAL
        ca, sa = math.cos(a), math.sin(a)
        by, bz = r * ca, r * sa
        by, bz = apply_bow_profile(nx, by, bz, r)

        if ballast_blend > 0.0 and sa < -0.03:
            side = smoothstep((abs(ca) - 0.14) / 0.74)
            depth = smoothstep((-sa - 0.03) / 0.92)
            belly = math.sin(depth * math.pi * 0.92)
            y_extra = r * BALLAST_BULGE_STRENGTH * side * belly * ballast_blend
            z_drop = r * BALLAST_BULGE_DROP * (0.68 + 0.32 * depth) * ballast_blend
            by += math.copysign(y_extra, ca)
            bz -= z_drop

        if aft_shoulder_blend > 0.0 and sa > 0.12:
            side_top = smoothstep((abs(ca) - 0.18) / 0.72) * smoothstep((sa - 0.12) / 0.80)
            bz += r * AFT_UPPER_SHOULDER_STRENGTH * side_top * aft_shoulder_blend

        if aft_dorsal_blend > 0.0 and sa > 0.18:
            center_top = smoothstep((0.36 - abs(ca)) / 0.36) * smoothstep((sa - 0.18) / 0.72)
            bz += r * AFT_DORSAL_STRENGTH * center_top * aft_dorsal_blend

        if sb > 0.0 and bz > shoulder_z:
            upper_t = smoothstep((bz - shoulder_z) / shoulder_span)
            pinch = 1.0 - 0.04 * math.sin(upper_t * math.pi) * sb
            base_hw = abs(by) * pinch
            target_hw = max(base_hw, super_hw * (abs(ca) ** 0.78))
            sign_y = 1.0 if ca >= 0.0 else -1.0
            crown = 0.90 + 0.10 * (1.0 - abs(ca) ** 0.55)
            y = sign_y * (base_hw + (target_hw - base_hw) * upper_t * sb)
            z = bz + super_h_local * upper_t * sb * crown
            pts.append((y, z))
        else:
            pts.append((by, bz))

    return pts


def inner_hull_ring(nx, overlap_cm=0.0):
    thickness = max(1.0, HULL_THICK - overlap_cm)
    profile = []
    for y, z in hull_ring(nx):
        radius = math.sqrt(y * y + z * z)
        if radius <= 1e-4:
            profile.append((y, z))
            continue
        inner_radius = max(1.0, radius - thickness)
        scale = inner_radius / radius
        profile.append((y * scale, z * scale))
    return profile


def build_inner_hull_volume(name, xs_cm, xe_cm, overlap_cm=0.0, sections=20):
    verts = []
    faces = []
    section_count = max(4, sections)
    loft_sections = []
    for index in range(section_count + 1):
        t = index / section_count
        x = xs_cm + (xe_cm - xs_cm) * t
        nx = max(0.0, min(1.0, x / LENGTH))
        loft_sections.append((x, inner_hull_ring(nx, overlap_cm)))
    append_loft_profiles_x(verts, faces, loft_sections)
    return make(name, verts, faces, 0.20, 0.50, 0.20, smooth=False)


def build_hull():
    verts = []
    faces = []

    for i in range(N_RINGS + 1):
        nx = i / N_RINGS
        x = nx * LENGTH
        for y, z in hull_ring(nx):
            verts.append((x, y, z))

    for i in range(N_RINGS):
        for s in range(RADIAL):
            ns = (s + 1) % RADIAL
            a = i * RADIAL + s
            b = i * RADIAL + ns
            c = (i + 1) * RADIAL + ns
            d = (i + 1) * RADIAL + s
            faces.append((a, b, c, d))

    bow_center = len(verts)
    verts.append((0, 0, 0))
    for s in range(RADIAL):
        faces.append((bow_center, (s + 1) % RADIAL, s))

    stern_center = len(verts)
    verts.append((LENGTH, 0, 0))
    last_base = N_RINGS * RADIAL
    for s in range(RADIAL):
        faces.append((stern_center, last_base + s, last_base + (s + 1) % RADIAL))

    return make("SM_Hull", verts, faces, 0.14, 0.18, 0.16)


def build_deck(name, z, xs_n, xe_n, r, g, b, hw_fn=None):
    xs = xs_n * LENGTH
    xe = xe_n * LENGTH
    nx = max(8, int((xe - xs) / 55))
    ny = 22 if name in ("main", "upper") else 18
    # Openings are carved by dedicated boolean recuts later to keep clean round/rect boundaries.
    cutouts = []
    if hw_fn is None:
        hw_fn = interior_hw

    verts = []
    faces = []
    grid = {}
    vi = 0

    for ix in range(nx + 1):
        x = xs + (xe - xs) * ix / nx
        hw = hw_fn(x, z)
        if hw < 15:
            continue
        for iy in range(ny + 1):
            y = -hw + 2 * hw * iy / ny
            verts.append((x, y, z))
            grid[(ix, iy)] = vi
            vi += 1

    for ix in range(nx):
        for iy in range(ny):
            ids = [grid.get(k) for k in ((ix, iy), (ix + 1, iy), (ix + 1, iy + 1), (ix, iy + 1))]
            if not all(idx is not None for idx in ids):
                continue

            cell = [verts[idx] for idx in ids]
            cx = sum(v[0] for v in cell) / 4.0
            cy = sum(v[1] for v in cell) / 4.0
            blocked = any(point_in_cutout(cx, cy, cutout) for cutout in cutouts)
            if not blocked:
                faces.append(tuple(ids))

    if not verts:
        return None

    obj = make(f"SM_Deck_{name}", verts, faces, r, g, b, smooth=False)
    mod = add_solidify(obj, -DECK_THICK, 0)
    apply_object_modifier(obj, mod.name)
    return obj


def build_upper_deck_clipped(z, xs_n, xe_n, r, g, b):
    xs = xs_n * LENGTH
    xe = xe_n * LENGTH
    margin_x = 28.0
    wide_half_width = SUPER_WIDTH * 0.9
    z0 = z - DECK_THICK * 0.5
    z1 = z + DECK_THICK * 0.5
    obj = build_box_object("SM_Deck_upper", xs - margin_x, xe + margin_x, -wide_half_width, wide_half_width, z0, z1, r, g, b)

    cutter = build_inner_hull_volume(
        "SM_UpperEnvelopeCut_Deck",
        xs - margin_x,
        xe + margin_x,
        overlap_cm=DECK_CONTACT_OVERLAP,
        sections=max(14, int((xe - xs) / 120.0)),
    )
    apply_boolean_intersect(obj, cutter, "UpperDeckClip")

    return obj


def cut_standard_door_opening(target_obj, name, x_center, sill_z, width, height, thickness, y_center=0.0):
    rough_half_w = width * 0.5 + DOOR_ROUGH_MARGIN_X
    center_z = sill_z + height * 0.5
    half_h = height * 0.5 + max(2.0, DOOR_ROUGH_MARGIN_Z - 2.0)
    lower_scale = 0.82
    radial = 28
    profile = []
    for i in range(radial):
        a = 2.0 * math.pi * i / radial
        c = math.cos(a)
        s = math.sin(a)
        h = half_h if s >= 0.0 else half_h * lower_scale
        profile.append((y_center + rough_half_w * c, center_z + h * s))

    verts = []
    faces = []
    append_extruded_profile_x(verts, faces, x_center - thickness, x_center + thickness, profile)
    cutter = make(name, verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(target_obj, cutter, f"{name}_Cut")


def recut_round_hatch_on_deck(deck_obj_name, cutout, deck_center_z):
    obj = bpy.data.objects.get(deck_obj_name)
    if obj is None:
        return
    cx = cutout["x_norm"] * LENGTH
    yc = cutout.get("y_center", 0.0)
    radius = cutout["radius"]
    cut_round_opening_z(
        obj,
        f"{deck_obj_name}_{cutout['name']}_RoundRecut",
        deck_center_z - DECK_THICK * 1.2,
        deck_center_z + DECK_THICK * 1.2,
        cx,
        yc,
        radius,
    )


def recut_rect_hatch_on_deck(deck_obj_name, cutout, deck_center_z):
    obj = bpy.data.objects.get(deck_obj_name)
    if obj is None:
        return
    cx = cutout["x_norm"] * LENGTH
    cy = cutout.get("y_center", 0.0)
    hl = cutout["half_length"]
    hw = cutout["half_width"]
    # Rect recuts run after build_upper_deck_clipped, whose EXACT intersect can
    # leave sliver edges that break a subsequent EXACT difference. Use FAST
    # solver with a prior mesh cleanup for tolerance.
    verts = []
    faces = []
    append_box(
        verts,
        faces,
        cx - hl,
        cx + hl,
        cy - hw,
        cy + hw,
        deck_center_z - DECK_THICK * 1.5,
        deck_center_z + DECK_THICK * 1.5,
    )
    cutter = make(f"{deck_obj_name}_{cutout['name']}_RectRecut", verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(obj, cutter, f"{deck_obj_name}_{cutout['name']}_RectRecut_Cut", solver="FAST")


def build_rect_hatch_liner(cutout, deck_center_z, name=None):
    """Close the inner walls of a rect hole cut into a deck.

    The FLOAT/FAST solver sometimes leaves the boundary faces of the cut
    without proper inner walls. This builder drops an explicit liner mesh of
    four vertical walls flush with the hole boundary, pushed 2 cm inward so
    they always sit inside the hole and cover any boolean artifact.

    The walls run from just below the deck underside to just at the deck top
    surface — deliberately flush, no raised coaming extension. The cutout's
    `coaming_height` is kept as data for a future dedicated coaming mesh, but
    this liner does NOT read it (per 2026-04-16 spec: "pas censé ressortir").
    """
    cx = cutout["x_norm"] * LENGTH
    cy = cutout.get("y_center", 0.0)
    hl = cutout["half_length"]
    hw = cutout["half_width"]
    wall_t = 2.0  # cm, thickness of the inner wall strip

    z_under = deck_center_z - DECK_THICK * 0.5 - 0.8
    z_above = deck_center_z + DECK_THICK * 0.5

    verts = []
    faces = []
    # Forward wall
    append_box(
        verts, faces,
        cx - hl, cx - hl + wall_t,
        cy - hw, cy + hw,
        z_under, z_above,
    )
    # Aft wall
    append_box(
        verts, faces,
        cx + hl - wall_t, cx + hl,
        cy - hw, cy + hw,
        z_under, z_above,
    )
    # Port wall
    append_box(
        verts, faces,
        cx - hl, cx + hl,
        cy - hw, cy - hw + wall_t,
        z_under, z_above,
    )
    # Stbd wall
    append_box(
        verts, faces,
        cx - hl, cx + hl,
        cy + hw - wall_t, cy + hw,
        z_under, z_above,
    )
    mesh_name = name or f"SM_{cutout['name']}_Liner"
    return make(mesh_name, verts, faces, 0.28, 0.28, 0.28, smooth=False)


def build_bulkhead_clipped(
    name,
    x_cm,
    z_min,
    z_max,
    door_sill_z=None,
    door_w=0.0,
    door_h=0.0,
    door_y=0.0,
    overlap_cm=BULKHEAD_CONTACT_OVERLAP,
    color=(0.44, 0.42, 0.38),
):
    x_pad = BH_THICK * 1.8
    wide_half_width = SUPER_WIDTH * 1.05
    z0 = z_min
    z1 = z_max + 8.0
    obj = build_box_object(
        name,
        x_cm - BH_THICK * 0.5,
        x_cm + BH_THICK * 0.5,
        -wide_half_width,
        wide_half_width,
        z0,
        z1,
        color[0],
        color[1],
        color[2],
    )

    cutter = build_inner_hull_volume(
        f"{name}_EnvCut",
        x_cm - x_pad,
        x_cm + x_pad,
        overlap_cm=overlap_cm,
        sections=8,
    )
    apply_boolean_intersect(obj, cutter, f"{name}_Clip")

    if door_w > 0.0 and door_h > 0.0 and door_sill_z is not None:
        cut_standard_door_opening(
            obj,
            f"{name}_DoorCut",
            x_cm,
            door_sill_z,
            door_w,
            door_h,
            BH_THICK * 1.2,
            y_center=door_y,
        )
    return obj


def build_upper_bulkhead_clipped(name, x_cm, z_floor, door_w, door_h, door_y=0.0, color=(0.44, 0.42, 0.38)):
    return build_bulkhead_clipped(
        name=name,
        x_cm=x_cm,
        z_min=z_floor,
        z_max=upper_room_apex_z(x_cm, BULKHEAD_CONTACT_OVERLAP),
        door_sill_z=z_floor,
        door_w=door_w,
        door_h=door_h,
        door_y=door_y,
        overlap_cm=BULKHEAD_CONTACT_OVERLAP,
        color=color,
    )


def build_hatch_collar(name, deck_z, cutout, r, g, b):
    verts = []
    faces = []
    cx = cutout["x_norm"] * LENGTH
    hw = cutout["half_width"]
    hl = cutout["half_length"]
    wall = 10.0
    z0 = deck_z - 2.0
    z1 = deck_z + cutout["coaming_height"]
    yc = cutout.get("y_center", 0.0)

    append_box(verts, faces, cx - hl - wall, cx - hl, yc - hw - wall, yc + hw + wall, z0, z1)
    append_box(verts, faces, cx + hl, cx + hl + wall, yc - hw - wall, yc + hw + wall, z0, z1)
    append_box(verts, faces, cx - hl, cx + hl, yc - hw - wall, yc - hw, z0, z1)
    append_box(verts, faces, cx - hl, cx + hl, yc + hw, yc + hw + wall, z0, z1)

    return make(name, verts, faces, r, g, b, smooth=False)


def build_ladder(name, x_center, y_center, z0, z1, rail_spacing=34.0):
    y_center += LADDER_Y_OFFSET
    verts = []
    faces = []
    x0 = x_center - rail_spacing * 0.5
    x1 = x_center + rail_spacing * 0.5
    z0, z1 = sorted((z0, z1))
    rail_t = 3.2
    rung_t = 2.6
    guide_t = 2.4
    guide_y = y_center - 6.0

    # Side rails.
    append_box(verts, faces, x0 - rail_t * 0.5, x0 + rail_t * 0.5, y_center - rail_t * 0.5, y_center + rail_t * 0.5, z0, z1)
    append_box(verts, faces, x1 - rail_t * 0.5, x1 + rail_t * 0.5, y_center - rail_t * 0.5, y_center + rail_t * 0.5, z0, z1)

    # Rear guide rail (behind rungs).
    append_box(
        verts,
        faces,
        x_center - guide_t * 0.5,
        x_center + guide_t * 0.5,
        guide_y - guide_t * 0.5,
        guide_y + guide_t * 0.5,
        z0,
        z1,
    )

    # Rungs.
    rung_count = max(2, int((z1 - z0) / 34.0))
    for i in range(rung_count + 1):
        z = z0 + (z1 - z0) * (i / rung_count)
        append_box(
            verts,
            faces,
            x0 - 1.5,
            x1 + 1.5,
            y_center - rung_t * 0.5,
            y_center + rung_t * 0.5,
            z - rung_t * 0.5,
            z + rung_t * 0.5,
        )

    # Small reinforcement ties between guide rail and rung plane.
    tie_t = 1.6
    for i in range(0, rung_count + 1, 2):
        z = z0 + (z1 - z0) * (i / rung_count)
        append_box(
            verts,
            faces,
            x_center - 4.5,
            x_center + 4.5,
            guide_y + guide_t * 0.5,
            y_center - rung_t * 0.5,
            z - tie_t * 0.5,
            z + tie_t * 0.5,
        )

    # Upper mounting brackets.
    for side_x in (x0, x1):
        append_box(
            verts,
            faces,
            side_x - 2.0,
            side_x + 2.0,
            y_center - 2.0,
            y_center + 2.0,
            z1 - 10.0,
            z1 + 10.0,
        )

    return make(name, verts, faces, 0.26, 0.26, 0.25, smooth=False)


def build_round_hatches_and_ladders(upper_z):
    for cutout in MAIN_DECK_CUTOUTS:
        if cutout.get("shape") != "round":
            continue
        recut_round_hatch_on_deck("SM_Deck_main", cutout, DECK_MAIN_Z)
        x = cutout["x_norm"] * LENGTH
        y = cutout.get("y_center", 0.0)
        radius = cutout["radius"]
        build_cylinder_z(
            f"SM_HatchDoor_{cutout['name']}",
            DECK_MAIN_Z + DECK_THICK * 0.5 + 2.0,
            DECK_MAIN_Z + DECK_THICK * 0.5 + 8.0,
            radius * 0.82,
            x + radius * 0.92,
            y,
            0.30,
            0.31,
            0.30,
            segments=20,
        )
        build_ladder(
            f"SM_Ladder_LowerToMain_{cutout['name']}",
            x,
            y,
            DECK_LOWER_Z + DECK_THICK * 0.5 + 4.0,
            DECK_MAIN_Z + 180.0,
        )

    for cutout in UPPER_DECK_CUTOUTS:
        x = cutout["x_norm"] * LENGTH
        y = cutout.get("y_center", 0.0)
        if cutout.get("shape") == "round":
            recut_round_hatch_on_deck("SM_Deck_upper", cutout, upper_z)
        elif cutout.get("shape") == "rect":
            recut_rect_hatch_on_deck("SM_Deck_upper", cutout, upper_z)
            # Explicit liner so the four inner walls of the hole always read
            # as closed, regardless of boolean solver quirks.
            build_rect_hatch_liner(cutout, upper_z)

        # Main->upper access via steep stair only (no ladder).
        # Stair X offset relative to cutout center was -240 cm in the original,
        # but the top step was landing too far forward of the hole. Shifted
        # +113 cm on X per 2026-04-16 user spec so the arrival meets the aft
        # edge of the hole and the player gets deck in front after the climb.
        #
        # z_top is chosen so that the top tread TOP surface is flush with the
        # upper deck TOP surface (i.e. tread_top_z == upper_z + DECK_THICK/2).
        # Tread thickness is 3.5 cm, so z_top (step bottom) = upper_z + DECK/2 - 3.5.
        step_count = 11
        tread_thickness = 3.5
        stair_x0 = x - 127.0
        stair_x1 = stair_x0 + 208.0
        run = (stair_x1 - stair_x0) / step_count
        z_bottom = DECK_MAIN_Z + DECK_THICK * 0.5 + 2.0
        z_top = upper_z + DECK_THICK * 0.5 - tread_thickness
        # Keep one extra step while making the last tread exactly meet arrival height.
        rise = (z_top - z_bottom) / max(1, (step_count - 1))
        stair_half_w = 32.0
        rail_outer = stair_half_w + 10.0
        handrail_height = 95.0  # cm above tread, ergonomic standard
        stair_parts = []

        def stair_z_at(xv):
            t = (xv - stair_x0) / max(1.0, stair_x1 - stair_x0)
            t = max(0.0, min(1.0, t))
            return z_bottom + (z_top - z_bottom) * t

        for i in range(step_count):
            sx0 = stair_x0 + run * i
            sx1 = sx0 + run * 0.9
            sz0 = z_bottom + rise * i
            sz1 = sz0 + 3.5
            stair_parts.append(build_box_object(
                f"SM_Stair_UpperToMain_{cutout['name']}_{i}",
                sx0,
                sx1,
                y - stair_half_w,
                y + stair_half_w,
                sz0,
                sz1,
                0.28,
                0.28,
                0.27,
            ))

        # Side stringers under the stair.
        stair_parts.append(build_ramp_x_oriented(
            f"SM_StairStringer_{cutout['name']}_Port",
            stair_x0,
            stair_x1,
            y - stair_half_w - 3.0,
            y - stair_half_w + 1.0,
            z_bottom - 10.0,
            z_top - 10.0,
            5.0,
            0.24,
            0.24,
            0.23,
        ))
        stair_parts.append(build_ramp_x_oriented(
            f"SM_StairStringer_{cutout['name']}_Stbd",
            stair_x0,
            stair_x1,
            y + stair_half_w - 1.0,
            y + stair_half_w + 3.0,
            z_bottom - 10.0,
            z_top - 10.0,
            5.0,
            0.24,
            0.24,
            0.23,
        ))

        # Handrails aligned with stair direction, parallel to the tread slope
        # at a constant 95 cm above the tread top surface.
        rail_z0 = z_bottom + tread_thickness + handrail_height
        rail_z1 = z_top + tread_thickness + handrail_height
        stair_parts.append(build_ramp_x_oriented(
            f"SM_StairRail_{cutout['name']}_Port",
            stair_x0,
            stair_x1,
            y - rail_outer - 2.0,
            y - rail_outer + 2.0,
            rail_z0,
            rail_z1,
            2.2,
            0.24,
            0.24,
            0.23,
        ))
        stair_parts.append(build_ramp_x_oriented(
            f"SM_StairRail_{cutout['name']}_Stbd",
            stair_x0,
            stair_x1,
            y + rail_outer - 2.0,
            y + rail_outer + 2.0,
            rail_z0,
            rail_z1,
            2.2,
            0.24,
            0.24,
            0.23,
        ))
        # Posts go from tread top up to the rail at a constant 95 cm height.
        for side, suffix in ((-1.0, "Port"), (1.0, "Stbd")):
            py = y + side * rail_outer
            for i in range(6):
                t = i / 5.0
                px = stair_x0 + (stair_x1 - stair_x0) * t
                tread_top_z = stair_z_at(px) + tread_thickness
                pz0 = tread_top_z
                pz1 = tread_top_z + handrail_height + 2.0
                stair_parts.append(build_box_object(
                    f"SM_StairRailPost_{cutout['name']}_{suffix}_{i}",
                    px - 1.8,
                    px + 1.8,
                    py - 1.8,
                    py + 1.8,
                    pz0,
                    pz1,
                    0.24,
                    0.24,
                    0.23,
                ))

        # Under-stair supports from main deck.
        for i, px in enumerate((stair_x0 + 58.0, stair_x0 + 136.0)):
            z0 = DECK_MAIN_Z + DECK_THICK * 0.5 + 2.0
            z1 = stair_z_at(px) - 10.5
            if z1 <= z0 + 8.0:
                continue
            stair_parts.append(build_box_object(
                f"SM_StairSupport_{cutout['name']}_{i}",
                px - 3.0,
                px + 3.0,
                y - 9.0,
                y + 9.0,
                z0,
                z1,
                0.24,
                0.24,
                0.23,
            ))

        # Hangers up to upper deck underside.
        hanger_z0 = z_top + 24.0
        hanger_z1 = deck_bottom_z(upper_z) - 2.0
        if hanger_z1 > hanger_z0 + 6.0:
            for side, suffix in ((-1.0, "Port"), (1.0, "Stbd")):
                py = y + side * rail_outer
                for i, px in enumerate((stair_x1 - 16.0, stair_x1 - 54.0)):
                    stair_parts.append(build_box_object(
                        f"SM_StairHanger_{cutout['name']}_{suffix}_{i}",
                        px - 1.8,
                        px + 1.8,
                        py - 1.8,
                        py + 1.8,
                        hanger_z0,
                        hanger_z1,
                        0.24,
                        0.24,
                        0.23,
                    ))

        merge_mesh_objects(
            f"SM_Stair_UpperToMain_{cutout['name']}",
            stair_parts,
        )

    for cutout in LOWER_DECK_CUTOUTS:
        if cutout.get("shape") != "round":
            continue
        recut_round_hatch_on_deck("SM_Deck_lower_main", cutout, DECK_LOWER_Z)
        x = cutout["x_norm"] * LENGTH
        y = cutout.get("y_center", 0.0)
        radius = cutout["radius"]
        door_name = "SM_HatchDoor_FondAccess" if cutout["name"] == "FondAccess" else f"SM_HatchDoor_{cutout['name']}"
        build_cylinder_z(
            door_name,
            DECK_LOWER_Z + DECK_THICK * 0.5 + 1.5,
            DECK_LOWER_Z + DECK_THICK * 0.5 + 7.5,
            radius * 0.84,
            x + radius * 0.84,
            y,
            0.30,
            0.31,
            0.30,
            segments=20,
        )
        hull_floor_z = -sample_curve(cutout["x_norm"]) + HULL_THICK + 16.0
        build_ladder(
            "SM_Ladder_LowerToFondAccess",
            x,
            y,
            hull_floor_z,
            DECK_LOWER_Z - DECK_THICK * 0.5 - 4.0,
        )

    # Engine upper deck access ladder down toward hull floor.
    engine_x = 0.865 * LENGTH
    hull_floor_z = -sample_curve(0.865) + HULL_THICK + 16.0
    build_ladder(
        "SM_Ladder_EngineUpper_ToHullFloor",
        engine_x,
        0.0,
        hull_floor_z,
        -98.0,
    )


def build_hydroplanes():
    """Bow + stern hydroplanes with tapered NACA-style silhouette and root fairing.

    Bow and stern stay separate (they pivot independently for depth control).
    A small conical pod at the root hides the insertion into the hull.
    Solidify is APPLIED so pivots compute correctly after the pass.
    """
    grouped = {"Bow": [], "Stern": []}
    fairing_parts = []
    for name, xn, span, chord in (("Bow", 0.12, 180.0, 130.0), ("Stern", 0.88, 180.0, 130.0)):
        x = xn * LENGTH
        r = sample_curve(xn)
        half_chord_root = chord * 0.5
        half_chord_tip = chord * 0.30
        le_fillet = 14.0
        for side, label in ((1.0, "Stbd"), (-1.0, "Port")):
            # 6-vertex NACA-ish silhouette: sharp-tapered tip with rounded root LE.
            y_root = r * side
            y_tip = (r + span) * side
            verts = [
                (x - half_chord_root + le_fillet, y_root, 0.0),   # 0 root LE fillet start
                (x + half_chord_root,             y_root, 0.0),   # 1 root TE
                (x + half_chord_root * 0.55,      (r + span * 0.55) * side, 0.0),  # 2 mid TE
                (x + half_chord_tip * 0.6,        y_tip, 0.0),    # 3 tip TE
                (x - half_chord_tip + 8.0,        y_tip, 0.0),    # 4 tip LE
                (x - half_chord_root * 0.7,       (r + span * 0.55) * side, 0.0),  # 5 mid LE
                (x - half_chord_root,             y_root, 0.0),   # 6 root LE
            ]
            faces = [
                (0, 1, 2, 5),
                (5, 2, 3, 4),
                (6, 0, 5),
            ]
            obj = make(f"SM_Hydro_{name}_{label}", verts, faces, 0.16, 0.20, 0.18, smooth=False)
            mod = add_solidify(obj, 12.0, 0)
            apply_object_modifier(obj, mod.name)
            grouped[name].append(obj)

            # Root fairing: small ellipsoidal-ish box covering the hull-to-plane joint.
            fair_x0 = x - half_chord_root * 0.55
            fair_x1 = x + half_chord_root * 0.55
            fair_y_inner = y_root - side * 6.0
            fair_y_outer = y_root + side * 18.0
            fair_z0 = -18.0
            fair_z1 = 18.0
            fy0, fy1 = sorted((fair_y_inner, fair_y_outer))
            fairing_parts.append(build_box_object(
                f"SM_Hydro_{name}_Fairing_{label}",
                fair_x0,
                fair_x1,
                fy0,
                fy1,
                fair_z0,
                fair_z1,
                0.16,
                0.18,
                0.17,
            ))

    merged = []
    for name in ("Bow", "Stern"):
        merged_obj = merge_mesh_objects(f"SM_Hydro_{name}", grouped[name])
        if merged_obj:
            merged.append(merged_obj)
    if fairing_parts:
        merge_mesh_objects("SM_Hydro_Fairings", fairing_parts)
    return merged


def build_fins():
    """Four X-shaped tail fins with rounded leading edge and root-to-tip taper.

    Geometry: 6-vertex planar silhouette per fin (swept leading edge + tapered
    tip + fillet at root). Solidify is APPLIED so the pivot reader sees the
    finished shell. All four fins merge into SM_FinAssembly since they are
    structural (no independent motion).
    """
    fx = 0.955 * LENGTH
    r = sample_curve(0.955)
    half_chord_root = 95.0
    half_chord_tip = 35.0
    le_fillet = 22.0
    te_taper = 16.0
    span = 220.0
    parts = []

    for angle_deg in (45, 135, 225, 315):
        a = math.radians(angle_deg)
        ca, sa = math.cos(a), math.sin(a)

        def at(r_local, dx):
            return (fx + dx, r_local * ca, r_local * sa)

        r_root = r
        r_mid = r + span * 0.5
        r_tip = r + span

        verts = [
            at(r_root, -half_chord_root + le_fillet),             # 0 root LE fillet start
            at(r_root, half_chord_root),                          # 1 root TE
            at(r_mid,  half_chord_root * 0.55),                   # 2 mid TE
            at(r_tip,  half_chord_tip * 0.6),                     # 3 tip TE
            at(r_tip, -half_chord_tip + te_taper),                # 4 tip LE
            at(r_mid, -half_chord_root * 0.7),                    # 5 mid LE
            at(r_root, -half_chord_root),                         # 6 root LE tip
        ]
        faces = [
            (0, 1, 2, 5),
            (5, 2, 3, 4),
            (6, 0, 5),      # leading-edge fillet triangle
        ]
        obj = make(f"SM_Fin_{angle_deg}", verts, faces, 0.16, 0.20, 0.18, smooth=False)
        mod = add_solidify(obj, 14.0, 0)
        apply_object_modifier(obj, mod.name)
        parts.append(obj)

    return [merge_mesh_objects("SM_FinAssembly", parts)]


def build_rudder():
    nx = 0.955
    x_root = nx * LENGTH
    hull_r = sample_curve(nx)
    rudder_parts = []

    fairing_verts = [
        (x_root - 92.0, 0.0, hull_r * 0.28),
        (x_root + 8.0, 0.0, hull_r * 0.24),
        (x_root - 8.0, 0.0, hull_r + 118.0),
        (x_root - 86.0, 0.0, hull_r + 128.0),
    ]
    fairing = make("SM_Rudder_Fairing", fairing_verts, [(0, 1, 2, 3)], 0.16, 0.19, 0.17, smooth=False)
    add_solidify(fairing, 18.0, 0)
    rudder_parts.append(fairing)

    post_verts = [
        (x_root - 18.0, 0.0, hull_r * 0.30),
        (x_root + 36.0, 0.0, hull_r * 0.26),
        (x_root + 22.0, 0.0, hull_r + 158.0),
        (x_root - 10.0, 0.0, hull_r + 170.0),
    ]
    post = make("SM_Rudder_Post", post_verts, [(0, 1, 2, 3)], 0.17, 0.20, 0.18, smooth=False)
    add_solidify(post, 16.0, 0)
    rudder_parts.append(post)

    blade_verts = [
        (x_root + 42.0, 0.0, hull_r * 0.34),
        (x_root + 164.0, 0.0, hull_r * 0.22),
        (x_root + 138.0, 0.0, hull_r + 238.0),
        (x_root + 34.0, 0.0, hull_r + 216.0),
    ]
    blade = make("SM_Rudder", blade_verts, [(0, 1, 2, 3)], 0.19, 0.22, 0.20, smooth=False)
    add_solidify(blade, 14.0, 0)
    rudder_parts.append(blade)

    tip_verts = [
        (x_root + 126.0, 0.0, hull_r + 152.0),
        (x_root + 184.0, 0.0, hull_r + 142.0),
        (x_root + 172.0, 0.0, hull_r + 208.0),
        (x_root + 118.0, 0.0, hull_r + 216.0),
    ]
    tip = make("SM_Rudder_Tip", tip_verts, [(0, 1, 2, 3)], 0.17, 0.20, 0.18, smooth=False)
    add_solidify(tip, 12.0, 0)
    rudder_parts.append(tip)

    skeg_verts = [
        (x_root - 44.0, 0.0, -hull_r * 0.58),
        (x_root + 74.0, 0.0, -hull_r * 0.62),
        (x_root + 56.0, 0.0, -hull_r - 146.0),
        (x_root - 28.0, 0.0, -hull_r - 126.0),
    ]
    skeg = make("SM_Skeg", skeg_verts, [(0, 1, 2, 3)], 0.16, 0.18, 0.17, smooth=False)
    add_solidify(skeg, 13.0, 0)
    merge_mesh_objects("SM_RudderAssembly", rudder_parts)


def build_propulsion_room_layout():
    x_reactor0 = 0.705 * LENGTH
    x_reactor1 = 0.775 * LENGTH
    x_engine0 = 0.862 * LENGTH
    x_engine1 = 0.934 * LENGTH
    main_z = DECK_MAIN_Z
    engine_deck_z = -96.0
    mezz_z = -62.0
    shaft_z = 0.0

    # Two deck levels only: transition deck + engine deck.
    build_box_object(
        "SM_Deck_engine_upper",
        0.782 * LENGTH,
        0.848 * LENGTH,
        -96.0,
        96.0,
        main_z,
        main_z + DECK_THICK,
        0.27,
        0.26,
        0.24,
    )
    build_box_object(
        "SM_Deck_engine_lower",
        0.868 * LENGTH,
        0.942 * LENGTH,
        -112.0,
        112.0,
        engine_deck_z,
        engine_deck_z + DECK_THICK,
        0.27,
        0.26,
        0.24,
    )

    ramp_x0 = 0.848 * LENGTH
    ramp_x1 = ramp_x0 + 100.0
    build_ramp_x_object(
        "SM_EngineRamp",
        ramp_x0,
        ramp_x1,
        -94.0,
        94.0,
        main_z + DECK_THICK,
        engine_deck_z + DECK_THICK,
        6.0,
        0.24,
        0.24,
        0.23,
    )

    del mezz_z

    build_box_object(
        "SM_Hatch_Engine_SideService",
        0.892 * LENGTH,
        0.924 * LENGTH,
        -112.0,
        -80.0,
        engine_deck_z + 10.0 + DECK_THICK,
        engine_deck_z + 10.0 + DECK_THICK + 8.0,
        0.30,
        0.31,
        0.30,
    )

    # Central reactor with side passages.
    build_box_object(
        "SM_Reactor_Block",
        x_reactor0,
        x_reactor1,
        -66.0,
        66.0,
        main_z - 116.0,
        main_z + 58.0,
        0.20,
        0.18,
        0.16,
    )
    build_box_object(
        "SM_ReactorPassage_Port",
        x_reactor0 - 22.0,
        x_reactor1 + 16.0,
        -152.0,
        -96.0,
        main_z - 24.0,
        main_z - 8.0,
        0.23,
        0.24,
        0.23,
    )
    build_box_object(
        "SM_ReactorPassage_Stbd",
        x_reactor0 - 22.0,
        x_reactor1 + 16.0,
        96.0,
        152.0,
        main_z - 24.0,
        main_z - 8.0,
        0.23,
        0.24,
        0.23,
    )

    # Engine block is centered and aligned to driveshaft axis.
    build_box_object(
        "SM_Engine_Block",
        x_engine0,
        x_engine1,
        -58.0,
        58.0,
        engine_deck_z - 80.0,
        shaft_z + 12.0,
        0.18,
        0.17,
        0.16,
    )

    # Conduits from reactor to engine.
    build_cylinder_x(
        "SM_ReactorLine_Port",
        x_reactor1,
        x_engine0,
        10.0,
        -38.0,
        shaft_z + 8.0,
        0.16,
        0.17,
        0.16,
    )
    build_cylinder_x(
        "SM_ReactorLine_Stbd",
        x_reactor1,
        x_engine0,
        10.0,
        38.0,
        shaft_z + 8.0,
        0.16,
        0.17,
        0.16,
    )

    # Shaft is the spatial truth: engine and stern alignment are keyed off this axis.
    build_cylinder_x(
        "SM_DriveShaft",
        0.885 * LENGTH,
        LENGTH + 124.0,
        12.0,
        0.0,
        shaft_z,
        0.18,
        0.18,
        0.18,
        segments=20,
    )


def build_propulsor():
    rotating_parts = []
    static_parts = []
    x = LENGTH - 8.0
    stern_r = sample_curve(0.995)
    duct_r = stern_r * 2.22
    duct_inner = duct_r - 24.0
    duct_len = 138.0
    hub_r = duct_inner * 0.36
    rotor_r = duct_inner * 0.52
    n = 28
    n_blades = 7
    blade_r = duct_inner - 6.0

    verts = []
    faces = []
    for iz in range(2):
        xx = x + duct_len * iz
        for i in range(n):
            a = 2 * math.pi * i / n
            verts.append((xx, duct_r * math.cos(a), duct_r * math.sin(a)))

    for i in range(n):
        ni = (i + 1) % n
        faces.append((i, ni, n + ni, n + i))

    inner_base = len(verts)
    for iz in range(2):
        xx = x + duct_len * iz
        for i in range(n):
            a = 2 * math.pi * i / n
            verts.append((xx, duct_inner * math.cos(a), duct_inner * math.sin(a)))

    for i in range(n):
        ni = (i + 1) % n
        faces.append((inner_base + i, inner_base + n + i, inner_base + n + ni, inner_base + ni))

    for i in range(n):
        ni = (i + 1) % n
        faces.append((i, inner_base + i, inner_base + ni, ni))
        faces.append((n + i, n + ni, inner_base + n + ni, inner_base + n + i))

    duct = make("SM_Duct", verts, faces, 0.20, 0.22, 0.20)
    static_parts.append(duct)

    hub_verts = []
    hub_faces = []
    hub_n = 16
    hub_sections = (
        (0.16, 1.00),
        (0.46, 0.95),
        (0.72, 0.70),
        (0.96, 0.24),
    )
    for t, radius_scale in hub_sections:
        xx = x + duct_len * t
        cr = hub_r * radius_scale
        for i in range(hub_n):
            a = 2 * math.pi * i / hub_n
            hub_verts.append((xx, cr * math.cos(a), cr * math.sin(a)))

    for iz in range(len(hub_sections) - 1):
        for i in range(hub_n):
            ni = (i + 1) % hub_n
            hub_faces.append((iz * hub_n + i, iz * hub_n + ni, (iz + 1) * hub_n + ni, (iz + 1) * hub_n + i))

    tip_center = len(hub_verts)
    hub_verts.append((x + duct_len * 0.98, 0, 0))
    for i in range(hub_n):
        hub_faces.append(
            (
                tip_center,
                (len(hub_sections) - 1) * hub_n + i,
                (len(hub_sections) - 1) * hub_n + (i + 1) % hub_n,
            )
        )

    front_center = len(hub_verts)
    hub_verts.append((x + duct_len * 0.14, 0, 0))
    for i in range(hub_n):
        hub_faces.append((front_center, (i + 1) % hub_n, i))

    hub_obj = make("SM_Hub", hub_verts, hub_faces, 0.30, 0.28, 0.25)
    rotating_parts.append(hub_obj)

    rotor_x0 = x + duct_len * 0.44
    rotor_x1 = x + duct_len * 0.58
    rotor_verts = []
    rotor_faces = []
    rotor_n = 20
    for iz, xx in enumerate((rotor_x0, rotor_x1)):
        for i in range(rotor_n):
            a = 2 * math.pi * i / rotor_n
            rotor_verts.append((xx, rotor_r * math.cos(a), rotor_r * math.sin(a)))
    for i in range(rotor_n):
        ni = (i + 1) % rotor_n
        rotor_faces.append((i, ni, rotor_n + ni, rotor_n + i))
    rotor_obj = make("SM_Rotor", rotor_verts, rotor_faces, 0.29, 0.27, 0.24)
    rotating_parts.append(rotor_obj)

    ring_obj = build_cylinder_x(
        "SM_RotorHubRing",
        rotor_x0 + 3.0,
        rotor_x1 - 3.0,
        rotor_r * 0.98,
        0.0,
        0.0,
        0.30,
        0.28,
        0.25,
        segments=18,
    )
    rotating_parts.append(ring_obj)

    blade_x_center = x + duct_len * 0.50
    blade_root = rotor_r * 0.99
    for blade_index in range(n_blades):
        blade_angle = 2 * math.pi * blade_index / n_blades
        cb, sb = math.cos(blade_angle), math.sin(blade_angle)
        twist = 26.0
        blade_verts = [
            (blade_x_center + twist * 0.48, blade_root * cb, blade_root * sb),
            (blade_x_center - twist * 0.48, blade_root * cb, blade_root * sb),
            (blade_x_center + twist * 0.24, blade_r * 0.66 * cb, blade_r * 0.66 * sb),
            (blade_x_center - twist * 0.24, blade_r * 0.66 * cb, blade_r * 0.66 * sb),
            (blade_x_center + twist * 0.02, blade_r * cb, blade_r * sb),
            (blade_x_center - twist * 0.02, blade_r * cb, blade_r * sb),
        ]
        blade_faces = [(0, 1, 3, 2), (2, 3, 5, 4)]
        obj = make(f"SM_Blade_{blade_index}", blade_verts, blade_faces, 0.35, 0.28, 0.22, smooth=False)
        add_solidify(obj, 6.0, 0)
        rotating_parts.append(obj)

    stator_count = 6
    stator_x0 = x + duct_len * 0.90
    stator_x1 = x + duct_len * 0.985
    stator_root = rotor_r * 0.98
    stator_tip = duct_inner - 4.0
    stator_thickness = 3.6
    stator_phase = math.pi / (n_blades * 1.4)
    for index in range(stator_count):
        angle = 2 * math.pi * index / stator_count + stator_phase
        ca = math.cos(angle)
        sa = math.sin(angle)
        pa = -sa
        pb = ca
        verts = [
            (stator_x0, stator_root * ca - pa * stator_thickness, stator_root * sa - pb * stator_thickness),
            (stator_x1, stator_tip * ca - pa * stator_thickness, stator_tip * sa - pb * stator_thickness),
            (stator_x1, stator_tip * ca + pa * stator_thickness, stator_tip * sa + pb * stator_thickness),
            (stator_x0, stator_root * ca + pa * stator_thickness, stator_root * sa + pb * stator_thickness),
        ]
        static_parts.append(make(f"SM_Stator_{index}", verts, [(0, 1, 2, 3)], 0.20, 0.21, 0.20, smooth=False))

    merged_rotor = merge_mesh_objects("SM_Propeller", rotating_parts)
    merged_static = merge_mesh_objects("SM_Propulsor_Duct", static_parts)
    return [obj for obj in (merged_rotor, merged_static) if obj]


def airlock_spec(upper_deck_z):
    x_back = UPPER_AIRLOCK_EXIT_BULKHEAD["x_norm"] * LENGTH
    x_front = UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH
    depth = x_back - x_front
    nx_mid = ((x_front + x_back) * 0.5) / LENGTH

    _, super_w_local, super_h_local, _, _ = superstructure_state(nx_mid)
    opening_hw = DOOR_W * 0.5
    sill_z = upper_deck_z
    opening_top_z = sill_z + DOOR_H
    terrace_floor_z = upper_deck_z - 18.0
    terrace_top_z = opening_top_z + max(24.0, min(38.0, super_h_local * 0.14))
    cassette_outer_hw = min(94.0, super_w_local * 0.20)
    pocket_hw = min(132.0, cassette_outer_hw + 34.0)
    terrace_hw = min(168.0, pocket_hw + 26.0)

    return {
        "x_front": x_front,
        "x_back": x_back,
        "length": depth,
        "opening_hw": opening_hw,
        "opening_top_z": opening_top_z,
        "sill_z": sill_z,
        "terrace_floor_z": terrace_floor_z,
        "terrace_top_z": terrace_top_z,
        "cassette_outer_hw": cassette_outer_hw,
        "pocket_hw": pocket_hw,
        "terrace_hw": terrace_hw,
        "terrace_start_x": x_back - AIRLOCK_TERRACE_START_OFFSET,
        "terrace_flat_x": x_back + AIRLOCK_TERRACE_FLAT_OFFSET,
        "terrace_recover_mid_x": x_back + AIRLOCK_TERRACE_RECOVER_MID_OFFSET,
        "terrace_end_x": x_back + AIRLOCK_TERRACE_RECOVER_OFFSET,
        "cassette_depth": AIRLOCK_CASSETTE_DEPTH,
    }


def build_airlock_exit_terrace_profile(half_width, z_bottom, z_top, corner=10.0):
    corner = max(2.0, min(corner, half_width * 0.25, (z_top - z_bottom) * 0.18))
    return [
        (-half_width + corner, z_bottom),
        (half_width - corner, z_bottom),
        (half_width, z_bottom + corner),
        (half_width, z_top - corner),
        (half_width - corner, z_top),
        (-half_width + corner, z_top),
        (-half_width, z_top - corner),
        (-half_width, z_bottom + corner),
    ]


def upper_z_for_airlock(spec):
    return spec["sill_z"]


def cut_airlock_exit_pocket(hull_obj, spec):
    cut_box_opening(
        hull_obj,
        "SM_Airlock_ExitDoorway",
        spec["x_back"] - 18.0,
        LENGTH + 36.0,
        -(spec["opening_hw"] + 12.0),
        spec["opening_hw"] + 12.0,
        spec["sill_z"] - 4.0,
        spec["opening_top_z"] + 16.0,
    )

    entry_profile = build_airlock_exit_terrace_profile(
        spec["opening_hw"] + 8.0,
        spec["sill_z"] - 2.0,
        spec["opening_top_z"] + 8.0,
        4.0,
    )
    face_profile = build_airlock_exit_terrace_profile(
        spec["pocket_hw"],
        spec["terrace_floor_z"],
        spec["terrace_top_z"],
        10.0,
    )
    flat_profile = build_airlock_exit_terrace_profile(
        spec["terrace_hw"],
        spec["terrace_floor_z"],
        spec["terrace_top_z"] + 4.0,
        12.0,
    )
    recover_mid_profile = build_airlock_exit_terrace_profile(
        spec["terrace_hw"] * 0.72,
        spec["terrace_floor_z"] + 48.0,
        spec["terrace_top_z"] - 22.0,
        10.0,
    )
    recover_end_profile = build_airlock_exit_terrace_profile(
        spec["opening_hw"] + 24.0,
        spec["terrace_floor_z"] + 132.0,
        spec["opening_top_z"] + 18.0,
        8.0,
    )

    verts = []
    faces = []
    append_loft_profiles_x(
        verts,
        faces,
        [
            (spec["x_back"] - 18.0, entry_profile),
            (spec["x_back"] + spec["cassette_depth"], face_profile),
            (spec["terrace_flat_x"], flat_profile),
            (spec["terrace_recover_mid_x"], recover_mid_profile),
            (spec["terrace_end_x"], recover_end_profile),
        ],
    )
    cutter = make("SM_Airlock_ExitCutter", verts, faces, 0.8, 0.1, 0.1, smooth=False)
    apply_boolean_difference(hull_obj, cutter, "AirlockExitCut")


def build_airlock_exit_cassette(spec):
    frame_x0 = spec["x_back"] - 6.0
    frame_x1 = frame_x0 + spec["cassette_depth"]
    opening_hw = spec["opening_hw"]
    cassette_hw = spec["cassette_outer_hw"]
    sill_z = spec["sill_z"]
    head_z = spec["opening_top_z"]

    # Four frame pieces (Port/Stbd walls, Header ceiling, Sill floor) merged
    # into a single SM_Airlock_Cassette static mesh. The former separate "Back"
    # piece was a 4 cm slab behind the battants that served no gameplay purpose
    # and visually sealed the passage; it has been removed per 2026-04-16 spec.
    parts = []
    parts.append(build_box_object(
        "SM_Airlock_Cassette_Port",
        frame_x0,
        frame_x1,
        -cassette_hw,
        -opening_hw,
        sill_z - 10.0,
        head_z + 18.0,
        0.28,
        0.30,
        0.29,
    ))
    parts.append(build_box_object(
        "SM_Airlock_Cassette_Stbd",
        frame_x0,
        frame_x1,
        opening_hw,
        cassette_hw,
        sill_z - 10.0,
        head_z + 18.0,
        0.28,
        0.30,
        0.29,
    ))
    parts.append(build_box_object(
        "SM_Airlock_Cassette_Header",
        frame_x0,
        frame_x1,
        -cassette_hw,
        cassette_hw,
        head_z,
        head_z + 18.0,
        0.28,
        0.30,
        0.29,
    ))
    parts.append(build_box_object(
        "SM_Airlock_Cassette_Sill",
        frame_x0,
        frame_x1,
        -cassette_hw,
        cassette_hw,
        sill_z - 10.0,
        sill_z,
        0.28,
        0.30,
        0.29,
    ))
    merge_mesh_objects("SM_Airlock_Cassette", parts)

    # Sliding battants: panel_gap=2, panel_margin=4 gives a 96x192 cm cover of
    # a 100x200 cm opening. Character is 180 cm tall so the 190 cm clearance
    # budget is preserved with 10 cm to spare. Seal strip is welded into the
    # Port battant mesh (bulkheads_doors handles that).
    build_sliding_split_door(
        name_prefix="SM_Airlock_Door",
        x_center=frame_x0 + 10.0,
        sill_z=sill_z,
        make_fn=make,
        append_box_fn=append_box,
        width=DOOR_W,
        height=DOOR_H,
        panel_gap=2.0,
        panel_margin=4.0,
        panel_depth=7.0,
        frame_depth=12.0,
        seal_center_width=2.0,
        color=(0.33, 0.35, 0.34),
    )

    # Manual SAS hull cutter.
    # The script's automatic cut_airlock_exit_pocket + solidify chain sometimes
    # leaves the hull not fully pierced at the battant level. Ship a ready-made
    # cutter mesh so the user can select hull + cutter and apply Boolean
    # Difference by hand in Blender. Sized to cover the doorway with a comfort
    # margin and extend well past the aft of the hull.
    cutter_x0 = spec["x_back"] - 24.0
    cutter_x1 = LENGTH + 160.0
    cutter_hw = spec["opening_hw"] + 14.0          # 64 cm half-width -> 128 cm wide
    cutter_z0 = spec["sill_z"] - 8.0
    cutter_z1 = spec["opening_top_z"] + 24.0        # >=190 cm vertical clearance
    build_box_object(
        "SM_SAS_HullCutter_Manual",
        cutter_x0,
        cutter_x1,
        -cutter_hw,
        cutter_hw,
        cutter_z0,
        cutter_z1,
        0.80,
        0.10,
        0.10,
    )


def build_aft_top_access(spec):
    deck_x0 = spec["x_front"] - 116.0
    deck_x1 = spec["x_front"] - 8.0
    deck_z0 = spec["terrace_floor_z"] + 2.0
    deck_z1 = deck_z0 + DECK_THICK

    build_box_object(
        "SM_AftTopAccess_Deck",
        deck_x0,
        deck_x1,
        -72.0,
        72.0,
        deck_z0,
        deck_z1,
        0.25,
        0.25,
        0.24,
    )

    build_ramp_x_object(
        "SM_AftTopAccess_Ramp",
        deck_x0 - 40.0,
        deck_x0,
        -66.0,
        66.0,
        deck_z0 - 14.0,
        deck_z0,
        4.0,
        0.24,
        0.24,
        0.23,
    )

    build_ramp_x_object(
        "SM_AftTopAccess_Link",
        deck_x1,
        spec["x_front"],
        -62.0,
        62.0,
        deck_z0,
        spec["terrace_floor_z"] + 2.0,
        4.0,
        0.24,
        0.24,
        0.23,
    )


def build_airlock_battants(spec):
    build_airlock_exit_cassette(spec)


def build_superstructure_armory_partition(upper_z):
    x_cm = UPPER_ARMORY_PARTITION_X_NORM * LENGTH
    return build_upper_bulkhead_clipped(
        "SM_BH_Upper_Armory",
        x_cm,
        upper_z,
        DOOR_W,
        DOOR_H,
        door_y=UPPER_ARMORY_DOOR_Y_OFFSET,
    )


def build_lower_technical_layout():
    z0 = LOWER_WALKWAY_Z
    catwalk_z0 = z0 + 3.0
    catwalk_z1 = catwalk_z0 + BALLAST_CATWALK_THICK

    # Lower Deck Main only in the central hub room between ballast compartments.
    build_deck("lower_main", DECK_LOWER_Z, LOWER_HUB_START, LOWER_HUB_END, 0.29, 0.28, 0.26, hw_fn=lower_deck_hw)

    for pair in BALLAST_PAIRS:
        half_len = pair["length_norm"] * LENGTH * 0.5
        bx0 = pair["center_norm"] * LENGTH - half_len
        bx1 = pair["center_norm"] * LENGTH + half_len
        fwd_bh_x = LOWER_BULKHEAD_SPECS[0]["x_norm"] * LENGTH
        aft_bh_x = LOWER_BULKHEAD_SPECS[1]["x_norm"] * LENGTH
        tech_x = LOWER_TECH_PARTITION_X * LENGTH
        if pair["name"] == "Fwd":
            room_x0 = max(0.07 * LENGTH, bx0)
            room_x1 = fwd_bh_x - BALLAST_COMPARTMENT_MARGIN
        else:
            room_x0 = aft_bh_x + BALLAST_COMPARTMENT_MARGIN
            room_x1 = tech_x - BALLAST_COMPARTMENT_MARGIN
        if pair["name"] == "Fwd":
            bx0 = room_x0 + 24.0
            bx1 = min(fwd_bh_x - BALLAST_COMPARTMENT_MARGIN, bx1)
        else:
            bx0 = max(room_x0 + 24.0, bx0)
            bx1 = min(tech_x - BALLAST_COMPARTMENT_MARGIN, bx1)
        if bx1 - bx0 < 80.0:
            continue
        for side, suffix in ((-1.0, "Port"), (1.0, "Stbd")):
            yc = side * pair["y_center"]
            hw = pair["half_width"]
            hh = pair["half_height"]

            build_internal_ballast_tank(
                f"SM_BallastTank_{pair['name']}_{suffix}",
                bx0,
                bx1,
                yc,
                pair["z_center"],
                hw,
                hh,
                0.12,
                0.16,
                0.14,
            )

            access_y0 = side * (LOWER_WALKWAY_HALF_WIDTH + 8.0)
            access_y1 = side * (LOWER_WALKWAY_HALF_WIDTH + 14.0)
            ay0, ay1 = sorted((access_y0, access_y1))
            build_box_object(
                f"SM_BallastAccess_{pair['name']}_{suffix}",
                room_x0 + BALLAST_CATWALK_END_MARGIN,
                room_x1 - BALLAST_CATWALK_END_MARGIN,
                ay0,
                ay1,
                catwalk_z0,
                catwalk_z1,
                0.24,
                0.25,
                0.24,
            )

        # Local narrow bridge between the two side catwalks inside this ballast room.
        mid_x = (bx0 + bx1) * 0.5
        build_box_object(
            f"SM_BallastPasserelle_{pair['name']}",
            mid_x - 24.0,
            mid_x + 24.0,
            -(LOWER_WALKWAY_HALF_WIDTH + 24.0),
            LOWER_WALKWAY_HALF_WIDTH + 24.0,
            catwalk_z0,
            catwalk_z1,
            0.24,
            0.24,
            0.23,
        )

        service_z = min(pair["z_center"] + BALLAST_SERVICE_Z_OFFSET, catwalk_z0 - 18.0)
        build_cylinder_x(
            f"SM_Ballast_Service_{pair['name']}",
            bx0 + 34.0,
            bx1 - 34.0,
            18.0,
            0.0,
            service_z,
            0.18,
            0.19,
            0.18,
            segments=16,
        )


def build_turret_hardpoints():
    for spec in TURRET_HARDPOINTS:
        nx = spec["x_norm"]
        x = nx * LENGTH
        radius = sample_curve(nx)
        if spec["mount"] == "top":
            z_min = radius - 4.0
            z_max = z_min + spec["height"]
        else:
            z_max = -radius + 4.0
            z_min = z_max - spec["height"]

        x0 = x - spec["length"] * 0.5
        x1 = x + spec["length"] * 0.5
        y0 = -spec["width"] * 0.5
        y1 = spec["width"] * 0.5

        build_box_object(
            f"SM_Hardpoint_{spec['name']}",
            x0,
            x1,
            y0,
            y1,
            z_min,
            z_max,
            0.26,
            0.28,
            0.26,
        )
        socket_h = 22.0
        socket_z0 = z_max if spec["mount"] == "top" else z_min - socket_h
        socket_z1 = socket_z0 + socket_h
        build_box_object(
            f"SM_TurretSocket_{spec['name']}",
            x - 8.0,
            x + 8.0,
            -18.0,
            18.0,
            socket_z0,
            socket_z1,
            0.30,
            0.31,
            0.30,
        )
        build_turret_module(
            spec["name"],
            x,
            spec["mount"],
            socket_z1 if spec["mount"] == "top" else socket_z0,
        )


def build_turret_module(name, x_center, mount, socket_plane_z):
    if name == "AftTop":
        return build_manual_turret_module(name, x_center, socket_plane_z)

    sign = 1.0 if mount == "top" else -1.0
    parts = []
    base_z0 = socket_plane_z if mount == "top" else socket_plane_z - 22.0
    base_z1 = base_z0 + 22.0
    parts.append(build_box_object(
        f"SM_TurretBase_{name}",
        x_center - 26.0,
        x_center + 26.0,
        -34.0,
        34.0,
        base_z0,
        base_z1,
        0.22,
        0.24,
        0.22,
    ))

    ring_z = base_z1 + sign * 6.0
    parts.append(build_cylinder_x(
        f"SM_TurretRing_{name}",
        x_center - 10.0,
        x_center + 10.0,
        26.0,
        0.0,
        ring_z,
        0.24,
        0.26,
        0.24,
        segments=18,
    ))

    body_z0 = ring_z + (4.0 if mount == "top" else -32.0)
    body_z1 = body_z0 + 32.0
    parts.append(build_box_object(
        f"SM_TurretBody_{name}",
        x_center - 34.0,
        x_center + 28.0,
        -28.0,
        28.0,
        min(body_z0, body_z1),
        max(body_z0, body_z1),
        0.20,
        0.21,
        0.20,
    ))

    yoke_z0 = (body_z1 - 8.0) if mount == "top" else (body_z0 + 8.0)
    yoke_z1 = yoke_z0 + (10.0 if mount == "top" else -10.0)
    parts.append(build_box_object(
        f"SM_TurretYoke_{name}",
        x_center + 10.0,
        x_center + 42.0,
        -34.0,
        34.0,
        min(yoke_z0, yoke_z1),
        max(yoke_z0, yoke_z1),
        0.21,
        0.22,
        0.21,
    ))

    barrel_z = body_z1 - 6.0 if mount == "top" else body_z0 + 6.0
    for index, barrel_y in enumerate((-12.0, 12.0)):
        parts.append(build_cylinder_x(
            f"SM_TurretBarrel_{name}_{index}",
            x_center + 34.0,
            x_center + 148.0,
            5.5,
            barrel_y,
            barrel_z,
            0.18,
            0.19,
            0.18,
            segments=12,
        ))

    return merge_mesh_objects(f"SM_Turret_{name}", parts)


def build_manual_turret_module(name, x_center, socket_plane_z):
    tub_z0 = socket_plane_z
    tub_z1 = tub_z0 + 24.0
    parts = []
    parts.append(build_box_object(
        f"SM_TurretBase_{name}",
        x_center - 28.0,
        x_center + 28.0,
        -36.0,
        36.0,
        tub_z0,
        tub_z1,
        0.22,
        0.24,
        0.22,
    ))
    parts.append(build_cylinder_x(
        f"SM_TurretTub_{name}",
        x_center - 16.0,
        x_center + 16.0,
        30.0,
        0.0,
        tub_z1 + 4.0,
        0.22,
        0.24,
        0.22,
        segments=18,
    ))
    parts.append(build_box_object(
        f"SM_TurretShield_{name}",
        x_center + 8.0,
        x_center + 22.0,
        -34.0,
        34.0,
        tub_z1 + 4.0,
        tub_z1 + 48.0,
        0.20,
        0.21,
        0.20,
    ))
    for index, barrel_y in enumerate((-11.0, 11.0)):
        parts.append(build_cylinder_x(
            f"SM_TurretBarrel_{name}_{index}",
            x_center + 18.0,
            x_center + 138.0,
            5.5,
            barrel_y,
            tub_z1 + 28.0,
            0.18,
            0.19,
            0.18,
            segments=12,
        ))
    for index, grip_y in enumerate((-18.0, 18.0)):
        parts.append(build_cylinder_x(
            f"SM_TurretGrip_{name}_{index}",
            x_center - 8.0,
            x_center + 12.0,
            2.0,
            grip_y,
            tub_z1 + 18.0,
            0.18,
            0.19,
            0.18,
            segments=10,
        ))

    return merge_mesh_objects(f"SM_Turret_{name}", parts)


def hull_surface_y(x_cm, z_cm, side, offset=0.0):
    hw = hull_half_width_at(x_cm, z_cm)
    if hw <= 1.0:
        return side * offset
    return side * (hw + offset)


def build_side_hull_panel(name, x_center, z_center, side, length, height, depth, color):
    y_surface = hull_surface_y(x_center, z_center, side, 0.0)
    x0 = x_center - length * 0.5
    x1 = x_center + length * 0.5
    z0 = z_center - height * 0.5
    z1 = z_center + height * 0.5
    if side > 0.0:
        y0 = y_surface - 2.0
        y1 = y_surface + depth
    else:
        y0 = y_surface - depth
        y1 = y_surface + 2.0
    build_box_object(name, x0, x1, y0, y1, z0, z1, *color)


def build_mooring_lug(name, x_center, side, z_center):
    y_surface = hull_surface_y(x_center, z_center, side, 6.0)
    side_sign = 1.0 if side > 0.0 else -1.0
    base_y0 = y_surface - side_sign * 12.0
    base_y1 = y_surface + side_sign * 12.0
    by0, by1 = sorted((base_y0, base_y1))
    build_box_object(
        f"{name}_Base",
        x_center - 10.0,
        x_center + 10.0,
        by0,
        by1,
        z_center - 9.0,
        z_center + 9.0,
        0.24,
        0.25,
        0.24,
    )
    build_box_object(
        f"{name}_Top",
        x_center - 12.0,
        x_center + 12.0,
        by0,
        by1,
        z_center + 8.0,
        z_center + 14.0,
        0.24,
        0.25,
        0.24,
    )


def cut_ballast_flood_ports(hull_obj):
    for pair in BALLAST_PAIRS:
        half_len = pair["length_norm"] * LENGTH * 0.5
        x_mid = pair["center_norm"] * LENGTH
        x_positions = (
            x_mid - half_len * 0.28,
            x_mid + half_len * 0.28,
        )
        z_center = pair["z_center"] + pair["half_height"] * 0.18
        for side in (-1.0, 1.0):
            for index, x_center in enumerate(x_positions):
                y_surface = hull_surface_y(x_center, z_center, side, 0.0)
                if side > 0.0:
                    y0 = y_surface - 2.0
                    y1 = y_surface + 28.0
                else:
                    y0 = y_surface - 28.0
                    y1 = y_surface + 2.0
                cut_box_opening(
                    hull_obj,
                    f"SM_FloodPort_{pair['name']}_{'Stbd' if side > 0.0 else 'Port'}_{index}",
                    x_center - 24.0,
                    x_center + 24.0,
                    y0,
                    y1,
                    z_center - 8.0,
                    z_center + 8.0,
                )


def build_turret_mount_fairings():
    for spec in TURRET_HARDPOINTS:
        nx = spec["x_norm"]
        x = nx * LENGTH
        radius = sample_curve(nx)
        fairing_len = spec["length"] + 22.0
        fairing_width = spec["width"] + 26.0
        fairing_height = 8.0
        if spec["mount"] == "top":
            z0 = radius - 8.0
            z1 = z0 + fairing_height
        else:
            z1 = -radius + 6.0
            z0 = z1 - fairing_height
        build_box_object(
            f"SM_HardpointFairing_{spec['name']}",
            x - fairing_len * 0.5,
            x + fairing_len * 0.5,
            -fairing_width * 0.5,
            fairing_width * 0.5,
            z0,
            z1,
            0.22,
            0.23,
            0.22,
        )


def build_hull_gameplay_detail_kit(hull_obj, airlock_spec):
    cut_ballast_flood_ports(hull_obj)
    build_turret_mount_fairings()

    build_mooring_lug("SM_MooringLug_Bow_Port", 0.072 * LENGTH, -1.0, 64.0)
    build_mooring_lug("SM_MooringLug_Bow_Stbd", 0.072 * LENGTH, 1.0, 64.0)
    build_mooring_lug("SM_MooringLug_Aft_Port", 0.874 * LENGTH, -1.0, 36.0)
    build_mooring_lug("SM_MooringLug_Aft_Stbd", 0.874 * LENGTH, 1.0, 36.0)


def build_interior_props(upper_z):
    # SM_HelmConsole and SM_HelmDisplay removed on 2026-04-16. They were blocky
    # placeholders that clashed with the lowpoly/voxel art direction. A proper
    # voxel helm kit can be authored in Blender directly, or added back here
    # later as a dedicated builder.
    ceiling_z = deck_bottom_z(upper_z) - 6.0

    def build_turret_scope_station(name, x_center, y_center):
        eye_z = DECK_MAIN_Z + 175.0
        scope_top = ceiling_z + 4.0
        scope_static_bottom = eye_z + 28.0
        static_parts = []
        rotating_parts = []

        static_parts.append(
            build_cylinder_z(
                f"SM_TurretStation_{name}_Drop",
                scope_static_bottom,
                scope_top,
                8.0,
                x_center,
                y_center,
                0.21,
                0.22,
                0.21,
                segments=18,
            )
        )
        static_parts.append(
            build_cylinder_z(
                f"SM_TurretStation_{name}_CeilingCollar",
                scope_top - 18.0,
                scope_top + 2.0,
                15.0,
                x_center,
                y_center,
                0.23,
                0.24,
                0.23,
                segments=20,
            )
        )

        rotating_parts.append(
            build_cylinder_z(
                f"SM_TurretStation_{name}_AxisHub",
                eye_z - 26.0,
                eye_z + 18.0,
                7.0,
                x_center,
                y_center,
                0.20,
                0.21,
                0.20,
                segments=16,
            )
        )
        rotating_parts.append(
            build_box_object(
                f"SM_TurretStation_{name}_Head",
                x_center - 16.0,
                x_center + 16.0,
                y_center - 12.0,
                y_center + 12.0,
                eye_z - 16.0,
                eye_z + 10.0,
                0.19,
                0.20,
                0.19,
            )
        )

        for suffix, side in (("Port", -1.0), ("Stbd", 1.0)):
            handle_y = y_center + side * 22.0
            rotating_parts.append(
                build_cylinder_x(
                    f"SM_TurretStation_{name}_HandleBar_{suffix}",
                    x_center - 14.0,
                    x_center + 14.0,
                    2.0,
                    handle_y,
                    eye_z - 4.0,
                    0.18,
                    0.19,
                    0.18,
                    segments=10,
                )
            )
            rotating_parts.append(
                build_cylinder_z(
                    f"SM_TurretStation_{name}_HandlePostFwd_{suffix}",
                    eye_z - 18.0,
                    eye_z + 8.0,
                    1.7,
                    x_center + 9.0,
                    handle_y,
                    0.18,
                    0.19,
                    0.18,
                    segments=10,
                )
            )
            rotating_parts.append(
                build_cylinder_z(
                    f"SM_TurretStation_{name}_HandlePostAft_{suffix}",
                    eye_z - 18.0,
                    eye_z + 8.0,
                    1.7,
                    x_center - 9.0,
                    handle_y,
                    0.18,
                    0.19,
                    0.18,
                    segments=10,
                )
            )

        merge_mesh_objects(f"SM_TurretStation_{name}_Static", static_parts)
        merge_mesh_objects(f"SM_TurretStation_{name}_Rotator", rotating_parts)

    build_turret_scope_station("Fwd", 0.506 * LENGTH, 0.0)
    build_turret_scope_station("Aft", 0.586 * LENGTH, 0.0)

    for suffix, side in (("Port", -1.0), ("Stbd", 1.0)):
        y0 = side * 188.0
        y1 = side * 122.0
        by0, by1 = sorted((y0, y1))
        build_box_object(
            f"SM_CrewLocker_{suffix}",
            0.126 * LENGTH,
            0.184 * LENGTH,
            by0,
            by1,
            DECK_MAIN_Z + 2.0,
            DECK_MAIN_Z + 170.0,
            0.23,
            0.24,
            0.23,
        )

    # Crew quarter: two single-mesh utility props (replace bunks/tables/chairs).
    def build_crew_quarter_prop(name, x0, x1, y0, y1):
        verts = []
        faces = []
        # Base chest
        append_box(verts, faces, x0, x1, y0, y1, DECK_MAIN_Z + 10.0, DECK_MAIN_Z + 60.0)
        # Top locker
        append_box(
            verts,
            faces,
            x0 + 6.0,
            x1 - 6.0,
            y0 + 4.0,
            y1 - 4.0,
            DECK_MAIN_Z + 60.0,
            DECK_MAIN_Z + 146.0,
        )
        # Front service shelf
        append_box(
            verts,
            faces,
            x0 - 12.0,
            x0 + 6.0,
            y0 + 10.0,
            y1 - 10.0,
            DECK_MAIN_Z + 78.0,
            DECK_MAIN_Z + 90.0,
        )
        return make(name, verts, faces, 0.22, 0.23, 0.22, smooth=False)

    build_crew_quarter_prop(
        "SM_CrewProp_Module_Port",
        0.108 * LENGTH,
        0.176 * LENGTH,
        -184.0,
        -118.0,
    )
    build_crew_quarter_prop(
        "SM_CrewProp_Module_Stbd",
        0.108 * LENGTH,
        0.176 * LENGTH,
        118.0,
        184.0,
    )

    # Upper deck storage crates/lockers.
    upper_storage_z0 = upper_z + 2.0
    for side_name, side in (("Port", -1.0), ("Stbd", 1.0)):
        for idx, xn in enumerate((0.246, 0.284, 0.330, 0.376)):
            x0 = xn * LENGTH
            x1 = x0 + 28.0
            y_outer = side * 164.0
            y_inner = side * 118.0
            y0, y1 = sorted((y_outer, y_inner))
            height = 68.0 + idx * 14.0
            build_box_object(
                f"SM_UpperStorageCrate_{side_name}_{idx}",
                x0,
                x1,
                y0,
                y1,
                upper_storage_z0,
                upper_storage_z0 + height,
                0.22,
                0.23,
                0.22,
            )
        locker_x0 = 0.430 * LENGTH
        locker_x1 = 0.462 * LENGTH
        ly_outer = side * 170.0
        ly_inner = side * 120.0
        ly0, ly1 = sorted((ly_outer, ly_inner))
        build_box_object(
            f"SM_UpperStorageLocker_{side_name}",
            locker_x0,
            locker_x1,
            ly0,
            ly1,
            upper_storage_z0,
            upper_storage_z0 + 154.0,
            0.21,
            0.22,
            0.21,
        )

    pipe_parts = []
    pipe_x0 = 0.214 * LENGTH
    pipe_x1 = 0.690 * LENGTH
    pipe_z = deck_bottom_z(upper_z) - 26.0
    for suffix, side in (("Port", -1.0), ("Stbd", 1.0)):
        py = side * 138.0
        pipe_parts.append(
            build_cylinder_x(
                f"SM_Pipe_Main_{suffix}",
                pipe_x0,
                pipe_x1,
                6.0,
                py,
                pipe_z,
                0.18,
                0.19,
                0.18,
                segments=16,
            )
        )
        for idx, px in enumerate((0.336 * LENGTH, 0.500 * LENGTH, 0.620 * LENGTH)):
            pipe_parts.append(
                build_cylinder_z(
                    f"SM_PipeDrop_{suffix}_{idx}",
                    DECK_MAIN_Z + 40.0,
                    pipe_z - 4.0,
                    3.8,
                    px,
                    py,
                    0.17,
                    0.18,
                    0.17,
                    segments=12,
                )
            )
            pipe_parts.append(
                build_box_object(
                    f"SM_PipeClamp_{suffix}_{idx}",
                    px - 3.8,
                    px + 3.8,
                    py - 8.0,
                    py + 8.0,
                    pipe_z - 6.0,
                    pipe_z + 6.0,
                    0.20,
                    0.21,
                    0.20,
                )
            )
    merge_mesh_objects("SM_Pipe_CeilingMain", pipe_parts)
    build_box_object(
        "SM_EngineControlCabinet",
        0.800 * LENGTH,
        0.840 * LENGTH,
        112.0,
        166.0,
        DECK_MAIN_Z + 2.0,
        DECK_MAIN_Z + 118.0,
        0.22,
        0.23,
        0.22,
    )


def build_bulkhead_doors(upper_z):
    for spec in MAIN_BULKHEAD_SPECS:
        build_standard_pressure_door(
            name=f"SM_Door_Main_{spec['name']}",
            x_center=spec["x_norm"] * LENGTH,
            sill_z=DECK_MAIN_Z,
            make_fn=make,
            append_box_fn=append_box,
            width=spec["door_w"],
            height=spec["door_h"],
            frame_margin=DOOR_FRAME_MARGIN,
            frame_depth=DOOR_FRAME_DEPTH,
            leaf_depth=DOOR_LEAF_DEPTH,
            threshold=DOOR_THRESHOLD,
            rough_margin_x=DOOR_ROUGH_MARGIN_X,
            rough_margin_z=DOOR_ROUGH_MARGIN_Z,
            leaf_overlap=DOOR_LEAF_OVERLAP,
            leaf_inset=2.0,
            color=(0.30, 0.31, 0.30),
        )

    for spec in LOWER_BULKHEAD_SPECS:
        build_standard_pressure_door(
            name=f"SM_Door_Lower_{spec['name']}",
            x_center=spec["x_norm"] * LENGTH,
            sill_z=DECK_LOWER_Z,
            make_fn=make,
            append_box_fn=append_box,
            width=spec["door_w"],
            height=spec["door_h"],
            frame_margin=DOOR_FRAME_MARGIN,
            frame_depth=DOOR_FRAME_DEPTH,
            leaf_depth=DOOR_LEAF_DEPTH,
            threshold=DOOR_THRESHOLD,
            rough_margin_x=DOOR_ROUGH_MARGIN_X,
            rough_margin_z=DOOR_ROUGH_MARGIN_Z,
            leaf_overlap=DOOR_LEAF_OVERLAP,
            leaf_inset=2.0,
            color=(0.30, 0.31, 0.30),
        )

    build_standard_pressure_door(
        name="SM_Door_Upper_Airlock_Inner",
        x_center=UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH,
        sill_z=upper_z,
        make_fn=make,
        append_box_fn=append_box,
        width=UPPER_AIRLOCK_INNER_BULKHEAD["door_w"],
        height=UPPER_AIRLOCK_INNER_BULKHEAD["door_h"],
        frame_margin=DOOR_FRAME_MARGIN,
        frame_depth=DOOR_FRAME_DEPTH,
        leaf_depth=DOOR_LEAF_DEPTH,
        threshold=DOOR_THRESHOLD,
        rough_margin_x=DOOR_ROUGH_MARGIN_X,
        rough_margin_z=DOOR_ROUGH_MARGIN_Z,
        leaf_overlap=DOOR_LEAF_OVERLAP,
        leaf_inset=2.0,
        color=(0.30, 0.31, 0.30),
    )

    build_standard_pressure_door(
        name="SM_Door_Upper_Armory",
        x_center=UPPER_ARMORY_PARTITION_X_NORM * LENGTH,
        sill_z=upper_z,
        make_fn=make,
        append_box_fn=append_box,
        width=DOOR_W,
        height=DOOR_H,
        frame_margin=DOOR_FRAME_MARGIN,
        frame_depth=DOOR_FRAME_DEPTH,
        leaf_depth=DOOR_LEAF_DEPTH,
        threshold=DOOR_THRESHOLD,
        rough_margin_x=DOOR_ROUGH_MARGIN_X,
        rough_margin_z=DOOR_ROUGH_MARGIN_Z,
        leaf_overlap=DOOR_LEAF_OVERLAP,
        leaf_inset=2.0,
        y_offset=UPPER_ARMORY_DOOR_Y_OFFSET,
        color=(0.30, 0.31, 0.30),
    )


# ------------------------------------------------------------------
# SF detail kit
# ------------------------------------------------------------------
#
# Interior: three tri-color pipe runs (water / hydraulic / reactor) along the
# starboard side of the main corridor, with valve wheels and junction boxes
# at regular intervals. Exterior superstructure: one periscope housing and
# three retractable antenna masts. Hull exterior: a few circumferential weld
# seams at frame positions. All deco is static — no pivot logic required
# beyond the antenna mast bases handled by core.pivots.

SF_PIPE_Y = 92.0
SF_PIPE_RADIUS = 5.5
SF_PIPE_VERT_STEP = 14.0
SF_VALVE_COUNT_PER_PIPE = 5
SF_JUNCTION_BH_EXTRA_X = 22.0
SF_JUNCTION_BH_EXTRA_Y = 22.0
SF_PIPE_DROP_RADIUS = 4.5   # vertical drop segments (slightly thinner than main run)

SF_PERISCOPE_X_NORM = 0.38
SF_PERISCOPE_RISE = 220.0
SF_PERISCOPE_RADIUS = 12.0

SF_ANTENNA_SPECS = (
    {"name": "ComsA", "x_norm": 0.58, "y": 40.0,  "rise": 180.0, "radius": 4.0, "tip_radius": 1.6, "flag": True},
    {"name": "ComsB", "x_norm": 0.62, "y": -40.0, "rise": 150.0, "radius": 4.0, "tip_radius": 1.6, "flag": False},
    {"name": "Sonar", "x_norm": 0.66, "y":  0.0,  "rise": 100.0, "radius": 6.0, "tip_radius": 3.0, "flag": False},
)

# Longitudinal welds: 4 beads running ALONG the hull at angles that stay
# clearly on the hull exterior, well outside the conning-tower silhouette.
# Upper pair (30, 150) sits on the high shoulders just below the super's
# side walls; lower pair (210, 330) mirrors on the bilge so the four welds
# read as a symmetric plate-seam grid. Scales with sample_curve at each X.
SF_LONG_WELD_ANGLES_DEG = (30, 150, 210, 330)
SF_LONG_WELD_X_START_NORM = 0.05
SF_LONG_WELD_X_END_NORM = 0.96
SF_LONG_WELD_SAMPLES = 48
SF_LONG_WELD_OUTWARD_OFFSET = 3.5   # crest height above hull skin (cm) — more voyant
SF_LONG_WELD_BASE_HALF_WIDTH = 3.2  # base half-width along tangent (cm)
SF_LONG_WELD_CREST_HALF_WIDTH = 1.6 # crest half-width, smaller => pointier bead


def _build_sf_pipe_runs(upper_z):
    """Three tri-color pipe runs under the upper-deck underside, each with a
    slight Y/Z offset (chaos), a pair of junction boxes at the Fwd and Control
    bulkheads that anchor the pipe endpoints, plus a floor-diving L-bend in
    the middle of the run so the pipes don't visually float in mid-air.

    Per 2026-04-16 spec: pipes should "s'enfoncer dans le sol ou dans les
    murs" at their ends, and show some chaos in their routing.
    """
    fwd_bh = next(spec for spec in MAIN_BULKHEAD_SPECS if spec["name"] == "Fwd")
    ctrl_bh = next(spec for spec in MAIN_BULKHEAD_SPECS if spec["name"] == "Control")
    pipe_x0 = fwd_bh["x_norm"] * LENGTH
    pipe_x1 = ctrl_bh["x_norm"] * LENGTH

    # Main horizontal level: ~12 cm below the upper-deck underside.
    top_z = upper_z - DECK_THICK * 0.5 - 12.0
    floor_z = DECK_MAIN_Z + DECK_THICK * 0.5 - 4.0   # L-bend target: just below main deck floor

    # Per-run Y/Z jitter + distinct L-bend position (staggered across the length).
    runs = (
        {"name": "Water",   "y_jitter":  0.0,  "z_offset":  0.0,                     "drop_t": 0.38, "color_rgb": (0.12, 0.26, 0.46)},
        {"name": "Hyd",     "y_jitter": -5.0,  "z_offset": -SF_PIPE_VERT_STEP,       "drop_t": 0.62, "color_rgb": (0.58, 0.44, 0.10)},
        {"name": "Reactor", "y_jitter":  5.0,  "z_offset": -SF_PIPE_VERT_STEP * 2.0, "drop_t": 0.52, "color_rgb": (0.46, 0.15, 0.12)},
    )

    for run in runs:
        pipe_y = SF_PIPE_Y + run["y_jitter"]
        pipe_z = top_z + run["z_offset"]
        cr, cg, cb = run["color_rgb"]

        # Main horizontal run along X.
        build_cylinder_x(
            f"SM_Pipe_{run['name']}_Main",
            pipe_x0,
            pipe_x1,
            SF_PIPE_RADIUS,
            pipe_y,
            pipe_z,
            cr, cg, cb,
            segments=12,
        )

        # Drop bend: vertical Z cylinder from pipe_z down into the main deck
        # floor, plus a short horizontal elbow cylinder linking it to the main
        # run. The drop ends slightly BELOW the deck surface so it reads as
        # "plunging into the floor".
        drop_x = pipe_x0 + (pipe_x1 - pipe_x0) * run["drop_t"]
        build_cylinder_z(
            f"SM_Pipe_{run['name']}_Drop",
            floor_z,
            pipe_z + 2.0,
            SF_PIPE_DROP_RADIUS,
            drop_x,
            pipe_y,
            cr, cg, cb,
            segments=10,
        )
        # Small elbow fitting at the top of the drop (reads as a real coupling).
        build_cylinder_z(
            f"SM_Pipe_{run['name']}_DropElbow",
            pipe_z - SF_PIPE_RADIUS - 2.0,
            pipe_z + SF_PIPE_RADIUS + 2.0,
            SF_PIPE_RADIUS + 1.8,
            drop_x,
            pipe_y,
            0.14, 0.14, 0.14,
            segments=10,
        )

        # Valve wheels spaced along the main horizontal run, avoiding the drop.
        for i in range(SF_VALVE_COUNT_PER_PIPE):
            t = (i + 1) / (SF_VALVE_COUNT_PER_PIPE + 1)
            if abs(t - run["drop_t"]) < 0.06:
                continue
            vx = pipe_x0 + (pipe_x1 - pipe_x0) * t
            build_cylinder_z(
                f"SM_Valve_{run['name']}_{i}",
                pipe_z - 10.0,
                pipe_z + 10.0,
                7.5,
                vx,
                pipe_y,
                0.40, 0.34, 0.12,
                segments=12,
            )

    # Junction boxes anchoring the pipe endpoints at both main bulkheads.
    # They extend in Y and Z enough to swallow all three pipes and their
    # per-run jitter, so the pipes appear to enter and exit real enclosures.
    box_z_top = top_z + 12.0
    box_z_bot = top_z - SF_PIPE_VERT_STEP * 2.0 - 14.0
    for spec in (fwd_bh, ctrl_bh):
        jx = spec["x_norm"] * LENGTH
        build_box_object(
            f"SM_Junction_{spec['name']}",
            jx - SF_JUNCTION_BH_EXTRA_X,
            jx + SF_JUNCTION_BH_EXTRA_X,
            SF_PIPE_Y - SF_JUNCTION_BH_EXTRA_Y,
            SF_PIPE_Y + SF_JUNCTION_BH_EXTRA_Y,
            box_z_bot,
            box_z_top,
            0.18,
            0.18,
            0.18,
        )


def _build_sf_periscope(upper_z):
    """Periscope housing: a tall cylinder rising from the upper-deck roof,
    with a short horizontal eye-piece at the top (gamified silhouette)."""
    x_cm = SF_PERISCOPE_X_NORM * LENGTH
    main_r = sample_curve(SF_PERISCOPE_X_NORM)
    _, _, super_h_local, _, _ = superstructure_state(SF_PERISCOPE_X_NORM)
    roof_z = main_r + max(0.0, super_h_local * 0.9)
    base_z = upper_z
    top_z = roof_z + SF_PERISCOPE_RISE

    # Mast
    build_cylinder_z(
        "SM_Periscope_Mast",
        base_z,
        top_z,
        SF_PERISCOPE_RADIUS,
        x_cm,
        0.0,
        0.14,
        0.15,
        0.16,
        segments=14,
    )
    # Housing at the base (thicker cylinder) to read as an optics turret.
    build_cylinder_z(
        "SM_Periscope_Housing",
        base_z + 6.0,
        base_z + 40.0,
        SF_PERISCOPE_RADIUS * 1.8,
        x_cm,
        0.0,
        0.14,
        0.15,
        0.16,
        segments=14,
    )
    # Eye-piece: small horizontal box pointing forward (-X).
    build_box_object(
        "SM_Periscope_Eye",
        x_cm - 20.0,
        x_cm + 4.0,
        -4.0,
        4.0,
        top_z - 12.0,
        top_z + 8.0,
        0.12,
        0.13,
        0.14,
    )


def _build_sf_antennas():
    """Retractable-style antenna masts: thin telescoping cylinders with a flag
    at the tip for the com antennas (stylized/gamified)."""
    for spec in SF_ANTENNA_SPECS:
        x_cm = spec["x_norm"] * LENGTH
        main_r = sample_curve(spec["x_norm"])
        _, _, super_h_local, _, _ = superstructure_state(spec["x_norm"])
        base_z = main_r + max(0.0, super_h_local * 0.9)
        mid_z = base_z + spec["rise"] * 0.55
        top_z = base_z + spec["rise"]

        # Lower telescoping segment.
        build_cylinder_z(
            f"SM_Antenna_{spec['name']}_Lower",
            base_z,
            mid_z,
            spec["radius"],
            x_cm,
            spec["y"],
            0.22,
            0.22,
            0.24,
            segments=12,
        )
        # Upper telescoping segment (thinner).
        build_cylinder_z(
            f"SM_Antenna_{spec['name']}_Upper",
            mid_z,
            top_z,
            spec["tip_radius"],
            x_cm,
            spec["y"],
            0.22,
            0.22,
            0.24,
            segments=10,
        )

        if spec["flag"]:
            # Gamified pennant at the tip. Renamed SM_Flag_* so the materials
            # classifier picks the red "flag" key instead of the antenna metal.
            build_box_object(
                f"SM_Flag_Antenna_{spec['name']}",
                x_cm + spec["tip_radius"] + 0.5,
                x_cm + spec["tip_radius"] + 0.5 + 18.0,
                spec["y"] - 0.8,
                spec["y"] + 0.8,
                top_z - 12.0,
                top_z - 2.0,
                0.72,
                0.18,
                0.12,
            )


def _build_sf_hull_welds():
    """Four LONGITUDINAL weld beads running along the hull at 45/135/225/315
    degrees (symmetric on the four quarter angles). Each bead has a proper
    trapezoidal 3D cross-section (base_left, crest_left, crest_right,
    base_right) and follows the hull curvature parametrically via
    sample_curve. Reads as real plate seams under flat shading, and avoids
    both the conning-tower zone (theta~90 deg) and the keel (theta~270 deg).
    """
    for angle_deg in SF_LONG_WELD_ANGLES_DEG:
        theta = math.radians(angle_deg)
        ct = math.cos(theta)
        st = math.sin(theta)
        # Tangent direction in the YZ plane, perpendicular to the radial normal.
        # Used to give the bead its width along the hull surface.
        tang_y = -st
        tang_z = ct

        verts = []
        faces = []

        n = SF_LONG_WELD_SAMPLES
        for i in range(n + 1):
            t = i / n
            xn = SF_LONG_WELD_X_START_NORM + (SF_LONG_WELD_X_END_NORM - SF_LONG_WELD_X_START_NORM) * t
            x_cm = xn * LENGTH
            r_hull = sample_curve(xn)
            r_crest = r_hull + SF_LONG_WELD_OUTWARD_OFFSET
            hw_base = SF_LONG_WELD_BASE_HALF_WIDTH
            hw_crest = SF_LONG_WELD_CREST_HALF_WIDTH

            # Cross-section: 4 verts in the YZ plane, all at the same X.
            y_bl = r_hull * ct - hw_base * tang_y
            z_bl = r_hull * st - hw_base * tang_z
            y_cl = r_crest * ct - hw_crest * tang_y
            z_cl = r_crest * st - hw_crest * tang_z
            y_cr = r_crest * ct + hw_crest * tang_y
            z_cr = r_crest * st + hw_crest * tang_z
            y_br = r_hull * ct + hw_base * tang_y
            z_br = r_hull * st + hw_base * tang_z

            verts.extend([
                (x_cm, y_bl, z_bl),
                (x_cm, y_cl, z_cl),
                (x_cm, y_cr, z_cr),
                (x_cm, y_br, z_br),
            ])

        # Bridge consecutive cross-sections with three quads each: left flank,
        # crest, right flank. Three strips give the bead its trapezoidal body.
        section_count = (n + 1)
        for i in range(section_count - 1):
            b0 = i * 4
            b1 = (i + 1) * 4
            faces.append((b0 + 0, b0 + 1, b1 + 1, b1 + 0))  # left flank
            faces.append((b0 + 1, b0 + 2, b1 + 2, b1 + 1))  # crest top
            faces.append((b0 + 2, b0 + 3, b1 + 3, b1 + 2))  # right flank

        # Cap the two ends so the bead is a closed solid.
        faces.append((0, 3, 2, 1))
        end_base = (section_count - 1) * 4
        faces.append((end_base + 0, end_base + 1, end_base + 2, end_base + 3))

        make(f"SM_Hull_Weld_Long_{angle_deg}", verts, faces, 0.14, 0.18, 0.16, smooth=False)


def build_sf_detail_kit(upper_z):
    _build_sf_pipe_runs(upper_z)
    _build_sf_periscope(upper_z)
    _build_sf_antennas()
    _build_sf_hull_welds()


def main():
    print("\n" + "=" * 60)
    print(f"Sub3D gameplay-first hull blockout - {SUB_NAME}")
    print("=" * 60)

    clear()

    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = "CENTIMETERS"

    for area in bpy.context.screen.areas:
        if area.type == "VIEW_3D":
            for space in area.spaces:
                if space.type == "VIEW_3D":
                    space.clip_start = 1
                    space.clip_end = 500000

    print("--- Hull ---")
    hull_obj = build_hull()

    print("--- Decks ---")
    main_fwd_norm = bulkhead_x_norm(BH_MAIN_FWD_NAME)
    build_deck("main", DECK_MAIN_Z, 0.002, 0.775, 0.36, 0.34, 0.30, hw_fn=main_deck_hw)
    upper_z = sample_curve(0.40) - 150
    build_upper_deck_clipped(upper_z, main_fwd_norm, UPPER_DECK_AFT_NORM, 0.40, 0.38, 0.34)

    print("--- Lower technical layout (hub lower-main + ballast compartments) ---")
    build_lower_technical_layout()

    print("--- Propulsion room layout (reactor / engine / shaft) ---")
    build_propulsion_room_layout()

    print("--- Turret hardpoints (3) ---")
    build_turret_hardpoints()

    print("--- Hatch openings + ladders ---")
    build_round_hatches_and_ladders(upper_z)
    build_superstructure_armory_partition(upper_z)

    print("--- Bulkheads main ---")
    upper_deck_underside_z = deck_bottom_z(upper_z)
    for spec in MAIN_BULKHEAD_SPECS:
        x_cm = spec["x_norm"] * LENGTH
        z_max = (
            min(
                compute_bulkhead_top_z(
                    spec["x_norm"],
                    upper_z,
                    DECK_MAIN_Z,
                    sample_curve,
                    superstructure_state,
                ),
                upper_room_apex_z(x_cm, BULKHEAD_CONTACT_OVERLAP) if spec["name"] == "Control" else upper_ceiling_z(x_cm, margin=-BULKHEAD_CONTACT_OVERLAP),
            )
            if spec.get("full_height")
            else min(upper_deck_underside_z, upper_room_apex_z(x_cm, BULKHEAD_CONTACT_OVERLAP) if spec["name"] == "Control" else upper_ceiling_z(x_cm, margin=-BULKHEAD_CONTACT_OVERLAP))
        )
        build_bulkhead_clipped(
            name=f"SM_BH_Main_{spec['name']}",
            x_cm=x_cm,
            z_min=DECK_MAIN_Z,
            z_max=z_max,
            door_sill_z=DECK_MAIN_Z,
            door_w=spec["door_w"],
            door_h=spec["door_h"],
            overlap_cm=BULKHEAD_CONTACT_OVERLAP,
            color=(0.44, 0.42, 0.38),
        )

    print("--- Bulkheads lower ---")
    for spec in LOWER_BULKHEAD_SPECS:
        lower_obj = build_bulkhead_clipped(
            name=f"SM_BH_Lower_{spec['name']}",
            x_cm=spec["x_norm"] * LENGTH,
            z_min=LOWER_BH_Z_MIN,
            z_max=deck_bottom_z(DECK_MAIN_Z),
            door_sill_z=DECK_LOWER_Z,
            door_w=spec["door_w"],
            door_h=spec["door_h"],
            overlap_cm=BULKHEAD_CONTACT_OVERLAP,
            color=(0.44, 0.42, 0.38),
        )
        if lower_obj:
            cut_box_opening(
                lower_obj,
                f"SM_BH_Lower_{spec['name']}_BilgeOpening",
                spec["x_norm"] * LENGTH - (BH_THICK * 1.5),
                spec["x_norm"] * LENGTH + (BH_THICK * 1.5),
                -78.0,
                78.0,
                LOWER_BH_Z_MIN - 2.0,
                DECK_LOWER_Z - 18.0,
            )
    build_bulkhead_clipped(
        name="SM_BH_Lower_TechPartition",
        x_cm=LOWER_TECH_PARTITION_X * LENGTH,
        z_min=LOWER_BH_Z_MIN,
        z_max=deck_bottom_z(DECK_MAIN_Z),
        door_sill_z=None,
        door_w=0.0,
        door_h=0.0,
        overlap_cm=BULKHEAD_CONTACT_OVERLAP,
        color=(0.44, 0.42, 0.38),
    )

    print("--- Bulkheads upper airlock (inner only) ---")
    build_upper_bulkhead_clipped(
        f"SM_BH_{UPPER_AIRLOCK_INNER_BULKHEAD['name']}",
        UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH,
        upper_z,
        UPPER_AIRLOCK_INNER_BULKHEAD["door_w"],
        UPPER_AIRLOCK_INNER_BULKHEAD["door_h"],
    )

    print("--- Watertight doors ---")
    build_bulkhead_doors(upper_z)
    print("--- Interior props / stations ---")
    build_interior_props(upper_z)

    print("--- Rear airlock pocket + cassette ---")
    spec = airlock_spec(upper_z)
    cut_airlock_exit_pocket(hull_obj, spec)
    hull_solidify = add_solidify(hull_obj, HULL_THICK, -1)
    apply_object_modifier(hull_obj, hull_solidify.name)
    build_airlock_exit_cassette(spec)
    build_hull_gameplay_detail_kit(hull_obj, spec)

    print("--- Hydroplanes ---")
    build_hydroplanes()

    print("--- Tail fins ---")
    build_fins()

    print("--- Rudder / skeg ---")
    build_rudder()

    print("--- Ducted propulsor ---")
    build_propulsor()

    print("--- SF detail kit (pipes / valves / junctions / periscope / antennas / hull welds) ---")
    build_sf_detail_kit(upper_z)

    print("--- Pivots (doors, rudder, hydroplanes, turrets, antennas, periscope) ---")
    fix_all_pivots()

    print("--- Solid-PBR material assignment (from core.materials) ---")
    assign_materials()

    screen = getattr(bpy.context, "screen", None)
    if screen:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                try:
                    with bpy.context.temp_override(area=area, region=area.regions[-1]):
                        bpy.ops.view3d.view_selected()
                except RuntimeError:
                    pass
                break

    print("\n" + "=" * 60)
    for obj in sorted(bpy.data.objects, key=lambda item: item.name):
        if obj.type == "MESH":
            print(f"  {obj.name}: {len(obj.data.vertices)}V")
    print("=" * 60)
    print("\nBlockout intent:")
    print("  - Hull includes smoother ballast-zone blending and wrapped lower hull relief.")
    print("  - Main deck reaches bow; lower-main deck stays only in lower hub.")
    print("  - Upper deck starts at BH_Main_Fwd and closes upper ceiling with aligned hatch axis.")
    print("  - Bulkheads/doors use one watertight standard (W100/H200) with 2m clearance logic.")
    print("  - Engine room keeps 2 deck levels + 1 main ramp, adds mezzanine and lower maintenance access.")
    print("  - Rear SAS uses a hull cutout + cassette frame (no Back wall) with 96x192 cm battants.")
    print("  - Upper deck stair hole resized and centered on stair arrival (x_norm=0.305, 160x88 cm).")
    print("  - SF detail kit: tri-color pipes, valve wheels, junction boxes, periscope, antennas.")
    print("  - Materials / pivots / stubs now live under core/ and are invoked at the end of main.")


main()
