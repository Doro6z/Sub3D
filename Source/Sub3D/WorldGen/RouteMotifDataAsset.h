#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldGenTypes.h"
#include "RouteMotifDataAsset.generated.h"

UCLASS(BlueprintType)
class SUB3D_API URouteMotifDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") FName MotifID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") EBranchProfileIntent PrimaryIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") EBranchPlacementWindow PreferredWindow = EBranchPlacementWindow::Anywhere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") ETraversalComplexityTier MinComplexity = ETraversalComplexityTier::Moderate;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="0")) int32 NodeCost = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="0")) int32 HubCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="0")) int32 OptionalCost = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="0.0")) float SelectionWeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="1", ClampMax="4")) int32 BranchCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") bool bRequiresHub = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif") bool bCanReconnect = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="RouteMotif", meta=(ClampMin="0", ClampMax="4")) int32 PocketDepth = 0;
};

