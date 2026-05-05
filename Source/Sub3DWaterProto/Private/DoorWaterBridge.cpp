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
#include "DrawDebugHelpers.h"
#include "RoomWaterBakedData.h"

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

    // [P-T4] Calcul des paires de cellules frontière une fois (transforms stables).
    RecomputeCellPairs();

    // [P-T4] Tick prerequisite : les renderers doivent ticker APRÈS ce composant pour que la
    // wave equation propage les valeurs de bord synchronisées dans le buffer interne du frame.
    if (RendererA.IsValid())
    {
        RendererA->AddTickPrerequisiteComponent(this);
    }
    if (RendererB.IsValid())
    {
        RendererB->AddTickPrerequisiteComponent(this);
    }

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

    // [P-T4.0 mini-test] Dump door + parent + renderers transforms pour calibrer la sync au bord.
    {
        const FTransform OwnerXform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
        const FTransform BridgeXform = GetComponentTransform();
        UE_LOG(LogWaterProto, Warning,
            TEXT("[P-T4.0 DOOR] %s | OwnerLoc=%s OwnerRot=%s | BridgeLoc=%s BridgeRot=%s | DoorWidth=%.1f DoorDepth=%.1f"),
            *GetNameSafe(GetOwner()),
            *OwnerXform.GetLocation().ToString(),
            *OwnerXform.GetRotation().Rotator().ToString(),
            *BridgeXform.GetLocation().ToString(),
            *BridgeXform.GetRotation().Rotator().ToString(),
            DoorWidth, DoorDepth);

        if (RendererA.IsValid())
        {
            const FTransform RXf = RendererA->GetComponentTransform();
            UE_LOG(LogWaterProto, Warning,
                TEXT("[P-T4.0 RENDERER A] %s | Loc=%s Rot=%s | ResX=%d ResY=%d"),
                *TargetA.ToString(),
                *RXf.GetLocation().ToString(),
                *RXf.GetRotation().Rotator().ToString(),
                RendererA->HeightfieldResolutionX,
                RendererA->HeightfieldResolutionY);
        }
        if (RendererB.IsValid())
        {
            const FTransform RXf = RendererB->GetComponentTransform();
            UE_LOG(LogWaterProto, Warning,
                TEXT("[P-T4.0 RENDERER B] %s | Loc=%s Rot=%s | ResX=%d ResY=%d"),
                *TargetB.ToString(),
                *RXf.GetLocation().ToString(),
                *RXf.GetRotation().Rotator().ToString(),
                RendererB->HeightfieldResolutionX,
                RendererB->HeightfieldResolutionY);
        }
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

    // [P-T4.3] Debug draw indépendant de l'état de la porte (utile en porte fermée pour vérifier
    // la calibration des paires sans déclencher de sync).
    if (bDrawDebugCells && CellPairs.Num() > 0 && GetWorld())
    {
        UWorld* World = GetWorld();
        for (const FCellPair& Pair : CellPairs)
        {
            // Recompute cell world centers each frame (water level may have moved).
            const FVector WorldA = RendererA.IsValid()
                ? RendererA->GetCellWorldCenter(Pair.IdxA % RendererA->GetGridX(), Pair.IdxA / RendererA->GetGridX())
                : Pair.WorldA;
            const FVector WorldB = RendererB.IsValid()
                ? RendererB->GetCellWorldCenter(Pair.IdxB % RendererB->GetGridX(), Pair.IdxB / RendererB->GetGridX())
                : Pair.WorldB;
            DrawDebugSphere(World, WorldA, 4.f, 6, FColor::Cyan, false, 0.f, 0, 0.5f);
            DrawDebugSphere(World, WorldB, 4.f, 6, FColor::Magenta, false, 0.f, 0, 0.5f);
            DrawDebugLine(World, WorldA, WorldB, FColor::Yellow, false, 0.f, 0, 0.3f);
        }
    }

    if (!bDoorOpen)
    {
        return;
    }
    if (!RendererA.IsValid() || !RendererB.IsValid())
    {
        return;
    }

    // [P-T4] Sync au bord avant tout calcul visuel pour propager les changements heightfield avant
    // qu'ils ne soient pushed vers la texture (le push à lieu dans le tick du renderer; on tick après).
    if (bBoundarySyncEnabled)
    {
        TickSyncBoundary();
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

// ── [P-T4 mini-test] Boundary cell sync implementation ──────────────────────────────────────

bool UDoorWaterBridge::ComputeBoundaryCellsForRenderer(
    URoomWaterRenderer* Renderer,
    const FVector& DoorWorldCenter,
    float DoorHalfWidthCm,
    TArray<int32>& OutIndices,
    TArray<FVector>& OutWorldPositions) const
{
    OutIndices.Reset();
    OutWorldPositions.Reset();

    if (!Renderer || !Renderer->BakedData)
    {
        return false;
    }

    const FTransform& RendererXf = Renderer->GetComponentTransform();
    const FVector DoorLocal = RendererXf.InverseTransformPosition(DoorWorldCenter);

    const FVector& Min = Renderer->BakedData->LocalBoundsMin;
    const FVector& Max = Renderer->BakedData->LocalBoundsMax;

    // Trouver la face la plus proche (4 candidats : Xmin/Xmax/Ymin/Ymax).
    const float dXmin = FMath::Abs(DoorLocal.X - Min.X);
    const float dXmax = FMath::Abs(DoorLocal.X - Max.X);
    const float dYmin = FMath::Abs(DoorLocal.Y - Min.Y);
    const float dYmax = FMath::Abs(DoorLocal.Y - Max.Y);
    const float MinDist = FMath::Min(FMath::Min(dXmin, dXmax), FMath::Min(dYmin, dYmax));

    enum class EFace : uint8 { Xmin, Xmax, Ymin, Ymax };
    EFace Face;
    if (MinDist == dXmin)      { Face = EFace::Xmin; }
    else if (MinDist == dXmax) { Face = EFace::Xmax; }
    else if (MinDist == dYmin) { Face = EFace::Ymin; }
    else                       { Face = EFace::Ymax; }

    const int32 GridX = Renderer->GetGridX();
    const int32 GridY = Renderer->GetGridY();

    // Direction perpendiculaire à la face (axe de la porte = vers la perpendiculaire).
    // L'axe parallèle à la face (le long duquel on itère les cellules) est l'autre.
    int32 FixedNX = -1;
    int32 FixedNY = -1;
    bool bIterateOverY = false;
    switch (Face)
    {
    case EFace::Xmin: FixedNX = 0;          bIterateOverY = true;  break;
    case EFace::Xmax: FixedNX = GridX - 1;  bIterateOverY = true;  break;
    case EFace::Ymin: FixedNY = 0;          bIterateOverY = false; break;
    case EFace::Ymax: FixedNY = GridY - 1;  bIterateOverY = false; break;
    }

    // Filtre par footprint de porte : distance (world) le long de l'axe perpendiculaire à la
    // forward de la porte = perpendiculaire à la "normale du mur" = direction de la largeur.
    // On utilise le RightVector du bridge component (axe Y local de la porte = la largeur).
    const FVector DoorRightWorld = GetRightVector();

    auto AddCellIfInFootprint = [&](int32 nx, int32 ny)
    {
        const FVector CellWorld = Renderer->GetCellWorldCenter(nx, ny);
        const FVector ToCell = CellWorld - DoorWorldCenter;
        const float SignedDistOnRight = FVector::DotProduct(ToCell, DoorRightWorld);
        if (FMath::Abs(SignedDistOnRight) <= DoorHalfWidthCm)
        {
            const int32 FlatIdx = ny * GridX + nx;
            OutIndices.Add(FlatIdx);
            OutWorldPositions.Add(CellWorld);
        }
    };

    if (bIterateOverY)
    {
        for (int32 ny = 0; ny < GridY; ++ny)
        {
            AddCellIfInFootprint(FixedNX, ny);
        }
    }
    else
    {
        for (int32 nx = 0; nx < GridX; ++nx)
        {
            AddCellIfInFootprint(nx, FixedNY);
        }
    }

    UE_LOG(LogWaterProto, Display,
        TEXT("[P-T4] Boundary cells for renderer (%s): face=%d, fixed=(nx=%d,ny=%d), iterY=%d, count=%d"),
        *GetNameSafe(Renderer->GetOwner()),
        static_cast<int32>(Face), FixedNX, FixedNY, bIterateOverY ? 1 : 0, OutIndices.Num());

    return OutIndices.Num() > 0;
}

void UDoorWaterBridge::RecomputeCellPairs()
{
    CellPairs.Reset();

    if (!RendererA.IsValid() || !RendererB.IsValid())
    {
        return;
    }

    const FVector DoorWorldCenter = GetComponentLocation();
    const float DoorHalfWidth = DoorWidth * 0.5f;

    TArray<int32> IndicesA, IndicesB;
    TArray<FVector> WorldA, WorldB;

    if (!ComputeBoundaryCellsForRenderer(RendererA.Get(), DoorWorldCenter, DoorHalfWidth, IndicesA, WorldA))
    {
        UE_LOG(LogWaterProto, Warning, TEXT("[P-T4] No boundary cells for RendererA — sync disabled"));
        return;
    }
    if (!ComputeBoundaryCellsForRenderer(RendererB.Get(), DoorWorldCenter, DoorHalfWidth, IndicesB, WorldB))
    {
        UE_LOG(LogWaterProto, Warning, TEXT("[P-T4] No boundary cells for RendererB — sync disabled"));
        return;
    }

    // Pairing nearest-neighbor (world distance) — robuste à orientation arbitraire.
    // O(NumA × NumB), trivial sur cas typique (~18×18).
    CellPairs.Reserve(IndicesA.Num());
    for (int32 i = 0; i < IndicesA.Num(); ++i)
    {
        int32 BestJ = INDEX_NONE;
        float BestDistSq = TNumericLimits<float>::Max();
        for (int32 j = 0; j < IndicesB.Num(); ++j)
        {
            const float DSq = FVector::DistSquared(WorldA[i], WorldB[j]);
            if (DSq < BestDistSq)
            {
                BestDistSq = DSq;
                BestJ = j;
            }
        }
        if (BestJ != INDEX_NONE)
        {
            FCellPair Pair;
            Pair.IdxA = IndicesA[i];
            Pair.IdxB = IndicesB[BestJ];
            Pair.WorldA = WorldA[i];
            Pair.WorldB = WorldB[BestJ];
            CellPairs.Add(Pair);
        }
    }

    UE_LOG(LogWaterProto, Warning,
        TEXT("[P-T4] CellPairs computed: %d pairs (A=%d cells, B=%d cells)"),
        CellPairs.Num(), IndicesA.Num(), IndicesB.Num());
}

void UDoorWaterBridge::TickSyncBoundary()
{
    if (CellPairs.Num() == 0 || !RendererA.IsValid() || !RendererB.IsValid())
    {
        return;
    }

    TArray<float>& HA = RendererA->MutableHeights();
    TArray<float>& HB = RendererB->MutableHeights();
    TArray<float>& VA = RendererA->MutableVelocities();
    TArray<float>& VB = RendererB->MutableVelocities();

    for (const FCellPair& Pair : CellPairs)
    {
        if (!HA.IsValidIndex(Pair.IdxA) || !HB.IsValidIndex(Pair.IdxB)) { continue; }

        // Heights : sync forte (continuité visuelle).
        const float AvgH = 0.5f * (HA[Pair.IdxA] + HB[Pair.IdxB]);
        HA[Pair.IdxA] = FMath::Lerp(HA[Pair.IdxA], AvgH, SyncStrengthHeights);
        HB[Pair.IdxB] = FMath::Lerp(HB[Pair.IdxB], AvgH, SyncStrengthHeights);

        // Velocities : sync faible + damping post-sync (anti-résonance R-6).
        const float AvgV = 0.5f * (VA[Pair.IdxA] + VB[Pair.IdxB]);
        VA[Pair.IdxA] = FMath::Lerp(VA[Pair.IdxA], AvgV, SyncStrengthVelocities) * VelocitiesPostSyncDamping;
        VB[Pair.IdxB] = FMath::Lerp(VB[Pair.IdxB], AvgV, SyncStrengthVelocities) * VelocitiesPostSyncDamping;
    }
}
