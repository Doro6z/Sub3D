"""
Sub3D — Fauna: The Stalker
===========================
Lovecraftian / mantis-shrimp / alien hybrid.
Appears behind the submarine and follows it silently.

Anatomy (head at +X, fan tail at -X):
  - Uropod fan tail
  - 5 articulated abdomen segments (curving upward toward thorax)
  - Broad thorax with 4 pairs of walking legs
  - Large cephalothorax with stalked compound eyes
  - Forward rostrum and paired raptorial claws
  - Lovecraftian trailing tentacles from head underside
  - Bioluminescent dorsal stripe

2cm voxels. Blender > Scripting > Alt+P
"""

import bpy

VOXEL = 2

# Material indices
CHITIN    = 0   # Main shell — dark slate teal
CHITIN_DK = 1   # Joint lines, underplates, segment borders — darker teal
EYE       = 2   # Compound stalked eyes — deep crimson
GLOW      = 3   # Dorsal bioluminescent stripe — pale blue-green
CLAW      = 4   # Raptorial claws — slightly lighter chitin
SOFT      = 5   # Soft membrane underside — pale pinkish-grey
TENTACLE  = 6   # Lovecraftian head tentacles — dark grey-violet

FACE_DEFS = [
    ((1,0,0),  [(1,0,0),(1,1,0),(1,1,1),(1,0,1)]),
    ((-1,0,0), [(0,0,0),(0,0,1),(0,1,1),(0,1,0)]),
    ((0,1,0),  [(0,1,0),(0,1,1),(1,1,1),(1,1,0)]),
    ((0,-1,0), [(0,0,0),(1,0,0),(1,0,1),(0,0,1)]),
    ((0,0,1),  [(0,0,1),(1,0,1),(1,1,1),(0,1,1)]),
    ((0,0,-1), [(0,0,0),(0,1,0),(1,1,0),(1,0,0)]),
]


def _fill(g, x0, y0, z0, x1, y1, z1, mat):
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                g[(x, y, z)] = mat


def build_stalker():
    g = {}
    F = lambda x0,y0,z0,x1,y1,z1,m: _fill(g,x0,y0,z0,x1,y1,z1,m)

    # ── Uropod fan tail ───────────────────────────────
    # Central tail plate
    F(-30,-3, 2, -22, 3, 8, CHITIN)
    # 5 fan blades radiating outward in Y and Z
    fan_defs = [
        # (y_lo, y_hi, z_lo, z_hi) — symmetric left/right
        ( 4,  6, 3,  7),
        ( 6,  9, 4,  7),
        ( 8, 11, 4,  6),
    ]
    for y0, y1, z0, z1 in fan_defs:
        F(-34,-y1, z0, -26,-y0, z1, CHITIN_DK)  # left blade
        F(-34, y0, z0, -26, y1, z1, CHITIN_DK)  # right blade
    # Top center fan blade
    F(-34,-2, 8, -26, 2,12, CHITIN_DK)
    # Segment border at tail root
    F(-22,-3, 2, -22, 3, 8, CHITIN_DK)

    # ── Abdomen — 5 segments, body curves upward ──────
    # (x0, x1, y_half_width, z_floor, z_height)
    segs = [
        (-22,-18, 3, 1, 7),
        (-18,-14, 4, 2, 8),
        (-14,-10, 5, 3, 9),
        (-10, -6, 5, 4,10),
        ( -6, -2, 5, 4,11),
    ]
    for x0, x1, yw, zb, zh in segs:
        F(x0,-yw,zb, x1, yw,zb+zh, CHITIN)
        # Dorsal ridge plate on top
        F(x0,-2,zb+zh, x1, 2,zb+zh+1, CHITIN_DK)
        # Segment articulation lines on sides
        F(x1,-yw,zb+2, x1,  yw,zb+zh-2, CHITIN_DK)
        F(x0,-yw,zb+2, x0+1,yw,zb+3,    CHITIN_DK)
        # Soft underside
        F(x0,-yw+1,zb, x1, yw-1,zb+1, SOFT)

    # ── Thorax ────────────────────────────────────────
    F(-2,-6, 4, 12, 6,16, CHITIN)
    # Dorsal scutes
    for x in range(-2, 12, 3):
        F(x,-5,14, x+2, 5,16, CHITIN_DK)
    # Lateral carinae
    F(-2,-6, 8, 12,-6,14, CHITIN_DK)
    F(-2, 6, 8, 12, 6,14, CHITIN_DK)
    # Soft sternal underside
    F(-2,-4, 3, 12, 4, 5, SOFT)

    # ── Cephalothorax (head) ──────────────────────────
    F(12,-5, 6, 22, 5,20, CHITIN)
    # Top and rear head plates
    F(14,-5,18, 22, 5,20, CHITIN_DK)
    F(12,-6, 8, 14, 6,18, CHITIN_DK)   # posterior collar
    # Rostrum — forward spike
    F(22,-2, 9, 28, 2,13, CHITIN)
    F(26,-1,10, 31, 1,12, CHITIN_DK)   # rostrum tip

    # ── Stalked compound eyes ─────────────────────────
    # Eye stalks protrude up from head
    F(16,-6,18, 18,-5,23, CHITIN_DK)   # left stalk
    F(16, 5,18, 18, 6,23, CHITIN_DK)   # right stalk
    # Compound eye bulbs (bulging, larger than the stalk)
    F(15,-9,20, 21,-5,25, EYE)
    F(15, 5,20, 21, 9,25, EYE)
    # Specular highlight dot on each eye
    _fill(g, 20,-7,23, 21,-6,24, GLOW)
    _fill(g, 20, 6,23, 21, 7,24, GLOW)

    # ── Raptorial claws (chelicerae) ──────────────────
    # Upper pair — striking claws
    F(22,-4, 6, 30,-2,10, CLAW)
    F(22, 2, 6, 30, 4,10, CLAW)
    # Claw arm extension
    F(28,-5, 7, 34,-3, 9, CLAW)
    F(28, 3, 7, 34, 5, 9, CLAW)
    # Notched claw tips (two opposing blades)
    F(33,-5, 8, 37,-4,10, CHITIN_DK)
    F(33,-3, 6, 37,-2, 8, CHITIN_DK)
    F(33, 4, 8, 37, 5,10, CHITIN_DK)
    F(33, 2, 6, 37, 3, 8, CHITIN_DK)

    # ── Walking legs — 4 pairs on thorax ─────────────
    leg_x_positions = [-1, 2, 6, 9]
    for lx in leg_x_positions:
        # Right side
        F(lx, 6, 4, lx+1,10, 9, CHITIN_DK)    # upper limb
        F(lx, 9,-6, lx+1,12, 4, CHITIN_DK)    # lower limb
        F(lx,11,-7, lx+2,13,-5, CHITIN)        # foot pad
        # Left side (mirror Y)
        F(lx,-10, 4, lx+1,-6, 9, CHITIN_DK)
        F(lx,-12,-6, lx+1,-9, 4, CHITIN_DK)
        F(lx,-13,-7, lx+2,-11,-5, CHITIN)

    # ── Maxillipeds (smaller feeding appendages) ──────
    F(14,-4, 3, 20,-3, 7, CHITIN_DK)
    F(14, 3, 3, 20, 4, 7, CHITIN_DK)

    # ── Bioluminescent dorsal stripe ──────────────────
    # Runs along the spine from tail to head.
    for x in range(-20, 14, 2):
        F(x,-1,14, x+1, 1,16, GLOW)

    # ── Lovecraftian tentacles — head underside ───────
    # Writhing appendages that trail below the cephalothorax.
    tentacle_roots = [
        (13,-3, 5), (15,-3, 4), (17,-2, 4), (19,-1, 4),
        (13, 2, 5), (15, 2, 4), (17, 1, 4), (19, 0, 3),
    ]
    for i, (tx, ty, tz) in enumerate(tentacle_roots):
        length = 9 + (i % 4)
        for step in range(length):
            # Drape down and curl slightly outward
            dz = -step
            dy = (1 if ty >= 0 else -1) * (step // 4)
            key = (tx, ty + dy, tz + dz)
            if key not in g:
                g[key] = TENTACLE
        # Tentacle tip
        tip_z = tz - length
        tip_y = ty + (1 if ty >= 0 else -1) * (length // 4)
        g[(tx, tip_y, tip_z)] = CHITIN_DK  # chitinous hook at tip

    return g


# ═══════════════════════════════════════════════════════════
# MESH + BLENDER HELPERS
# ═══════════════════════════════════════════════════════════

def generate_mesh_mat(grid_dict, voxel_size=2):
    half = voxel_size / 2
    cache = {}; verts = []; faces = []; fmats = []
    def vid(gx, gy, gz):
        key = (gx, gy, gz)
        if key not in cache:
            cache[key] = len(verts)
            verts.append((gx*voxel_size - half, gy*voxel_size - half, gz*voxel_size))
        return cache[key]
    for (vx, vy, vz), mat in grid_dict.items():
        for (dx,dy,dz), corners in FACE_DEFS:
            if (vx+dx, vy+dy, vz+dz) not in grid_dict:
                faces.append(tuple(vid(vx+cx, vy+cy, vz+cz) for cx,cy,cz in corners))
                fmats.append(mat)
    return verts, faces, fmats


def make_mat(name, r, g, b, metallic=0.0, roughness=0.85, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if emit > 0:
            try:
                bsdf.inputs["Emission Color"].default_value = (r, g, b, 1)
                bsdf.inputs["Emission Strength"].default_value = emit
            except KeyError:
                pass
    return m


def spawn(grid_dict, name, mats, loc=(0, 0, 0)):
    verts, faces, fmats = generate_mesh_mat(grid_dict, VOXEL)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate(); me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    for m in mats:
        ob.data.materials.append(m)
    for i, poly in enumerate(me.polygons):
        poly.material_index = fmats[i]
    ob.location = loc
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_flat()
    ob.select_set(False)
    return ob


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D Fauna — The Stalker")
    print("=" * 60)

    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for d in bpy.data.meshes:
        if d.users == 0: bpy.data.meshes.remove(d)
    for d in bpy.data.materials:
        if d.users == 0: bpy.data.materials.remove(d)

    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 0.1
                    sp.clip_end = 100000

    mats = [
        make_mat("ST_Chitin",   0.10, 0.16, 0.18, metallic=0.1, roughness=0.6),
        make_mat("ST_ChitinDk", 0.05, 0.09, 0.10, metallic=0.1, roughness=0.7),
        make_mat("ST_Eye",      0.55, 0.03, 0.03, roughness=0.15),
        make_mat("ST_Glow",     0.20, 0.82, 0.72, roughness=0.3, emit=2.5),
        make_mat("ST_Claw",     0.18, 0.24, 0.26, metallic=0.2, roughness=0.5),
        make_mat("ST_Soft",     0.42, 0.34, 0.30, roughness=0.95),
        make_mat("ST_Tentacle", 0.20, 0.12, 0.24, roughness=0.9),
    ]

    creature = build_stalker()
    spawn(creature, "Stalker", mats, loc=(0, 0, 0))

    voxel_count = len(creature)
    print(f"  Stalker built — {voxel_count} voxels")

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'MATERIAL'
            break

    print("=" * 60)
    print("""
  STALKER — DESIGN NOTES
  ───────────────────────
  Orientation: head (+X) / tail (-X). View from +Y for side profile.

  Key anatomy:
    Fan tail       x=-34..-22  uropod blades, top-center blade
    Abdomen x5     x=-22..-2   segments curving upward, soft underside
    Thorax         x=-2..12    4 leg pairs, lateral carinae, sternal plate
    Head           x=12..22    stalked compound eyes (EYE=crimson)
    Rostrum        x=22..31    forward spike
    Claws          x=22..37    raptorial, notched tips
    Tentacles      root x=13-19, drape down from head underside
    Glow stripe    dorsal midline, runs abdomen to head (emit=2.5)

  EXPORT: SM_Stalker.fbx — Static Mesh reference for UE5 creature art.
""")


main()
