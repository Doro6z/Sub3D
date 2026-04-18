"""
Sub3D — Apply Materials to Rigged Crew Mesh
=============================================
NON-DESTRUCTIVE: does NOT recreate geometry or armature.
Adds 6 materials to the existing SM_Crew mesh and assigns
faces by body zone based on vertex position.

Run AFTER rigging. Safe to re-run multiple times.

Blender > Scripting > Open > Alt+P
"""

import bpy

# ═══════════════════════════════════════════════════════════
# MATERIAL DEFINITIONS
# ═══════════════════════════════════════════════════════════

MAT_DEFS = [
    # (name,        R,    G,    B,   metallic, roughness)
    ("M_Skin",     0.72, 0.55, 0.42, 0.0, 0.85),   # 0
    ("M_Suit",     0.15, 0.28, 0.22, 0.0, 0.88),   # 1
    ("M_SuitDk",   0.07, 0.16, 0.12, 0.0, 0.90),   # 2
    ("M_Boot",     0.08, 0.07, 0.06, 0.0, 0.92),   # 3
    ("M_Hair",     0.20, 0.12, 0.06, 0.0, 0.95),   # 4
    ("M_Accent",   0.10, 0.10, 0.10, 0.0, 0.88),   # 5
]

SKIN    = 0
SUIT    = 1
SUIT_DK = 2
BOOT    = 3
HAIR    = 4
ACCENT  = 5


# ═══════════════════════════════════════════════════════════
# ZONE CLASSIFICATION
# ═══════════════════════════════════════════════════════════

def classify_face(center_x, center_y, center_z):
    """Determine material index from face center position (cm).
    Body is at 2cm voxels, T-pose arms at z~136."""

    abs_y = abs(center_y)
    is_arm = abs_y >= 22 and 128 <= center_z <= 144

    # ── HEAD (z >= 156cm, i.e. voxel z=78+) ──
    if center_z >= 156:
        return SKIN  # Bald base head, hair is accessory

    # ── NECK (148–156cm) ──
    if center_z >= 148:
        return SKIN

    # ── HANDS (at arm ends, abs_y >= 66) ──
    if is_arm and abs_y >= 64:
        return SKIN

    # ── FOREARMS (abs_y 44–64, exposed skin) ──
    if is_arm and abs_y >= 44:
        return SKIN

    # ── UPPER ARMS ──
    if is_arm and abs_y >= 22:
        # Sleeve cuffs
        if 42 <= abs_y <= 46:
            return SUIT_DK
        return SUIT

    # ── CLAVICLE AREA ──
    if center_z >= 140 and abs_y >= 8:
        return SUIT

    # ── BOOTS (z <= 10cm) ──
    if center_z <= 10:
        if center_z <= 4:
            return SUIT_DK  # Sole
        return BOOT

    # ── BELT (z 96–102cm, voxel z=48-51) ──
    if 96 <= center_z <= 102:
        if abs_y <= 4 and center_x >= 4:
            return ACCENT   # Belt buckle
        return SUIT_DK      # Belt

    # ── LEGS (z 10–88cm) ──
    if center_z <= 88:
        # Cargo pockets
        if 56 <= center_z <= 66 and 4 <= abs_y <= 10 and center_x >= 2:
            return SUIT_DK
        return SUIT

    # ── TORSO (z 88–148cm) ──
    # Chest pockets
    if 116 <= center_z <= 124 and 6 <= abs_y <= 14 and center_x >= 4:
        return SUIT_DK
    # Collar
    if center_z >= 142 and abs_y <= 8 and center_x >= 0:
        return SUIT_DK
    # Center seam
    if abs(center_y) <= 2 and center_x >= 6 and 104 <= center_z <= 134:
        return SUIT_DK

    return SUIT


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Apply materials to SM_Crew")
    print("=" * 60)

    # Find the mesh
    ob = bpy.data.objects.get("SM_Crew")
    if not ob:
        print("  ERROR: Object 'SM_Crew' not found!")
        print("  Make sure your mesh is named SM_Crew.")
        return

    me = ob.data

    # Ensure object mode
    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Clear existing materials
    me.materials.clear()

    # Create and assign materials
    print("\n  Materials:")
    for name, r, g, b, met, rough in MAT_DEFS:
        m = bpy.data.materials.get(name)
        if not m:
            m = bpy.data.materials.new(name)
            m.use_nodes = True
            bsdf = m.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
                bsdf.inputs["Metallic"].default_value = met
                bsdf.inputs["Roughness"].default_value = rough
        me.materials.append(m)
        idx = len(me.materials) - 1
        print(f"    [{idx}] {name}")

    # Assign faces by zone
    print("\n  Assigning faces by zone...")
    counts = {i: 0 for i in range(len(MAT_DEFS))}

    for poly in me.polygons:
        cx, cy, cz = poly.center
        mat_idx = classify_face(cx, cy, cz)
        poly.material_index = mat_idx
        counts[mat_idx] += 1

    print("\n  Face distribution:")
    for idx, count in counts.items():
        print(f"    {MAT_DEFS[idx][0]:12s}: {count} faces")

    print(f"\n  Total: {len(me.polygons)} faces assigned")

    # Switch viewport to Material Preview
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'MATERIAL'
            break

    print("\n" + "=" * 60)
    print("Done. 6 materials applied. Armature + weights untouched.")
    print("=" * 60)


main()
