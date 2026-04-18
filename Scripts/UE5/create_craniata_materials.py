"""
Sub3D - Create Craniata Gameplay Materials in UE5
=================================================

Creates a simple opaque PBR master material and the Material Instances used by
compose_craniata_bp_v2.py for the handmade Craniata submarine.

Design target:
- minimal
- clean
- readable in gameplay
- no texture dependency
- deterministic re-run in the editor

Assets created under:
    /Game/Sub3D/FirstPlayableRun/Materials/Craniata

Creates:
- M_Craniata_Master
- MI_Craniata_* instances expected by the compose script

Run in UE5 Editor Python console:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/create_craniata_materials.py").read())
"""

import unreal


MATERIAL_PATH = "/Game/Sub3D/FirstPlayableRun/Materials/Craniata"
MASTER_NAME = "M_Craniata_Master"

# Palette aligned with Scripts/Blender/hull_blockout_gpt/core/materials.py
MI_DEFS = [
    ("MI_Craniata_Hull",          (0.09, 0.11, 0.13), 0.42, 0.92),
    ("MI_Craniata_Deck",          (0.23, 0.24, 0.26), 0.70, 0.65),
    ("MI_Craniata_DeckUpper",     (0.25, 0.26, 0.28), 0.68, 0.60),
    ("MI_Craniata_Bulkhead",      (0.30, 0.32, 0.35), 0.78, 0.35),
    ("MI_Craniata_BulkheadLower", (0.25, 0.27, 0.30), 0.76, 0.40),
    ("MI_Craniata_Door",          (0.33, 0.34, 0.36), 0.50, 0.80),
    ("MI_Craniata_DoorFrame",     (0.12, 0.13, 0.14), 0.84, 0.55),
    ("MI_Craniata_Hatch",         (0.28, 0.29, 0.31), 0.60, 0.70),
    ("MI_Craniata_Fin",           (0.22, 0.24, 0.26), 0.54, 0.70),
    ("MI_Craniata_Propulsor",     (0.35, 0.32, 0.27), 0.34, 0.95),
    ("MI_Craniata_Duct",          (0.16, 0.17, 0.19), 0.45, 0.85),
    ("MI_Craniata_PipeWater",     (0.12, 0.26, 0.46), 0.70, 0.45),
    ("MI_Craniata_PipeHyd",       (0.58, 0.44, 0.10), 0.66, 0.42),
    ("MI_Craniata_PipeReactor",   (0.46, 0.15, 0.12), 0.66, 0.44),
    ("MI_Craniata_Pipe",          (0.15, 0.22, 0.31), 0.74, 0.45),
    ("MI_Craniata_Valve",         (0.40, 0.34, 0.12), 0.56, 0.55),
    ("MI_Craniata_Junction",      (0.18, 0.18, 0.18), 0.62, 0.55),
    ("MI_Craniata_Periscope",     (0.14, 0.15, 0.16), 0.38, 0.82),
    ("MI_Craniata_Antenna",       (0.22, 0.22, 0.24), 0.34, 0.80),
    ("MI_Craniata_Flag",          (0.72, 0.18, 0.12), 0.80, 0.05),
    ("MI_Craniata_Catwalk",       (0.17, 0.18, 0.19), 0.74, 0.72),
    ("MI_Craniata_Ladder",        (0.46, 0.38, 0.19), 0.70, 0.28),
    ("MI_Craniata_Turret",        (0.21, 0.22, 0.23), 0.58, 0.72),
    ("MI_Craniata_Mount",         (0.17, 0.18, 0.20), 0.56, 0.75),
    ("MI_Craniata_Engine",        (0.13, 0.14, 0.16), 0.66, 0.70),
    ("MI_Craniata_Reactor",       (0.10, 0.12, 0.15), 0.72, 0.60),
    ("MI_Craniata_Storage",       (0.24, 0.25, 0.27), 0.82, 0.30),
    ("MI_Craniata_Seal",          (0.06, 0.06, 0.07), 0.92, 0.00),
    ("MI_Craniata_UI",            (0.10, 0.22, 0.25), 0.25, 0.10),
    ("MI_Craniata_Default",       (0.24, 0.25, 0.27), 0.72, 0.40),
]


def log(msg: str) -> None:
    unreal.log(f"[CreateCraniataMaterials] {msg}")


def warn(msg: str) -> None:
    unreal.log_warning(f"[CreateCraniataMaterials] {msg}")


def err(msg: str) -> None:
    unreal.log_error(f"[CreateCraniataMaterials] {msg}")


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def asset_ref(path: str, name: str) -> str:
    return f"{path}/{name}"


def load_if_exists(path: str, name: str):
    full_path = asset_ref(path, name)
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        return unreal.EditorAssetLibrary.load_asset(full_path)
    return None


def connect_to_material_property(expression, output_name: str, material_property) -> None:
    mel = unreal.MaterialEditingLibrary
    if hasattr(mel, "connect_material_property"):
        mel.connect_material_property(expression, output_name, material_property)
        return
    if hasattr(mel, "connect_material_expression_to_property"):
        mel.connect_material_expression_to_property(expression, output_name, material_property)
        return
    raise RuntimeError("No compatible MaterialEditingLibrary property connection API found.")


def create_or_load_master_material():
    ensure_directory(MATERIAL_PATH)

    master = load_if_exists(MATERIAL_PATH, MASTER_NAME)
    if master:
        log(f"Reusing existing master: {MASTER_NAME}")
        return master

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    master = asset_tools.create_asset(MASTER_NAME, MATERIAL_PATH, unreal.Material, factory)
    if not master:
        raise RuntimeError(f"Failed to create {MASTER_NAME}")

    log(f"Created master: {MASTER_NAME}")
    return master


def build_master_graph(master) -> None:
    mel = unreal.MaterialEditingLibrary

    try:
        mel.delete_all_material_expressions(master)
    except Exception as exc:
        warn(f"Could not clear material graph for {master.get_name()}: {exc}")

    try:
        master.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    except Exception as exc:
        warn(f"Could not set blend_mode on {master.get_name()}: {exc}")

    try:
        master.set_editor_property("two_sided", False)
    except Exception as exc:
        warn(f"Could not set two_sided on {master.get_name()}: {exc}")

    base_color = mel.create_material_expression(master, unreal.MaterialExpressionVectorParameter, -500, -150)
    base_color.set_editor_property("parameter_name", "BaseColor")
    base_color.set_editor_property("default_value", unreal.LinearColor(0.24, 0.25, 0.27, 1.0))

    roughness = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -500, 40)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.72)

    metallic = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -500, 200)
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", 0.40)

    specular = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -500, 360)
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.50)

    connect_to_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    connect_to_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    connect_to_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    connect_to_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    try:
        mel.layout_material_expressions(master)
    except Exception as exc:
        warn(f"Could not layout expressions for {master.get_name()}: {exc}")

    try:
        mel.recompile_material(master)
    except Exception as exc:
        warn(f"Could not recompile {master.get_name()}: {exc}")

    unreal.EditorAssetLibrary.save_asset(asset_ref(MATERIAL_PATH, MASTER_NAME), only_if_is_dirty=False)


def set_mi_vector(mi, parameter_name: str, color: unreal.LinearColor) -> None:
    try:
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, parameter_name, color)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mi,
            parameter_name,
            color,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )


def set_mi_scalar(mi, parameter_name: str, value: float) -> None:
    try:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, parameter_name, value)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mi,
            parameter_name,
            value,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )


def create_or_update_material_instance(master, mi_name: str, rgb, roughness_value: float, metallic_value: float):
    mi = load_if_exists(MATERIAL_PATH, mi_name)
    if not mi:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mi = asset_tools.create_asset(mi_name, MATERIAL_PATH, unreal.MaterialInstanceConstant, factory)
        if not mi:
            raise RuntimeError(f"Failed to create {mi_name}")
        log(f"Created instance: {mi_name}")
    else:
        log(f"Reusing instance: {mi_name}")

    try:
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, master)
    except Exception:
        mi.set_editor_property("parent", master)

    color = unreal.LinearColor(float(rgb[0]), float(rgb[1]), float(rgb[2]), 1.0)
    set_mi_vector(mi, "BaseColor", color)
    set_mi_scalar(mi, "Roughness", float(roughness_value))
    set_mi_scalar(mi, "Metallic", float(metallic_value))
    set_mi_scalar(mi, "Specular", 0.50)

    unreal.EditorAssetLibrary.save_asset(asset_ref(MATERIAL_PATH, mi_name), only_if_is_dirty=False)


def main() -> None:
    log("=" * 60)
    log("Creating Craniata materials")
    log("=" * 60)

    master = create_or_load_master_material()
    build_master_graph(master)

    for mi_name, rgb, roughness_value, metallic_value in MI_DEFS:
        create_or_update_material_instance(master, mi_name, rgb, roughness_value, metallic_value)

    log("")
    log(f"Master: {asset_ref(MATERIAL_PATH, MASTER_NAME)}")
    log(f"Instances created/updated: {len(MI_DEFS)}")
    log("Compose script can now apply MI_Craniata_* overrides.")
    log("=" * 60)


main()
