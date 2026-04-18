"""@UnrealClaude Script
@Description: Set LightColor and Behavior CDO defaults on BP_SubLight_{Warm,Cold,Alarm,Faulty}.
"""
import unreal

BP_PATH = "/Game/Sub3D/Blueprint/SubBP"

# (bp_name, (R,G,B), behavior_value_name)
BP_DEFS = [
    ("BP_SubLight_Warm",   (1.00, 0.93, 0.84), "STEADY"),
    ("BP_SubLight_Cold",   (0.78, 0.86, 1.00), "STEADY"),
    ("BP_SubLight_Alarm",  (1.00, 0.62, 0.30), "BLINK"),
    ("BP_SubLight_Faulty", (0.86, 0.93, 1.00), "FAULTY"),
]

def resolve_behavior_enum():
    for n in ("SubLightBehavior", "ESubLightBehavior"):
        e = getattr(unreal, n, None)
        if e is not None:
            return e
    return None

def resolve_cdo(bp):
    gc = getattr(bp, "generated_class", None)
    if gc is None:
        gc = bp.get_editor_property("GeneratedClass")
    if gc is None:
        return None
    g = getattr(gc, "get_default_object", None)
    return g() if callable(g) else unreal.get_default_object(gc)

behavior_enum = resolve_behavior_enum()
if behavior_enum is None:
    unreal.log_warning("ESubLightBehavior not bound; Behavior will not be set.")

for name, rgb, beh in BP_DEFS:
    p = f"{BP_PATH}/{name}"
    bp = unreal.EditorAssetLibrary.load_asset(p)
    if not bp:
        unreal.log_error(f"Missing {p}")
        continue
    cdo = resolve_cdo(bp)
    if cdo is None:
        unreal.log_error(f"No CDO for {name}")
        continue
    r, g, b = rgb
    cdo.set_editor_property("LightColor", unreal.LinearColor(r, g, b, 1.0))
    if behavior_enum is not None:
        val = getattr(behavior_enum, beh, None)
        if val is not None:
            cdo.set_editor_property("Behavior", val)
    unreal.EditorAssetLibrary.save_asset(p)
    unreal.log(f"  {name}: color=({r:.2f},{g:.2f},{b:.2f}) behavior={beh}")
unreal.log("SubLight CDO defaults applied.")
