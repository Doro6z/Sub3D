#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRingHandlesComponent.generated.h"

class USub3DSubmarineAuthoringAsset;

/**
 * Editor-only component attached to ASubmarinePreviewActor.
 * Holds the authoring asset reference and the currently selected ring index
 * so FSubmarineRingHandleVisualizer can read and modify ring data without
 * needing to cast the owning actor.
 *
 * No tick, no physics — pure data carrier for the visualizer.
 */
UCLASS(ClassGroup="Sub3D", Meta=(BlueprintSpawnableComponent), HideCategories=(Activation, Collision, Tags, Rendering))
class SUB3DBAKE_API USubmarineRingHandlesComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USubmarineRingHandlesComponent();

    void BindToAsset(USub3DSubmarineAuthoringAsset* InAsset);

    USub3DSubmarineAuthoringAsset* GetAuthoringAsset() const;

    /** Index of the ring currently highlighted/selected in the viewport. INDEX_NONE = none. */
    int32 SelectedRingIndex = INDEX_NONE;

private:
    UPROPERTY()
    TObjectPtr<USub3DSubmarineAuthoringAsset> AuthoringAsset = nullptr;
};
