#include "Bake/SubmarineFloorBakeService.h"

namespace Sub3DWave4
{
namespace
{
struct FFloorRegionContext
{
    const FCompiledBayData* Bay = nullptr;
    const FCompiledDeckData* Deck = nullptr;
};

static FSub3DCompiledHullSection InterpolateSectionAtX(
    const FSub3DCompiledHullData& HullData,
    const float X)
{
    if (HullData.Sections.IsEmpty())
    {
        return FSub3DCompiledHullSection();
    }

    if (HullData.Sections.Num() == 1 || X <= HullData.Sections[0].PositionX)
    {
        return HullData.Sections[0];
    }

    const int32 Last = HullData.Sections.Num() - 1;
    if (X >= HullData.Sections[Last].PositionX)
    {
        return HullData.Sections[Last];
    }

    for (int32 Index = 0; Index < Last; ++Index)
    {
        const FSub3DCompiledHullSection& Left = HullData.Sections[Index];
        const FSub3DCompiledHullSection& Right = HullData.Sections[Index + 1];
        if (X <= Right.PositionX)
        {
            const float Span = FMath::Max(Right.PositionX - Left.PositionX, KINDA_SMALL_NUMBER);
            const float Alpha = FMath::Clamp((X - Left.PositionX) / Span, 0.0f, 1.0f);

            FSub3DCompiledHullSection Out;
            Out.PositionX = X;
            Out.HalfWidthCm = FMath::Lerp(Left.HalfWidthCm, Right.HalfWidthCm, Alpha);
            Out.HalfHeightCm = FMath::Lerp(Left.HalfHeightCm, Right.HalfHeightCm, Alpha);
            Out.WallThicknessCm = FMath::Lerp(Left.WallThicknessCm, Right.WallThicknessCm, Alpha);
            Out.SectionRoundness = FMath::Lerp(Left.SectionRoundness, Right.SectionRoundness, Alpha);
            Out.SectionProfile = (Alpha < 0.5f) ? Left.SectionProfile : Right.SectionProfile;
            return Out;
        }
    }

    return HullData.Sections[Last];
}

static float ComputeInteriorHalfWidthAtZ(const FSub3DCompiledHullSection& Section, const float ZCm)
{
    const float InteriorHH = FMath::Max(Section.HalfHeightCm - Section.WallThicknessCm, 1.0f);
    const float InteriorHW = FMath::Max(Section.HalfWidthCm - Section.WallThicknessCm, 1.0f);

    const float AbsZ = FMath::Abs(ZCm);
    if (AbsZ >= InteriorHH)
    {
        return 0.0f;
    }

    const float ZRatio = AbsZ / InteriorHH;

    if (Section.SectionProfile == ESub3DSectionProfile::Superellipse)
    {
        const float Exponent = FMath::Lerp(1.2f, 6.0f, FMath::Clamp(Section.SectionRoundness, 0.0f, 1.0f));
        const float InvExp = 1.0f / FMath::Max(Exponent, 1.0f);
        const float Factor = FMath::Pow(FMath::Max(1.0f - FMath::Pow(ZRatio, Exponent), 0.0f), InvExp);
        return InteriorHW * Factor;
    }

    // Ellipse / Circle
    return InteriorHW * FMath::Sqrt(FMath::Max(1.0f - ZRatio * ZRatio, 0.0f));
}

static float GetUsableInteriorHeightCm(const FSub3DCompiledHullData& HullData)
{
    float MaxHH = 100.0f;
    for (const FSub3DCompiledHullSection& Section : HullData.Sections)
    {
        const float InteriorHH = Section.HalfHeightCm - Section.WallThicknessCm;
        MaxHH = FMath::Max(MaxHH, InteriorHH * 2.0f);
    }
    return MaxHH;
}

static const FCompiledBayData* FindBayById(const TArray<FCompiledBayData>& StructuralBays, const FName BayId)
{
    for (const FCompiledBayData& Bay : StructuralBays)
    {
        if (Bay.BayId == BayId)
        {
            return &Bay;
        }
    }

    return nullptr;
}

static const FCompiledDeckData* FindDeckById(const TArray<FCompiledDeckData>& Decks, const FName DeckLevelId)
{
    for (const FCompiledDeckData& Deck : Decks)
    {
        if (Deck.DeckLevelId == DeckLevelId)
        {
            return &Deck;
        }
    }

    return nullptr;
}

static bool ResolveFloorRegionContext(
    const TArray<FCompiledBayData>& StructuralBays,
    const TArray<FCompiledDeckData>& CompiledDecks,
    const FFloorRegionDef& FloorRegion,
    FFloorRegionContext& OutContext,
    TArray<FString>& OutErrors)
{
    OutContext = FFloorRegionContext();

    OutContext.Bay = FindBayById(StructuralBays, FloorRegion.StructuralBayId);
    if (!OutContext.Bay)
    {
        OutErrors.Add(FString::Printf(TEXT("Floor Region '%s' references unknown Structural Bay '%s'."), *FloorRegion.FloorRegionId.ToString(), *FloorRegion.StructuralBayId.ToString()));
        return false;
    }

    OutContext.Deck = FindDeckById(CompiledDecks, FloorRegion.DeckLevelId);
    if (!OutContext.Deck)
    {
        OutErrors.Add(FString::Printf(TEXT("Floor Region '%s' references unknown Deck Level '%s'."), *FloorRegion.FloorRegionId.ToString(), *FloorRegion.DeckLevelId.ToString()));
        return false;
    }

    if (OutContext.Deck->StructuralBayId != FloorRegion.StructuralBayId)
    {
        OutErrors.Add(FString::Printf(
            TEXT("Floor Region '%s' Deck Level '%s' does not belong to Structural Bay '%s'."),
            *FloorRegion.FloorRegionId.ToString(),
            *FloorRegion.DeckLevelId.ToString(),
            *FloorRegion.StructuralBayId.ToString()));
        return false;
    }

    return true;
}

static void BuildFloorStripMesh(
    const FSub3DCompiledHullData& HullData,
    const float StartX,
    const float EndX,
    const float ZCm,
    const FName SectionId,
    FCompiledMeshSection& OutMesh)
{
    OutMesh = FCompiledMeshSection();
    OutMesh.SectionId = SectionId;

    const float SpanX = EndX - StartX;
    if (SpanX <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const int32 XSamples = FMath::Clamp(FMath::RoundToInt(SpanX / 50.0f), 4, 64);

    OutMesh.Positions.Reserve((XSamples + 1) * 2);
    OutMesh.Normals.Reserve((XSamples + 1) * 2);
    OutMesh.UV0.Reserve((XSamples + 1) * 2);
    OutMesh.Indices.Reserve(XSamples * 6);

    const FVector3f FloorNormal(0.0f, 0.0f, 1.0f);

    for (int32 Sample = 0; Sample <= XSamples; ++Sample)
    {
        const float T = static_cast<float>(Sample) / static_cast<float>(XSamples);
        const float X = FMath::Lerp(StartX, EndX, T);

        const FSub3DCompiledHullSection Section = InterpolateSectionAtX(HullData, X);
        const float HalfWidth = ComputeInteriorHalfWidthAtZ(Section, ZCm);

        // Left vertex
        OutMesh.Positions.Add(FVector3f(X, -HalfWidth, ZCm));
        OutMesh.Normals.Add(FloorNormal);
        OutMesh.UV0.Add(FVector2f(T, 0.0f));

        // Right vertex
        OutMesh.Positions.Add(FVector3f(X, HalfWidth, ZCm));
        OutMesh.Normals.Add(FloorNormal);
        OutMesh.UV0.Add(FVector2f(T, 1.0f));
    }

    for (int32 Sample = 0; Sample < XSamples; ++Sample)
    {
        const int32 BL = Sample * 2;
        const int32 BR = Sample * 2 + 1;
        const int32 TL = (Sample + 1) * 2;
        const int32 TR = (Sample + 1) * 2 + 1;

        // Top-facing triangles (in Unreal left-handed coords: Y x X = +Z)
        OutMesh.Indices.Add(BL);
        OutMesh.Indices.Add(BR);
        OutMesh.Indices.Add(TL);

        OutMesh.Indices.Add(BR);
        OutMesh.Indices.Add(TR);
        OutMesh.Indices.Add(TL);
    }
}
} // namespace

bool FSubmarineFloorBakeService::BakeDeckLevels(
    const FSub3DCompiledHullData& HullData,
    const TArray<FCompiledBayData>& StructuralBays,
    const TArray<FDeckLevelDef>& DeckLevels,
    TArray<FCompiledDeckData>& OutCompiledDecks,
    TArray<FString>& OutErrors)
{
    OutCompiledDecks.Reset();
    OutErrors.Reset();

    const float UsableInteriorHeightCm = GetUsableInteriorHeightCm(HullData);
    const float MinDeckZ = -0.5f * UsableInteriorHeightCm;
    const float MaxDeckZ = 0.5f * UsableInteriorHeightCm;

    TMap<FName, TArray<float>> DeckZPerBay;

    for (int32 Index = 0; Index < DeckLevels.Num(); ++Index)
    {
        const FDeckLevelDef& Deck = DeckLevels[Index];
        const FName DeckLevelId = Deck.DeckLevelId.IsNone() ? FName(*FString::Printf(TEXT("DeckLevel_%d"), Index)) : Deck.DeckLevelId;

        const FCompiledBayData* Bay = FindBayById(StructuralBays, Deck.StructuralBayId);
        if (!Bay)
        {
            OutErrors.Add(FString::Printf(TEXT("Deck Level '%s' references unknown Structural Bay '%s'."), *DeckLevelId.ToString(), *Deck.StructuralBayId.ToString()));
            continue;
        }

        if (Deck.ZOffsetCm < MinDeckZ || Deck.ZOffsetCm > MaxDeckZ)
        {
            OutErrors.Add(FString::Printf(TEXT("Deck Level '%s' ZOffsetCm is outside usable hull interior range [%.0f..%.0f]."), *DeckLevelId.ToString(), MinDeckZ, MaxDeckZ));
            continue;
        }

        FCompiledDeckData& Compiled = OutCompiledDecks.AddDefaulted_GetRef();
        Compiled.DeckLevelId = DeckLevelId;
        Compiled.StructuralBayId = Bay->BayId;
        Compiled.ZOffsetCm = Deck.ZOffsetCm;

        DeckZPerBay.FindOrAdd(Bay->BayId).Add(Deck.ZOffsetCm);
    }

    for (const FCompiledBayData& Bay : StructuralBays)
    {
        const TArray<float>* ZValuesPtr = DeckZPerBay.Find(Bay.BayId);
        const int32 DeckCount = ZValuesPtr ? ZValuesPtr->Num() : 0;
        if (DeckCount > Bay.MaxDeckLevels)
        {
            OutErrors.Add(FString::Printf(
                TEXT("Structural Bay '%s' has %d Deck Levels but max allowed is %d."),
                *Bay.BayId.ToString(),
                DeckCount,
                Bay.MaxDeckLevels));
        }
    }

    for (const FCompiledBayData& Bay : StructuralBays)
    {
        TArray<int32> DeckIndices;
        for (int32 DeckIndex = 0; DeckIndex < OutCompiledDecks.Num(); ++DeckIndex)
        {
            if (OutCompiledDecks[DeckIndex].StructuralBayId == Bay.BayId)
            {
                DeckIndices.Add(DeckIndex);
            }
        }

        DeckIndices.Sort([&OutCompiledDecks](const int32 A, const int32 B)
        {
            return OutCompiledDecks[A].ZOffsetCm < OutCompiledDecks[B].ZOffsetCm;
        });

        const float MaxDeckZ_Local = 0.5f * GetUsableInteriorHeightCm(HullData);
        for (int32 LocalIndex = 0; LocalIndex < DeckIndices.Num(); ++LocalIndex)
        {
            const int32 DeckIndex = DeckIndices[LocalIndex];
            const float CurrentZ = OutCompiledDecks[DeckIndex].ZOffsetCm;
            float ClearanceAbove = MaxDeckZ_Local - CurrentZ;

            if (LocalIndex + 1 < DeckIndices.Num())
            {
                const float NextZ = OutCompiledDecks[DeckIndices[LocalIndex + 1]].ZOffsetCm;
                ClearanceAbove = NextZ - CurrentZ;
            }

            OutCompiledDecks[DeckIndex].ClearanceAboveCm = FMath::Max(ClearanceAbove, 0.0f);
        }
    }

    return OutErrors.Num() == 0;
}

bool FSubmarineFloorBakeService::ValidateFloorRegionWalkability(
    const FSub3DCompiledHullData& HullData,
    const TArray<FCompiledBayData>& StructuralBays,
    const TArray<FCompiledDeckData>& CompiledDecks,
    const TArray<FFloorRegionDef>& FloorRegions,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    for (const FFloorRegionDef& FloorRegion : FloorRegions)
    {
        FFloorRegionContext Context;
        if (!ResolveFloorRegionContext(StructuralBays, CompiledDecks, FloorRegion, Context, OutErrors))
        {
            continue;
        }

        if (FloorRegion.EndAlpha <= FloorRegion.StartAlpha)
        {
            OutErrors.Add(FString::Printf(TEXT("Floor Region '%s' must have EndAlpha > StartAlpha."), *FloorRegion.FloorRegionId.ToString()));
            continue;
        }

        if (FloorRegion.StartAlpha < Context.Bay->StartAlpha || FloorRegion.EndAlpha > Context.Bay->EndAlpha)
        {
            OutErrors.Add(FString::Printf(TEXT("Floor Region '%s' exceeds Structural Bay '%s' bounds."), *FloorRegion.FloorRegionId.ToString(), *Context.Bay->BayId.ToString()));
            continue;
        }

        if (Context.Deck->ClearanceAboveCm < 160.0f)
        {
            // Just a warning in logs, don't fail bake
            UE_LOG(LogTemp, Warning, TEXT("Floor Region '%s' has low headroom: %.1fcm."), *FloorRegion.FloorRegionId.ToString(), Context.Deck->ClearanceAboveCm);
        }

        // Check minimum walkable width at the midpoint of the floor region
        const float MidX = FMath::Lerp(FloorRegion.StartAlpha, FloorRegion.EndAlpha, 0.5f) * HullData.LengthCm;
        const FSub3DCompiledHullSection MidSection = InterpolateSectionAtX(HullData, MidX);
        const float MidWidth = ComputeInteriorHalfWidthAtZ(MidSection, Context.Deck->ZOffsetCm) * 2.0f;
        if (MidWidth < 60.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("Floor Region '%s' has narrow width: %.1fcm at midpoint."), *FloorRegion.FloorRegionId.ToString(), MidWidth);
        }
    }

    return OutErrors.Num() == 0;
}

bool FSubmarineFloorBakeService::BakeFloorRegions(
    const FSub3DCompiledHullData& HullData,
    const TArray<FCompiledBayData>& StructuralBays,
    const TArray<FCompiledDeckData>& CompiledDecks,
    const TArray<FFloorRegionDef>& FloorRegions,
    TArray<FCompiledFloorRegionData>& OutCompiledFloorRegions,
    TArray<FString>& OutErrors)
{
    OutCompiledFloorRegions.Reset();

    TArray<FString> ValidationErrors;
    if (!ValidateFloorRegionWalkability(HullData, StructuralBays, CompiledDecks, FloorRegions, ValidationErrors))
    {
        OutErrors = ValidationErrors;
        return false;
    }

    OutCompiledFloorRegions.Reserve(FloorRegions.Num());
    for (const FFloorRegionDef& FloorRegion : FloorRegions)
    {
        FFloorRegionContext Context;
        TArray<FString> ResolveErrors;
        if (!ResolveFloorRegionContext(StructuralBays, CompiledDecks, FloorRegion, Context, ResolveErrors))
        {
            OutErrors.Append(ResolveErrors);
            continue;
        }

        const float StartX = FloorRegion.StartAlpha * HullData.LengthCm;
        const float EndX = FloorRegion.EndAlpha * HullData.LengthCm;
        const float ZCm = Context.Deck->ZOffsetCm;

        // Compute walkable width at midpoint for metadata
        const float MidX = (StartX + EndX) * 0.5f;
        const FSub3DCompiledHullSection MidSection = InterpolateSectionAtX(HullData, MidX);
        const float MidHalfWidth = ComputeInteriorHalfWidthAtZ(MidSection, ZCm);

        FCompiledFloorRegionData& Compiled = OutCompiledFloorRegions.AddDefaulted_GetRef();
        Compiled.FloorRegionId = FloorRegion.FloorRegionId;
        Compiled.DeckLevelId = FloorRegion.DeckLevelId;
        Compiled.StructuralBayId = FloorRegion.StructuralBayId;
        Compiled.Kind = FloorRegion.Kind;
        Compiled.StartX = StartX;
        Compiled.EndX = EndX;
        Compiled.WalkableWidthCm = MidHalfWidth * 2.0f;
        Compiled.HeadroomCm = Context.Deck->ClearanceAboveCm;
        Compiled.bIsWalkable = (Compiled.HeadroomCm >= 160.0f) && (Compiled.WalkableWidthCm >= 60.0f);

        // Generate the floor strip mesh conforming to the hull interior
        BuildFloorStripMesh(
            HullData,
            StartX,
            EndX,
            ZCm,
            FloorRegion.FloorRegionId,
            Compiled.FloorMesh);
    }

    return OutErrors.Num() == 0;
}
} // namespace Sub3DWave4
