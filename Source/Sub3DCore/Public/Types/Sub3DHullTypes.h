#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DHullTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FControlRingDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(EditCondition="false", EditConditionHides))
    FName ControlRingId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="0.0"))
    float PositionX = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="1.0"))
    float HalfWidthCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="1.0"))
    float HalfHeightCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SectionRoundness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="0.0"))
    float WallThicknessCm = 12.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FHullProfileParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile")
    ESub3DHullLongitudinalProfile Profile = ESub3DHullLongitudinalProfile::Manual;

    /** Myring: nose power exponent (higher = blunter nose). Typical range 1.0-4.0. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Myring", meta=(ClampMin="0.5", ClampMax="6.0", EditCondition="Profile==ESub3DHullLongitudinalProfile::Myring"))
    float MyringNoseExponent = 2.0f;

    /** Myring: tail angle in degrees. Typical range 15-35. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Myring", meta=(ClampMin="5.0", ClampMax="60.0", EditCondition="Profile==ESub3DHullLongitudinalProfile::Myring"))
    float MyringTailAngleDeg = 25.0f;

    /** Myring: fraction of hull length used by the nose section (0..1). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Myring", meta=(ClampMin="0.05", ClampMax="0.5", EditCondition="Profile==ESub3DHullLongitudinalProfile::Myring"))
    float MyringNoseFraction = 0.20f;

    /** Myring: fraction of hull length used by the tail section (0..1). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Myring", meta=(ClampMin="0.05", ClampMax="0.5", EditCondition="Profile==ESub3DHullLongitudinalProfile::Myring"))
    float MyringTailFraction = 0.25f;

    /** Series 58: body fineness ratio (L/D). Typical 6-10. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Series58", meta=(ClampMin="3.0", ClampMax="15.0", EditCondition="Profile==ESub3DHullLongitudinalProfile::Series58"))
    float Series58Fineness = 7.0f;

    /** Superellipse longitudinal exponent. 2.0 = ellipse, higher = more box-like. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile|Superellipse", meta=(ClampMin="1.2", ClampMax="8.0", EditCondition="Profile==ESub3DHullLongitudinalProfile::SuperellipseLongitudinal"))
    float LongitudinalExponent = 2.5f;

    /** Fraction of hull length for the parallel midbody (Superellipse/Uniform). 0.0 = pure ellipsoid. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull Profile", meta=(ClampMin="0.0", ClampMax="0.8", EditCondition="Profile!=ESub3DHullLongitudinalProfile::Manual"))
    float ParallelMidbodyFraction = 0.4f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSubmarineHullDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    FName HullId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="1.0"))
    float LengthCm = 7200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="1.0"))
    float DefaultHalfWidthCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="1.0"))
    float DefaultHalfHeightCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    ESub3DSectionProfile DefaultSectionProfile = ESub3DSectionProfile::Ellipse;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DefaultSectionRoundness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(ClampMin="0.0"))
    float DefaultWallThicknessCm = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull")
    FHullProfileParams ProfileParams;
};
