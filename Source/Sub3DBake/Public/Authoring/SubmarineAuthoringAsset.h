#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/Sub3DAppendageTypes.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DClosureTypes.h"
#include "Types/Sub3DConnectorTypes.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Types/Sub3DHullTypes.h"
#include "Types/Sub3DOpeningTypes.h"
#include "Types/Sub3DPartitionTypes.h"
#include "SubmarineAuthoringAsset.generated.h"

UCLASS(BlueprintType)
class SUB3DBAKE_API USub3DSubmarineAuthoringAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ─── [A] Hull Geometry ────────────────────────────────────────────────────
    // Lock bHullGeometryConfirmed = true before advancing to [B] authoring.

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Pressure Hull")
    FSubmarineHullDef Hull;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Pressure Hull")
    TArray<FControlRingDef> ControlRings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Pressure Hull")
    TArray<FFrameRingDef> FrameRings;

    // ─── [A] Appendages ───────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages")
    FSailDef Sail;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages")
    FBowSectionDef BowSection;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages")
    FSternSectionDef SternSection;

    // ─── [A] Outer Envelope ───────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope")
    FOuterEnvelopeDef OuterEnvelope;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope")
    FOuterCasingDef OuterCasing;

    /** Secondary ring chain that defines the outer envelope silhouette.
     *  Authored and ordered exactly like ControlRings but drives the
     *  fairwater / casing outer surface, not the pressure hull. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope")
    TArray<FControlRingDef> OuterEnvelopeRings;

    // ─── [A] Bake State ───────────────────────────────────────────────────────

    /** When true, hull geometry (Layer A) is considered confirmed.
     *  Layer B authoring (bays, decks) is only reliable after this is set. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Bake State")
    bool bHullGeometryConfirmed = false;

    /** CRC32 of Hull + ControlRings at the time bHullGeometryConfirmed was set.
     *  If this diverges from the live asset, Layer B data may be stale. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="[A] Bake State")
    int32 HullGeometryHash = 0;

    // ─── [B] Structure ────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[B] Structural Bays")
    TArray<FStructuralBayDef> StructuralBays;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[B] Deck Levels")
    TArray<FDeckLevelDef> DeckLevels;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[B] Floor Regions")
    TArray<FFloorRegionDef> FloorRegions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[B] Openings")
    TArray<FOpeningDef> Openings;

    /** CRC32 of StructuralBays + DeckLevels.
     *  Layer C data (partitions, connectors) may be stale if this changes. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="[B] Bake State")
    int32 LayoutHash = 0;

    // ─── [C] Fit-Out ──────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[C] Connectors")
    TArray<FConnectorDef> Connectors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[C] Closures")
    TArray<FClosureDef> Closures;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[C] Partitions")
    TArray<FPressureBulkheadDef> PressureBulkheads;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[C] Partitions")
    TArray<FInternalWallDef> InternalWalls;
};
