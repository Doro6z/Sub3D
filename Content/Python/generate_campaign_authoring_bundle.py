import unreal


ROOT_DIR = "/Game/ProcGen/AutoAuthoring"
PROFILE_DIR = f"{ROOT_DIR}/BranchProfiles/Profiles"
SET_DIR = f"{ROOT_DIR}/BranchProfiles/Sets"
ARCHETYPE_DIR = f"{ROOT_DIR}/RouteArchetypes"
BIOME_DIR = f"{ROOT_DIR}/Biomes"
CAMPAIGN_DIR = f"{ROOT_DIR}/Campaigns"


def enum_value(type_names, member_names, fallback_int: int):
    if isinstance(type_names, str):
        type_names = [type_names]
    if isinstance(member_names, str):
        member_names = [member_names]

    candidate_member_names = []
    for member_name in member_names:
        candidate_member_names.extend(
            [
                member_name,
                member_name.upper(),
                member_name.lower(),
                member_name.replace(" ", "_"),
                member_name.replace(" ", "_").upper(),
                member_name.replace(" ", "_").lower(),
            ]
        )

    for type_name in type_names:
        enum_type = getattr(unreal, type_name, None)
        if enum_type is None:
            continue
        for candidate in candidate_member_names:
            if hasattr(enum_type, candidate):
                return getattr(enum_type, candidate)

    return fallback_int


BRANCH_INTENT = {
    "CANONICAL_BYPASS": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["CANONICAL_BYPASS", "CanonicalBypass"], 0),
    "OPTIONAL_RESOURCE_DETOUR": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["OPTIONAL_RESOURCE_DETOUR", "OptionalResourceDetour"], 1),
    "OPTIONAL_DANGER_BRANCH": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["OPTIONAL_DANGER_BRANCH", "OptionalDangerBranch"], 2),
    "HUB_CONNECTOR": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["HUB_CONNECTOR", "HubConnector"], 3),
    "POCKET_CHAIN": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["POCKET_CHAIN", "PocketChain"], 4),
    "CHECKPOINT_SIDE_BRANCH": enum_value(["EBranchProfileIntent", "BranchProfileIntent"], ["CHECKPOINT_SIDE_BRANCH", "CheckpointSideBranch"], 5),
}

BRANCH_WINDOW = {
    "ANYWHERE": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["ANYWHERE", "Anywhere"], 0),
    "START_THIRD": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["START_THIRD", "StartThird"], 1),
    "MID": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["MID", "Mid"], 2),
    "LATE": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["LATE", "Late"], 3),
    "NEAR_HUB": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["NEAR_HUB", "NearHub"], 4),
    "NEAR_CHECKPOINT": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["NEAR_CHECKPOINT", "NearCheckpoint"], 5),
    "DEEP_SEGMENT": enum_value(["EBranchPlacementWindow", "BranchPlacementWindow"], ["DEEP_SEGMENT", "DeepSegment"], 6),
}

ARCHETYPE_TYPE = {
    "MAIN_TRANSIT": enum_value(["ETraversalRouteArchetype", "TraversalRouteArchetype"], ["MAIN_TRANSIT", "MainTransit"], 0),
    "STEALTH_PASSAGE": enum_value(["ETraversalRouteArchetype", "TraversalRouteArchetype"], ["STEALTH_PASSAGE", "StealthPassage"], 1),
    "COMBAT_CORRIDOR": enum_value(["ETraversalRouteArchetype", "TraversalRouteArchetype"], ["COMBAT_CORRIDOR", "CombatCorridor"], 2),
    "DEEP_DESCENT": enum_value(["ETraversalRouteArchetype", "TraversalRouteArchetype"], ["DEEP_DESCENT", "DeepDescent"], 3),
    "SALVAGE_ROUTE": enum_value(["ETraversalRouteArchetype", "TraversalRouteArchetype"], ["SALVAGE_ROUTE", "SalvageRoute"], 4),
}

COMPLEXITY_TIER = {
    "SIMPLE": enum_value(["ETraversalComplexityTier", "TraversalComplexityTier"], ["SIMPLE", "Simple"], 0),
    "MODERATE": enum_value(["ETraversalComplexityTier", "TraversalComplexityTier"], ["MODERATE", "Moderate"], 1),
    "DENSE": enum_value(["ETraversalComplexityTier", "TraversalComplexityTier"], ["DENSE", "Dense"], 2),
}

CHECKPOINT_SHAPE = {
    "GOULOT": enum_value(["ECheckpointSpaceShape", "CheckpointSpaceShape"], ["GOULOT", "Goulot"], 0),
    "POCKET": enum_value(["ECheckpointSpaceShape", "CheckpointSpaceShape"], ["POCKET", "Pocket"], 1),
    "LARGE_CAVITY": enum_value(["ECheckpointSpaceShape", "CheckpointSpaceShape"], ["LARGE_CAVITY", "LargeCavity"], 2),
}

CAMPAIGN_ROLE = {
    "ENTRY": enum_value(["ECampaignSegmentRole", "CampaignSegmentRole"], ["ENTRY", "Entry"], 0),
    "MAIN": enum_value(["ECampaignSegmentRole", "CampaignSegmentRole"], ["MAIN", "Main"], 1),
    "SIDE_BRANCH": enum_value(["ECampaignSegmentRole", "CampaignSegmentRole"], ["SIDE_BRANCH", "SideBranch"], 2),
    "EXIT": enum_value(["ECampaignSegmentRole", "CampaignSegmentRole"], ["EXIT", "Exit"], 3),
    "CONNECTOR": enum_value(["ECampaignSegmentRole", "CampaignSegmentRole"], ["CONNECTOR", "Connector"], 4),
}


def ensure_dir(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def save_asset(asset) -> None:
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False)


def make_data_asset(asset_name: str, package_path: str, asset_class):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)

    asset_path = f"{package_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(asset_path)
        if existing:
            return existing

    return asset_tools.create_asset(asset_name, package_path, asset_class, factory)


def set_if_present(obj, name: str, value) -> None:
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        unreal.log_warning(f"[CampaignBundleGen] Could not set {obj}.{name}: {exc}")


def apply_struct_settings(asset, property_name: str, values: dict) -> None:
    struct_value = asset.get_editor_property(property_name)
    for key, value in values.items():
        set_if_present(struct_value, key, value)
    set_if_present(asset, property_name, struct_value)


def find_first_existing_asset(paths):
    for path in paths:
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                asset = unreal.EditorAssetLibrary.load_asset(path)
                if asset:
                    return asset
        except Exception:
            continue
    return None


def make_text(value: str):
    return unreal.Text(value)


def branch_profile_configs():
    return [
        {
            "asset_name": "DA_BP_Auto_CanonicalBypass_4200",
            "profile_id": "Auto_CanonicalBypass_4200",
            "display_name": "Auto Canonical Bypass 4200",
            "intent": BRANCH_INTENT["CANONICAL_BYPASS"],
            "canonical": True,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 4200.0,
            "length_scale": 1.05,
            "curvature_scale": 0.85,
            "verticality_scale": 0.85,
            "rejoin_chance": 0.92,
            "pocket_chance": 0.08,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": BRANCH_WINDOW["MID"],
            "selection_weight": 1.0,
        },
        {
            "asset_name": "DA_BP_Auto_HubConnector_5600",
            "profile_id": "Auto_HubConnector_5600",
            "display_name": "Auto Hub Connector 5600",
            "intent": BRANCH_INTENT["HUB_CONNECTOR"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 5600.0,
            "length_scale": 0.95,
            "curvature_scale": 0.65,
            "verticality_scale": 0.65,
            "rejoin_chance": 0.95,
            "pocket_chance": 0.12,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": BRANCH_WINDOW["NEAR_HUB"],
            "selection_weight": 1.0,
        },
        {
            "asset_name": "DA_BP_Auto_ResourceDetour_3200",
            "profile_id": "Auto_ResourceDetour_3200",
            "display_name": "Auto Resource Detour 3200",
            "intent": BRANCH_INTENT["OPTIONAL_RESOURCE_DETOUR"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 3200.0,
            "length_scale": 1.2,
            "curvature_scale": 1.15,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.48,
            "pocket_chance": 0.62,
            "secondary_split_chance": 0.15,
            "max_depth": 2,
            "preferred_window": BRANCH_WINDOW["ANYWHERE"],
            "selection_weight": 1.15,
        },
        {
            "asset_name": "DA_BP_Auto_DangerBranch_2800",
            "profile_id": "Auto_DangerBranch_2800",
            "display_name": "Auto Danger Branch 2800",
            "intent": BRANCH_INTENT["OPTIONAL_DANGER_BRANCH"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2800.0,
            "length_scale": 1.0,
            "curvature_scale": 1.35,
            "verticality_scale": 1.2,
            "rejoin_chance": 0.32,
            "pocket_chance": 0.58,
            "secondary_split_chance": 0.08,
            "max_depth": 2,
            "preferred_window": BRANCH_WINDOW["DEEP_SEGMENT"],
            "selection_weight": 0.9,
        },
        {
            "asset_name": "DA_BP_Auto_PocketChain_2600",
            "profile_id": "Auto_PocketChain_2600",
            "display_name": "Auto Pocket Chain 2600",
            "intent": BRANCH_INTENT["POCKET_CHAIN"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2600.0,
            "length_scale": 0.9,
            "curvature_scale": 1.5,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.12,
            "pocket_chance": 0.9,
            "secondary_split_chance": 0.22,
            "max_depth": 3,
            "preferred_window": BRANCH_WINDOW["LATE"],
            "selection_weight": 0.8,
        },
        {
            "asset_name": "DA_BP_Auto_CheckpointSide_3000",
            "profile_id": "Auto_CheckpointSide_3000",
            "display_name": "Auto Checkpoint Side 3000",
            "intent": BRANCH_INTENT["CHECKPOINT_SIDE_BRANCH"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": True,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 0.8,
            "curvature_scale": 0.9,
            "verticality_scale": 0.6,
            "rejoin_chance": 0.2,
            "pocket_chance": 0.5,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": BRANCH_WINDOW["NEAR_CHECKPOINT"],
            "selection_weight": 0.55,
        },
        {
            "asset_name": "DA_BP_Auto_TightShortcut_2400",
            "profile_id": "Auto_TightShortcut_2400",
            "display_name": "Auto Tight Shortcut 2400",
            "intent": BRANCH_INTENT["OPTIONAL_DANGER_BRANCH"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 2400.0,
            "length_scale": 0.85,
            "curvature_scale": 1.55,
            "verticality_scale": 0.95,
            "rejoin_chance": 0.26,
            "pocket_chance": 0.52,
            "secondary_split_chance": 0.1,
            "max_depth": 2,
            "preferred_window": BRANCH_WINDOW["MID"],
            "selection_weight": 0.8,
        },
        {
            "asset_name": "DA_BP_Auto_WideCavernLink_6400",
            "profile_id": "Auto_WideCavernLink_6400",
            "display_name": "Auto Wide Cavern Link 6400",
            "intent": BRANCH_INTENT["HUB_CONNECTOR"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": True,
            "radius_cm": 6400.0,
            "length_scale": 1.05,
            "curvature_scale": 0.6,
            "verticality_scale": 0.7,
            "rejoin_chance": 0.88,
            "pocket_chance": 0.18,
            "secondary_split_chance": 0.0,
            "max_depth": 1,
            "preferred_window": BRANCH_WINDOW["NEAR_HUB"],
            "selection_weight": 0.95,
        },
        {
            "asset_name": "DA_BP_Auto_VerticalLift_3000",
            "profile_id": "Auto_VerticalLift_3000",
            "display_name": "Auto Vertical Lift 3000",
            "intent": BRANCH_INTENT["OPTIONAL_RESOURCE_DETOUR"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 1.1,
            "curvature_scale": 1.1,
            "verticality_scale": 1.8,
            "rejoin_chance": 0.42,
            "pocket_chance": 0.4,
            "secondary_split_chance": 0.05,
            "max_depth": 2,
            "preferred_window": BRANCH_WINDOW["MID"],
            "selection_weight": 0.9,
        },
        {
            "asset_name": "DA_BP_Auto_LabyrinthWeave_3000",
            "profile_id": "Auto_LabyrinthWeave_3000",
            "display_name": "Auto Labyrinth Weave 3000",
            "intent": BRANCH_INTENT["POCKET_CHAIN"],
            "canonical": False,
            "optional": True,
            "near_checkpoint": False,
            "near_hub": False,
            "radius_cm": 3000.0,
            "length_scale": 1.25,
            "curvature_scale": 1.55,
            "verticality_scale": 1.0,
            "rejoin_chance": 0.22,
            "pocket_chance": 0.82,
            "secondary_split_chance": 0.25,
            "max_depth": 3,
            "preferred_window": BRANCH_WINDOW["LATE"],
            "selection_weight": 1.0,
        },
    ]


def branch_profile_set_configs():
    return [
        {
            "asset_name": "DA_BranchProfileSet_Auto_DefaultTraversal_01",
            "profile_set_id": "Auto_DefaultTraversal_01",
            "profiles": [
                "Auto_CanonicalBypass_4200",
                "Auto_HubConnector_5600",
                "Auto_ResourceDetour_3200",
                "Auto_DangerBranch_2800",
                "Auto_PocketChain_2600",
                "Auto_CheckpointSide_3000",
            ],
            "simple_selection_bias": 1.0,
            "moderate_selection_bias": 1.0,
            "dense_selection_bias": 1.0,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 6,
            "max_pocket_chain_profiles": 3,
        },
        {
            "asset_name": "DA_BranchProfileSet_Auto_Tight_01",
            "profile_set_id": "Auto_Tight_01",
            "profiles": [
                "Auto_TightShortcut_2400",
                "Auto_DangerBranch_2800",
                "Auto_PocketChain_2600",
                "Auto_CheckpointSide_3000",
            ],
            "simple_selection_bias": 0.9,
            "moderate_selection_bias": 1.1,
            "dense_selection_bias": 1.25,
            "max_canonical_bypass_profiles": 1,
            "max_optional_profiles": 6,
            "max_pocket_chain_profiles": 4,
        },
        {
            "asset_name": "DA_BranchProfileSet_Auto_WideHub_01",
            "profile_set_id": "Auto_WideHub_01",
            "profiles": [
                "Auto_CanonicalBypass_4200",
                "Auto_HubConnector_5600",
                "Auto_WideCavernLink_6400",
                "Auto_ResourceDetour_3200",
            ],
            "simple_selection_bias": 1.15,
            "moderate_selection_bias": 1.0,
            "dense_selection_bias": 0.95,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 5,
            "max_pocket_chain_profiles": 1,
        },
        {
            "asset_name": "DA_BranchProfileSet_Auto_Vertical_01",
            "profile_set_id": "Auto_Vertical_01",
            "profiles": [
                "Auto_CanonicalBypass_4200",
                "Auto_VerticalLift_3000",
                "Auto_DangerBranch_2800",
                "Auto_HubConnector_5600",
            ],
            "simple_selection_bias": 0.95,
            "moderate_selection_bias": 1.05,
            "dense_selection_bias": 1.15,
            "max_canonical_bypass_profiles": 2,
            "max_optional_profiles": 5,
            "max_pocket_chain_profiles": 2,
        },
        {
            "asset_name": "DA_BranchProfileSet_Auto_LabyrinthLite_01",
            "profile_set_id": "Auto_LabyrinthLite_01",
            "profiles": [
                "Auto_ResourceDetour_3200",
                "Auto_PocketChain_2600",
                "Auto_LabyrinthWeave_3000",
                "Auto_CheckpointSide_3000",
                "Auto_TightShortcut_2400",
            ],
            "simple_selection_bias": 0.8,
            "moderate_selection_bias": 1.05,
            "dense_selection_bias": 1.3,
            "max_canonical_bypass_profiles": 1,
            "max_optional_profiles": 8,
            "max_pocket_chain_profiles": 5,
        },
    ]


def route_archetype_configs(profile_sets):
    return [
        {
            "asset_name": "DA_RouteArchetype_Auto_WideHub",
            "archetype_id": "Auto_WideHub",
            "archetype_type": ARCHETYPE_TYPE["MAIN_TRANSIT"],
            "branch_profile_set": profile_sets["Auto_WideHub_01"],
            "complexity_tier": COMPLEXITY_TIER["MODERATE"],
            "checkpoints": {
                "start_shape": CHECKPOINT_SHAPE["GOULOT"],
                "end_shape": CHECKPOINT_SHAPE["LARGE_CAVITY"],
                "start_radius_cm": 4200.0,
                "end_radius_cm": 7200.0,
            },
            "connections": {
                "start_dock_length_cm": 3400.0,
                "end_dock_length_cm": 4200.0,
                "start_dock_radius_scale": 0.82,
                "end_dock_radius_scale": 0.88,
                "preferred_campaign_overlap_cm": 1800.0,
            },
            "flow": {
                "main_spine_node_count": 11,
                "max_split_anchors": 3,
                "max_branch_fanout": 4,
                "rejoin_chance": 0.6,
            },
            "complexity": {
                "max_hub_count": 3,
                "max_optional_side_branches": 4,
                "max_pocket_depth": 2,
            },
            "volumes": {
                "trunk_radius_cm": 4300.0,
                "branch_a_radius_cm": 3400.0,
                "branch_b_radius_cm": 4200.0,
                "branch_c_radius_cm": 3800.0,
                "merge_radius_cm": 4600.0,
                "hub_radius_cm": 7600.0,
                "pocket_radius_cm": 3400.0,
                "junction_transition_start_t": 0.86,
                "junction_transition_span_t": 0.12,
                "junction_throat_scale": 1.02,
            },
            "spatial_shape": {
                "branch_separation_cm": 22000.0,
                "vertical_offset_cm": 10000.0,
                "verticality_bias": 0.35,
                "segment_curvature_cm": 3200.0,
                "intermediate_point_jitter_cm": 1800.0,
                "hub_approach_curvature_scale": 0.35,
                "optional_branch_curvature_scale": 1.1,
            },
            "rhythm": {
                "branch_density": 0.28,
                "hub_chance": 0.58,
                "pocket_chance": 0.28,
            },
        },
        {
            "asset_name": "DA_RouteArchetype_Auto_Vertical",
            "archetype_id": "Auto_Vertical",
            "archetype_type": ARCHETYPE_TYPE["DEEP_DESCENT"],
            "branch_profile_set": profile_sets["Auto_Vertical_01"],
            "complexity_tier": COMPLEXITY_TIER["MODERATE"],
            "checkpoints": {
                "start_shape": CHECKPOINT_SHAPE["GOULOT"],
                "end_shape": CHECKPOINT_SHAPE["POCKET"],
                "start_radius_cm": 3800.0,
                "end_radius_cm": 5600.0,
            },
            "connections": {
                "start_dock_length_cm": 3200.0,
                "end_dock_length_cm": 3600.0,
                "start_dock_radius_scale": 0.8,
                "end_dock_radius_scale": 0.82,
                "preferred_campaign_overlap_cm": 1600.0,
            },
            "flow": {
                "main_spine_node_count": 10,
                "max_split_anchors": 3,
                "max_branch_fanout": 3,
                "rejoin_chance": 0.48,
            },
            "complexity": {
                "max_hub_count": 2,
                "max_optional_side_branches": 4,
                "max_pocket_depth": 2,
            },
            "volumes": {
                "trunk_radius_cm": 3900.0,
                "branch_a_radius_cm": 3000.0,
                "branch_b_radius_cm": 3600.0,
                "branch_c_radius_cm": 3200.0,
                "merge_radius_cm": 4100.0,
                "hub_radius_cm": 6000.0,
                "pocket_radius_cm": 3000.0,
            },
            "spatial_shape": {
                "branch_separation_cm": 18000.0,
                "vertical_offset_cm": 26000.0,
                "verticality_bias": 0.78,
                "segment_curvature_cm": 3800.0,
                "intermediate_point_jitter_cm": 1600.0,
                "hub_approach_curvature_scale": 0.4,
                "optional_branch_curvature_scale": 1.25,
            },
            "rhythm": {
                "branch_density": 0.32,
                "hub_chance": 0.34,
                "pocket_chance": 0.3,
            },
        },
        {
            "asset_name": "DA_RouteArchetype_Auto_LabyrinthLite",
            "archetype_id": "Auto_LabyrinthLite",
            "archetype_type": ARCHETYPE_TYPE["STEALTH_PASSAGE"],
            "branch_profile_set": profile_sets["Auto_LabyrinthLite_01"],
            "complexity_tier": COMPLEXITY_TIER["DENSE"],
            "checkpoints": {
                "start_shape": CHECKPOINT_SHAPE["POCKET"],
                "end_shape": CHECKPOINT_SHAPE["POCKET"],
                "start_radius_cm": 4600.0,
                "end_radius_cm": 5200.0,
            },
            "connections": {
                "start_dock_length_cm": 3600.0,
                "end_dock_length_cm": 3600.0,
                "start_dock_radius_scale": 0.75,
                "end_dock_radius_scale": 0.78,
                "preferred_campaign_overlap_cm": 1500.0,
            },
            "flow": {
                "main_spine_node_count": 12,
                "max_split_anchors": 4,
                "max_branch_fanout": 4,
                "rejoin_chance": 0.42,
            },
            "complexity": {
                "max_hub_count": 2,
                "max_optional_side_branches": 6,
                "max_pocket_depth": 2,
            },
            "volumes": {
                "trunk_radius_cm": 3500.0,
                "branch_a_radius_cm": 2600.0,
                "branch_b_radius_cm": 3200.0,
                "branch_c_radius_cm": 2900.0,
                "merge_radius_cm": 3600.0,
                "hub_radius_cm": 5400.0,
                "pocket_radius_cm": 3000.0,
            },
            "spatial_shape": {
                "branch_separation_cm": 16000.0,
                "vertical_offset_cm": 12000.0,
                "verticality_bias": 0.5,
                "segment_curvature_cm": 4400.0,
                "intermediate_point_jitter_cm": 2400.0,
                "hub_approach_curvature_scale": 0.5,
                "optional_branch_curvature_scale": 1.45,
            },
            "rhythm": {
                "branch_density": 0.5,
                "hub_chance": 0.28,
                "pocket_chance": 0.48,
            },
        },
        {
            "asset_name": "DA_RouteArchetype_Auto_Tight",
            "archetype_id": "Auto_Tight",
            "archetype_type": ARCHETYPE_TYPE["STEALTH_PASSAGE"],
            "branch_profile_set": profile_sets["Auto_Tight_01"],
            "complexity_tier": COMPLEXITY_TIER["MODERATE"],
            "checkpoints": {
                "start_shape": CHECKPOINT_SHAPE["GOULOT"],
                "end_shape": CHECKPOINT_SHAPE["GOULOT"],
                "start_radius_cm": 3400.0,
                "end_radius_cm": 4000.0,
            },
            "connections": {
                "start_dock_length_cm": 3200.0,
                "end_dock_length_cm": 3200.0,
                "start_dock_radius_scale": 0.72,
                "end_dock_radius_scale": 0.72,
                "preferred_campaign_overlap_cm": 1400.0,
            },
            "flow": {
                "main_spine_node_count": 9,
                "max_split_anchors": 3,
                "max_branch_fanout": 3,
                "rejoin_chance": 0.35,
            },
            "complexity": {
                "max_hub_count": 1,
                "max_optional_side_branches": 4,
                "max_pocket_depth": 2,
            },
            "volumes": {
                "trunk_radius_cm": 3100.0,
                "branch_a_radius_cm": 2300.0,
                "branch_b_radius_cm": 2900.0,
                "branch_c_radius_cm": 2600.0,
                "merge_radius_cm": 3300.0,
                "hub_radius_cm": 4800.0,
                "pocket_radius_cm": 2800.0,
            },
            "spatial_shape": {
                "branch_separation_cm": 15000.0,
                "vertical_offset_cm": 9000.0,
                "verticality_bias": 0.45,
                "segment_curvature_cm": 4200.0,
                "intermediate_point_jitter_cm": 2200.0,
                "hub_approach_curvature_scale": 0.45,
                "optional_branch_curvature_scale": 1.35,
            },
            "rhythm": {
                "branch_density": 0.38,
                "hub_chance": 0.2,
                "pocket_chance": 0.42,
            },
        },
    ]


def biome_configs(profile_sets):
    return [
        {
            "asset_name": "DA_BiomeField_Auto_UpperShelf",
            "biome_id": "Auto_UpperShelf",
            "preferred_branch_profile_set": profile_sets["Auto_DefaultTraversal_01"],
            "default_traversal_complexity": COMPLEXITY_TIER["SIMPLE"],
            "noise_freq1": 1.0 / 4800.0,
            "noise_freq2": 1.0 / 3200.0,
            "noise_cave_threshold": 0.0,
            "organic_amplitude": 0.0,
            "large_scale_warp_amplitude": 0.0,
            "medium_noise_amplitude": 0.0,
        },
        {
            "asset_name": "DA_BiomeField_Auto_DeepFault",
            "biome_id": "Auto_DeepFault",
            "preferred_branch_profile_set": profile_sets["Auto_Vertical_01"],
            "default_traversal_complexity": COMPLEXITY_TIER["MODERATE"],
            "noise_freq1": 1.0 / 4800.0,
            "noise_freq2": 1.0 / 3200.0,
            "noise_cave_threshold": 0.0,
            "organic_amplitude": 0.0,
            "large_scale_warp_amplitude": 0.0,
            "medium_noise_amplitude": 0.0,
        },
        {
            "asset_name": "DA_BiomeField_Auto_Labyrinth",
            "biome_id": "Auto_Labyrinth",
            "preferred_branch_profile_set": profile_sets["Auto_LabyrinthLite_01"],
            "default_traversal_complexity": COMPLEXITY_TIER["DENSE"],
            "noise_freq1": 1.0 / 4800.0,
            "noise_freq2": 1.0 / 3200.0,
            "noise_cave_threshold": 0.0,
            "organic_amplitude": 0.0,
            "large_scale_warp_amplitude": 0.0,
            "medium_noise_amplitude": 0.0,
        },
    ]


def campaign_segment_configs(archetypes, biomes):
    upper = biomes["upper_shelf"]
    deep = biomes["deep_fault"]
    labyrinth = biomes["labyrinth"]
    return {
        "campaign_asset_name": "DA_CampaignGraph_Auto_01",
        "campaign_id": "AutoCampaign_01",
        "display_name": "Auto Campaign 01",
        "root_segment_id": "Seg_01",
        "terminal_segment_id": "Seg_10",
        "total_depth_meters": 12000.0,
        "segments": [
            {"id": "Seg_01", "name": "Entry Shelf", "role": "ENTRY", "archetype": archetypes["Auto_WideHub"], "biome": upper, "complexity": COMPLEXITY_TIER["SIMPLE"], "length_m": 4200.0, "depth_m": 500.0, "seed": 0, "next": ["Seg_02"]},
            {"id": "Seg_02", "name": "Initial Descent", "role": "MAIN", "archetype": archetypes["Auto_Vertical"], "biome": upper, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 5600.0, "depth_m": 1200.0, "seed": 1, "next": ["Seg_03", "Seg_04"]},
            {"id": "Seg_03", "name": "Hub Traverse", "role": "MAIN", "archetype": archetypes["Auto_WideHub"], "biome": upper, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 6100.0, "depth_m": 2100.0, "seed": 2, "next": ["Seg_05"]},
            {"id": "Seg_04", "name": "Tight Detour", "role": "SIDE_BRANCH", "archetype": archetypes["Auto_Tight"], "biome": upper, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 3300.0, "depth_m": 1850.0, "seed": 3, "next": ["Seg_05"]},
            {"id": "Seg_05", "name": "Biolink Merge", "role": "MAIN", "archetype": archetypes["Auto_LabyrinthLite"], "biome": deep, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 5900.0, "depth_m": 3000.0, "seed": 4, "next": ["Seg_06", "Seg_07"]},
            {"id": "Seg_06", "name": "Deep Fault", "role": "MAIN", "archetype": archetypes["Auto_Vertical"], "biome": deep, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 6700.0, "depth_m": 4300.0, "seed": 5, "next": ["Seg_08"]},
            {"id": "Seg_07", "name": "Pocket Survey", "role": "SIDE_BRANCH", "archetype": archetypes["Auto_LabyrinthLite"], "biome": labyrinth, "complexity": COMPLEXITY_TIER["DENSE"], "length_m": 4100.0, "depth_m": 3900.0, "seed": 6, "next": ["Seg_08"]},
            {"id": "Seg_08", "name": "Wide Fault", "role": "MAIN", "archetype": archetypes["Auto_WideHub"], "biome": deep, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 6200.0, "depth_m": 5600.0, "seed": 7, "next": ["Seg_09"]},
            {"id": "Seg_09", "name": "Final Labyrinth", "role": "MAIN", "archetype": archetypes["Auto_LabyrinthLite"], "biome": labyrinth, "complexity": COMPLEXITY_TIER["DENSE"], "length_m": 5400.0, "depth_m": 7000.0, "seed": 8, "next": ["Seg_10"]},
            {"id": "Seg_10", "name": "Exit Sink", "role": "EXIT", "archetype": archetypes["Auto_Vertical"], "biome": deep, "complexity": COMPLEXITY_TIER["MODERATE"], "length_m": 5000.0, "depth_m": 8200.0, "seed": 9, "next": []},
        ],
    }


def create_branch_profile(package_path: str, asset_name: str, config: dict):
    asset = make_data_asset(asset_name, package_path, unreal.BranchProfileDataAsset)
    set_if_present(asset, "profile_id", config["profile_id"])
    set_if_present(asset, "schema_version", 1)
    set_if_present(asset, "display_name", make_text(config["display_name"]))
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
    set_if_present(asset, "preferred_window", config.get("preferred_window", BRANCH_WINDOW["ANYWHERE"]))
    set_if_present(asset, "selection_weight", config.get("selection_weight", 1.0))
    save_asset(asset)
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
    return asset


def apply_envelope_spec(asset, length_meters: float) -> None:
    envelope = asset.get_editor_property("envelope_spec")
    set_if_present(envelope, "sub_length", 3500.0)
    set_if_present(envelope, "sub_width", 450.0)
    set_if_present(envelope, "sub_height", 450.0)
    set_if_present(envelope, "hard_clearance", 350.0)
    set_if_present(envelope, "preferred_clearance", 1200.0)
    set_if_present(envelope, "min_turn_radius", 2200.0)
    set_if_present(envelope, "preferred_combat_radius", 4500.0)
    set_if_present(envelope, "min_sightline", 5000.0)
    set_if_present(asset, "envelope_spec", envelope)
    set_if_present(asset, "route_length_meters", length_meters)


def create_route_archetype(package_path: str, config: dict):
    asset = make_data_asset(config["asset_name"], package_path, unreal.RouteArchetypeDataAsset)
    set_if_present(asset, "archetype_id", config["archetype_id"])
    set_if_present(asset, "archetype_type", config["archetype_type"])
    set_if_present(asset, "use_grouped_constrained_authoring", True)
    set_if_present(asset, "use_constrained_graph_pattern", True)
    set_if_present(asset, "use_multi_stage_split_pattern", False)
    set_if_present(asset, "use_split_merge_pattern", False)
    set_if_present(asset, "default_complexity_tier", config["complexity_tier"])
    set_if_present(asset, "branch_profile_set", config["branch_profile_set"])
    apply_envelope_spec(asset, 5000.0)
    apply_struct_settings(asset, "checkpoints", config["checkpoints"])
    apply_struct_settings(asset, "connections", config["connections"])
    apply_struct_settings(asset, "flow", config["flow"])
    apply_struct_settings(asset, "complexity", config["complexity"])
    apply_struct_settings(asset, "volumes", config["volumes"])
    apply_struct_settings(asset, "spatial_shape", config["spatial_shape"])
    apply_struct_settings(asset, "rhythm", config["rhythm"])
    save_asset(asset)
    return asset


def create_biome(package_path: str, config: dict):
    asset = make_data_asset(config["asset_name"], package_path, unreal.BiomeFieldProfileDataAsset)
    set_if_present(asset, "biome_id", config["biome_id"])
    set_if_present(asset, "preferred_branch_profile_set", config["preferred_branch_profile_set"])
    set_if_present(asset, "default_traversal_complexity", config["default_traversal_complexity"])
    set_if_present(asset, "noise_freq1", config["noise_freq1"])
    set_if_present(asset, "noise_freq2", config["noise_freq2"])
    set_if_present(asset, "noise_cave_threshold", config["noise_cave_threshold"])
    set_if_present(asset, "organic_amplitude", config["organic_amplitude"])
    set_if_present(asset, "large_scale_warp_amplitude", config["large_scale_warp_amplitude"])
    set_if_present(asset, "medium_noise_amplitude", config["medium_noise_amplitude"])
    save_asset(asset)
    return asset


def make_segment_descriptor(config: dict):
    desc = unreal.CampaignSegmentDescriptor()
    set_if_present(desc, "segment_id", config["id"])
    set_if_present(desc, "display_name", make_text(config["name"]))
    set_if_present(desc, "role", CAMPAIGN_ROLE[config["role"]])
    set_if_present(desc, "archetype", config["archetype"])
    set_if_present(desc, "biome", config["biome"])
    set_if_present(desc, "complexity_tier", config["complexity"])
    set_if_present(desc, "preferred_length_meters", config["length_m"])
    set_if_present(desc, "depth_at_segment_end_meters", config["depth_m"])
    set_if_present(desc, "seed_offset", config["seed"])
    # Unreal Python exposes this reflected field as `next_segment_i_ds`.
    set_if_present(desc, "next_segment_i_ds", config["next"])
    return desc


def create_campaign_graph(package_path: str, config: dict):
    asset = make_data_asset(config["campaign_asset_name"], package_path, unreal.CampaignGraphAsset)
    set_if_present(asset, "campaign_graph_id", config["campaign_id"])
    set_if_present(asset, "display_name", make_text(config["display_name"]))
    set_if_present(asset, "root_segment_id", config["root_segment_id"])
    set_if_present(asset, "terminal_segment_id", config["terminal_segment_id"])
    set_if_present(asset, "total_depth_meters", config["total_depth_meters"])
    set_if_present(asset, "segments", [make_segment_descriptor(segment) for segment in config["segments"]])
    save_asset(asset)
    return asset


def main():
    for path in [ROOT_DIR, PROFILE_DIR, SET_DIR, ARCHETYPE_DIR, BIOME_DIR, CAMPAIGN_DIR]:
        ensure_dir(path)

    created_profiles = {}
    for config in branch_profile_configs():
        created_profiles[config["profile_id"]] = create_branch_profile(PROFILE_DIR, config["asset_name"], config)

    created_sets = {}
    for config in branch_profile_set_configs():
        created_sets[config["profile_set_id"]] = create_profile_set(SET_DIR, config["asset_name"], config, created_profiles)

    archetypes = {}
    for config in route_archetype_configs(created_sets):
        archetypes[config["archetype_id"]] = create_route_archetype(ARCHETYPE_DIR, config)

    biomes = {}
    biome_key_map = {
        "Auto_UpperShelf": "upper_shelf",
        "Auto_DeepFault": "deep_fault",
        "Auto_Labyrinth": "labyrinth",
    }
    for config in biome_configs(created_sets):
        biome_asset = create_biome(BIOME_DIR, config)
        biomes[biome_key_map[config["biome_id"]]] = biome_asset

    campaign_config = campaign_segment_configs(archetypes, biomes)
    campaign_asset = create_campaign_graph(CAMPAIGN_DIR, campaign_config)

    unreal.log("[CampaignBundleGen] Done.")
    unreal.log(f"[CampaignBundleGen] Campaign graph: {campaign_asset.get_path_name()}")
    unreal.log("[CampaignBundleGen] Archetypes:")
    for archetype in archetypes.values():
        unreal.log(f"  - {archetype.get_path_name()}")


if __name__ == "__main__":
    main()
