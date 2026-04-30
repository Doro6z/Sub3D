#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase0TestActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Phase 0 — Validation préalable du combo Single Layer Water + ProceduralMeshComponent + WPO.
 * À utiliser dans une carte de test (L_WaterProto_Phase0) avant d'attaquer la voxelisation
 * de l'Étape A. Si l'eau s'affiche correctement avec l'animation Gerstner, le combo
 * shading model + ProcMesh est validé pour la suite du proto. Sinon, fallback translucent
 * custom (cf. doc d'implémentation §4.0).
 */
UCLASS()
class SUB3DWATERPROTO_API APhase0TestActor : public AActor
{
    GENERATED_BODY()

public:
    APhase0TestActor();

    UPROPERTY(VisibleAnywhere, Category = "Phase 0")
    TObjectPtr<UProceduralMeshComponent> ProcMesh;

    /** Assigner M_Phase0_SLW dans le BP dérivé. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 0")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    /** Côté du quad en cm (5 m = 500 cm par défaut). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 0", meta = (ClampMin = "10"))
    float QuadSize = 500.f;

protected:
    virtual void BeginPlay() override;

private:
    void GenerateQuad();
};
