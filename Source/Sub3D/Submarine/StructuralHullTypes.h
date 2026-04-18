#pragma once

#include "CoreMinimal.h"
#include "StructuralHullTypes.generated.h"

UENUM(BlueprintType)
enum class EBreachPassageState : uint8
{
	LeakOnly        UMETA(DisplayName = "LeakOnly"),
	StrongSuction   UMETA(DisplayName = "StrongSuction"),
	ActorEjectable  UMETA(DisplayName = "ActorEjectable"),
	CreatureEnterable UMETA(DisplayName = "CreatureEnterable")
};

UENUM(BlueprintType)
enum class ESheetSide : uint8
{
	Unknown   UMETA(DisplayName = "Unknown"),
	Port      UMETA(DisplayName = "Port"),
	Starboard UMETA(DisplayName = "Starboard"),
	Top       UMETA(DisplayName = "Top"),
	Bottom    UMETA(DisplayName = "Bottom"),
	Bulkhead  UMETA(DisplayName = "Bulkhead")
};

USTRUCT(BlueprintType)
struct FCompiledHullRegionRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 SectionIndexStart = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 SectionIndexEnd = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ExteriorVertexStart = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ExteriorVertexCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ExteriorTriangleStart = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ExteriorTriangleCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 InteriorVertexStart = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 InteriorVertexCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 InteriorTriangleStart = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 InteriorTriangleCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector LocalNormal = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FBox LocalBounds = FBox(EForceInit::ForceInit);
};

USTRUCT(BlueprintType)
struct FStructuralSheetCompiledBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName SheetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 CompartmentIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	ESheetSide Side = ESheetSide::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FCompiledHullRegionRange MeshRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector2D ChartMin = FVector2D(0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector2D ChartMax = FVector2D(1.f, 1.f);
};

USTRUCT(BlueprintType)
struct FSubCompartmentDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	float CapacityLiters = 12000.f;

	// Interior hydraulic volume used by flooding, immersion and future water visuals.
	// This is intentionally distinct from structural sheets and envelope hull bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment|Hydro")
	FVector HydroBoundsMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment|Hydro")
	FVector HydroBoundsMax = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment|Hydro")
	float WalkableFloorZCm = 0.f;
};

USTRUCT(BlueprintType)
struct FStructuralSheetDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FName SheetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FName ParentCompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FName AdjacentCompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FVector LocalOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FVector LocalNormal = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FVector LocalTangentX = FVector::RightVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FVector LocalTangentY = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	FVector2D SizeCm = FVector2D(200.f, 200.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	float ThicknessCm = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	float MaterialStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	int32 GridResolutionX = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	int32 GridResolutionY = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet")
	bool bCanOpenToExterior = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual")
	bool bSupportsVisualRupture = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual", meta = (ClampMin = "0"))
	int32 ExteriorVisualMaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual")
	FVector VisualLocalOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual")
	FVector VisualLocalTangentX = FVector::RightVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual")
	FVector VisualLocalTangentY = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual")
	FVector2D VisualProjectionSizeCm = FVector2D(200.f, 200.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual", meta = (ClampMin = "0.0"))
	float MaxVisibleRuptureRadiusCm = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheet|Visual", meta = (ClampMin = "0.1"))
	float PreferredRuptureBorderScale = 1.f;
};

USTRUCT(BlueprintType)
struct FStructuralCellState
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 CellIndex = 0;

	UPROPERTY()
	float Damage01 = 0.f;

	UPROPERTY()
	float ThicknessRemaining = 1.f;

	UPROPERTY()
	bool bLeaking = false;

	UPROPERTY()
	bool bOpen = false;
};

USTRUCT(BlueprintType)
struct FStructuralSheetRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	FName SheetId = NAME_None;

	UPROPERTY()
	TArray<FStructuralCellState> Cells;
};

USTRUCT(BlueprintType)
struct FBreachClusterState
{
	GENERATED_BODY()

	UPROPERTY()
	FName SheetId = NAME_None;

	UPROPERTY()
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector LocalNormal = FVector::ForwardVector;

	UPROPERTY()
	float OpenAreaCm2 = 0.f;

	UPROPERTY()
	float InscribedRadiusCm = 0.f;

	UPROPERTY()
	bool bTouchesExterior = false;
};

USTRUCT(BlueprintType)
struct FBreachFlowField
{
	GENERATED_BODY()

	UPROPERTY()
	FName SheetId = NAME_None;

	UPROPERTY()
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	UPROPERTY()
	float InnerRadiusCm = 30.f;

	UPROPERTY()
	float OuterRadiusCm = 120.f;

	UPROPERTY()
	float ForceScale = 0.f;

	UPROPERTY()
	EBreachPassageState PassageState = EBreachPassageState::LeakOnly;
};

USTRUCT(BlueprintType)
struct FCompartmentRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	FName CompartmentId = NAME_None;

	UPROPERTY()
	float CapacityLiters = 12000.f;

	UPROPERTY()
	float CurrentWaterLiters = 0.f;

	UPROPERTY()
	float WaterLevelNormalized = 0.f;

	UPROPERTY()
	float WaterHeightCm = 0.f;

	UPROPERTY()
	float MaxWaterHeightCm = 200.f;

	UPROPERTY()
	float FreeAirLiters = 12000.f;

	UPROPERTY()
	float InternalPressureKPa = 101.325f;

	UPROPERTY()
	float ExternalReferencePressureKPa = 101.325f;

	UPROPERTY()
	float PressureDeltaKPa = 0.f;

	UPROPERTY()
	float FloodRateIn = 0.f;

	UPROPERTY()
	float FloodRateOut = 0.f;

	UPROPERTY()
	float PumpRateOut = 0.f;

	UPROPERTY()
	bool bPumpActive = false;

	UPROPERTY()
	bool bFullyFlooded = false;

	UPROPERTY()
	bool bPressureCritical = false;
};
