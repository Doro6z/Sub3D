#include "RoomWaterRenderer.h"

#include "Sub3DWaterProto.h"
#include "RoomWaterBakedData.h"
#include "RoomWaterDebugDrawer.h"
#include "Components/BoxComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "RHI.h"
#include "RenderingThread.h"

URoomWaterRenderer::URoomWaterRenderer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;

    // PAS de CreateDefaultSubobject ici : "Template Mismatch" sur attachement quand un BP dérive
    // de ARoomActor (URoomWaterRenderer est lui-même default subobject de ARoomActor, et UE ne
    // résout pas correctement la hiérarchie des sub-sub-objets sur l'instance BP).
    // Pattern correct : NewObject + RegisterComponent dans BeginPlay (cf. FloodWaterPlaneComponent).
}

void URoomWaterRenderer::EnsureMeshComponents()
{
    if (!CapMeshComp)
    {
        CapMeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("CapMesh"));
        // Movable AVANT SetupAttachment+RegisterComponent : la chaîne d'attachement Unreal
        // n'autorise pas un enfant Movable sous un parent Static. En portage Sub3D, le sub bouge
        // → tous les enfants doivent être Movable pour que la transform propagation fonctionne.
        CapMeshComp->SetMobility(EComponentMobility::Movable);
        CapMeshComp->SetupAttachment(this);
        CapMeshComp->RegisterComponent();
        CapMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CapMeshComp->bUseAsyncCooking = false;
    }
    if (!SkirtMeshComp)
    {
        SkirtMeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("SkirtMesh"));
        SkirtMeshComp->SetMobility(EComponentMobility::Movable);
        SkirtMeshComp->SetupAttachment(this);
        SkirtMeshComp->RegisterComponent();
        SkirtMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SkirtMeshComp->bUseAsyncCooking = false;
    }
}

void URoomWaterRenderer::BeginPlay()
{
    Super::BeginPlay();

    // Garde-fou : un BP enfant peut avoir sérialisé WaterRenderer comme top-level non attaché
    // (le constructeur C++ appelle SetupAttachment(CompartmentVolume), mais les BP créés avant
    // cette ligne — ou modifiés manuellement — gardent leur structure d'origine). Sans parent,
    // un USceneComponent flotte à world(RelativeLocation) au lieu de suivre l'Actor → cap mesh
    // à world(0,0,0). On corrige ici en ré-attachant à la racine de l'Actor.
    if (!GetAttachParent())
    {
        if (AActor* Owner = GetOwner())
        {
            if (USceneComponent* Root = Owner->GetRootComponent())
            {
                AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
                UE_LOG(LogWaterProto, Warning,
                    TEXT("URoomWaterRenderer (%s): was unparented in BP — auto-attached to root %s"),
                    *GetNameSafe(Owner), *GetNameSafe(Root));
            }
        }
    }

    EnsureMeshComponents();

    if (!SourceVolume.IsValid())
    {
        SourceVolume = Cast<UBoxComponent>(GetAttachParent());
    }

    LazyInitializeFromBakedData();
}

void URoomWaterRenderer::LazyInitializeFromBakedData()
{
    if (bInitialized)
    {
        return;
    }
    if (!BakedData)
    {
        return;
    }

    EnsureMeshComponents();

    const int32 Total = HeightfieldResolutionX * HeightfieldResolutionY;
    Heights.SetNumZeroed(Total);
    Velocities.SetNumZeroed(Total);

    HeightfieldTexture = UTexture2D::CreateTransient(
        HeightfieldResolutionX, HeightfieldResolutionY, PF_R32_FLOAT);
    if (HeightfieldTexture)
    {
        HeightfieldTexture->Filter = TF_Bilinear;
        HeightfieldTexture->AddressX = TA_Clamp;
        HeightfieldTexture->AddressY = TA_Clamp;
        HeightfieldTexture->UpdateResource();
    }

    UMaterialInterface* EffectiveCap = CapMaterial;
    if (!EffectiveCap)
    {
        EffectiveCap = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
        if (EffectiveCap)
        {
            UE_LOG(LogWaterProto, Display,
                TEXT("URoomWaterRenderer (%s): CapMaterial null, fallback WorldGridMaterial."),
                *GetNameSafe(GetOwner()));
        }
    }
    if (EffectiveCap && CapMeshComp)
    {
        CapMID = UMaterialInstanceDynamic::Create(EffectiveCap, this);
        if (CapMID)
        {
            if (HeightfieldTexture)
            {
                CapMID->SetTextureParameterValue(TEXT("HeightfieldTex"), HeightfieldTexture);
            }
            CapMID->SetVectorParameterValue(TEXT("LocalBoundsMin"), FLinearColor(BakedData->LocalBoundsMin));
            CapMID->SetVectorParameterValue(TEXT("LocalBoundsMax"), FLinearColor(BakedData->LocalBoundsMax));
            CapMeshComp->SetMaterial(0, CapMID);
        }
    }

    UMaterialInterface* EffectiveSkirt = SkirtMaterial;
    if (!EffectiveSkirt)
    {
        EffectiveSkirt = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
    }
    if (EffectiveSkirt && SkirtMeshComp)
    {
        SkirtMeshComp->SetMaterial(0, EffectiveSkirt);
    }

    CurrentSliceIndex = PickClosestSlice(CurrentWaterLevelLocalZ);
    RebuildBlendedCapMesh();
    BuildSkirtMeshOnce();
    UpdateSkirtScale();

    if (CapMeshComp)
    {
        CapMeshComp->SetRelativeLocation(FVector(0, 0, CurrentWaterLevelLocalZ));
    }

    bInitialized = true;

    UE_LOG(LogWaterProto, Display,
        TEXT("URoomWaterRenderer (%s): initialized | slices=%d | initialSlice=%d | initialZ=%.1f"),
        *GetNameSafe(GetOwner()), BakedData->Slices.Num(), CurrentSliceIndex, CurrentWaterLevelLocalZ);
}

void URoomWaterRenderer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialized)
    {
        LazyInitializeFromBakedData();
    }

    if (!bInitialized || Heights.Num() == 0)
    {
        return;
    }

    TickHeightfield(DeltaTime);
    PushHeightfieldToTexture();
}

void URoomWaterRenderer::SetWaterLevel(float NewLocalZ)
{
    CurrentWaterLevelLocalZ = NewLocalZ;

    if (!bInitialized)
    {
        LazyInitializeFromBakedData();
    }

    if (!BakedData)
    {
        return;
    }

    // Rebuild systématique chaque appel : la lerp entre slices encadrantes change avec NewLocalZ,
    // pas d'optimisation "même slice" comme avant. UpdateMeshSection est cheap (juste un buffer
    // de verts à uploader, pas de re-cook collision).
    CurrentSliceIndex = PickClosestSlice(NewLocalZ);
    RebuildBlendedCapMesh();

    if (CapMeshComp)
    {
        FVector RelLoc = CapMeshComp->GetRelativeLocation();
        RelLoc.Z = NewLocalZ;
        CapMeshComp->SetRelativeLocation(RelLoc);
    }

    UpdateSkirtScale();
}

int32 URoomWaterRenderer::PickClosestSlice(float WaterZ_Local) const
{
    if (!BakedData || BakedData->Slices.Num() == 0)
    {
        return -1;
    }

    int32 BestIdx = 0;
    float BestDist = TNumericLimits<float>::Max();
    for (int32 i = 0; i < BakedData->Slices.Num(); ++i)
    {
        const float D = FMath::Abs(BakedData->Slices[i].SliceZ_Local - WaterZ_Local);
        if (D < BestDist)
        {
            BestDist = D;
            BestIdx = i;
        }
    }
    return BestIdx;
}

void URoomWaterRenderer::FindBracketingSlices(float Z, int32& OutIdxBelow, int32& OutIdxAbove, float& OutT) const
{
    OutIdxBelow = OutIdxAbove = 0;
    OutT = 0.f;

    if (!BakedData || BakedData->Slices.Num() == 0)
    {
        return;
    }

    const TArray<FCompartmentSlice>& Slices = BakedData->Slices;
    const int32 N = Slices.Num();

    // Hors range bas : clamp slice 0.
    if (Z <= Slices[0].SliceZ_Local)
    {
        OutIdxBelow = OutIdxAbove = 0;
        OutT = 0.f;
        return;
    }
    // Hors range haut : clamp dernière slice.
    if (Z >= Slices[N - 1].SliceZ_Local)
    {
        OutIdxBelow = OutIdxAbove = N - 1;
        OutT = 0.f;
        return;
    }

    // Cherche [i, i+1] tel que Slices[i].Z <= Z <= Slices[i+1].Z. Linéaire suffisant pour
    // ~12 slices typiques ; si plus tard on en a beaucoup plus, passer en binary search.
    for (int32 i = 0; i < N - 1; ++i)
    {
        if (Z >= Slices[i].SliceZ_Local && Z <= Slices[i + 1].SliceZ_Local)
        {
            OutIdxBelow = i;
            OutIdxAbove = i + 1;
            const float Span = Slices[i + 1].SliceZ_Local - Slices[i].SliceZ_Local;
            OutT = (Span > KINDA_SMALL_NUMBER)
                ? (Z - Slices[i].SliceZ_Local) / Span
                : 0.f;
            return;
        }
    }
}

void URoomWaterRenderer::RebuildBlendedCapMesh()
{
    if (!BakedData || !CapMeshComp || BakedData->CapMeshesPerSlice.Num() == 0)
    {
        return;
    }

    int32 IdxBelow = 0, IdxAbove = 0;
    float T = 0.f;
    FindBracketingSlices(CurrentWaterLevelLocalZ, IdxBelow, IdxAbove, T);

    if (!BakedData->CapMeshesPerSlice.IsValidIndex(IdxBelow) ||
        !BakedData->CapMeshesPerSlice.IsValidIndex(IdxAbove))
    {
        return;
    }

    const FCachedWaterMesh& Below = BakedData->CapMeshesPerSlice[IdxBelow];
    const FCachedWaterMesh& Above = BakedData->CapMeshesPerSlice[IdxAbove];

    // Cas slices vides (haut/bas du compartiment, 0 verts) : on choisit la slice non-vide,
    // ou on clear si les deux sont vides.
    const bool bBelowEmpty = (Below.Vertices.Num() == 0);
    const bool bAboveEmpty = (Above.Vertices.Num() == 0);

    if (bBelowEmpty && bAboveEmpty)
    {
        if (bCapSectionCreated)
        {
            CapMeshComp->ClearAllMeshSections();
            bCapSectionCreated = false;
        }
        return;
    }

    // Une seule des deux non-vide → on prend celle-là sans blending (transitions aux bords du
    // compartiment). Pas idéal visuellement (saut sec sur 1 slice) mais cohérent avec un
    // compartiment fermé en haut/bas.
    const FCachedWaterMesh* SourceForTopology = nullptr;
    TArray<FVector> BlendedVerts;

    if (bBelowEmpty)
    {
        SourceForTopology = &Above;
        BlendedVerts = Above.Vertices;
    }
    else if (bAboveEmpty)
    {
        SourceForTopology = &Below;
        BlendedVerts = Below.Vertices;
    }
    else if (Below.Vertices.Num() == Above.Vertices.Num())
    {
        // Cas nominal : topologie identique (garantie par le resampling au bake).
        SourceForTopology = &Below;
        const int32 NumVerts = Below.Vertices.Num();
        BlendedVerts.SetNumUninitialized(NumVerts);
        for (int32 i = 0; i < NumVerts; ++i)
        {
            BlendedVerts[i] = FMath::Lerp(Below.Vertices[i], Above.Vertices[i], T);
        }
    }
    else
    {
        // Mismatch (ne devrait pas arriver après le bake refactor) : fallback sur la plus
        // proche. Log une fois par session pour signaler une bake stale.
        UE_LOG(LogWaterProto, Warning,
            TEXT("RebuildBlendedCapMesh: vertex count mismatch (below=%d above=%d) — re-bake required after switch to fan/resample"),
            Below.Vertices.Num(), Above.Vertices.Num());
        SourceForTopology = (T < 0.5f) ? &Below : &Above;
        BlendedVerts = SourceForTopology->Vertices;
    }

    if (!SourceForTopology || SourceForTopology->Triangles.Num() == 0)
    {
        return;
    }

    // Première frame : CreateMeshSection (alloue le buffer + crée la section).
    // Frames suivantes : UpdateMeshSection (re-upload juste les verts, pas de re-cook).
    if (!bCapSectionCreated)
    {
        CapMeshComp->CreateMeshSection(
            /*SectionIndex*/ 0,
            BlendedVerts,
            SourceForTopology->Triangles,
            SourceForTopology->Normals,
            SourceForTopology->UV0,
            TArray<FColor>(),
            TArray<FProcMeshTangent>(),
            /*bCreateCollision*/ false);
        bCapSectionCreated = true;
    }
    else
    {
        CapMeshComp->UpdateMeshSection(
            /*SectionIndex*/ 0,
            BlendedVerts,
            SourceForTopology->Normals,
            SourceForTopology->UV0,
            TArray<FColor>(),
            TArray<FProcMeshTangent>());
    }
}

void URoomWaterRenderer::BuildSkirtMeshOnce()
{
    if (!BakedData || bSkirtBuilt || !SkirtMeshComp)
    {
        return;
    }

    const int32 N = BakedData->OpeningSegmentStarts.Num();
    if (N == 0 || BakedData->OpeningSegmentEnds.Num() != N)
    {
        bSkirtBuilt = true;
        return;
    }

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    Vertices.Reserve(N * 4);
    Triangles.Reserve(N * 6);
    Normals.Reserve(N * 4);
    UVs.Reserve(N * 4);

    int32 BaseIdx = 0;
    for (int32 i = 0; i < N; ++i)
    {
        const FVector2D& A2D = BakedData->OpeningSegmentStarts[i];
        const FVector2D& B2D = BakedData->OpeningSegmentEnds[i];

        Vertices.Add(FVector(A2D.X, A2D.Y, 1.0f));
        Vertices.Add(FVector(B2D.X, B2D.Y, 1.0f));
        Vertices.Add(FVector(B2D.X, B2D.Y, 0.0f));
        Vertices.Add(FVector(A2D.X, A2D.Y, 0.0f));

        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 2);
        Triangles.Add(BaseIdx + 3);

        const FVector2D Edge = B2D - A2D;
        const FVector Normal = FVector(-Edge.Y, Edge.X, 0.f).GetSafeNormal();
        for (int32 k = 0; k < 4; ++k)
        {
            Normals.Add(Normal);
        }

        UVs.Add(FVector2D(0, 0));
        UVs.Add(FVector2D(1, 0));
        UVs.Add(FVector2D(1, 1));
        UVs.Add(FVector2D(0, 1));

        BaseIdx += 4;
    }

    SkirtMeshComp->CreateMeshSection(
        0, Vertices, Triangles, Normals, UVs,
        TArray<FColor>(), TArray<FProcMeshTangent>(), false);

    const float FloorZ = BakedData->LocalBoundsMin.Z;
    SkirtMeshComp->SetRelativeLocation(FVector(0, 0, FloorZ));

    bSkirtBuilt = true;
}

void URoomWaterRenderer::UpdateSkirtScale()
{
    if (!bSkirtBuilt || !BakedData || !SkirtMeshComp)
    {
        return;
    }

    const float FloorZ = BakedData->LocalBoundsMin.Z;
    const float WaterZ = CurrentWaterLevelLocalZ;
    const float Height = FMath::Max(0.f, WaterZ - FloorZ);

    SkirtMeshComp->SetRelativeScale3D(FVector(1.f, 1.f, Height));
    SkirtMeshComp->SetVisibility(Height > 0.01f);
}

void URoomWaterRenderer::TickHeightfield(float dt)
{
    const int32 W = HeightfieldResolutionX;
    const int32 H = HeightfieldResolutionY;

    TArray<float> NewHeights;
    NewHeights.SetNumZeroed(W * H);

    for (int32 y = 1; y < H - 1; ++y)
    {
        for (int32 x = 1; x < W - 1; ++x)
        {
            const int32 idx = y * W + x;
            const float h_center = Heights[idx];
            const float h_avg = (Heights[idx - 1] + Heights[idx + 1] +
                                 Heights[idx - W] + Heights[idx + W]) * 0.25f;
            const float laplacian = h_avg - h_center;

            Velocities[idx] += laplacian * WaveSpeed * dt;
            Velocities[idx] *= Damping;
            NewHeights[idx] = h_center + Velocities[idx] * dt;
        }
    }

    Heights = NewHeights;
}

void URoomWaterRenderer::PushHeightfieldToTexture()
{
    if (!HeightfieldTexture)
    {
        return;
    }

    struct FUpdateData
    {
        FUpdateTextureRegion2D Region;
        uint32 SrcPitch = 0;
        TArray<float> Buffer;
    };

    FUpdateData* Data = new FUpdateData();
    Data->Region = FUpdateTextureRegion2D(0, 0, 0, 0, HeightfieldResolutionX, HeightfieldResolutionY);
    Data->SrcPitch = HeightfieldResolutionX * sizeof(float);
    Data->Buffer = Heights;

    HeightfieldTexture->UpdateTextureRegions(
        /*MipIndex*/   0,
        /*NumRegions*/ 1,
        /*Regions*/    &Data->Region,
        /*SrcPitch*/   Data->SrcPitch,
        /*SrcBpp*/     sizeof(float),
        /*SrcData*/    reinterpret_cast<uint8*>(Data->Buffer.GetData()),
        /*Cleanup*/    [Data](uint8*, const FUpdateTextureRegion2D*) { delete Data; });
}

void URoomWaterRenderer::InjectAt(FVector2D LocalPosXY, float Force, float Radius)
{
    if (!BakedData || Heights.Num() == 0)
    {
        return;
    }

    // Trace debug si SourceVolume disponible.
    if (UBoxComponent* Vol = SourceVolume.Get())
    {
        const FVector LocalPos3D(LocalPosXY.X, LocalPosXY.Y, CurrentWaterLevelLocalZ);
        const FVector WorldPos = Vol->GetComponentTransform().TransformPosition(LocalPos3D);
        URoomWaterDebugDrawer::MarkInjection(this, WorldPos, Force, Radius);
    }

    const int32 W = HeightfieldResolutionX;
    const int32 H = HeightfieldResolutionY;

    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;

    const float SpanX = Max.X - Min.X;
    const float SpanY = Max.Y - Min.Y;
    if (SpanX <= 0.f || SpanY <= 0.f)
    {
        return;
    }

    const float u = (LocalPosXY.X - Min.X) / SpanX;
    const float v = (LocalPosXY.Y - Min.Y) / SpanY;

    const int32 cx = FMath::Clamp(static_cast<int32>(u * W), 0, W - 1);
    const int32 cy = FMath::Clamp(static_cast<int32>(v * H), 0, H - 1);

    const float CellWorldSize = SpanX / W;
    const int32 RadiusCells = FMath::Max(1, FMath::CeilToInt(Radius / CellWorldSize));

    for (int32 dy = -RadiusCells; dy <= RadiusCells; ++dy)
    {
        for (int32 dx = -RadiusCells; dx <= RadiusCells; ++dx)
        {
            const int32 nx = cx + dx;
            const int32 ny = cy + dy;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
            {
                continue;
            }

            const float dist = FMath::Sqrt(static_cast<float>(dx * dx + dy * dy));
            const float falloff = FMath::Max(0.f, 1.0f - dist / RadiusCells);

            Heights[ny * W + nx] += Force * falloff;
        }
    }
}

bool URoomWaterRenderer::InjectAtWorldPoint(FVector WorldPos, float Force, float Radius)
{
    if (!BakedData)
    {
        return false;
    }

    // Conversion world → local du compartiment via la transform du component (le renderer
    // est attaché à CompartmentVolume → sa transform = celle du Box). Cohérent avec ce que
    // fait UDoorWaterBridge::OnDoorStateChanged.
    const FVector LocalPos3D = GetComponentTransform().InverseTransformPosition(WorldPos);

    // Filtre out-of-bounds : évite de polluer le heightfield avec un click hors du compartiment.
    // Tolérance Radius pour permettre les clicks proches du bord.
    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;
    if (LocalPos3D.X < Min.X - Radius || LocalPos3D.X > Max.X + Radius ||
        LocalPos3D.Y < Min.Y - Radius || LocalPos3D.Y > Max.Y + Radius)
    {
        return false;
    }

    InjectAt(FVector2D(LocalPos3D.X, LocalPos3D.Y), Force, Radius);
    return true;
}

void URoomWaterRenderer::ResetHeightfield()
{
    FMemory::Memzero(Heights.GetData(), Heights.Num() * sizeof(float));
    FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(float));
}

void URoomWaterRenderer::DrawDebugSnapshot()
{
    UBoxComponent* Volume = SourceVolume.Get();
    if (!Volume)
    {
        Volume = Cast<UBoxComponent>(GetAttachParent());
    }
    if (!Volume)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("DrawDebugSnapshot: SourceVolume non résolu"));
        return;
    }
    if (!BakedData)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("DrawDebugSnapshot: BakedData manquant"));
        return;
    }

    // Bornes Box (jaune).
    URoomWaterDebugDrawer::DrawBoxBounds(this, Volume, DebugDrawDuration);

    // Détermine quelles slices afficher : isolated si >= 0 et valide, sinon toutes.
    TArray<int32> SlicesToDraw;
    const bool bIsolated = (DebugIsolateSliceIndex >= 0) && BakedData->Slices.IsValidIndex(DebugIsolateSliceIndex);
    if (bIsolated)
    {
        SlicesToDraw.Add(DebugIsolateSliceIndex);
    }
    else
    {
        for (int32 i = 0; i < BakedData->Slices.Num(); ++i)
        {
            SlicesToDraw.Add(i);
        }
    }

    // Pour chaque slice : gradient SDF ou binary (selon mode), + contour détaillé optionnel.
    for (int32 SliceIdx : SlicesToDraw)
    {
        if (bDebugShowSDFGradient)
        {
            URoomWaterDebugDrawer::DrawSliceSDFGradient(
                this, Volume, BakedData, SliceIdx,
                DebugSDFMaxDistance, DebugDrawDuration,
                /*bShowValues=*/ bIsolated);
        }
        else
        {
            URoomWaterDebugDrawer::DrawSliceDetailed(
                this, Volume, BakedData, SliceIdx,
                DebugDrawDuration, bDebugIncludeMaskMisses);
        }

        if (bDebugShowContourDetail)
        {
            URoomWaterDebugDrawer::DrawSliceContourDetailed(
                this, Volume, BakedData, SliceIdx,
                DebugDrawDuration,
                /*bShowTValues=*/ bIsolated);
        }
    }

    URoomWaterDebugDrawer::DumpBakedDataToLog(BakedData);
}
