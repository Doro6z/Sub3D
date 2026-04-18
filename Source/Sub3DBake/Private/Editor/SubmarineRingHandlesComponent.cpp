#include "Editor/SubmarineRingHandlesComponent.h"

USubmarineRingHandlesComponent::USubmarineRingHandlesComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bIsEditorOnly = true;
}

void USubmarineRingHandlesComponent::BindToAsset(USub3DSubmarineAuthoringAsset* InAsset)
{
    AuthoringAsset   = InAsset;
    SelectedRingIndex = INDEX_NONE;
}

USub3DSubmarineAuthoringAsset* USubmarineRingHandlesComponent::GetAuthoringAsset() const
{
    return AuthoringAsset;
}
