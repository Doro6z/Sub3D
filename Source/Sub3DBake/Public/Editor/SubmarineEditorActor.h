#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineEditorActor.generated.h"

class ASubmarineRuntimeActor;
class UCompiledSubmarineBaseAsset;
class UCompiledSubmarineRuntimeAsset;
class USceneComponent;
class USub3DSubmarineAuthoringAsset;

UCLASS(BlueprintType, Blueprintable)
class SUB3DBAKE_API ASubmarineEditorActor : public AActor
{
    GENERATED_BODY()

public:
    ASubmarineEditorActor();

    virtual bool IsEditorOnly() const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TObjectPtr<USub3DSubmarineAuthoringAsset> AuthoringAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TObjectPtr<UCompiledSubmarineBaseAsset> CompiledBaseAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TObjectPtr<UCompiledSubmarineRuntimeAsset> CompiledRuntimeAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TObjectPtr<ASubmarineRuntimeActor> RuntimeActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TSubclassOf<ASubmarineRuntimeActor> RuntimeActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine Editor")
    bool bAutoSpawnOrUpdateRuntimeActorOnFullBake = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine Editor")
    TArray<FString> LastMessages;

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool ValidateAuthoring();

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool BakeBaseAsset();

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool BakeRuntimeAsset();

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool FullBakeAssets();

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool SpawnOrUpdateRuntimeActor();

    UFUNCTION(BlueprintCallable, Category="Submarine Editor")
    bool FullBakeAndSpawnRuntimeActor();

    UFUNCTION(CallInEditor, Category="Submarine Editor", meta=(DisplayName="Validate Authoring"))
    void ValidateAuthoringNow();

    UFUNCTION(CallInEditor, Category="Submarine Editor", meta=(DisplayName="Bake Assets (Base + Runtime)"))
    void FullBakeAssetsNow();

    UFUNCTION(CallInEditor, Category="Submarine Editor", meta=(DisplayName="Bake + Spawn Runtime Actor"))
    void FullBakeAndSpawnRuntimeActorNow();

private:
    // Internal implementations that do not reset messages — called by orchestrators.
    bool BakeBaseAssetImpl();
    bool BakeRuntimeAssetImpl();
    bool SpawnOrUpdateRuntimeActorImpl();

    bool EnsureAuthoringAssetIsPersistent() const;
    UCompiledSubmarineBaseAsset* ResolveOrCreateBaseAsset();
    UCompiledSubmarineRuntimeAsset* ResolveOrCreateRuntimeAsset();
    bool CopyBaseBakeResultToPersistentAsset(UCompiledSubmarineBaseAsset* TargetAsset);
    bool CopyRuntimeBakeResultToPersistentAsset(UCompiledSubmarineRuntimeAsset* TargetAsset);
    void AddMessage(const FString& Message, bool bIsError);
    void ResetMessages();
};
