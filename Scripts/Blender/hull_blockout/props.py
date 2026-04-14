"""
Sub3D — Submarine Props Library
=================================
Standalone prop generator. Each prop is placed at origin, ready to duplicate.
Props are spaced along X for easy browsing.

Run independently: Blender > Scripting > Open > Alt+P
"""

import bpy
import math

TUBE_SEGS = 8
SPACING = 200  # Distance between props along X for display


def simple_mat(name, r, g, b, metal=0.5, rough=0.7):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
    return m


def make(name, verts, faces, color, metal=0.5, rough=0.7, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(simple_mat(name, *color, metal, rough))
    if smooth:
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        try:
            bpy.ops.object.shade_auto_smooth()
        except:
            for p in ob.data.polygons: p.use_smooth = True
        ob.select_set(False)
    return ob


def tube_vf(path, radius, segs=8):
    """Generate tube vertices+faces along a path."""
    v, f = [], []
    for pi, p in enumerate(path):
        if pi < len(path)-1:
            fw = tuple(path[pi+1][i]-p[i] for i in range(3))
        else:
            fw = tuple(p[i]-path[pi-1][i] for i in range(3))
        ln = math.sqrt(sum(c*c for c in fw))
        fw = tuple(c/ln for c in fw) if ln > 0.01 else (0,0,1)
        up = (0,0,1) if abs(fw[2]) < 0.9 else (1,0,0)
        rx = (fw[1]*up[2]-fw[2]*up[1], fw[2]*up[0]-fw[0]*up[2], fw[0]*up[1]-fw[1]*up[0])
        ln_r = math.sqrt(sum(c*c for c in rx))
        rx = tuple(c/ln_r for c in rx) if ln_r > 0.01 else (1,0,0)
        ry = (fw[1]*rx[2]-fw[2]*rx[1], fw[2]*rx[0]-fw[0]*rx[2], fw[0]*rx[1]-fw[1]*rx[0])
        for si in range(segs):
            a = 2*math.pi*si/segs
            ca, sa = math.cos(a), math.sin(a)
            v.append((p[0]+radius*(rx[0]*ca+ry[0]*sa),
                       p[1]+radius*(rx[1]*ca+ry[1]*sa),
                       p[2]+radius*(rx[2]*ca+ry[2]*sa)))
    for pi in range(len(path)-1):
        for si in range(segs):
            ni = (si+1)%segs
            a,b,c,d = pi*segs+si, pi*segs+ni, (pi+1)*segs+ni, (pi+1)*segs+si
            f.append((a,b,c,d))
    return v, f


def box_vf(cx, cy, cz, w, d, h):
    """Simple box centered at (cx, cy, cz+h/2)."""
    hw, hd, hh = w/2, d/2, h
    v = [(cx-hd,cy-hw,cz),(cx+hd,cy-hw,cz),(cx+hd,cy+hw,cz),(cx-hd,cy+hw,cz),
         (cx-hd,cy-hw,cz+hh),(cx+hd,cy-hw,cz+hh),(cx+hd,cy+hw,cz+hh),(cx-hd,cy+hw,cz+hh)]
    f = [(0,1,2,3),(4,7,6,5),(0,4,5,1),(2,6,7,3),(0,3,7,4),(1,5,6,2)]
    return v, f


def cylinder_vf(cx, cy, cz, radius, height, n=12):
    """Vertical cylinder."""
    v, f = [], []
    for iz in range(2):
        z = cz + height * iz
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((cx+radius*math.cos(a), cy+radius*math.sin(a), z))
    for i in range(n):
        ni = (i+1)%n
        f.append((i, ni, n+ni, n+i))
    # Top cap
    tc = len(v); v.append((cx, cy, cz+height))
    for i in range(n): f.append((tc, n+i, n+(i+1)%n))
    # Bottom cap
    bc = len(v); v.append((cx, cy, cz))
    for i in range(n): f.append((bc, (i+1)%n, i))
    return v, f


def merge_vf(all_v, all_f, new_v, new_f):
    """Merge new vertices/faces into existing lists."""
    base = len(all_v)
    all_v.extend(new_v)
    all_f.extend([tuple(i+base for i in face) for face in new_f])


# ═══════════════════════════════════════════════════════════════
# PROPS
# ═══════════════════════════════════════════════════════════════

def prop_ladder(x_offset=0):
    """Vertical ladder — 250cm, 8 rungs."""
    v, f = [], []
    hw = 22.5  # Half-width between rails
    h = 250
    # Rails
    for side in [-1, 1]:
        path = []
        for i in range(13):
            t = i/12
            z = t * h
            inward = 0
            if t > 0.85:
                inward = 8 * ((t-0.85)/0.15)**2 * side * -1
            path.append((x_offset, hw*side + inward, z))
        rv, rf = tube_vf(path, 2.5, 8)
        merge_vf(v, f, rv, rf)
    # Rungs
    for ri in range(8):
        z = 30 * (ri+1)
        if z > h-20: break
        path = [(x_offset, -hw, z), (x_offset, 0, z), (x_offset, hw, z)]
        rv, rf = tube_vf(path, 1.8, 6)
        merge_vf(v, f, rv, rf)
    return make("PROP_Ladder", v, f, (0.45, 0.42, 0.35), 0.8, 0.6)


def prop_valve_wheel(x_offset=0):
    """Valve wheel — round handle on a pipe stub."""
    v, f = [], []
    # Pipe stub (horizontal)
    path = [(x_offset, 0, 80), (x_offset, 0, 100), (x_offset, 0, 115)]
    rv, rf = tube_vf(path, 4, 8)
    merge_vf(v, f, rv, rf)
    # Wheel ring (torus-ish — 12 segments around a circle)
    wheel_r = 18
    tube_r = 2
    wheel_z = 118
    n_ring = 16
    n_tube = 6
    for i in range(n_ring):
        a = 2*math.pi*i/n_ring
        cx = x_offset + wheel_r * math.cos(a)
        cy = wheel_r * math.sin(a)
        for j in range(n_tube):
            ta = 2*math.pi*j/n_tube
            vx = cx + tube_r * math.cos(a) * math.cos(ta)
            vy = cy + tube_r * math.sin(a) * math.cos(ta)
            vz = wheel_z + tube_r * math.sin(ta)
            v.append((vx, vy, vz))
    base = len(v) - n_ring * n_tube
    for i in range(n_ring):
        ni = (i+1) % n_ring
        for j in range(n_tube):
            nj = (j+1) % n_tube
            a = base + i*n_tube + j
            b = base + i*n_tube + nj
            c = base + ni*n_tube + nj
            d = base + ni*n_tube + j
            f.append((a, b, c, d))
    # Spokes (4 tubes from center to ring)
    for spoke in range(4):
        sa = math.pi/2 * spoke
        path = [
            (x_offset, 0, wheel_z),
            (x_offset + wheel_r*0.5*math.cos(sa), wheel_r*0.5*math.sin(sa), wheel_z),
            (x_offset + wheel_r*math.cos(sa), wheel_r*math.sin(sa), wheel_z),
        ]
        rv, rf = tube_vf(path, 1.2, 6)
        merge_vf(v, f, rv, rf)
    return make("PROP_ValveWheel", v, f, (0.50, 0.45, 0.35), 0.85, 0.5)


def prop_pipe_horizontal(x_offset=0):
    """Horizontal pipe run — 200cm with elbow."""
    path = [
        (x_offset - 100, 0, 150),
        (x_offset - 30, 0, 150),
        (x_offset, 0, 150),
        (x_offset + 20, 0, 155),
        (x_offset + 30, 0, 170),
        (x_offset + 30, 0, 200),
        (x_offset + 30, 0, 250),
    ]
    v, f = tube_vf(path, 5, 10)
    return make("PROP_Pipe", v, f, (0.35, 0.30, 0.25), 0.7, 0.6)


def prop_pipe_vertical(x_offset=0):
    """Vertical pipe — floor to ceiling."""
    path = [(x_offset, 0, z) for z in range(0, 260, 20)]
    v, f = tube_vf(path, 4, 8)
    return make("PROP_PipeVertical", v, f, (0.32, 0.28, 0.24), 0.7, 0.6)


def prop_gauge_panel(x_offset=0):
    """Wall-mounted gauge panel — flat box with circular gauges."""
    v, f = [], []
    # Panel
    bv, bf = box_vf(x_offset, 0, 100, 80, 8, 60)
    merge_vf(v, f, bv, bf)
    # 3 gauge circles on front face
    for gi, gy in enumerate([-22, 0, 22]):
        gz = 130
        cv, cf = cylinder_vf(x_offset + 5, gy, gz - 8, 9, 3, 10)
        merge_vf(v, f, cv, cf)
    return make("PROP_GaugePanel", v, f, (0.25, 0.28, 0.25), 0.6, 0.8, smooth=False)


def prop_locker(x_offset=0):
    """Tall storage locker."""
    v, f = box_vf(x_offset, 0, 0, 50, 40, 180)
    ob = make("PROP_Locker", v, f, (0.35, 0.33, 0.28), 0.4, 0.8, smooth=False)
    # Handle
    hv, hf = box_vf(x_offset + 21, 8, 90, 3, 2, 20)
    base = len(v)
    v.extend(hv)
    f.extend([tuple(i+base for i in face) for face in hf])
    return ob


def prop_crate(x_offset=0):
    """Supply crate — small box."""
    v, f = box_vf(x_offset, 0, 0, 60, 45, 35)
    return make("PROP_Crate", v, f, (0.30, 0.25, 0.18), 0.2, 0.9, smooth=False)


def prop_fire_extinguisher(x_offset=0):
    """Wall-mounted fire extinguisher."""
    v, f = cylinder_vf(x_offset, 0, 20, 7, 45, 10)
    # Top nozzle
    nv, nf = cylinder_vf(x_offset, 0, 65, 3, 10, 6)
    merge_vf(v, f, nv, nf)
    return make("PROP_FireExt", v, f, (0.70, 0.15, 0.10), 0.6, 0.7)


def prop_bench(x_offset=0):
    """Simple metal bench / seat."""
    v, f = [], []
    # Seat surface
    bv, bf = box_vf(x_offset, 0, 42, 80, 35, 4)
    merge_vf(v, f, bv, bf)
    # 2 legs
    for ly in [-30, 30]:
        lv, lf = box_vf(x_offset, ly, 0, 5, 30, 42)
        merge_vf(v, f, lv, lf)
    return make("PROP_Bench", v, f, (0.35, 0.33, 0.30), 0.7, 0.7, smooth=False)


def prop_periscope(x_offset=0):
    """Periscope column — floor to ceiling."""
    v, f = [], []
    # Main column
    cv, cf = cylinder_vf(x_offset, 0, 0, 10, 280, 12)
    merge_vf(v, f, cv, cf)
    # Eyepiece housing
    ev, ef = box_vf(x_offset, 0, 150, 25, 35, 20)
    merge_vf(v, f, ev, ef)
    # Handles
    for side in [-1, 1]:
        path = [(x_offset, side*18, 148), (x_offset, side*22, 155), (x_offset, side*22, 165), (x_offset, side*18, 170)]
        hv, hf = tube_vf(path, 1.5, 6)
        merge_vf(v, f, hv, hf)
    return make("PROP_Periscope", v, f, (0.30, 0.30, 0.30), 0.8, 0.5)


def prop_torpedo(x_offset=0):
    """Torpedo — cylindrical body with conical nose."""
    v, f = [], []
    n = 12
    body_r = 27
    body_l = 280
    # Body cylinder
    for iz in range(2):
        x = x_offset + 50 + body_l * iz
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((x, body_r*math.cos(a), 30 + body_r*math.sin(a)))
    for i in range(n):
        ni = (i+1)%n
        f.append((i, ni, n+ni, n+i))
    # Nose cone
    tip = len(v); v.append((x_offset, 0, 30))
    for i in range(n): f.append((tip, (i+1)%n, i))
    # Tail cap
    tc = len(v); v.append((x_offset + 50 + body_l + 20, 0, 30))
    for i in range(n): f.append((tc, n+i, n+(i+1)%n))
    # 4 tail fins
    fin_x = x_offset + 50 + body_l
    for fa in [0, 90, 180, 270]:
        a = math.radians(fa)
        ca, sa = math.cos(a), math.sin(a)
        bv = [
            (fin_x, body_r*ca, 30+body_r*sa),
            (fin_x+30, body_r*ca, 30+body_r*sa),
            (fin_x+40, (body_r+20)*ca, 30+(body_r+20)*sa),
            (fin_x-5, (body_r+15)*ca, 30+(body_r+15)*sa),
        ]
        base = len(v)
        v.extend(bv)
        f.append((base, base+1, base+2, base+3))
    return make("PROP_Torpedo", v, f, (0.20, 0.25, 0.18), 0.7, 0.6)


def prop_watertight_door(x_offset=0):
    """Round watertight door with volant."""
    v, f = [], []
    r = 85
    n = 20
    # Disc
    for dx in [4, -4]:
        base = len(v)
        center = len(v); v.append((x_offset+dx, 0, r+10))
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((x_offset+dx, r*math.cos(a), r+10+r*math.sin(a)))
        for i in range(n):
            ni = (i+1)%n
            if dx > 0: f.append((center, center+1+i, center+1+ni))
            else: f.append((center, center+1+ni, center+1+i))
    # Rim
    for i in range(n):
        ni = (i+1)%n
        f.append((1+i, 1+ni, n+2+ni, n+2+i))
    # Volant (simple cross)
    vr = 25
    for spoke in range(4):
        sa = math.pi/2 * spoke
        path = [
            (x_offset+6, 0, r+10),
            (x_offset+6, vr*math.cos(sa), r+10+vr*math.sin(sa)),
        ]
        sv, sf = tube_vf(path, 2, 6)
        merge_vf(v, f, sv, sf)
    return make("PROP_Door_WT", v, f, (0.50, 0.45, 0.38), 0.8, 0.5)


# ═══════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════

def main():
    print("\n" + "="*60)
    print("Sub3D — Submarine Props Library")
    print("="*60)

    # Clear scene
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)

    # Scene setup
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 1; sp.clip_end = 50000

    # Generate all props, spaced along X
    props = [
        ("Ladder",           prop_ladder),
        ("Valve Wheel",      prop_valve_wheel),
        ("Pipe Horizontal",  prop_pipe_horizontal),
        ("Pipe Vertical",    prop_pipe_vertical),
        ("Gauge Panel",      prop_gauge_panel),
        ("Locker",           prop_locker),
        ("Crate",            prop_crate),
        ("Fire Extinguisher", prop_fire_extinguisher),
        ("Bench",            prop_bench),
        ("Periscope",        prop_periscope),
        ("Torpedo",          prop_torpedo),
        ("Watertight Door",  prop_watertight_door),
    ]

    for i, (label, func) in enumerate(props):
        x = i * SPACING
        ob = func(x)
        print(f"  {ob.name:25s} X={x:5.0f}  [{label}]")

    # Frame view
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print(f"\n  {len(props)} props generated, spaced {SPACING}cm apart.")
    print("  Copy into submarine scene: select prop > Ctrl+C > switch to sub scene > Ctrl+V")
    print("  Or append from this .blend file.")

main()
