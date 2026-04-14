"""
Sub3D — Semi-Realistic Captain Character
==========================================
Smoother, more organic proportions. Subdivision-friendly topology.
~180cm tall. Standalone script.

Blender > Scripting > Open > Alt+P
"""

import bpy
import math
from mathutils import Vector

# ═══════════════════════════════════════════════════════════════
# CHARACTER PROFILE — Cross-sections at different heights
# ═══════════════════════════════════════════════════════════════
# Same approach as the submarine: define the body as cross-sections
# at key heights, then loft between them.
# Each section: (z_height, front_depth, back_depth, half_width)
# The body is symmetric left-right.

BODY_SECTIONS = [
    # z,    front, back,  half_w   — cross-section ellipse radii
    (0,     14,    10,    5),      # Feet base
    (5,     14,    10,    6),      # Ankle
    (22,    6,     5,     5),      # Boot top / ankle
    (35,    7,     6,     6),      # Calf bottom
    (55,    8,     7,     7),      # Calf mid — widest
    (70,    7,     7,     6.5),    # Knee area
    (75,    7.5,   7,     7),      # Above knee
    (90,    9,     9,     8),      # Thigh
    (97,    10,    10,    10),     # Upper thigh / crotch
]

TORSO_SECTIONS = [
    # z,    front, back,  half_w
    (97,    12,    10,    16),     # Hip
    (100,   12,    10,    16.5),   # Belt line
    (108,   13,    10,    16),     # Waist
    (118,   14,    11,    17),     # Lower ribcage
    (130,   14,    12,    19),     # Chest
    (140,   13,    12,    21),     # Upper chest — widest
    (148,   12,    11,    22),     # Shoulders
    (152,   10,    10,    23),     # Shoulder top
    (155,   8,     8,     20),     # Neck base
]

HEAD_SECTIONS = [
    # z,    front, back,  half_w
    (155,   5,     5,     5.5),    # Neck bottom
    (159,   5.5,   5,     6),      # Neck mid
    (163,   6,     5.5,   6.5),    # Neck top / chin line
    (166,   9,     8,     8),      # Jaw
    (170,   10,    9,     9),      # Mouth level
    (174,   10,    9.5,   9.5),    # Cheek / nose
    (178,   9,     10,    9),      # Eye level
    (182,   8,     10,    8.5),    # Forehead
    (186,   7,     9,     8),      # Crown
    (189,   5,     7,     6),      # Top of head
    (191,   3,     4,     4),      # Top
]

N_AROUND = 12  # Points around each cross-section


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def section_to_ring(z, front, back, half_w, n=12):
    """Generate a ring of points for one cross-section.
    Egg-shaped: wider at sides, front/back can differ."""
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        ca, sa = math.cos(a), math.sin(a)
        # Y = lateral (sa), X = front/back (ca)
        # Front half (ca > 0) uses front radius, back half uses back
        if ca >= 0:
            depth = front
        else:
            depth = back
        x = depth * ca
        y = half_w * sa
        pts.append((x, y, z))
    return pts


def loft_sections(sections, n_around=12, n_interp=3):
    """Loft between body sections with interpolation."""
    all_rings = []

    for i in range(len(sections)):
        z, fr, bk, hw = sections[i]
        ring = section_to_ring(z, fr, bk, hw, n_around)
        all_rings.append(ring)

        # Interpolate to next section
        if i < len(sections) - 1:
            z2, fr2, bk2, hw2 = sections[i + 1]
            for j in range(1, n_interp):
                t = j / n_interp
                t = smoothstep(t)
                iz = z + (z2 - z) * t
                ifr = fr + (fr2 - fr) * t
                ibk = bk + (bk2 - bk) * t
                ihw = hw + (hw2 - hw) * t
                ring = section_to_ring(iz, ifr, ibk, ihw, n_around)
                all_rings.append(ring)

    # Build mesh
    v = []
    f = []
    ring_size = n_around

    for ring in all_rings:
        for pt in ring:
            v.append(pt)

    for ri in range(len(all_rings) - 1):
        for si in range(ring_size):
            ni = (si + 1) % ring_size
            a = ri * ring_size + si
            b = ri * ring_size + ni
            c = (ri + 1) * ring_size + ni
            d = (ri + 1) * ring_size + si
            f.append((a, b, c, d))

    # Top cap
    last_base = (len(all_rings) - 1) * ring_size
    tc = len(v)
    last_z = all_rings[-1][0][2]
    v.append((0, 0, last_z + 2))
    for i in range(ring_size):
        f.append((tc, last_base + i, last_base + (i + 1) % ring_size))

    # Bottom cap
    bc = len(v)
    first_z = all_rings[0][0][2]
    v.append((0, 0, first_z))
    for i in range(ring_size):
        f.append((bc, (i + 1) % ring_size, i))

    return v, f


def build_arm(shoulder_x, shoulder_y, shoulder_z, side, n=8):
    """Build one arm as lofted sections. Side: 1=right, -1=left."""
    # Arm hangs slightly out and forward
    angle_out = math.radians(15 * side)
    angle_fwd = math.radians(5)

    sections = []
    arm_length = 58  # Total arm length

    for i in range(10):
        t = i / 9
        z = shoulder_z - arm_length * t

        # Radius varies: shoulder thick, elbow thinner, forearm, wrist thin
        if t < 0.15:
            r = 5.5 + t * 5  # Deltoid
        elif t < 0.45:
            r = 5.5 - (t - 0.15) * 4  # Upper arm taper
        elif t < 0.55:
            r = 4.5  # Elbow
        elif t < 0.85:
            r = 4.8 - (t - 0.55) * 3  # Forearm taper
        else:
            r = 3.5  # Wrist

        # Position offset from shoulder
        dy = math.sin(angle_out) * arm_length * t
        dx = math.sin(angle_fwd) * arm_length * t * 0.5
        y = shoulder_y + dy
        x = shoulder_x + dx

        sections.append((z, r, r, r))  # Circular cross-section at offset

    # Build as series of rings
    rings = []
    for z, fr, bk, hw in sections:
        rings.append(section_to_ring(z, fr, bk, hw, n))

    # Offset rings to arm position
    arm_v = []
    for ri, ring in enumerate(rings):
        t = ri / max(1, len(rings) - 1)
        dy = math.sin(angle_out) * arm_length * t
        dx = math.sin(angle_fwd) * arm_length * t * 0.5
        for pt in ring:
            arm_v.append((pt[0] + dx, pt[1] + shoulder_y + dy - shoulder_y * (1 - t), pt[2]))

    # Actually simpler: just offset all points
    v, f = [], []
    for ri, ring in enumerate(rings):
        t = ri / max(1, len(rings) - 1)
        oy = shoulder_y + math.sin(angle_out) * arm_length * t * side
        ox = shoulder_x + math.sin(angle_fwd) * arm_length * t
        for pt in ring:
            v.append((pt[0] + ox, pt[1] + oy - 0, pt[2]))

    rs = n
    for ri in range(len(rings) - 1):
        for si in range(rs):
            ni = (si + 1) % rs
            a, b = ri * rs + si, ri * rs + ni
            c, d = (ri + 1) * rs + ni, (ri + 1) * rs + si
            f.append((a, b, c, d))

    # Hand (simple box)
    last_z = sections[-1][0]
    hand_oy = shoulder_y + math.sin(angle_out) * arm_length * side
    hand_ox = shoulder_x + math.sin(angle_fwd) * arm_length * 0.5
    hv = [
        (hand_ox - 4, hand_oy - 3, last_z),
        (hand_ox + 5, hand_oy - 3, last_z),
        (hand_ox + 5, hand_oy + 3, last_z),
        (hand_ox - 4, hand_oy + 3, last_z),
        (hand_ox - 4, hand_oy - 3, last_z - 10),
        (hand_ox + 5, hand_oy - 3, last_z - 10),
        (hand_ox + 5, hand_oy + 3, last_z - 10),
        (hand_ox - 4, hand_oy + 3, last_z - 10),
    ]
    hf = [(0,1,2,3),(4,7,6,5),(0,4,5,1),(2,6,7,3),(0,3,7,4),(1,5,6,2)]
    base = len(v)
    v.extend(hv)
    f.extend([tuple(i + base for i in face) for face in hf])

    return v, f


def build_cap(head_top_z):
    """Captain's cap — smooth cylinder with visor."""
    v, f = [], []
    cap_z = head_top_z - 3
    n = 16

    # Cap body — 3 rings (bottom wider, top narrower)
    for iz, (z_off, r, r_mult) in enumerate([
        (0, 13, 1.0),      # Brim level
        (3, 12.5, 1.0),    # Band
        (8, 12, 0.95),     # Mid
        (12, 10, 0.85),    # Top approach
        (14, 6, 0.6),      # Crown
    ]):
        z = cap_z + z_off
        for i in range(n):
            a = 2 * math.pi * i / n
            # Slightly oval (wider side to side)
            v.append((r * 0.9 * math.cos(a), r * math.sin(a), z))

    # Connect cap rings
    rs = n
    for ri in range(4):
        for si in range(rs):
            ni = (si + 1) % rs
            a, b = ri * rs + si, ri * rs + ni
            c, d = (ri + 1) * rs + ni, (ri + 1) * rs + si
            f.append((a, b, c, d))

    # Top cap
    tc = len(v)
    v.append((0, 0, cap_z + 15))
    top_base = 4 * rs
    for i in range(rs):
        f.append((tc, top_base + i, top_base + (i + 1) % rs))

    # Visor — curved plate extending forward
    visor_base = len(v)
    visor_n = 8
    for vi in range(3):  # 3 rows: inner, mid, outer
        t = vi / 2
        vr = 13 + t * 10  # Extends outward
        vz = cap_z - 1 - t * 2  # Angles down
        for i in range(visor_n + 1):
            # Only front 120° arc
            a = -math.pi / 3 + (2 * math.pi / 3) * i / visor_n
            v.append((vr * math.cos(a), vr * math.sin(a) * 0.9, vz))

    vrs = visor_n + 1
    for ri in range(2):
        for si in range(visor_n):
            a = visor_base + ri * vrs + si
            b = visor_base + ri * vrs + si + 1
            c = visor_base + (ri + 1) * vrs + si + 1
            d = visor_base + (ri + 1) * vrs + si
            f.append((a, b, c, d))

    return v, f


def simple_mat(name, r, g, b, metal=0.3, rough=0.8):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
    return m


def merge(all_v, all_f, new_v, new_f):
    base = len(all_v)
    all_v.extend(new_v)
    all_f.extend([tuple(i + base for i in face) for face in new_f])


# ═══════════════════════════════════════════════════════════════
# MAIN BUILD
# ═══════════════════════════════════════════════════════════════

def build_captain():
    v, f = [], []

    # ─── Right leg ───
    leg_r = [(z, fr, bk, hw) for z, fr, bk, hw in BODY_SECTIONS]
    # Offset to right leg position
    leg_offset_y = 10
    rv, rf = loft_sections(leg_r, N_AROUND, 2)
    rv = [(x, y + leg_offset_y, z) for x, y, z in rv]
    merge(v, f, rv, rf)

    # ─── Left leg ───
    lv, lf = loft_sections(leg_r, N_AROUND, 2)
    lv = [(x, y - leg_offset_y, z) for x, y, z in lv]
    merge(v, f, lv, lf)

    # ─── Torso ───
    tv, tf = loft_sections(TORSO_SECTIONS, N_AROUND, 3)
    merge(v, f, tv, tf)

    # ─── Head ───
    hv, hf = loft_sections(HEAD_SECTIONS, N_AROUND, 2)
    merge(v, f, hv, hf)

    # ─── Arms ───
    for side in [1, -1]:
        av, af = build_arm(0, 0, 150, side, 8)
        merge(v, f, av, af)

    # ─── Cap ───
    capv, capf = build_cap(191)
    merge(v, f, capv, capf)

    # ─── Belt ───
    belt_v, belt_f = loft_sections([
        (99, 13.5, 11.5, 17.5),
        (101, 14, 12, 18),
        (104, 13.5, 11.5, 17.5),
    ], N_AROUND, 1)
    merge(v, f, belt_v, belt_f)

    # Create mesh
    me = bpy.data.meshes.new("Captain")
    me.from_pydata(v, [], f)
    me.update(calc_edges=True)

    ob = bpy.data.objects.new("CHAR_Captain", me)
    bpy.context.collection.objects.link(ob)

    mat = simple_mat("M_Captain", 0.08, 0.10, 0.15, 0.2, 0.85)
    ob.data.materials.append(mat)

    # Auto smooth
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    try:
        bpy.ops.object.shade_auto_smooth()
    except:
        for p in ob.data.polygons:
            p.use_smooth = True
    ob.select_set(False)

    # Subdivision ready
    mod = ob.modifiers.new("Subdiv", 'SUBSURF')
    mod.levels = 1
    mod.render_levels = 2

    return ob


def main():
    print("\n" + "=" * 60)
    print("Sub3D — Semi-Realistic Captain")
    print("=" * 60)

    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)

    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 0.1
                    sp.clip_end = 50000

    captain = build_captain()

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print(f"  CHAR_Captain: {len(captain.data.vertices)}V {len(captain.data.polygons)}F")
    print(f"  Subdivision Surface: level 1 viewport, level 2 render")
    print(f"  Approach: lofted cross-sections (same as submarine hull)")

main()
