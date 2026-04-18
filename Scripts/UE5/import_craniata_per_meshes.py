"""
Sub3D - Import Craniata Per-Mesh FBX Assets into UE5
====================================================

Imports every per-mesh FBX produced by:
    Scripts/Blender/hull_blockout_gpt/export_per_mesh.py

Source folder:
    Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/*.fbx

Destination folder:
    /Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh

This is the import path used by compose_craniata_bp_v2.py.
It replaces manual drag-and-drop import so the asset set and import settings
stay deterministic across recomposition attempts.

Interchange settings:
- bake_meshes = False
- bake_pivot_meshes = True
- combine_static_meshes = False
- import_static_meshes = True
- import_skeletal_meshes = False
- collision = False
- generate_lightmap_u_vs = False
- delete existing same-name assets before import

Reason:
- UE 5.7 imports FBX through Interchange by default.
- The old FbxImportUI path produced assets with world transforms baked into the
  mesh vertices. That makes BP recomposition apply placement twice.
- The settings below keep each static mesh in object-local space around the
  authored Blender pivot.

Run in UE5 Editor:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/import_craniata_per_meshes.py").read())

Run headless:
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/import_craniata_per_meshes.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4
"""

from pathlib import Path

import unreal


REPO_ROOT = Path(r"C:/Dev/Sub3D")
SOURCE_DIR = REPO_ROOT / "Content" / "Sub3D" / "FirstPlayableRun" / "Meshes" / "Blockout" / "Craniata_PerMesh"
DEST_FOLDER = "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh"


def log(msg: str) -> None:
    unreal.log(f"[ImportCraniataPerMesh] {msg}")


def warn(msg: str) -> None:
    unreal.log_warning(f"[ImportCraniataPerMesh] {msg}")


def delete_existing_asset_for_fbx(fbx_path: Path) -> None:
    asset_name = fbx_path.stem
    asset_path = f"{DEST_FOLDER}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if unreal.EditorAssetLibrary.delete_asset(asset_path):
            log(f"deleted existing asset: {asset_path}")
        else:
            warn(f"failed to delete existing asset: {asset_path}")


def make_interchange_override() -> "unreal.InterchangePipelineStackOverride":
    pipeline = unreal.InterchangeGenericAssetsPipeline()

    common = pipeline.get_editor_property("common_meshes_properties")
    common.set_editor_property("bake_meshes", False)
    common.set_editor_property("bake_pivot_meshes", True)
    common.set_editor_property("recompute_normals", False)
    common.set_editor_property("recompute_tangents", False)

    mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
    mesh_pipeline.set_editor_property("combine_static_meshes", False)
    mesh_pipeline.set_editor_property("import_static_meshes", True)
    mesh_pipeline.set_editor_property("import_skeletal_meshes", False)
    mesh_pipeline.set_editor_property("collision", False)
    mesh_pipeline.set_editor_property("generate_lightmap_u_vs", False)
    mesh_pipeline.set_editor_property("build_scale3d", unreal.Vector(1.0, 1.0, 1.0))

    try:
        material_pipeline = pipeline.get_editor_property("material_pipeline")
        for prop_name, prop_value in (
            ("import_materials", False),
            ("import_textures", False),
        ):
            try:
                material_pipeline.set_editor_property(prop_name, prop_value)
            except Exception:
                pass
        try:
            material_pipeline.set_editor_property(
                "material_import",
                unreal.InterchangeMaterialImportOption.DO_NOT_IMPORT,
            )
        except Exception:
            pass
    except Exception:
        pass

    override = unreal.InterchangePipelineStackOverride()
    override.add_pipeline(pipeline)
    return override


def make_task(fbx_path: Path, override: "unreal.InterchangePipelineStackOverride") -> "unreal.AssetImportTask":
    task = unreal.AssetImportTask()
    task.filename = str(fbx_path)
    task.destination_path = DEST_FOLDER
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.replace_existing_settings = True
    task.options = override
    return task


def main() -> None:
    if not SOURCE_DIR.is_dir():
        unreal.log_error(f"[ImportCraniataPerMesh] source dir not found: {SOURCE_DIR}")
        return

    fbx_files = sorted(SOURCE_DIR.glob("*.fbx"))
    if not fbx_files:
        unreal.log_error(f"[ImportCraniataPerMesh] no .fbx files found in {SOURCE_DIR}")
        return

    log(f"source dir: {SOURCE_DIR}")
    log(f"dest folder: {DEST_FOLDER}")
    log(f"fbx count: {len(fbx_files)}")

    override = make_interchange_override()

    tasks = []
    for path in fbx_files:
        delete_existing_asset_for_fbx(path)
        tasks.append(make_task(path, override))

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported_count = 0
    empty_results = 0

    for task in tasks:
        imported_paths = list(task.imported_object_paths)
        if imported_paths:
            imported_count += len(imported_paths)
        else:
            empty_results += 1
            warn(f"import returned no asset paths: {task.filename}")

    unreal.EditorAssetLibrary.save_directory(DEST_FOLDER, only_if_is_dirty=True, recursive=True)

    log("=" * 60)
    log(f"FBX files processed: {len(fbx_files)}")
    log(f"Imported assets:     {imported_count}")
    log(f"Empty task results:  {empty_results}")
    log("Saved destination folder")
    log("=" * 60)


if __name__ == "__main__":
    main()
