#include "Bake/SubmarinePartitionBakeService.h"

namespace Sub3DWave7
{
namespace
{
static bool HasOpening(const TArray<FCompiledOpeningData>& Openings, const FName OpeningId)
{
    for (const FCompiledOpeningData& Opening : Openings)
    {
        if (Opening.OpeningId == OpeningId)
        {
            return true;
        }
    }

    return false;
}

static FCompiledMeshSection BuildSimpleQuadSection(const FName SectionId, const FVector2D SizeCm)
{
    FCompiledMeshSection Mesh;
    Mesh.SectionId = SectionId;

    const float HalfW = FMath::Max(SizeCm.X * 0.5f, 1.0f);
    const float HalfH = FMath::Max(SizeCm.Y * 0.5f, 1.0f);

    Mesh.Positions = {
        FVector3f(0.0f, -HalfW, -HalfH),
        FVector3f(0.0f, -HalfW, HalfH),
        FVector3f(0.0f, HalfW, HalfH),
        FVector3f(0.0f, HalfW, -HalfH)
    };

    Mesh.Normals = {
        FVector3f(1.0f, 0.0f, 0.0f),
        FVector3f(1.0f, 0.0f, 0.0f),
        FVector3f(1.0f, 0.0f, 0.0f),
        FVector3f(1.0f, 0.0f, 0.0f)
    };

    Mesh.UV0 = {
        FVector2f(0.0f, 0.0f),
        FVector2f(0.0f, 1.0f),
        FVector2f(1.0f, 1.0f),
        FVector2f(1.0f, 0.0f)
    };

    Mesh.Indices = {0, 1, 2, 0, 2, 3};
    return Mesh;
}
} // namespace

bool FSubmarinePartitionBakeService::BakePartitions(
    const TArray<FCompiledOpeningData>& CompiledOpenings,
    const TArray<FPressureBulkheadDef>& PressureBulkheads,
    const TArray<FInternalWallDef>& InternalWalls,
    TArray<FCompiledPartitionData>& OutCompiledPartitions,
    TArray<FString>& OutErrors)
{
    OutCompiledPartitions.Reset();
    OutErrors.Reset();

    for (const FPressureBulkheadDef& Bulkhead : PressureBulkheads)
    {
        bool bValid = true;
        for (const FName OpeningId : Bulkhead.OpeningIds)
        {
            if (!HasOpening(CompiledOpenings, OpeningId))
            {
                OutErrors.Add(FString::Printf(TEXT("Pressure Bulkhead '%s' references missing Opening '%s'."), *Bulkhead.BulkheadId.ToString(), *OpeningId.ToString()));
                bValid = false;
            }
        }

        if (!bValid)
        {
            continue;
        }

        FCompiledPartitionData& Compiled = OutCompiledPartitions.AddDefaulted_GetRef();
        Compiled.PartitionId = Bulkhead.BulkheadId;
        Compiled.Type = ESub3DPartitionType::PressureBulkhead;
        Compiled.StrengthMultiplier = FMath::Max(Bulkhead.StrengthMultiplier, 0.1f);
        Compiled.PartitionMesh = BuildSimpleQuadSection(Bulkhead.BulkheadId, FVector2D(600.0f, 500.0f));
    }

    for (const FInternalWallDef& Wall : InternalWalls)
    {
        bool bValid = true;
        for (const FName OpeningId : Wall.OpeningIds)
        {
            if (!HasOpening(CompiledOpenings, OpeningId))
            {
                OutErrors.Add(FString::Printf(TEXT("Internal Wall '%s' references missing Opening '%s'."), *Wall.WallId.ToString(), *OpeningId.ToString()));
                bValid = false;
            }
        }

        if (!bValid)
        {
            continue;
        }

        FCompiledPartitionData& Compiled = OutCompiledPartitions.AddDefaulted_GetRef();
        Compiled.PartitionId = Wall.WallId;
        Compiled.Type = ESub3DPartitionType::InternalWall;
        Compiled.StrengthMultiplier = 1.0f;
        Compiled.PartitionMesh = BuildSimpleQuadSection(Wall.WallId, Wall.SizeCm);
    }

    return OutErrors.Num() == 0;
}
} // namespace Sub3DWave7