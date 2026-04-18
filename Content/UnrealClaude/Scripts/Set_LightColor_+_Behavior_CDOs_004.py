"""@UnrealClaude Script
@Description: Set LightColor + Behavior CDOs on SubLight BPs (v3 fix: generated_class is method).
"""
import unreal

BP_PATH = "/Game/Sub3D/Blueprint/SubBP"

BP_DEFS = [
    ("BP_SubLight_Warm",   (1.00, 0.93, 0.84), "STEADY"),
    ("BP_SubLight_Cold",   (0.78, 0.86, 1.00), "STEADY"),
    ("BP_SubLight_Alarm",  (1.00, 0.62, 0.30), "BLINK"),
    ("BP_SubLight_Faulty", (0.86, 0.93, 1.00), "FAULTY"),
]

eal = unreal.EditorAssetLibrary
behavior_enum = getattr(unreal, "SubLightBehavior", None) or getattr(unreal, "ESubLightBehavior", None)
if behavior_enum is None:
    unreal.log_warning("ESubLightBehavior not exposed; skipping Behavior override.")

for name, rgb, beh in BP_DEFS:
    path = f"{BP_PATH}/{name}"
    bp = eal.load_asset(path)
    if bp is None:
        unreal.log_error(f"Could not load {path}")
        continue
    gen_class = bp.generated_class()
    if gen_class is None:
        unreal.log_error(f"{name}: generated_class() returned None")
        continue
    cdo = unreal.get_default_object(gen_class)
    if cdo is None:
        unreal.log_error(f"{name}: could not resolve CDO")
        continue
    r, g, b = rgb
    cdo.set_editor_property("LightColor", unreal.LinearColor(r, g, b, 1.0))
    if behavior_enum is not None:
        val = getattr(behavior_enum, beh, None)
        if val is not None:
            cdo.set_editor_property("Behavior", val)
    bp.modify()
    eal.save_asset(path)
    unreal.log(f"  {name}: color=({r:.2f},{g:.2f},{b:.2f}) behavior={beh}")

unreal.log("SubLight CDO defaults applied.")
