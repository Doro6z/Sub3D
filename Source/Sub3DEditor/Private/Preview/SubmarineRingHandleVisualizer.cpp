#include "Preview/SubmarineRingHandleVisualizer.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Editor/SubmarinePreviewActor.h"
#include "Editor/SubmarineRingHandlesComponent.h"
#include "Types/Sub3DHullTypes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "SceneManagement.h"
#include "UnrealWidget.h"

// ── Hit proxy ─────────────────────────────────────────────────────────────────

struct HSubmarineRingHandleProxy : public HComponentVisProxy
{
    DECLARE_HIT_PROXY()

    int32 RingIndex = INDEX_NONE;

    HSubmarineRingHandleProxy(const UActorComponent* InComp, int32 InRingIndex)
        : HComponentVisProxy(InComp, HPP_Wireframe)
        , RingIndex(InRingIndex)
    {}
};
IMPLEMENT_HIT_PROXY(HSubmarineRingHandleProxy, HComponentVisProxy)

// ── Static helpers ────────────────────────────────────────────────────────────

ASubmarinePreviewActor* FSubmarineRingHandleVisualizer::FindPreviewActor(
    const USubmarineRingHandlesComponent* Comp)
{
    if (!Comp) { return nullptr; }
    return Cast<ASubmarinePreviewActor>(Comp->GetOwner());
}

// ── DrawVisualization ─────────────────────────────────────────────────────────

void FSubmarineRingHandleVisualizer::DrawVisualization(
    const UActorComponent* Component,
    const FSceneView*      View,
    FPrimitiveDrawInterface* PDI)
{
    const USubmarineRingHandlesComponent* HandlesComp =
        Cast<const USubmarineRingHandlesComponent>(Component);
    if (!HandlesComp) { return; }

    const USub3DSubmarineAuthoringAsset* Asset = HandlesComp->GetAuthoringAsset();
    if (!Asset) { return; }

    const AActor* Owner = HandlesComp->GetOwner();
    if (!Owner) { return; }

    const FTransform& ActorTransform = Owner->GetActorTransform();
    const int32 SelectedIdx = HandlesComp->SelectedRingIndex;

    for (int32 i = 0; i < Asset->ControlRings.Num(); ++i)
    {
        const FControlRingDef& Ring = Asset->ControlRings[i];
        const bool bSelected = (i == SelectedIdx);

        // Spine center of this ring in world space
        const FVector LocalCenter(Ring.PositionX, 0.0f, 0.0f);
        const FVector WorldCenter = ActorTransform.TransformPosition(LocalCenter);

        // Handle sphere radius: visible even for tiny rings
        const float HandleR = FMath::Max(Ring.HalfWidthCm * 0.12f, 25.0f);

        // Colour: selected = yellow, hovered = cyan, normal = steel blue
        const FLinearColor HandleColor = bSelected
            ? FLinearColor(1.0f, 0.92f, 0.1f, 1.0f)
            : FLinearColor(0.2f, 0.55f, 0.85f, 1.0f);

        // Register hit proxy so the handle is clickable
        PDI->SetHitProxy(new HSubmarineRingHandleProxy(Component, i));
        DrawWireSphere(PDI, WorldCenter, HandleColor, HandleR, 8, SDPG_Foreground);
        PDI->SetHitProxy(nullptr);

        // Draw the cross-section ellipse outline at this ring (thin, no hit proxy)
        const FLinearColor OutlineColor = bSelected
            ? FLinearColor(1.0f, 0.9f, 0.3f, 0.7f)
            : FLinearColor(0.15f, 0.35f, 0.6f, 0.5f);

        const float HW = Ring.HalfWidthCm;
        const float HH = Ring.HalfHeightCm;
        constexpr int32 EllipseSegs = 16;
        FVector PrevPt = ActorTransform.TransformPosition(
            FVector(Ring.PositionX, HW, 0.0f));

        for (int32 s = 1; s <= EllipseSegs; ++s)
        {
            const float Angle = (float)s / (float)EllipseSegs * TWO_PI;
            const FVector Local(Ring.PositionX,
                HW * FMath::Cos(Angle),
                HH * FMath::Sin(Angle));
            const FVector WorldPt = ActorTransform.TransformPosition(Local);
            PDI->DrawLine(PrevPt, WorldPt, OutlineColor, SDPG_Foreground, 1.0f);
            PrevPt = WorldPt;
        }
    }
}

// ── VisProxyHandleClick ───────────────────────────────────────────────────────

bool FSubmarineRingHandleVisualizer::VisProxyHandleClick(
    FEditorViewportClient*  InViewportClient,
    HComponentVisProxy*     VisProxy,
    const FViewportClick&   Click)
{
    if (HSubmarineRingHandleProxy* Proxy = HitProxyCast<HSubmarineRingHandleProxy>(VisProxy))
    {
        USubmarineRingHandlesComponent* Comp =
            Cast<USubmarineRingHandlesComponent>(const_cast<UActorComponent*>(VisProxy->Component.Get()));
        if (Comp)
        {
            EditedComponent = Comp;
            Comp->SelectedRingIndex = Proxy->RingIndex;
            return true;
        }
    }
    return false;
}

// ── HandleInputDelta ──────────────────────────────────────────────────────────

bool FSubmarineRingHandleVisualizer::HandleInputDelta(
    FEditorViewportClient* InViewportClient,
    FViewport*             InViewport,
    FVector&               InDrag,
    FRotator&              InRot,
    FVector&               InScale)
{
    if (!EditedComponent.IsValid()) { return false; }

    USubmarineRingHandlesComponent* Comp = EditedComponent.Get();
    USub3DSubmarineAuthoringAsset*  Asset = Comp->GetAuthoringAsset();
    if (!Asset) { return false; }

    const int32 Idx = Comp->SelectedRingIndex;
    if (!Asset->ControlRings.IsValidIndex(Idx)) { return false; }

    if (InDrag.IsNearlyZero()) { return false; }

    // Open an undo transaction on first drag delta
    if (!bIsDragging)
    {
        bIsDragging = true;
        GEditor->BeginTransaction(FText::FromString(TEXT("Adjust Ring Handle")));
        Asset->Modify();
    }

    FControlRingDef& Ring = Asset->ControlRings[Idx];

    // Save identifying info before we sort (ring ref will become dangling after sort).
    const FName SavedId = Ring.ControlRingId;
    const bool  bDragX  = !FMath::IsNearlyZero(InDrag.X);

    // X drag → spine position
    Ring.PositionX = FMath::Clamp(
        Ring.PositionX + InDrag.X,
        0.0f, Asset->Hull.LengthCm);

    // Y drag → half width (lateral)
    Ring.HalfWidthCm = FMath::Max(Ring.HalfWidthCm + InDrag.Y, 1.0f);

    // Z drag → half height (vertical)
    Ring.HalfHeightCm = FMath::Max(Ring.HalfHeightCm + InDrag.Z, 1.0f);

    // Keep rings ordered by PositionX after any X movement.
    if (bDragX)
    {
        const float SavedX = Asset->ControlRings[Idx].PositionX;

        Asset->ControlRings.StableSort([](const FControlRingDef& A, const FControlRingDef& B)
        {
            return A.PositionX < B.PositionX;
        });

        // Re-find the moved ring by name, or by closest X as fallback.
        int32 NewIdx = INDEX_NONE;
        if (!SavedId.IsNone())
        {
            for (int32 k = 0; k < Asset->ControlRings.Num(); ++k)
            {
                if (Asset->ControlRings[k].ControlRingId == SavedId)
                {
                    NewIdx = k;
                    break;
                }
            }
        }
        if (NewIdx == INDEX_NONE)
        {
            float BestDist = FLT_MAX;
            for (int32 k = 0; k < Asset->ControlRings.Num(); ++k)
            {
                const float Dist = FMath::Abs(Asset->ControlRings[k].PositionX - SavedX);
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    NewIdx   = k;
                }
            }
        }
        if (NewIdx != INDEX_NONE)
        {
            Comp->SelectedRingIndex = NewIdx;
        }
    }

    // Refresh viewport preview.
    if (ASubmarinePreviewActor* PreviewActor = FindPreviewActor(Comp))
    {
        PreviewActor->RefreshPreview();
    }

    return true;
}

// ── GetWidgetLocation ─────────────────────────────────────────────────────────

bool FSubmarineRingHandleVisualizer::GetWidgetLocation(
    const FEditorViewportClient* InViewportClient,
    FVector& OutLocation) const
{
    if (!EditedComponent.IsValid()) { return false; }

    const USubmarineRingHandlesComponent* Comp = EditedComponent.Get();
    const USub3DSubmarineAuthoringAsset*  Asset = Comp->GetAuthoringAsset();
    if (!Asset) { return false; }

    const int32 Idx = Comp->SelectedRingIndex;
    if (!Asset->ControlRings.IsValidIndex(Idx)) { return false; }

    const AActor* Owner = Comp->GetOwner();
    if (!Owner) { return false; }

    const FVector LocalPos(Asset->ControlRings[Idx].PositionX, 0.0f, 0.0f);
    OutLocation = Owner->GetActorTransform().TransformPosition(LocalPos);
    return true;
}

// ── EndEditing ────────────────────────────────────────────────────────────────

void FSubmarineRingHandleVisualizer::EndEditing()
{
    if (bIsDragging)
    {
        bIsDragging = false;
        GEditor->EndTransaction();
    }

    if (EditedComponent.IsValid())
    {
        EditedComponent->SelectedRingIndex = INDEX_NONE;
    }

    EditedComponent.Reset();
}
