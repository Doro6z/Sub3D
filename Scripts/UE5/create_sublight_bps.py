"""
Sub3D - Create SubLight Blueprint variants in UE5
==================================================
Run in UE5:
  Tools > Run Python Script... > Scripts/UE5/create_sublight_bps.py
  or paste in Output Log Python console.

Creates (or updates, if already present) 4 child Blueprints of ASubLightBase
under /Game/Sub3D/Blueprint/SubBP:

  BP_SubLight_Warm     interior default       ~4000-4300K   steady
  BP_SubLight_Cold     engine / ballast       ~6200-7000K   steady
  BP_SubLight_Alarm    warning / breach       ~2400-3000K   blink
  BP_SubLight_Faulty   old cyan fluorescent   ~5000-5600K   faulty

Only LightColor and Behavior are overridden on the CDO; every other
parameter (intensity, attenuation, cone angles, blink/faulty ranges)
stays on the ASubLightBase defaults so per-placement tweaks remain possible.
"""

import unreal

BP_PATH = "/Game/Sub3D/Blueprint/SubBP"
PARENT_CLASS_PATH = "/Script/Sub3D.SubLightBase"

# (bp_name, (R, G, B), behavior_value_name)
# ESubLightBehavior values: STEADY, BLINK, FAULTY, GYRO_SPIN
BP_DEFS = [
    ("BP_SubLight_Warm",   (1.00, 0.93, 0.84), "STEADY"),
    ("BP_SubLight_Cold",   (0.78, 0.86, 1.00), "STEADY"),
    ("BP_SubLight_Alarm",  (1.00, 0.62, 0.30), "BLINK"),
    ("BP_SubLight_Faulty", (0.86, 0.93, 1.00), "FAULTY"),
]


def resolve_behavior_enum():
    """Python bindings usually strip the E prefix; fall back to the raw name."""
    for class_name in ("SubLightBehavior", "ESubLightBehavior"):
        enum_cls = getattr(unreal, class_name, None)
        if enum_cls is not None:
            return enum_cls
    return None


def resolve_cdo(bp):
    # UE 5.7 Python: UBlueprint.generated_class is a method, not a property.
    gen_class = bp.generated_class()
    if gen_class is None:
        return None
    return unreal.get_default_object(gen_class)


def get_or_create_bp(bp_name, parent_class):
    asset_path = f"{BP_PATH}/{bp_name}"
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        return existing, False

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = asset_tools.create_asset(bp_name, BP_PATH, None, factory)
    return bp, True


def apply_defaults(bp, rgb, behavior_enum, behavior_value_name):
    cdo = resolve_cdo(bp)
    if cdo is None:
        unreal.log_error(f"Could not resolve CDO for {bp.get_name()}")
        return

    r, g, b = rgb
    cdo.set_editor_property("LightColor", unreal.LinearColor(r, g, b, 1.0))

    if behavior_enum is not None:
        behavior_value = getattr(behavior_enum, behavior_value_name, None)
        if behavior_value is not None:
            cdo.set_editor_property("Behavior", behavior_value)
        else:
            unreal.log_warning(
                f"{bp.get_name()}: enum value {behavior_value_name} not found on {behavior_enum.__name__}"
            )

    bp.modify()


def main():
    unreal.log("=" * 56)
    unreal.log("Sub3D - Creating SubLight Blueprint variants")
    unreal.log("=" * 56)

    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if not parent_class:
        unreal.log_error(f"Could not load parent class {PARENT_CLASS_PATH}")
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(BP_PATH):
        unreal.EditorAssetLibrary.make_directory(BP_PATH)

    behavior_enum = resolve_behavior_enum()
    if behavior_enum is None:
        unreal.log_warning(
            "ESubLightBehavior not found in unreal module; Behavior will be left at parent default."
        )

    for bp_name, rgb, behavior_value_name in BP_DEFS:
        bp, created = get_or_create_bp(bp_name, parent_class)
        if not bp:
            unreal.log_error(f"Failed to resolve {bp_name}")
            continue

        apply_defaults(bp, rgb, behavior_enum, behavior_value_name)

        unreal.EditorAssetLibrary.save_asset(f"{BP_PATH}/{bp_name}")
        verb = "Created" if created else "Updated"
        unreal.log(
            f"  {verb:<7} {bp_name:<22} "
            f"color=({rgb[0]:.2f}, {rgb[1]:.2f}, {rgb[2]:.2f})  "
            f"behavior={behavior_value_name}"
        )

    unreal.log("")
    unreal.log("Done. Child BPs ready under /Game/Sub3D/Blueprint/SubBP.")
    unreal.log("=" * 56)


main()
