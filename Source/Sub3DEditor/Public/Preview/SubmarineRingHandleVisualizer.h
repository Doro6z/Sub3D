#pragma once

#include "ComponentVisualizer.h"

class USubmarineRingHandlesComponent;
class ASubmarinePreviewActor;

/**
 * Component visualizer for USubmarineRingHandlesComponent.
 *
 * Draws coloured sphere handles at each control ring position on the hull spine.
 * Selected ring is yellow; others are blue.
 *
 * Interaction:
 *   Click handle → select ring (highlighted, widget appears)
 *   Drag widget X → move ring along spine (PositionX)
 *   Drag widget Y → adjust ring HalfWidthCm
 *   Drag widget Z → adjust ring HalfHeightCm
 *
 * After any modification the preview actor's RefreshPreview() is called
 * and the asset is marked dirty for undo tracking.
 */
class FSubmarineRingHandleVisualizer : public FComponentVisualizer
{
public:
    // ── FComponentVisualizer interface ────────────────────────────────────────

    virtual void DrawVisualization(
        const UActorComponent* Component,
        const FSceneView*      View,
        FPrimitiveDrawInterface* PDI) override;

    virtual bool VisProxyHandleClick(
        FEditorViewportClient* InViewportClient,
        HComponentVisProxy*    VisProxy,
        const FViewportClick&  Click) override;

    virtual bool HandleInputDelta(
        FEditorViewportClient* InViewportClient,
        FViewport*             InViewport,
        FVector&               InDrag,
        FRotator&              InRot,
        FVector&               InScale) override;

    virtual bool GetWidgetLocation(
        const FEditorViewportClient* InViewportClient,
        FVector& OutLocation) const override;

    virtual void EndEditing() override;

private:
    TWeakObjectPtr<USubmarineRingHandlesComponent> EditedComponent;

    /** True while a drag is active (transaction is open). */
    bool bIsDragging = false;

    static ASubmarinePreviewActor* FindPreviewActor(
        const USubmarineRingHandlesComponent* Comp);
};
