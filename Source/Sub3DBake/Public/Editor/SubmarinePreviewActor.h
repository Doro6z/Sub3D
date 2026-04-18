#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarinePreviewActor.generated.h"

class USub3DSubmarineAuthoringAsset;
class UProceduralMeshComponent;
class USceneComponent;
class USubmarineRingHandlesComponent;

/**
 * Editor-only actor that renders a low-resolution preview hull driven directly
 * from a USub3DSubmarineAuthoringAsset.  Updated via RefreshPreview() without
 * running the full bake pipeline.
 *
 * Mesh sections:
 *   0 — Exterior hull (pressure hull, always baked)
 *   1 — Outer envelope ring chain (if OuterEnvelopeRings.Num() >= 2)
 *   2 — Sail / fin
 *   3 — Bow dome
 *   4 — Stern fairing
 *
 * Spawned by FSubmarineEditorToolkit when the user clicks Preview Layout.
 */
UCLASS(NotBlueprintable)
class SUB3DBAKE_API ASubmarinePreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ASubmarinePreviewActor();

    virtual bool IsEditorOnly() const override { return true; }

    /** Bind to an authoring asset and immediately rebuild all sections. */
    void InitializeForAsset(USub3DSubmarineAuthoringAsset* Asset);

    /**
     * Rebuild all preview mesh sections from the current asset state.
     * Uses 8 rings / 12 radial segments for real-time performance.
     * Safe to call on every property change.
     */
    void RefreshPreview();

    /** Clear all mesh sections without destroying the actor. */
    void ClearPreview();

    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USceneComponent> Root;

    /** All preview geometry — multiple sections, no collision. */
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UProceduralMeshComponent> PreviewMesh;

    /** Data carrier for FSubmarineRingHandleVisualizer. */
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USubmarineRingHandlesComponent> RingHandles;

private:
    TWeakObjectPtr<USub3DSubmarineAuthoringAsset> AuthoringAsset;

    static constexpr int32 SectionHull     = 0;
    static constexpr int32 SectionEnvelope = 1;
    static constexpr int32 SectionSail     = 2;
    static constexpr int32 SectionBowDome  = 3;
    static constexpr int32 SectionStern    = 4;
};
