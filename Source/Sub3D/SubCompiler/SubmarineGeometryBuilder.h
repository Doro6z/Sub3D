#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubCompilerTypes.h"
#include "ProceduralMeshComponent.h"
#include "SubmarineGeometryBuilder.generated.h"

class AActor;
class UMaterialInterface;
class UProceduralMeshComponent;
class USubmarineEnvelopeDef;

USTRUCT(BlueprintType)
struct FSubmarineMeshSectionData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	TArray<FVector> Vertices;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	TArray<FVector> Normals;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	TArray<FVector2D> UVs;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	TArray<FProcMeshTangent> Tangents;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	int32 VertexStart = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	int32 VertexCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	int32 TriangleStart = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	int32 TriangleCount = 0;
};

USTRUCT(BlueprintType)
struct FSubmarineInteriorCompartmentMeshData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FName CompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FSubmarineMeshSectionData WallSection;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FSubmarineMeshSectionData FloorSection;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FSubmarineMeshSectionData BowCapSection;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FSubmarineMeshSectionData SternCapSection;
};

USTRUCT(BlueprintType)
struct FSubmarineBulkheadMeshData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FName BulkheadId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FName ForeCompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FName AftCompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	FSubmarineMeshSectionData PanelSection;

	UPROPERTY(BlueprintReadOnly, Category = "Geometry")
	EPassageType PassageType = EPassageType::WatertightDoor;
};

UCLASS(BlueprintType)
class SUB3D_API USubmarineGeometryBuilder : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	bool GenerateInteriorMeshData(
		const FSubmarineLayoutSolution& Solution,
		TArray<FSubmarineInteriorCompartmentMeshData>& OutMeshData,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f,
		int32 InteriorArcSegments = 24,
		float WallThicknessCm = 12.f) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	bool GenerateBulkheadMeshData(
		const FSubmarineLayoutSolution& Solution,
		TArray<FSubmarineBulkheadMeshData>& OutMeshData,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	bool GenerateExteriorMeshData(
		const FSubmarineLayoutSolution& Solution,
		FSubmarineMeshSectionData& OutMeshData,
		int32 RadialSegments = 32,
		int32 LongitudinalSubdivisionsPerSpan = 6,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f,
		const USubmarineEnvelopeDef* Envelope = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	TArray<UProceduralMeshComponent*> BuildInteriorMeshes(
		const FSubmarineLayoutSolution& Solution,
		AActor* ParentActor,
		UMaterialInterface* WallMaterialOverride = nullptr,
		UMaterialInterface* FloorMaterialOverride = nullptr,
		bool bEnableCollision = false,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f,
		int32 InteriorArcSegments = 24,
		float WallThicknessCm = 12.f) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	UProceduralMeshComponent* BuildExteriorMesh(
		const FSubmarineLayoutSolution& Solution,
		AActor* ParentActor,
		UMaterialInterface* ExteriorMaterialOverride = nullptr,
		bool bEnableCollision = false,
		int32 RadialSegments = 32,
		int32 LongitudinalSubdivisionsPerSpan = 6,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f,
		const USubmarineEnvelopeDef* Envelope = nullptr) const;

private:
	bool GenerateCompartmentInteriorMeshData(
		const FCompartmentPlacement& Placement,
		bool bGenerateBowCap,
		bool bGenerateSternCap,
		float SectionExponent,
		float WidthToHeightRatio,
		int32 InteriorArcSegments,
		float WallThicknessCm,
		FSubmarineInteriorCompartmentMeshData& OutMeshData) const;

	bool GenerateSingleBulkheadMeshData(
		const FSubmarineLayoutSolution& Solution,
		const FBulkheadPlacement& Bulkhead,
		float SectionExponent,
		float WidthToHeightRatio,
		FSubmarineBulkheadMeshData& OutMeshData) const;
};
