"""
Sub3D - Pivot placement for blockout meshes (Blender 5.0 safe)
==============================================================
Fusion of the old fix_pivots.py and fix_turret_pivots.py scripts.

Rules per name prefix:
- SM_Door_*, SM_HatchDoor_*, SM_Airlock_Door_*   -> hinge at min Y, mid height
  (the central airlock seal strip is welded INTO the Port battant mesh so it
  inherits the battant pivot automatically)
- SM_Propeller*                                  -> center of rotor
- SM_RudderAssembly*, SM_Rudder*                 -> hinge line at forward-most X
- SM_Hydro_*                                     -> inner edge (closest to centerline)
- SM_Turret_*                                    -> yaw pivot at BAND-SLICE base center
                                                    (mean of lower 8 percent of verts, Z = min)
- SM_Antenna_* (retractable)                     -> base of mast, centered
- SM_Periscope*                                  -> base of housing, centered
- Fallback                                       -> geometric center

Calling convention:
    from core.pivots import fix_all_pivots
    fix_all_pivots()

This module is also exposed through the legacy wrappers fix_pivots.py and
fix_turret_pivots.py at the package root so existing Blender bookmarks still work.
"""

import bpy
from mathutils import Vector


PREFIXES = ("SM_",)

TURRET_BAND_SLICE_ABSOLUTE_MIN = 2.0
TURRET_BAND_SLICE_ABSOLUTE_MAX = 12.0
TURRET_BAND_SLICE_RELATIVE = 0.08


def _is_target(obj):
    if obj.type != "MESH":
        return False
    name = obj.name or ""
    return name.startswith(PREFIXES)


def _is_turret(obj):
    if obj.type != "MESH":
        return False
    name = obj.name or ""
    if not name.startswith("SM_Turret_"):
        return False
    if "Socket" in name:
        return False
    return True


def _bounds_world(obj):
    mat = obj.matrix_world
    pts = [mat @ Vector(corner) for corner in obj.bound_box]
    min_x = min(p.x for p in pts)
    max_x = max(p.x for p in pts)
    min_y = min(p.y for p in pts)
    max_y = max(p.y for p in pts)
    min_z = min(p.z for p in pts)
    max_z = max(p.z for p in pts)
    return min_x, max_x, min_y, max_y, min_z, max_z


def _band_slice_base_center(obj):
    """Precise yaw pivot for turret-style assemblies.

    Averages the XY of the vertices in the lowest Z band and anchors Z at min.
    """
    mat = obj.matrix_world
    verts = [mat @ v.co for v in obj.data.vertices]
    if not verts:
        return None
    min_z = min(v.z for v in verts)
    max_z = max(v.z for v in verts)
    dz = max_z - min_z
    band_height = max(TURRET_BAND_SLICE_ABSOLUTE_MIN, min(TURRET_BAND_SLICE_ABSOLUTE_MAX, dz * TURRET_BAND_SLICE_RELATIVE))
    band_top = min_z + band_height
    band = [v for v in verts if v.z <= band_top]
    if not band:
        return None
    cx = sum(v.x for v in band) / len(band)
    cy = sum(v.y for v in band) / len(band)
    return Vector((cx, cy, min_z))


def _set_origin_world(obj, location):
    if bpy.context.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    cursor = bpy.context.scene.cursor
    prev = cursor.location.copy()
    cursor.location = location
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    cursor.location = prev


def _pick_pivot(obj):
    name = obj.name or ""
    min_x, max_x, min_y, max_y, min_z, max_z = _bounds_world(obj)
    x_center = (min_x + max_x) * 0.5
    y_center = (min_y + max_y) * 0.5
    z_center = (min_z + max_z) * 0.5

    if name.startswith("SM_Door_") or name.startswith("SM_HatchDoor_") or name.startswith("SM_Airlock_Door"):
        return Vector((x_center, min_y, z_center))

    if name.startswith("SM_Propeller") or name == "SM_Propeller":
        return Vector((x_center, y_center, z_center))

    if name.startswith("SM_RudderAssembly") or name.startswith("SM_Rudder"):
        return Vector((min_x, y_center, z_center))

    if name.startswith("SM_Hydro_"):
        y_inner = min_y if abs(min_y) < abs(max_y) else max_y
        return Vector((x_center, y_inner, z_center))

    if name.startswith("SM_Turret_"):
        band = _band_slice_base_center(obj)
        if band is not None:
            return band
        return Vector((x_center, y_center, min_z))

    if "TurretStation" in name and name.endswith("_Rotator"):
        return Vector((x_center, y_center, z_center))

    if name.startswith("SM_Antenna_") or name.startswith("SM_Periscope"):
        return Vector((x_center, y_center, min_z))

    return Vector((x_center, y_center, z_center))


def fix_all_pivots():
    """Set logical origins on every SM_* mesh in the scene.

    Turrets get the band-slice pivot (precise base center). All other SM_* meshes
    get the rule above. Non-SM meshes are ignored.
    """
    targets = [obj for obj in bpy.data.objects if _is_target(obj)]
    updated = 0
    for obj in targets:
        pivot = _pick_pivot(obj)
        _set_origin_world(obj, pivot)
        updated += 1
    print(f"[pivots] Placed {updated} origins across SM_* meshes.")
    return updated


def fix_turret_pivots():
    """Legacy entry point: only retouch turret-style meshes."""
    turrets = [obj for obj in bpy.data.objects if _is_turret(obj)]
    updated = 0
    for obj in turrets:
        pivot = _band_slice_base_center(obj)
        if pivot is None:
            min_x, max_x, min_y, max_y, min_z, _ = _bounds_world(obj)
            pivot = Vector(((min_x + max_x) * 0.5, (min_y + max_y) * 0.5, min_z))
        _set_origin_world(obj, pivot)
        updated += 1
    print(f"[pivots] Placed {updated} turret base-center pivots.")
    return updated


if __name__ == "__main__":
    fix_all_pivots()
