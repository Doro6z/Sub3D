"""
Sub3D - create blank water material v2 assets
=============================================

Follows:
    reports/plans/2026-05-08_water_material_v2_build_plan.md

This script creates the editor assets needed for the corrected SLW gate and for
the v2 material pass. It intentionally does NOT configure the material graphs,
material settings, or BP_Submarine_Craniata. The plan requires a manual SLW gate
before the final material instance becomes DefaultWaterMaterial.

Run inside the UE editor Python console:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/create_water_material_v2_assets.py").read())

Created assets:
    /Game/Sub3D/Material/_Spike/M_Spike_CompartmentWater_SLW
    /Game/Sub3D/Material/Functions/MF_WaterCompartmentUVs
    /Game/Sub3D/Material/M_CompartmentWater_v2
    /Game/Sub3D/Material/MI_CompartmentWater_v2

The script creates only blank uassets. Graph nodes, Single Layer Water settings,
and parameter names are wired manually in the Material Editor for the active
UE 5.7 build.
"""

import unreal


MATERIAL_DIR = "/Game/Sub3D/Material"
SPIKE_DIR = "/Game/Sub3D/Material/_Spike"
FUNCTIONS_DIR = "/Game/Sub3D/Material/Functions"

SPIKE_NAME = "M_Spike_CompartmentWater_SLW"
MASTER_NAME = "M_CompartmentWater_v2"
INSTANCE_NAME = "MI_CompartmentWater_v2"

MF_NAMES = [
    "MF_WaterCompartmentUVs",
]

def log(message):
    unreal.log("[water_v2] " + message)


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        log("Created directory " + path)


def object_path(directory, name):
    return "{0}/{1}.{1}".format(directory, name)


def load_asset(directory, name):
    path = object_path(directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return None


def save_asset(directory, name):
    unreal.EditorAssetLibrary.save_asset(object_path(directory, name), only_if_is_dirty=False)


def create_material(directory, name):
    existing = load_asset(directory, name)
    if existing:
        log("Reusing material " + object_path(directory, name))
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Failed to create material " + object_path(directory, name))

    log("Created material " + object_path(directory, name))
    save_asset(directory, name)
    return material


def create_material_function(name):
    existing = load_asset(FUNCTIONS_DIR, name)
    if existing:
        log("Reusing material function " + object_path(FUNCTIONS_DIR, name))
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    function = asset_tools.create_asset(
        name,
        FUNCTIONS_DIR,
        unreal.MaterialFunction,
        unreal.MaterialFunctionFactoryNew(),
    )
    if not function:
        raise RuntimeError("Failed to create material function " + object_path(FUNCTIONS_DIR, name))

    log("Created material function " + object_path(FUNCTIONS_DIR, name))
    save_asset(FUNCTIONS_DIR, name)
    return function


def set_material_instance_parent(instance, parent):
    try:
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        return
    except Exception:
        pass

    instance.set_editor_property("parent", parent)


def create_material_instance(master):
    existing = load_asset(MATERIAL_DIR, INSTANCE_NAME)
    if existing:
        log("Reusing material instance " + object_path(MATERIAL_DIR, INSTANCE_NAME))
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = asset_tools.create_asset(INSTANCE_NAME, MATERIAL_DIR, unreal.MaterialInstanceConstant, factory)
    if not instance:
        raise RuntimeError("Failed to create material instance " + object_path(MATERIAL_DIR, INSTANCE_NAME))

    set_material_instance_parent(instance, master)
    log("Created material instance " + object_path(MATERIAL_DIR, INSTANCE_NAME))
    save_asset(MATERIAL_DIR, INSTANCE_NAME)
    return instance


def main():
    log("Creating water material v2 assets from corrected plan.")

    ensure_directory(MATERIAL_DIR)
    ensure_directory(SPIKE_DIR)
    ensure_directory(FUNCTIONS_DIR)

    create_material(SPIKE_DIR, SPIKE_NAME)

    for mf_name in MF_NAMES:
        create_material_function(mf_name)

    master = create_material(MATERIAL_DIR, MASTER_NAME)

    create_material_instance(master)

    log("Done. Next step: manually wire the spike graph and run the SLW gate before assigning MI_CompartmentWater_v2.")
    log("Spike:    " + object_path(SPIKE_DIR, SPIKE_NAME))
    log("Master:   " + object_path(MATERIAL_DIR, MASTER_NAME))
    log("Instance: " + object_path(MATERIAL_DIR, INSTANCE_NAME))


main()
