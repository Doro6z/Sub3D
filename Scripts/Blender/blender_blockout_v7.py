"""
Sub3D — Blockout Submarine v7: Dual-Hull with Superstructure
=============================================================
ONE hull, thick (15cm solidify). No interior wall meshes.
The player sees the inside of the hull shell directly.
Hull + deck plates + bulkheads. That's it.

All in centimeters. X=forward, Y=lateral, Z=up.
"""

import bpy
import bmesh
import math
from mathutils import Vector


# ═══════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════

LENGTH = 4400.0
HULL_THICK = 15.0     # Single hull, 15cm thick via solidify
DECK_THICK = 18.0
PROFILE_SEGS = 28
HULL_RINGS = 120

# Deck Z positions
MAIN_FLOOR = -20.0
MAIN_CEIL = 140.0
LOWER_FLOOR = -220.0
LOWER_CEIL = MAIN_FLOOR - DECK_THICK
UPPER_FLOOR = MAIN_CEIL + DECK_THICK
UPPER_CEIL = 390.0

# Doors
MAIN_DOOR = (90, 155)
LOWER_DOOR = (80, 145)
UPPER_DOOR = (75, 140)

# Colors
COLORS = {
    'hull': (0.15, 0.19, 0.17),
    'deck': (0.33, 0.31, 0.27),
    'bulk': (0.43, 0.41, 0.37),
}


# ═══════════════════════════════════════════════════════════════
# CROSS-SECTION PROFILES
# ═══════════════════════════════════════════════════════════════
# ALL profiles output EXACTLY N_PTS points with semantic alignment:
#   Points 0..KEEL_END         = keel to equator (bottom half of hull)
#   Points KEEL_END..HULL_TOP  = equator to hull top (upper half of hull)
#   Points HULL_TOP..N_PTS     = superstructure (hull top to super top)
# When there's no superstructure, super points collapse to hull top.
# This ensures interpolation between ANY two profiles is safe.

N_PTS = 30              # Total points per half-profile (starboard)
KEEL_END = 10           # Points for bottom half (keel → equator)
HULL_TOP = 20           # Points for upper half (equator → hull top)
# HULL_TOP..N_PTS = 10 points for superstructure


def make_profile(hull_r, super_w=0, super_h=0):
    """
    Unified profile generator. Always returns exactly N_PTS points.
    hull_r: main hull radius (0 for tip)
    super_w: superstructure width (0 = no superstructure)
    super_h: superstructure height above hull top (0 = none)
    """
    if hull_r <= 0:
        return [(0, 0)] * N_PTS

    pts = []

    # --- Section 1: Keel to equator (KEEL_END points) ---
    for i in range(KEEL_END):
        t = i / (KEEL_END - 1)
        a = -math.pi/2 + (math.pi/2) * t  # -90° to 0°
        y = hull_r * math.cos(a)
        z = hull_r * math.sin(a)
        # Slight teardrop: wider at bottom
        if z < 0:
            y *= 1.0 + 0.04 * abs(math.sin(a))
        pts.append((y, z))

    # --- Section 2: Equator to hull top (HULL_TOP - KEEL_END points) ---
    upper_count = HULL_TOP - KEEL_END
    for i in range(upper_count):
        t = (i + 1) / upper_count
        a = (math.pi/2) * t  # 0° to 90°
        hull_y = hull_r * math.cos(a)
        hull_z = hull_r * math.sin(a)

        # If superstructure exists, blend width toward super_w at top
        if super_w > 0 and t > 0.5:
            bt = (t - 0.5) / 0.5
            bt = bt * bt * (3 - 2 * bt)  # smoothstep
            y = hull_y * (1 - bt) + (super_w / 2) * bt
        else:
            y = hull_y

        pts.append((y, hull_z))

    # --- Section 3: Superstructure (N_PTS - HULL_TOP points) ---
    super_count = N_PTS - HULL_TOP
    hull_top_z = hull_r

    # Get the last hull point (where the upper hull section ended)
    last_y = pts[-1][0] if pts else 0

    for i in range(super_count):
        t = (i + 1) / super_count

        if super_h <= 0 or super_w <= 0:
            # No superstructure: smooth cap closing from last hull point to centerline
            # Arc over the top to (0, hull_top_z) — avoids degenerate collapsed points
            y = last_y * (1.0 - t)
            z = hull_top_z + last_y * 0.08 * math.sin(t * math.pi)  # Tiny dome
            pts.append((max(0, y), z))
        else:
            if t < 0.7:
                taper = 1.0 - 0.1 * t
                y = (super_w / 2) * taper
                z = hull_top_z + super_h * t
            else:
                cap_t = (t - 0.7) / 0.3
                a = cap_t * math.pi / 2
                y = (super_w / 2) * 0.9 * math.cos(a)
                z = hull_top_z + super_h * 0.7 + super_h * 0.3 * math.sin(a)
            pts.append((y, z))

    return pts


# Station definitions: (norm_x, hull_radius, super_width, super_height)
STATION_DEFS = [
    (0.000,   0,   0,   0),     # Bow tip
    (0.025,  50,   0,   0),
    (0.055, 130,   0,   0),
    (0.090, 230,   0,   0),
    (0.130, 310,   0,   0),
    (0.170, 370,   0,   0),
    # Superstructure emerges
    (0.210, 390, 100,  40),
    (0.250, 400, 140, 100),
    (0.300, 405, 180, 180),
    # Full superstructure
    (0.350, 405, 210, 250),
    (0.420, 405, 220, 270),
    (0.500, 405, 220, 270),
    (0.560, 400, 210, 250),
    # Superstructure fading
    (0.620, 395, 180, 160),
    (0.670, 385, 140,  80),
    (0.710, 375, 100,  30),
    # Back to simple hull
    (0.750, 360,   0,   0),
    (0.800, 330,   0,   0),
    (0.850, 280,   0,   0),
    (0.900, 210,   0,   0),
    (0.940, 140,   0,   0),
    (0.970,  70,   0,   0),
    (1.000,   0,   0,   0),     # Stern tip
]

# Pre-generate all station profiles
STATIONS = [(x, make_profile(r, sw, sh)) for x, r, sw, sh in STATION_DEFS]

# Compartments
COMPARTMENTS = [
    {"name": "Torpedo",    "x": (0.10, 0.19), "has_upper": False, "has_lower": False},
    {"name": "Sonar",      "x": (0.19, 0.28), "has_upper": False, "has_lower": True},
    {"name": "Navigation", "x": (0.28, 0.38), "has_upper": True,  "has_lower": True},
    {"name": "Command",    "x": (0.38, 0.50), "has_upper": True,  "has_lower": True},
    {"name": "Crew",       "x": (0.50, 0.62), "has_upper": True,  "has_lower": True},
    {"name": "Reactor",    "x": (0.62, 0.73), "has_upper": False, "has_lower": True},
    {"name": "Engine",     "x": (0.73, 0.85), "has_upper": False, "has_lower": True},
    {"name": "Propulsion", "x": (0.85, 0.93), "has_upper": False, "has_lower": False},
]


# ═══════════════════════════════════════════════════════════════
# INTERPOLATION
# ═══════════════════════════════════════════════════════════════

def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def interpolate_profile(x_norm):
    """Interpolate between station profiles. All profiles have exactly N_PTS points."""
    x_norm = max(0.0, min(1.0, x_norm))
    for i in range(len(STATIONS) - 1):
        x0, p0 = STATIONS[i]
        x1, p1 = STATIONS[i + 1]
        if x0 <= x_norm <= x1:
            t = smoothstep((x_norm - x0) / max(1e-6, x1 - x0))
            # Direct 1:1 interpolation — same point count, same semantics
            result = []
            for j in range(N_PTS):
                y0, z0 = p0[j]
                y1, z1 = p1[j]
                result.append((y0 + (y1 - y0) * t, z0 + (z1 - z0) * t))
            return result
    return STATIONS[-1][1]


def profile_half_width(x_norm, z_query):
    profile = interpolate_profile(x_norm)
    best = 0
    for y, z in profile:
        if abs(z - z_query) < 25:
            best = max(best, abs(y))
    return best


# ═══════════════════════════════════════════════════════════════
# BLENDER UTILITIES
# ═══════════════════════════════════════════════════════════════

def clear_all():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)


def get_mat(name):
    m = bpy.data.materials.get(name)
    if not m:
        m = bpy.data.materials.new(name=name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            c = COLORS.get(name, (0.5, 0.5, 0.5))
            bsdf.inputs["Base Color"].default_value = (*c, 1)
            bsdf.inputs["Roughness"].default_value = 0.7
            bsdf.inputs["Metallic"].default_value = 0.55
    return m


def make_obj(name, verts, faces, color_key, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(get_mat(color_key))
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    if smooth:
        for p in ob.data.polygons:
            p.use_smooth = True
    ob.select_set(False)
    return ob


# ═══════════════════════════════════════════════════════════════
# HULL — One shell, solidified to 15cm thick
# ═══════════════════════════════════════════════════════════════

def build_hull():
    verts, faces = [], []

    all_rings = []
    for i in range(HULL_RINGS + 1):
        nx = i / HULL_RINGS
        half = interpolate_profile(nx)

        # Half profile is always N_PTS points (starboard, keel to super top)
        # Mirror for port side: reverse, negate Y, skip first and last to avoid duplicates
        full_ring = list(half)
        for y, z in reversed(half[1:-1]):
            full_ring.append((-y, z))

        all_rings.append(full_ring)

    ring_size = len(all_rings[0])

    for i, ring in enumerate(all_rings):
        x = (i / HULL_RINGS) * LENGTH
        for y, z in ring:
            verts.append(Vector((x, y, z)))

    for i in range(HULL_RINGS):
        for j in range(ring_size):
            jn = (j + 1) % ring_size
            a = i * ring_size + j
            b = i * ring_size + jn
            c = (i + 1) * ring_size + jn
            d = (i + 1) * ring_size + j
            faces.append((a, b, c, d))

    ob = make_obj("SM_Hull", verts, faces, 'hull')

    # Solidify: 15cm thick, inward (simple offset, no even thickness)
    mod = ob.modifiers.new("Thickness", 'SOLIDIFY')
    mod.thickness = -HULL_THICK
    mod.offset = -1  # Grow inward
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)

    return ob


# ═══════════════════════════════════════════════════════════════
# DECK PLATES — Continuous, no holes
# ═══════════════════════════════════════════════════════════════

def build_deck(name, floor_z, x_start, x_end):
    bm = bmesh.new()
    nx = max(12, int((x_end - x_start) / 50))
    ny = 20

    grid = {}
    for ix in range(nx + 1):
        x = x_start + (x_end - x_start) * ix / nx
        xn = x / LENGTH
        hw = profile_half_width(xn, floor_z) - HULL_THICK - 5  # 5cm clearance
        if hw < 15:
            continue
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

    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free(); me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(get_mat('deck'))
    mod = ob.modifiers.new("S", 'SOLIDIFY')
    mod.thickness = -DECK_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob


def build_all_decks():
    objs = []
    x_min = min(c["x"][0] for c in COMPARTMENTS) * LENGTH
    x_max = max(c["x"][1] for c in COMPARTMENTS) * LENGTH
    objs.append(build_deck("SM_Deck_Main", MAIN_FLOOR, x_min, x_max))

    # Upper deck spans
    spans, cur = [], None
    for c in COMPARTMENTS:
        xs, xe = c["x"][0] * LENGTH, c["x"][1] * LENGTH
        if c["has_upper"]:
            if cur is None: cur = [xs, xe]
            else: cur[1] = xe
        else:
            if cur: spans.append(cur); cur = None
    if cur: spans.append(cur)
    for i, (s, e) in enumerate(spans):
        objs.append(build_deck("SM_Deck_Upper_%d" % i, UPPER_FLOOR, s, e))

    # Lower deck spans
    spans, cur = [], None
    for c in COMPARTMENTS:
        xs, xe = c["x"][0] * LENGTH, c["x"][1] * LENGTH
        if c["has_lower"]:
            if cur is None: cur = [xs, xe]
            else: cur[1] = xe
        else:
            if cur: spans.append(cur); cur = None
    if cur: spans.append(cur)
    for i, (s, e) in enumerate(spans):
        objs.append(build_deck("SM_Deck_Lower_%d" % i, LOWER_FLOOR, s, e))

    return objs


# ═══════════════════════════════════════════════════════════════
# BULKHEADS — Fill cross-section with door cutout
# ═══════════════════════════════════════════════════════════════

def build_bulkhead(idx, x_pos, z_min, z_max, door_w, door_h, door_sill):
    name = "SM_BH_%d" % idx
    bm = bmesh.new()
    gy, gz = 22, 18

    profile = interpolate_profile(x_pos / LENGTH)

    grid = {}
    for iz in range(gz + 1):
        z = z_min + (z_max - z_min) * iz / gz
        hw = profile_half_width(x_pos / LENGTH, z) - HULL_THICK - 5
        if hw < 1: continue
        for iy in range(gy + 1):
            y = -hw + 2 * hw * iy / gy
            if -door_w/2 < y < door_w/2 and door_sill < z < door_sill + door_h:
                continue
            grid[(iy, iz)] = bm.verts.new((x_pos, y, z))

    bm.verts.ensure_lookup_table()
    for iz in range(gz):
        for iy in range(gy):
            vs = [grid.get(k) for k in [(iy,iz),(iy+1,iz),(iy+1,iz+1),(iy,iz+1)]]
            if all(vs):
                try: bm.faces.new(vs)
                except: pass

    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free(); me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(get_mat('bulk'))
    mod = ob.modifiers.new("S", 'SOLIDIFY')
    mod.thickness = HULL_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob


# ═══════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout v7")
    print("One hull (15cm thick) + decks + bulkheads")
    print("No interior wall meshes")
    print("=" * 60)

    clear_all()
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 1
                    sp.clip_end = 500000

    print(f"\nHull: {LENGTH/100:.0f}m, thickness {HULL_THICK}cm")
    print(f"Stations: {len(STATIONS)} profiles")
    print(f"Decks: Main Z={MAIN_FLOOR}, Upper Z={UPPER_FLOOR}, Lower Z={LOWER_FLOOR}")

    for c in COMPARTMENTS:
        flags = "main"
        if c["has_upper"]: flags += "+upper"
        if c["has_lower"]: flags += "+lower"
        print(f"  {c['name']:12s} [{c['x'][0]*LENGTH:6.0f},{c['x'][1]*LENGTH:6.0f}] {flags}")

    print("\n--- Hull (single shell, 15cm solidify) ---")
    build_hull()

    print("--- Decks (continuous, no holes) ---")
    build_all_decks()

    print("--- Bulkheads ---")
    for i in range(len(COMPARTMENTS) - 1):
        bx = COMPARTMENTS[i]["x"][1] * LENGTH
        cl, cr = COMPARTMENTS[i], COMPARTMENTS[i + 1]

        # Main deck bulkhead
        build_bulkhead(i * 3, bx, MAIN_FLOOR, MAIN_CEIL, *MAIN_DOOR, MAIN_FLOOR)

        # Upper
        if cl["has_upper"] or cr["has_upper"]:
            build_bulkhead(i * 3 + 1, bx, UPPER_FLOOR, UPPER_CEIL, *UPPER_DOOR, UPPER_FLOOR)

        # Lower
        if cl["has_lower"] or cr["has_lower"]:
            build_bulkhead(i * 3 + 2, bx, LOWER_FLOOR, LOWER_CEIL, *LOWER_DOOR, LOWER_FLOOR)

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    tv, tf = 0, 0
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type == 'MESH':
            v, f = len(ob.data.vertices), len(ob.data.polygons)
            tv += v; tf += f
            print(f"  {ob.name}: {v}V {f}F")
    print(f"  TOTAL: {tv}V {tf}F")
    print("=" * 60)


if __name__ == "__main__":
    main()
