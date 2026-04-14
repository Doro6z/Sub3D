"""
Sub3D — Voxel Crew Body (base mannequin)
==========================================
Corps de base NU sur lequel se greffent les accessoires :
  - SM_Hat_*   → sur la tete
  - SM_Face_*  → devant la tete
  - SM_Beard_* → sous la tete
  - SM_Gear_*  → sur le torse/ceinture

Proportions chunky inspirees Deep Rock / Teardown.
Epaules larges, torse epais, mains grosses, bottes lourdes.
Tete = cube neutre (les details viennent des accessoires).

Aligned to UE5 Mannequin skeleton for rigging.
Blender > Scripting > Open > Alt+P
"""

import bpy

VOXEL = 4
HALF  = VOXEL / 2


# =================================================================
# BASE BODY — Mannequin only. No gear, no face, no hat.
# =================================================================

def build_grid():
    g = set()

    def fb(x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    g.add((x, y, z))

    # ══════════════════════════════════════
    # HEAD — Plain cube, socket for Face/Hat/Beard
    # 5x5x5 = 20x20x20cm
    # ══════════════════════════════════════
    fb(-2, -2, 40, 2, 2, 44)

    # ══════════════════════════════════════
    # NECK — Short, thick, industrial
    # 5x5x2 = 20x20x8cm (trapeze feel)
    # ══════════════════════════════════════
    fb(-2, -2, 38, 2, 2, 39)

    # ══════════════════════════════════════
    # TORSO — Wide & deep. Barrel chest.
    # ══════════════════════════════════════

    # Shoulders — WIDE slab. 7x15x3 = 28x60x12cm
    fb(-3, -7, 35, 3, 7, 37)

    # Shoulder-to-chest transition
    fb(-3, -6, 34, 3, 6, 34)

    # Chest — deep barrel. 7x13x4 = 28x52x16cm
    fb(-3, -6, 30, 3, 6, 33)

    # Waist — tapers in. 5x11x3 = 20x44x12cm
    fb(-2, -5, 27, 2, 5, 29)

    # Hips/pelvis — widens slightly. 5x13x2 = 20x52x8cm
    fb(-2, -6, 25, 2, 6, 26)

    # Crotch bridge — connects to legs
    fb(-1, -3, 23, 1, 3, 24)

    # ══════════════════════════════════════
    # ARMS — Thick tubes, hang from wide shoulders
    # ══════════════════════════════════════

    # Right upper arm — 3x3x8 = 12x12x32cm
    fb(-1, 8, 29, 1, 10, 37)
    # Left upper arm
    fb(-1, -10, 29, 1, -8, 37)

    # Right lower arm — 3x3x6 = 12x12x24cm
    fb(-1, 8, 23, 1, 10, 28)
    # Left lower arm
    fb(-1, -10, 23, 1, -8, 28)

    # ══════════════════════════════════════
    # HANDS — Big chunky mitts
    # 4x5x3 = 16x20x12cm
    # ══════════════════════════════════════

    fb(-1, 7, 20, 2, 11, 22)    # Right
    fb(-1, -11, 20, 2, -7, 22)  # Left

    # ══════════════════════════════════════
    # LEGS — Sturdy, 3x3 cross-section
    # ══════════════════════════════════════

    # Right thigh — z13-24 (52-96cm)
    fb(-1, 2, 13, 1, 4, 24)
    # Left thigh
    fb(-1, -4, 13, 1, -2, 24)

    # Right shin — z5-12 (20-52cm)
    fb(-1, 2, 5, 1, 4, 12)
    # Left shin
    fb(-1, -4, 5, 1, -2, 12)

    # ══════════════════════════════════════
    # BOOTS — Forward-pointing, heavy sole
    # ══════════════════════════════════════

    # Right boot shaft — z3-4
    fb(-1, 2, 3, 2, 4, 4)
    # Left boot shaft
    fb(-1, -4, 3, 2, -2, 4)

    # Right boot sole — long forward. 6x3x3 = 24x12x12cm
    fb(-1, 2, 0, 4, 4, 2)
    # Left boot sole
    fb(-1, -4, 0, 4, -2, 2)

    return g


# =================================================================
# VOXEL MESH GENERATOR
# =================================================================

def generate_mesh(grid):
    cache = {}
    verts = []
    faces = []

    def vid(gx, gy, gz):
        key = (gx, gy, gz)
        if key not in cache:
            cache[key] = len(verts)
            verts.append((
                gx * VOXEL - HALF,
                gy * VOXEL - HALF,
                gz * VOXEL,
            ))
        return cache[key]

    face_defs = [
        ((1, 0, 0),  [(1,0,0), (1,1,0), (1,1,1), (1,0,1)]),
        ((-1,0, 0),  [(0,0,0), (0,0,1), (0,1,1), (0,1,0)]),
        ((0, 1, 0),  [(0,1,0), (0,1,1), (1,1,1), (1,1,0)]),
        ((0,-1, 0),  [(0,0,0), (1,0,0), (1,0,1), (0,0,1)]),
        ((0, 0, 1),  [(0,0,1), (1,0,1), (1,1,1), (0,1,1)]),
        ((0, 0,-1),  [(0,0,0), (0,1,0), (1,1,0), (1,0,0)]),
    ]

    for (vx, vy, vz) in grid:
        for (dx, dy, dz), corners in face_defs:
            if (vx + dx, vy + dy, vz + dz) not in grid:
                f = tuple(
                    vid(vx + cx, vy + cy, vz + cz)
                    for cx, cy, cz in corners
                )
                faces.append(f)

    return verts, faces


# =================================================================
# MAIN
# =================================================================

SHOW_JOINTS = False


def main():
    print("\n" + "=" * 60)
    print("Sub3D — Voxel Crew Body (base)")
    print("=" * 60)

    # Force object mode (previous run may have crashed in edit mode)
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

    grid = build_grid()
    verts, faces = generate_mesh(grid)

    me = bpy.data.meshes.new("SM_Body_Crew")
    me.from_pydata(verts, [], faces)
    me.validate()
    me.update(calc_edges=True)

    ob = bpy.data.objects.new("SM_Body_Crew", me)
    bpy.context.collection.objects.link(ob)

    # Skin tone neutral — accessories will have their own materials
    m = bpy.data.materials.new("M_Skin")
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.62, 0.48, 0.38, 1)
        bsdf.inputs["Roughness"].default_value = 0.9
        bsdf.inputs["Metallic"].default_value = 0.0
    ob.data.materials.append(m)

    # Flat shading + fix normals
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
    print(f"\n  SM_Body_Crew: {len(verts)}V  {len(faces)}F  ({len(grid)} voxels)")
    print(f"  Height:  {max(zs):.0f}cm")
    print(f"  Width:   {max(ys)-min(ys):.0f}cm (shoulders)")
    print(f"  Depth:   {max(xs)-min(xs):.0f}cm")
    print(f"  Voxel:   {VOXEL}cm")
    print(f"\n  Socket points (for accessories):")
    print(f"    Head top:   z={45*VOXEL}cm  (hats)")
    print(f"    Head front: z={42*VOXEL}cm  (faces)")
    print(f"    Chin:       z={40*VOXEL}cm  (beards)")
    print(f"    Belt:       z={25*VOXEL}cm  (gear)")
    print(f"    Chest:      z={32*VOXEL}cm  (gear)")

    if SHOW_JOINTS:
        joints = [
            ("pelvis", 0, 0, 97.5), ("spine_02", 0, 0, 117),
            ("spine_04", 0, 0, 137), ("neck", 0, 0, 151),
            ("head", 0, 0, 161),
            ("shoulder_r", 0, 22, 147), ("shoulder_l", 0, -22, 147),
            ("elbow_r", 0, 36, 117), ("elbow_l", 0, -36, 117),
            ("wrist_r", 0, 44, 93), ("wrist_l", 0, -44, 93),
            ("knee_r", 0, 12, 50), ("knee_l", 0, -12, 50),
            ("ankle_r", 0, 12, 8), ("ankle_l", 0, -12, 8),
        ]
        for name, jx, jy, jz in joints:
            bpy.ops.mesh.primitive_uv_sphere_add(
                radius=2.5, location=(jx*0.01, jy*0.01, jz*0.01))
            j = bpy.context.active_object
            j.name = f"J_{name}"
            jm = bpy.data.materials.new(f"M_J")
            jm.use_nodes = True
            jb = jm.node_tree.nodes.get("Principled BSDF")
            if jb:
                jb.inputs["Base Color"].default_value = (1, 0.4, 0, 1)
            j.data.materials.append(jm)

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    print("Base mannequin. Pas de gear/face/hat.")
    print("Les accessoires se greffent aux socket points.")
    print("SHOW_JOINTS = True pour debug skeleton")
    print("=" * 60)


main()
