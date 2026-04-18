#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineBuilderComponent.generated.h"

class USub3DSubmarineAuthoringAsset;

/**
 * Pure generation logic — no UI, no mesh.
 * Generates ControlRings, FrameRings, StructuralBays from hull profile.
 * Also builds hull mesh geometry (vertices/indices) for ProceduralMesh consumption.
 */
UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class SUB3DBUILDER_API USubmarineBuilderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // ── Hull Profile ─────────────────────────────────────────────

    /** Generate N evenly-spaced ControlRings from the hull profile.
     *  Each ring's radius is computed via the HullProfileEvaluator. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Hull")
    void GenerateControlRings(USub3DSubmarineAuthoringAsset* Asset, int32 RingCount = 12);

    // ── Frame Rings ──────────────────────────────────────────────

    /** Generate FrameRings at structural spacing intervals.
     *  Compressed in bow/stern zones (15% each side).
     *  Every BayInterval-th frame is marked bIsBayBoundary. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Structure")
    void GenerateFrameRings(
        USub3DSubmarineAuthoringAsset* Asset,
        float NominalSpacingCm = 55.0f,
        int32 BayInterval = 5);

    // ── Structural Bays ──────────────────────────────────────────

    /** Generate StructuralBays from FrameRings marked bIsBayBoundary.
     *  Assigns default RoomTags based on longitudinal position. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Structure")
    void GenerateStructuralBays(USub3DSubmarineAuthoringAsset* Asset);

    // ── Deck Levels & Floor Regions ─────────────────────────────

    /** Generate one DeckLevel per StructuralBay.
     *  ZOffset is computed so the floor sits at the bottom of the hull cross-section
     *  minus a walkable clearance margin. For tall bays (midbody), adds a second deck. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Structure")
    void GenerateDeckLevels(USub3DSubmarineAuthoringAsset* Asset);

    /** Generate one FloorRegion per DeckLevel, covering the full bay span. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Structure")
    void GenerateFloorRegions(USub3DSubmarineAuthoringAsset* Asset);

    // ── Full Auto-Structure ──────────────────────────────────────

    /** One-shot: ControlRings + FrameRings + Bays + DeckLevels + FloorRegions. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Structure")
    void AutoStructure(USub3DSubmarineAuthoringAsset* Asset, int32 RingCount = 12, float FrameSpacingCm = 55.0f);

    // ── Mesh Generation ──────────────────────────────────────────

    /** Build hull mesh geometry from ControlRings.
     *  RadialSegments: number of segments around circumference (12 = fast, 32 = quality).
     *  Returns data suitable for UProceduralMeshComponent::CreateMeshSection. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Mesh")
    static void BuildHullMeshFromRings(
        const USub3DSubmarineAuthoringAsset* Asset,
        int32 RadialSegments,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals,
        TArray<FVector2D>& OutUVs);

    // ── Presets ──────────────────────────────────────────────────

    /** Apply a named preset to the asset's Hull definition, then auto-generate rings. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Builder|Presets")
    void ApplyPreset(USub3DSubmarineAuthoringAsset* Asset, FName PresetName);
};
