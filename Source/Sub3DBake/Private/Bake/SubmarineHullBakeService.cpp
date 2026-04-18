#include "Bake/SubmarineHullBakeService.h"

namespace Sub3DWave2
{
namespace
{
struct FBakeMeshParams
{
    int32 RadialSegments = 0;
    bool bInterior = false;
    FName SectionId = NAME_None;
};

static FVector2f MakeUV(const float Alpha, const float CircumferenceAlpha)
{
    return FVector2f(CircumferenceAlpha, Alpha);
}

static float ToExponent(const float Roundness)
{
    return FMath::Lerp(1.2f, 6.0f, FMath::Clamp(Roundness, 0.0f, 1.0f));
}

static FVector2f EvaluateSectionXY(const FGeneratedRingData& Ring, const float Angle, const float InwardOffsetCm = 0.0f)
{
    const float RawHalfHeight = FMath::Max(Ring.RadiusCm, KINDA_SMALL_NUMBER);
    const float RawHalfWidth = RawHalfHeight * FMath::Max(Ring.WidthToHeightRatio, KINDA_SMALL_NUMBER);
    const float HalfHeight = FMath::Max(RawHalfHeight - InwardOffsetCm, KINDA_SMALL_NUMBER);
    const float HalfWidth = FMath::Max(RawHalfWidth - InwardOffsetCm, KINDA_SMALL_NUMBER);

    if (Ring.Profile == ESub3DSectionProfile::Circle)
    {
        const float Radius = FMath::Min(HalfWidth, HalfHeight);
        return FVector2f(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle));
    }

    if (Ring.Profile == ESub3DSectionProfile::Superellipse)
    {
        const float Exponent = ToExponent(Ring.Roundness);
        const float CosValue = FMath::Cos(Angle);
        const float SinValue = FMath::Sin(Angle);
        const float X = HalfWidth * FMath::Sign(CosValue) * FMath::Pow(FMath::Abs(CosValue), 2.0f / Exponent);
        const float Y = HalfHeight * FMath::Sign(SinValue) * FMath::Pow(FMath::Abs(SinValue), 2.0f / Exponent);
        return FVector2f(X, Y);
    }

    return FVector2f(HalfWidth * FMath::Cos(Angle), HalfHeight * FMath::Sin(Angle));
}

static bool BakeHullSection(
    const FSubmarineHullDef& Hull,
    const TArray<FGeneratedRingData>& RingSequence,
    const FBakeMeshParams& Params,
    FCompiledMeshSection& OutSection)
{
    if (RingSequence.Num() < 2)
    {
        return false;
    }

    if (Params.RadialSegments < 3)
    {
        return false;
    }

    OutSection = FCompiledMeshSection();
    OutSection.SectionId = Params.SectionId;

    const int32 RingCount = RingSequence.Num();
    const int32 VerticesPerRing = Params.RadialSegments;
    const int32 SideVertexCount = RingCount * VerticesPerRing;
    OutSection.Positions.Reserve(SideVertexCount);
    OutSection.Normals.Reserve(SideVertexCount);
    OutSection.UV0.Reserve(SideVertexCount);

    for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        const FGeneratedRingData& Ring = RingSequence[RingIndex];
        const float X = FMath::Clamp(Ring.SpineAlpha, 0.0f, 1.0f) * Hull.LengthCm;
        const float U = (RingCount > 1) ? static_cast<float>(RingIndex) / static_cast<float>(RingCount - 1) : 0.0f;

        for (int32 SegmentIndex = 0; SegmentIndex < VerticesPerRing; ++SegmentIndex)
        {
            const float V = static_cast<float>(SegmentIndex) / static_cast<float>(VerticesPerRing);
            const float Angle = V * UE_TWO_PI;
            const float WallOffset = Params.bInterior ? Ring.WallThicknessCm : 0.0f;
            const FVector2f SectionXY = EvaluateSectionXY(Ring, Angle, WallOffset);
            const FVector3f Position(static_cast<float>(X), SectionXY.X, SectionXY.Y);
            FVector3f Normal(0.0f, SectionXY.X, SectionXY.Y);
            if (!Normal.Normalize())
            {
                Normal = FVector3f(0.0f, 1.0f, 0.0f);
            }

            if (Params.bInterior)
            {
                Normal *= -1.0f;
            }

            OutSection.Positions.Add(Position);
            OutSection.Normals.Add(Normal);
            OutSection.UV0.Add(MakeUV(U, V));
        }
    }

    const int32 SideTriangleCount = (RingCount - 1) * VerticesPerRing * 2;
    const int32 CapTriangleCount = (VerticesPerRing - 2) * 2;
    OutSection.Indices.Reserve((SideTriangleCount + CapTriangleCount) * 3);

    for (int32 RingIndex = 0; RingIndex < RingCount - 1; ++RingIndex)
    {
        const int32 BaseA = RingIndex * VerticesPerRing;
        const int32 BaseB = (RingIndex + 1) * VerticesPerRing;
        for (int32 SegmentIndex = 0; SegmentIndex < VerticesPerRing; ++SegmentIndex)
        {
            const int32 NextSegment = (SegmentIndex + 1) % VerticesPerRing;
            const int32 A = BaseA + SegmentIndex;
            const int32 B = BaseA + NextSegment;
            const int32 C = BaseB + SegmentIndex;
            const int32 D = BaseB + NextSegment;

            if (Params.bInterior)
            {
                OutSection.Indices.Add(A);
                OutSection.Indices.Add(B);
                OutSection.Indices.Add(C);

                OutSection.Indices.Add(B);
                OutSection.Indices.Add(D);
                OutSection.Indices.Add(C);
            }
            else
            {
                OutSection.Indices.Add(A);
                OutSection.Indices.Add(C);
                OutSection.Indices.Add(B);

                OutSection.Indices.Add(B);
                OutSection.Indices.Add(C);
                OutSection.Indices.Add(D);
            }
        }
    }

    const int32 FrontBase = 0;
    const int32 BackBase = (RingCount - 1) * VerticesPerRing;
    for (int32 SegmentIndex = 1; SegmentIndex < VerticesPerRing - 1; ++SegmentIndex)
    {
        if (Params.bInterior)
        {
            OutSection.Indices.Add(FrontBase + 0);
            OutSection.Indices.Add(FrontBase + SegmentIndex + 1);
            OutSection.Indices.Add(FrontBase + SegmentIndex);

            OutSection.Indices.Add(BackBase + 0);
            OutSection.Indices.Add(BackBase + SegmentIndex);
            OutSection.Indices.Add(BackBase + SegmentIndex + 1);
        }
        else
        {
            OutSection.Indices.Add(FrontBase + 0);
            OutSection.Indices.Add(FrontBase + SegmentIndex);
            OutSection.Indices.Add(FrontBase + SegmentIndex + 1);

            OutSection.Indices.Add(BackBase + 0);
            OutSection.Indices.Add(BackBase + SegmentIndex + 1);
            OutSection.Indices.Add(BackBase + SegmentIndex);
        }
    }

    return true;
}
} // namespace

bool FSubmarineHullBakeService::BakeExteriorHull(
    const FSubmarineHullDef& Hull,
    const TArray<FGeneratedRingData>& RingSequence,
    const FHullBakeSettings& Settings,
    FCompiledMeshSection& OutSection)
{
    FBakeMeshParams Params;
    Params.RadialSegments = FMath::Max(Settings.RadialSegments, 3);
    Params.bInterior = false;
    Params.SectionId = TEXT("ExteriorHull");
    return BakeHullSection(Hull, RingSequence, Params, OutSection);
}

bool FSubmarineHullBakeService::BakeInteriorHull(
    const FSubmarineHullDef& Hull,
    const TArray<FGeneratedRingData>& RingSequence,
    const FHullBakeSettings& Settings,
    FCompiledMeshSection& OutSection)
{
    FBakeMeshParams Params;
    Params.RadialSegments = FMath::Max(Settings.RadialSegments, 3);
    Params.bInterior = true;
    Params.SectionId = TEXT("InteriorHull");
    return BakeHullSection(Hull, RingSequence, Params, OutSection);
}

bool FSubmarineHullBakeService::BakeHullCollisionProxy(
    const FSubmarineHullDef& Hull,
    const TArray<FGeneratedRingData>& RingSequence,
    const FHullBakeSettings& Settings,
    FCompiledMeshSection& OutCollision)
{
    if (!Settings.bBakeCollision)
    {
        OutCollision = FCompiledMeshSection();
        OutCollision.SectionId = TEXT("CollisionProxy");
        return true;
    }

    FBakeMeshParams Params;
    Params.RadialSegments = FMath::Clamp(Settings.CollisionRadialSegments, 3, FMath::Max(Settings.RadialSegments, 3));
    Params.bInterior = false;
    Params.SectionId = TEXT("CollisionProxy");
    return BakeHullSection(Hull, RingSequence, Params, OutCollision);
}

bool FSubmarineHullBakeService::BuildHullOwnership(
    const TArray<FGeneratedRingData>& RingSequence,
    const FCompiledMeshSection& HullSection,
    FCompiledHullOwnership& OutOwnership)
{
    OutOwnership = FCompiledHullOwnership();

    if (RingSequence.Num() < 2)
    {
        return false;
    }

    const int32 TriangleCount = HullSection.Indices.Num() / 3;
    if (TriangleCount <= 0)
    {
        return false;
    }

    const int32 VerticesPerRing = HullSection.Positions.Num() / RingSequence.Num();
    if (VerticesPerRing < 3)
    {
        return false;
    }

    const int32 SideTriangles = (RingSequence.Num() - 1) * VerticesPerRing * 2;
    OutOwnership.TriangleOwnerIds.Reserve(TriangleCount);

    for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
    {
        int32 OwnerRingIndex = 0;
        if (TriangleIndex < SideTriangles)
        {
            const int32 SegmentPair = TriangleIndex / 2;
            OwnerRingIndex = FMath::Clamp(SegmentPair / VerticesPerRing, 0, RingSequence.Num() - 1);
        }
        else
        {
            const int32 CapTriangle = TriangleIndex - SideTriangles;
            const int32 CapThreshold = FMath::Max(VerticesPerRing - 2, 0);
            OwnerRingIndex = (CapTriangle < CapThreshold) ? 0 : (RingSequence.Num() - 1);
        }

        const FGeneratedRingData& OwnerRing = RingSequence[OwnerRingIndex];
        FName OwnerId = NAME_None;
        if (OwnerRing.bIsControlRing && OwnerRing.SourceControlRingIndex != INDEX_NONE)
        {
            OwnerId = FName(*FString::Printf(TEXT("ControlRing_%d"), OwnerRing.SourceControlRingIndex));
        }
        else
        {
            OwnerId = FName(*FString::Printf(TEXT("GeneratedRing_%d"), OwnerRingIndex));
        }

        OutOwnership.TriangleOwnerIds.Add(OwnerId);
    }

    return OutOwnership.TriangleOwnerIds.Num() == TriangleCount;
}
} // namespace Sub3DWave2
