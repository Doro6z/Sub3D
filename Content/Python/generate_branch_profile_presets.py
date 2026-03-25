import unreal


ROOT_DIR = "/Game/ProcGen/BranchProfiles"
PROFILE_DIR = f"{ROOT_DIR}/Profiles"
SET_DIR = f"{ROOT_DIR}/Sets"


def ensure_dir(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def save_asset(asset) -> None:
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False)


def make_data_asset(asset_name: str, package_path: str, asset_class):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)

    existing = unreal.EditorAssetLibrary.load_asset(f"{package_path}/{asset_name}")
    if existing:
        return existing

    return asset_tools.create_asset(asset_name, package_path, asset_class, factory)


def set_if_present(asset, name: str, value):
    try:
        asset.set_editor_property(name, value)
    except Exception as exc:
        unreal.log_warning(f"[BranchProfileGen] Could not set {asset.get_name()}.{name}: {exc}")


def branch_profile_configs():
    return [
        {
            "asset_name": "DA_BP_CanonicalBypass_4200",
            "profile_id": "CanonicalBypass4200",
            "display_name": "Canonical Bypass 4200",
            "intent": unreal.EBranchProfileIntent.CANONICAL_BYPASS,
            "canonical": True,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 4200.0,
            "length_scale": 1.05,
            "curvature_scale": 0.8,
            "verticality_scale": 0.8,
            "rejoin_chance": 0.9,
            "pocket_chance": 0.1,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": unreal.EBranchPlacementWindow.MID,
            "selection_weight": 1.0,
        },
        {
            "asset_name": "DA_BP_HubConnector_5200",
            "profile_id": "HubConnector5200",
            "display_name": "Hub Connector 5200",
            "intent": unreal.EBranchProfileIntent.HUB_CONNECTOR,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 5200.0,
            "length_scale": 0.95,
            "curvature_scale": 0.7,
            "verticality_scale": 0.7,
            "rejoin_chance": 0.95,
            "pocket_chance": 0.15,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": unreal.EBranchPlacementWindow.NEAR_HUB,
            "selection_weight": 1.0,
        },
        {
            "asset_name": "DA_BP_OptionalResourceDetour_3200",
            "profile_id": "OptionalResourceDetour3200",
            "display_name": "Optional Resource Detour 3200",
            "intent": unreal.EBranchProfileIntent.OPTIONAL_RESOURCE_DETOUR,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 3200.0,
            "length_scale": 1.2,
            "curvature_scale": 1.15,
            "verticality_scale": 0.95,
            "rejoin_chance": 0.45,
            "pocket_chance": 0.6,
            "secondary_split_chance": 0.15,
            "max_depth": 2,
            "preferred_window": unreal.EBranchPlacementWindow.ANYWHERE,
            "selection_weight": 1.2,
        },
        {
            "asset_name": "DA_BP_OptionalDangerBranch_2800",
            "profile_id": "OptionalDangerBranch2800",
            "display_name": "Optional Danger Branch 2800",
            "intent": unreal.EBranchProfileIntent.OPTIONAL_DANGER_BRANCH,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2800.0,
            "length_scale": 1.0,
            "curvature_scale": 1.35,
            "verticality_scale": 1.15,
            "rejoin_chance": 0.3,
            "pocket_chance": 0.55,
            "secondary_split_chance": 0.1,
            "max_depth": 2,
            "preferred_window": unreal.EBranchPlacementWindow.DEEP_SEGMENT,
            "selection_weight": 0.9,
        },
        {
            "asset_name": "DA_BP_PocketChain_2600",
            "profile_id": "PocketChain2600",
            "display_name": "Pocket Chain 2600",
            "intent": unreal.EBranchProfileIntent.POCKET_CHAIN,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2600.0,
            "length_scale": 0.85,
            "curvature_scale": 1.45,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.1,
            "pocket_chance": 0.9,
            "secondary_split_chance": 0.2,
            "max_depth": 3,
            "preferred_window": unreal.EBranchPlacementWindow.LATE,
            "selection_weight": 0.75,
        },
        {
            "asset_name": "DA_BP_CheckpointSideBranch_3000",
            "profile_id": "CheckpointSideBranch3000",
            "display_name": "Checkpoint Side Branch 3000",
            "intent": unreal.EBranchProfileIntent.CHECKPOINT_SIDE_BRANCH,
            "canonical": False,
            "optional": True,
            "near_checkpoint": True,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 0.8,
            "curvature_scale": 0.9,
            "verticality_scale": 0.6,
            "rejoin_chance": 0.2,
            "pocket_chance": 0.55,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": unreal.EBranchPlacementWindow.NEAR_CHECKPOINT,
            "selection_weight": 0.5,
        },
        {
            "asset_name": "DA_BP_TightShortcut_2400",
            "profile_id": "TightShortcut2400",
            "display_name": "Tight Shortcut 2400",
            "intent": unreal.EBranchProfileIntent.OPTIONAL_DANGER_BRANCH,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2400.0,
            "length_scale": 0.9,
            "curvature_scale": 1.6,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.25,
            "pocket_chance": 0.5,
            "secondary_split_chance": 0.1,
            "max_depth": 2,
            "preferred_window": unreal.EBranchPlacementWindow.MID,
            "selection_weight": 0.8,
        },
        {
            "asset_name": "DA_BP_WideCavernLink_6200",
            "profile_id": "WideCavernLink6200",
            "display_name": "Wide Cavern Link 6200",
            "intent": unreal.EBranchProfileIntent.HUB_CONNECTOR,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 6200.0,
            "length_scale": 1.05,
            "curvature_scale": 0.65,
            "verticality_scale": 0.7,
            "rejoin_chance": 0.85,
            "pocket_chance": 0.2,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": unreal.EBranchPlacementWindow.NEAR_HUB,
            "selection_weight": 0.95,
        },
        {
            "asset_name": "DA_BP_VerticalLift_3000",
            "profile_id": "VerticalLift3000",
            "display_name": "Vertical Lift 3000",
            "intent": unreal.EBranchProfileIntent.OPTIONAL_RESOURCE_DETOUR,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 1.1,
            "curvature_scale": 1.1,
            "verticality_scale": 1.8,
            "rejoin_chance": 0.4,
            "pocket_chance": 0.45,
            "secondary_split_chance": 0.05,
            "max_depth": 2,
            "preferred_window": unreal.EBranchPlacementWindow.MID,
            "selection_weight": 0.9,
        },
        {
            "asset_name": "DA_BP_LabyrinthWeave_3000",
            "profile_id": "LabyrinthWeave3000",
            "display_name": "Labyrinth Weave 3000",
            "intent": unreal.EBranchProfileIntent.POCKET_CHAIN,
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 1.25,
            "curvature_scale": 1.5,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.25,
            "pocket_chance": 0.8,
            "secondary_split_chance": 0.25,
            "max_depth": 3,
            "preferred_window": unreal.EBranchPlacementWindow.LATE,
            "selection_weight": 1.0,
        },
    ]


def branch_profile_set_configs():
    return [
        {
            "asset_name": "DA_BranchProfileSet_DefaultTraversal_01",
            "profile_set_id": "Traversal_Default_01",
            "profiles": [
                "CanonicalBypass4200",
                "HubConnector5200",
                "OptionalResourceDetour3200",
                "OptionalDangerBranch2800",
                "PocketChain2600",
                "CheckpointSideBranch3000",
            ],
            "simple_selection_bias": 1.0,
            "moderate_selection_bias": 1.0,
            "dense_selection_bias": 1.0,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 6,
            "max_pocket_chain_profiles": 3,
        },
        {
            "asset_name": "DA_BranchProfileSet_Tight_01",
            "profile_set_id": "Traversal_Tight_01",
            "profiles": [
                "TightShortcut2400",
                "OptionalDangerBranch2800",
                "PocketChain2600",
                "CheckpointSideBranch3000",
            ],
            "simple_selection_bias": 0.9,
            "moderate_selection_bias": 1.1,
            "dense_selection_bias": 1.25,
            "max_canonical_bypass_profiles": 1,
            "max_optional_profiles": 6,
            "max_pocket_chain_profiles": 4,
        },
        {
            "asset_name": "DA_BranchProfileSet_WideHub_01",
            "profile_set_id": "Traversal_WideHub_01",
            "profiles": [
                "CanonicalBypass4200",
                "HubConnector5200",
                "WideCavernLink6200",
                "OptionalResourceDetour3200",
            ],
            "simple_selection_bias": 1.15,
            "moderate_selection_bias": 1.0,
            "dense_selection_bias": 0.95,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 5,
            "max_pocket_chain_profiles": 1,
        },
        {
            "asset_name": "DA_BranchProfileSet_Vertical_01",
            "profile_set_id": "Traversal_Vertical_01",
            "profiles": [
                "CanonicalBypass4200",
                "VerticalLift3000",
                "OptionalDangerBranch2800",
                "HubConnector5200",
            ],
            "simple_selection_bias": 0.95,
            "moderate_selection_bias": 1.05,
            "dense_selection_bias": 1.15,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 5,
            "max_pocket_chain_profiles": 2,
        },
        {
            "asset_name": "DA_BranchProfileSet_LabyrinthLite_01",
            "profile_set_id": "Traversal_LabyrinthLite_01",
            "profiles": [
                "OptionalResourceDetour3200",
                "PocketChain2600",
                "LabyrinthWeave3000",
                "CheckpointSideBranch3000",
                "TightShortcut2400",
            ],
            "simple_selection_bias": 0.8,
            "moderate_selection_bias": 1.05,
            "dense_selection_bias": 1.3,
            "max_canonical_bypass_profiles": 1,
            "max_optional_profiles": 8,
            "max_pocket_chain_profiles": 5,
        },
    ]


def create_branch_profile(package_path: str, asset_name: str, config: dict):
    asset = make_data_asset(asset_name, package_path, unreal.BranchProfileDataAsset)

    set_if_present(asset, "profile_id", config["profile_id"])
    set_if_present(asset, "schema_version", 1)
    set_if_present(asset, "display_name", unreal.Text(config["display_name"]))

    set_if_present(asset, "intent", config["intent"])
    set_if_present(asset, "allowed_on_canonical_route", config.get("canonical", False))
    set_if_present(asset, "allowed_on_optional_route", config.get("optional", True))
    set_if_present(asset, "allowed_near_checkpoint", config.get("near_checkpoint", False))
    set_if_present(asset, "allowed_near_hub", config.get("near_hub", True))

    set_if_present(asset, "target_radius_cm", config["radius_cm"])
    set_if_present(asset, "length_scale", config.get("length_scale", 1.0))
    set_if_present(asset, "curvature_scale", config.get("curvature_scale", 1.0))
    set_if_present(asset, "verticality_scale", config.get("verticality_scale", 1.0))
    set_if_present(asset, "rejoin_chance", config.get("rejoin_chance", 0.55))
    set_if_present(asset, "pocket_chance", config.get("pocket_chance", 0.45))
    set_if_present(asset, "secondary_split_chance", config.get("secondary_split_chance", 0.0))
    set_if_present(asset, "max_depth", config.get("max_depth", 2))

    set_if_present(asset, "preferred_window", config.get("preferred_window", unreal.EBranchPlacementWindow.ANYWHERE))
    set_if_present(asset, "selection_weight", config.get("selection_weight", 1.0))

    save_asset(asset)
    unreal.log(f"[BranchProfileGen] Created/updated {asset.get_path_name()}")
    return asset


def create_profile_set(package_path: str, asset_name: str, config: dict, created_profiles: dict):
    asset = make_data_asset(asset_name, package_path, unreal.BranchProfileSetDataAsset)

    set_if_present(asset, "profile_set_id", config["profile_set_id"])
    set_if_present(asset, "schema_version", 1)
    set_if_present(asset, "profiles", [created_profiles[profile_id] for profile_id in config["profiles"]])
    set_if_present(asset, "simple_selection_bias", config.get("simple_selection_bias", 1.0))
    set_if_present(asset, "moderate_selection_bias", config.get("moderate_selection_bias", 1.0))
    set_if_present(asset, "dense_selection_bias", config.get("dense_selection_bias", 1.0))
    set_if_present(asset, "max_canonical_bypass_profiles", config.get("max_canonical_bypass_profiles", 2))
    set_if_present(asset, "max_optional_profiles", config.get("max_optional_profiles", 8))
    set_if_present(asset, "max_pocket_chain_profiles", config.get("max_pocket_chain_profiles", 4))

    save_asset(asset)
    unreal.log(f"[BranchProfileGen] Created/updated {asset.get_path_name()}")
    return asset


def main():
    ensure_dir(ROOT_DIR)
    ensure_dir(PROFILE_DIR)
    ensure_dir(SET_DIR)

    created_profiles = {}
    for config in branch_profile_configs():
        asset_name = config["asset_name"]
        created_profiles[config["profile_id"]] = create_branch_profile(PROFILE_DIR, asset_name, config)

    for config in branch_profile_set_configs():
        create_profile_set(SET_DIR, config["asset_name"], config, created_profiles)

    unreal.log("[BranchProfileGen] Done. Created profiles and themed profile sets.")


if __name__ == "__main__":
    main()
