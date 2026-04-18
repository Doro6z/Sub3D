"""
Sub3D - Create Lighting and VFX Assets for Craniata
===================================================

Creates the first-pass lighting support assets described in:
    reports/plans/2026-04-17_submarine_lighting_and_atmosphere_implementation_plan.md

This script does the durable part only:
- imports generated textures from Saved/GenImages
- fixes texture settings for masks and decals
- creates real Light Function materials and material instances
- creates a real deferred decal master and decal material instances

It does not pretend to create production-ready Niagara systems automatically.
Niagara systems still need to be created manually from emitter templates in the
editor so the result is valid and reviewable.

Run inside Unreal Editor Python:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/create_lighting_and_vfx.py").read())
"""

import os
import unreal


REPO_ROOT = r"C:\Dev\Sub3D"
GEN_DIR = os.path.join(REPO_ROOT, "Saved", "GenImages")

VFX_BASE = "/Game/Sub3D/VFX/Lighting"
TEXTURE_PATH = f"{VFX_BASE}/Textures"
LIGHT_FUNCTION_PATH = f"{VFX_BASE}/LightFunctions"
NIAGARA_PATH = f"{VFX_BASE}/Niagara"

DECAL_BASE = "/Game/Sub3D/Materials/Decals"
DECAL_TEXTURE_PATH = f"{DECAL_BASE}/Textures"
DECAL_SUB_PATH = f"{DECAL_BASE}/Submarine"


TEXTURE_SPECS = [
    {
        "filename": "t_lf_gridbars.tga",
        "asset_path": f"{TEXTURE_PATH}/T_LF_GridBars_Mask",
        "mask": True,
        "no_mips": True,
    },
    {
        "filename": "t_lf_alarmsweep.tga",
        "asset_path": f"{TEXTURE_PATH}/T_LF_AlarmSweep_Mask",
        "mask": True,
        "no_mips": True,
    },
    {
        "filename": "t_vfx_steam.png",
        "asset_path": f"{TEXTURE_PATH}/T_VFX_SteamAlpha",
        "mask": True,
        "no_mips": False,
    },
    {
        "filename": "t_vfx_dust.png",
        "asset_path": f"{TEXTURE_PATH}/T_VFX_DustNoise",
        "mask": True,
        "no_mips": False,
    },
    {
        "filename": "t_decal_rust.png",
        "asset_path": f"{DECAL_TEXTURE_PATH}/T_Decal_Rust_D",
        "mask": False,
        "no_mips": False,
    },
    {
        "filename": "t_decal_leak.png",
        "asset_path": f"{DECAL_TEXTURE_PATH}/T_Decal_Leak_D",
        "mask": False,
        "no_mips": False,
    },
    {
        "filename": "t_decal_warning.png",
        "asset_path": f"{DECAL_TEXTURE_PATH}/T_Decal_Warning_D",
        "mask": False,
        "no_mips": False,
    },
]


def log(message: str) -> None:
    unreal.log(f"[CreateLightingAndVFX] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[CreateLightingAndVFX] {message}")


def error(message: str) -> None:
    unreal.log_error(f"[CreateLightingAndVFX] {message}")


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def asset_exists(asset_path: str) -> bool:
    return unreal.EditorAssetLibrary.does_asset_exist(asset_path)


def load_asset(asset_path: str):
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def package_path(asset_path: str) -> str:
    return asset_path.rsplit("/", 1)[0]


def asset_name(asset_path: str) -> str:
    return asset_path.rsplit("/", 1)[-1]


def save_asset(asset) -> None:
    if asset:
        unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False)


def connect_property(expression, output_name: str, material_property) -> None:
    mel = unreal.MaterialEditingLibrary
    if hasattr(mel, "connect_material_property"):
        mel.connect_material_property(expression, output_name, material_property)
        return
    mel.connect_material_expression_to_property(expression, output_name, material_property)


def create_expression(material, expression_class, x_pos: int, y_pos: int):
    expr = unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x_pos, y_pos)
    if not expr:
        raise RuntimeError(f"Failed to create {expression_class.__name__} for {material.get_name()}")
    return expr


def safe_set(obj, property_name: str, value) -> bool:
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        warn(f"{obj.get_name()}.{property_name} unsupported: {exc}")
        return False


def set_texture_import_settings(texture, mask: bool, no_mips: bool) -> None:
    if not texture:
        return

    safe_set(texture, "srgb", not mask)

    try:
        compression = unreal.TextureCompressionSettings.TC_MASKS if mask else unreal.TextureCompressionSettings.TC_DEFAULT
        safe_set(texture, "compression_settings", compression)
    except Exception as exc:
        warn(f"Could not set compression for {texture.get_name()}: {exc}")

    try:
        mip_setting = unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS if no_mips else unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP
        safe_set(texture, "mip_gen_settings", mip_setting)
    except Exception as exc:
        warn(f"Could not set mip settings for {texture.get_name()}: {exc}")

    try:
        texture.post_edit_change()
    except Exception:
        pass

    save_asset(texture)


def import_texture(spec: dict):
    source_file = os.path.join(GEN_DIR, spec["filename"])
    if not os.path.isfile(source_file):
        warn(f"Missing source texture: {source_file}")
        return None

    dest_asset_path = spec["asset_path"]
    ensure_directory(package_path(dest_asset_path))

    task = unreal.AssetImportTask()
    task.filename = source_file
    task.destination_path = package_path(dest_asset_path)
    task.destination_name = asset_name(dest_asset_path)
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = load_asset(dest_asset_path)
    set_texture_import_settings(imported, spec["mask"], spec["no_mips"])
    log(f"Imported texture: {dest_asset_path}")
    return imported


def load_or_create_material(asset_path: str):
    ensure_directory(package_path(asset_path))
    if asset_exists(asset_path):
        return load_asset(asset_path)

    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name(asset_path),
        package_path(asset_path),
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not asset:
        raise RuntimeError(f"Failed to create material {asset_path}")
    return asset


def load_or_create_mi(asset_path: str, parent):
    ensure_directory(package_path(asset_path))
    if asset_exists(asset_path):
        mi = load_asset(asset_path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name(asset_path),
            package_path(asset_path),
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )
        if not mi:
            raise RuntimeError(f"Failed to create material instance {asset_path}")

    try:
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
    except Exception:
        safe_set(mi, "parent", parent)

    save_asset(mi)
    return mi


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


def set_mi_texture(mi, parameter_name: str, texture) -> None:
    if not texture:
        return
    try:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, parameter_name, texture)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            mi,
            parameter_name,
            texture,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )


def clear_material_graph(material) -> None:
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    except Exception as exc:
        warn(f"Could not clear graph for {material.get_name()}: {exc}")


def finalize_material(material) -> None:
    try:
        unreal.MaterialEditingLibrary.layout_material_expressions(material)
    except Exception as exc:
        warn(f"Could not layout {material.get_name()}: {exc}")

    try:
        unreal.MaterialEditingLibrary.recompile_material(material)
    except Exception as exc:
        warn(f"Could not recompile {material.get_name()}: {exc}")

    save_asset(material)


def create_scalar_parameter(material, name: str, default_value: float, x_pos: int, y_pos: int):
    node = create_expression(material, unreal.MaterialExpressionScalarParameter, x_pos, y_pos)
    safe_set(node, "parameter_name", name)
    safe_set(node, "default_value", default_value)
    return node


def create_vector_parameter(material, name: str, default_value: unreal.LinearColor, x_pos: int, y_pos: int):
    node = create_expression(material, unreal.MaterialExpressionVectorParameter, x_pos, y_pos)
    safe_set(node, "parameter_name", name)
    safe_set(node, "default_value", default_value)
    return node


def create_texture_parameter(material, name: str, texture, x_pos: int, y_pos: int):
    node = create_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x_pos, y_pos)
    safe_set(node, "parameter_name", name)
    if texture:
        safe_set(node, "texture", texture)
    return node


def create_texcoord(material, x_pos: int, y_pos: int):
    return create_expression(material, unreal.MaterialExpressionTextureCoordinate, x_pos, y_pos)


def create_append(material, x_pos: int, y_pos: int):
    return create_expression(material, unreal.MaterialExpressionAppendVector, x_pos, y_pos)


def build_light_function_grid(material, mask_texture) -> None:
    clear_material_graph(material)
    safe_set(material, "material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    safe_set(material, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    texcoord = create_texcoord(material, -900, -100)
    tiling = create_scalar_parameter(material, "Tiling", 1.0, -900, 80)
    append = create_append(material, -700, -10)
    power = create_expression(material, unreal.MaterialExpressionPower, -280, -10)
    brightness = create_scalar_parameter(material, "Brightness", 1.0, -280, 170)
    multiply = create_expression(material, unreal.MaterialExpressionMultiply, -30, 10)
    texture_sample = create_texture_parameter(material, "MaskTexture", mask_texture, -500, -40)

    unreal.MaterialEditingLibrary.connect_material_expressions(tiling, "", append, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tiling, "", append, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", texture_sample, "Coordinates")
    unreal.MaterialEditingLibrary.connect_material_expressions(append, "", texture_sample, "Coordinates")
    unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "", power, "Base")

    contrast = create_scalar_parameter(material, "Contrast", 1.35, -500, 170)
    unreal.MaterialEditingLibrary.connect_material_expressions(contrast, "", power, "Exp")
    unreal.MaterialEditingLibrary.connect_material_expressions(power, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(brightness, "", multiply, "B")
    connect_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finalize_material(material)


def build_light_function_dirty_flicker(material, noise_texture) -> None:
    clear_material_graph(material)
    safe_set(material, "material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    safe_set(material, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    texcoord = create_texcoord(material, -1150, -50)
    noise_tiling = create_scalar_parameter(material, "NoiseTiling", 1.0, -1150, 130)
    tiling_uv = create_append(material, -930, 20)
    noise_sample = create_texture_parameter(material, "NoiseTexture", noise_texture, -690, 0)

    time_node = create_expression(material, unreal.MaterialExpressionTime, -1150, 350)
    flicker_speed = create_scalar_parameter(material, "FlickerSpeed", 7.0, -1150, 500)
    time_mul = create_expression(material, unreal.MaterialExpressionMultiply, -920, 420)
    sine_node = create_expression(material, unreal.MaterialExpressionSine, -700, 420)
    half_scale = create_scalar_parameter(material, "WaveHalfScale", 0.5, -520, 500)
    half_bias = create_scalar_parameter(material, "WaveBias", 0.5, -520, 620)
    wave_mul = create_expression(material, unreal.MaterialExpressionMultiply, -470, 420)
    wave_add = create_expression(material, unreal.MaterialExpressionAdd, -250, 420)

    min_brightness = create_scalar_parameter(material, "MinBrightness", 0.45, -250, 620)
    max_brightness = create_scalar_parameter(material, "MaxBrightness", 1.0, -250, 760)
    lerp_node = create_expression(material, unreal.MaterialExpressionLinearInterpolate, -20, 560)

    intensity_mul = create_expression(material, unreal.MaterialExpressionMultiply, 220, 240)
    brightness_mul = create_expression(material, unreal.MaterialExpressionMultiply, 440, 280)
    output_scale = create_scalar_parameter(material, "Brightness", 1.0, 220, 430)

    unreal.MaterialEditingLibrary.connect_material_expressions(noise_tiling, "", tiling_uv, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(noise_tiling, "", tiling_uv, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", noise_sample, "Coordinates")
    unreal.MaterialEditingLibrary.connect_material_expressions(tiling_uv, "", noise_sample, "Coordinates")

    unreal.MaterialEditingLibrary.connect_material_expressions(time_node, "", time_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(flicker_speed, "", time_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(time_mul, "", sine_node, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(sine_node, "", wave_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(half_scale, "", wave_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_mul, "", wave_add, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(half_bias, "", wave_add, "B")

    unreal.MaterialEditingLibrary.connect_material_expressions(min_brightness, "", lerp_node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(max_brightness, "", lerp_node, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(wave_add, "", lerp_node, "Alpha")

    unreal.MaterialEditingLibrary.connect_material_expressions(noise_sample, "", intensity_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(lerp_node, "", intensity_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(intensity_mul, "", brightness_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(output_scale, "", brightness_mul, "B")

    connect_property(brightness_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finalize_material(material)


def build_light_function_alarm_sweep(material, mask_texture) -> None:
    clear_material_graph(material)
    safe_set(material, "material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    safe_set(material, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    texcoord = create_texcoord(material, -980, -50)
    panner = create_expression(material, unreal.MaterialExpressionPanner, -730, -20)
    speed_x = create_scalar_parameter(material, "SweepSpeedX", 0.2, -980, 140)
    speed_y = create_scalar_parameter(material, "SweepSpeedY", 0.0, -980, 260)
    texture_sample = create_texture_parameter(material, "MaskTexture", mask_texture, -470, -10)
    brightness = create_scalar_parameter(material, "Brightness", 1.0, -470, 180)
    contrast = create_scalar_parameter(material, "Contrast", 1.0, -470, 320)
    power = create_expression(material, unreal.MaterialExpressionPower, -210, 30)
    multiply = create_expression(material, unreal.MaterialExpressionMultiply, 20, 50)

    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", panner, "Coordinate")
    unreal.MaterialEditingLibrary.connect_material_expressions(speed_x, "", panner, "SpeedX")
    unreal.MaterialEditingLibrary.connect_material_expressions(speed_y, "", panner, "SpeedY")
    unreal.MaterialEditingLibrary.connect_material_expressions(panner, "", texture_sample, "Coordinates")
    unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "", power, "Base")
    unreal.MaterialEditingLibrary.connect_material_expressions(contrast, "", power, "Exp")
    unreal.MaterialEditingLibrary.connect_material_expressions(power, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(brightness, "", multiply, "B")

    connect_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finalize_material(material)


def build_decal_master(material, default_texture) -> None:
    clear_material_graph(material)
    safe_set(material, "material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    safe_set(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    safe_set(material, "two_sided", False)

    texture_sample = create_texture_parameter(material, "DecalTexture", default_texture, -720, -80)
    tint = create_vector_parameter(material, "TintColor", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -720, 180)
    opacity = create_scalar_parameter(material, "Opacity", 0.75, -720, 340)
    roughness = create_scalar_parameter(material, "Roughness", 0.75, -720, 500)
    alpha_bias = create_scalar_parameter(material, "MaskBias", 0.0, -720, 660)
    alpha_scale = create_scalar_parameter(material, "MaskScale", 1.0, -720, 820)

    color_mul = create_expression(material, unreal.MaterialExpressionMultiply, -420, 40)
    mask_mul = create_expression(material, unreal.MaterialExpressionMultiply, -420, 420)
    mask_add = create_expression(material, unreal.MaterialExpressionAdd, -180, 420)
    clamp_min = create_scalar_parameter(material, "ClampMin", 0.0, -180, 600)
    clamp_max = create_scalar_parameter(material, "ClampMax", 1.0, -180, 760)
    max_node = create_expression(material, unreal.MaterialExpressionMax, 70, 360)
    min_node = create_expression(material, unreal.MaterialExpressionMin, 290, 360)

    unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "RGB", color_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", color_mul, "B")
    connect_property(color_mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "A", mask_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha_scale, "", mask_mul, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_mul, "", mask_add, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha_bias, "", mask_add, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_add, "", max_node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(clamp_min, "", max_node, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(max_node, "", min_node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(clamp_max, "", min_node, "B")

    opacity_mul = create_expression(material, unreal.MaterialExpressionMultiply, 310, 380)
    unreal.MaterialEditingLibrary.connect_material_expressions(min_node, "", opacity_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", opacity_mul, "B")
    connect_property(opacity_mul, "", unreal.MaterialProperty.MP_OPACITY)
    connect_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    finalize_material(material)


def create_or_update_light_function_assets(textures: dict) -> None:
    ensure_directory(LIGHT_FUNCTION_PATH)

    m_grid = load_or_create_material(f"{LIGHT_FUNCTION_PATH}/M_LF_Sub_GridBars")
    build_light_function_grid(m_grid, textures.get("t_lf_gridbars.tga"))
    mi_grid = load_or_create_mi(f"{LIGHT_FUNCTION_PATH}/MI_LF_Sub_GridBars_Soft", m_grid)
    set_mi_scalar(mi_grid, "Tiling", 1.0)
    set_mi_scalar(mi_grid, "Contrast", 1.15)
    set_mi_scalar(mi_grid, "Brightness", 0.9)
    save_asset(mi_grid)

    m_flicker = load_or_create_material(f"{LIGHT_FUNCTION_PATH}/M_LF_Sub_DirtyFlicker")
    build_light_function_dirty_flicker(m_flicker, textures.get("t_vfx_dust.png"))
    mi_flicker_warm = load_or_create_mi(f"{LIGHT_FUNCTION_PATH}/MI_LF_Sub_DirtyFlicker_Warm", m_flicker)
    set_mi_scalar(mi_flicker_warm, "NoiseTiling", 1.0)
    set_mi_scalar(mi_flicker_warm, "FlickerSpeed", 6.0)
    set_mi_scalar(mi_flicker_warm, "MinBrightness", 0.55)
    set_mi_scalar(mi_flicker_warm, "MaxBrightness", 1.0)
    set_mi_scalar(mi_flicker_warm, "Brightness", 0.95)
    save_asset(mi_flicker_warm)

    mi_flicker_cold = load_or_create_mi(f"{LIGHT_FUNCTION_PATH}/MI_LF_Sub_DirtyFlicker_Cold", m_flicker)
    set_mi_scalar(mi_flicker_cold, "NoiseTiling", 1.15)
    set_mi_scalar(mi_flicker_cold, "FlickerSpeed", 8.0)
    set_mi_scalar(mi_flicker_cold, "MinBrightness", 0.4)
    set_mi_scalar(mi_flicker_cold, "MaxBrightness", 1.0)
    set_mi_scalar(mi_flicker_cold, "Brightness", 1.0)
    save_asset(mi_flicker_cold)

    m_sweep = load_or_create_material(f"{LIGHT_FUNCTION_PATH}/M_LF_Sub_AlarmSweep")
    build_light_function_alarm_sweep(m_sweep, textures.get("t_lf_alarmsweep.tga"))
    mi_sweep = load_or_create_mi(f"{LIGHT_FUNCTION_PATH}/MI_LF_Sub_AlarmSweep_Red", m_sweep)
    set_mi_scalar(mi_sweep, "SweepSpeedX", 0.35)
    set_mi_scalar(mi_sweep, "SweepSpeedY", 0.0)
    set_mi_scalar(mi_sweep, "Brightness", 1.0)
    set_mi_scalar(mi_sweep, "Contrast", 1.0)
    save_asset(mi_sweep)

    log("Created Light Function materials and instances")


def create_or_update_decal_assets(textures: dict) -> None:
    ensure_directory(DECAL_SUB_PATH)

    m_decal = load_or_create_material(f"{DECAL_SUB_PATH}/M_Decal_Sub_GrimeMaster")
    build_decal_master(m_decal, textures.get("t_decal_leak.png"))

    mi_leak = load_or_create_mi(f"{DECAL_SUB_PATH}/MI_Decal_Sub_Leak_Dark", m_decal)
    set_mi_texture(mi_leak, "DecalTexture", textures.get("t_decal_leak.png"))
    set_mi_vector(mi_leak, "TintColor", unreal.LinearColor(0.19, 0.20, 0.22, 1.0))
    set_mi_scalar(mi_leak, "Opacity", 0.65)
    set_mi_scalar(mi_leak, "Roughness", 0.88)
    set_mi_scalar(mi_leak, "MaskBias", 0.0)
    set_mi_scalar(mi_leak, "MaskScale", 1.0)
    save_asset(mi_leak)

    mi_rust = load_or_create_mi(f"{DECAL_SUB_PATH}/MI_Decal_Sub_Rust_Orange", m_decal)
    set_mi_texture(mi_rust, "DecalTexture", textures.get("t_decal_rust.png"))
    set_mi_vector(mi_rust, "TintColor", unreal.LinearColor(0.82, 0.38, 0.12, 1.0))
    set_mi_scalar(mi_rust, "Opacity", 0.8)
    set_mi_scalar(mi_rust, "Roughness", 0.95)
    set_mi_scalar(mi_rust, "MaskBias", 0.0)
    set_mi_scalar(mi_rust, "MaskScale", 1.0)
    save_asset(mi_rust)

    mi_warning = load_or_create_mi(f"{DECAL_SUB_PATH}/MI_Decal_Sub_Warning_Yellow", m_decal)
    set_mi_texture(mi_warning, "DecalTexture", textures.get("t_decal_warning.png"))
    set_mi_vector(mi_warning, "TintColor", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    set_mi_scalar(mi_warning, "Opacity", 0.9)
    set_mi_scalar(mi_warning, "Roughness", 0.6)
    set_mi_scalar(mi_warning, "MaskBias", 0.0)
    set_mi_scalar(mi_warning, "MaskScale", 1.0)
    save_asset(mi_warning)

    log("Created decal master and instances")


def ensure_niagara_path_only() -> None:
    ensure_directory(NIAGARA_PATH)
    warn(
        "Niagara systems were not auto-generated. Create "
        "`NS_Sub_DustMotes_LightBeam` and `NS_Sub_SteamLeak_LightHaze` manually "
        "from valid emitter templates in the editor."
    )


def main() -> None:
    log("=" * 60)
    log("Importing lighting textures and creating material assets")
    log("=" * 60)

    ensure_directory(TEXTURE_PATH)
    ensure_directory(DECAL_TEXTURE_PATH)
    ensure_directory(LIGHT_FUNCTION_PATH)
    ensure_directory(DECAL_SUB_PATH)
    ensure_directory(NIAGARA_PATH)

    textures = {}
    for spec in TEXTURE_SPECS:
        textures[spec["filename"]] = import_texture(spec)

    create_or_update_light_function_assets(textures)
    create_or_update_decal_assets(textures)
    ensure_niagara_path_only()

    unreal.EditorAssetLibrary.save_directory(VFX_BASE, only_if_is_dirty=False, recursive=True)
    unreal.EditorAssetLibrary.save_directory(DECAL_BASE, only_if_is_dirty=False, recursive=True)

    log("Done")
    log("=" * 60)


if __name__ == "__main__":
    main()
