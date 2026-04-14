"""
Sub3D — Blockout Submarine: Frame-Station Lofting (Script 4)
=============================================================
RECOMMENDED METHOD — Naval architecture approach.

Instead of a radius curve + cross-section formula, define the hull as a
series of FRAME STATIONS (cross-section profiles at specific X positions).
The hull surface is then LOFTED between these profiles.

Why this is better:
  1. Direct control over shape at every station (bow is narrow+tall,
     midship is wide+flat, stern is round+small)
  2. Each frame is a separate 2D profile — easy to edit in Blender
  3. Natural integration with compartment boundaries (one frame per bulkhead)
  4. This is how real submarines are designed (table of offsets)
  5. Interior walls AUTOMATICALLY follow the hull shape
  6. Different deck heights per compartment are trivial

Each frame station is defined as a list of (y, z) points forming a
closed cross-section contour. The script interpolates between stations
using Catmull-Rom splines for smooth hull surface.
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ---------------------------------------------------------------------------
# FRAME STATIONS — Hand-designed cross-sections
# ---------------------------------------------------------------------------

# Each station: (x_position_cm, [(y, z), ...] contour points)
# Contour is a HALF-PROFILE (starboard side only, y >= 0)
# Mirrored automatically for port side.
# Points go from keel (bottom) clockwise to deck (top center)

SPINE_LENGTH = 4000.0  # 40m

def make_ellipse_frame(width, height, n=20, flat_bottom=0.0):
    """Generate a half-frame profile (starboard, keel to top).
    flat_bottom: fraction of bottom that's flat (0-0.3)"""
    points = []
    for i in range(n + 1):
        t = i / n  # 0=keel, 1=top
        angle = -math.pi/2 + math.pi * t  # -90° to +90°

        y = width/2 * math.cos(angle)
        z = height/2 * math.sin(angle)

        # Flatten bottom
        if z < -height/2 * (1 - flat_bottom) and flat_bottom > 0:
            z = -height/2 * (1 - flat_bottom)

        points.append((y, z))
    return points


def make_teardrop_frame(width, height, n=20):
    """Tear-drop: narrow at bottom, wider at mid, round at top."""
    points = []
    for i in range(n + 1):
        t = i / n
        angle = -math.pi/2 + math.pi * t

        # Teardrop: wider in upper half
        if math.sin(angle) < 0:
            # Lower half: narrower
            w_mod = 0.7 + 0.3 * (1 + math.sin(angle))
        else:
            w_mod = 1.0

        y = width/2 * w_mod * math.cos(angle)
        z = height/2 * math.sin(angle)
        points.append((y, z))
    return points


# Frame station definitions
# (normalized_x, half_width, height, profile_type, flat_bottom)
FRAME_DEFS = [
    # Bow tip
    (0.000,   0,     0,    'tip',     0),
    # Bow nose — tall and narrow
    (0.030,  40,   120,   'ellipse',  0),
    # Bow forward — still narrow, getting taller
    (0.070,  80,   250,   'ellipse',  0),
    # Bow mid — opening up, slightly wider than tall
    (0.120, 160,   380,   'ellipse',  0.05),
    # Bow shoulder — approaching body width
    (0.170, 240,   480,   'ellipse',  0.10),
    # Forward body — nearly full width, flat bottom developing
    (0.220, 300,   540,   'ellipse',  0.15),
    # Helm station — full body
    (0.280, 320,   560,   'ellipse',  0.18),
    # Mid-body forward — maximum beam
    (0.400, 330,   560,   'ellipse',  0.20),
    # Mid-body center — sustained max
    (0.500, 330,   560,   'ellipse',  0.20),
    # Mid-body aft — still max
    (0.600, 325,   555,   'ellipse',  0.18),
    # Engine forward
    (0.700, 310,   540,   'ellipse',  0.15),
    # Engine aft — starting to narrow
    (0.780, 280,   510,   'ellipse',  0.10),
    # Stern shoulder
    (0.840, 240,   460,   'ellipse',  0.05),
    # Stern mid — narrowing
    (0.890, 180,   380,   'teardrop', 0),
    # Stern aft — getting small
    (0.930, 120,   280,   'teardrop', 0),
    # Stern narrow
    (0.960,  60,   180,   'teardrop', 0),
    # Stern tip
    (0.985,  20,    80,   'ellipse',  0),
    # Stern end
    (1.000,   0,     0,   'tip',      0),
]

# Generate actual frame profiles
FRAMES = []  # (x_cm, [(y, z), ...])
PROFILE_PTS = 24  # Points per half-profile

for norm_x, hw, h, ptype, fb in FRAME_DEFS:
    x = norm_x * SPINE_LENGTH
    if ptype == 'tip':
        FRAMES.append((x, [(0, 0)]))
    elif ptype == 'teardrop':
        FRAMES.append((x, make_teardrop_frame(hw * 2, h, PROFILE_PTS)))
    else:
        FRAMES.append((x, make_ellipse_frame(hw * 2, h, PROFILE_PTS, fb)))


# ---------------------------------------------------------------------------
# COMPARTMENTS
# ---------------------------------------------------------------------------

WALL_THICK = 14.0
DECK_THICK = 22.0
UPPER_FLOOR_Z = 5.0
UPPER_CEIL_Z = 210.0
LOWER_FLOOR_Z = -185.0
LOWER_CEIL_Z = UPPER_FLOOR_Z - DECK_THICK

COMP_NAMES = ["Helm", "Crew", "Crew2", "Engine"]
COMP_BOUNDS_NORM = [(0.22, 0.36), (0.36, 0.52), (0.52, 0.68), (0.68, 0.82)]
COMP_BOUNDS = [(s * SPINE_LENGTH, e * SPINE_LENGTH) for s, e in COMP_BOUNDS_NORM]
BULKHEAD_X = [COMP_BOUNDS[i][1] for i in range(len(COMP_NAMES) - 1)]
HATCH_X = [(b[0] + b[1]) / 2 for b in COMP_BOUNDS]
HATCH_SIZE = 85.0
DOOR_W, DOOR_H = 95.0, 190.0
DOOR_LW, DOOR_LH = 85.0, 160.0

# Colors
COLORS = {
    'hull': (0.20, 0.22, 0.26), 'deck': (0.34, 0.32, 0.28),
    'upper': (0.42, 0.42, 0.38), 'lower': (0.30, 0.30, 0.26),
    'bulk': (0.46, 0.44, 0.40),
}


# ---------------------------------------------------------------------------
# INTERPOLATION — Catmull-Rom between frames
# ---------------------------------------------------------------------------

def interpolate_frames(x_cm):
    """Get interpolated half-profile at arbitrary X position."""
    # Find bracketing frames
    for i in range(len(FRAMES) - 1):
        x0, pts0 = FRAMES[i]
        x1, pts1 = FRAMES[i + 1]
        if x0 <= x_cm <= x1:
            t = (x_cm - x0) / max(1e-6, x1 - x0)
            # Smooth hermite
            t = t * t * (3 - 2 * t)

            # Interpolate between profiles
            n0, n1 = len(pts0), len(pts1)
            n_out = max(n0, n1, PROFILE_PTS + 1)
            result = []
            for j in range(n_out):
                t_profile = j / max(1, n_out - 1)
                # Sample from each profile
                idx0 = min(int(t_profile * (n0 - 1)), n0 - 1)
                idx1 = min(int(t_profile * (n1 - 1)), n1 - 1)
                y0, z0 = pts0[idx0] if n0 > 1 else (0, 0)
                y1, z1 = pts1[idx1] if n1 > 1 else (0, 0)
                y = y0 + (y1 - y0) * t
                z = z0 + (z1 - z0) * t
                result.append((y, z))
            return result

    # Outside range
    return FRAMES[-1][1] if x_cm >= FRAMES[-1][0] else FRAMES[0][1]


def frame_half_width_at_z(x_cm, z_query):
    """Get half-width at given Z for interpolated frame at X."""
    profile = interpolate_frames(x_cm)
    if len(profile) < 2:
        return 0

    # Find the Y at closest Z
    best_y = 0
    for y, z in profile:
        if abs(z - z_query) < 30:  # Within 30cm tolerance
            best_y = max(best_y, y)
    return best_y


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
            bsdf.inputs["Roughness"].default_value = 0.7
            bsdf.inputs["Metallic"].default_value = 0.5
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
# HULL — Lofted between frame stations
# ---------------------------------------------------------------------------

def build_hull():
    """Build hull by lofting between interpolated frame stations."""
    verts = []
    faces = []

    n_rings = 100
    n_profile = PROFILE_PTS + 1  # Points per half-profile
    ring_size = n_profile * 2 - 2  # Full ring (mirrored, no duplicate at center)

    for i in range(n_rings + 1):
        x = SPINE_LENGTH * i / n_rings
        half_profile = interpolate_frames(x)

        if len(half_profile) < 2:
            # Degenerate (tip) — single point
            for _ in range(ring_size):
                verts.append(Vector((x, 0, 0)))
            continue

        # Build full ring: starboard (half_profile) + port (mirrored)
        ring_pts = []
        # Starboard: keel to top
        for y, z in half_profile:
            ring_pts.append((y, z))
        # Port: top to keel (mirror Y, skip endpoints to avoid duplication)
        for y, z in reversed(half_profile[1:-1]):
            ring_pts.append((-y, z))

        # Pad or trim to ring_size
        while len(ring_pts) < ring_size:
            ring_pts.append(ring_pts[-1])
        ring_pts = ring_pts[:ring_size]

        for y, z in ring_pts:
            verts.append(Vector((x, y, z)))

    # Connect rings
    for i in range(n_rings):
        for j in range(ring_size):
            jn = (j + 1) % ring_size
            a = i * ring_size + j
            b = i * ring_size + jn
            c = (i + 1) * ring_size + jn
            d = (i + 1) * ring_size + j
            faces.append((a, b, c, d))

    return make_obj("SM_Hull", verts, faces, 'hull')


# ---------------------------------------------------------------------------
# DECK + INTERIORS + BULKHEADS (same pattern as other scripts)
# ---------------------------------------------------------------------------

def build_deck():
    bm = bmesh.new()
    nx, ny = 60, 28
    xmin, xmax = COMP_BOUNDS[0][0], COMP_BOUNDS[-1][1]
    grid = {}
    for ix in range(nx + 1):
        x = xmin + (xmax - xmin) * ix / nx
        hw = frame_half_width_at_z(x, UPPER_FLOOR_Z) - WALL_THICK
        if hw < 10: continue
        for iy in range(ny + 1):
            y = -hw + 2 * hw * iy / ny
            grid[(ix, iy)] = bm.verts.new((x, y, UPPER_FLOOR_Z))
    bm.verts.ensure_lookup_table()
    for ix in range(nx):
        for iy in range(ny):
            vs = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
            if all(vs):
                cx = (vs[0].co.x + vs[2].co.x) / 2
                cy = (vs[0].co.y + vs[2].co.y) / 2
                if not any(abs(cx-hx) < HATCH_SIZE/2 and abs(cy) < HATCH_SIZE/2 for hx in HATCH_X):
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

def build_interior(name, xs, xe, fz, cz, ck):
    verts, faces = [], []
    arc, xn = 22, 8
    is_lower = fz < 0
    for ix in range(xn + 1):
        x = xs + (xe - xs) * ix / xn
        profile = interpolate_frames(x)
        if len(profile) < 2: continue
        # Get inner profile (offset by wall thickness)
        for ia in range(arc + 1):
            t = ia / arc
            if is_lower:
                z_target = fz + (cz - fz) * (1 - t)
            else:
                z_target = fz + (cz - fz) * t
            hw = frame_half_width_at_z(x, z_target) - WALL_THICK
            hw = max(hw, 5)
            # Starboard wall point
            y = hw
            z = max(fz, min(cz, z_target))
            verts.append(Vector((x, y, z)))
    ring = arc + 1
    for ix in range(xn):
        for ia in range(arc):
            v0, v1 = ix*ring+ia, ix*ring+ia+1
            v2, v3 = (ix+1)*ring+ia+1, (ix+1)*ring+ia
            faces.append((v0, v1, v2, v3))
    # Mirror for port side
    n = len(verts)
    for v in list(verts[:n]):
        verts.append(Vector((v.x, -v.y, v.z)))
    for f in list(faces[:len(faces)]):
        faces.append((f[3]+n, f[2]+n, f[1]+n, f[0]+n))

    return make_obj(name, verts, faces, ck)

def build_bulkhead(name, x, zmin, zmax, dw, dh, dsill):
    bm = bmesh.new()
    gy, gz = 22, 18
    grid = {}
    for iz in range(gz + 1):
        z = zmin + (zmax - zmin) * iz / gz
        hw = frame_half_width_at_z(x, z) - WALL_THICK
        if hw < 1: continue
        for iy in range(gy + 1):
            y = -hw + 2 * hw * iy / gy
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
    print("Sub3D Blockout — Frame-Station Lofting (Script 4)")
    print("RECOMMENDED: Naval architecture approach")
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

    print(f"\nFrame stations: {len(FRAME_DEFS)}")
    print(f"Spine: {SPINE_LENGTH/100:.0f}m")
    for nx, hw, h, pt, fb in FRAME_DEFS:
        if hw > 0:
            print(f"  X={nx:.2f} ({nx*SPINE_LENGTH:.0f}cm): {hw*2:.0f}W x {h:.0f}H [{pt}] flat={fb:.0f}")

    print("\n--- Lofting Hull ---")
    build_hull()
    print("--- Deck ---")
    build_deck()
    print("--- Interiors ---")
    for name, (xs, xe) in zip(COMP_NAMES, COMP_BOUNDS):
        build_interior(f"SM_{name}_Upper", xs, xe, UPPER_FLOOR_Z, UPPER_CEIL_Z, 'upper')
        build_interior(f"SM_{name}_Lower", xs, xe, LOWER_FLOOR_Z, LOWER_CEIL_Z, 'lower')
    print("--- Bulkheads ---")
    for i, bx in enumerate(BULKHEAD_X):
        build_bulkhead(f"SM_BH_Upper_{i}", bx, UPPER_FLOOR_Z, UPPER_CEIL_Z, DOOR_W, DOOR_H, UPPER_FLOOR_Z)
        build_bulkhead(f"SM_BH_Lower_{i}", bx, LOWER_FLOOR_Z, LOWER_CEIL_Z, DOOR_LW, DOOR_LH, LOWER_FLOOR_Z)

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
