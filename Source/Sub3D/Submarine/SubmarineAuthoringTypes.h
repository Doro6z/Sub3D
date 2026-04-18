#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "SubCompiler/SubCompilerTypes.h"
#include "StructuralHullTypes.h"
#include "SubmarineAuthoringTypes.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class ESubmarineSectionProfile : uint8
{
	Circle       UMETA(DisplayName = "Circle"),
	Ellipse      UMETA(DisplayName = "Ellipse"),
	Superellipse UMETA(DisplayName = "Superellipse")
};

UENUM(BlueprintType)
enum class ESubmarineVerticalConnectorType : uint8
{
	Ramp         UMETA(DisplayName = "Ramp"),
	LadderAnchor UMETA(DisplayName = "Ladder Anchor")
};

UENUM(BlueprintType)
enum class ESubmarineVerticalOpeningType : uint8
{
	HatchOpening UMETA(DisplayName = "Hatch Opening"),
	LadderOpening UMETA(DisplayName = "Ladder Opening")
};

USTRUCT(BlueprintType)
struct FSubmarineHullAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "500.0"))
	float LengthCm = 7200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "100.0"))
	float MaxOuterDiameterCm = 760.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "2.0", ClampMax = "50.0"))
	float WallThicknessCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	EBowSternProfile BowProfile = EBowSternProfile::Rounded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	EBowSternProfile SternProfile = EBowSternProfile::Tapered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float BowTaperFraction = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float SternTaperFraction = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	ESubmarineSectionProfile SectionProfile = ESubmarineSectionProfile::Superellipse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float SectionRoundness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float WidthToHeightRatio = 1.f;

	// Multiplier curve over [0..1] along the spine. Empty curve = 1.0 everywhere.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	FRuntimeFloatCurve LongitudinalRadiusCurve;

	// Direct width-to-height override over [0..1] along the spine. Empty curve = WidthToHeightRatio.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	FRuntimeFloatCurve WidthToHeightCurve;
};

USTRUCT(BlueprintType)
struct FSubmarineDeckAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck")
	FName DeckId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck", meta = (ClampMin = "-500.0", ClampMax = "500.0"))
	float LocalZCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float WidthScale = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck", meta = (ClampMin = "0.0"))
	float StartInsetCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck", meta = (ClampMin = "0.0"))
	float EndInsetCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck")
	bool bWalkable = true;
};

USTRUCT(BlueprintType)
struct FSubmarineCompartmentAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	ECompartmentType Type = ECompartmentType::Corridor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment", meta = (ClampMin = "100.0"))
	float TargetLengthCm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment", meta = (ClampMin = "100.0"))
	float MinLengthCm = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FSubmarineVerticalOpeningAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	FName OpeningId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	ESubmarineVerticalOpeningType Type = ESubmarineVerticalOpeningType::HatchOpening;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "0"))
	int32 FromDeckIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "0"))
	int32 ToDeckIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "0.0"))
	float LocalX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "20.0"))
	float WidthCm = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "20.0"))
	float LengthCm = 120.f;
};

USTRUCT(BlueprintType)
struct FSubmarineBulkheadOpeningAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	FName OpeningId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "0"))
	int32 DeckIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening", meta = (ClampMin = "0.0"))
	FVector2D DoorSizeCm = FVector2D(90.f, 190.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Opening")
	bool bBlockedByDefault = true;
};

USTRUCT(BlueprintType)
struct FSubmarineBulkheadConnectionAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName BoundaryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName CompartmentA = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName CompartmentB = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	TArray<FSubmarineBulkheadOpeningAuthoring> Openings;
};

USTRUCT(BlueprintType)
struct FSubmarineVerticalConnectorAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector")
	FName ConnectorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector")
	FName OpeningId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector")
	ESubmarineVerticalConnectorType Type = ESubmarineVerticalConnectorType::Ramp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (ClampMin = "0"))
	int32 FromDeckIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (ClampMin = "0"))
	int32 ToDeckIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (ClampMin = "0.0"))
	float LocalX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (ClampMin = "20.0"))
	float WidthCm = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connector", meta = (ClampMin = "5.0", ClampMax = "45.0"))
	float SlopeDegrees = 18.f;
};

USTRUCT(BlueprintType)
struct FSubmarineStructureAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "1", ClampMax = "16"))
	int32 LongitudinalSheetsPerCompartment = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "4", ClampMax = "16"))
	int32 CircumferentialSheetCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bGenerateStructuralBindings = true;
};

USTRUCT(BlueprintType)
struct FSubmarineMaterialSetAuthoring
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> ExteriorHull;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> InteriorHull;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Deck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Bulkhead;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> DoorFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Ramp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> BreachRim;
};

USTRUCT(BlueprintType)
struct FSubmarineBakeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bake", meta = (ClampMin = "8", ClampMax = "256"))
	int32 LongitudinalSegments = 48;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bake", meta = (ClampMin = "12", ClampMax = "96"))
	int32 RadialSegments = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bake")
	bool bBakeCollision = true;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineMaterialSlot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName SlotName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TSoftObjectPtr<UMaterialInterface> Material;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineMeshSection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName SectionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 MaterialSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FVector3f> Positions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FVector3f> Normals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FVector4f> Tangents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FVector2f> UV0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<int32> Indices;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineCollisionData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FCompiledSubmarineMeshSection ExteriorProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineMeshSection> WalkableDeckSections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineMeshSection> BulkheadBlockerSections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineMeshSection> RampSections;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineCompartmentData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName CompartmentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	ECompartmentType Type = ECompartmentType::Corridor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float StartXcm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float EndXcm = 0.f;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineBulkheadOpeningData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName OpeningId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 DeckIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector2D DoorSizeCm = FVector2D(90.f, 190.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float DoorSillZCm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	bool bBlockedByDefault = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName BlockerSectionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName DoorFrameSectionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName DoorLeafSectionId = NAME_None;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineBulkheadConnectionData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName BoundaryId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName CompartmentA = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName CompartmentB = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float LocalX = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName BlockerSectionId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineBulkheadOpeningData> Openings;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineVerticalConnectorData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName ConnectorId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName OpeningId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	ESubmarineVerticalConnectorType Type = ESubmarineVerticalConnectorType::Ramp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 FromDeckIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ToDeckIndex = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector StartLocal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FVector EndLocal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float WidthCm = 120.f;
};

USTRUCT(BlueprintType)
struct FCompiledSubmarineVerticalOpeningData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FName OpeningId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	ESubmarineVerticalOpeningType Type = ESubmarineVerticalOpeningType::HatchOpening;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 FromDeckIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	int32 ToDeckIndex = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float LocalX = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float WidthCm = 120.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float LengthCm = 120.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float LowerDeckZCm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	float UpperDeckZCm = 0.f;
};
