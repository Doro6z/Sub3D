#include "TraversalRouteActor.h"

#include "BiomeFieldProfileDataAsset.h"
#include "BranchProfileDataAsset.h"
#include "BranchProfileSetDataAsset.h"
#include "CampaignRouteCompiler.h"
#include "NavigableVolumeGenerator.h"
#include "Net/UnrealNetwork.h"
#include "OrganicDeformationGenerator.h"
#include "ProceduralMeshComponent.h"
#include "RouteArchetypeDataAsset.h"
#include "RouteMeshBuilder.h"
#include "RouteMotifDataAsset.h"
#include "RouteSemanticGenerator.h"
#include "RouteValidator.h"
#include "SkeletonResolver.h"
#include "SonarFieldComponent.h"
#include "TraversalTopologyGenerator.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#endif

DEFINE_LOG_CATEGORY(LogRouteGen);

namespace
{
static const TCHAR* BakedRouteComponentName = TEXT("BakedRouteMesh");
static const TCHAR* GeneratedRouteComponentTag = TEXT("GeneratedRoutePMC");
static const TCHAR* GeneratedRouteSectionPrefix = TEXT("RouteSection_");

void ConfigureEndpointMarker(UArrowComponent* Marker, const FColor& Color)
{
	if (!IsValid(Marker))
	{
		return;
	}

	Marker->ArrowColor = Color;
	Marker->ArrowLength = 600.f;
	Marker->ArrowSize = 1.5f;
	Marker->SetHiddenInGame(true);
	Marker->SetIsVisualizationComponent(true);
	Marker->bTreatAsASprite = true;
	Marker->bUseInEditorScaling = false;
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ConfigurePlacedEndpointOverrideMarker(UArrowComponent* Marker, const FColor& Color)
{
	if (!IsValid(Marker))
	{
		return;
	}

	Marker->ArrowColor = Color;
	Marker->ArrowLength = 800.f;
	Marker->ArrowSize = 2.f;
	Marker->SetHiddenInGame(true);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ApplySubmarineCollisionResponses(UPrimitiveComponent* PrimitiveComponent)
{
	if (!IsValid(PrimitiveComponent))
	{
		return;
	}

	PrimitiveComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	PrimitiveComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
}

bool IsManagedGeneratedRouteMeshComponent(const UProceduralMeshComponent* PMC)
{
	if (!IsValid(PMC))
	{
		return false;
	}

	return PMC->ComponentHasTag(FName(GeneratedRouteComponentTag))
		|| PMC->GetName().StartsWith(GeneratedRouteSectionPrefix);
}

void MixHash(uint32& H, uint32 V)
{
	H ^= V + 0x9e3779b9u + (H << 6) + (H >> 2);
}

void MixBool(uint32& H, bool bValue)
{
	MixHash(H, bValue ? 0x9E3779B9u : 0x85EBCA6Bu);
}

void MixInt(uint32& H, int32 V)
{
	MixHash(H, (uint32)V);
}

void MixFloat(uint32& H, float V, float Quantization = 100.f)
{
	MixInt(H, FMath::RoundToInt(V * Quantization));
}

void MixName(uint32& H, const FName& Name)
{
	MixHash(H, GetTypeHash(Name));
}

void MixString(uint32& H, const FString& Value)
{
	MixHash(H, FCrc::StrCrc32(*Value));
}

void MixTagContainer(uint32& H, const FGameplayTagContainer& Tags)
{
	TArray<FGameplayTag> SortedTags;
	Tags.GetGameplayTagArray(SortedTags);
	SortedTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().LexicalLess(B.GetTagName());
	});
	for (const FGameplayTag& Tag : SortedTags)
	{
		MixName(H, Tag.GetTagName());
	}
}

ERouteMeshCollisionMode ResolveCollisionModeForStrategy(const FRouteSurfaceBuildSettings& SurfaceSettings, bool bBaked)
{
	switch (SurfaceSettings.BoundsCollisionStrategy)
	{
	case ERouteBoundsCollisionStrategy::VisualMeshComplex:
		return ERouteMeshCollisionMode::ComplexAsSimple;
	case ERouteBoundsCollisionStrategy::VisualMeshAndSimpleProxy:
		return ERouteMeshCollisionMode::SimpleAndComplex;
	case ERouteBoundsCollisionStrategy::CanonicalProxyOnly:
		return ERouteMeshCollisionMode::SimpleAndComplex; // proto fallback until dedicated proxy exists
	case ERouteBoundsCollisionStrategy::UseSurfaceModes:
	default:
		return bBaked ? SurfaceSettings.BakedMeshCollisionMode : SurfaceSettings.RuntimeMeshCollisionMode;
	}
}

ECollisionEnabled::Type RuntimeCollisionEnabledFromMode(ERouteMeshCollisionMode Mode)
{
	switch (Mode)
	{
	case ERouteMeshCollisionMode::None:
		return ECollisionEnabled::NoCollision;
	case ERouteMeshCollisionMode::ComplexAsSimple:
	case ERouteMeshCollisionMode::SimpleAndComplex:
	default:
		return ECollisionEnabled::QueryAndPhysics;
	}
}

void MixBranchProfileSetHash(uint32& H, const TSoftObjectPtr<UBranchProfileSetDataAsset>& ProfileSetPtr)
{
	MixString(H, ProfileSetPtr.ToSoftObjectPath().ToString());
	if (const UBranchProfileSetDataAsset* ProfileSet = ProfileSetPtr.LoadSynchronous())
	{
		MixName(H, ProfileSet->ProfileSetID);
		MixInt(H, ProfileSet->SchemaVersion);
		MixFloat(H, ProfileSet->SimpleSelectionBias);
		MixFloat(H, ProfileSet->ModerateSelectionBias);
		MixFloat(H, ProfileSet->DenseSelectionBias);
		MixInt(H, ProfileSet->MaxCanonicalBypassProfiles);
		MixInt(H, ProfileSet->MaxOptionalProfiles);
		MixInt(H, ProfileSet->MaxPocketChainProfiles);
		for (const TSoftObjectPtr<UBranchProfileDataAsset>& ProfilePtr : ProfileSet->Profiles)
		{
			MixString(H, ProfilePtr.ToSoftObjectPath().ToString());
			if (const UBranchProfileDataAsset* Profile = ProfilePtr.LoadSynchronous())
			{
				MixName(H, Profile->ProfileID);
				MixInt(H, Profile->SchemaVersion);
				MixInt(H, (int32)Profile->Intent);
				MixBool(H, Profile->bAllowedOnCanonicalRoute);
				MixBool(H, Profile->bAllowedOnOptionalRoute);
				MixBool(H, Profile->bAllowedNearCheckpoint);
				MixBool(H, Profile->bAllowedNearHub);
				MixFloat(H, Profile->TargetRadiusCm);
				MixFloat(H, Profile->LengthScale);
				MixFloat(H, Profile->CurvatureScale);
				MixFloat(H, Profile->VerticalityScale);
				MixFloat(H, Profile->RejoinChance);
				MixFloat(H, Profile->PocketChance);
				MixFloat(H, Profile->SecondarySplitChance);
				MixInt(H, Profile->MaxDepth);
				MixInt(H, (int32)Profile->PreferredWindow);
				MixFloat(H, Profile->SelectionWeight);
				MixTagContainer(H, Profile->RequiredBiomeTags);
				MixTagContainer(H, Profile->ForbiddenBiomeTags);
				MixTagContainer(H, Profile->SemanticTags);
			}
		}
	}
}

float ResolveStartDockLengthCm(const URouteArchetypeDataAsset* Archetype)
{
	return Archetype ? Archetype->Connections.StartDockLengthCm : 3800.f;
}

float ResolveEndDockLengthCm(const URouteArchetypeDataAsset* Archetype)
{
	return Archetype ? Archetype->Connections.EndDockLengthCm : 3800.f;
}

float ResolveStartDockRadiusScale(const URouteArchetypeDataAsset* Archetype)
{
	return Archetype ? Archetype->Connections.StartDockRadiusScale : 0.78f;
}

float ResolveEndDockRadiusScale(const URouteArchetypeDataAsset* Archetype)
{
	return Archetype ? Archetype->Connections.EndDockRadiusScale : 0.78f;
}

ECollisionTraceFlag CollisionTraceFlagFromMode(ERouteMeshCollisionMode Mode)
{
	switch (Mode)
	{
	case ERouteMeshCollisionMode::None:
		return CTF_UseDefault;
	case ERouteMeshCollisionMode::SimpleAndComplex:
		return CTF_UseSimpleAndComplex;
	case ERouteMeshCollisionMode::ComplexAsSimple:
	default:
		return CTF_UseComplexAsSimple;
	}
}

float ApproximateSegmentLength(const FTraversalSkeletonSegment& Seg)
{
	float Length = 0.f;
	FVector Prev = USkeletonResolver::EvalBezier(Seg, 0.f);
	const int32 Steps = 12;
	for (int32 Step = 1; Step <= Steps; Step++)
	{
		const float T = (float)Step / (float)Steps;
		const FVector Current = USkeletonResolver::EvalBezier(Seg, T);
		Length += FVector::Distance(Prev, Current);
		Prev = Current;
	}
	return Length;
}

FVector EvalSegmentTangent(const FTraversalSkeletonSegment& Seg, float T)
{
	const float ClampedT = FMath::Clamp(T, 0.f, 1.f);
	const float OneMinusT = 1.f - ClampedT;
	return (
		3.f * OneMinusT * OneMinusT * (Seg.P1 - Seg.P0) +
		6.f * OneMinusT * ClampedT * (Seg.P2 - Seg.P1) +
		3.f * ClampedT * ClampedT * (Seg.P3 - Seg.P2)).GetSafeNormal();
}

FTransform MakeFrameTransform(const FVector& Position, const FVector& Forward)
{
	const FVector SafeForward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
	FVector UpHint = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(SafeForward, UpHint)) > 0.98f)
	{
		UpHint = FVector::RightVector;
	}

	return FTransform(FRotationMatrix::MakeFromXZ(SafeForward, UpHint).ToQuat(), Position);
}

void LogTopologySummary(const TArray<FTraversalTopologyNode>& Nodes, const TArray<FTraversalSkeletonSegment>& Skeleton)
{
	const FTraversalTopologyNode* SplitNode = nullptr;
	const FTraversalTopologyNode* MergeNode = nullptr;
	const FTraversalTopologyNode* SplitNodeStage2 = nullptr;
	const FTraversalTopologyNode* MergeNodeStage2 = nullptr;
	const FTraversalTopologyNode* StartCheckpoint = nullptr;
	const FTraversalTopologyNode* EndCheckpoint = nullptr;
	int32 BranchNodeCount = 0;
	int32 Stage1BranchNodeCount = 0;
	int32 Stage2BranchNodeCount = 0;
	int32 OptionalNodeCount = 0;
	int32 DecorativeNodeCount = 0;
	int32 HubNodeCount = 0;

	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.NodeType == ETopologyNodeType::SplitAnchor && Node.NormalizedDistance < 0.5f)
		{
			SplitNode = &Node;
		}
		else if (Node.NodeType == ETopologyNodeType::StartCheckpointSpace)
		{
			StartCheckpoint = &Node;
		}
		else if (Node.NodeType == ETopologyNodeType::EndCheckpointSpace)
		{
			EndCheckpoint = &Node;
		}
		else if (Node.NodeType == ETopologyNodeType::MergeAnchor && Node.NormalizedDistance < 0.7f)
		{
			MergeNode = &Node;
		}
		else if (Node.NodeType == ETopologyNodeType::SplitAnchor)
		{
			SplitNodeStage2 = &Node;
		}
		else if (Node.NodeType == ETopologyNodeType::MergeAnchor)
		{
			MergeNodeStage2 = &Node;
		}

		if (Node.BranchIndex != INDEX_NONE)
		{
			BranchNodeCount++;
			if (Node.BranchStage == 1)
			{
				Stage1BranchNodeCount++;
			}
			else if (Node.BranchStage == 2)
			{
				Stage2BranchNodeCount++;
			}
		}
		if (Node.bIsOptionalSideContent)
		{
			OptionalNodeCount++;
		}
		if (Node.bIsDecorativeDisconnected)
		{
			DecorativeNodeCount++;
		}
		if (Node.LogicalRole == ELogicalRouteNodeRole::Hub && !Node.bIsDecorativeDisconnected)
		{
			HubNodeCount++;
		}
	}

	float BranchALength = 0.f;
	float BranchBLength = 0.f;
	float BranchCLength = 0.f;
	float Stage1TotalLength = 0.f;
	float Stage2TotalLength = 0.f;
	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		if (!Seg.bGuaranteedPath)
		{
			continue;
		}

		if (Seg.BranchIndex == 0)
		{
			BranchALength += ApproximateSegmentLength(Seg);
		}
		else if (Seg.BranchIndex == 1)
		{
			BranchBLength += ApproximateSegmentLength(Seg);
		}
		else if (Seg.BranchIndex == 2)
		{
			BranchCLength += ApproximateSegmentLength(Seg);
		}

		if (Seg.BranchStage == 1)
		{
			Stage1TotalLength += ApproximateSegmentLength(Seg);
		}
		else if (Seg.BranchStage == 2)
		{
			Stage2TotalLength += ApproximateSegmentLength(Seg);
		}
	}

	UE_LOG(LogRouteGen, Log,
		TEXT("[TopologySummary] BranchNodes=%d OptionalNodes=%d DecorativeNodes=%d HubNodes=%d Stage1Nodes=%d Stage2Nodes=%d SplitCount=%d MergeCount=%d BranchA_Length=%.0fcm BranchB_Length=%.0fcm BranchC_Length=%.0fcm Stage1_Length=%.0fcm Stage2_Length=%.0fcm"),
		BranchNodeCount,
		OptionalNodeCount,
		DecorativeNodeCount,
		HubNodeCount,
		Stage1BranchNodeCount,
		Stage2BranchNodeCount,
		(SplitNode ? 1 : 0) + (SplitNodeStage2 ? 1 : 0),
		(MergeNode ? 1 : 0) + (MergeNodeStage2 ? 1 : 0),
		BranchALength,
		BranchBLength,
		BranchCLength,
		Stage1TotalLength,
		Stage2TotalLength);

	if (SplitNode)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] SplitA=(%.0f, %.0f, %.0f)"),
			SplitNode->WorldPosition.X, SplitNode->WorldPosition.Y, SplitNode->WorldPosition.Z);
	}

	if (StartCheckpoint)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] StartCheckpoint=(%.0f, %.0f, %.0f) Shape=%d Radius=%.0f"),
			StartCheckpoint->WorldPosition.X, StartCheckpoint->WorldPosition.Y, StartCheckpoint->WorldPosition.Z,
			(int32)StartCheckpoint->CheckpointSpaceShape,
			StartCheckpoint->PreferredRadius);
	}

	if (EndCheckpoint)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] EndCheckpoint=(%.0f, %.0f, %.0f) Shape=%d Radius=%.0f"),
			EndCheckpoint->WorldPosition.X, EndCheckpoint->WorldPosition.Y, EndCheckpoint->WorldPosition.Z,
			(int32)EndCheckpoint->CheckpointSpaceShape,
			EndCheckpoint->PreferredRadius);
	}

	if (MergeNode)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] MergeA=(%.0f, %.0f, %.0f)"),
			MergeNode->WorldPosition.X, MergeNode->WorldPosition.Y, MergeNode->WorldPosition.Z);
	}

	if (SplitNodeStage2)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] SplitB=(%.0f, %.0f, %.0f)"),
			SplitNodeStage2->WorldPosition.X, SplitNodeStage2->WorldPosition.Y, SplitNodeStage2->WorldPosition.Z);
	}

	if (MergeNodeStage2)
	{
		UE_LOG(LogRouteGen, Log, TEXT("[TopologySummary] MergeFinal=(%.0f, %.0f, %.0f)"),
			MergeNodeStage2->WorldPosition.X, MergeNodeStage2->WorldPosition.Y, MergeNodeStage2->WorldPosition.Z);
	}
}

FString SanitizeLogToken(const FString& InValue)
{
	FString Out = InValue;
	Out.ReplaceInline(TEXT(" "), TEXT("_"));
	Out.ReplaceInline(TEXT("/"), TEXT("_"));
	Out.ReplaceInline(TEXT("\\"), TEXT("_"));
	Out.ReplaceInline(TEXT(":"), TEXT("_"));
	return Out;
}
}

ATraversalRouteActor::ATraversalRouteActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SonarField = CreateDefaultSubobject<USonarFieldComponent>(TEXT("SonarField"));

	RouteStartMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("RouteStartMarker"));
	RouteStartMarker->SetupAttachment(RootComponent);
	ConfigureEndpointMarker(RouteStartMarker, FColor::Green);

	RouteStartDockMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("RouteStartDockMarker"));
	RouteStartDockMarker->SetupAttachment(RootComponent);
	ConfigureEndpointMarker(RouteStartDockMarker, FColor::Cyan);

	RouteEndMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("RouteEndMarker"));
	RouteEndMarker->SetupAttachment(RootComponent);
	ConfigureEndpointMarker(RouteEndMarker, FColor(255, 196, 0));

	RouteEndDockMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("RouteEndDockMarker"));
	RouteEndDockMarker->SetupAttachment(RootComponent);
	ConfigureEndpointMarker(RouteEndDockMarker, FColor::Magenta);

	PlacedRouteStartOverride = CreateDefaultSubobject<UArrowComponent>(TEXT("PlacedRouteStartOverride"));
	PlacedRouteStartOverride->SetupAttachment(RootComponent);
	ConfigurePlacedEndpointOverrideMarker(PlacedRouteStartOverride, FColor::Green);

	PlacedRouteStartDockOverride = CreateDefaultSubobject<UArrowComponent>(TEXT("PlacedRouteStartDockOverride"));
	PlacedRouteStartDockOverride->SetupAttachment(RootComponent);
	ConfigurePlacedEndpointOverrideMarker(PlacedRouteStartDockOverride, FColor::Cyan);

	PlacedRouteEndOverride = CreateDefaultSubobject<UArrowComponent>(TEXT("PlacedRouteEndOverride"));
	PlacedRouteEndOverride->SetupAttachment(RootComponent);
	ConfigurePlacedEndpointOverrideMarker(PlacedRouteEndOverride, FColor(255, 196, 0));

	PlacedRouteEndDockOverride = CreateDefaultSubobject<UArrowComponent>(TEXT("PlacedRouteEndDockOverride"));
	PlacedRouteEndDockOverride->SetupAttachment(RootComponent);
	ConfigurePlacedEndpointOverrideMarker(PlacedRouteEndDockOverride, FColor::Magenta);

	RefreshEndpointDebugMarkers();
	RefreshPlacedEndpointOverrideMarkers();
}

void ATraversalRouteActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshEndpointDebugMarkers();
	RefreshPlacedEndpointOverrideMarkers();
}

void ATraversalRouteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATraversalRouteActor, RouteNetSpec);
}

void ATraversalRouteActor::BeginPlay()
{
	Super::BeginPlay();

	RefreshEndpointDebugMarkers();
	RefreshPlacedEndpointOverrideMarkers();

	SetBakedStaticMeshRuntimeActive(bUseBakedStaticMeshAtRuntime);

	if (bUseBakedStaticMeshAtRuntime
		&& ((BakedStaticMeshAsset && BakedRouteHash != 0) || (bAutoResolveBakedAssetFromHash && ResolveBuildHashForCurrentState() != 0)))
	{
		TryResolveBakedAssetForHash(ResolveBuildHashForCurrentState());
	}

	RebuildManagedMeshComponentList();

	if (MeshComponents.Num() > 0)
	{
		UE_LOG(LogRouteGen, Log,
			TEXT("ATraversalRouteActor: reusing %d baked PMC components in BeginPlay; skipping automatic rebuild."),
			MeshComponents.Num());
	}
	else if (HasAuthority() && RouteNetSpec.GenSpec.RouteLengthMeters > 0.f)
	{
		BuildRouteFromSpec(RouteNetSpec.GenSpec, RouteNetSpec.Seeds);
	}
}

void ATraversalRouteActor::EnsureBakedStaticMeshComponent()
{
	if (IsValid(BakedStaticMeshComponent))
	{
		return;
	}

	TInlineComponentArray<UStaticMeshComponent*> FoundComponents(this);
	GetComponents(FoundComponents);
	for (UStaticMeshComponent* StaticMeshComp : FoundComponents)
	{
		if (!IsValid(StaticMeshComp))
		{
			continue;
		}
		if (StaticMeshComp->GetFName() == FName(BakedRouteComponentName))
		{
			BakedStaticMeshComponent = StaticMeshComp;
			return;
		}
	}

	BakedStaticMeshComponent = NewObject<UStaticMeshComponent>(this, FName(BakedRouteComponentName));
	BakedStaticMeshComponent->CreationMethod = EComponentCreationMethod::Instance;
	AddInstanceComponent(BakedStaticMeshComponent);
	BakedStaticMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	BakedStaticMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ApplySubmarineCollisionResponses(BakedStaticMeshComponent);
	BakedStaticMeshComponent->SetMobility(EComponentMobility::Static);
	BakedStaticMeshComponent->RegisterComponent();
}

void ATraversalRouteActor::ApplyBakedStaticMeshAsset(UStaticMesh* InMeshAsset)
{
	BakedStaticMeshAsset = InMeshAsset;
	EnsureBakedStaticMeshComponent();
	if (IsValid(BakedStaticMeshComponent))
	{
		BakedStaticMeshComponent->SetStaticMesh(InMeshAsset);
		BakedStaticMeshComponent->EmptyOverrideMaterials();
		RefreshVisualDebugMaterials();
		ApplySubmarineCollisionResponses(BakedStaticMeshComponent);
		SetBakedStaticMeshRuntimeActive(bUseBakedStaticMeshAtRuntime);
	}
}

void ATraversalRouteActor::SetBakedStaticMeshRuntimeActive(bool bActive)
{
	if (!IsValid(BakedStaticMeshComponent))
	{
		return;
	}

	const bool bHasMesh = IsValid(BakedStaticMeshAsset) && IsValid(BakedStaticMeshComponent->GetStaticMesh());
	const bool bShouldBeActive = bActive && bHasMesh;
	const ERouteMeshCollisionMode ActiveBakedCollisionMode = ResolveCollisionModeForStrategy(SurfaceBuildSettings, true);

	BakedStaticMeshComponent->SetVisibility(bShouldBeActive);
	BakedStaticMeshComponent->SetHiddenInGame(!bShouldBeActive);
	BakedStaticMeshComponent->SetCollisionEnabled(
		bShouldBeActive
			? RuntimeCollisionEnabledFromMode(ActiveBakedCollisionMode)
			: ECollisionEnabled::NoCollision);
}

void ATraversalRouteActor::ConfigureCampaignDebugView(bool bEnableSegmentTint,
	UMaterialInterface* InSegmentDebugMaterial,
	const FLinearColor& InSegmentTintColor,
	bool bEnableRoleVertexDebug)
{
	bCampaignSegmentTintEnabled = bEnableSegmentTint && IsValid(InSegmentDebugMaterial);
	bCampaignRoleVertexDebugEnabled = bEnableRoleVertexDebug;
	CampaignSegmentTintColor = InSegmentTintColor;
	CampaignSegmentDebugMaterial = InSegmentDebugMaterial;
	CampaignSegmentDebugMID = nullptr;
	RefreshVisualDebugMaterials();
}

void ATraversalRouteActor::ClearCampaignDebugView()
{
	bCampaignSegmentTintEnabled = false;
	bCampaignRoleVertexDebugEnabled = false;
	CampaignSegmentTintColor = FLinearColor::White;
	CampaignSegmentDebugMaterial = nullptr;
	CampaignSegmentDebugMID = nullptr;
	RefreshVisualDebugMaterials();
}

void ATraversalRouteActor::SetCampaignExternalUnionBrushes(const TArray<FVolumeBrushDef>& InBrushes)
{
	CampaignExternalUnionBrushes = InBrushes;
}

void ATraversalRouteActor::ClearCampaignExternalUnionBrushes()
{
	CampaignExternalUnionBrushes.Reset();
}

void ATraversalRouteActor::RefreshVisualDebugMaterials()
{
	UMaterialInterface* BiomeMaterial = (BiomeAsset && !BiomeAsset->PrimaryMaterial.IsNull())
		? BiomeAsset->PrimaryMaterial.LoadSynchronous()
		: nullptr;

	UMaterialInterface* OverrideMaterial = nullptr;
	if (bCampaignRoleVertexDebugEnabled && bUseDebugVertexColorMaterial && IsValid(DebugVertexColorMaterial))
	{
		OverrideMaterial = DebugVertexColorMaterial;
	}
	else if (bCampaignSegmentTintEnabled && IsValid(CampaignSegmentDebugMaterial))
	{
		if (!IsValid(CampaignSegmentDebugMID))
		{
			CampaignSegmentDebugMID = UMaterialInstanceDynamic::Create(CampaignSegmentDebugMaterial, this);
		}
		if (IsValid(CampaignSegmentDebugMID))
		{
			CampaignSegmentDebugMID->SetVectorParameterValue(TEXT("SegmentColor"), CampaignSegmentTintColor);
			CampaignSegmentDebugMID->SetVectorParameterValue(TEXT("ColorTint"), CampaignSegmentTintColor);
			OverrideMaterial = CampaignSegmentDebugMID;
		}
	}

	for (UProceduralMeshComponent* PMC : MeshComponents)
	{
		if (!IsValid(PMC))
		{
			continue;
		}
		if (OverrideMaterial)
		{
			PMC->SetMaterial(0, OverrideMaterial);
		}
		else if (BiomeMaterial)
		{
			PMC->SetMaterial(0, BiomeMaterial);
		}
	}

	if (IsValid(BakedStaticMeshComponent))
	{
		if (OverrideMaterial)
		{
			BakedStaticMeshComponent->SetMaterial(0, OverrideMaterial);
		}
		else
		{
			BakedStaticMeshComponent->EmptyOverrideMaterials();
		}
	}
}

void ATraversalRouteActor::WriteRouteGenerationLogSnapshot(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds) const
{
	const FString LogDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RouteGenLogs"), TEXT("Routes"));
	IFileManager::Get().MakeDirectory(*LogDir, true);

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString SegmentToken = CampaignSegmentID.IsNone() ? TEXT("Standalone") : SanitizeLogToken(CampaignSegmentID.ToString());
	const FString FilePath = FPaths::Combine(LogDir, FString::Printf(TEXT("Route_%s_%s_Route%d_H%08X.log"), *Timestamp, *SegmentToken, Spec.RouteID, RouteNetSpec.BuildHash));

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Timestamp=%s"), *Timestamp));
	Lines.Add(FString::Printf(TEXT("CampaignSegmentID=%s"), *CampaignSegmentID.ToString()));
	Lines.Add(FString::Printf(TEXT("CampaignPathIndex=%d"), CampaignPathIndex));
	Lines.Add(FString::Printf(TEXT("ArchetypeID=%s"), ArchetypeAsset ? *ArchetypeAsset->ArchetypeID.ToString() : TEXT("None")));
	Lines.Add(FString::Printf(TEXT("BiomeID=%s"), BiomeAsset ? *BiomeAsset->BiomeID.ToString() : TEXT("None")));
	Lines.Add(FString::Printf(TEXT("RouteID=%d"), Spec.RouteID));
	Lines.Add(FString::Printf(TEXT("RouteLengthMeters=%.0f"), Spec.RouteLengthMeters));
	Lines.Add(FString::Printf(TEXT("ComplexityTier=%d"), (int32)Spec.ComplexityTier));
	Lines.Add(FString::Printf(TEXT("ForceStartInterfaceOnly=%d"), Spec.bForceStartInterfaceOnly));
	Lines.Add(FString::Printf(TEXT("ForceEndInterfaceOnly=%d"), Spec.bForceEndInterfaceOnly));
	Lines.Add(FString::Printf(TEXT("BuildHash=0x%08X"), RouteNetSpec.BuildHash));
	Lines.Add(FString::Printf(TEXT("Triangles=%d"), TotalTriangles));
	Lines.Add(FString::Printf(TEXT("Seed_Route=%d"), Seeds.RouteSeed));
	Lines.Add(FString::Printf(TEXT("Seed_Topology=%d"), Seeds.TopologySeed));
	Lines.Add(FString::Printf(TEXT("Seed_Volume=%d"), Seeds.VolumeSeed));
	Lines.Add(FString::Printf(TEXT("Seed_Organic=%d"), Seeds.OrganicSeed));
	Lines.Add(FString::Printf(TEXT("Seed_Semantic=%d"), Seeds.SemanticSeed));
	Lines.Add(FString::Printf(TEXT("Seed_Population=%d"), Seeds.PopulationSeed));
	Lines.Add(FString::Printf(TEXT("Validation_Pass=%d"), LastValidationReport.bPass));
	Lines.Add(FString::Printf(TEXT("Validation_ClearanceCm=%.0f"), LastValidationReport.MinObservedClearance));
	Lines.Add(FString::Printf(TEXT("Validation_Hubs=%d"), LastValidationReport.HubCount));
	Lines.Add(FString::Printf(TEXT("Validation_OptionalBranches=%d"), LastValidationReport.OptionalBranchCount));
	Lines.Add(FString::Printf(TEXT("Validation_OptionalPockets=%d"), LastValidationReport.OptionalPocketCount));
	Lines.Add(FString::Printf(TEXT("Validation_SelectedProfiles=%d"), LastValidationReport.SelectedProfileCount));
	for (const FString& FailReason : LastValidationReport.FailReasons)
	{
		Lines.Add(FString::Printf(TEXT("FailReason=%s"), *FailReason));
	}

	FFileHelper::SaveStringArrayToFile(Lines, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

uint32 ATraversalRouteActor::ResolveBuildHashForCurrentState() const
{
	if (RouteNetSpec.BuildHash != 0)
	{
		return RouteNetSpec.BuildHash;
	}

	const FRouteSeedCascade Seeds = UCampaignRouteCompiler::DeriveSeedCascade(DebugSpec);
	return ComputeBuildHash(DebugSpec, Seeds);
}

FString ATraversalRouteActor::BuildBakedAssetObjectPath(uint32 BuildHash) const
{
	FString PackageFolder = BakedAssetFolder;
	if (!PackageFolder.StartsWith(TEXT("/Game")))
	{
		PackageFolder = TEXT("/Game/GeneratedRoutes");
	}
	PackageFolder.RemoveFromEnd(TEXT("/"));

	const FString AssetName = bUseRouteHashInBakedAssetName
		? FString::Printf(TEXT("SM_Route_%08X"), BuildHash)
		: TEXT("SM_Route_Baked");
	return FString::Printf(TEXT("%s/%s.%s"), *PackageFolder, *AssetName, *AssetName);
}

bool ATraversalRouteActor::TryResolveBakedAssetForHash(uint32 BuildHash)
{
	if (!bUseBakedStaticMeshAtRuntime)
	{
		return false;
	}

	if (BuildHash == 0)
	{
		return false;
	}

	if (BakedStaticMeshAsset && BakedRouteHash == (int32)BuildHash)
	{
		ApplyBakedStaticMeshAsset(BakedStaticMeshAsset);
		return true;
	}

	if (!bAutoResolveBakedAssetFromHash)
	{
		return false;
	}

	const FString ObjectPath = BuildBakedAssetObjectPath(BuildHash);
	if (UStaticMesh* ResolvedAsset = LoadObject<UStaticMesh>(nullptr, *ObjectPath))
	{
		BakedRouteHash = (int32)BuildHash;
		ApplyBakedStaticMeshAsset(ResolvedAsset);
		UE_LOG(LogRouteGen, Log, TEXT("[Bake] Resolved baked asset from hash %08X at %s"), BuildHash, *ObjectPath);
		return true;
	}

	return false;
}

void ATraversalRouteActor::RebuildManagedMeshComponentList()
{
	MeshComponents.Reset();
	TotalTriangles = 0;

	TInlineComponentArray<UProceduralMeshComponent*> FoundComponents(this);
	GetComponents(FoundComponents);

	for (UProceduralMeshComponent* PMC : FoundComponents)
	{
		if (!IsManagedGeneratedRouteMeshComponent(PMC))
		{
			continue;
		}

		MeshComponents.Add(PMC);

		for (int32 SectionIdx = 0; SectionIdx < PMC->GetNumSections(); SectionIdx++)
		{
			if (const FProcMeshSection* Section = PMC->GetProcMeshSection(SectionIdx))
			{
				TotalTriangles += Section->ProcIndexBuffer.Num() / 3;
			}
		}
	}
}

void ATraversalRouteActor::UpdateRouteEndpointTransforms(const TArray<FTraversalTopologyNode>& Nodes,
	const TArray<FTraversalSkeletonSegment>& Skeleton)
{
	bHasRouteStartTransform = false;
	RouteStartTransform = FTransform::Identity;
	RouteStartRadiusCm = 0.f;
	bHasRouteStartDockTransform = false;
	RouteStartDockTransform = FTransform::Identity;
	RouteStartDockRadiusCm = 0.f;
	bHasRouteEndTransform = false;
	RouteEndTransform = FTransform::Identity;
	RouteEndRadiusCm = 0.f;
	bHasRouteEndDockTransform = false;
	RouteEndDockTransform = FTransform::Identity;
	RouteEndDockRadiusCm = 0.f;

	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.NodeType == ETopologyNodeType::StartCheckpointSpace)
		{
			for (const FTraversalSkeletonSegment& Seg : Skeleton)
			{
				if (Seg.StartNodeID == Node.NodeID)
				{
					const FVector Forward = EvalSegmentTangent(Seg, 0.05f);
					const FVector DockForward = -Forward;
					const float DockOffset = ResolveStartDockLengthCm(ArchetypeAsset);
					RouteStartTransform = MakeFrameTransform(Node.WorldPosition, Forward);
					bHasRouteStartTransform = true;
					RouteStartRadiusCm = Node.PreferredRadius;
					RouteStartDockTransform = MakeFrameTransform(Node.WorldPosition + DockForward * DockOffset, DockForward);
					bHasRouteStartDockTransform = true;
					RouteStartDockRadiusCm = FMath::Max(1200.f, Node.PreferredRadius * ResolveStartDockRadiusScale(ArchetypeAsset));
					break;
				}
			}
			if (!bHasRouteStartTransform)
			{
				RouteStartTransform = FTransform(Node.WorldPosition);
				bHasRouteStartTransform = true;
				RouteStartRadiusCm = Node.PreferredRadius;
				RouteStartDockTransform = RouteStartTransform;
				bHasRouteStartDockTransform = true;
				RouteStartDockRadiusCm = Node.PreferredRadius;
			}
		}
		else if (Node.NodeType == ETopologyNodeType::EndCheckpointSpace)
		{
			for (const FTraversalSkeletonSegment& Seg : Skeleton)
			{
				if (Seg.EndNodeID == Node.NodeID)
				{
					const FVector Forward = EvalSegmentTangent(Seg, 0.95f);
					const FVector DockForward = Forward;
					const float DockOffset = ResolveEndDockLengthCm(ArchetypeAsset);
					RouteEndTransform = MakeFrameTransform(Node.WorldPosition, Forward);
					bHasRouteEndTransform = true;
					RouteEndRadiusCm = Node.PreferredRadius;
					RouteEndDockTransform = MakeFrameTransform(Node.WorldPosition + DockForward * DockOffset, DockForward);
					bHasRouteEndDockTransform = true;
					RouteEndDockRadiusCm = FMath::Max(1200.f, Node.PreferredRadius * ResolveEndDockRadiusScale(ArchetypeAsset));
					break;
				}
			}
			if (!bHasRouteEndTransform)
			{
				RouteEndTransform = FTransform(Node.WorldPosition);
				bHasRouteEndTransform = true;
				RouteEndRadiusCm = Node.PreferredRadius;
				RouteEndDockTransform = RouteEndTransform;
				bHasRouteEndDockTransform = true;
				RouteEndDockRadiusCm = Node.PreferredRadius;
			}
		}
	}

	if (bHasRouteStartTransform && bHasRouteEndTransform && bHasRouteStartDockTransform && bHasRouteEndDockTransform)
	{
		return;
	}

	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		if (!Seg.bGuaranteedPath)
		{
			continue;
		}

		const FVector StartPoint = USkeletonResolver::EvalBezier(Seg, 0.2f);
		const FVector Forward = EvalSegmentTangent(Seg, 0.2f);
		if (!bHasRouteStartTransform)
		{
			RouteStartTransform = MakeFrameTransform(StartPoint, Forward);
			bHasRouteStartTransform = true;
			RouteStartRadiusCm = Seg.StartRadius;
		}
		if (!bHasRouteStartDockTransform)
		{
			RouteStartDockTransform = RouteStartTransform;
			bHasRouteStartDockTransform = true;
			RouteStartDockRadiusCm = Seg.StartRadius;
		}

		if (!bHasRouteEndTransform)
		{
			const FVector EndPoint = USkeletonResolver::EvalBezier(Seg, 0.8f);
			const FVector EndForward = EvalSegmentTangent(Seg, 0.8f);
			RouteEndTransform = MakeFrameTransform(EndPoint, EndForward);
			bHasRouteEndTransform = true;
			RouteEndRadiusCm = Seg.EndRadius;
		}
		if (!bHasRouteEndDockTransform)
		{
			RouteEndDockTransform = RouteEndTransform;
			bHasRouteEndDockTransform = true;
			RouteEndDockRadiusCm = Seg.EndRadius;
		}
		break;
	}

	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (!bHasRouteStartTransform && (Node.NodeType == ETopologyNodeType::StartAnchor || Node.NodeType == ETopologyNodeType::EntryBuffer))
		{
			RouteStartTransform = FTransform(Node.WorldPosition);
			bHasRouteStartTransform = true;
			RouteStartRadiusCm = Node.PreferredRadius;
			if (!bHasRouteStartDockTransform)
			{
				RouteStartDockTransform = RouteStartTransform;
				bHasRouteStartDockTransform = true;
				RouteStartDockRadiusCm = Node.PreferredRadius;
			}
		}
		if (!bHasRouteEndTransform && (Node.NodeType == ETopologyNodeType::ExitApproach || Node.NodeType == ETopologyNodeType::ExitAnchor))
		{
			RouteEndTransform = FTransform(Node.WorldPosition);
			bHasRouteEndTransform = true;
			RouteEndRadiusCm = Node.PreferredRadius;
			if (!bHasRouteEndDockTransform)
			{
				RouteEndDockTransform = RouteEndTransform;
				bHasRouteEndDockTransform = true;
				RouteEndDockRadiusCm = Node.PreferredRadius;
			}
		}
	}

	RefreshEndpointDebugMarkers();
	LogRouteEndpointDebugSummaryInternal();
}

FTransform ATraversalRouteActor::GetRouteStartTransformWorld() const
{
	if (bUsePlacedEndpointOverrides && IsValid(PlacedRouteStartOverride))
	{
		return PlacedRouteStartOverride->GetComponentTransform();
	}

	return RouteStartTransform * GetActorTransform();
}

FTransform ATraversalRouteActor::GetRouteEndTransformWorld() const
{
	if (bUsePlacedEndpointOverrides && IsValid(PlacedRouteEndOverride))
	{
		return PlacedRouteEndOverride->GetComponentTransform();
	}

	return RouteEndTransform * GetActorTransform();
}

FTransform ATraversalRouteActor::GetRouteStartDockTransformWorld() const
{
	if (bUsePlacedEndpointOverrides && IsValid(PlacedRouteStartDockOverride))
	{
		return PlacedRouteStartDockOverride->GetComponentTransform();
	}

	return RouteStartDockTransform * GetActorTransform();
}

FTransform ATraversalRouteActor::GetRouteEndDockTransformWorld() const
{
	if (bUsePlacedEndpointOverrides && IsValid(PlacedRouteEndDockOverride))
	{
		return PlacedRouteEndDockOverride->GetComponentTransform();
	}

	return RouteEndDockTransform * GetActorTransform();
}

void ATraversalRouteActor::LogRouteEndpointDebugSummary()
{
	LogRouteEndpointDebugSummaryInternal();
}

void ATraversalRouteActor::CopyComputedEndpointsToPlacedOverrides()
{
	if (IsValid(PlacedRouteStartOverride))
	{
		PlacedRouteStartOverride->SetRelativeTransform(RouteStartTransform);
	}

	if (IsValid(PlacedRouteStartDockOverride))
	{
		PlacedRouteStartDockOverride->SetRelativeTransform(RouteStartDockTransform);
	}

	if (IsValid(PlacedRouteEndOverride))
	{
		PlacedRouteEndOverride->SetRelativeTransform(RouteEndTransform);
	}

	if (IsValid(PlacedRouteEndDockOverride))
	{
		PlacedRouteEndDockOverride->SetRelativeTransform(RouteEndDockTransform);
	}

	bUsePlacedEndpointOverrides = true;
	RefreshPlacedEndpointOverrideMarkers();
	LogRouteEndpointDebugSummaryInternal();
}

void ATraversalRouteActor::RefreshEndpointDebugMarkers()
{
	UpdateEndpointDebugMarker(RouteStartMarker, RouteStartTransform, bHasRouteStartTransform, RouteStartRadiusCm);
	UpdateEndpointDebugMarker(RouteStartDockMarker, RouteStartDockTransform, bHasRouteStartDockTransform, RouteStartDockRadiusCm);
	UpdateEndpointDebugMarker(RouteEndMarker, RouteEndTransform, bHasRouteEndTransform, RouteEndRadiusCm);
	UpdateEndpointDebugMarker(RouteEndDockMarker, RouteEndDockTransform, bHasRouteEndDockTransform, RouteEndDockRadiusCm);
}

void ATraversalRouteActor::RefreshPlacedEndpointOverrideMarkers()
{
	const bool bShowPlacedMarkers = bUsePlacedEndpointOverrides;
	if (IsValid(PlacedRouteStartOverride))
	{
		PlacedRouteStartOverride->SetVisibility(bShowPlacedMarkers);
	}
	if (IsValid(PlacedRouteStartDockOverride))
	{
		PlacedRouteStartDockOverride->SetVisibility(bShowPlacedMarkers);
	}
	if (IsValid(PlacedRouteEndOverride))
	{
		PlacedRouteEndOverride->SetVisibility(bShowPlacedMarkers);
	}
	if (IsValid(PlacedRouteEndDockOverride))
	{
		PlacedRouteEndDockOverride->SetVisibility(bShowPlacedMarkers);
	}
}

void ATraversalRouteActor::UpdateEndpointDebugMarker(UArrowComponent* Marker,
	const FTransform& EndpointTransform,
	bool bHasTransform,
	float RadiusCm)
{
	if (!IsValid(Marker))
	{
		return;
	}

	const bool bShowMarker = bShowEndpointDebugMarkers && bHasTransform;
	Marker->SetVisibility(bShowMarker);
	Marker->SetHiddenInGame(true);

	if (!bShowMarker)
	{
		return;
	}

	Marker->SetRelativeTransform(EndpointTransform);
	Marker->ArrowLength = FMath::Clamp(RadiusCm * 0.4f, 300.f, 3000.f);
	Marker->ArrowSize = FMath::Clamp(RadiusCm / 2000.f, 1.f, 4.f);
}

void ATraversalRouteActor::LogRouteEndpointDebugSummaryInternal() const
{
	const FTransform ActorTransform = GetActorTransform();
	const FTransform StartWorld = RouteStartTransform * ActorTransform;
	const FTransform StartDockWorld = RouteStartDockTransform * ActorTransform;
	const FTransform EndWorld = RouteEndTransform * ActorTransform;
	const FTransform EndDockWorld = RouteEndDockTransform * ActorTransform;

	const float StartDockOffsetCm = bHasRouteStartTransform && bHasRouteStartDockTransform
		? FVector::Distance(StartWorld.GetLocation(), StartDockWorld.GetLocation())
		: 0.f;
	const float EndDockOffsetCm = bHasRouteEndTransform && bHasRouteEndDockTransform
		? FVector::Distance(EndWorld.GetLocation(), EndDockWorld.GetLocation())
		: 0.f;

	UE_LOG(
		LogRouteGen,
		Log,
		TEXT("[RouteEndpoints] Override=%d | Start=%d StartDock=%d End=%d EndDock=%d | StartRadius=%.0f StartDockRadius=%.0f EndRadius=%.0f EndDockRadius=%.0f | StartDockOffset=%.0f EndDockOffset=%.0f"),
		bUsePlacedEndpointOverrides ? 1 : 0,
		bHasRouteStartTransform ? 1 : 0,
		bHasRouteStartDockTransform ? 1 : 0,
		bHasRouteEndTransform ? 1 : 0,
		bHasRouteEndDockTransform ? 1 : 0,
		RouteStartRadiusCm,
		RouteStartDockRadiusCm,
		RouteEndRadiusCm,
		RouteEndDockRadiusCm,
		StartDockOffsetCm,
		EndDockOffsetCm);

	if (bHasRouteStartTransform)
	{
		UE_LOG(
			LogRouteGen,
			Log,
			TEXT("[RouteEndpoints] StartWorld=(%.0f, %.0f, %.0f) Forward=(%.2f, %.2f, %.2f)"),
			StartWorld.GetLocation().X,
			StartWorld.GetLocation().Y,
			StartWorld.GetLocation().Z,
			StartWorld.GetRotation().GetForwardVector().X,
			StartWorld.GetRotation().GetForwardVector().Y,
			StartWorld.GetRotation().GetForwardVector().Z);
	}

	if (bHasRouteStartDockTransform)
	{
		UE_LOG(
			LogRouteGen,
			Log,
			TEXT("[RouteEndpoints] StartDockWorld=(%.0f, %.0f, %.0f) Forward=(%.2f, %.2f, %.2f)"),
			StartDockWorld.GetLocation().X,
			StartDockWorld.GetLocation().Y,
			StartDockWorld.GetLocation().Z,
			StartDockWorld.GetRotation().GetForwardVector().X,
			StartDockWorld.GetRotation().GetForwardVector().Y,
			StartDockWorld.GetRotation().GetForwardVector().Z);
	}

	if (bHasRouteEndTransform)
	{
		UE_LOG(
			LogRouteGen,
			Log,
			TEXT("[RouteEndpoints] EndWorld=(%.0f, %.0f, %.0f) Forward=(%.2f, %.2f, %.2f)"),
			EndWorld.GetLocation().X,
			EndWorld.GetLocation().Y,
			EndWorld.GetLocation().Z,
			EndWorld.GetRotation().GetForwardVector().X,
			EndWorld.GetRotation().GetForwardVector().Y,
			EndWorld.GetRotation().GetForwardVector().Z);
	}

	if (bHasRouteEndDockTransform)
	{
		UE_LOG(
			LogRouteGen,
			Log,
			TEXT("[RouteEndpoints] EndDockWorld=(%.0f, %.0f, %.0f) Forward=(%.2f, %.2f, %.2f)"),
			EndDockWorld.GetLocation().X,
			EndDockWorld.GetLocation().Y,
			EndDockWorld.GetLocation().Z,
			EndDockWorld.GetRotation().GetForwardVector().X,
			EndDockWorld.GetRotation().GetForwardVector().Y,
			EndDockWorld.GetRotation().GetForwardVector().Z);
	}
}

uint32 ATraversalRouteActor::ComputeBuildHash(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds) const
{
	uint32 H = 2166136261U;

	MixInt(H, Spec.CampaignSeed);
	MixInt(H, Spec.RouteID);
	MixInt(H, Spec.StartCheckpointID);
	MixInt(H, Spec.EndCheckpointID);
	MixFloat(H, Spec.RouteLengthMeters);
	MixFloat(H, Spec.StartDepthMeters);
	MixFloat(H, Spec.EndDepthMeters);
	MixInt(H, Spec.BiomeID);
	MixInt(H, Spec.DifficultyTier);
	MixInt(H, (int32)Spec.ComplexityTier);
	MixInt(H, Spec.CrewSizeHint);
	MixInt(H, (int32)Spec.Archetype);
	MixFloat(H, Spec.Envelope.SubLength);
	MixFloat(H, Spec.Envelope.SubWidth);
	MixFloat(H, Spec.Envelope.SubHeight);
	MixFloat(H, Spec.Envelope.HardClearance);
	MixFloat(H, Spec.Envelope.PreferredClearance);
	MixFloat(H, Spec.Envelope.MinTurnRadius);
	MixFloat(H, Spec.Envelope.PreferredCombatRadius);
	MixFloat(H, Spec.Envelope.MinSightline);

	MixInt(H, Seeds.RouteSeed);
	MixInt(H, Seeds.TopologySeed);
	MixInt(H, Seeds.VolumeSeed);
	MixInt(H, Seeds.OrganicSeed);
	MixInt(H, Seeds.SemanticSeed);
	MixInt(H, Seeds.PopulationSeed);

	MixInt(H, (int32)SurfaceBuildSettings.BoundsCollisionStrategy);
	MixBool(H, SurfaceBuildSettings.bEnableVisualSurfaceSmoothing);
	MixInt(H, SurfaceBuildSettings.SurfaceSmoothingIterations);
	MixFloat(H, SurfaceBuildSettings.SurfaceSmoothingStrength, 1000.f);
	MixFloat(H, SurfaceBuildSettings.SurfaceSmoothingMaxDisplacementCm);
	MixBool(H, SurfaceBuildSettings.bUseWeightedVertexNormals);
	MixBool(H, SurfaceBuildSettings.bBatchRuntimeMeshSections);
	MixInt(H, SurfaceBuildSettings.RuntimeMeshSectionBatchSize);
	MixInt(H, (int32)SurfaceBuildSettings.RuntimeMeshCollisionMode);
	MixInt(H, (int32)SurfaceBuildSettings.BakedMeshCollisionMode);
	MixFloat(H, SurfaceBuildSettings.CollisionProxyInflationCm);

	for (const FVolumeBrushDef& Brush : CampaignExternalUnionBrushes)
	{
		MixInt(H, (int32)Brush.BrushType);
		MixFloat(H, Brush.CenterA.X);
		MixFloat(H, Brush.CenterA.Y);
		MixFloat(H, Brush.CenterA.Z);
		MixFloat(H, Brush.CenterB.X);
		MixFloat(H, Brush.CenterB.Y);
		MixFloat(H, Brush.CenterB.Z);
		MixFloat(H, Brush.Radius);
		MixFloat(H, Brush.Smoothness);
		MixInt(H, (int32)Brush.LogicalRole);
		MixInt(H, Brush.BranchIndex);
	}

	if (ArchetypeAsset)
	{
		MixString(H, ArchetypeAsset->GetPathName());
		MixName(H, ArchetypeAsset->ArchetypeID);
		MixInt(H, (int32)ArchetypeAsset->ArchetypeType);
		MixFloat(H, ArchetypeAsset->RouteLengthMeters);
		MixBool(H, ArchetypeAsset->bUseGroupedConstrainedAuthoring);
		MixBool(H, ArchetypeAsset->bUseConstrainedGraphPattern);
		MixBool(H, ArchetypeAsset->bUseSplitMergePattern);
		MixBool(H, ArchetypeAsset->bUseMultiStageSplitPattern);
		MixInt(H, (int32)ArchetypeAsset->DefaultComplexityTier);
		MixFloat(H, ArchetypeAsset->Checkpoints.StartRadiusCm);
		MixFloat(H, ArchetypeAsset->Checkpoints.EndRadiusCm);
		MixInt(H, (int32)ArchetypeAsset->Checkpoints.StartShape);
		MixInt(H, (int32)ArchetypeAsset->Checkpoints.EndShape);
		MixFloat(H, ArchetypeAsset->Connections.StartDockLengthCm);
		MixFloat(H, ArchetypeAsset->Connections.EndDockLengthCm);
		MixFloat(H, ArchetypeAsset->Connections.StartDockRadiusScale, 1000.f);
		MixFloat(H, ArchetypeAsset->Connections.EndDockRadiusScale, 1000.f);
		MixFloat(H, ArchetypeAsset->Connections.PreferredCampaignOverlapCm);
		MixInt(H, ArchetypeAsset->Flow.MainSpineNodeCount);
		MixInt(H, ArchetypeAsset->Flow.MaxSplitAnchors);
		MixInt(H, ArchetypeAsset->Flow.MaxBranchFanout);
		MixFloat(H, ArchetypeAsset->Flow.RejoinChance);
		MixInt(H, ArchetypeAsset->Complexity.MaxHubCount);
		MixInt(H, ArchetypeAsset->Complexity.MaxOptionalSideBranches);
		MixInt(H, ArchetypeAsset->Complexity.MaxPocketDepth);
		MixFloat(H, ArchetypeAsset->Volumes.TrunkRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.BranchARadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.BranchBRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.BranchCRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.MergeRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.HubRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.PocketRadiusCm);
		MixFloat(H, ArchetypeAsset->Volumes.JunctionTransitionStartT, 1000.f);
		MixFloat(H, ArchetypeAsset->Volumes.JunctionTransitionSpanT, 1000.f);
		MixFloat(H, ArchetypeAsset->Volumes.JunctionThroatScale, 1000.f);
		MixFloat(H, ArchetypeAsset->SpatialShape.BranchSeparationCm);
		MixFloat(H, ArchetypeAsset->SpatialShape.VerticalOffsetCm);
		MixFloat(H, ArchetypeAsset->SpatialShape.VerticalityBias, 1000.f);
		MixFloat(H, ArchetypeAsset->SpatialShape.SegmentCurvatureCm);
		MixFloat(H, ArchetypeAsset->SpatialShape.IntermediatePointJitterCm);
		MixFloat(H, ArchetypeAsset->SpatialShape.HubApproachCurvatureScale, 1000.f);
		MixFloat(H, ArchetypeAsset->SpatialShape.OptionalBranchCurvatureScale, 1000.f);
		MixFloat(H, ArchetypeAsset->Rhythm.BranchDensity, 1000.f);
		MixFloat(H, ArchetypeAsset->Rhythm.HubChance, 1000.f);
		MixFloat(H, ArchetypeAsset->Rhythm.PocketChance, 1000.f);
		MixBranchProfileSetHash(H, ArchetypeAsset->BranchProfileSet);
		for (const TSoftObjectPtr<URouteMotifDataAsset>& MotifPtr : ArchetypeAsset->AllowedMotifs)
		{
			MixString(H, MotifPtr.ToSoftObjectPath().ToString());
			if (const URouteMotifDataAsset* Motif = MotifPtr.LoadSynchronous())
			{
				MixName(H, Motif->MotifID);
				MixInt(H, (int32)Motif->PrimaryIntent);
				MixInt(H, (int32)Motif->PreferredWindow);
				MixInt(H, (int32)Motif->MinComplexity);
				MixInt(H, Motif->NodeCost);
				MixInt(H, Motif->HubCost);
				MixInt(H, Motif->OptionalCost);
				MixFloat(H, Motif->SelectionWeight);
				MixInt(H, Motif->BranchCount);
				MixBool(H, Motif->bRequiresHub);
				MixBool(H, Motif->bCanReconnect);
				MixInt(H, Motif->PocketDepth);
			}
		}
	}

	if (BiomeAsset)
	{
		MixString(H, BiomeAsset->GetPathName());
		MixName(H, BiomeAsset->BiomeID);
		MixFloat(H, BiomeAsset->NoiseFreq1, 1000000.f);
		MixFloat(H, BiomeAsset->NoiseFreq2, 1000000.f);
		MixFloat(H, BiomeAsset->NoiseCaveThreshold, 10000.f);
		MixFloat(H, BiomeAsset->OrganicAmplitude, 1000.f);
		MixFloat(H, BiomeAsset->LargeScaleWarpAmplitude);
		MixFloat(H, BiomeAsset->MediumNoiseAmplitude);
		MixString(H, BiomeAsset->PrimaryMaterial.ToSoftObjectPath().ToString());
		MixTagContainer(H, BiomeAsset->BiomeTags);
		MixBranchProfileSetHash(H, BiomeAsset->PreferredBranchProfileSet);
		MixInt(H, (int32)BiomeAsset->DefaultTraversalComplexity);
	}

	return H;
}

bool ATraversalRouteActor::BuildRouteFromSpec(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (IsValid(BakedStaticMeshComponent) && BakedRouteHash != 0 && BakedRouteHash != (int32)ComputeBuildHash(Spec, Seeds))
	{
		BakedStaticMeshComponent->SetVisibility(false);
		BakedStaticMeshComponent->SetHiddenInGame(true);
	}

	ClearMeshComponents();
	LastValidationReport = FRouteValidationReport();
	TotalTriangles = 0;

	const bool bOk = RunPipeline(Spec, Seeds);

	RouteNetSpec.GenSpec = Spec;
	RouteNetSpec.Seeds = Seeds;
	RouteNetSpec.BuildHash = ComputeBuildHash(Spec, Seeds);
	RouteNetSpec.BuildVersion = 1;
	LastSavedRecipe.GenSpec = Spec;
	LastSavedRecipe.Seeds = Seeds;
	LastSavedRecipe.ArchetypeID = ArchetypeAsset ? ArchetypeAsset->ArchetypeID : NAME_None;
	LastSavedRecipe.BiomeProfileID = BiomeAsset ? BiomeAsset->BiomeID : NAME_None;
	LastResolvedBranchProfileSet = ArchetypeAsset && !ArchetypeAsset->BranchProfileSet.IsNull()
		? ArchetypeAsset->BranchProfileSet
		: (BiomeAsset ? BiomeAsset->PreferredBranchProfileSet : TSoftObjectPtr<UBranchProfileSetDataAsset>());
	if (const UBranchProfileSetDataAsset* ProfileSet = LastResolvedBranchProfileSet.LoadSynchronous())
	{
		LastSavedRecipe.BranchProfileSetID = ProfileSet->ProfileSetID;
	}
	else
	{
		LastSavedRecipe.BranchProfileSetID = NAME_None;
	}
	LastSavedRecipe.ComplexityTier = Spec.ComplexityTier;
	LastSavedRecipe.BuildHash = RouteNetSpec.BuildHash;
	LastSavedRecipe.SchemaVersion = 1;

	if (bOk)
	{
		UE_LOG(LogRouteGen, Log, TEXT("ATraversalRouteActor: route built. Triangles=%d Hash=0x%08X"),
			TotalTriangles, RouteNetSpec.BuildHash);
		WriteRouteGenerationLogSnapshot(Spec, Seeds);
	}
	else
	{
		UE_LOG(LogRouteGen, Warning, TEXT("ATraversalRouteActor: route validation FAILED. %s"),
			LastValidationReport.FailReasons.Num() > 0
				? *LastValidationReport.FailReasons[0]
				: TEXT("Unknown reason"));
	}

	return bOk;
}

bool ATraversalRouteActor::RunPipeline(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds)
{
	if (TunnelDebugOptions.bEnableTunnelDebug)
	{
		UE_LOG(LogRouteGen, Log,
			TEXT("[TunnelDebug] Actor debug enabled. FilterToChunkWindow=%d Chunk=(%d,%d,%d) Radius=%d"),
			TunnelDebugOptions.bFilterToChunkWindow,
			TunnelDebugOptions.DebugChunkCoord.X,
			TunnelDebugOptions.DebugChunkCoord.Y,
			TunnelDebugOptions.DebugChunkCoord.Z,
			TunnelDebugOptions.DebugChunkRadius);
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C4 Topology..."));
	TArray<FTraversalTopologyNode> Nodes;
	UTraversalTopologyGenerator* TopoGen = NewObject<UTraversalTopologyGenerator>(this);
	if (!TopoGen->GenerateTopology(Spec, Seeds, ArchetypeAsset, Nodes))
	{
		return false;
	}
	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C4 done. Nodes=%d"), Nodes.Num());

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C5 Skeleton..."));
	TArray<FTraversalSkeletonSegment> Skeleton;
	USkeletonResolver* SkeletonRes = NewObject<USkeletonResolver>(this);
	if (ArchetypeAsset)
	{
		const bool bUseGroupedAuthoring = ArchetypeAsset->bUseGroupedConstrainedAuthoring;
		SkeletonRes->SegmentCurvatureCm = bUseGroupedAuthoring ? ArchetypeAsset->SpatialShape.SegmentCurvatureCm : 0.f;
		SkeletonRes->IntermediatePointJitterCm = bUseGroupedAuthoring ? ArchetypeAsset->SpatialShape.IntermediatePointJitterCm : 0.f;
		SkeletonRes->HubApproachCurvatureScale = bUseGroupedAuthoring ? ArchetypeAsset->SpatialShape.HubApproachCurvatureScale : 0.45f;
		SkeletonRes->OptionalBranchCurvatureScale = bUseGroupedAuthoring ? ArchetypeAsset->SpatialShape.OptionalBranchCurvatureScale : 1.2f;
		SkeletonRes->JunctionTransitionStartT = bUseGroupedAuthoring ? ArchetypeAsset->Volumes.JunctionTransitionStartT : 0.82f;
		SkeletonRes->JunctionTransitionSpanT = bUseGroupedAuthoring ? ArchetypeAsset->Volumes.JunctionTransitionSpanT : 0.18f;
		SkeletonRes->JunctionThroatScale = bUseGroupedAuthoring ? ArchetypeAsset->Volumes.JunctionThroatScale : 1.0f;
	}
	if (!SkeletonRes->ResolveSkeleton(Nodes, Skeleton))
	{
		return false;
	}
	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C5 done. Segments=%d TotalBrushes(approx)=%d"),
		Skeleton.Num(), Skeleton.Num() * 8);
	LogTopologySummary(Nodes, Skeleton);
	UpdateRouteEndpointTransforms(Nodes, Skeleton);
	{
		TSet<FName> SelectedProfileIDs;
		for (const FTraversalTopologyNode& Node : Nodes)
		{
			if (!Node.BranchProfileID.IsNone())
			{
				SelectedProfileIDs.Add(Node.BranchProfileID);
			}
		}
		LastSavedRecipe.SelectedBranchProfileIDs = SelectedProfileIDs.Array();
		LastSavedRecipe.SelectedBranchProfileIDs.Sort([](const FName& A, const FName& B)
		{
			return A.LexicalLess(B);
		});
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C6 Volume..."));
	FRouteFieldModel FieldModel;
	UNavigableVolumeGenerator* VolGen = NewObject<UNavigableVolumeGenerator>(this);
	if (!VolGen->BuildGuaranteedVolume(Spec, ArchetypeAsset, Nodes, Skeleton, &CampaignExternalUnionBrushes, FieldModel))
	{
		return false;
	}
	{
		int32 GuaranteedRender = 0;
		for (const auto& Pair : FieldModel.RenderField)
		{
			if (Pair.Value.bContainsGuaranteedPath)
			{
				GuaranteedRender++;
			}
		}
		UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C6 done. RenderChunks=%d (guaranteed=%d) SonarChunks=%d"),
			FieldModel.RenderField.Num(), GuaranteedRender, FieldModel.SonarField.Num());
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C7 Organic..."));
	UOrganicDeformationGenerator* OrgGen = NewObject<UOrganicDeformationGenerator>(this);
	OrgGen->ApplyDeformation(Spec, Seeds, BiomeAsset, FieldModel);
	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C7 done."));

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C8 Semantics..."));
	FRouteSemanticModel SemanticModel;
	URouteSemanticGenerator* SemGen = NewObject<URouteSemanticGenerator>(this);
	SemGen->BuildSemantics(Spec, Nodes, Skeleton, SemanticModel);
	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C8 done. Zones=%d Sockets=%d"),
		SemanticModel.Zones.Num(), SemanticModel.MissionSockets.Num());

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C9 Validation..."));
	URouteValidator* Validator = NewObject<URouteValidator>(this);
	LastValidationReport = Validator->Validate(Spec, Nodes, Skeleton, FieldModel, SemanticModel);
	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C9 done. Pass=%d Clearance=%.0fcm FailReasons=%d"),
		LastValidationReport.bPass, LastValidationReport.MinObservedClearance,
		LastValidationReport.FailReasons.Num());
	UE_LOG(LogRouteGen, Log,
		TEXT("[ValidationSummary] SplitCount=%d TraversableBranches=%d OptionalBranches=%d OptionalPockets=%d CanonicalBypasses=%d DecorativeCavities=%d Hubs=%d SelectedProfiles=%d MergeConnected=%d BranchA_Pass=%d BranchB_Pass=%d BranchC_Pass=%d BranchA_Clearance=%.0fcm BranchB_Clearance=%.0fcm BranchC_Clearance=%.0fcm MergeFailures=%d"),
		LastValidationReport.SplitCount,
		LastValidationReport.TraversableBranches,
		LastValidationReport.OptionalBranchCount,
		LastValidationReport.OptionalPocketCount,
		LastValidationReport.CanonicalBypassCount,
		LastValidationReport.DecorativeCavityCount,
		LastValidationReport.HubCount,
		LastValidationReport.SelectedProfileCount,
		LastValidationReport.bMergeConnected,
		LastValidationReport.bBranchA_Pass,
		LastValidationReport.bBranchB_Pass,
		LastValidationReport.bBranchC_Pass,
		LastValidationReport.MinClearanceBranchA,
		LastValidationReport.MinClearanceBranchB,
		LastValidationReport.MinClearanceBranchC,
		LastValidationReport.MergeFailures);
	if (!LastValidationReport.bPass)
	{
		return false;
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C10 MeshBuild..."));
	TArray<FRouteMeshChunkData> Chunks;
	URouteMeshBuilder* MeshBuilder = NewObject<URouteMeshBuilder>(this);
	if (!MeshBuilder->BuildMeshChunks(Spec, FieldModel, BiomeAsset, Chunks, &SurfaceBuildSettings, &TunnelDebugOptions))
	{
		return false;
	}
	{
		int32 TotalVerts = 0;
		int32 TotalTris = 0;
		int32 EmptyChunks = 0;
		for (const FRouteMeshChunkData& Chunk : Chunks)
		{
			TotalVerts += Chunk.Vertices.Num();
			TotalTris += Chunk.Triangles.Num() / 3;
			if (Chunk.Triangles.Num() == 0)
			{
				EmptyChunks++;
			}
		}
		UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] C10 done. Chunks=%d Verts=%d Tris=%d EmptyChunks=%d"),
			Chunks.Num(), TotalVerts, TotalTris, EmptyChunks);
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] SonarField init..."));
	SonarField->InitializeFromField(FieldModel);

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] SpawnMesh..."));
	SpawnMeshComponents(Chunks);

	UE_LOG(LogRouteGen, Log, TEXT("[Pipeline] Complete."));
	return true;
}

void ATraversalRouteActor::SpawnMeshComponentSection(const TArray<FRouteMeshChunkData>& Chunks, int32 StartIndex, int32 Count, int32 SectionIndex)
{
	UMaterialInterface* BiomeMaterial = nullptr;
	if (BiomeAsset && !BiomeAsset->PrimaryMaterial.IsNull())
	{
		BiomeMaterial = BiomeAsset->PrimaryMaterial.LoadSynchronous();
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	bool bSectionCollision = false;
	int32 SectionTriangleCount = 0;

	for (int32 ChunkIndex = StartIndex; ChunkIndex < StartIndex + Count && ChunkIndex < Chunks.Num(); ChunkIndex++)
	{
		const FRouteMeshChunkData& Chunk = Chunks[ChunkIndex];
		if (Chunk.Triangles.Num() == 0)
		{
			continue;
		}

		const int32 BaseVertex = Vertices.Num();
		Vertices.Append(Chunk.Vertices);
		Normals.Append(Chunk.Normals);
		UVs.Append(Chunk.UVs);
		Colors.Append(Chunk.VertexColors);
		for (int32 TriangleIndex : Chunk.Triangles)
		{
			Triangles.Add(BaseVertex + TriangleIndex);
		}
		bSectionCollision |= Chunk.bServerCollision;
		SectionTriangleCount += Chunk.Triangles.Num() / 3;
	}

	if (Triangles.Num() == 0)
	{
		return;
	}

	const FName ComponentName = MakeUniqueObjectName(
		this,
		UProceduralMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("RouteSection_%03d"), SectionIndex)));
	const EObjectFlags RouteMeshFlags = bPersistGeneratedRouteMeshInLevel
		? RF_Transactional
		: RF_Transient | RF_TextExportTransient;
	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this, ComponentName, RouteMeshFlags);
	PMC->ComponentTags.AddUnique(FName(GeneratedRouteComponentTag));
	if (bPersistGeneratedRouteMeshInLevel)
	{
		PMC->CreationMethod = EComponentCreationMethod::Instance;
		AddInstanceComponent(PMC);
	}
	else
	{
		AddOwnedComponent(PMC);
	}
	PMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PMC->RegisterComponent();

	PMC->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		Tangents,
		HasAuthority() && bSectionCollision && ResolveCollisionModeForStrategy(SurfaceBuildSettings, false) != ERouteMeshCollisionMode::None);
	PMC->SetCollisionEnabled(RuntimeCollisionEnabledFromMode(ResolveCollisionModeForStrategy(SurfaceBuildSettings, false)));
	PMC->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ApplySubmarineCollisionResponses(PMC);

	if (bUseDebugVertexColorMaterial && IsValid(DebugVertexColorMaterial))
	{
		PMC->SetMaterial(0, DebugVertexColorMaterial);
	}
	else if (BiomeMaterial)
	{
		PMC->SetMaterial(0, BiomeMaterial);
	}

	MeshComponents.Add(PMC);
	TotalTriangles += SectionTriangleCount;
}

void ATraversalRouteActor::SpawnMeshComponents(const TArray<FRouteMeshChunkData>& Chunks)
{
	if (IsValid(BakedStaticMeshComponent))
	{
		BakedStaticMeshComponent->SetVisibility(false);
		BakedStaticMeshComponent->SetHiddenInGame(true);
	}

	const int32 BatchSize = SurfaceBuildSettings.bBatchRuntimeMeshSections
		? FMath::Max(1, SurfaceBuildSettings.RuntimeMeshSectionBatchSize)
		: 1;

	int32 SectionIndex = 0;
	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ChunkIndex += BatchSize)
	{
		SpawnMeshComponentSection(Chunks, ChunkIndex, BatchSize, SectionIndex++);
	}

	UE_LOG(LogRouteGen, Log, TEXT("[SpawnMesh] Done. Spawned %d PMC sections. TotalTriangles=%d BatchSize=%d"),
		MeshComponents.Num(), TotalTriangles, BatchSize);
	RefreshVisualDebugMaterials();
}

void ATraversalRouteActor::ClearMeshComponents()
{
	RebuildManagedMeshComponentList();

	for (UProceduralMeshComponent* PMC : MeshComponents)
	{
		if (IsValid(PMC))
		{
			RemoveOwnedComponent(PMC);
			RemoveInstanceComponent(PMC);
			PMC->DestroyComponent();
		}
	}

	MeshComponents.Reset();
	TotalTriangles = 0;
}

void ATraversalRouteActor::OnRep_RouteNetSpec()
{
	if (!HasAuthority())
	{
		if (TryResolveBakedAssetForHash(RouteNetSpec.BuildHash))
		{
			UE_LOG(LogRouteGen, Log, TEXT("ATraversalRouteActor [Client]: reusing baked static mesh asset; skipping replicated rebuild."));
			return;
		}

		RebuildManagedMeshComponentList();
		if (MeshComponents.Num() > 0)
		{
			UE_LOG(LogRouteGen, Log,
				TEXT("ATraversalRouteActor [Client]: reusing %d baked PMC components; skipping replicated rebuild."),
				MeshComponents.Num());
			return;
		}

		ClearMeshComponents();
		RunPipeline(RouteNetSpec.GenSpec, RouteNetSpec.Seeds);
		UE_LOG(LogRouteGen, Log, TEXT("ATraversalRouteActor [Client]: rebuilt route. Hash=0x%08X"), RouteNetSpec.BuildHash);
	}
}

void ATraversalRouteActor::RebakeInEditor()
{
#if WITH_EDITOR
	Modify();
	FRouteSeedCascade Seeds = UCampaignRouteCompiler::DeriveSeedCascade(DebugSpec);
	BuildRouteFromSpec(DebugSpec, Seeds);
#endif
}

void ATraversalRouteActor::PurgeGeneratedRouteMeshComponents()
{
#if WITH_EDITOR
	Modify();
	RebuildManagedMeshComponentList();
	const int32 PurgedComponentCount = MeshComponents.Num();
	const int32 PurgedTriangleCount = TotalTriangles;
	ClearMeshComponents();
	MarkPackageDirty();
	UE_LOG(LogRouteGen, Warning,
		TEXT("[Persistence] Purged %d generated route PMCs from %s. PurgedTriangles=%d PersistInLevel=%d"),
		PurgedComponentCount,
		*GetName(),
		PurgedTriangleCount,
		bPersistGeneratedRouteMeshInLevel ? 1 : 0);
#endif
}

void ATraversalRouteActor::BakeCurrentRouteToStaticMeshAsset()
{
#if WITH_EDITOR
	RebuildManagedMeshComponentList();
	if (MeshComponents.Num() == 0)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Bake] No generated PMC chunks to bake."));
		return;
	}

	const uint32 BuildHash = ResolveBuildHashForCurrentState();
	const FString ObjectPath = BuildBakedAssetObjectPath(BuildHash);
	if (UStaticMesh* ExistingAsset = LoadObject<UStaticMesh>(nullptr, *ObjectPath))
	{
		UE_LOG(LogRouteGen, Log, TEXT("[Bake] Reusing existing baked asset %s"), *ObjectPath);
		BakedRouteHash = (int32)BuildHash;
		ApplyBakedStaticMeshAsset(ExistingAsset);
		if (bUseBakedStaticMeshAtRuntime && bReplaceGeneratedMeshWithBakedAsset)
		{
			ClearMeshComponents();
		}
		return;
	}

	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();
	auto VertexPositions = Attributes.GetVertexPositions();
	auto VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	auto VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	auto VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	auto VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	auto VertexInstanceColors = Attributes.GetVertexInstanceColors();
	VertexInstanceUVs.SetNumChannels(1);
	const FPolygonGroupID PolygonGroup = MeshDescription.CreatePolygonGroup();

	int32 TriangleCount = 0;
	for (UProceduralMeshComponent* PMC : MeshComponents)
	{
		if (!IsValid(PMC))
		{
			continue;
		}

		for (int32 SectionIndex = 0; SectionIndex < PMC->GetNumSections(); SectionIndex++)
		{
			const FProcMeshSection* Section = PMC->GetProcMeshSection(SectionIndex);
			if (!Section)
			{
				continue;
			}

			for (int32 TriIndex = 0; TriIndex + 2 < Section->ProcIndexBuffer.Num(); TriIndex += 3)
			{
				TArray<FVertexInstanceID> TriangleVertexInstances;
				TriangleVertexInstances.Reserve(3);
				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					const int32 ProcVertIndex = Section->ProcIndexBuffer[TriIndex + Corner];
					if (!Section->ProcVertexBuffer.IsValidIndex(ProcVertIndex))
					{
						continue;
					}

					const FProcMeshVertex& ProcVertex = Section->ProcVertexBuffer[ProcVertIndex];
					const FVertexID VertexID = MeshDescription.CreateVertex();
					VertexPositions[VertexID] = FVector3f(ProcVertex.Position);

					const FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
					VertexInstanceNormals[VertexInstanceID] = FVector3f(ProcVertex.Normal);
					VertexInstanceTangents[VertexInstanceID] = FVector3f(ProcVertex.Tangent.TangentX);
					VertexInstanceBinormalSigns[VertexInstanceID] = ProcVertex.Tangent.bFlipTangentY ? -1.f : 1.f;
					VertexInstanceUVs.Set(VertexInstanceID, 0, FVector2f(ProcVertex.UV0));
					VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(ProcVertex.Color));
					TriangleVertexInstances.Add(VertexInstanceID);
				}

				if (TriangleVertexInstances.Num() == 3)
				{
					MeshDescription.CreatePolygon(PolygonGroup, TriangleVertexInstances);
					TriangleCount++;
				}
			}
		}
	}

	if (TriangleCount == 0)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Bake] No triangles collected from PMC chunks."));
		return;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName = FPackageName::ObjectPathToObjectName(ObjectPath);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogRouteGen, Error, TEXT("[Bake] Failed to create package %s"), *PackageName);
		return;
	}

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();
	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDescription);

	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	const ERouteMeshCollisionMode ActiveBakedCollisionMode = ResolveCollisionModeForStrategy(SurfaceBuildSettings, true);
	BuildParams.bBuildSimpleCollision = ActiveBakedCollisionMode == ERouteMeshCollisionMode::SimpleAndComplex;
	BuildParams.bAllowCpuAccess = true;
	BuildParams.bFastBuild = true;
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams);

	if (UMaterialInterface* BiomeMaterial = (BiomeAsset && !BiomeAsset->PrimaryMaterial.IsNull())
		? BiomeAsset->PrimaryMaterial.LoadSynchronous()
		: nullptr)
	{
		StaticMesh->GetStaticMaterials().SetNum(1);
		StaticMesh->SetMaterial(0, BiomeMaterial);
	}

	StaticMesh->CreateBodySetup();
	if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
	{
		BodySetup->CollisionTraceFlag = CollisionTraceFlagFromMode(ActiveBakedCollisionMode);
	}

	StaticMesh->MarkPackageDirty();
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(StaticMesh);

	UE_LOG(LogRouteGen, Log, TEXT("[Bake] Created baked route asset %s Triangles=%d"), *ObjectPath, TriangleCount);
	BakedRouteHash = (int32)BuildHash;
	ApplyBakedStaticMeshAsset(StaticMesh);
	if (bUseBakedStaticMeshAtRuntime && bReplaceGeneratedMeshWithBakedAsset)
	{
		ClearMeshComponents();
	}
#endif
}
