"""
Sub3D — Create Crew Character Materials in UE5
================================================
Run in UE5: Output Log > Python console, or
Edit > Editor Preferences > Python > Execute Script

Creates:
  - M_Crew_Master (Material with BaseColor/Roughness/Metallic params)
  - 5 Material Instances (Skin, Suit, SuitDk, Boot, Accent)
"""

import unreal

MATERIAL_PATH = "/Game/Sub3D/Characters/Materials"

# Material Instance definitions: (name, R, G, B, roughness)
MI_DEFS = [
    ("MI_Crew_Skin",    0.72, 0.55, 0.42, 0.85),
    ("MI_Crew_Suit",    0.15, 0.28, 0.22, 0.88),
    ("MI_Crew_SuitDk",  0.07, 0.16, 0.12, 0.90),
    ("MI_Crew_Boot",    0.08, 0.07, 0.06, 0.92),
    ("MI_Crew_Accent",  0.10, 0.10, 0.10, 0.88),
]


def create_master_material():
    """Create M_Crew_Master with 3 parameters."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    # Create material
    mat_factory = unreal.MaterialFactoryNew()
    master = asset_tools.create_asset(
        "M_Crew_Master", MATERIAL_PATH,
        unreal.Material, mat_factory)

    if not master:
        unreal.log_error("Failed to create M_Crew_Master")
        return None

    # BaseColor parameter
    base_color = mel.create_material_expression(
        master, unreal.MaterialExpressionVectorParameter, -400, 0)
    base_color.set_editor_property("parameter_name", "BaseColor")
    base_color.set_editor_property("default_value",
        unreal.LinearColor(0.5, 0.5, 0.5, 1.0))

    # Roughness parameter
    roughness = mel.create_material_expression(
        master, unreal.MaterialExpressionScalarParameter, -400, 200)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.85)

    # Metallic parameter
    metallic = mel.create_material_expression(
        master, unreal.MaterialExpressionScalarParameter, -400, 350)
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", 0.0)

    # Connect to material outputs
    mel.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    # Compile
    mel.recompile_material(master)
    unreal.EditorAssetLibrary.save_asset(f"{MATERIAL_PATH}/M_Crew_Master")

    unreal.log(f"Created M_Crew_Master at {MATERIAL_PATH}")
    return master


def create_material_instances(master):
    """Create 5 Material Instances from master."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    for name, r, g, b, rough in MI_DEFS:
        # Create via AssetTools with factory
        mi_factory = unreal.MaterialInstanceConstantFactoryNew()

        mi = asset_tools.create_asset(
            name, MATERIAL_PATH,
            unreal.MaterialInstanceConstant, mi_factory)

        if not mi:
            unreal.log_error(f"Failed to create {name}")
            continue

        # Set parent material
        mi.set_editor_property("parent", master)

        # Set parameters
        mel.set_material_instance_vector_parameter_value(
            mi, "BaseColor", unreal.LinearColor(r, g, b, 1.0))
        mel.set_material_instance_scalar_parameter_value(
            mi, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(
            mi, "Metallic", 0.0)

        unreal.EditorAssetLibrary.save_asset(f"{MATERIAL_PATH}/{name}")
        unreal.log(f"  Created {name} ({r:.2f}, {g:.2f}, {b:.2f})")


def main():
    unreal.log("=" * 50)
    unreal.log("Sub3D — Creating Crew Materials")
    unreal.log("=" * 50)

    # Check if master already exists
    master = unreal.EditorAssetLibrary.load_asset(f"{MATERIAL_PATH}/M_Crew_Master")
    if not master:
        master = create_master_material()
    else:
        unreal.log("M_Crew_Master already exists, skipping creation.")

    if master:
        create_material_instances(master)

    unreal.log("")
    unreal.log("Done. Assign to SK_Crew_Basic material slots:")
    unreal.log("  [0] MI_Crew_Skin")
    unreal.log("  [1] MI_Crew_Suit")
    unreal.log("  [2] MI_Crew_SuitDk")
    unreal.log("  [3] MI_Crew_Boot")
    unreal.log("  [4] MI_Crew_Accent")
    unreal.log("=" * 50)


main()
