#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WorldGenTypes.h"
#include "BranchProfileDataAsset.generated.h"

UCLASS(BlueprintType)
class SUB3D_API UBranchProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfile") FName ProfileID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfile", meta=(ClampMin="1")) int32 SchemaVersion = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfile") FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intent") EBranchProfileIntent Intent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intent") bool bAllowedOnCanonicalRoute = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intent") bool bAllowedOnOptionalRoute = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intent") bool bAllowedNearCheckpoint = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intent") bool bAllowedNearHub = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="1000.0")) float TargetRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.25", ClampMax="4.0")) float LengthScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.0", ClampMax="3.0")) float CurvatureScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.0", ClampMax="3.0")) float VerticalityScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.0", ClampMax="1.0")) float RejoinChance = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.0", ClampMax="1.0")) float PocketChance = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="0.0", ClampMax="1.0")) float SecondarySplitChance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Geometry", meta=(ClampMin="1", ClampMax="4")) int32 MaxDepth = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") EBranchPlacementWindow PreferredWindow = EBranchPlacementWindow::Anywhere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0")) float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tags") FGameplayTagContainer RequiredBiomeTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tags") FGameplayTagContainer ForbiddenBiomeTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tags") FGameplayTagContainer SemanticTags;
};

