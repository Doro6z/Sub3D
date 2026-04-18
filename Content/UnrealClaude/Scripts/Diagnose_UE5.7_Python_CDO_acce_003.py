"""@UnrealClaude Script
@Description: Diagnose UE5.7 Python CDO access for BP_SubLight_Cold.
"""
import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Sub3D/Blueprint/SubBP/BP_SubLight_Cold")
unreal.log(f"bp type: {type(bp).__name__}")

gc = bp.generated_class
unreal.log(f"gc type: {type(gc).__name__}  callable={callable(gc)}")

# Find all 'default' related attributes
unreal_attrs = [a for a in dir(unreal) if 'default' in a.lower()]
unreal.log(f"unreal.* default attrs: {unreal_attrs}")

gc_attrs = [a for a in dir(gc) if 'default' in a.lower() or 'cdo' in a.lower()]
unreal.log(f"gc.* default/cdo attrs: {gc_attrs}")

# Try methods / properties
for name in ('get_default_object', 'default_object', 'class_default_object'):
    v = getattr(gc, name, '<missing>')
    unreal.log(f"  gc.{name} -> {type(v).__name__}  callable={callable(v) if v != '<missing>' else 'N/A'}")

# Try unreal module function variants
for name in ('get_default_object', 'default_object_of_class'):
    v = getattr(unreal, name, '<missing>')
    unreal.log(f"  unreal.{name} -> {type(v).__name__ if v!='<missing>' else '<missing>'}")
    if v != '<missing>' and callable(v):
        try:
            cdo = v(gc)
            unreal.log(f"    called: cdo type={type(cdo).__name__}")
        except Exception as exc:
            unreal.log(f"    call failed: {exc}")

# Try get_default_object() as a callable method on UClass
try:
    cdo = gc.get_default_object()
    unreal.log(f"gc.get_default_object() -> {type(cdo).__name__}")
    # Does it have LightColor?
    lc = cdo.get_editor_property("LightColor")
    unreal.log(f"  LightColor={lc}")
except Exception as exc:
    unreal.log(f"gc.get_default_object() failed: {exc}")
