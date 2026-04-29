"""
Sub3D - Launcher for voxel abyss pack build
===========================================

Runs the canonical build_all.py from Scripts/Blender/voxel_abyss_pack.
Use this launcher in Blender Text Editor to avoid path/import ambiguity.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _resolve_build_script() -> Path:
    candidates = []

    file_value = globals().get("__file__")
    if file_value:
        this_dir = Path(file_value).resolve().parent
        candidates.append(this_dir / "voxel_abyss_pack" / "build_all.py")
        candidates.append(this_dir / "Scripts" / "Blender" / "voxel_abyss_pack" / "build_all.py")

    try:
        import bpy

        for text in (
            getattr(bpy.context, "edit_text", None),
            getattr(getattr(bpy.context, "space_data", None), "text", None),
        ):
            filepath = getattr(text, "filepath", "")
            if filepath:
                base = Path(bpy.path.abspath(filepath)).resolve().parent
                candidates.append(base / "voxel_abyss_pack" / "build_all.py")
                candidates.append(base / "Scripts" / "Blender" / "voxel_abyss_pack" / "build_all.py")
    except Exception:
        pass

    cwd = Path.cwd().resolve()
    for base in (cwd, *cwd.parents):
        candidates.append(base / "Scripts" / "Blender" / "voxel_abyss_pack" / "build_all.py")

    candidates.append(Path(r"C:\Dev\Sub3D\Scripts\Blender\voxel_abyss_pack\build_all.py"))

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise RuntimeError(
        "Unable to find build_all.py for voxel_abyss_pack. Checked: "
        + " | ".join(str(path) for path in candidates)
    )


def main() -> None:
    build_script = _resolve_build_script()
    print(f"[VoxelAbyssLauncher] build script: {build_script}")

    module_name = "voxel_abyss_build_all_entry"
    spec = importlib.util.spec_from_file_location(module_name, build_script)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to load script spec: {build_script}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)

    if hasattr(module, "main"):
        module.main()


if __name__ == "__main__":
    main()
