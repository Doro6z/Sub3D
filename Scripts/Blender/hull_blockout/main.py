"""
Sub3D — Type 212A Minimal (Blender 5.0 safe)
==============================================
Hull + superstructure + decks + bulkheads ONLY.
No solidify applied. No edit mode. No complex geometry.
Doors, hatches, fins, turrets = add manually in Blender.

Run: Blender > Scripting > Open > Alt+P
"""

import bpy
import math
from mathutils import Vector

LENGTH = 4200.0
HULL_THICK = 15.0
DECK_THICK = 18.0
BH_THICK = 14.0
RADIAL = 64
N_RINGS = 300

# Hull curve — Albacore-inspired: fat rounded bow, gentle teardrop, blunt stern
HULL_CURVE = [
    (0.000,  50),     # Bow — fatter start (was 30)
    (0.010, 140),     # Opens faster
    (0.025, 240),     # Bigger nose
    (0.050, 330),     # Already wide at 5%
    (0.080, 380),
    (0.110, 400),
    (0.150, 415),
    (0.200, 425),     # Near peak
    (0.280, 430),     # Peak beam — broad plateau
    (0.380, 430),     # Sustained max
    (0.480, 425),
    (0.560, 415),     # Very gradual taper
    (0.640, 400),
    (0.720, 375),
    (0.780, 345),
    (0.830, 305),
    (0.870, 260),
    (0.910, 210),
    (0.940, 160),
    (0.960, 120),
    (0.980, 80),
    (0.995, 50),
    (1.000, 40),      # Stern NOT a point — blunt truncation
]

DECK_MAIN_Z = -20.0
DECK_LOWER_Z = -240.0          # -40cm (était -200)

SUPER_X_START = 0.16
SUPER_X_END = 0.72
SUPER_WIDTH = 480.0
SUPER_HEIGHT = 200.0           # Abaissé (était 350)

BULKHEAD_X = [0.15, 0.40, 0.62, 0.82]
BULKHEAD_LOWER_X = [0.35, 0.70]
DOOR_W = 85.0
DOOR_H = 175.0
DOOR_LOWER_W = 70.0
DOOR_LOWER_H = 145.0

# ─── Math ───

def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)

# Pre-compute cubic spline for hull curve (C2 continuous — no visible rings)
def _build_cubic_spline(points):
    """Build natural cubic spline coefficients for smooth interpolation."""
    n = len(points) - 1
    x = [p[0] for p in points]
    y = [p[1] for p in points]

    h = [x[i+1] - x[i] for i in range(n)]
    alpha = [0] * (n + 1)
    for i in range(1, n):
        alpha[i] = (3/h[i])*(y[i+1]-y[i]) - (3/h[i-1])*(y[i]-y[i-1])

    l = [1] + [0]*n
    mu = [0] * (n + 1)
    z = [0] * (n + 1)

    for i in range(1, n):
        l[i] = 2*(x[i+1]-x[i-1]) - h[i-1]*mu[i-1]
        mu[i] = h[i] / l[i]
        z[i] = (alpha[i] - h[i-1]*z[i-1]) / l[i]

    l[n] = 1
    z[n] = 0

    b = [0] * n
    c = [0] * (n + 1)
    d = [0] * n

    for j in range(n-1, -1, -1):
        c[j] = z[j] - mu[j]*c[j+1]
        b[j] = (y[j+1]-y[j])/h[j] - h[j]*(c[j+1]+2*c[j])/3
        d[j] = (c[j+1]-c[j]) / (3*h[j])

    return x, y, b, c, d

_spline = _build_cubic_spline(HULL_CURVE)

def sample_curve(nx):
    """Sample hull radius using cubic spline (C2 smooth — no ring artifacts)."""
    nx = max(0.0, min(1.0, nx))
    sx, sy, sb, sc, sd = _spline
    n = len(sx) - 1

    # Find segment
    i = 0
    for j in range(n):
        if sx[j] <= nx <= sx[j+1]:
            i = j
            break
        i = j

    dx = nx - sx[i]
    result = sy[i] + sb[i]*dx + sc[i]*dx*dx + sd[i]*dx*dx*dx
    return max(0, result)

def interior_hw(x_cm, z):
    nx = x_cm / LENGTH
    r = max(0, sample_curve(nx) - HULL_THICK - 5)
    main_r = sample_curve(nx)
    if SUPER_X_START <= nx <= SUPER_X_END:
        t = (nx-SUPER_X_START)/(SUPER_X_END-SUPER_X_START)
        sb = smoothstep(t/0.20) if t < 0.20 else (smoothstep((1-t)/0.05) if t > 0.95 else 1.0)
        if sb > 0 and z >= main_r * 0.5:
            sw = (SUPER_WIDTH/2 - HULL_THICK - 5) * sb
            if z <= main_r:
                bt = (z - main_r*0.5) / (main_r*0.5)
                hh = math.sqrt(max(0, r*r - z*z)) if abs(z) < r else 0
                return max(hh, hh + (sw-hh)*smoothstep(bt))
            else:
                dz = z - main_r; sh = SUPER_HEIGHT * sb
                return sw * math.sqrt(max(0, 1-(dz/sh)**2)) if dz < sh else 0
    if r <= 0 or abs(z) >= r: return 0
    return math.sqrt(max(0, r*r - z*z))

# ─── Blender helpers (NO edit mode, NO modifier_apply) ───

def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)

def simple_mat(name, r, g, b):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = 0.7
    return m

def make(name, verts, faces, r, g, b, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(simple_mat(name, r, g, b))
    if smooth:
        # Blender 5.0: use shade_auto_smooth on the object for proper interpolation
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        try:
            bpy.ops.object.shade_auto_smooth()
        except:
            # Fallback for older Blender
            for p in ob.data.polygons:
                p.use_smooth = True
        ob.select_set(False)
    return ob

def add_solidify(ob, thick, offset=-1):
    """Add solidify as LIVE modifier (not applied)."""
    mod = ob.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = thick
    mod.offset = offset

# ─── Hull with integrated superstructure ───

def hull_ring(nx):
    r = sample_curve(nx)
    if r < 0.3: r = 0.3

    # Béluga nose: minimal center shift, more top stretch
    z_offset = 0.0
    bow_stretch_top = 1.0
    bow_squash_bot = 1.0
    if nx < 0.25:
        bow_t = 1.0 - nx / 0.25
        z_offset = -r * 0.08 * bow_t  # Very slight center drop (was 0.25 — too much)
        bow_stretch_top = 1.0 + 0.30 * bow_t  # Stretch top MORE (forehead effect)
        bow_squash_bot = 1.0 - 0.10 * bow_t  # Squash bottom slightly (flatter keel)

    # Ballast bulges: saddle tanks on the lower sides of the hull
    # Active in the body zone (15% to 85%), fading at ends
    ballast_blend = 0.0
    if 0.10 < nx < 0.88:
        if nx < 0.18:
            ballast_blend = (nx - 0.10) / 0.08
        elif nx > 0.82:
            ballast_blend = (0.88 - nx) / 0.06
        else:
            ballast_blend = 1.0
        ballast_blend = min(1.0, max(0.0, ballast_blend))

    sb = 0.0
    super_h_local = SUPER_HEIGHT
    if SUPER_X_START <= nx <= SUPER_X_END:
        t = (nx-SUPER_X_START)/(SUPER_X_END-SUPER_X_START)
        # Smooth blend: 40% taper in, 3% taper out (abrupt stern = flat wall for door)
        sb = smoothstep(t/0.40) if t < 0.40 else (smoothstep((1-t)/0.03) if t > 0.97 else 1.0)
        # Avant plus bas, arrière plus haut (slope)
        slope = 0.75 + 0.25 * t  # 75% height at front, 100% at back
        super_h_local = SUPER_HEIGHT * slope
    pts = []
    super_hw = SUPER_WIDTH / 2
    for s in range(RADIAL):
        a = 2*math.pi*s/RADIAL
        ca, sa = math.cos(a), math.sin(a)
        # Apply béluga: stretch top, squash bottom, slight offset
        z_raw = r * sa
        if sa > 0:
            z_raw *= bow_stretch_top
        else:
            z_raw *= bow_squash_bot

        # Apply ballast bulge on lower sides
        ballast_y_extra = 0.0
        if ballast_blend > 0 and sa < -0.1 and abs(ca) > 0.2:
            # Bulge profile: peaks at ~45° below equator (sa ≈ -0.7, ca ≈ ±0.7)
            # Shaped like a smooth bump on each side
            side_factor = abs(ca)  # 0 at top/bottom, 1 at sides
            depth_factor = max(0, -sa - 0.1) / 0.9  # 0 at equator, ~1 at bottom
            # Bump shape: sine curve, peaks at mid-depth
            bump = math.sin(depth_factor * math.pi) * side_factor
            ballast_y_extra = r * 0.12 * bump * ballast_blend  # 12% of radius max

        by = r * ca + math.copysign(ballast_y_extra, ca)
        bz = z_raw + z_offset

        if sb > 0 and sa > 0:
            # Single continuous curve from equator to super top
            # t: 0 at equator, 1 at top
            t = sa

            # Width: smooth S-curve with subtle concavity
            # Concave dip peaks around t=0.3, fades to zero at top
            concave = 0.05 * math.sin(t * math.pi) * (1.0 - t * t) * sb
            # Width blend: very slow start (cubic), accelerates toward top
            wb = t * t * t
            hull_w = abs(by) * (1.0 - concave)
            super_w = super_hw * abs(ca)
            final_w = hull_w + (super_w - hull_w) * wb * sb
            sgn = 1.0 if ca >= 0 else -1.0
            y = sgn * final_w

            # Height: quadratic blend to super top
            hb = t * t
            z = bz + super_h_local * hb * sb

            pts.append((y, z))
        else:
            pts.append((by, bz))
    return pts

def build_hull():
    v, f = [], []
    for i in range(N_RINGS+1):
        nx = i/N_RINGS; x = nx*LENGTH
        for y, z in hull_ring(nx):
            v.append((x, y, z))
    for i in range(N_RINGS):
        for s in range(RADIAL):
            ns = (s+1)%RADIAL
            a,b,c,d = i*RADIAL+s, i*RADIAL+ns, (i+1)*RADIAL+ns, (i+1)*RADIAL+s
            f.append((a,b,c,d))
    bc = len(v); v.append((0,0,0))
    for s in range(RADIAL): f.append((bc, (s+1)%RADIAL, s))
    sc = len(v); v.append((LENGTH,0,0)); lb = N_RINGS*RADIAL
    for s in range(RADIAL): f.append((sc, lb+s, lb+(s+1)%RADIAL))
    ob = make("SM_Hull", v, f, 0.14, 0.18, 0.16)
    # PAS de solidify sur la coque — c'est ça qui crée les anneaux visibles
    # L'épaisseur sera gérée par la collision dans UE
    return ob

# ─── Decks ───

def build_deck(name, z, xs_n, xe_n, r, g, b):
    xs, xe = xs_n*LENGTH, xe_n*LENGTH
    nx = max(8, int((xe-xs)/60)); ny = 16
    v, f, grid, vi = [], [], {}, 0
    for ix in range(nx+1):
        x = xs + (xe-xs)*ix/nx
        hw = interior_hw(x, z)
        if hw < 15: continue
        for iy in range(ny+1):
            y = -hw + 2*hw*iy/ny
            v.append((x, y, z)); grid[(ix,iy)] = vi; vi += 1
    for ix in range(nx):
        for iy in range(ny):
            ids = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
            if all(i is not None for i in ids): f.append(tuple(ids))
    if not v: return None
    ob = make(f"SM_Deck_{name}", v, f, r, g, b, smooth=False)
    add_solidify(ob, -DECK_THICK, 0)
    return ob

# ─── Bulkheads ───

def build_bh(name, x_norm, dw, dh, z_min, z_max, dsill):
    x = x_norm * LENGTH
    gy, gz = 20, 16
    v, f, g, vi = [], [], {}, 0
    for iz in range(gz+1):
        z = z_min + (z_max-z_min)*iz/gz
        hw = interior_hw(x, z)
        if hw < 5: continue
        for iy in range(gy+1):
            y = -hw + 2*hw*iy/gy
            if abs(y) < dw/2 and dsill < z < dsill + dh:
                continue
            v.append((x,y,z)); g[(iy,iz)] = vi; vi += 1
    for iz in range(gz):
        for iy in range(gy):
            ids = [g.get(k) for k in [(iy,iz),(iy+1,iz),(iy+1,iz+1),(iy,iz+1)]]
            if all(i is not None for i in ids): f.append(tuple(ids))
    if not v: return None
    ob = make(name, v, f, 0.44, 0.42, 0.38, smooth=False)
    add_solidify(ob, BH_THICK, 0)
    return ob

# ─── Hydroplanes (4 simple quads) ───

def build_hydroplanes():
    objs = []
    for name, xn, span, chord in [("Bow", 0.12, 180, 130), ("Stern", 0.88, 180, 130)]:
        x = xn*LENGTH; r = sample_curve(xn); hc = chord/2
        for side, lb in [(1,"Stbd"),(-1,"Port")]:
            v = [(x-hc, r*side, 0), (x+hc, r*side, 0),
                 (x+hc*0.5, (r+span)*side, 0), (x-hc*0.4, (r+span)*side, 0)]
            ob = make(f"SM_Hydro_{name}_{lb}", v, [(0,1,2,3)], 0.16, 0.20, 0.18, smooth=False)
            add_solidify(ob, 10, 0)
            objs.append(ob)
    return objs

# ─── Tail fins (X-shape, 4 quads) ───

def build_fins():
    objs = []
    fx = 0.955*LENGTH; r = sample_curve(0.955); hc = 90
    for ad in [45, 135, 225, 315]:
        a = math.radians(ad); ca, sa = math.cos(a), math.sin(a)
        span = 250
        v = [(fx-hc, r*ca, r*sa), (fx+hc, r*ca, r*sa),
             (fx+hc*0.6, (r+span)*ca, (r+span)*sa),
             (fx-hc*0.3, (r+span)*ca, (r+span)*sa)]
        ob = make(f"SM_Fin_{ad}", v, [(0,1,2,3)], 0.16, 0.20, 0.18, smooth=False)
        add_solidify(ob, 12, 0)
        objs.append(ob)
    return objs

# ─── Ducted Propulsor (anneau + hub + 7 pales) ───

def build_propulsor():
    objs = []
    x = LENGTH
    stern_r = sample_curve(1.0)  # Hull radius at stern
    duct_r = stern_r * 1.8      # Duct outer radius (bigger than hull stern)
    duct_inner = duct_r - 20    # Duct inner radius
    duct_len = 80               # Duct length along X
    hub_r = 30
    n = 16                      # Ring segments
    n_blades = 7
    blade_r = duct_inner - 10   # Blades fit inside duct

    # ── Duct ring (torus-like: 2 rings outer + 2 rings inner) ──
    v, f = [], []
    for iz in range(2):
        xx = x + duct_len * iz
        # Outer ring
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((xx, duct_r*math.cos(a), duct_r*math.sin(a)))
    # Outer wall
    for i in range(n):
        ni = (i+1)%n
        f.append((i, ni, n+ni, n+i))
    # Inner ring
    inner_base = len(v)
    for iz in range(2):
        xx = x + duct_len * iz
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((xx, duct_inner*math.cos(a), duct_inner*math.sin(a)))
    # Inner wall
    for i in range(n):
        ni = (i+1)%n
        f.append((inner_base+i, inner_base+n+i, inner_base+n+ni, inner_base+ni))
    # Front lip (connect outer front to inner front)
    for i in range(n):
        ni = (i+1)%n
        f.append((i, inner_base+i, inner_base+ni, ni))
    # Back lip (connect outer back to inner back)
    for i in range(n):
        ni = (i+1)%n
        f.append((n+i, n+ni, inner_base+n+ni, inner_base+n+i))

    objs.append(make("SM_Duct", v, f, 0.20, 0.22, 0.20))

    # ── Hub (cone in the center) ──
    v2, f2 = [], []
    hub_n = 10
    for iz in range(3):
        t = iz/2
        xx = x + duct_len*0.2 + duct_len*0.8*t
        cr = hub_r * (1 - 0.3*t)  # Tapered
        for i in range(hub_n):
            a = 2*math.pi*i/hub_n
            v2.append((xx, cr*math.cos(a), cr*math.sin(a)))
    for iz in range(2):
        for i in range(hub_n):
            ni = (i+1)%hub_n
            f2.append((iz*hub_n+i, iz*hub_n+ni, (iz+1)*hub_n+ni, (iz+1)*hub_n+i))
    # Tip
    tc = len(v2); v2.append((x + duct_len, 0, 0))
    for i in range(hub_n):
        f2.append((tc, 2*hub_n+i, 2*hub_n+(i+1)%hub_n))
    # Front cap
    fc = len(v2); v2.append((x + duct_len*0.2, 0, 0))
    for i in range(hub_n):
        f2.append((fc, (i+1)%hub_n, i))

    objs.append(make("SM_Hub", v2, f2, 0.30, 0.28, 0.25))

    # ── 7 Blades (curved quads from hub to duct inner) ──
    blade_x_center = x + duct_len * 0.5
    for b in range(n_blades):
        ba = 2*math.pi*b/n_blades
        cb, sb = math.cos(ba), math.sin(ba)
        # Each blade: 2 strips (inner→mid, mid→outer) with slight twist
        twist = 15  # Twist offset in cm along X
        bv = [
            # Inner edge (at hub)
            (blade_x_center + twist*0.5, hub_r*1.2*cb, hub_r*1.2*sb),
            (blade_x_center - twist*0.5, hub_r*1.2*cb, hub_r*1.2*sb),
            # Mid
            (blade_x_center + twist*0.3, blade_r*0.6*cb, blade_r*0.6*sb),
            (blade_x_center - twist*0.3, blade_r*0.6*cb, blade_r*0.6*sb),
            # Outer edge (near duct)
            (blade_x_center + twist*0.1, blade_r*cb, blade_r*sb),
            (blade_x_center - twist*0.1, blade_r*cb, blade_r*sb),
        ]
        bf = [(0,1,3,2), (2,3,5,4)]
        ob = make(f"SM_Blade_{b}", bv, bf, 0.35, 0.28, 0.22, smooth=False)
        add_solidify(ob, 6, 0)
        objs.append(ob)

    return objs

# (Super exit removed — the superstructure stern opening is formed
#  by the hull shape itself. Add door battants manually in Blender.)

# ─── Main ───

def main():
    print("\n" + "="*60)
    print("Sub3D Type 212A — Minimal Safe (Blender 5.0)")
    print("="*60)

    clear()
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 1; sp.clip_end = 500000

    print("--- Hull ---")
    build_hull()

    print("--- Decks ---")
    build_deck("main", DECK_MAIN_Z, 0.08, 0.93, 0.36, 0.34, 0.30)
    build_deck("lower", DECK_LOWER_Z, 0.15, 0.82, 0.28, 0.27, 0.25)
    upper_z = sample_curve(0.40) - 150  # Descendu de 150cm
    build_deck("upper", upper_z, SUPER_X_START+0.04, SUPER_X_END-0.03, 0.40, 0.38, 0.34)

    print("--- Bulkheads main ---")
    zt = DECK_MAIN_Z + 220
    for i, bx in enumerate(BULKHEAD_X):
        build_bh(f"SM_BH_{i}", bx, DOOR_W, DOOR_H, DECK_MAIN_Z, zt, DECK_MAIN_Z)

    print("--- Bulkheads lower ---")
    for i, bx in enumerate(BULKHEAD_LOWER_X):
        build_bh(f"SM_BH_Low_{i}", bx, DOOR_LOWER_W, DOOR_LOWER_H, DECK_LOWER_Z, DECK_MAIN_Z, DECK_LOWER_Z)

    print("--- Hydroplanes ---")
    build_hydroplanes()

    print("--- Tail fins ---")
    build_fins()

    print("--- Ducted Propulsor ---")
    build_propulsor()

    # Super exit removed — add door battants manually in Blender

    # Frame
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "="*60)
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type == 'MESH':
            print(f"  {ob.name}: {len(ob.data.vertices)}V")
    print("="*60)
    print("\nSolidify modifiers are LIVE (not applied).")
    print("Apply manually: select object > Ctrl+A > Visual Geometry to Mesh")
    print("Or before export: select all > Ctrl+A")
    print("\nAdd manually in Blender:")
    print("  - Fins (Shift+A > Mesh > Plane, extrude)")
    print("  - Doors (Shift+A > Mesh > Cylinder)")
    print("  - Hatches (Shift+A > Mesh > Cube)")
    print("  - Propeller, turrets, masts")

main()
