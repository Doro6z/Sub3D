"""
Sub3D — Station Blockouts
===========================
Simple geometry for each gameplay station.
Run AFTER main.py (additive — doesn't clear the scene).

Blender > Scripting > Open > Alt+P
"""

import bpy
import math
from mathutils import Vector

LENGTH = 4200.0
DECK_MAIN_Z = -20.0

# ─── Station definitions ───
# Each station: name, position (x_norm, y_offset, z), size (w, d, h), type
# Types affect shape: console, column, seat, table, rack

STATIONS = [
    # ═══ COMMAND COMPARTMENT (x: 0.15 → 0.40) ═══
    {"name": "Helm_Console",
     "x_norm": 0.22, "y": 0, "z": DECK_MAIN_Z,
     "w": 160, "d": 80, "h": 120,
     "type": "console",
     "desc": "Helm station — steering + depth control"},

    {"name": "Sonar_Console",
     "x_norm": 0.28, "y": -120, "z": DECK_MAIN_Z,
     "w": 120, "d": 60, "h": 140,
     "type": "console",
     "desc": "Sonar station — port side"},

    {"name": "Nav_Table",
     "x_norm": 0.28, "y": 120, "z": DECK_MAIN_Z,
     "w": 140, "d": 90, "h": 85,
     "type": "table",
     "desc": "Navigation table — starboard side"},

    {"name": "Periscope",
     "x_norm": 0.32, "y": 0, "z": DECK_MAIN_Z,
     "w": 40, "d": 40, "h": 250,
     "type": "column",
     "desc": "Periscope column — center"},

    {"name": "Comms_Console",
     "x_norm": 0.36, "y": -100, "z": DECK_MAIN_Z,
     "w": 100, "d": 50, "h": 160,
     "type": "console",
     "desc": "Communications — port side"},

    # ═══ BAIE CENTRALE (x: 0.40 → 0.62) ═══
    {"name": "Ballast_Panel",
     "x_norm": 0.44, "y": -140, "z": DECK_MAIN_Z,
     "w": 80, "d": 40, "h": 180,
     "type": "console",
     "desc": "Ballast control panel — port"},

    {"name": "Damage_Control",
     "x_norm": 0.44, "y": 140, "z": DECK_MAIN_Z,
     "w": 80, "d": 40, "h": 180,
     "type": "console",
     "desc": "Damage control panel — starboard"},

    {"name": "Central_Table",
     "x_norm": 0.50, "y": 0, "z": DECK_MAIN_Z,
     "w": 180, "d": 100, "h": 80,
     "type": "table",
     "desc": "Central operations table"},

    {"name": "Weapons_Console",
     "x_norm": 0.56, "y": -110, "z": DECK_MAIN_Z,
     "w": 100, "d": 50, "h": 140,
     "type": "console",
     "desc": "Weapons/torpedo control — port"},

    # ═══ ENGINE COMPARTMENT (x: 0.62 → 0.82) ═══
    {"name": "Engine_Console",
     "x_norm": 0.68, "y": 0, "z": DECK_MAIN_Z,
     "w": 140, "d": 70, "h": 150,
     "type": "console",
     "desc": "Engine control station"},

    {"name": "Engine_Block_Port",
     "x_norm": 0.74, "y": -100, "z": DECK_MAIN_Z,
     "w": 200, "d": 150, "h": 160,
     "type": "machinery",
     "desc": "Engine block — port"},

    {"name": "Engine_Block_Stbd",
     "x_norm": 0.74, "y": 100, "z": DECK_MAIN_Z,
     "w": 200, "d": 150, "h": 160,
     "type": "machinery",
     "desc": "Engine block — starboard"},

    # ═══ TORPEDO COMPARTMENT (x: 0.00 → 0.15) ═══
    {"name": "Torpedo_Rack_Port",
     "x_norm": 0.06, "y": -80, "z": DECK_MAIN_Z,
     "w": 250, "d": 60, "h": 100,
     "type": "rack",
     "desc": "Torpedo storage rack — port"},

    {"name": "Torpedo_Rack_Stbd",
     "x_norm": 0.06, "y": 80, "z": DECK_MAIN_Z,
     "w": 250, "d": 60, "h": 100,
     "type": "rack",
     "desc": "Torpedo storage rack — starboard"},

    {"name": "Torpedo_Tubes",
     "x_norm": 0.02, "y": 0, "z": DECK_MAIN_Z,
     "w": 60, "d": 160, "h": 120,
     "type": "machinery",
     "desc": "Torpedo tube breech doors"},
]

# ─── Colors per type ───
TYPE_COLORS = {
    "console":   (0.25, 0.35, 0.30),  # Dark teal
    "table":     (0.40, 0.35, 0.25),  # Warm wood
    "column":    (0.30, 0.30, 0.30),  # Neutral gray
    "rack":      (0.35, 0.30, 0.25),  # Metal brown
    "machinery": (0.22, 0.25, 0.22),  # Dark green-gray
    "seat":      (0.30, 0.25, 0.20),  # Leather brown
}


def simple_mat(name, r, g, b):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = 0.8
    return m


def build_station(st):
    """Build a station blockout based on its type."""
    x = st["x_norm"] * LENGTH
    y = st["y"]
    z = st["z"]
    w = st["w"]  # Width (Y axis)
    d = st["d"]  # Depth (X axis)
    h = st["h"]  # Height (Z axis)
    stype = st["type"]
    name = f"ST_{st['name']}"
    color = TYPE_COLORS.get(stype, (0.3, 0.3, 0.3))

    if stype == "console":
        # Angled console: vertical back panel + angled desk surface
        v = [
            # Base (floor level)
            (x-d/2, y-w/2, z), (x+d/2, y-w/2, z),
            (x+d/2, y+w/2, z), (x-d/2, y+w/2, z),
            # Back panel top
            (x-d/2, y-w/2, z+h), (x-d/2+10, y-w/2, z+h),
            (x-d/2+10, y+w/2, z+h), (x-d/2, y+w/2, z+h),
            # Desk surface front edge (angled, lower)
            (x+d/2, y-w/2, z+h*0.55), (x+d/2, y+w/2, z+h*0.55),
            # Desk surface back edge (meets panel)
            (x-d/2+10, y-w/2, z+h*0.75), (x-d/2+10, y+w/2, z+h*0.75),
        ]
        f = [
            (0,1,2,3),    # Bottom
            (0,4,7,3),    # Back panel outer
            (4,5,6,7),    # Back panel top edge
            (8,9,11,10),  # Desk surface (angled)
            (1,8,9,2),    # Front face
            (0,1,8,10),   # Side right → connect base to desk
            (3,2,9,11),   # Side left
            (5,10,11,6),  # Panel to desk connection
        ]

    elif stype == "table":
        # Simple table: flat top on 4 legs
        leg = 8
        v = [
            # Table top
            (x-d/2, y-w/2, z+h), (x+d/2, y-w/2, z+h),
            (x+d/2, y+w/2, z+h), (x-d/2, y+w/2, z+h),
            # Table top bottom
            (x-d/2, y-w/2, z+h-5), (x+d/2, y-w/2, z+h-5),
            (x+d/2, y+w/2, z+h-5), (x-d/2, y+w/2, z+h-5),
        ]
        f = [
            (0,1,2,3), (4,7,6,5),
            (0,4,5,1), (2,6,7,3),
            (0,3,7,4), (1,5,6,2),
        ]
        # 4 legs
        for lx, ly in [(-d/2+leg, -w/2+leg), (d/2-leg, -w/2+leg),
                        (d/2-leg, w/2-leg), (-d/2+leg, w/2-leg)]:
            base = len(v)
            v += [
                (x+lx-leg/2, y+ly-leg/2, z), (x+lx+leg/2, y+ly-leg/2, z),
                (x+lx+leg/2, y+ly+leg/2, z), (x+lx-leg/2, y+ly+leg/2, z),
                (x+lx-leg/2, y+ly-leg/2, z+h-5), (x+lx+leg/2, y+ly-leg/2, z+h-5),
                (x+lx+leg/2, y+ly+leg/2, z+h-5), (x+lx-leg/2, y+ly+leg/2, z+h-5),
            ]
            b = base
            f += [(b,b+1,b+2,b+3),(b+4,b+7,b+6,b+5),
                  (b,b+4,b+5,b+1),(b+2,b+6,b+7,b+3),
                  (b,b+3,b+7,b+4),(b+1,b+5,b+6,b+2)]

    elif stype == "column":
        # Vertical cylinder (8-sided)
        n = 8
        v = []
        f_list = []
        for iz in range(2):
            zz = z if iz == 0 else z + h
            for i in range(n):
                a = 2*math.pi*i/n
                v.append((x + d/2*math.cos(a), y + d/2*math.sin(a), zz))
        for i in range(n):
            ni = (i+1)%n
            f_list.append((i, ni, n+ni, n+i))
        # Top cap
        tc = len(v); v.append((x, y, z+h))
        for i in range(n): f_list.append((tc, n+i, n+(i+1)%n))
        f = f_list

    elif stype == "machinery":
        # Chunky box with chamfered top edges
        v = [
            (x-d/2, y-w/2, z), (x+d/2, y-w/2, z),
            (x+d/2, y+w/2, z), (x-d/2, y+w/2, z),
            (x-d/2+15, y-w/2+15, z+h), (x+d/2-15, y-w/2+15, z+h),
            (x+d/2-15, y+w/2-15, z+h), (x-d/2+15, y+w/2-15, z+h),
        ]
        f = [
            (0,1,2,3), (4,7,6,5),
            (0,4,5,1), (2,6,7,3),
            (0,3,7,4), (1,5,6,2),
        ]

    else:  # rack, seat, generic
        # Simple box
        v = [
            (x-d/2, y-w/2, z), (x+d/2, y-w/2, z),
            (x+d/2, y+w/2, z), (x-d/2, y+w/2, z),
            (x-d/2, y-w/2, z+h), (x+d/2, y-w/2, z+h),
            (x+d/2, y+w/2, z+h), (x-d/2, y+w/2, z+h),
        ]
        f = [
            (0,1,2,3), (4,7,6,5),
            (0,4,5,1), (2,6,7,3),
            (0,3,7,4), (1,5,6,2),
        ]

    me = bpy.data.meshes.new(name)
    me.from_pydata(v, [], f)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(simple_mat(name, *color))
    # Custom property for UE
    ob["station_type"] = stype
    ob["station_desc"] = st["desc"]
    return ob


def main():
    print("\n" + "="*60)
    print("Sub3D — Station Blockouts")
    print("="*60)

    for st in STATIONS:
        ob = build_station(st)
        print(f"  {ob.name}: {st['type']} at X={st['x_norm']*LENGTH:.0f} [{st['desc']}]")

    print(f"\n  {len(STATIONS)} stations created.")
    print("  Custom properties: station_type, station_desc")
    print("  Move/scale/duplicate freely in Blender.")

main()
