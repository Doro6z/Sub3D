"""
Sub3D - Create empty Blueprint child of UCrewUnderwaterPPComponent
==================================================================
Run in UE5:
  Tools > Execute Python Script... > Scripts/UE5/create_bp_underwater_pp.py
  or paste the path in the Output Log Python console.

Creates a single empty Blueprint Class (BPC) under
  /Game/Sub3D/Blueprint/PlayerBP/BPC_CrewUnderwaterPP

Parent class: UCrewUnderwaterPPComponent (Source/Sub3D/Submarine/CrewUnderwaterPPComponent.h).

Intent
------
Today the underwater PP material slot is set directly on BP_SubmarineCrew
(the C++ component is added there and its UnderwaterPostProcessMaterial
property is exposed on the actor). This BPC gives us a dedicated, reusable
component asset where the designer can author the underwater PP defaults
(material, blend speeds, waterline threshold) once, then BP_SubmarineCrew
just attaches BPC_CrewUnderwaterPP as a component.

Per request: NO defaults are applied here. Asset ships as a bare child.
Tuning is a separate step.

Idempotent: if the asset already exists, the script logs and exits without
overwriting.
"""

import unreal

BP_PATH = "/Game/Sub3D/Blueprint/PlayerBP"
BP_NAME = "BPC_CrewUnderwaterPP"
PARENT_CLASS_PATH = "/Script/Sub3D.CrewUnderwaterPPComponent"


def main():
    unreal.log("=" * 64)
    unreal.log(f"Sub3D - Create {BP_NAME}")
    unreal.log("=" * 64)

    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if not parent_class:
        unreal.log_error(
            f"Could not load parent class {PARENT_CLASS_PATH}. "
            "Make sure the Sub3D module is compiled and loaded."
        )
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(BP_PATH):
        unreal.EditorAssetLibrary.make_directory(BP_PATH)

    asset_path = f"{BP_PATH}/{BP_NAME}"

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log_warning(
            f"Asset already exists at {asset_path} - leaving untouched."
        )
        return

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    bp = asset_tools.create_asset(BP_NAME, BP_PATH, None, factory)
    if not bp:
        unreal.log_error(f"Failed to create {asset_path}")
        return

    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"Created {asset_path}")
    unreal.log(f"  Parent: {PARENT_CLASS_PATH}")
    unreal.log("  No defaults applied (per request).")
    unreal.log("=" * 64)


main()
