#include "RoomWaterDebugDrawer.h"

#include "Sub3DWaterProto.h"
#include "RoomWaterBakedData.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void URoomWaterDebugDrawer::DrawCompartmentSnapshot(
    UObject* WorldContextObject,
    UBoxComponent* Volume,
    URoomWaterBakedData* BakedData,
    float CurrentWaterLevelLocalZ,
    float Duration,
    bool bIncludeMaskMisses)
{
    if (!WorldContextObject || !Volume || !BakedData)
    {
        UE_LOG(LogWaterProto, Warning,
            TEXT("DrawCompartmentSnapshot: paramètre null (Volume=%p, BakedData=%p)"),
            Volume, BakedData);
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    UE_LOG(LogWaterProto, Display, TEXT("=== Snapshot %s @ Z=%.1f ==="),
        *BakedData->SourceRoomId.ToString(), CurrentWaterLevelLocalZ);
    UE_LOG(LogWaterProto, Display, TEXT("  Bounds: Min=%s Max=%s"),
        *BakedData->LocalBoundsMin.ToString(), *BakedData->LocalBoundsMax.ToString());
    UE_LOG(LogWaterProto, Display, TEXT("  Slices: %d  Openings: %d segments"),
        BakedData->Slices.Num(), BakedData->OpeningSegmentStarts.Num());

    // 1) Bornes Box (jaune)
    DrawBoxBounds(WorldContextObject, Volume, Duration);

    // 2) Plan d'eau actuel (semi-transparent bleu translucide)
    {
        const FTransform Xform = Volume->GetComponentTransform();
        const FVector& Min = BakedData->LocalBoundsMin;
        const FVector& Max = BakedData->LocalBoundsMax;
        const FVector Center = Xform.TransformPosition(FVector(
            (Min.X + Max.X) * 0.5f,
            (Min.Y + Max.Y) * 0.5f,
            CurrentWaterLevelLocalZ));
        const FVector Extent(
            FMath::Abs(Max.X - Min.X) * 0.5f,
            FMath::Abs(Max.Y - Min.Y) * 0.5f,
            1.0f);
        DrawDebugBox(World, Center, Extent, Xform.GetRotation(),
            FColor(0, 100, 255, 80), false, Duration, 0, 1.0f);
        DrawDebugString(World, Center + FVector(0, 0, 30),
            FString::Printf(TEXT("Water Z=%.1f"), CurrentWaterLevelLocalZ),
            nullptr, FColor::White, Duration, true);
    }

    // 3) Toutes les slices
    for (int32 i = 0; i < BakedData->Slices.Num(); ++i)
    {
        DrawSliceDetailed(WorldContextObject, Volume, BakedData, i, Duration, bIncludeMaskMisses);
    }
}

void URoomWaterDebugDrawer::DrawBoxBounds(
    UObject* WorldContextObject,
    UBoxComponent* Volume,
    float Duration)
{
    if (!WorldContextObject || !Volume) { return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    const FTransform Xform = Volume->GetComponentTransform();
    const FVector Extent = Volume->GetUnscaledBoxExtent();
    const FVector Center = Xform.GetLocation();

    DrawDebugBox(World, Center, Extent, Xform.GetRotation(),
        FColor::Yellow, false, Duration, 0, 3.0f);

    DrawDebugString(World, Center + FVector(0, 0, Extent.Z + 50),
        TEXT("CompartmentVolume"),
        nullptr, FColor::Yellow, Duration, true);
}

void URoomWaterDebugDrawer::DrawSliceDetailed(
    UObject* WorldContextObject,
    UBoxComponent* Volume,
    URoomWaterBakedData* BakedData,
    int32 SliceIndex,
    float Duration,
    bool bIncludeMaskMisses)
{
    if (!WorldContextObject || !Volume || !BakedData) { return; }
    if (!BakedData->Slices.IsValidIndex(SliceIndex)) { return; }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    const FCompartmentSlice& Slice = BakedData->Slices[SliceIndex];
    const FTransform Xform = Volume->GetComponentTransform();
    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;
    const float SpanX = Max.X - Min.X;

    const int32 W = Slice.GridWidth;
    const int32 H = Slice.GridHeight;
    if (W <= 0 || H <= 0) { return; }

    const float CellSize = SpanX / W;

    int32 InsideCount = 0;
    int32 OutsideCount = 0;

    // SDF cellule par cellule. Gradient vert (inside, négatif) / rouge (outside, positif),
    // avec luminosité = proximité de la surface (clair = près du mur, sombre = loin).
    constexpr float SDF_VizRangeCm = 200.0f; // saturation à ±2m

    for (int32 y = 0; y < H; ++y)
    {
        for (int32 x = 0; x < W; ++x)
        {
            const float Dist = Slice.SignedDistance[y * W + x];
            const bool bInside = (Dist < 0.f);
            if (bInside) { ++InsideCount; } else { ++OutsideCount; }

            if (!bInside && !bIncludeMaskMisses) { continue; }

            const FVector LocalPos(
                Min.X + (x + 0.5f) * CellSize,
                Min.Y + (y + 0.5f) * CellSize,
                Slice.SliceZ_Local);
            const FVector WorldPos = Xform.TransformPosition(LocalPos);

            FColor C;
            if (bInside)
            {
                // Vert : 255 à la surface, 100 saturé (loin de tout mur)
                const float t = FMath::Clamp(-Dist / SDF_VizRangeCm, 0.f, 1.f);
                const uint8 G = static_cast<uint8>(100 + 155 * (1.0f - t));
                const uint8 B = static_cast<uint8>(50 * t);
                C = FColor(0, G, B);
            }
            else
            {
                // Rouge : 255 à la surface, 100 saturé (loin de tout mur)
                const float t = FMath::Clamp(Dist / SDF_VizRangeCm, 0.f, 1.f);
                const uint8 R = static_cast<uint8>(100 + 155 * (1.0f - t));
                const uint8 B = static_cast<uint8>(50 * t);
                C = FColor(R, 0, B);
            }
            DrawDebugSphere(World, WorldPos, CellSize * 0.15f, 4, C, false, Duration, 0, 0.5f);
        }
    }

    // Contour Marching Squares (bleu)
    for (int32 i = 0; i + 1 < Slice.ContourPolygon.Num(); i += 2)
    {
        const FVector A_local(Slice.ContourPolygon[i].X, Slice.ContourPolygon[i].Y, Slice.SliceZ_Local);
        const FVector B_local(Slice.ContourPolygon[i + 1].X, Slice.ContourPolygon[i + 1].Y, Slice.SliceZ_Local);
        const FVector A = Xform.TransformPosition(A_local);
        const FVector B = Xform.TransformPosition(B_local);
        DrawDebugLine(World, A, B, FColor(0, 100, 255), false, Duration, 0, 2.0f);
    }

    // Label de la slice
    const FVector LabelLoc = Xform.TransformPosition(FVector(Min.X - 30, Min.Y - 30, Slice.SliceZ_Local));
    DrawDebugString(World, LabelLoc,
        FString::Printf(TEXT("S%d Z=%.0f In=%d/%d"),
            SliceIndex, Slice.SliceZ_Local, InsideCount, InsideCount + OutsideCount),
        nullptr, FColor::White, Duration, true);

    UE_LOG(LogWaterProto, Display,
        TEXT("  Slice %2d: Z=%6.1f  Inside=%d/%d (%.0f%%)  Contour=%d pts"),
        SliceIndex, Slice.SliceZ_Local, InsideCount, InsideCount + OutsideCount,
        100.f * InsideCount / FMath::Max(1, InsideCount + OutsideCount),
        Slice.ContourPolygon.Num());

    // Segments openings (orange épais) — uniquement sur la slice médiane (= source de DetectOpenings)
    if (SliceIndex == BakedData->Slices.Num() / 2)
    {
        const int32 NumOpen = BakedData->OpeningSegmentStarts.Num();
        for (int32 i = 0; i < NumOpen; ++i)
        {
            const FVector A_local(
                BakedData->OpeningSegmentStarts[i].X,
                BakedData->OpeningSegmentStarts[i].Y,
                Slice.SliceZ_Local);
            const FVector B_local(
                BakedData->OpeningSegmentEnds[i].X,
                BakedData->OpeningSegmentEnds[i].Y,
                Slice.SliceZ_Local);
            const FVector A = Xform.TransformPosition(A_local);
            const FVector B = Xform.TransformPosition(B_local);
            DrawDebugLine(World, A, B, FColor(255, 140, 0), false, Duration, 0, 5.0f);
            DrawDebugSphere(World, (A + B) * 0.5f, 8.0f, 8, FColor(255, 140, 0),
                false, Duration, 0, 1.0f);
        }
    }
}

void URoomWaterDebugDrawer::DrawSliceSDFGradient(
    UObject* WorldContextObject,
    UBoxComponent* Volume,
    URoomWaterBakedData* BakedData,
    int32 SliceIndex,
    float MaxDistance,
    float Duration,
    bool bShowValues)
{
    if (!WorldContextObject || !Volume || !BakedData) { return; }
    if (!BakedData->Slices.IsValidIndex(SliceIndex)) { return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    const FCompartmentSlice& Slice = BakedData->Slices[SliceIndex];
    const FTransform Xform = Volume->GetComponentTransform();
    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;
    const float SpanX = Max.X - Min.X;
    const int32 W = Slice.GridWidth;
    const int32 H = Slice.GridHeight;
    if (W <= 0 || H <= 0) { return; }
    const float CellSize = SpanX / W;

    if (Slice.SignedDistance.Num() != W * H)
    {
        UE_LOG(LogWaterProto, Warning,
            TEXT("DrawSliceSDFGradient: SDF size mismatch (got %d, expected %d)"),
            Slice.SignedDistance.Num(), W * H);
        return;
    }

    const float ClampedMax = FMath::Max(MaxDistance, 1.0f);

    float SDFMin = TNumericLimits<float>::Max();
    float SDFMax = -TNumericLimits<float>::Max();
    for (int32 y = 0; y < H; ++y)
    {
        for (int32 x = 0; x < W; ++x)
        {
            const int32 idx = y * W + x;
            const float SDF = Slice.SignedDistance[idx];
            SDFMin = FMath::Min(SDFMin, SDF);
            SDFMax = FMath::Max(SDFMax, SDF);

            const float Magnitude = FMath::Clamp(FMath::Abs(SDF) / ClampedMax, 0.f, 1.f);

            // Gradient : sombre proche de la surface (50), vif loin (255). Vert si inside, rouge si outside.
            const uint8 Intensity = static_cast<uint8>(50 + 205 * Magnitude);
            const FColor C = (SDF < 0.f) ? FColor(0, Intensity, 0) : FColor(Intensity, 0, 0);

            const FVector LocalPos(
                Min.X + (x + 0.5f) * CellSize,
                Min.Y + (y + 0.5f) * CellSize,
                Slice.SliceZ_Local);
            const FVector WorldPos = Xform.TransformPosition(LocalPos);

            DrawDebugSphere(World, WorldPos, CellSize * 0.18f, 4, C, false, Duration, 0, 0.5f);

            if (bShowValues)
            {
                DrawDebugString(World, WorldPos + FVector(0, 0, 5),
                    FString::Printf(TEXT("%.1f"), SDF),
                    nullptr, FColor::White, Duration, true);
            }
        }
    }

    // Contour bleu (toujours).
    for (int32 i = 0; i + 1 < Slice.ContourPolygon.Num(); i += 2)
    {
        const FVector A_local(Slice.ContourPolygon[i].X, Slice.ContourPolygon[i].Y, Slice.SliceZ_Local);
        const FVector B_local(Slice.ContourPolygon[i + 1].X, Slice.ContourPolygon[i + 1].Y, Slice.SliceZ_Local);
        const FVector A = Xform.TransformPosition(A_local);
        const FVector B = Xform.TransformPosition(B_local);
        DrawDebugLine(World, A, B, FColor(0, 100, 255), false, Duration, 0, 2.0f);
    }

    // Label de la slice avec stats SDF.
    const FVector LabelLoc = Xform.TransformPosition(FVector(Min.X - 30, Min.Y - 30, Slice.SliceZ_Local));
    DrawDebugString(World, LabelLoc,
        FString::Printf(TEXT("S%d Z=%.0f SDF[%.1f..%.1f]"),
            SliceIndex, Slice.SliceZ_Local, SDFMin, SDFMax),
        nullptr, FColor::White, Duration, true);
}

void URoomWaterDebugDrawer::DrawSliceContourDetailed(
    UObject* WorldContextObject,
    UBoxComponent* Volume,
    URoomWaterBakedData* BakedData,
    int32 SliceIndex,
    float Duration,
    bool bShowTValues)
{
    if (!WorldContextObject || !Volume || !BakedData) { return; }
    if (!BakedData->Slices.IsValidIndex(SliceIndex)) { return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    const FCompartmentSlice& Slice = BakedData->Slices[SliceIndex];
    const FTransform Xform = Volume->GetComponentTransform();

    // Sphères jaunes sur chaque point du contour.
    for (int32 i = 0; i < Slice.ContourPolygon.Num(); ++i)
    {
        const FVector P_local(Slice.ContourPolygon[i].X, Slice.ContourPolygon[i].Y, Slice.SliceZ_Local);
        const FVector P = Xform.TransformPosition(P_local);
        DrawDebugSphere(World, P, 3.0f, 8, FColor::Yellow, false, Duration, 0, 1.0f);

        if (bShowTValues)
        {
            DrawDebugString(World, P + FVector(0, 0, 8),
                FString::Printf(TEXT("[%d] (%.1f, %.1f)"),
                    i, Slice.ContourPolygon[i].X, Slice.ContourPolygon[i].Y),
                nullptr, FColor::Yellow, Duration, true);
        }
    }

    // Lignes bleues épaisses entre chaque paire (segments MS).
    for (int32 i = 0; i + 1 < Slice.ContourPolygon.Num(); i += 2)
    {
        const FVector A_local(Slice.ContourPolygon[i].X, Slice.ContourPolygon[i].Y, Slice.SliceZ_Local);
        const FVector B_local(Slice.ContourPolygon[i + 1].X, Slice.ContourPolygon[i + 1].Y, Slice.SliceZ_Local);
        const FVector A = Xform.TransformPosition(A_local);
        const FVector B = Xform.TransformPosition(B_local);
        DrawDebugLine(World, A, B, FColor(0, 100, 255), false, Duration, 0, 3.0f);
    }
}

void URoomWaterDebugDrawer::MarkInjection(
    UObject* WorldContextObject,
    FVector WorldLocation,
    float Force,
    float Radius)
{
    if (!WorldContextObject) { return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) { return; }

    DrawDebugSphere(World, WorldLocation, Radius, 12, FColor::Cyan, false, 0.5f, 0, 2.0f);
    UE_LOG(LogWaterProto, Verbose, TEXT("Inject @ %s (force=%.2f, radius=%.1f)"),
        *WorldLocation.ToString(), Force, Radius);
}

void URoomWaterDebugDrawer::DumpBakedDataToLog(URoomWaterBakedData* BakedData)
{
    if (!BakedData)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("DumpBakedDataToLog: BakedData null"));
        return;
    }

    UE_LOG(LogWaterProto, Display, TEXT("====================================="));
    UE_LOG(LogWaterProto, Display, TEXT("BakedData dump: %s"), *BakedData->SourceRoomId.ToString());
    UE_LOG(LogWaterProto, Display, TEXT("====================================="));
    UE_LOG(LogWaterProto, Display, TEXT("LocalBoundsMin = %s"), *BakedData->LocalBoundsMin.ToString());
    UE_LOG(LogWaterProto, Display, TEXT("LocalBoundsMax = %s"), *BakedData->LocalBoundsMax.ToString());
    UE_LOG(LogWaterProto, Display, TEXT("Span = (%.1f, %.1f, %.1f)"),
        BakedData->LocalBoundsMax.X - BakedData->LocalBoundsMin.X,
        BakedData->LocalBoundsMax.Y - BakedData->LocalBoundsMin.Y,
        BakedData->LocalBoundsMax.Z - BakedData->LocalBoundsMin.Z);
    UE_LOG(LogWaterProto, Display, TEXT("Slices: %d"), BakedData->Slices.Num());

    for (int32 i = 0; i < BakedData->Slices.Num(); ++i)
    {
        const FCompartmentSlice& S = BakedData->Slices[i];
        int32 InsideCount = 0;
        float MinSDF = TNumericLimits<float>::Max();
        float MaxSDF = -TNumericLimits<float>::Max();
        for (float v : S.SignedDistance)
        {
            if (v < 0.f) { ++InsideCount; }
            MinSDF = FMath::Min(MinSDF, v);
            MaxSDF = FMath::Max(MaxSDF, v);
        }

        UE_LOG(LogWaterProto, Display,
            TEXT("  Slice %2d: Z=%7.2f  Grid=%dx%d  Inside=%d/%d (%.1f%%)  SDF=[%.1f, %.1f]  Contour=%d pts"),
            i, S.SliceZ_Local, S.GridWidth, S.GridHeight,
            InsideCount, S.SignedDistance.Num(),
            100.f * InsideCount / FMath::Max(1, S.SignedDistance.Num()),
            MinSDF, MaxSDF,
            S.ContourPolygon.Num());
    }

    UE_LOG(LogWaterProto, Display, TEXT("CapMeshesPerSlice: %d entries"),
        BakedData->CapMeshesPerSlice.Num());
    for (int32 i = 0; i < BakedData->CapMeshesPerSlice.Num(); ++i)
    {
        const FCachedWaterMesh& M = BakedData->CapMeshesPerSlice[i];
        UE_LOG(LogWaterProto, Display, TEXT("  Cap %2d: V=%d T=%d"),
            i, M.Vertices.Num(), M.Triangles.Num());
    }

    UE_LOG(LogWaterProto, Display, TEXT("Openings: %d segments"),
        BakedData->OpeningSegmentStarts.Num());
    for (int32 i = 0; i < BakedData->OpeningSegmentStarts.Num(); ++i)
    {
        const FVector2D& A = BakedData->OpeningSegmentStarts[i];
        const FVector2D& B = BakedData->OpeningSegmentEnds[i];
        const FVector2D Mid = (A + B) * 0.5f;
        const float Length = FVector2D::Distance(A, B);
        UE_LOG(LogWaterProto, Display,
            TEXT("  Opening %2d: A=(%.1f, %.1f) B=(%.1f, %.1f) Mid=(%.1f, %.1f) Len=%.1f"),
            i, A.X, A.Y, B.X, B.Y, Mid.X, Mid.Y, Length);
    }

    UE_LOG(LogWaterProto, Display, TEXT("====================================="));
}
