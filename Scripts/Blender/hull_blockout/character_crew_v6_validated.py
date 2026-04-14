"""
Sub3D — Crew Character v6 — 2cm Micro-voxels
===============================================
Voxels 2cm pour formes graduelles (diagonales, tapers).
Profile interpolation : chaque niveau Z a sa section
interpolee entre des slices clefs → pentes fluides.

Reference : "Blocky Stylized Base Mesh" mannequin.
  - Epaules en pente (pas une barre)
  - V-taper subtil (poitrine → taille)
  - Jambes clean et longues
  - Pieds simples, petits
  - Bras fins, separes du corps
  - Mains avec doigts individuels
  - Tete voxel avec machoire et arcade

180cm = 90 voxels @ 2cm
Crotch z=44 (88cm = 49% hauteur)

Anatomie Z (2cm voxels) :
  z  0- 3   Pieds        0-8cm
  z  4- 5   Chevilles    8-12cm
  z  6-21   Tibias      12-44cm
  z 22-23   Genoux      44-48cm
  z 24-43   Cuisses     48-88cm
  z 44-49   Hanches     88-100cm
  z 50-52   Taille     100-106cm
  z 53-67   Poitrine   106-136cm
  z 68-73   Epaules    136-148cm
  z 74-77   Cou        148-156cm
  z 78-89   Tete       156-180cm

Blender > Scripting > Open > Alt+P
"""

import bpy

VOXEL = 2
HALF = VOXEL / 2


def build_character():
    g = set()

    def fb(x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    g.add((x, y, z))

    def profile(cy, cx, slices):
        """Z-interpolated column. Slices: [(z, half_w_y, half_d_x), ...]
        Linearly interpolates width/depth between keyframes."""
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

    # ══════════════════════════════════════════════════════
    # HEAD  z78-89 (24cm)
    # Voxel cube with brow, jaw, ears
    # ══════════════════════════════════════════════════════
    fb(-5, -5, 78, 5, 5, 89)           # Main block 22x22x24cm

    # Brow ridge — forward overhang
    fb(6, -4, 86, 6, 4, 87)

    # Jaw — projects forward, narrower
    fb(6, -3, 78, 6, 3, 79)

    # Ears — protrude 1 voxel each side
    fb(-1, -6, 82, 0, -6, 84)          # L
    fb(-1, 6, 82, 0, 6, 84)            # R

    # ══════════════════════════════════════════════════════
    # NECK  z74-77 (8cm)
    # ══════════════════════════════════════════════════════
    profile(0, 0, [
        (74, 3, 2),                     # Neck bottom: ~14cm W
        (77, 2, 2),                     # Neck top: ~10cm W
    ])

    # ══════════════════════════════════════════════════════
    # TORSO + SHOULDERS  z44-73
    # Single interpolated profile: hips → waist → chest → shoulders
    # Creates smooth V-taper and shoulder slope
    # ══════════════════════════════════════════════════════
    profile(0, 0, [
        (44, 7, 4),                     # Hip bottom: 30cm W × 18cm D
        (49, 7, 4),                     # Hip top: same
        (52, 6, 3),                     # Waist: 26cm W × 14cm D
        (56, 7, 4),                     # Lower chest: widens
        (62, 9, 5),                     # Chest: 38cm W × 22cm D
        (67, 9, 5),                     # Chest top: maintains
        (68, 11, 3),                    # Shoulder bar: 46cm W (widest)
        (70, 10, 3),                    # Shoulder mid
        (72, 6, 2),                     # Trapezius
        (73, 3, 2),                     # Neck base: 14cm W
    ])

    # ══════════════════════════════════════════════════════
    # DELTOID CAPS — smooth shoulder-to-arm transition
    # ══════════════════════════════════════════════════════
    fb(0, 9, 66, 1, 12, 70)            # R deltoid
    fb(0, -12, 66, 1, -9, 70)          # L deltoid

    # ══════════════════════════════════════════════════════
    # CROTCH BRIDGE
    # ══════════════════════════════════════════════════════
    fb(-1, -1, 42, 1, 1, 43)

    # ══════════════════════════════════════════════════════
    # THIGHS  z24-43 — tapered columns
    # Center: R y=4, L y=-4
    # ══════════════════════════════════════════════════════
    profile(4, 0, [
        (24, 2, 3),                     # Near knee: 10cm W × 14cm D
        (34, 3, 3),                     # Mid thigh: 14cm W
        (43, 3, 3),                     # Near hip: 14cm W × 14cm D
    ])
    profile(-4, 0, [
        (24, 2, 3),
        (34, 3, 3),
        (43, 3, 3),
    ])

    # ══════════════════════════════════════════════════════
    # KNEES  z22-23 — subtle forward extension
    # ══════════════════════════════════════════════════════
    fb(-3, 2, 22, 4, 6, 23)            # R: extra x=4 forward
    fb(-3, -6, 22, 4, -2, 23)          # L

    # ══════════════════════════════════════════════════════
    # SHINS  z6-21 — clean, thins toward ankle
    # ══════════════════════════════════════════════════════
    profile(4, 0, [
        (6, 2, 1),                      # Near ankle: 10cm W × 6cm D
        (14, 2, 2),                     # Mid shin: 10cm W × 10cm D
        (21, 2, 3),                     # Below knee: 10cm W × 14cm D
    ])
    profile(-4, 0, [
        (6, 2, 1),
        (14, 2, 2),
        (21, 2, 3),
    ])

    # ══════════════════════════════════════════════════════
    # ANKLES  z4-5
    # ══════════════════════════════════════════════════════
    fb(-1, 2, 4, 2, 6, 5)              # R
    fb(-1, -6, 4, 2, -2, 5)            # L

    # ══════════════════════════════════════════════════════
    # FEET  z0-3 — small, forward-pointing
    # ══════════════════════════════════════════════════════
    fb(-2, 2, 0, 5, 6, 1)              # R sole: 16cm fwd
    fb(-1, 2, 2, 3, 6, 3)              # R top: 10cm
    fb(-2, -6, 0, 5, -2, 1)            # L sole
    fb(-1, -6, 2, 3, -2, 3)            # L top

    # ══════════════════════════════════════════════════════
    # UPPER ARMS  z56-70
    # Center: R y=13, L y=-13
    # ══════════════════════════════════════════════════════
    profile(13, 0, [
        (56, 2, 2),                     # Elbow: 10cm × 10cm
        (70, 2, 2),                     # Shoulder: same
    ])
    profile(-13, 0, [
        (56, 2, 2),
        (70, 2, 2),
    ])

    # ══════════════════════════════════════════════════════
    # FOREARMS  z40-55 — thins toward wrist
    # ══════════════════════════════════════════════════════
    profile(13, 0, [
        (40, 1, 1),                     # Wrist: 6cm × 6cm
        (48, 2, 1),                     # Mid forearm
        (55, 2, 2),                     # Near elbow: 10cm × 10cm
    ])
    profile(-13, 0, [
        (40, 1, 1),
        (48, 2, 1),
        (55, 2, 2),
    ])

    # ══════════════════════════════════════════════════════
    # HANDS  z34-39 — palm + 4 fingers + thumb
    # ══════════════════════════════════════════════════════

    # R hand
    fb(0, 12, 36, 1, 14, 39)           # Palm 2d × 3w × 4h
    fb(0, 11, 34, 0, 11, 36)           # Pinky
    fb(0, 12, 34, 0, 12, 36)           # Ring
    fb(0, 13, 34, 0, 13, 36)           # Middle
    fb(0, 14, 34, 0, 14, 36)           # Index
    fb(2, 15, 37, 2, 15, 38)           # Thumb

    # L hand
    fb(0, -14, 36, 1, -12, 39)         # Palm
    fb(0, -14, 34, 0, -14, 36)         # Index
    fb(0, -13, 34, 0, -13, 36)         # Middle
    fb(0, -12, 34, 0, -12, 36)         # Ring
    fb(0, -11, 34, 0, -11, 36)         # Pinky
    fb(2, -15, 37, 2, -15, 38)         # Thumb

    return g


# ═════════════════════════════════════════════════════════════
# VOXEL MESH GENERATOR
# ═════════════════════════════════════════════════════════════

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


# ═════════════════════════════════════════════════════════════
# MAIN
# ═════════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D — Crew v6 — 2cm Micro-voxels")
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
    verts, faces = generate_mesh(grid)

    me = bpy.data.meshes.new("SM_Crew")
    me.from_pydata(verts, [], faces)
    me.validate()
    me.update(calc_edges=True)

    ob = bpy.data.objects.new("SM_Crew", me)
    bpy.context.collection.objects.link(ob)

    # Clay material — warm neutral
    m = bpy.data.materials.new("M_Clay")
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.76, 0.70, 0.63, 1)
        bsdf.inputs["Roughness"].default_value = 0.92
        bsdf.inputs["Metallic"].default_value = 0.0
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

    print(f"\n  Anatomy:")
    print(f"    Crotch:   z=44 → {44*VOXEL}cm = {44*VOXEL/180*100:.0f}%")
    print(f"    Shoulder: z=68 → {68*VOXEL}cm (widest)")
    print(f"    Head:     z=78-89 → {78*VOXEL}-{90*VOXEL}cm")

    print(f"\n  Profile taper (front view Y):")
    print(f"    Hips:     yw=7  → {7*2*2+2}cm")
    print(f"    Waist:    yw=6  → {6*2*2+2}cm")
    print(f"    Chest:    yw=9  → {9*2*2+2}cm")
    print(f"    Shoulder: yw=11 → {11*2*2+2}cm")
    print(f"    Neck:     yw=2  → {2*2*2+2}cm")

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'SOLID'
                    sp.shading.color_type = 'MATERIAL'
            break

    print("\n" + "=" * 60)
    print("2cm voxels. Profile interpolation. Clay shading.")
    print("=" * 60)


main()
