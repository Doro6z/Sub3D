import unreal

def inspect_mf(path, name):
    mf = unreal.load_asset(path)
    print("=== " + name + " ===")
    print("Class:", mf.get_class().get_name())
    # Try to list function expressions via MaterialEditingLibrary
    lib = unreal.MaterialEditingLibrary
    try:
        scalars = lib.get_scalar_parameter_names(mf)
        for p in scalars:
            val = lib.get_scalar_parameter_value(mf, p)
            print("  Scalar:", str(p), "=", val)
    except Exception as e:
        print("  Scalars err:", e)
    try:
        vecs = lib.get_vector_parameter_names(mf)
        for p in vecs:
            val = lib.get_vector_parameter_value(mf, p)
            print("  Vector:", str(p), "=", val)
    except Exception as e:
        print("  Vectors err:", e)
    try:
        texs = lib.get_texture_parameter_names(mf)
        for p in texs:
            val = lib.get_texture_parameter_value(mf, p)
            print("  Texture:", str(p), "=", val.get_name() if val else "None")
    except Exception as e:
        print("  Textures err:", e)
    try:
        switches = lib.get_static_switch_parameter_names(mf)
        for p in switches:
            print("  StaticSwitch:", str(p))
    except Exception as e:
        print("  Switches err:", e)
    # Try direct property access
    try:
        props = dir(mf)
        interesting = [x for x in props if any(k in x.lower() for k in ['expression','input','output','function','param','node'])]
        print("  Props:", interesting[:20])
    except Exception as e:
        print("  dir err:", e)

inspect_mf("/Game/Sub3D/Material/Functions/MF_CompartmentWater_Containment", "MF_Containment")
inspect_mf("/Game/Sub3D/Material/Functions/MF_CompartmentWater_Edge", "MF_Edge")
inspect_mf("/Game/Sub3D/Material/Functions/MF_CompartmentWater_Look", "MF_Look")