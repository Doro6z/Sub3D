"""
Sub3D — UV Unwrap for Craniata blockout
========================================
Run AFTER main.py in the same Blender scene.
Applies Smart UV Project to all SM_* objects with type-specific settings.

Blender > Scripting > Open > Alt+P
"""

import bpy
import math

# ═══════════════════════════════════════════════════════════════
# UV STRATEGY BY OBJECT TYPE
# ═══════════════════════════════════════════════════════════════

# angle_limit in radians: lower = more islands but less distortion
# island_margin: spacing between UV islands to prevent texture bleed
UV_PROFILES = {
    "hull": {
        "method": "smart",
        "angle_limit": 0.9,
        "island_margin": 0.02,
        "desc": "Hull — moderate islands for large curved surface",
    },
    "deck": {
        "method": "smart",
        "angle_limit": 1.2,
        "island_margin": 0.015,
        "desc": "Decks — mostly flat, fewer islands needed",
    },
    "bulkhead": {
        "method": "smart",
        "angle_limit": 1.0,
        "island_margin": 0.02,
        "desc": "Bulkheads — vertical flat surfaces",
    },
    "door": {
        "method": "smart",
        "angle_limit": 1.0,
        "island_margin": 0.02,
        "desc": "Doors — small detailed objects",
    },
    "fin": {
        "method": "smart",
        "angle_limit": 1.3,
        "island_margin": 0.01,
        "desc": "Fins/hydroplanes — thin flat surfaces",
    },
    "prop": {
        "method": "smart",
        "angle_limit": 0.8,
        "island_margin": 0.02,
        "desc": "Propulsor — complex curved geometry",
    },
    "default": {
        "method": "smart",
        "angle_limit": 1.0,
        "island_margin": 0.02,
        "desc": "Default — general purpose",
    },
}


def classify_object(obj_name):
    """Determine UV profile based on object name."""
    name = obj_name.lower()
    if "hull" in name:
        return "hull"
    if "deck" in name or "walkway" in name or "catwalk" in name or "ramp" in name:
        return "deck"
    if "bh_" in name or "bulkhead" in name or "partition" in name:
        return "bulkhead"
    if "door" in name or "hatch" in name or "battant" in name:
        return "door"
    if "fin" in name or "hydro" in name or "rudder" in name:
        return "fin"
    if "prop" in name or "duct" in name or "hub" in name or "blade" in name:
        return "prop"
    return "default"


def uv_unwrap_object(obj, profile):
    """Apply Smart UV Project to one object."""
    if obj.type != "MESH":
        return False
    if len(obj.data.polygons) == 0:
        return False

    # Ensure UV map exists
    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")

    # Deselect everything
    bpy.ops.object.select_all(action='DESELECT')

    # Select and activate this object
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    # Enter edit mode
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')

    # Apply Smart UV Project
    try:
        bpy.ops.uv.smart_project(
            angle_limit=profile["angle_limit"],
            margin_method='SCALED',
            island_margin=profile["island_margin"],
            correct_aspect=True,
            scale_to_bounds=False,
        )
    except Exception as e:
        print(f"  WARNING: Smart UV failed on {obj.name}: {e}")
        # Fallback to basic unwrap
        try:
            bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=profile["island_margin"])
        except Exception:
            pass

    # Pack islands
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin=profile["island_margin"])
    except Exception:
        pass

    # Back to object mode
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.select_set(False)

    return True


def main():
    print("\n" + "=" * 60)
    print("Sub3D — UV Unwrap (Craniata)")
    print("=" * 60)

    # Collect all mesh objects
    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    print(f"\nFound {len(mesh_objects)} mesh objects")

    results = {"ok": 0, "skip": 0, "fail": 0}
    stats = {}

    for obj in sorted(mesh_objects, key=lambda o: o.name):
        obj_type = classify_object(obj.name)
        profile = UV_PROFILES[obj_type]

        if len(obj.data.polygons) == 0:
            results["skip"] += 1
            continue

        success = uv_unwrap_object(obj, profile)

        if success:
            results["ok"] += 1
            stats[obj_type] = stats.get(obj_type, 0) + 1
            uv_layer = obj.data.uv_layers.active
            print(f"  OK  {obj.name:40s} [{obj_type:10s}] {len(obj.data.polygons):5d} faces  angle={profile['angle_limit']:.1f}")
        else:
            results["fail"] += 1
            print(f"  FAIL {obj.name}")

    print(f"\n" + "=" * 60)
    print(f"Results: {results['ok']} OK, {results['skip']} skipped, {results['fail']} failed")
    print(f"\nBy type:")
    for obj_type, count in sorted(stats.items()):
        print(f"  {obj_type:12s}: {count} objects")
    print("=" * 60)
    print("\nUV unwrap complete. Check UVs: select object > Tab > UV Editing workspace")
    print("Next: run assign_materials.py to apply PBR textures.")


main()
