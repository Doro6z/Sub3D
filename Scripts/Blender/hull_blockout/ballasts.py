"""
Sub3D — Ballast Tanks (lateral, long)
======================================
Two long tanks along the inner flanks of the hull, under the main deck.
Visible from the lower deck as curved walls following the hull shape.
Run AFTER main.py (additive).

Blender > Scripting > Open > Alt+P
"""

import bpy
import math
from mathutils import Vector

LENGTH = 4200.0
DECK_MAIN_Z = -20.0
DECK_LOWER_Z = -240.0

# Ballast tank dimensions
BALLAST_X_START = 0.12   # Start (forward)
BALLAST_X_END = 0.85     # End (aft) — spans most of the hull
BALLAST_INSET = 30.0     # How far inside the hull the inner wall sits
BALLAST_HEIGHT_TOP = DECK_MAIN_Z  # Top of tank = main deck
BALLAST_HEIGHT_BOT = DECK_LOWER_Z + 40  # Bottom of tank = above lower deck floor

# Hull curve (same as main.py)
HULL_CURVE = [
    (0.000,  50), (0.010, 140), (0.025, 240), (0.050, 330),
    (0.080, 380), (0.110, 400), (0.150, 415), (0.200, 425),
    (0.280, 430), (0.380, 430), (0.480, 425), (0.560, 415),
    (0.640, 400), (0.720, 375), (0.780, 345), (0.830, 305),
    (0.870, 260), (0.910, 210), (0.940, 160), (0.960, 120),
    (0.980, 80), (0.995, 50), (1.000, 40),
]

def _build_spline(points):
    n = len(points) - 1
    x = [p[0] for p in points]; y = [p[1] for p in points]
    h = [x[i+1]-x[i] for i in range(n)]
    alpha = [0]*(n+1)
    for i in range(1, n):
        alpha[i] = (3/h[i])*(y[i+1]-y[i]) - (3/h[i-1])*(y[i]-y[i-1])
    l = [1]+[0]*n; mu = [0]*(n+1); z = [0]*(n+1)
    for i in range(1, n):
        l[i] = 2*(x[i+1]-x[i-1]) - h[i-1]*mu[i-1]
        mu[i] = h[i]/l[i]; z[i] = (alpha[i]-h[i-1]*z[i-1])/l[i]
    l[n] = 1; z[n] = 0
    b = [0]*n; c = [0]*(n+1); d = [0]*n
    for j in range(n-1, -1, -1):
        c[j] = z[j]-mu[j]*c[j+1]; b[j] = (y[j+1]-y[j])/h[j]-h[j]*(c[j+1]+2*c[j])/3
        d[j] = (c[j+1]-c[j])/(3*h[j])
    return x, y, b, c, d

_sp = _build_spline(HULL_CURVE)

def hull_r(nx):
    nx = max(0, min(1, nx))
    sx, sy, sb, sc, sd = _sp
    i = 0
    for j in range(len(sx)-1):
        if sx[j] <= nx <= sx[j+1]: i = j; break
        i = j
    dx = nx - sx[i]
    return max(0, sy[i]+sb[i]*dx+sc[i]*dx*dx+sd[i]*dx*dx*dx)


def simple_mat(name, r, g, b):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = 0.75
    return m


def build_ballast_tank(name, side):
    """
    Build one ballast tank as a curved wall following the hull interior.
    side: 1 = starboard, -1 = port
    """
    v = []
    f = []
    nx_segs = 40
    nz_segs = 8

    xs = BALLAST_X_START * LENGTH
    xe = BALLAST_X_END * LENGTH

    for ix in range(nx_segs + 1):
        t = ix / nx_segs
        x = xs + (xe - xs) * t
        nx = x / LENGTH
        r = hull_r(nx)

        for iz in range(nz_segs + 1):
            tz = iz / nz_segs
            z = BALLAST_HEIGHT_BOT + (BALLAST_HEIGHT_TOP - BALLAST_HEIGHT_BOT) * tz

            # Hull inner wall Y at this Z
            if abs(z) < r:
                hull_y = math.sqrt(max(0, r*r - z*z))
            else:
                hull_y = 0

            # Outer face (hull wall) — follows hull curvature
            y_outer = (hull_y - 5) * side  # 5cm inside hull surface

            # Inner face (ballast wall) — flat-ish, inset from hull
            y_inner = (hull_y - BALLAST_INSET) * side

            v.append((x, y_outer, z))
            v.append((x, y_inner, z))

    ring = (nz_segs + 1) * 2

    # Outer wall faces
    for ix in range(nx_segs):
        for iz in range(nz_segs):
            base = ix * ring + iz * 2
            next_x = (ix + 1) * ring + iz * 2
            # Outer wall (hull side)
            if side > 0:
                f.append((base, next_x, next_x + 2, base + 2))
            else:
                f.append((base, base + 2, next_x + 2, next_x))
            # Inner wall (compartment side)
            if side > 0:
                f.append((base + 1, base + 3, next_x + 3, next_x + 1))
            else:
                f.append((base + 1, next_x + 1, next_x + 3, base + 3))

    # Top cap (connects outer to inner at top)
    for ix in range(nx_segs):
        top_iz = nz_segs
        base = ix * ring + top_iz * 2
        next_x = (ix + 1) * ring + top_iz * 2
        f.append((base, base + 1, next_x + 1, next_x))

    # Bottom cap
    for ix in range(nx_segs):
        base = ix * ring
        next_x = (ix + 1) * ring
        f.append((base, next_x, next_x + 1, base + 1))

    me = bpy.data.meshes.new(name)
    me.from_pydata(v, [], f)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(simple_mat(name, 0.30, 0.25, 0.20))

    # Auto smooth
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    try:
        bpy.ops.object.shade_auto_smooth()
    except:
        for p in ob.data.polygons:
            p.use_smooth = True
    ob.select_set(False)

    # Custom property
    ob["type"] = "ballast_tank"
    return ob


def main():
    print("\n" + "="*60)
    print("Sub3D — Ballast Tanks (lateral)")
    print("="*60)

    build_ballast_tank("SM_Ballast_Port", -1)
    build_ballast_tank("SM_Ballast_Stbd", 1)

    print("  SM_Ballast_Port: lateral tank, port side")
    print("  SM_Ballast_Stbd: lateral tank, starboard side")
    print(f"  X range: [{BALLAST_X_START*LENGTH:.0f}, {BALLAST_X_END*LENGTH:.0f}]")
    print(f"  Z range: [{BALLAST_HEIGHT_BOT:.0f}, {BALLAST_HEIGHT_TOP:.0f}]")
    print(f"  Inset: {BALLAST_INSET}cm from hull wall")

main()
