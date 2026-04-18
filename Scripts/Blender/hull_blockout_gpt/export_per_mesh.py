"""
Sub3D - Per-mesh FBX export for Craniata
========================================

Exports each SM_* mesh into its own FBX file under
Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/.

Why per-mesh instead of one combined FBX:
- Each FBX is centered on its own object origin (which core/pivots.py set to
  the gameplay-correct pivot: door hinge at min Y, rudder at min X, etc.).
- UE imports each FBX as one StaticMesh asset whose vertices are in
  object-local space and whose pivot matches the Blender pivot exactly.
- World placement is then composed separately (drag actors into a level, or
  via Craniata_Transforms.json + compose_craniata_bp.py).

Axis convention (neutral FBX, no axis tricks):
- We use Blender's FBX export defaults: axis_forward='-Y', axis_up='Z',
  bake_space_transform=False. This produces an FBX that UE reads with its
  standard FBX import convert (Blender -Y -> UE +X, Blender +Z -> UE +Z).
- Asset orientation is consistent across all per-mesh exports.
- Sub bow lands at UE local -Y when actor yaw=0; compose_craniata_bp_v2.py
  applies a +90 deg root yaw to bring the bow to UE +X (Forward).
- We previously tried bake_space_transform=True with various axis options;
  it interacted badly with the cm scene unit and produced double-baked
  scales on some meshes (visible as cassette/door scale != 1). The neutral
  export below avoids that whole class of bugs.

Skips: cutter / boolean artifact meshes (same set as export_fbx.py).

Run: Blender > Scripting > Open > Alt+P
"""

import bpy
import os
from pathlib import Path


EXPORT_DIR = Path(r"C:\Dev\Sub3D\Content\Sub3D\FirstPlayableRun\Meshes\Blockout\Craniata_PerMesh")
PREFIX = "SM_"
SKIP_SUBSTRINGS = (
    "_Cutter_Manual",
    "_RectRecut",
    "_EnvCut",
    "_EnvelopeCut",
)


def _should_export(obj):
    if obj.type != "MESH":
        return False
    name = obj.name or ""
    if not name.startswith(PREFIX):
        return False
    for skip in SKIP_SUBSTRINGS:
        if skip in name:
            return False
    return True


def _save_selection_state():
    active = bpy.context.view_layer.objects.active
    selected = [o for o in bpy.data.objects if o.select_get()]
    return active, selected


def _restore_selection_state(active, selected):
    bpy.ops.object.select_all(action="DESELECT")
    for o in selected:
        try:
            o.select_set(True)
        except ReferenceError:
            pass
    if active and active.name in bpy.data.objects:
        bpy.context.view_layer.objects.active = active


def main():
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)

    backup_active, backup_selected = _save_selection_state()

    targets = [o for o in bpy.data.objects if _should_export(o)]
    print("=" * 60)
    print(f"export_per_mesh: {len(targets)} meshes -> {EXPORT_DIR}")
    print("=" * 60)

    exported = 0
    failed = []

    for obj in targets:
        try:
            bpy.ops.object.select_all(action="DESELECT")
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj

            filepath = str(EXPORT_DIR / f"{obj.name}.fbx")

            bpy.ops.export_scene.fbx(
                filepath=filepath,
                use_selection=True,
                object_types={"MESH"},
                apply_scale_options="FBX_SCALE_UNITS",  # let Blender handle cm cleanly
                apply_unit_scale=True,
                axis_forward="-Y",             # Blender default
                axis_up="Z",                   # Blender default
                bake_space_transform=False,    # NO axis bake -> no scale double-apply
                use_mesh_modifiers=True,       # bake Solidify, etc.
                mesh_smooth_type="FACE",
                use_tspace=True,
                use_triangles=False,
                bake_anim=False,
            )
            exported += 1
        except Exception as exc:
            failed.append((obj.name, repr(exc)))

    _restore_selection_state(backup_active, backup_selected)

    print("=" * 60)
    print(f"export_per_mesh DONE: {exported}/{len(targets)} exported")
    if failed:
        print(f"  failures: {len(failed)}")
        for name, msg in failed:
            print(f"    [FAIL] {name}: {msg}")
    print(f"  output dir: {EXPORT_DIR}")
    print("=" * 60)


main()
