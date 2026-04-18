#include "SubmarineBuilderComponent.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Generation/HullProfileEvaluator.h"
#include "Types/Sub3DHullTypes.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Types/Sub3DCanonicalEnums.h"

// ─── ControlRings ────────────────────────────────────────────────────────────

void USubmarineBuilderComponent::GenerateControlRings(USub3DSubmarineAuthoringAsset* Asset, int32 RingCount)
{
    if (!Asset) return;
    RingCount = FMath::Clamp(RingCount, 2, 64);

    const FSubmarineHullDef& Hull = Asset->Hull;

    Asset->ControlRings.Reset();
    Asset->ControlRings.Reserve(RingCount);

    for (int32 i = 0; i < RingCount; ++i)
    {
        const float T = static_cast<float>(i) / static_cast<float>(RingCount - 1);
        const float Radius = UHullProfileEvaluator::EvaluateRadius(
            Hull.ProfileParams, Hull, T);

        FControlRingDef Ring;
        Ring.ControlRingId    = FName(*FString::Printf(TEXT("Ring_%02d"), i));
        Ring.PositionX        = T * Hull.LengthCm;
        Ring.HalfWidthCm     = FMath::Max(Radius, 1.0f);
        Ring.HalfHeightCm    = FMath::Max(Radius, 1.0f);
        Ring.SectionProfile   = Hull.DefaultSectionProfile;
        Ring.SectionRoundness = Hull.DefaultSectionRoundness;
        Ring.WallThicknessCm  = Hull.DefaultWallThicknessCm;
        Asset->ControlRings.Add(Ring);
    }
}

// ─── FrameRings ──────────────────────────────────────────────────────────────

void USubmarineBuilderComponent::GenerateFrameRings(
    USub3DSubmarineAuthoringAsset* Asset,
    float NominalSpacingCm,
    int32 BayInterval)
{
    if (!Asset) return;

    const float LengthCm = Asset->Hull.LengthCm;
    const float BowEnd = LengthCm * 0.15f;
    const float SternStart = LengthCm * 0.85f;
    const float BowSpacing = NominalSpacingCm * 0.6f;
    const float SternSpacing = NominalSpacingCm * 0.6f;

    Asset->FrameRings.Reset();

    float CurrentX = 0.0f;
    int32 FrameIndex = 0;

    while (CurrentX <= LengthCm)
    {
        FFrameRingDef Frame;
        Frame.FrameRingId = FName(*FString::Printf(TEXT("Frame_%03d"), FrameIndex));
        Frame.SpineAlpha = FMath::Clamp(CurrentX / LengthCm, 0.0f, 1.0f);
        Frame.ThicknessCm = 8.0f;
        Frame.DepthCm = 15.0f;
        Frame.bIsBayBoundary = false;
        Frame.StrengthMultiplier = 1.0f;

        Asset->FrameRings.Add(Frame);

        // Advance based on zone
        float Spacing;
        if (CurrentX < BowEnd)
        {
            Spacing = BowSpacing;
        }
        else if (CurrentX > SternStart)
        {
            Spacing = SternSpacing;
        }
        else
        {
            Spacing = NominalSpacingCm;
        }

        CurrentX += FMath::Max(Spacing, 1.0f);
        ++FrameIndex;
    }

    // Force exact bay boundaries
    const TArray<float> LogicalBoundaries = { 0.0f, 1.0f };
    for (float BoundAlpha : LogicalBoundaries)
    {
        int32 ClosestIdx = 0;
        float MinDist = 1000.0f;
        for (int32 i = 0; i < Asset->FrameRings.Num(); ++i)
        {
            const float Dist = FMath::Abs(Asset->FrameRings[i].SpineAlpha - BoundAlpha);
            if (Dist < MinDist)
            {
                MinDist = Dist;
                ClosestIdx = i;
            }
        }
        if (Asset->FrameRings.IsValidIndex(ClosestIdx))
        {
            Asset->FrameRings[ClosestIdx].bIsBayBoundary = true;
        }
    }
}

// ─── StructuralBays ──────────────────────────────────────────────────────────

namespace
{
    ESub3DRoomTag DeduceRoomTag(float NormalizedMidpoint)
    {
        if (NormalizedMidpoint < 0.10f) return ESub3DRoomTag::Ballast;
        if (NormalizedMidpoint < 0.20f) return ESub3DRoomTag::Navigation;
        if (NormalizedMidpoint < 0.35f) return ESub3DRoomTag::Habitat;
        if (NormalizedMidpoint < 0.55f) return ESub3DRoomTag::Navigation; // Command center area
        if (NormalizedMidpoint < 0.70f) return ESub3DRoomTag::Machine;
        if (NormalizedMidpoint < 0.85f) return ESub3DRoomTag::Storage;
        if (NormalizedMidpoint < 0.95f) return ESub3DRoomTag::Machine;
        return ESub3DRoomTag::Ballast;
    }
}

void USubmarineBuilderComponent::GenerateStructuralBays(USub3DSubmarineAuthoringAsset* Asset)
{
    if (!Asset) return;

    // Collect bay-boundary frame rings
    TArray<int32> BoundaryIndices;
    for (int32 i = 0; i < Asset->FrameRings.Num(); ++i)
    {
        if (Asset->FrameRings[i].bIsBayBoundary)
        {
            BoundaryIndices.Add(i);
        }
    }

    Asset->StructuralBays.Reset();

    // Create a single main bay that spans the whole submarine length
    if (BoundaryIndices.Num() >= 2)
    {
        FStructuralBayDef Bay;
        Bay.BayId = FName(TEXT("Bay_Main"));
        Bay.StartAlpha = Asset->FrameRings[BoundaryIndices[0]].SpineAlpha;
        Bay.EndAlpha = Asset->FrameRings[BoundaryIndices.Last()].SpineAlpha;
        Bay.RequestedDeckLevels = 3;  // Allow up to 3 decks (Main, Upper, Lower)
        Bay.DisplayName = FText::FromString(TEXT("Main Compartment"));
        Asset->StructuralBays.Add(Bay);
    }
}

// ─── DeckLevels ──────────────────────────────────────────────────────────────

void USubmarineBuilderComponent::GenerateDeckLevels(USub3DSubmarineAuthoringAsset* Asset)
{
    if (!Asset) return;

    Asset->DeckLevels.Reset();

    const float HullLength = Asset->Hull.LengthCm;
    // Minimum headroom for a walkable deck (190cm standing + 10cm margin)
    constexpr float MinHeadroomCm = 200.0f;
    // Deck thickness
    constexpr float DeckThicknessCm = 5.0f;

    int32 DeckIndex = 0;

    for (const FStructuralBayDef& Bay : Asset->StructuralBays)
    {
        // Find hull radius at the bay midpoint
        const float MidAlpha = (Bay.StartAlpha + Bay.EndAlpha) * 0.5f;
        const float Radius = UHullProfileEvaluator::EvaluateRadius(
            Asset->Hull.ProfileParams, Asset->Hull, MidAlpha);

        if (Radius < 50.0f) continue; // Too narrow for any deck

        // Inner diameter (minus wall thickness)
        const float InnerRadius = Radius - Asset->Hull.DefaultWallThicknessCm;
        const float InnerDiameter = InnerRadius * 2.0f;

        // Primary deck: at bottom of hull + walkable offset
        // The hull center is at Z=0. Bottom of inner hull is at Z = -InnerRadius.
        // Place the main deck so crew can stand:
        // ZOffset = -InnerRadius + some floor offset so there's room below for keel/pipes
        const float KeelMarginCm = FMath::Min(InnerRadius * 0.15f, 60.0f);
        const float MainDeckZ = -InnerRadius + KeelMarginCm;

        FDeckLevelDef MainDeck;
        MainDeck.DeckLevelId = FName(*FString::Printf(TEXT("Deck_%02d_Main"), DeckIndex));
        MainDeck.StructuralBayId = Bay.BayId;
        MainDeck.ZOffsetCm = MainDeckZ;
        Asset->DeckLevels.Add(MainDeck);

        // Second deck if hull is tall enough (inner diameter > 2 * MinHeadroom + DeckThickness)
        const float SpaceAboveMainDeck = InnerRadius - MainDeckZ;
        if (SpaceAboveMainDeck > (MinHeadroomCm * 2.0f + DeckThicknessCm))
        {
            FDeckLevelDef UpperDeck;
            UpperDeck.DeckLevelId = FName(*FString::Printf(TEXT("Deck_%02d_Upper"), DeckIndex));
            UpperDeck.StructuralBayId = Bay.BayId;
            UpperDeck.ZOffsetCm = MainDeckZ + MinHeadroomCm + DeckThicknessCm;
            Asset->DeckLevels.Add(UpperDeck);
        }

        ++DeckIndex;
    }
}

// ─── FloorRegions ────────────────────────────────────────────────────────────

void USubmarineBuilderComponent::GenerateFloorRegions(USub3DSubmarineAuthoringAsset* Asset)
{
    if (!Asset) return;

    Asset->FloorRegions.Reset();

    int32 RegionIndex = 0;

    for (const FDeckLevelDef& Deck : Asset->DeckLevels)
    {
        // Find the bay this deck belongs to
        const FStructuralBayDef* OwningBay = nullptr;
        for (const FStructuralBayDef& Bay : Asset->StructuralBays)
        {
            if (Bay.BayId == Deck.StructuralBayId)
            {
                OwningBay = &Bay;
                break;
            }
        }
        if (!OwningBay) continue;

        FFloorRegionDef Region;
        Region.FloorRegionId = FName(*FString::Printf(TEXT("Floor_%03d"), RegionIndex));
        Region.DeckLevelId = Deck.DeckLevelId;
        Region.StructuralBayId = Deck.StructuralBayId;
        Region.StartAlpha = OwningBay->StartAlpha;
        Region.EndAlpha = OwningBay->EndAlpha;
        Region.Kind = ESub3DFloorRegionKind::MainBand;

        Asset->FloorRegions.Add(Region);
        ++RegionIndex;
    }
}

// ─── AutoStructure ───────────────────────────────────────────────────────────

void USubmarineBuilderComponent::AutoStructure(
    USub3DSubmarineAuthoringAsset* Asset,
    int32 RingCount,
    float FrameSpacingCm)
{
    if (!Asset) return;

    GenerateControlRings(Asset, RingCount);
    GenerateFrameRings(Asset, FrameSpacingCm);
    GenerateStructuralBays(Asset);
    GenerateDeckLevels(Asset);
    GenerateFloorRegions(Asset);
}

// ─── Mesh Generation ─────────────────────────────────────────────────────────

void USubmarineBuilderComponent::BuildHullMeshFromRings(
    const USub3DSubmarineAuthoringAsset* Asset,
    int32 RadialSegments,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector>& OutNormals,
    TArray<FVector2D>& OutUVs)
{
    OutVertices.Reset();
    OutTriangles.Reset();
    OutNormals.Reset();
    OutUVs.Reset();

    if (!Asset || Asset->ControlRings.Num() < 2) return;

    RadialSegments = FMath::Clamp(RadialSegments, 6, 64);
    const int32 NumRings = Asset->ControlRings.Num();
    const int32 VertsPerRing = RadialSegments + 1; // +1 for UV seam

    // Reserve
    OutVertices.Reserve(NumRings * VertsPerRing + 2); // +2 for end caps
    OutNormals.Reserve(NumRings * VertsPerRing + 2);
    OutUVs.Reserve(NumRings * VertsPerRing + 2);

    // Build ring vertices
    for (int32 RingIdx = 0; RingIdx < NumRings; ++RingIdx)
    {
        const FControlRingDef& Ring = Asset->ControlRings[RingIdx];
        const float U = (NumRings > 1) ? static_cast<float>(RingIdx) / static_cast<float>(NumRings - 1) : 0.0f;

        for (int32 Seg = 0; Seg <= RadialSegments; ++Seg)
        {
            const float V = static_cast<float>(Seg) / static_cast<float>(RadialSegments);
            const float AngleRad = V * 2.0f * PI;

            const float CosA = FMath::Cos(AngleRad);
            const float SinA = FMath::Sin(AngleRad);

            // Position: X = longitudinal, Y = lateral, Z = vertical
            const FVector Pos(
                Ring.PositionX,
                CosA * Ring.HalfWidthCm,
                SinA * Ring.HalfHeightCm);

            // Normal: approximate as ellipse normal
            const FVector Normal(
                0.0f,
                CosA * Ring.HalfHeightCm,
                SinA * Ring.HalfWidthCm);

            OutVertices.Add(Pos);
            OutNormals.Add(Normal.GetSafeNormal());
            OutUVs.Add(FVector2D(U, V));
        }
    }

    // Build quads between consecutive rings
    for (int32 RingIdx = 0; RingIdx + 1 < NumRings; ++RingIdx)
    {
        const int32 BaseA = RingIdx * VertsPerRing;
        const int32 BaseB = (RingIdx + 1) * VertsPerRing;

        for (int32 Seg = 0; Seg < RadialSegments; ++Seg)
        {
            const int32 A0 = BaseA + Seg;
            const int32 A1 = BaseA + Seg + 1;
            const int32 B0 = BaseB + Seg;
            const int32 B1 = BaseB + Seg + 1;

            // Two triangles per quad (winding: CCW)
            OutTriangles.Add(A0);
            OutTriangles.Add(B0);
            OutTriangles.Add(A1);

            OutTriangles.Add(A1);
            OutTriangles.Add(B0);
            OutTriangles.Add(B1);
        }
    }

    // Bow cap (fan from center point)
    {
        const FControlRingDef& FirstRing = Asset->ControlRings[0];
        const int32 CenterIdx = OutVertices.Num();
        OutVertices.Add(FVector(FirstRing.PositionX, 0.0f, 0.0f));
        OutNormals.Add(FVector(-1.0f, 0.0f, 0.0f));
        OutUVs.Add(FVector2D(0.0f, 0.5f));

        for (int32 Seg = 0; Seg < RadialSegments; ++Seg)
        {
            OutTriangles.Add(CenterIdx);
            OutTriangles.Add(Seg + 1);  // next segment
            OutTriangles.Add(Seg);      // current segment
        }
    }

    // Stern cap (fan from center point)
    {
        const FControlRingDef& LastRing = Asset->ControlRings.Last();
        const int32 CenterIdx = OutVertices.Num();
        const int32 LastRingBase = (NumRings - 1) * VertsPerRing;
        OutVertices.Add(FVector(LastRing.PositionX, 0.0f, 0.0f));
        OutNormals.Add(FVector(1.0f, 0.0f, 0.0f));
        OutUVs.Add(FVector2D(1.0f, 0.5f));

        for (int32 Seg = 0; Seg < RadialSegments; ++Seg)
        {
            OutTriangles.Add(CenterIdx);
            OutTriangles.Add(LastRingBase + Seg);
            OutTriangles.Add(LastRingBase + Seg + 1);
        }
    }
}

// ─── Presets ─────────────────────────────────────────────────────────────────

void USubmarineBuilderComponent::ApplyPreset(USub3DSubmarineAuthoringAsset* Asset, FName PresetName)
{
    if (!Asset) return;

    if (PresetName == FName("Kilo"))
    {
        Asset->Hull.LengthCm = 7200.0f;
        Asset->Hull.DefaultHalfWidthCm = 435.0f;
        Asset->Hull.DefaultHalfHeightCm = 435.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Series58;
        Asset->Hull.ProfileParams.Series58Fineness = 6.0f;
        Asset->Hull.ProfileParams.ParallelMidbodyFraction = 0.4f;
    }
    else if (PresetName == FName("Barracuda"))
    {
        Asset->Hull.LengthCm = 9900.0f;
        Asset->Hull.DefaultHalfWidthCm = 420.0f;
        Asset->Hull.DefaultHalfHeightCm = 420.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Myring;
        Asset->Hull.ProfileParams.MyringNoseExponent = 2.0f;
        Asset->Hull.ProfileParams.MyringNoseFraction = 0.18f;
        Asset->Hull.ProfileParams.MyringTailFraction = 0.22f;
        Asset->Hull.ProfileParams.MyringTailAngleDeg = 22.0f;
        Asset->Hull.ProfileParams.ParallelMidbodyFraction = 0.35f;
    }
    else if (PresetName == FName("Typhoon"))
    {
        Asset->Hull.LengthCm = 17500.0f;
        Asset->Hull.DefaultHalfWidthCm = 1180.0f;
        Asset->Hull.DefaultHalfHeightCm = 1180.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::SuperellipseLongitudinal;
        Asset->Hull.ProfileParams.LongitudinalExponent = 2.8f;
        Asset->Hull.ProfileParams.ParallelMidbodyFraction = 0.45f;
    }
    else if (PresetName == FName("CompactAIP"))
    {
        Asset->Hull.LengthCm = 6200.0f;
        Asset->Hull.DefaultHalfWidthCm = 320.0f;
        Asset->Hull.DefaultHalfHeightCm = 320.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Myring;
        Asset->Hull.ProfileParams.MyringNoseExponent = 2.5f;
        Asset->Hull.ProfileParams.MyringNoseFraction = 0.22f;
        Asset->Hull.ProfileParams.MyringTailFraction = 0.28f;
        Asset->Hull.ProfileParams.MyringTailAngleDeg = 28.0f;
        Asset->Hull.ProfileParams.ParallelMidbodyFraction = 0.30f;
    }

    // Auto-generate rings after preset
    GenerateControlRings(Asset, 12);
}
