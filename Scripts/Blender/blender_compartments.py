"""
Sub3D — Modular Compartment Assembly
======================================
10 compartments as individual box-rooms.
Each is a hollow box with door openings on fore/aft walls.
Assemble them to build a submarine interior.

Each compartment is ONE Blender object you can move, scale, delete walls.
The hull wraps around the assembly (generated separately or by v8 script).

All in centimeters. X=forward, Y=lateral, Z=up.
"""

import bpy
import bmesh
import math
from mathutils import Vector

# ═══════════════════════════════════════════════════════════════
# COMPARTMENT DEFINITIONS
# ═══════════════════════════════════════════════════════════════

WALL = 14.0          # Wall thickness (applied via solidify)
DOOR_W = 90.0        # Standard door width
DOOR_H = 185.0       # Standard door height

# Each compartment: name, position, size, connections
# pos = (x, y, z) of the FLOOR CENTER
# size = (length_x, width_y, height_z) INTERIOR dimensions
# doors: which faces have door openings
#   "fore" = -X face, "aft" = +X face
#   "port" = -Y face, "stbd" = +Y face
#   (floor/ceiling connections are hatches, not modeled — actors in UE)

COMPARTMENTS = [
    # ─── MAIN DECK (z_floor = 0) ───
    {
        "name": "Torpedo",
        "pos": (250, 0, 0),
        "size": (400, 280, 200),
        "doors": ["aft"],             # Only exit toward stern
        "color": (0.30, 0.32, 0.35),
    },
    {
        "name": "Sonar",
        "pos": (650, 0, 0),
        "size": (350, 360, 200),
        "doors": ["fore", "aft"],
        "color": (0.32, 0.34, 0.32),
    },
    {
        "name": "CrewQuarters",
        "pos": (1100, 0, 0),
        "size": (500, 480, 200),
        "doors": ["fore", "aft"],
        "color": (0.38, 0.36, 0.30),
    },
    {
        "name": "MedBay",
        "pos": (1600, 0, 0),
        "size": (400, 480, 200),
        "doors": ["fore", "aft"],
        "color": (0.35, 0.38, 0.36),
    },
    {
        "name": "Engine",
        "pos": (2100, 0, 0),
        "size": (400, 400, 200),
        "doors": ["fore", "aft"],
        "color": (0.28, 0.28, 0.26),
    },
    {
        "name": "Propulsion",
        "pos": (2500, 0, 0),
        "size": (350, 300, 200),
        "doors": ["fore"],            # Dead end at stern
        "color": (0.26, 0.27, 0.25),
    },

    # ─── UPPER DECK (stacked on CrewQuarters / MedBay) ───
    {
        "name": "Navigation",
        "pos": (1100, 0, 218),        # 200 height + 18 deck
        "size": (500, 380, 195),
        "doors": ["fore", "aft"],
        "color": (0.40, 0.38, 0.34),
    },
    {
        "name": "Bridge",
        "pos": (1600, 0, 218),
        "size": (400, 400, 195),
        "doors": ["fore", "aft"],
        "color": (0.42, 0.40, 0.36),
    },

    # ─── LOWER DECK (below CrewQuarters / MedBay) ───
    {
        "name": "Machinery",
        "pos": (1100, 0, -218),       # Below main deck
        "size": (500, 420, 180),
        "doors": ["fore", "aft"],
        "color": (0.25, 0.25, 0.23),
    },
    {
        "name": "Reactor",
        "pos": (1600, 0, -218),
        "size": (400, 440, 180),
        "doors": ["fore", "aft"],
        "color": (0.22, 0.24, 0.22),
    },
]

# SAS: small room on top of Bridge, accessible from upper deck
SAS = {
    "name": "SAS",
    "pos": (1600, 0, 431),            # On top of Bridge (218 + 195 + 18)
    "size": (250, 220, 210),
    "doors": [],                       # No horizontal doors — vertical access only
    "color": (0.30, 0.28, 0.25),
}

ALL_COMPARTMENTS = COMPARTMENTS + [SAS]


# ═══════════════════════════════════════════════════════════════
# BUILD A SINGLE COMPARTMENT
# ═══════════════════════════════════════════════════════════════

def build_compartment(comp):
    """
    Build one compartment as a hollow box with door openings.

    The box has 6 faces: floor, ceiling, fore wall, aft wall, port wall, stbd wall.
    Fore/aft walls get door cutouts if specified in comp["doors"].
    """
    name = f"SM_Comp_{comp['name']}"
    cx, cy, cz = comp["pos"]
    lx, wy, hz = comp["size"]
    doors = comp["doors"]
    color = comp["color"]

    hl = lx / 2   # Half-length (X)
    hw = wy / 2   # Half-width (Y)

    bm = bmesh.new()

    def add_solid_wall(verts_2d, normal_axis):
        """Add a flat quad wall from 4 corner positions."""
        bm_verts = [bm.verts.new(v) for v in verts_2d]
        bm.faces.new(bm_verts)

    def add_wall_with_door(x_pos, y_min, y_max, z_floor, z_ceil, door_w, door_h):
        """Build a wall at constant X with a centered door opening."""
        gy, gz = 12, 10
        door_sill = z_floor
        grid = {}
        for iz in range(gz + 1):
            z = z_floor + (z_ceil - z_floor) * iz / gz
            for iy in range(gy + 1):
                y = y_min + (y_max - y_min) * iy / gy
                # Skip door region
                if -door_w/2 < y < door_w/2 and door_sill < z < door_sill + door_h:
                    continue
                grid[(iy, iz)] = bm.verts.new((x_pos, y, z))

        for iz in range(gz):
            for iy in range(gy):
                vs = [grid.get(k) for k in [(iy,iz),(iy+1,iz),(iy+1,iz+1),(iy,iz+1)]]
                if all(vs):
                    try:
                        bm.faces.new(vs)
                    except:
                        pass

    def add_solid_rect(corners):
        """Add a simple quad from 4 Vector corners."""
        vs = [bm.verts.new(c) for c in corners]
        bm.faces.new(vs)

    # ─── Floor ───
    add_solid_rect([
        (cx - hl, -hw, cz),
        (cx + hl, -hw, cz),
        (cx + hl,  hw, cz),
        (cx - hl,  hw, cz),
    ])

    # ─── Ceiling ───
    add_solid_rect([
        (cx - hl, -hw, cz + hz),
        (cx - hl,  hw, cz + hz),
        (cx + hl,  hw, cz + hz),
        (cx + hl, -hw, cz + hz),
    ])

    # ─── Port wall (Y = -hw) ───
    add_solid_rect([
        (cx - hl, -hw, cz),
        (cx + hl, -hw, cz),
        (cx + hl, -hw, cz + hz),
        (cx - hl, -hw, cz + hz),
    ])

    # ─── Starboard wall (Y = +hw) ───
    add_solid_rect([
        (cx - hl, hw, cz),
        (cx - hl, hw, cz + hz),
        (cx + hl, hw, cz + hz),
        (cx + hl, hw, cz),
    ])

    # ─── Fore wall (X = cx - hl) — with or without door ───
    if "fore" in doors:
        add_wall_with_door(cx - hl, -hw, hw, cz, cz + hz, DOOR_W, DOOR_H)
    else:
        add_solid_rect([
            (cx - hl, -hw, cz),
            (cx - hl,  hw, cz),
            (cx - hl,  hw, cz + hz),
            (cx - hl, -hw, cz + hz),
        ])

    # ─── Aft wall (X = cx + hl) — with or without door ───
    if "aft" in doors:
        add_wall_with_door(cx + hl, -hw, hw, cz, cz + hz, DOOR_W, DOOR_H)
    else:
        add_solid_rect([
            (cx + hl, -hw, cz),
            (cx + hl, -hw, cz + hz),
            (cx + hl,  hw, cz + hz),
            (cx + hl,  hw, cz),
        ])

    bm.verts.ensure_lookup_table()

    # Create mesh
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.update()

    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)

    # Material
    mat = bpy.data.materials.new(name=f"M_{comp['name']}")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1)
        bsdf.inputs["Roughness"].default_value = 0.75
        bsdf.inputs["Metallic"].default_value = 0.4
    ob.data.materials.append(mat)

    # Solidify for wall thickness
    mod = ob.modifiers.new("Wall", 'SOLIDIFY')
    mod.thickness = WALL
    mod.offset = -1  # Grow inward
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)

    # Normals
    ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    ob.select_set(False)

    return ob


# ═══════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D — Modular Compartment Assembly")
    print("10 compartments + SAS. Each is an independent object.")
    print("=" * 60)

    # Clear
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
                    sp.clip_start = 1
                    sp.clip_end = 500000

    # Layout display
    print(f"\nLayout:")
    print(f"  {'Name':14s} {'Position':>20s} {'Size':>20s} Doors")
    print(f"  {'─'*14} {'─'*20} {'─'*20} {'─'*15}")
    for c in ALL_COMPARTMENTS:
        pos = f"({c['pos'][0]:5.0f},{c['pos'][1]:4.0f},{c['pos'][2]:5.0f})"
        size = f"({c['size'][0]:3.0f}x{c['size'][1]:3.0f}x{c['size'][2]:3.0f})"
        doors = ",".join(c["doors"]) if c["doors"] else "none (vertical)"
        print(f"  {c['name']:14s} {pos:>20s} {size:>20s} {doors}")

    # Build all compartments
    print(f"\n--- Building {len(ALL_COMPARTMENTS)} compartments ---")
    for comp in ALL_COMPARTMENTS:
        ob = build_compartment(comp)
        cx, cy, cz = comp["pos"]
        lx, wy, hz = comp["size"]
        print(f"  {ob.name}: {len(ob.data.vertices)}V {len(ob.data.polygons)}F "
              f"at ({cx:.0f}, {cy:.0f}, {cz:.0f})")

    # Frame view
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    print("DONE. Each compartment is a separate object.")
    print("")
    print("To use in Blender:")
    print("  - Move compartments: G then X/Y/Z")
    print("  - Duplicate: Shift+D")
    print("  - Delete a wall: Tab (edit mode) > select face > X > Faces")
    print("  - Add door to side wall: edit mode > select wall face >")
    print("    Knife tool (K) > cut opening > delete inner face")
    print("")
    print("To add hull: run blender_blockout_v8.py in same scene")
    print("  (it wraps a hull around whatever extents exist)")
    print("=" * 60)


if __name__ == "__main__":
    main()
