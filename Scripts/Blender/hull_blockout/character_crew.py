"""
Sub3D — Crew Character v6b — v6 body + rounded head
=====================================================
Exact v6 validated body, new rounded head using
elliptical cross-sections for a stylized look.

180cm = 90 voxels @ 2cm
Blender > Scripting > Open > Alt+P
"""

import bpy

VOXEL = 2
HALF = VOXEL / 2
SHOW_JOINTS = True


def build_character():
    g = set()

    def fb(x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    g.add((x, y, z))

    def profile(cy, cx, slices):
        """Z-interpolated column. Slices: [(z, half_w_y, half_d_x), ...]"""
        sl = sorted(slices, key=lambda s: s[0])
        for z in range(sl[0][0], sl[-1][0] + 1):
            lo, hi = sl[0], sl[-1]
            for i in range(len(sl) - 1):
                if sl[i][0] <= z <= sl[i + 1][0]:
                    lo, hi = sl[i], sl[i + 1]
                    break
            span = hi[0] - lo[0]
            t = (z - lo[0]) / span if span > 0 else 0
            yw = round(lo[1] + (hi[1] - lo[1]) * t)
            xd = round(lo[2] + (hi[2] - lo[2]) * t)
            for y in range(cy - yw, cy + yw + 1):
                for x in range(cx - xd, cx + xd + 1):
                    g.add((x, y, z))

    def ellipse_z(cx, cy, z, rx, ry):
        """Fill voxels within ellipse at a Z level."""
        for x in range(cx - rx, cx + rx + 1):
            for y in range(cy - ry, cy + ry + 1):
                dx = (x - cx) / rx if rx > 0 else 0
                dy = (y - cy) / ry if ry > 0 else 0
                if dx * dx + dy * dy <= 1.05:
                    g.add((x, y, z))

    # ══════════════════════════════════════════════════════
    # HEAD  z78-89 (24cm)
    # Cube with corners cut off (top + bottom edges)
    # ══════════════════════════════════════════════════════
    fb(-5, -5, 80, 5, 5, 87)           # Main block (middle 8 levels)
    fb(-4, -4, 78, 4, 4, 79)           # Chin: 1 voxel inset
    fb(-4, -4, 88, 4, 4, 89)           # Crown: 1 voxel inset

    # Cut top 4 corners (z=87)
    for cx, cy in [(-5,-5), (-5,5), (5,-5), (5,5)]:
        g.discard((cx, cy, 87))
    # Cut bottom 4 corners (z=80)
    for cx, cy in [(-5,-5), (-5,5), (5,-5), (5,5)]:
        g.discard((cx, cy, 80))

    # Ears — protrude 1 voxel each side
    fb(-1, -6, 82, 0, -6, 84)          # L
    fb(-1, 6, 82, 0, 6, 84)            # R

    # ══════════════════════════════════════════════════════
    # NECK  z=74–77  (from v6 validated)
    # ══════════════════════════════════════════════════════
    profile(0, 0, [
        (74, 3, 2),
        (77, 2, 2),
    ])

    # ══════════════════════════════════════════════════════
    # TORSO + SHOULDERS  z=44–73  (from v6 validated)
    # ══════════════════════════════════════════════════════
    profile(0, 0, [
        (44, 7, 4),         # Hips
        (47, 7, 4),         # Hip top
        (49, 6, 3),         # Waist (lowered)
        (53, 7, 4),         # Lower chest
        (59, 9, 5),         # Chest
        (65, 9, 5),         # Upper chest
        (67, 11, 3),        # Shoulder bar
        (69, 10, 3),        # Shoulder slope
        (71, 6, 2),         # Trapezius
        (73, 3, 2),         # Neck base
    ])

    # Deltoid caps
    fb(0, 9, 66, 1, 12, 70)
    fb(0, -12, 66, 1, -9, 70)

    # Crotch bridge
    fb(-1, -1, 42, 1, 1, 43)

    # ══════════════════════════════════════════════════════
    # THIGHS  z=24–43  (from v6 validated)
    # ══════════════════════════════════════════════════════
    profile(4, 0, [
        (24, 2, 3),
        (34, 3, 3),
        (43, 3, 3),
    ])
    profile(-4, 0, [
        (24, 2, 3),
        (34, 3, 3),
        (43, 3, 3),
    ])

    # Knees
    fb(-3, 2, 22, 4, 6, 23)
    fb(-3, -6, 22, 4, -2, 23)

    # ══════════════════════════════════════════════════════
    # SHINS  z=6–21  (from v6 validated)
    # ══════════════════════════════════════════════════════
    profile(4, 0, [
        (6, 2, 1),
        (14, 2, 2),
        (21, 2, 3),
    ])
    profile(-4, 0, [
        (6, 2, 1),
        (14, 2, 2),
        (21, 2, 3),
    ])

    # Ankles
    fb(-1, 2, 4, 2, 6, 5)
    fb(-1, -6, 4, 2, -2, 5)

    # Feet
    fb(-2, 2, 0, 5, 6, 1)
    fb(-1, 2, 2, 3, 6, 3)
    fb(-2, -6, 0, 5, -2, 1)
    fb(-1, -6, 2, 3, -2, 3)

    # ══════════════════════════════════════════════════════
    # ARMS — T-POSE (horizontal at shoulder height z=68)
    # Shoulder edge body at y=±11, arm extends outward
    # Upper arm: y=12→21, elbow crease at y=22, forearm: y=23→32
    # ══════════════════════════════════════════════════════
    arm_z = 68   # Shoulder height in 2cm grid

    # R upper arm
    for y in range(12, 22):
        fb(-2, y, arm_z-2, 2, y, arm_z+2)
    # R elbow crease (1 voxel thinner in depth)
    for y in range(22, 23):
        fb(-1, y, arm_z-2, 1, y, arm_z+2)
    # R forearm
    for y in range(23, 33):
        fb(-2, y, arm_z-2, 2, y, arm_z+2)

    # L upper arm
    for y in range(-21, -11):
        fb(-2, y, arm_z-2, 2, y, arm_z+2)
    # L elbow crease
    for y in range(-22, -21):
        fb(-1, y, arm_z-2, 1, y, arm_z+2)
    # L forearm
    for y in range(-32, -22):
        fb(-2, y, arm_z-2, 2, y, arm_z+2)

    # ══════════════════════════════════════════════════════
    # HANDS — 2cm, in same mesh. T-pose: extend +Y from forearm.
    # Wrist at y=33 (R), forearm section is 5×5 (hd=2, hz=2)
    # Hand: flatter (thinner in X), wider in Z, with finger block
    # ══════════════════════════════════════════════════════

    # R hand — palm in 2cm grid (thumb now in finger grid)
    fb(-1, 33, arm_z-2, 1, 36, arm_z+2)     # Palm: 3D × 4W × 5H

    # L hand — palm in 2cm grid
    fb(-1, -36, arm_z-2, 1, -33, arm_z+2)

    # ── Fingers built in fine_grid (0.4cm = VOXEL/5) ──
    # fine_grid is a separate set at 0.4cm resolution
    # Fingers extend from palm end (y=37 in 2cm = y=185 in 0.4cm)
    # arm_z=68 in 2cm = 340 in 0.4cm
    pass  # fingers added below via fine_grid

    return g


def build_fingers_r(arm_z_2cm):
    """R hand fingers + thumb at 0.67cm voxels (VOXEL/3). Returns set.
    Fingers extend +Y from palm end. Crescendo: pinky small → middle big."""
    g = set()
    # Scale: 1 voxel at 2cm = 3 voxels at 0.67cm
    # Palm end: y=37 in 2cm → y=37*3=111 in fine grid
    # arm_z center: 68*3 = 204
    py = 111
    cz = arm_z_2cm * 3

    def fb(x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1+1):
            for y in range(y0, y1+1):
                for z in range(z0, z1+1):
                    g.add((x, y, z))

    # Crescendo: pinky thin/short → middle thick/long → index slightly shorter
    # Each finger: width(x) × height(z), 1-voxel gap between

    # Pinky:  2×2 section, length 7  (~4.7cm)
    fb(-1, py, cz-6, 0, py+7, cz-5)
    # Ring:   2×3 section, length 9  (~6cm)
    fb(-1, py, cz-3, 0, py+9, cz-1)
    # Middle: 3×3 section, length 11 (~7.3cm, longest)
    fb(-2, py, cz+1, 0, py+11, cz+3)
    # Index:  2×3 section, length 10 (~6.7cm)
    fb(-1, py, cz+5, 0, py+10, cz+7)

    # Thumb — thicker, extends +X (forward) from palm side
    # Starts from palm mid-height, angles outward
    tx = 3 * 3  # x=3 in 2cm = 9 in fine
    fb(tx-2, py-4, cz+5, tx, py+6, cz+8)

    return g

def build_fingers_l(arm_z_2cm):
    """L hand fingers — mirror of R."""
    r = build_fingers_r(arm_z_2cm)
    return {(x, -y, z) for (x, y, z) in r}


# ═════════════════════════════════════════════════════════════
# VOXEL MESH GENERATOR
# ═════════════════════════════════════════════════════════════

FINGER_VS = VOXEL / 3  # ~0.67cm

FACE_DEFS_TOPO = [
    ((1, 0, 0),  [(1,0,0), (1,1,0), (1,1,1), (1,0,1)]),
    ((-1,0, 0),  [(0,0,0), (0,0,1), (0,1,1), (0,1,0)]),
    ((0, 1, 0),  [(0,1,0), (0,1,1), (1,1,1), (1,1,0)]),
    ((0,-1, 0),  [(0,0,0), (1,0,0), (1,0,1), (0,0,1)]),
    ((0, 0, 1),  [(0,0,1), (1,0,1), (1,1,1), (0,1,1)]),
    ((0, 0,-1),  [(0,0,0), (0,1,0), (1,1,0), (1,0,0)]),
]

def generate_mesh_multi(body_grid, finger_grids):
    """Generate combined mesh from 2cm body + 0.4cm finger grids.
    finger_grids: list of sets at FINGER_VS resolution."""
    cache = {}
    verts = []
    faces = []

    def vid(wx, wy, wz):
        """Vertex by world position (cm), rounded to avoid float drift."""
        key = (round(wx, 4), round(wy, 4), round(wz, 4))
        if key not in cache:
            cache[key] = len(verts)
            verts.append((wx, wy, wz))
        return cache[key]

    # Body at 2cm
    vs = VOXEL
    h = HALF
    for (vx, vy, vz) in body_grid:
        for (dx, dy, dz), corners in FACE_DEFS_TOPO:
            if (vx+dx, vy+dy, vz+dz) not in body_grid:
                f = tuple(vid((vx+cx)*vs - h, (vy+cy)*vs - h, (vz+cz)*vs)
                          for cx, cy, cz in corners)
                faces.append(f)

    # Fingers at 0.4cm
    fvs = FINGER_VS
    fh = fvs / 2
    for fg in finger_grids:
        for (vx, vy, vz) in fg:
            for (dx, dy, dz), corners in FACE_DEFS_TOPO:
                if (vx+dx, vy+dy, vz+dz) not in fg:
                    f = tuple(vid((vx+cx)*fvs - fh, (vy+cy)*fvs - fh, (vz+cz)*fvs)
                              for cx, cy, cz in corners)
                    faces.append(f)

    return verts, faces


# ═════════════════════════════════════════════════════════════
# MAIN
# ═════════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D — Crew v6b — v6 body + rounded head")
    print("=" * 60)

    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
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

    grid = build_character()
    fingers_r = build_fingers_r(68)
    fingers_l = build_fingers_l(68)
    verts, faces = generate_mesh_multi(grid, [fingers_r, fingers_l])

    me = bpy.data.meshes.new("SM_Crew")
    me.from_pydata(verts, [], faces)
    me.validate(); me.update(calc_edges=True)

    ob = bpy.data.objects.new("SM_Crew", me)
    bpy.context.collection.objects.link(ob)

    m = bpy.data.materials.new("M_Clay")
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.76, 0.70, 0.63, 1)
        bsdf.inputs["Roughness"].default_value = 0.92
    ob.data.materials.append(m)

    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_flat()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    ob.select_set(False)

    zs = [v[2] for v in verts]
    ys = [v[1] for v in verts]
    xs = [v[0] for v in verts]
    print(f"\n  Voxels:  {len(grid)}")
    print(f"  Mesh:    {len(verts)}V  {len(faces)}F")
    print(f"  Height:  {max(zs):.0f}cm")
    print(f"  Width:   {max(ys)-min(ys):.0f}cm")
    print(f"  Depth:   {max(xs)-min(xs):.0f}cm")

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'SOLID'
                    sp.shading.color_type = 'MATERIAL'
            break

    # ═════════════════════════════════════════════════════
    # ARMATURE — UE5 Mannequin bone names, T-pose
    # ═════════════════════════════════════════════════════
    print("\n  Building armature...")

    # (name, head_xyz, tail_xyz, parent_name, connected)
    bones_def = [
        ("root",        (0, 0, 0),      (0, 0, 97),     None,          False),
        ("pelvis",      (0, 0, 97),     (0, 0, 107),    "root",        True),
        ("spine_01",    (0, 0, 107),    (0, 0, 117),    "pelvis",      True),
        ("spine_02",    (0, 0, 117),    (0, 0, 127),    "spine_01",    True),
        ("spine_03",    (0, 0, 127),    (0, 0, 137),    "spine_02",    True),
        ("spine_04",    (0, 0, 137),    (0, 0, 144),    "spine_03",    True),
        ("spine_05",    (0, 0, 144),    (0, 0, 151),    "spine_04",    True),
        ("neck_01",     (0, 0, 151),    (0, 0, 156),    "spine_05",    True),
        ("neck_02",     (0, 0, 156),    (0, 0, 161),    "neck_01",     True),
        ("head",        (0, 0, 161),    (0, 0, 180),    "neck_02",     True),

        # R arm
        ("clavicle_r",  (0, 4, 142),    (0, 20, 136),   "spine_05",    False),
        ("upperarm_r",  (0, 20, 136),   (0, 44, 136),   "clavicle_r",  True),
        ("lowerarm_r",  (0, 44, 136),   (0, 66, 136),   "upperarm_r",  True),
        ("hand_r",      (0, 66, 136),   (0, 80, 136),   "lowerarm_r",  True),

        # L arm
        ("clavicle_l",  (0, -4, 142),   (0, -20, 136),  "spine_05",    False),
        ("upperarm_l",  (0, -20, 136),  (0, -44, 136),  "clavicle_l",  True),
        ("lowerarm_l",  (0, -44, 136),  (0, -66, 136),  "upperarm_l",  True),
        ("hand_l",      (0, -66, 136),  (0, -80, 136),  "lowerarm_l",  True),

        # R leg
        ("thigh_r",     (0, 8, 88),     (0, 8, 48),     "pelvis",      False),
        ("calf_r",      (0, 8, 48),     (0, 8, 8),      "thigh_r",     True),
        ("foot_r",      (0, 8, 8),      (8, 8, 0),      "calf_r",      True),
        ("ball_r",      (8, 8, 0),      (14, 8, 0),     "foot_r",      True),

        # L leg
        ("thigh_l",     (0, -8, 88),    (0, -8, 48),    "pelvis",      False),
        ("calf_l",      (0, -8, 48),    (0, -8, 8),     "thigh_l",     True),
        ("foot_l",      (0, -8, 8),     (8, -8, 0),     "calf_l",      True),
        ("ball_l",      (8, -8, 0),     (14, -8, 0),    "foot_l",      True),
    ]

    arm_data = bpy.data.armatures.new("SKEL_Crew")
    arm_obj = bpy.data.objects.new("Armature_Crew", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)

    # Create bones in edit mode
    bpy.ops.object.mode_set(mode='EDIT')
    edit_bones = arm_data.edit_bones

    for name, head, tail, parent_name, connected in bones_def:
        b = edit_bones.new(name)
        b.head = head
        b.tail = tail
        if parent_name and parent_name in edit_bones:
            b.parent = edit_bones[parent_name]
            b.use_connect = connected

    bpy.ops.object.mode_set(mode='OBJECT')

    # Armature display
    arm_data.display_type = 'OCTAHEDRAL'
    arm_obj.show_in_front = True

    # Parent mesh → armature with automatic weights
    print("  Parenting mesh to armature (auto weights)...")
    ob.select_set(True)
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')

    print(f"  Bones: {len(bones_def)}")
    print(f"  Hierarchy: root > pelvis > spine×5 > neck×2 > head")
    print(f"             pelvis > thigh > calf > foot > ball (×2)")
    print(f"             spine_05 > clavicle > upperarm > lowerarm > hand (×2)")

    # Frame
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'SOLID'
                    sp.shading.color_type = 'MATERIAL'
            break

    print("\n" + "=" * 60)
    print("v6 body, T-pose, multi-res fingers, RIGGED")
    print("Select armature > Pose Mode to test deformation")
    print("=" * 60)


main()
