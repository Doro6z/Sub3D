"""
Sub3D — Blockout Submarine Generator v3 for Blender
====================================================
Inspired by the AuthoringAsset / CompilerV2 pipeline that produced the
good-looking Proto03/04 submarines. Uses:
  - FRuntimeFloatCurve-style radius profile (hand-designed S-curve)
  - Cosine falloff for bow/stern caps
  - Superellipse cross-sections
  - Smooth InterpEaseInOut between compartment boundaries

Run: Blender > Scripting tab > Open > Run Script

All dimensions in centimeters. X=forward, Y=lateral, Z=up.
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ---------------------------------------------------------------------------
# HULL SHAPE — Ported from SubCompilerMvpFactory.cpp radius curve
# Scaled up from R~290 peak to R~320 for multi-deck (need 536cm interior height)
# ---------------------------------------------------------------------------

# Radius profile: (normalized_x, radius_cm)
# Inspired by MVP factory curve, scaled for R=320 peak
RADIUS_CURVE = [
    (0.00, 0.0),     # Bow tip (closed)
    (0.04, 120.0),   # Bow nose
    (0.10, 220.0),   # Bow shoulder
    (0.20, 290.0),   # Forward body begins
    (0.30, 315.0),   # Fore-body
    (0.45, 325.0),   # Mid-body peak (maître-bau)
    (0.55, 325.0),   # Mid-body sustained
    (0.70, 320.0),   # Aft-body
    (0.82, 280.0),   # Stern shoulder
    (0.92, 200.0),   # Stern taper
    (0.97, 100.0),   # Stern cone
    (1.00, 0.0),     # Stern tip (closed)
]

# Derived constants
SPINE_LENGTH = 2400.0          # Slightly longer for proportions
PEAK_RADIUS = max(r for _, r in RADIUS_CURVE)
WALL_THICKNESS = 14.0
SECTION_EXPONENT = 2.2         # Slightly squared for military sub look
WIDTH_TO_HEIGHT_RATIO = 1.05   # Slightly wider than tall

# Mesh resolution
RADIAL_SEGS = 36
LONG_SUBDIVISIONS = 6

# Multi-deck
UPPER_FLOOR_Z = 0.0
UPPER_CEIL_Z = 210.0
DECK_THICKNESS = 22.0
LOWER_CEIL_Z = UPPER_FLOOR_Z - DECK_THICKNESS
LOWER_FLOOR_Z = -185.0

# Compartments — positioned in the constant-radius zone
# Body zone: where radius >= 90% of peak (~292cm)
COMP_NAMES = ["Helm", "Crew", "Crew2", "Engine"]
# Manual placement for better proportions (not equally spaced)
COMP_BOUNDS_NORM = [
    (0.18, 0.32),   # Helm — forward, slightly in the taper
    (0.32, 0.48),   # Crew — forward-mid
    (0.48, 0.64),   # Crew2 — aft-mid
    (0.64, 0.80),   # Engine — aft, slightly in the taper
]

COMP_BOUNDS = [(s * SPINE_LENGTH, e * SPINE_LENGTH) for s, e in COMP_BOUNDS_NORM]
COMP_LENGTH = COMP_BOUNDS[0][1] - COMP_BOUNDS[0][0]  # ~336cm each
BULKHEAD_X = [COMP_BOUNDS[i][1] for i in range(len(COMP_NAMES) - 1)]
HATCH_X = [(b[0] + b[1]) / 2.0 for b in COMP_BOUNDS]
HATCH_SIZE = 85.0

# Doors
DOOR_UPPER_W, DOOR_UPPER_H = 95.0, 190.0
DOOR_LOWER_W, DOOR_LOWER_H = 85.0, 160.0

# Airlock
AIRLOCK_L, AIRLOCK_W, AIRLOCK_H = 160.0, 130.0, 210.0

# Colors
COLORS = {
    'M_Hull':       (0.22, 0.24, 0.28),
    'M_Interior_U': (0.42, 0.42, 0.40),
    'M_Interior_L': (0.32, 0.33, 0.30),
    'M_Deck':       (0.36, 0.34, 0.30),
    'M_Bulkhead':   (0.48, 0.46, 0.42),
    'M_Airlock':    (0.38, 0.36, 0.33),
}


# ---------------------------------------------------------------------------
# CORE MATH
# ---------------------------------------------------------------------------

def superellipse_pow(base, exp):
    """Signed power: sign(x) * |x|^exp. From SubmarineMeshBuilder.cpp:29."""
    if abs(base) < 1e-6:
        return 0.0
    return math.copysign(1, base) * (abs(base) ** exp)


def lerp(a, b, t):
    return a + (b - a) * t


def ease_in_out(t, exp=2.0):
    """InterpEaseInOut from UE. Smooth S-curve interpolation."""
    if t < 0.5:
        return 0.5 * (2.0 * t) ** exp
    else:
        return 1.0 - 0.5 * (2.0 * (1.0 - t)) ** exp


def sample_radius_curve(norm_x):
    """Sample the hand-designed radius curve with smooth cubic interpolation."""
    norm_x = max(0.0, min(1.0, norm_x))

    # Find the bracketing keys
    for i in range(len(RADIUS_CURVE) - 1):
        x0, r0 = RADIUS_CURVE[i]
        x1, r1 = RADIUS_CURVE[i + 1]
        if x0 <= norm_x <= x1:
            t = (norm_x - x0) / max(1e-6, x1 - x0)
            # Smooth hermite interpolation (cubic)
            t2 = t * t
            t3 = t2 * t
            h = 3 * t2 - 2 * t3  # smoothstep
            return r0 + (r1 - r0) * h

    return RADIUS_CURVE[-1][1]


def section_point(radius, angle):
    """Superellipse cross-section point at given angle. Returns (y, z)."""
    half_h = max(1.0, radius)
    half_w = half_h * WIDTH_TO_HEIGHT_RATIO
    exp = 2.0 / max(0.5, SECTION_EXPONENT)
    y = half_w * superellipse_pow(math.cos(angle), exp)
    z = half_h * superellipse_pow(math.sin(angle), exp)
    return y, z


def section_half_width(radius, z_offset):
    """Half-width of superellipse at given vertical offset."""
    half_h = max(1.0, radius)
    half_w = half_h * WIDTH_TO_HEIGHT_RATIO
    n = max(0.5, SECTION_EXPONENT)
    ratio = abs(z_offset) / half_h
    if ratio >= 1.0:
        return 0.0
    return half_w * max(0.0, 1.0 - ratio ** n) ** (1.0 / n)


# ---------------------------------------------------------------------------
# BLENDER HELPERS
# ---------------------------------------------------------------------------

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)


def get_mat(name):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            c = COLORS.get(name, (0.5, 0.5, 0.5))
            bsdf.inputs["Base Color"].default_value = (*c, 1.0)
            bsdf.inputs["Roughness"].default_value = 0.75
    return mat


def make_obj(name, verts, faces, mat_name):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(mat_name))
    # Fix normals
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.select_set(False)
    # Smooth shading
    for poly in obj.data.polygons:
        poly.use_smooth = True
    return obj


# ---------------------------------------------------------------------------
# HULL EXTERIOR — Curve-based, inspired by SubmarineGeometryBuilder
# ---------------------------------------------------------------------------

def build_hull():
    """Hull with smooth radius curve profile. No parametric EnvelopeDef."""
    verts = []
    faces = []

    # Generate rings along the full spine using the radius curve
    num_rings = 80  # High resolution for smooth hull
    rings = []  # (x, radius)

    for i in range(num_rings + 1):
        norm = i / num_rings
        x = norm * SPINE_LENGTH
        r = sample_radius_curve(norm)
        if r < 0.5:
            r = 0.5  # Avoid degenerate
        rings.append((x, r))

    # Emit vertices with superellipse cross-section
    for rx, rr in rings:
        for seg in range(RADIAL_SEGS):
            theta = 2.0 * math.pi * seg / RADIAL_SEGS
            y, z = section_point(rr, theta)
            verts.append(Vector((rx, y, z)))

    # Connect rings
    for i in range(len(rings) - 1):
        for seg in range(RADIAL_SEGS):
            ns = (seg + 1) % RADIAL_SEGS
            a0 = i * RADIAL_SEGS + seg
            a1 = i * RADIAL_SEGS + ns
            b0 = (i + 1) * RADIAL_SEGS + seg
            b1 = (i + 1) * RADIAL_SEGS + ns
            faces.append((a0, a1, b1, b0))

    # Bow tip fan
    bow_ci = len(verts)
    verts.append(Vector((0, 0, 0)))
    for seg in range(RADIAL_SEGS):
        ns = (seg + 1) % RADIAL_SEGS
        faces.append((bow_ci, ns, seg))

    # Stern tip fan
    stern_ci = len(verts)
    verts.append(Vector((SPINE_LENGTH, 0, 0)))
    last_base = (len(rings) - 1) * RADIAL_SEGS
    for seg in range(RADIAL_SEGS):
        ns = (seg + 1) % RADIAL_SEGS
        faces.append((stern_ci, last_base + seg, last_base + ns))

    return make_obj("SM_Hull_Exterior", verts, faces, 'M_Hull')


# ---------------------------------------------------------------------------
# DECK FLOOR
# ---------------------------------------------------------------------------

def build_deck():
    bm = bmesh.new()
    nx, ny = 60, 28

    # Deck spans only the compartment zone
    x_min = COMP_BOUNDS[0][0]
    x_max = COMP_BOUNDS[-1][1]

    grid = {}
    for ix in range(nx + 1):
        x = x_min + (x_max - x_min) * ix / nx
        norm_x = x / SPINE_LENGTH
        local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)
        hw = section_half_width(local_r, UPPER_FLOOR_Z)
        if hw < 10:
            continue

        for iy in range(ny + 1):
            y = -hw + 2.0 * hw * iy / ny
            # Verify inside superellipse
            half_h = local_r
            half_w = local_r * WIDTH_TO_HEIGHT_RATIO
            n = SECTION_EXPONENT
            if half_w > 0 and half_h > 0:
                test = (abs(y) / half_w) ** n + (abs(UPPER_FLOOR_Z) / half_h) ** n
                if test <= 1.0:
                    grid[(ix, iy)] = bm.verts.new((x, y, UPPER_FLOOR_Z))

    bm.verts.ensure_lookup_table()

    for ix in range(nx):
        for iy in range(ny):
            vs = [grid.get((ix, iy)), grid.get((ix + 1, iy)),
                  grid.get((ix + 1, iy + 1)), grid.get((ix, iy + 1))]
            if all(vs):
                cx = (vs[0].co.x + vs[2].co.x) / 2
                cy = (vs[0].co.y + vs[2].co.y) / 2
                in_hatch = any(abs(cx - hx) < HATCH_SIZE / 2 and abs(cy) < HATCH_SIZE / 2
                               for hx in HATCH_X)
                if not in_hatch:
                    try:
                        bm.faces.new(vs)
                    except ValueError:
                        pass

    mesh = bpy.data.meshes.new("SM_DeckFloor")
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    obj = bpy.data.objects.new("SM_DeckFloor", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat('M_Deck'))

    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = -DECK_THICKNESS
    mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# INTERIOR COMPARTMENTS — walls follow hull radius curve
# ---------------------------------------------------------------------------

def build_interior_upper(name, x_start, x_end):
    verts = []
    faces = []
    arc_segs = 20
    x_segs = 8

    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        norm_x = x / SPINE_LENGTH
        local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)

        for ia in range(arc_segs + 1):
            angle = math.pi * ia / arc_segs  # 0 (starboard) to pi (port)
            y, z = section_point(local_r, angle)
            z = max(UPPER_FLOOR_Z, min(UPPER_CEIL_Z, z))
            verts.append(Vector((x, y, z)))

    ring = arc_segs + 1
    for ix in range(x_segs):
        for ia in range(arc_segs):
            v0 = ix * ring + ia
            v1 = ix * ring + ia + 1
            v2 = (ix + 1) * ring + ia + 1
            v3 = (ix + 1) * ring + ia
            faces.append((v0, v3, v2, v1))

    # Ceiling
    cb = len(verts)
    ceil_segs = 8
    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        norm_x = x / SPINE_LENGTH
        local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)
        hw = section_half_width(local_r, UPPER_CEIL_Z)
        if hw < 5:
            hw = 50
        for iy in range(ceil_segs + 1):
            y = -hw + 2 * hw * iy / ceil_segs
            verts.append(Vector((x, y, UPPER_CEIL_Z)))

    cr = ceil_segs + 1
    for ix in range(x_segs):
        for iy in range(ceil_segs):
            faces.append((cb + ix * cr + iy, cb + ix * cr + iy + 1,
                          cb + (ix + 1) * cr + iy + 1, cb + (ix + 1) * cr + iy))

    return make_obj(f"SM_Interior_{name}Upper", verts, faces, 'M_Interior_U')


def build_interior_lower(name, x_start, x_end):
    verts = []
    faces = []
    arc_segs = 20
    x_segs = 8

    # Lower arc: from LOWER_CEIL_Z (starboard) through bottom to LOWER_CEIL_Z (port)
    a_ceil = math.asin(max(-1, min(1, LOWER_CEIL_Z / max(1, PEAK_RADIUS - WALL_THICKNESS))))
    a_start = a_ceil
    a_end = -(math.pi + a_ceil)

    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        norm_x = x / SPINE_LENGTH
        local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)

        for ia in range(arc_segs + 1):
            t = ia / arc_segs
            angle = a_start + (a_end - a_start) * t
            y, z = section_point(local_r, angle)
            z = min(z, LOWER_CEIL_Z)
            verts.append(Vector((x, y, z)))

    ring = arc_segs + 1
    for ix in range(x_segs):
        for ia in range(arc_segs):
            v0 = ix * ring + ia
            v1 = ix * ring + ia + 1
            v2 = (ix + 1) * ring + ia + 1
            v3 = (ix + 1) * ring + ia
            faces.append((v0, v1, v2, v3))

    # Floor plane at LOWER_FLOOR_Z
    fb = len(verts)
    fy_segs = 10
    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        norm_x = x / SPINE_LENGTH
        local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)
        hw = section_half_width(local_r, LOWER_FLOOR_Z)
        if hw < 30:
            hw = 80
        for iy in range(fy_segs + 1):
            y = -hw + 2 * hw * iy / fy_segs
            verts.append(Vector((x, y, LOWER_FLOOR_Z)))

    fr = fy_segs + 1
    for ix in range(x_segs):
        for iy in range(fy_segs):
            faces.append((fb + ix * fr + iy, fb + ix * fr + iy + 1,
                          fb + (ix + 1) * fr + iy + 1, fb + (ix + 1) * fr + iy))

    return make_obj(f"SM_Interior_{name}Lower", verts, faces, 'M_Interior_L')


# ---------------------------------------------------------------------------
# BULKHEADS
# ---------------------------------------------------------------------------

def build_bulkhead(name, x_pos, z_min, z_max, door_w, door_h, door_sill_z):
    bm = bmesh.new()
    gy, gz = 22, 18
    norm_x = x_pos / SPINE_LENGTH
    local_r = max(10, sample_radius_curve(norm_x) - WALL_THICKNESS)
    half_h = local_r
    half_w = local_r * WIDTH_TO_HEIGHT_RATIO
    n = SECTION_EXPONENT

    grid = {}
    for iz in range(gz + 1):
        z = z_min + (z_max - z_min) * iz / gz
        for iy in range(gy + 1):
            hw = section_half_width(local_r, z)
            if hw < 1:
                continue
            y = -hw + 2 * hw * iy / gy

            # Inside superellipse?
            if half_w > 0 and half_h > 0:
                test = (abs(y) / half_w) ** n + (abs(z) / half_h) ** n
                if test > 1.0:
                    continue

            # Inside door?
            if (-door_w / 2 < y < door_w / 2 and
                    door_sill_z < z < door_sill_z + door_h):
                continue

            grid[(iy, iz)] = bm.verts.new((x_pos, y, z))

    bm.verts.ensure_lookup_table()
    for iz in range(gz):
        for iy in range(gy):
            vs = [grid.get((iy, iz)), grid.get((iy + 1, iz)),
                  grid.get((iy + 1, iz + 1)), grid.get((iy, iz + 1))]
            if all(vs):
                try:
                    bm.faces.new(vs)
                except ValueError:
                    pass

    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat('M_Bulkhead'))
    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = WALL_THICKNESS
    mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# AIRLOCK
# ---------------------------------------------------------------------------

def build_airlock():
    # Position: port side of Engine compartment
    eng_mid_x = (COMP_BOUNDS[-1][0] + COMP_BOUNDS[-1][1]) / 2
    norm_x = eng_mid_x / SPINE_LENGTH
    local_r = sample_radius_curve(norm_x) - WALL_THICKNESS
    cx = eng_mid_x
    cy = -(local_r + AIRLOCK_W / 2)
    cz = -90.0
    hl, hw, hh = AIRLOCK_L / 2, AIRLOCK_W / 2, AIRLOCK_H / 2

    verts = [
        Vector((cx - hl, cy - hw, cz - hh)),
        Vector((cx + hl, cy - hw, cz - hh)),
        Vector((cx + hl, cy + hw, cz - hh)),
        Vector((cx - hl, cy + hw, cz - hh)),
        Vector((cx - hl, cy - hw, cz + hh)),
        Vector((cx + hl, cy - hw, cz + hh)),
        Vector((cx + hl, cy + hw, cz + hh)),
        Vector((cx - hl, cy + hw, cz + hh)),
    ]
    faces = [
        (0, 1, 2, 3), (4, 7, 6, 5),
        (0, 4, 5, 1), (2, 6, 7, 3),
        (0, 3, 7, 4), (1, 5, 6, 2),
    ]
    obj = make_obj("SM_Airlock", verts, faces, 'M_Airlock')
    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = -WALL_THICKNESS
    mod.offset = -1
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout Submarine v3 — AuthoringAsset-inspired")
    print("=" * 60)

    clear_scene()
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = 'CENTIMETERS'

    # Fix viewport clipping
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.clip_start = 1.0
                    space.clip_end = 500000.0

    print(f"\nSpine: {SPINE_LENGTH}cm, Peak R: {PEAK_RADIUS}cm")
    print(f"Section: exponent={SECTION_EXPONENT}, W/H={WIDTH_TO_HEIGHT_RATIO}")
    print(f"Radius curve: {len(RADIUS_CURVE)} control points")
    print(f"Compartments: {len(COMP_NAMES)} x 2 decks")

    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        r_s = sample_radius_curve(xs / SPINE_LENGTH)
        r_e = sample_radius_curve(xe / SPINE_LENGTH)
        print(f"  {name}: X=[{xs:.0f}, {xe:.0f}] R=[{r_s:.0f}, {r_e:.0f}]")

    print("\n--- Building Hull ---")
    build_hull()

    print("--- Building Deck ---")
    build_deck()

    print("--- Building Interiors ---")
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        build_interior_upper(name, xs, xe)
        build_interior_lower(name, xs, xe)

    print("--- Building Bulkheads ---")
    for i, bx in enumerate(BULKHEAD_X):
        build_bulkhead(f"SM_Bulkhead_Upper_{i}", bx,
                       UPPER_FLOOR_Z, UPPER_CEIL_Z,
                       DOOR_UPPER_W, DOOR_UPPER_H, UPPER_FLOOR_Z)
        build_bulkhead(f"SM_Bulkhead_Lower_{i}", bx,
                       LOWER_FLOOR_Z, LOWER_CEIL_Z,
                       DOOR_LOWER_W, DOOR_LOWER_H, LOWER_FLOOR_Z)

    print("--- Building Airlock ---")
    build_airlock()

    # Frame view
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    print("OBJECTS:")
    tv, tf = 0, 0
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if obj.type == 'MESH':
            v, f = len(obj.data.vertices), len(obj.data.polygons)
            tv += v; tf += f
            print(f"  {obj.name}: {v}V {f}F")
    print(f"  TOTAL: {tv}V {tf}F")
    print("=" * 60)
    print(f"Export: FBX, Scale 1.0, Forward X, Up Z")


if __name__ == "__main__":
    main()
