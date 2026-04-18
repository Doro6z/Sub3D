#include "Bake/SubmarineOuterEnvelopeBakeService.h"

#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Types/Sub3DCompiledTypes.h"

namespace Sub3DWave3
{
namespace
{
static float EvaluateHullTopZ(const FSubmarineHullDef& Hull, const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence, const float Alpha)
{
    if (RingSequence.IsEmpty())
    {
        return Hull.DefaultHalfHeightCm;
    }

    int32 BestIndex = 0;
    float BestDistance = FLT_MAX;
    for (int32 Index = 0; Index < RingSequence.Num(); ++Index)
    {
        const float Distance = FMath::Abs(RingSequence[Index].SpineAlpha - Alpha);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestIndex = Index;
        }
    }

    return FMath::Max(0.0f, RingSequence[BestIndex].RadiusCm);
}

static float EvaluateShapeScale(const ESub3DEnvelopeShape Shape, const float Alpha)
{
    switch (Shape)
    {
    case ESub3DEnvelopeShape::Box:
        return 1.0f;
    case ESub3DEnvelopeShape::Teardrop:
        return FMath::Sqrt(FMath::Clamp(1.0f - Alpha, 0.0f, 1.0f));
    case ESub3DEnvelopeShape::Faired:
    default:
        return FMath::Sin(FMath::Clamp(Alpha, 0.0f, 1.0f) * PI);
    }
}

static bool BuildEnvelopeSection(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    const FOuterEnvelopeDef& OuterEnvelope,
    const int32 SegmentCount,
    const FName SectionId,
    FCompiledMeshSection& OutSection)
{
    if (SegmentCount < 2)
    {
        return false;
    }

    const float HalfLength = OuterEnvelope.LengthCm * 0.5f;
    const float CenterX = FMath::Clamp(OuterEnvelope.BaseSpineAlpha, 0.0f, 1.0f) * Hull.LengthCm;
    const float StartX = FMath::Clamp(CenterX - HalfLength, 0.0f, Hull.LengthCm);
    const float EndX = FMath::Clamp(CenterX + HalfLength, 0.0f, Hull.LengthCm);
    const float SpanX = FMath::Max(EndX - StartX, 1.0f);

    OutSection = FCompiledMeshSection();
    OutSection.SectionId = SectionId;

    const int32 VerticesPerSection = 4;
    OutSection.Positions.Reserve((SegmentCount + 1) * VerticesPerSection);
    OutSection.Normals.Reserve((SegmentCount + 1) * VerticesPerSection);
    OutSection.UV0.Reserve((SegmentCount + 1) * VerticesPerSection);

    for (int32 SegmentIndex = 0; SegmentIndex <= SegmentCount; ++SegmentIndex)
    {
        const float SegmentAlpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
        const float X = StartX + SegmentAlpha * SpanX;
        const float HullAlpha = X / FMath::Max(Hull.LengthCm, 1.0f);
        const float BaseZ = EvaluateHullTopZ(Hull, RingSequence, HullAlpha);
        const float ShapeScale = FMath::Max(EvaluateShapeScale(OuterEnvelope.Shape, SegmentAlpha), 0.05f);
        const float HalfWidth = FMath::Max(OuterEnvelope.WidthCm * 0.5f * ShapeScale, 1.0f);
        const float Height = FMath::Max(OuterEnvelope.HeightCm * ShapeScale, 1.0f);
        const float YCenter = OuterEnvelope.LateralOffsetCm;

        const FVector3f P0(static_cast<float>(X), static_cast<float>(YCenter - HalfWidth), static_cast<float>(BaseZ));
        const FVector3f P1(static_cast<float>(X), static_cast<float>(YCenter - HalfWidth), static_cast<float>(BaseZ + Height));
        const FVector3f P2(static_cast<float>(X), static_cast<float>(YCenter + HalfWidth), static_cast<float>(BaseZ + Height));
        const FVector3f P3(static_cast<float>(X), static_cast<float>(YCenter + HalfWidth), static_cast<float>(BaseZ));

        OutSection.Positions.Add(P0);
        OutSection.Positions.Add(P1);
        OutSection.Positions.Add(P2);
        OutSection.Positions.Add(P3);

        OutSection.Normals.Add(FVector3f(0.0f, -1.0f, 0.0f));
        OutSection.Normals.Add(FVector3f(0.0f, 0.0f, 1.0f));
        OutSection.Normals.Add(FVector3f(0.0f, 1.0f, 0.0f));
        OutSection.Normals.Add(FVector3f(0.0f, 0.0f, -1.0f));

        OutSection.UV0.Add(FVector2f(SegmentAlpha, 0.0f));
        OutSection.UV0.Add(FVector2f(SegmentAlpha, 0.33f));
        OutSection.UV0.Add(FVector2f(SegmentAlpha, 0.66f));
        OutSection.UV0.Add(FVector2f(SegmentAlpha, 1.0f));
    }

    auto AddQuad = [&OutSection](const int32 A, const int32 B, const int32 C, const int32 D)
    {
        OutSection.Indices.Add(A);
        OutSection.Indices.Add(B);
        OutSection.Indices.Add(C);
        OutSection.Indices.Add(A);
        OutSection.Indices.Add(C);
        OutSection.Indices.Add(D);
    };

    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const int32 BaseA = SegmentIndex * VerticesPerSection;
        const int32 BaseB = (SegmentIndex + 1) * VerticesPerSection;

        AddQuad(BaseA + 0, BaseB + 0, BaseB + 1, BaseA + 1);
        AddQuad(BaseA + 1, BaseB + 1, BaseB + 2, BaseA + 2);
        AddQuad(BaseA + 2, BaseB + 2, BaseB + 3, BaseA + 3);
        AddQuad(BaseA + 3, BaseB + 3, BaseB + 0, BaseA + 0);
    }

    const int32 StartBase = 0;
    const int32 EndBase = SegmentCount * VerticesPerSection;
    AddQuad(StartBase + 0, StartBase + 1, StartBase + 2, StartBase + 3);
    AddQuad(EndBase + 3, EndBase + 2, EndBase + 1, EndBase + 0);

    return true;
}
} // namespace

bool FSubmarineOuterEnvelopeBakeService::BakeOuterEnvelope(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    const FOuterEnvelopeDef& OuterEnvelope,
    FCompiledMeshSection& OutEnvelopeSection,
    FCompiledMeshSection& OutCollisionSection,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    OutEnvelopeSection = FCompiledMeshSection();
    OutEnvelopeSection.SectionId = TEXT("OuterEnvelope");

    OutCollisionSection = FCompiledMeshSection();
    OutCollisionSection.SectionId = TEXT("OuterEnvelopeCollision");

    if (!OuterEnvelope.bEnabled)
    {
        return true;
    }

    if (Hull.LengthCm <= KINDA_SMALL_NUMBER)
    {
        OutErrors.Add(TEXT("Hull.LengthCm must be greater than zero for Outer Envelope bake."));
        return false;
    }

    if (OuterEnvelope.LengthCm < 100.0f)
    {
        OutErrors.Add(TEXT("Outer Envelope LengthCm must be >= 100."));
        return false;
    }

    if (OuterEnvelope.WidthCm < 50.0f || OuterEnvelope.HeightCm < 50.0f)
    {
        OutErrors.Add(TEXT("Outer Envelope WidthCm and HeightCm must be >= 50."));
        return false;
    }

    if (OuterEnvelope.bHasTrunk && OuterEnvelope.TrunkDiameterCm < 40.0f)
    {
        OutErrors.Add(TEXT("Outer Envelope trunk diameter must be >= 40 when trunk is enabled."));
        return false;
    }

    if (!BuildEnvelopeSection(Hull, RingSequence, OuterEnvelope, 12, TEXT("OuterEnvelope"), OutEnvelopeSection))
    {
        OutErrors.Add(TEXT("Failed to build Outer Envelope render section."));
        return false;
    }

    if (!BuildEnvelopeSection(Hull, RingSequence, OuterEnvelope, 4, TEXT("OuterEnvelopeCollision"), OutCollisionSection))
    {
        OutErrors.Add(TEXT("Failed to build Outer Envelope collision section."));
        return false;
    }

    return true;
}
} // namespace Sub3DWave3