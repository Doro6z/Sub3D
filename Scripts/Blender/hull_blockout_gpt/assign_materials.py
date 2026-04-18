"""
Sub3D - assign_materials.py (legacy entry point)
================================================
Thin wrapper around core.materials.assign_materials. The real palette and
classification logic live in core/materials.py; main.py calls it at the end of
its run so you rarely need to invoke this script manually. It exists for
backward compatibility with older Blender text-editor bookmarks.

Run: Blender > Scripting > Open > Alt+P
"""

import os
import sys
import importlib.util


def _load_materials_module():
    here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else r"C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt"
    if here not in sys.path:
        sys.path.append(here)
    core_dir = os.path.join(here, "core")
    if core_dir not in sys.path:
        sys.path.append(core_dir)

    candidate = os.path.join(core_dir, "materials.py")
    if not os.path.isfile(candidate):
        raise ModuleNotFoundError("core/materials.py not found next to assign_materials.py")
    module_name = "hull_blockout_gpt_materials"
    sys.modules.pop(module_name, None)
    spec = importlib.util.spec_from_file_location(module_name, candidate)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    _load_materials_module().assign_materials()


if __name__ == "__main__":
    main()
