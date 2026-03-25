#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CampaignGraphAsset.h"
#include "CampaignWorldManager.generated.h"

class ATraversalRouteActor;
class UCampaignGraphAsset;
class ARouteConnectorActor;
class UMaterialInterface;

UCLASS()
class SUB3D_API ACampaignWorldManager : public AActor
{
	GENERATED_BODY()

public:
	ACampaignWorldManager();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<UCampaignGraphAsset> CampaignGraph = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	int32 MasterSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<ATraversalRouteActor> TargetRouteActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<ATraversalRouteActor> PreviewRouteActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<ARouteConnectorActor> ConnectorActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	bool bAutoBuildCurrentSegmentOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	bool bResetManagedActorTransformsOnBuild = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	float PreviewConnectorGapCm = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	bool bUseOverlapSegmentConnections = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	bool bShowDebugConnectorMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	bool bAutoSpawnPathPreviewActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Debug")
	bool bWriteCampaignGenerationLogs = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Debug")
	bool bEnableSegmentColorDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Debug")
	TObjectPtr<UMaterialInterface> SegmentDebugMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Debug")
	bool bEnableRoleDebugForSelectedSegments = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Debug")
	TArray<FName> RoleDebugSegmentIDs;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	FCampaignProgressState ProgressState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	FName PreviewNextSegmentID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FName SelectedNextSegmentID;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	FTransform PreviewConnectorStart = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	FTransform PreviewConnectorEnd = FTransform::Identity;

	UFUNCTION(CallInEditor, Category="Campaign")
	void ResetCampaignProgress();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void BuildRootSegmentInEditor();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void BuildCurrentSegmentInEditor();

	UFUNCTION(CallInEditor, Category="Campaign")
	void BuildCampaignSliceInEditor();

	UFUNCTION(CallInEditor, Category="Campaign")
	void AdvanceCampaignSliceInEditor();

	UFUNCTION(CallInEditor, Category="Campaign")
	void BuildCampaignPathPreviewInEditor();

	UFUNCTION(CallInEditor, Category="Campaign")
	void ClearCampaignPathPreviewInEditor();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void AdvanceToFirstConnectedSegmentInEditor();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void PreviewSelectedNextSegmentAndConnectorInEditor();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void SelectFirstConnectedSegmentInEditor();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	bool BuildSegmentByID(FName SegmentID);

	UFUNCTION(BlueprintCallable, Category="Campaign")
	bool AdvanceToFirstConnectedSegment();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	bool PreviewSelectedNextSegmentAndConnector();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	bool ValidateCampaignGraph(FString& OutError) const;

protected:
	virtual void BeginPlay() override;

private:
	ATraversalRouteActor* ResolveTargetRouteActor();
	ATraversalRouteActor* ResolvePreviewRouteActor();
	ARouteConnectorActor* ResolveConnectorActor();
	ATraversalRouteActor* ResolvePathRouteActor(int32 PathIndex);
	void ResetPreviewArtifacts();
	void AlignRouteActorToPrevious(ATraversalRouteActor* PreviousRoute, ATraversalRouteActor* CurrentRoute) const;
	void ConfigureSpecForCampaignSegment(FRouteGenSpec& InOutSpec, const FCampaignSegmentDescriptor& Descriptor) const;
	bool BuildDescriptorToActor(const FCampaignSegmentDescriptor& Descriptor, int32 SegmentIndex, ATraversalRouteActor* RouteActor, bool bUpdateProgressState);
	TArray<const FCampaignSegmentDescriptor*> BuildPreviewPathDescriptors() const;
	void ApplyDebugViewsToBuiltPath();
	void WriteCampaignPathPreviewLogSnapshot() const;
	void ApplyCrossSegmentUnionToBuiltPath();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATraversalRouteActor>> SpawnedPathRouteActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATraversalRouteActor>> BuiltPathRouteActors;

	UPROPERTY(Transient)
	TArray<FName> BuiltPathSegmentIDs;
};
