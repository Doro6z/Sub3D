"""@UnrealClaude Script
@Description: Readback LightColor + Behavior from SubLight BP CDOs for verification.
"""
import unreal

for name in ("BP_SubLight_Warm","BP_SubLight_Cold","BP_SubLight_Alarm","BP_SubLight_Faulty"):
    bp = unreal.EditorAssetLibrary.load_asset(f"/Game/Sub3D/Blueprint/SubBP/{name}")
    cdo = unreal.get_default_object(bp.generated_class())
    lc = cdo.get_editor_property("LightColor")
    beh = cdo.get_editor_property("Behavior")
    unreal.log(f"VERIFY {name}: LightColor=({lc.r:.2f},{lc.g:.2f},{lc.b:.2f})  Behavior={beh}")
