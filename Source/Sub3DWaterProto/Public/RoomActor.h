#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class URoomWaterBakedData;
class URoomWaterRenderer;

/**
 * Conteneur de test du proto Sub3DWaterProto — UN SEUL compartiment, autonome dans une carte.
 *
 * Cette classe est UNIQUEMENT un conteneur pédagogique. Aucune classe runtime
 * (URoomWaterRenderer, baker, DoorWaterBridge) ne référence ARoomActor — toutes prennent
 * UBoxComponent / URoomWaterRenderer en paramètre. Cette discipline garantit que le portage
 * vers Sub3D (compartiment = component sur Pawn, pas Actor) est un simple renommage.
 */
UCLASS(meta = (PrioritizeCategories = "Water Proto, Water Proto|Bake"))
class SUB3DWATERPROTO_API ARoomActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomActor();

    /** Volume box du compartiment (zone à voxeliser au bake). Root du Actor. */
    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<UBoxComponent> CompartmentVolume;

    /** Géométrie statique de la salle (murs/sol/plafond, dont au moins un mur courbe). */
    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<UStaticMeshComponent> RoomMesh;

    /** Composant qui rend l'eau. Auto-bind son SourceVolume sur CompartmentVolume au BeginPlay. */
    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<URoomWaterRenderer> WaterRenderer;

    /** DataAsset cuit pour cette salle. Assigné après bake. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<URoomWaterBakedData> BakedData;

    /** Niveau d'eau exposé pour test (0..1, mappé sur les bornes Z du compartiment au runtime). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto",
        meta = (ClampMin = "0", ClampMax = "1"))
    float WaterLevelNormalized = 0.5f;

    /** ID unique de la salle (clé pour Étape B et le rename mapping vers CompartmentId Sub3D). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    FName RoomId = TEXT("Room_01");

    /** Taille de cellule du bake en cm. Plus petit = boundary plus précise (moins stair-stepped) mais bake plus lent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "5", ClampMax = "100"))
    float BakeCellSize = 25.f;

    /** Nombre de slices Z. Plus = transitions de silhouette plus fines en hauteur. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "2", ClampMax = "32"))
    int32 BakeNumSlices = 12;

    /**
     * Détection automatique des ouvertures (pour skirt). Heuristique naïve "contour proche du bord
     * du Box" — faux positifs sur salle close. Désactivé par défaut. À activer post-Étape B
     * quand le système d'ouvertures explicites via ASubDoorActor sera en place.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake")
    bool bAutoDetectOpenings = false;

    /**
     * Inset cosmétique du cap mesh vers l'intérieur (cm). Pousse les vertices du contour le long
     * de la normale inward avant triangulation Delaunay. Garantit que le cap est tucké dans le
     * mur même si l'interpolation MS a 1-2mm d'erreur. 0 = pas d'inset (contour exact à SDF=0).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float CapInsetCm = 2.0f;

    /**
     * Nombre de points uniformément resamplés sur le contour avant triangulation. Tous les caps
     * de toutes les slices ont exactement N points dans le même ordre → topologie cohérente,
     * permet le vertex blending au runtime entre slices adjacentes (transitions continues du
     * niveau d'eau, pas de paliers). 32 = grossier mais léger. 64 = bon compromis. 128 = très
     * fluide pour murs courbes complexes mais .uasset 2× plus gros.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "16", ClampMax = "256"))
    int32 BakeResampleN = 64;

    /**
     * Nombre d'anneaux concentriques intermédiaires entre le centroïde et le polygone du
     * contour, pour tessellater l'intérieur du cap mesh. Chaque vertex intérieur sample sa
     * propre cellule du heightfield → la propagation des ondes devient visible (au lieu d'être
     * concentrée sur un unique centroïde).
     *
     * 0 = aucun anneau intermédiaire (fan classique, cap = centroïde + polygone). Singularité
     *     du centre + dead zones près des coins étroits visibles.
     * 1-2 = tessellation modérée, propagation à 50% de visible. Léger côté .uasset.
     * 3 = recommandé. Tessellation dense (4 anneaux dont le polygone), propagation visible
     *     entre rings, pas de dead zone. Coût : .uasset ~4× plus gros, négligeable.
     * 6-8 = très haute qualité, surdimensionné pour la plupart des cas.
     *
     * Total verts par slice : 1 + (BakeRingsCount + 1) × BakeResampleN.
     * Total triangles : (2 × BakeRingsCount + 1) × BakeResampleN.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "0", ClampMax = "8"))
    int32 BakeRingsCount = 3;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * Wrapper de confort : appelle URoomWaterBakerLibrary::BakeAndSave() sur CompartmentVolume.
     * La logique baker reste découplée — ce wrapper n'est que pour cliquer depuis le Details panel.
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Water Proto|Bake")
    void Bake();
};
