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

    # Stable project fallback for this repository.
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


def _load_bulkheads_module():
    _install_import_paths()

    for base in _resolve_script_dirs():
        candidate = os.path.join(base, "core", "bulkheads_doors.py")
        if not os.path.isfile(candidate):
            continue
        module_name = "hull_blockout_gpt_bulkheads_doors"
        sys.modules.pop(module_name, None)
        spec = importlib.util.spec_from_file_location(module_name, candidate)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module

    importlib.invalidate_caches()
    sys.modules.pop("bulkheads_doors", None)
    try:
        import bulkheads_doors as module
        return module
    except ModuleNotFoundError:
        pass

    raise ModuleNotFoundError("Unable to load bulkheads_doors.py from hull_blockout_gpt/core")


_bulkheads_doors = _load_bulkheads_module()
build_compartment_bulkhead = getattr(_bulkheads_doors, "build_compartment_bulkhead", _bulkheads_doors.build_standard_bulkhead)
build_standard_pressure_door = getattr(_bulkheads_doors, "build_standard_pressure_door", _bulkheads_doors.build_standard_watertight_door)
build_standard_bulkhead = _bulkheads_doors.build_standard_bulkhead
build_sliding_split_door = _bulkheads_doors.build_sliding_split_door
build_standard_watertight_door = _bulkheads_doors.build_standard_watertight_door
compute_bulkhead_top_z = _bulkheads_doors.compute_bulkhead_top_z
place_bulkhead_with_clearance = _bulkheads_doors.place_bulkhead_with_clearance

LENGTH = 4200.0
SUB_NAME = "Craniata"
HULL_THICK = 15.0
DECK_THICK = 18.0
BH_THICK = 14.0
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

BH_MAIN_FWD_NAME = "Fwd"
DOOR_CLEARANCE_H = 200.0
UPPER_DECK_AFT_NORM = SUPER_X_END - 0.003

MAIN_DECK_CUTOUTS = [
    {
        "name": "LowerAccess",
        "x_norm": 0.425,
        "y_center": 0.0,
        "half_length": 52.0,
        "half_width": 46.0,
        "coaming_height": 48.0,
    },
]

UPPER_DECK_CUTOUTS = [
    {
        "name": "UpperAccess",
        "x_norm": 0.425,
        "y_center": 0.0,
        "half_length": 56.0,
        "half_width": 48.0,
        "coaming_height": 52.0,
    },
]

LOWER_DECK_CUTOUTS = [
    {
        "name": "LowerService",
        "x_norm": 0.425,
        "y_center": 0.0,
        "half_length": 48.0,
        "half_width": 40.0,
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
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    r = max(0.0, main_r - HULL_THICK - 1.5)
    sb, super_w_local, super_h_local, shoulder_norm, _ = superstructure_state(nx)

    if sb > 0.0 and z >= main_r * shoulder_norm:
        sw = max(0.0, super_w_local * 0.5 - HULL_THICK - 1.5) * sb
        shoulder_z = main_r * shoulder_norm
        if z <= main_r:
            bt = (z - shoulder_z) / max(1.0, main_r - shoulder_z)
            hh = math.sqrt(max(0.0, r * r - z * z)) if abs(z) < r else 0.0
            return max(hh, hh + (sw - hh) * smoothstep(bt))

        dz = z - main_r
        sh = max(1.0, super_h_local * sb - 1.5)
        if dz < sh:
            return sw * math.sqrt(max(0.0, 1.0 - (dz / sh) ** 2))
        return 0.0

    if r <= 0.0 or abs(z) >= r:
        return 0.0
    return math.sqrt(max(0.0, r * r - z * z))


def upper_inner_apex_z(x_cm, margin=1.5):
    nx = max(0.0, min(1.0, x_cm / LENGTH))
    main_r = sample_curve(nx)
    sb, _, super_h_local, _, _ = superstructure_state(nx)
    if sb > 0.02:
        return main_r + super_h_local * sb - margin
    return main_r - HULL_THICK - margin


def upper_deck_hw(x_cm, z):
    return interior_contact_hw(x_cm, z)


def upper_bulkhead_hw(x_cm, z):
    return interior_contact_hw(x_cm, z)


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
    return (
        abs(x - cx) <= cutout["half_length"]
        and abs(y - cutout.get("y_center", 0.0)) <= cutout["half_width"]
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
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    try:
        bpy.ops.object.modifier_apply(modifier=modifier_name)
    finally:
        obj.select_set(False)


def apply_boolean_difference(target_obj, cutter_obj, modifier_name):
    mod = target_obj.modifiers.new(modifier_name, "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.solver = "EXACT"
    mod.object = cutter_obj

    bpy.context.view_layer.objects.active = target_obj
    target_obj.select_set(True)
    try:
        bpy.ops.object.modifier_apply(modifier=mod.name)
    finally:
        target_obj.select_set(False)

    bpy.data.objects.remove(cutter_obj, do_unlink=True)


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
    cutter.display_type = "WIRE"
    cutter.hide_render = True
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
    cutouts = deck_cutouts_for(name)
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
    add_solidify(obj, -DECK_THICK, 0)
    return obj


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


def build_hydroplanes():
    objs = []
    for name, xn, span, chord in (("Bow", 0.12, 180, 130), ("Stern", 0.88, 180, 130)):
        x = xn * LENGTH
        r = sample_curve(xn)
        half_chord = chord / 2
        for side, label in ((1, "Stbd"), (-1, "Port")):
            verts = [
                (x - half_chord, r * side, 0),
                (x + half_chord, r * side, 0),
                (x + half_chord * 0.5, (r + span) * side, 0),
                (x - half_chord * 0.4, (r + span) * side, 0),
            ]
            obj = make(f"SM_Hydro_{name}_{label}", verts, [(0, 1, 2, 3)], 0.16, 0.20, 0.18, smooth=False)
            add_solidify(obj, 10, 0)
            objs.append(obj)
    return objs


def build_fins():
    objs = []
    fx = 0.955 * LENGTH
    r = sample_curve(0.955)
    half_chord = 90

    for angle_deg in (45, 135, 225, 315):
        a = math.radians(angle_deg)
        ca, sa = math.cos(a), math.sin(a)
        span = 250
        verts = [
            (fx - half_chord, r * ca, r * sa),
            (fx + half_chord, r * ca, r * sa),
            (fx + half_chord * 0.6, (r + span) * ca, (r + span) * sa),
            (fx - half_chord * 0.3, (r + span) * ca, (r + span) * sa),
        ]
        obj = make(f"SM_Fin_{angle_deg}", verts, [(0, 1, 2, 3)], 0.16, 0.20, 0.18, smooth=False)
        add_solidify(obj, 12, 0)
        objs.append(obj)

    return objs


def build_rudder():
    nx = 0.955
    x_root = nx * LENGTH
    hull_r = sample_curve(nx)

    fairing_verts = [
        (x_root - 92.0, 0.0, hull_r * 0.28),
        (x_root + 8.0, 0.0, hull_r * 0.24),
        (x_root - 8.0, 0.0, hull_r + 118.0),
        (x_root - 86.0, 0.0, hull_r + 128.0),
    ]
    fairing = make("SM_Rudder_Fairing", fairing_verts, [(0, 1, 2, 3)], 0.16, 0.19, 0.17, smooth=False)
    add_solidify(fairing, 18.0, 0)

    post_verts = [
        (x_root - 18.0, 0.0, hull_r * 0.30),
        (x_root + 36.0, 0.0, hull_r * 0.26),
        (x_root + 22.0, 0.0, hull_r + 158.0),
        (x_root - 10.0, 0.0, hull_r + 170.0),
    ]
    post = make("SM_Rudder_Post", post_verts, [(0, 1, 2, 3)], 0.17, 0.20, 0.18, smooth=False)
    add_solidify(post, 16.0, 0)

    blade_verts = [
        (x_root + 42.0, 0.0, hull_r * 0.34),
        (x_root + 164.0, 0.0, hull_r * 0.22),
        (x_root + 138.0, 0.0, hull_r + 238.0),
        (x_root + 34.0, 0.0, hull_r + 216.0),
    ]
    blade = make("SM_Rudder", blade_verts, [(0, 1, 2, 3)], 0.19, 0.22, 0.20, smooth=False)
    add_solidify(blade, 14.0, 0)

    tip_verts = [
        (x_root + 126.0, 0.0, hull_r + 152.0),
        (x_root + 184.0, 0.0, hull_r + 142.0),
        (x_root + 172.0, 0.0, hull_r + 208.0),
        (x_root + 118.0, 0.0, hull_r + 216.0),
    ]
    tip = make("SM_Rudder_Tip", tip_verts, [(0, 1, 2, 3)], 0.17, 0.20, 0.18, smooth=False)
    add_solidify(tip, 12.0, 0)

    skeg_verts = [
        (x_root - 44.0, 0.0, -hull_r * 0.58),
        (x_root + 74.0, 0.0, -hull_r * 0.62),
        (x_root + 56.0, 0.0, -hull_r - 146.0),
        (x_root - 28.0, 0.0, -hull_r - 126.0),
    ]
    skeg = make("SM_Skeg", skeg_verts, [(0, 1, 2, 3)], 0.16, 0.18, 0.17, smooth=False)
    add_solidify(skeg, 13.0, 0)


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
    objs = []
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

    objs.append(make("SM_Duct", verts, faces, 0.20, 0.22, 0.20))

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

    objs.append(make("SM_Hub", hub_verts, hub_faces, 0.30, 0.28, 0.25))

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
    objs.append(make("SM_Rotor", rotor_verts, rotor_faces, 0.29, 0.27, 0.24))

    build_cylinder_x(
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
        objs.append(obj)

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
        objs.append(make(f"SM_Stator_{index}", verts, [(0, 1, 2, 3)], 0.20, 0.21, 0.20, smooth=False))

    return objs


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
    cutter.display_type = "WIRE"
    cutter.hide_render = True
    apply_boolean_difference(hull_obj, cutter, "AirlockExitCut")


def build_airlock_exit_cassette(spec):
    frame_x0 = spec["x_back"] - 6.0
    frame_x1 = frame_x0 + spec["cassette_depth"]
    opening_hw = spec["opening_hw"]
    cassette_hw = spec["cassette_outer_hw"]
    sill_z = spec["sill_z"]
    head_z = spec["opening_top_z"]

    build_box_object(
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
    )
    build_box_object(
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
    )
    build_box_object(
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
    )
    build_box_object(
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
    )
    build_box_object(
        "SM_Airlock_Cassette_Back",
        frame_x1,
        frame_x1 + 4.0,
        -cassette_hw,
        cassette_hw,
        sill_z,
        head_z,
        0.22,
        0.24,
        0.23,
    )

    build_sliding_split_door(
        name_prefix="SM_Airlock_Door",
        x_center=frame_x0 + 10.0,
        sill_z=sill_z,
        make_fn=make,
        append_box_fn=append_box,
        width=DOOR_W,
        height=DOOR_H,
        panel_gap=8.0,
        panel_margin=12.0,
        panel_depth=7.0,
        frame_depth=12.0,
        color=(0.33, 0.35, 0.34),
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
    top_cap = upper_inner_apex_z(x_cm, margin=1.5)
    top_z = min(
        compute_bulkhead_top_z(
            UPPER_ARMORY_PARTITION_X_NORM,
            upper_z,
            DECK_MAIN_Z,
            sample_curve,
            superstructure_state,
            min_main_height=420.0,
            min_upper_clearance=240.0,
            top_margin=12.0,
        ),
        top_cap,
    )
    place_bulkhead_with_clearance(
        name="SM_BH_Upper_Armory",
        x_cm=x_cm,
        z_min=upper_z,
        z_max=top_z,
        door_sill_z=upper_z,
        interior_hw_fn=upper_bulkhead_hw,
        make_fn=make,
        add_solidify_fn=add_solidify,
        bh_thickness=BH_THICK,
        door_w=DOOR_W,
        door_h=DOOR_H,
        door_clearance_h=DOOR_CLEARANCE_H,
        top_margin=24.0,
        max_z_cap=top_cap,
        color=(0.44, 0.42, 0.38),
    )


def build_lower_technical_layout():
    z0 = LOWER_WALKWAY_Z
    catwalk_z0 = z0 + 3.0
    catwalk_z1 = catwalk_z0 + BALLAST_CATWALK_THICK

    # Lower Deck Main only in the central hub room between ballast compartments.
    build_deck("lower_main", DECK_LOWER_Z, LOWER_HUB_START, LOWER_HUB_END, 0.29, 0.28, 0.26)

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
        build_manual_turret_module(name, x_center, socket_plane_z)
        return

    sign = 1.0 if mount == "top" else -1.0
    base_z0 = socket_plane_z if mount == "top" else socket_plane_z - 22.0
    base_z1 = base_z0 + 22.0
    build_box_object(
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
    )

    ring_z = base_z1 + sign * 6.0
    build_cylinder_x(
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
    )

    body_z0 = ring_z + (4.0 if mount == "top" else -32.0)
    body_z1 = body_z0 + 32.0
    build_box_object(
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
    )

    yoke_z0 = (body_z1 - 8.0) if mount == "top" else (body_z0 + 8.0)
    yoke_z1 = yoke_z0 + (10.0 if mount == "top" else -10.0)
    build_box_object(
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
    )

    barrel_z = body_z1 - 6.0 if mount == "top" else body_z0 + 6.0
    for index, barrel_y in enumerate((-12.0, 12.0)):
        build_cylinder_x(
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
        )


def build_manual_turret_module(name, x_center, socket_plane_z):
    tub_z0 = socket_plane_z
    tub_z1 = tub_z0 + 24.0
    build_box_object(
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
    )
    build_cylinder_x(
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
    )
    build_box_object(
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
    )
    for index, barrel_y in enumerate((-11.0, 11.0)):
        build_cylinder_x(
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
        )
    for index, grip_y in enumerate((-18.0, 18.0)):
        build_cylinder_x(
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
        )


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
    console_z0 = DECK_MAIN_Z + DECK_THICK
    console_z1 = console_z0 + 82.0
    build_box_object(
        "SM_HelmConsole",
        0.438 * LENGTH,
        0.492 * LENGTH,
        -46.0,
        46.0,
        console_z0,
        console_z1,
        0.22,
        0.23,
        0.22,
    )
    build_box_object(
        "SM_HelmDisplay",
        0.480 * LENGTH,
        0.492 * LENGTH,
        -42.0,
        42.0,
        console_z1 - 28.0,
        console_z1 + 18.0,
        0.18,
        0.20,
        0.19,
    )

    for suffix, side in (("Port", -1.0), ("Stbd", 1.0)):
        y0 = side * 150.0
        y1 = side * 92.0
        by0, by1 = sorted((y0, y1))
        build_box_object(
            f"SM_TurretStation_Fwd_{suffix}",
            0.472 * LENGTH,
            0.520 * LENGTH,
            by0,
            by1,
            console_z0,
            console_z0 + 74.0,
            0.21,
            0.22,
            0.21,
        )
        build_box_object(
            f"SM_TurretStation_Aft_{suffix}",
            0.560 * LENGTH,
            0.608 * LENGTH,
            by0,
            by1,
            console_z0,
            console_z0 + 74.0,
            0.21,
            0.22,
            0.21,
        )

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

    build_box_object(
        "SM_UpperArmoryRack",
        0.242 * LENGTH,
        0.266 * LENGTH,
        -150.0,
        150.0,
        upper_z + 4.0,
        upper_z + 150.0,
        0.22,
        0.23,
        0.22,
    )
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
            leaf_inset=4.0,
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
            leaf_inset=4.0,
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
        leaf_inset=4.0,
        color=(0.30, 0.31, 0.30),
    )


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
    build_deck("main", DECK_MAIN_Z, 0.002, 0.775, 0.36, 0.34, 0.30)
    upper_z = sample_curve(0.40) - 150
    build_deck("upper", upper_z, main_fwd_norm, UPPER_DECK_AFT_NORM, 0.40, 0.38, 0.34, hw_fn=upper_deck_hw)

    print("--- Lower technical layout (hub lower-main + ballast compartments) ---")
    build_lower_technical_layout()

    print("--- Propulsion room layout (reactor / engine / shaft) ---")
    build_propulsion_room_layout()

    print("--- Turret hardpoints (3) ---")
    build_turret_hardpoints()

    print("--- Hatch coamings ---")
    for cutout in MAIN_DECK_CUTOUTS:
        build_hatch_collar(f"SM_Hatch_{cutout['name']}", DECK_MAIN_Z, cutout, 0.34, 0.35, 0.33)
    for cutout in LOWER_DECK_CUTOUTS:
        build_hatch_collar(f"SM_Hatch_{cutout['name']}", DECK_LOWER_Z, cutout, 0.33, 0.34, 0.32)
    for cutout in UPPER_DECK_CUTOUTS:
        build_hatch_collar(f"SM_Hatch_{cutout['name']}", upper_z, cutout, 0.34, 0.35, 0.33)
    build_superstructure_armory_partition(upper_z)

    print("--- Bulkheads main ---")
    upper_deck_underside_z = upper_z - (DECK_THICK * 0.5) + 1.0
    for spec in MAIN_BULKHEAD_SPECS:
        x_cm = spec["x_norm"] * LENGTH
        hw_fn = upper_bulkhead_hw if spec["name"] == "Control" else interior_hw
        z_max = (
            min(
                compute_bulkhead_top_z(
                    spec["x_norm"],
                    upper_z,
                    DECK_MAIN_Z,
                    sample_curve,
                    superstructure_state,
                ),
                upper_inner_apex_z(x_cm, margin=1.5) if spec["name"] == "Control" else upper_ceiling_z(x_cm, margin=8.0),
            )
            if spec.get("full_height")
            else min(upper_deck_underside_z, upper_inner_apex_z(x_cm, margin=1.5) if spec["name"] == "Control" else upper_ceiling_z(x_cm, margin=8.0))
        )
        place_bulkhead_with_clearance(
            name=f"SM_BH_Main_{spec['name']}",
            x_cm=x_cm,
            z_min=DECK_MAIN_Z,
            z_max=z_max,
            door_sill_z=DECK_MAIN_Z,
            interior_hw_fn=hw_fn,
            make_fn=make,
            add_solidify_fn=add_solidify,
            bh_thickness=BH_THICK,
            door_w=spec["door_w"],
            door_h=spec["door_h"],
            door_clearance_h=DOOR_CLEARANCE_H,
            top_margin=36.0,
            max_z_cap=z_max,
            color=(0.44, 0.42, 0.38),
        )

    print("--- Bulkheads lower ---")
    for spec in LOWER_BULKHEAD_SPECS:
        lower_obj = place_bulkhead_with_clearance(
            name=f"SM_BH_Lower_{spec['name']}",
            x_cm=spec["x_norm"] * LENGTH,
            z_min=LOWER_BH_Z_MIN,
            z_max=DECK_MAIN_Z + 24.0,
            door_sill_z=DECK_LOWER_Z,
            interior_hw_fn=interior_hw,
            make_fn=make,
            add_solidify_fn=add_solidify,
            bh_thickness=BH_THICK,
            door_w=spec["door_w"],
            door_h=spec["door_h"],
            door_clearance_h=DOOR_CLEARANCE_H,
            top_margin=34.0,
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
    build_standard_bulkhead(
        name="SM_BH_Lower_TechPartition",
        x_cm=LOWER_TECH_PARTITION_X * LENGTH,
        z_min=LOWER_BH_Z_MIN,
        z_max=DECK_MAIN_Z + 32.0,
        door_sill_z=DECK_LOWER_Z,
        interior_hw_fn=interior_hw,
        make_fn=make,
        add_solidify_fn=add_solidify,
        bh_thickness=BH_THICK,
        door_w=0.0,
        door_h=0.0,
        gy=24,
        gz=20,
        color=(0.44, 0.42, 0.38),
    )

    print("--- Bulkheads upper airlock (inner only) ---")
    upper_bh_top = compute_bulkhead_top_z(
        UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"],
        upper_z,
        DECK_MAIN_Z,
        sample_curve,
        superstructure_state,
        min_main_height=420.0,
        min_upper_clearance=280.0,
        top_margin=16.0,
    )
    place_bulkhead_with_clearance(
        name=f"SM_BH_{UPPER_AIRLOCK_INNER_BULKHEAD['name']}",
        x_cm=UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH,
        z_min=upper_z,
        z_max=min(max(upper_z + 360.0, upper_bh_top), upper_inner_apex_z(UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH, margin=1.5)),
        door_sill_z=upper_z,
        interior_hw_fn=upper_bulkhead_hw,
        make_fn=make,
        add_solidify_fn=add_solidify,
        bh_thickness=BH_THICK,
        door_w=UPPER_AIRLOCK_INNER_BULKHEAD["door_w"],
        door_h=UPPER_AIRLOCK_INNER_BULKHEAD["door_h"],
        door_clearance_h=DOOR_CLEARANCE_H,
        top_margin=34.0,
        max_z_cap=upper_inner_apex_z(UPPER_AIRLOCK_INNER_BULKHEAD["x_norm"] * LENGTH, margin=1.5),
        color=(0.44, 0.42, 0.38),
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
    build_aft_top_access(spec)
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
    print("  - Rear SAS now uses a true hull cutout aligned to the exit door, without flat exit shell.")


main()
