#include "DoorWaterBridge.h"

#include "Sub3DWaterProto.h"
#include "RoomActor.h"
#include "RoomWaterRenderer.h"
#include "Submarine/SubDoorActor.h"

#include "EngineUtils.h" // TActorIterator
#include "ProceduralMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UDoorWaterBridge::UDoorWaterBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;

    // Pas de CreateDefaultSubobject ici : pattern NewObject + RegisterComponent au BeginPlay
    // (cohérent avec URoomWaterRenderer, évite les Template Mismatch sur BP enfants).
}

void UDoorWaterBridge::BeginPlay()
{
    Super::BeginPlay();

    EnsureSubComponents();

    // Cache la porte parent si on est attaché à une ASubDoorActor.
    if (AActor* Owner = GetOwner())
    {
        CachedDoor = Cast<ASubDoorActor>(Owner);
    }

    // Résolution FName → URoomWaterRenderer (auto si parent porte, manuel sinon).
    ResolveRenderers();

    // État initial : appliqué après le premier tick parce que le parent peut ne pas avoir
    // encore exécuté son propre BeginPlay (qui résout bClosed depuis bStartsClosed).
    bInitialStateApplied = false;
}

void UDoorWaterBridge::ResolveRenderers()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 1) Détermine les FName cibles : porte parent en priorité, override sinon.
    FName TargetA = CompartmentAOverride;
    FName TargetB = CompartmentBOverride;

    if (CachedDoor.IsValid())
    {
        if (TargetA.IsNone()) { TargetA = CachedDoor->CompartmentA; }
        if (TargetB.IsNone()) { TargetB = CachedDoor->CompartmentB; }
    }

    if (TargetA.IsNone() || TargetB.IsNone())
    {
        UE_LOG(LogWaterProto, Warning,
            TEXT("UDoorWaterBridge (%s): unresolved compartment IDs (A=%s, B=%s) — set CompartmentAOverride/BOverride or attach to an ASubDoorActor with CompartmentA/B configured"),
            *GetNameSafe(GetOwner()),
            *TargetA.ToString(), *TargetB.ToString());
        return;
    }

    // 2) Walke ARoomActor du level, match par RoomId. En portage Sub3D : remplacer par lookup
    // sur ASubmarineBase manager (CompartmentId → URoomWaterRenderer via UCompartmentVolumeComponent).
    RendererA.Reset();
    RendererB.Reset();
    int32 NumRoomsScanned = 0;

    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (!Room) { continue; }
        ++NumRoomsScanned;

        if (Room->RoomId == TargetA && !RendererA.IsValid() && Room->WaterRenderer)
        {
            RendererA = Room->WaterRenderer;
        }
        if (Room->RoomId == TargetB && !RendererB.IsValid() && Room->WaterRenderer)
        {
            RendererB = Room->WaterRenderer;
        }
    }

    UE_LOG(LogWaterProto, Display,
        TEXT("UDoorWaterBridge (%s): resolved A=%s→%s, B=%s→%s (scanned %d rooms)"),
        *GetNameSafe(GetOwner()),
        *TargetA.ToString(), RendererA.IsValid() ? TEXT("OK") : TEXT("MISS"),
        *TargetB.ToString(), RendererB.IsValid() ? TEXT("OK") : TEXT("MISS"),
        NumRoomsScanned);

    if (!RendererA.IsValid() || !RendererB.IsValid())
    {
        UE_LOG(LogWaterProto, Warning,
            TEXT("UDoorWaterBridge (%s): missing renderer — bridge inactive jusqu'à ce que les rooms soient placées avec les RoomId attendus"),
            *GetNameSafe(GetOwner()));
    }
}

void UDoorWaterBridge::EnsureSubComponents()
{
    if (!BridgeMesh)
    {
        BridgeMesh = NewObject<UProceduralMeshComponent>(this, TEXT("BridgeMesh"));
        BridgeMesh->SetMobility(EComponentMobility::Movable);
        BridgeMesh->SetupAttachment(this);
        BridgeMesh->RegisterComponent();
        BridgeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BridgeMesh->bUseAsyncCooking = false;
        BridgeMesh->SetVisibility(false);

        if (WaterMaterial)
        {
            BridgeMesh->SetMaterial(0, WaterMaterial);
        }
    }

    if (!FlowFxComp && FlowEffect)
    {
        FlowFxComp = NewObject<UNiagaraComponent>(this, TEXT("FlowFx"));
        FlowFxComp->SetMobility(EComponentMobility::Movable);
        FlowFxComp->SetupAttachment(this);
        FlowFxComp->SetAsset(FlowEffect);
        FlowFxComp->RegisterComponent();
        FlowFxComp->SetAutoActivate(false);
        FlowFxComp->Deactivate();
    }
}

void UDoorWaterBridge::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* TickFunc)
{
    Super::TickComponent(DeltaTime, TickType, TickFunc);

    PollDoorState();

    if (!bDoorOpen)
    {
        return;
    }
    if (!RendererA.IsValid() || !RendererB.IsValid())
    {
        return;
    }

    UpdateBridgeMesh();
    UpdateFlowFx();
}

void UDoorWaterBridge::PollDoorState()
{
    if (!CachedDoor.IsValid())
    {
        return;
    }

    const bool bClosedNow = CachedDoor->bClosed;

    // Premier appel : toujours appliquer (init).
    if (!bInitialStateApplied)
    {
        OnDoorStateChanged(bClosedNow);
        bLastObservedClosed = bClosedNow;
        bInitialStateApplied = true;
        return;
    }

    // Transitions seulement.
    if (bClosedNow != bLastObservedClosed)
    {
        OnDoorStateChanged(bClosedNow);
        bLastObservedClosed = bClosedNow;
    }
}

void UDoorWaterBridge::OnDoorStateChanged(bool bClosed)
{
    bDoorOpen = !bClosed;

    if (BridgeMesh)
    {
        BridgeMesh->SetVisibility(bDoorOpen);
    }
    if (FlowFxComp)
    {
        if (bDoorOpen)
        {
            FlowFxComp->Activate();
        }
        else
        {
            FlowFxComp->Deactivate();
        }
    }

    // Slosh à l'ouverture : impulsion symétrique des deux côtés au niveau de la porte.
    // Position d'injection en local de chaque salle = projection de la porte dans la frame
    // du compartiment, en ne gardant que XY (le niveau Z est géré par la cellule du heightfield).
    if (bDoorOpen && OpenSloshForce > 0.f)
    {
        const FVector DoorWorld = GetComponentLocation();
        for (URoomWaterRenderer* Renderer : { RendererA.Get(), RendererB.Get() })
        {
            if (!Renderer)
            {
                continue;
            }
            const FVector LocalPos = Renderer->GetComponentTransform().InverseTransformPosition(DoorWorld);
            Renderer->InjectAt(FVector2D(LocalPos.X, LocalPos.Y), OpenSloshForce, 80.f);
        }
    }
}

float UDoorWaterBridge::GetRendererWaterWorldZ(const URoomWaterRenderer* Renderer)
{
    if (!Renderer)
    {
        return 0.f;
    }
    // Renderer est attaché à CompartmentVolume. La transform du component intègre la hauteur de
    // l'Actor + offset éventuel du Renderer. CurrentWaterLevelLocalZ est en local du Box.
    return Renderer->GetComponentLocation().Z + Renderer->CurrentWaterLevelLocalZ;
}

void UDoorWaterBridge::UpdateBridgeMesh()
{
    if (!BridgeMesh)
    {
        return;
    }

    const float WaterZ_A = GetRendererWaterWorldZ(RendererA.Get());
    const float WaterZ_B = GetRendererWaterWorldZ(RendererB.Get());

    // Plan de surface visible au niveau de l'embrasure : l'eau s'écoule par le HAUT du
    // niveau le plus bas (Torricelli). Le bridge est donc à min(A, B).
    const float BridgeWorldZ = FMath::Min(WaterZ_A, WaterZ_B);

    // Quad horizontal en local space du bridge (door's frame). Z=0 dans le mesh, Z absolu
    // géré via SetWorldLocation pour rester gravity-aligned indépendamment du yaw de la porte.
    const float HalfDepth = DoorDepth * 0.5f;
    const float HalfWidth = DoorWidth * 0.5f;

    TArray<FVector> Vertices = {
        FVector(-HalfDepth, -HalfWidth, 0.f),
        FVector(+HalfDepth, -HalfWidth, 0.f),
        FVector(+HalfDepth, +HalfWidth, 0.f),
        FVector(-HalfDepth, +HalfWidth, 0.f),
    };
    TArray<int32> Triangles = { 0, 1, 2, 0, 2, 3 };
    TArray<FVector> Normals = {
        FVector(0, 0, 1), FVector(0, 0, 1), FVector(0, 0, 1), FVector(0, 0, 1),
    };
    TArray<FVector2D> UVs = {
        FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1),
    };

    if (BridgeMesh->GetNumSections() == 0)
    {
        BridgeMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
            TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial)
        {
            BridgeMesh->SetMaterial(0, WaterMaterial);
        }
    }

    // Position World du bridge : XY = position de la porte (la component transform), Z = niveau
    // d'eau bas. Garde la geometry stable, déplace juste le component.
    const FVector DoorWorld = GetComponentLocation();
    BridgeMesh->SetWorldLocation(FVector(DoorWorld.X, DoorWorld.Y, BridgeWorldZ));

    // Yaw seulement (gravity-aligned). Yaw hérité de la porte via attachement → on annule
    // pitch/roll du parent en forçant un WorldRotation qui ne garde que le yaw.
    const FRotator OwnerRot = GetComponentRotation();
    BridgeMesh->SetWorldRotation(FRotator(0.f, OwnerRot.Yaw, 0.f));
}

void UDoorWaterBridge::UpdateFlowFx()
{
    if (!FlowFxComp)
    {
        return;
    }

    const float WaterZ_A = GetRendererWaterWorldZ(RendererA.Get());
    const float WaterZ_B = GetRendererWaterWorldZ(RendererB.Get());
    const float Delta = FMath::Abs(WaterZ_A - WaterZ_B);

    if (Delta < MinDeltaForFlowFx)
    {
        if (FlowFxComp->IsActive())
        {
            FlowFxComp->Deactivate();
        }
        return;
    }

    if (!FlowFxComp->IsActive())
    {
        FlowFxComp->Activate();
    }

    // Vélocité Torricelli simplifié : v = sqrt(2 g h).
    constexpr float G = 980.f; // cm/s²
    const float FlowSpeed = FMath::Sqrt(2.f * G * Delta);

    // Direction : du compartiment HAUT vers le BAS, en world.
    const URoomWaterRenderer* High = (WaterZ_A > WaterZ_B) ? RendererA.Get() : RendererB.Get();
    const URoomWaterRenderer* Low = (WaterZ_A > WaterZ_B) ? RendererB.Get() : RendererA.Get();
    if (!High || !Low)
    {
        return;
    }

    FVector Dir = Low->GetComponentLocation() - High->GetComponentLocation();
    Dir.Z = 0.f; // composante horizontale uniquement (l'eau coule ~à plat à travers la porte)
    if (!Dir.Normalize())
    {
        return;
    }

    FlowFxComp->SetVariableFloat(TEXT("FlowSpeed"), FlowSpeed);
    FlowFxComp->SetVariableVec3(TEXT("FlowDirection"), Dir);

    // Injection continue côté arrivée pour faire onduler la salle qui se remplit.
    URoomWaterRenderer* Receiver = (WaterZ_A > WaterZ_B) ? RendererB.Get() : RendererA.Get();
    if (Receiver && FlowReceiverForce > 0.f)
    {
        const FVector DoorWorld = GetComponentLocation();
        const FVector LocalPos = Receiver->GetComponentTransform().InverseTransformPosition(DoorWorld);
        Receiver->InjectAt(FVector2D(LocalPos.X, LocalPos.Y), FlowReceiverForce, 40.f);
    }
}
