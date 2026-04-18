#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineGeneratedGeometryComponent.generated.h"

class UMaterialInterface;
class USubmarineDefinition;
class UProceduralMeshComponent;

/**
 * Reads mesh data from USubmarineDefinition and creates
 * UProceduralMeshComponent instances for visible geometry,
 * walkable floors, and collision surfaces.
 *
 * Attached to ASubmarineBase. No separate actor.
 * Render and collision components are split: render is visible
 * with no collision, collision is hidden with BlockAll profile.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineGeneratedGeometryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineGeneratedGeometryComponent();

	/**
	 * Build all ProceduralMeshComponents from the definition's mesh data.
	 * Destroys any previously built components before rebuilding.
	 * Returns true if at least one render component was created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
	bool BuildFromDefinition(const USubmarineDefinition* Definition);

	/** Destroy all previously built ProceduralMeshComponents. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
	void ClearGeometry();

	UFUNCTION(BlueprintPure, Category = "Submarine|Generator")
	bool HasBuiltGeometry() const { return RenderComponents.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Generator")
	const TArray<UProceduralMeshComponent*>& GetRenderComponents() const { return RenderComponents; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Generator")
	const TArray<UProceduralMeshComponent*>& GetCollisionComponents() const { return CollisionComponents; }

	/** Exterior hull collision PMCs (convex hulls per longitudinal slice). */
	UFUNCTION(BlueprintPure, Category = "Submarine|Generator")
	const TArray<UProceduralMeshComponent*>& GetExteriorHullCollisionComponents() const { return ExteriorHullCollisionComponents; }

	/** Interior floor collision PMCs (walkable, SubInteriorWalkable profile). */
	UFUNCTION(BlueprintPure, Category = "Submarine|Generator")
	const TArray<UProceduralMeshComponent*>& GetInteriorFloorCollisionComponents() const { return InteriorFloorCollisionComponents; }

	// ── Collision tuning ─────────────────────────────────────────────────

	/** Number of longitudinal slices for exterior hull convex collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Collision", meta = (ClampMin = "2", ClampMax = "16"))
	int32 HullCollisionSlices = 4;

	// ── Validation-only section isolation (Shape phase Step 1) ───────────
	// These toggles skip materialization of a given section to isolate it
	// visually in PIE. They are not a runtime gameplay feature.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Debug")
	bool bBuildExteriorHull = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Debug")
	bool bBuildInterior = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Debug")
	bool bBuildBulkheads = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Debug")
	bool bBuildAirlock = true;

	// ── Materials ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Materials")
	TObjectPtr<UMaterialInterface> ExteriorHullMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Materials")
	TObjectPtr<UMaterialInterface> InteriorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator|Materials")
	TObjectPtr<UMaterialInterface> BulkheadMaterial;

private:
	bool BuildExteriorHull(const USubmarineDefinition* Definition);
	bool BuildInteriorCompartments(const USubmarineDefinition* Definition);
	bool BuildBulkheads(const USubmarineDefinition* Definition);

	UProceduralMeshComponent* CreatePMC(
		FName ComponentName,
		bool bVisible,
		bool bCollision,
		FName RoleTag);

	void ApplySectionToPMC(
		UProceduralMeshComponent* PMC,
		const struct FSubmarineMeshSectionData& Section,
		bool bEnableCollision);

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> RenderComponents;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> CollisionComponents;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> ExteriorHullCollisionComponents;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> InteriorFloorCollisionComponents;
};
