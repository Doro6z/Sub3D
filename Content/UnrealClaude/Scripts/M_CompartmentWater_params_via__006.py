import unreal
lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset("/Game/Sub3D/Material/M_CompartmentWater")
print("=== Scalar Params ===")
for p in lib.get_scalar_parameter_names(mat):
    val = lib.get_scalar_parameter_value(mat, p)
    print(" ", p, "=", val)
print("=== Vector Params ===")
for p in lib.get_vector_parameter_names(mat):
    val = lib.get_vector_parameter_value(mat, p)
    print(" ", p, "=", val)
print("=== Texture Params ===")
for p in lib.get_texture_parameter_names(mat):
    val = lib.get_texture_parameter_value(mat, p)
    print(" ", p, "=", val.get_name() if val else "None")
print("=== Static Switch Params ===")
for p in lib.get_static_switch_parameter_names(mat):
    print(" ", p)