"""
Sub3D - Voxel Abyss Pack Manifest
=================================

Central manifest for voxel abyss generators and their expected assets.
"""


def _variant_assets(base_names, variants):
    return [f"SM_VX_{base}_{variant}" for base in base_names for variant in variants]


SCOUT_ASSETS = _variant_assets(
    [
        "SiltLeech",
        "PincherMite",
        "GlimmerSkate",
        "RiftEel",
        "BlindCrawler",
        "NeedleLamprey",
        "SnoutCrab",
        "PulseMinnow",
    ],
    ["Baby", "Small", "Adult", "Large"],
)

BRUTE_ASSETS = _variant_assets(
    [
        "MawDrifter",
        "BarnacleBoar",
        "ShellbackRam",
        "AnchorJaw",
        "SlugBrute",
        "VentStalker",
        "MudGorger",
        "ReefCrusher",
    ],
    ["Juvenile", "Adult", "Alpha"],
)

GIANT_ASSETS = [
    "SM_VX_GigaVer_6m",
    "SM_VX_GigaVer_12m",
    "SM_VX_GigaVer_24m",
    "SM_VX_MegaSerpent_10m",
    "SM_VX_MegaSerpent_20m",
    "SM_VX_MegaSerpent_30m",
    "SM_VX_SeaWorm_15m",
    "SM_VX_TrenchTitan_8m",
    "SM_VX_TrenchTitan_14m",
    "SM_VX_ChoirWhale_12m",
    "SM_VX_ChoirWhale_20m",
    "SM_VX_RookLeviathan_10m",
    "SM_VX_RookLeviathan_18m",
    "SM_VX_SpineCathedral_9m",
    "SM_VX_SpineCathedral_16m",
    "SM_VX_MirrorKraken_11m",
    "SM_VX_MirrorKraken_19m",
]

CRANIATA_ASSETS = [
    "SM_VX_Craniata_FullRebuild_A",
    "SM_VX_Craniata_FullRebuild_Derelict",
    "SM_VX_Craniata_Nose_10m",
    "SM_VX_Craniata_Mid_8m_A",
    "SM_VX_Craniata_Mid_8m_B",
    "SM_VX_Craniata_Engine_8m",
    "SM_VX_Craniata_Tail_6m",
    "SM_VX_Craniata_Tower_Module",
    "SM_VX_Craniata_Airlock_Module",
    "SM_VX_Craniata_BallastPod_Port",
    "SM_VX_Craniata_BallastPod_Stbd",
    "SM_VX_Craniata_PropCluster",
    "SM_VX_Craniata_InteriorCorridor_6m",
    "SM_VX_Craniata_UtilityCapsule_4m",
]

OUTPOST_ASSETS = [
    "SM_VX_OutpostTower_10m_A",
    "SM_VX_OutpostTower_15m_A",
    "SM_VX_OutpostTower_20m_A",
    "SM_VX_OutpostHub_2F_A",
    "SM_VX_OutpostHub_3F_A",
    "SM_VX_OutpostCorridor_6m_Straight",
    "SM_VX_OutpostCorridor_12m_Straight",
    "SM_VX_OutpostCorridor_18m_Straight",
    "SM_VX_OutpostCorridor_Elbow_A",
    "SM_VX_OutpostCorridor_T_A",
    "SM_VX_OutpostCorridor_Cross_A",
    "SM_VX_OutpostDockClamp_10m_A",
    "SM_VX_OutpostDockClamp_16m_A",
    "SM_VX_OutpostPressureDome_8m_A",
    "SM_VX_OutpostPressureDome_12m_A",
    "SM_VX_OutpostAirlock_6m_A",
    "SM_VX_OutpostAirlock_8m_A",
    "SM_VX_OutpostStairs_10m_A",
    "SM_VX_OutpostLift_15m_A",
    "SM_VX_OutpostWalkway_10m_A",
    "SM_VX_OutpostWalkway_20m_A",
    "SM_VX_OutpostServiceSpine_14m_A",
    "SM_VX_OutpostServiceSpine_22m_A",
]

SUBMARINE_MODULAR_ASSETS = [
    "SM_VX_SubMod_Nose_8m",
    "SM_VX_SubMod_Mid_8m_A",
    "SM_VX_SubMod_Mid_8m_B",
    "SM_VX_SubMod_Stern_10m",
    "SM_VX_SubMod_Core_12m",
    "SM_VX_SubMod_Bridge_6m",
    "SM_VX_SubMod_Engine_8m",
    "SM_VX_SubMod_BallastPort_6m",
    "SM_VX_SubMod_BallastStbd_6m",
    "SM_VX_SubMod_DockCollar_6m",
    "SM_VX_SubMod_TorpedoBay_8m",
    "SM_VX_SubMod_FinSet_A",
    "SM_VX_SubMod_Spine_10m",
    "SM_VX_SubMod_InteriorCorridor_4m",
    "SM_VX_SubMod_ServiceRoom_6m",
    "SM_VX_SubMod_PropRing_A",
    "SM_VX_SubMod_ReactorCapsule_6m",
]


SCRIPT_SPECS = [
    {
        "module": "mobs_scouts",
        "collection": "VX_Mobs_Scouts",
        "title": "Abyssal Scout Mobs",
        "assets": SCOUT_ASSETS,
    },
    {
        "module": "mobs_brutes",
        "collection": "VX_Mobs_Brutes",
        "title": "Abyssal Brute Mobs",
        "assets": BRUTE_ASSETS,
    },
    {
        "module": "mobs_giants",
        "collection": "VX_Mobs_Giants",
        "title": "Abyssal Giant Mobs",
        "assets": GIANT_ASSETS,
    },
    {
        "module": "voxel_craniata",
        "collection": "VX_Craniata_Voxel",
        "title": "Voxel Craniata Family",
        "assets": CRANIATA_ASSETS,
    },
    {
        "module": "submarine_modular",
        "collection": "VX_Submarine_Modular",
        "title": "Modular Submarine Kit",
        "assets": SUBMARINE_MODULAR_ASSETS,
    },
    {
        "module": "outposts",
        "collection": "VX_Outposts",
        "title": "Submarine Outposts",
        "assets": OUTPOST_ASSETS,
    },
    {
        "module": "cave_columns",
        "collection": "VX_Cave_Columns",
        "title": "Cave Columns and Arches",
        "assets": [
            "SM_VX_CaveArch_Tight",
            "SM_VX_CaveArch_Wide",
            "SM_VX_CaveArch_SplitPillar",
            "SM_VX_CaveArch_ToothGate",
            "SM_VX_CaveArch_RibTunnel",
            "SM_VX_CaveArch_Buttress",
            "SM_VX_CaveArch_ShelfSpine",
            "SM_VX_CaveArch_CathedralColumn",
        ],
    },
    {
        "module": "stalactites",
        "collection": "VX_Stalactites",
        "title": "Stalactite Modules",
        "assets": [
            "SM_VX_Stalactite_LongA",
            "SM_VX_Stalactite_LongB",
            "SM_VX_Stalactite_ClusterA",
            "SM_VX_Stalactite_ClusterB",
            "SM_VX_Stalactite_BrokenA",
            "SM_VX_Stalactite_BrokenB",
            "SM_VX_Stalagmite_A",
            "SM_VX_Stalagmite_B",
            "SM_VX_RoofFang_A",
            "SM_VX_RoofFang_B",
        ],
    },
    {
        "module": "metal_intrusions",
        "collection": "VX_Metal_Intrusions",
        "title": "Metal Intrusions",
        "assets": [
            "SM_VX_MetalLance_Straight",
            "SM_VX_MetalLance_Twisted",
            "SM_VX_MetalLance_Crossbrace",
            "SM_VX_MetalLance_BuriedTruss",
            "SM_VX_MetalLance_RoofSpears",
            "SM_VX_MetalLance_JaggedPile",
        ],
    },
    {
        "module": "rock_formations",
        "collection": "VX_Rock_Formations",
        "title": "Geological Rock Formations",
        "assets": [
            "SM_VX_Rock_BoulderA",
            "SM_VX_Rock_BoulderB",
            "SM_VX_Rock_ShelfA",
            "SM_VX_Rock_ShelfB",
            "SM_VX_Rock_NeedleA",
            "SM_VX_Rock_NeedleB",
            "SM_VX_Rock_WallChunkA",
            "SM_VX_Rock_FumaroleA",
        ],
    },
    {
        "module": "abyssal_flora",
        "collection": "VX_Abyssal_Flora",
        "title": "Abyssal Flora",
        "assets": [
            "SM_VX_Flora_TubewormPatch",
            "SM_VX_Flora_FanCoral",
            "SM_VX_Flora_BambooCoral",
            "SM_VX_Flora_SeaPen",
            "SM_VX_Flora_VentReeds",
            "SM_VX_Flora_SporePalm",
            "SM_VX_Flora_BulbAnemone",
            "SM_VX_Flora_LanternKelp",
            "SM_VX_Flora_BoneMoss",
            "SM_VX_Flora_BlackCoralShrub",
        ],
    },
    {
        "module": "seafloor_clutter",
        "collection": "VX_Seafloor_Clutter",
        "title": "Seafloor Clutter",
        "assets": [
            "SM_VX_WreckPlate",
            "SM_VX_ChainNest",
            "SM_VX_AnchorBone",
            "SM_VX_PipeDebris",
            "SM_VX_EggCluster",
            "SM_VX_ShellPile",
            "SM_VX_CrateRemains",
            "SM_VX_RustedPanelGarden",
        ],
    },
    {
        "module": "vent_fields",
        "collection": "VX_Vent_Fields",
        "title": "Vent Fields",
        "assets": [
            "SM_VX_Vent_BlackSmoker",
            "SM_VX_Vent_WhiteSmoker",
            "SM_VX_Vent_ColdSeep",
            "SM_VX_Vent_SulfurMound",
            "SM_VX_Vent_BubbleSpire",
            "SM_VX_Vent_ChimneyBroken",
            "SM_VX_Vent_ChimneyRing",
            "SM_VX_Vent_ChemMat",
        ],
    },
]


def total_asset_count() -> int:
    return sum(len(spec["assets"]) for spec in SCRIPT_SPECS)
