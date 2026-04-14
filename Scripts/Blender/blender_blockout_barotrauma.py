"""
Sub3D — Blockout Submarine: Barotrauma-Inspired (Script 6)
===========================================================
3 decks au centre, 2 aux extrémités, 1 à la proue.
Cloisons irrégulières. Sail comme repère central.
Deck floor continu SANS trous de trappes (les hatches seront
des actors gameplay, pas des trous dans le mesh).

Design:
  Proue:    1 deck  — torpedo, étroit, claustro
  Avant:    2 decks — sonar/électronique + stockage
  Centre:   3 decks — command (upper), ops (main), machines (lower)
  Arrière:  2 decks — crew + réacteur
  Poupe:    2→1 deck — engine, se comprime
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ---------------------------------------------------------------------------
# CONFIGURATION
# ---------------------------------------------------------------------------

SPINE_LENGTH = 4400.0  # 44m
WALL_THICK = 14.0
DECK_THICK = 18.0
RADIAL_SEGS = 40
HULL_RINGS = 120

# Hull radius curve — organic, Barotrauma-inspired
# (norm_x, radius) with smooth transitions
HULL_CURVE = [
    (0.000,   0),
    (0.020,  60),
    (0.050, 140),
    (0.085, 230),
    (0.120, 300),
    (0.160, 350),
    (0.200, 380),
    (0.250, 395),
    (0.300, 400),    # Approaching max
    (0.380, 405),    # Sail zone — peak
    (0.500, 405),    # Mid — sustained peak
    (0.620, 400),
    (0.700, 390),
    (0.760, 370),
    (0.820, 330),
    (0.870, 270),
    (0.910, 200),
    (0.945, 130),
    (0.975,  60),
    (1.000,   0),
]

# Section shape: slightly wider than tall, mild flat bottom
W_RATIO = 1.06
H_TOP = 1.0
H_BOT = 0.93  # Flatter bottom
SEC_EXP = 2.15  # Slightly squared

# ---------------------------------------------------------------------------
# DECK LAYOUT — The key design: variable deck count
# ---------------------------------------------------------------------------
# Three possible deck levels:
#   Upper command:  Z = +140 to +310   (170cm standing)
#   Main:           Z = -30  to +120   (150cm — tighter main corridor)
#   Lower:          Z = -230 to -48    (182cm standing)
#
# Not every zone has all 3. This creates vertical rhythm.

DECK_UPPER_FLOOR = 140.0
DECK_UPPER_CEIL  = 310.0
DECK_MAIN_FLOOR  = -30.0
DECK_MAIN_CEIL   = 120.0     # = DECK_UPPER_FLOOR - DECK_THICK
DECK_LOWER_FLOOR = -230.0
DECK_LOWER_CEIL  = -48.0     # = DECK_MAIN_FLOOR - DECK_THICK

# Compartments with irregular spacing and variable deck presence
# has_upper, has_main, has_lower
COMPARTMENTS = [
    {"name": "Torpedo",    "x": (0.08, 0.17), "decks": (False, True,  False), "door_w": 70,  "door_h": 160},
    {"name": "Sonar",      "x": (0.17, 0.27), "decks": (False, True,  True),  "door_w": 80,  "door_h": 175},
    {"name": "Navigation", "x": (0.27, 0.36), "decks": (True,  True,  True),  "door_w": 90,  "door_h": 185},
    {"name": "Command",    "x": (0.36, 0.48), "decks": (True,  True,  True),  "door_w": 95,  "door_h": 190},
    {"name": "Crew",       "x": (0.48, 0.60), "decks": (True,  True,  True),  "door_w": 90,  "door_h": 185},
    {"name": "Reactor",    "x": (0.60, 0.72), "decks": (False, True,  True),  "door_w": 100, "door_h": 190},
    {"name": "Engine",     "x": (0.72, 0.84), "decks": (False, True,  True),  "door_w": 85,  "door_h": 175},
    {"name": "Propulsion", "x": (0.84, 0.93), "decks": (False, True,  False), "door_w": 75,  "door_h": 165},
]

for c in COMPARTMENTS:
    s, e = c["x"]
    c["x_start"] = s * SPINE_LENGTH
    c["x_end"] = e * SPINE_LENGTH

# Upper Structure / Kiosque-Sas
# Sits ON TOP of the hull. Mix between a conning tower and a full upper deck.
# Contains: walkable interior, ladder access down into hull, exterior exit (sas door).
# Like the Barotrauma upper structure — a second hull level on top.
#
# Side view:
#          ┌─────────────────┐
#          │  upper structure │  ← walkable interior, sas function
#          │   ┌──ladder──┐  │
#   ───────┘   └──────────┘  └───────── hull top
#   ═══════════════════════════════════ main hull
#
UPPER_STRUCT_X_NORM = (0.30, 0.52)   # Spans Navigation + Command compartments
UPPER_STRUCT_WIDTH = 220.0           # Interior width (Y)
UPPER_STRUCT_HEIGHT = 250.0          # Interior standing height
UPPER_STRUCT_WALL = 12.0             # Wall thickness
LADDER_SIZE = 90.0                   # Ladder platform (square)
# The structure sits on the hull top and rises above it

# Colors
COLORS = {
    'hull':   (0.16, 0.20, 0.18),  # Dark military green
    'upper':  (0.38, 0.36, 0.32),
    'main':   (0.42, 0.40, 0.36),
    'lower':  (0.28, 0.28, 0.26),
    'deck_u': (0.34, 0.32, 0.28),
    'deck_m': (0.36, 0.34, 0.30),
    'bulk':   (0.44, 0.42, 0.38),
    'sail':   (0.20, 0.24, 0.22),
    'airlock': (0.30, 0.28, 0.25),
}

# ---------------------------------------------------------------------------
# MATH
# ---------------------------------------------------------------------------

def spow(base, exp):
    if abs(base) < 1e-6: return 0.0
    return math.copysign(1, base) * abs(base) ** exp

def smoothstep(t):
    t = max(0, min(1, t))
    return t * t * (3 - 2 * t)

def sample_curve(curve, nx):
    nx = max(0, min(1, nx))
    for i in range(len(curve) - 1):
        x0, r0 = curve[i]
        x1, r1 = curve[i + 1]
        if x0 <= nx <= x1:
            t = smoothstep((nx - x0) / max(1e-6, x1 - x0))
            return r0 + (r1 - r0) * t
    return curve[-1][1]

def hull_r(nx):
    return sample_curve(HULL_CURVE, nx)

def interior_r(nx):
    return max(1, hull_r(nx) - WALL_THICK)

def sec_point(r, angle):
    """Cross-section point with flat bottom + width ratio."""
    exp = 2.0 / SEC_EXP
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    h_r = H_TOP if sin_a >= 0 else H_BOT
    y = r * W_RATIO * spow(cos_a, exp)
    z = r * h_r * spow(sin_a, exp)
    return y, z

def sec_hw(r, z):
    """Half-width at given Z."""
    if r <= 0: return 0
    h_r = H_TOP if z >= 0 else H_BOT
    eff_h = r * h_r
    if abs(z) >= eff_h: return 0
    n = SEC_EXP
    ratio = abs(z) / eff_h
    return r * W_RATIO * max(0, 1 - ratio ** n) ** (1 / n)


# ---------------------------------------------------------------------------
# BLENDER
# ---------------------------------------------------------------------------

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)

def get_mat(name, key):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            c = COLORS.get(key, (0.5, 0.5, 0.5))
            bsdf.inputs["Base Color"].default_value = (*c, 1)
            bsdf.inputs["Roughness"].default_value = 0.72
            bsdf.inputs["Metallic"].default_value = 0.55
    return mat

def finalize(obj, mat_key, smooth=True):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    if smooth:
        for p in obj.data.polygons: p.use_smooth = True
    obj.select_set(False)
    return obj

def mesh_obj(name, verts, faces, mat_key, smooth=True):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(name, mat_key))
    return finalize(obj, mat_key, smooth)


# ---------------------------------------------------------------------------
# HULL EXTERIOR
# ---------------------------------------------------------------------------

def build_hull():
    verts, faces = [], []
    for i in range(HULL_RINGS + 1):
        nx = i / HULL_RINGS
        x = nx * SPINE_LENGTH
        r = hull_r(nx)
        if r < 0.3: r = 0.3
        for seg in range(RADIAL_SEGS):
            a = 2 * math.pi * seg / RADIAL_SEGS
            y, z = sec_point(r, a)
            verts.append(Vector((x, y, z)))
    for i in range(HULL_RINGS):
        for s in range(RADIAL_SEGS):
            ns = (s + 1) % RADIAL_SEGS
            a = i * RADIAL_SEGS + s
            b = i * RADIAL_SEGS + ns
            c = (i + 1) * RADIAL_SEGS + ns
            d = (i + 1) * RADIAL_SEGS + s
            faces.append((a, b, c, d))
    # Caps
    bc = len(verts); verts.append(Vector((0, 0, 0)))
    for s in range(RADIAL_SEGS):
        faces.append((bc, (s + 1) % RADIAL_SEGS, s))
    sc = len(verts); verts.append(Vector((SPINE_LENGTH, 0, 0)))
    lb = HULL_RINGS * RADIAL_SEGS
    for s in range(RADIAL_SEGS):
        faces.append((sc, lb + s, lb + (s + 1) % RADIAL_SEGS))
    return mesh_obj("SM_Hull", verts, faces, 'hull')


# ---------------------------------------------------------------------------
# SAIL / KIOSK
# ---------------------------------------------------------------------------

def build_sail():
    sail_x = SAIL_X_NORM * SPINE_LENGTH
    r_at = hull_r(SAIL_X_NORM)
    _, z_top = sec_point(r_at, math.pi / 2)
    base_z = z_top

    # Tapered sail: wider at base, narrower at top, rounded front
    hl = SAIL_LENGTH / 2
    hw = SAIL_WIDTH / 2

    verts, faces = [], []
    nx, nz = 6, 4

    for iz in range(nz + 1):
        tz = iz / nz
        z = base_z + SAIL_HEIGHT * tz
        # Taper: top is 65% of base width, 70% of base length
        taper_w = 1.0 - 0.35 * tz
        taper_l = 1.0 - 0.30 * tz
        # Round front by using ellipse
        for ix in range(nx + 1):
            tx = ix / nx
            local_hl = hl * taper_l
            local_hw = hw * taper_w
            x = sail_x - local_hl + 2 * local_hl * tx
            # Y follows an ellipse for rounded shape
            # At edges (tx=0 or 1), Y=0. At center, Y=max
            y_frac = math.sin(tx * math.pi)
            y = local_hw * y_frac
            verts.append(Vector((x, y, z)))
            verts.append(Vector((x, -y, z)))

    ring = (nx + 1) * 2
    for iz in range(nz):
        for ix in range(nx):
            for side in range(2):
                base = iz * ring + ix * 2 + side
                # Connect to next ring
                a = base
                b = base + 2  # Next x, same z
                c = base + 2 + ring  # Next x, next z
                d = base + ring  # Same x, next z
                if side == 0:
                    faces.append((a, b, c, d))
                else:
                    faces.append((a, d, c, b))

    # Simplified: just make a box-like sail
    verts2, faces2 = [], []
    for iz in range(nz + 1):
        tz = iz / nz
        z = base_z + SAIL_HEIGHT * tz
        tl = 1.0 - 0.30 * tz
        tw = 1.0 - 0.35 * tz
        lhl, lhw = hl * tl, hw * tw
        # 4 corners at this height
        verts2.append(Vector((sail_x - lhl, -lhw, z)))  # 0: back-port
        verts2.append(Vector((sail_x + lhl, -lhw, z)))  # 1: front-port
        verts2.append(Vector((sail_x + lhl,  lhw, z)))  # 2: front-stbd
        verts2.append(Vector((sail_x - lhl,  lhw, z)))  # 3: back-stbd

    for iz in range(nz):
        b = iz * 4
        n = b + 4
        # 4 side faces
        faces2.append((b+0, b+1, n+1, n+0))  # Port
        faces2.append((b+1, b+2, n+2, n+1))  # Front
        faces2.append((b+2, b+3, n+3, n+2))  # Starboard
        faces2.append((b+3, b+0, n+0, n+3))  # Back
    # Top cap
    t = nz * 4
    faces2.append((t+0, t+3, t+2, t+1))

    return mesh_obj("SM_Sail", verts2, faces2, 'sail', smooth=False)


# ---------------------------------------------------------------------------
# DECK FLOORS — Continuous plates, NO hatch holes
# ---------------------------------------------------------------------------

def build_deck_plate(name, floor_z, x_start, x_end, mat_key):
    """Solid deck plate. No holes."""
    bm = bmesh.new()
    nx, ny = max(12, int((x_end - x_start) / 60)), 20

    grid = {}
    for ix in range(nx + 1):
        x = x_start + (x_end - x_start) * ix / nx
        r = interior_r(x / SPINE_LENGTH)
        hw = sec_hw(r, floor_z)
        if hw < 15: continue
        for iy in range(ny + 1):
            y = -hw + 2 * hw * iy / ny
            grid[(ix, iy)] = bm.verts.new((x, y, floor_z))

    bm.verts.ensure_lookup_table()
    for ix in range(nx):
        for iy in range(ny):
            vs = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
            if all(vs):
                try: bm.faces.new(vs)
                except: pass

    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(name, mat_key))

    mod = obj.modifiers.new("Sol", 'SOLIDIFY')
    mod.thickness = -DECK_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def build_all_decks():
    """Build deck plates only where needed — continuous spans between compartments that share a deck."""

    # Upper deck: only where has_upper is True
    upper_spans = []
    current_span = None
    for c in COMPARTMENTS:
        if c["decks"][0]:  # has_upper
            if current_span is None:
                current_span = [c["x_start"], c["x_end"]]
            else:
                current_span[1] = c["x_end"]
        else:
            if current_span:
                upper_spans.append(tuple(current_span))
                current_span = None
    if current_span: upper_spans.append(tuple(current_span))

    # Main deck floor: where both main exists (almost everywhere)
    main_spans = []
    current_span = None
    for c in COMPARTMENTS:
        if c["decks"][1]:  # has_main (always true here)
            if current_span is None:
                current_span = [c["x_start"], c["x_end"]]
            else:
                current_span[1] = c["x_end"]
        else:
            if current_span:
                main_spans.append(tuple(current_span))
                current_span = None
    if current_span: main_spans.append(tuple(current_span))

    objs = []
    for i, (xs, xe) in enumerate(upper_spans):
        objs.append(build_deck_plate(f"SM_DeckUpper_{i}", DECK_UPPER_FLOOR, xs, xe, 'deck_u'))

    for i, (xs, xe) in enumerate(main_spans):
        objs.append(build_deck_plate(f"SM_DeckMain_{i}", DECK_MAIN_FLOOR, xs, xe, 'deck_m'))

    return objs


# ---------------------------------------------------------------------------
# INTERIOR WALLS — Per-compartment arc sections
# ---------------------------------------------------------------------------

def build_walls(comp, deck_name, floor_z, ceil_z, mat_key):
    """Interior walls following hull curvature for one deck of one compartment."""
    name = f"SM_{comp['name']}_{deck_name}"
    xs, xe = comp["x_start"], comp["x_end"]
    verts, faces = [], []
    arc_segs = 22
    x_segs = max(4, int((xe - xs) / 70))

    is_lower = (floor_z < -100)

    for ix in range(x_segs + 1):
        x = xs + (xe - xs) * ix / x_segs
        r = interior_r(x / SPINE_LENGTH)
        if r < 5: r = 5

        if is_lower:
            # Lower arc: from ceil_z starboard through bottom to ceil_z port
            a_c = math.asin(max(-1, min(1, ceil_z / (r * H_BOT)))) if r * H_BOT > abs(ceil_z) else -math.pi/2
            a_start = a_c
            a_end = -(math.pi + a_c)
        elif floor_z > 100:
            # Upper: from floor_z starboard through top to floor_z port
            a_f = math.asin(max(-1, min(1, floor_z / (r * H_TOP)))) if r * H_TOP > abs(floor_z) else 0
            a_start = a_f
            a_end = math.pi - a_f
        else:
            # Main deck: walls between main floor and main ceil
            a_start = 0
            a_end = math.pi

        for ia in range(arc_segs + 1):
            t = ia / arc_segs
            angle = a_start + (a_end - a_start) * t
            y, z = sec_point(r, angle)
            z = max(floor_z, min(ceil_z, z))
            verts.append(Vector((x, y, z)))

    ring = arc_segs + 1
    for ix in range(x_segs):
        for ia in range(arc_segs):
            v0 = ix * ring + ia
            v1 = ix * ring + ia + 1
            v2 = (ix+1) * ring + ia + 1
            v3 = (ix+1) * ring + ia
            if is_lower:
                faces.append((v0, v1, v2, v3))
            else:
                faces.append((v0, v3, v2, v1))

    # Floor for lower deck
    if is_lower:
        fb = len(verts)
        fs = 8
        for ix in range(x_segs + 1):
            x = xs + (xe - xs) * ix / x_segs
            r = interior_r(x / SPINE_LENGTH)
            hw = sec_hw(r, floor_z)
            hw = max(hw, 40)
            for iy in range(fs + 1):
                y = -hw + 2 * hw * iy / fs
                verts.append(Vector((x, y, floor_z)))
        fr = fs + 1
        for ix in range(x_segs):
            for iy in range(fs):
                faces.append((fb+ix*fr+iy, fb+ix*fr+iy+1,
                              fb+(ix+1)*fr+iy+1, fb+(ix+1)*fr+iy))

    # Ceiling for upper deck
    if floor_z > 100:
        cb = len(verts)
        cs = 8
        for ix in range(x_segs + 1):
            x = xs + (xe - xs) * ix / x_segs
            r = interior_r(x / SPINE_LENGTH)
            hw = sec_hw(r, ceil_z)
            hw = max(hw, 20)
            for iy in range(cs + 1):
                y = -hw + 2 * hw * iy / cs
                verts.append(Vector((x, y, ceil_z)))
        cr = cs + 1
        for ix in range(x_segs):
            for iy in range(cs):
                faces.append((cb+ix*cr+iy, cb+ix*cr+iy+1,
                              cb+(ix+1)*cr+iy+1, cb+(ix+1)*cr+iy))

    return mesh_obj(name, verts, faces, mat_key)


# ---------------------------------------------------------------------------
# BULKHEADS
# ---------------------------------------------------------------------------

def build_bulkhead(idx, x_pos, comp_left, comp_right):
    """Bulkhead spanning the full height where both compartments have decks."""
    name = f"SM_BH_{idx}"
    bm = bmesh.new()

    # Determine Z range: union of all decks present on either side
    z_min = DECK_LOWER_FLOOR
    z_max = DECK_UPPER_CEIL

    # But only extend to decks that exist on at least one side
    has_upper = comp_left["decks"][0] or comp_right["decks"][0]
    has_lower = comp_left["decks"][2] or comp_right["decks"][2]

    if not has_upper:
        z_max = DECK_MAIN_CEIL
    if not has_lower:
        z_min = DECK_MAIN_FLOOR

    r = interior_r(x_pos / SPINE_LENGTH)
    gy, gz = 24, 22

    # Door for main deck (always present)
    dw = min(comp_left["door_w"], comp_right["door_w"])
    dh = min(comp_left["door_h"], comp_right["door_h"])
    dsill = DECK_MAIN_FLOOR

    # Lower door (if both sides have lower)
    has_lower_door = comp_left["decks"][2] and comp_right["decks"][2]
    ldw, ldh, ldsill = 75, 155, DECK_LOWER_FLOOR

    # Upper door (if both sides have upper)
    has_upper_door = comp_left["decks"][0] and comp_right["decks"][0]
    udw, udh, udsill = 80, 165, DECK_UPPER_FLOOR

    grid = {}
    for iz in range(gz + 1):
        z = z_min + (z_max - z_min) * iz / gz
        hw = sec_hw(r, z)
        if hw < 1: continue
        for iy in range(gy + 1):
            y = -hw + 2 * hw * iy / gy

            # Main door
            if -dw/2 < y < dw/2 and dsill < z < dsill + dh:
                continue
            # Lower door
            if has_lower_door and -ldw/2 < y < ldw/2 and ldsill < z < ldsill + ldh:
                continue
            # Upper door
            if has_upper_door and -udw/2 < y < udw/2 and udsill < z < udsill + udh:
                continue

            grid[(iy, iz)] = bm.verts.new((x_pos, y, z))

    bm.verts.ensure_lookup_table()
    for iz in range(gz):
        for iy in range(gy):
            vs = [grid.get(k) for k in [(iy,iz),(iy+1,iz),(iy+1,iz+1),(iy,iz+1)]]
            if all(vs):
                try: bm.faces.new(vs)
                except: pass

    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(name, 'bulk'))
    mod = obj.modifiers.new("Sol", 'SOLIDIFY')
    mod.thickness = WALL_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# AIRLOCK / SAS — Hull-integrated, pierces the hull
# ---------------------------------------------------------------------------

def build_airlock():
    """
    The airlock PIERCES the hull. The hull wraps around it.

    Top view (port side):

        ════════ HULL INTERIOR ════════
              │              │
              │   (inner opening — passage to sub)
              │              │
        ┌─────┘              └─────┐
        │                          │
        │      ┌──────────┐        │
        │      │  échelle │        │  ← Hull blister wraps around
        │      │ (platform)│       │
        │      └──────────┘        │
        │                          │
        └──────────────────────────┘
              OPEN (sea door here)

    - Inner face: OPEN → connects to submarine interior
    - Outer face: OPEN → sea, door actor will close it
    - 2 side walls (bow + stern) + floor + ceiling
    - Hull blister mesh wraps the exterior
    """
    comp = COMPARTMENTS[AIRLOCK_ATTACH_COMP]
    comp_mid_x = (comp["x_start"] + comp["x_end"]) / 2
    r_hull = hull_r(comp_mid_x / SPINE_LENGTH)
    r_int = interior_r(comp_mid_x / SPINE_LENGTH)

    side = AIRLOCK_SIDE  # -1 = port

    # Where the hull surface is at Z=0 (port side)
    hull_surface_y = r_int * W_RATIO * side

    # Airlock extends FROM hull surface OUTWARD
    # Inner edge at hull surface, outer edge further out
    inner_y = hull_surface_y
    outer_y = hull_surface_y + AIRLOCK_OUTER_W * side

    cx = comp_mid_x
    hl = AIRLOCK_OUTER_L / 2
    fz = DECK_LOWER_FLOOR
    hh = AIRLOCK_HEIGHT
    w = AIRLOCK_WALL

    objects = []

    # ─── AIRLOCK INTERIOR WALLS ───
    # Only bow wall and stern wall (inner+outer faces are OPEN)
    bm = bmesh.new()

    # 4 corners floor
    f0 = bm.verts.new((cx - hl, inner_y, fz))          # bow, hull-side
    f1 = bm.verts.new((cx + hl, inner_y, fz))          # stern, hull-side
    f2 = bm.verts.new((cx + hl, outer_y, fz))          # stern, sea-side
    f3 = bm.verts.new((cx - hl, outer_y, fz))          # bow, sea-side

    # 4 corners ceiling
    t0 = bm.verts.new((cx - hl, inner_y, fz + hh))
    t1 = bm.verts.new((cx + hl, inner_y, fz + hh))
    t2 = bm.verts.new((cx + hl, outer_y, fz + hh))
    t3 = bm.verts.new((cx - hl, outer_y, fz + hh))

    # Floor
    bm.faces.new([f0, f1, f2, f3])
    # Ceiling
    bm.faces.new([t0, t3, t2, t1])
    # Bow wall (closed)
    bm.faces.new([f0, f3, t3, t0])
    # Stern wall (closed)
    bm.faces.new([f1, f2, t2, t1] if side < 0 else [f2, f1, t1, t2])
    # Inner face (hull-side): OPEN — passage to submarine
    # Outer face (sea-side): OPEN — door actor goes here

    # ─── LADDER PLATFORM (centered, raised 30cm) ───
    ls = LADDER_SIZE / 2
    ladder_x = cx
    ladder_y = (inner_y + outer_y) / 2  # Center of airlock
    lz = fz + 30

    lf0 = bm.verts.new((ladder_x - ls, ladder_y - ls * side, fz))
    lf1 = bm.verts.new((ladder_x + ls, ladder_y - ls * side, fz))
    lf2 = bm.verts.new((ladder_x + ls, ladder_y + ls * side, fz))
    lf3 = bm.verts.new((ladder_x - ls, ladder_y + ls * side, fz))

    lt0 = bm.verts.new((ladder_x - ls, ladder_y - ls * side, lz))
    lt1 = bm.verts.new((ladder_x + ls, ladder_y - ls * side, lz))
    lt2 = bm.verts.new((ladder_x + ls, ladder_y + ls * side, lz))
    lt3 = bm.verts.new((ladder_x - ls, ladder_y + ls * side, lz))

    # Platform top
    bm.faces.new([lt0, lt1, lt2, lt3])
    # Platform sides
    bm.faces.new([lf0, lf1, lt1, lt0])
    bm.faces.new([lf1, lf2, lt2, lt1])
    bm.faces.new([lf2, lf3, lt3, lt2])
    bm.faces.new([lf3, lf0, lt0, lt3])

    bm.verts.ensure_lookup_table()

    mesh = bpy.data.meshes.new("SM_Airlock_Interior")
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new("SM_Airlock_Interior", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat("SM_Airlock_Interior", 'airlock'))

    # Solidify for wall thickness
    mod = obj.modifiers.new("Sol", 'SOLIDIFY')
    mod.thickness = w; mod.offset = -1
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    finalize(obj, 'airlock', smooth=False)
    objects.append(obj)

    # ─── HULL BLISTER (wraps around the airlock exterior) ───
    # A smooth bump on the hull that covers the airlock
    bm2 = bmesh.new()

    blister_segs_x = 10
    blister_segs_arc = 16
    blister_margin = 40  # Extra coverage around the airlock

    bx_start = cx - hl - blister_margin
    bx_end = cx + hl + blister_margin

    # The blister is a half-cylinder that starts at the hull surface
    # and extends outward to cover the airlock
    blister_radius = AIRLOCK_OUTER_W + blister_margin
    blister_height_range = (fz - blister_margin, fz + hh + blister_margin)

    for ix in range(blister_segs_x + 1):
        t_x = ix / blister_segs_x
        x = bx_start + (bx_end - bx_start) * t_x

        # Taper blister at the ends (smooth blend into hull)
        end_taper = 1.0
        if t_x < 0.15:
            end_taper = smoothstep(t_x / 0.15)
        elif t_x > 0.85:
            end_taper = smoothstep((1 - t_x) / 0.15)

        r_local = hull_r(x / SPINE_LENGTH)
        hull_y_at_x = r_local * W_RATIO * side

        for ia in range(blister_segs_arc + 1):
            t_a = ia / blister_segs_arc
            # Arc from hull bottom to hull top of the blister zone
            angle = -math.pi/2 + math.pi * t_a  # -90° to +90°

            # Blister protrusion amount (tapered at ends)
            protrusion = blister_radius * end_taper * math.cos(angle)
            z_offset = blister_radius * 0.6 * math.sin(angle)

            y = hull_y_at_x + protrusion * side
            z = (fz + hh/2) + z_offset  # Centered on airlock

            # Clamp Z to blister height range
            z = max(blister_height_range[0], min(blister_height_range[1], z))

            bm2.verts.new((x, y, z))

    bm2.verts.ensure_lookup_table()
    ring = blister_segs_arc + 1
    for ix in range(blister_segs_x):
        for ia in range(blister_segs_arc):
            a = ix * ring + ia
            b = ix * ring + ia + 1
            c = (ix + 1) * ring + ia + 1
            d = (ix + 1) * ring + ia
            bm2.faces.new([bm2.verts[a], bm2.verts[b], bm2.verts[c], bm2.verts[d]])

    mesh2 = bpy.data.meshes.new("SM_Airlock_Blister")
    bm2.to_mesh(mesh2); bm2.free(); mesh2.update()
    obj2 = bpy.data.objects.new("SM_Airlock_Blister", mesh2)
    bpy.context.collection.objects.link(obj2)
    obj2.data.materials.append(get_mat("SM_Airlock_Blister", 'hull'))
    finalize(obj2, 'hull', smooth=True)
    objects.append(obj2)

    return objects


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout v6 — Barotrauma-Inspired")
    print("3 decks variable, no hatch holes, sail landmark")
    print("=" * 60)

    clear_scene()
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 1; sp.clip_end = 500000

    print(f"\nLayout ({SPINE_LENGTH/100:.0f}m):")
    for c in COMPARTMENTS:
        u, m, l = c["decks"]
        decks = []
        if u: decks.append("upper")
        if m: decks.append("main")
        if l: decks.append("lower")
        print(f"  {c['name']:12s} X=[{c['x_start']:6.0f},{c['x_end']:6.0f}] "
              f"decks=[{'+'.join(decks)}] door={c['door_w']}x{c['door_h']}")

    print("\n--- Hull ---")
    build_hull()

    print("--- Sail ---")
    build_sail()

    print("--- Deck Plates (continuous, no holes) ---")
    build_all_decks()

    print("--- Interior Walls ---")
    for c in COMPARTMENTS:
        has_u, has_m, has_l = c["decks"]
        if has_u:
            build_walls(c, "Upper", DECK_UPPER_FLOOR, DECK_UPPER_CEIL, 'upper')
        if has_m:
            build_walls(c, "Main", DECK_MAIN_FLOOR, DECK_MAIN_CEIL, 'main')
        if has_l:
            build_walls(c, "Lower", DECK_LOWER_FLOOR, DECK_LOWER_CEIL, 'lower')

    print("--- Bulkheads ---")
    for i in range(len(COMPARTMENTS) - 1):
        bx = COMPARTMENTS[i]["x_end"]
        build_bulkhead(i, bx, COMPARTMENTS[i], COMPARTMENTS[i + 1])

    print("--- Airlock ---")
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
    print(f"\nDeck layout:")
    print(f"  Upper  Z=[{DECK_UPPER_FLOOR:.0f}, {DECK_UPPER_CEIL:.0f}] ({DECK_UPPER_CEIL-DECK_UPPER_FLOOR:.0f}cm)")
    print(f"  Main   Z=[{DECK_MAIN_FLOOR:.0f}, {DECK_MAIN_CEIL:.0f}] ({DECK_MAIN_CEIL-DECK_MAIN_FLOOR:.0f}cm)")
    print(f"  Lower  Z=[{DECK_LOWER_FLOOR:.0f}, {DECK_LOWER_CEIL:.0f}] ({DECK_LOWER_CEIL-DECK_LOWER_FLOOR:.0f}cm)")
    print(f"\n8 compartments, variable deck count:")
    print(f"  3-deck zone: Navigation, Command, Crew")
    print(f"  2-deck zone: Sonar, Reactor, Engine")
    print(f"  1-deck zone: Torpedo, Propulsion")


if __name__ == "__main__":
    main()
