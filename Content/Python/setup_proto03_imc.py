import unreal


ROOT = "/Game/Sub3D/Input"

IA_MOVE = f"{ROOT}/IA_Move"
IA_LOOK = f"{ROOT}/IA_Look"
IA_INTERACT = f"{ROOT}/IA_Interact"
IA_THRUST = f"{ROOT}/IA_Thrust"
IA_RUDDER = f"{ROOT}/IA_Rudder"
IA_DIVE = f"{ROOT}/IA_DivePlane"
IA_EXIT_STATION = f"{ROOT}/IA_ExitStation"

IMC_CREW = f"{ROOT}/IMC_Crew"
IMC_ON_FOOT = f"{ROOT}/IMC_OnFoot"
IMC_HELM = f"{ROOT}/IMC_Helm"
IMC_STATION_UI = f"{ROOT}/IMC_StationUI"


def log(msg: str) -> None:
    unreal.log(f"[Proto03-IMC] {msg}")


def warn(msg: str) -> None:
    unreal.log_warning(f"[Proto03-IMC] {msg}")


def ensure_dir(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        log(f"Created directory: {path}")


def load(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


def save(path: str) -> None:
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)


def create_asset_if_missing(asset_name: str, package_path: str, asset_class, factory):
    full_path = f"{package_path}/{asset_name}"
    existing = load(full_path)
    if existing:
        return existing

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = tools.create_asset(asset_name, package_path, asset_class, factory)
    log(f"Created asset: {full_path}")
    return created


def duplicate_if_missing(source_path: str, target_path: str):
    existing = load(target_path)
    if existing:
        return existing

    if not unreal.EditorAssetLibrary.does_asset_exist(source_path):
        warn(f"Cannot duplicate. Missing source: {source_path}")
        return None

    ok = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not ok:
        warn(f"Duplicate failed: {source_path} -> {target_path}")
        return None

    log(f"Duplicated asset: {target_path}")
    return load(target_path)


def ensure_input_action(path: str, value_type=None):
    package_path, asset_name = path.rsplit("/", 1)
    ia = create_asset_if_missing(
        asset_name,
        package_path,
        unreal.InputAction,
        unreal.InputActionFactory(),
    )
    if value_type is not None:
        try:
            ia.set_editor_property("value_type", value_type)
        except Exception as exc:
            warn(f"Could not set value_type for {path}: {exc}")
    save(path)
    return ia


def ensure_imc(path: str):
    package_path, asset_name = path.rsplit("/", 1)
    imc = create_asset_if_missing(
        asset_name,
        package_path,
        unreal.InputMappingContext,
        unreal.InputMappingContextFactory(),
    )
    save(path)
    return imc


def clear_mappings(imc):
    # Robust clear across UE versions.
    if hasattr(imc, "unmap_all"):
        imc.unmap_all()
        return

    if hasattr(imc, "get_mappings") and hasattr(imc, "unmap_key"):
        for mapping in list(imc.get_mappings()):
            try:
                action = mapping.get_editor_property("action")
                key = mapping.get_editor_property("key")
                imc.unmap_key(action, key)
            except Exception:
                pass


def add_negate_modifier(mapping):
    try:
        modifiers = list(mapping.get_editor_property("modifiers"))
        modifiers.append(unreal.InputModifierNegate())
        mapping.set_editor_property("modifiers", modifiers)
    except Exception as exc:
        warn(f"Could not add negate modifier: {exc}")


def map_key(imc, action, key, negate=False):
    if not imc or not action or not key:
        return
    try:
        mapping = imc.map_key(action, key)
        if negate:
            add_negate_modifier(mapping)
    except Exception as exc:
        warn(f"Map failed ({action.get_name()} -> {key}): {exc}")


def build_on_foot_imc():
    # Keep your existing walking setup/modifiers by duplicating IMC_Crew.
    on_foot = duplicate_if_missing(IMC_CREW, IMC_ON_FOOT)
    if not on_foot:
        warn("IMC_OnFoot not created from IMC_Crew. Creating empty IMC_OnFoot fallback.")
        on_foot = ensure_imc(IMC_ON_FOOT)
    save(IMC_ON_FOOT)


def build_helm_imc():
    imc = ensure_imc(IMC_HELM)
    clear_mappings(imc)

    ia_thrust = load(IA_THRUST)
    ia_rudder = load(IA_RUDDER)
    ia_dive = load(IA_DIVE)

    # Thrust axis: forward and backward (supports AZERTY + QWERTY habits).
    map_key(imc, ia_thrust, unreal.Keys.W, negate=False)
    map_key(imc, ia_thrust, unreal.Keys.Z, negate=False)
    map_key(imc, ia_thrust, unreal.Keys.S, negate=True)

    # Rudder axis: right and left (AZERTY + QWERTY).
    map_key(imc, ia_rudder, unreal.Keys.D, negate=False)
    map_key(imc, ia_rudder, unreal.Keys.A, negate=True)
    map_key(imc, ia_rudder, unreal.Keys.Q, negate=True)

    # Dive plane axis (optional defaults).
    map_key(imc, ia_dive, unreal.Keys.R, negate=False)
    map_key(imc, ia_dive, unreal.Keys.F, negate=True)

    save(IMC_HELM)
    log("Configured IMC_Helm")


def build_station_ui_imc():
    # Ensure a dedicated exit action for station UI.
    ia_exit = ensure_input_action(IA_EXIT_STATION, unreal.InputActionValueType.BOOLEAN)
    ia_interact = load(IA_INTERACT)

    imc = ensure_imc(IMC_STATION_UI)
    clear_mappings(imc)

    map_key(imc, ia_exit, unreal.Keys.Escape, negate=False)
    map_key(imc, ia_interact, unreal.Keys.E, negate=False)

    save(IMC_STATION_UI)
    save(IA_EXIT_STATION)
    log("Configured IMC_StationUI")


def verify_required_assets():
    required = [IA_THRUST, IA_RUDDER, IA_DIVE, IA_INTERACT, IMC_CREW]
    missing = [path for path in required if not unreal.EditorAssetLibrary.does_asset_exist(path)]
    if missing:
        for path in missing:
            warn(f"Missing required asset: {path}")
        return False
    return True


def main():
    ensure_dir(ROOT)

    if not verify_required_assets():
        warn("Aborted. Create missing base assets first.")
        return

    build_on_foot_imc()
    build_helm_imc()
    build_station_ui_imc()

    unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    log("Done. IMC_OnFoot, IMC_Helm, IMC_StationUI are ready.")


if __name__ == "__main__":
    main()
