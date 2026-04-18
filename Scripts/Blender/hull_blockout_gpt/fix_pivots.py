"""
Sub3D - fix_pivots.py (legacy entry point)
==========================================
Now a thin wrapper around core.pivots.fix_all_pivots so existing Blender text
editor bookmarks keep working. The fused logic lives in core/pivots.py and is
also called automatically from main.py at the end of the blockout run.

Run: Blender > Scripting > Open > Alt+P
"""

import os
import sys
import importlib.util


def _load_pivots_module():
    here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else r"C:\Dev\Sub3D\Scripts\Blender\hull_blockout_gpt"
    if here not in sys.path:
        sys.path.append(here)
    core_dir = os.path.join(here, "core")
    if core_dir not in sys.path:
        sys.path.append(core_dir)

    candidate = os.path.join(core_dir, "pivots.py")
    if not os.path.isfile(candidate):
        raise ModuleNotFoundError("core/pivots.py not found next to fix_pivots.py")
    module_name = "hull_blockout_gpt_pivots"
    sys.modules.pop(module_name, None)
    spec = importlib.util.spec_from_file_location(module_name, candidate)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    _load_pivots_module().fix_all_pivots()


if __name__ == "__main__":
    main()
