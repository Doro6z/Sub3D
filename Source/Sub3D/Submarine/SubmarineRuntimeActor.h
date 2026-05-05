#pragma once

#include "CoreMinimal.h"
#include "SubmarineBase.h"
#include "Components/SubmarineBreachRuntimeComponent.h"
#include "Components/SubmarineDoorRuntimeComponent.h"
#include "Components/SubmarineFloodRuntimeComponent.h"
#include "SubHullComponent.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"
#include "SubmarineRuntimeActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class SUB3D_API ASubmarineRuntimeActor : public ASubmarineBase
{
    GENERATED_BODY()

public:
    ASubmarineRuntimeActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine")
    TObjectPtr<UCompiledSubmarineRuntimeAsset> RuntimeAsset = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine")
    TObjectPtr<USubmarineBreachRuntimeComponent> BreachRuntimeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine")
    TObjectPtr<USubmarineFloodRuntimeComponent> FloodRuntimeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine")
    TObjectPtr<USubmarineDoorRuntimeComponent> DoorRuntimeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine")
    TArray<TObjectPtr<UProceduralMeshComponent>> RenderMeshComponents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine")
    TArray<TObjectPtr<UProceduralMeshComponent>> CollisionMeshComponents;

    UFUNCTION(BlueprintCallable, Category="Submarine|Debug")
    void DebugTriggerBreach(FVector WorldLocation, float Damage, float Radius = 50.0f);

    UFUNCTION(BlueprintCallable, Category="Submarine")
    bool LoadCompiledAsset(UCompiledSubmarineRuntimeAsset* InRuntimeAsset);

    UFUNCTION(BlueprintCallable, Category="Submarine")
    bool BuildRenderComponents();

    UFUNCTION(BlueprintCallable, Category="Submarine")
    bool BuildCollisionComponents();

    UFUNCTION(BlueprintCallable, Category="Submarine")
    bool InitializeRuntimeSystems();

    UFUNCTION(BlueprintCallable, Category="Submarine")
    bool InitializeFromRuntimeAsset();

protected:
    virtual void BeginPlay() override;

private:
    void ClearBuiltGeometry();
};
