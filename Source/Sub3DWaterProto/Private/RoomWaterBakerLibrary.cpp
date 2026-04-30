#include "RoomWaterBakerLibrary.h"

#include "Sub3DWaterProto.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "RoomWaterBakedData.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace
{
    // ─────────────────────────────────────────────────────────────────────────
    // Marching Squares
    //
    // Production : "soupe" de segments (paires de FVector2D consécutives), pas un polygone
    // ordonné. Suffit pour DetectOpenings (test de bord) et pour visualisation debug.
    // Pour une triangulation propre du cap, on garde Delaunay/ear-clipping comme amélioration
    // ultérieure — pour le proto on génère le cap par grille de quads (option B, cf. doc §4.3.3).
    // ─────────────────────────────────────────────────────────────────────────
    TArray<FVector2D> ExtractContourMarchingSquares(
        const TArray<float>& SDF, int32 W, int32 H, float CellSize, const FVector& LocalMin)
    {
        TArray<FVector2D> Contour;
        Contour.Reserve(W * H);

        // Phase A : on lit le SDF en binaire (SDF < 0 = inside) pour conserver le contour
        // stair-step en attendant Phase B (Marching Squares interpolé).
        for (int32 y = 0; y < H - 1; ++y)
        {
            for (int32 x = 0; x < W - 1; ++x)
            {
                const uint8 tl = (SDF[y * W + x] < 0.f) ? 1 : 0;
                const uint8 tr = (SDF[y * W + (x + 1)] < 0.f) ? 1 : 0;
                const uint8 bl = (SDF[(y + 1) * W + x] < 0.f) ? 1 : 0;
                const uint8 br = (SDF[(y + 1) * W + (x + 1)] < 0.f) ? 1 : 0;

                const int32 caseIdx = (tl ? 8 : 0) | (tr ? 4 : 0) | (br ? 2 : 0) | (bl ? 1 : 0);

                const FVector2D pTop   ((x + 0.5f) * CellSize + LocalMin.X, y * CellSize + LocalMin.Y);
                const FVector2D pRight ((x + 1.0f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
                const FVector2D pBottom((x + 0.5f) * CellSize + LocalMin.X, (y + 1.0f) * CellSize + LocalMin.Y);
                const FVector2D pLeft  (x * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);

                switch (caseIdx)
                {
                case 0: case 15: break;
                case 1: case 14: Contour.Add(pLeft);   Contour.Add(pBottom); break;
                case 2: case 13: Contour.Add(pBottom); Contour.Add(pRight);  break;
                case 3: case 12: Contour.Add(pLeft);   Contour.Add(pRight);  break;
                case 4: case 11: Contour.Add(pTop);    Contour.Add(pRight);  break;
                case 5:          Contour.Add(pLeft);   Contour.Add(pTop);
                                 Contour.Add(pBottom); Contour.Add(pRight); break;
                case 6: case 9:  Contour.Add(pTop);    Contour.Add(pBottom); break;
                case 7: case 8:  Contour.Add(pLeft);   Contour.Add(pTop);    break;
                case 10:         Contour.Add(pLeft);   Contour.Add(pBottom);
                                 Contour.Add(pTop);    Contour.Add(pRight); break;
                }
            }
        }

        return Contour;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Cap mesh — option B (Phase A) : un quad par cellule "intérieur" (SDF < 0).
    //
    // Stair-stepped au bord (résolution = CellSize). À remplacer en Phase B par triangulation
    // Delaunay du contour MS interpolé pour un bord sub-cellulaire.
    // Vertices stockés à Z=0 ; le runtime translate via SetRelativeLocation.
    // ─────────────────────────────────────────────────────────────────────────
    FCachedWaterMesh GenerateCapMeshFromSDFBinary(
        const TArray<float>& SDF, int32 W, int32 H,
        float CellSize, const FVector& LocalMin, const FVector& LocalMax)
    {
        FCachedWaterMesh Mesh;

        const float SpanX = LocalMax.X - LocalMin.X;
        const float SpanY = LocalMax.Y - LocalMin.Y;
        if (SpanX <= 0.f || SpanY <= 0.f)
        {
            return Mesh;
        }

        // Préallocation grossière (worst case = grille pleine).
        Mesh.Vertices.Reserve(W * H * 4);
        Mesh.Triangles.Reserve(W * H * 6);
        Mesh.Normals.Reserve(W * H * 4);
        Mesh.UV0.Reserve(W * H * 4);

        int32 BaseIdx = 0;
        for (int32 cy = 0; cy < H; ++cy)
        {
            for (int32 cx = 0; cx < W; ++cx)
            {
                if (SDF[cy * W + cx] >= 0.f)
                {
                    continue;
                }

                const float x0 = LocalMin.X + cx * CellSize;
                const float x1 = LocalMin.X + (cx + 1) * CellSize;
                const float y0 = LocalMin.Y + cy * CellSize;
                const float y1 = LocalMin.Y + (cy + 1) * CellSize;

                Mesh.Vertices.Add(FVector(x0, y0, 0.f));
                Mesh.Vertices.Add(FVector(x1, y0, 0.f));
                Mesh.Vertices.Add(FVector(x1, y1, 0.f));
                Mesh.Vertices.Add(FVector(x0, y1, 0.f));

                for (int32 k = 0; k < 4; ++k)
                {
                    Mesh.Normals.Add(FVector(0.f, 0.f, 1.f));
                }

                Mesh.UV0.Add(FVector2D((x0 - LocalMin.X) / SpanX, (y0 - LocalMin.Y) / SpanY));
                Mesh.UV0.Add(FVector2D((x1 - LocalMin.X) / SpanX, (y0 - LocalMin.Y) / SpanY));
                Mesh.UV0.Add(FVector2D((x1 - LocalMin.X) / SpanX, (y1 - LocalMin.Y) / SpanY));
                Mesh.UV0.Add(FVector2D((x0 - LocalMin.X) / SpanX, (y1 - LocalMin.Y) / SpanY));

                Mesh.Triangles.Add(BaseIdx + 0);
                Mesh.Triangles.Add(BaseIdx + 1);
                Mesh.Triangles.Add(BaseIdx + 2);
                Mesh.Triangles.Add(BaseIdx + 0);
                Mesh.Triangles.Add(BaseIdx + 2);
                Mesh.Triangles.Add(BaseIdx + 3);

                BaseIdx += 4;
            }
        }

        Mesh.Vertices.Shrink();
        Mesh.Triangles.Shrink();
        Mesh.Normals.Shrink();
        Mesh.UV0.Shrink();

        return Mesh;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Détection des ouvertures pour le skirt.
    //
    // Heuristique simple : segments du contour (slice à mi-hauteur) dont le milieu est proche
    // d'un bord du Box. Suffit pour le proto avec une ouverture rectangulaire authored. Pour
    // la prod, hooker sur les vrais ASubDoorActor (CompartmentA/B) plutôt qu'inférer.
    // ─────────────────────────────────────────────────────────────────────────
    void DetectOpenings(URoomWaterBakedData* Data, float BorderToleranceCm = 30.f)
    {
        if (!Data || Data->Slices.Num() == 0)
        {
            return;
        }

        const int32 MidIdx = Data->Slices.Num() / 2;
        const FCompartmentSlice& MidSlice = Data->Slices[MidIdx];

        for (int32 i = 0; i + 1 < MidSlice.ContourPolygon.Num(); i += 2)
        {
            const FVector2D& A = MidSlice.ContourPolygon[i];
            const FVector2D& B = MidSlice.ContourPolygon[i + 1];
            const FVector2D Mid = (A + B) * 0.5f;

            const bool bNearBorder =
                FMath::Abs(Mid.X - Data->LocalBoundsMin.X) < BorderToleranceCm ||
                FMath::Abs(Mid.X - Data->LocalBoundsMax.X) < BorderToleranceCm ||
                FMath::Abs(Mid.Y - Data->LocalBoundsMin.Y) < BorderToleranceCm ||
                FMath::Abs(Mid.Y - Data->LocalBoundsMax.Y) < BorderToleranceCm;

            if (bNearBorder)
            {
                Data->OpeningSegmentStarts.Add(A);
                Data->OpeningSegmentEnds.Add(B);
            }
        }
    }
} // namespace anonyme

// ─────────────────────────────────────────────────────────────────────────────
// BakeVolume — entry point principal.
// ─────────────────────────────────────────────────────────────────────────────

URoomWaterBakedData* URoomWaterBakerLibrary::BakeVolume(
    UBoxComponent* Volume,
    FName CompartmentId,
    int32 NumSlices,
    float CellSize,
    bool bAutoDetectOpenings)
{
    if (!Volume)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeVolume: Volume null"));
        return nullptr;
    }

    if (NumSlices < 2)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeVolume: NumSlices=%d trop petit, clamp à 2"), NumSlices);
        NumSlices = 2;
    }
    if (CellSize <= 0.f)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeVolume: CellSize=%f invalide, clamp à 25"), CellSize);
        CellSize = 25.f;
    }

    UWorld* World = Volume->GetWorld();
    if (!World)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeVolume: pas de World"));
        return nullptr;
    }

    const FVector BoxExtent = Volume->GetUnscaledBoxExtent();
    const FVector BoxLocalMin = -BoxExtent;
    const FVector BoxLocalMax = BoxExtent;

    const int32 GridW = FMath::Max(2, FMath::CeilToInt((BoxLocalMax.X - BoxLocalMin.X) / CellSize));
    const int32 GridH = FMath::Max(2, FMath::CeilToInt((BoxLocalMax.Y - BoxLocalMin.Y) / CellSize));

    URoomWaterBakedData* Data = NewObject<URoomWaterBakedData>(GetTransientPackage());
    if (!Data)
    {
        return nullptr;
    }
    Data->SourceRoomId = CompartmentId;
    Data->LocalBoundsMin = BoxLocalMin;
    Data->LocalBoundsMax = BoxLocalMax;

    const FTransform VolumeXf = Volume->GetComponentTransform();

    // ─── Voxelisation par slice — Signed Distance Field (SDF) 2D ─────────
    //
    // Pour chaque cellule, deux étapes :
    //   1) Sign : majority parity vote sur 6 directions (cardinaux ±X,±Y + verticaux ±Z).
    //      Une cellule est inside si ≥4 des 6 rayons ont parité impaire (= rayon traverse
    //      un nombre impair de surfaces blocantes avant de s'échapper). Plus robuste qu'un
    //      seul +Z parity face aux meshes avec collision incomplète (plancher manquant,
    //      triangles single-side, etc.).
    //   2) Magnitude : min hit distance sur 8 directions XY (4 cardinaux + 4 diagonaux),
    //      restreinte au plan de la slice pour cohérence avec MS 2D. Les 4 cardinaux XY
    //      sont récupérés gratuitement depuis les LineTraceMulti des 6 directions de signe
    //      (Hits[0].Distance = nearest surface in that direction). Plus 4 diagonales en
    //      LineTraceSingle.
    //
    // Convention SDF : SDF[cell] = bIsInside ? -MinDist : +MinDist.
    //
    // Coût total : 6 LineTraceMulti + 4 LineTraceSingle = 10 raycasts/cellule.
    // bTraceComplex=true → utilise les triangles du static mesh.
    const ECollisionChannel BakeChannel = ECC_WorldStatic;
    constexpr float SDF_MaxDistanceCm = 1000.f;       // 10m — saturation max pour magnitude
    constexpr float SDF_ParityRayLength = 100000.f;   // 1km — escape garantie pour parity
    constexpr float Diag = 0.70710678f;               // sqrt(2)/2

    // 6 directions : sign (parity vote) + magnitude pour les 4 cardinaux XY (d < 4).
    const FVector LocalDirs6[6] = {
        FVector(+1.f, 0.f, 0.f),
        FVector(-1.f, 0.f, 0.f),
        FVector(0.f, +1.f, 0.f),
        FVector(0.f, -1.f, 0.f),
        FVector(0.f, 0.f, +1.f),
        FVector(0.f, 0.f, -1.f),
    };

    // 4 diagonales XY : magnitude uniquement (le plan de la slice pour MS 2D).
    const FVector LocalDirsDiag[4] = {
        FVector(+Diag, +Diag, 0.f),
        FVector(-Diag, -Diag, 0.f),
        FVector(+Diag, -Diag, 0.f),
        FVector(-Diag, +Diag, 0.f),
    };

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WaterBakeSDF), /*bTraceComplex*/ true);

    Data->Slices.Reserve(NumSlices);

    int32 TotalProbeCount = 0;
    for (int32 sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
    {
        const float t = static_cast<float>(sliceIdx) / static_cast<float>(NumSlices - 1);
        const float SliceZ_Local = FMath::Lerp(BoxLocalMin.Z, BoxLocalMax.Z, t);

        FCompartmentSlice Slice;
        Slice.SliceZ_Local = SliceZ_Local;
        Slice.GridWidth = GridW;
        Slice.GridHeight = GridH;
        Slice.SignedDistance.SetNumZeroed(GridW * GridH);

        for (int32 y = 0; y < GridH; ++y)
        {
            for (int32 x = 0; x < GridW; ++x)
            {
                const FVector LocalPos(
                    BoxLocalMin.X + (x + 0.5f) * CellSize,
                    BoxLocalMin.Y + (y + 0.5f) * CellSize,
                    SliceZ_Local);
                const FVector WorldPos = VolumeXf.TransformPosition(LocalPos);

                // ── 6 LineTraceSingle : test "all 6 hit" (signe) + magnitude des 4 cardinaux XY (d<4) ──
                //
                // Test simple : la cellule est inside ssi LES 6 rayons touchent une surface.
                // Pas de parity (faillible avec multi-shell submarine), juste "ai-je quelque chose
                // de blocant dans toutes les 6 directions". Robuste pour mesh fermé typique.
                //
                // Hit.Distance = distance au PREMIER hit (LineTraceSingle), utilisée pour magnitude
                // sur les 4 directions XY cardinales (les verticales +Z/-Z ne contribuent pas — on
                // veut la distance in-plane à un mur, pas au plafond/plancher).
                int32 HitCount = 0;
                float MinDist = SDF_MaxDistanceCm;

                for (int32 d = 0; d < 6; ++d)
                {
                    const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirs6[d]);
                    FHitResult Hit;
                    if (World->LineTraceSingleByChannel(
                            Hit,
                            WorldPos,
                            WorldPos + DirWorld * SDF_ParityRayLength,
                            BakeChannel,
                            TraceParams))
                    {
                        ++HitCount;

                        // Cardinaux XY (d=0..3) contribuent à la magnitude in-plane.
                        if (d < 4)
                        {
                            const float HitDist = FMath::Min(Hit.Distance, SDF_MaxDistanceCm);
                            if (HitDist < MinDist)
                            {
                                MinDist = HitDist;
                            }
                        }
                    }
                }

                // ── 4 LineTraceSingle XY diagonaux : magnitude uniquement ──────────
                for (int32 d = 0; d < 4; ++d)
                {
                    const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirsDiag[d]);
                    FHitResult Hit;
                    if (World->LineTraceSingleByChannel(
                            Hit,
                            WorldPos,
                            WorldPos + DirWorld * SDF_MaxDistanceCm,
                            BakeChannel,
                            TraceParams))
                    {
                        if (Hit.Distance < MinDist)
                        {
                            MinDist = Hit.Distance;
                        }
                    }
                }

                // Strict enclosed test : 6/6 directions doivent toucher → inside.
                const bool bIsInside = (HitCount == 6);
                Slice.SignedDistance[y * GridW + x] = bIsInside ? -MinDist : +MinDist;
                ++TotalProbeCount;
            }
        }

        Slice.ContourPolygon = ExtractContourMarchingSquares(Slice.SignedDistance, GridW, GridH, CellSize, BoxLocalMin);
        Data->Slices.Add(MoveTemp(Slice));
    }

    // ─── Cap meshes par slice (Phase A : binary threshold sur SDF) ───────
    Data->CapMeshesPerSlice.Reserve(Data->Slices.Num());
    int32 EmptyCount = 0;
    for (const FCompartmentSlice& Slice : Data->Slices)
    {
        FCachedWaterMesh Mesh = GenerateCapMeshFromSDFBinary(Slice.SignedDistance, GridW, GridH, CellSize, BoxLocalMin, BoxLocalMax);
        if (Mesh.Vertices.Num() == 0)
        {
            ++EmptyCount;
        }
        Data->CapMeshesPerSlice.Add(MoveTemp(Mesh));
    }

    if (bAutoDetectOpenings)
    {
        DetectOpenings(Data);
    }

    UE_LOG(LogWaterProto, Display,
        TEXT("BakeVolume: id=%s | grid=%dx%d (cell=%.0fcm) | slices=%d | probes=%d | empty=%d | openings=%d (auto=%s)"),
        *CompartmentId.ToString(), GridW, GridH, CellSize, NumSlices, TotalProbeCount, EmptyCount,
        Data->OpeningSegmentStarts.Num(), bAutoDetectOpenings ? TEXT("true") : TEXT("false"));

    return Data;
}

// ─────────────────────────────────────────────────────────────────────────────
// BakeAndSave — bake + sauvegarde dans un asset persistant.
// ─────────────────────────────────────────────────────────────────────────────

URoomWaterBakedData* URoomWaterBakerLibrary::BakeAndSave(
    UBoxComponent* Volume,
    FName CompartmentId,
    const FString& PackagePath,
    int32 NumSlices,
    float CellSize,
    bool bAutoDetectOpenings)
{
#if WITH_EDITOR
    URoomWaterBakedData* Transient = BakeVolume(Volume, CompartmentId, NumSlices, CellSize, bAutoDetectOpenings);
    if (!Transient)
    {
        return nullptr;
    }

    FString Folder = PackagePath;
    if (!Folder.EndsWith(TEXT("/")))
    {
        Folder += TEXT("/");
    }

    const FString AssetName = FString::Printf(TEXT("BD_%s"), *CompartmentId.ToString());
    const FString FullPackagePath = Folder + AssetName;

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeAndSave: CreatePackage failed for %s"), *FullPackagePath);
        return Transient;
    }
    Package->FullyLoad();

    // Si un asset existe déjà au même path, on l'écrase (overwrite).
    URoomWaterBakedData* Asset = NewObject<URoomWaterBakedData>(
        Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    if (!Asset)
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeAndSave: NewObject<URoomWaterBakedData> failed"));
        return Transient;
    }

    // Copy depuis le transient.
    Asset->SourceRoomId = Transient->SourceRoomId;
    Asset->LocalBoundsMin = Transient->LocalBoundsMin;
    Asset->LocalBoundsMax = Transient->LocalBoundsMax;
    Asset->Slices = Transient->Slices;
    Asset->CapMeshesPerSlice = Transient->CapMeshesPerSlice;
    Asset->OpeningSegmentStarts = Transient->OpeningSegmentStarts;
    Asset->OpeningSegmentEnds = Transient->OpeningSegmentEnds;

    FAssetRegistryModule::AssetCreated(Asset);
    Package->MarkPackageDirty();

    const FString FileName = FPackageName::LongPackageNameToFilename(
        FullPackagePath, FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const FSavePackageResultStruct SaveResult = UPackage::Save(Package, Asset, *FileName, SaveArgs);

    if (!SaveResult.IsSuccessful())
    {
        UE_LOG(LogWaterProto, Warning, TEXT("BakeAndSave: UPackage::Save failed for %s"), *FullPackagePath);
        return Asset; // l'asset existe en mémoire même si pas sauvegardé
    }

    UE_LOG(LogWaterProto, Display, TEXT("BakeAndSave: saved %s"), *FullPackagePath);
    return Asset;
#else
    UE_LOG(LogWaterProto, Warning, TEXT("BakeAndSave: editor-only, ignoré dans une build runtime"));
    return BakeVolume(Volume, CompartmentId, NumSlices, CellSize, bAutoDetectOpenings);
#endif
}
