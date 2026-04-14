"""
Sub3D — Blockout Submarine: Surcouf-Inspired Hull (Script 2)
=============================================================
Analysis-driven hull generation based on real Surcouf proportions.

The Surcouf (NN3, 1929) was a French submarine cruiser:
  - 110m long, 9.07m beam, 7.25m draft
  - Double hull construction (pressure hull inside outer fairing)
  - L/B ratio ~12.1 (very elongated)
  - 2-3 internal decks in the pressure hull
  - Distinctive elongated bow, stubby stern

This script extracts the Surcouf's proportions as normalized ratios,
then scales them to gameplay dimensions (multi-deck, FPS-navigable).

Methodology:
  1. Hull profile: 20-point radius curve traced from historical side profiles
  2. Cross-section: NOT a simple circle — compound shape:
     - Upper half: rounded (semi-ellipse, wider than tall)
     - Lower half: flatter, with a broader keel line
  3. Scale factor computed from interior deck height requirements

Run: Blender > Scripting tab > Open > Run Script
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ---------------------------------------------------------------------------
# SURCOUF PROPORTIONS — Normalized analysis
# ---------------------------------------------------------------------------

# Historical dimensions (cm)
SURCOUF_LENGTH = 11000.0  # 110m
SURCOUF_BEAM = 907.0      # 9.07m (outer hull)
SURCOUF_PRESSURE_HULL_DIAM = 650.0  # ~6.5m estimated pressure hull

# Ratio analysis
SURCOUF_LB_RATIO = SURCOUF_LENGTH / SURCOUF_BEAM  # ~12.1
SURCOUF_BOW_FRACTION = 0.18     # Bow taper occupies ~18% of length
SURCOUF_BODY_FRACTION = 0.52    # Constant-section body ~52%
SURCOUF_STERN_FRACTION = 0.16   # Stern taper ~16%
SURCOUF_SAIL_POSITION = 0.35    # Sail/kiosk at ~35% from bow
# Remaining 14% is transitions

# ---------------------------------------------------------------------------
# GAMEPLAY SCALE — Derived from interior requirements
# ---------------------------------------------------------------------------

# Interior requirements
UPPER_DECK_HEIGHT = 210.0    # Standing height upper deck (cm)
LOWER_DECK_HEIGHT = 170.0    # Standing height lower deck (cm)
DECK_PLATE_THICK = 22.0      # Deck floor thickness (cm)
WALL_THICKNESS = 14.0         # Hull wall thickness (cm)

# Minimum interior height = upper deck + plate + lower deck
MIN_INTERIOR_HEIGHT = UPPER_DECK_HEIGHT + DECK_PLATE_THICK + LOWER_DECK_HEIGHT  # = 402cm
# Interior radius needed = height/2 (for circular section, adjustable for oval)
# With oval section (taller than wide in interior):
INTERIOR_HALF_HEIGHT = MIN_INTERIOR_HEIGHT / 2.0 + 30  # +30 margin = 231cm
PRESSURE_HULL_RADIUS = INTERIOR_HALF_HEIGHT + WALL_THICKNESS  # ~245cm

# Outer hull: ~15% larger than pressure hull (historical double-hull ratio)
OUTER_HULL_RADIUS_TOP = PRESSURE_HULL_RADIUS * 1.10   # ~270cm top
OUTER_HULL_RADIUS_SIDE = PRESSURE_HULL_RADIUS * 1.15  # ~282cm sides (wider)
OUTER_HULL_RADIUS_BOTTOM = PRESSURE_HULL_RADIUS * 1.08  # ~265cm bottom (flatter)

# Length: use compressed L/B ratio for gameplay (not 12:1 which would be 65m)
GAMEPLAY_LB_RATIO = 8.5
SPINE_LENGTH = GAMEPLAY_LB_RATIO * OUTER_HULL_RADIUS_SIDE * 2  # ~4800cm (48m)
# Cap to reasonable value
SPINE_LENGTH = 4200.0  # 42m — traversable in ~14s at sprint speed

# Deck positions (relative to submarine center Z=0)
UPPER_FLOOR_Z = 10.0      # Slightly above center (more headroom below)
UPPER_CEIL_Z = UPPER_FLOOR_Z + UPPER_DECK_HEIGHT  # 220
LOWER_CEIL_Z = UPPER_FLOOR_Z - DECK_PLATE_THICK    # -12
LOWER_FLOOR_Z = LOWER_CEIL_Z - LOWER_DECK_HEIGHT   # -182

# ---------------------------------------------------------------------------
# HULL PROFILE — Traced from Surcouf side-view photographs
# ---------------------------------------------------------------------------

# Normalized (x, radius_multiplier) where 1.0 = max radius
# Traced from the Surcouf cutaway image, emphasizing:
# - Very gradual, long bow approach
# - Gentle shoulder (not abrupt transition)
# - Long constant body
# - Short, more aggressive stern
# - Slight "cigar" asymmetry (bow narrower than stern at equivalent distance)

HULL_PROFILE = [
    # x_norm, radius_fraction (0-1 where 1 = max beam at that height)
    (0.000, 0.000),   # Bow tip
    (0.015, 0.050),   # Bow — very sharp start
    (0.035, 0.150),   # Bow — still narrow
    (0.060, 0.300),   # Bow — opening up
    (0.090, 0.480),   # Bow shoulder — the "swelling" begins
    (0.120, 0.650),   # Approaching body
    (0.155, 0.810),   # Nearly full width
    (0.190, 0.920),   # Transition zone
    (0.220, 0.975),   # Almost body
    (0.250, 1.000),   # Body begins — maximum section
    (0.500, 1.000),   # Mid-body — sustained max
    (0.650, 1.000),   # Aft body — still max
    (0.720, 0.985),   # Aft — very slight taper begins
    (0.770, 0.950),   # Stern transition starts
    (0.820, 0.880),   # Stern shoulder
    (0.860, 0.780),   # Stern tapering
    (0.900, 0.620),   # Stern — narrowing fast
    (0.935, 0.420),   # Stern cone
    (0.960, 0.250),   # Stern narrow
    (0.980, 0.120),   # Near tip
    (1.000, 0.000),   # Stern tip
]

# ---------------------------------------------------------------------------
# CROSS-SECTION — Compound shape (not a simple circle)
# ---------------------------------------------------------------------------
# The Surcouf's pressure hull was roughly circular, but the outer hull had:
# - Flat bottom (for resting on keel blocks in drydock)
# - Slightly wider beam (horizontal > vertical)
# - Rounded top
#
# We model this as a height-dependent radius modifier:
#   At angle θ (0=starboard, π/2=top, π=port, 3π/2=bottom):
#   radius = base_R * section_modifier(θ)

def section_modifier(angle):
    """Modifies the radius based on angle around the cross-section.
    Creates a slightly flattened bottom, wider sides, rounded top."""
    sin_a = math.sin(angle)
    cos_a = math.cos(angle)

    # Base: slightly wider than tall (W/H = 1.08)
    wh = 1.08

    # Bottom flattening: reduce radius at bottom by ~8%
    # Top rounding: keep full radius at top
    if sin_a < 0:  # Bottom half
        # Flatten: interpolate between circle and flat
        flatten = 0.08 * (abs(sin_a) ** 1.5)  # More flatten at very bottom
        return wh * abs(cos_a) + (1.0 - flatten) * abs(sin_a)
    else:  # Top half
        # Normal ellipse, slightly taller top
        height_boost = 1.02
        y = wh * cos_a
        z = height_boost * sin_a
        return math.sqrt(y * y + z * z)


def section_point_surcouf(radius, angle):
    """Cross-section point using compound shape."""
    # Base superellipse with slight squaring (exponent 2.15)
    exp = 2.0 / 2.15
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)

    def spow(base, e):
        if abs(base) < 1e-6: return 0.0
        return math.copysign(1, base) * abs(base) ** e

    # Width/height differentiation
    w_ratio = 1.08   # Wider than tall
    h_top = 1.02     # Slightly taller on top
    h_bot = 0.92     # Flatter on bottom

    h_ratio = h_top if sin_a >= 0 else h_bot

    y = radius * w_ratio * spow(cos_a, exp)
    z = radius * h_ratio * spow(sin_a, exp)

    return y, z


# ---------------------------------------------------------------------------
# RADIUS SAMPLING
# ---------------------------------------------------------------------------

def sample_hull_profile(norm_x):
    """Sample hull profile with cubic hermite interpolation."""
    norm_x = max(0.0, min(1.0, norm_x))
    for i in range(len(HULL_PROFILE) - 1):
        x0, r0 = HULL_PROFILE[i]
        x1, r1 = HULL_PROFILE[i + 1]
        if x0 <= norm_x <= x1:
            t = (norm_x - x0) / max(1e-6, x1 - x0)
            # Cubic hermite (smoothstep)
            t = t * t * (3.0 - 2.0 * t)
            return r0 + (r1 - r0) * t
    return 0.0


def hull_radius_at(norm_x):
    """Actual radius in cm at normalized position."""
    profile = sample_hull_profile(norm_x)
    return profile * OUTER_HULL_RADIUS_SIDE


def interior_radius_at(norm_x):
    """Interior radius (pressure hull) at normalized position."""
    outer = hull_radius_at(norm_x)
    # Pressure hull is a constant fraction of outer hull in the body zone,
    # but follows the taper in bow/stern
    if outer < WALL_THICKNESS * 2:
        return max(1.0, outer - WALL_THICKNESS)
    return max(1.0, outer - WALL_THICKNESS)


def section_half_width(radius, z_offset):
    """Half-width at given Z for the Surcouf section shape."""
    # Using compound section shape
    if radius <= 0:
        return 0
    h_ratio = 1.02 if z_offset >= 0 else 0.92
    effective_h = radius * h_ratio
    if abs(z_offset) >= effective_h:
        return 0
    ratio = abs(z_offset) / effective_h
    n = 2.15
    return radius * 1.08 * max(0, 1 - ratio ** n) ** (1 / n)


# ---------------------------------------------------------------------------
# COMPARTMENTS
# ---------------------------------------------------------------------------

COMP_NAMES = ["Torpedo", "Helm", "Crew", "Crew2", "Engine"]

# Placement based on Surcouf layout analysis:
# - Torpedo room at bow (narrower, single deck possible)
# - Operations/Helm forward-center
# - Crew quarters mid
# - Secondary crew/storage aft-mid
# - Engine room aft
COMP_BOUNDS_NORM = [
    (0.15, 0.25),   # Torpedo — in the bow taper (narrower)
    (0.25, 0.38),   # Helm — forward body
    (0.38, 0.52),   # Crew — mid-body
    (0.52, 0.66),   # Crew2 — aft-mid
    (0.66, 0.82),   # Engine — aft body
]

COMP_BOUNDS = [(s * SPINE_LENGTH, e * SPINE_LENGTH) for s, e in COMP_BOUNDS_NORM]
BULKHEAD_X = [COMP_BOUNDS[i][1] for i in range(len(COMP_NAMES) - 1)]
HATCH_X = [(b[0] + b[1]) / 2.0 for b in COMP_BOUNDS]
HATCH_SIZE = 85.0

DOOR_UPPER_W, DOOR_UPPER_H = 95.0, 190.0
DOOR_LOWER_W, DOOR_LOWER_H = 85.0, 160.0

# Airlock
AIRLOCK_L, AIRLOCK_W, AIRLOCK_H = 160.0, 130.0, 210.0

# Mesh resolution
RADIAL_SEGS = 40
HULL_LONG_SEGS = 100  # Very smooth hull

# Colors
COLORS = {
    'M_Hull':       (0.18, 0.20, 0.24),  # Dark steel
    'M_Hull_Lower': (0.30, 0.12, 0.10),  # Anti-fouling red (below waterline)
    'M_Interior_U': (0.42, 0.42, 0.38),
    'M_Interior_L': (0.30, 0.30, 0.28),
    'M_Deck':       (0.35, 0.33, 0.28),
    'M_Bulkhead':   (0.45, 0.43, 0.40),
    'M_Airlock':    (0.36, 0.34, 0.30),
}


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
            bsdf.inputs["Roughness"].default_value = 0.7
            bsdf.inputs["Metallic"].default_value = 0.6
    return mat


def make_obj(name, verts, faces, mat_name, smooth=True):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(mat_name))
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True
    obj.select_set(False)
    return obj


# ---------------------------------------------------------------------------
# HULL — Surcouf profile with compound cross-section
# ---------------------------------------------------------------------------

def build_hull():
    """Build outer hull using Surcouf profile + compound section."""
    verts = []
    faces = []

    for i in range(HULL_LONG_SEGS + 1):
        norm_x = i / HULL_LONG_SEGS
        x = norm_x * SPINE_LENGTH
        r = hull_radius_at(norm_x)

        if r < 0.5:
            r = 0.5

        for seg in range(RADIAL_SEGS):
            angle = 2.0 * math.pi * seg / RADIAL_SEGS
            y, z = section_point_surcouf(r, angle)
            verts.append(Vector((x, y, z)))

    # Connect rings
    for i in range(HULL_LONG_SEGS):
        for seg in range(RADIAL_SEGS):
            ns = (seg + 1) % RADIAL_SEGS
            a0 = i * RADIAL_SEGS + seg
            a1 = i * RADIAL_SEGS + ns
            b0 = (i + 1) * RADIAL_SEGS + seg
            b1 = (i + 1) * RADIAL_SEGS + ns
            faces.append((a0, a1, b1, b0))

    # Bow tip
    bc = len(verts)
    verts.append(Vector((0, 0, 0)))
    for seg in range(RADIAL_SEGS):
        ns = (seg + 1) % RADIAL_SEGS
        faces.append((bc, ns, seg))

    # Stern tip
    sc = len(verts)
    verts.append(Vector((SPINE_LENGTH, 0, 0)))
    lb = HULL_LONG_SEGS * RADIAL_SEGS
    for seg in range(RADIAL_SEGS):
        ns = (seg + 1) % RADIAL_SEGS
        faces.append((sc, lb + seg, lb + ns))

    return make_obj("SM_Hull_Exterior", verts, faces, 'M_Hull')


# ---------------------------------------------------------------------------
# DECK FLOOR
# ---------------------------------------------------------------------------

def build_deck():
    bm = bmesh.new()
    nx, ny = 60, 28
    x_min = COMP_BOUNDS[0][0]
    x_max = COMP_BOUNDS[-1][1]

    grid = {}
    for ix in range(nx + 1):
        x = x_min + (x_max - x_min) * ix / nx
        norm_x = x / SPINE_LENGTH
        local_r = interior_radius_at(norm_x)
        hw = section_half_width(local_r, UPPER_FLOOR_Z)
        if hw < 10:
            continue
        for iy in range(ny + 1):
            y = -hw + 2.0 * hw * iy / ny
            grid[(ix, iy)] = bm.verts.new((x, y, UPPER_FLOOR_Z))

    bm.verts.ensure_lookup_table()

    for ix in range(nx):
        for iy in range(ny):
            vs = [grid.get(k) for k in [(ix, iy), (ix+1, iy), (ix+1, iy+1), (ix, iy+1)]]
            if all(vs):
                cx = (vs[0].co.x + vs[2].co.x) / 2
                cy = (vs[0].co.y + vs[2].co.y) / 2
                in_hatch = any(abs(cx - hx) < HATCH_SIZE/2 and abs(cy) < HATCH_SIZE/2
                               for hx in HATCH_X)
                if not in_hatch:
                    try: bm.faces.new(vs)
                    except ValueError: pass

    mesh = bpy.data.meshes.new("SM_DeckFloor")
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new("SM_DeckFloor", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat('M_Deck'))
    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = -DECK_PLATE_THICK
    mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# INTERIOR WALLS
# ---------------------------------------------------------------------------

def build_interior_upper(name, x_start, x_end):
    verts, faces = [], []
    arc_segs, x_segs = 22, 8

    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        local_r = interior_radius_at(x / SPINE_LENGTH)

        for ia in range(arc_segs + 1):
            angle = math.pi * ia / arc_segs
            y, z = section_point_surcouf(local_r, angle)
            z = max(UPPER_FLOOR_Z, min(UPPER_CEIL_Z, z))
            verts.append(Vector((x, y, z)))

    ring = arc_segs + 1
    for ix in range(x_segs):
        for ia in range(arc_segs):
            v0 = ix * ring + ia
            v1 = ix * ring + ia + 1
            v2 = (ix+1) * ring + ia + 1
            v3 = (ix+1) * ring + ia
            faces.append((v0, v3, v2, v1))

    # Ceiling
    cb = len(verts)
    cs = 8
    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        lr = interior_radius_at(x / SPINE_LENGTH)
        hw = section_half_width(lr, UPPER_CEIL_Z)
        if hw < 5: hw = 50
        for iy in range(cs + 1):
            y = -hw + 2 * hw * iy / cs
            verts.append(Vector((x, y, UPPER_CEIL_Z)))

    cr = cs + 1
    for ix in range(x_segs):
        for iy in range(cs):
            faces.append((cb+ix*cr+iy, cb+ix*cr+iy+1,
                          cb+(ix+1)*cr+iy+1, cb+(ix+1)*cr+iy))

    return make_obj(f"SM_Interior_{name}Upper", verts, faces, 'M_Interior_U')


def build_interior_lower(name, x_start, x_end):
    verts, faces = [], []
    arc_segs, x_segs = 22, 8

    # Lower arc angles
    peak_r = interior_radius_at(((x_start + x_end) / 2) / SPINE_LENGTH)
    h_bot = 0.92  # From section shape
    effective_h = peak_r * h_bot
    a_ceil = math.asin(max(-1, min(1, LOWER_CEIL_Z / max(1, effective_h))))
    a_start = a_ceil
    a_end = -(math.pi + a_ceil)

    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        local_r = interior_radius_at(x / SPINE_LENGTH)
        for ia in range(arc_segs + 1):
            t = ia / arc_segs
            angle = a_start + (a_end - a_start) * t
            y, z = section_point_surcouf(local_r, angle)
            z = min(z, LOWER_CEIL_Z)
            verts.append(Vector((x, y, z)))

    ring = arc_segs + 1
    for ix in range(x_segs):
        for ia in range(arc_segs):
            v0 = ix*ring+ia; v1 = ix*ring+ia+1
            v2 = (ix+1)*ring+ia+1; v3 = (ix+1)*ring+ia
            faces.append((v0, v1, v2, v3))

    # Floor
    fb = len(verts)
    fs = 10
    for ix in range(x_segs + 1):
        x = x_start + (x_end - x_start) * ix / x_segs
        lr = interior_radius_at(x / SPINE_LENGTH)
        hw = section_half_width(lr, LOWER_FLOOR_Z)
        if hw < 30: hw = 80
        for iy in range(fs + 1):
            y = -hw + 2 * hw * iy / fs
            verts.append(Vector((x, y, LOWER_FLOOR_Z)))

    fr = fs + 1
    for ix in range(x_segs):
        for iy in range(fs):
            faces.append((fb+ix*fr+iy, fb+ix*fr+iy+1,
                          fb+(ix+1)*fr+iy+1, fb+(ix+1)*fr+iy))

    return make_obj(f"SM_Interior_{name}Lower", verts, faces, 'M_Interior_L')


# ---------------------------------------------------------------------------
# BULKHEADS
# ---------------------------------------------------------------------------

def build_bulkhead(name, x_pos, z_min, z_max, door_w, door_h, door_sill_z):
    bm = bmesh.new()
    gy, gz = 24, 20
    local_r = interior_radius_at(x_pos / SPINE_LENGTH)

    grid = {}
    for iz in range(gz + 1):
        z = z_min + (z_max - z_min) * iz / gz
        hw = section_half_width(local_r, z)
        if hw < 1: continue
        for iy in range(gy + 1):
            y = -hw + 2 * hw * iy / gy
            # Verify inside section
            actual_hw = section_half_width(local_r, z)
            if abs(y) > actual_hw: continue
            # Door hole
            if (-door_w/2 < y < door_w/2 and
                    door_sill_z < z < door_sill_z + door_h):
                continue
            grid[(iy, iz)] = bm.verts.new((x_pos, y, z))

    bm.verts.ensure_lookup_table()
    for iz in range(gz):
        for iy in range(gy):
            vs = [grid.get((iy,iz)), grid.get((iy+1,iz)),
                  grid.get((iy+1,iz+1)), grid.get((iy,iz+1))]
            if all(vs):
                try: bm.faces.new(vs)
                except ValueError: pass

    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh); bm.free(); mesh.update()
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
    eng_mid = (COMP_BOUNDS[-1][0] + COMP_BOUNDS[-1][1]) / 2
    lr = interior_radius_at(eng_mid / SPINE_LENGTH)
    cx, cy, cz = eng_mid, -(lr + AIRLOCK_W/2), -90.0
    hl, hw, hh = AIRLOCK_L/2, AIRLOCK_W/2, AIRLOCK_H/2
    verts = [Vector((cx+dx*hl, cy+dy*hw, cz+dz*hh))
             for dx in (-1,1) for dy in (-1,1) for dz in (-1,1)]
    # Reorder for proper faces
    v = [
        Vector((cx-hl, cy-hw, cz-hh)),  # 0
        Vector((cx+hl, cy-hw, cz-hh)),  # 1
        Vector((cx+hl, cy+hw, cz-hh)),  # 2
        Vector((cx-hl, cy+hw, cz-hh)),  # 3
        Vector((cx-hl, cy-hw, cz+hh)),  # 4
        Vector((cx+hl, cy-hw, cz+hh)),  # 5
        Vector((cx+hl, cy+hw, cz+hh)),  # 6
        Vector((cx-hl, cy+hw, cz+hh)),  # 7
    ]
    f = [(0,1,2,3),(4,7,6,5),(0,4,5,1),(2,6,7,3),(0,3,7,4),(1,5,6,2)]
    obj = make_obj("SM_Airlock", v, f, 'M_Airlock', smooth=False)
    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = -WALL_THICKNESS; mod.offset = -1
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# SAIL / KIOSK (distinctive Surcouf feature)
# ---------------------------------------------------------------------------

def build_sail():
    """Build the conning tower / sail — distinctive Surcouf element."""
    # Sail position: 35% from bow
    sail_x = SPINE_LENGTH * SURCOUF_SAIL_POSITION
    sail_length = 280.0   # Along X
    sail_width = 180.0    # Along Y
    sail_height = 250.0   # Above hull top

    # Get hull top at sail position
    r_at_sail = hull_radius_at(SURCOUF_SAIL_POSITION)
    _, z_top = section_point_surcouf(r_at_sail, math.pi / 2)  # Top of hull
    sail_base_z = z_top

    # Simple rounded box for sail
    verts = []
    faces = []

    # Create sail as tapered box with rounded front
    nx, ny = 8, 6
    hl, hw = sail_length / 2, sail_width / 2

    for iz in range(2):  # Bottom and top
        z = sail_base_z if iz == 0 else sail_base_z + sail_height
        # Taper: top is narrower
        taper = 1.0 if iz == 0 else 0.75
        for ix in range(nx + 1):
            tx = ix / nx
            x = sail_x - hl * taper + sail_length * taper * tx
            for iy in range(ny + 1):
                ty = iy / ny
                y = -hw * taper + sail_width * taper * ty
                verts.append(Vector((x, y, z)))

    ring = (nx + 1) * (ny + 1)
    # Bottom faces
    for ix in range(nx):
        for iy in range(ny):
            v0 = ix * (ny+1) + iy
            v1 = (ix+1) * (ny+1) + iy
            v2 = (ix+1) * (ny+1) + iy+1
            v3 = ix * (ny+1) + iy+1
            # Side walls (connect bottom to top)
            if ix == 0 or ix == nx - 1 or iy == 0 or iy == ny - 1:
                # Top face
                faces.append((ring+v0, ring+v3, ring+v2, ring+v1))
            # Connect bottom to top for edge vertices
    # Side walls
    for ix in range(nx):
        # Front wall (iy=0)
        v0 = ix*(ny+1); v1 = (ix+1)*(ny+1)
        faces.append((v0, v1, ring+v1, ring+v0))
        # Back wall (iy=ny)
        v0 = ix*(ny+1)+ny; v1 = (ix+1)*(ny+1)+ny
        faces.append((v0, ring+v0, ring+v1, v1))
    for iy in range(ny):
        # Left wall (ix=0)
        v0 = iy; v1 = iy+1
        faces.append((v0, ring+v0, ring+v1, v1))
        # Right wall (ix=nx)
        v0 = nx*(ny+1)+iy; v1 = nx*(ny+1)+iy+1
        faces.append((v0, v1, ring+v1, ring+v0))
    # Top cap
    for ix in range(nx):
        for iy in range(ny):
            v0 = ring + ix*(ny+1)+iy
            v1 = ring + (ix+1)*(ny+1)+iy
            v2 = ring + (ix+1)*(ny+1)+iy+1
            v3 = ring + ix*(ny+1)+iy+1
            faces.append((v0, v3, v2, v1))

    return make_obj("SM_Sail", verts, faces, 'M_Hull', smooth=False)


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout — Surcouf-Inspired (Script 2)")
    print("=" * 60)

    clear_scene()
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = 'CENTIMETERS'

    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.clip_start = 1.0
                    space.clip_end = 500000.0

    print(f"\nSurcouf analysis:")
    print(f"  Historical: {SURCOUF_LENGTH/100:.0f}m L, {SURCOUF_BEAM/100:.1f}m B, L/B={SURCOUF_LB_RATIO:.1f}")
    print(f"  Gameplay:   {SPINE_LENGTH/100:.0f}m L, ~{OUTER_HULL_RADIUS_SIDE*2/100:.1f}m B")
    print(f"  Profile:    {len(HULL_PROFILE)} control points")
    print(f"  Section:    compound (flat bottom, wide sides, round top)")
    print(f"  Decks:      upper Z={UPPER_FLOOR_Z}, lower Z={LOWER_FLOOR_Z}")
    print(f"  Comps:      {len(COMP_NAMES)} ({', '.join(COMP_NAMES)})")

    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        rs = hull_radius_at(xs / SPINE_LENGTH)
        re = hull_radius_at(xe / SPINE_LENGTH)
        print(f"    {name:8s}: X=[{xs:7.0f}, {xe:7.0f}] R=[{rs:.0f}, {re:.0f}]")

    print("\n--- Hull ---")
    build_hull()

    print("--- Sail ---")
    build_sail()

    print("--- Deck ---")
    build_deck()

    print("--- Interiors ---")
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        build_interior_upper(name, xs, xe)
        build_interior_lower(name, xs, xe)

    print("--- Bulkheads ---")
    for i, bx in enumerate(BULKHEAD_X):
        build_bulkhead(f"SM_Bulkhead_Upper_{i}", bx,
                       UPPER_FLOOR_Z, UPPER_CEIL_Z,
                       DOOR_UPPER_W, DOOR_UPPER_H, UPPER_FLOOR_Z)
        build_bulkhead(f"SM_Bulkhead_Lower_{i}", bx,
                       LOWER_FLOOR_Z, LOWER_CEIL_Z,
                       DOOR_LOWER_W, DOOR_LOWER_H, LOWER_FLOOR_Z)

    print("--- Airlock ---")
    build_airlock()

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


if __name__ == "__main__":
    main()
