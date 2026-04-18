#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubCompiler/SubCompilerTypes.h"
#include "SubmarineBase.h"
#include "SubmarineAuthoringAssets.h"
#include "SubmarineAuthoringActors.generated.h"

class UProceduralMeshComponent;
class USceneComponent;
class USubmarineLayoutAsset;

UCLASS(Blueprintable)
class SUB3D_API ASubmarineAuthoringPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASubmarineAuthoringPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Authoring")
	TObjectPtr<USubmarineAuthoringAsset> AuthoringAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Authoring")
	TObjectPtr<UCompiledSubmarineAsset> BakedAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authoring")
	bool bAutoRefreshPreviewOnConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewXRayMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowExteriorHull = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowInteriorHull = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowDecks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowBulkheads = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowDoors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewShowRamps = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authoring")
	TObjectPtr<USceneComponent> PreviewRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Authoring")
	TArray<FLayoutValidationMessage> LastValidationMessages;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Authoring")
	bool ValidateAuthoring();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Authoring")
	bool BakeAuthoring();

	UFUNCTION(CallInEditor, Category = "Authoring")
	void ValidateAuthoringNow();

	UFUNCTION(CallInEditor, Category = "Authoring")
	void BakeAuthoringNow();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Authoring")
	void RefreshPreview();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Authoring")
	void ClearPreview();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Preview")
	void ApplyPreviewVisibility();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> PreviewRenderSections;

	UPROPERTY(Transient)
	TObjectPtr<UCompiledSubmarineAsset> TransientPreviewAsset = nullptr;
};

UCLASS(Blueprintable)
class SUB3D_API ASubmarineBakedRuntimeActor : public ASubmarineBase
{
	GENERATED_BODY()

public:
	ASubmarineBakedRuntimeActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual UPrimitiveComponent* GetMovementCollisionComponent() const override;
	virtual TArray<UPrimitiveComponent*> GetInteriorWalkableComponents() const override;
	virtual FTransform GetCrewEmbarkTransform() const override;
	virtual bool ValidateSpawnCollision() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compiled")
	TObjectPtr<UCompiledSubmarineAsset> CompiledAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compiled")
	bool bBuildOnConstruction = true;

	UFUNCTION(BlueprintCallable, Category = "Compiled")
	bool BuildFromCompiledAsset();

	UFUNCTION(BlueprintCallable, Category = "Compiled")
	void ClearBakedGeometry();

	UFUNCTION(BlueprintCallable, Category = "Compiled")
	bool SetBulkheadConnectionBlocked(FName BoundaryId, bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Compiled")
	bool IsBulkheadConnectionBlocked(FName BoundaryId) const;

	UFUNCTION(BlueprintCallable, Category = "Compiled")
	bool SetBulkheadOpeningBlocked(FName OpeningId, bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Compiled")
	bool IsBulkheadOpeningBlocked(FName OpeningId) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> RenderSectionComponents;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UProceduralMeshComponent>> RenderSectionComponentMap;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> WalkableCollisionComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> BulkheadCollisionComponents;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UProceduralMeshComponent>> BulkheadBlockerComponentMap;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ExteriorCollisionProxyComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USubmarineLayoutAsset> TransientCompiledLayout = nullptr;
};
