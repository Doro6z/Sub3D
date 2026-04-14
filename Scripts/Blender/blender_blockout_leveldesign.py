"""
Sub3D — Blockout Submarine: Level-Design-First (Script 5)
==========================================================
FULL AUTONOMY — Every decision optimized for player experience.

Design philosophy:
  - The submarine is a GAME LEVEL, not a vehicle model
  - Each compartment is a ROOM with gameplay purpose
  - Proportions serve navigation clarity, not naval realism
  - Sightlines, flow, and drama drive the layout

Key design decisions:
  1. VARIED room sizes — tight corridors + open spaces = rhythm
  2. NON-SYMMETRIC layout — player builds mental map from landmarks
  3. VERTICAL DRAMA — multi-level with dramatic height changes
  4. FLOODING AS LEVEL DESIGN — water paths create puzzle-like gameplay
  5. THE JOURNEY — bow-to-stern traversal tells a spatial story:
     Torpedo Room (cramped) → Operations (open, tall) → Crew (medium) →
     Reactor (massive, industrial) → Engine (narrow, long, loud)

Inspiration: Dead Space corridors, Alien: Isolation atmosphere,
             Barotrauma gameplay loops, Hunt: Showdown spatial audio
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ---------------------------------------------------------------------------
# THE SPATIAL STORY — Each room designed for gameplay feel
# ---------------------------------------------------------------------------

SPINE_LENGTH = 3800.0  # 38m — tight, every meter matters
WALL_THICK = 14.0
DECK_THICK = 20.0

# Room definitions: (name, x_start_norm, x_end_norm, design_notes)
# Each room has its own cross-section profile for varied feel
ROOMS = [
    {
        "name": "TorpedoRoom",
        "x_norm": (0.12, 0.22),
        "upper_h": 240,    # Tall — torpedo racks floor to ceiling
        "lower_h": 0,      # No lower deck — single tall space
        "width": 260,       # Narrow — cramped, claustrophobic
        "floor_z": -180,    # Low floor — player feels "in the belly"
        "mood": "cramped_tall",
    },
    {
        "name": "Operations",
        "x_norm": (0.22, 0.38),
        "upper_h": 220,    # Standard standing
        "lower_h": 180,    # Full lower deck — sonar, electronics
        "width": 380,       # Wide — the "hub" of the submarine
        "floor_z": 0,       # Normal floor — open and breathable
        "mood": "open_hub",
    },
    {
        "name": "CrewQuarters",
        "x_norm": (0.38, 0.52),
        "upper_h": 200,    # Slightly low ceiling — cozy
        "lower_h": 160,    # Lower bunks — very tight
        "width": 340,       # Medium
        "floor_z": 0,
        "mood": "cozy_medium",
    },
    {
        "name": "Reactor",
        "x_norm": (0.52, 0.68),
        "upper_h": 250,    # Tallest room — reactor vessel dominates
        "lower_h": 200,    # Big lower space — pipes, machinery
        "width": 400,       # Widest — industrial cathedral
        "floor_z": -10,     # Slightly low — heavy feel
        "mood": "industrial_cathedral",
    },
    {
        "name": "EngineRoom",
        "x_norm": (0.68, 0.84),
        "upper_h": 190,    # Low ceiling — engine mounts above
        "lower_h": 170,    # Tight — crawling between machines
        "width": 300,       # Narrower — tunneling toward stern
        "floor_z": 0,
        "mood": "narrow_long",
    },
]

# Derived
for room in ROOMS:
    s, e = room["x_norm"]
    room["x_start"] = s * SPINE_LENGTH
    room["x_end"] = e * SPINE_LENGTH
    room["length"] = room["x_end"] - room["x_start"]
    room["has_lower"] = room["lower_h"] > 0

COMP_NAMES = [r["name"] for r in ROOMS]
COMP_BOUNDS = [(r["x_start"], r["x_end"]) for r in ROOMS]
BULKHEAD_X = [COMP_BOUNDS[i][1] for i in range(len(ROOMS) - 1)]
HATCH_X = [(r["x_start"] + r["x_end"]) / 2 for r in ROOMS if r["has_lower"]]
HATCH_SIZE = 90.0

# Hull: organic envelope around the rooms
# The hull radius at each X depends on what the room needs
def room_at_x(x):
    for r in ROOMS:
        if r["x_start"] <= x <= r["x_end"]:
            return r
    return None

def hull_radius_at(norm_x):
    x = norm_x * SPINE_LENGTH
    r = room_at_x(x)
    if r:
        # Hull must contain room width/2 + wall
        total_h = r["upper_h"] + (DECK_THICK + r["lower_h"] if r["has_lower"] else 0)
        needed_r = max(r["width"] / 2 + WALL_THICK + 20,
                       total_h / 2 + WALL_THICK + 20)
        return needed_r

    # Bow/stern taper
    body_start = ROOMS[0]["x_norm"][0]
    body_end = ROOMS[-1]["x_norm"][1]

    if norm_x < body_start:
        t = norm_x / body_start
        peak_r = hull_radius_at(body_start + 0.01)
        return peak_r * math.sin(t * math.pi / 2)
    elif norm_x > body_end:
        t = (1.0 - norm_x) / (1.0 - body_end)
        peak_r = hull_radius_at(body_end - 0.01)
        return peak_r * (t ** 0.7)

    return 200  # Fallback

# Smooth the hull radius
_radius_cache = {}
def hull_radius_smooth(norm_x):
    # Sample neighbors and average for smooth transitions
    samples = 5
    total = 0
    for i in range(-samples, samples + 1):
        nx = norm_x + i * 0.005
        nx = max(0, min(1, nx))
        total += hull_radius_at(nx)
    return total / (2 * samples + 1)

def section_pt(r, angle):
    if r < 1: return 0, 0
    # Slightly squashed circle (W/H = 1.05) for submarine feel
    y = r * 1.05 * math.cos(angle)
    z = r * math.sin(angle)
    return y, z

def section_hw(r, z):
    if r <= 0: return 0
    eff_r = r  # Simplified
    if abs(z) >= eff_r: return 0
    return 1.05 * math.sqrt(max(0, eff_r**2 - z**2))

# Door dimensions vary by room
DOOR_CONFIGS = {
    "TorpedoRoom": (70, 170),   # Tight hatch
    "Operations":  (100, 190),  # Wide command door
    "CrewQuarters": (85, 180),  # Standard
    "Reactor":     (110, 200),  # Extra wide for equipment
    "EngineRoom":  (80, 175),   # Narrow
}

RADIAL_SEGS = 36

# Colors — mood-based
COLORS = {
    'hull': (0.18, 0.20, 0.24),
    'deck': (0.32, 0.30, 0.26),
    'TorpedoRoom': (0.28, 0.30, 0.32),     # Cold steel
    'Operations': (0.40, 0.38, 0.35),       # Warm command
    'CrewQuarters': (0.38, 0.36, 0.30),     # Warm wood tones
    'Reactor': (0.25, 0.28, 0.30),          # Industrial cold
    'EngineRoom': (0.30, 0.28, 0.25),       # Oily warm
    'bulk': (0.45, 0.42, 0.38),
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

def get_mat(name, color):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*color, 1)
            bsdf.inputs["Roughness"].default_value = 0.75
            bsdf.inputs["Metallic"].default_value = 0.5
    return mat

def make_obj(name, verts, faces, color_key):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    col = COLORS.get(color_key, (0.5, 0.5, 0.5))
    obj.data.materials.append(get_mat(name, col))
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    for p in obj.data.polygons: p.use_smooth = True
    obj.select_set(False)
    return obj


# ---------------------------------------------------------------------------
# HULL
# ---------------------------------------------------------------------------

def build_hull():
    verts, faces = [], []
    n_rings = 100
    for i in range(n_rings + 1):
        nx = i / n_rings
        r = hull_radius_smooth(nx)
        x = nx * SPINE_LENGTH
        if r < 0.5: r = 0.5
        for seg in range(RADIAL_SEGS):
            a = 2 * math.pi * seg / RADIAL_SEGS
            y, z = section_pt(r, a)
            verts.append(Vector((x, y, z)))
    for i in range(n_rings):
        for s in range(RADIAL_SEGS):
            ns = (s+1) % RADIAL_SEGS
            a, b = i*RADIAL_SEGS+s, i*RADIAL_SEGS+ns
            c, d = (i+1)*RADIAL_SEGS+ns, (i+1)*RADIAL_SEGS+s
            faces.append((a, b, c, d))
    bc = len(verts); verts.append(Vector((0,0,0)))
    for s in range(RADIAL_SEGS):
        faces.append((bc, (s+1)%RADIAL_SEGS, s))
    sc = len(verts); verts.append(Vector((SPINE_LENGTH,0,0)))
    lb = n_rings * RADIAL_SEGS
    for s in range(RADIAL_SEGS):
        faces.append((sc, lb+s, lb+(s+1)%RADIAL_SEGS))
    return make_obj("SM_Hull", verts, faces, 'hull')


# ---------------------------------------------------------------------------
# ROOMS — Each with unique proportions
# ---------------------------------------------------------------------------

def build_room_upper(room):
    name = room["name"]
    xs, xe = room["x_start"], room["x_end"]
    fz = room["floor_z"]
    cz = fz + room["upper_h"]
    hw = room["width"] / 2

    verts, faces = [], []
    arc, xn = 20, max(4, int(room["length"] / 80))

    for ix in range(xn + 1):
        x = xs + (xe - xs) * ix / xn
        r = hull_radius_smooth(x / SPINE_LENGTH) - WALL_THICK
        local_hw = min(hw, section_hw(r, fz) if r > 0 else hw)

        for ia in range(arc + 1):
            t = ia / arc
            angle = math.pi * t
            y = local_hw * math.cos(angle)
            z_hull = r * math.sin(angle) if r > 0 else cz
            z = max(fz, min(cz, z_hull))
            verts.append(Vector((x, y, z)))

    ring = arc + 1
    for ix in range(xn):
        for ia in range(arc):
            v0, v1 = ix*ring+ia, ix*ring+ia+1
            v2, v3 = (ix+1)*ring+ia+1, (ix+1)*ring+ia
            faces.append((v0, v3, v2, v1))

    return make_obj(f"SM_{name}_Upper", verts, faces, name)

def build_room_lower(room):
    if not room["has_lower"]:
        return None
    name = room["name"]
    xs, xe = room["x_start"], room["x_end"]
    fz_upper = room["floor_z"]
    cz = fz_upper - DECK_THICK
    fz = cz - room["lower_h"]
    hw = room["width"] / 2 * 0.85  # Lower deck slightly narrower

    verts, faces = [], []
    arc, xn = 20, max(4, int(room["length"] / 80))

    for ix in range(xn + 1):
        x = xs + (xe - xs) * ix / xn
        r = hull_radius_smooth(x / SPINE_LENGTH) - WALL_THICK
        local_hw = min(hw, section_hw(r, fz) if r > 0 else hw)

        for ia in range(arc + 1):
            t = ia / arc
            angle = -math.pi/2 + math.pi * t  # Bottom half arc
            y_raw = local_hw * math.cos(angle)
            z_raw = r * math.sin(angle) if r > 0 else fz
            y = y_raw
            z = max(fz, min(cz, z_raw))
            verts.append(Vector((x, y, z)))

    ring = arc + 1
    for ix in range(xn):
        for ia in range(arc):
            v0, v1 = ix*ring+ia, ix*ring+ia+1
            v2, v3 = (ix+1)*ring+ia+1, (ix+1)*ring+ia
            faces.append((v0, v1, v2, v3))

    # Floor
    fb = len(verts); fs = 8
    for ix in range(xn + 1):
        x = xs + (xe - xs) * ix / xn
        for iy in range(fs + 1):
            y = -local_hw + 2 * local_hw * iy / fs
            verts.append(Vector((x, y, fz)))
    fr = fs + 1
    for ix in range(xn):
        for iy in range(fs):
            faces.append((fb+ix*fr+iy, fb+ix*fr+iy+1, fb+(ix+1)*fr+iy+1, fb+(ix+1)*fr+iy))

    return make_obj(f"SM_{name}_Lower", verts, faces, name)


def build_deck():
    """Deck floor only between rooms that have lower decks."""
    bm = bmesh.new()
    nx, ny = 50, 20

    for room in ROOMS:
        if not room["has_lower"]:
            continue
        xs, xe = room["x_start"], room["x_end"]
        hw = room["width"] / 2
        fz = room["floor_z"]

        grid = {}
        local_nx = max(8, int(room["length"] / 60))
        for ix in range(local_nx + 1):
            x = xs + (xe - xs) * ix / local_nx
            for iy in range(ny + 1):
                y = -hw + 2 * hw * iy / ny
                grid[(ix, iy)] = bm.verts.new((x, y, fz))

        bm.verts.ensure_lookup_table()
        for ix in range(local_nx):
            for iy in range(ny):
                vs = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
                if all(vs):
                    cx = (vs[0].co.x + vs[2].co.x) / 2
                    cy = (vs[0].co.y + vs[2].co.y) / 2
                    if not any(abs(cx-hx) < HATCH_SIZE/2 and abs(cy) < HATCH_SIZE/2
                               for hx in HATCH_X):
                        try: bm.faces.new(vs)
                        except: pass

    mesh = bpy.data.meshes.new("SM_Deck")
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new("SM_Deck", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat("SM_Deck", COLORS['deck']))
    mod = obj.modifiers.new("Sol", 'SOLIDIFY')
    mod.thickness = -DECK_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def build_bulkhead(name, x, room_left, room_right):
    """Bulkhead shaped by the LARGER of the two adjacent rooms."""
    bm = bmesh.new()
    gy, gz = 22, 18

    # Height: from lowest floor to highest ceiling of adjacent rooms
    fz_l = room_left["floor_z"] - (DECK_THICK + room_left["lower_h"] if room_left["has_lower"] else 0)
    fz_r = room_right["floor_z"] - (DECK_THICK + room_right["lower_h"] if room_right["has_lower"] else 0)
    zmin = min(fz_l, fz_r)
    cz_l = room_left["floor_z"] + room_left["upper_h"]
    cz_r = room_right["floor_z"] + room_right["upper_h"]
    zmax = max(cz_l, cz_r)

    hw = max(room_left["width"], room_right["width"]) / 2
    r = hull_radius_smooth(x / SPINE_LENGTH) - WALL_THICK
    hw = min(hw, r)

    dw, dh = DOOR_CONFIGS.get(room_right["name"], (90, 180))
    dsill = max(room_left["floor_z"], room_right["floor_z"])

    grid = {}
    for iz in range(gz + 1):
        z = zmin + (zmax - zmin) * iz / gz
        local_hw = section_hw(r, z) if r > 0 else hw
        local_hw = min(hw, local_hw)
        if local_hw < 1: continue
        for iy in range(gy + 1):
            y = -local_hw + 2 * local_hw * iy / gy
            if -dw/2 < y < dw/2 and dsill < z < dsill + dh:
                continue
            grid[(iy, iz)] = bm.verts.new((x, y, z))

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
    obj.data.materials.append(get_mat(name, COLORS['bulk']))
    mod = obj.modifiers.new("Sol", 'SOLIDIFY')
    mod.thickness = WALL_THICK; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout — Level-Design-First (Script 5)")
    print("Autonomous design: gameplay feel drives geometry")
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

    print(f"\nSpatial Story ({SPINE_LENGTH/100:.0f}m):")
    for r in ROOMS:
        deck_info = f"upper={r['upper_h']}cm"
        if r["has_lower"]:
            deck_info += f" + lower={r['lower_h']}cm"
        print(f"  {r['name']:15s} [{r['x_start']:6.0f}-{r['x_end']:6.0f}] "
              f"W={r['width']}cm {deck_info} [{r['mood']}]")

    print("\n--- Hull (organic envelope around rooms) ---")
    build_hull()
    print("--- Deck Floor ---")
    build_deck()
    print("--- Rooms ---")
    for room in ROOMS:
        build_room_upper(room)
        if room["has_lower"]:
            build_room_lower(room)
    print("--- Bulkheads ---")
    for i in range(len(ROOMS) - 1):
        build_bulkhead(f"SM_BH_{i}", BULKHEAD_X[i], ROOMS[i], ROOMS[i+1])

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    print("OBJECTS:")
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if obj.type == 'MESH':
            print(f"  {obj.name}: {len(obj.data.vertices)}V {len(obj.data.polygons)}F")
    print("=" * 60)
    print("\nDesign rationale:")
    print("  TorpedoRoom: Tall+narrow — claustrophobic entry, sets tension")
    print("  Operations:  Wide+bright — relief after torpedo room, command hub")
    print("  CrewQuarters: Medium+cozy — human scale, bunks, personal space")
    print("  Reactor:     Massive+industrial — awe, danger, vertical drama")
    print("  EngineRoom:  Narrow+long — compression toward stern, urgency")

if __name__ == "__main__":
    main()
