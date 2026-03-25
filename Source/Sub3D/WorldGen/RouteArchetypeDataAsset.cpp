#include "RouteArchetypeDataAsset.h"

#include "Misc/AssertionMacros.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
constexpr int32 CurrentGroupedAuthoringSchemaVersion = 1;

template <typename TValue>
void CopyIfUnset(TValue& Target, const TValue& LegacyValue)
{
	if (Target == TValue{} && LegacyValue != TValue{})
	{
		Target = LegacyValue;
	}
}
}

void URouteArchetypeDataAsset::PostLoad()
{
	Super::PostLoad();
	MigrateGroupedAuthoringIfNeeded();
}

#if WITH_EDITOR
void URouteArchetypeDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(URouteArchetypeDataAsset, bUseGroupedConstrainedAuthoring)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(URouteArchetypeDataAsset, GroupedAuthoringSchemaVersion))
	{
		MigrateGroupedAuthoringIfNeeded();
	}
}
#endif

void URouteArchetypeDataAsset::MigrateGroupedAuthoringIfNeeded()
{
	if (!bUseGroupedConstrainedAuthoring || GroupedAuthoringSchemaVersion >= CurrentGroupedAuthoringSchemaVersion)
	{
		return;
	}

	const FRouteFlowAuthoringSettings DefaultFlow;
	const FRouteCheckpointAuthoringSettings DefaultCheckpoints;
	const FRouteComplexityAuthoringSettings DefaultComplexity;
	const FRouteVolumeAuthoringSettings DefaultVolumes;
	const FRouteSpatialShapeAuthoringSettings DefaultSpatialShape;
	const FRouteRhythmAuthoringSettings DefaultRhythm;

	bool bChanged = false;

	auto AssignInt = [&](int32& Target, int32 LegacyValue, int32 DefaultValue, bool bAllowZero = false)
	{
		if (Target != 0)
		{
			return;
		}

		if (LegacyValue > 0)
		{
			Target = LegacyValue;
			bChanged = true;
			return;
		}

		if (!bAllowZero && DefaultValue > 0)
		{
			Target = DefaultValue;
			bChanged = true;
		}
	};

	auto AssignFloat = [&](float& Target, float LegacyValue, float DefaultValue)
	{
		if (!FMath::IsNearlyZero(Target))
		{
			return;
		}

		if (!FMath::IsNearlyZero(LegacyValue))
		{
			Target = LegacyValue;
			bChanged = true;
			return;
		}

		if (!FMath::IsNearlyZero(DefaultValue))
		{
			Target = DefaultValue;
			bChanged = true;
		}
	};

	AssignInt(Flow.MainSpineNodeCount, MainSpineNodeCount, DefaultFlow.MainSpineNodeCount);
	AssignInt(Flow.MaxSplitAnchors, MaxSplitAnchors, DefaultFlow.MaxSplitAnchors);
	AssignInt(Flow.MaxBranchFanout, MaxBranchFanout, DefaultFlow.MaxBranchFanout);
	AssignFloat(Flow.RejoinChance, RejoinChance, DefaultFlow.RejoinChance);

	AssignFloat(Checkpoints.StartRadiusCm, TrunkRadiusCm, DefaultCheckpoints.StartRadiusCm);
	AssignFloat(Checkpoints.EndRadiusCm, TrunkRadiusCm, DefaultCheckpoints.EndRadiusCm);

	AssignInt(Complexity.MaxHubCount, MaxHubCount, DefaultComplexity.MaxHubCount, true);
	AssignInt(Complexity.MaxDecorativeDisconnectedCavities, MaxDecorativeDisconnectedCavities, DefaultComplexity.MaxDecorativeDisconnectedCavities, true);
	AssignInt(Complexity.MaxPocketDepth, MaxPocketDepth, DefaultComplexity.MaxPocketDepth);

	if (Complexity.MaxOptionalSideBranches == 0
		&& (Rhythm.BranchDensity > KINDA_SMALL_NUMBER || BranchDensity > KINDA_SMALL_NUMBER)
		&& Flow.MaxSplitAnchors > 0)
	{
		Complexity.MaxOptionalSideBranches = MaxOptionalSideBranches > 0
			? MaxOptionalSideBranches
			: DefaultComplexity.MaxOptionalSideBranches;
		bChanged = true;
	}

	AssignFloat(Volumes.TrunkRadiusCm, TrunkRadiusCm, DefaultVolumes.TrunkRadiusCm);
	AssignFloat(Volumes.BranchARadiusCm, BranchARadiusCm, DefaultVolumes.BranchARadiusCm);
	AssignFloat(Volumes.BranchBRadiusCm, BranchBRadiusCm, DefaultVolumes.BranchBRadiusCm);
	AssignFloat(Volumes.BranchCRadiusCm, BranchCRadiusCm, DefaultVolumes.BranchCRadiusCm);
	AssignFloat(Volumes.MergeRadiusCm, MergeRadiusCm, DefaultVolumes.MergeRadiusCm);
	AssignFloat(Volumes.HubRadiusCm, HubRadiusCm, DefaultVolumes.HubRadiusCm);
	AssignFloat(Volumes.PocketRadiusCm, PocketRadiusCm, DefaultVolumes.PocketRadiusCm);
	AssignFloat(Volumes.DecorativeCavityRadiusCm, DecorativeCavityRadiusCm, DefaultVolumes.DecorativeCavityRadiusCm);

	AssignFloat(SpatialShape.BranchSeparationCm, ConstrainedBranchSeparationCm, DefaultSpatialShape.BranchSeparationCm);
	AssignFloat(SpatialShape.VerticalOffsetCm, ConstrainedVerticalOffsetCm, DefaultSpatialShape.VerticalOffsetCm);
	AssignFloat(SpatialShape.VerticalityBias, VerticalityBias, DefaultSpatialShape.VerticalityBias);

	AssignFloat(Rhythm.BranchDensity, BranchDensity, DefaultRhythm.BranchDensity);
	AssignFloat(Rhythm.HubChance, HubChance, DefaultRhythm.HubChance);
	AssignFloat(Rhythm.PocketChance, PocketChance, DefaultRhythm.PocketChance);
	AssignFloat(Rhythm.DecorativeCavityChance, DecorativeCavityChance, DefaultRhythm.DecorativeCavityChance);

	GroupedAuthoringSchemaVersion = CurrentGroupedAuthoringSchemaVersion;

#if WITH_EDITOR
	if (bChanged && !HasAnyFlags(RF_ClassDefaultObject))
	{
		MarkPackageDirty();
		UE_LOG(LogRouteGen, Warning,
			TEXT("[RouteArchetype] Migrated grouped constrained authoring values for %s. OptionalBranches=%d Hubs=%d Decorative=%d"),
			*GetName(),
			Complexity.MaxOptionalSideBranches,
			Complexity.MaxHubCount,
			Complexity.MaxDecorativeDisconnectedCavities);
	}
#endif
}
