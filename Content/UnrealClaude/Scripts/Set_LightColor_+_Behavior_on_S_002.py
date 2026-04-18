"""@UnrealClaude Script
@Description: Set LightColor + Behavior on SubLight BP CDOs (v2, UE 5.7 API).
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

def resolve_cdo(bp):
    # UE 5.7 Python: UBlueprint.generated_class is a property returning UClass
    gen_class = None
    for attr in ("generated_class",):
        v = getattr(bp, attr, None)
        if v is not None and not callable(v):
            gen_class = v
            break
    if gen_class is None:
        try:
            gen_class = bp.get_editor_property("GeneratedClass")
        except Exception as exc:
            unreal.log_warning(f"get_editor_property GeneratedClass failed: {exc}")
    if gen_class is None:
        # Last resort: load the _C class by path
        class_path = f"{BP_PATH}/{bp.get_name()}.{bp.get_name()}_C"
        gen_class = unreal.load_object(None, class_path)
    if gen_class is None:
        return None
    # UClass.get_default_object() is the canonical UE5 Python call
    getter = getattr(gen_class, "get_default_object", None)
    if callable(getter):
        return getter()
    return None

for name, rgb, beh in BP_DEFS:
    path = f"{BP_PATH}/{name}"
    bp = eal.load_asset(path)
    if bp is None:
        unreal.log_error(f"Could not load {path}")
        continue
    cdo = resolve_cdo(bp)
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
