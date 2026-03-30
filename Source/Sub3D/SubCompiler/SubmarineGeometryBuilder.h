#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubCompilerTypes.h"
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
		float WidthToHeightRatio = 1.f) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	bool GenerateBulkheadMeshData(
		const FSubmarineLayoutSolution& Solution,
		TArray<FSubmarineBulkheadMeshData>& OutMeshData) const;

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
		UMaterialInterface* MaterialOverride = nullptr,
		bool bEnableCollision = false,
		float SectionExponent = 2.f,
		float WidthToHeightRatio = 1.f) const;

	UFUNCTION(BlueprintCallable, Category = "SubCompiler|Geometry")
	UProceduralMeshComponent* BuildExteriorMesh(
		const FSubmarineLayoutSolution& Solution,
		AActor* ParentActor,
		UMaterialInterface* MaterialOverride = nullptr,
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
		FSubmarineInteriorCompartmentMeshData& OutMeshData) const;

	bool GenerateSingleBulkheadMeshData(
		const FSubmarineLayoutSolution& Solution,
		const FBulkheadPlacement& Bulkhead,
		FSubmarineBulkheadMeshData& OutMeshData) const;
};
