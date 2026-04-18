#include "Components/SubmarineBreachRuntimeComponent.h"

void USubmarineBreachRuntimeComponent::InitializeFromCompiledPartitions(const TArray<FCompiledPartitionData>& InPartitions)
{
    PartitionBindings = InPartitions;
}