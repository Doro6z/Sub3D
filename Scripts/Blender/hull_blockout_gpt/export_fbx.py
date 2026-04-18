"""
Sub3D — FBX Export for UE5
===========================
Run AFTER assign_materials.py.
Exports all SM_* objects as FBX with UE5-compatible settings.

Blender > Scripting > Open > Alt+P
"""

import bpy
import os

# Output path
OUTPUT_DIR = r"C:\Dev\Sub3D\Content\Sub3D\FirstPlayableRun\Meshes\Blockout"
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "Craniata_Blockout.fbx")

# Names containing any of these substrings are skipped at export time. They
# are auxiliary meshes shipped by main.py for manual boolean operations and
# should never end up in the UE5 import.
EXPORT_SKIP_SUBSTRINGS = (
    "_Cutter_Manual",
    "_RectRecut",
    "_EnvCut",
    "_EnvelopeCut",
)


def _should_export(obj):
    if obj.type != "MESH":
        return False
    name = obj.name or ""
    for marker in EXPORT_SKIP_SUBSTRINGS:
        if marker in name:
            return False
    return True


def main():
    print("\n" + "=" * 60)
    print("Sub3D — FBX Export for UE5")
    print("=" * 60)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Select all mesh objects EXCEPT auxiliary / cutter meshes.
    bpy.ops.object.select_all(action='DESELECT')
    count = 0
    skipped = []
    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue
        if _should_export(obj):
            obj.select_set(True)
            count += 1
        else:
            skipped.append(obj.name)

    if skipped:
        print(f"Skipped {len(skipped)} auxiliary mesh(es):")
        for name in skipped:
            print(f"  - {name}")

    print(f"Exporting {count} mesh objects to:")
    print(f"  {OUTPUT_FILE}")

    # Export FBX with UE5 settings.
    # Note: `use_custom_properties` was removed in Blender 5.0; we don't need
    # custom props for the blockout pipeline anyway.
    bpy.ops.export_scene.fbx(
        filepath=OUTPUT_FILE,
        use_selection=True,
        apply_scale_options='FBX_SCALE_UNITS',
        apply_unit_scale=True,
        use_mesh_modifiers=True,
        mesh_smooth_type='FACE',
        use_tspace=True,           # Tangent space for normal maps
        use_triangles=False,       # UE5 handles triangulation
        axis_forward='X',
        axis_up='Z',
    )

    size_mb = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)
    print(f"\n  Exported: {size_mb:.1f} MB")
    print(f"  Objects: {count}")
    print("=" * 60)
    print("\nImport in UE5:")
    print("  1. Content Browser > Content/Sub3D/FirstPlayableRun/Meshes/Blockout/")
    print("  2. Right-click > Import > select Craniata_Blockout.fbx")
    print("  3. Import Settings:")
    print("     - Auto Generate Collision: OFF")
    print("     - Generate Lightmap UVs: OFF (we have our own UVs)")
    print("     - Import Normals: Import Normals and Tangents")
    print("     - Material Import: Do Not Create Materials (assign UE materials instead)")


main()
