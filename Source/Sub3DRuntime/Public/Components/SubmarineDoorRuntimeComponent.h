#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Sub3DClosureTypes.h"
#include "SubmarineDoorRuntimeComponent.generated.h"

UCLASS(ClassGroup=(Sub3D), meta=(BlueprintSpawnableComponent))
class SUB3DRUNTIME_API USubmarineDoorRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime")
    TArray<FCompiledClosureData> Closures;

    UFUNCTION(BlueprintCallable, Category="Runtime")
    void InitializeFromClosures(const TArray<FCompiledClosureData>& InClosures);
};