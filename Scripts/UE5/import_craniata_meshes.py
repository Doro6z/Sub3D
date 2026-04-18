"""
Sub3D - Import Craniata Blender Meshes into UE5
================================================

Imports Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_Blockout.fbx
as individual static meshes under /Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata/.

Matches the import settings documented at the end of export_fbx.py:
    - Auto Generate Collision:  OFF
    - Generate Lightmap UVs:    OFF
    - Import Normals:           Import Normals and Tangents
    - Material Import:          Do Not Create Materials

Run in UE5 Editor:
    Tools > Python, then:
        exec(open(r"C:/Dev/Sub3D/Scripts/UE5/import_craniata_meshes.py").read())

Or headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/import_craniata_meshes.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4

Safe to re-run: existing assets are replaced with fresh imports (bReplaceExisting).
"""

from pathlib import Path

import unreal


FBX_PATH = Path(r"C:/Dev/Sub3D/Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_Blockout.fbx")
DEST_FOLDER = "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata"


def log(msg: str) -> None:
    unreal.log(f"[ImportCraniata] {msg}")


def build_task() -> "unreal.AssetImportTask":
    task = unreal.AssetImportTask()
    task.filename = str(FBX_PATH)
    task.destination_path = DEST_FOLDER
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.replace_existing_settings = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.create_physics_asset = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH

    sm_data: unreal.FbxStaticMeshImportData = options.static_mesh_import_data
    sm_data.set_editor_property("auto_generate_collision", False)
    sm_data.set_editor_property("generate_lightmap_u_vs", False)
    sm_data.set_editor_property(
        "normal_import_method",
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    )
    sm_data.set_editor_property("combine_meshes", False)
    sm_data.set_editor_property("import_uniform_scale", 1.0)
    # The Blender export uses axis_forward='X', axis_up='Z' which matches UE,
    # so no axis conversion override is needed here.

    task.options = options
    return task


def main() -> None:
    if not FBX_PATH.is_file():
        unreal.log_error(f"[ImportCraniata] FBX not found at {FBX_PATH}. Run export_fbx.py in Blender first.")
        return

    log(f"FBX  : {FBX_PATH} ({FBX_PATH.stat().st_size / (1024*1024):.2f} MB)")
    log(f"Dest : {DEST_FOLDER}")

    task = build_task()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    imported_paths = list(task.imported_object_paths)
    if not imported_paths:
        unreal.log_warning("[ImportCraniata] import returned no asset paths; check Output Log for FBX errors")
        return

    log(f"imported {len(imported_paths)} asset(s)")
    for p in imported_paths[:10]:
        log(f"  - {p}")
    if len(imported_paths) > 10:
        log(f"  ... and {len(imported_paths) - 10} more")

    # Save the whole destination folder so assets persist to disk.
    unreal.EditorAssetLibrary.save_directory(DEST_FOLDER, only_if_is_dirty=True, recursive=True)
    log("saved destination folder")


if __name__ == "__main__":
    main()
