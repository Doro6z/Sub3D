"""
Sub3D — Assign Material Instances to SK_Crew_Basic slots
=========================================================
Run in UE5 Python console:
exec(open(r"C:/Dev/Sub3D/Scripts/UE5/assign_crew_materials.py").read())
"""

import unreal

MESH_PATH = "/Game/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic"
MAT_PATH = "/Game/Sub3D/Characters/Materials"

# Slot index → Material Instance name
SLOT_MAP = [
    "MI_Crew_Skin",      # 0
    "MI_Crew_Suit",      # 1
    "MI_Crew_SuitDk",    # 2
    "MI_Crew_Boot",      # 3
    "MI_Crew_Accent",    # 4
]

def main():
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not mesh:
        unreal.log_error(f"SK_Crew_Basic not found at {MESH_PATH}")
        return

    materials = mesh.get_editor_property("materials")
    unreal.log(f"SK_Crew_Basic has {len(materials)} material slots")

    for i, mi_name in enumerate(SLOT_MAP):
        if i >= len(materials):
            unreal.log_error(f"  Slot {i} does not exist (only {len(materials)} slots)")
            break

        mi = unreal.EditorAssetLibrary.load_asset(f"{MAT_PATH}/{mi_name}")
        if not mi:
            unreal.log_error(f"  {mi_name} not found!")
            continue

        # Get the slot, set its material
        slot = materials[i]
        slot.set_editor_property("material_interface", mi)
        unreal.log(f"  [{i}] {mi_name} assigned")

    # Apply changes
    mesh.set_editor_property("materials", materials)
    unreal.EditorAssetLibrary.save_asset(MESH_PATH)
    unreal.log("Materials assigned and saved.")


main()
