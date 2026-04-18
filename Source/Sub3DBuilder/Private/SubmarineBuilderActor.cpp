#include "SubmarineBuilderActor.h"

#include "SubmarineBuilderComponent.h"
#include "Authoring/SubmarineAuthoringAsset.h"
#include "ProceduralMeshComponent.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"
#include "SubmarineRuntimeActor.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Bake/SubmarineBakeSubsystem.h"
#include "Editor/SubmarineRingHandlesComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Data/CompiledSubmarineBaseAsset.h"
#endif

ASubmarineBuilderActor::ASubmarineBuilderActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    HullPreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HullPreviewMesh"));
    HullPreviewMesh->SetupAttachment(SceneRoot);
    HullPreviewMesh->bUseComplexAsSimpleCollision = false;
    HullPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BuilderLogic = CreateDefaultSubobject<USubmarineBuilderComponent>(TEXT("BuilderLogic"));

#if WITH_EDITORONLY_DATA
    RingHandles = CreateDefaultSubobject<USubmarineRingHandlesComponent>(TEXT("RingHandles"));
#endif
}

void ASubmarineBuilderActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    EnsureAuthoringAsset();

#if WITH_EDITORONLY_DATA
    if (RingHandles && AuthoringAsset)
    {
        RingHandles->BindToAsset(AuthoringAsset);
    }
#endif

    if (AuthoringAsset && AuthoringAsset->ControlRings.Num() >= 2)
    {
        RebuildPreview();
    }
}

#if WITH_EDITOR
void ASubmarineBuilderActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropName = PropertyChangedEvent.GetPropertyName();

    if (PropName == GET_MEMBER_NAME_CHECKED(ASubmarineBuilderActor, RingCount) ||
        PropName == GET_MEMBER_NAME_CHECKED(ASubmarineBuilderActor, PreviewRadialSegments))
    {
        GenerateRingsAndPreview();
    }
}
#endif

void ASubmarineBuilderActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

#if WITH_EDITOR
    if (AuthoringAsset && AuthoringAsset->ControlRings.Num() > 0)
    {
        uint32 CurrentHash = FCrc::MemCrc32(AuthoringAsset->ControlRings.GetData(), AuthoringAsset->ControlRings.Num() * sizeof(FControlRingDef));
        if (CurrentHash != LastAssetHash)
        {
            LastAssetHash = CurrentHash;
            RebuildPreview();
        }
    }
#endif
}

bool ASubmarineBuilderActor::ShouldTickIfViewportsOnly() const
{
    return true;
}

// ─── Asset Management ────────────────────────────────────────────────────────

void ASubmarineBuilderActor::EnsureAuthoringAsset()
{
    if (AuthoringAsset) return;

    AuthoringAsset = NewObject<USub3DSubmarineAuthoringAsset>(
        this,
        USub3DSubmarineAuthoringAsset::StaticClass(),
        FName("TransientAuthoringAsset"),
        RF_Transient);

    if (BuilderLogic)
    {
        BuilderLogic->ApplyPreset(AuthoringAsset, FName("Kilo"));
        ActivePresetName = FName("Kilo");
    }
}

void ASubmarineBuilderActor::SaveAuthoringAsset()
{
#if WITH_EDITOR
    if (!AuthoringAsset)
    {
        LastStatus = TEXT("No authoring asset to save.");
        return;
    }

    // If the asset is already persistent, just mark it dirty
    const FString PackageName = AuthoringAsset->GetOutermost()->GetName();
    if (!PackageName.StartsWith(TEXT("/Temp/")) && !PackageName.Contains(TEXT("Transient")))
    {
        AuthoringAsset->MarkPackageDirty();
        LastStatus = FString::Printf(TEXT("Asset marked dirty: %s"), *PackageName);
        return;
    }

    // Save transient asset as persistent
    const FString AssetName = ActivePresetName.IsNone()
        ? TEXT("DA_NewSubmarine_Auth")
        : FString::Printf(TEXT("DA_%s_Auth"), *ActivePresetName.ToString());
    const FString TargetPackageName = FString::Printf(TEXT("/Game/Sub3D/Submarines/%s"), *AssetName);

    UPackage* Package = CreatePackage(*TargetPackageName);
    if (!Package)
    {
        LastStatus = TEXT("Failed to create package.");
        return;
    }

    USub3DSubmarineAuthoringAsset* SavedAsset = DuplicateObject<USub3DSubmarineAuthoringAsset>(
        AuthoringAsset, Package, *AssetName);
    SavedAsset->SetFlags(RF_Public | RF_Standalone);
    SavedAsset->ClearFlags(RF_Transient);

    FAssetRegistryModule::AssetCreated(SavedAsset);
    Package->MarkPackageDirty();

    // Now point to the persistent asset
    AuthoringAsset = SavedAsset;

    LastStatus = FString::Printf(TEXT("Saved: %s"), *TargetPackageName);
    UE_LOG(LogTemp, Log, TEXT("SubmarineBuilder: %s"), *LastStatus);
#else
    LastStatus = TEXT("SaveAuthoringAsset is editor-only.");
#endif
}

// ─── Actions ─────────────────────────────────────────────────────────────────

void ASubmarineBuilderActor::ApplyPreset(FName PresetName)
{
    EnsureAuthoringAsset();
    if (!BuilderLogic || !AuthoringAsset) return;

    BuilderLogic->ApplyPreset(AuthoringAsset, PresetName);
    ActivePresetName = PresetName;
    RebuildPreview();
    UpdateStats();

    LastStatus = FString::Printf(TEXT("Applied preset: %s"), *PresetName.ToString());
}

void ASubmarineBuilderActor::RebuildPreview()
{
    if (!AuthoringAsset || !HullPreviewMesh) return;

    if (AuthoringAsset->ControlRings.Num() < 2)
    {
        HullPreviewMesh->ClearAllMeshSections();
        return;
    }

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;

    USubmarineBuilderComponent::BuildHullMeshFromRings(
        AuthoringAsset, PreviewRadialSegments,
        Vertices, Triangles, Normals, UVs);

    if (Vertices.Num() == 0)
    {
        HullPreviewMesh->ClearAllMeshSections();
        return;
    }

    HullPreviewMesh->ClearAllMeshSections();
    HullPreviewMesh->CreateMeshSection(
        0, Vertices, Triangles, Normals, UVs,
        TArray<FColor>(), TArray<FProcMeshTangent>(),
        false);

    UpdateStats();
}

void ASubmarineBuilderActor::GenerateRingsAndPreview()
{
    EnsureAuthoringAsset();
    if (!BuilderLogic || !AuthoringAsset) return;

    BuilderLogic->GenerateControlRings(AuthoringAsset, RingCount);
    RebuildPreview();

    LastStatus = FString::Printf(TEXT("Generated %d rings."), RingCount);
}

void ASubmarineBuilderActor::AutoStructure()
{
    EnsureAuthoringAsset();
    if (!BuilderLogic || !AuthoringAsset) return;

    BuilderLogic->AutoStructure(AuthoringAsset, RingCount);
    RebuildPreview();

    LastStatus = FString::Printf(TEXT("AutoStructure: %d rings, %d frames, %d bays, %d decks, %d floors"),
        AuthoringAsset->ControlRings.Num(),
        AuthoringAsset->FrameRings.Num(),
        AuthoringAsset->StructuralBays.Num(),
        AuthoringAsset->DeckLevels.Num(),
        AuthoringAsset->FloorRegions.Num());

    UE_LOG(LogTemp, Log, TEXT("SubmarineBuilder: %s"), *LastStatus);
}

// ─── Compile to Playable ─────────────────────────────────────────────────────

void ASubmarineBuilderActor::CompileToPlayable()
{
#if WITH_EDITOR
    EnsureAuthoringAsset();
    if (!AuthoringAsset)
    {
        LastStatus = TEXT("No authoring asset.");
        return;
    }

    // Step 1: Ensure asset is persistent (bake requires it)
    const FString PkgName = AuthoringAsset->GetOutermost()->GetName();
    if (PkgName.StartsWith(TEXT("/Temp/")) || PkgName.Contains(TEXT("Transient")))
    {
        SaveAuthoringAsset();
        if (!AuthoringAsset || AuthoringAsset->GetOutermost()->GetName().Contains(TEXT("Transient")))
        {
            LastStatus = TEXT("Failed to save authoring asset before compile.");
            return;
        }
    }

    // Step 2: Ensure we have structure
    if (AuthoringAsset->ControlRings.Num() < 2)
    {
        if (BuilderLogic)
        {
            BuilderLogic->AutoStructure(AuthoringAsset, RingCount);
        }
    }

    // Step 3: Confirm hull geometry
    AuthoringAsset->bHullGeometryConfirmed = true;
    AuthoringAsset->HullGeometryHash = static_cast<int32>(
        FCrc::MemCrc32(&AuthoringAsset->Hull, sizeof(FSubmarineHullDef)));

    // Step 4: Bake
    if (!GEditor)
    {
        LastStatus = TEXT("Editor context unavailable.");
        return;
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        LastStatus = TEXT("BakeSubsystem unavailable.");
        return;
    }

    if (!BakeSubsystem->FullBake(AuthoringAsset))
    {
        LastStatus = TEXT("FullBake FAILED — check Output Log.");
        UE_LOG(LogTemp, Error, TEXT("SubmarineBuilder: FullBake failed for %s"), *AuthoringAsset->GetName());
        return;
    }

    // Step 5: Retrieve compiled assets from subsystem
    UCompiledSubmarineRuntimeAsset* TransRuntimeAsset = BakeSubsystem->LastBakedRuntimeAsset;
    UCompiledSubmarineBaseAsset* TransBaseAsset = BakeSubsystem->LastBakedBaseAsset;
    if (!TransRuntimeAsset || !TransBaseAsset)
    {
        LastStatus = TEXT("FullBake succeeded but transient assets are missing.");
        return;
    }

    FString PkgPath = FPackageName::GetLongPackagePath(AuthoringAsset->GetOutermost()->GetName());

    // Save Base Asset
    FString BaseName = FString::Printf(TEXT("%s_BaseCompiled"), *AuthoringAsset->GetName());
    UPackage* BasePkg = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PkgPath, *BaseName));
    if (UCompiledSubmarineBaseAsset* SavedBaseAsset = DuplicateObject<UCompiledSubmarineBaseAsset>(TransBaseAsset, BasePkg, *BaseName))
    {
        SavedBaseAsset->SetFlags(RF_Public | RF_Standalone);
        SavedBaseAsset->ClearFlags(RF_Transient);
        FAssetRegistryModule::AssetCreated(SavedBaseAsset);
        BasePkg->MarkPackageDirty();
    }

    // Save Runtime Asset
    FString RuntimeName = FString::Printf(TEXT("%s_RuntimeCompiled"), *AuthoringAsset->GetName());
    UPackage* RuntimePkg = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PkgPath, *RuntimeName));
    if (UCompiledSubmarineRuntimeAsset* SavedRuntimeAsset = DuplicateObject<UCompiledSubmarineRuntimeAsset>(TransRuntimeAsset, RuntimePkg, *RuntimeName))
    {
        SavedRuntimeAsset->SetFlags(RF_Public | RF_Standalone);
        SavedRuntimeAsset->ClearFlags(RF_Transient);
        FAssetRegistryModule::AssetCreated(SavedRuntimeAsset);
        RuntimePkg->MarkPackageDirty();
        
        LastStatus = FString::Printf(TEXT("Successfully baked %s. Ready to be used in Blueprint!"), *SavedRuntimeAsset->GetName());
    }
    else
    {
        LastStatus = TEXT("Failed to save Runtime Asset package.");
    }

    UE_LOG(LogTemp, Log, TEXT("SubmarineBuilder: %s"), *LastStatus);
#else
    LastStatus = TEXT("CompileToPlayable is editor-only.");
#endif
}

// ─── Stats ───────────────────────────────────────────────────────────────────

void ASubmarineBuilderActor::UpdateStats()
{
    if (AuthoringAsset)
    {
        NumControlRings = AuthoringAsset->ControlRings.Num();
        NumFrameRings = AuthoringAsset->FrameRings.Num();
        NumBays = AuthoringAsset->StructuralBays.Num();
        NumDecks = AuthoringAsset->DeckLevels.Num();
        NumFloors = AuthoringAsset->FloorRegions.Num();
    }
    else
    {
        NumControlRings = 0;
        NumFrameRings = 0;
        NumBays = 0;
        NumDecks = 0;
        NumFloors = 0;
    }
}
