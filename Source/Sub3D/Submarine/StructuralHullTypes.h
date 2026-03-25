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
	float FloodRateIn = 0.f;

	UPROPERTY()
	float PumpRateOut = 0.f;

	UPROPERTY()
	bool bPumpActive = false;
};

