"""
Sub3D — Submarine Ladder (single section)
==========================================
One ladder segment you can duplicate and place at hatch positions.
Run AFTER main.py (additive).

Blender > Scripting > Open > Alt+P
"""

import bpy
import math

# ─── Ladder dimensions ───
LADDER_WIDTH = 45.0       # Between side rails (cm)
RAIL_DIAMETER = 5.0       # Rail tube diameter
RUNG_DIAMETER = 3.5       # Rung tube diameter
RUNG_SPACING = 30.0       # Vertical distance between rungs
LADDER_HEIGHT = 250.0     # Total height of this section
RAIL_OFFSET = 8.0         # Rails curve inward at top/bottom

# Derived
N_RUNGS = int(LADDER_HEIGHT / RUNG_SPACING)
RAIL_R = RAIL_DIAMETER / 2
RUNG_R = RUNG_DIAMETER / 2
TUBE_SEGS = 8             # Tube cross-section resolution


def simple_mat(name, r, g, b):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = 0.6
        bsdf.inputs["Metallic"].default_value = 0.8
    return m


def make_tube(name, path_points, radius, segs=8):
    """
    Create a tube mesh along a path of (x, y, z) points.
    Simple sweep: circle cross-section at each path point.
    """
    v = []
    f = []
    n_path = len(path_points)

    for pi in range(n_path):
        p = path_points[pi]

        # Direction vector for orientation
        if pi < n_path - 1:
            fwd = (path_points[pi+1][0] - p[0],
                   path_points[pi+1][1] - p[1],
                   path_points[pi+1][2] - p[2])
        else:
            fwd = (p[0] - path_points[pi-1][0],
                   p[1] - path_points[pi-1][1],
                   p[2] - path_points[pi-1][2])

        # Normalize
        ln = math.sqrt(fwd[0]**2 + fwd[1]**2 + fwd[2]**2)
        if ln < 0.01:
            fwd = (0, 0, 1)
        else:
            fwd = (fwd[0]/ln, fwd[1]/ln, fwd[2]/ln)

        # Build perpendicular axes
        # Find a vector not parallel to fwd
        if abs(fwd[2]) < 0.9:
            up = (0, 0, 1)
        else:
            up = (1, 0, 0)

        # Cross products for local frame
        rx = (fwd[1]*up[2] - fwd[2]*up[1],
              fwd[2]*up[0] - fwd[0]*up[2],
              fwd[0]*up[1] - fwd[1]*up[0])
        ln_r = math.sqrt(rx[0]**2 + rx[1]**2 + rx[2]**2)
        if ln_r > 0.01:
            rx = (rx[0]/ln_r, rx[1]/ln_r, rx[2]/ln_r)

        ry = (fwd[1]*rx[2] - fwd[2]*rx[1],
              fwd[2]*rx[0] - fwd[0]*rx[2],
              fwd[0]*rx[1] - fwd[1]*rx[0])

        for si in range(segs):
            a = 2 * math.pi * si / segs
            ca, sa = math.cos(a), math.sin(a)
            vx = p[0] + radius * (rx[0]*ca + ry[0]*sa)
            vy = p[1] + radius * (rx[1]*ca + ry[1]*sa)
            vz = p[2] + radius * (rx[2]*ca + ry[2]*sa)
            v.append((vx, vy, vz))

    # Connect rings
    for pi in range(n_path - 1):
        for si in range(segs):
            ni = (si + 1) % segs
            a = pi * segs + si
            b = pi * segs + ni
            c = (pi+1) * segs + ni
            d = (pi+1) * segs + si
            f.append((a, b, c, d))

    return v, f


def build_ladder():
    all_v = []
    all_f = []

    hw = LADDER_WIDTH / 2

    # ─── Two side rails (vertical tubes, slight curve at top) ───
    for side in [-1, 1]:
        y = hw * side
        # Path: straight vertical with slight inward curve at top
        path = []
        n_pts = 12
        for i in range(n_pts + 1):
            t = i / n_pts
            z = t * LADDER_HEIGHT

            # Slight inward curve at top (last 15%)
            inward = 0
            if t > 0.85:
                curve_t = (t - 0.85) / 0.15
                inward = RAIL_OFFSET * curve_t * curve_t * side * -1

            path.append((0, y + inward, z))

        rv, rf = make_tube(f"rail_{side}", path, RAIL_R, TUBE_SEGS)
        # Offset face indices
        base = len(all_v)
        all_v.extend(rv)
        all_f.extend([(a+base, b+base, c+base, d+base) for a, b, c, d in rf])

    # ─── Rungs (horizontal tubes between rails) ───
    for ri in range(N_RUNGS):
        z = RUNG_SPACING * (ri + 1)
        if z > LADDER_HEIGHT - 20:
            break

        # Simple straight tube from port rail to starboard rail
        path = [
            (0, -hw, z),
            (0, -hw * 0.3, z),  # Intermediate points for smoother tube
            (0,  hw * 0.3, z),
            (0,  hw, z),
        ]

        rv, rf = make_tube(f"rung_{ri}", path, RUNG_R, 6)  # 6 segs for rungs (thinner)
        base = len(all_v)
        all_v.extend(rv)
        all_f.extend([(a+base, b+base, c+base, d+base) for a, b, c, d in rf])

    # Create mesh
    me = bpy.data.meshes.new("SM_Ladder")
    me.from_pydata(all_v, [], all_f)
    me.update(calc_edges=True)

    ob = bpy.data.objects.new("SM_Ladder", me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(simple_mat("M_Ladder", 0.45, 0.42, 0.35))

    # Auto smooth
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    try:
        bpy.ops.object.shade_auto_smooth()
    except:
        for p in ob.data.polygons:
            p.use_smooth = True
    ob.select_set(False)

    ob["type"] = "ladder"
    return ob


def main():
    print("\n" + "="*60)
    print("Sub3D — Submarine Ladder")
    print("="*60)

    ladder = build_ladder()

    print(f"  SM_Ladder: {len(ladder.data.vertices)}V")
    print(f"  Height: {LADDER_HEIGHT}cm, Width: {LADDER_WIDTH}cm")
    print(f"  Rungs: {N_RUNGS}, spacing: {RUNG_SPACING}cm")
    print(f"  Rail diameter: {RAIL_DIAMETER}cm, Rung diameter: {RUNG_DIAMETER}cm")
    print(f"\n  Place at hatch positions. Duplicate with Shift+D.")
    print(f"  Rotate 90° (R > X > 90) if needed for horizontal hatches.")

main()
