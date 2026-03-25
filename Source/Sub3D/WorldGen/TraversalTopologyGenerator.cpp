#include "TraversalTopologyGenerator.h"
#include "BranchProfileDataAsset.h"
#include "BranchProfileSetDataAsset.h"
#include "RouteArchetypeDataAsset.h"

namespace
{
int32 ComputeNodeCount(float RouteLengthMeters)
{
	return FMath::Clamp(FMath::RoundToInt(RouteLengthMeters / 280.f), 12, 20);
}

float RadiusForNodeType(ETopologyNodeType Type)
{
	switch (Type)
	{
	case ETopologyNodeType::StartCheckpointSpace: return 5200.f;
	case ETopologyNodeType::StartAnchor:    return 4000.f;
	case ETopologyNodeType::EntryBuffer:    return 3500.f;
	case ETopologyNodeType::WideTransit:    return 3200.f;
	case ETopologyNodeType::SplitAnchor:    return 3600.f;
	case ETopologyNodeType::BranchTransit:  return 3000.f;
	case ETopologyNodeType::MergeAnchor:    return 3800.f;
	case ETopologyNodeType::NarrowTransit:  return 2400.f;
	case ETopologyNodeType::TurnPocket:     return 3800.f;
	case ETopologyNodeType::VerticalDrop:   return 2600.f;
	case ETopologyNodeType::HubChamber:     return 5500.f;
	case ETopologyNodeType::AmbushPocket:   return 4200.f;
	case ETopologyNodeType::WreckPocket:    return 4000.f;
	case ETopologyNodeType::ResourcePocket: return 3000.f;
	case ETopologyNodeType::ExitApproach:   return 3500.f;
	case ETopologyNodeType::EndCheckpointSpace: return 5200.f;
	case ETopologyNodeType::ExitAnchor:     return 4000.f;
	default:                                return 3000.f;
	}
}

void BuildOffsetFrame(const FVector& From, const FVector& To, FVector& OutAlong, FVector& OutSide)
{
	const FVector Axis = To - From;
	const float AxisLength = Axis.Size();
	OutAlong = (AxisLength > KINDA_SMALL_NUMBER) ? Axis / AxisLength : FVector::ForwardVector;
	OutSide = FVector::CrossProduct(OutAlong, FVector::UpVector);
	if (OutSide.IsNearlyZero())
	{
		OutSide = FVector::RightVector;
	}
	OutSide = OutSide.GetSafeNormal();
}

struct FResolvedBranchProfile
{
	FName ProfileID;
	EBranchProfileIntent Intent = EBranchProfileIntent::OptionalResourceDetour;
	float TargetRadiusCm = 3200.f;
	float LengthScale = 1.0f;
	float CurvatureScale = 1.0f;
	float VerticalityScale = 1.0f;
	float RejoinChance = 0.55f;
	float PocketChance = 0.45f;
	float SecondarySplitChance = 0.0f;
	int32 MaxDepth = 2;
	EBranchPlacementWindow PreferredWindow = EBranchPlacementWindow::Anywhere;
	float SelectionWeight = 1.0f;
	bool bAllowedOnCanonicalRoute = false;
	bool bAllowedOnOptionalRoute = true;
	bool bAllowedNearCheckpoint = false;
	bool bAllowedNearHub = true;
	FGameplayTagContainer SemanticTags;
};

bool PlacementWindowMatches(EBranchPlacementWindow Window, float NormalizedDistance, bool bNearHub, bool bNearCheckpoint)
{
	switch (Window)
	{
	case EBranchPlacementWindow::Anywhere:
		return true;
	case EBranchPlacementWindow::StartThird:
		return NormalizedDistance <= 0.33f;
	case EBranchPlacementWindow::Mid:
		return NormalizedDistance > 0.25f && NormalizedDistance < 0.75f;
	case EBranchPlacementWindow::Late:
		return NormalizedDistance >= 0.66f;
	case EBranchPlacementWindow::NearHub:
		return bNearHub;
	case EBranchPlacementWindow::NearCheckpoint:
		return bNearCheckpoint;
	case EBranchPlacementWindow::DeepSegment:
		return NormalizedDistance >= 0.55f;
	default:
		return true;
	}
}

FResolvedBranchProfile MakeLegacyProfile(FName ProfileID, EBranchProfileIntent Intent, float RadiusCm)
{
	FResolvedBranchProfile Profile;
	Profile.ProfileID = ProfileID;
	Profile.Intent = Intent;
	Profile.TargetRadiusCm = RadiusCm;
	Profile.bAllowedOnOptionalRoute = true;
	Profile.bAllowedOnCanonicalRoute = Intent == EBranchProfileIntent::CanonicalBypass;
	Profile.MaxDepth = Intent == EBranchProfileIntent::PocketChain ? 3 : 2;
	Profile.PocketChance = Intent == EBranchProfileIntent::PocketChain ? 0.8f : 0.45f;
	Profile.RejoinChance = Intent == EBranchProfileIntent::HubConnector ? 0.8f : 0.55f;
	return Profile;
}

TArray<FResolvedBranchProfile> BuildResolvedBranchProfiles(const URouteArchetypeDataAsset* Archetype)
{
	TArray<FResolvedBranchProfile> Profiles;
	if (!Archetype)
	{
		return Profiles;
	}

	if (!Archetype->BranchProfileSet.IsNull())
	{
		if (const UBranchProfileSetDataAsset* ProfileSet = Archetype->BranchProfileSet.LoadSynchronous())
		{
			for (const TSoftObjectPtr<UBranchProfileDataAsset>& ProfilePtr : ProfileSet->Profiles)
			{
				const UBranchProfileDataAsset* ProfileAsset = ProfilePtr.LoadSynchronous();
				if (!ProfileAsset)
				{
					continue;
				}

				FResolvedBranchProfile Profile;
				Profile.ProfileID = !ProfileAsset->ProfileID.IsNone() ? ProfileAsset->ProfileID : ProfileAsset->GetFName();
				Profile.Intent = ProfileAsset->Intent;
				Profile.TargetRadiusCm = ProfileAsset->TargetRadiusCm;
				Profile.LengthScale = ProfileAsset->LengthScale;
				Profile.CurvatureScale = ProfileAsset->CurvatureScale;
				Profile.VerticalityScale = ProfileAsset->VerticalityScale;
				Profile.RejoinChance = ProfileAsset->RejoinChance;
				Profile.PocketChance = ProfileAsset->PocketChance;
				Profile.SecondarySplitChance = ProfileAsset->SecondarySplitChance;
				Profile.MaxDepth = ProfileAsset->MaxDepth;
				Profile.PreferredWindow = ProfileAsset->PreferredWindow;
				Profile.SelectionWeight = ProfileAsset->SelectionWeight;
				Profile.bAllowedOnCanonicalRoute = ProfileAsset->bAllowedOnCanonicalRoute;
				Profile.bAllowedOnOptionalRoute = ProfileAsset->bAllowedOnOptionalRoute;
				Profile.bAllowedNearCheckpoint = ProfileAsset->bAllowedNearCheckpoint;
				Profile.bAllowedNearHub = ProfileAsset->bAllowedNearHub;
				Profile.SemanticTags = ProfileAsset->SemanticTags;
				Profiles.Add(Profile);
			}
		}
	}

	if (Profiles.Num() == 0)
	{
		Profiles.Add(MakeLegacyProfile(TEXT("LegacyNarrow"), EBranchProfileIntent::OptionalDangerBranch, Archetype->Volumes.BranchARadiusCm));
		Profiles.Add(MakeLegacyProfile(TEXT("LegacyWide"), EBranchProfileIntent::HubConnector, Archetype->Volumes.BranchBRadiusCm));
		Profiles.Add(MakeLegacyProfile(TEXT("LegacyStandard"), EBranchProfileIntent::OptionalResourceDetour, Archetype->Volumes.BranchCRadiusCm));
	}

	return Profiles;
}

const FResolvedBranchProfile* SelectBranchProfile(const TArray<FResolvedBranchProfile>& Profiles,
	float NormalizedDistance,
	bool bCanonical,
	bool bNearHub,
	bool bNearCheckpoint,
	bool bPreferReconnect,
	FRandomStream& Rng)
{
	struct FWeightedCandidate
	{
		const FResolvedBranchProfile* Profile = nullptr;
		float Weight = 0.f;
	};

	TArray<FWeightedCandidate> Candidates;
	float TotalWeight = 0.f;

	for (const FResolvedBranchProfile& Profile : Profiles)
	{
		if (bCanonical && !Profile.bAllowedOnCanonicalRoute)
		{
			continue;
		}
		if (!bCanonical && !Profile.bAllowedOnOptionalRoute)
		{
			continue;
		}
		if (bNearHub && !Profile.bAllowedNearHub)
		{
			continue;
		}
		if (bNearCheckpoint && !Profile.bAllowedNearCheckpoint)
		{
			continue;
		}
		if (!PlacementWindowMatches(Profile.PreferredWindow, NormalizedDistance, bNearHub, bNearCheckpoint))
		{
			continue;
		}

		float Weight = FMath::Max(Profile.SelectionWeight, 0.01f);
		if (bPreferReconnect)
		{
			if (Profile.Intent == EBranchProfileIntent::HubConnector || Profile.Intent == EBranchProfileIntent::CanonicalBypass)
			{
				Weight *= 2.0f;
			}
			if (Profile.Intent == EBranchProfileIntent::PocketChain)
			{
				Weight *= 0.35f;
			}
		}
		else if (Profile.Intent == EBranchProfileIntent::PocketChain)
		{
			Weight *= 1.8f;
		}

		Candidates.Add({&Profile, Weight});
		TotalWeight += Weight;
	}

	if (Candidates.Num() == 0)
	{
		return Profiles.Num() > 0 ? &Profiles[0] : nullptr;
	}

	float Pick = Rng.FRandRange(0.f, TotalWeight);
	for (const FWeightedCandidate& Candidate : Candidates)
	{
		Pick -= Candidate.Weight;
		if (Pick <= 0.f)
		{
			return Candidate.Profile;
		}
	}

	return Candidates.Last().Profile;
}
}

ETopologyNodeType UTraversalTopologyGenerator::SelectNodeType(float NormalizedDist,
                                                              int32 DifficultyTier,
                                                              float BranchDensity,
                                                              FRandomStream& Rng) const
{
	if (NormalizedDist < 0.05f) return ETopologyNodeType::EntryBuffer;
	if (NormalizedDist > 0.95f) return ETopologyNodeType::ExitApproach;

	const float R = Rng.GetFraction();

	if (NormalizedDist < 0.20f)
	{
		return (R < 0.4f) ? ETopologyNodeType::TurnPocket : ETopologyNodeType::WideTransit;
	}

	if (NormalizedDist < 0.50f)
	{
		if (DifficultyTier >= 2 && R < 0.3f) return ETopologyNodeType::AmbushPocket;
		if (R < 0.25f)                        return ETopologyNodeType::VerticalDrop;
		if (R < 0.55f)                        return ETopologyNodeType::NarrowTransit;
		return ETopologyNodeType::WideTransit;
	}

	if (NormalizedDist < 0.75f)
	{
		if (R < 0.25f) return ETopologyNodeType::HubChamber;
		if (R < 0.45f) return ETopologyNodeType::WreckPocket;
		if (R < 0.60f) return ETopologyNodeType::ResourcePocket;
		return ETopologyNodeType::WideTransit;
	}

	return (R < 0.35f) ? ETopologyNodeType::TurnPocket : ETopologyNodeType::WideTransit;
}

FVector UTraversalTopologyGenerator::ComputeNodePosition(int32 NodeIndex,
                                                         int32 TotalNodes,
                                                         const FRouteGenSpec& Spec,
                                                         float VerticalityBias,
                                                         FRandomStream& Rng) const
{
	const float T = (TotalNodes > 1) ? (float)NodeIndex / (float)(TotalNodes - 1) : 0.f;
	return ComputeNodePositionAtT(T, Spec, VerticalityBias, Rng);
}

FVector UTraversalTopologyGenerator::ComputeNodePositionAtT(float T,
                                                            const FRouteGenSpec& Spec,
                                                            float VerticalityBias,
                                                            FRandomStream& Rng) const
{
	const float RouteLengthCm = Spec.RouteLengthMeters * 100.f;
	const float X = T * RouteLengthCm;

	const float StartZ = -Spec.StartDepthMeters * 100.f;
	const float EndZ = -Spec.EndDepthMeters * 100.f;
	const float BaseZ = FMath::Lerp(StartZ, EndZ, T);
	const float ZVariation = (Rng.GetFraction() - 0.5f) * 2.f
		* 800.f * Spec.RouteLengthMeters / 4000.f
		* VerticalityBias;
	const float Z = BaseZ + ZVariation;

	const float YMax = Spec.RouteLengthMeters * 15.f;
	const float Y = (Rng.GetFraction() - 0.5f) * 2.f * YMax;

	return FVector(X, Y, Z);
}

ETraversalComplexityTier UTraversalTopologyGenerator::ResolveComplexityTier(const FRouteGenSpec& Spec,
                                                                            const URouteArchetypeDataAsset* Archetype) const
{
	if (Spec.ComplexityTier != ETraversalComplexityTier::Moderate)
	{
		return Spec.ComplexityTier;
	}

	if (Archetype)
	{
		return Archetype->DefaultComplexityTier;
	}

	return ETraversalComplexityTier::Moderate;
}

FTraversalComplexityBudget UTraversalTopologyGenerator::ResolveComplexityBudget(ETraversalComplexityTier ComplexityTier,
                                                                                const URouteArchetypeDataAsset* Archetype) const
{
	FTraversalComplexityBudget Budget;
	if (!Archetype)
	{
		return Budget;
	}

	const int32 BaseOptional = FMath::Max(0, Archetype->Complexity.MaxOptionalSideBranches);
	const int32 BaseHubs = FMath::Max(0, Archetype->Complexity.MaxHubCount);
	const int32 BasePocketDepth = FMath::Max(1, Archetype->Complexity.MaxPocketDepth);
	const int32 BaseSplits = FMath::Max(1, Archetype->Flow.MaxSplitAnchors);
	const int32 BaseSpine = FMath::Max(6, Archetype->Flow.MainSpineNodeCount);

	switch (ComplexityTier)
	{
	case ETraversalComplexityTier::Simple:
		Budget.MaxCanonicalBranchCount = 1;
		Budget.MaxOptionalBranchCount = FMath::Clamp(FMath::CeilToInt((float)BaseOptional * 0.5f), 0, 4);
		Budget.MaxHubCount = FMath::Clamp(FMath::CeilToInt((float)BaseHubs * 0.5f), 0, 2);
		Budget.MaxPocketDepth = 1;
		Budget.MaxReconnectCount = 1;
		Budget.MaxOptionalLengthRatio = 0.25f;
		Budget.MaxNodeBudget = BaseSpine + BaseSplits * 4;
		break;

	case ETraversalComplexityTier::Dense:
		Budget.MaxCanonicalBranchCount = 2;
		Budget.MaxOptionalBranchCount = FMath::Clamp(BaseOptional + 2, 0, 10);
		Budget.MaxHubCount = FMath::Clamp(BaseHubs + 1, 0, 5);
		Budget.MaxPocketDepth = FMath::Clamp(BasePocketDepth + 1, 1, 3);
		Budget.MaxReconnectCount = 4;
		Budget.MaxOptionalLengthRatio = 0.5f;
		Budget.MaxNodeBudget = BaseSpine + BaseSplits * 9;
		break;

	case ETraversalComplexityTier::Moderate:
	default:
		Budget.MaxCanonicalBranchCount = 1;
		Budget.MaxOptionalBranchCount = FMath::Clamp(BaseOptional, 0, 8);
		Budget.MaxHubCount = FMath::Clamp(BaseHubs, 0, 4);
		Budget.MaxPocketDepth = BasePocketDepth;
		Budget.MaxReconnectCount = 2;
		Budget.MaxOptionalLengthRatio = 0.35f;
		Budget.MaxNodeBudget = BaseSpine + BaseSplits * 6;
		break;
	}

	return Budget;
}

void UTraversalTopologyGenerator::AddBranchNodes(const FRouteGenSpec& Spec,
                                                 float BranchDensity,
                                                 FRandomStream& Rng,
                                                 TArray<FTraversalTopologyNode>& InOutNodes) const
{
	const int32 BranchCount = FMath::RoundToInt(BranchDensity * 3.f);

	for (int32 BranchIdx = 0; BranchIdx < BranchCount; BranchIdx++)
	{
		const int32 MainPathCount = InOutNodes.Num();
		if (MainPathCount < 3)
		{
			break;
		}

		const int32 ParentIdx = FMath::RoundToInt(Rng.FRandRange(0.2f, 0.7f) * (MainPathCount - 1));

		FTraversalTopologyNode Branch;
		Branch.NodeID = InOutNodes.Num();
		Branch.bIsBranch = true;
		Branch.bIsDeadEnd = true;
		Branch.bIsCanonicalPath = false;
		Branch.bIsOptionalSideContent = true;
		Branch.LogicalRole = ELogicalRouteNodeRole::PocketDeadEnd;
		Branch.LogicalDepth = 1;
		Branch.NodeType = (Rng.GetFraction() < 0.5f) ? ETopologyNodeType::WreckPocket : ETopologyNodeType::ResourcePocket;
		Branch.PreferredRadius = RadiusForNodeType(Branch.NodeType);
		Branch.NormalizedDistance = InOutNodes[ParentIdx].NormalizedDistance;
		Branch.BranchIndex = INDEX_NONE;

		const FVector& ParentPos = InOutNodes[ParentIdx].WorldPosition;
		const float BranchLength = Rng.FRandRange(600.f, 1800.f) * 100.f;
		const float Angle = Rng.FRandRange(30.f, 90.f) * (Rng.GetFraction() > 0.5f ? 1.f : -1.f);
		const float Rad = FMath::DegreesToRadians(Angle);
		Branch.WorldPosition = ParentPos + FVector(
			BranchLength * FMath::Cos(Rad),
			BranchLength * FMath::Sin(Rad),
			Rng.FRandRange(-500.f, 500.f) * 100.f);

		InOutNodes[ParentIdx].NextNodeIDs.Add(Branch.NodeID);
		InOutNodes.Add(Branch);
	}
}

bool UTraversalTopologyGenerator::BuildConstrainedGraphTopology(const FRouteGenSpec& Spec,
                                                                const FRouteSeedCascade& Seeds,
                                                                const URouteArchetypeDataAsset* Archetype,
                                                                TArray<FTraversalTopologyNode>& OutNodes) const
{
	if (!Archetype || !Archetype->bUseConstrainedGraphPattern)
	{
		return false;
	}

	FRandomStream Rng(Seeds.TopologySeed);
	const bool bUseGroupedAuthoring = Archetype->bUseGroupedConstrainedAuthoring;
	const int32 SpineNodeCount = FMath::Clamp(bUseGroupedAuthoring ? Archetype->Flow.MainSpineNodeCount : Archetype->MainSpineNodeCount, 6, 18);
	const ETraversalComplexityTier ComplexityTier = ResolveComplexityTier(Spec, Archetype);
	const FTraversalComplexityBudget ComplexityBudget = ResolveComplexityBudget(ComplexityTier, Archetype);
	const float VerticalityBias = bUseGroupedAuthoring ? Archetype->SpatialShape.VerticalityBias : Archetype->VerticalityBias;
	const float TrunkRadius = bUseGroupedAuthoring ? Archetype->Volumes.TrunkRadiusCm : Archetype->TrunkRadiusCm;
	const float BranchSeparation = bUseGroupedAuthoring ? Archetype->SpatialShape.BranchSeparationCm : Archetype->ConstrainedBranchSeparationCm;
	const float BranchVerticalOffset = bUseGroupedAuthoring ? Archetype->SpatialShape.VerticalOffsetCm : Archetype->ConstrainedVerticalOffsetCm;
	int32 MaxSplitAnchors = FMath::Clamp(bUseGroupedAuthoring ? Archetype->Flow.MaxSplitAnchors : Archetype->MaxSplitAnchors, 1, 6);
	int32 MaxFanout = FMath::Clamp(bUseGroupedAuthoring ? Archetype->Flow.MaxBranchFanout : Archetype->MaxBranchFanout, 2, 4);
	const ECheckpointSpaceShape StartCheckpointShape = bUseGroupedAuthoring ? Archetype->Checkpoints.StartShape : ECheckpointSpaceShape::Pocket;
	const ECheckpointSpaceShape EndCheckpointShape = bUseGroupedAuthoring ? Archetype->Checkpoints.EndShape : ECheckpointSpaceShape::Pocket;
	const float StartCheckpointRadius = bUseGroupedAuthoring ? Archetype->Checkpoints.StartRadiusCm : FMath::Max(TrunkRadius, 5200.f);
	const float EndCheckpointRadius = bUseGroupedAuthoring ? Archetype->Checkpoints.EndRadiusCm : FMath::Max(TrunkRadius, 5200.f);
	const ECheckpointSpaceShape ResolvedStartCheckpointShape = Spec.bForceStartInterfaceOnly ? ECheckpointSpaceShape::InterfaceOnly : StartCheckpointShape;
	const ECheckpointSpaceShape ResolvedEndCheckpointShape = Spec.bForceEndInterfaceOnly ? ECheckpointSpaceShape::InterfaceOnly : EndCheckpointShape;
	const float ResolvedStartCheckpointRadius = Spec.bForceStartInterfaceOnly ? TrunkRadius : StartCheckpointRadius;
	const float ResolvedEndCheckpointRadius = Spec.bForceEndInterfaceOnly ? TrunkRadius : EndCheckpointRadius;
	int32 MaxHubCount = FMath::Clamp(bUseGroupedAuthoring ? Archetype->Complexity.MaxHubCount : Archetype->MaxHubCount, 0, 4);
	const int32 RawMaxOptionalBranches = bUseGroupedAuthoring ? Archetype->Complexity.MaxOptionalSideBranches : Archetype->MaxOptionalSideBranches;
	int32 MaxOptionalBranches = FMath::Clamp(RawMaxOptionalBranches, 0, 8);
	int32 MaxPocketDepth = FMath::Clamp(bUseGroupedAuthoring ? Archetype->Complexity.MaxPocketDepth : Archetype->MaxPocketDepth, 1, 2);
	const float HubChance = bUseGroupedAuthoring ? Archetype->Rhythm.HubChance : Archetype->HubChance;
	const float RejoinChance = bUseGroupedAuthoring ? Archetype->Flow.RejoinChance : Archetype->RejoinChance;
	const float PocketChance = bUseGroupedAuthoring ? Archetype->Rhythm.PocketChance : Archetype->PocketChance;
	const float BranchDensity = bUseGroupedAuthoring ? Archetype->Rhythm.BranchDensity : Archetype->BranchDensity;
	const float HubRadius = bUseGroupedAuthoring ? Archetype->Volumes.HubRadiusCm : Archetype->HubRadiusCm;
	const float PocketRadius = bUseGroupedAuthoring ? Archetype->Volumes.PocketRadiusCm : Archetype->PocketRadiusCm;
	const float MergeRadius = bUseGroupedAuthoring ? Archetype->Volumes.MergeRadiusCm : Archetype->MergeRadiusCm;
	const TArray<FResolvedBranchProfile> ResolvedBranchProfiles = BuildResolvedBranchProfiles(Archetype);

	switch (ComplexityTier)
	{
	case ETraversalComplexityTier::Simple:
		MaxSplitAnchors = FMath::Max(1, FMath::CeilToInt((float)MaxSplitAnchors * 0.5f));
		MaxFanout = FMath::Clamp(MaxFanout, 2, 3);
		break;
	case ETraversalComplexityTier::Dense:
		MaxSplitAnchors = FMath::Clamp(MaxSplitAnchors + 1, 1, 6);
		MaxFanout = 4;
		break;
	case ETraversalComplexityTier::Moderate:
	default:
		break;
	}
	MaxHubCount = FMath::Min(MaxHubCount, ComplexityBudget.MaxHubCount);
	MaxOptionalBranches = FMath::Min(MaxOptionalBranches, ComplexityBudget.MaxOptionalBranchCount);
	MaxPocketDepth = FMath::Min(MaxPocketDepth, ComplexityBudget.MaxPocketDepth);

	if (bUseGroupedAuthoring
		&& MaxOptionalBranches == 0
		&& BranchDensity > KINDA_SMALL_NUMBER
		&& MaxSplitAnchors > 0)
	{
		const int32 LegacyBudget = FMath::Clamp(Archetype->MaxOptionalSideBranches, 0, 8);
		MaxOptionalBranches = LegacyBudget > 0 ? LegacyBudget : 1;
		UE_LOG(LogRouteGen, Warning,
			TEXT("[C4Settings] Grouped MaxOptionalSideBranches was 0 while BranchDensity=%.2f and MaxSplit=%d. Falling back to %d."),
			BranchDensity,
			MaxSplitAnchors,
			MaxOptionalBranches);
	}

	UE_LOG(LogRouteGen, Log,
		TEXT("[C4Settings] Grouped=%d Complexity=%d Spine=%d MaxSplit=%d MaxFanout=%d MaxHubs=%d MaxOptional=%d (GroupedRaw=%d Legacy=%d) MaxPocketDepth=%d BranchDensity=%.2f HubChance=%.2f RejoinChance=%.2f PocketChance=%.2f DecorativeChance=0.00 Profiles=%d"),
		bUseGroupedAuthoring,
		(int32)ComplexityTier,
		SpineNodeCount,
		MaxSplitAnchors,
		MaxFanout,
		MaxHubCount,
		MaxOptionalBranches,
		RawMaxOptionalBranches,
		Archetype->MaxOptionalSideBranches,
		MaxPocketDepth,
		BranchDensity,
		HubChance,
		RejoinChance,
		PocketChance,
		ResolvedBranchProfiles.Num());
	UE_LOG(LogRouteGen, Log,
		TEXT("[C4Budget] Canonical=%d Optional=%d Hubs=%d PocketDepth=%d Reconnects=%d OptionalLen=%.2f NodeBudget=%d"),
		ComplexityBudget.MaxCanonicalBranchCount,
		ComplexityBudget.MaxOptionalBranchCount,
		ComplexityBudget.MaxHubCount,
		ComplexityBudget.MaxPocketDepth,
		ComplexityBudget.MaxReconnectCount,
		ComplexityBudget.MaxOptionalLengthRatio,
		ComplexityBudget.MaxNodeBudget);

	auto AddNode = [&](ETopologyNodeType Type,
	                   const FVector& Position,
	                   float NormalizedDistance,
	                   float Radius,
	                   bool bIsBranch,
	                   bool bIsDeadEnd,
	                   bool bIsCanonicalPath,
	                   bool bIsOptionalSideContent,
	                   bool bIsDecorativeDisconnected,
	                   ELogicalRouteNodeRole LogicalRole,
	                   FName BranchProfileID,
	                   EBranchProfileIntent BranchIntent,
	                   int32 LogicalDepth,
	                   int32 BranchStage,
	                   int32 BranchIndex) -> int32
	{
		FTraversalTopologyNode Node;
		Node.NodeID = OutNodes.Num();
		Node.NodeType = Type;
		Node.WorldPosition = Position;
		Node.NormalizedDistance = NormalizedDistance;
		Node.PreferredRadius = Radius;
		Node.BranchStage = BranchStage;
		Node.BranchIndex = BranchIndex;
		Node.BranchProfileID = BranchProfileID;
		Node.BranchIntent = BranchIntent;
		Node.LogicalDepth = LogicalDepth;
		Node.LogicalRole = LogicalRole;
		Node.bIsBranch = bIsBranch;
		Node.bIsDeadEnd = bIsDeadEnd;
		Node.bIsCanonicalPath = bIsCanonicalPath;
		Node.bIsOptionalSideContent = bIsOptionalSideContent;
		Node.bIsDecorativeDisconnected = bIsDecorativeDisconnected;
		OutNodes.Add(Node);
		return Node.NodeID;
	};

	TArray<int32> SpineNodeIDs;
	SpineNodeIDs.Reserve(SpineNodeCount);

	for (int32 Index = 0; Index < SpineNodeCount; Index++)
	{
		const float T = (SpineNodeCount > 1) ? (float)Index / (float)(SpineNodeCount - 1) : 0.f;
		ETopologyNodeType Type = ETopologyNodeType::WideTransit;
		ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
		float PreferredRadius = TrunkRadius;
		ECheckpointSpaceShape CheckpointShape = ECheckpointSpaceShape::Goulot;
		if (Index == 0)
		{
			Type = ETopologyNodeType::StartCheckpointSpace;
			LogicalRole = ELogicalRouteNodeRole::StartCheckpoint;
			PreferredRadius = ResolvedStartCheckpointRadius;
			CheckpointShape = ResolvedStartCheckpointShape;
		}
		else if (Index == 1)
		{
			Type = ETopologyNodeType::EntryBuffer;
		}
		else if (Index == SpineNodeCount - 2)
		{
			Type = ETopologyNodeType::ExitApproach;
		}
		else if (Index == SpineNodeCount - 1)
		{
			Type = ETopologyNodeType::EndCheckpointSpace;
			LogicalRole = ELogicalRouteNodeRole::EndCheckpoint;
			PreferredRadius = ResolvedEndCheckpointRadius;
			CheckpointShape = ResolvedEndCheckpointShape;
		}

		const FVector Position = ComputeNodePositionAtT(T, Spec, VerticalityBias, Rng);
		const int32 NodeID = AddNode(
			Type,
			Position,
			T,
			PreferredRadius,
			false,
			false,
			true,
			false,
			false,
			LogicalRole,
			NAME_None,
			EBranchProfileIntent::CanonicalBypass,
			0,
			0,
			INDEX_NONE);
		OutNodes[NodeID].CheckpointSpaceShape = CheckpointShape;
		SpineNodeIDs.Add(NodeID);
	}

	for (int32 Index = 0; Index < SpineNodeIDs.Num() - 1; Index++)
	{
		OutNodes[SpineNodeIDs[Index]].NextNodeIDs.Add(SpineNodeIDs[Index + 1]);
	}

	TArray<int32> CandidateSpineIndices;
	for (int32 Index = 2; Index < SpineNodeIDs.Num() - 2; Index++)
	{
		CandidateSpineIndices.Add(Index);
	}

	int32 BranchIndexCounter = 0;
	int32 SplitAnchorCount = 0;
	int32 HubCount = 0;
	int32 OptionalBranchCount = 0;
	int32 ReconnectCount = 0;

	for (int32 CandidatePos = 0; CandidatePos < CandidateSpineIndices.Num(); CandidatePos++)
	{
		if (OutNodes.Num() >= ComplexityBudget.MaxNodeBudget)
		{
			break;
		}

		const int32 SpineIndex = CandidateSpineIndices[CandidatePos];
		const int32 SplitNodeID = SpineNodeIDs[SpineIndex];
		const bool bCanSpawnOptionalBranches = OptionalBranchCount < MaxOptionalBranches;
		const bool bMakeHub = HubCount < MaxHubCount && Rng.GetFraction() < HubChance;
		bool bMakeSplit = SplitAnchorCount < MaxSplitAnchors
			&& bCanSpawnOptionalBranches
			&& Rng.GetFraction() < FMath::Max(BranchDensity, 0.2f);
		const bool bForceFallbackSplit = OptionalBranchCount == 0
			&& MaxOptionalBranches > 0
			&& CandidatePos == CandidateSpineIndices.Num() / 2;
		if (!bMakeHub && !bMakeSplit && bForceFallbackSplit)
		{
			bMakeSplit = true;
		}

		if (!bMakeHub && !bMakeSplit)
		{
			continue;
		}

		if (bMakeHub)
		{
			OutNodes[SplitNodeID].NodeType = ETopologyNodeType::HubChamber;
			OutNodes[SplitNodeID].PreferredRadius = HubRadius;
			OutNodes[SplitNodeID].LogicalRole = ELogicalRouteNodeRole::Hub;
			HubCount++;
		}
		else
		{
			OutNodes[SplitNodeID].NodeType = ETopologyNodeType::SplitAnchor;
			OutNodes[SplitNodeID].PreferredRadius = FMath::Max(TrunkRadius, HubRadius * 0.8f);
		}

		if (!bCanSpawnOptionalBranches)
		{
			continue;
		}

		const int32 AdditionalBranches = FMath::Clamp(Rng.RandRange(1, MaxFanout - 1), 1, MaxOptionalBranches - OptionalBranchCount);
		if (AdditionalBranches <= 0)
		{
			continue;
		}

		SplitAnchorCount++;

		const FVector SplitPos = OutNodes[SplitNodeID].WorldPosition;
		const FVector FuturePos = OutNodes[SpineNodeIDs[FMath::Min(SpineIndex + 2, SpineNodeIDs.Num() - 1)]].WorldPosition;
		FVector AlongDir;
		FVector SideDir;
		BuildOffsetFrame(SplitPos, FuturePos, AlongDir, SideDir);

		for (int32 LocalBranchIdx = 0; LocalBranchIdx < AdditionalBranches && OptionalBranchCount < MaxOptionalBranches; LocalBranchIdx++)
		{
			if (OutNodes.Num() >= ComplexityBudget.MaxNodeBudget)
			{
				break;
			}

			const int32 BranchIndex = BranchIndexCounter++;
			const float SideSign = (LocalBranchIdx % 2 == 0) ? 1.f : -1.f;
			const float Layer = 1.f + 0.45f * (float)(LocalBranchIdx / 2);
			const float BranchOffset = BranchSeparation * 0.5f * Layer * SideSign;
			const float VerticalOffset = BranchVerticalOffset * (LocalBranchIdx == 1 ? -0.5f : 0.5f);
			const int32 BranchStage = SplitAnchorCount;
			const bool bNearHub = OutNodes[SplitNodeID].LogicalRole == ELogicalRouteNodeRole::Hub;
			const bool bNearCheckpoint = OutNodes[SplitNodeID].NormalizedDistance <= 0.18f || OutNodes[SplitNodeID].NormalizedDistance >= 0.82f;
			const bool bPreferReconnect = (SpineIndex + 3 < SpineNodeIDs.Num() - 1)
				&& (ReconnectCount < ComplexityBudget.MaxReconnectCount)
				&& (Rng.GetFraction() < RejoinChance);
			const FResolvedBranchProfile* BranchProfile = SelectBranchProfile(
				ResolvedBranchProfiles,
				OutNodes[SplitNodeID].NormalizedDistance,
				false,
				bNearHub,
				bNearCheckpoint,
				bPreferReconnect,
				Rng);
			if (!BranchProfile)
			{
				continue;
			}

			const bool bShouldRejoin = (SpineIndex + 3 < SpineNodeIDs.Num() - 1)
				&& (ReconnectCount < ComplexityBudget.MaxReconnectCount)
				&& (Rng.GetFraction() < FMath::Clamp((RejoinChance + BranchProfile->RejoinChance) * 0.5f, 0.f, 1.f));
			const int32 PocketDepth = FMath::Clamp(Rng.RandRange(1, FMath::Max(1, BranchProfile->MaxDepth)), 1, MaxPocketDepth);

			UE_LOG(LogRouteGen, Log,
				TEXT("[C4ProfileSelect] SplitNode=%d Profile=%s Intent=%d Optional=1 Rejoin=%d Window=%d Depth=%d Radius=%.0f"),
				SplitNodeID,
				*BranchProfile->ProfileID.ToString(),
				(int32)BranchProfile->Intent,
				bShouldRejoin,
				(int32)BranchProfile->PreferredWindow,
				PocketDepth,
				BranchProfile->TargetRadiusCm);

			if (bShouldRejoin)
			{
				ReconnectCount++;
				const int32 MaxForwardAdvance = FMath::Clamp(
					FMath::RoundToInt((float)(SpineNodeIDs.Num() - SpineIndex - 2) * ComplexityBudget.MaxOptionalLengthRatio),
					2,
					4);
				const int32 MergeSpineIndex = FMath::Clamp(
					SpineIndex + Rng.RandRange(2, FMath::Min(MaxForwardAdvance, SpineNodeIDs.Num() - SpineIndex - 2)),
					SpineIndex + 2,
					SpineNodeIDs.Num() - 2);
				const int32 MergeNodeID = SpineNodeIDs[MergeSpineIndex];
				if (OutNodes[MergeNodeID].NodeType != ETopologyNodeType::HubChamber)
				{
					OutNodes[MergeNodeID].NodeType = ETopologyNodeType::MergeAnchor;
				}
				OutNodes[MergeNodeID].PreferredRadius = FMath::Max(OutNodes[MergeNodeID].PreferredRadius, MergeRadius);

				const FVector MergePos = OutNodes[MergeNodeID].WorldPosition;
				FVector BranchAlong;
				FVector BranchSide;
				BuildOffsetFrame(SplitPos, MergePos, BranchAlong, BranchSide);
				const FVector MidPos = FMath::Lerp(SplitPos, MergePos, 0.5f)
					+ BranchSide * BranchOffset
					+ FVector(0.f, 0.f, VerticalOffset)
					+ BranchAlong * Rng.FRandRange(-4000.f, 4000.f);

				const int32 BranchNodeID = AddNode(
					ETopologyNodeType::BranchTransit,
					MidPos,
					FMath::Clamp((OutNodes[SplitNodeID].NormalizedDistance + OutNodes[MergeNodeID].NormalizedDistance) * 0.5f, 0.f, 1.f),
					BranchProfile->TargetRadiusCm,
					true,
					false,
					false,
					true,
					false,
					ELogicalRouteNodeRole::OptionalSideBranch,
					BranchProfile->ProfileID,
					BranchProfile->Intent,
					1,
					BranchStage,
					BranchIndex);
				OutNodes[BranchNodeID].SemanticTags.AppendTags(BranchProfile->SemanticTags);

				OutNodes[SplitNodeID].NextNodeIDs.Add(BranchNodeID);
				OutNodes[BranchNodeID].NextNodeIDs.Add(MergeNodeID);

				if (Rng.GetFraction() < FMath::Clamp((PocketChance + BranchProfile->PocketChance) * 0.5f, 0.f, 1.f) && PocketDepth > 0)
				{
					int32 PreviousPocketNodeID = BranchNodeID;
					for (int32 Depth = 0; Depth < PocketDepth; Depth++)
					{
						const FVector PocketPos = MidPos
							+ BranchSide * (BranchOffset * 0.45f)
							+ BranchAlong * (2500.f * (Depth + 1))
							+ FVector(0.f, 0.f, VerticalOffset * 0.5f * (Depth + 1));
						const int32 PocketNodeID = AddNode(
							Depth == PocketDepth - 1 ? ETopologyNodeType::ResourcePocket : ETopologyNodeType::BranchTransit,
							PocketPos,
							FMath::Clamp(OutNodes[BranchNodeID].NormalizedDistance + 0.02f * (Depth + 1), 0.f, 1.f),
							Depth == PocketDepth - 1 ? PocketRadius : BranchProfile->TargetRadiusCm * 0.85f,
							true,
							Depth == PocketDepth - 1,
							false,
							true,
							false,
							Depth == PocketDepth - 1 ? ELogicalRouteNodeRole::PocketDeadEnd : ELogicalRouteNodeRole::OptionalSideBranch,
							BranchProfile->ProfileID,
							BranchProfile->Intent,
							Depth + 2,
							BranchStage,
							BranchIndex);
						OutNodes[PocketNodeID].SemanticTags.AppendTags(BranchProfile->SemanticTags);
						OutNodes[PreviousPocketNodeID].NextNodeIDs.Add(PocketNodeID);
						PreviousPocketNodeID = PocketNodeID;
					}
				}
			}
			else
			{
				int32 PreviousNodeID = SplitNodeID;
				FVector PreviousPos = SplitPos;

				for (int32 Depth = 0; Depth < PocketDepth; Depth++)
				{
					const FVector PocketPos = PreviousPos
						+ AlongDir * Rng.FRandRange(9000.f, 16000.f)
						+ SideDir * (BranchOffset * (1.f + 0.25f * Depth))
						+ FVector(0.f, 0.f, VerticalOffset * (1.f + 0.4f * Depth));
					const bool bDeadEnd = Depth == PocketDepth - 1;
					const ETopologyNodeType PocketType = bDeadEnd
						? (Rng.GetFraction() < 0.5f ? ETopologyNodeType::ResourcePocket : ETopologyNodeType::WreckPocket)
						: ETopologyNodeType::BranchTransit;
					const int32 PocketNodeID = AddNode(
						PocketType,
						PocketPos,
						FMath::Clamp(OutNodes[SplitNodeID].NormalizedDistance + 0.03f * (Depth + 1), 0.f, 1.f),
						bDeadEnd ? PocketRadius : BranchProfile->TargetRadiusCm,
						true,
						bDeadEnd,
						false,
						true,
						false,
						bDeadEnd ? ELogicalRouteNodeRole::PocketDeadEnd : ELogicalRouteNodeRole::OptionalSideBranch,
						BranchProfile->ProfileID,
						BranchProfile->Intent,
						Depth + 1,
						BranchStage,
						BranchIndex);
					OutNodes[PocketNodeID].SemanticTags.AppendTags(BranchProfile->SemanticTags);
					OutNodes[PreviousNodeID].NextNodeIDs.Add(PocketNodeID);
					PreviousNodeID = PocketNodeID;
					PreviousPos = PocketPos;
				}
			}

			OptionalBranchCount++;
		}
	}

	return OutNodes.Num() >= SpineNodeCount;
}

bool UTraversalTopologyGenerator::BuildSplitMergeTopology(const FRouteGenSpec& Spec,
                                                          const FRouteSeedCascade& Seeds,
                                                          const URouteArchetypeDataAsset* Archetype,
                                                          TArray<FTraversalTopologyNode>& OutNodes) const
{
	if (!Archetype || !Archetype->bUseSplitMergePattern || Archetype->SplitCount < 2 || Archetype->SplitCount > 3)
	{
		return false;
	}

	FRandomStream Rng(Seeds.TopologySeed);
	const float VerticalityBias = Archetype->VerticalityBias;
	const float TrunkRadius = Archetype->TrunkRadiusCm;
	const float BranchARadius = Archetype->BranchARadiusCm;
	const float BranchBRadius = Archetype->BranchBRadiusCm;
	const float BranchCRadius = Archetype->BranchCRadiusCm;
	const int32 SplitCount = Archetype->SplitCount;
	const float SplitRadius = FMath::Max(FMath::Max3(TrunkRadius, BranchARadius, BranchBRadius), BranchCRadius) + 200.f;
	const float MergeRadius = Archetype->MergeRadiusCm;

	struct FNodeSpec
	{
		ETopologyNodeType Type;
		float T;
		float Radius;
		int32 BranchStage;
		bool bIsBranch;
		bool bIsDeadEnd;
		int32 BranchIndex;
	};

	TArray<FNodeSpec> BaseSpecs;
	BaseSpecs.Add({ETopologyNodeType::StartAnchor, 0.00f, TrunkRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::EntryBuffer, 0.12f, TrunkRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::WideTransit, 0.28f, TrunkRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::SplitAnchor, 0.42f, SplitRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::MergeAnchor, 0.78f, MergeRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::ExitApproach, 0.90f, TrunkRadius, 0, false, false, INDEX_NONE});
	BaseSpecs.Add({ETopologyNodeType::ExitAnchor, 1.00f, TrunkRadius, 0, false, false, INDEX_NONE});

	for (int32 Index = 0; Index < BaseSpecs.Num(); Index++)
	{
		const FNodeSpec& SpecDef = BaseSpecs[Index];
		FTraversalTopologyNode Node;
		Node.NodeID = OutNodes.Num();
		Node.NodeType = SpecDef.Type;
		Node.NormalizedDistance = SpecDef.T;
		Node.PreferredRadius = SpecDef.Radius;
		Node.BranchStage = SpecDef.BranchStage;
		Node.WorldPosition = ComputeNodePositionAtT(SpecDef.T, Spec, VerticalityBias, Rng);
		Node.bIsBranch = SpecDef.bIsBranch;
		Node.bIsDeadEnd = SpecDef.bIsDeadEnd;
		Node.BranchIndex = SpecDef.BranchIndex;
		OutNodes.Add(Node);
	}

	const int32 SplitNodeID = 3;
	const int32 MergeNodeID = 4;
	const FVector SplitPos = OutNodes[SplitNodeID].WorldPosition;
	const FVector MergePos = OutNodes[MergeNodeID].WorldPosition;
	const FVector Axis = MergePos - SplitPos;
	const float AxisLength = Axis.Size();
	const FVector AlongDir = (AxisLength > KINDA_SMALL_NUMBER) ? Axis / AxisLength : FVector::ForwardVector;

	FVector SideDir = FVector::CrossProduct(AlongDir, FVector::UpVector);
	if (SideDir.IsNearlyZero())
	{
		SideDir = FVector::RightVector;
	}
	SideDir = SideDir.GetSafeNormal();

	const FVector MidPoint = (SplitPos + MergePos) * 0.5f;
	const float AlongOffset = AxisLength * FMath::Clamp(Archetype->BranchLengthVariancePct, 0.f, 0.2f) * 0.5f;
	const float FullSeparation = Archetype->BranchSeparationCm;
	const float HalfSeparation = FullSeparation * 0.5f;
	const float HalfVerticalOffset = Archetype->BranchVerticalOffsetCm * 0.5f;

	struct FBranchSpec
	{
		float NormalizedDistance;
		float Radius;
		int32 BranchStage;
		FVector WorldPosition;
		int32 BranchIndex;
	};

	TArray<FBranchSpec> BranchSpecs;
	BranchSpecs.Reserve(SplitCount);
	BranchSpecs.Add({
		0.56f,
		BranchARadius,
		1,
		MidPoint - AlongDir * AlongOffset + SideDir * HalfSeparation + FVector(0.f, 0.f, HalfVerticalOffset),
		0});
	BranchSpecs.Add({
		SplitCount == 3 ? 0.60f : 0.62f,
		BranchBRadius,
		1,
		SplitCount == 3
			? MidPoint + FVector(0.f, 0.f, -HalfVerticalOffset)
			: MidPoint + AlongDir * AlongOffset - SideDir * HalfSeparation - FVector(0.f, 0.f, HalfVerticalOffset),
		1});

	if (SplitCount == 3)
	{
		BranchSpecs.Add({
			0.64f,
			BranchCRadius,
			1,
			MidPoint + AlongDir * AlongOffset - SideDir * HalfSeparation + FVector(0.f, 0.f, 0.f),
			2});
	}

	TArray<int32> BranchNodeIDs;
	for (const FBranchSpec& BranchSpec : BranchSpecs)
	{
		FTraversalTopologyNode Branch;
		Branch.NodeID = OutNodes.Num();
		Branch.NodeType = ETopologyNodeType::BranchTransit;
		Branch.NormalizedDistance = BranchSpec.NormalizedDistance;
		Branch.PreferredRadius = BranchSpec.Radius;
		Branch.BranchStage = BranchSpec.BranchStage;
		Branch.WorldPosition = BranchSpec.WorldPosition;
		Branch.bIsBranch = true;
		Branch.bIsDeadEnd = false;
		Branch.bIsCanonicalPath = true;
		Branch.LogicalRole = ELogicalRouteNodeRole::CanonicalBypass;
		Branch.BranchIndex = BranchSpec.BranchIndex;
		OutNodes.Add(Branch);
		BranchNodeIDs.Add(Branch.NodeID);
	}

	OutNodes[0].NextNodeIDs.Add(1);
	OutNodes[1].NextNodeIDs.Add(2);
	OutNodes[2].NextNodeIDs.Add(SplitNodeID);
	for (const int32 BranchNodeID : BranchNodeIDs)
	{
		OutNodes[SplitNodeID].NextNodeIDs.Add(BranchNodeID);
		OutNodes[BranchNodeID].NextNodeIDs.Add(MergeNodeID);
	}
	OutNodes[MergeNodeID].NextNodeIDs.Add(5);
	OutNodes[5].NextNodeIDs.Add(6);

	return OutNodes.Num() >= 7 + SplitCount;
}

bool UTraversalTopologyGenerator::BuildMultiStageSplitTopology(const FRouteGenSpec& Spec,
                                                               const FRouteSeedCascade& Seeds,
                                                               const URouteArchetypeDataAsset* Archetype,
                                                               TArray<FTraversalTopologyNode>& OutNodes) const
{
	if (!Archetype
		|| !Archetype->bUseMultiStageSplitPattern
		|| Archetype->FirstSplitCount != 3
		|| Archetype->SecondSplitCount != 3
		|| Archetype->MidMergeTargetCount != 2)
	{
		return false;
	}

	FRandomStream Rng(Seeds.TopologySeed);
	const float VerticalityBias = Archetype->VerticalityBias;
	const float TrunkRadius = Archetype->TrunkRadiusCm;
	const float BranchRadii[3] = {
		Archetype->BranchARadiusCm,
		Archetype->BranchBRadiusCm,
		Archetype->BranchCRadiusCm
	};
	const float FirstSplitRadius = FMath::Max(FMath::Max3(TrunkRadius, BranchRadii[0], BranchRadii[1]), BranchRadii[2]) + 200.f;
	const float MidMergeRadius = Archetype->MidMergeRadiusCm;
	const float SecondSplitRadius = Archetype->SecondSplitRadiusCm;
	const float FinalMergeRadius = Archetype->MergeRadiusCm;

	struct FNodeSpec
	{
		ETopologyNodeType Type;
		float T;
		float Radius;
		int32 BranchStage;
		bool bIsBranch;
		int32 BranchIndex;
	};

	TArray<FNodeSpec> BaseSpecs;
	BaseSpecs.Add({ETopologyNodeType::StartAnchor, 0.00f, TrunkRadius, 0, false, INDEX_NONE});      // 0
	BaseSpecs.Add({ETopologyNodeType::EntryBuffer, 0.08f, TrunkRadius, 0, false, INDEX_NONE});      // 1
	BaseSpecs.Add({ETopologyNodeType::WideTransit, 0.18f, TrunkRadius, 0, false, INDEX_NONE});      // 2
	BaseSpecs.Add({ETopologyNodeType::SplitAnchor, 0.28f, FirstSplitRadius, 0, false, INDEX_NONE}); // 3 SplitA
	BaseSpecs.Add({ETopologyNodeType::MergeAnchor, 0.50f, MidMergeRadius, 0, false, INDEX_NONE});   // 4 MergeA
	BaseSpecs.Add({ETopologyNodeType::WideTransit, 0.62f, TrunkRadius, 0, false, INDEX_NONE});      // 5 MidTransit
	BaseSpecs.Add({ETopologyNodeType::SplitAnchor, 0.72f, SecondSplitRadius, 0, false, INDEX_NONE});// 6 SplitB
	BaseSpecs.Add({ETopologyNodeType::MergeAnchor, 0.90f, FinalMergeRadius, 0, false, INDEX_NONE}); // 7 MergeFinal
	BaseSpecs.Add({ETopologyNodeType::ExitAnchor, 1.00f, TrunkRadius, 0, false, INDEX_NONE});       // 8

	for (const FNodeSpec& SpecDef : BaseSpecs)
	{
		FTraversalTopologyNode Node;
		Node.NodeID = OutNodes.Num();
		Node.NodeType = SpecDef.Type;
		Node.NormalizedDistance = SpecDef.T;
		Node.PreferredRadius = SpecDef.Radius;
		Node.BranchStage = SpecDef.BranchStage;
		Node.WorldPosition = ComputeNodePositionAtT(SpecDef.T, Spec, VerticalityBias, Rng);
		Node.bIsBranch = SpecDef.bIsBranch;
		Node.bIsDeadEnd = false;
		Node.BranchIndex = SpecDef.BranchIndex;
		OutNodes.Add(Node);
	}

	const int32 SplitAID = 3;
	const int32 MergeAID = 4;
	const int32 TransitMidID = 5;
	const int32 SplitBID = 6;
	const int32 MergeFinalID = 7;

	const auto BuildFrame = [](const FVector& From, const FVector& To, FVector& OutAlong, FVector& OutSide)
	{
		const FVector Axis = To - From;
		const float AxisLength = Axis.Size();
		OutAlong = (AxisLength > KINDA_SMALL_NUMBER) ? Axis / AxisLength : FVector::ForwardVector;
		OutSide = FVector::CrossProduct(OutAlong, FVector::UpVector);
		if (OutSide.IsNearlyZero())
		{
			OutSide = FVector::RightVector;
		}
		OutSide = OutSide.GetSafeNormal();
	};

	FVector AlongA, SideA;
	BuildFrame(OutNodes[SplitAID].WorldPosition, OutNodes[MergeAID].WorldPosition, AlongA, SideA);
	const FVector MidA = (OutNodes[SplitAID].WorldPosition + OutNodes[MergeAID].WorldPosition) * 0.5f;
	const float SepA = Archetype->FirstSplitSeparationCm * 0.5f;
	const float VertA = Archetype->BranchVerticalOffsetCm * 0.5f;
	const float AlongOffsetA = FVector::Distance(OutNodes[SplitAID].WorldPosition, OutNodes[MergeAID].WorldPosition)
		* FMath::Clamp(Archetype->BranchLengthVariancePct, 0.f, 0.2f) * 0.5f;

	TArray<int32> Stage1BranchIDs;
	for (int32 BranchIndex = 0; BranchIndex < 3; BranchIndex++)
	{
		FTraversalTopologyNode Branch;
		Branch.NodeID = OutNodes.Num();
		Branch.NodeType = ETopologyNodeType::BranchTransit;
		Branch.BranchStage = 1;
		Branch.BranchIndex = BranchIndex;
		Branch.bIsBranch = true;
		Branch.bIsDeadEnd = false;
		Branch.PreferredRadius = BranchRadii[BranchIndex];
		Branch.NormalizedDistance = 0.39f + 0.03f * BranchIndex;

		if (BranchIndex == 0)
		{
			Branch.WorldPosition = MidA - AlongA * AlongOffsetA + SideA * SepA + FVector(0.f, 0.f, VertA);
		}
		else if (BranchIndex == 1)
		{
			Branch.WorldPosition = MidA + FVector(0.f, 0.f, -VertA);
		}
		else
		{
		Branch.WorldPosition = MidA + AlongA * AlongOffsetA - SideA * SepA + FVector(0.f, 0.f, 0.f);
		}
		Branch.bIsCanonicalPath = true;
		Branch.LogicalRole = ELogicalRouteNodeRole::CanonicalBypass;

		OutNodes.Add(Branch);
		Stage1BranchIDs.Add(Branch.NodeID);
	}

	FVector AlongB, SideB;
	BuildFrame(OutNodes[SplitBID].WorldPosition, OutNodes[MergeFinalID].WorldPosition, AlongB, SideB);
	const FVector MidB = (OutNodes[SplitBID].WorldPosition + OutNodes[MergeFinalID].WorldPosition) * 0.5f;
	const float SepB = Archetype->SecondSplitSeparationCm * 0.5f;
	const float VertB = Archetype->BranchVerticalOffsetCm * 0.35f;
	const float AlongOffsetB = FVector::Distance(OutNodes[SplitBID].WorldPosition, OutNodes[MergeFinalID].WorldPosition)
		* FMath::Clamp(Archetype->BranchLengthVariancePct, 0.f, 0.2f) * 0.4f;

	TArray<int32> Stage2BranchIDs;
	for (int32 BranchIndex = 0; BranchIndex < 3; BranchIndex++)
	{
		FTraversalTopologyNode Branch;
		Branch.NodeID = OutNodes.Num();
		Branch.NodeType = ETopologyNodeType::BranchTransit;
		Branch.BranchStage = 2;
		Branch.BranchIndex = BranchIndex;
		Branch.bIsBranch = true;
		Branch.bIsDeadEnd = false;
		Branch.PreferredRadius = BranchRadii[BranchIndex];
		Branch.NormalizedDistance = 0.79f + 0.02f * BranchIndex;

		if (BranchIndex == 0)
		{
			Branch.WorldPosition = MidB - AlongB * AlongOffsetB + SideB * SepB + FVector(0.f, 0.f, VertB);
		}
		else if (BranchIndex == 1)
		{
			Branch.WorldPosition = MidB + FVector(0.f, 0.f, -VertB);
		}
		else
		{
			Branch.WorldPosition = MidB + AlongB * AlongOffsetB - SideB * SepB;
		}
		Branch.bIsCanonicalPath = true;
		Branch.LogicalRole = ELogicalRouteNodeRole::CanonicalBypass;

		OutNodes.Add(Branch);
		Stage2BranchIDs.Add(Branch.NodeID);
	}

	OutNodes[0].NextNodeIDs.Add(1);
	OutNodes[1].NextNodeIDs.Add(2);
	OutNodes[2].NextNodeIDs.Add(SplitAID);

	for (const int32 BranchNodeID : Stage1BranchIDs)
	{
		OutNodes[SplitAID].NextNodeIDs.Add(BranchNodeID);
	}

	OutNodes[Stage1BranchIDs[0]].NextNodeIDs.Add(MergeAID);
	OutNodes[Stage1BranchIDs[1]].NextNodeIDs.Add(MergeAID);
	// Branch 2 from stage A bypasses the first merge and rejoins at the second split stage.
	OutNodes[Stage1BranchIDs[2]].NextNodeIDs.Add(SplitBID);

	OutNodes[MergeAID].NextNodeIDs.Add(TransitMidID);
	OutNodes[TransitMidID].NextNodeIDs.Add(SplitBID);

	for (const int32 BranchNodeID : Stage2BranchIDs)
	{
		OutNodes[SplitBID].NextNodeIDs.Add(BranchNodeID);
		OutNodes[BranchNodeID].NextNodeIDs.Add(MergeFinalID);
	}

	OutNodes[MergeFinalID].NextNodeIDs.Add(8);

	return OutNodes.Num() >= 15;
}

bool UTraversalTopologyGenerator::GenerateTopology(const FRouteGenSpec& Spec,
                                                   const FRouteSeedCascade& Seeds,
                                                   URouteArchetypeDataAsset* Archetype,
                                                   TArray<FTraversalTopologyNode>& OutNodes) const
{
	OutNodes.Reset();

	if (BuildConstrainedGraphTopology(Spec, Seeds, Archetype, OutNodes))
	{
		return true;
	}

	if (BuildMultiStageSplitTopology(Spec, Seeds, Archetype, OutNodes))
	{
		return true;
	}

	if (BuildSplitMergeTopology(Spec, Seeds, Archetype, OutNodes))
	{
		return true;
	}

	FRandomStream Rng(Seeds.TopologySeed);
	const int32 NodeCount = Archetype ? FMath::Clamp(Archetype->MainPathNodeCount, 4, 32) : ComputeNodeCount(Spec.RouteLengthMeters);
	const float BranchDensity = Archetype ? Archetype->BranchDensity : 0.15f;
	const float VerticalityBias = Archetype ? Archetype->VerticalityBias : 0.5f;

	for (int32 Index = 0; Index < NodeCount; Index++)
	{
		FTraversalTopologyNode Node;
		Node.NodeID = Index;
		Node.NormalizedDistance = (NodeCount > 1) ? (float)Index / (float)(NodeCount - 1) : 0.f;

		if (Index == 0)
		{
			Node.NodeType = ETopologyNodeType::StartAnchor;
		}
		else if (Index == NodeCount - 1)
		{
			Node.NodeType = ETopologyNodeType::ExitAnchor;
		}
		else
		{
			Node.NodeType = SelectNodeType(Node.NormalizedDistance, Spec.DifficultyTier, BranchDensity, Rng);
		}

		Node.PreferredRadius = RadiusForNodeType(Node.NodeType);
		Node.WorldPosition = ComputeNodePosition(Index, NodeCount, Spec, VerticalityBias, Rng);
		if (Index < NodeCount - 1)
		{
			Node.NextNodeIDs.Add(Index + 1);
		}

		OutNodes.Add(Node);
	}

	AddBranchNodes(Spec, BranchDensity, Rng, OutNodes);
	return OutNodes.Num() >= 2;
}
