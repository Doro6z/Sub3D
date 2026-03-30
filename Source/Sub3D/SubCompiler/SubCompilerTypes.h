#pragma once

#include "CoreMinimal.h"
#include "SubmarineTypes.h"
#include "SubCompilerTypes.generated.h"

UENUM(BlueprintType)
enum class EBowSternProfile : uint8
{
	Rounded UMETA(DisplayName = "Rounded"),
	Needle UMETA(DisplayName = "Needle"),
	Blunt UMETA(DisplayName = "Blunt"),
	Bulbous UMETA(DisplayName = "Bulbous"),
	Tapered UMETA(DisplayName = "Tapered")
};

UENUM(BlueprintType)
enum class ECompartmentType : uint8
{
	Helm UMETA(DisplayName = "Helm"),
	Engine UMETA(DisplayName = "Engine"),
	Ballast UMETA(DisplayName = "Ballast"),
	Airlock UMETA(DisplayName = "Airlock"),
	Corridor UMETA(DisplayName = "Corridor"),
	Storage UMETA(DisplayName = "Storage"),
	Crew UMETA(DisplayName = "Crew"),
	Medical UMETA(DisplayName = "Medical")
};

UENUM(BlueprintType)
enum class EPassageType : uint8
{
	WatertightDoor UMETA(DisplayName = "Watertight Door"),
	Hatch UMETA(DisplayName = "Hatch"),
	Open UMETA(DisplayName = "Open"),
	SealedBulkhead UMETA(DisplayName = "Sealed Bulkhead")
};

UENUM(BlueprintType)
enum class EWallSide : uint8
{
	Port UMETA(DisplayName = "Port"),
	Starboard UMETA(DisplayName = "Starboard"),
	Bow UMETA(DisplayName = "Bow"),
	Stern UMETA(DisplayName = "Stern"),
	Floor UMETA(DisplayName = "Floor"),
	Ceiling UMETA(DisplayName = "Ceiling")
};

UENUM(BlueprintType)
enum class ELayoutValidationSeverity : uint8
{
	OK,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct FCompartmentNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	ECompartmentType Type = ECompartmentType::Corridor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "100.0"))
	float MinLengthCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "80.0"))
	float MinWidthCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "180.0"))
	float MinHeightCm = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	TArray<ESubStationType> RequiredSystems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "1"))
	int32 CrewCapacity = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (EditCondition = "bLocked"))
	FVector2D LockedSpineRangeCm = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct FPassageEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage")
	FName FromCompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage")
	FName ToCompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage")
	EPassageType Type = EPassageType::WatertightDoor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage", meta = (ClampMin = "60.0"))
	float MinWidthCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage", meta = (ClampMin = "140.0"))
	float MinHeightCm = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passage", meta = (ClampMin = "0.0"))
	float PressureRatingATM = 10.f;
};

USTRUCT(BlueprintType)
struct FCompartmentPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FName CompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	ECompartmentType Type = ECompartmentType::Corridor;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float SpineStartCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float SpineEndCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float EffectiveRadiusCm = 180.f;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float FloorOffsetCm = -90.f;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float ClearanceHeightCm = 200.f;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float FloorWidthCm = 300.f;
};

USTRUCT(BlueprintType)
struct FBulkheadPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	float SpinePositionCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	float RadiusCm = 180.f;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	EPassageType PassageType = EPassageType::WatertightDoor;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	FVector2D DoorOffsetCm = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	float DoorWidthCm = 90.f;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	float DoorHeightCm = 180.f;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	FName ForeCompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Bulkhead")
	FName AftCompartmentId = NAME_None;
};

USTRUCT(BlueprintType)
struct FStationPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Station")
	ESubStationType StationType = ESubStationType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Station")
	FName CompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Station")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Station")
	EWallSide WallSide = EWallSide::Port;

	UPROPERTY(BlueprintReadOnly, Category = "Station")
	FVector2D ClearanceRectCm = FVector2D(100.f, 120.f);
};

USTRUCT(BlueprintType)
struct FLayoutValidationMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	ELayoutValidationSeverity Severity = ELayoutValidationSeverity::OK;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FName RelatedId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FText Message;
};

USTRUCT(BlueprintType)
struct FSubmarineBuildMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	float TotalLengthCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	float EstimatedMassKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	float EstimatedVolumeLiters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	float BallastCapacityLiters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	int32 CompartmentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	int32 DoorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	int32 StationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Metrics")
	int32 TotalCrewCapacity = 0;
};

USTRUCT(BlueprintType)
struct FSubmarineLayoutSolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Solution")
	TArray<FCompartmentPlacement> Compartments;

	UPROPERTY(BlueprintReadOnly, Category = "Solution")
	TArray<FBulkheadPlacement> Bulkheads;

	UPROPERTY(BlueprintReadOnly, Category = "Solution")
	TArray<FStationPlacement> Stations;

	UPROPERTY(BlueprintReadOnly, Category = "Solution")
	FSubmarineBuildMetrics Metrics;

	UPROPERTY(BlueprintReadOnly, Category = "Solution")
	TArray<FLayoutValidationMessage> ValidationMessages;

	bool HasErrors() const
	{
		return ValidationMessages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
		{
			return Message.Severity == ELayoutValidationSeverity::Error;
		});
	}

	bool IsValid() const
	{
		return !HasErrors() && Compartments.Num() > 0;
	}
};
