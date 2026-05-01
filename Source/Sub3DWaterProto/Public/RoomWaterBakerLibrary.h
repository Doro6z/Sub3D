#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RoomWaterBakerLibrary.generated.h"

class UBoxComponent;
class URoomWaterBakedData;

/**
 * Pipeline de bake exposé en BlueprintCallable, consommable depuis un Editor Utility Widget
 * ou un wrapper sur ARoomActor.
 *
 * API découplée de tout Actor : prend un UBoxComponent + FName, produit un URoomWaterBakedData.
 * Cette discipline garantit que le portage vers Sub3D (où le compartiment est un
 * UCompartmentVolumeComponent — hérite de UBoxComponent — attaché à la Pawn) est un simple
 * renommage sans refactor d'API.
 */
UCLASS()
class SUB3DWATERPROTO_API URoomWaterBakerLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Voxelise le BoxComponent en NumSlices niveaux Z, génère les masks + contours + cap meshes,
     * détecte les segments d'ouverture. Retourne un URoomWaterBakedData transient (non sauvegardé).
     */
    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Bake")
    static URoomWaterBakedData* BakeVolume(
        UBoxComponent* Volume,
        FName CompartmentId,
        int32 NumSlices = 12,
        float CellSize = 25.0f,
        bool bAutoDetectOpenings = false,
        float CapInsetCm = 2.0f,
        int32 BakeResampleN = 64);

    /**
     * Bake + sauvegarde dans un asset persistant. PackagePath = chemin du dossier (avec slash final),
     * AssetName par défaut = "BD_<CompartmentId>".
     *
     * bAutoDetectOpenings : heuristique naïve "contour proche du bord du Box". Faux positifs sur
     * salle close avec Box légèrement plus grand. Désactivé par défaut. À remplacer en Étape B
     * par un système d'ouvertures explicites via ASubDoorActor.
     *
     * CapInsetCm : décalage cosmétique du cap mesh vers l'intérieur (le long de la normale
     * inward du contour). Garantit que le cap est tucké dans le mur même si l'interpolation
     * MS a 1-2mm d'erreur. 0 = pas d'inset (contour exact à SDF=0).
     */
    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Bake")
    static URoomWaterBakedData* BakeAndSave(
        UBoxComponent* Volume,
        FName CompartmentId,
        const FString& PackagePath = TEXT("/Game/Sub3DWaterProto/BakedData/"),
        int32 NumSlices = 12,
        float CellSize = 25.0f,
        bool bAutoDetectOpenings = false,
        float CapInsetCm = 2.0f,
        int32 BakeResampleN = 64);
};
