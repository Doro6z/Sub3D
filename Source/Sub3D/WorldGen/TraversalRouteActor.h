#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TunnelNavDataAsset.h"
#include "WorldGenTypes.h"
#include "Net/UnrealNetwork.h"
#include "TraversalRouteActor.generated.h"

class UProceduralMeshComponent;
class USonarFieldComponent;
class URouteArchetypeDataAsset;
class UBiomeFieldProfileDataAsset;
class UBranchProfileSetDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UArrowComponent;
class UTunnelNavDataBuilder;

// C11 — Runtime container for a generated route.
// Server: builds, validates, holds collision + semantic data.
// Clients: receive FRouteNetSpec via replication and rebuild mesh locally.
UCLASS()
class SUB3D_API ATraversalRouteActor : public AActor
{
	GENERATED_BODY()

public:
	ATraversalRouteActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	// ── Replication ──────────────────────────────────────────────────────────
	// Only the "recipe" (spec + seeds + hash) is replicated — never the meshes.
	UPROPERTY(ReplicatedUsing=OnRep_RouteNetSpec, BlueprintReadOnly)
	FRouteNetSpec RouteNetSpec;

	// ── Editor / Designer ───────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route")
	TObjectPtr<URouteArchetypeDataAsset> ArchetypeAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route")
	TObjectPtr<UBiomeFieldProfileDataAsset> BiomeAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route")
	FRouteGenSpec DebugSpec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	FRouteTunnelDebugOptions TunnelDebugOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Visual")
	bool bUseDebugVertexColorMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Visual")
	TObjectPtr<UMaterialInterface> DebugVertexColorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface")
	FRouteSurfaceBuildSettings SurfaceBuildSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake")
	FString BakedAssetFolder = TEXT("/Game/GeneratedRoutes");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake")
	bool bUseRouteHashInBakedAssetName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake")
	bool bReplaceGeneratedMeshWithBakedAsset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake")
	bool bAutoResolveBakedAssetFromHash = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake")
	bool bUseBakedStaticMeshAtRuntime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bake|Persistence")
	bool bPersistGeneratedRouteMeshInLevel = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bake")
	TObjectPtr<UStaticMesh> BakedStaticMeshAsset;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bake")
	int32 BakedRouteHash = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bake")
	FSavedTraversalRecipe LastSavedRecipe;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bake")
	TSoftObjectPtr<UBranchProfileSetDataAsset> LastResolvedBranchProfileSet;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	FTransform RouteStartTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	bool bHasRouteStartTransform = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	float RouteStartRadiusCm = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	FTransform RouteStartDockTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	bool bHasRouteStartDockTransform = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	float RouteStartDockRadiusCm = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	FTransform RouteEndTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	bool bHasRouteEndTransform = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	float RouteEndRadiusCm = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	FTransform RouteEndDockTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	bool bHasRouteEndDockTransform = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route")
	float RouteEndDockRadiusCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route|TunnelNav")
	bool bBuildTunnelNavData = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route|TunnelNav")
	FTunnelNavBuildSettings TunnelNavBuildSettings;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Route|TunnelNav")
	TObjectPtr<UTunnelNavDataAsset> GeneratedTunnelNavData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	FName CampaignSegmentID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Campaign")
	int32 CampaignPathIndex = INDEX_NONE;

	// ── Build API ────────────────────────────────────────────────────────────
	// Call from server (or PIE) to run the full generation pipeline.
	UFUNCTION(BlueprintCallable, Category="Route")
	bool BuildRouteFromSpec(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds);

	// Editor utility: bake from DebugSpec (can be called from Blueprint CallInEditor button)
	UFUNCTION(CallInEditor, Category="Route")
	void RebakeInEditor();

	UFUNCTION(CallInEditor, Category="Bake")
	void BakeCurrentRouteToStaticMeshAsset();

	UFUNCTION(CallInEditor, Category="Bake|Persistence")
	void PurgeGeneratedRouteMeshComponents();

	UFUNCTION(CallInEditor, Category="Debug|Route")
	void LogRouteEndpointDebugSummary();

	UFUNCTION(CallInEditor, Category="Debug|Route")
	void CopyComputedEndpointsToPlacedOverrides();

	UFUNCTION(BlueprintCallable, Category="Route")
	FTransform GetRouteStartTransformWorld() const;

	UFUNCTION(BlueprintCallable, Category="Route")
	FTransform GetRouteEndTransformWorld() const;

	UFUNCTION(BlueprintCallable, Category="Route")
	FTransform GetRouteStartDockTransformWorld() const;

	UFUNCTION(BlueprintCallable, Category="Route")
	FTransform GetRouteEndDockTransformWorld() const;

	UFUNCTION(BlueprintPure, Category="Route|Sonar")
	USonarFieldComponent* GetSonarFieldComponent() const
	{
		return SonarField;
	}

	UFUNCTION(BlueprintPure, Category="Route|TunnelNav")
	UTunnelNavDataAsset* GetTunnelNavData() const
	{
		return GeneratedTunnelNavData;
	}

	UFUNCTION(BlueprintCallable, Category="Debug|Visual")
	void ConfigureCampaignDebugView(bool bEnableSegmentTint,
		UMaterialInterface* InSegmentDebugMaterial,
		const FLinearColor& InSegmentTintColor,
		bool bEnableRoleVertexDebug);

	UFUNCTION(BlueprintCallable, Category="Debug|Visual")
	void ClearCampaignDebugView();

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void SetCampaignExternalUnionBrushes(const TArray<FVolumeBrushDef>& InBrushes);

	UFUNCTION(BlueprintCallable, Category="Campaign")
	void ClearCampaignExternalUnionBrushes();

	// ── Debug / Validation ───────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category="Debug")
	FRouteValidationReport LastValidationReport;

	UPROPERTY(BlueprintReadOnly, Category="Debug")
	int32 TotalTriangles = 0;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_RouteNetSpec();

private:
	void RefreshEndpointDebugMarkers();
	void UpdateEndpointDebugMarker(UArrowComponent* Marker, const FTransform& EndpointTransform, bool bHasTransform, float RadiusCm);
	void RefreshPlacedEndpointOverrideMarkers();
	void LogRouteEndpointDebugSummaryInternal() const;

	UPROPERTY()
	TObjectPtr<USonarFieldComponent> SonarField;

	// PMC chunks: not replicated — each client builds locally
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> MeshComponents;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BakedStaticMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug|Route", meta=(AllowPrivateAccess="true"))
	bool bShowEndpointDebugMarkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route|Overrides", meta=(AllowPrivateAccess="true"))
	bool bUsePlacedEndpointOverrides = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug|Route", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> RouteStartMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug|Route", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> RouteStartDockMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug|Route", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> RouteEndMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Debug|Route", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> RouteEndDockMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route|Overrides", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> PlacedRouteStartOverride;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route|Overrides", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> PlacedRouteStartDockOverride;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route|Overrides", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> PlacedRouteEndOverride;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route|Overrides", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> PlacedRouteEndDockOverride;

	// Internal pipeline
	bool RunPipeline(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds, bool bSpawnVisualMesh = true);
	bool HasGeneratedTunnelNavRuntimeData() const;
	void EnsureRuntimeNavigationDataForCurrentSpec(bool bReusingExistingVisuals);
	void RefreshVisualDebugMaterials();
	void WriteRouteGenerationLogSnapshot(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds) const;
	void SpawnMeshComponents(const TArray<FRouteMeshChunkData>& Chunks);
	void SpawnMeshComponentSection(const TArray<FRouteMeshChunkData>& Chunks, int32 StartIndex, int32 Count, int32 SectionIndex);
	void ClearMeshComponents();
	void EnsureBakedStaticMeshComponent();
	void ApplyBakedStaticMeshAsset(UStaticMesh* InMeshAsset);
	void SetBakedStaticMeshRuntimeActive(bool bActive);
	void RebuildManagedMeshComponentList();
	void UpdateRouteEndpointTransforms(const TArray<FTraversalTopologyNode>& Nodes,
		const TArray<FTraversalSkeletonSegment>& Skeleton);
	bool TryResolveBakedAssetForHash(uint32 BuildHash);
	uint32 ResolveBuildHashForCurrentState() const;
	FString BuildBakedAssetObjectPath(uint32 BuildHash) const;

	// Computes a deterministic build hash from spec + seeds
	uint32 ComputeBuildHash(const FRouteGenSpec& Spec, const FRouteSeedCascade& Seeds) const;

	bool bCampaignSegmentTintEnabled = false;
	bool bCampaignRoleVertexDebugEnabled = false;
	FLinearColor CampaignSegmentTintColor = FLinearColor::White;
	TArray<FVolumeBrushDef> CampaignExternalUnionBrushes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CampaignSegmentDebugMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CampaignSegmentDebugMID;
};
