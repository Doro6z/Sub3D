#pragma once

#include "CoreMinimal.h"
#include "Sub3DCanonicalEnums.generated.h"

UENUM(BlueprintType)
enum class ESub3DSectionProfile : uint8
{
    Circle UMETA(DisplayName="Circle"),
    Ellipse UMETA(DisplayName="Ellipse"),
    Superellipse UMETA(DisplayName="Superellipse")
};

UENUM(BlueprintType)
enum class ESub3DConnectorType : uint8
{
    Ladder UMETA(DisplayName="Ladder"),
    Ramp UMETA(DisplayName="Ramp"),
    Passage UMETA(DisplayName="Passage"),
    Trunk UMETA(DisplayName="Trunk")
};

UENUM(BlueprintType)
enum class ESub3DClosureType : uint8
{
    Door UMETA(DisplayName="Door"),
    Hatch UMETA(DisplayName="Hatch"),
    Valve UMETA(DisplayName="Valve")
};

UENUM(BlueprintType)
enum class ESub3DOpeningType : uint8
{
    DeckCutout UMETA(DisplayName="Deck Cutout"),
    HatchOpening UMETA(DisplayName="Hatch Opening"),
    Shaft UMETA(DisplayName="Shaft"),
    Doorway UMETA(DisplayName="Doorway")
};

UENUM(BlueprintType)
enum class ESub3DPartitionType : uint8
{
    PressureBulkhead UMETA(DisplayName="Pressure Bulkhead"),
    InternalWall UMETA(DisplayName="Internal Wall")
};

UENUM(BlueprintType)
enum class ESub3DRoomTag : uint8
{
    Navigation UMETA(DisplayName="Navigation"),
    Ballast UMETA(DisplayName="Ballast"),
    Machine UMETA(DisplayName="Machine"),
    Habitat UMETA(DisplayName="Habitat"),
    Storage UMETA(DisplayName="Storage"),
    Medical UMETA(DisplayName="Medical"),
    Airlock UMETA(DisplayName="Airlock"),
    Custom UMETA(DisplayName="Custom")
};

UENUM(BlueprintType)
enum class ESub3DFloorRegionKind : uint8
{
    MainBand UMETA(DisplayName="Main Band"),
    SideCorridor UMETA(DisplayName="Side Corridor"),
    Mezzanine UMETA(DisplayName="Mezzanine"),
    Platform UMETA(DisplayName="Platform"),
    HalfLevel UMETA(DisplayName="Half Level"),
    TechnicalZone UMETA(DisplayName="Technical Zone")
};

UENUM(BlueprintType)
enum class ESub3DHullLongitudinalProfile : uint8
{
    Manual UMETA(DisplayName="Manual", ToolTip="Control rings are placed manually"),
    Myring UMETA(DisplayName="Myring", ToolTip="Classic torpedo profile: power-law nose, cosine-power tail"),
    Series58 UMETA(DisplayName="Series 58", ToolTip="US Navy Series 58 polynomial body of revolution"),
    SuperellipseLongitudinal UMETA(DisplayName="Superellipse", ToolTip="Superellipse longitudinal profile: |x/a|^n = 1 - |r/b|^n"),
    Uniform UMETA(DisplayName="Uniform", ToolTip="Constant radius with hemispherical caps")
};

