#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RouteConnectorGenerator.generated.h"

USTRUCT(BlueprintType)
struct FRouteConnectorBuildSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="100.0"))
	float GapLengthCm = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CurvatureAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="4", ClampMax="64"))
	int32 RadialSides = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="2", ClampMax="64"))
	int32 RingCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="100.0"))
	float TargetRingSpacingCm = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector", meta=(ClampMin="100.0"))
	float UVTileLengthCm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connector")
	bool bEnableCollision = true;
};

USTRUCT()
struct FRouteConnectorMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> Vertices;

	UPROPERTY()
	TArray<int32> Triangles;

	UPROPERTY()
	TArray<FVector> Normals;

	UPROPERTY()
	TArray<FVector2D> UVs;
};

UCLASS()
class SUB3D_API URouteConnectorGenerator : public UObject
{
	GENERATED_BODY()

public:
	static bool BuildConnectorMesh(
		const FTransform& StartWorld,
		float StartRadiusCm,
		const FTransform& EndWorld,
		float EndRadiusCm,
		const FRouteConnectorBuildSettings& Settings,
		FRouteConnectorMeshData& OutMesh);
};
