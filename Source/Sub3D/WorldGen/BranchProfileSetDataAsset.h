#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BranchProfileSetDataAsset.generated.h"

class UBranchProfileDataAsset;

UCLASS(BlueprintType)
class SUB3D_API UBranchProfileSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfileSet") FName ProfileSetID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfileSet", meta=(ClampMin="1")) int32 SchemaVersion = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BranchProfileSet") TArray<TSoftObjectPtr<UBranchProfileDataAsset>> Profiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Complexity", meta=(ClampMin="0.0")) float SimpleSelectionBias = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Complexity", meta=(ClampMin="0.0")) float ModerateSelectionBias = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Complexity", meta=(ClampMin="0.0")) float DenseSelectionBias = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budgets", meta=(ClampMin="0")) int32 MaxCanonicalBypassProfiles = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budgets", meta=(ClampMin="0")) int32 MaxOptionalProfiles = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budgets", meta=(ClampMin="0")) int32 MaxPocketChainProfiles = 4;
};

