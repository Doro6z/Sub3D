"""
Sub3D — Blockout Submarine: Gameplay-Constrained (Script 3)
============================================================
Every dimension is derived from gameplay code constraints, NOT aesthetics.
The hull shape is a CONSEQUENCE of what the gameplay systems need.

Constraints (from source code):
  - Crew capsule: 88cm half-height, 176cm full → min standing height 180cm
  - Door: 90×180cm → compartment wider than 150cm (door + margins)
  - Brace probe: 90cm → walls within reach for stabilization
  - Interaction: 250cm → stations reachable
  - Flood capacity: 12000L per compartment → drives volume
  - Water padding: 50cm → bounds extend beyond geometry
  - Walk speed: 300cm/s → submarine traversable in <20 seconds

This script computes minimum volumes, then wraps a hull around them.
Also outputs a Definition config file for UE integration.
"""

import bpy
import bmesh
import math
import json
import os
from mathutils import Vector

# ---------------------------------------------------------------------------
# GAMEPLAY CONSTRAINTS — All from code, no aesthetic choices
# ---------------------------------------------------------------------------

# Crew dimensions
CAPSULE_HALF_HEIGHT = 88.0   # cm
CAPSULE_RADIUS = 42.0        # cm
CREW_FULL_HEIGHT = CAPSULE_HALF_HEIGHT * 2  # 176cm
EYE_HEIGHT = 70.0            # cm above capsule root
MIN_STANDING = CREW_FULL_HEIGHT + 10  # 186cm (margin for uneven floor)
CROUCH_HEIGHT = CAPSULE_HALF_HEIGHT  # 88cm

# Doors (from SubmarineDefinitionTypes.h)
DOOR_WIDTH = 90.0
DOOR_HEIGHT = 180.0
DOOR_DEPTH = 5.0
DOOR_MIN_WIDTH = 60.0
DOOR_MIN_HEIGHT = 140.0

# Hatch (vertical passage)
HATCH_SIZE = 80.0  # 80x80cm square

# Movement
WALK_SPEED = 300.0      # cm/s
BRACE_DISTANCE = 90.0   # cm (wall proximity for stabilization)
INTERACT_DISTANCE = 250.0  # cm
FLOOR_SNAP_MAX = 352.0  # cm (4 * half-height)

# Flood
DEFAULT_CAPACITY = 12000.0  # liters
MAX_INFLOW = 3000.0         # L/s
MIN_COMP_HEIGHT = 100.0     # cm (SubFloodComponent)
WATER_PADDING = 50.0        # cm (water plane extends beyond bounds)

# Spawn
SPAWN_Z_OFFSET = 92.0       # cm above floor (capsule center)

# Traversal target
MAX_TRAVERSE_TIME = 18.0     # seconds to walk full length
MAX_LENGTH = WALK_SPEED * MAX_TRAVERSE_TIME  # 5400cm

# Structure
WALL_THICKNESS = 14.0
DECK_THICKNESS = 22.0

# ---------------------------------------------------------------------------
# COMPUTED DIMENSIONS — Pure math from constraints
# ---------------------------------------------------------------------------

# Deck heights
UPPER_DECK_HEIGHT = MIN_STANDING + 20  # 206cm (headroom margin)
LOWER_DECK_HEIGHT = MIN_STANDING       # 186cm (tighter, engine room feel)

# Interior height needed for 2 decks
TOTAL_INTERIOR_HEIGHT = UPPER_DECK_HEIGHT + DECK_THICKNESS + LOWER_DECK_HEIGHT  # 414cm
INTERIOR_HALF_HEIGHT = TOTAL_INTERIOR_HEIGHT / 2  # 207cm

# Hull radius (circular section)
HULL_RADIUS = INTERIOR_HALF_HEIGHT + WALL_THICKNESS  # 221cm
# But! Interior width = 2*R at center = 442cm. Too wide? Check brace distance.
# With brace at 90cm, ideal corridor width = ~180cm (brace both sides)
# For open compartments, 442cm is fine (room with walls reachable)

# Actually hull is too small for comfortable 2-deck. Increase radius.
# At R=280, interior height = 560cm → upper=206, plate=22, lower=186 → 414cm used, 146cm margin
HULL_RADIUS = 280.0
INTERIOR_RADIUS = HULL_RADIUS - WALL_THICKNESS  # 266cm

# Deck positions
UPPER_FLOOR_Z = (INTERIOR_RADIUS - TOTAL_INTERIOR_HEIGHT) / 2 + LOWER_DECK_HEIGHT + DECK_THICKNESS
# Simpler: center the 414cm stack in 532cm interior
STACK_BOTTOM = -TOTAL_INTERIOR_HEIGHT / 2  # -207
LOWER_FLOOR_Z = STACK_BOTTOM  # -207
LOWER_CEIL_Z = LOWER_FLOOR_Z + LOWER_DECK_HEIGHT  # -21
UPPER_FLOOR_Z = LOWER_CEIL_Z + DECK_THICKNESS  # 1
UPPER_CEIL_Z = UPPER_FLOOR_Z + UPPER_DECK_HEIGHT  # 207

# Compartment sizing — from flood capacity
# Volume of a cylinder section: V = π * R² * L (approximate)
# 12000L = 12m³ = 12,000,000 cm³
# Interior cross-section area ≈ π * (INTERIOR_RADIUS)² / 2 (half for each deck)
# Area per deck ≈ π * 266² / 4 ≈ 55,600 cm² (quarter circle approx for deck height)
# Actually: area of circular segment for 186cm height at R=266
# Simplified: use rectangular approximation
DECK_WIDTH_AT_UPPER = 2 * math.sqrt(max(0, INTERIOR_RADIUS**2 - UPPER_FLOOR_Z**2))  # ~532cm
DECK_WIDTH_AT_LOWER = 2 * math.sqrt(max(0, INTERIOR_RADIUS**2 - LOWER_FLOOR_Z**2))  # ~328cm
UPPER_CROSS_AREA = DECK_WIDTH_AT_UPPER * UPPER_DECK_HEIGHT * 0.7  # 70% fill factor
LOWER_CROSS_AREA = DECK_WIDTH_AT_LOWER * LOWER_DECK_HEIGHT * 0.7

TARGET_VOLUME_CM3 = DEFAULT_CAPACITY * 1000  # 12,000,000 cm³
MIN_COMP_LENGTH_UPPER = TARGET_VOLUME_CM3 / max(1, UPPER_CROSS_AREA)
MIN_COMP_LENGTH_LOWER = TARGET_VOLUME_CM3 / max(1, LOWER_CROSS_AREA)
MIN_COMP_LENGTH = max(MIN_COMP_LENGTH_UPPER, MIN_COMP_LENGTH_LOWER, 200)  # At least 200cm

# Station clearance
STATION_CLEARANCE = INTERACT_DISTANCE  # 250cm around station

# Number of compartments: fit in max length
BODY_FRACTION = 0.6  # 60% of length is body (rest is bow/stern taper)
BODY_LENGTH = MAX_LENGTH * BODY_FRACTION  # 3240cm
NUM_COMPARTMENTS = max(3, min(6, int(BODY_LENGTH / MIN_COMP_LENGTH)))
COMP_LENGTH = BODY_LENGTH / NUM_COMPARTMENTS

# Final spine length
BOW_LENGTH = MAX_LENGTH * 0.22
STERN_LENGTH = MAX_LENGTH * 0.18
SPINE_LENGTH = BOW_LENGTH + BODY_LENGTH + STERN_LENGTH

COMP_NAMES = ["Helm", "Crew", "Crew2", "Engine"]
if NUM_COMPARTMENTS >= 5:
    COMP_NAMES = ["Torpedo", "Helm", "Crew", "Crew2", "Engine"]
if NUM_COMPARTMENTS >= 6:
    COMP_NAMES = ["Torpedo", "Helm", "Crew", "Crew2", "Aux", "Engine"]
COMP_NAMES = COMP_NAMES[:NUM_COMPARTMENTS]

BODY_START = BOW_LENGTH
BODY_END = BOW_LENGTH + BODY_LENGTH
COMP_BOUNDS = [(BODY_START + i * COMP_LENGTH, BODY_START + (i+1) * COMP_LENGTH)
               for i in range(NUM_COMPARTMENTS)]
BULKHEAD_X = [COMP_BOUNDS[i][1] for i in range(NUM_COMPARTMENTS - 1)]
HATCH_X = [(b[0] + b[1]) / 2 for b in COMP_BOUNDS]

# Radius profile (simple: taper in bow/stern, constant in body)
RADIAL_SEGS = 32

def hull_radius(norm_x):
    bs = BODY_START / SPINE_LENGTH
    be = BODY_END / SPINE_LENGTH
    if norm_x < bs:
        t = norm_x / bs
        return HULL_RADIUS * math.sin(t * math.pi / 2)  # Smooth bow
    elif norm_x > be:
        t = (1.0 - norm_x) / (1.0 - be)
        return HULL_RADIUS * t ** 0.8  # Tapered stern
    return HULL_RADIUS

def section_pt(r, angle):
    if r < 1: r = 1
    y = r * math.cos(angle)
    z = r * math.sin(angle)
    return y, z

def section_hw(r, z):
    if r <= 0: return 0
    if abs(z) >= r: return 0
    return math.sqrt(max(0, r*r - z*z))

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

COLORS = {
    'hull': (0.24, 0.26, 0.30), 'deck': (0.36, 0.34, 0.30),
    'upper': (0.44, 0.44, 0.40), 'lower': (0.32, 0.32, 0.28),
    'bulk': (0.48, 0.46, 0.42), 'airlock': (0.38, 0.36, 0.32),
}

def get_mat(name, color):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*color, 1)
            bsdf.inputs["Roughness"].default_value = 0.75
    return mat

def make_obj(name, verts, faces, color_key):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat(name, COLORS[color_key]))
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
# MESH GENERATORS
# ---------------------------------------------------------------------------

def build_hull():
    verts, faces = [], []
    n_rings = 80
    for i in range(n_rings + 1):
        nx = i / n_rings
        x = nx * SPINE_LENGTH
        r = hull_radius(nx)
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
    # Caps
    bc = len(verts); verts.append(Vector((0,0,0)))
    for s in range(RADIAL_SEGS):
        faces.append((bc, (s+1)%RADIAL_SEGS, s))
    sc = len(verts); verts.append(Vector((SPINE_LENGTH,0,0)))
    lb = n_rings * RADIAL_SEGS
    for s in range(RADIAL_SEGS):
        faces.append((sc, lb+s, lb+(s+1)%RADIAL_SEGS))
    return make_obj("SM_Hull", verts, faces, 'hull')

def build_deck():
    bm = bmesh.new()
    nx, ny = 50, 24
    xmin, xmax = COMP_BOUNDS[0][0], COMP_BOUNDS[-1][1]
    grid = {}
    for ix in range(nx+1):
        x = xmin + (xmax-xmin) * ix/nx
        r = hull_radius(x/SPINE_LENGTH) - WALL_THICKNESS
        hw = section_hw(r, UPPER_FLOOR_Z)
        if hw < 10: continue
        for iy in range(ny+1):
            y = -hw + 2*hw*iy/ny
            grid[(ix,iy)] = bm.verts.new((x, y, UPPER_FLOOR_Z))
    bm.verts.ensure_lookup_table()
    for ix in range(nx):
        for iy in range(ny):
            vs = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
            if all(vs):
                cx = (vs[0].co.x+vs[2].co.x)/2; cy = (vs[0].co.y+vs[2].co.y)/2
                if not any(abs(cx-hx)<HATCH_SIZE/2 and abs(cy)<HATCH_SIZE/2 for hx in HATCH_X):
                    try: bm.faces.new(vs)
                    except: pass
    mesh = bpy.data.meshes.new("SM_Deck")
    bm.to_mesh(mesh); bm.free(); mesh.update()
    obj = bpy.data.objects.new("SM_Deck", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(get_mat("SM_Deck", COLORS['deck']))
    mod = obj.modifiers.new("Sol", 'SOLIDIFY'); mod.thickness = -DECK_THICKNESS; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def build_interior(name, xs, xe, floor_z, ceil_z, color_key):
    verts, faces = [], []
    arc, xn = 20, 8
    is_lower = floor_z < 0
    for ix in range(xn+1):
        x = xs + (xe-xs)*ix/xn
        r = hull_radius(x/SPINE_LENGTH) - WALL_THICKNESS
        if r < 10: r = 10
        if is_lower:
            a0 = math.asin(max(-1, min(1, ceil_z/r)))
            a1 = -(math.pi + a0)
        else:
            a0 = 0; a1 = math.pi
        for ia in range(arc+1):
            t = ia/arc
            a = a0 + (a1-a0)*t
            y, z = section_pt(r, a)
            z = max(floor_z, min(ceil_z, z))
            verts.append(Vector((x, y, z)))
    ring = arc+1
    for ix in range(xn):
        for ia in range(arc):
            v0,v1 = ix*ring+ia, ix*ring+ia+1
            v2,v3 = (ix+1)*ring+ia+1, (ix+1)*ring+ia
            faces.append((v0,v1,v2,v3) if is_lower else (v0,v3,v2,v1))
    # Floor for lower
    if is_lower:
        fb = len(verts); fs = 8
        for ix in range(xn+1):
            x = xs + (xe-xs)*ix/xn
            r = hull_radius(x/SPINE_LENGTH) - WALL_THICKNESS
            hw = section_hw(r, floor_z); hw = max(hw, 60)
            for iy in range(fs+1):
                y = -hw + 2*hw*iy/fs
                verts.append(Vector((x, y, floor_z)))
        fr = fs+1
        for ix in range(xn):
            for iy in range(fs):
                faces.append((fb+ix*fr+iy, fb+ix*fr+iy+1, fb+(ix+1)*fr+iy+1, fb+(ix+1)*fr+iy))
    # Ceiling for upper
    if not is_lower:
        cb = len(verts); cs = 8
        for ix in range(xn+1):
            x = xs + (xe-xs)*ix/xn
            r = hull_radius(x/SPINE_LENGTH) - WALL_THICKNESS
            hw = section_hw(r, ceil_z); hw = max(hw, 30)
            for iy in range(cs+1):
                y = -hw + 2*hw*iy/cs
                verts.append(Vector((x, y, ceil_z)))
        cr = cs+1
        for ix in range(xn):
            for iy in range(cs):
                faces.append((cb+ix*cr+iy, cb+ix*cr+iy+1, cb+(ix+1)*cr+iy+1, cb+(ix+1)*cr+iy))
    return make_obj(name, verts, faces, color_key)

def build_bulkhead(name, x, zmin, zmax, dw, dh, dsill):
    bm = bmesh.new()
    gy, gz = 20, 16
    r = hull_radius(x/SPINE_LENGTH) - WALL_THICKNESS
    grid = {}
    for iz in range(gz+1):
        z = zmin + (zmax-zmin)*iz/gz
        hw = section_hw(r, z)
        if hw < 1: continue
        for iy in range(gy+1):
            y = -hw + 2*hw*iy/gy
            if y*y + z*z > r*r: continue
            if -dw/2 < y < dw/2 and dsill < z < dsill+dh: continue
            grid[(iy,iz)] = bm.verts.new((x, y, z))
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
    mod = obj.modifiers.new("Sol", 'SOLIDIFY'); mod.thickness = WALL_THICKNESS; mod.offset = 0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

# ---------------------------------------------------------------------------
# DEFINITION OUTPUT — JSON for UE integration
# ---------------------------------------------------------------------------

def export_definition():
    """Export a JSON file with USubmarineDefinition data for UE."""
    definition = {
        "submarine_name": "FP_Blockout_Gameplay",
        "hull_radius": HULL_RADIUS,
        "spine_length": SPINE_LENGTH,
        "wall_thickness": WALL_THICKNESS,
        "compartments": [],
        "connections": [],
    }

    comp_id = 0
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        # Upper deck compartment
        r_mid = hull_radius(((xs+xe)/2) / SPINE_LENGTH) - WALL_THICKNESS
        hw_upper = section_hw(r_mid, UPPER_FLOOR_Z)
        hw_lower = section_hw(r_mid, LOWER_FLOOR_Z)

        definition["compartments"].append({
            "id": f"{name}_Upper",
            "semantic_type": name if name in ["Helm","Engine"] else "Crew",
            "deck": "upper",
            "hydro_bounds_min": [xs, -hw_upper, UPPER_FLOOR_Z],
            "hydro_bounds_max": [xe, hw_upper, INTERIOR_RADIUS],
            "walkable_floor_z": UPPER_FLOOR_Z,
            "capacity_liters": DEFAULT_CAPACITY,
        })
        definition["compartments"].append({
            "id": f"{name}_Lower",
            "semantic_type": name if name in ["Helm","Engine"] else "Crew",
            "deck": "lower",
            "hydro_bounds_min": [xs, -hw_lower, -INTERIOR_RADIUS],
            "hydro_bounds_max": [xe, hw_lower, LOWER_CEIL_Z],
            "walkable_floor_z": LOWER_FLOOR_Z,
            "capacity_liters": DEFAULT_CAPACITY,
        })

    # Horizontal connections (upper deck)
    for i, bx in enumerate(BULKHEAD_X):
        definition["connections"].append({
            "id": f"BH{i}_Upper",
            "compartment_a": f"{COMP_NAMES[i]}_Upper",
            "compartment_b": f"{COMP_NAMES[i+1]}_Upper",
            "type": "Door",
            "door_width": DOOR_WIDTH,
            "door_height": DOOR_HEIGHT,
            "position_x": bx,
            "starts_closed": True,
        })
        definition["connections"].append({
            "id": f"BH{i}_Lower",
            "compartment_a": f"{COMP_NAMES[i]}_Lower",
            "compartment_b": f"{COMP_NAMES[i+1]}_Lower",
            "type": "Door",
            "door_width": DOOR_MIN_WIDTH + 20,
            "door_height": DOOR_MIN_HEIGHT + 20,
            "position_x": bx,
            "starts_closed": True,
        })

    # Vertical hatches
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        mid_x = (xs + xe) / 2
        definition["connections"].append({
            "id": f"Hatch_{name}",
            "compartment_a": f"{name}_Upper",
            "compartment_b": f"{name}_Lower",
            "type": "Hatch",
            "door_width": HATCH_SIZE,
            "door_height": HATCH_SIZE,
            "position_x": mid_x,
            "starts_closed": False,
        })

    # Write JSON
    out_path = os.path.join(os.path.dirname(bpy.data.filepath) if bpy.data.filepath else
                            "C:/Dev/Sub3D/Source/scripts",
                            "submarine_definition_gameplay.json")
    # Fallback path
    out_path = "C:/Dev/Sub3D/Source/scripts/submarine_definition_gameplay.json"
    with open(out_path, 'w') as f:
        json.dump(definition, f, indent=2)
    print(f"\nDefinition exported: {out_path}")
    return out_path

# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout — Gameplay-Constrained (Script 3)")
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

    print(f"\n--- Computed from gameplay constraints ---")
    print(f"Crew: {CREW_FULL_HEIGHT}cm tall, capsule R={CAPSULE_RADIUS}cm")
    print(f"Min standing: {MIN_STANDING}cm → Upper deck: {UPPER_DECK_HEIGHT}cm, Lower: {LOWER_DECK_HEIGHT}cm")
    print(f"Hull R: {HULL_RADIUS}cm (driven by 2-deck stack: {TOTAL_INTERIOR_HEIGHT}cm)")
    print(f"Spine: {SPINE_LENGTH:.0f}cm ({SPINE_LENGTH/100:.1f}m) — traverse in {SPINE_LENGTH/WALK_SPEED:.1f}s")
    print(f"Compartments: {NUM_COMPARTMENTS} x {COMP_LENGTH:.0f}cm each")
    print(f"  Min comp length from flood capacity: {MIN_COMP_LENGTH:.0f}cm")
    print(f"  Actual: {COMP_LENGTH:.0f}cm (>{MIN_COMP_LENGTH:.0f} ✓)")
    print(f"Decks: upper Z={UPPER_FLOOR_Z:.0f}→{UPPER_CEIL_Z:.0f}, lower Z={LOWER_FLOOR_Z:.0f}→{LOWER_CEIL_Z:.0f}")

    build_hull()
    build_deck()
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        build_interior(f"SM_{name}_Upper", xs, xe, UPPER_FLOOR_Z, UPPER_CEIL_Z, 'upper')
        build_interior(f"SM_{name}_Lower", xs, xe, LOWER_FLOOR_Z, LOWER_CEIL_Z, 'lower')
    for i, bx in enumerate(BULKHEAD_X):
        build_bulkhead(f"SM_BH_Upper_{i}", bx, UPPER_FLOOR_Z, UPPER_CEIL_Z, DOOR_WIDTH, DOOR_HEIGHT, UPPER_FLOOR_Z)
        build_bulkhead(f"SM_BH_Lower_{i}", bx, LOWER_FLOOR_Z, LOWER_CEIL_Z, DOOR_MIN_WIDTH+20, DOOR_MIN_HEIGHT+20, LOWER_FLOOR_Z)

    export_definition()

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n--- Objects ---")
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if obj.type == 'MESH':
            print(f"  {obj.name}: {len(obj.data.vertices)}V {len(obj.data.polygons)}F")
    print("=" * 60)

if __name__ == "__main__":
    main()
