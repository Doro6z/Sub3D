#include "Components/SubmarineDoorRuntimeComponent.h"

void USubmarineDoorRuntimeComponent::InitializeFromClosures(const TArray<FCompiledClosureData>& InClosures)
{
    Closures = InClosures;
}