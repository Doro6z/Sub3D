import unreal
mat = unreal.load_asset("/Game/Sub3D/Material/M_CompartmentWater")
print("BlendMode:", mat.get_editor_property("blend_mode"))
print("ShadingModel:", mat.get_editor_property("shading_model"))
print("TwoSided:", mat.get_editor_property("two_sided"))
print("UseMaterialAttributes:", mat.get_editor_property("use_material_attributes"))
print("TranslucencyLightingMode:", mat.get_editor_property("translucency_lighting_mode"))
print("DisableDepthTest:", mat.get_editor_property("disable_depth_test"))
print("ScreenSpaceReflections:", mat.get_editor_property("screen_space_reflections"))
print("OpacityMaskClipValue:", mat.get_editor_property("opacity_mask_clip_value"))
expressions = mat.get_editor_property("expressions")
print("ExpressionCount:", len(expressions))
for e in expressions:
    cls = e.get_class().get_name()
    desc = ""
    try:
        desc = e.get_editor_property("desc")
    except:
        pass
    mat_fn = ""
    try:
        fn_ref = e.get_editor_property("material_function")
        if fn_ref:
            mat_fn = fn_ref.get_name()
    except:
        pass
    label = desc or mat_fn or ""
    print(" -", cls, "|", label)