import unreal
lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset("/Game/Sub3D/Material/M_CompartmentWater")
mi = unreal.load_asset("/Game/Sub3D/Material/MI_CompartmentWater")
print("=== M_CompartmentWater Params ===")
for p in lib.get_scalar_parameter_names(mat):
    val = lib.get_scalar_parameter_value(mat, p)
    print("  Scalar:", str(p), "=", val)
for p in lib.get_vector_parameter_names(mat):
    val = lib.get_vector_parameter_value(mat, p)
    print("  Vector:", str(p), "=", val)
for p in lib.get_texture_parameter_names(mat):
    val = lib.get_texture_parameter_value(mat, p)
    print("  Texture:", str(p), "=", val.get_name() if val else "None")
for p in lib.get_static_switch_parameter_names(mat):
    print("  StaticSwitch:", str(p))
print("=== MI_CompartmentWater Overrides ===")
for p in lib.get_scalar_parameter_names(mi):
    val = lib.get_scalar_parameter_value(mi, p)
    print("  Scalar:", str(p), "=", val)
for p in lib.get_vector_parameter_names(mi):
    val = lib.get_vector_parameter_value(mi, p)
    print("  Vector:", str(p), "=", val)
for p in lib.get_texture_parameter_names(mi):
    val = lib.get_texture_parameter_value(mi, p)
    print("  Texture:", str(p), "=", val.get_name() if val else "None")