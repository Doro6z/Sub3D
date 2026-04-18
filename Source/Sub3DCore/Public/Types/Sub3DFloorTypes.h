#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Sub3DFloorTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FDeckLevelDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Deck Level")
    FName DeckLevelId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Deck Level")
    FName StructuralBayId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Deck Level")
    float ZOffsetCm = 0.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FFloorRegionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName FloorRegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName DeckLevelId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName StructuralBayId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region", meta=(ClampMin="0.0", ClampMax="1.0"))
    float StartAlpha = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region", meta=(ClampMin="0.0", ClampMax="1.0"))
    float EndAlpha = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor Region")
    ESub3DFloorRegionKind Kind = ESub3DFloorRegionKind::MainBand;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledDeckData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Deck Level")
    FName DeckLevelId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Deck Level")
    FName StructuralBayId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Deck Level")
    float ZOffsetCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Deck Level")
    float ClearanceAboveCm = 0.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledFloorRegionData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName FloorRegionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName DeckLevelId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    FName StructuralBayId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    ESub3DFloorRegionKind Kind = ESub3DFloorRegionKind::MainBand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    float StartX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    float EndX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    float WalkableWidthCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    float HeadroomCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    bool bIsWalkable = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Region")
    FCompiledMeshSection FloorMesh;
};