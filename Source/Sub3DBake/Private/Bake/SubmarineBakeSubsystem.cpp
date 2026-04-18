#include "Bake/SubmarineBakeSubsystem.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Bake/SubmarineBaySolveService.h"
#include "Bake/SubmarineFloorBakeService.h"
#include "Bake/SubmarineHullBakeService.h"
#include "Bake/SubmarineOpeningBakeService.h"
#include "Bake/SubmarineOuterEnvelopeBakeService.h"
#include "Bake/SubmarinePartitionBakeService.h"
#include "Bake/SubmarineAppendageBakeService.h"
#include "Bake/SubmarineHullProfileService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Data/CompiledSubmarineBaseAsset.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"
#include "Graph/SubmarineFloodGraphBuilder.h"
#include "Validation/SubmarineHullValidation.h"

namespace
{
static bool ValidateFrameRings(const TArray<FFrameRingDef>& FrameRings, TArray<FString>& OutErrors)
{
    bool bValid = true;
    float PreviousAlpha = -1.0f;

    for (int32 Index = 0; Index < FrameRings.Num(); ++Index)
    {
        const FFrameRingDef& FrameRing = FrameRings[Index];
        const FString Label = FString::Printf(TEXT("FrameRing[%d]"), Index);

        if (FrameRing.SpineAlpha < 0.0f || FrameRing.SpineAlpha > 1.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("%s.SpineAlpha must be in [0..1]."), *Label));
            bValid = false;
        }

        if (FrameRing.ThicknessCm < 1.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("%s.ThicknessCm must be >= 1."), *Label));
            bValid = false;
        }

        if (FrameRing.DepthCm < 1.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("%s.DepthCm must be >= 1."), *Label));
            bValid = false;
        }

        if (Index > 0 && FrameRing.SpineAlpha <= PreviousAlpha)
        {
            OutErrors.Add(TEXT("Frame-Rings must be strictly increasing by SpineAlpha."));
            bValid = false;
        }

        PreviousAlpha = FrameRing.SpineAlpha;
    }

    return bValid;
}

static void BuildCompiledFrameRings(
    const FSubmarineHullDef& Hull,
    const TArray<FFrameRingDef>& FrameRings,
    TArray<FCompiledFrameRingData>& OutCompiledFrameRings)
{
    OutCompiledFrameRings.Reset();
    OutCompiledFrameRings.Reserve(FrameRings.Num());

    for (int32 Index = 0; Index < FrameRings.Num(); ++Index)
    {
        const FFrameRingDef& FrameRing = FrameRings[Index];
        FCompiledFrameRingData& Compiled = OutCompiledFrameRings.AddDefaulted_GetRef();
        Compiled.FrameRingId = FrameRing.FrameRingId.IsNone() ? FName(*FString::Printf(TEXT("FrameRing_%d"), Index)) : FrameRing.FrameRingId;
        Compiled.SpineAlpha = FMath::Clamp(FrameRing.SpineAlpha, 0.0f, 1.0f);
        Compiled.PositionX = Compiled.SpineAlpha * Hull.LengthCm;
        Compiled.ThicknessCm = FrameRing.ThicknessCm;
        Compiled.DepthCm = FrameRing.DepthCm;
        Compiled.bIsBayBoundary = FrameRing.bIsBayBoundary;
    }
}

static void BuildCompiledHullData(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    FSub3DCompiledHullData& OutHullData)
{
    OutHullData = FSub3DCompiledHullData();
    OutHullData.LengthCm = Hull.LengthCm;
    OutHullData.Sections.Reserve(RingSequence.Num());

    for (const Sub3DWave2::FGeneratedRingData& Ring : RingSequence)
    {
        FSub3DCompiledHullSection& Section = OutHullData.Sections.AddDefaulted_GetRef();
        Section.PositionX = FMath::Clamp(Ring.SpineAlpha, 0.0f, 1.0f) * Hull.LengthCm;
        Section.HalfHeightCm = FMath::Max(Ring.RadiusCm, KINDA_SMALL_NUMBER);
        Section.HalfWidthCm = Section.HalfHeightCm * FMath::Max(Ring.WidthToHeightRatio, KINDA_SMALL_NUMBER);
        Section.SectionProfile = Ring.Profile;
        Section.SectionRoundness = Ring.Roundness;
        Section.WallThicknessCm = Ring.WallThicknessCm;
    }
}

// ── Hash helpers ──────────────────────────────────────────────────────────────

static uint32 ComputeHullGeometryHash(const USub3DSubmarineAuthoringAsset* Asset)
{
    uint32 Hash = 0;
    const FSubmarineHullDef& H = Asset->Hull;
    Hash = FCrc::MemCrc32(&H.LengthCm,             sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.DefaultHalfWidthCm,   sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.DefaultHalfHeightCm,  sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.DefaultWallThicknessCm, sizeof(float), Hash);
    const uint8 Profile = static_cast<uint8>(H.ProfileParams.Profile);
    Hash = FCrc::MemCrc32(&Profile, sizeof(uint8), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.MyringNoseExponent,  sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.MyringTailAngleDeg,  sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.MyringNoseFraction,  sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.MyringTailFraction,  sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.Series58Fineness,    sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.LongitudinalExponent, sizeof(float), Hash);
    Hash = FCrc::MemCrc32(&H.ProfileParams.ParallelMidbodyFraction, sizeof(float), Hash);
    for (const FControlRingDef& Ring : Asset->ControlRings)
    {
        Hash = FCrc::MemCrc32(&Ring.PositionX,       sizeof(float), Hash);
        Hash = FCrc::MemCrc32(&Ring.HalfWidthCm,     sizeof(float), Hash);
        Hash = FCrc::MemCrc32(&Ring.HalfHeightCm,    sizeof(float), Hash);
        Hash = FCrc::MemCrc32(&Ring.WallThicknessCm, sizeof(float), Hash);
        const uint8 Prof = static_cast<uint8>(Ring.SectionProfile);
        Hash = FCrc::MemCrc32(&Prof, sizeof(uint8), Hash);
        Hash = FCrc::MemCrc32(&Ring.SectionRoundness, sizeof(float), Hash);
    }
    return Hash;
}

static uint32 ComputeLayoutHash(const USub3DSubmarineAuthoringAsset* Asset)
{
    uint32 Hash = 0;
    for (const FStructuralBayDef& Bay : Asset->StructuralBays)
    {
        Hash = FCrc::MemCrc32(&Bay.StartAlpha, sizeof(float), Hash);
        Hash = FCrc::MemCrc32(&Bay.EndAlpha,   sizeof(float), Hash);
    }
    for (const FDeckLevelDef& Deck : Asset->DeckLevels)
    {
        Hash = FCrc::MemCrc32(&Deck.ZOffsetCm, sizeof(float), Hash);
    }
    return Hash;
}

static void AppendErrors(const TArray<FString>& InErrors, TArray<FString>& OutErrors)
{
    OutErrors.Append(InErrors);
}

static void LogErrors(const TCHAR* Prefix, const TArray<FString>& Errors)
{
    for (const FString& Message : Errors)
    {
        UE_LOG(LogTemp, Error, TEXT("%s %s"), Prefix, *Message);
    }
}
} // namespace

TArray<FControlRingDef> USubmarineBakeSubsystem::ResolveEffectiveControlRings(
    const USub3DSubmarineAuthoringAsset* Asset,
    TArray<FString>& OutErrors)
{
    if (!Asset)
    {
        return TArray<FControlRingDef>();
    }

    const FHullProfileParams& ProfileParams = Asset->Hull.ProfileParams;
    if (ProfileParams.Profile != ESub3DHullLongitudinalProfile::Manual)
    {
        TArray<FControlRingDef> GeneratedRings;
        TArray<FString> ProfileErrors;
        if (Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(
                Asset->Hull, GeneratedRings, ProfileErrors))
        {
            return GeneratedRings;
        }
        else
        {
            OutErrors.Append(ProfileErrors);
        }
    }

    return Asset->ControlRings;
}

bool USubmarineBakeSubsystem::ValidateAuthoringAsset(USub3DSubmarineAuthoringAsset* Asset, TArray<FString>& OutErrors)
{
    OutErrors.Reset();
    if (!Asset)
    {
        OutErrors.Add(TEXT("Authoring Asset is null."));
        return false;
    }

    const TArray<FControlRingDef> EffectiveRings = ResolveEffectiveControlRings(Asset, OutErrors);

    bool bValid = Sub3DWave1::FSubmarineHullValidation::ValidateGeometry(Asset->Hull, EffectiveRings, OutErrors);

    if (EffectiveRings.Num() < 2)
    {
        OutErrors.Add(TEXT("At least two Control Rings are required."));
        bValid = false;
    }

    if (!ValidateFrameRings(Asset->FrameRings, OutErrors))
    {
        bValid = false;
    }

    TArray<FString> LocalErrors;

    // Build temporary ring sequence and hull data for validation
    FSub3DCompiledHullData TempHullData;
    {
        TArray<Sub3DWave2::FGeneratedRingData> TempRingSequence;
        TArray<FString> RingErrors;
        if (Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
                Asset->Hull, EffectiveRings, 48, TempRingSequence, RingErrors, Asset->FrameRings))
        {
            BuildCompiledHullData(Asset->Hull, TempRingSequence, TempHullData);
        }
        else
        {
            TempHullData.LengthCm = Asset->Hull.LengthCm;
            FSub3DCompiledHullSection DefaultSection;
            DefaultSection.HalfWidthCm = Asset->Hull.DefaultHalfWidthCm;
            DefaultSection.HalfHeightCm = Asset->Hull.DefaultHalfHeightCm;
            DefaultSection.WallThicknessCm = Asset->Hull.DefaultWallThicknessCm;
            DefaultSection.SectionProfile = Asset->Hull.DefaultSectionProfile;
            DefaultSection.SectionRoundness = Asset->Hull.DefaultSectionRoundness;
            DefaultSection.PositionX = 0.0f;
            TempHullData.Sections.Add(DefaultSection);
            DefaultSection.PositionX = Asset->Hull.LengthCm;
            TempHullData.Sections.Add(DefaultSection);
        }
    }

    TArray<FCompiledBayData> CompiledBays;
    if (!Sub3DWave3::FSubmarineBaySolveService::SolveStructuralBays(
            Asset->Hull,
            Asset->FrameRings,
            Asset->StructuralBays,
            CompiledBays,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledDeckData> CompiledDecks;
    if (!Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(
            TempHullData,
            CompiledBays,
            Asset->DeckLevels,
            CompiledDecks,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    if (!Sub3DWave4::FSubmarineFloorBakeService::ValidateFloorRegionWalkability(
            TempHullData,
            CompiledBays,
            CompiledDecks,
            Asset->FloorRegions,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledFloorRegionData> CompiledFloorRegions;
    if (!Sub3DWave4::FSubmarineFloorBakeService::BakeFloorRegions(
            TempHullData,
            CompiledBays,
            CompiledDecks,
            Asset->FloorRegions,
            CompiledFloorRegions,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledOpeningData> CompiledOpenings;
    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeOpenings(
            CompiledFloorRegions,
            Asset->Openings,
            CompiledOpenings,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledConnectorData> CompiledConnectors;
    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeConnectors(
            CompiledDecks,
            CompiledOpenings,
            Asset->Connectors,
            CompiledConnectors,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledClosureData> CompiledClosures;
    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeClosures(
            CompiledOpenings,
            Asset->Closures,
            CompiledClosures,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    TArray<FCompiledPartitionData> CompiledPartitions;
    if (!Sub3DWave7::FSubmarinePartitionBakeService::BakePartitions(
            CompiledOpenings,
            Asset->PressureBulkheads,
            Asset->InternalWalls,
            CompiledPartitions,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    LocalErrors.Reset();
    FCompiledFloodGraph FloodGraph;
    if (!Sub3DWave8::FSubmarineFloodGraphBuilder::BuildFloodGraph(
            CompiledBays,
            CompiledDecks,
            CompiledConnectors,
            CompiledClosures,
            FloodGraph,
            LocalErrors))
    {
        AppendErrors(LocalErrors, OutErrors);
        bValid = false;
    }

    return bValid;
}

bool USubmarineBakeSubsystem::BakeBase(USub3DSubmarineAuthoringAsset* Asset)
{
    TArray<FString> Errors;
    if (!ValidateAuthoringAsset(Asset, Errors))
    {
        LogErrors(TEXT("Bake Validation Error:"), Errors);
        return false;
    }

    const FSubmarineHullDef& Hull = Asset->Hull;
    const TArray<FControlRingDef> ControlRings = ResolveEffectiveControlRings(Asset, Errors);

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    if (!Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
            Hull,
            ControlRings,
            48,
            RingSequence,
            Errors,
            Asset->FrameRings))
    {
        LogErrors(TEXT("BuildRingSequence Error:"), Errors);
        return false;
    }

    if (!Sub3DWave2::FSubmarineRingSequenceBuilder::ValidateRingContinuity(RingSequence, 1000.0f, Errors))
    {
        LogErrors(TEXT("ValidateRingContinuity Error:"), Errors);
        return false;
    }

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = 32;
    Settings.bBakeCollision = true;
    Settings.CollisionRadialSegments = 12;

    UCompiledSubmarineBaseAsset* BakedAsset = NewObject<UCompiledSubmarineBaseAsset>(this);
    if (!BakedAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to allocate UCompiledSubmarineBaseAsset."));
        return false;
    }

    BakedAsset->HullLengthCm = Hull.LengthCm;
    BuildCompiledHullData(Hull, RingSequence, BakedAsset->HullData);

    if (!Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(Hull, RingSequence, Settings, BakedAsset->ExteriorHull))
    {
        UE_LOG(LogTemp, Error, TEXT("BakeExteriorHull failed."));
        return false;
    }

    if (!Sub3DWave2::FSubmarineHullBakeService::BakeInteriorHull(Hull, RingSequence, Settings, BakedAsset->InteriorHull))
    {
        UE_LOG(LogTemp, Error, TEXT("BakeInteriorHull failed."));
        return false;
    }

    if (!Sub3DWave2::FSubmarineHullBakeService::BakeHullCollisionProxy(Hull, RingSequence, Settings, BakedAsset->CollisionProxy))
    {
        UE_LOG(LogTemp, Error, TEXT("BakeHullCollisionProxy failed."));
        return false;
    }

    if (!Sub3DWave2::FSubmarineHullBakeService::BuildHullOwnership(RingSequence, BakedAsset->ExteriorHull, BakedAsset->HullOwnership))
    {
        UE_LOG(LogTemp, Error, TEXT("BuildHullOwnership failed."));
        return false;
    }

    BuildCompiledFrameRings(Hull, Asset->FrameRings, BakedAsset->FrameRings);

    if (!Sub3DWave3::FSubmarineOuterEnvelopeBakeService::BakeOuterEnvelope(
            Hull,
            RingSequence,
            Asset->OuterEnvelope,
            BakedAsset->OuterEnvelope,
            BakedAsset->OuterEnvelopeCollision,
            Errors))
    {
        LogErrors(TEXT("Outer Envelope Error:"), Errors);
        return false;
    }

    if (Asset->Sail.bEnabled)
    {
        Sub3DWave9::FSubmarineAppendageBakeService::BakeSail(Hull, RingSequence, Asset->Sail, BakedAsset->SailMesh);
    }
    if (Asset->BowSection.bEnabled)
    {
        Sub3DWave9::FSubmarineAppendageBakeService::BakeBowDome(Hull, RingSequence, Asset->BowSection, BakedAsset->BowMesh);
    }
    if (Asset->SternSection.bEnabled)
    {
        Sub3DWave9::FSubmarineAppendageBakeService::BakeSternFairing(Hull, RingSequence, Asset->SternSection, BakedAsset->SternMesh);
    }

    if (!Sub3DWave3::FSubmarineBaySolveService::SolveStructuralBays(
            Hull,
            Asset->FrameRings,
            Asset->StructuralBays,
            BakedAsset->StructuralBays,
            Errors))
    {
        LogErrors(TEXT("Structural Bay Error:"), Errors);
        return false;
    }

    if (!Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(
            BakedAsset->HullData,
            BakedAsset->StructuralBays,
            Asset->DeckLevels,
            BakedAsset->CompiledDecks,
            Errors))
    {
        LogErrors(TEXT("Deck Level Error:"), Errors);
        return false;
    }

    if (!Sub3DWave4::FSubmarineFloorBakeService::BakeFloorRegions(
            BakedAsset->HullData,
            BakedAsset->StructuralBays,
            BakedAsset->CompiledDecks,
            Asset->FloorRegions,
            BakedAsset->CompiledFloorRegions,
            Errors))
    {
        LogErrors(TEXT("Floor Region Error:"), Errors);
        return false;
    }

    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeOpenings(
            BakedAsset->CompiledFloorRegions,
            Asset->Openings,
            BakedAsset->CompiledOpenings,
            Errors))
    {
        LogErrors(TEXT("Opening Error:"), Errors);
        return false;
    }

    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeConnectors(
            BakedAsset->CompiledDecks,
            BakedAsset->CompiledOpenings,
            Asset->Connectors,
            BakedAsset->CompiledConnectors,
            Errors))
    {
        LogErrors(TEXT("Connector Error:"), Errors);
        return false;
    }

    if (!Sub3DWave6::FSubmarineOpeningBakeService::BakeClosures(
            BakedAsset->CompiledOpenings,
            Asset->Closures,
            BakedAsset->CompiledClosures,
            Errors))
    {
        LogErrors(TEXT("Closure Error:"), Errors);
        return false;
    }

    if (!Sub3DWave7::FSubmarinePartitionBakeService::BakePartitions(
            BakedAsset->CompiledOpenings,
            Asset->PressureBulkheads,
            Asset->InternalWalls,
            BakedAsset->CompiledPartitions,
            Errors))
    {
        LogErrors(TEXT("Partition Error:"), Errors);
        return false;
    }

    if (!Sub3DWave8::FSubmarineFloodGraphBuilder::BuildFloodGraph(
            BakedAsset->StructuralBays,
            BakedAsset->CompiledDecks,
            BakedAsset->CompiledConnectors,
            BakedAsset->CompiledClosures,
            BakedAsset->FloodGraph,
            Errors))
    {
        LogErrors(TEXT("Derived Flood Volume Error:"), Errors);
        return false;
    }

    LastBakedBaseAsset = BakedAsset;

    // Persist hull geometry and layout hashes back to the authoring asset.
    // These allow downstream editors to detect stale Layer B/C data.
    Asset->Modify();
    Asset->HullGeometryHash = static_cast<int32>(ComputeHullGeometryHash(Asset));
    Asset->LayoutHash       = static_cast<int32>(ComputeLayoutHash(Asset));

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BakeBase succeeded. Exterior Tris: %d, Frame-Rings: %d, Structural Bays: %d, Deck Levels: %d, Floor Regions: %d, Openings: %d, Partitions: %d, Derived Flood Volumes: %d"),
        BakedAsset->ExteriorHull.Indices.Num() / 3,
        BakedAsset->FrameRings.Num(),
        BakedAsset->StructuralBays.Num(),
        BakedAsset->CompiledDecks.Num(),
        BakedAsset->CompiledFloorRegions.Num(),
        BakedAsset->CompiledOpenings.Num(),
        BakedAsset->CompiledPartitions.Num(),
        BakedAsset->FloodGraph.Volumes.Num());

    return true;
}

bool USubmarineBakeSubsystem::BakeRuntime(USub3DSubmarineAuthoringAsset* Asset)
{
    if (!LastBakedBaseAsset)
    {
        if (!BakeBase(Asset))
        {
            return false;
        }
    }

    if (!LastBakedBaseAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("BakeRuntime failed: no base asset available."));
        return false;
    }

    UCompiledSubmarineRuntimeAsset* RuntimeAsset = NewObject<UCompiledSubmarineRuntimeAsset>(this);
    if (!RuntimeAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to allocate UCompiledSubmarineRuntimeAsset."));
        return false;
    }

    RuntimeAsset->HullLengthCm = LastBakedBaseAsset->HullLengthCm;
    RuntimeAsset->HullData = LastBakedBaseAsset->HullData;
    RuntimeAsset->ExteriorHull = LastBakedBaseAsset->ExteriorHull;
    RuntimeAsset->InteriorHull = LastBakedBaseAsset->InteriorHull;
    RuntimeAsset->CollisionProxy = LastBakedBaseAsset->CollisionProxy;
    RuntimeAsset->HullOwnership = LastBakedBaseAsset->HullOwnership;
    RuntimeAsset->FrameRings = LastBakedBaseAsset->FrameRings;
    RuntimeAsset->OuterEnvelope = LastBakedBaseAsset->OuterEnvelope;
    RuntimeAsset->OuterEnvelopeCollision = LastBakedBaseAsset->OuterEnvelopeCollision;
    RuntimeAsset->SailMesh = LastBakedBaseAsset->SailMesh;
    RuntimeAsset->BowMesh = LastBakedBaseAsset->BowMesh;
    RuntimeAsset->SternMesh = LastBakedBaseAsset->SternMesh;
    RuntimeAsset->StructuralBays = LastBakedBaseAsset->StructuralBays;
    RuntimeAsset->CompiledDecks = LastBakedBaseAsset->CompiledDecks;
    RuntimeAsset->CompiledFloorRegions = LastBakedBaseAsset->CompiledFloorRegions;
    RuntimeAsset->CompiledOpenings = LastBakedBaseAsset->CompiledOpenings;
    RuntimeAsset->CompiledConnectors = LastBakedBaseAsset->CompiledConnectors;
    RuntimeAsset->CompiledClosures = LastBakedBaseAsset->CompiledClosures;
    RuntimeAsset->CompiledPartitions = LastBakedBaseAsset->CompiledPartitions;
    RuntimeAsset->FloodGraph = LastBakedBaseAsset->FloodGraph;

    LastBakedRuntimeAsset = RuntimeAsset;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BakeRuntime succeeded. Closures: %d, Derived Flood Volumes: %d"),
        RuntimeAsset->CompiledClosures.Num(),
        RuntimeAsset->FloodGraph.Volumes.Num());

    return true;
}

bool USubmarineBakeSubsystem::FullBake(USub3DSubmarineAuthoringAsset* Asset)
{
    if (!BakeBase(Asset))
    {
        return false;
    }

    return BakeRuntime(Asset);
}
