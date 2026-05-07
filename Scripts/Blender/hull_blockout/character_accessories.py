"""
Sub3D — Crew Character + Full Accessories Kit
===============================================
One scene, everything visible:
  - 1 base nude body (multi-material, 2cm voxels + 0.67cm fingers)
  - 5 modular clothing previews (base body + clothing mesh)
  - 1 normalized Tier 1 pressure suit (base body + suit mesh)
  - 6 face variations (0.5cm relief plates with 3D depth)
  - 5 beards (2cm)
  - 5 head equipment (2cm)
  - 11 hairstyles (2cm)

Blender > Scripting > Open > Alt+P
"""

import random

import bpy

VOXEL = 2
CHARACTER_SPACING_CM = 200
ACCESSORY_SPACING_CM = 60
ITEM_SPACING_CM = 90
SECTION_GAP_CM = 100
ASSEMBLY_X_CM = 240
FACE_FORWARD_CM = 2

# ═══════════════════════════════════════════════════════════
# MATERIAL INDICES
# ═══════════════════════════════════════════════════════════
SKIN    = 0
SUIT    = 1
SUIT_DK = 2
BOOT    = 3
HAIR    = 4
ACCENT  = 5
HELMET     = 6
SUIT_SHELL = 7
JOINT      = 8
BRASS      = 9
VISOR_GLOW = 10
LCD_GREEN  = 11
LIGHT      = 12
ARMOR      = 13
EXOSKEL    = 14
BIOLUM_C   = 15
BIOLUM_V   = 16

# Per-outfit colors for SUIT, SUIT_DK, ACCENT slots
OUTFIT_DEFS = {
    'crew': {
        'label': 'Crew_Basic',
        'suit':    (0.03, 0.07, 0.13),   # Navy mechanic coverall
        'suit_dk': (0.01, 0.02, 0.04),
        'accent':  (0.70, 0.55, 0.18),   # Worn brass patches
    },
    'engineer': {
        'label': 'Engineer',
        'suit':    (0.70, 0.26, 0.09),   # Dirty orange coverall
        'suit_dk': (0.28, 0.12, 0.05),
        'accent':  (0.78, 0.56, 0.12),   # Yellow hardhat/paint
    },
    'captain': {
        'label': 'Captain',
        'suit':    (0.16, 0.16, 0.15),   # Faded charcoal jacket
        'suit_dk': (0.06, 0.06, 0.05),
        'accent':  (0.85, 0.65, 0.10),   # Gold
    },
    'diver': {
        'label': 'Diver',
        'suit':    (0.025, 0.025, 0.03), # Black wetsuit
        'suit_dk': (0.01, 0.01, 0.012),
        'accent':  (0.66, 0.52, 0.12),   # Brass/gold bands
    },
    'medic': {
        'label': 'Medic',
        'suit':    (0.70, 0.67, 0.58),   # Worn off-white jumpsuit
        'suit_dk': (0.42, 0.39, 0.33),
        'accent':  (0.65, 0.08, 0.06),   # Red cross
    },
}
CLOTHING_KEYS = ['crew', 'engineer', 'captain', 'diver', 'medic']

# ═══════════════════════════════════════════════════════════
# COMBI TIER 1 DEFINITIONS
# ═══════════════════════════════════════════════════════════

COMBI_TIER1_DEFS = {
    'default': {
        'label': 'Combi_T1_Default',
        'shell':    (0.29, 0.31, 0.32),  # #4A4E52 brushed gunmetal
        'shell_dk': (0.10, 0.10, 0.10),  # #1A1A1A rubber/dark plate
        'helmet':   (0.29, 0.31, 0.32),  # Same tier color for every role
        'brass':    (0.72, 0.45, 0.17),  # #B8732C copper/brass
        'accent':   (0.79, 0.52, 0.12),
    },
    't2_abyss': {
        'label': 'Combi_T2_Abyss',
        'shell':    (0.36, 0.53, 0.42),  # Hospital green, oxidized and colder
        'shell_dk': (0.04, 0.12, 0.16),  # Deep blue rubber
        'helmet':   (0.20, 0.34, 0.36),
        'brass':    (0.72, 0.45, 0.17),
        'accent':   (0.08, 0.28, 0.48),
    },
    't3_hadal': {
        'label': 'Combi_T3_Hadal',
        'shell':    (0.42, 0.08, 0.06),  # Red pressure plating
        'shell_dk': (0.02, 0.04, 0.12),  # Dark blue under-suit
        'helmet':   (0.07, 0.10, 0.22),
        'brass':    (0.82, 0.34, 0.10),
        'accent':   (0.06, 0.30, 0.68),
    },
    'camera_black': {
        'label': 'Combi_CameraRig',
        'shell':    (0.15, 0.18, 0.20),
        'shell_dk': (0.02, 0.02, 0.025),
        'helmet':   (0.08, 0.09, 0.10),
        'brass':    (0.55, 0.34, 0.15),
        'accent':   (0.18, 0.42, 0.56),
    },
}

# ═══════════════════════════════════════════════════════════
# MODULAR EQUIPMENT MODULE LISTS
# ═══════════════════════════════════════════════════════════

HELMET_MODULES = {
    'helmet_t1_default': {
        'label': 'Helmet_T1_Default',
        'tier_visual': 1,
        'flags': {'has_lights': True, 'has_panoramic_visor': True},
        'build_fn': 'build_helmet_t1_default',
    },
    'helmet_t1_lit': {
        'label': 'Helmet_T1_Lit',
        'tier_visual': 1,
        'flags': {'has_lights': True, 'light_consumption_high': True},
        'build_fn': 'build_helmet_t1_lit',
    },
    'helmet_t1_modular': {
        'label': 'Helmet_T1_ModularHUD',
        'tier_visual': 1,
        'flags': {'has_lights': True, 'has_hud_slot': True},
        'build_fn': 'build_helmet_t1_modular',
    },
    'helmet_t1_armored': {
        'label': 'Helmet_T1_Armored',
        'tier_visual': 1,
        'flags': {'has_lights': True, 'is_armored': True},
        'build_fn': 'build_helmet_t1_armored',
    },
    'helmet_t2_observer': {
        'label': 'Helmet_T2_Observer',
        'tier_visual': 2,
        'flags': {'has_lights': True, 'has_panoramic_visor': True, 'has_sensor_crown': True},
        'build_fn': 'build_helmet_t2_observer',
        'material_variant': 't2_abyss',
    },
    'helmet_t2_camera': {
        'label': 'Helmet_T2_CameraRig',
        'tier_visual': 2,
        'flags': {'has_lights': True, 'no_visor': True, 'has_camera_cluster': True},
        'build_fn': 'build_helmet_t2_camera',
        'material_variant': 'camera_black',
    },
    'helmet_t3_splitlens': {
        'label': 'Helmet_T3_SplitLens',
        'tier_visual': 3,
        'flags': {'has_lights': True, 'has_split_lenses': True, 'is_armored': True},
        'build_fn': 'build_helmet_t3_splitlens',
        'material_variant': 't3_hadal',
    },
}

SUIT_MODULES = {
    'suit_t1_default': {
        'label': 'Suit_T1_Default',
        'tier_visual': 1,
        'flags': {'has_o2_tank': True, 'inventory_size': 'medium'},
        'build_fn': 'build_suit_t1_default',
    },
    'suit_t1_explorer': {
        'label': 'Suit_T1_Explorer',
        'tier_visual': 1,
        'flags': {'has_o2_tank': True, 'inventory_size': 'medium', 'has_sensor_pack': True},
        'build_fn': 'build_suit_t1_explorer',
    },
    'suit_t1_mining': {
        'label': 'Suit_T1_Mining',
        'tier_visual': 1,
        'flags': {'has_o2_tank': True, 'inventory_size': 'large', 'is_high_visibility': True},
        'build_fn': 'build_suit_t1_mining',
    },
    'suit_t1_combat': {
        'label': 'Suit_T1_Combat',
        'tier_visual': 1,
        'flags': {'has_o2_tank': True, 'is_armored': True, 'inventory_size': 'small'},
        'build_fn': 'build_suit_t1_combat',
    },
    'suit_t2_abyssal': {
        'label': 'Suit_T2_Abyssal',
        'tier_visual': 2,
        'flags': {'has_o2_tank': True, 'inventory_size': 'large', 'has_external_frame': True},
        'build_fn': 'build_suit_t2_abyssal',
        'material_variant': 't2_abyss',
    },
    'suit_t3_hadal': {
        'label': 'Suit_T3_HadalFrame',
        'tier_visual': 3,
        'flags': {'has_o2_tank': True, 'is_armored': True, 'has_external_frame': True},
        'build_fn': 'build_suit_t3_hadal',
        'material_variant': 't3_hadal',
    },
}

GLOVE_MODULES = {
    'gloves_t1_default': {
        'label': 'Gloves_T1_Default',
        'tier_visual': 1,
        'flags': {'sealed': True},
        'build_fn': 'build_gloves_t1_default',
    },
    'gloves_t1_grapple': {
        'label': 'Gloves_T1_Grapple',
        'tier_visual': 1,
        'flags': {'sealed': True, 'has_grapple': True},
        'build_fn': 'build_gloves_t1_grapple',
    },
    'gloves_t1_tool': {
        'label': 'Gloves_T1_UtilityGrip',
        'tier_visual': 1,
        'flags': {'sealed': True, 'has_tool_mount': True, 'has_reinforced_grip': True},
        'build_fn': 'build_gloves_t1_tool',
    },
    'gloves_t1_reinforced': {
        'label': 'Gloves_T1_Reinforced',
        'tier_visual': 1,
        'flags': {'sealed': True, 'is_armored': True},
        'build_fn': 'build_gloves_t1_reinforced',
    },
}

BOOT_MODULES = {
    'boots_t1_default': {
        'label': 'Boots_T1_Default',
        'tier_visual': 1,
        'flags': {'sealed': True, 'anti_slip': True},
        'build_fn': 'build_boots_t1_default',
    },
    'boots_t1_propeller': {
        'label': 'Boots_T1_Propeller',
        'tier_visual': 1,
        'flags': {'sealed': True, 'has_swim_propulsion': True},
        'build_fn': 'build_boots_t1_propeller',
    },
    'boots_t1_magnetic': {
        'label': 'Boots_T1_Magnetic',
        'tier_visual': 1,
        'flags': {'sealed': True, 'is_magnetic': True},
        'build_fn': 'build_boots_t1_magnetic',
    },
    'boots_t1_combat': {
        'label': 'Boots_T1_Combat',
        'tier_visual': 1,
        'flags': {'sealed': True, 'is_armored': True},
        'build_fn': 'build_boots_t1_combat',
    },
}

EQUIPMENT_MODULES = {
    'helmet': HELMET_MODULES,
    'suit': SUIT_MODULES,
    'gloves': GLOVE_MODULES,
    'boots': BOOT_MODULES,
}

EQUIPMENT_LOADOUTS = [
    (
        'Standard',
        {
            'helmet': 'helmet_t1_default',
            'suit': 'suit_t1_default',
            'gloves': 'gloves_t1_default',
            'boots': 'boots_t1_default',
        },
    ),
    (
        'T1_Explorer',
        {
            'helmet': 'helmet_t1_modular',
            'suit': 'suit_t1_explorer',
            'gloves': 'gloves_t1_grapple',
            'boots': 'boots_t1_propeller',
            'material_variant': 'default',
        },
    ),
    (
        'T1_Mining',
        {
            'helmet': 'helmet_t1_lit',
            'suit': 'suit_t1_mining',
            'gloves': 'gloves_t1_tool',
            'boots': 'boots_t1_magnetic',
            'material_variant': 'default',
        },
    ),
    (
        'T1_Combat',
        {
            'helmet': 'helmet_t1_armored',
            'suit': 'suit_t1_combat',
            'gloves': 'gloves_t1_reinforced',
            'boots': 'boots_t1_combat',
            'material_variant': 'default',
        },
    ),
    (
        'T2_Abyss',
        {
            'helmet': 'helmet_t2_observer',
            'suit': 'suit_t2_abyssal',
            'gloves': 'gloves_t1_tool',
            'boots': 'boots_t1_propeller',
            'material_variant': 't2_abyss',
        },
    ),
    (
        'T2_CameraRig',
        {
            'helmet': 'helmet_t2_camera',
            'suit': 'suit_t1_explorer',
            'gloves': 'gloves_t1_tool',
            'boots': 'boots_t1_magnetic',
            'material_variant': 'camera_black',
        },
    ),
    (
        'T3_Hadal',
        {
            'helmet': 'helmet_t3_splitlens',
            'suit': 'suit_t3_hadal',
            'gloves': 'gloves_t1_reinforced',
            'boots': 'boots_t1_combat',
            'material_variant': 't3_hadal',
        },
    ),
    (
        'Recovery',
        {
            'helmet': 'helmet_t1_lit',
            'suit': 'suit_t1_explorer',
            'gloves': 'gloves_t1_default',
            'boots': 'boots_t1_propeller',
            'material_variant': 't2_abyss',
        },
    ),
    (
        'HullRepair',
        {
            'helmet': 'helmet_t1_modular',
            'suit': 'suit_t1_mining',
            'gloves': 'gloves_t1_tool',
            'boots': 'boots_t1_magnetic',
            'material_variant': 'default',
        },
    ),
]

BACK_MODULES = {
    'tank_o2_single': {
        'label': 'Tank_O2_Single',
        'flags': {'o2_tanks': 1},
        'build_fn': 'build_back_tank_o2_single',
        'material_variant': 'default',
    },
    'tank_o2_double': {
        'label': 'Tank_O2_Double',
        'flags': {'o2_tanks': 2},
        'build_fn': 'build_back_tank_o2_double',
        'material_variant': 't2_abyss',
    },
    'tank_o2_liquid_heavy': {
        'label': 'Tank_LiquidO2_Heavy',
        'flags': {'o2_tanks': 1, 'is_pressurized_heavy': True},
        'build_fn': 'build_back_tank_o2_liquid_heavy',
        'material_variant': 't3_hadal',
    },
    'pack_co2_recycler': {
        'label': 'Pack_CO2_Recycler',
        'flags': {'has_recycler': True},
        'build_fn': 'build_back_pack_co2_recycler',
        'material_variant': 't2_abyss',
    },
    'pack_abyssal_battery': {
        'label': 'Pack_Abyssal_Battery',
        'flags': {'has_battery': True, 'has_bioluminescent_accents': True},
        'build_fn': 'build_back_pack_abyssal_battery',
        'material_variant': 't3_hadal',
    },
}

HANDHELD_TOOLS = {
    'tool_pipe_wrench': {
        'label': 'Tool_PipeWrench',
        'flags': {'is_repair_tool': True, 'two_handed': False},
        'build_fn': 'build_tool_pipe_wrench',
    },
    'tool_welder': {
        'label': 'Tool_Welder',
        'flags': {'is_repair_tool': True, 'has_heat': True},
        'build_fn': 'build_tool_welder',
    },
    'tool_diagnostic': {
        'label': 'Tool_DiagnosticMeter',
        'flags': {'is_repair_tool': True, 'has_screen': True},
        'build_fn': 'build_tool_diagnostic',
    },
    'tool_flashlight': {
        'label': 'Tool_IndustrialFlashlight',
        'flags': {'has_light': True},
        'build_fn': 'build_tool_flashlight',
    },
    'tool_drill_t1': {
        'label': 'Tool_Drill_T1_TwoHand',
        'flags': {'is_mining_tool': True, 'two_handed': True},
        'build_fn': 'build_tool_drill_t1',
    },
    'tool_drill_t2_heavy': {
        'label': 'Tool_Drill_T2_Heavy',
        'flags': {'is_mining_tool': True, 'two_handed': True, 'is_heavy': True},
        'build_fn': 'build_tool_drill_t2_heavy',
    },
    'tool_propulsor_onehand': {
        'label': 'Tool_Propulsor_OneHand',
        'flags': {'has_swim_propulsion': True, 'two_handed': False},
        'build_fn': 'build_tool_propulsor_onehand',
    },
    'tool_propulsor_heavy': {
        'label': 'Tool_Propulsor_HeavyTwoHand',
        'flags': {'has_swim_propulsion': True, 'two_handed': True, 'is_heavy': True},
        'build_fn': 'build_tool_propulsor_heavy',
    },
}

WEAPONS = {
    'weapon_harpoon_pistol': {
        'label': 'Weapon_HarpoonPistol',
        'flags': {'fires_harpoon': True},
        'build_fn': 'build_weapon_harpoon_pistol',
    },
    'weapon_board_revolver': {
        'label': 'Weapon_BoardRevolver',
        'flags': {'ballistic': True, 'emergency_only': True},
        'build_fn': 'build_weapon_board_revolver',
    },
    'weapon_harpoon_rifle': {
        'label': 'Weapon_HarpoonRifle',
        'flags': {'fires_harpoon': True, 'two_handed': True},
        'build_fn': 'build_weapon_harpoon_rifle',
    },
    'weapon_needle_launcher': {
        'label': 'Weapon_NeedleLauncher',
        'flags': {'fires_needles': True, 'underwater': True},
        'build_fn': 'build_weapon_needle_launcher',
    },
    'weapon_net_launcher': {
        'label': 'Weapon_NetLauncher',
        'flags': {'fires_net': True, 'non_lethal': True},
        'build_fn': 'build_weapon_net_launcher',
    },
    'weapon_cavitation_pistol': {
        'label': 'Weapon_CavitationPistol',
        'flags': {'experimental': True, 'underwater': True},
        'build_fn': 'build_weapon_cavitation_pistol',
    },
    'weapon_signal_pistol': {
        'label': 'Weapon_SignalPistol',
        'flags': {'signal': True},
        'build_fn': 'build_weapon_signal_pistol',
    },
}

# Fixed colors for SKIN, BOOT, HAIR (same all outfits)
SKIN_COL = (0.72, 0.55, 0.42)
BOOT_COL = (0.08, 0.07, 0.06)
HAIR_COL = (0.20, 0.12, 0.06)
JOINT_COL = (0.10, 0.10, 0.10)
VISOR_GLOW_COL = (1.00, 0.69, 0.38)
LCD_GREEN_COL = (0.29, 0.88, 0.44)
SAFETY_ACCENT_COL = (0.79, 0.52, 0.12)
LIGHT_COL = (0.95, 0.93, 0.85)
ARMOR_COL = (0.18, 0.20, 0.22)
EXOSKEL_COL = (0.10, 0.10, 0.10)
BIOLUM_C_COL = (0.29, 0.88, 0.88)
BIOLUM_V_COL = (0.69, 0.25, 0.88)


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
# BODY + CLOTHING SPLIT — 2cm voxels
# ═══════════════════════════════════════════════════════════

def build_base_body_mat(body_set):
    return {p: SKIN for p in body_set}

def build_clothing(clothing_key, body_set):
    g = {}

    def set_mat(x, y, z, mat):
        g[(x, y, z)] = mat

    def fb(x0, y0, z0, x1, y1, z1, mat):
        for x in range(x0, x1+1):
            for y in range(y0, y1+1):
                for z in range(z0, z1+1):
                    set_mat(x, y, z, mat)

    def add_if_body(cond, mat):
        for p in body_set:
            if cond(*p):
                g[p] = mat

    def seam_front(z0, z1, mat=SUIT_DK):
        for z in range(z0, z1+1):
            for x in range(3, 6):
                set_mat(x, 0, z, mat)

    def belt(z=49):
        for x in range(-5, 6):
            for y in range(-9, 10):
                if (x, y, z) in body_set or abs(x) >= 4 or abs(y) >= 8:
                    set_mat(x, y, z, SUIT_DK)
        fb(4, -1, z, 5, 1, z+1, ACCENT)

    # Shared clothing coverage: torso, legs, boots, upper arms.
    add_if_body(lambda x, y, z: 44 <= z <= 73 and abs(y) <= 13, SUIT)
    add_if_body(lambda x, y, z: 6 <= z <= 43, SUIT)
    add_if_body(lambda x, y, z: z <= 5, BOOT)
    add_if_body(lambda x, y, z: abs(y) >= 11 and 56 <= z <= 72, SUIT)
    add_if_body(lambda x, y, z: abs(y) >= 11 and 40 <= z <= 55, SUIT_DK)
    seam_front(52, 70)
    belt()

    if clothing_key == 'crew':
        # Navy operator: harness, headset, chest radio, thigh straps.
        fb(5, -7, 56, 6, -4, 66, SUIT_DK)
        fb(5, 4, 56, 6, 7, 66, SUIT_DK)
        fb(6, -3, 59, 7, 3, 64, ACCENT)
        fb(-3, -9, 30, 4, -7, 33, SUIT_DK)
        fb(-3, 7, 30, 4, 9, 33, SUIT_DK)
        fb(-1, -7, 83, 1, -6, 86, SUIT_DK)
        fb(-1, 6, 83, 1, 7, 86, SUIT_DK)
        fb(4, -1, 57, 6, 1, 60, ACCENT)

    elif clothing_key == 'engineer':
        # Orange heavy coverall: hardhat, rolled sleeves, tool belt.
        fb(-5, -6, 90, 5, 6, 91, ACCENT)
        fb(-4, -5, 92, 4, 5, 93, ACCENT)
        fb(3, -7, 90, 7, 7, 90, ACCENT)
        fb(0, -12, 58, 1, -10, 61, SUIT_DK)
        fb(0, 10, 58, 1, 12, 61, SUIT_DK)
        fb(-5, -9, 47, -3, -6, 52, ACCENT)
        fb(-5, 6, 47, -3, 9, 52, ACCENT)
        fb(5, -2, 44, 6, 2, 48, SUIT_DK)
        fb(3, -7, 24, 5, -3, 31, SUIT_DK)
        fb(3, 3, 24, 5, 7, 31, SUIT_DK)

    elif clothing_key == 'captain':
        # Charcoal jacket and peaked cap with gold rank details.
        fb(-5, -9, 52, 6, 9, 57, SUIT_DK)
        fb(1, -12, 69, 2, -9, 71, ACCENT)
        fb(1, 9, 69, 2, 12, 71, ACCENT)
        fb(-5, -5, 90, 5, 5, 91, SUIT_DK)
        fb(-4, -4, 92, 4, 4, 93, SUIT_DK)
        fb(4, -5, 90, 8, 5, 90, SUIT_DK)
        fb(5, -1, 91, 6, 1, 91, ACCENT)
        fb(3, -3, 72, 4, 3, 73, ACCENT)
        fb(4, -8, 35, 5, -4, 41, SUIT_DK)
        fb(4, 4, 35, 5, 8, 41, SUIT_DK)

    elif clothing_key == 'diver':
        # Black diver undersuit: gold bands, hoses, compact back tank.
        add_if_body(lambda x, y, z: abs(y) >= 11 and 40 <= z <= 72, SUIT)
        fb(4, -12, 62, 5, -10, 64, ACCENT)
        fb(4, 10, 62, 5, 12, 64, ACCENT)
        fb(3, -8, 24, 5, -1, 26, ACCENT)
        fb(3, 1, 24, 5, 8, 26, ACCENT)
        fb(-7, -3, 50, -5, 3, 67, SUIT_DK)
        fb(-8, -2, 52, -6, 2, 65, ACCENT)
        for i in range(12):
            set_mat(-6 + i // 3, -8 + i, 62 + i // 3, SUIT_DK)
            set_mat(-5 + i // 3, -8 + i, 62 + i // 3, SUIT_DK)
        fb(5, -1, 55, 6, 1, 59, ACCENT)

    elif clothing_key == 'medic':
        # Worn off-white medical suit: red crosses, chest pouch, armband.
        fb(3, -7, 56, 6, 7, 63, SUIT_DK)
        fb(6, -1, 58, 7, 1, 61, ACCENT)
        fb(5, 0, 57, 7, 0, 62, ACCENT)
        fb(3, 5, 46, 6, 9, 53, SUIT_DK)
        fb(5, 6, 48, 7, 8, 51, ACCENT)
        fb(6, 7, 47, 6, 7, 52, ACCENT)
        fb(0, -12, 62, 1, -10, 65, ACCENT)
        fb(0, 10, 62, 1, 12, 65, ACCENT)
        fb(2, -8, 29, 5, -4, 38, SUIT_DK)
        fb(2, 4, 29, 5, 8, 38, SUIT_DK)

    return g

def build_visible_body_under_clothing(body_set, clothing_grid):
    covered_body = {p for p in clothing_grid if p in body_set}
    return {p: SKIN for p in body_set if p not in covered_body}


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
# COMBI TIER 1 BUILDER — 2cm voxels
# ═══════════════════════════════════════════════════════════

def build_combi_tier1(body_voxels=None):
    if body_voxels is None:
        body = set()
    elif hasattr(body_voxels, 'keys'):
        body = set(body_voxels.keys())
    else:
        body = set(body_voxels)

    g = {
        SUIT_DK: set(),
        HELMET: set(),
        SUIT_SHELL: set(),
        JOINT: set(),
        BRASS: set(),
        VISOR_GLOW: set(),
        LCD_GREEN: set(),
    }

    def overlay(mat, x, y, z):
        p = (x, y, z)
        if p in body:
            return
        for layer in g.values():
            layer.discard(p)
        g[mat].add(p)

    def fb(mat, x0, y0, z0, x1, y1, z1):
        for x in range(x0, x1+1):
            for y in range(y0, y1+1):
                for z in range(z0, z1+1):
                    overlay(mat, x, y, z)

    def profile(mat, slices):
        sl = sorted(slices, key=lambda s: s[0])
        for z in range(sl[0][0], sl[-1][0]+1):
            lo, hi = sl[0], sl[-1]
            for i in range(len(sl)-1):
                if sl[i][0] <= z <= sl[i+1][0]:
                    lo, hi = sl[i], sl[i+1]
                    break
            span = hi[0] - lo[0]
            t = (z - lo[0]) / span if span > 0 else 0
            yw = round(lo[1] + (hi[1] - lo[1]) * t)
            xd = round(lo[2] + (hi[2] - lo[2]) * t)
            fb(mat, -xd, -yw, z, xd, yw, z)

    def tube_y(mat, y0, y1, cx, cz, rx, rz):
        for y in range(y0, y1+1):
            for x in range(cx-rx, cx+rx+1):
                for z in range(cz-rz, cz+rz+1):
                    if ((x - cx) / (rx + 0.25)) ** 2 + ((z - cz) / (rz + 0.25)) ** 2 <= 1.0:
                        overlay(mat, x, y, z)

    def leg_volume(mat, cy, z0, z1, rx, ry):
        for z in range(z0, z1+1):
            for x in range(-rx, rx+1):
                for y in range(cy-ry, cy+ry+1):
                    if (x / (rx + 0.25)) ** 2 + ((y - cy) / (ry + 0.25)) ** 2 <= 1.0:
                        overlay(mat, x, y, z)

    # Exterior panoramic visor: front glass continues over the crown to the upper nape.
    for z in range(80, 92):
        y_half = 5 if z in (80, 91) else 7
        for y in range(-y_half, y_half+1):
            for x in range(11, 15):
                overlay(VISOR_GLOW, x, y, z)
            if abs(y) >= y_half - 1:
                for x in range(8, 12):
                    overlay(VISOR_GLOW, x, y, z)

    for z in range(90, 95):
        for x in range(-6, 13):
            for y in range(-6, 7):
                d = ((x - 3) / 9.6) ** 2 + (y / 6.4) ** 2 + ((z - 89) / 5.8) ** 2
                if 0.72 <= d <= 1.22:
                    overlay(VISOR_GLOW, x, y, z)

    for x in range(-8, -2):
        for y in range(-5, 6):
            for z in range(86, 94):
                d = ((x + 5) / 3.8) ** 2 + (y / 5.8) ** 2 + ((z - 89) / 5.8) ** 2
                if 0.68 <= d <= 1.16:
                    overlay(VISOR_GLOW, x, y, z)

    # Thin helmet frame, rear shell, temple lamps, and sealed neck collar.
    fb(HELMET, -10, -7, 80, -8, 7, 93)
    fb(HELMET, -9, -8, 80, 6, -7, 91)
    fb(HELMET, -9, 7, 80, 6, 8, 91)
    fb(HELMET, -9, -6, 93, -5, 6, 95)
    fb(JOINT, -8, -8, 75, 8, 8, 77)
    fb(JOINT, -7, -7, 78, 7, 7, 78)
    fb(VISOR_GLOW, 11, -10, 84, 14, -9, 87)
    fb(VISOR_GLOW, 11, 9, 84, 14, 10, 87)
    fb(BRASS, 15, -7, 80, 15, -6, 91)
    fb(BRASS, 15, 6, 80, 15, 7, 91)
    fb(BRASS, 15, -6, 80, 15, 6, 81)
    fb(BRASS, 15, -5, 91, 15, 5, 92)
    fb(BRASS, -8, -6, 88, -8, 6, 92)

    # Pressurized torso shell with armor plates.
    profile(SUIT_SHELL, [
        (42, 10, 7), (48, 11, 8), (56, 13, 9),
        (64, 14, 8), (70, 11, 7), (74, 7, 5),
    ])
    fb(SUIT_DK, 7, -8, 53, 9, 8, 69)
    fb(SUIT_DK, 6, -6, 44, 8, 6, 53)
    fb(SUIT_DK, -9, -8, 50, -7, 8, 72)
    fb(SUIT_DK, -6, -5, 69, -4, 5, 74)
    fb(SUIT_SHELL, -5, 12, 64, 6, 18, 74)
    fb(SUIT_SHELL, -5, -18, 64, 6, -12, 74)

    # Arms, gloves, legs, and boots.
    tube_y(SUIT_SHELL, 12, 36, 0, 68, 4, 5)
    tube_y(SUIT_SHELL, -36, -12, 0, 68, 4, 5)
    fb(SUIT_DK, -4, 32, 64, 5, 40, 72)
    fb(SUIT_DK, -4, -40, 64, 5, -32, 72)
    leg_volume(SUIT_SHELL, 4, 23, 43, 5, 5)
    leg_volume(SUIT_SHELL, -4, 23, 43, 5, 5)
    leg_volume(SUIT_SHELL, 4, 6, 22, 4, 5)
    leg_volume(SUIT_SHELL, -4, 6, 22, 4, 5)
    fb(SUIT_DK, -4, 1, 0, 8, 9, 7)
    fb(SUIT_DK, -4, -9, 0, 8, -1, 7)

    # Rubber seals and articulation bands.
    fb(JOINT, -5, 11, 65, 5, 15, 72)
    fb(JOINT, -5, -15, 65, 5, -11, 72)
    fb(JOINT, -4, 21, 64, 4, 24, 72)
    fb(JOINT, -4, -24, 64, 4, -21, 72)
    fb(JOINT, -4, 32, 64, 4, 34, 72)
    fb(JOINT, -4, -34, 64, 4, -32, 72)
    fb(JOINT, -6, -11, 43, 6, 11, 46)
    fb(JOINT, -5, 0, 22, 5, 9, 25)
    fb(JOINT, -5, -9, 22, 5, 0, 25)
    fb(JOINT, -4, 1, 5, 5, 9, 7)
    fb(JOINT, -4, -9, 5, 5, -1, 7)

    # Back tank, harness, hoses, front valve, LCD, rivets.
    for z in range(48, 76):
        for x in range(-15, -8):
            for y in range(-5, 6):
                if ((x + 12) / 3.5) ** 2 + (y / 5.4) ** 2 <= 1.0:
                    overlay(BRASS, x, y, z)
    fb(SUIT_DK, -15, -4, 48, -9, 4, 50)
    fb(SUIT_DK, -15, -4, 74, -9, 4, 76)
    fb(BRASS, -10, -7, 55, -8, 7, 57)
    fb(BRASS, -10, -7, 66, -8, 7, 68)
    fb(BRASS, -13, -1, 76, -11, 1, 78)

    hose_path = [
        (-10, -3, 70), (-9, -3, 70), (-8, -3, 69), (-7, -3, 69),
        (-6, -3, 68), (-5, -3, 68), (-4, -3, 67), (-3, -3, 66),
        (-2, -3, 65), (-1, -3, 64), (0, -3, 63), (1, -3, 62),
        (2, -2, 61), (3, -2, 60), (4, -2, 60), (5, -2, 60),
    ]
    for x, y, z in hose_path:
        fb(BRASS, x, y, z, x, y+1, z)
    fb(BRASS, 8, -3, 57, 10, 3, 62)
    fb(BRASS, 10, -1, 59, 11, 1, 60)
    fb(LCD_GREEN, 5, -29, 66, 6, -25, 70)

    for y in (-8, -4, 4, 8):
        for z in (54, 61, 68):
            overlay(BRASS, 10, y, z)
    for y in (-17, 17):
        for z in (66, 72):
            overlay(BRASS, 6, y, z)

    return {mat: voxels for mat, voxels in g.items() if voxels}


# ═══════════════════════════════════════════════════════════
# MODULAR EQUIPMENT BUILDERS — split from canonical T1 combi
# ═══════════════════════════════════════════════════════════

def _merge_layers(dst, src):
    for mat, voxels in src.items():
        dst.setdefault(mat, set()).update(voxels)

def _is_combi_glove_zone(p):
    x, y, z = p
    return abs(y) >= 29 and 60 <= z <= 75

def _is_combi_boot_zone(p):
    x, y, z = p
    return z <= 7 and -10 <= y <= 10

def _is_combi_helmet_brass(p):
    x, y, z = p
    return z >= 80 and x >= -9

def _filter_combi_tier1(body_voxels, keep_fn):
    result = {}
    for mat, voxels in build_combi_tier1(body_voxels).items():
        kept = {p for p in voxels if keep_fn(mat, p)}
        if kept:
            result[mat] = kept
    return result

def build_helmet_t1_default(body_voxels):
    return _filter_combi_tier1(
        body_voxels,
        lambda mat, p: mat in (HELMET, VISOR_GLOW) or (mat == BRASS and _is_combi_helmet_brass(p)),
    )

def build_suit_t1_default(body_voxels):
    return _filter_combi_tier1(
        body_voxels,
        lambda mat, p: (
            mat in (SUIT_SHELL, LCD_GREEN)
            or (
                mat in (SUIT_DK, JOINT, BRASS)
                and not _is_combi_glove_zone(p)
                and not _is_combi_boot_zone(p)
                and not (mat == BRASS and _is_combi_helmet_brass(p))
            )
        ),
    )

def build_gloves_t1_default(body_voxels):
    return _filter_combi_tier1(
        body_voxels,
        lambda mat, p: mat in (SUIT_DK, JOINT, BRASS) and _is_combi_glove_zone(p),
    )

def build_boots_t1_default(body_voxels):
    return _filter_combi_tier1(
        body_voxels,
        lambda mat, p: mat in (SUIT_DK, JOINT, BRASS, BOOT) and _is_combi_boot_zone(p),
    )

def _box_layer(layers, mat, x0, y0, z0, x1, y1, z1):
    voxels = layers.setdefault(mat, set())
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                voxels.add((x, y, z))

def _line_layer(layers, mat, points):
    voxels = layers.setdefault(mat, set())
    for p in points:
        voxels.add(p)

def _body_points(body_voxels):
    if body_voxels is None:
        return set()
    if hasattr(body_voxels, 'keys'):
        return set(body_voxels.keys())
    return set(body_voxels)

def _overlay_layer(layers, body, mat, x, y, z):
    p = (x, y, z)
    if p in body:
        return
    for voxels in layers.values():
        voxels.discard(p)
    layers.setdefault(mat, set()).add(p)

def _box_overlay_layer(layers, body, mat, x0, y0, z0, x1, y1, z1):
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                _overlay_layer(layers, body, mat, x, y, z)

def build_helmet_t1_lit(body_voxels):
    layers = build_helmet_t1_default(body_voxels)
    _box_layer(layers, LIGHT, 9, -10, 86, 12, -9, 89)
    _box_layer(layers, LIGHT, 9, 9, 86, 12, 10, 89)
    _box_layer(layers, LIGHT, 7, -3, 94, 11, 3, 96)
    _box_layer(layers, BRASS, 8, -10, 85, 12, -9, 85)
    _box_layer(layers, BRASS, 8, 9, 85, 12, 10, 85)
    _box_layer(layers, BRASS, 6, -4, 93, 12, 4, 93)
    return layers

def build_helmet_t1_modular(body_voxels):
    layers = build_helmet_t1_default(body_voxels)
    _box_layer(layers, BRASS, 12, 6, 83, 14, 8, 91)
    _box_layer(layers, LCD_GREEN, 10, -3, 92, 12, 3, 94)
    _box_layer(layers, BIOLUM_C, 13, 7, 85, 14, 8, 89)
    _box_layer(layers, BIOLUM_V, 13, -8, 85, 14, -7, 89)
    return layers

def build_helmet_t1_armored(body_voxels):
    layers = build_helmet_t1_default(body_voxels)
    _box_layer(layers, ARMOR, -12, -8, 81, -10, 8, 93)
    _box_layer(layers, ARMOR, -10, -9, 84, 6, -8, 91)
    _box_layer(layers, ARMOR, -10, 8, 84, 6, 9, 91)
    _box_layer(layers, ARMOR, -7, -6, 95, 7, 6, 96)
    _box_layer(layers, LIGHT, 10, -8, 82, 12, -7, 84)
    _box_layer(layers, LIGHT, 10, 7, 82, 12, 8, 84)
    return layers

def build_helmet_t2_observer(body_voxels):
    layers = build_helmet_t1_default(body_voxels)
    _box_layer(layers, HELMET, -13, -8, 82, -10, 8, 94)
    _box_layer(layers, BRASS, -14, -5, 86, -13, 5, 92)
    _box_layer(layers, EXOSKEL, -12, -9, 80, 10, -8, 88)
    _box_layer(layers, EXOSKEL, -12, 8, 80, 10, 9, 88)
    _box_layer(layers, BIOLUM_C, 16, -4, 84, 17, -2, 89)
    _box_layer(layers, BIOLUM_C, 16, 2, 84, 17, 4, 89)
    _box_layer(layers, LIGHT, 13, -9, 82, 15, -8, 85)
    _box_layer(layers, LIGHT, 13, 8, 82, 15, 9, 85)
    _box_layer(layers, LCD_GREEN, 7, -2, 95, 12, 2, 96)
    return layers

def build_helmet_t2_camera(body_voxels):
    body = _body_points(body_voxels)
    layers = {}
    _box_overlay_layer(layers, body, HELMET, -11, -8, 78, 9, 8, 93)
    _box_overlay_layer(layers, body, HELMET, -13, -6, 82, -10, 6, 94)
    _box_overlay_layer(layers, body, JOINT, -8, -8, 75, 8, 8, 78)
    _box_overlay_layer(layers, body, ARMOR, 8, -6, 81, 13, 6, 91)
    _box_overlay_layer(layers, body, ARMOR, 10, -7, 84, 15, -5, 89)
    _box_overlay_layer(layers, body, ARMOR, 10, 5, 84, 15, 7, 89)
    _box_overlay_layer(layers, body, BRASS, 14, -1, 83, 16, 1, 88)
    _box_overlay_layer(layers, body, LIGHT, 16, -5, 85, 17, -4, 87)
    _box_overlay_layer(layers, body, LIGHT, 16, 4, 85, 17, 5, 87)
    _box_overlay_layer(layers, body, BIOLUM_C, 16, -1, 84, 17, 1, 89)
    _box_overlay_layer(layers, body, LCD_GREEN, 11, -3, 92, 15, 3, 94)
    _box_overlay_layer(layers, body, EXOSKEL, -14, -8, 86, -13, 8, 91)
    return layers

def build_helmet_t3_splitlens(body_voxels):
    body = _body_points(body_voxels)
    layers = {}
    _box_overlay_layer(layers, body, HELMET, -12, -8, 78, 10, 8, 94)
    _box_overlay_layer(layers, body, JOINT, -9, -8, 75, 9, 8, 78)
    _box_overlay_layer(layers, body, ARMOR, -14, -7, 82, -11, 7, 95)
    _box_overlay_layer(layers, body, ARMOR, 11, -8, 80, 14, -2, 92)
    _box_overlay_layer(layers, body, ARMOR, 11, 2, 80, 14, 8, 92)
    _box_overlay_layer(layers, body, ARMOR, 12, -1, 80, 15, 1, 93)
    _box_overlay_layer(layers, body, VISOR_GLOW, 15, -6, 82, 16, -3, 90)
    _box_overlay_layer(layers, body, VISOR_GLOW, 15, 3, 82, 16, 6, 90)
    _box_overlay_layer(layers, body, BIOLUM_V, 16, -1, 86, 17, 1, 91)
    _box_overlay_layer(layers, body, LIGHT, 13, -9, 84, 15, -8, 87)
    _box_overlay_layer(layers, body, LIGHT, 13, 8, 84, 15, 9, 87)
    _box_overlay_layer(layers, body, BRASS, -15, -4, 87, -14, 4, 94)
    return layers

def build_suit_t1_explorer(body_voxels):
    layers = build_suit_t1_default(body_voxels)
    _box_layer(layers, BIOLUM_C, 10, -9, 54, 11, -8, 68)
    _box_layer(layers, BIOLUM_C, 10, 8, 54, 11, 9, 68)
    _box_layer(layers, LCD_GREEN, 9, -4, 64, 11, 4, 68)
    _box_layer(layers, BRASS, -16, -5, 56, -14, 5, 72)
    _box_layer(layers, LIGHT, 8, -10, 60, 10, -9, 63)
    _box_layer(layers, LIGHT, 8, 9, 60, 10, 10, 63)
    return layers

def build_suit_t1_mining(body_voxels):
    layers = build_suit_t1_default(body_voxels)
    _box_layer(layers, ACCENT, 9, -9, 52, 11, 9, 69)
    _box_layer(layers, ACCENT, 7, -12, 43, 10, -8, 58)
    _box_layer(layers, ACCENT, 7, 8, 43, 10, 12, 58)
    _box_layer(layers, BRASS, -17, -7, 49, -15, -2, 74)
    _box_layer(layers, BRASS, -17, 2, 49, -15, 7, 74)
    _box_layer(layers, EXOSKEL, 11, -13, 45, 12, -10, 56)
    _box_layer(layers, EXOSKEL, 11, 10, 45, 12, 13, 56)
    return layers

def build_suit_t1_combat(body_voxels):
    layers = build_suit_t1_default(body_voxels)
    _box_layer(layers, ARMOR, 10, -10, 52, 13, 10, 68)
    _box_layer(layers, ARMOR, 8, -7, 42, 11, -2, 54)
    _box_layer(layers, ARMOR, 8, 2, 42, 11, 7, 54)
    _box_layer(layers, ARMOR, 6, -8, 24, 9, -2, 39)
    _box_layer(layers, ARMOR, 6, 2, 24, 9, 8, 39)
    _box_layer(layers, EXOSKEL, -14, -2, 52, -12, 2, 73)
    _box_layer(layers, BRASS, 12, -10, 49, 13, 10, 51)
    return layers

def build_suit_t2_abyssal(body_voxels):
    layers = build_suit_t1_default(body_voxels)
    _box_layer(layers, SUIT_SHELL, 10, -12, 50, 13, -9, 71)
    _box_layer(layers, SUIT_SHELL, 10, 9, 50, 13, 12, 71)
    _box_layer(layers, ARMOR, 9, -8, 46, 12, 8, 66)
    _box_layer(layers, EXOSKEL, -16, -8, 46, -14, -6, 76)
    _box_layer(layers, EXOSKEL, -16, 6, 46, -14, 8, 76)
    _box_layer(layers, BRASS, -19, -6, 48, -17, -1, 76)
    _box_layer(layers, BRASS, -19, 1, 48, -17, 6, 76)
    _box_layer(layers, ACCENT, 12, -10, 55, 13, -9, 69)
    _box_layer(layers, ACCENT, 12, 9, 55, 13, 10, 69)
    _box_layer(layers, BIOLUM_C, 14, -3, 58, 15, -2, 70)
    _box_layer(layers, BIOLUM_C, 14, 2, 58, 15, 3, 70)
    _box_layer(layers, LIGHT, 13, -8, 62, 15, -7, 65)
    _box_layer(layers, LIGHT, 13, 7, 62, 15, 8, 65)
    return layers

def build_suit_t3_hadal(body_voxels):
    layers = build_suit_t1_default(body_voxels)
    _box_layer(layers, ARMOR, 10, -11, 47, 14, 11, 70)
    _box_layer(layers, ARMOR, 7, -8, 24, 11, -2, 43)
    _box_layer(layers, ARMOR, 7, 2, 24, 11, 8, 43)
    _box_layer(layers, EXOSKEL, -18, -9, 44, -16, -7, 78)
    _box_layer(layers, EXOSKEL, -18, 7, 44, -16, 9, 78)
    _box_layer(layers, EXOSKEL, -18, -9, 73, 12, -7, 75)
    _box_layer(layers, EXOSKEL, -18, 7, 73, 12, 9, 75)
    _box_layer(layers, BRASS, -21, -4, 50, -19, 4, 76)
    _box_layer(layers, BIOLUM_V, -22, -2, 58, -21, 2, 70)
    _box_layer(layers, BIOLUM_C, 15, -8, 53, 16, -7, 69)
    _box_layer(layers, BIOLUM_C, 15, 7, 53, 16, 8, 69)
    _box_layer(layers, LCD_GREEN, 14, -4, 60, 16, 4, 65)
    return layers

def build_gloves_t1_grapple(body_voxels):
    layers = build_gloves_t1_default(body_voxels)
    _box_layer(layers, BRASS, 5, 33, 69, 10, 38, 74)
    _box_layer(layers, EXOSKEL, 8, 36, 68, 14, 36, 70)
    _line_layer(layers, EXOSKEL, [(x, 37, 72) for x in range(8, 18)])
    _box_layer(layers, LIGHT, 14, 36, 70, 16, 38, 72)
    return layers

def build_gloves_t1_tool(body_voxels):
    layers = build_gloves_t1_default(body_voxels)
    _box_layer(layers, ARMOR, 5, 32, 64, 9, 38, 71)
    _box_layer(layers, ARMOR, 5, -38, 64, 9, -32, 71)
    _box_layer(layers, BRASS, 8, 34, 66, 10, 36, 70)
    _box_layer(layers, BRASS, 8, -36, 66, 10, -34, 70)
    return layers

def build_gloves_t1_reinforced(body_voxels):
    layers = build_gloves_t1_default(body_voxels)
    _box_layer(layers, ARMOR, 4, 31, 64, 8, 39, 72)
    _box_layer(layers, ARMOR, 4, -39, 64, 8, -31, 72)
    _box_layer(layers, EXOSKEL, -5, 33, 64, -4, 39, 72)
    _box_layer(layers, EXOSKEL, -5, -39, 64, -4, -33, 72)
    return layers

def build_boots_t1_propeller(body_voxels):
    layers = build_boots_t1_default(body_voxels)
    _box_layer(layers, BRASS, -9, 3, 1, -6, 8, 5)
    _box_layer(layers, BRASS, -9, -8, 1, -6, -3, 5)
    _box_layer(layers, LIGHT, -10, 4, 2, -10, 7, 4)
    _box_layer(layers, LIGHT, -10, -7, 2, -10, -4, 4)
    _box_layer(layers, EXOSKEL, -6, 1, 5, -4, 9, 7)
    _box_layer(layers, EXOSKEL, -6, -9, 5, -4, -1, 7)
    return layers

def build_boots_t1_magnetic(body_voxels):
    layers = build_boots_t1_default(body_voxels)
    _box_layer(layers, EXOSKEL, -5, 1, -3, 8, 9, -1)
    _box_layer(layers, EXOSKEL, -5, -9, -3, 8, -1, -1)
    _box_layer(layers, BIOLUM_C, 4, 3, 5, 6, 7, 7)
    _box_layer(layers, BIOLUM_C, 4, -7, 5, 6, -3, 7)
    return layers

def build_boots_t1_combat(body_voxels):
    layers = build_boots_t1_default(body_voxels)
    _box_layer(layers, ARMOR, 4, 1, 6, 8, 9, 18)
    _box_layer(layers, ARMOR, 4, -9, 6, 8, -1, 18)
    _box_layer(layers, EXOSKEL, -5, 1, -2, 9, 9, 0)
    _box_layer(layers, EXOSKEL, -5, -9, -2, 9, -1, 0)
    _box_layer(layers, BRASS, 8, 3, 10, 10, 7, 12)
    _box_layer(layers, BRASS, 8, -7, 10, 10, -3, 12)
    return layers

def _item_box(layers, mat, x0, y0, z0, x1, y1, z1):
    voxels = layers.setdefault(mat, set())
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                voxels.add((x, y, z))

def _item_line(layers, mat, points):
    voxels = layers.setdefault(mat, set())
    for a, b in zip(points, points[1:]):
        ax, ay, az = a
        bx, by, bz = b
        steps = max(abs(bx - ax), abs(by - ay), abs(bz - az), 1)
        for i in range(steps + 1):
            t = i / steps
            voxels.add((
                round(ax + (bx - ax) * t),
                round(ay + (by - ay) * t),
                round(az + (bz - az) * t),
            ))
    if points:
        voxels.add(points[-1])

def _item_tube_z(layers, mat, cx, cy, z0, z1, rx, ry):
    voxels = layers.setdefault(mat, set())
    for z in range(z0, z1+1):
        for x in range(cx-rx, cx+rx+1):
            for y in range(cy-ry, cy+ry+1):
                if ((x - cx) / (rx + 0.25)) ** 2 + ((y - cy) / (ry + 0.25)) ** 2 <= 1.0:
                    voxels.add((x, y, z))

def _item_tube_y(layers, mat, y0, y1, cx, cz, rx, rz):
    voxels = layers.setdefault(mat, set())
    for y in range(y0, y1+1):
        for x in range(cx-rx, cx+rx+1):
            for z in range(cz-rz, cz+rz+1):
                if ((x - cx) / (rx + 0.25)) ** 2 + ((z - cz) / (rz + 0.25)) ** 2 <= 1.0:
                    voxels.add((x, y, z))

def _layers_to_grid(layers):
    grid = {}
    for mat, voxels in layers.items():
        for p in voxels:
            grid[p] = mat
    return grid


# ═══════════════════════════════════════════════════════════
# BACK MODULE BUILDERS — character-space, 2cm voxels
# ═══════════════════════════════════════════════════════════

def build_back_tank_o2_single():
    layers = {}
    _item_tube_z(layers, BRASS, -15, 0, 48, 76, 3, 5)
    _item_box(layers, SUIT_DK, -18, -4, 49, -12, 4, 51)
    _item_box(layers, SUIT_DK, -18, -4, 73, -12, 4, 75)
    _item_box(layers, BRASS, -16, -1, 76, -14, 1, 79)
    _item_box(layers, JOINT, -11, -7, 56, -10, 7, 58)
    _item_box(layers, JOINT, -11, -7, 67, -10, 7, 69)
    return layers

def build_back_tank_o2_double():
    layers = {}
    for cy in (-4, 4):
        _item_tube_z(layers, BRASS, -16, cy, 46, 78, 3, 3)
        _item_box(layers, SUIT_DK, -19, cy-2, 47, -13, cy+2, 49)
        _item_box(layers, SUIT_DK, -19, cy-2, 75, -13, cy+2, 77)
        _item_box(layers, BRASS, -17, cy-1, 78, -15, cy+1, 80)
    _item_box(layers, EXOSKEL, -12, -7, 55, -10, 7, 57)
    _item_box(layers, EXOSKEL, -12, -7, 67, -10, 7, 69)
    return layers

def build_back_tank_o2_liquid_heavy():
    layers = {}
    _item_tube_z(layers, ARMOR, -19, 0, 42, 82, 5, 7)
    _item_tube_z(layers, BRASS, -19, 0, 46, 78, 3, 5)
    _item_box(layers, EXOSKEL, -25, -7, 45, -13, 7, 48)
    _item_box(layers, EXOSKEL, -25, -7, 76, -13, 7, 79)
    _item_box(layers, BIOLUM_C, -26, -2, 55, -25, 2, 70)
    _item_box(layers, LCD_GREEN, -13, -4, 60, -12, 4, 66)
    return layers

def build_back_pack_co2_recycler():
    layers = {}
    _item_box(layers, ARMOR, -20, -7, 48, -13, 7, 73)
    _item_box(layers, SUIT_DK, -21, -5, 51, -20, 5, 70)
    _item_box(layers, BRASS, -12, -6, 56, -11, -2, 64)
    _item_box(layers, BRASS, -12, 2, 56, -11, 6, 64)
    _item_box(layers, LCD_GREEN, -12, -3, 66, -11, 3, 70)
    _item_box(layers, BIOLUM_C, -21, -7, 53, -20, -6, 68)
    _item_box(layers, BIOLUM_C, -21, 6, 53, -20, 7, 68)
    return layers

def build_back_pack_abyssal_battery():
    layers = {}
    _item_box(layers, EXOSKEL, -22, -8, 46, -14, 8, 78)
    _item_tube_z(layers, ARMOR, -20, -4, 50, 75, 3, 3)
    _item_tube_z(layers, ARMOR, -20, 4, 50, 75, 3, 3)
    _item_box(layers, BIOLUM_V, -24, -2, 54, -23, 2, 72)
    _item_box(layers, BIOLUM_C, -13, -6, 58, -12, 6, 64)
    _item_box(layers, BRASS, -19, -9, 76, -15, 9, 79)
    return layers


# ═══════════════════════════════════════════════════════════
# HANDHELD TOOL BUILDERS — display-space, 2cm voxels
# ═══════════════════════════════════════════════════════════

def build_tool_pipe_wrench():
    layers = {}
    _item_box(layers, JOINT, -1, -14, 1, 1, 8, 3)
    _item_box(layers, ARMOR, -3, 7, 1, 3, 12, 5)
    _item_box(layers, BRASS, -5, 11, 3, -2, 16, 7)
    _item_box(layers, BRASS, 2, 11, 3, 5, 16, 7)
    _item_box(layers, EXOSKEL, -2, 13, 0, 2, 14, 2)
    return layers

def build_tool_welder():
    layers = {}
    _item_box(layers, ARMOR, -3, -4, 3, 3, 7, 7)
    _item_box(layers, JOINT, -2, -8, -1, 2, -4, 4)
    _item_box(layers, BRASS, -1, 7, 4, 1, 14, 6)
    _item_box(layers, LIGHT, -1, 14, 4, 1, 16, 6)
    _item_line(layers, JOINT, [(-3, -3, 4), (-7, -6, 3), (-10, -5, 4), (-12, -2, 4)])
    _item_tube_z(layers, BRASS, -13, -1, 0, 8, 2, 2)
    return layers

def build_tool_diagnostic():
    layers = {}
    _item_box(layers, SUIT_DK, -5, -7, 0, 5, 7, 8)
    _item_box(layers, LCD_GREEN, -4, -5, 6, 4, 1, 9)
    _item_box(layers, BRASS, -3, 3, 7, -1, 5, 9)
    _item_box(layers, BRASS, 1, 3, 7, 3, 5, 9)
    _item_line(layers, JOINT, [(-3, -7, 4), (-7, -10, 3), (-10, -9, 4)])
    _item_line(layers, ACCENT, [(3, -7, 4), (7, -10, 3), (10, -9, 4)])
    return layers

def build_tool_flashlight():
    layers = {}
    _item_tube_y(layers, ARMOR, -12, 8, 0, 3, 2, 2)
    _item_box(layers, JOINT, -2, -10, 1, 2, 0, 5)
    _item_tube_y(layers, BRASS, 8, 13, 0, 3, 4, 3)
    _item_box(layers, LIGHT, -3, 13, 1, 3, 15, 5)
    return layers

def build_tool_drill_t1():
    layers = {}
    _item_tube_y(layers, ARMOR, -16, 10, 0, 4, 3, 3)
    _item_tube_y(layers, BRASS, 10, 18, 0, 4, 2, 2)
    _item_box(layers, JOINT, -5, -10, -2, 5, -6, 2)
    _item_box(layers, JOINT, -5, 1, -2, 5, 5, 2)
    _item_line(layers, EXOSKEL, [(0, 18, 4), (0, 20, 4), (0, 22, 5)])
    _item_box(layers, LIGHT, -2, 7, 7, 2, 9, 9)
    return layers

def build_tool_drill_t2_heavy():
    layers = {}
    _item_tube_y(layers, ARMOR, -22, 12, 0, 5, 5, 4)
    _item_tube_y(layers, EXOSKEL, -18, 8, 0, 5, 6, 5)
    _item_tube_y(layers, BRASS, 12, 24, 0, 5, 3, 3)
    _item_line(layers, BRASS, [(0, 24, 5), (0, 27, 5), (0, 30, 6)])
    _item_box(layers, JOINT, -7, -18, -2, 7, -14, 3)
    _item_box(layers, JOINT, -7, 0, -2, 7, 4, 3)
    _item_box(layers, BIOLUM_C, -2, -4, 9, 2, 8, 10)
    return layers

def build_tool_propulsor_onehand():
    layers = {}
    _item_tube_y(layers, ARMOR, -9, 9, 0, 4, 4, 3)
    _item_tube_y(layers, BRASS, 5, 13, 0, 4, 5, 4)
    _item_box(layers, LIGHT, -3, 13, 2, 3, 15, 6)
    _item_box(layers, JOINT, -2, -8, -2, 2, -3, 2)
    _item_box(layers, BIOLUM_C, -1, 1, 8, 1, 8, 9)
    return layers

def build_tool_propulsor_heavy():
    layers = {}
    _item_tube_y(layers, EXOSKEL, -20, 16, 0, 5, 6, 5)
    _item_tube_y(layers, BRASS, 10, 22, 0, 5, 7, 6)
    _item_box(layers, LIGHT, -5, 22, 1, 5, 24, 9)
    _item_box(layers, JOINT, -8, -14, -2, 8, -10, 3)
    _item_box(layers, JOINT, -8, 0, -2, 8, 4, 3)
    _item_box(layers, BIOLUM_C, -2, -4, 11, 2, 12, 12)
    return layers


# ═══════════════════════════════════════════════════════════
# WEAPON BUILDERS — display-space, 2cm voxels
# ═══════════════════════════════════════════════════════════

def build_weapon_harpoon_pistol():
    layers = {}
    _item_box(layers, ARMOR, -3, -4, 3, 3, 8, 7)
    _item_box(layers, JOINT, -2, -8, -1, 2, -4, 4)
    _item_tube_y(layers, BRASS, 6, 15, 0, 5, 1, 1)
    _item_tube_y(layers, SUIT_DK, -1, 8, 0, 1, 2, 2)
    _item_line(layers, EXOSKEL, [(0, 8, 6), (0, 16, 6), (0, 19, 7)])
    return layers

def build_weapon_board_revolver():
    layers = {}
    _item_box(layers, ARMOR, -3, -5, 3, 3, 5, 7)
    _item_tube_y(layers, ARMOR, 4, 13, 0, 5, 1, 1)
    _item_tube_y(layers, BRASS, -3, 2, 0, 5, 3, 3)
    _item_box(layers, JOINT, -2, -9, -1, 2, -5, 4)
    _item_box(layers, BRASS, -4, -2, 4, 4, 0, 7)
    return layers

def build_weapon_harpoon_rifle():
    layers = {}
    _item_tube_y(layers, ARMOR, -20, 20, 0, 5, 2, 2)
    _item_tube_y(layers, BRASS, 12, 24, 0, 5, 1, 1)
    _item_box(layers, JOINT, -5, -20, 2, 5, -14, 7)
    _item_box(layers, JOINT, -3, -7, -1, 3, -3, 4)
    _item_tube_z(layers, BRASS, -5, 3, 0, 8, 2, 2)
    _item_line(layers, EXOSKEL, [(0, 18, 6), (0, 26, 6), (0, 30, 7)])
    return layers

def build_weapon_needle_launcher():
    layers = {}
    for x in (-3, 0, 3):
        _item_tube_y(layers, ARMOR, -12, 18, x, 5, 1, 1)
    _item_box(layers, SUIT_DK, -5, -5, 2, 5, 4, 8)
    _item_box(layers, JOINT, -2, -10, -1, 2, -5, 4)
    _item_box(layers, LCD_GREEN, -4, 4, 7, 4, 8, 9)
    _item_box(layers, BRASS, -5, 11, 3, 5, 14, 7)
    return layers

def build_weapon_net_launcher():
    layers = {}
    _item_box(layers, ARMOR, -5, -8, 2, 5, 10, 8)
    _item_box(layers, BRASS, -7, 10, 1, 7, 17, 9)
    _item_box(layers, JOINT, -2, -13, -1, 2, -8, 4)
    _item_tube_z(layers, EXOSKEL, -6, -2, 1, 8, 2, 2)
    _item_tube_z(layers, EXOSKEL, 6, -2, 1, 8, 2, 2)
    _item_line(layers, JOINT, [(-5, 13, 6), (-2, 16, 6), (2, 16, 6), (5, 13, 6)])
    return layers

def build_weapon_cavitation_pistol():
    layers = {}
    _item_box(layers, ARMOR, -4, -5, 2, 4, 7, 8)
    _item_box(layers, JOINT, -2, -10, -2, 2, -5, 4)
    _item_tube_y(layers, BRASS, 6, 13, 0, 5, 3, 3)
    _item_box(layers, BIOLUM_C, -4, 12, 1, 4, 15, 9)
    _item_box(layers, BIOLUM_V, -2, 15, 3, 2, 17, 7)
    _item_box(layers, LCD_GREEN, -3, 1, 8, 3, 5, 10)
    return layers

def build_weapon_signal_pistol():
    layers = {}
    _item_box(layers, ACCENT, -3, -5, 2, 3, 6, 7)
    _item_box(layers, JOINT, -2, -10, -2, 2, -5, 4)
    _item_tube_y(layers, ARMOR, 4, 12, 0, 5, 2, 2)
    _item_box(layers, BRASS, -4, 0, 7, 4, 3, 9)
    _item_box(layers, LIGHT, -2, 12, 3, 2, 14, 7)
    return layers

def assemble_equipment_loadout(loadout, body_voxels):
    layers = {}
    for slot in ['helmet', 'suit', 'gloves', 'boots']:
        module_id = loadout.get(slot)
        if not module_id:
            continue
        module_defs = EQUIPMENT_MODULES[slot]
        if module_id not in module_defs:
            print(f"WARN: unknown {slot} module '{module_id}'")
            continue
        build_fn = globals()[module_defs[module_id]['build_fn']]
        _merge_layers(layers, build_fn(body_voxels))
    return layers


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

def make_base_body_materials():
    return [make_mat("BaseBody_Skin", *SKIN_COL)]

def make_clothing_materials(clothing_key):
    od = OUTFIT_DEFS[clothing_key]
    return [
        make_mat(f"{od['label']}_UnusedSkin", *SKIN_COL),
        make_mat(f"{od['label']}_Cloth",      *od['suit']),
        make_mat(f"{od['label']}_ClothDk",    *od['suit_dk']),
        make_mat(f"{od['label']}_Boot",       *BOOT_COL),
        make_mat(f"{od['label']}_UnusedHair", *HAIR_COL),
        make_mat(f"{od['label']}_Accent",     *od['accent']),
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

def spawn_body_with_fingers(body_dict, name, materials, loc=(0,0,0), fingers=True):
    finger_grids = [build_fingers_r(), build_fingers_l()] if fingers else []
    v, f, fm = generate_mesh_mat_multi(body_dict, finger_grids)
    me = bpy.data.meshes.new(name)
    me.from_pydata(v, [], f)
    me.validate(); me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    for m in materials:
        ob.data.materials.append(m)
    for i, poly in enumerate(me.polygons):
        poly.material_index = fm[i]
    ob.location = loc
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_flat()
    ob.select_set(False)
    return ob

def spawn_clothing_preview(clothing_key, y_offset):
    od = OUTFIT_DEFS[clothing_key]
    body_set = build_body()
    clothing = build_clothing(clothing_key, body_set)
    visible_body = build_visible_body_under_clothing(body_set, clothing)
    body_ob = spawn_body_with_fingers(
        visible_body,
        f"{od['label']}_BaseBodyPreview",
        make_base_body_materials(),
        loc=(0, y_offset, 0),
    )
    clothing_ob = spawn_mat_mesh(
        clothing,
        f"Clothing_{od['label']}",
        make_clothing_materials(clothing_key),
        loc=(0, y_offset, 0),
        vs=VOXEL,
    )
    return body_ob, clothing_ob

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
# COMBI TIER 1 BLENDER HELPERS
# ═══════════════════════════════════════════════════════════

def _set_mat_alpha(mat, color, alpha):
    mat.diffuse_color = (color[0], color[1], color[2], alpha)
    mat.blend_method = 'BLEND'
    if hasattr(mat, 'show_transparent_back'):
        mat.show_transparent_back = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        alpha_socket = bsdf.inputs.get("Alpha")
        if alpha_socket is not None:
            alpha_socket.default_value = alpha

def _set_mat_emission(mat, color, strength):
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        color_socket = bsdf.inputs.get("Emission Color")
        strength_socket = bsdf.inputs.get("Emission Strength")
        if color_socket is not None:
            color_socket.default_value = (color[0], color[1], color[2], 1)
        if strength_socket is not None:
            strength_socket.default_value = strength

def make_combi_tier1_materials(variant):
    """Create material slots 0..16 for monolithic and modular pressure gear."""
    cd = COMBI_TIER1_DEFS[variant]
    visor = make_mat(f"{cd['label']}_VisorGlow", *VISOR_GLOW_COL, 0.0, 0.25)
    lcd = make_mat(f"{cd['label']}_LCDGreen", *LCD_GREEN_COL, 0.0, 0.35)
    light = make_mat(f"{cd['label']}_Light", *LIGHT_COL, 0.0, 0.25)
    armor = make_mat(f"{cd['label']}_Armor", *ARMOR_COL, 0.35, 0.55)
    exoskel = make_mat(f"{cd['label']}_Exoskel", *EXOSKEL_COL, 0.25, 0.8)
    biolum_c = make_mat(f"{cd['label']}_BiolumCyan", *BIOLUM_C_COL, 0.0, 0.35)
    biolum_v = make_mat(f"{cd['label']}_BiolumViolet", *BIOLUM_V_COL, 0.0, 0.35)
    _set_mat_alpha(visor, VISOR_GLOW_COL, 0.26)
    _set_mat_emission(visor, VISOR_GLOW_COL, 0.16)
    _set_mat_emission(lcd, LCD_GREEN_COL, 0.7)
    _set_mat_emission(light, LIGHT_COL, 0.9)
    _set_mat_emission(biolum_c, BIOLUM_C_COL, 0.8)
    _set_mat_emission(biolum_v, BIOLUM_V_COL, 0.8)
    return [
        make_mat(f"{cd['label']}_UnusedSkin", *SKIN_COL),
        make_mat(f"{cd['label']}_UnusedSuit", *cd['shell']),
        make_mat(f"{cd['label']}_ShellDk", *cd['shell_dk'], 0.2, 0.75),
        make_mat(f"{cd['label']}_UnusedBoot", *BOOT_COL),
        make_mat(f"{cd['label']}_UnusedHair", *HAIR_COL),
        make_mat(f"{cd['label']}_SafetyAccent", *cd.get('accent', SAFETY_ACCENT_COL), 0.1, 0.65),
        make_mat(f"{cd['label']}_Helmet", *cd['helmet'], 0.25, 0.65),
        make_mat(f"{cd['label']}_Shell", *cd['shell'], 0.18, 0.7),
        make_mat(f"{cd['label']}_Joint", *JOINT_COL, 0.0, 0.95),
        make_mat(f"{cd['label']}_Brass", *cd['brass'], 0.65, 0.35),
        visor,
        lcd,
        light,
        armor,
        exoskel,
        biolum_c,
        biolum_v,
    ]

def spawn_combi_tier1(variant, y_offset, x_offset=0):
    cd = COMBI_TIER1_DEFS[variant]
    body_set = build_body()
    base_body = build_base_body_mat(body_set)
    combi_layers = build_combi_tier1(body_set)
    combi_grid = {}
    for mat, voxels in combi_layers.items():
        for p in voxels:
            combi_grid[p] = mat

    body_ob = spawn_body_with_fingers(
        base_body,
        f"{cd['label']}_BaseBody",
        make_base_body_materials(),
        loc=(x_offset, y_offset, 0),
        fingers=False,
    )
    combi_ob = spawn_mat_mesh(
        combi_grid,
        cd['label'],
        make_combi_tier1_materials(variant),
        loc=(x_offset, y_offset, 0),
        vs=VOXEL,
    )
    return body_ob, combi_ob

def spawn_equipment_loadout(loadout, label, y_offset, x_offset=0):
    body_set = build_body()
    base_body = build_base_body_mat(body_set)
    equipment_layers = assemble_equipment_loadout(loadout, body_set)
    material_variant = loadout.get('material_variant', 'default')
    equipment_grid = _layers_to_grid(equipment_layers)

    body_ob = spawn_body_with_fingers(
        base_body,
        f"{label}_BaseBody",
        make_base_body_materials(),
        loc=(x_offset, y_offset, 0),
        fingers=not loadout.get('gloves'),
    )
    equipment_ob = None
    if equipment_grid:
        equipment_ob = spawn_mat_mesh(
            equipment_grid,
            label,
            make_combi_tier1_materials(material_variant),
            loc=(x_offset, y_offset, 0),
            vs=VOXEL,
        )
    return body_ob, equipment_ob

def spawn_slot_preview(slot, module_id, y_offset, x_offset=0):
    module_def = EQUIPMENT_MODULES[slot][module_id]
    loadout = {
        slot: module_id,
        'material_variant': module_def.get('material_variant', 'default'),
    }
    return spawn_equipment_loadout(loadout, f"Preview_{module_def['label']}", y_offset, x_offset=x_offset)

def spawn_back_module_preview(module_id, y_offset, x_offset=0):
    module_def = BACK_MODULES[module_id]
    body_set = build_body()
    base_body = build_base_body_mat(body_set)
    body_ob = spawn_body_with_fingers(
        base_body,
        f"Preview_{module_def['label']}_BaseBody",
        make_base_body_materials(),
        loc=(x_offset, y_offset, 0),
        fingers=False,
    )
    build_fn = globals()[module_def['build_fn']]
    module_ob = spawn_mat_mesh(
        _layers_to_grid(build_fn()),
        f"Preview_{module_def['label']}",
        make_combi_tier1_materials(module_def.get('material_variant', 'default')),
        loc=(x_offset, y_offset, 0),
        vs=VOXEL,
    )
    return body_ob, module_ob

def spawn_catalog_item(module_def, y_offset, x_offset=0, category="Item"):
    build_fn = globals()[module_def['build_fn']]
    item_ob = spawn_mat_mesh(
        _layers_to_grid(build_fn()),
        f"{category}_{module_def['label']}",
        make_combi_tier1_materials(module_def.get('material_variant', 'default')),
        loc=(x_offset, y_offset, 72),
        vs=VOXEL,
    )
    return item_ob


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
    base_body = build_base_body_mat(body_set)

    # ── Base head for previews (skin face, hair/accessories over it) ──
    head_dressed = {k: v for k, v in base_body.items() if k[2] >= 74}
    head_mats = make_base_body_materials()

    # Shared materials
    mat_beard = make_mat("M_Beard", *HAIR_COL)
    mat_equip = make_mat("M_Equip", 0.85, 0.65, 0.10)
    mat_assembly_hair = make_mat("M_AssemblyHair", *HAIR_COL)
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
    cursor_y = 0

    face_builders = [
        face_01_stoic, face_02_worried, face_03_unhinged,
        face_04_hardened, face_05_visor, face_06_blank,
    ]
    beard_builders = [
        beard_01_stubble, beard_02_full, beard_03_mustache,
        beard_04_goatee, beard_05_mutton,
    ]
    hair_builders = [
        hair_01_bald, hair_02_buzzcut, hair_03_mohawk,
        hair_04_curly, hair_05_sidepart, hair_06_braids,
        hair_07_long, hair_08_ponytail, hair_09_dreads,
        hair_10_receding, hair_11_messy,
    ]

    def spawn_identity_accessories(label, y_offset, x_offset):
        face_dict, face_name = random.choice(face_builders)()
        beard_grid, beard_name = random.choice(beard_builders)()
        hair_grid, hair_name = random.choice(hair_builders)()
        spawn_mat_mesh(face_dict, f"{label}_{face_name}", face_plate_mats, loc=(x_offset + FACE_FORWARD_CM, y_offset, HEAD_Z), vs=FACE_VS)
        spawn_simple_mesh(beard_grid, f"{label}_{beard_name}", mat_beard, loc=(x_offset, y_offset, HEAD_Z), vs=VOXEL)
        spawn_simple_mesh(hair_grid, f"{label}_{hair_name}", mat_assembly_hair, loc=(x_offset, y_offset, HEAD_Z), vs=VOXEL)
        return face_name, beard_name, hair_name

    def spawn_civil_assembly(y_offset, x_offset):
        clothing_key = random.choice(CLOTHING_KEYS)
        clothing = build_clothing(clothing_key, body_set)
        visible_body = build_visible_body_under_clothing(body_set, clothing)
        spawn_body_with_fingers(
            visible_body,
            "Assembly_Civil_BaseBody",
            make_base_body_materials(),
            loc=(x_offset, y_offset, 0),
        )
        spawn_mat_mesh(
            clothing,
            f"Assembly_Civil_Clothing_{OUTFIT_DEFS[clothing_key]['label']}",
            make_clothing_materials(clothing_key),
            loc=(x_offset, y_offset, 0),
            vs=VOXEL,
        )
        face_name, beard_name, hair_name = spawn_identity_accessories("Assembly_Civil", y_offset, x_offset)
        return clothing_key, face_name, beard_name, hair_name

    def spawn_combi_assembly(y_offset, x_offset):
        spawn_combi_tier1('default', y_offset, x_offset=x_offset)
        face_name, beard_name, hair_name = spawn_identity_accessories("Assembly_Combi", y_offset, x_offset)
        return face_name, beard_name, hair_name

    def spawn_modular_assembly(loadout_name, loadout, y_offset, x_offset):
        label = f"Assembly_Modular_{loadout_name}"
        spawn_equipment_loadout(loadout, label, y_offset, x_offset=x_offset)
        face_name, beard_name, hair_name = spawn_identity_accessories(label, y_offset, x_offset)
        return face_name, beard_name, hair_name

    # ══════════════════════════════════════════════════
    # ROW 1: BASE BODY (no clothing)
    # ══════════════════════════════════════════════════
    print("\n  BASE BODY:")
    spawn_body_with_fingers(base_body, "BaseBody_Nude", make_base_body_materials(), loc=(0, cursor_y, 0))
    print(f"    BaseBody_Nude at Y={cursor_y}")
    cursor_y += CHARACTER_SPACING_CM

    # ══════════════════════════════════════════════════
    # ROW 1B: CLOTHING LIST (base body + clothing overlay)
    # ══════════════════════════════════════════════════
    print("\n  CLOTHING:")
    for clothing_key in CLOTHING_KEYS:
        od = OUTFIT_DEFS[clothing_key]
        spawn_clothing_preview(clothing_key, cursor_y)
        print(f"    Clothing_{od['label']} at Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM

    cursor_y += SECTION_GAP_CM

    # ══════════════════════════════════════════════════
    # ROW 1C: MODULAR EQUIPMENT + ITEM CATALOG (back axis)
    # ══════════════════════════════════════════════════
    showcase_start_y = cursor_y

    print("\n  HELMETS:")
    for module_id, module_def in HELMET_MODULES.items():
        spawn_slot_preview('helmet', module_id, cursor_y)
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  SUITS:")
    for module_id, module_def in SUIT_MODULES.items():
        spawn_slot_preview('suit', module_id, cursor_y)
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  GLOVES:")
    for module_id, module_def in GLOVE_MODULES.items():
        spawn_slot_preview('gloves', module_id, cursor_y)
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  BOOTS:")
    for module_id, module_def in BOOT_MODULES.items():
        spawn_slot_preview('boots', module_id, cursor_y)
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  BACK MODULES:")
    for module_id, module_def in BACK_MODULES.items():
        spawn_back_module_preview(module_id, cursor_y)
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += CHARACTER_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  HANDHELD TOOLS:")
    for module_id, module_def in HANDHELD_TOOLS.items():
        spawn_catalog_item(module_def, cursor_y, category="Tool")
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += ITEM_SPACING_CM
    cursor_y += SECTION_GAP_CM

    print("\n  WEAPONS:")
    for module_id, module_def in WEAPONS.items():
        spawn_catalog_item(module_def, cursor_y, category="Weapon")
        print(f"    {module_def['label']} at X=0 Y={cursor_y}")
        cursor_y += ITEM_SPACING_CM
    cursor_y += SECTION_GAP_CM

    # ══════════════════════════════════════════════════
    # ROW 1D: ASSEMBLED RANDOM PREVIEWS (advanced on X axis)
    # ══════════════════════════════════════════════════
    print("\n  ASSEMBLED PREVIEWS:")
    civil_key, civil_face, civil_beard, civil_hair = spawn_civil_assembly(showcase_start_y, ASSEMBLY_X_CM)
    print(
        f"    Assembly_Civil at X={ASSEMBLY_X_CM} Y={showcase_start_y} "
        f"({civil_key}, {civil_face}, {civil_beard}, {civil_hair})"
    )
    combi_y = showcase_start_y + CHARACTER_SPACING_CM
    combi_face, combi_beard, combi_hair = spawn_combi_assembly(combi_y, ASSEMBLY_X_CM)
    print(
        f"    Assembly_Combi at X={ASSEMBLY_X_CM} Y={combi_y} "
        f"({combi_face}, {combi_beard}, {combi_hair})"
    )
    modular_y = combi_y + CHARACTER_SPACING_CM
    showcase_name, showcase_loadout = next(
        (item for item in EQUIPMENT_LOADOUTS if item[0] == 'T2_Abyss'),
        EQUIPMENT_LOADOUTS[0],
    )
    modular_face, modular_beard, modular_hair = spawn_modular_assembly(
        showcase_name,
        showcase_loadout,
        modular_y,
        ASSEMBLY_X_CM,
    )
    print(
        f"    Assembly_Modular_{showcase_name} at X={ASSEMBLY_X_CM} Y={modular_y} "
        f"({modular_face}, {modular_beard}, {modular_hair})"
    )
    cursor_y = max(cursor_y, modular_y + CHARACTER_SPACING_CM) + SECTION_GAP_CM
    col = int(cursor_y / ACCESSORY_SPACING_CM)

    # ══════════════════════════════════════════════════
    # ROW 2: FACES (1cm voxels on head preview)
    # ══════════════════════════════════════════════════
    print("\n  FACES (0.5cm relief):")
    for builder in [face_01_stoic, face_02_worried, face_03_unhinged,
                    face_04_hardened, face_05_visor, face_06_blank]:
        acc_dict, name = builder()
        y_pos = col * ACCESSORY_SPACING_CM
        # Head preview (2cm) — shows hair/back of head
        spawn_mat_mesh(head_dressed, f"{name}_head", head_mats, loc=(0, y_pos, 0))
        # Face plate (0.5cm relief) — offset to head Z
        spawn_mat_mesh(acc_dict, name, face_plate_mats, loc=(FACE_FORWARD_CM, y_pos, HEAD_Z), vs=FACE_VS)
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
        y_pos = col * ACCESSORY_SPACING_CM
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
        y_pos = col * ACCESSORY_SPACING_CM
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
        y_pos = col * ACCESSORY_SPACING_CM
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

    print(f"\n  Total preview slots: {col}")
    print("=" * 60)
    print("""
  ╔══════════════════════════════════════════════════════╗
  ║           EXPORT GUIDE: Blender → UE5               ║
  ╚══════════════════════════════════════════════════════╝

  STRUCTURE DES FICHIERS A EXPORTER:
  ──────────────────────────────────
  SK_BaseBody_Nude.fbx       ← Corps nu commun
  SM_Clothing_*.fbx          ← Vetements modulaires
  SM_Combi_T1_Default.fbx    ← Combinaison Tier 1 normalisee
  SM_Face_01..05.fbx         ← Visages (overlay pieces)
  SM_Beard_01..05.fbx        ← Barbes
  SM_Equip_01..05.fbx        ← Equipements tete

  ETAPE 1 — EXPORT DEPUIS BLENDER:
  ─────────────────────────────────
  Pour le body et chaque vetement/combi/accessoire:
    1. Selectionner UNIQUEMENT l'objet (ex: BaseBody_Nude)
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
