#include "Components/SubmarineFloodRuntimeComponent.h"

void USubmarineFloodRuntimeComponent::InitializeFromFloodGraph(const FCompiledFloodGraph& InFloodGraph)
{
    FloodGraph = InFloodGraph;
}