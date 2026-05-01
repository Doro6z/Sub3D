#include "RoomWaterBakerLibrary.h"

#include "Sub3DWaterProto.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "RoomWaterBakedData.h"
#include "Algo/Reverse.h"
#include "ConstrainedDelaunay2.h"
#include "IndexTypes.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

namespace
{
    // ─────────────────────────────────────────────────────────────────────────
    // Marching Squares INTERPOLÉ par RAYCAST RÉEL (Phase B v2).
    //
    // Pour chaque 2x2 de cellules, on lit les 4 valeurs SDF aux 4 cell centers (pTL/TR/BL/BR).
    // Convention : SDF négatif = intérieur. Le contour est placé exactement à la position du mur,
    // déterminée par un RAYCAST RÉEL depuis le cell-center intérieur vers le cell-center
    // extérieur (au lieu d'une interpolation linéaire à partir des magnitudes SDF).
    //
    // Pourquoi raycast plutôt que `t = SDF_a / (SDF_a - SDF_b)` :
    // Le SDF stocké est `min(distance) sur 8 directions XY` — un scalaire qui peut représenter
    // la distance à n'importe quel mur, pas forcément celui dans la direction de l'arête de
    // cellule où on interpole. Conséquence : interpolation biaisée → contour bowed-inward sur
    // les arêtes de cellules dont la SDF magnitude vient d'une perpendiculaire éloignée.
    // Raycast réel = position exacte du mur sur cette arête, indépendant des magnitudes.
    //
    // Coût : 1 raycast par changement de signe (typiquement 2-4 par 2x2 cell, mais seulement
    // pour les cellules de bord). ~100-200 raycasts/slice. +~5% temps de bake. Négligeable.
    //
    // Production : "soupe" de segments (paires de FVector2D consécutives). Chaining → polygone
    // est fait par ChainSegmentsIntoPolygon.
    // ─────────────────────────────────────────────────────────────────────────
    TArray<FVector2D> ExtractContourMarchingSquaresInterpolated(
        const TArray<float>& SDF, int32 W, int32 H, float CellSize, const FVector& LocalMin,
        UWorld* World, const FTransform& VolumeXf, ECollisionChannel BakeChannel,
        const FCollisionQueryParams& TraceParams, float SliceZ_Local)
    {
        TArray<FVector2D> Contour;
        Contour.Reserve(W * H);

        // Raycast réel sur une arête de cellule pour trouver la position exacte du mur.
        // Cast depuis le cell-center intérieur (SDF<0) vers le cell-center extérieur (SDF>0)
        // pour obtenir la surface intérieure du mur (côté salle).
        auto RaycastEdge = [&](float sdfA, float sdfB, const FVector2D& pa, const FVector2D& pb) -> FVector2D
        {
            // Cas dégénéré : signes identiques → fallback midpoint (ne devrait pas arriver dans
            // les branches du switch, mais robustesse numérique).
            if (FMath::Sign(sdfA) == FMath::Sign(sdfB))
            {
                return (pa + pb) * 0.5f;
            }

            // Détermine sens : depuis intérieur (négatif) vers extérieur (positif).
            const bool aInside = (sdfA < 0.f);
            const FVector2D& fromPos = aInside ? pa : pb;
            const FVector2D& toPos = aInside ? pb : pa;

            const FVector StartWorld = VolumeXf.TransformPosition(FVector(fromPos.X, fromPos.Y, SliceZ_Local));
            const FVector EndWorld = VolumeXf.TransformPosition(FVector(toPos.X, toPos.Y, SliceZ_Local));

            FHitResult Hit;
            if (World->LineTraceSingleByChannel(Hit, StartWorld, EndWorld, BakeChannel, TraceParams))
            {
                const FVector HitLocal = VolumeXf.InverseTransformPosition(Hit.ImpactPoint);
                return FVector2D(HitLocal.X, HitLocal.Y);
            }

            // Pas de hit (mesh non blocant ou raycast manqué) : fallback interpolation linéaire.
            const float t = FMath::Clamp(sdfA / (sdfA - sdfB), 0.0f, 1.0f);
            return FMath::Lerp(pa, pb, t);
        };

        // Alias court pour préserver la lisibilité du switch case ci-dessous.
        auto Interp = RaycastEdge;

        for (int32 y = 0; y < H - 1; ++y)
        {
            for (int32 x = 0; x < W - 1; ++x)
            {
                // 4 valeurs SDF aux 4 cell centers du carré 2x2.
                const float d_tl = SDF[y * W + x];
                const float d_tr = SDF[y * W + (x + 1)];
                const float d_bl = SDF[(y + 1) * W + x];
                const float d_br = SDF[(y + 1) * W + (x + 1)];

                // 4 positions des cell centers (pas des cell edges comme en Phase A).
                const FVector2D pTL((x + 0.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
                const FVector2D pTR((x + 1.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
                const FVector2D pBL((x + 0.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);
                const FVector2D pBR((x + 1.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);

                // Convention : intérieur = SDF négatif.
                const int32 caseIdx =
                    (d_tl < 0.f ? 8 : 0) |
                    (d_tr < 0.f ? 4 : 0) |
                    (d_br < 0.f ? 2 : 0) |
                    (d_bl < 0.f ? 1 : 0);

                switch (caseIdx)
                {
                case 0: case 15:
                    break;

                case 1: case 14:
                    Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
                    Contour.Add(Interp(d_bl, d_br, pBL, pBR));
                    break;

                case 2: case 13:
                    Contour.Add(Interp(d_bl, d_br, pBL, pBR));
                    Contour.Add(Interp(d_tr, d_br, pTR, pBR));
                    break;

                case 3: case 12:
                    Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
                    Contour.Add(Interp(d_tr, d_br, pTR, pBR));
                    break;

                case 4: case 11:
                    Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
                    Contour.Add(Interp(d_tr, d_br, pTR, pBR));
                    break;

                case 5: // saddle : encloser TL et BR (= les coins négatifs)
                    Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
                    Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
                    Contour.Add(Interp(d_bl, d_br, pBL, pBR));
                    Contour.Add(Interp(d_tr, d_br, pTR, pBR));
                    break;

                case 6: case 9:
                    Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
                    Contour.Add(Interp(d_bl, d_br, pBL, pBR));
                    break;

                case 7: case 8:
                    Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
                    Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
                    break;

                case 10: // saddle : encloser TR et BL
                    Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
                    Contour.Add(Interp(d_bl, d_br, pBL, pBR));
                    Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
                    Contour.Add(Interp(d_tr, d_br, pTR, pBR));
                    break;
                }
            }
        }

        return Contour;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ChainSegmentsIntoPolygon : reconstitue un polygone fermé ordonné depuis la "soupe"
    // de segments produite par MS. Greedy : prend le premier segment, étend la chaîne en
    // cherchant un segment dont une extrémité matche le dernier point.
    //
    // Limitation : en cas de polygone multi-loops (par exemple salle avec un îlot intérieur),
    // seul le premier loop est récupéré. Les segments restants sont ignorés. Pour le proto
    // single-room, suffisant. Si multi-loops devient nécessaire (Phase C+), il faudra appeler
    // ce helper en boucle jusqu'à épuisement des segments.
    // ─────────────────────────────────────────────────────────────────────────
    TArray<FVector2D> ChainSegmentsIntoPolygon(const TArray<FVector2D>& Segments)
    {
        TArray<FVector2D> Polygon;
        if (Segments.Num() < 4)
        {
            return Polygon;
        }

        const int32 NumSegments = Segments.Num() / 2;
        TArray<bool> Used;
        Used.Init(false, NumSegments);

        Polygon.Add(Segments[0]);
        Polygon.Add(Segments[1]);
        Used[0] = true;

        constexpr float EPS_SQ = 0.25f; // 0.5cm tolérance² (cohérent avec précision sub-cell)

        bool bExtended = true;
        while (bExtended)
        {
            bExtended = false;
            const FVector2D LastPoint = Polygon.Last();

            for (int32 i = 0; i < NumSegments; ++i)
            {
                if (Used[i])
                {
                    continue;
                }
                const FVector2D& A = Segments[i * 2];
                const FVector2D& B = Segments[i * 2 + 1];

                if (FVector2D::DistSquared(A, LastPoint) < EPS_SQ)
                {
                    Polygon.Add(B);
                    Used[i] = true;
                    bExtended = true;
                    break;
                }
                else if (FVector2D::DistSquared(B, LastPoint) < EPS_SQ)
                {
                    Polygon.Add(A);
                    Used[i] = true;
                    bExtended = true;
                    break;
                }
            }
        }

        // Polygone fermé : retire le dernier point s'il duplique le premier.
        if (Polygon.Num() > 2 && FVector2D::DistSquared(Polygon[0], Polygon.Last()) < EPS_SQ)
        {
            Polygon.RemoveAt(Polygon.Num() - 1);
        }

        return Polygon;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // EnsureCCWWinding : aire signée par shoelace ; reverse si CW (négatif). Outer loop CCW
    // est la convention attendue par FConstrainedDelaunay2d (et la majorité des
    // triangulateurs).
    // ─────────────────────────────────────────────────────────────────────────
    void EnsureCCWWinding(TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return;
        }
        const int32 N = Polygon.Num();
        float SignedArea2 = 0.f;
        for (int32 i = 0; i < N; ++i)
        {
            const FVector2D& A = Polygon[i];
            const FVector2D& B = Polygon[(i + 1) % N];
            SignedArea2 += A.X * B.Y - B.X * A.Y;
        }
        if (SignedArea2 < 0.f)
        {
            Algo::Reverse(Polygon);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // InsetPolygon : pousse chaque vertex vers l'intérieur de InsetCm cm le long de la
    // bissectrice des deux normales d'arêtes adjacentes. Suppose un polygone CCW : la
    // normale "gauche" d'une arête (rotation +90°) pointe vers l'intérieur.
    //
    // Cosmétique : garantit que le cap mesh est tucké dans le mur même si l'interpolation
    // MS a 1-2mm d'erreur. Aussi cache les artefacts au bord du contour.
    //
    // Pour un polygone fortement non-convexe (L-shape, etc.), l'inset peut produire des
    // self-intersections. Pour le proto single-room (~convex), suffisant.
    // ─────────────────────────────────────────────────────────────────────────
    TArray<FVector2D> InsetPolygon(const TArray<FVector2D>& Polygon, float InsetCm)
    {
        if (InsetCm <= 0.f || Polygon.Num() < 3)
        {
            return Polygon;
        }

        const int32 N = Polygon.Num();
        TArray<FVector2D> Result;
        Result.Reserve(N);

        for (int32 i = 0; i < N; ++i)
        {
            const FVector2D& Prev = Polygon[(i + N - 1) % N];
            const FVector2D& Curr = Polygon[i];
            const FVector2D& Next = Polygon[(i + 1) % N];

            const FVector2D EdgeIn = (Curr - Prev).GetSafeNormal();
            const FVector2D EdgeOut = (Next - Curr).GetSafeNormal();

            // Normale gauche d'une arête (rotation +90° : (dx,dy) → (-dy, dx)) pointe vers
            // l'intérieur d'un polygone CCW.
            const FVector2D NormalIn(-EdgeIn.Y, EdgeIn.X);
            const FVector2D NormalOut(-EdgeOut.Y, EdgeOut.X);

            const FVector2D InwardNormal = ((NormalIn + NormalOut) * 0.5f).GetSafeNormal();
            Result.Add(Curr + InwardNormal * InsetCm);
        }
        return Result;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Cap mesh par triangulation Delaunay (Phase B).
    //
    // Input : polygone fermé ordonné CCW (Outer loop convention).
    // Output : FCachedWaterMesh triangulé via UE::Geometry::FConstrainedDelaunay2d.
    //   - Vertices stockés à Z=0 (le runtime translate via SetRelativeLocation).
    //   - Normales (0,0,1) up.
    //   - UV0 planaire en mètres (1 UV unit = 100 cm, pour tilage de normal map à 1m).
    //
    // bSplitBowties true → robustesse aux self-intersections éventuelles du contour.
    // bOutputCCW true → triangles CCW depuis +Z (= front face up = visible vu d'en haut).
    // ─────────────────────────────────────────────────────────────────────────
    FCachedWaterMesh GenerateCapMeshFromPolygon(const TArray<FVector2D>& Polygon)
    {
        FCachedWaterMesh Mesh;
        if (Polygon.Num() < 3)
        {
            return Mesh;
        }

        UE::Geometry::FConstrainedDelaunay2d Triangulator;
        Triangulator.bOutputCCW = true;
        Triangulator.bSplitBowties = true;

        const int32 N = Polygon.Num();
        Triangulator.Vertices.Reserve(N);
        Triangulator.Edges.Reserve(N);

        for (int32 i = 0; i < N; ++i)
        {
            Triangulator.Vertices.Add(FVector2d(Polygon[i].X, Polygon[i].Y));
        }
        for (int32 i = 0; i < N; ++i)
        {
            Triangulator.Edges.Emplace(i, (i + 1) % N);
        }

        if (!Triangulator.Triangulate())
        {
            UE_LOG(LogWaterProto, Warning, TEXT("GenerateCapMeshFromPolygon: Triangulate() failed for polygon (N=%d)"), N);
            return Mesh;
        }

        Mesh.Vertices.Reserve(Triangulator.Vertices.Num());
        Mesh.Normals.Reserve(Triangulator.Vertices.Num());
        Mesh.UV0.Reserve(Triangulator.Vertices.Num());

        for (const FVector2d& V : Triangulator.Vertices)
        {
            Mesh.Vertices.Add(FVector(static_cast<float>(V.X), static_cast<float>(V.Y), 0.f));
            Mesh.Normals.Add(FVector(0.f, 0.f, 1.f));
            // 1 UV unit = 100 cm.
            Mesh.UV0.Add(FVector2D(static_cast<float>(V.X) * 0.01f, static_cast<float>(V.Y) * 0.01f));
        }

        Mesh.Triangles.Reserve(Triangulator.Triangles.Num() * 3);
        for (const UE::Geometry::FIndex3i& Tri : Triangulator.Triangles)
        {
            Mesh.Triangles.Add(Tri.A);
            Mesh.Triangles.Add(Tri.B);
            Mesh.Triangles.Add(Tri.C);
        }

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
    bool bAutoDetectOpenings,
    float CapInsetCm)
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

        Slice.ContourPolygon = ExtractContourMarchingSquaresInterpolated(
            Slice.SignedDistance, GridW, GridH, CellSize, BoxLocalMin,
            World, VolumeXf, BakeChannel, TraceParams, SliceZ_Local);
        Data->Slices.Add(MoveTemp(Slice));
    }

    // ─── Cap meshes par slice (Phase B : MS interpolated → chained polygon → Delaunay) ───
    Data->CapMeshesPerSlice.Reserve(Data->Slices.Num());
    int32 EmptyCount = 0;
    int32 DegenerateCount = 0;
    for (const FCompartmentSlice& Slice : Data->Slices)
    {
        // Si pas de contour (slice 100% inside ou 100% outside), pas de cap mesh.
        if (Slice.ContourPolygon.Num() < 4)
        {
            Data->CapMeshesPerSlice.Add(FCachedWaterMesh{});
            ++EmptyCount;
            continue;
        }

        TArray<FVector2D> OrderedPoly = ChainSegmentsIntoPolygon(Slice.ContourPolygon);
        if (OrderedPoly.Num() < 3)
        {
            Data->CapMeshesPerSlice.Add(FCachedWaterMesh{});
            ++DegenerateCount;
            continue;
        }

        EnsureCCWWinding(OrderedPoly);

        if (CapInsetCm > 0.f)
        {
            OrderedPoly = InsetPolygon(OrderedPoly, CapInsetCm);
        }

        FCachedWaterMesh Mesh = GenerateCapMeshFromPolygon(OrderedPoly);
        if (Mesh.Vertices.Num() == 0)
        {
            ++DegenerateCount;
        }
        Data->CapMeshesPerSlice.Add(MoveTemp(Mesh));
    }

    if (bAutoDetectOpenings)
    {
        DetectOpenings(Data);
    }

    UE_LOG(LogWaterProto, Display,
        TEXT("BakeVolume: id=%s | grid=%dx%d (cell=%.0fcm) | slices=%d | probes=%d | empty=%d | degenerate=%d | inset=%.1fcm | openings=%d (auto=%s)"),
        *CompartmentId.ToString(), GridW, GridH, CellSize, NumSlices, TotalProbeCount, EmptyCount, DegenerateCount,
        CapInsetCm, Data->OpeningSegmentStarts.Num(), bAutoDetectOpenings ? TEXT("true") : TEXT("false"));

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
    bool bAutoDetectOpenings,
    float CapInsetCm)
{
#if WITH_EDITOR
    URoomWaterBakedData* Transient = BakeVolume(Volume, CompartmentId, NumSlices, CellSize, bAutoDetectOpenings, CapInsetCm);
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
    return BakeVolume(Volume, CompartmentId, NumSlices, CellSize, bAutoDetectOpenings, CapInsetCm);
#endif
}
