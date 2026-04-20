"""
Sub3D — Rig: Innsmouth Hunter (Strate 1)
=========================================
v2 — clean jaw / skull weight separation.
Run AFTER innsmouth.py.

18 bones, programmatic vertex weights.

Jaw zone rules (no blending across the jaw gap at z=5):
  vx < 0        → Jaw (all negative-x geometry)
  vz < 5.5      → Jaw (below gap, includes jaw body at z=0..4)
  vz < 8.5 and tooth column → Jaw (teeth above gap, full height)
  otherwise x<13 → falls through to spine zones → Head

Spine zones use soft overlap (BLEND=5 vox) between adjacent bones.
Pectoral fingers are Y-banded with smooth blend at body junction.

Blender > Scripting > Alt+P
"""

import bpy
from mathutils import Vector

VOXEL = 7
HALF  = VOXEL / 2  # 3.5

# (name, head_vox, tail_vox, parent_name)
BONES = [
    ("Hips",         (38,  0, 10), (33,  0, 10), None),
    ("Spine_Thorax", (33,  0, 10), (20,  0, 10), "Hips"),
    ("Neck",         (20,  0, 10), ( 8,  0, 10), "Spine_Thorax"),
    ("Head",         ( 8,  0, 10), ( 0,  0, 10), "Neck"),
    # Jaw hinge at x=10 (back of lower jaw), tip points toward -x
    ("Jaw",          (10,  0,  4), (-6,  0,  2), "Head"),
    ("Cirre_L",      (-2, -2,  1), ( 1, -2, -3), "Jaw"),
    ("Cirre_R",      (-2,  2,  1), ( 2,  2, -3), "Jaw"),
    ("Spine_Mid",    (38,  0, 10), (55,  0,  7), "Hips"),
    ("Peduncle",     (55,  0,  7), (68,  0,  9), "Spine_Mid"),
    ("Caudal",       (68,  0, 10), (83,  0, 10), "Peduncle"),
    ("Pect_L_01",    (28,  -9, 3), (33,  -9, 3), "Spine_Thorax"),
    ("Pect_L_02",    (28, -11, 3), (34, -11, 3), "Spine_Thorax"),
    ("Pect_L_03",    (28, -13, 3), (32, -13, 3), "Spine_Thorax"),
    ("Pect_L_04",    (28, -14, 3), (33, -14, 3), "Spine_Thorax"),
    ("Pect_R_01",    (28,   9, 3), (34,   9, 3), "Spine_Thorax"),
    ("Pect_R_02",    (28,  11, 3), (33,  11, 3), "Spine_Thorax"),
    ("Pect_R_03",    (28,  12, 3), (34,  12, 3), "Spine_Thorax"),
    ("Pect_R_04",    (28,  13, 3), (31,  13, 3), "Spine_Thorax"),
]

# Spine zones: (bone, x_start_vox, x_end_vox)
# Head zone includes full front skull region (0..13)
SPINE_ZONES = [
    ("Head",          0, 13),
    ("Neck",         13, 23),
    ("Spine_Thorax", 23, 36),
    ("Hips",         36, 42),
    ("Spine_Mid",    42, 57),
    ("Peduncle",     57, 70),
    ("Caudal",       70, 84),
]
BLEND = 5.0  # voxel soft-blend width at zone boundaries

# Jaw tooth x-positions from innsmouth.py jaw_teeth definition
# [(-7,2),(-5,1),(-3,3),(-1,1),(2,2),(4,3),(7,1)]
_TOOTH_GX = (-7, -5, -3, -1, 2, 4, 7)


def v2w(vc):
    return Vector((vc[0] * VOXEL, vc[1] * VOXEL, vc[2] * VOXEL))


def _is_tooth_col(vx, vy):
    """
    True if this vertex sits in a jaw tooth column (above the jaw gap).
    Teeth are 1-voxel wide at specific x positions, |y| <= 2 voxels.
    """
    if abs(vy) > 3.5:
        return False
    # Each tooth at grid gx maps to vertex range vx ≈ gx..gx+1
    return any(abs(vx - (tx + 0.5)) < 1.0 for tx in _TOOTH_GX)


def compute_weights(vx, vy, vz):
    """
    vx, vy, vz — float voxel coordinates back-computed from world vertex.
    Returns {bone_name: weight} (unnormalized).

    Priority order (first match wins):
      1. Cirres
      2. Jaw (all negative x, jaw body z<5.5, tooth cols z<8.5)
      3. Pectoral fingers (wide Y in thorax zone)
      4. Spine zones with soft blend (catches skull via Head zone)
    """
    av = abs(vy)

    # ── 1. Cirres (hang below z=0, front of creature) ──────────
    if vz < 0.5 and vx < 5.0:
        return {"Cirre_L" if vy < 0 else "Cirre_R": 1.0}

    # ── 2. Jaw zone — three sub-conditions ─────────────────────
    # 2a. All negative-x geometry (jaw protrusion beyond skull)
    if vx < 0.5:
        return {"Jaw": 1.0}

    # 2b. Below the jaw gap (z < 5.5) in the front region
    #     Gap is at z=5 (empty voxel space separating jaw from skull).
    #     Jaw body: z=0..4 → top vertex at gz=5, wz=35, vz=5.0 ← caught here.
    #     Skull floor: z=6 → bottom vertex at gz=6, wz=42, vz=6.0 ← NOT caught.
    if vz < 5.5 and vx < 13.0:
        return {"Jaw": 1.0}

    # 2c. Jaw teeth columns above the gap (z=5..7 at tooth x-positions)
    #     Without this, tooth-tip vertices would fall into Head zone.
    if vz < 8.5 and vx < 13.0 and _is_tooth_col(vx, vy):
        return {"Jaw": 1.0}

    # ── 3. Pectoral fingers (Y-banded, thorax region) ──────────
    if av > 8.0 and 25.0 < vx < 40.0:
        if vy < 0:
            if av < 10.5:   finger = "Pect_L_01"
            elif av < 12.0: finger = "Pect_L_02"
            elif av < 13.5: finger = "Pect_L_03"
            else:           finger = "Pect_L_04"
        else:
            if av < 10.5:   finger = "Pect_R_01"
            elif av < 11.5: finger = "Pect_R_02"
            elif av < 12.5: finger = "Pect_R_03"
            else:           finger = "Pect_R_04"
        # Blend with Spine_Thorax at body junction (~8.5 vox half-width)
        t = min(1.0, max(0.0, (av - 8.5) / 3.0))
        return {finger: t, "Spine_Thorax": 1.0 - t}

    # ── 4. Spine chain — soft overlap between adjacent zones ────
    #     Skull voxels (vx<13, vz>=8.5) fall here and get Head weight.
    #     BLEND radius creates smooth transitions at zone boundaries.
    weights = {}
    for bname, zs, ze in SPINE_ZONES:
        if vx >= ze + BLEND or vx <= zs - BLEND:
            continue
        if vx < zs:
            w = max(0.0, 1.0 - (zs - vx) / BLEND)
        elif vx > ze:
            w = max(0.0, 1.0 - (vx - ze) / BLEND)
        else:
            w = 1.0
        if w > 0.0:
            weights[bname] = w

    return weights or {"Hips": 1.0}


def main():
    print("\n" + "=" * 55)
    print("Sub3D Fauna — Innsmouth Hunter Rig  (v2)")
    print("=" * 55)

    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    mesh_ob = bpy.data.objects.get("InnsmouthHunter")
    if not mesh_ob or mesh_ob.type != 'MESH':
        print("ERROR: InnsmouthHunter not found. Run innsmouth.py first.")
        return

    # Safe to re-run: remove old rig
    for ob in list(bpy.data.objects):
        if ob.type == 'ARMATURE' and "InnsmouthRig" in ob.name:
            bpy.data.objects.remove(ob, do_unlink=True)
    for mod in list(mesh_ob.modifiers):
        if mod.type == 'ARMATURE':
            mesh_ob.modifiers.remove(mod)
    if mesh_ob.parent and mesh_ob.parent.type == 'ARMATURE':
        mesh_ob.parent = None

    # ── Build armature ─────────────────────────────────────────
    arm_data = bpy.data.armatures.new("InnsmouthRig")
    arm_ob   = bpy.data.objects.new("InnsmouthRig", arm_data)
    bpy.context.collection.objects.link(arm_ob)

    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = arm_ob
    arm_ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')

    eb = arm_data.edit_bones
    bone_refs = {}
    for bname, head_v, tail_v, _ in BONES:
        b = eb.new(bname)
        b.head = v2w(head_v)
        b.tail = v2w(tail_v)
        b.use_deform = True
        bone_refs[bname] = b

    for bname, _, _, parent_name in BONES:
        if parent_name:
            bone_refs[bname].parent = bone_refs[parent_name]

    bpy.ops.object.mode_set(mode='OBJECT')

    # ── Vertex groups + programmatic weights ───────────────────
    bpy.context.view_layer.objects.active = mesh_ob

    for bname in (b[0] for b in BONES):
        if bname not in mesh_ob.vertex_groups:
            mesh_ob.vertex_groups.new(name=bname)

    vgm = {vg.name: vg for vg in mesh_ob.vertex_groups}

    jaw_ct = skull_ct = 0
    for vi, vert in enumerate(mesh_ob.data.vertices):
        wx, wy, wz = vert.co
        # Invert: wx = gx*VOXEL - HALF → gx = (wx+HALF)/VOXEL
        vx = (wx + HALF) / VOXEL
        vy = (wy + HALF) / VOXEL
        vz =  wz         / VOXEL

        raw = compute_weights(vx, vy, vz)
        total = sum(raw.values()) or 1.0
        for bname, w in raw.items():
            vgm[bname].add([vi], w / total, 'REPLACE')

        if "Jaw" in raw:   jaw_ct   += 1
        if "Head" in raw:  skull_ct += 1

    # ── Armature modifier + parent ─────────────────────────────
    mod = mesh_ob.modifiers.new("InnsmouthRig", 'ARMATURE')
    mod.object = arm_ob
    mod.use_vertex_groups = True
    mesh_ob.parent = arm_ob

    nv = len(mesh_ob.data.vertices)
    print(f"  {len(BONES)} bones / {nv} total vertices")
    print(f"  Jaw group  : {jaw_ct} vertices")
    print(f"  Head group : {skull_ct} vertices")
    print(f"  Modifier   : {mod.name}")
    print()
    print("  Jaw open  : Pose Mode → select Jaw → R X (rotate around X)")
    print("  Head turn : Pose Mode → select Head → R Z")
    print()
    print("  Weight Paint check:")
    print("    Jaw bone  → jaw body dark below skull + teeth (no skull bleed)")
    print("    Head bone → skull only, no jaw influence")
    print("=" * 55)


main()
