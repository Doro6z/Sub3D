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
        CapMeshComp->SetupAttachment(this);
        CapMeshComp->RegisterComponent();
        CapMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CapMeshComp->bUseAsyncCooking = false;
    }
    if (!SkirtMeshComp)
    {
        SkirtMeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("SkirtMesh"));
        SkirtMeshComp->SetupAttachment(this);
        SkirtMeshComp->RegisterComponent();
        SkirtMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SkirtMeshComp->bUseAsyncCooking = false;
    }
}

void URoomWaterRenderer::BeginPlay()
{
    Super::BeginPlay();

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
    RegenerateCapMesh();
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

    const int32 NewSliceIdx = PickClosestSlice(NewLocalZ);
    if (NewSliceIdx != CurrentSliceIndex)
    {
        CurrentSliceIndex = NewSliceIdx;
        RegenerateCapMesh();
    }

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

void URoomWaterRenderer::RegenerateCapMesh()
{
    if (!BakedData || CurrentSliceIndex < 0 || !BakedData->CapMeshesPerSlice.IsValidIndex(CurrentSliceIndex))
    {
        if (CapMeshComp)
        {
            CapMeshComp->ClearAllMeshSections();
        }
        return;
    }

    const FCachedWaterMesh& Tpl = BakedData->CapMeshesPerSlice[CurrentSliceIndex];

    CapMeshComp->ClearAllMeshSections();
    if (Tpl.Vertices.Num() == 0 || Tpl.Triangles.Num() == 0)
    {
        return;
    }

    CapMeshComp->CreateMeshSection(
        /*SectionIndex*/ 0,
        Tpl.Vertices, Tpl.Triangles, Tpl.Normals, Tpl.UV0,
        TArray<FColor>(), TArray<FProcMeshTangent>(),
        /*bCreateCollision*/ false);
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

    URoomWaterDebugDrawer::DrawCompartmentSnapshot(
        this, Volume, BakedData, CurrentWaterLevelLocalZ,
        DebugDrawDuration, bDebugIncludeMaskMisses);

    URoomWaterDebugDrawer::DumpBakedDataToLog(BakedData);
}
