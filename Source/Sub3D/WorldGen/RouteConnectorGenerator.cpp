#include "RouteConnectorGenerator.h"

namespace
{
FVector EvalBezierPoint(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
{
	const float U = 1.f - T;
	return
		(U * U * U) * P0 +
		(3.f * U * U * T) * P1 +
		(3.f * U * T * T) * P2 +
		(T * T * T) * P3;
}

FVector EvalBezierTangent(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
{
	const float U = 1.f - T;
	return (
		3.f * U * U * (P1 - P0) +
		6.f * U * T * (P2 - P1) +
		3.f * T * T * (P3 - P2)).GetSafeNormal();
}
}

bool URouteConnectorGenerator::BuildConnectorMesh(
	const FTransform& StartWorld,
	float StartRadiusCm,
	const FTransform& EndWorld,
	float EndRadiusCm,
	const FRouteConnectorBuildSettings& Settings,
	FRouteConnectorMeshData& OutMesh)
{
	OutMesh = FRouteConnectorMeshData();

	const FVector StartPos = StartWorld.GetLocation();
	const FVector EndPos = EndWorld.GetLocation();
	const float Distance = FVector::Distance(StartPos, EndPos);
	if (Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector StartForward = StartWorld.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector EndForward = EndWorld.GetRotation().GetForwardVector().GetSafeNormal();
	const float TangentLength = FMath::Max(200.f, Distance * FMath::Clamp(Settings.CurvatureAlpha, 0.f, 1.f));

	const FVector P0 = StartPos;
	const FVector P1 = StartPos + StartForward * TangentLength;
	const FVector P2 = EndPos - EndForward * TangentLength;
	const FVector P3 = EndPos;

	const int32 RingCount = FMath::Clamp(
		FMath::Max(Settings.RingCount, FMath::CeilToInt(Distance / FMath::Max(100.f, Settings.TargetRingSpacingCm))),
		2,
		128);
	const int32 RadialSides = FMath::Max(4, Settings.RadialSides);
	const float UVTileLength = FMath::Max(100.f, Settings.UVTileLengthCm);

	OutMesh.Vertices.Reserve((RingCount + 1) * RadialSides);
	OutMesh.Normals.Reserve((RingCount + 1) * RadialSides);
	OutMesh.UVs.Reserve((RingCount + 1) * RadialSides);
	OutMesh.Triangles.Reserve(RingCount * RadialSides * 6);

	FVector PreviousRight = FVector::CrossProduct(StartForward, FVector::UpVector).GetSafeNormal();
	if (PreviousRight.IsNearlyZero())
	{
		PreviousRight = FVector::CrossProduct(StartForward, FVector::RightVector).GetSafeNormal();
	}

	float AccumulatedDistance = 0.f;
	FVector PreviousCenter = P0;

	for (int32 RingIndex = 0; RingIndex <= RingCount; RingIndex++)
	{
		const float T = (float)RingIndex / (float)RingCount;
		const FVector Center = EvalBezierPoint(P0, P1, P2, P3, T);
		if (RingIndex > 0)
		{
			AccumulatedDistance += FVector::Distance(PreviousCenter, Center);
		}
		const FVector Forward = EvalBezierTangent(P0, P1, P2, P3, T);
		FVector Right = FVector::VectorPlaneProject(PreviousRight, Forward).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
			if (Right.IsNearlyZero())
			{
				Right = FVector::CrossProduct(Forward, FVector::RightVector).GetSafeNormal();
			}
		}
		const FVector Up = FVector::CrossProduct(Right, Forward).GetSafeNormal();
		PreviousRight = Right;

		const float Radius = FMath::Lerp(StartRadiusCm, EndRadiusCm, T);
		for (int32 SideIndex = 0; SideIndex < RadialSides; SideIndex++)
		{
			const float Angle = (2.f * PI * (float)SideIndex) / (float)RadialSides;
			const FVector RadialDir = FMath::Cos(Angle) * Right + FMath::Sin(Angle) * Up;
			OutMesh.Vertices.Add(Center + RadialDir * Radius);
			OutMesh.Normals.Add(RadialDir);
			OutMesh.UVs.Add(FVector2D((float)SideIndex / (float)RadialSides, AccumulatedDistance / UVTileLength));
		}

		PreviousCenter = Center;
	}

	for (int32 RingIndex = 0; RingIndex < RingCount; RingIndex++)
	{
		const int32 ThisRingBase = RingIndex * RadialSides;
		const int32 NextRingBase = (RingIndex + 1) * RadialSides;
		for (int32 SideIndex = 0; SideIndex < RadialSides; SideIndex++)
		{
			const int32 NextSide = (SideIndex + 1) % RadialSides;
			const int32 A = ThisRingBase + SideIndex;
			const int32 B = NextRingBase + SideIndex;
			const int32 C = NextRingBase + NextSide;
			const int32 D = ThisRingBase + NextSide;

			OutMesh.Triangles.Add(A);
			OutMesh.Triangles.Add(B);
			OutMesh.Triangles.Add(C);

			OutMesh.Triangles.Add(A);
			OutMesh.Triangles.Add(C);
			OutMesh.Triangles.Add(D);
		}
	}

	return OutMesh.Vertices.Num() > 0 && OutMesh.Triangles.Num() > 0;
}
