#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Sub3DFloodTypes.h"
#include "SubmarineFloodRuntimeComponent.generated.h"

UCLASS(ClassGroup=(Sub3D), meta=(BlueprintSpawnableComponent))
class SUB3DRUNTIME_API USubmarineFloodRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime")
    FCompiledFloodGraph FloodGraph;

    UFUNCTION(BlueprintCallable, Category="Runtime")
    void InitializeFromFloodGraph(const FCompiledFloodGraph& InFloodGraph);
};