#include "SubmarineWaterBakerLibrary.h"

#include "Sub3DWaterBake.h"
#include "Types/CompartmentWaterBake.h"

#include "Algo/Reverse.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Submarine/CompartmentVolumeComponent.h"
#include "Submarine/Generator/SubmarineDefinition.h"
#include "Submarine/Generator/SubmarineDefinitionTypes.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Direct port of Sub3DWaterProto::URoomWaterBakerLibrary algorithm. The only
// non-helper differences vs the proto are in BakeCompartment:
//   - input UBoxComponent* → discovered UCompartmentVolumeComponent on HullSourceActor
//   - bounds stored in SUBMARINE-LOCAL space (volume center ± extent), not volume-local
//   - raycast transform = submarine actor world transform (so cell positions in sub-local
//     project to world correctly)
//   - output UCompartmentWaterBake (Sub3DCore) instead of URoomWaterBakedData (proto)
// All MS, chain, resample, ring tessellation helpers are copied verbatim.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	// Marching Squares with ray-cast edge interpolation. Produces a segment soup —
	// chain into a polygon downstream.
	TArray<FVector2D> ExtractContourMS(
		const TArray<float>& SDF, int32 W, int32 H, float CellSize, const FVector& LocalMin,
		UWorld* World, const FTransform& VolumeXf, ECollisionChannel BakeChannel,
		const FCollisionQueryParams& TraceParams, float SliceZ_Local)
	{
		TArray<FVector2D> Contour;
		Contour.Reserve(W * H);

		auto RaycastEdge = [&](float sdfA, float sdfB, const FVector2D& pa, const FVector2D& pb) -> FVector2D
		{
			if (FMath::Sign(sdfA) == FMath::Sign(sdfB))
			{
				return (pa + pb) * 0.5f;
			}
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
			const float t = FMath::Clamp(sdfA / (sdfA - sdfB), 0.0f, 1.0f);
			return FMath::Lerp(pa, pb, t);
		};
		auto Interp = RaycastEdge;

		for (int32 y = 0; y < H - 1; ++y)
		{
			for (int32 x = 0; x < W - 1; ++x)
			{
				const float d_tl = SDF[y * W + x];
				const float d_tr = SDF[y * W + (x + 1)];
				const float d_bl = SDF[(y + 1) * W + x];
				const float d_br = SDF[(y + 1) * W + (x + 1)];

				const FVector2D pTL((x + 0.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
				const FVector2D pTR((x + 1.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
				const FVector2D pBL((x + 0.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);
				const FVector2D pBR((x + 1.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);

				const int32 caseIdx =
					(d_tl < 0.f ? 8 : 0) |
					(d_tr < 0.f ? 4 : 0) |
					(d_br < 0.f ? 2 : 0) |
					(d_bl < 0.f ? 1 : 0);

				switch (caseIdx)
				{
				case 0: case 15: break;
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
				case 5: // saddle: TR + BL inside (diagonal). Wrap each inside corner separately.
					// BL contour: edges adjacent to BL = left + bottom
					Contour.Add(Interp(d_tl, d_bl, pTL, pBL));   // left
					Contour.Add(Interp(d_bl, d_br, pBL, pBR));   // bottom
					// TR contour: edges adjacent to TR = top + right
					Contour.Add(Interp(d_tl, d_tr, pTL, pTR));   // top
					Contour.Add(Interp(d_tr, d_br, pTR, pBR));   // right
					break;
				case 6: case 9:
					Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
					Contour.Add(Interp(d_bl, d_br, pBL, pBR));
					break;
				case 7: case 8:
					Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
					Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
					break;
				case 10: // saddle: TL + BR inside (other diagonal). Wrap each inside corner separately.
					// TL contour: edges adjacent to TL = left + top
					Contour.Add(Interp(d_tl, d_bl, pTL, pBL));   // left
					Contour.Add(Interp(d_tl, d_tr, pTL, pTR));   // top
					// BR contour: edges adjacent to BR = bottom + right
					Contour.Add(Interp(d_bl, d_br, pBL, pBR));   // bottom
					Contour.Add(Interp(d_tr, d_br, pTR, pBR));   // right
					break;
				}
			}
		}
		return Contour;
	}

	// Hash-grid based chaining. Returns the largest-AREA CLOSED loop, or empty if none closes.
	//
	//   1. Snap every endpoint to a 0.1 cm integer grid → O(1) hash lookup.
	//   2. Walk all connected components from each unused segment.
	//   3. Track per-loop closure (first/last identical AFTER walking, BEFORE stripping).
	//   4. Among CLOSED loops, pick the one with the largest enclosed area (shoelace formula).
	//   5. Reject the slice if no closed loop exists (caller marks it degenerate, no cap mesh).
	//
	// Why area > perimeter for selection: a long thin "noise" arc can have similar perimeter to
	// a small compartment outline. Area robustly favors the actual room (an enclosed region) over
	// stringy noise components (low area).
	TArray<FVector2D> ChainSegmentsIntoPolygon(const TArray<FVector2D>& Segments, FString* OutDiag = nullptr)
	{
		TArray<FVector2D> Result;
		if (Segments.Num() < 4)
		{
			if (OutDiag) *OutDiag = FString::Printf(TEXT("raw=%d (too few)"), Segments.Num() / 2);
			return Result;
		}

		const int32 NumSegments = Segments.Num() / 2;

		constexpr float SnapResolution = 0.1f; // 1 mm
		auto Snap = [SnapResolution](const FVector2D& P) -> FIntPoint
		{
			return FIntPoint(
				FMath::RoundToInt(P.X / SnapResolution),
				FMath::RoundToInt(P.Y / SnapResolution));
		};

		auto PolyArea = [](const TArray<FVector2D>& P) -> float
		{
			const int32 N = P.Num();
			if (N < 3) return 0.f;
			float A = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& a = P[i];
				const FVector2D& b = P[(i + 1) % N];
				A += a.X * b.Y - b.X * a.Y;
			}
			return FMath::Abs(A) * 0.5f;
		};

		TMap<FIntPoint, TArray<int32>> VertexToSegs;
		VertexToSegs.Reserve(NumSegments * 2);
		for (int32 s = 0; s < NumSegments; ++s)
		{
			VertexToSegs.FindOrAdd(Snap(Segments[s * 2 + 0])).Add(s);
			VertexToSegs.FindOrAdd(Snap(Segments[s * 2 + 1])).Add(s);
		}

		TArray<bool> Used;
		Used.Init(false, NumSegments);

		// Walk forward from StartSeg. Returns the loop AFTER stripping the closing duplicate (if
		// any) and writes whether the loop actually closed into bOutClosed (P1 fix: closure must
		// be detected BEFORE stripping the duplicate, otherwise the post-strip first/last
		// comparison always reports "not closed").
		auto WalkFrom = [&](int32 StartSeg, bool& bOutClosed) -> TArray<FVector2D>
		{
			TArray<FVector2D> Loop;
			bOutClosed = false;
			if (Used[StartSeg]) return Loop;
			Used[StartSeg] = true;
			Loop.Add(Segments[StartSeg * 2 + 0]);
			Loop.Add(Segments[StartSeg * 2 + 1]);

			while (true)
			{
				const FIntPoint Key = Snap(Loop.Last());
				const TArray<int32>* Incident = VertexToSegs.Find(Key);
				if (!Incident) break;

				int32 NextSeg = INDEX_NONE;
				for (int32 s : *Incident)
				{
					if (!Used[s]) { NextSeg = s; break; }
				}
				if (NextSeg == INDEX_NONE) break;

				Used[NextSeg] = true;
				const FVector2D& A = Segments[NextSeg * 2 + 0];
				const FVector2D& B = Segments[NextSeg * 2 + 1];
				Loop.Add(Snap(A) == Key ? B : A);
			}

			// Detect closure BEFORE stripping (first==last means the chain wrapped back).
			if (Loop.Num() > 2 && Snap(Loop[0]) == Snap(Loop.Last()))
			{
				bOutClosed = true;
				Loop.RemoveAt(Loop.Num() - 1); // strip duplicate now that closure is recorded
			}
			return Loop;
		};

		int32 NumLoops = 0;
		int32 NumClosedLoops = 0;
		float BestArea = 0.f;
		for (int32 s = 0; s < NumSegments; ++s)
		{
			if (Used[s]) continue;
			bool bClosed = false;
			TArray<FVector2D> Loop = WalkFrom(s, bClosed);
			if (Loop.Num() == 0) continue;
			++NumLoops;
			if (!bClosed) continue; // P2: open chains never become cap meshes
			++NumClosedLoops;
			const float Area = PolyArea(Loop);
			if (Area > BestArea)
			{
				BestArea = Area;
				Result = MoveTemp(Loop);
			}
		}

		if (OutDiag)
		{
			int32 UsedCount = 0;
			for (bool b : Used) if (b) ++UsedCount;
			*OutDiag = FString::Printf(
				TEXT("raw=%d chain=%d used=%d/%d loops=%d closed=%d area=%.0f"),
				NumSegments, Result.Num(), UsedCount, NumSegments, NumLoops,
				NumClosedLoops, BestArea);
		}

		return Result;
	}

	void EnsureCCWWinding(TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3) return;
		const int32 N = Polygon.Num();
		float SignedArea2 = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % N];
			SignedArea2 += A.X * B.Y - B.X * A.Y;
		}
		if (SignedArea2 < 0.f) Algo::Reverse(Polygon);
	}

	TArray<FVector2D> InsetPolygon(const TArray<FVector2D>& Polygon, float InsetCm)
	{
		if (InsetCm <= 0.f || Polygon.Num() < 3) return Polygon;
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
			const FVector2D NormalIn(-EdgeIn.Y, EdgeIn.X);
			const FVector2D NormalOut(-EdgeOut.Y, EdgeOut.X);
			const FVector2D InwardNormal = ((NormalIn + NormalOut) * 0.5f).GetSafeNormal();
			Result.Add(Curr + InwardNormal * InsetCm);
		}
		return Result;
	}

	TArray<FVector2D> ResamplePolygonUniform(const TArray<FVector2D>& Polygon, int32 N)
	{
		TArray<FVector2D> Out;
		const int32 M = Polygon.Num();
		if (M < 3 || N < 3) return Out;

		TArray<float> SegLengths;
		SegLengths.SetNumUninitialized(M);
		float TotalLength = 0.f;
		for (int32 i = 0; i < M; ++i)
		{
			SegLengths[i] = FVector2D::Distance(Polygon[i], Polygon[(i + 1) % M]);
			TotalLength += SegLengths[i];
		}
		if (TotalLength < KINDA_SMALL_NUMBER) return Out;

		const float Step = TotalLength / static_cast<float>(N);
		Out.Reserve(N);
		int32 SegIdx = 0;
		float SegStart = 0.f;
		for (int32 k = 0; k < N; ++k)
		{
			const float Target = static_cast<float>(k) * Step;
			while (SegIdx < M && SegStart + SegLengths[SegIdx] < Target)
			{
				SegStart += SegLengths[SegIdx];
				++SegIdx;
			}
			if (SegIdx >= M)
			{
				Out.Add(Polygon[M - 1]);
				continue;
			}
			const float SegT = (SegLengths[SegIdx] > KINDA_SMALL_NUMBER)
				? (Target - SegStart) / SegLengths[SegIdx]
				: 0.f;
			const FVector2D& A = Polygon[SegIdx];
			const FVector2D& B = Polygon[(SegIdx + 1) % M];
			Out.Add(FMath::Lerp(A, B, SegT));
		}
		return Out;
	}

	void AlignPolygonStart(TArray<FVector2D>& Polygon)
	{
		const int32 N = Polygon.Num();
		if (N < 3) return;
		FVector2D Centroid(0.f, 0.f);
		for (const FVector2D& P : Polygon) Centroid += P;
		Centroid /= static_cast<float>(N);

		int32 BestIdx = 0;
		float BestAbsAngle = TNumericLimits<float>::Max();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D D = Polygon[i] - Centroid;
			const float Angle = FMath::Atan2(D.Y, D.X);
			const float AbsAngle = FMath::Abs(Angle);
			if (AbsAngle < BestAbsAngle) { BestAbsAngle = AbsAngle; BestIdx = i; }
		}
		if (BestIdx > 0)
		{
			TArray<FVector2D> Rotated;
			Rotated.Reserve(N);
			for (int32 i = 0; i < N; ++i) Rotated.Add(Polygon[(BestIdx + i) % N]);
			Polygon = MoveTemp(Rotated);
		}
	}

	// Concentric-rings cap mesh — N polygon verts × (R+1) rings + 1 centroid.
	FCachedBakeMesh GenerateCapMeshConcentricFromPolygon(const TArray<FVector2D>& Polygon, int32 RingsCount)
	{
		FCachedBakeMesh Mesh;
		const int32 N = Polygon.Num();
		if (N < 3) return Mesh;
		const int32 R = FMath::Max(0, RingsCount);

		FVector2D Centroid(0.f, 0.f);
		for (const FVector2D& P : Polygon) Centroid += P;
		Centroid /= static_cast<float>(N);

		const int32 TotalRings = R + 1;
		const int32 TotalVerts = 1 + TotalRings * N;
		Mesh.Vertices.Reserve(TotalVerts);
		Mesh.Normals.Reserve(TotalVerts);
		Mesh.UV0.Reserve(TotalVerts);

		auto PushVert = [&](const FVector2D& P)
		{
			Mesh.Vertices.Add(FVector(P.X, P.Y, 0.f));
			Mesh.Normals.Add(FVector(0.f, 0.f, 1.f));
			Mesh.UV0.Add(FVector2D(P.X * 0.01f, P.Y * 0.01f));
		};

		PushVert(Centroid);
		for (int32 k = 0; k < TotalRings; ++k)
		{
			const float T = static_cast<float>(k + 1) / static_cast<float>(R + 1);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D Pos = FMath::Lerp(Centroid, Polygon[i], T);
				PushVert(Pos);
			}
		}

		auto RingIdx = [N](int32 RingK, int32 VertI) -> int32 { return 1 + RingK * N + VertI; };

		Mesh.Triangles.Reserve((2 * R + 1) * N * 3);
		for (int32 i = 0; i < N; ++i)
		{
			Mesh.Triangles.Add(0);
			Mesh.Triangles.Add(RingIdx(0, i));
			Mesh.Triangles.Add(RingIdx(0, (i + 1) % N));
		}
		for (int32 k = 0; k < R; ++k)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const int32 i_next = (i + 1) % N;
				const int32 a = RingIdx(k, i);
				const int32 b = RingIdx(k + 1, i);
				const int32 c = RingIdx(k + 1, i_next);
				const int32 d = RingIdx(k, i_next);
				Mesh.Triangles.Add(a); Mesh.Triangles.Add(b); Mesh.Triangles.Add(c);
				Mesh.Triangles.Add(a); Mesh.Triangles.Add(c); Mesh.Triangles.Add(d);
			}
		}
		return Mesh;
	}

	// Find a UCompartmentVolumeComponent on Actor whose CompartmentId matches.
	// Returns the first match (compound rooms — multi-volume — are not yet supported in 2d).
	UCompartmentVolumeComponent* FindCompartmentVolume(AActor* Actor, FName CompartmentId)
	{
		if (!Actor) return nullptr;
		TArray<UCompartmentVolumeComponent*> Volumes;
		Actor->GetComponents<UCompartmentVolumeComponent>(Volumes);
		for (UCompartmentVolumeComponent* V : Volumes)
		{
			if (V && V->CompartmentId == CompartmentId)
			{
				return V;
			}
		}
		return nullptr;
	}
} // namespace

UCompartmentWaterBake* USubmarineWaterBakerLibrary::BakeCompartment(
	USubmarineDefinition* Definition,
	FName CompartmentId,
	AActor* HullSourceActor,
	const FSubmarineWaterBakeParams& Params,
	FString& OutReport)
{
#if !WITH_EDITOR
	UE_LOG(LogWaterBake, Warning, TEXT("BakeCompartment: editor-only, ignored in non-editor build"));
	OutReport += TEXT("[FAIL] non-editor build\n");
	return nullptr;
#else
	if (!Definition || !HullSourceActor || CompartmentId.IsNone())
	{
		const FString Line = FString::Printf(
			TEXT("[FAIL] %s: invalid args (Definition=%s HullActor=%s)"),
			*CompartmentId.ToString(),
			*GetNameSafe(Definition), *GetNameSafe(HullSourceActor));
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}

	UCompartmentVolumeComponent* Volume = FindCompartmentVolume(HullSourceActor, CompartmentId);
	if (!Volume)
	{
		const FString Line = FString::Printf(
			TEXT("[FAIL] %s: no UCompartmentVolumeComponent matching this id on %s"),
			*CompartmentId.ToString(), *HullSourceActor->GetName());
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}

	UWorld* World = HullSourceActor->GetWorld();
	if (!World)
	{
		const FString Line = FString::Printf(
			TEXT("[FAIL] %s: HullSourceActor has no World"), *CompartmentId.ToString());
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}

	const int32 NumSlices = FMath::Max(2, Params.NumSlices);
	const float CellSize = FMath::Max(5.f, Params.CellSizeCm);
	const int32 BakeResampleN = FMath::Clamp(Params.PolygonResampleN, 16, 256);
	const int32 BakeRingsCount = FMath::Clamp(Params.RingsCount, 0, 8);

	// Compute SUB-LOCAL bounds of the volume. The volume is attached somewhere under the
	// submarine root; its sub-local AABB = (volume world center → sub-local) ± box extent.
	const FTransform SubmarineXf = HullSourceActor->GetActorTransform();
	const FVector VolumeWorldCenter = Volume->GetComponentLocation();
	const FVector VolumeSubLocalCenter = SubmarineXf.InverseTransformPosition(VolumeWorldCenter);
	const FVector BoxExtent = Volume->GetUnscaledBoxExtent();
	const FVector UserMin = VolumeSubLocalCenter - BoxExtent;
	const FVector UserMax = VolumeSubLocalCenter + BoxExtent;

	// Internal grid padding: extend bounds by ONE cell in X/Y. The cell-boundary pass below
	// (Fix A) forces the outer ring of cells to "outside"; with 1-cell padding, that ring is
	// the padding itself — the user's authored volume cells are fully preserved, and the
	// contour's MS interpolation can place a vertex AT the user volume's edge by raycasting
	// from the padding cell (forced outside) into the first user cell (inside).
	//
	// Visible cost: the bake's stored bounds (cyan in the viewer) are 1 cell larger than the
	// authored BP volume (yellow). With CellSizeCm=10 (default) this is a 10 cm visual delta.
	// Cap meshes themselves stay inside the user's authored volume — no leak across bulkheads.
	const FVector LocalMin(UserMin.X - CellSize, UserMin.Y - CellSize, UserMin.Z);
	const FVector LocalMax(UserMax.X + CellSize, UserMax.Y + CellSize, UserMax.Z);

	if ((LocalMax - LocalMin).GetMin() < 1.f)
	{
		const FString Line = FString::Printf(
			TEXT("[FAIL] %s: degenerate volume bounds (extent=%s)"),
			*CompartmentId.ToString(), *(LocalMax - LocalMin).ToString());
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}

	const int32 GridW = FMath::Max(2, FMath::CeilToInt((LocalMax.X - LocalMin.X) / CellSize));
	const int32 GridH = FMath::Max(2, FMath::CeilToInt((LocalMax.Y - LocalMin.Y) / CellSize));

	// Asset creation / overwrite.
	FString Folder = Params.PackagePathRoot;
	if (!Folder.EndsWith(TEXT("/"))) Folder += TEXT("/");
	const FString AssetName = FString::Printf(TEXT("CWB_%s"), *CompartmentId.ToString());
	const FString FullPackagePath = Folder + AssetName;

	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		const FString Line = FString::Printf(TEXT("[FAIL] %s: CreatePackage failed for %s"),
			*CompartmentId.ToString(), *FullPackagePath);
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}
	Package->FullyLoad();

	UCompartmentWaterBake* Bake = NewObject<UCompartmentWaterBake>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Bake)
	{
		const FString Line = FString::Printf(TEXT("[FAIL] %s: NewObject failed"), *CompartmentId.ToString());
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return nullptr;
	}

	Bake->CompartmentId = CompartmentId;
	// Store the AUTHORED USER bounds (not the internally-padded grid bounds). Viewers and
	// runtime see cyan = yellow. Cell positions in Slices[i].SignedDistance are computed by
	// consumers as `LocalBoundsMin - CellSizeCm + (x + 0.5) * CellSizeCm` — see header doc.
	Bake->LocalBoundsMin = UserMin;
	Bake->LocalBoundsMax = UserMax;
	Bake->CellSizeCm = CellSize;
	Bake->Slices.Reset();
	Bake->CapMeshesPerSlice.Reset();

	// Voxelisation per slice — SDF 2D. VolumeXf for raycast = SubmarineXf so that sub-local
	// cell positions map to world correctly.
	const ECollisionChannel BakeChannel = ECC_WorldStatic;
	constexpr float SDF_MaxDistanceCm = 1000.f;
	constexpr float SDF_ParityRayLength = 100000.f;
	constexpr float Diag = 0.70710678f;

	const FVector LocalDirs6[6] = {
		FVector(+1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f),
		FVector(0.f, +1.f, 0.f), FVector(0.f, -1.f, 0.f),
		FVector(0.f, 0.f, +1.f), FVector(0.f, 0.f, -1.f),
	};
	const FVector LocalDirsDiag[4] = {
		FVector(+Diag, +Diag, 0.f), FVector(-Diag, -Diag, 0.f),
		FVector(+Diag, -Diag, 0.f), FVector(-Diag, +Diag, 0.f),
	};

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SubmarineWaterBakeSDF), /*bTraceComplex*/ true);

	const FTransform& VolumeXf = SubmarineXf;
	int32 TotalProbes = 0;

	Bake->Slices.Reserve(NumSlices);
	for (int32 sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
	{
		const float t = static_cast<float>(sliceIdx) / static_cast<float>(NumSlices - 1);
		const float SliceZ_Local = FMath::Lerp(LocalMin.Z, LocalMax.Z, t);

		FCompartmentBakeSlice Slice;
		Slice.SliceZ_Local = SliceZ_Local;
		Slice.GridWidth = GridW;
		Slice.GridHeight = GridH;
		Slice.SignedDistance.SetNumZeroed(GridW * GridH);

		for (int32 y = 0; y < GridH; ++y)
		{
			for (int32 x = 0; x < GridW; ++x)
			{
				const FVector LocalPos(
					LocalMin.X + (x + 0.5f) * CellSize,
					LocalMin.Y + (y + 0.5f) * CellSize,
					SliceZ_Local);
				const FVector WorldPos = VolumeXf.TransformPosition(LocalPos);

				int32 HitCount = 0;
				float MinDist = SDF_MaxDistanceCm;

				for (int32 d = 0; d < 6; ++d)
				{
					const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirs6[d]);
					FHitResult Hit;
					if (World->LineTraceSingleByChannel(
							Hit, WorldPos, WorldPos + DirWorld * SDF_ParityRayLength,
							BakeChannel, TraceParams))
					{
						++HitCount;
						if (d < 4)
						{
							const float HitDist = FMath::Min(Hit.Distance, SDF_MaxDistanceCm);
							if (HitDist < MinDist) MinDist = HitDist;
						}
					}
				}

				for (int32 d = 0; d < 4; ++d)
				{
					const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirsDiag[d]);
					FHitResult Hit;
					if (World->LineTraceSingleByChannel(
							Hit, WorldPos, WorldPos + DirWorld * SDF_MaxDistanceCm,
							BakeChannel, TraceParams))
					{
						if (Hit.Distance < MinDist) MinDist = Hit.Distance;
					}
				}

				const bool bIsInside = (HitCount == 6);
				Slice.SignedDistance[y * GridW + x] = bIsInside ? -MinDist : +MinDist;
				++TotalProbes;
			}
		}

		// Fix A: force AABB boundary cells to outside. MS iterates 2x2 squares with x=0..W-2 and
		// y=0..H-2 — transitions at the very last column/row are only processed by ONE adjacent
		// square instead of two, producing odd-degree vertices and Eulerian paths (open chains)
		// instead of cycles. Forcing boundary cells outside guarantees the contour terminates
		// within the grid and every contour vertex has degree exactly 2.
		for (int32 i = 0; i < GridW; ++i)
		{
			Slice.SignedDistance[0 * GridW + i] = +SDF_MaxDistanceCm;          // top row
			Slice.SignedDistance[(GridH - 1) * GridW + i] = +SDF_MaxDistanceCm; // bottom row
		}
		for (int32 j = 0; j < GridH; ++j)
		{
			Slice.SignedDistance[j * GridW + 0] = +SDF_MaxDistanceCm;          // left column
			Slice.SignedDistance[j * GridW + (GridW - 1)] = +SDF_MaxDistanceCm; // right column
		}

		// Fix B: keep only the LARGEST connected component of "inside" cells; force all other
		// inside cells to outside. Replaces the fragile flood-from-volume-center heuristic
		// (which skipped slices where the centre was in a wall/outside). When oversized volumes
		// bleed into adjacent compartments via open doors, the leaks form smaller satellite
		// components — those get pruned. The room is the largest blob.
		{
			TArray<int32> CompId;
			CompId.Init(-1, GridW * GridH);
			TArray<int32> CompSize;
			int32 NumComponents = 0;
			for (int32 seed = 0; seed < GridW * GridH; ++seed)
			{
				if (Slice.SignedDistance[seed] >= 0.f) continue;
				if (CompId[seed] != -1) continue;
				const int32 ThisComp = NumComponents++;
				CompId[seed] = ThisComp;
				int32 Size = 1;
				TArray<int32> Stack;
				Stack.Push(seed);
				while (Stack.Num() > 0)
				{
					const int32 idx = Stack.Pop();
					const int32 cx = idx % GridW;
					const int32 cy = idx / GridW;
					const int32 Neighbors[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
					for (int32 n = 0; n < 4; ++n)
					{
						const int32 nx = cx + Neighbors[n][0];
						const int32 ny = cy + Neighbors[n][1];
						if (nx < 0 || nx >= GridW || ny < 0 || ny >= GridH) continue;
						const int32 nidx = ny * GridW + nx;
						if (CompId[nidx] != -1) continue;
						if (Slice.SignedDistance[nidx] >= 0.f) continue;
						CompId[nidx] = ThisComp;
						Stack.Push(nidx);
						++Size;
					}
				}
				CompSize.Add(Size);
			}

			int32 LargestComp = -1;
			int32 LargestSize = 0;
			for (int32 c = 0; c < NumComponents; ++c)
			{
				if (CompSize[c] > LargestSize)
				{
					LargestSize = CompSize[c];
					LargestComp = c;
				}
			}
			if (LargestComp >= 0)
			{
				for (int32 i = 0; i < GridW * GridH; ++i)
				{
					if (CompId[i] >= 0 && CompId[i] != LargestComp)
					{
						Slice.SignedDistance[i] = +SDF_MaxDistanceCm;
					}
				}
			}
		}

		// Count saddle case occurrences while extracting MS contour. Lets us verify whether
		// case 5/10 ever fire on this geometry.
		int32 SaddleCount = 0;
		for (int32 ySaddle = 0; ySaddle < GridH - 1; ++ySaddle)
		{
			for (int32 xSaddle = 0; xSaddle < GridW - 1; ++xSaddle)
			{
				const float d_tl = Slice.SignedDistance[ySaddle * GridW + xSaddle];
				const float d_tr = Slice.SignedDistance[ySaddle * GridW + (xSaddle + 1)];
				const float d_bl = Slice.SignedDistance[(ySaddle + 1) * GridW + xSaddle];
				const float d_br = Slice.SignedDistance[(ySaddle + 1) * GridW + (xSaddle + 1)];
				const int32 c =
					(d_tl < 0.f ? 8 : 0) | (d_tr < 0.f ? 4 : 0) |
					(d_br < 0.f ? 2 : 0) | (d_bl < 0.f ? 1 : 0);
				if (c == 5 || c == 10) ++SaddleCount;
			}
		}
		Slice.SaddleCount = SaddleCount;

		Slice.ContourPolygon = ExtractContourMS(
			Slice.SignedDistance, GridW, GridH, CellSize, LocalMin,
			World, VolumeXf, BakeChannel, TraceParams, SliceZ_Local);
		// Snapshot the raw MS segment soup BEFORE chain/inset/resample. Used by the bake
		// viewer to diagnose whether MS itself is producing correct segments independently
		// of whether chain succeeded in joining them. Same memory cost as ContourPolygon.
		Slice.RawContourSegments = Slice.ContourPolygon;
		Bake->Slices.Add(MoveTemp(Slice));
	}

	// Per-slice cap mesh.
	Bake->CapMeshesPerSlice.Reserve(Bake->Slices.Num());
	int32 EmptyCount = 0;
	int32 DegenerateCount = 0;
	int32 ValidCount = 0;
	TArray<FString> PerSliceDiag;  // captured for inclusion in OutReport (panel-visible)
	PerSliceDiag.Reserve(Bake->Slices.Num());
	for (int32 SliceIdx = 0; SliceIdx < Bake->Slices.Num(); ++SliceIdx)
	{
		FCompartmentBakeSlice& Slice = Bake->Slices[SliceIdx];
		FString SliceDiag;

		if (Slice.ContourPolygon.Num() < 4)
		{
			Bake->CapMeshesPerSlice.Add(FCachedBakeMesh{});
			++EmptyCount;
			PerSliceDiag.Add(FString::Printf(TEXT("    slice %2d: raw=%d (too few segments → EMPTY)"),
				SliceIdx, Slice.ContourPolygon.Num() / 2));
			continue;
		}

		TArray<FVector2D> OrderedPoly = ChainSegmentsIntoPolygon(Slice.ContourPolygon, &SliceDiag);
		PerSliceDiag.Add(FString::Printf(TEXT("    slice %2d: %s saddles=%d"), SliceIdx, *SliceDiag, Slice.SaddleCount));

		if (OrderedPoly.Num() < 3)
		{
			Bake->CapMeshesPerSlice.Add(FCachedBakeMesh{});
			++DegenerateCount;
			continue;
		}

		EnsureCCWWinding(OrderedPoly);
		if (Params.CapInsetCm > 0.f) OrderedPoly = InsetPolygon(OrderedPoly, Params.CapInsetCm);
		OrderedPoly = ResamplePolygonUniform(OrderedPoly, BakeResampleN);
		AlignPolygonStart(OrderedPoly);

		// Persist the cleaned polygon back so consumers (debug viz, Phase 4 boundary cells)
		// see usable data.
		Slice.ContourPolygon = OrderedPoly;

		FCachedBakeMesh Mesh = GenerateCapMeshConcentricFromPolygon(OrderedPoly, BakeRingsCount);
		if (Mesh.Vertices.Num() < 3 || Mesh.Triangles.Num() < 3)
		{
			Bake->CapMeshesPerSlice.Add(FCachedBakeMesh{});
			++DegenerateCount;
			continue;
		}
		Bake->CapMeshesPerSlice.Add(MoveTemp(Mesh));
		++ValidCount;
	}

	const FString StatusTag = (ValidCount > 0) ? TEXT("[OK]  ") : TEXT("[FAIL]");
	const FString ReportLine = FString::Printf(
		TEXT("%s %s: %d slices (valid=%d, empty=%d, degen=%d) | %d probes | grid=%dx%d | bounds=[%s..%s]"),
		*StatusTag, *CompartmentId.ToString(),
		Bake->Slices.Num(), ValidCount, EmptyCount, DegenerateCount,
		TotalProbes, GridW, GridH, *LocalMin.ToString(), *LocalMax.ToString());
	UE_LOG(LogWaterBake, Display, TEXT("%s"), *ReportLine);
	OutReport += ReportLine + TEXT("\n");
	for (const FString& Line : PerSliceDiag)
	{
		OutReport += Line + TEXT("\n");
	}

	if (ValidCount == 0)
	{
		UE_LOG(LogWaterBake, Warning,
			TEXT("Bake %s rejected: zero valid slices. Inspect bounds, collision profile, or volume placement."),
			*CompartmentId.ToString());
		// Return nullptr so caller knows this bake is unusable. The package is left created
		// (with empty slices) but the caller should NOT add it to Definition->WaterBakes.
		return nullptr;
	}

	// P2.5: register the bake in the DA's WaterBakes map. Runtime FloodWaterPlaneComponent
	// looks up its compartment's CWB through this map. Mark the DA dirty so the panel's
	// SaveLoadedAsset (asset action) actually persists the new reference.
	Definition->WaterBakes.Add(CompartmentId, Bake);

	// Sync derived fields back into Compartments[]. Removes the manual / bake-driven drift on
	// CapacityLiters + MaxWaterHeightCm + WalkableFloorZCm — these are now bake outputs, not
	// hand-edited inputs. Manual edits get overwritten on next bake by design.
	{
		auto PolyArea2D = [](const TArray<FVector2D>& P) -> float
		{
			const int32 N = P.Num();
			if (N < 3) return 0.f;
			float Acc = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& a = P[i];
				const FVector2D& b = P[(i + 1) % N];
				Acc += a.X * b.Y - b.X * a.Y;
			}
			return FMath::Abs(Acc) * 0.5f;
		};

		// Trapezoid integration along Z over the slice stack.
		const float SliceSpacingCm = (NumSlices > 1)
			? (LocalMax.Z - LocalMin.Z) / static_cast<float>(NumSlices - 1)
			: (LocalMax.Z - LocalMin.Z);
		float TotalVolumeCm3 = 0.f;
		float PrevAreaCm2 = -1.f;
		for (const FCompartmentBakeSlice& Slice : Bake->Slices)
		{
			const float AreaCm2 = (Slice.ContourPolygon.Num() >= 3)
				? PolyArea2D(Slice.ContourPolygon)
				: 0.f;
			if (PrevAreaCm2 >= 0.f)
			{
				TotalVolumeCm3 += 0.5f * (PrevAreaCm2 + AreaCm2) * SliceSpacingCm;
			}
			PrevAreaCm2 = AreaCm2;
		}
		const float ComputedCapacityLiters = TotalVolumeCm3 / 1000.f;
		const float ComputedMaxWaterHeightCm = LocalMax.Z - LocalMin.Z;
		const float ComputedFloorZCm = LocalMin.Z;

		for (FGeneratedCompartmentDef& Comp : Definition->Compartments)
		{
			if (Comp.CompartmentId == CompartmentId)
			{
				const FString SyncLine = FString::Printf(
					TEXT("    sync DA: CapacityLiters %.0f → %.0f | MaxWaterHeightCm %.0f → %.0f | WalkableFloorZCm %.0f → %.0f"),
					Comp.CapacityLiters, ComputedCapacityLiters,
					Comp.MaxWaterHeightCm, ComputedMaxWaterHeightCm,
					Comp.WalkableFloorZCm, ComputedFloorZCm);
				UE_LOG(LogWaterBake, Display, TEXT("%s"), *SyncLine);
				OutReport += SyncLine + TEXT("\n");

				Comp.CapacityLiters = FMath::Max(1.f, ComputedCapacityLiters);
				Comp.MaxWaterHeightCm = FMath::Max(1.f, ComputedMaxWaterHeightCm);
				Comp.WalkableFloorZCm = ComputedFloorZCm;
				break;
			}
		}
	}

	Definition->MarkPackageDirty();

	// Save the asset.
	FAssetRegistryModule::AssetCreated(Bake);
	Package->MarkPackageDirty();

	const FString FileName = FPackageName::LongPackageNameToFilename(
		FullPackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FSavePackageResultStruct SaveResult = UPackage::Save(Package, Bake, *FileName, SaveArgs);
	if (!SaveResult.IsSuccessful())
	{
		UE_LOG(LogWaterBake, Warning, TEXT("BakeCompartment %s: UPackage::Save failed for %s"),
			*CompartmentId.ToString(), *FullPackagePath);
		// Return the in-memory asset anyway — caller can decide what to do.
	}

	return Bake;
#endif
}

int32 USubmarineWaterBakerLibrary::BakeAllCompartments(
	USubmarineDefinition* Definition,
	AActor* HullSourceActor,
	const FSubmarineWaterBakeParams& Params,
	FString& OutReport)
{
	if (!Definition || !HullSourceActor)
	{
		const FString Line = FString::Printf(TEXT("[FAIL] BakeAllCompartments: invalid args (Def=%s Hull=%s)"),
			*GetNameSafe(Definition), *GetNameSafe(HullSourceActor));
		UE_LOG(LogWaterBake, Warning, TEXT("%s"), *Line);
		OutReport += Line + TEXT("\n");
		return 0;
	}

	const FString HeaderLine = FString::Printf(
		TEXT("Bake on %s — %d compartments — params: NumSlices=%d CellSize=%.1fcm RingsCount=%d ResampleN=%d Inset=%.1fcm"),
		*HullSourceActor->GetName(), Definition->Compartments.Num(),
		Params.NumSlices, Params.CellSizeCm, Params.RingsCount, Params.PolygonResampleN, Params.CapInsetCm);
	UE_LOG(LogWaterBake, Display, TEXT("%s"), *HeaderLine);
	OutReport += HeaderLine + TEXT("\n");

	int32 SuccessCount = 0;
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.CompartmentId.IsNone()) continue;
		UCompartmentWaterBake* Bake = BakeCompartment(Definition, Comp.CompartmentId, HullSourceActor, Params, OutReport);
		if (Bake) ++SuccessCount;
	}

	const FString FooterLine = FString::Printf(TEXT("─ Done: %d/%d compartments baked ─"),
		SuccessCount, Definition->Compartments.Num());
	UE_LOG(LogWaterBake, Display, TEXT("%s"), *FooterLine);
	OutReport += FooterLine + TEXT("\n");
	return SuccessCount;
}
