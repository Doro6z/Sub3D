"""
Sub3D - Build All Voxel Abyss Assets
====================================

Runs the complete abyssal voxel pack and lays each family into a review grid.
"""

from __future__ import annotations

import importlib.util
import sys
import traceback
from pathlib import Path

def _resolve_this_dir() -> Path:
    candidates = []
    checked = []

    file_value = globals().get("__file__")
    if file_value:
        candidates.append(Path(file_value).resolve().parent)

    try:
        import bpy

        for text in (
            getattr(bpy.context, "edit_text", None),
            getattr(getattr(bpy.context, "space_data", None), "text", None),
        ):
            filepath = getattr(text, "filepath", "")
            if filepath:
                candidates.append(Path(bpy.path.abspath(filepath)).resolve().parent)
    except Exception:
        pass

    # Local search from current working directory toward filesystem root.
    cwd = Path.cwd().resolve()
    for base in (cwd, *cwd.parents):
        candidates.append(base / "Scripts" / "Blender" / "voxel_abyss_pack")

    # Known workspace fallback for Sub3D.
    candidates.append(Path(r"C:\Dev\Sub3D\Scripts\Blender\voxel_abyss_pack"))

    for candidate in candidates:
        checked.append(str(candidate))
        if (candidate / "core.py").exists() and (candidate / "pack_manifest.py").exists():
            return candidate

    raise RuntimeError(
        "Unable to resolve voxel_abyss_pack directory. Checked: "
        + " | ".join(checked)
    )


THIS_DIR = _resolve_this_dir()
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))


def _load_local_module(module_name: str, filename: str):
    module_path = THIS_DIR / filename
    existing = sys.modules.get(module_name)
    if existing is not None:
        existing_file = getattr(existing, "__file__", None)
        if existing_file:
            try:
                if Path(existing_file).resolve() == module_path.resolve():
                    return existing
            except Exception:
                pass
        # Evict foreign module with same name (e.g. Blender/plugin "core").
        sys.modules.pop(module_name, None)

    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise ModuleNotFoundError(f"Unable to load local module '{module_name}' from '{module_path}'")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


core = _load_local_module("core", "core.py")
pack_manifest = _load_local_module("pack_manifest", "pack_manifest.py")

if not hasattr(core, "ensure_collection"):
    raise RuntimeError(
        "Invalid core module loaded. "
        f"module_file={getattr(core, '__file__', None)} "
        f"module_name={getattr(core, '__name__', None)} "
        f"available_attrs_sample={sorted([name for name in dir(core) if not name.startswith('_')])[:20]}"
    )

ensure_collection = core.ensure_collection
ensure_scene = core.ensure_scene
focus_collection = core.focus_collection
grid_origin = core.grid_origin
SCRIPT_SPECS = pack_manifest.SCRIPT_SPECS
total_asset_count = pack_manifest.total_asset_count

FAMILY_COLUMNS = 3
FAMILY_STEP_X = 70000.0
FAMILY_STEP_Y = 65000.0


def main() -> None:
    print("\n" + "=" * 72)
    print("Sub3D - Voxel Abyss Pack")
    print("=" * 72)
    print(f"PackDir: {THIS_DIR}")
    print(f"Scripts: {len(SCRIPT_SPECS)}")
    print(f"Assets:  {total_asset_count()}")

    ensure_scene(clear_scene=True)
    root = ensure_collection("VX_AbyssPack_All")

    built_total = 0
    for index, spec in enumerate(SCRIPT_SPECS):
        try:
            module = _load_local_module(spec["module"], f"{spec['module']}.py")
            if not hasattr(module, "build_assets"):
                raise RuntimeError(
                    f"Module '{spec['module']}' loaded from '{getattr(module, '__file__', None)}' "
                    "does not expose build_assets()."
                )
            origin = grid_origin(index, columns=FAMILY_COLUMNS, step_x=FAMILY_STEP_X, step_y=FAMILY_STEP_Y)
            built = module.build_assets(parent_collection=root, clear_scene=False, origin=origin)
            built_total += len(built)
            print(f"  {spec['title']}: {len(built)} assets")
        except Exception:
            print(f"\n[BuildAll] Failure in module '{spec['module']}'")
            print(traceback.format_exc())
            raise

    focus_collection(root)
    print(f"\nBuilt {built_total} assets.")
    print("=" * 72)


if __name__ == "__main__":
    main()
