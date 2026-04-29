import unreal

mf_path = "/Game/Sub3D/Material/Functions/MF_CompartmentWater_Containment"
mf = unreal.load_asset(mf_path)

new_code = r'''// OBB containment - pure AABB in CV local space, no distance field dependency.
// Inputs: WorldPos (float3), CVCenter (float3), HalfExtent (float3),
//         W2LRow0/1/2 (float4) - rows of the world-to-CV-local matrix (rotation+scale block only).
// Pre-subtracts CVCenter so the translation column (.w) is not needed.
float3 Delta;
Delta.x = WorldPos.x - CVCenter.x;
Delta.y = WorldPos.y - CVCenter.y;
Delta.z = WorldPos.z - CVCenter.z;

float3 P;
P.x = Delta.x * W2LRow0.x + Delta.y * W2LRow1.x + Delta.z * W2LRow2.x;
P.y = Delta.x * W2LRow0.y + Delta.y * W2LRow1.y + Delta.z * W2LRow2.y;
P.z = Delta.x * W2LRow0.z + Delta.y * W2LRow1.z + Delta.z * W2LRow2.z;

float3 AbsP = abs(P);
if (AbsP.x > HalfExtent.x) return 0.0;
if (AbsP.y > HalfExtent.y) return 0.0;
if (AbsP.z > HalfExtent.z) return 0.0;
return 1.0;'''

# Try to find the Custom expression by iterating inner objects
found = False
for i in range(30):
    for prefix in ["MaterialExpressionCustom_", "MaterialExpressionCustom"]:
        candidate = prefix + (str(i) if i > 0 else "")
        obj_path = f"{mf_path}.MF_CompartmentWater_Containment:{candidate}"
        obj = unreal.find_object(None, obj_path)
        if obj and hasattr(obj, 'get_editor_property'):
            try:
                old = obj.get_editor_property('code')
                print('Found Custom at:', obj_path)
                print('Old code (first 80):', str(old)[:80])
                obj.set_editor_property('code', new_code)
                print('Code updated.')
                found = True
                break
            except Exception as e:
                pass
    if found:
        break

if not found:
    print('Custom expression not found via find_object - must edit in material editor.')
    print('New HLSL to paste:')
    print(new_code)

# Save the asset regardless
try:
    unreal.EditorAssetLibrary.save_asset(mf_path, only_if_is_dirty=False)
    print('MF saved.')
except Exception as e:
    print('Save err:', e)