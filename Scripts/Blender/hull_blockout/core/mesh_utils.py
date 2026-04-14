"""
Blender mesh creation helpers.
"""

import bpy
from mathutils import Vector


def create_mesh_object(name, verts, faces, color=(0.5, 0.5, 0.5), smooth=True):
    """
    Create a Blender mesh object from vertex/face lists.
    Returns the created object.
    """
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)

    # Material
    mat = bpy.data.materials.new(name=f"M_{name}")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.7
        bsdf.inputs["Metallic"].default_value = 0.5
    obj.data.materials.append(mat)

    # Fix normals
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')

    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True

    obj.select_set(False)
    return obj


def apply_solidify(obj, thickness, offset=-1):
    """Apply a solidify modifier and bake it."""
    mod = obj.modifiers.new("Solidify", 'SOLIDIFY')
    mod.thickness = thickness
    mod.offset = offset
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def setup_scene():
    """Configure scene for centimeter workflow."""
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = 'CENTIMETERS'

    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.clip_start = 1.0
                    space.clip_end = 500000.0


def clear_scene():
    """Remove all objects and orphan data."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in bpy.data.meshes:
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in bpy.data.materials:
        if block.users == 0:
            bpy.data.materials.remove(block)


def frame_view():
    """Frame the viewport to show all objects."""
    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break


def export_fbx(filepath):
    """Export scene as FBX with UE5-compatible settings."""
    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=False,
        apply_scale_options='FBX_SCALE_UNITS',
        apply_unit_scale=True,
        mesh_smooth_type='FACE',
        use_mesh_modifiers=True,
        axis_forward='X',
        axis_up='Z',
    )
