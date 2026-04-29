import unreal
mat = unreal.load_asset("/Game/Sub3D/Material/M_CompartmentWater")
print("=== M_CompartmentWater ===")
print("BlendMode:", mat.get_editor_property("blend_mode"))
print("ShadingModel:", mat.get_editor_property("shading_model"))
print("TwoSided:", mat.get_editor_property("two_sided"))
print("UseMaterialAttributes:", mat.get_editor_property("use_material_attributes"))
print("TranslucencyLightingMode:", mat.get_editor_property("translucency_lighting_mode"))
print("DisableDepthTest:", mat.get_editor_property("disable_depth_test"))
print("bScreenSpaceReflections:", mat.get_editor_property("screen_space_reflections"))
print("bContactShadows:", mat.get_editor_property("contact_shadows"))
print("OpacityMaskClipValue:", mat.get_editor_property("opacity_mask_clip_value"))
print("TranslucencyDirectionalLightingIntensity:", mat.get_editor_property("translucency_directional_lighting_intensity"))
expressions = mat.get_editor_property("expressions")
print("
=== Expressions (", len(expressions), ") ===")
for e in expressions:
    cls = e.get_class().get_name()
    desc = ""
    try: desc = e.get_editor_property("desc")
    except: pass
    print(" -", cls, "|" if desc else "", desc if desc else "")
