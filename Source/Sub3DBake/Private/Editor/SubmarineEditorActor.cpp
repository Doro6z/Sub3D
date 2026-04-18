#include "Editor/SubmarineEditorActor.h"

#include "SubmarineRuntimeActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Authoring/SubmarineAuthoringAsset.h"
#include "Bake/SubmarineBakeSubsystem.h"
#include "Components/SceneComponent.h"
#include "Data/CompiledSubmarineBaseAsset.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Misc/PackageName.h"

namespace
{
template <typename TAssetType>
TAssetType* FindOrCreateAsset(UObject* Context, USub3DSubmarineAuthoringAsset* AuthoringAsset, const TCHAR* Suffix)
{
    if (!Context || !AuthoringAsset)
    {
        return nullptr;
    }

    const FString AuthoringPackageName = AuthoringAsset->GetOutermost()->GetName();
    if (AuthoringPackageName.StartsWith(TEXT("/Temp/")) || AuthoringPackageName.StartsWith(TEXT("/Engine/Transient")))
    {
        return nullptr;
    }

    const FString PackagePath = FPackageName::GetLongPackagePath(AuthoringPackageName);
    const FString AssetName = FString::Printf(TEXT("%s%s"), *AuthoringAsset->GetName(), Suffix);
    const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);

    if (TAssetType* ExistingAsset = LoadObject<TAssetType>(nullptr, *ObjectPath))
    {
        return ExistingAsset;
    }

    const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return nullptr;
    }

    TAssetType* NewAsset = NewObject<TAssetType>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    if (!NewAsset)
    {
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(NewAsset);
    NewAsset->MarkPackageDirty();
    Package->MarkPackageDirty();

    return NewAsset;
}

void CopyBaseAssetData(const UCompiledSubmarineBaseAsset& SourceAsset, UCompiledSubmarineBaseAsset& TargetAsset)
{
    TargetAsset.HullLengthCm = SourceAsset.HullLengthCm;
    TargetAsset.HullData = SourceAsset.HullData;
    TargetAsset.ExteriorHull = SourceAsset.ExteriorHull;
    TargetAsset.InteriorHull = SourceAsset.InteriorHull;
    TargetAsset.CollisionProxy = SourceAsset.CollisionProxy;
    TargetAsset.HullOwnership = SourceAsset.HullOwnership;
    TargetAsset.FrameRings = SourceAsset.FrameRings;
    TargetAsset.OuterEnvelope = SourceAsset.OuterEnvelope;
    TargetAsset.OuterEnvelopeCollision = SourceAsset.OuterEnvelopeCollision;
    TargetAsset.StructuralBays = SourceAsset.StructuralBays;
    TargetAsset.CompiledDecks = SourceAsset.CompiledDecks;
    TargetAsset.CompiledFloorRegions = SourceAsset.CompiledFloorRegions;
    TargetAsset.CompiledOpenings = SourceAsset.CompiledOpenings;
    TargetAsset.CompiledConnectors = SourceAsset.CompiledConnectors;
    TargetAsset.CompiledClosures = SourceAsset.CompiledClosures;
    TargetAsset.CompiledPartitions = SourceAsset.CompiledPartitions;
    TargetAsset.FloodGraph = SourceAsset.FloodGraph;
}

void CopyRuntimeAssetData(const UCompiledSubmarineRuntimeAsset& SourceAsset, UCompiledSubmarineRuntimeAsset& TargetAsset)
{
    TargetAsset.HullLengthCm = SourceAsset.HullLengthCm;
    TargetAsset.HullData = SourceAsset.HullData;
    TargetAsset.ExteriorHull = SourceAsset.ExteriorHull;
    TargetAsset.InteriorHull = SourceAsset.InteriorHull;
    TargetAsset.CollisionProxy = SourceAsset.CollisionProxy;
    TargetAsset.HullOwnership = SourceAsset.HullOwnership;
    TargetAsset.FrameRings = SourceAsset.FrameRings;
    TargetAsset.OuterEnvelope = SourceAsset.OuterEnvelope;
    TargetAsset.OuterEnvelopeCollision = SourceAsset.OuterEnvelopeCollision;
    TargetAsset.StructuralBays = SourceAsset.StructuralBays;
    TargetAsset.CompiledDecks = SourceAsset.CompiledDecks;
    TargetAsset.CompiledFloorRegions = SourceAsset.CompiledFloorRegions;
    TargetAsset.CompiledOpenings = SourceAsset.CompiledOpenings;
    TargetAsset.CompiledConnectors = SourceAsset.CompiledConnectors;
    TargetAsset.CompiledClosures = SourceAsset.CompiledClosures;
    TargetAsset.CompiledPartitions = SourceAsset.CompiledPartitions;
    TargetAsset.FloodGraph = SourceAsset.FloodGraph;
}
}

ASubmarineEditorActor::ASubmarineEditorActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    RuntimeActorClass = ASubmarineRuntimeActor::StaticClass();
    bIsEditorOnlyActor = true;
}

bool ASubmarineEditorActor::IsEditorOnly() const
{
    return true;
}

bool ASubmarineEditorActor::ValidateAuthoring()
{
    ResetMessages();

    if (!AuthoringAsset)
    {
        AddMessage(TEXT("AuthoringAsset is null."), true);
        return false;
    }

    if (!GEditor)
    {
        AddMessage(TEXT("Editor context is unavailable."), true);
        return false;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        AddMessage(TEXT("USubmarineBakeSubsystem is unavailable."), true);
        return false;
    }

    TArray<FString> Errors;
    const bool bValid = BakeSubsystem->ValidateAuthoringAsset(AuthoringAsset, Errors);
    if (bValid)
    {
        AddMessage(TEXT("ValidateAuthoring succeeded."), false);
        return true;
    }

    for (const FString& Error : Errors)
    {
        AddMessage(Error, true);
    }

    return false;
}

void ASubmarineEditorActor::ValidateAuthoringNow()
{
    ValidateAuthoring();
}

bool ASubmarineEditorActor::BakeBaseAsset()
{
    ResetMessages();
    return BakeBaseAssetImpl();
}

bool ASubmarineEditorActor::BakeBaseAssetImpl()
{
    if (!EnsureAuthoringAssetIsPersistent())
    {
        AddMessage(TEXT("AuthoringAsset must be a saved content asset before baking."), true);
        return false;
    }

    if (!GEditor)
    {
        AddMessage(TEXT("Editor context is unavailable."), true);
        return false;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        AddMessage(TEXT("USubmarineBakeSubsystem is unavailable."), true);
        return false;
    }

    if (!BakeSubsystem->BakeBase(AuthoringAsset))
    {
        AddMessage(TEXT("BakeBase failed. Inspect the Output Log for detailed validation errors."), true);
        return false;
    }

    UCompiledSubmarineBaseAsset* TargetAsset = ResolveOrCreateBaseAsset();
    if (!TargetAsset)
    {
        AddMessage(TEXT("Failed to resolve or create the compiled base asset."), true);
        return false;
    }

    if (!CopyBaseBakeResultToPersistentAsset(TargetAsset))
    {
        AddMessage(TEXT("Failed to copy the base bake result into the persistent asset."), true);
        return false;
    }

    AddMessage(FString::Printf(TEXT("BakeBase: %s"), *TargetAsset->GetPathName()), false);
    return true;
}

bool ASubmarineEditorActor::BakeRuntimeAsset()
{
    ResetMessages();
    return BakeRuntimeAssetImpl();
}

bool ASubmarineEditorActor::BakeRuntimeAssetImpl()
{
    if (!EnsureAuthoringAssetIsPersistent())
    {
        AddMessage(TEXT("AuthoringAsset must be a saved content asset before baking."), true);
        return false;
    }

    if (!GEditor)
    {
        AddMessage(TEXT("Editor context is unavailable."), true);
        return false;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        AddMessage(TEXT("USubmarineBakeSubsystem is unavailable."), true);
        return false;
    }

    if (!BakeSubsystem->BakeRuntime(AuthoringAsset))
    {
        AddMessage(TEXT("BakeRuntime failed. Inspect the Output Log for detailed validation errors."), true);
        return false;
    }

    UCompiledSubmarineBaseAsset* TargetBaseAsset = ResolveOrCreateBaseAsset();
    if (TargetBaseAsset)
    {
        CopyBaseBakeResultToPersistentAsset(TargetBaseAsset);
    }

    UCompiledSubmarineRuntimeAsset* TargetRuntimeAsset = ResolveOrCreateRuntimeAsset();
    if (!TargetRuntimeAsset)
    {
        AddMessage(TEXT("Failed to resolve or create the compiled runtime asset."), true);
        return false;
    }

    if (!CopyRuntimeBakeResultToPersistentAsset(TargetRuntimeAsset))
    {
        AddMessage(TEXT("Failed to copy the runtime bake result into the persistent asset."), true);
        return false;
    }

    AddMessage(FString::Printf(TEXT("BakeRuntime: %s"), *TargetRuntimeAsset->GetPathName()), false);
    return true;
}

bool ASubmarineEditorActor::FullBakeAssets()
{
    ResetMessages();

    if (!BakeBaseAssetImpl())
    {
        return false;
    }

    if (!BakeRuntimeAssetImpl())
    {
        return false;
    }

    AddMessage(TEXT("FullBakeAssets: base + runtime succeeded."), false);
    return true;
}

void ASubmarineEditorActor::FullBakeAssetsNow()
{
    FullBakeAssets();
}

bool ASubmarineEditorActor::SpawnOrUpdateRuntimeActor()
{
    ResetMessages();
    return SpawnOrUpdateRuntimeActorImpl();
}

bool ASubmarineEditorActor::SpawnOrUpdateRuntimeActorImpl()
{
    if (!CompiledRuntimeAsset)
    {
        AddMessage(TEXT("CompiledRuntimeAsset is null. Run BakeRuntimeAsset or FullBakeAssets first."), true);
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        AddMessage(TEXT("World is unavailable."), true);
        return false;
    }

    if (!RuntimeActor)
    {
        UClass* SpawnClass = RuntimeActorClass ? RuntimeActorClass.Get() : ASubmarineRuntimeActor::StaticClass();
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        RuntimeActor = World->SpawnActor<ASubmarineRuntimeActor>(SpawnClass, GetActorTransform(), SpawnParameters);
        if (!RuntimeActor)
        {
            AddMessage(TEXT("Failed to spawn ASubmarineRuntimeActor."), true);
            return false;
        }

#if WITH_EDITOR
        RuntimeActor->SetActorLabel(FString::Printf(TEXT("%s_Runtime"), *GetActorLabel()));
#endif
    }

    RuntimeActor->SetActorTransform(GetActorTransform());
    RuntimeActor->RuntimeAsset = CompiledRuntimeAsset;

    if (!RuntimeActor->InitializeFromRuntimeAsset())
    {
        AddMessage(TEXT("Runtime actor initialization failed."), true);
        return false;
    }

    Modify();
    MarkPackageDirty();
    AddMessage(FString::Printf(TEXT("SpawnOrUpdateRuntimeActor succeeded: %s"), *RuntimeActor->GetPathName()), false);
    return true;
}

bool ASubmarineEditorActor::FullBakeAndSpawnRuntimeActor()
{
    ResetMessages();

    if (!BakeBaseAssetImpl())
    {
        return false;
    }

    if (!BakeRuntimeAssetImpl())
    {
        return false;
    }

    if (!SpawnOrUpdateRuntimeActorImpl())
    {
        return false;
    }

    AddMessage(TEXT("FullBakeAndSpawnRuntimeActor: succeeded."), false);
    return true;
}

void ASubmarineEditorActor::FullBakeAndSpawnRuntimeActorNow()
{
    FullBakeAndSpawnRuntimeActor();
}

UCompiledSubmarineBaseAsset* ASubmarineEditorActor::ResolveOrCreateBaseAsset()
{
    if (CompiledBaseAsset)
    {
        return CompiledBaseAsset;
    }

    UCompiledSubmarineBaseAsset* Asset = FindOrCreateAsset<UCompiledSubmarineBaseAsset>(this, AuthoringAsset, TEXT("_BaseCompiled"));
    if (!Asset)
    {
        return nullptr;
    }

    CompiledBaseAsset = Asset;
    Modify();
    MarkPackageDirty();
    return Asset;
}

UCompiledSubmarineRuntimeAsset* ASubmarineEditorActor::ResolveOrCreateRuntimeAsset()
{
    if (CompiledRuntimeAsset)
    {
        return CompiledRuntimeAsset;
    }

    UCompiledSubmarineRuntimeAsset* Asset = FindOrCreateAsset<UCompiledSubmarineRuntimeAsset>(this, AuthoringAsset, TEXT("_RuntimeCompiled"));
    if (!Asset)
    {
        return nullptr;
    }

    CompiledRuntimeAsset = Asset;
    Modify();
    MarkPackageDirty();
    return Asset;
}

bool ASubmarineEditorActor::EnsureAuthoringAssetIsPersistent() const
{
    return AuthoringAsset
        && AuthoringAsset->GetOutermost()
        && !AuthoringAsset->GetOutermost()->GetName().StartsWith(TEXT("/Temp/"))
        && !AuthoringAsset->GetOutermost()->GetName().StartsWith(TEXT("/Engine/Transient"));
}

bool ASubmarineEditorActor::CopyBaseBakeResultToPersistentAsset(UCompiledSubmarineBaseAsset* TargetAsset)
{
    if (!GEditor || !TargetAsset)
    {
        return false;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem || !BakeSubsystem->LastBakedBaseAsset)
    {
        return false;
    }

    TargetAsset->Modify();
    CopyBaseAssetData(*BakeSubsystem->LastBakedBaseAsset, *TargetAsset);
    TargetAsset->MarkPackageDirty();
    if (UPackage* Package = TargetAsset->GetOutermost())
    {
        Package->MarkPackageDirty();
    }

    return true;
}

bool ASubmarineEditorActor::CopyRuntimeBakeResultToPersistentAsset(UCompiledSubmarineRuntimeAsset* TargetAsset)
{
    if (!GEditor || !TargetAsset)
    {
        return false;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem || !BakeSubsystem->LastBakedRuntimeAsset)
    {
        return false;
    }

    TargetAsset->Modify();
    CopyRuntimeAssetData(*BakeSubsystem->LastBakedRuntimeAsset, *TargetAsset);
    TargetAsset->MarkPackageDirty();
    if (UPackage* Package = TargetAsset->GetOutermost())
    {
        Package->MarkPackageDirty();
    }

    return true;
}

void ASubmarineEditorActor::AddMessage(const FString& Message, const bool bIsError)
{
    LastMessages.Add(Message);
    if (bIsError)
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
}

void ASubmarineEditorActor::ResetMessages()
{
    LastMessages.Reset();
}
