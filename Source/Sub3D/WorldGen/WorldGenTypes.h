#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WorldGenTypes.generated.h"

// Dedicated log category for all WorldGen pipeline steps.
// Verbosity set to Verbose in editor builds so per-chunk stats are visible in Output Log.
DECLARE_LOG_CATEGORY_EXTERN(LogRouteGen, Verbose, All);

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ETraversalRouteArchetype : uint8
{
	MainTransit      UMETA(DisplayName="MainTransit"),
	StealthPassage   UMETA(DisplayName="StealthPassage"),
	CombatCorridor   UMETA(DisplayName="CombatCorridor"),
	DeepDescent      UMETA(DisplayName="DeepDescent"),
	SalvageRoute     UMETA(DisplayName="SalvageRoute")
};

UENUM(BlueprintType)
enum class ETopologyNodeType : uint8
{
	StartCheckpointSpace,
	StartAnchor,
	EntryBuffer,
	WideTransit,
	SplitAnchor,
	BranchTransit,
	MergeAnchor,
	NarrowTransit,
	TurnPocket,
	VerticalDrop,
	HubChamber,
	AmbushPocket,
	WreckPocket,
	ResourcePocket,
	ExitApproach,
	EndCheckpointSpace,
	ExitAnchor
};

UENUM(BlueprintType)
enum class ECheckpointSpaceShape : uint8
{
	Goulot,
	Pocket,
	LargeCavity,
	InterfaceOnly
};

UENUM(BlueprintType)
enum class EVolumeBrushType : uint8
{
	CapsuleCorridor,
	SpherePocket,
	EllipsoidChamber
};

UENUM(BlueprintType)
enum class ELogicalRouteNodeRole : uint8
{
	StartCheckpoint,
	EndCheckpoint,
	MainSpine,
	CanonicalBypass,
	OptionalSideBranch,
	Hub,
	PocketDeadEnd,
	DecorativeDisconnected
};

UENUM(BlueprintType)
enum class ETraversalComplexityTier : uint8
{
	Simple,
	Moderate,
	Dense
};

UENUM(BlueprintType)
enum class EBranchProfileIntent : uint8
{
	CanonicalBypass,
	OptionalResourceDetour,
	OptionalDangerBranch,
	HubConnector,
	PocketChain,
	CheckpointSideBranch
};

UENUM(BlueprintType)
enum class ERouteMeshCollisionMode : uint8
{
	None,
	ComplexAsSimple,
	SimpleAndComplex
};

UENUM(BlueprintType)
enum class ERouteBoundsCollisionStrategy : uint8
{
	UseSurfaceModes,
	VisualMeshComplex,
	VisualMeshAndSimpleProxy,
	CanonicalProxyOnly
};

UENUM(BlueprintType)
enum class EBranchPlacementWindow : uint8
{
	Anywhere,
	StartThird,
	Mid,
	Late,
	NearHub,
	NearCheckpoint,
	DeepSegment
};

UENUM(BlueprintType)
enum class ESemanticZoneType : uint8
{
	StartCheckpointDock,
	EndCheckpointDock,
	SafeTransit,
	CombatSpace,
	StealthSpace,
	SalvageSpace,
	SonarInterference,
	CurrentHazard,
	ExitGate
};

// ─────────────────────────────────────────────────────────────────────────────
// Navigation envelope
// ─────────────────────────────────────────────────────────────────────────────

// All values in centimetres. Derived from GDD_02 submarine Tier 2.
USTRUCT(BlueprintType)
struct FNavigationEnvelopeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubLength            = 2700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubWidth             = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubHeight            = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HardClearance        = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PreferredClearance   = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinTurnRadius        = 2200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PreferredCombatRadius = 4500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinSightline         = 5000.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Route generation spec + seed cascade
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRouteGenSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  CampaignSeed       = 12345;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  RouteID            = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  StartCheckpointID  = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  EndCheckpointID    = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  RouteLengthMeters  = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  StartDepthMeters   = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  EndDepthMeters     = 1600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  BiomeID            = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  DifficultyTier     = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETraversalComplexityTier ComplexityTier = ETraversalComplexityTier::Moderate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  CrewSizeHint       = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETraversalRouteArchetype Archetype = ETraversalRouteArchetype::MainTransit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FNavigationEnvelopeSpec  Envelope;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bForceStartInterfaceOnly = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bForceEndInterfaceOnly = false;
};

USTRUCT(BlueprintType)
struct FRouteSeedCascade
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 RouteSeed      = 0;
	UPROPERTY(BlueprintReadOnly) int32 TopologySeed   = 0;
	UPROPERTY(BlueprintReadOnly) int32 VolumeSeed     = 0;
	UPROPERTY(BlueprintReadOnly) int32 OrganicSeed    = 0;
	UPROPERTY(BlueprintReadOnly) int32 SemanticSeed   = 0;
	UPROPERTY(BlueprintReadOnly) int32 PopulationSeed = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Topology
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FTraversalTopologyNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32               NodeID            = 0;
	UPROPERTY(BlueprintReadOnly) ETopologyNodeType   NodeType          = ETopologyNodeType::WideTransit;
	UPROPERTY(BlueprintReadOnly) FVector             WorldPosition     = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float               NormalizedDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float               PreferredRadius   = 3000.f;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) int32               BranchStage       = 0;
	UPROPERTY(BlueprintReadOnly) int32               BranchIndex       = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName               BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32               LogicalDepth      = 0;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole     = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer SemanticTags;
	UPROPERTY(BlueprintReadOnly) TArray<int32>       NextNodeIDs;
	UPROPERTY(BlueprintReadOnly) bool                bIsBranch         = false;
	UPROPERTY(BlueprintReadOnly) bool                bIsDeadEnd        = false;
	UPROPERTY(BlueprintReadOnly) bool                bIsCanonicalPath  = true;
	UPROPERTY(BlueprintReadOnly) bool                bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool                bIsDecorativeDisconnected = false;
};

USTRUCT(BlueprintType)
struct FTraversalSkeletonSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32  StartNodeID    = 0;
	UPROPERTY(BlueprintReadOnly) int32  EndNodeID      = 0;
	UPROPERTY(BlueprintReadOnly) FVector P0             = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector P1             = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector P2             = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector P3             = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector SupportPoint   = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float  StartRadius    = 3000.f;
	UPROPERTY(BlueprintReadOnly) float  EndRadius      = 3000.f;
	UPROPERTY(BlueprintReadOnly) float  RadiusTransitionStartT = 0.f;
	UPROPERTY(BlueprintReadOnly) float  RadiusTransitionEndT = 1.f;
	UPROPERTY(BlueprintReadOnly) float  JunctionThroatScale = 1.f;
	UPROPERTY(BlueprintReadOnly) int32  BranchStage    = 0;
	UPROPERTY(BlueprintReadOnly) int32  BranchIndex    = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32  LogicalDepth   = 0;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) bool   bGuaranteedPath = true;
	UPROPERTY(BlueprintReadOnly) bool   bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool   bIsDecorativeDisconnected = false;
};

USTRUCT(BlueprintType)
struct FLogicalRouteNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 NodeID = 0;
	UPROPERTY(BlueprintReadOnly) ETopologyNodeType NodeType = ETopologyNodeType::WideTransit;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32 LogicalDepth = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchStage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) bool bGuaranteesProgress = true;
	UPROPERTY(BlueprintReadOnly) bool bIsDecorativeDisconnected = false;
	UPROPERTY(BlueprintReadOnly) FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) TArray<int32> ConnectedNodeIDs;
};

USTRUCT(BlueprintType)
struct FLogicalRouteEdge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 FromNodeID = 0;
	UPROPERTY(BlueprintReadOnly) int32 ToNodeID = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchStage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) bool bIsGuaranteedTraversal = true;
	UPROPERTY(BlueprintReadOnly) bool bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool bIsDecorativeDisconnected = false;
};

USTRUCT(BlueprintType)
struct FLogicalRouteGraph
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FLogicalRouteNode> Nodes;
	UPROPERTY(BlueprintReadOnly) TArray<FLogicalRouteEdge> Edges;
};

// ─────────────────────────────────────────────────────────────────────────────
// Volume brushes + field
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FVolumeBrushDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EVolumeBrushType BrushType  = EVolumeBrushType::CapsuleCorridor;
	UPROPERTY(BlueprintReadOnly) FVector          CenterA    = FVector::ZeroVector; // capsule endpoint A
	UPROPERTY(BlueprintReadOnly) FVector          CenterB    = FVector::ZeroVector; // capsule endpoint B (=A for sphere)
	UPROPERTY(BlueprintReadOnly) FVector          HalfExtents = FVector(3000.f);
	UPROPERTY(BlueprintReadOnly) float            Radius     = 3000.f;
	UPROPERTY(BlueprintReadOnly) float            Smoothness = 200.f; // k for SmoothMin (cm)
	UPROPERTY(BlueprintReadOnly) bool             bAffectsRenderField = true;
	UPROPERTY(BlueprintReadOnly) bool             bAffectsSonarField = true;
	UPROPERTY(BlueprintReadOnly) bool             bGuaranteedTraversal = true;
	UPROPERTY(BlueprintReadOnly) bool             bDecorativeOnly = false;
	UPROPERTY(BlueprintReadOnly) int32            BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) FLinearColor     DebugColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct FFieldChunkCoord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 X = 0;
	UPROPERTY(BlueprintReadOnly) int32 Y = 0;
	UPROPERTY(BlueprintReadOnly) int32 Z = 0;

	bool operator==(const FFieldChunkCoord& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
};

FORCEINLINE uint32 GetTypeHash(const FFieldChunkCoord& C)
{
	return HashCombine(HashCombine(GetTypeHash(C.X), GetTypeHash(C.Y)), GetTypeHash(C.Z));
}

USTRUCT()
struct FRouteFieldChunkData
{
	GENERATED_BODY()

	// Render field: float density per voxel (200cm voxels). density < 0 = rock, >= 0 = water.
	TArray<float>  DensitySamples;
	// Sonar field: 0 = water, 255 = rock (500cm voxels, coarse).
	TArray<uint8>  OccupancySamples;

	UPROPERTY() FVector ChunkOrigin     = FVector::ZeroVector;
	UPROPERTY() float   VoxelSize       = 200.f;
	UPROPERTY() int32   SamplesPerAxis  = 0;   // samples per axis (cube grid: SamplesPerAxis³)
	UPROPERTY() bool    bContainsGuaranteedPath = false;
};

USTRUCT()
struct FRouteFieldModel
{
	GENERATED_BODY()

	UPROPERTY() TArray<FVolumeBrushDef> GuaranteedBrushes;
	UPROPERTY() TArray<FVolumeBrushDef> RenderOnlyBrushes;
	UPROPERTY() TArray<FVolumeBrushDef> OrganicBrushes;

	// TMap with custom key: not UPROPERTY() on purpose (no reflection needed for proto).
	TMap<FFieldChunkCoord, FRouteFieldChunkData> RenderField;  // 200cm voxels
	TMap<FFieldChunkCoord, FRouteFieldChunkData> SonarField;   // 500cm voxels
};

USTRUCT(BlueprintType)
struct FRouteTunnelDebugOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bEnableTunnelDebug = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bFilterToChunkWindow = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") FIntVector DebugChunkCoord = FIntVector::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="0")) int32 DebugChunkRadius = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bLogChunkSelectionSummary = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bLogGuaranteedUniformChunks = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bLogZeroVertChunks = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bLogRejectedTriangleChunks = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="1")) int32 RejectedTriangleLogThreshold = 32;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bLogKeptShellChunks = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Semantic model
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRouteSemanticZone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) ESemanticZoneType    ZoneType   = ESemanticZoneType::SafeTransit;
	UPROPERTY(BlueprintReadOnly) FBox                 Bounds     = FBox(EForceInit::ForceInit);
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer Tags;
	UPROPERTY(BlueprintReadOnly) int32                Importance = 0;
};

USTRUCT(BlueprintType)
struct FMissionSocketDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName    SocketID;
	UPROPERTY(BlueprintReadOnly) FVector  WorldLocation  = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FRotator WorldRotation  = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly) float    ClearanceRadius = 1500.f;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer SupportedMissionTags;
};

USTRUCT()
struct FRouteSemanticModel
{
	GENERATED_BODY()

	UPROPERTY() TArray<FRouteSemanticZone> Zones;
	UPROPERTY() TArray<FMissionSocketDef>  MissionSockets;
};

// ─────────────────────────────────────────────────────────────────────────────
// Mesh chunks
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT()
struct FRouteMeshChunkData
{
	GENERATED_BODY()

	FFieldChunkCoord  Coord;
	TArray<FVector>   Vertices;
	TArray<int32>     Triangles;
	TArray<FVector>   Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	FBox              Bounds        = FBox(EForceInit::ForceInit);
	int32             MaterialSlot  = 0;
	bool              bServerCollision = false;
};

USTRUCT(BlueprintType)
struct FRouteSurfaceBuildSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") bool bEnableVisualSurfaceSmoothing = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0", ClampMax="4")) int32 SurfaceSmoothingIterations = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0")) float SurfaceSmoothingStrength = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0")) float SurfaceSmoothingMaxDisplacementCm = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") bool bUseWeightedVertexNormals = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") bool bBatchRuntimeMeshSections = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="1", ClampMax="256")) int32 RuntimeMeshSectionBatchSize = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") ERouteBoundsCollisionStrategy BoundsCollisionStrategy = ERouteBoundsCollisionStrategy::UseSurfaceModes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") ERouteMeshCollisionMode RuntimeMeshCollisionMode = ERouteMeshCollisionMode::ComplexAsSimple;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") ERouteMeshCollisionMode BakedMeshCollisionMode = ERouteMeshCollisionMode::ComplexAsSimple;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0")) float CollisionProxyInflationCm = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// Network + validation
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRouteNetSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FRouteGenSpec     GenSpec;
	UPROPERTY(BlueprintReadOnly) FRouteSeedCascade Seeds;
	UPROPERTY(BlueprintReadOnly) int32             BuildVersion = 1;
	UPROPERTY(BlueprintReadOnly) int32             BuildHash    = 0;
};

USTRUCT(BlueprintType)
struct FTraversalComplexityBudget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 MaxCanonicalBranchCount = 1;
	UPROPERTY(BlueprintReadOnly) int32 MaxOptionalBranchCount = 3;
	UPROPERTY(BlueprintReadOnly) int32 MaxHubCount = 2;
	UPROPERTY(BlueprintReadOnly) int32 MaxPocketDepth = 2;
	UPROPERTY(BlueprintReadOnly) int32 MaxReconnectCount = 2;
	UPROPERTY(BlueprintReadOnly) float MaxOptionalLengthRatio = 0.35f;
	UPROPERTY(BlueprintReadOnly) int32 MaxNodeBudget = 48;
};

USTRUCT(BlueprintType)
struct FSavedTraversalRecipe
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FRouteGenSpec GenSpec;
	UPROPERTY(BlueprintReadOnly) FRouteSeedCascade Seeds;
	UPROPERTY(BlueprintReadOnly) FName ArchetypeID;
	UPROPERTY(BlueprintReadOnly) FName BiomeProfileID;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileSetID;
	UPROPERTY(BlueprintReadOnly) ETraversalComplexityTier ComplexityTier = ETraversalComplexityTier::Moderate;
	UPROPERTY(BlueprintReadOnly) int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly) int32 BuildHash = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FName> SelectedBranchProfileIDs;
};

USTRUCT(BlueprintType)
struct FRouteValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool              bPass                 = false;
	UPROPERTY(BlueprintReadOnly) float             MinObservedClearance  = 0.f;
	UPROPERTY(BlueprintReadOnly) float             MainPathCoverageRatio = 0.f;
	UPROPERTY(BlueprintReadOnly) int32             DeadEndCount          = 0;
	UPROPERTY(BlueprintReadOnly) int32             SplitCount            = 0;
	UPROPERTY(BlueprintReadOnly) int32             TraversableBranches   = 0;
	UPROPERTY(BlueprintReadOnly) int32             MergeFailures         = 0;
	UPROPERTY(BlueprintReadOnly) int32             OptionalBranchCount   = 0;
	UPROPERTY(BlueprintReadOnly) int32             DecorativeCavityCount = 0;
	UPROPERTY(BlueprintReadOnly) int32             HubCount              = 0;
	UPROPERTY(BlueprintReadOnly) int32             CanonicalBypassCount  = 0;
	UPROPERTY(BlueprintReadOnly) int32             OptionalPocketCount   = 0;
	UPROPERTY(BlueprintReadOnly) int32             SelectedProfileCount  = 0;
	UPROPERTY(BlueprintReadOnly) bool              bMergeConnected       = false;
	UPROPERTY(BlueprintReadOnly) bool              bBranchA_Pass         = false;
	UPROPERTY(BlueprintReadOnly) bool              bBranchB_Pass         = false;
	UPROPERTY(BlueprintReadOnly) bool              bBranchC_Pass         = false;
	UPROPERTY(BlueprintReadOnly) float             MinClearanceBranchA   = 0.f;
	UPROPERTY(BlueprintReadOnly) float             MinClearanceBranchB   = 0.f;
	UPROPERTY(BlueprintReadOnly) float             MinClearanceBranchC   = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FString>   FailReasons;
};
