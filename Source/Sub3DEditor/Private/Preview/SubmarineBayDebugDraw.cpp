#include "Preview/SubmarineBayDebugDraw.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace Sub3DWave3
{
void FSubmarineBayDebugDraw::DrawStructuralBays(
    UWorld* World,
    const FSubmarineHullDef& Hull,
    const TArray<FCompiledBayData>& StructuralBays,
    const bool bPersistentLines,
    const float LifeTime)
{
    if (!World)
    {
        return;
    }

    const float HalfWidth = FMath::Max(Hull.DefaultHalfWidthCm, 50.0f);
    const float HalfHeight = FMath::Max(Hull.DefaultHalfHeightCm, 50.0f);

    for (int32 Index = 0; Index < StructuralBays.Num(); ++Index)
    {
        const FCompiledBayData& Bay = StructuralBays[Index];
        const float CenterX = (Bay.StartX + Bay.EndX) * 0.5f;
        const float ExtentX = FMath::Max((Bay.EndX - Bay.StartX) * 0.5f, 1.0f);

        const FVector Center(CenterX, 0.0f, 0.0f);
        const FVector Extent(ExtentX, HalfWidth, HalfHeight);
        const FColor Color = FColor::MakeRedToGreenColorFromScalar(static_cast<float>((Index % 8) + 1) / 8.0f);

        DrawDebugBox(World, Center, Extent, Color, bPersistentLines, LifeTime, 0, 2.0f);

        const FString Label = FString::Printf(TEXT("%s | Deck Levels: %d"), *Bay.BayId.ToString(), Bay.MaxDeckLevels);
        DrawDebugString(World, Center + FVector(0.0f, 0.0f, HalfHeight + 35.0f), Label, nullptr, Color, LifeTime, bPersistentLines);
    }
}
} // namespace Sub3DWave3