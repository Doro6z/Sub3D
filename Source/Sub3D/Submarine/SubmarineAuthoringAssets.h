#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubmarineAuthoringTypes.h"
#include "SubmarineAuthoringAssets.generated.h"

UCLASS(BlueprintType)
class SUB3D_API USubmarineAuthoringAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName SubmarineId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	FSubmarineHullAuthoring Hull;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decks")
	TArray<FSubmarineDeckAuthoring> Decks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vertical")
	TArray<FSubmarineVerticalOpeningAuthoring> VerticalOpenings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartments")
	TArray<FSubmarineCompartmentAuthoring> Compartments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connections")
	TArray<FSubmarineBulkheadConnectionAuthoring> BulkheadConnections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connections")
	TArray<FSubmarineVerticalConnectorAuthoring> VerticalConnectors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	FSubmarineStructureAuthoring Structure;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	FSubmarineMaterialSetAuthoring Materials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bake")
	FSubmarineBakeSettings BakeSettings;
};

UCLASS(BlueprintType)
class SUB3D_API UCompiledSubmarineAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineMeshSection> RenderSections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	FCompiledSubmarineCollisionData Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineCompartmentData> Compartments;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineBulkheadConnectionData> BulkheadConnections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineVerticalOpeningData> VerticalOpenings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineVerticalConnectorData> VerticalConnectors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FStructuralSheetCompiledBinding> StructuralBindings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Compiled")
	TArray<FCompiledSubmarineMaterialSlot> MaterialSlots;
};
