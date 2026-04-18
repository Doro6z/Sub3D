#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Sub3DPartitionTypes.h"
#include "SubmarineBreachRuntimeComponent.generated.h"

UCLASS(ClassGroup=(Sub3D), meta=(BlueprintSpawnableComponent))
class SUB3DRUNTIME_API USubmarineBreachRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime")
    TArray<FCompiledPartitionData> PartitionBindings;

    UFUNCTION(BlueprintCallable, Category="Runtime")
    void InitializeFromCompiledPartitions(const TArray<FCompiledPartitionData>& InPartitions);
};