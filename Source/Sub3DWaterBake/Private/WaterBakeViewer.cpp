#include "WaterBakeViewer.h"

#include "Sub3DWaterBake.h"
#include "Types/CompartmentWaterBake.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AWaterBakeViewer::AWaterBakeViewer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
#endif

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(Root);
		struct FBillboardSpriteFinder
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> Sprite;
			FBillboardSpriteFinder() : Sprite(TEXT("/Engine/EditorResources/S_Note.S_Note")) {}
		};
		static FBillboardSpriteFinder SpriteFinder;
		if (SpriteFinder.Sprite.Succeeded())
		{
			Billboard->SetSprite(SpriteFinder.Sprite.Get());
		}
		Billboard->bIsEditorOnly = true;
		Billboard->bHiddenInGame = true;
	}
#endif
}

void AWaterBakeViewer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!BakeToView)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform Xf = GetActorTransform();
	const int32 NumSlices = BakeToView->Slices.Num();

	// ── Bounds box ───────────────────────────────────────────────────────
	if (bDrawBounds)
	{
		const FVector Center = Xf.TransformPosition(
			(BakeToView->LocalBoundsMin + BakeToView->LocalBoundsMax) * 0.5f);
		const FVector Extent = (BakeToView->LocalBoundsMax - BakeToView->LocalBoundsMin) * 0.5f;
		DrawDebugBox(World, Center, Extent, Xf.GetRotation(), FColor::Cyan, false, 0.f, 0, 2.f);
	}

	// ── Slice contour polygons ───────────────────────────────────────────
	if (bDrawSliceContours && NumSlices > 0)
	{
		auto DrawSlice = [&](int32 SliceIdx)
		{
			if (!BakeToView->Slices.IsValidIndex(SliceIdx)) return;
			const FCompartmentBakeSlice& S = BakeToView->Slices[SliceIdx];
			const int32 N = S.ContourPolygon.Num();
			if (N < 3) return;
			const float t = NumSlices > 1
				? static_cast<float>(SliceIdx) / static_cast<float>(NumSlices - 1)
				: 0.f;
			const FColor Color = FLinearColor::LerpUsingHSV(
				FLinearColor::Blue, FLinearColor::Yellow, t).ToFColor(true);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& A = S.ContourPolygon[i];
				const FVector2D& B = S.ContourPolygon[(i + 1) % N];
				const FVector AW = Xf.TransformPosition(FVector(A.X, A.Y, S.SliceZ_Local));
				const FVector BW = Xf.TransformPosition(FVector(B.X, B.Y, S.SliceZ_Local));
				DrawDebugLine(World, AW, BW, Color, false, 0.f, 0, 1.5f);
			}
		};

		if (SliceContourIndex < 0)
		{
			for (int32 i = 0; i < NumSlices; ++i) DrawSlice(i);
		}
		else
		{
			DrawSlice(SliceContourIndex);
		}
	}

	// ── Polygon endpoints (open-chain diagnostic) ───────────────────────
	if (bDrawPolygonEndpoints && NumSlices > 0)
	{
		auto DrawEndpoints = [&](int32 SliceIdx)
		{
			if (!BakeToView->Slices.IsValidIndex(SliceIdx)) return;
			const FCompartmentBakeSlice& S = BakeToView->Slices[SliceIdx];
			const int32 N = S.ContourPolygon.Num();
			if (N < 2) return;
			const FVector2D& First = S.ContourPolygon[0];
			const FVector2D& Last = S.ContourPolygon[N - 1];
			const FVector FirstW = Xf.TransformPosition(FVector(First.X, First.Y, S.SliceZ_Local));
			const FVector LastW = Xf.TransformPosition(FVector(Last.X, Last.Y, S.SliceZ_Local));
			DrawDebugSphere(World, FirstW, 12.f, 8, FColor::Green, false, 0.f, 0, 2.f);
			DrawDebugSphere(World, LastW, 12.f, 8, FColor::Red, false, 0.f, 0, 2.f);
		};

		// Use the same slice filter as DrawSliceContours when available, otherwise all.
		if (SliceContourIndex < 0)
		{
			for (int32 i = 0; i < NumSlices; ++i) DrawEndpoints(i);
		}
		else
		{
			DrawEndpoints(SliceContourIndex);
		}
	}

	// ── Raw MS segments (pre-chain) ──────────────────────────────────────
	if (bDrawRawMSSegments && NumSlices > 0)
	{
		auto DrawRawSlice = [&](int32 SliceIdx)
		{
			if (!BakeToView->Slices.IsValidIndex(SliceIdx)) return;
			const FCompartmentBakeSlice& S = BakeToView->Slices[SliceIdx];
			const int32 NumSeg = S.RawContourSegments.Num() / 2;
			if (NumSeg <= 0) return;
			// Bright orange to distinguish from the chained polygon (blue→yellow gradient).
			const FColor SegColor = FColor::Orange;
			for (int32 s = 0; s < NumSeg; ++s)
			{
				const FVector2D& A = S.RawContourSegments[s * 2 + 0];
				const FVector2D& B = S.RawContourSegments[s * 2 + 1];
				const FVector AW = Xf.TransformPosition(FVector(A.X, A.Y, S.SliceZ_Local));
				const FVector BW = Xf.TransformPosition(FVector(B.X, B.Y, S.SliceZ_Local));
				DrawDebugLine(World, AW, BW, SegColor, false, 0.f, 0, 1.f);
			}
		};

		if (RawMSSegmentsSliceIndex < 0)
		{
			for (int32 i = 0; i < NumSlices; ++i) DrawRawSlice(i);
		}
		else
		{
			DrawRawSlice(RawMSSegmentsSliceIndex);
		}
	}

	// ── SDF cells ────────────────────────────────────────────────────────
	if (bDrawSDFCells)
	{
		auto DrawSDFForSlice = [&](int32 SliceIdx)
		{
			if (!BakeToView->Slices.IsValidIndex(SliceIdx)) return;
			const FCompartmentBakeSlice& S = BakeToView->Slices[SliceIdx];
			const int32 W = S.GridWidth;
			const int32 H = S.GridHeight;
			const float CellSize = BakeToView->CellSizeCm;
			const FVector& Min = BakeToView->LocalBoundsMin;
			const int32 Step = FMath::Max(1, SDFSubsampleStep);
			constexpr float MaxDist = 100.f;
			constexpr float SaturatedOutsideCm = 900.f;

			for (int32 y = 0; y < H; y += Step)
			{
				for (int32 x = 0; x < W; x += Step)
				{
					const float D = S.SignedDistance[y * W + x];
					if (D > SaturatedOutsideCm) continue;
					const FVector LocalPos(
						Min.X + (x + 0.5f) * CellSize,
						Min.Y + (y + 0.5f) * CellSize,
						S.SliceZ_Local);
					const FVector WP = Xf.TransformPosition(LocalPos);
					const float Mag01 = FMath::Clamp(FMath::Abs(D) / MaxDist, 0.f, 1.f);
					const FColor C = D < 0.f
						? FColor(uint8((1.f - Mag01) * 255), 255, uint8((1.f - Mag01) * 100), 255)
						: FColor(255, uint8((1.f - Mag01) * 255), uint8((1.f - Mag01) * 100), 255);
					const float PointSize = FMath::Clamp(CellSize * 0.5f, 6.f, 24.f);
					DrawDebugPoint(World, WP, PointSize, C, false, 0.f);
				}
			}
		};

		if (SDFCellsSliceIndex < 0)
		{
			// All slices stacked. Heavy — use SDFSubsampleStep ≥ 4.
			for (int32 i = 0; i < NumSlices; ++i) DrawSDFForSlice(i);
		}
		else
		{
			DrawSDFForSlice(SDFCellsSliceIndex);
		}
	}

	// ── Cap mesh wireframe ───────────────────────────────────────────────
	if (bDrawCapMesh
		&& BakeToView->CapMeshesPerSlice.IsValidIndex(CapMeshSliceIndex)
		&& BakeToView->Slices.IsValidIndex(CapMeshSliceIndex))
	{
		const FCachedBakeMesh& M = BakeToView->CapMeshesPerSlice[CapMeshSliceIndex];
		const FCompartmentBakeSlice& S = BakeToView->Slices[CapMeshSliceIndex];
		const int32 NumTris = M.Triangles.Num() / 3;
		for (int32 t = 0; t < NumTris; ++t)
		{
			const int32 I0 = M.Triangles[t * 3 + 0];
			const int32 I1 = M.Triangles[t * 3 + 1];
			const int32 I2 = M.Triangles[t * 3 + 2];
			if (!M.Vertices.IsValidIndex(I0) || !M.Vertices.IsValidIndex(I1) || !M.Vertices.IsValidIndex(I2))
				continue;
			const FVector& V0 = M.Vertices[I0];
			const FVector& V1 = M.Vertices[I1];
			const FVector& V2 = M.Vertices[I2];
			// Cap mesh vertices are stored at Z=0; offset to slice Z so the wireframe renders
			// at the right horizontal plane.
			const FVector P0 = Xf.TransformPosition(FVector(V0.X, V0.Y, S.SliceZ_Local));
			const FVector P1 = Xf.TransformPosition(FVector(V1.X, V1.Y, S.SliceZ_Local));
			const FVector P2 = Xf.TransformPosition(FVector(V2.X, V2.Y, S.SliceZ_Local));
			DrawDebugLine(World, P0, P1, FColor::Green, false, 0.f, 0, 0.5f);
			DrawDebugLine(World, P1, P2, FColor::Green, false, 0.f, 0, 0.5f);
			DrawDebugLine(World, P2, P0, FColor::Green, false, 0.f, 0, 0.5f);
		}
	}
}
