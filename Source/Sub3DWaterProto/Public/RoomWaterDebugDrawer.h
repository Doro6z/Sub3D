#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RoomWaterDebugDrawer.generated.h"

class UBoxComponent;
class URoomWaterBakedData;
class URoomWaterRenderer;

/**
 * Centralise toutes les fonctions de dessin debug pour le proto Sub3DWaterProto.
 * Statique, sans état, appelable depuis n'importe où.
 *
 * Codes couleur conventionnés (à respecter strictement) :
 *  - Cellule mask=1 (intérieur)        → vert  (0,255,0)   sphère
 *  - Cellule mask=0 (mur, mode verbose) → rouge (255,0,0)
 *  - Contour Marching Squares           → bleu  (0,100,255) ligne 2px
 *  - Segment ouverture détecté          → orange (255,140,0) ligne 5px
 *  - Bornes Box (LocalBoundsMin/Max)    → jaune (255,255,0) box 3px
 *  - Vertices cap mesh                  → magenta (255,0,255) point
 *  - Injection heightfield              → cyan (0,255,255) sphère pulsée 0.5s
 */
UCLASS()
class SUB3DWATERPROTO_API URoomWaterDebugDrawer : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Snapshot complet : bornes, masque slice par slice, contours, ouvertures, plan d'eau actuel.
     * Persistant Duration secondes (default 30s). Logue summary dans LogWaterProto.
     */
    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Debug",
        meta = (DefaultToSelf = "WorldContextObject", AdvancedDisplay = "Duration,bIncludeMaskMisses"))
    static void DrawCompartmentSnapshot(
        UObject* WorldContextObject,
        UBoxComponent* Volume,
        URoomWaterBakedData* BakedData,
        float CurrentWaterLevelLocalZ,
        float Duration = 30.0f,
        bool bIncludeMaskMisses = false);

    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Debug",
        meta = (DefaultToSelf = "WorldContextObject"))
    static void DrawBoxBounds(
        UObject* WorldContextObject,
        UBoxComponent* Volume,
        float Duration = 30.0f);

    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Debug",
        meta = (DefaultToSelf = "WorldContextObject"))
    static void DrawSliceDetailed(
        UObject* WorldContextObject,
        UBoxComponent* Volume,
        URoomWaterBakedData* BakedData,
        int32 SliceIndex,
        float Duration = 30.0f,
        bool bIncludeMaskMisses = false);

    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Debug",
        meta = (DefaultToSelf = "WorldContextObject"))
    static void MarkInjection(
        UObject* WorldContextObject,
        FVector WorldLocation,
        float Force,
        float Radius);

    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Debug")
    static void DumpBakedDataToLog(URoomWaterBakedData* BakedData);
};
