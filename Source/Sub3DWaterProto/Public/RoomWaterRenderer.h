#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RoomWaterRenderer.generated.h"

class UBoxComponent;
class URoomWaterBakedData;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * Composant runtime qui rend l'eau d'un compartiment :
 *  - Charge un cap mesh (différent par slice) + un skirt mesh (build-once + scale Z)
 *  - Tient un heightfield CPU 2D mis à jour chaque frame, pushé en texture au matériau
 *  - Expose une API d'injection ponctuelle de perturbation
 *
 * En proto : assigner SourceVolume vers le UBoxComponent du ARoomActor parent.
 * En Sub3D : pointera vers un UCompartmentVolumeComponent (qui hérite de UBoxComponent).
 * Si SourceVolume laissé null, fallback automatique sur GetAttachParent() casté en UBoxComponent.
 */
UCLASS(ClassGroup = (Sub3DWaterProto),
    meta = (BlueprintSpawnableComponent,
            PrioritizeCategories = "Water Proto, Heightfield, Water Proto|Debug"))
class SUB3DWATERPROTO_API URoomWaterRenderer : public USceneComponent
{
    GENERATED_BODY()

public:
    URoomWaterRenderer();

    /** Source de vérité géométrique du compartiment. Si null, fallback sur GetAttachParent(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TWeakObjectPtr<UBoxComponent> SourceVolume;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<URoomWaterBakedData> BakedData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<UMaterialInterface> CapMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<UMaterialInterface> SkirtMaterial;

    /** Niveau d'eau en local Z (cm), dans les bornes du compartiment. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    float CurrentWaterLevelLocalZ = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
    int32 HeightfieldResolutionX = 64;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
    int32 HeightfieldResolutionY = 64;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "0.1"))
    float WaveSpeed = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "0.9", ClampMax = "1.0"))
    float Damping = 0.985f;

    /** Si true, DrawCompartmentSnapshot affichera aussi les cellules mask=0 (rouge). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug")
    bool bDebugIncludeMaskMisses = false;

    /** Durée d'affichage des debug draws en secondes (snapshot persistant). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug",
        meta = (ClampMin = "1.0", ClampMax = "300.0"))
    float DebugDrawDuration = 30.0f;

    /**
     * Si >= 0, le snapshot affiche UNIQUEMENT cette slice (au lieu de toutes).
     * Permet d'isoler visuellement quand le bruit visuel devient excessif.
     * -1 = toutes les slices.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug",
        meta = (ClampMin = "-1"))
    int32 DebugIsolateSliceIndex = -1;

    /** Si true, snapshot affiche le SDF en gradient continu (au lieu de binaire vert/rouge). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug")
    bool bDebugShowSDFGradient = false;

    /** Si true, affiche le contour Marching Squares avec points jaunes interpolés. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug")
    bool bDebugShowContourDetail = false;

    /** Distance max utilisée pour normaliser le gradient SDF (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Debug",
        meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float DebugSDFMaxDistance = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void SetWaterLevel(float NewLocalZ);

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void InjectAt(FVector2D LocalPosXY, float Force, float Radius = 50.0f);

    /**
     * Helper BP-friendly : convertit un point world en local du compartiment et appelle InjectAt.
     * Exemple typique : raycast caméra→clic souris → FHitResult.ImpactPoint → cet appel.
     * Évite de coder la conversion world→local dans le BP (qui passerait par le component
     * transform du renderer).
     *
     * Retourne true si l'injection a été appliquée. False si le point est hors du compartiment
     * (XY hors LocalBoundsMin/Max), pour éviter de polluer le heightfield avec des injections
     * outside.
     */
    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    bool InjectAtWorldPoint(FVector WorldPos, float Force, float Radius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void ResetHeightfield();

    // ── [P-T4 mini-test] Public accessors for boundary-sync experiment ──
    // These are only useful for UDoorWaterBridge's TickSyncBoundary. To be removed after
    // the test concludes (port to Sub3D will use a different access pattern via the manager).
    TArray<float>& MutableHeights() { return Heights; }
    TArray<float>& MutableVelocities() { return Velocities; }
    int32 GetGridX() const { return HeightfieldResolutionX; }
    int32 GetGridY() const { return HeightfieldResolutionY; }

    /** World position of cell (nx, ny) center at the current water level. Requires BakedData. */
    FVector GetCellWorldCenter(int32 nx, int32 ny) const;

    /**
     * Snapshot complet du compartiment dans le viewport (DebugDrawDuration secondes persistant).
     * Logue aussi un dump détaillé dans WaterProto.log. Appelable depuis Details panel (PIE et éditeur).
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Water Proto|Debug")
    void DrawDebugSnapshot();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> CapMeshComp;

    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> SkirtMeshComp;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> CapMID;

    UPROPERTY()
    TObjectPtr<UTexture2D> HeightfieldTexture;

    /** Buffers heightfield CPU. Taille = ResolutionX * ResolutionY. */
    TArray<float> Heights;
    TArray<float> Velocities;

    int32 CurrentSliceIndex = -1;
    bool bSkirtBuilt = false;
    bool bInitialized = false;

    /** Crée les ProcMesh sub-components via NewObject + RegisterComponent (pattern FloodWaterPlaneComponent). */
    void EnsureMeshComponents();

    /** Init complète (heightfield buffers, texture, MID, fallback matériaux, premier mesh). Idempotent. */
    void LazyInitializeFromBakedData();

    void BuildSkirtMeshOnce();
    void UpdateSkirtScale();
    void TickHeightfield(float DeltaTime);
    void PushHeightfieldToTexture();
    int32 PickClosestSlice(float WaterZ_Local) const;

    /**
     * Trouve les deux slices encadrant Z et le facteur d'interpolation t ∈ [0..1].
     * t=0 → 100% slice IdxBelow, t=1 → 100% slice IdxAbove.
     * Si Z hors range, retourne IdxBelow == IdxAbove (clamp aux bornes).
     */
    void FindBracketingSlices(float Z, int32& OutIdxBelow, int32& OutIdxAbove, float& OutT) const;

    /**
     * Recalcule le cap mesh par lerp des vertices entre les deux slices encadrantes au niveau
     * d'eau courant. Suppose que toutes les slices ont la MÊME topologie (même nombre de verts,
     * même triangulation) — assuré par le resampling au bake (BakeResampleN points + fan
     * triangulation). Première frame : CreateMeshSection. Frames suivantes : UpdateMeshSection
     * (modifie juste les verts, pas les triangles → pas de re-cook).
     */
    void RebuildBlendedCapMesh();

    bool bCapSectionCreated = false;
};
