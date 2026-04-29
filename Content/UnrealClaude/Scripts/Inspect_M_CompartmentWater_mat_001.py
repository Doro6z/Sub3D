import unreal
mat = unreal.load_asset("/Game/Sub3D/Material/M_CompartmentWater")
print("BlendMode:", mat.get_editor_property("blend_mode"))
print("ShadingModel:", mat.get_editor_property("shading_model"))
print("TwoSided:", mat.get_editor_property("two_sided"))
print("UseMaterialAttributes:", mat.get_editor_property("use_material_attributes"))
print("TranslucencyLightingMode:", mat.get_editor_property("translucency_lighting_mode"))
print("bDisableDepthTest:", mat.get_editor_property("disable_depth_test"))
print("OpacityMaskClipValue:", mat.get_editor_property("opacity_mask_clip_value"))
print("DitheredLOD:", mat.get_editor_property("dithered_lod_transition"))
print("NumCustomizedUVs:", mat.get_editor_property("num_customized_uvs"))
expressions = mat.get_editor_property("expressions")
print("ExpressionCount:", len(expressions))
expr_types = {}
for e in expressions:
    t = type(e).__name__
    expr_types[t] = expr_types.get(t,0)+1
for k,v in sorted(expr_types.items(), key=lambda x: -x[1]):
    print(f"  {k}: {v}")
