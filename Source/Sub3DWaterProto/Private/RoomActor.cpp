#include "RoomActor.h"

#include "Sub3DWaterProto.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RoomWaterRenderer.h"
#include "RoomWaterBakedData.h"
#include "RoomWaterBakerLibrary.h"

ARoomActor::ARoomActor()
{
    PrimaryActorTick.bCanEverTick = true;

    CompartmentVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CompartmentVolume"));
    SetRootComponent(CompartmentVolume);
    CompartmentVolume->SetBoxExtent(FVector(400.f, 300.f, 150.f));   // 8m x 6m x 3m par défaut
    CompartmentVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RoomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoomMesh"));
    RoomMesh->SetupAttachment(CompartmentVolume);

    WaterRenderer = CreateDefaultSubobject<URoomWaterRenderer>(TEXT("WaterRenderer"));
    WaterRenderer->SetupAttachment(CompartmentVolume);
}

void ARoomActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!WaterRenderer || !BakedData)
    {
        return;
    }

    // Sync BakedData AVANT SetWaterLevel — sinon le renderer voit BakedData=null et early-return.
    // (Ce sync arrive aussi quand l'utilisateur change le DataAsset via Details panel après BeginPlay.)
    if (WaterRenderer->BakedData != BakedData)
    {
        WaterRenderer->BakedData = BakedData;
    }

    // Mappe WaterLevelNormalized (0..1) sur la plage Z bakée et pousse au renderer.
    const float MinZ = BakedData->LocalBoundsMin.Z;
    const float MaxZ = BakedData->LocalBoundsMax.Z;
    const float TargetZ = FMath::Lerp(MinZ, MaxZ, FMath::Clamp(WaterLevelNormalized, 0.f, 1.f));
    WaterRenderer->SetWaterLevel(TargetZ);
}

void ARoomActor::Bake()
{
    if (!CompartmentVolume)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("ARoomActor::Bake: CompartmentVolume manquant"));
        return;
    }

    URoomWaterBakedData* Result = URoomWaterBakerLibrary::BakeAndSave(
        CompartmentVolume,
        RoomId,
        TEXT("/Game/Sub3DWaterProto/BakedData/"),
        BakeNumSlices,
        BakeCellSize,
        bAutoDetectOpenings,
        CapInsetCm);

    if (Result)
    {
        BakedData = Result;
        if (WaterRenderer)
        {
            WaterRenderer->BakedData = Result;
        }
        UE_LOG(LogWaterProto, Display, TEXT("ARoomActor::Bake: %s baked (%d slices)"),
            *RoomId.ToString(), Result->Slices.Num());
    }
    else
    {
        UE_LOG(LogWaterProto, Warning, TEXT("ARoomActor::Bake: bake failed for %s"), *RoomId.ToString());
    }
}
