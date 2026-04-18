#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"

namespace Sub3DWave1
{
struct SUB3DCORE_API FControlRingAuthoring
{
    FName ControlRingId = NAME_None;
    float PositionX = 0.0f;
    float HalfWidthCm = 180.0f;
    float HalfHeightCm = 180.0f;
    ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;
    float SectionRoundness = 0.5f;
    float WallThicknessCm = 12.0f;
};

struct SUB3DCORE_API FSubmarineHullAuthoring
{
    FName HullId = NAME_None;
    float LengthCm = 7200.0f;
    float DefaultHalfWidthCm = 180.0f;
    float DefaultHalfHeightCm = 180.0f;
    ESub3DSectionProfile DefaultSectionProfile = ESub3DSectionProfile::Ellipse;
    float DefaultSectionRoundness = 0.5f;
    float DefaultWallThicknessCm = 12.0f;
};
} // namespace Sub3DWave1
