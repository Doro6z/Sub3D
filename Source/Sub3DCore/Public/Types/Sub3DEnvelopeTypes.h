#pragma once

#include "CoreMinimal.h"
#include "Sub3DEnvelopeTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FFrameRingDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    FName FrameRingId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SpineAlpha = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring", meta=(ClampMin="1.0"))
    float ThicknessCm = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring", meta=(ClampMin="1.0"))
    float DepthCm = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    bool bIsBayBoundary = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame-Ring", meta=(ClampMin="0.1"))
    float StrengthMultiplier = 1.0f;
};

UENUM(BlueprintType)
enum class ESub3DEnvelopeShape : uint8
{
    Box UMETA(DisplayName="Box"),
    Faired UMETA(DisplayName="Faired"),
    Teardrop UMETA(DisplayName="Teardrop")
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FOuterEnvelopeDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BaseSpineAlpha = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope", meta=(ClampMin="100.0"))
    float LengthCm = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope", meta=(ClampMin="50.0"))
    float WidthCm = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope", meta=(ClampMin="50.0"))
    float HeightCm = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    ESub3DEnvelopeShape Shape = ESub3DEnvelopeShape::Faired;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    float LateralOffsetCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    bool bHasTrunk = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outer Envelope", meta=(ClampMin="40.0", EditCondition="bHasTrunk"))
    float TrunkDiameterCm = 80.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledFrameRingData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    FName FrameRingId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    float SpineAlpha = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    float PositionX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    float ThicknessCm = 8.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    float DepthCm = 15.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frame-Ring")
    bool bIsBayBoundary = false;
};