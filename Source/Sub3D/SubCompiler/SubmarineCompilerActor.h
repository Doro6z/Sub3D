#pragma once

#include "CoreMinimal.h"
#include "SubCompilerTypes.h"
#include "Submarine/SubmarineBase.h"
#include "SubmarineCompilerActor.generated.h"

class UMaterialInterface;
class UCapsuleComponent;
class UProceduralMeshComponent;
class USubmarineEnvelopeDef;
class USubmarineFunctionalGraph;
class USubmarineLayoutAsset;
class ASubDoorActor;
class ASubStationBase;
class UPrimitiveComponent;
class UPointLightComponent;

UCLASS(Blueprintable)
class SUB3D_API ASubmarineCompilerActor : public ASubmarineBase
{
	GENERATED_BODY()

public:
	ASubmarineCompilerActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual UPrimitiveComponent* GetMovementCollisionComponent() const override;
	virtual TArray<UPrimitiveComponent*> GetInteriorWalkableComponents() const override;
	virtual bool ValidateSpawnCollision() const override;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler")
	bool CompileCurrentDefinitions();

	UFUNCTION(BlueprintCallable, Category = "SubCompiler")
	bool BuildGeneratedGeometry();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "SubCompiler")
	bool CompileAndBuild();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "SubCompiler")
	bool CompileAndBuildDirty();

	UFUNCTION(CallInEditor, Category = "SubCompiler", meta = (DisplayName = "Compile And Build"))
	void CompileAndBuildInEditor();

	UFUNCTION(CallInEditor, Category = "SubCompiler", meta = (DisplayName = "Compile And Build Dirty"))
	void CompileAndBuildDirtyInEditor();

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Preview")
	bool SavePreviewToEnvelope();

	UFUNCTION(CallInEditor, Category = "SubCompiler|Preview", meta = (DisplayName = "Save Preview To Envelope"))
	void SavePreviewToEnvelopeInEditor();

	UFUNCTION(BlueprintCallable, Category = "SubCompiler")
	void ClearGeneratedGeometry();

	UFUNCTION(BlueprintPure, Category = "SubCompiler")
	const USubmarineLayoutAsset* GetCompiledLayoutAsset() const { return CompiledLayoutAsset; }

	const FSubmarineLayoutSolution& GetLastSolution() const { return LastSolution; }

	UFUNCTION(BlueprintPure, Category = "SubCompiler")
	FSubmarineBuildMetrics GetLastMetrics() const { return LastSolution.Metrics; }

	UFUNCTION(BlueprintPure, Category = "SubCompiler")
	const TArray<FCompartmentPlacement>& GetCompartmentPlacements() const { return LastSolution.Compartments; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	TObjectPtr<USubmarineEnvelopeDef> EnvelopeDef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	TObjectPtr<USubmarineFunctionalGraph> FunctionalGraph = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview")
	bool bUseEnvelopePreviewOverrides = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "500.0"))
	float PreviewSpineLengthCm = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewDefaultRadiusCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.0"))
	float PreviewFloorDropBiasCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewBowRadiusCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewForeShoulderRadiusCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewMidBodyRadiusCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewAftShoulderRadiusCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "50.0"))
	float PreviewSternRadiusCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "1", ClampMax = "16"))
	int32 PreviewExteriorLongitudinalSubdivisionsPerSpan = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "12", ClampMax = "64"))
	int32 PreviewExteriorRadialSegments = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "8", ClampMax = "48"))
	int32 PreviewInteriorArcSegments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.5", ClampMax = "8.0"))
	float PreviewSectionExponent = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.5", ClampMax = "2.0"))
	float PreviewWidthToHeightRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides"))
	EBowSternProfile PreviewBowProfile = EBowSternProfile::Rounded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides"))
	EBowSternProfile PreviewSternProfile = EBowSternProfile::Tapered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.0", ClampMax = "0.4"))
	float PreviewBowTaperFraction = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.0", ClampMax = "0.4"))
	float PreviewSternTaperFraction = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "0.0", ClampMax = "50.0"))
	float PreviewExteriorHullOffsetCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Preview", meta = (EditCondition = "bUseEnvelopePreviewOverrides", ClampMin = "2.0", ClampMax = "20.0"))
	float PreviewWallThicknessCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler|Materials")
	TObjectPtr<UMaterialInterface> ExteriorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler|Materials")
	TObjectPtr<UMaterialInterface> InteriorWallMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler|Materials")
	TObjectPtr<UMaterialInterface> FloorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler|Materials")
	TObjectPtr<UMaterialInterface> InteriorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	bool bBuildOnConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	bool bEnableInteriorCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Collision")
	bool bUseExteriorCollisionProxy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Collision")
	bool bPreferGeneratedExteriorMeshCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Collision", meta = (ClampMin = "0.0"))
	float ExteriorCollisionPaddingCm = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubCompiler|Collision")
	bool bShowExteriorCollisionProxyInEditor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	bool bUseMvpDefaultsWhenUnset = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler")
	bool bLogValidationMessages = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubCompiler|Doors")
	TSubclassOf<ASubDoorActor> DoorActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TObjectPtr<USubmarineLayoutAsset> CompiledLayoutAsset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TObjectPtr<USubmarineEnvelopeDef> ResolvedEnvelopeDef = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TArray<FLayoutValidationMessage> LastMessages;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TArray<TObjectPtr<UProceduralMeshComponent>> GeneratedInteriorMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TObjectPtr<UProceduralMeshComponent> GeneratedExteriorMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TArray<TObjectPtr<ASubStationBase>> GeneratedStations;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TArray<TObjectPtr<ASubDoorActor>> GeneratedDoors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SubCompiler")
	TArray<TObjectPtr<UPointLightComponent>> GeneratedCompartmentLights;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SubCompiler|Collision")
	TObjectPtr<UCapsuleComponent> ExteriorCollisionProxy = nullptr;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "SubCompiler|Collision")
	void RefreshExteriorCollisionProxy();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "SubCompiler|Collision")
	void LogExteriorCollisionProxyState() const;

private:
	void EnsureDefaultDefinitions();
	USubmarineEnvelopeDef* ResolveEnvelopeDefinition();
	void ApplyPreviewOverridesToEnvelope(USubmarineEnvelopeDef& Envelope) const;
	void RefreshSocketsFromSolution();
	void LogValidationMessages() const;
	void DestroyGeneratedInteriorMeshes();
	void DestroyDirtyInteriorMeshes(const TSet<FName>& DirtyIds);
	bool BuildGeneratedDoors();
	void DestroyGeneratedDoors();
	bool BuildGeneratedStations();
	void DestroyGeneratedStations();
	void ComputeDirtyCompartments(const FSubmarineLayoutSolution& OldSolution, const FSubmarineLayoutSolution& NewSolution, TSet<FName>& OutDirtyIds) const;
	void ConfigureExteriorCollisionProxy();

private:
	FSubmarineLayoutSolution LastSolution;

	UPROPERTY(Transient)
	FSubmarineLayoutSolution PreviousSolution;
};
