"""
Sub3D — Modular Character Kit
===============================
Base mannequin + swappable parts (hats, faces, bodies, beards).
Stylized low-poly. All parts share the same head/body attachment points.

Parts:
  SM_Body_Base       — Mannequin body (all characters share this)
  SM_Head_Base       — Bald head (attachment point for hats/faces/beards)
  SM_Hat_Captain     — Captain's cap
  SM_Hat_Beanie      — Crew beanie
  SM_Hat_Helmet      — Emergency helmet
  SM_Face_Neutral    — Default face (eyes, nose, mouth as simple geometry)
  SM_Face_Angry      — Angry expression
  SM_Face_Worried    — Worried expression
  SM_Beard_Full      — Full beard
  SM_Beard_Stubble   — Short stubble (chin box)
  SM_Beard_Mustache  — Mustache only

All parts at origin. Assemble by parenting to head/body.

Blender > Scripting > Open > Alt+P
"""

import bpy
import math

# ═══════════════════════════════════════════════════════════════
# SHARED DIMENSIONS — All parts must match these
# ═══════════════════════════════════════════════════════════════

# Body
BODY_H = 105          # Body height (feet to neck)
SHOULDER_W = 42
WAIST_W = 30
HIP_W = 32
LEG_SPREAD = 10
NECK_TOP_Z = 105      # Where head attaches

# Head
HEAD_W = 17           # Half-width
HEAD_H = 22           # Total height
HEAD_D_FRONT = 11     # Depth front
HEAD_D_BACK = 10      # Depth back
HEAD_CENTER_Z = NECK_TOP_Z + HEAD_H / 2 + 2  # Center of head sphere

# Hat attachment
HAT_Z = NECK_TOP_Z + HEAD_H + 1  # Bottom of hat sits here
HAT_RADIUS = HEAD_W + 1

# Face attachment
FACE_Z = HEAD_CENTER_Z          # Face center Z
FACE_X = HEAD_D_FRONT + 0.5    # Face protrudes slightly from head front

# Beard attachment
BEARD_Z = HEAD_CENTER_Z - 5    # Chin level

# Display layout
COL_SPACING = 80   # Spacing between columns for display


# ═══════════════════════════════════════════════════════════════
# HELPERS
# ═══════════════════════════════════════════════════════════════

def mat(name, r, g, b, metal=0.2, rough=0.85):
    m = bpy.data.materials.new(name=name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
    return m


def mk(name, verts, faces, color, smooth=True):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat(name, *color))
    if smooth:
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        try:
            bpy.ops.object.shade_auto_smooth()
        except:
            for p in ob.data.polygons: p.use_smooth = True
        ob.select_set(False)
    return ob


def box(cx, cy, cz, w, d, h, tw=1, td=1):
    hw, hd = w/2, d/2
    v = [(cx-hd,cy-hw,cz),(cx+hd,cy-hw,cz),(cx+hd,cy+hw,cz),(cx-hd,cy+hw,cz),
         (cx-hd*td,cy-hw*tw,cz+h),(cx+hd*td,cy-hw*tw,cz+h),
         (cx+hd*td,cy+hw*tw,cz+h),(cx-hd*td,cy+hw*tw,cz+h)]
    f = [(0,1,2,3),(4,7,6,5),(0,4,5,1),(2,6,7,3),(0,3,7,4),(1,5,6,2)]
    return v, f


def cyl(cx, cy, cz, r, h, n=10, rt=None):
    if rt is None: rt = r
    v, f = [], []
    for iz in range(2):
        z = cz + h*iz; cr = r if iz == 0 else rt
        for i in range(n):
            a = 2*math.pi*i/n
            v.append((cx+cr*math.cos(a), cy+cr*math.sin(a), z))
    for i in range(n):
        ni = (i+1)%n
        f.append((i, ni, n+ni, n+i))
    tc = len(v); v.append((cx, cy, cz+h))
    for i in range(n): f.append((tc, n+i, n+(i+1)%n))
    bc = len(v); v.append((cx, cy, cz))
    for i in range(n): f.append((bc, (i+1)%n, i))
    return v, f


def sphere(cx, cy, cz, rx, ry, rz, nlat=5, nlon=10):
    v, f = [], []
    for lat in range(nlat+1):
        phi = math.pi * lat / nlat
        sp, cp = math.sin(phi), math.cos(phi)
        for lon in range(nlon):
            th = 2*math.pi*lon/nlon
            v.append((cx+rx*sp*math.cos(th), cy+ry*sp*math.sin(th), cz+rz*cp))
    for lat in range(nlat):
        for lon in range(nlon):
            nl = (lon+1)%nlon
            a,b = lat*nlon+lon, lat*nlon+nl
            c,d = (lat+1)*nlon+nl, (lat+1)*nlon+lon
            f.append((a,b,c,d))
    return v, f


def merge(av, af, nv, nf):
    b = len(av); av.extend(nv)
    af.extend([tuple(i+b for i in face) for face in nf])


# ═══════════════════════════════════════════════════════════════
# BODY BASE — Shared by all characters
# ═══════════════════════════════════════════════════════════════

def build_body(x_off=0):
    v, f = [], []

    # Boots
    for side in [-1, 1]:
        bv, bf = box(x_off+2, LEG_SPREAD*side, 0, 10, 14, 4)
        merge(v, f, bv, bf)
        bv, bf = box(x_off, LEG_SPREAD*side, 4, 10, 11, 14, tw=0.9)
        merge(v, f, bv, bf)

    # Legs
    for side in [-1, 1]:
        # Lower leg
        bv, bf = box(x_off, LEG_SPREAD*side, 18, 10, 10, 30, tw=1.05, td=1.05)
        merge(v, f, bv, bf)
        # Upper leg
        bv, bf = box(x_off, LEG_SPREAD*side, 48, 11, 11, 30, tw=1.1, td=1.1)
        merge(v, f, bv, bf)

    # Hips
    bv, bf = box(x_off, 0, 75, HIP_W, 20, 10, tw=1.05)
    merge(v, f, bv, bf)

    # Torso
    bv, bf = box(x_off+1, 0, 85, WAIST_W, 20, 20, tw=1.2, td=1.1)
    merge(v, f, bv, bf)

    # Chest
    bv, bf = box(x_off+1, 0, 105, WAIST_W*1.2, 22, 18, tw=1.15, td=0.95)
    merge(v, f, bv, bf)

    # Shoulders
    bv, bf = box(x_off, 0, 123, SHOULDER_W, 20, 10, tw=0.85, td=0.9)
    merge(v, f, bv, bf)

    # Arms
    for side in [-1, 1]:
        sy = (SHOULDER_W/2 - 2) * side
        # Upper arm
        bv, bf = box(x_off, sy, 100, 9, 9, 28)
        merge(v, f, bv, bf)
        # Forearm
        bv, bf = box(x_off+2, sy, 75, 8, 8, 26, tw=0.85)
        merge(v, f, bv, bf)
        # Hand
        bv, bf = box(x_off+2, sy, 66, 7, 4, 10)
        merge(v, f, bv, bf)

    # Neck
    cv, cf = cyl(x_off, 0, 133, 5, 8, 8, 4.5)
    merge(v, f, cv, cf)

    return mk(f"SM_Body_Base", v, f, (0.25, 0.28, 0.22), smooth=False)


# ═══════════════════════════════════════════════════════════════
# HEAD BASE — Bald head, attachment point for everything
# ═══════════════════════════════════════════════════════════════

def build_head(x_off=0):
    v, f = [], []

    # Head — slightly egg-shaped (wider at top)
    sv, sf = sphere(x_off, 0, HEAD_CENTER_Z, HEAD_D_FRONT, HEAD_W, HEAD_H/2, 5, 10)
    merge(v, f, sv, sf)

    # Ears
    for side in [-1, 1]:
        ev, ef = box(x_off, (HEAD_W+2)*side, HEAD_CENTER_Z-1, 4, 3, 6)
        merge(v, f, ev, ef)

    return mk(f"SM_Head_Base", v, f, (0.72, 0.55, 0.45), smooth=True)


# ═══════════════════════════════════════════════════════════════
# HATS
# ═══════════════════════════════════════════════════════════════

def build_hat_captain(x_off=0):
    v, f = [], []
    z = HAT_Z
    # Cap body
    cv, cf = cyl(x_off, 0, z, HAT_RADIUS, 10, 12, HAT_RADIUS*0.85)
    merge(v, f, cv, cf)
    # Band
    cv, cf = cyl(x_off, 0, z-1, HAT_RADIUS+1.5, 4, 12, HAT_RADIUS+0.5)
    merge(v, f, cv, cf)
    # Crown
    cv, cf = cyl(x_off, 0, z+10, HAT_RADIUS*0.8, 3, 12, HAT_RADIUS*0.3)
    merge(v, f, cv, cf)
    # Visor
    vv = [
        (x_off+HAT_RADIUS*0.4, -12, z-1), (x_off+HAT_RADIUS+8, -8, z-3),
        (x_off+HAT_RADIUS+8, 8, z-3), (x_off+HAT_RADIUS*0.4, 12, z-1),
        (x_off+HAT_RADIUS*0.4, -12, z+1), (x_off+HAT_RADIUS+8, -8, z-1),
        (x_off+HAT_RADIUS+8, 8, z-1), (x_off+HAT_RADIUS*0.4, 12, z+1),
    ]
    vf = [(0,1,2,3),(4,7,6,5),(0,4,5,1),(2,6,7,3),(0,3,7,4),(1,5,6,2)]
    merge(v, f, vv, vf)
    # Badge
    bv, bf = box(x_off+HAT_RADIUS+1, 0, z+1, 2, 6, 5)
    merge(v, f, bv, bf)
    return mk("SM_Hat_Captain", v, f, (0.06, 0.08, 0.12))


def build_hat_beanie(x_off=0):
    v, f = [], []
    z = HAT_Z - 3
    # Beanie body — rounder, sits lower
    cv, cf = cyl(x_off, 0, z, HAT_RADIUS+1, 14, 10, HAT_RADIUS*0.5)
    merge(v, f, cv, cf)
    # Fold/cuff at bottom
    cv, cf = cyl(x_off, 0, z-2, HAT_RADIUS+2, 5, 10, HAT_RADIUS+1)
    merge(v, f, cv, cf)
    return mk("SM_Hat_Beanie", v, f, (0.15, 0.15, 0.18))


def build_hat_helmet(x_off=0):
    v, f = [], []
    z = HAT_Z - 5
    # Helmet dome
    sv, sf = sphere(x_off, 0, z+12, 16, 17, 14, 4, 10)
    merge(v, f, sv, sf)
    # Brim
    cv, cf = cyl(x_off, 0, z, 18, 3, 12, 17)
    merge(v, f, cv, cf)
    return mk("SM_Hat_Helmet", v, f, (0.65, 0.55, 0.10))


# ═══════════════════════════════════════════════════════════════
# FACES — Attach to front of head
# ═══════════════════════════════════════════════════════════════

def build_face(name, x_off, eye_shape="round", mouth_shape="neutral", brow_angle=0):
    v, f = [], []
    fz = FACE_Z
    fx = x_off + FACE_X

    # Nose
    nv, nf = box(fx+3, 0, fz-2, 3, 5, 6, tw=0.5, td=0.5)
    merge(v, f, nv, nf)

    # Eyes
    for side in [-1, 1]:
        ey = 5.5 * side
        if eye_shape == "round":
            ev, ef = cyl(fx+1, ey, fz+2, 2.5, 2, 8)
        elif eye_shape == "narrow":
            ev, ef = box(fx+1, ey, fz+2, 5, 2, 1.5)
        elif eye_shape == "wide":
            ev, ef = cyl(fx+1, ey, fz+1, 3, 2, 8)
        merge(v, f, ev, ef)

        # Eyebrow
        brow_z = fz + 5.5 + abs(brow_angle) * 0.5 * (1 if side * brow_angle > 0 else -1)
        bv, bf = box(fx+1.5, ey, brow_z, 6, 1.5, 1)
        merge(v, f, bv, bf)

    # Pupils (dark dots inside eyes)
    for side in [-1, 1]:
        pv, pf = cyl(fx+3, 5.5*side, fz+2.5, 1, 0.5, 6)
        merge(v, f, pv, pf)

    # Mouth
    if mouth_shape == "neutral":
        mv, mf = box(fx+1, 0, fz-6, 8, 1, 0.8)
        merge(v, f, mv, mf)
    elif mouth_shape == "frown":
        # Curved down
        for i in range(3):
            t = i / 2
            my = -4 + 4 * t
            mz = fz - 6 - 1.5 * math.sin(t * math.pi)
            sv, sf = box(fx+1, my, mz, 2.5, 1, 0.8)
            merge(v, f, sv, sf)
    elif mouth_shape == "open":
        mv, mf = cyl(fx+1, 0, fz-7, 3, 1.5, 8)
        merge(v, f, mv, mf)

    color = (0.72, 0.55, 0.45)  # Skin tone
    return mk(name, v, f, color, smooth=False)


# ═══════════════════════════════════════════════════════════════
# BEARDS
# ═══════════════════════════════════════════════════════════════

def build_beard_full(x_off=0):
    v, f = [], []
    bz = BEARD_Z
    bx = x_off + HEAD_D_FRONT * 0.5
    # Full beard — covers chin and cheeks
    # Chin
    bv, bf = box(bx+2, 0, bz-10, 16, 10, 12, tw=0.7, td=0.8)
    merge(v, f, bv, bf)
    # Cheeks
    for side in [-1, 1]:
        cv, cf = box(bx-2, 10*side, bz-5, 6, 5, 10, tw=0.8)
        merge(v, f, cv, cf)
    # Mustache
    mv, mf = box(bx+5, 0, bz-2, 12, 3, 3)
    merge(v, f, mv, mf)
    return mk("SM_Beard_Full", v, f, (0.15, 0.10, 0.06))


def build_beard_stubble(x_off=0):
    v, f = [], []
    bz = BEARD_Z
    bx = x_off + HEAD_D_FRONT * 0.5
    # Stubble — thin box on chin
    bv, bf = box(bx+3, 0, bz-8, 14, 8, 8, tw=0.8)
    merge(v, f, bv, bf)
    return mk("SM_Beard_Stubble", v, f, (0.20, 0.15, 0.10))


def build_beard_mustache(x_off=0):
    v, f = [], []
    bz = BEARD_Z
    bx = x_off + HEAD_D_FRONT * 0.5
    # Mustache only — two halves
    for side in [-1, 1]:
        mv, mf = box(bx+5, 4*side, bz-2, 5, 3, 2.5, tw=0.7)
        merge(v, f, mv, mf)
    return mk("SM_Beard_Mustache", v, f, (0.12, 0.08, 0.04))


# ═══════════════════════════════════════════════════════════════
# MAIN — Generate all parts in a grid layout
# ═══════════════════════════════════════════════════════════════

def main():
    print("\n" + "="*60)
    print("Sub3D — Modular Character Kit")
    print("="*60)

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
                    sp.clip_start = 0.1; sp.clip_end = 50000

    col = 0

    # ─── Row 1: Base parts (body + head) ───
    print("\n--- Base ---")
    build_body(col * COL_SPACING)
    build_head(col * COL_SPACING)
    print(f"  SM_Body_Base + SM_Head_Base at X={col*COL_SPACING}")

    # ─── Row 2: Hats ───
    col += 1
    print("\n--- Hats ---")
    for i, func in enumerate([build_hat_captain, build_hat_beanie, build_hat_helmet]):
        ob = func(col * COL_SPACING + i * 50)
        print(f"  {ob.name}")

    # ─── Row 3: Faces ───
    col += 1
    print("\n--- Faces ---")
    faces = [
        ("SM_Face_Neutral", "round", "neutral", 0),
        ("SM_Face_Angry", "narrow", "frown", 15),
        ("SM_Face_Worried", "wide", "open", -10),
    ]
    for i, (name, eye, mouth, brow) in enumerate(faces):
        ob = build_face(name, col * COL_SPACING + i * 50, eye, mouth, brow)
        print(f"  {ob.name}")

    # ─── Row 4: Beards ───
    col += 1
    print("\n--- Beards ---")
    for i, func in enumerate([build_beard_full, build_beard_stubble, build_beard_mustache]):
        ob = func(col * COL_SPACING + i * 50)
        print(f"  {ob.name}")

    # Frame
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break

    print("\n" + "="*60)
    print("Kit generated. To assemble a character:")
    print("  1. Select SM_Body_Base")
    print("  2. Add SM_Head_Base (already positioned)")
    print("  3. Add a SM_Hat_* (snaps to HAT_Z)")
    print("  4. Add a SM_Face_* (snaps to FACE_Z)")
    print("  5. Add a SM_Beard_* (snaps to BEARD_Z)")
    print("  6. Parent all to Body: select parts, then body, Ctrl+P")
    print("="*60)

main()
