"""
Sub3D — Crew Character + Full Accessories Kit
===============================================
One scene, everything visible:
  - 5 dressed body outfits (multi-material, 2cm voxels)
  - 6 face variations (0.5cm relief plates with 3D depth)
  - 5 beards (2cm)
  - 5 head equipment (2cm)
  - 11 hairstyles (2cm)

Blender > Scripting > Open > Alt+P
"""

import bpy

VOXEL = 2

# ═══════════════════════════════════════════════════════════
# MATERIAL INDICES
# ═══════════════════════════════════════════════════════════
SKIN    = 0
SUIT    = 1
SUIT_DK = 2
BOOT    = 3
HAIR    = 4
ACCENT  = 5

# Per-outfit colors for SUIT, SUIT_DK, ACCENT slots
OUTFIT_DEFS = {
    'crew': {
        'label': 'Crew_Basic',
        'suit':    (0.15, 0.28, 0.22),
        'suit_dk': (0.07, 0.16, 0.12),
        'accent':  (0.10, 0.10, 0.10),
    },
    'engineer': {
        'label': 'Engineer',
        'suit':    (0.15, 0.28, 0.22),
        'suit_dk': (0.07, 0.16, 0.12),
        'accent':  (0.85, 0.45, 0.05),  # Orange hi-vis
    },
    'captain': {
        'label': 'Captain',
        'suit':    (0.05, 0.08, 0.18),   # Navy
        'suit_dk': (0.03, 0.04, 0.10),
        'accent':  (0.85, 0.65, 0.10),   # Gold
    },
    'diver': {
        'label': 'Diver',
        'suit':    (0.05, 0.05, 0.05),   # Black wetsuit
        'suit_dk': (0.02, 0.02, 0.02),
        'accent':  (0.90, 0.80, 0.10),   # Yellow
    },
    'medic': {
        'label': 'Medic',
        'suit':    (0.80, 0.80, 0.78),   # White coat
        'suit_dk': (0.60, 0.60, 0.58),
        'accent':  (0.75, 0.10, 0.10),   # Red cross
    },
}

# Fixed colors for SKIN, BOOT, HAIR (same all outfits)
SKIN_COL = (0.72, 0.55, 0.42)
BOOT_COL = (0.08, 0.07, 0.06)
HAIR_COL = (0.20, 0.12, 0.06)


# ═══════════════════════════════════════════════════════════
# BASE BODY BUILDER (set, 2cm voxels)
# ═══════════════════════════════════════════════════════════

def build_body():
    g = set()

    def fb(x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1+1):
            for y in range(y0, y1+1):
                for z in range(z0, z1+1):
                    g.add((x, y, z))

    def profile(cy, cx, slices):
        sl = sorted(slices, key=lambda s: s[0])
        for z in range(sl[0][0], sl[-1][0]+1):
            lo, hi = sl[0], sl[-1]
            for i in range(len(sl)-1):
                if sl[i][0] <= z <= sl[i+1][0]:
                    lo, hi = sl[i], sl[i+1]; break
            span = hi[0]-lo[0]
            t = (z-lo[0])/span if span > 0 else 0
            yw = round(lo[1]+(hi[1]-lo[1])*t)
            xd = round(lo[2]+(hi[2]-lo[2])*t)
            for y in range(cy-yw, cy+yw+1):
                for x in range(cx-xd, cx+xd+1):
                    g.add((x, y, z))

    # Head (blank)
    fb(-5,-5,80, 5,5,87)
    fb(-4,-4,78, 4,4,79)
    fb(-4,-4,88, 4,4,89)
    for cx,cy in [(-5,-5),(-5,5),(5,-5),(5,5)]:
        g.discard((cx,cy,87)); g.discard((cx,cy,80))
    fb(-1,-6,82, 0,-6,84); fb(-1,6,82, 0,6,84)

    # Neck
    profile(0,0,[(74,3,2),(77,2,2)])
    # Torso (lowered waist)
    profile(0,0,[(44,7,4),(47,7,4),(49,6,3),(53,7,4),
                 (59,9,5),(65,9,5),(67,11,3),(69,10,3),(71,6,2),(73,3,2)])
    fb(0,9,66, 1,12,70); fb(0,-12,66, 1,-9,70)
    fb(-1,-1,42, 1,1,43)
    # Thighs
    profile(4,0,[(24,2,3),(34,3,3),(43,3,3)])
    profile(-4,0,[(24,2,3),(34,3,3),(43,3,3)])
    fb(-3,2,22, 4,6,23); fb(-3,-6,22, 4,-2,23)
    # Shins
    profile(4,0,[(6,2,1),(14,2,2),(21,2,3)])
    profile(-4,0,[(6,2,1),(14,2,2),(21,2,3)])
    # Ankles + feet
    fb(-1,2,4, 2,6,5); fb(-1,-6,4, 2,-2,5)
    fb(-2,2,0, 5,6,1); fb(-1,2,2, 3,6,3)
    fb(-2,-6,0, 5,-2,1); fb(-1,-6,2, 3,-2,3)
    # Arms — T-POSE at z=68
    arm_z = 68
    # R upper arm
    for y in range(12, 22):
        fb(-2, y, arm_z-2, 2, y, arm_z+2)
    # R elbow crease
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
    # Palms (2cm)
    fb(-1, 33, arm_z-2, 1, 36, arm_z+2)
    fb(-1, -36, arm_z-2, 1, -33, arm_z+2)

    return g


# ═══════════════════════════════════════════════════════════
# FINGERS — 0.67cm voxels (VOXEL/3)
# ═══════════════════════════════════════════════════════════

FINGER_VS = VOXEL / 3

def build_fingers_r(arm_z_2cm=68):
    g = set()
    py = 37 * 3  # Palm end y in fine grid
    cz = arm_z_2cm * 3
    def fb(x0,y0,z0,x1,y1,z1):
        for x in range(x0,x1+1):
            for y in range(y0,y1+1):
                for z in range(z0,z1+1):
                    g.add((x,y,z))
    fb(-1, py, cz-6, 0, py+7, cz-5)       # Pinky
    fb(-1, py, cz-3, 0, py+9, cz-1)       # Ring
    fb(-2, py, cz+1, 0, py+11, cz+3)      # Middle
    fb(-1, py, cz+5, 0, py+10, cz+7)      # Index
    tx = 3 * 3
    fb(tx-2, py-4, cz+5, tx, py+6, cz+8)  # Thumb
    return g

def build_fingers_l(arm_z_2cm=68):
    return {(x, -y, z) for (x,y,z) in build_fingers_r(arm_z_2cm)}


# ═══════════════════════════════════════════════════════════
# BODY DRESSER — assigns materials per zone + extra geometry
# ═══════════════════════════════════════════════════════════

def dress_body(body_set, outfit_key):
    g = {}
    for (x, y, z) in body_set:
        is_arm = abs(y) >= 11

        # ── HEAD: face = SKIN, rest = SKIN too (bald base, hair is accessory) ──
        if z >= 78:
            mat = SKIN

        # ── NECK ──
        elif z >= 74:
            mat = SKIN

        # ── HANDS ──
        elif is_arm and z <= 39:
            mat = SKIN

        # ── FOREARMS ──
        elif is_arm and z <= 55:
            mat = SKIN if outfit_key != 'diver' else SUIT

        # ── UPPER ARMS ──
        elif is_arm:
            mat = SUIT
            # Sleeve cuffs
            if z in (56, 57):
                mat = SUIT_DK

        # ── BOOTS ──
        elif z <= 5:
            if z <= 1:
                mat = SUIT_DK   # Boot sole (darker)
            else:
                mat = BOOT

        # ── BELT ──
        elif 49 <= z <= 51:
            if abs(y) <= 1 and x >= 3:
                mat = ACCENT    # Belt buckle
            else:
                mat = SUIT_DK   # Belt

        # ── LEGS ──
        elif z <= 43:
            mat = SUIT
            # Cargo pockets
            if 28 <= z <= 33 and 2 <= abs(y) <= 4 and x >= 2:
                mat = SUIT_DK

        # ── TORSO ──
        else:
            mat = SUIT
            # Chest pockets
            if 58 <= z <= 62 and 3 <= abs(y) <= 6 and x >= 3:
                mat = SUIT_DK
            # Collar
            if z >= 72 and abs(y) <= 3 and x >= 1:
                mat = SUIT_DK
            # Center seam
            if y == 0 and x >= 4 and 52 <= z <= 67:
                mat = SUIT_DK

        g[(x, y, z)] = mat

    # ── Outfit-specific extras ──
    def fb_mat(x0,y0,z0,x1,y1,z1, m):
        for x in range(x0,x1+1):
            for y in range(y0,y1+1):
                for z in range(z0,z1+1):
                    g[(x,y,z)] = m

    if outfit_key == 'engineer':
        # Hi-vis vest overlay on chest
        for (x,y,z), m in list(g.items()):
            if 56 <= z <= 67 and abs(y) <= 9 and abs(x) <= 5:
                g[(x,y,z)] = ACCENT
        # Tool belt pouches
        fb_mat(-4,6,49, -2,8,51, ACCENT)
        fb_mat(-4,-8,49, -2,-6,51, ACCENT)

    elif outfit_key == 'captain':
        # Shoulder boards (epaulettes)
        fb_mat(-1,9,69, 1,12,70, ACCENT)
        fb_mat(-1,-12,69, 1,-9,70, ACCENT)
        # Collar trim
        for y in range(-3, 4):
            g[(2, y, 73)] = ACCENT

    elif outfit_key == 'diver':
        # O2 tank on back
        fb_mat(-6,-2,50, -4,2,67, ACCENT)
        fb_mat(-7,-1,52, -5,1,65, ACCENT)
        # All forearms covered (already handled above)
        # Gloves
        for (x,y,z), m in list(g.items()):
            if abs(y) >= 11 and z <= 39:
                g[(x,y,z)] = SUIT

    elif outfit_key == 'medic':
        # Red cross armband
        fb_mat(0,11,62, 1,12,64, ACCENT)
        fb_mat(0,-12,62, 1,-11,64, ACCENT)
        # Med pouch at hip
        fb_mat(3,7,46, 5,9,50, ACCENT)
        # White coat extends below waist (already white suit)
        for (x,y,z), m in list(g.items()):
            if 44 <= z <= 55 and abs(y) <= 7:
                g[(x,y,z)] = SUIT  # Long coat

    return g


# ═══════════════════════════════════════════════════════════
# FACE BUILDERS — 0.5cm voxels, 3D RELIEF
#
# Each face is a sculpted front-face plate.
# Local coords: x=forward, y=lateral, z=up
# Head front surface at x≈20 (5 voxels * 4 = 20 in 0.5cm)
# Face plate: x=16..23, y=-12..12, z=0..44
# (maps to head 2cm: x=4..5.75, y=-3..3, z=78..89)
#
# Returns (grid_dict, name) — dict {(x,y,z): mat_index}
# Mat 0 = SKIN (face plate), Mat 1 = DARK (socket shadows)
# ═══════════════════════════════════════════════════════════

FACE_VS = 0.5  # Face voxel resolution in cm
F_SKIN   = 0   # Flesh tone
F_DARK   = 1   # Deep shadow (socket interiors)
F_BROW   = 2   # Brow/eyebrow color (dark brown)
F_EYE_W  = 3   # Eye white
F_EYE_IR = 4   # Eye iris (blue)
F_EYE_PU = 5   # Eye pupil (black)
F_VISOR  = 6   # Visor (dark tinted)
F_SKIN2  = 7   # Slightly darker skin (lower face, subtle gradient)

def _face_plate():
    """Base face plate: front portion of head at 0.5cm resolution.
    Subtle gradient: lighter at forehead, slightly darker at chin/jaw.
    Returns mutable dict {(x,y,z): mat}."""
    g = {}
    # Front face slab: x=17..21, y=-10..10, z=4..36
    for x in range(17, 22):
        for y in range(-10, 11):
            for z in range(4, 37):
                g[(x, y, z)] = F_SKIN2 if z < 12 else F_SKIN
    # Chin taper (narrower at bottom)
    for x in range(17, 22):
        for y in range(-8, 9):
            for z in range(0, 4):
                g[(x, y, z)] = F_SKIN2
    # Forehead (top, slightly rounded)
    for x in range(17, 21):
        for y in range(-9, 10):
            for z in range(37, 42):
                g[(x, y, z)] = F_SKIN
    return g

def _carve(g, x0, y0, z0, x1, y1, z1):
    """Remove voxels (eye sockets, etc)."""
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                g.pop((x, y, z), None)

def _fill(g, x0, y0, z0, x1, y1, z1, mat=F_SKIN):
    """Add voxels (nose, brow, chin protrusions)."""
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                g[(x, y, z)] = mat

def _shadow(g, x0, y0, z0, x1, y1, z1):
    """Mark existing voxels as dark (socket rims, brow undersides)."""
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                if (x, y, z) in g:
                    g[(x, y, z)] = F_DARK

def _eyeball(g, cx, cy, cz):
    """Place a 3×3×2 eyeball: white + blue iris + black pupil.
    cx,cy,cz = center of eye at the socket opening (front face)."""
    # White surround (3×3 on face surface)
    for dy in (-1, 0, 1):
        for dz in (-1, 0, 1):
            g[(cx, cy+dy, cz+dz)] = F_EYE_W
    # Iris (center 1×1, blue)
    g[(cx, cy, cz)] = F_EYE_IR
    # Pupil (front of iris)
    g[(cx+1, cy, cz)] = F_EYE_PU

def _brow_line(g, y0, y1, z, protrude_x=22):
    """Dark eyebrow line across face."""
    for y in range(y0, y1+1):
        g[(protrude_x, y, z)] = F_BROW
        g[(protrude_x+1, y, z)] = F_BROW


def face_01_stoic():
    """Serious default crew face. Strong brow, deep-set eyes, straight nose, firm jaw."""
    g = _face_plate()
    # Brow ridge — protrudes 2 voxels forward
    _fill(g, 22, -8, 30, 23, 8, 32)
    _brow_line(g, -8, -4, 31)   # R eyebrow
    _brow_line(g,  4,  8, 31)   # L eyebrow
    # Brow shadow underneath
    _shadow(g, 20, -8, 29, 21, 8, 29)
    # Eye sockets — carved 3 deep
    _carve(g, 19, -8, 24, 21, -4, 27)
    _carve(g, 19,  4, 24, 21,  8, 27)
    # Eyeballs
    _eyeball(g, 20, -6, 26)
    _eyeball(g, 20,  6, 26)
    # Socket rim shadow
    _shadow(g, 18, -9, 23, 18, -3, 28)
    _shadow(g, 18,  3, 23, 18,  9, 28)
    # Nose
    _fill(g, 22, -1, 16, 24, 1, 26)
    _fill(g, 23, -2, 14, 24, 2, 16)
    # Jaw — squared, strong
    _fill(g, 21, -7, 0, 22, 7, 3)
    # Cheekbones
    _fill(g, 22, -9, 20, 22, -7, 24)
    _fill(g, 22,  7, 20, 22,  9, 24)
    return g, "Face_Stoic"

def face_02_worried():
    """Tense, anxious. High arched brow, wide shallow sockets, narrow chin."""
    g = _face_plate()
    # High arched brow
    _fill(g, 22, -8, 32, 23, -5, 34)
    _fill(g, 22, -4, 33, 23, -2, 35)
    _fill(g, 22,  2, 33, 23,  4, 35)
    _fill(g, 22,  5, 32, 23,  8, 34)
    # Worried brows — angled up in center
    _brow_line(g, -7, -3, 33)
    _brow_line(g,  3,  7, 33)
    # Brow shadow
    _shadow(g, 20, -8, 31, 21, 8, 31)
    # Eye sockets — wide (worried = wide open)
    _carve(g, 20, -8, 24, 21, -3, 29)
    _carve(g, 20,  3, 24, 21,  8, 29)
    # Eyeballs — placed higher (wide-eyed look)
    _eyeball(g, 20, -6, 27)
    _eyeball(g, 20,  6, 27)
    # Nose — narrow, pinched
    _fill(g, 22, 0, 16, 23, 0, 25)
    _fill(g, 22, -1, 14, 23, 1, 16)
    # Chin — receding, weak
    _fill(g, 20, -5, 0, 21, 5, 2)
    # Furrow between brows
    _shadow(g, 20, -2, 30, 21, 2, 32)
    return g, "Face_Worried"

def face_03_unhinged():
    """Crazy, wild. Asymmetric everything."""
    g = _face_plate()
    # Asymmetric brow — R high, L low
    _fill(g, 22, -8, 33, 23, -4, 35)
    _fill(g, 22,  4, 29, 23,  8, 31)
    _brow_line(g, -8, -4, 34)   # R brow: high
    _brow_line(g,  4,  8, 30)   # L brow: low
    # R eye socket — large
    _carve(g, 19, -9, 24, 21, -3, 30)
    # L eye socket — small slit
    _carve(g, 20,  4, 26, 21,  8, 28)
    # Eyeballs — one big (R), one squished (L)
    _eyeball(g, 20, -6, 27)
    # L eye — just iris+pupil, no room for full eye
    g[(20, 6, 27)] = F_EYE_IR
    g[(21, 6, 27)] = F_EYE_PU
    # Nose — offset, crooked
    _fill(g, 22, 1, 16, 24, 3, 25)
    _fill(g, 23, 0, 14, 24, 3, 16)
    # Jaw — uneven
    _fill(g, 21, -8, 0, 22, -2, 4)
    _fill(g, 21,  2, 0, 22,  6, 3)
    # Mouth slit — slightly open
    _carve(g, 20, -3, 7, 21, 4, 8)
    return g, "Face_Unhinged"

def face_04_hardened():
    """Battle veteran. Heavy brow, deep narrow slits, broad nose, scars."""
    g = _face_plate()
    # Heavy brow — thick, menacing
    _fill(g, 22, -9, 30, 24, 9, 33)
    _brow_line(g, -9, -4, 32)
    _brow_line(g,  4,  9, 32)
    _shadow(g, 20, -9, 29, 21, 9, 29)
    # Eye slits — very narrow, deep
    _carve(g, 18, -8, 26, 21, -4, 28)
    _carve(g, 18,  4, 26, 21,  8, 28)
    # Eyes barely visible in deep slits
    g[(19, -6, 27)] = F_EYE_W
    g[(19,  6, 27)] = F_EYE_W
    g[(20, -6, 27)] = F_EYE_PU
    g[(20,  6, 27)] = F_EYE_PU
    # Nose — broad, flat
    _fill(g, 22, -2, 16, 24, 2, 26)
    _fill(g, 23, -3, 14, 24, 3, 16)
    # Strong jaw
    _fill(g, 21, -8, 0, 23, 8, 4)
    # Cheekbones
    _fill(g, 22, -10, 18, 23, -8, 24)
    _fill(g, 22,  8, 18, 23, 10, 24)
    # Scars — missing voxels
    _carve(g, 21, -6, 14, 21, -5, 18)
    _carve(g, 21, 3, 10, 21, 5, 14)
    return g, "Face_Hardened"

def face_05_visor():
    """Tactical visor band. Nose underneath, no visible eyes, chin strap."""
    g = _face_plate()
    # Nose — before visor covers
    _fill(g, 22, -1, 14, 24, 1, 24)
    _fill(g, 23, -2, 12, 24, 2, 14)
    # Visor band — dark tinted, protrudes 4 forward
    _fill(g, 22, -12, 26, 26, 12, 34, F_VISOR)
    # Visor frame
    _fill(g, 22, -13, 25, 26, 13, 25, F_VISOR)
    _fill(g, 22, -13, 35, 26, 13, 35, F_VISOR)
    # Chin strap
    _fill(g, 20, -10, 5, 21, -10, 12, F_DARK)
    _fill(g, 20,  10, 5, 21,  10, 12, F_DARK)
    _fill(g, 20, -9, 4, 21, 9, 5, F_DARK)
    return g, "Face_Visor"

def face_06_blank():
    """Eerie minimal face. Barely any features — shallow everything."""
    g = _face_plate()
    # Very subtle brow
    _fill(g, 22, -7, 30, 22, 7, 31)
    # Shallow eye indents
    _carve(g, 21, -7, 25, 21, -4, 28)
    _carve(g, 21,  4, 25, 21,  7, 28)
    # Faint eyes — just white dots, staring
    g[(21, -6, 26)] = F_EYE_W
    g[(21,  6, 26)] = F_EYE_W
    # Minimal nose
    _fill(g, 22, 0, 18, 22, 0, 23)
    return g, "Face_Blank"


# ═══════════════════════════════════════════════════════════
# HAIR BUILDERS — 2cm voxels, relative to head (z=0 = head bottom)
# Head block: x=-5..5, y=-5..5, z=0..11
# Top of head = z=11 (crown z=89 in world = z=11 in head-local)
# ═══════════════════════════════════════════════════════════

def hair_01_bald():
    """Clean bald — no geometry."""
    g = set()
    g.add((0, 0, 12))  # Marker dot
    return g, "Hair_Bald"

def hair_02_buzzcut():
    """Short uniform layer."""
    g = set()
    for x in range(-4, 5):
        for y in range(-4, 5):
            g.add((x, y, 12))
    return g, "Hair_Buzzcut"

def hair_03_mohawk():
    """Center ridge, 4 voxels high."""
    g = set()
    for x in range(-2, 3):
        for z in range(11, 15):
            g.add((x, 0, z))
    for x in range(-1, 2):
        g.add((x, 0, 15))
    # Base — slightly wider
    for x in range(-3, 4):
        g.add((x, -1, 11)); g.add((x, 1, 11))
    return g, "Hair_Mohawk"

def hair_04_curly():
    """Thick curly volume — irregular surface."""
    g = set()
    # Main volume
    for x in range(-5, 6):
        for y in range(-5, 6):
            g.add((x, y, 12))
            g.add((x, y, 13))
    for x in range(-4, 5):
        for y in range(-4, 5):
            g.add((x, y, 14))
    for x in range(-3, 4):
        for y in range(-3, 4):
            g.add((x, y, 15))
    # Irregular: remove some for texture
    for v in [(-4,-4,13), (3,4,13), (-3,-3,14), (2,3,14),
              (-5,2,12), (4,-3,12), (-2,5,12), (3,-5,12),
              (-4,3,13), (4,-2,13), (-3,4,14), (2,-4,14)]:
        g.discard(v)
    # Side puffs
    for z in range(9, 13):
        g.add((-1, -6, z)); g.add((0, -6, z)); g.add((1, -6, z))
        g.add((-1,  6, z)); g.add((0,  6, z)); g.add((1,  6, z))
    return g, "Hair_Curly"

def hair_05_sidepart():
    """Clean side part, swept right."""
    g = set()
    # Flat top
    for x in range(-5, 6):
        for y in range(-5, 6):
            g.add((x, y, 12))
    # Part line — gap at y=-2
    for x in range(-4, 5):
        g.discard((x, -2, 12))
    # Thicker on right side (positive Y)
    for x in range(-4, 4):
        for y in range(0, 6):
            g.add((x, y, 13))
    # Front sweep
    for y in range(-1, 5):
        g.add((6, y, 10)); g.add((6, y, 11))
    return g, "Hair_SidePart"

def hair_06_braids():
    """Twin braids hanging from sides."""
    g = set()
    # Top cap
    for x in range(-4, 5):
        for y in range(-4, 5):
            g.add((x, y, 12))
    # R braid — hangs from y=5 down to z=-6
    for z in range(-6, 10):
        g.add((-1, 5, z)); g.add((0, 5, z))
    # L braid
    for z in range(-6, 10):
        g.add((-1, -5, z)); g.add((0, -5, z))
    # Braid ties
    g.add((-1, 5, -6)); g.add((0, 5, -6))
    g.add((-1, -5, -6)); g.add((0, -5, -6))
    return g, "Hair_Braids"

def hair_07_long():
    """Long straight hair to shoulders."""
    g = set()
    # Top volume
    for x in range(-5, 6):
        for y in range(-5, 6):
            g.add((x, y, 12))
            g.add((x, y, 13))
    # Cascading sides and back, tapering down
    for z in range(-8, 10):
        w = max(2, min(5, 5 - abs(z - 5) // 3))
        for y in range(-w, w+1):
            g.add((-5, y, z))  # Back
        if z > 2:
            g.add((-4, -5, z)); g.add((-4, 5, z))  # Sides
            g.add((-3, -5, z)); g.add((-3, 5, z))
    # Front fringe
    for y in range(-4, 5):
        g.add((6, y, 10)); g.add((6, y, 11))
    return g, "Hair_Long"

def hair_08_ponytail():
    """Short top + ponytail behind."""
    g = set()
    # Tight top
    for x in range(-4, 5):
        for y in range(-4, 5):
            g.add((x, y, 12))
    # Ponytail — column behind head going down
    for z in range(-4, 10):
        g.add((-6, -1, z)); g.add((-6, 0, z)); g.add((-6, 1, z))
    # Tie
    g.add((-6, -1, 9)); g.add((-6, 1, 9))
    return g, "Hair_Ponytail"

def hair_09_dreads():
    """Dreadlocks — multiple hanging columns, varied lengths."""
    g = set()
    # Base cap
    for x in range(-4, 5):
        for y in range(-4, 5):
            g.add((x, y, 12))
    # Dreads — hanging columns at various positions and lengths
    dread_positions = [
        (-3, -4, -4), (-1, -5, -6), (1, -5, -3), (3, -4, -5),
        (-4, -2, -5), (-4, 2, -4), (-3, 4, -6), (-1, 5, -3),
        (1, 5, -5), (3, 4, -4), (4, 2, -3), (4, -2, -5),
        (-2, 0, -2), (2, 0, -3),
    ]
    for px, py, end_z in dread_positions:
        for z in range(end_z, 11):
            g.add((px, py, z))
    return g, "Hair_Dreads"

def hair_10_receding():
    """Receding hairline — M-pattern, front bare, back+sides only."""
    g = set()
    # Back and sides only
    for x in range(-5, 3):  # No front (x >= 3 is bare)
        for y in range(-5, 6):
            g.add((x, y, 12))
    # Sides extend forward but thin out
    for y in (-5, -4, 4, 5):
        for x in range(3, 6):
            g.add((x, y, 11))
    # M-pattern: two peaks at temples
    g.add((3, -3, 12)); g.add((4, -4, 11))
    g.add((3,  3, 12)); g.add((4,  4, 11))
    return g, "Hair_Receding"

def hair_11_messy():
    """Thick messy — irregular top + side tufts + front fringe."""
    g = set()
    for x in range(-5, 6):
        for y in range(-5, 6):
            g.add((x, y, 12)); g.add((x, y, 13))
    for x in range(-3, 4):
        for y in range(-3, 4):
            g.add((x, y, 14))
    # Side tufts
    for z in range(8, 12):
        g.add((-1, -6, z)); g.add((0, -6, z))
        g.add((-1,  6, z)); g.add((0,  6, z))
    # Front fringe — messy
    for y in range(-3, 4):
        g.add((6, y, 10)); g.add((6, y, 11))
    g.add((6, -4, 11)); g.add((6, 4, 10))
    return g, "Hair_Messy"


# ═══════════════════════════════════════════════════════════
# BEARD BUILDERS — 2cm voxels, relative to head z=0
# ═══════════════════════════════════════════════════════════

def beard_01_stubble():
    g = set()
    for y in range(-4,5):
        g.add((5,y,1)); g.add((5,y,0))
    for y in range(-3,4):
        g.add((6,y,0))
    return g, "Beard_Stubble"

def beard_02_full():
    g = set()
    for z in range(-2,2):
        for y in range(-4,5): g.add((5,y,z))
        for y in range(-3,4): g.add((6,y,z))
    for y in range(-2,3):
        g.add((5,y,-3)); g.add((6,y,-3))
    for y in range(-1,2): g.add((5,y,-4))
    for z in range(0,5):
        g.add((4,-5,z)); g.add((4,5,z))
    return g, "Beard_Full"

def beard_03_mustache():
    g = set()
    for y in range(-3,4): g.add((6,y,3))
    g.add((6,-1,4)); g.add((6,0,4)); g.add((6,1,4))
    g.add((6,-4,3)); g.add((6,4,3))
    g.add((6,-4,2)); g.add((6,4,2))
    return g, "Beard_Mustache"

def beard_04_goatee():
    g = set()
    for y in range(-2,3): g.add((6,y,3))
    for y in range(-1,2):
        g.add((6,y,1)); g.add((6,y,0)); g.add((6,y,-1))
    g.add((6,0,-2))
    return g, "Beard_Goatee"

def beard_05_mutton():
    g = set()
    for z in range(-1,6):
        g.add((4,-5,z)); g.add((5,-5,z))
        g.add((4,5,z)); g.add((5,5,z))
    for z in range(0,4):
        g.add((5,-4,z)); g.add((5,4,z))
    g.add((6,-3,3)); g.add((6,-2,3))
    g.add((6,2,3)); g.add((6,3,3))
    return g, "Beard_Mutton"


# ═══════════════════════════════════════════════════════════
# EQUIPMENT BUILDERS — 2cm voxels
# ═══════════════════════════════════════════════════════════

def equip_01_hardhat():
    g = set()
    for x in range(-5,6):
        for y in range(-6,7): g.add((x,y,12))
    for x in range(-4,5):
        for y in range(-5,6): g.add((x,y,13))
    for x in range(-3,4):
        for y in range(-4,5): g.add((x,y,14))
    for x in range(-6,7):
        for y in range(-7,8): g.add((x,y,11))
    return g, "Equip_HardHat"

def equip_02_headlamp():
    g = set()
    for y in range(-6,7):
        g.add((-5,y,9)); g.add((5,y,9))
    for x in range(-5,6):
        g.add((x,-5,9)); g.add((x,5,9))
    g.add((6,-1,9)); g.add((6,0,9)); g.add((6,1,9)); g.add((7,0,9))
    return g, "Equip_Headlamp"

def equip_03_beanie():
    g = set()
    for x in range(-5,6):
        for y in range(-5,6):
            g.add((x,y,10)); g.add((x,y,11))
    for x in range(-4,5):
        for y in range(-4,5): g.add((x,y,12))
    for x in range(-3,4):
        for y in range(-3,4): g.add((x,y,13))
    for x in range(-5,6):
        for y in range(-6,7): g.add((x,y,9))
    return g, "Equip_Beanie"

def equip_04_headset():
    g = set()
    for y in range(-5,6): g.add((0,y,11))
    for x in range(-1,2):
        for z in range(5,9):
            g.add((x,-6,z)); g.add((x,-7,z))
            g.add((x,6,z)); g.add((x,7,z))
    g.add((2,6,5)); g.add((3,6,5)); g.add((4,5,5)); g.add((5,4,4))
    return g, "Equip_Headset"

def equip_05_gasmask():
    g = set()
    for y in range(-3,4):
        for z in range(1,6): g.add((6,y,z))
    for y in range(-2,3):
        g.add((7,y,2)); g.add((7,y,3)); g.add((7,y,4))
    g.add((8,0,3)); g.add((8,0,4)); g.add((9,0,3))
    for z in range(3,7):
        g.add((3,-5,z)); g.add((3,5,z))
    return g, "Equip_GasMask"


# ═══════════════════════════════════════════════════════════
# MESH GENERATORS
# ═══════════════════════════════════════════════════════════

FACE_DEFS = [
    ((1,0,0),  [(1,0,0),(1,1,0),(1,1,1),(1,0,1)]),
    ((-1,0,0), [(0,0,0),(0,0,1),(0,1,1),(0,1,0)]),
    ((0,1,0),  [(0,1,0),(0,1,1),(1,1,1),(1,1,0)]),
    ((0,-1,0), [(0,0,0),(1,0,0),(1,0,1),(0,0,1)]),
    ((0,0,1),  [(0,0,1),(1,0,1),(1,1,1),(0,1,1)]),
    ((0,0,-1), [(0,0,0),(0,1,0),(1,1,0),(1,0,0)]),
]

def generate_mesh(grid_set, voxel_size=2):
    half = voxel_size / 2
    cache = {}; verts = []; faces = []
    def vid(gx,gy,gz):
        key=(gx,gy,gz)
        if key not in cache:
            cache[key]=len(verts)
            verts.append((gx*voxel_size-half, gy*voxel_size-half, gz*voxel_size))
        return cache[key]
    for (vx,vy,vz) in grid_set:
        for (dx,dy,dz),corners in FACE_DEFS:
            if (vx+dx,vy+dy,vz+dz) not in grid_set:
                faces.append(tuple(vid(vx+cx,vy+cy,vz+cz) for cx,cy,cz in corners))
    return verts, faces

def generate_mesh_mat(grid_dict, voxel_size=2):
    """Material-aware: grid_dict = {(x,y,z): mat_index}"""
    half = voxel_size / 2
    cache = {}; verts = []; faces = []; fmats = []
    def vid(gx,gy,gz):
        key=(gx,gy,gz)
        if key not in cache:
            cache[key]=len(verts)
            verts.append((gx*voxel_size-half, gy*voxel_size-half, gz*voxel_size))
        return cache[key]
    for (vx,vy,vz), mat in grid_dict.items():
        for (dx,dy,dz),corners in FACE_DEFS:
            if (vx+dx,vy+dy,vz+dz) not in grid_dict:
                faces.append(tuple(vid(vx+cx,vy+cy,vz+cz) for cx,cy,cz in corners))
                fmats.append(mat)
    return verts, faces, fmats

def generate_mesh_mat_multi(body_dict, finger_grids, body_vs=2, finger_vs=None):
    """Multi-res: body (material-aware) + finger grids (single mat, SKIN=0)."""
    if finger_vs is None:
        finger_vs = FINGER_VS
    cache = {}; verts = []; faces = []; fmats = []
    def vid(wx,wy,wz):
        key = (round(wx,4), round(wy,4), round(wz,4))
        if key not in cache:
            cache[key] = len(verts)
            verts.append((wx, wy, wz))
        return cache[key]
    # Body
    h = body_vs / 2
    for (vx,vy,vz), mat in body_dict.items():
        for (dx,dy,dz),corners in FACE_DEFS:
            if (vx+dx,vy+dy,vz+dz) not in body_dict:
                faces.append(tuple(vid((vx+cx)*body_vs-h,(vy+cy)*body_vs-h,(vz+cz)*body_vs)
                                   for cx,cy,cz in corners))
                fmats.append(mat)
    # Fingers
    fh = finger_vs / 2
    for fg in finger_grids:
        for (vx,vy,vz) in fg:
            for (dx,dy,dz),corners in FACE_DEFS:
                if (vx+dx,vy+dy,vz+dz) not in fg:
                    faces.append(tuple(vid((vx+cx)*finger_vs-fh,(vy+cy)*finger_vs-fh,(vz+cz)*finger_vs)
                                       for cx,cy,cz in corners))
                    fmats.append(SKIN)  # Fingers = skin material
    return verts, faces, fmats


# ═══════════════════════════════════════════════════════════
# BLENDER HELPERS
# ═══════════════════════════════════════════════════════════

def make_mat(name, r, g, b, metallic=0.0, roughness=0.85):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
    return m

def make_outfit_materials(outfit_key):
    """Create 6 materials for an outfit: SKIN, SUIT, SUIT_DK, BOOT, HAIR, ACCENT"""
    od = OUTFIT_DEFS[outfit_key]
    return [
        make_mat(f"{od['label']}_Skin",    *SKIN_COL),
        make_mat(f"{od['label']}_Suit",    *od['suit']),
        make_mat(f"{od['label']}_SuitDk",  *od['suit_dk']),
        make_mat(f"{od['label']}_Boot",    *BOOT_COL),
        make_mat(f"{od['label']}_Hair",    *HAIR_COL),
        make_mat(f"{od['label']}_Accent",  *od['accent']),
    ]

def spawn_mat_mesh(grid_dict, name, materials, loc=(0,0,0), vs=2):
    verts, faces, fmats = generate_mesh_mat(grid_dict, vs)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate(); me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    for m in materials:
        ob.data.materials.append(m)
    for i, poly in enumerate(me.polygons):
        poly.material_index = fmats[i]
    ob.location = loc
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_flat()
    ob.select_set(False)
    return ob

def spawn_simple_mesh(grid_set, name, mat, loc=(0,0,0), vs=2):
    verts, faces = generate_mesh(grid_set, vs)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate(); me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat)
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
    print("Sub3D — Full Character + Accessories Kit")
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
                    sp.clip_start = 0.1; sp.clip_end = 50000

    body_set = build_body()
    fingers_r = build_fingers_r()
    fingers_l = build_fingers_l()

    # ── Dressed head for previews (skin face, hair top) ──
    head_dressed = {k: v for k, v in dress_body(body_set, 'crew').items() if k[2] >= 74}
    head_mats = make_outfit_materials('crew')

    # Shared materials
    mat_beard = make_mat("M_Beard", *HAIR_COL)
    mat_equip = make_mat("M_Equip", 0.85, 0.65, 0.10)
    # Face plate materials: indices match F_SKIN..F_SKIN2
    face_plate_mats = [
        make_mat("M_FaceSkin",   0.72, 0.55, 0.42),          # 0: F_SKIN
        make_mat("M_FaceDark",   0.08, 0.06, 0.05),          # 1: F_DARK (socket shadow)
        make_mat("M_FaceBrow",   0.14, 0.08, 0.04),          # 2: F_BROW (eyebrow)
        make_mat("M_EyeWhite",  0.92, 0.90, 0.88),          # 3: F_EYE_W
        make_mat("M_EyeIris",   0.20, 0.45, 0.65),          # 4: F_EYE_IR (blue)
        make_mat("M_EyePupil",  0.02, 0.02, 0.02),          # 5: F_EYE_PU (black)
        make_mat("M_Visor",     0.03, 0.05, 0.08, 0.4, 0.3),# 6: F_VISOR (dark reflective)
        make_mat("M_FaceSkin2", 0.65, 0.48, 0.36),          # 7: F_SKIN2 (lower face)
    ]

    HEAD_Z = 78 * 2  # World Z of head bottom (cm)
    col = 0

    # ══════════════════════════════════════════════════
    # ROW 1: DRESSED BODIES (5 outfits)
    # ══════════════════════════════════════════════════
    print("\n  OUTFITS:")
    for outfit_key in ['crew', 'engineer', 'captain', 'diver', 'medic']:
        od = OUTFIT_DEFS[outfit_key]
        dressed = dress_body(body_set, outfit_key)
        mats = make_outfit_materials(outfit_key)
        # Multi-res: body 2cm + fingers 0.67cm
        v, f, fm = generate_mesh_mat_multi(dressed, [fingers_r, fingers_l])
        me = bpy.data.meshes.new(od['label'])
        me.from_pydata(v, [], f)
        me.validate(); me.update(calc_edges=True)
        ob = bpy.data.objects.new(od['label'], me)
        bpy.context.collection.objects.link(ob)
        for m in mats: ob.data.materials.append(m)
        for i, poly in enumerate(me.polygons): poly.material_index = fm[i]
        ob.location = (0, col*60, 0)
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        bpy.ops.object.shade_flat()
        ob.select_set(False)
        print(f"    {od['label']} at Y={col*60}")
        col += 1

    # Gap between sections
    col += 1

    # ══════════════════════════════════════════════════
    # ROW 2: FACES (1cm voxels on head preview)
    # ══════════════════════════════════════════════════
    print("\n  FACES (0.5cm relief):")
    for builder in [face_01_stoic, face_02_worried, face_03_unhinged,
                    face_04_hardened, face_05_visor, face_06_blank]:
        acc_dict, name = builder()
        y_pos = col * 60
        # Head preview (2cm) — shows hair/back of head
        spawn_mat_mesh(head_dressed, f"{name}_head", head_mats, loc=(0, y_pos, 0))
        # Face plate (0.5cm relief) — offset to head Z
        spawn_mat_mesh(acc_dict, name, face_plate_mats, loc=(0, y_pos, HEAD_Z), vs=FACE_VS)
        print(f"    {name} at Y={y_pos}")
        col += 1

    col += 1

    # ══════════════════════════════════════════════════
    # ROW 3: BEARDS (2cm on head preview)
    # ══════════════════════════════════════════════════
    print("\n  BEARDS:")
    for builder in [beard_01_stubble, beard_02_full, beard_03_mustache,
                    beard_04_goatee, beard_05_mutton]:
        acc_grid, name = builder()
        y_pos = col * 60
        spawn_mat_mesh(head_dressed, f"{name}_head", head_mats, loc=(0, y_pos, 0))
        spawn_simple_mesh(acc_grid, name, mat_beard, loc=(0, y_pos, HEAD_Z), vs=2)
        print(f"    {name} at Y={y_pos}")
        col += 1

    col += 1

    # ══════════════════════════════════════════════════
    # ROW 4: EQUIPMENT (2cm on head preview)
    # ══════════════════════════════════════════════════
    print("\n  EQUIPMENT:")
    for builder in [equip_01_hardhat, equip_02_headlamp, equip_03_beanie,
                    equip_04_headset, equip_05_gasmask]:
        acc_grid, name = builder()
        y_pos = col * 60
        spawn_mat_mesh(head_dressed, f"{name}_head", head_mats, loc=(0, y_pos, 0))
        spawn_simple_mesh(acc_grid, name, mat_equip, loc=(0, y_pos, HEAD_Z), vs=2)
        print(f"    {name} at Y={y_pos}")
        col += 1

    col += 1

    # ══════════════════════════════════════════════════
    # ROW 5: HAIRSTYLES (2cm on head preview)
    # ══════════════════════════════════════════════════
    mat_hair_acc = make_mat("M_HairAcc", *HAIR_COL)
    print("\n  HAIRSTYLES:")
    for builder in [hair_01_bald, hair_02_buzzcut, hair_03_mohawk,
                    hair_04_curly, hair_05_sidepart, hair_06_braids,
                    hair_07_long, hair_08_ponytail, hair_09_dreads,
                    hair_10_receding, hair_11_messy]:
        acc_grid, name = builder()
        y_pos = col * 60
        spawn_mat_mesh(head_dressed, f"{name}_head", head_mats, loc=(0, y_pos, 0))
        spawn_simple_mesh(acc_grid, name, mat_hair_acc, loc=(0, y_pos, HEAD_Z), vs=2)
        print(f"    {name} at Y={y_pos}")
        col += 1

    # ── Frame ──
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'MATERIAL'
            break

    print(f"\n  Total columns: {col}")
    print("=" * 60)
    print("""
  ╔══════════════════════════════════════════════════════╗
  ║           EXPORT GUIDE: Blender → UE5               ║
  ╚══════════════════════════════════════════════════════╝

  STRUCTURE DES FICHIERS A EXPORTER:
  ──────────────────────────────────
  SK_Crew_Basic.fbx      ← Body complet (outfit Crew)
  SK_Crew_Engineer.fbx   ← Body complet (outfit Engineer)
  SK_Crew_Captain.fbx    ← Body complet (outfit Captain)
  SK_Crew_Diver.fbx      ← Body complet (outfit Diver)
  SK_Crew_Medic.fbx      ← Body complet (outfit Medic)
  SM_Face_01..05.fbx     ← Visages (overlay pieces)
  SM_Beard_01..05.fbx    ← Barbes
  SM_Equip_01..05.fbx    ← Equipements tete

  ETAPE 1 — EXPORT DEPUIS BLENDER:
  ─────────────────────────────────
  Pour chaque objet body:
    1. Selectionner UNIQUEMENT l'objet (ex: Crew_Basic)
    2. File > Export > FBX (.fbx)
    3. Settings:
       - Selected Objects: ON
       - Scale: 1.0
       - Apply Scalings: "FBX All"
       - Forward: "-Y Forward"  (UE5 convention)
       - Up: "Z Up"
       - Geometry > Smoothing: "Face"
       - Armature: decocher (pas encore rigge)
    4. Sauver dans: C:/Dev/Sub3D/Content/Characters/Meshes/

  Pour chaque accessoire:
    1. Selectionner l'accessoire (pas la tete preview)
    2. IMPORTANT: Apply Location (Ctrl+A > Location)
       pour que l'origin soit au centre de la tete
    3. Export FBX memes settings

  ETAPE 2 — RIGGING (Mixamo):
  ────────────────────────────
    1. Aller sur mixamo.adobe.com
    2. Upload le body FBX
    3. Auto-rig (placer les joints)
    4. Download: FBX, "Without Skin" coché, T-pose
    5. Re-import dans Blender pour verifier
    6. Re-export avec armature

  ETAPE 3 — IMPORT UE5:
  ──────────────────────
    1. Drag FBX dans Content/Characters/Meshes/
    2. Import Settings:
       - Skeleton: "None" (premiere fois) ou choisir
         le skeleton existant pour les variants
       - Import Materials: ON
       - Import Textures: OFF (pas de textures image)
    3. Les material slots seront crees automatiquement

  ETAPE 4 — MATERIALS UE5:
  ─────────────────────────
    1. Creer M_Character_Master (Material)
       - Base Color: Vector Parameter "Color"
       - Roughness: Scalar Parameter "Roughness"
    2. Pour chaque slot: creer Material Instance
       - MI_Skin, MI_Suit, MI_SuitDk, MI_Boot, etc.
       - Setter les couleurs matching celles du script
    3. Assigner les MI aux slots du Skeletal Mesh

  ETAPE 5 — ACCESSOIRES MODULAIRES (UE5):
  ────────────────────────────────────────
    1. Import accessoires comme Static Mesh
    2. Sur le Skeleton: ajouter Socket "head_socket"
       a la bone "head" (position 0,0,0)
    3. En Blueprint (BP_SubCrewCharacter):
       - Ajouter StaticMeshComponent "FaceAccessory"
       - Attach to: "head_socket"
       - Set Static Mesh au runtime
    4. Pareil pour Beard, Equipment
""")


main()
