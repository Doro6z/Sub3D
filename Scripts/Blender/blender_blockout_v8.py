"""
Sub3D — Blockout v8: Gameplay First
====================================
No submarine shape preconception. Start from gameplay needs:

1. DECKS: horizontal plates where crew walks
2. BULKHEADS: vertical walls that divide compartments (with door holes)
3. HULL: one outer shell that wraps everything

That's it. Build from inside out.

Step 1: Place decks (flat plates at different Z heights)
Step 2: Place bulkheads (vertical plates at X positions, with doors)
Step 3: Wrap a hull around everything (convex-ish shell)

All in centimeters. X=forward, Y=lateral, Z=up.
"""

import bpy
import bmesh
import math
from mathutils import Vector


# ═══════════════════════════════════════════════════════════════
# GAMEPLAY LAYOUT — Define the spaces, not the shape
# ═══════════════════════════════════════════════════════════════

# Deck levels
DECKS = {
    "upper": {"z": 160, "thick": 16},
    "main":  {"z": -15, "thick": 16},
    "lower": {"z": -200, "thick": 16},
}

# Compartments: what gameplay needs
# Each has X bounds, which decks exist, and width at each deck
COMPS = [
    {
        "name": "Torpedo",
        "x": (200, 550),
        "decks": ["main"],
        "width": {"main": 300},
        "door_w": 70, "door_h": 160,
    },
    {
        "name": "Sonar",
        "x": (550, 950),
        "decks": ["main", "lower"],
        "width": {"main": 450, "lower": 350},
        "door_w": 80, "door_h": 170,
    },
    {
        "name": "Navigation",
        "x": (950, 1400),
        "decks": ["upper", "main", "lower"],
        "width": {"upper": 250, "main": 500, "lower": 400},
        "door_w": 90, "door_h": 185,
    },
    {
        "name": "Command",
        "x": (1400, 1950),
        "decks": ["upper", "main", "lower"],
        "width": {"upper": 280, "main": 520, "lower": 420},
        "door_w": 95, "door_h": 190,
    },
    {
        "name": "Crew",
        "x": (1950, 2500),
        "decks": ["upper", "main", "lower"],
        "width": {"upper": 260, "main": 500, "lower": 400},
        "door_w": 90, "door_h": 185,
    },
    {
        "name": "Reactor",
        "x": (2500, 3000),
        "decks": ["main", "lower"],
        "width": {"main": 480, "lower": 420},
        "door_w": 100, "door_h": 185,
    },
    {
        "name": "Engine",
        "x": (3000, 3500),
        "decks": ["main", "lower"],
        "width": {"main": 440, "lower": 360},
        "door_w": 85, "door_h": 175,
    },
    {
        "name": "Propulsion",
        "x": (3500, 3900),
        "decks": ["main"],
        "width": {"main": 320},
        "door_w": 75, "door_h": 165,
    },
]

# Hull margin: how much bigger the hull is than the compartments
HULL_MARGIN = 30.0       # cm beyond the widest compartment at each X
HULL_MARGIN_Z_TOP = 40.0
HULL_MARGIN_Z_BOT = 30.0
HULL_THICK = 15.0
BULKHEAD_THICK = 14.0

# How much to extend hull beyond first/last compartment for bow/stern
BOW_EXTENSION = 500.0    # cm of hull before first compartment
STERN_EXTENSION = 400.0

# Hull smoothing
HULL_RINGS = 100
HULL_RADIAL = 32

COLORS = {
    'hull': (0.15, 0.19, 0.17),
    'deck': (0.36, 0.34, 0.30),
    'bulk': (0.46, 0.44, 0.40),
}


# ═══════════════════════════════════════════════════════════════
# STEP 0: Compute the hull envelope from compartment extents
# ═══════════════════════════════════════════════════════════════

def get_extent_at_x(x):
    """What's the widest and tallest compartment content at this X?"""
    max_half_w = 0
    min_z = 0
    max_z = 0

    for comp in COMPS:
        cx0, cx1 = comp["x"]
        if x < cx0 or x > cx1:
            continue

        for deck_name in comp["decks"]:
            deck = DECKS[deck_name]
            half_w = comp["width"][deck_name] / 2
            max_half_w = max(max_half_w, half_w)

            deck_z = deck["z"]
            deck_top = deck_z + 200  # Approximate standing height above deck
            deck_bot = deck_z

            min_z = min(min_z, deck_bot)
            max_z = max(max_z, deck_top)

    return max_half_w, min_z, max_z


def hull_profile_at_x(x):
    """
    Generate a hull cross-section at X based on what compartments need.
    Returns half-width and half-height for an ellipse.
    """
    total_x_min = COMPS[0]["x"][0] - BOW_EXTENSION
    total_x_max = COMPS[-1]["x"][1] + STERN_EXTENSION

    if x <= total_x_min or x >= total_x_max:
        return 0, 0  # Tips

    hw, z_bot, z_top = get_extent_at_x(x)

    if hw <= 0:
        # Outside compartment zone but inside hull — taper
        # Find nearest compartment
        comp_x_min = COMPS[0]["x"][0]
        comp_x_max = COMPS[-1]["x"][1]

        if x < comp_x_min:
            # Bow taper
            t = (x - total_x_min) / BOW_EXTENSION
            t = t * t * (3 - 2 * t)  # smoothstep
            _, _, _ = get_extent_at_x(comp_x_min)
            near_hw, near_zb, near_zt = get_extent_at_x(comp_x_min + 10)
            hw = near_hw * t
            z_bot = near_zb * t
            z_top = near_zt * t
        elif x > comp_x_max:
            # Stern taper
            t = (total_x_max - x) / STERN_EXTENSION
            t = t * t * (3 - 2 * t)
            near_hw, near_zb, near_zt = get_extent_at_x(comp_x_max - 10)
            hw = near_hw * t
            z_bot = near_zb * t
            z_top = near_zt * t
        else:
            # Between compartments — interpolate
            hw = 50
            z_bot = -100
            z_top = 100

    # Add margin
    half_w = hw + HULL_MARGIN
    z_bottom = z_bot - HULL_MARGIN_Z_BOT
    z_top_m = z_top + HULL_MARGIN_Z_TOP

    # Center and radii for ellipse
    center_z = (z_top_m + z_bottom) / 2
    half_h = (z_top_m - z_bottom) / 2

    return half_w, half_h, center_z


# ═══════════════════════════════════════════════════════════════
# BLENDER HELPERS
# ═══════════════════════════════════════════════════════════════

def clear_all():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for b in bpy.data.meshes:
        if b.users == 0: bpy.data.meshes.remove(b)
    for b in bpy.data.materials:
        if b.users == 0: bpy.data.materials.remove(b)


def get_mat(key):
    name = "M_" + key
    m = bpy.data.materials.get(name)
    if not m:
        m = bpy.data.materials.new(name=name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            c = COLORS.get(key, (0.5, 0.5, 0.5))
            bsdf.inputs["Base Color"].default_value = (*c, 1)
            bsdf.inputs["Roughness"].default_value = 0.7
            bsdf.inputs["Metallic"].default_value = 0.5
    return m


def make_obj(name, verts, faces, key, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(get_mat(key))
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    if smooth:
        for p in ob.data.polygons:
            p.use_smooth = True
    ob.select_set(False)
    return ob


# ═══════════════════════════════════════════════════════════════
# STEP 1: DECKS — Flat plates where crew walks
# ═══════════════════════════════════════════════════════════════

def hull_half_width_at(x, z):
    """Max half-width the hull allows at position (x, z). Decks must stay inside this."""
    result = hull_profile_at_x(x)
    if result is None or len(result) < 3:
        return 0
    half_w, half_h, center_z = result
    if half_h <= 0:
        return 0
    # Ellipse: (y/half_w)^2 + ((z-cz)/half_h)^2 = 1 => y = half_w * sqrt(1 - ((z-cz)/half_h)^2)
    dz = (z - center_z) / half_h
    if abs(dz) >= 1.0:
        return 0
    return (half_w - HULL_THICK - 5) * math.sqrt(1.0 - dz * dz)


def build_decks():
    """Build deck plates clipped to hull interior."""
    objs = []

    for deck_name, deck_info in DECKS.items():
        dz = deck_info["z"]
        dt = deck_info["thick"]

        # Find X spans where this deck exists
        spans = []
        cur = None
        for comp in COMPS:
            if deck_name in comp["decks"]:
                if cur is None:
                    cur = [comp["x"][0], comp["x"][1]]
                else:
                    cur[1] = comp["x"][1]
            else:
                if cur:
                    spans.append(cur)
                    cur = None
        if cur:
            spans.append(cur)

        for si, (xs, xe) in enumerate(spans):
            bm = bmesh.new()
            nx = max(8, int((xe - xs) / 60))
            ny = 16

            grid = {}
            for ix in range(nx + 1):
                x = xs + (xe - xs) * ix / nx

                # Deck width: min of compartment width AND hull interior width
                comp_hw = 0
                for comp in COMPS:
                    if comp["x"][0] <= x <= comp["x"][1] and deck_name in comp["decks"]:
                        comp_hw = max(comp_hw, comp["width"][deck_name] / 2)

                hull_hw = hull_half_width_at(x, dz)
                hw = min(comp_hw, hull_hw) if hull_hw > 0 else comp_hw

                if hw < 10:
                    continue

                for iy in range(ny + 1):
                    y = -hw + 2 * hw * iy / ny
                    grid[(ix, iy)] = bm.verts.new((x, y, dz))

            bm.verts.ensure_lookup_table()
            for ix in range(nx):
                for iy in range(ny):
                    vs = [grid.get(k) for k in [(ix,iy),(ix+1,iy),(ix+1,iy+1),(ix,iy+1)]]
                    if all(vs):
                        try:
                            bm.faces.new(vs)
                        except:
                            pass

            name = f"SM_Deck_{deck_name}_{si}"
            me = bpy.data.meshes.new(name)
            bm.to_mesh(me)
            bm.free()
            me.update()
            ob = bpy.data.objects.new(name, me)
            bpy.context.collection.objects.link(ob)
            ob.data.materials.append(get_mat('deck'))

            mod = ob.modifiers.new("S", 'SOLIDIFY')
            mod.thickness = -dt
            mod.offset = 0
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.modifier_apply(modifier=mod.name)
            objs.append(ob)

    return objs


# ═══════════════════════════════════════════════════════════════
# STEP 2: ONE BULKHEAD TEMPLATE — You duplicate in Blender
# ═══════════════════════════════════════════════════════════════

def build_bulkhead_template():
    """
    One single bulkhead with a door cutout, placed at X=0.
    Sized for the largest compartment (main deck width).
    Duplicate and position manually in Blender.
    """
    # Use the widest main deck width, but clipped to hull at X=center
    max_hw = max(c["width"].get("main", 0) for c in COMPS) / 2

    # Height: from lower deck floor to upper deck ceiling
    z_bot = DECKS["lower"]["z"]
    z_top = DECKS["upper"]["z"] + 220

    # Place template at the center of the sub for hull clipping
    center_x = (COMPS[0]["x"][0] + COMPS[-1]["x"][1]) / 2

    dw = 90
    dh = 185
    dsill = DECKS["main"]["z"]

    bm = bmesh.new()
    gy, gz = 20, 18

    grid = {}
    for iz in range(gz + 1):
        z = z_bot + (z_top - z_bot) * iz / gz
        # Clip width to hull at this Z
        hull_hw = hull_half_width_at(center_x, z)
        hw = min(max_hw, hull_hw) if hull_hw > 0 else max_hw
        if hw < 5:
            continue
        for iy in range(gy + 1):
            y = -hw + 2 * hw * iy / gy
            if -dw/2 < y < dw/2 and dsill < z < dsill + dh:
                continue
            grid[(iy, iz)] = bm.verts.new((0, y, z))

    bm.verts.ensure_lookup_table()
    for iz in range(gz):
        for iy in range(gy):
            vs = [grid.get(k) for k in [(iy,iz),(iy+1,iz),(iy+1,iz+1),(iy,iz+1)]]
            if all(vs):
                try:
                    bm.faces.new(vs)
                except:
                    pass

    me = bpy.data.meshes.new("SM_Bulkhead")
    bm.to_mesh(me)
    bm.free()
    me.update()
    ob = bpy.data.objects.new("SM_Bulkhead", me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(get_mat('bulk'))

    mod = ob.modifiers.new("S", 'SOLIDIFY')
    mod.thickness = BULKHEAD_THICK
    mod.offset = 0
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)

    return ob


# ═══════════════════════════════════════════════════════════════
# STEP 3: HULL — Wrap a shell around everything
# ═══════════════════════════════════════════════════════════════

def build_hull():
    """Generate hull as an ellipsoid that wraps all compartment extents."""
    verts = []
    faces = []

    total_x_min = COMPS[0]["x"][0] - BOW_EXTENSION
    total_x_max = COMPS[-1]["x"][1] + STERN_EXTENSION
    total_length = total_x_max - total_x_min

    for i in range(HULL_RINGS + 1):
        t = i / HULL_RINGS
        x = total_x_min + total_length * t

        result = hull_profile_at_x(x)
        if result is None or len(result) < 3:
            half_w, half_h, center_z = 0, 0, 0
        else:
            half_w, half_h, center_z = result

        if half_w < 1:
            half_w = 1
        if half_h < 1:
            half_h = 1

        for seg in range(HULL_RADIAL):
            a = 2 * math.pi * seg / HULL_RADIAL
            y = half_w * math.cos(a)
            z = center_z + half_h * math.sin(a)
            verts.append(Vector((x, y, z)))

    # Connect rings
    for i in range(HULL_RINGS):
        for s in range(HULL_RADIAL):
            ns = (s + 1) % HULL_RADIAL
            a = i * HULL_RADIAL + s
            b = i * HULL_RADIAL + ns
            c = (i + 1) * HULL_RADIAL + ns
            d = (i + 1) * HULL_RADIAL + s
            faces.append((a, b, c, d))

    # Bow cap
    bc = len(verts)
    verts.append(Vector((total_x_min, 0, 0)))
    for s in range(HULL_RADIAL):
        ns = (s + 1) % HULL_RADIAL
        faces.append((bc, ns, s))

    # Stern cap
    sc = len(verts)
    verts.append(Vector((total_x_max, 0, 0)))
    lb = HULL_RINGS * HULL_RADIAL
    for s in range(HULL_RADIAL):
        ns = (s + 1) % HULL_RADIAL
        faces.append((sc, lb + s, lb + ns))

    ob = make_obj("SM_Hull", verts, faces, 'hull')

    # Solidify inward
    mod = ob.modifiers.new("Thick", 'SOLIDIFY')
    mod.thickness = -HULL_THICK
    mod.offset = -1
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)

    return ob


# ═══════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D Blockout v8 — Gameplay First")
    print("Decks + Bulkheads + Hull shell. Nothing else.")
    print("=" * 60)

    clear_all()
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

    print(f"\nDecks:")
    for name, info in DECKS.items():
        print(f"  {name:8s} Z={info['z']:6.0f} thick={info['thick']}")

    print(f"\nCompartments:")
    for c in COMPS:
        print(f"  {c['name']:12s} X=[{c['x'][0]:5.0f},{c['x'][1]:5.0f}] "
              f"decks={c['decks']} door={c['door_w']}x{c['door_h']}")
        for d in c["decks"]:
            print(f"    {d:8s} width={c['width'][d]}")

    print("\n--- Step 1: Decks ---")
    build_decks()

    print("--- Step 2: One bulkhead template at X=0 (duplicate in Blender) ---")
    build_bulkhead_template()

    print("--- Step 3: Hull ---")
    build_hull()

    # Frame view
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "=" * 60)
    print("OBJECTS:")
    tv, tf = 0, 0
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type == 'MESH':
            v, f = len(ob.data.vertices), len(ob.data.polygons)
            tv += v; tf += f
            print(f"  {ob.name}: {v}V {f}F")
    print(f"  TOTAL: {tv}V {tf}F")
    print("=" * 60)


if __name__ == "__main__":
    main()
