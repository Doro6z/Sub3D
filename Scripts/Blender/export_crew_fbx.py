"""
Sub3D — Export Crew Character FBX for UE5
==========================================
Exports SM_Crew + Armature_Crew as a single FBX
ready for UE5 import.

NON-DESTRUCTIVE: does not modify the scene.

Blender > Scripting > Open > Alt+P
"""

import bpy
import os

EXPORT_DIR = "C:/Dev/Sub3D/Art/Characters/Export"
EXPORT_FILE = "SK_Crew_Basic.fbx"

def main():
    print("\n" + "=" * 60)
    print("Sub3D — Export Crew FBX")
    print("=" * 60)

    # Ensure export directory exists
    os.makedirs(EXPORT_DIR, exist_ok=True)
    filepath = os.path.join(EXPORT_DIR, EXPORT_FILE)

    # Find objects
    mesh = bpy.data.objects.get("SM_Crew")
    armature = bpy.data.objects.get("Armature_Crew")

    if not mesh:
        print("  ERROR: SM_Crew not found!"); return
    if not armature:
        print("  ERROR: Armature_Crew not found!"); return

    # Ensure object mode
    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Select only mesh + armature
    bpy.ops.object.select_all(action='DESELECT')
    mesh.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    # Reset pose to T-pose before export
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.pose.transforms_clear()
    bpy.ops.object.mode_set(mode='OBJECT')

    # Export FBX
    print(f"\n  Exporting to: {filepath}")
    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=True,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Y',
        axis_up='Z',
        object_types={'ARMATURE', 'MESH'},
        use_armature_deform_only=True,
        add_leaf_bones=False,
        mesh_smooth_type='FACE',
        use_mesh_modifiers=True,
        bake_anim=False,
    )

    # Stats
    print(f"\n  Exported:")
    print(f"    Mesh:     {mesh.name} ({len(mesh.data.polygons)} faces)")
    print(f"    Armature: {armature.name} ({len(armature.data.bones)} bones)")
    print(f"    Materials: {len(mesh.data.materials)}")
    for i, m in enumerate(mesh.data.materials):
        print(f"      [{i}] {m.name}")
    print(f"    File: {filepath}")
    print(f"    Size: {os.path.getsize(filepath) / 1024:.0f} KB")

    print("\n" + "=" * 60)
    print("FBX ready for UE5 import.")
    print("Import into: Content/Sub3D/Characters/Meshes/Bodies/")
    print("=" * 60)


main()
