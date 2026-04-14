import unreal


BASE_PATH = "/Game/Sub3D/Proto04C"
FX_PATH = f"{BASE_PATH}/FX"
MAT_PATH = f"{BASE_PATH}/Materials"
FEEDBACK_PATH = f"{BASE_PATH}/Feedback"

PARTICLE_COLOR_DEFAULT = unreal.LinearColor(0.1, 0.3, 0.45, 1.0)
MIST_COLOR_DEFAULT = unreal.LinearColor(0.3, 0.45, 0.55, 1.0)
FLOOD_WATER_COLOR_DEFAULT = unreal.LinearColor(0.05, 0.18, 0.25, 1.0)


def log_step(message):
    unreal.log(f"[Proto04C Assets] {message}")


def log_warning(message):
    unreal.log_warning(f"[Proto04C Assets] {message}")


def log_error(message):
    unreal.log_error(f"[Proto04C Assets] {message}")


def ensure_path(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def asset_path(package_path, asset_name):
    return f"{package_path}/{asset_name}"


def load_asset_if_exists(package_path, asset_name):
    full_path = asset_path(package_path, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        return unreal.EditorAssetLibrary.load_asset(full_path)
    return None


def load_or_create_asset(asset_name, package_path, asset_class, factory):
    ensure_path(package_path)

    existing_asset = load_asset_if_exists(package_path, asset_name)
    if existing_asset:
        log_step(f"Reusing existing asset {asset_name}")
        return existing_asset

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_asset = asset_tools.create_asset(asset_name, package_path, asset_class, factory)
    if not new_asset:
        log_error(f"Failed to create asset {asset_name} in {package_path}")
    return new_asset


def save_asset(asset):
    if asset:
        unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False)


def safe_set_property(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        log_warning(f"{obj.get_class().get_name()}.{property_name} unsupported: {exc}")
        return False


def clear_material_expressions(material):
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    except Exception as exc:
        log_warning(f"Could not clear material graph for {material.get_name()}: {exc}")


def create_material_expression(material, expression_class, x_pos, y_pos):
    expression = unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x_pos, y_pos)
    if not expression:
        raise RuntimeError(f"Failed to create {expression_class.__name__} in {material.get_name()}")
    return expression


def connect_to_material_property(expression, output_name, material_property):
    editing_lib = unreal.MaterialEditingLibrary

    if hasattr(editing_lib, "connect_material_property"):
        editing_lib.connect_material_property(expression, output_name, material_property)
        return

    if hasattr(editing_lib, "connect_material_expression_to_property"):
        editing_lib.connect_material_expression_to_property(expression, output_name, material_property)
        return

    raise RuntimeError("No MaterialEditingLibrary property connection API available in this UE Python build.")


def finalize_material(material):
    try:
        unreal.MaterialEditingLibrary.layout_material_expressions(material)
    except Exception as exc:
        log_warning(f"Could not layout material expressions for {material.get_name()}: {exc}")

    try:
        unreal.MaterialEditingLibrary.recompile_material(material)
    except Exception as exc:
        log_warning(f"Could not recompile material {material.get_name()}: {exc}")

    save_asset(material)


def create_material(asset_name, package_path, translucent=False, unlit=False, two_sided=False):
    material = load_or_create_asset(asset_name, package_path, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        return None

    safe_set_property(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_OPAQUE)
    if unlit:
        safe_set_property(material, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    safe_set_property(material, "two_sided", bool(two_sided))
    return material


def set_material_instance_scalar(material_instance, parameter_name, value):
    try:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(material_instance, parameter_name, value)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            material_instance,
            parameter_name,
            value,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )
    except Exception as exc:
        log_warning(f"Could not set scalar parameter {parameter_name} on {material_instance.get_name()}: {exc}")


def set_material_instance_vector(material_instance, parameter_name, value):
    try:
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(material_instance, parameter_name, value)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            material_instance,
            parameter_name,
            value,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )
    except Exception as exc:
        log_warning(f"Could not set vector parameter {parameter_name} on {material_instance.get_name()}: {exc}")


def set_material_instance_texture(material_instance, parameter_name, value):
    try:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material_instance, parameter_name, value)
    except TypeError:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            material_instance,
            parameter_name,
            value,
            unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
        )
    except Exception as exc:
        log_warning(f"Could not set texture parameter {parameter_name} on {material_instance.get_name()}: {exc}")


def create_material_instance(asset_name, package_path, parent_material):
    material_instance = load_or_create_asset(
        asset_name,
        package_path,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )
    if not material_instance:
        return None

    try:
        unreal.MaterialEditingLibrary.set_material_instance_parent(material_instance, parent_material)
    except Exception:
        safe_set_property(material_instance, "parent", parent_material)

    save_asset(material_instance)
    return material_instance


def create_niagara_system(asset_name, package_path):
    factory_class = getattr(unreal, "NiagaraSystemFactoryNew", None)
    if not factory_class:
        log_warning(f"NiagaraSystemFactoryNew unavailable, skipping {asset_name}")
        return None

    return load_or_create_asset(asset_name, package_path, unreal.NiagaraSystem, factory_class())


def create_blueprint_asset(asset_name, package_path, parent_class):
    blueprint = load_asset_if_exists(package_path, asset_name)
    if blueprint:
        log_step(f"Reusing existing blueprint {asset_name}")
        return blueprint

    factory = unreal.BlueprintFactory()
    safe_set_property(factory, "parent_class", parent_class)
    return load_or_create_asset(asset_name, package_path, unreal.Blueprint, factory)


def try_load_first_asset(paths):
    for candidate in paths:
        loaded = unreal.EditorAssetLibrary.load_asset(candidate)
        if loaded:
            return loaded
    return None


def resolve_enum_value(enum_type, candidate_names):
    for candidate_name in candidate_names:
        if hasattr(enum_type, candidate_name):
            return getattr(enum_type, candidate_name)
    return None


def try_set_first_supported_property(obj, property_names, value):
    for property_name in property_names:
        if safe_set_property(obj, property_name, value):
            return True
    return False


def create_texture_parameter_sample(material, parameter_name, x_pos, y_pos, candidate_paths):
    texture_param_class = getattr(unreal, "MaterialExpressionTextureSampleParameter2D", None)
    expression_class = texture_param_class if texture_param_class else unreal.MaterialExpressionTextureSample
    texture_sample = create_material_expression(material, expression_class, x_pos, y_pos)

    if texture_param_class:
        safe_set_property(texture_sample, "parameter_name", parameter_name)

    texture_asset = try_load_first_asset(candidate_paths)
    if texture_asset and texture_asset.get_class().get_name() == "Texture2D":
        safe_set_property(texture_sample, "texture", texture_asset)
    else:
        log_warning(f"Texture parameter {parameter_name} has no valid Texture2D source.")

    return texture_sample, texture_asset


def create_particle_color_alpha_chain(material, opacity_default, x_pos, y_pos, texture_candidates, texture_parameter_name):
    particle_color = create_material_expression(material, unreal.MaterialExpressionParticleColor, x_pos, y_pos)
    opacity_scalar = create_material_expression(material, unreal.MaterialExpressionScalarParameter, x_pos, y_pos + 220)
    safe_set_property(opacity_scalar, "parameter_name", "Opacity")
    safe_set_property(opacity_scalar, "default_value", opacity_default)

    texture_sample, texture_asset = create_texture_parameter_sample(
        material,
        texture_parameter_name,
        x_pos,
        y_pos + 420,
        texture_candidates,
    )

    opacity_mul = create_material_expression(material, unreal.MaterialExpressionMultiply, x_pos + 420, y_pos + 120)
    unreal.MaterialEditingLibrary.connect_material_expressions(particle_color, "A", opacity_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "", opacity_mul, "B")

    final_opacity = create_material_expression(material, unreal.MaterialExpressionMultiply, x_pos + 650, y_pos + 180)
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_mul, "", final_opacity, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_scalar, "", final_opacity, "B")
    connect_to_material_property(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)

    return {
        "particle_color": particle_color,
        "opacity_scalar": opacity_scalar,
        "texture_sample": texture_sample,
        "texture_asset": texture_asset,
    }


def setup_water_particle_materials():
    log_step("Setting up particle materials")
    ensure_path(FX_PATH)

    m_particle = create_material("M_WaterParticle", FX_PATH, translucent=True)
    if m_particle:
        clear_material_expressions(m_particle)

        color_node = create_material_expression(m_particle, unreal.MaterialExpressionVectorParameter, -400, -100)
        safe_set_property(color_node, "parameter_name", "BaseColor")
        safe_set_property(color_node, "default_value", PARTICLE_COLOR_DEFAULT)
        connect_to_material_property(color_node, "", unreal.MaterialProperty.MP_BASE_COLOR)

        create_particle_color_alpha_chain(
            m_particle,
            1.0,
            -450,
            80,
            [
                "/Engine/EngineMaterials/T_Default_Material_Grid_N",
                "/Engine/EditorResources/S_Terrain",
            ],
            "OpacityMaskTexture",
        )

        finalize_material(m_particle)
        mi_particle = create_material_instance("MI_WaterParticle", FX_PATH, m_particle)
        if mi_particle:
            set_material_instance_vector(mi_particle, "BaseColor", PARTICLE_COLOR_DEFAULT)
            set_material_instance_scalar(mi_particle, "Opacity", 0.7)
            save_asset(mi_particle)

    m_mist = create_material("M_WaterMist", FX_PATH, translucent=True, unlit=True)
    if m_mist:
        clear_material_expressions(m_mist)

        emissive_color = create_material_expression(m_mist, unreal.MaterialExpressionVectorParameter, -450, -80)
        safe_set_property(emissive_color, "parameter_name", "EmissiveColor")
        safe_set_property(emissive_color, "default_value", MIST_COLOR_DEFAULT)

        emissive_intensity = create_material_expression(m_mist, unreal.MaterialExpressionScalarParameter, -450, 80)
        safe_set_property(emissive_intensity, "parameter_name", "EmissiveIntensity")
        safe_set_property(emissive_intensity, "default_value", 0.3)

        emissive_mul = create_material_expression(m_mist, unreal.MaterialExpressionMultiply, -200, -20)
        unreal.MaterialEditingLibrary.connect_material_expressions(emissive_color, "", emissive_mul, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(emissive_intensity, "", emissive_mul, "B")
        connect_to_material_property(emissive_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        create_particle_color_alpha_chain(
            m_mist,
            1.0,
            -450,
            220,
            [
                "/Engine/EngineResources/DefaultTexture",
                "/Engine/EditorResources/NoisePreview",
            ],
            "CloudNoiseTexture",
        )

        finalize_material(m_mist)
        mi_mist = create_material_instance("MI_WaterMist", FX_PATH, m_mist)
        if mi_mist:
            set_material_instance_vector(mi_mist, "EmissiveColor", MIST_COLOR_DEFAULT)
            set_material_instance_scalar(mi_mist, "EmissiveIntensity", 0.3)
            set_material_instance_scalar(mi_mist, "Opacity", 0.15)
            save_asset(mi_mist)


def setup_flood_water_material():
    log_step("Setting up flood water material")
    ensure_path(MAT_PATH)

    m_base = create_material("M_FloodWater_Base", MAT_PATH, translucent=True, two_sided=True)
    if not m_base:
        return

    clear_material_expressions(m_base)
    translucency_mode = resolve_enum_value(
        unreal.TranslucencyLightingMode,
        [
            "TLM_SURFACE_FORWARD_SHADING",
            "TLM_SURFACE_PER_PIXEL_LIGHTING",
            "SURFACE_FORWARD_SHADING",
            "SURFACE_PER_PIXEL_LIGHTING",
        ],
    )
    if translucency_mode is not None:
        safe_set_property(m_base, "translucency_lighting_mode", translucency_mode)
    else:
        log_warning("No compatible TranslucencyLightingMode enum found for forward/surface lighting. Keeping engine default.")
    safe_set_property(m_base, "use_translucency_vertex_fog", True)

    color_node = create_material_expression(m_base, unreal.MaterialExpressionVectorParameter, -700, -220)
    safe_set_property(color_node, "parameter_name", "WaterColor")
    safe_set_property(color_node, "default_value", FLOOD_WATER_COLOR_DEFAULT)
    connect_to_material_property(color_node, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity_param = create_material_expression(m_base, unreal.MaterialExpressionScalarParameter, -700, 20)
    safe_set_property(opacity_param, "parameter_name", "Opacity")
    safe_set_property(opacity_param, "default_value", 0.55)

    depth_fade = create_material_expression(m_base, unreal.MaterialExpressionDepthFade, -700, 220)
    try_set_first_supported_property(depth_fade, ["fade_distance", "fade_distance_default"], 30.0)
    try_set_first_supported_property(depth_fade, ["opacity_default"], 1.0)

    opacity_mul = create_material_expression(m_base, unreal.MaterialExpressionMultiply, -420, 120)
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_param, "", opacity_mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(depth_fade, "", opacity_mul, "B")
    connect_to_material_property(opacity_mul, "", unreal.MaterialProperty.MP_OPACITY)

    specular = create_material_expression(m_base, unreal.MaterialExpressionConstant, -420, 360)
    safe_set_property(specular, "r", 0.02)
    connect_to_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    roughness = create_material_expression(m_base, unreal.MaterialExpressionConstant, -420, 460)
    safe_set_property(roughness, "r", 0.85)
    connect_to_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    texture_sample, water_normal = create_texture_parameter_sample(
        m_base,
        "NormalMap",
        -420,
        -420,
        [
            "/Game/StarterContent/Textures/T_Water_N",
            "/Engine/EngineResources/DefaultTexture",
        ],
    )

    panner = create_material_expression(m_base, unreal.MaterialExpressionPanner, -700, -420)
    safe_set_property(panner, "speed_x", 0.02)
    safe_set_property(panner, "speed_y", 0.01)

    texcoord = create_material_expression(m_base, unreal.MaterialExpressionTextureCoordinate, -930, -420)

    unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", panner, "Coordinate")
    unreal.MaterialEditingLibrary.connect_material_expressions(panner, "", texture_sample, "UVs")

    flatten_normal_class = getattr(unreal, "MaterialExpressionFlattenNormal", None)
    if flatten_normal_class:
        flatten_normal = create_material_expression(m_base, flatten_normal_class, -140, -420)
        safe_set_property(flatten_normal, "flatness", 0.7)
        unreal.MaterialEditingLibrary.connect_material_expressions(texture_sample, "", flatten_normal, "Normal")
        connect_to_material_property(flatten_normal, "", unreal.MaterialProperty.MP_NORMAL)
    else:
        log_warning("MaterialExpressionFlattenNormal unavailable. Connecting panned normal texture directly.")
        connect_to_material_property(texture_sample, "", unreal.MaterialProperty.MP_NORMAL)

    finalize_material(m_base)
    mi_flood = create_material_instance("MI_FloodWater", MAT_PATH, m_base)
    if mi_flood:
        set_material_instance_vector(mi_flood, "WaterColor", FLOOD_WATER_COLOR_DEFAULT)
        set_material_instance_scalar(mi_flood, "Opacity", 0.55)
        if water_normal:
            set_material_instance_texture(mi_flood, "NormalMap", water_normal)
        save_asset(mi_flood)


def log_niagara_placeholder_spec(system_name, spec_lines):
    log_warning(f"{system_name} created as structured placeholder. Manual graph setup still required:")
    for line in spec_lines:
        unreal.log_warning(f"[Proto04C Assets][{system_name}] {line}")


def setup_niagara_assets():
    log_step("Setting up Niagara systems")
    ensure_path(FX_PATH)

    ns_jet = create_niagara_system("NS_BreachWaterJet", FX_PATH)
    if ns_jet:
        save_asset(ns_jet)
        log_niagara_placeholder_spec(
            "NS_BreachWaterJet",
            [
                "Emitters: WaterJet_Core, WaterJet_Mist, WaterJet_Splash",
                "User params: JetScale (float), JetColor (LinearColor)",
                "Recommended materials: MI_WaterParticle for Core/Splash, MI_WaterMist for Mist",
                "Sim target: CPU, Local Space: true",
            ],
        )

    ns_drip = create_niagara_system("NS_LeakDrip", FX_PATH)
    if ns_drip:
        save_asset(ns_drip)
        log_niagara_placeholder_spec(
            "NS_LeakDrip",
            [
                "Emitter: LeakDrip",
                "User params: DripRate (float), DripColor (LinearColor)",
                "Recommended material: MI_WaterParticle",
                "Sim target: CPU, Local Space: true",
            ],
        )


def setup_camera_shake():
    log_step("Setting up camera shake blueprint")
    ensure_path(FEEDBACK_PATH)

    camera_shake_bp = create_blueprint_asset("CS_HullImpact", FEEDBACK_PATH, unreal.CameraShakeBase)
    if camera_shake_bp:
        save_asset(camera_shake_bp)
        log_warning("CS_HullImpact created. Perlin root shake pattern tuning remains manual in the editor.")


def run_step(step_name, step_fn, failures):
    try:
        step_fn()
    except Exception as exc:
        failures.append((step_name, str(exc)))
        log_error(f"{step_name} failed: {exc}")


def main():
    failures = []

    run_step("Particle Materials", setup_water_particle_materials, failures)
    run_step("Flood Water Material", setup_flood_water_material, failures)
    run_step("Niagara Assets", setup_niagara_assets, failures)
    run_step("Camera Shake", setup_camera_shake, failures)

    if failures:
        for step_name, reason in failures:
            log_error(f"FAILED: {step_name} -> {reason}")
        raise RuntimeError(f"Proto04C asset generation finished with {len(failures)} failing step(s).")

    log_step("Proto04C asset generation complete")


if __name__ == "__main__":
    main()
