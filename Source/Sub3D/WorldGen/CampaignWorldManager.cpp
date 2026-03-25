#include "CampaignWorldManager.h"

#include "CampaignRouteCompiler.h"
#include "BiomeFieldProfileDataAsset.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RouteConnectorActor.h"
#include "RouteArchetypeDataAsset.h"
#include "TraversalRouteActor.h"

namespace
{
FTransform MakeConnectorFacingTransform(const FVector& Position, const FVector& Forward)
{
	const FVector SafeForward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
	FVector UpHint = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(SafeForward, UpHint)) > 0.98f)
	{
		UpHint = FVector::RightVector;
	}

	return FTransform(FRotationMatrix::MakeFromXZ(SafeForward, UpHint).ToQuat(), Position);
}

float ResolvePreferredOverlapCm(const ATraversalRouteActor* CurrentRoute, const ATraversalRouteActor* PreviewRoute)
{
	const float CurrentOverlap = (CurrentRoute && CurrentRoute->ArchetypeAsset)
		? CurrentRoute->ArchetypeAsset->Connections.PreferredCampaignOverlapCm
		: 1800.f;
	const float PreviewOverlap = (PreviewRoute && PreviewRoute->ArchetypeAsset)
		? PreviewRoute->ArchetypeAsset->Connections.PreferredCampaignOverlapCm
		: 1800.f;
	return FMath::Max(0.f, FMath::Min(CurrentOverlap, PreviewOverlap));
}

FLinearColor ResolveSegmentPaletteColor(int32 SegmentIndex)
{
	static const FLinearColor Palette[] =
	{
		FLinearColor(0.95f, 0.35f, 0.35f),
		FLinearColor(0.25f, 0.70f, 1.00f),
		FLinearColor(0.95f, 0.80f, 0.25f),
		FLinearColor(0.45f, 0.90f, 0.45f),
		FLinearColor(0.85f, 0.45f, 0.95f),
		FLinearColor(1.00f, 0.55f, 0.20f),
		FLinearColor(0.20f, 0.90f, 0.85f),
		FLinearColor(0.90f, 0.90f, 0.90f)
	};
	return Palette[FMath::Abs(SegmentIndex) % UE_ARRAY_COUNT(Palette)];
}

FString SanitizeCampaignLogToken(const FString& InValue)
{
	FString Out = InValue;
	Out.ReplaceInline(TEXT(" "), TEXT("_"));
	Out.ReplaceInline(TEXT("/"), TEXT("_"));
	Out.ReplaceInline(TEXT("\\"), TEXT("_"));
	Out.ReplaceInline(TEXT(":"), TEXT("_"));
	return Out;
}
}

ACampaignWorldManager::ACampaignWorldManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACampaignWorldManager::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoBuildCurrentSegmentOnBeginPlay)
	{
		if (ProgressState.CurrentSegmentID.IsNone())
		{
			BuildRootSegmentInEditor();
		}
		else
		{
			BuildCurrentSegmentInEditor();
		}
	}
}

void ACampaignWorldManager::ResetCampaignProgress()
{
	ProgressState = FCampaignProgressState();
	ProgressState.MasterSeed = MasterSeed;

	if (CampaignGraph)
	{
		ProgressState.CurrentSegmentID = CampaignGraph->RootSegmentID;
		ProgressState.DiscoveredSegmentIDs.Add(CampaignGraph->RootSegmentID);
	}

	PreviewNextSegmentID = NAME_None;
}

void ACampaignWorldManager::BuildRootSegmentInEditor()
{
	ResetCampaignProgress();
	if (CampaignGraph)
	{
		BuildSegmentByID(CampaignGraph->RootSegmentID);
	}
}

void ACampaignWorldManager::BuildCurrentSegmentInEditor()
{
	if (ProgressState.CurrentSegmentID.IsNone() && CampaignGraph)
	{
		ProgressState.CurrentSegmentID = CampaignGraph->RootSegmentID;
	}

	if (!ProgressState.CurrentSegmentID.IsNone())
	{
		BuildSegmentByID(ProgressState.CurrentSegmentID);
	}
}

void ACampaignWorldManager::BuildCampaignSliceInEditor()
{
	ResetCampaignProgress();
	BuildCurrentSegmentInEditor();
	SelectFirstConnectedSegmentInEditor();
	PreviewSelectedNextSegmentAndConnectorInEditor();
}

void ACampaignWorldManager::AdvanceCampaignSliceInEditor()
{
	if (SelectedNextSegmentID.IsNone())
	{
		SelectFirstConnectedSegmentInEditor();
	}

	if (SelectedNextSegmentID.IsNone())
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] AdvanceCampaignSliceInEditor failed: no next segment selected."));
		return;
	}

	const FName NewCurrentSegmentID = SelectedNextSegmentID;
	SelectedNextSegmentID = NAME_None;

	if (!BuildSegmentByID(NewCurrentSegmentID))
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] AdvanceCampaignSliceInEditor failed: could not build '%s' as current segment."), *NewCurrentSegmentID.ToString());
		return;
	}

	SelectFirstConnectedSegmentInEditor();
	if (!SelectedNextSegmentID.IsNone())
	{
		PreviewSelectedNextSegmentAndConnectorInEditor();
	}
	else if (ARouteConnectorActor* PreviewConnector = ResolveConnectorActor())
	{
		PreviewConnector->ClearConnector();
		PreviewConnectorStart = FTransform::Identity;
		PreviewConnectorEnd = FTransform::Identity;
		PreviewNextSegmentID = NAME_None;
	}
}

namespace
{
void ResetManagedActorTransform(AActor* Actor)
{
	if (Actor)
	{
		Actor->SetActorTransform(FTransform::Identity);
	}
}
}

void ACampaignWorldManager::AdvanceToFirstConnectedSegmentInEditor()
{
	AdvanceToFirstConnectedSegment();
}

void ACampaignWorldManager::PreviewSelectedNextSegmentAndConnectorInEditor()
{
	PreviewSelectedNextSegmentAndConnector();
}

void ACampaignWorldManager::SelectFirstConnectedSegmentInEditor()
{
	if (!CampaignGraph || ProgressState.CurrentSegmentID.IsNone())
	{
		return;
	}

	const FCampaignSegmentDescriptor* Current = CampaignGraph->FindSegmentByID(ProgressState.CurrentSegmentID);
	if (!Current || Current->NextSegmentIDs.Num() == 0)
	{
		return;
	}

	SelectedNextSegmentID = Current->NextSegmentIDs[0];
}

bool ACampaignWorldManager::ValidateCampaignGraph(FString& OutError) const
{
	if (!CampaignGraph)
	{
		OutError = TEXT("No CampaignGraph assigned.");
		return false;
	}

	return CampaignGraph->IsValidGraph(OutError);
}

ATraversalRouteActor* ACampaignWorldManager::ResolveTargetRouteActor()
{
	if (TargetRouteActor)
	{
		return TargetRouteActor;
	}

	if (ATraversalRouteActor* ExistingActor = Cast<ATraversalRouteActor>(UGameplayStatics::GetActorOfClass(this, ATraversalRouteActor::StaticClass())))
	{
		return ExistingActor;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(GetWorld()->PersistentLevel, ATraversalRouteActor::StaticClass(), TEXT("CampaignTargetRouteActor"));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TargetRouteActor = GetWorld()->SpawnActor<ATraversalRouteActor>(ATraversalRouteActor::StaticClass(), FTransform::Identity, SpawnParams);
	return TargetRouteActor;
}

ATraversalRouteActor* ACampaignWorldManager::ResolvePreviewRouteActor()
{
	if (PreviewRouteActor)
	{
		return PreviewRouteActor;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(GetWorld()->PersistentLevel, ATraversalRouteActor::StaticClass(), TEXT("CampaignPreviewRouteActor"));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewRouteActor = GetWorld()->SpawnActor<ATraversalRouteActor>(ATraversalRouteActor::StaticClass(), FTransform::Identity, SpawnParams);
	return PreviewRouteActor;
}

ARouteConnectorActor* ACampaignWorldManager::ResolveConnectorActor()
{
	if (ConnectorActor)
	{
		return ConnectorActor;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(GetWorld()->PersistentLevel, ARouteConnectorActor::StaticClass(), TEXT("CampaignRouteConnectorActor"));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ConnectorActor = GetWorld()->SpawnActor<ARouteConnectorActor>(ARouteConnectorActor::StaticClass(), FTransform::Identity, SpawnParams);
	return ConnectorActor;
}

ATraversalRouteActor* ACampaignWorldManager::ResolvePathRouteActor(int32 PathIndex)
{
	if (PathIndex == 0)
	{
		return ResolveTargetRouteActor();
	}

	if (PathIndex == 1)
	{
		return ResolvePreviewRouteActor();
	}

	const int32 ExtraIndex = PathIndex - 2;
	if (SpawnedPathRouteActors.IsValidIndex(ExtraIndex) && IsValid(SpawnedPathRouteActors[ExtraIndex]))
	{
		return SpawnedPathRouteActors[ExtraIndex];
	}

	if (!bAutoSpawnPathPreviewActors || !GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(
		GetWorld()->PersistentLevel,
		ATraversalRouteActor::StaticClass(),
		*FString::Printf(TEXT("CampaignPathRouteActor_%02d"), PathIndex));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATraversalRouteActor* SpawnedRouteActor = GetWorld()->SpawnActor<ATraversalRouteActor>(ATraversalRouteActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!SpawnedRouteActor)
	{
		return nullptr;
	}

	while (SpawnedPathRouteActors.Num() <= ExtraIndex)
	{
		SpawnedPathRouteActors.Add(nullptr);
	}
	SpawnedPathRouteActors[ExtraIndex] = SpawnedRouteActor;
	return SpawnedRouteActor;
}

void ACampaignWorldManager::ResetPreviewArtifacts()
{
	BuiltPathRouteActors.Reset();
	BuiltPathSegmentIDs.Reset();

	if (ConnectorActor)
	{
		ConnectorActor->ClearConnector();
	}

	PreviewConnectorStart = FTransform::Identity;
	PreviewConnectorEnd = FTransform::Identity;
	PreviewNextSegmentID = NAME_None;
}

void ACampaignWorldManager::ConfigureSpecForCampaignSegment(FRouteGenSpec& InOutSpec, const FCampaignSegmentDescriptor& Descriptor) const
{
	InOutSpec.RouteLengthMeters = Descriptor.PreferredLengthMeters;
	InOutSpec.ComplexityTier = Descriptor.ComplexityTier;

	if (!CampaignGraph)
	{
		return;
	}

	const bool bHasIncomingConnection = CampaignGraph->CountIncomingConnections(Descriptor.SegmentID) > 0;
	const bool bHasOutgoingConnection = Descriptor.NextSegmentIDs.Num() > 0;
	InOutSpec.bForceStartInterfaceOnly = bHasIncomingConnection;
	InOutSpec.bForceEndInterfaceOnly = bHasOutgoingConnection;
}

void ACampaignWorldManager::AlignRouteActorToPrevious(ATraversalRouteActor* PreviousRoute, ATraversalRouteActor* CurrentRoute) const
{
	if (!PreviousRoute || !CurrentRoute)
	{
		return;
	}

	const FTransform PreviousEndDockWorld = PreviousRoute->GetRouteEndDockTransformWorld();
	const FVector PreviousEndDockForward = PreviousEndDockWorld.GetRotation().GetForwardVector().GetSafeNormal();
	const float ResolvedSeparationCm = bUseOverlapSegmentConnections
		? -ResolvePreferredOverlapCm(PreviousRoute, CurrentRoute)
		: PreviewConnectorGapCm;
	const FTransform DesiredCurrentStartDockWorld = MakeConnectorFacingTransform(
		PreviousEndDockWorld.GetLocation() + PreviousEndDockForward * ResolvedSeparationCm,
		-PreviousEndDockForward);
	const FTransform CurrentStartDockLocal = CurrentRoute->RouteStartDockTransform;
	const FTransform CurrentActorWorld = CurrentStartDockLocal.Inverse() * DesiredCurrentStartDockWorld;
	CurrentRoute->SetActorTransform(CurrentActorWorld);
}

bool ACampaignWorldManager::BuildDescriptorToActor(const FCampaignSegmentDescriptor& Descriptor, int32 SegmentIndex, ATraversalRouteActor* RouteActor, bool bUpdateProgressState)
{
	if (!RouteActor)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] No TraversalRouteActor found for segment '%s'."), *Descriptor.SegmentID.ToString());
		return false;
	}

	if (bResetManagedActorTransformsOnBuild)
	{
		ResetManagedActorTransform(RouteActor);
	}

	UCampaignRouteCompiler* Compiler = NewObject<UCampaignRouteCompiler>(this);
	if (!Compiler)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Could not allocate CampaignRouteCompiler."));
		return false;
	}

	const int32 SegmentIDHash = static_cast<int32>(FCrc::StrCrc32(*Descriptor.SegmentID.ToString()));
	const int32 StableRouteID = FMath::Abs(UCampaignRouteCompiler::HashInts(SegmentIDHash, Descriptor.SeedOffset + SegmentIndex + 1));

	FRouteGenSpec Spec;
	FRouteSeedCascade Seeds;
	if (!Compiler->BuildRouteSpec(MasterSeed, StableRouteID, Spec, Seeds))
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] BuildRouteSpec failed for segment '%s'."), *Descriptor.SegmentID.ToString());
		return false;
	}

	ConfigureSpecForCampaignSegment(Spec, Descriptor);
	Spec.RouteID = StableRouteID;
	Spec.BiomeID = SegmentIndex;
	Spec.Archetype = Descriptor.Archetype ? Descriptor.Archetype->ArchetypeType : ETraversalRouteArchetype::MainTransit;

	RouteActor->ArchetypeAsset = Descriptor.Archetype;
	RouteActor->BiomeAsset = Descriptor.Biome;
	RouteActor->DebugSpec = Spec;
	RouteActor->CampaignSegmentID = Descriptor.SegmentID;
	RouteActor->CampaignPathIndex = SegmentIndex;
	RouteActor->ClearCampaignExternalUnionBrushes();

	const bool bBuilt = RouteActor->BuildRouteFromSpec(Spec, Seeds);
	if (!bBuilt)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Route build failed for segment '%s'."), *Descriptor.SegmentID.ToString());
		return false;
	}

	if (bUpdateProgressState)
	{
		ProgressState.MasterSeed = MasterSeed;
		ProgressState.CurrentSegmentID = Descriptor.SegmentID;
		ProgressState.DiscoveredSegmentIDs.Add(Descriptor.SegmentID);
		ProgressState.ExploredSegmentIDs.Add(Descriptor.SegmentID);
		ProgressState.DeepestDepthMeters = FMath::Max(ProgressState.DeepestDepthMeters, Descriptor.DepthAtSegmentEndMeters);
		ProgressState.SegmentBuildHashes.Add(Descriptor.SegmentID, RouteActor->BakedRouteHash);

		PreviewNextSegmentID = Descriptor.NextSegmentIDs.Num() > 0 ? Descriptor.NextSegmentIDs[0] : NAME_None;
		if (SelectedNextSegmentID.IsNone())
		{
			SelectedNextSegmentID = PreviewNextSegmentID;
		}
	}

	UE_LOG(LogRouteGen, Log,
		TEXT("[Campaign] Built segment '%s' Role=%d Length=%.0fm DepthEnd=%.0fm NextCount=%d RouteID=%d Actor=%s ActorLoc=(%.0f,%.0f,%.0f)"),
		*Descriptor.SegmentID.ToString(),
		static_cast<int32>(Descriptor.Role),
		Descriptor.PreferredLengthMeters,
		Descriptor.DepthAtSegmentEndMeters,
		Descriptor.NextSegmentIDs.Num(),
		StableRouteID,
		*RouteActor->GetName(),
		RouteActor->GetActorLocation().X,
		RouteActor->GetActorLocation().Y,
		RouteActor->GetActorLocation().Z);

	return true;
}

bool ACampaignWorldManager::BuildSegmentByID(FName SegmentID)
{
	FString ValidationError;
	if (!ValidateCampaignGraph(ValidationError))
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Invalid graph: %s"), *ValidationError);
		return false;
	}

	const int32 SegmentIndex = CampaignGraph->FindSegmentIndexByID(SegmentID);
	const FCampaignSegmentDescriptor* Descriptor = CampaignGraph->FindSegmentByID(SegmentID);
	if (!Descriptor || SegmentIndex == INDEX_NONE)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Missing segment '%s'."), *SegmentID.ToString());
		return false;
	}

	return BuildDescriptorToActor(*Descriptor, SegmentIndex, ResolveTargetRouteActor(), true);
}

bool ACampaignWorldManager::AdvanceToFirstConnectedSegment()
{
	if (!CampaignGraph || ProgressState.CurrentSegmentID.IsNone())
	{
		return false;
	}

	const FCampaignSegmentDescriptor* Current = CampaignGraph->FindSegmentByID(ProgressState.CurrentSegmentID);
	if (!Current || Current->NextSegmentIDs.Num() == 0)
	{
		return false;
	}

	return BuildSegmentByID(Current->NextSegmentIDs[0]);
}

bool ACampaignWorldManager::PreviewSelectedNextSegmentAndConnector()
{
	FString ValidationError;
	if (!ValidateCampaignGraph(ValidationError))
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Invalid graph: %s"), *ValidationError);
		return false;
	}

	ATraversalRouteActor* CurrentRoute = ResolveTargetRouteActor();
	ATraversalRouteActor* PreviewRoute = ResolvePreviewRouteActor();
	ARouteConnectorActor* PreviewConnector = ResolveConnectorActor();
	if (!CurrentRoute || !PreviewRoute)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Missing route actors for preview."));
		return false;
	}

	if (SelectedNextSegmentID.IsNone())
	{
		SelectFirstConnectedSegmentInEditor();
	}

	if (SelectedNextSegmentID.IsNone())
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] No selected next segment to preview."));
		return false;
	}

	const int32 SegmentIndex = CampaignGraph->FindSegmentIndexByID(SelectedNextSegmentID);
	const FCampaignSegmentDescriptor* Descriptor = CampaignGraph->FindSegmentByID(SelectedNextSegmentID);
	if (!Descriptor || SegmentIndex == INDEX_NONE)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Selected next segment '%s' was not found."), *SelectedNextSegmentID.ToString());
		return false;
	}

	UCampaignRouteCompiler* Compiler = NewObject<UCampaignRouteCompiler>(this);
	if (!Compiler)
	{
		return false;
	}

	const int32 SegmentIDHash = static_cast<int32>(FCrc::StrCrc32(*Descriptor->SegmentID.ToString()));
	const int32 StableRouteID = FMath::Abs(UCampaignRouteCompiler::HashInts(SegmentIDHash, Descriptor->SeedOffset + SegmentIndex + 1));

	FRouteGenSpec Spec;
	FRouteSeedCascade Seeds;
	if (!Compiler->BuildRouteSpec(MasterSeed, StableRouteID, Spec, Seeds))
	{
		return false;
	}

	ConfigureSpecForCampaignSegment(Spec, *Descriptor);
	Spec.RouteID = StableRouteID;
	Spec.BiomeID = SegmentIndex;
	Spec.Archetype = Descriptor->Archetype ? Descriptor->Archetype->ArchetypeType : ETraversalRouteArchetype::MainTransit;

	PreviewRoute->ArchetypeAsset = Descriptor->Archetype;
	PreviewRoute->BiomeAsset = Descriptor->Biome;
	PreviewRoute->DebugSpec = Spec;
	if (bResetManagedActorTransformsOnBuild)
	{
		ResetManagedActorTransform(PreviewRoute);
	}

	if (!PreviewRoute->BuildRouteFromSpec(Spec, Seeds))
	{
		return false;
	}

	AlignRouteActorToPrevious(CurrentRoute, PreviewRoute);

	PreviewConnectorStart = CurrentRoute->GetRouteEndDockTransformWorld();
	PreviewConnectorEnd = PreviewRoute->GetRouteStartDockTransformWorld();

	const float StartRadius = FMath::Max(1000.f, CurrentRoute->RouteEndDockRadiusCm);
	const float EndRadius = FMath::Max(1000.f, PreviewRoute->RouteStartDockRadiusCm);
	if (PreviewConnector)
	{
		if (bShowDebugConnectorMesh)
		{
			PreviewConnector->BuildConnector(PreviewConnectorStart, StartRadius, PreviewConnectorEnd, EndRadius);
		}
		else
		{
			PreviewConnector->ClearConnector();
		}
	}

	UE_LOG(LogRouteGen, Log,
		TEXT("[Campaign] Previewed next segment '%s'. Mode=%s Separation=%.0fcm StartRadius=%.0f EndRadius=%.0f CurrentActor=%s PreviewActor=%s DesiredPreviewDock=(%.0f,%.0f,%.0f) CurrentDock=(%.0f,%.0f,%.0f) PreviewDock=(%.0f,%.0f,%.0f)"),
		*SelectedNextSegmentID.ToString(),
		bUseOverlapSegmentConnections ? TEXT("Overlap") : TEXT("ConnectorGap"),
		bUseOverlapSegmentConnections ? -ResolvePreferredOverlapCm(CurrentRoute, PreviewRoute) : PreviewConnectorGapCm,
		StartRadius,
		EndRadius,
		*CurrentRoute->GetName(),
		*PreviewRoute->GetName(),
		PreviewRoute->GetRouteStartDockTransformWorld().GetLocation().X,
		PreviewRoute->GetRouteStartDockTransformWorld().GetLocation().Y,
		PreviewRoute->GetRouteStartDockTransformWorld().GetLocation().Z,
		PreviewConnectorStart.GetLocation().X,
		PreviewConnectorStart.GetLocation().Y,
		PreviewConnectorStart.GetLocation().Z,
		PreviewConnectorEnd.GetLocation().X,
		PreviewConnectorEnd.GetLocation().Y,
		PreviewConnectorEnd.GetLocation().Z);

	return true;
}

TArray<const FCampaignSegmentDescriptor*> ACampaignWorldManager::BuildPreviewPathDescriptors() const
{
	TArray<const FCampaignSegmentDescriptor*> Path;
	if (!CampaignGraph || CampaignGraph->RootSegmentID.IsNone())
	{
		return Path;
	}

	TSet<FName> Visited;
	const FCampaignSegmentDescriptor* Current = CampaignGraph->FindSegmentByID(CampaignGraph->RootSegmentID);
	while (Current && !Visited.Contains(Current->SegmentID))
	{
		Path.Add(Current);
		Visited.Add(Current->SegmentID);
		if (Current->NextSegmentIDs.Num() == 0)
		{
			break;
		}

		const FName NextSegmentID = Current->NextSegmentIDs[0];
		Current = CampaignGraph->FindSegmentByID(NextSegmentID);
	}

	return Path;
}

void ACampaignWorldManager::BuildCampaignPathPreviewInEditor()
{
	FString ValidationError;
	if (!ValidateCampaignGraph(ValidationError))
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Invalid graph: %s"), *ValidationError);
		return;
	}

	const TArray<const FCampaignSegmentDescriptor*> Path = BuildPreviewPathDescriptors();
	if (Path.Num() == 0)
	{
		UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] BuildCampaignPathPreviewInEditor failed: no path from root."));
		return;
	}

	ResetCampaignProgress();
	ResetPreviewArtifacts();

	ATraversalRouteActor* PreviousRoute = nullptr;
	for (int32 PathIndex = 0; PathIndex < Path.Num(); ++PathIndex)
	{
		const FCampaignSegmentDescriptor* Descriptor = Path[PathIndex];
		if (!Descriptor)
		{
			continue;
		}

		const int32 SegmentIndex = CampaignGraph->FindSegmentIndexByID(Descriptor->SegmentID);
		ATraversalRouteActor* RouteActor = ResolvePathRouteActor(PathIndex);
		if (!RouteActor)
		{
			UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Missing route actor for path index %d."), PathIndex);
			return;
		}

		if (!BuildDescriptorToActor(*Descriptor, SegmentIndex, RouteActor, false))
		{
			UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Failed to build preview path segment '%s'."), *Descriptor->SegmentID.ToString());
			return;
		}

		if (PreviousRoute)
		{
			AlignRouteActorToPrevious(PreviousRoute, RouteActor);
		}

		BuiltPathRouteActors.Add(RouteActor);
		BuiltPathSegmentIDs.Add(Descriptor->SegmentID);
		PreviousRoute = RouteActor;
	}

	const int32 NeededSpawnedExtras = FMath::Max(0, Path.Num() - 2);
	for (int32 ExtraIndex = NeededSpawnedExtras; ExtraIndex < SpawnedPathRouteActors.Num(); ++ExtraIndex)
	{
		if (IsValid(SpawnedPathRouteActors[ExtraIndex]))
		{
			SpawnedPathRouteActors[ExtraIndex]->Destroy();
		}
	}
	if (SpawnedPathRouteActors.Num() > NeededSpawnedExtras)
	{
		SpawnedPathRouteActors.SetNum(NeededSpawnedExtras);
	}

	if (ConnectorActor)
	{
		ConnectorActor->ClearConnector();
	}

	ApplyCrossSegmentUnionToBuiltPath();
	ApplyDebugViewsToBuiltPath();
	if (bWriteCampaignGenerationLogs)
	{
		WriteCampaignPathPreviewLogSnapshot();
	}

	UE_LOG(LogRouteGen, Log, TEXT("[Campaign] Built campaign path preview with %d segments."), Path.Num());
}

void ACampaignWorldManager::ClearCampaignPathPreviewInEditor()
{
	for (ATraversalRouteActor* SpawnedRouteActor : SpawnedPathRouteActors)
	{
		if (IsValid(SpawnedRouteActor))
		{
			SpawnedRouteActor->Destroy();
		}
	}
	SpawnedPathRouteActors.Reset();
	ResetPreviewArtifacts();
}

void ACampaignWorldManager::ApplyCrossSegmentUnionToBuiltPath()
{
	if (BuiltPathRouteActors.Num() < 2)
	{
		return;
	}

	TArray<TArray<FVolumeBrushDef>> BrushesPerRoute;
	BrushesPerRoute.SetNum(BuiltPathRouteActors.Num());

	for (int32 Index = 0; Index < BuiltPathRouteActors.Num() - 1; ++Index)
	{
		ATraversalRouteActor* CurrentRoute = BuiltPathRouteActors[Index];
		ATraversalRouteActor* NextRoute = BuiltPathRouteActors[Index + 1];
		if (!IsValid(CurrentRoute) || !IsValid(NextRoute))
		{
			continue;
		}

		const FTransform CurrentDockWorld = CurrentRoute->GetRouteEndDockTransformWorld();
		const FTransform NextDockWorld = NextRoute->GetRouteStartDockTransformWorld();
		const FVector CurrentPos = CurrentDockWorld.GetLocation();
		const FVector NextPos = NextDockWorld.GetLocation();
		const FVector Delta = NextPos - CurrentPos;
		const FVector BridgeDir = Delta.IsNearlyZero()
			? CurrentDockWorld.GetRotation().GetForwardVector().GetSafeNormal()
			: Delta.GetSafeNormal();

		const float BridgeRadius = FMath::Max(1400.f, FMath::Min(CurrentRoute->RouteEndDockRadiusCm, NextRoute->RouteStartDockRadiusCm) * 1.08f);
		const float BridgeSmoothness = 260.f;
		const FVector MidPoint = (CurrentPos + NextPos) * 0.5f;

		FVolumeBrushDef CurrentCapsule;
		CurrentCapsule.BrushType = EVolumeBrushType::CapsuleCorridor;
		CurrentCapsule.CenterA = CurrentRoute->GetActorTransform().InverseTransformPosition(CurrentPos);
		CurrentCapsule.CenterB = CurrentRoute->GetActorTransform().InverseTransformPosition(NextPos);
		CurrentCapsule.Radius = BridgeRadius;
		CurrentCapsule.Smoothness = BridgeSmoothness;
		CurrentCapsule.bGuaranteedTraversal = true;
		CurrentCapsule.bDecorativeOnly = false;
		CurrentCapsule.bAffectsRenderField = true;
		CurrentCapsule.bAffectsSonarField = true;
		CurrentCapsule.LogicalRole = ELogicalRouteNodeRole::MainSpine;
		CurrentCapsule.BranchIndex = INDEX_NONE;
		CurrentCapsule.DebugColor = ResolveSegmentPaletteColor(Index);
		BrushesPerRoute[Index].Add(CurrentCapsule);

		FVolumeBrushDef CurrentMidSphere;
		CurrentMidSphere.BrushType = EVolumeBrushType::SpherePocket;
		CurrentMidSphere.CenterA = CurrentRoute->GetActorTransform().InverseTransformPosition(MidPoint);
		CurrentMidSphere.CenterB = CurrentMidSphere.CenterA;
		CurrentMidSphere.Radius = BridgeRadius * 1.12f;
		CurrentMidSphere.Smoothness = BridgeSmoothness;
		CurrentMidSphere.bGuaranteedTraversal = true;
		CurrentMidSphere.bDecorativeOnly = false;
		CurrentMidSphere.bAffectsRenderField = true;
		CurrentMidSphere.bAffectsSonarField = true;
		CurrentMidSphere.LogicalRole = ELogicalRouteNodeRole::MainSpine;
		CurrentMidSphere.BranchIndex = INDEX_NONE;
		CurrentMidSphere.DebugColor = ResolveSegmentPaletteColor(Index);
		BrushesPerRoute[Index].Add(CurrentMidSphere);

		FVolumeBrushDef NextCapsule = CurrentCapsule;
		NextCapsule.CenterA = NextRoute->GetActorTransform().InverseTransformPosition(CurrentPos);
		NextCapsule.CenterB = NextRoute->GetActorTransform().InverseTransformPosition(NextPos);
		NextCapsule.DebugColor = ResolveSegmentPaletteColor(Index + 1);
		BrushesPerRoute[Index + 1].Add(NextCapsule);

		FVolumeBrushDef NextMidSphere = CurrentMidSphere;
		NextMidSphere.CenterA = NextRoute->GetActorTransform().InverseTransformPosition(MidPoint);
		NextMidSphere.CenterB = NextMidSphere.CenterA;
		NextMidSphere.DebugColor = ResolveSegmentPaletteColor(Index + 1);
		BrushesPerRoute[Index + 1].Add(NextMidSphere);

		const float InterfaceExtensionCm = FMath::Max(800.f, BridgeRadius * 1.2f);
		FVolumeBrushDef CurrentExtension;
		CurrentExtension.BrushType = EVolumeBrushType::CapsuleCorridor;
		CurrentExtension.CenterA = CurrentRoute->GetActorTransform().InverseTransformPosition(CurrentPos - BridgeDir * InterfaceExtensionCm);
		CurrentExtension.CenterB = CurrentRoute->GetActorTransform().InverseTransformPosition(CurrentPos + BridgeDir * InterfaceExtensionCm);
		CurrentExtension.Radius = BridgeRadius * 0.98f;
		CurrentExtension.Smoothness = BridgeSmoothness;
		CurrentExtension.bGuaranteedTraversal = true;
		CurrentExtension.bDecorativeOnly = false;
		CurrentExtension.bAffectsRenderField = true;
		CurrentExtension.bAffectsSonarField = true;
		CurrentExtension.LogicalRole = ELogicalRouteNodeRole::MainSpine;
		CurrentExtension.BranchIndex = INDEX_NONE;
		CurrentExtension.DebugColor = ResolveSegmentPaletteColor(Index);
		BrushesPerRoute[Index].Add(CurrentExtension);

		FVolumeBrushDef NextExtension = CurrentExtension;
		NextExtension.CenterA = NextRoute->GetActorTransform().InverseTransformPosition(NextPos - BridgeDir * InterfaceExtensionCm);
		NextExtension.CenterB = NextRoute->GetActorTransform().InverseTransformPosition(NextPos + BridgeDir * InterfaceExtensionCm);
		NextExtension.DebugColor = ResolveSegmentPaletteColor(Index + 1);
		BrushesPerRoute[Index + 1].Add(NextExtension);
	}

	for (int32 Index = 0; Index < BuiltPathRouteActors.Num(); ++Index)
	{
		ATraversalRouteActor* RouteActor = BuiltPathRouteActors[Index];
		if (!IsValid(RouteActor))
		{
			continue;
		}

		RouteActor->SetCampaignExternalUnionBrushes(BrushesPerRoute[Index]);
		if (!RouteActor->BuildRouteFromSpec(RouteActor->DebugSpec, RouteActor->RouteNetSpec.Seeds))
		{
			UE_LOG(LogRouteGen, Warning, TEXT("[Campaign] Cross-segment union rebuild failed for '%s'."), *RouteActor->CampaignSegmentID.ToString());
		}
	}
}

void ACampaignWorldManager::ApplyDebugViewsToBuiltPath()
{
	for (int32 Index = 0; Index < BuiltPathRouteActors.Num(); ++Index)
	{
		ATraversalRouteActor* RouteActor = BuiltPathRouteActors[Index];
		if (!IsValid(RouteActor))
		{
			continue;
		}

		const FName SegmentID = BuiltPathSegmentIDs.IsValidIndex(Index) ? BuiltPathSegmentIDs[Index] : NAME_None;
		const bool bEnableRoleDebug = bEnableRoleDebugForSelectedSegments && RoleDebugSegmentIDs.Contains(SegmentID);
		RouteActor->ConfigureCampaignDebugView(
			bEnableSegmentColorDebug,
			SegmentDebugMaterial,
			ResolveSegmentPaletteColor(Index),
			bEnableRoleDebug);
	}
}

void ACampaignWorldManager::WriteCampaignPathPreviewLogSnapshot() const
{
	if (!CampaignGraph || BuiltPathSegmentIDs.Num() == 0)
	{
		return;
	}

	const FString LogDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RouteGenLogs"), TEXT("Campaign"));
	IFileManager::Get().MakeDirectory(*LogDir, true);

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString GraphToken = SanitizeCampaignLogToken(CampaignGraph->CampaignGraphID.IsNone()
		? CampaignGraph->GetName()
		: CampaignGraph->CampaignGraphID.ToString());
	const FString FilePath = FPaths::Combine(LogDir, FString::Printf(TEXT("Campaign_%s_%s.log"), *Timestamp, *GraphToken));

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Timestamp=%s"), *Timestamp));
	Lines.Add(FString::Printf(TEXT("CampaignGraphID=%s"), *CampaignGraph->CampaignGraphID.ToString()));
	Lines.Add(FString::Printf(TEXT("MasterSeed=%d"), MasterSeed));
	Lines.Add(FString::Printf(TEXT("BuiltPathCount=%d"), BuiltPathSegmentIDs.Num()));

	for (int32 Index = 0; Index < BuiltPathSegmentIDs.Num(); ++Index)
	{
		const FName SegmentID = BuiltPathSegmentIDs[Index];
		Lines.Add(FString::Printf(TEXT("Segment[%d]=%s"), Index, *SegmentID.ToString()));

		if (const FCampaignSegmentDescriptor* Descriptor = CampaignGraph->FindSegmentByID(SegmentID))
		{
			const FString ArchetypeName = Descriptor->Archetype ? Descriptor->Archetype->GetName() : TEXT("None");
			const FString BiomeName = Descriptor->Biome ? Descriptor->Biome->GetName() : TEXT("None");
			Lines.Add(FString::Printf(TEXT("  Role=%d LengthMeters=%.0f DepthEndMeters=%.0f Archetype=%s Biome=%s"),
				(int32)Descriptor->Role,
				Descriptor->PreferredLengthMeters,
				Descriptor->DepthAtSegmentEndMeters,
				*ArchetypeName,
				*BiomeName));
		}

		if (BuiltPathRouteActors.IsValidIndex(Index) && IsValid(BuiltPathRouteActors[Index]))
		{
			const ATraversalRouteActor* RouteActor = BuiltPathRouteActors[Index];
			const FVector Loc = RouteActor->GetActorLocation();
			Lines.Add(FString::Printf(TEXT("  Actor=%s Loc=(%.0f,%.0f,%.0f) Hash=0x%08X Tris=%d"),
				*RouteActor->GetName(),
				Loc.X,
				Loc.Y,
				Loc.Z,
				RouteActor->BakedRouteHash,
				RouteActor->TotalTriangles));
		}
	}

	FFileHelper::SaveStringArrayToFile(Lines, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
