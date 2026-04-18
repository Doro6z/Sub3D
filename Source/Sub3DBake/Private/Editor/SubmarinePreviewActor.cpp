#include "Editor/SubmarinePreviewActor.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Bake/SubmarineAppendageBakeService.h"
#include "Bake/SubmarineHullBakeService.h"
#include "Bake/SubmarineHullProfileService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Editor/SubmarineRingHandlesComponent.h"
#include "ProceduralMeshComponent.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Types/Sub3DHullTypes.h"

namespace
{
static constexpr int32 PreviewRingCount    = 8;
static constexpr int32 PreviewRadialSegments = 12;

static void UploadMeshSection(UProceduralMeshComponent* Mesh, int32 SectionIndex,
    const FCompiledMeshSection& Section)
{
    if (!Mesh || Section.Positions.IsEmpty() || Section.Indices.IsEmpty())
    {
        if (Mesh) { Mesh->ClearMeshSection(SectionIndex); }
        return;
    }

    TArray<FVector>       Positions;
    TArray<FVector>       Normals;
    TArray<FVector2D>     UV0;
    Positions.Reserve(Section.Positions.Num());
    Normals.Reserve(Section.Normals.Num());
    UV0.Reserve(Section.UV0.Num());

    for (const FVector3f& P : Section.Positions) { Positions.Add(FVector(P)); }
    for (const FVector3f& N : Section.Normals)   { Normals.Add(FVector(N)); }
    for (const FVector2f& UV : Section.UV0)       { UV0.Add(FVector2D(UV)); }

    TArray<FLinearColor>    Colors;
    TArray<FProcMeshTangent> Tangents;
    Mesh->CreateMeshSection_LinearColor(SectionIndex, Positions, Section.Indices,
        Normals, UV0, Colors, Tangents, /*bCreateCollision=*/false);
}
} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ASubmarinePreviewActor::ASubmarinePreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(Root);
    PreviewMesh->bUseAsyncCooking = false;
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetCastShadow(false);

    RingHandles = CreateDefaultSubobject<USubmarineRingHandlesComponent>(TEXT("RingHandles"));
}

// ── InitializeForAsset ────────────────────────────────────────────────────────

void ASubmarinePreviewActor::InitializeForAsset(USub3DSubmarineAuthoringAsset* Asset)
{
    AuthoringAsset = Asset;
    if (RingHandles)
    {
        RingHandles->BindToAsset(Asset);
    }
    RefreshPreview();
}

// ── RefreshPreview ────────────────────────────────────────────────────────────

void ASubmarinePreviewActor::RefreshPreview()
{
    if (!AuthoringAsset.IsValid())
    {
        ClearPreview();
        return;
    }

    USub3DSubmarineAuthoringAsset* Asset = AuthoringAsset.Get();
    const FSubmarineHullDef& Hull = Asset->Hull;

    // ── 1. Resolve effective control rings (profile-generated or manual) ──────

    TArray<FControlRingDef> EffectiveRings;
    if (Hull.ProfileParams.Profile != ESub3DHullLongitudinalProfile::Manual)
    {
        TArray<FString> ProfileErrors;
        if (!Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(
                Hull, EffectiveRings, ProfileErrors))
        {
            EffectiveRings = Asset->ControlRings;
        }
    }
    else
    {
        EffectiveRings = Asset->ControlRings;
    }

    if (EffectiveRings.Num() < 2)
    {
        ClearPreview();
        return;
    }

    // ── 2. Build low-resolution ring sequence ─────────────────────────────────

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TArray<FString> SeqErrors;
    if (!Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
            Hull, EffectiveRings, PreviewRingCount, RingSequence, SeqErrors))
    {
        ClearPreview();
        return;
    }

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = PreviewRadialSegments;
    Settings.bBakeCollision = false;

    // ── Section 0: Exterior hull ──────────────────────────────────────────────

    FCompiledMeshSection ExteriorSection;
    if (Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(
            Hull, RingSequence, Settings, ExteriorSection))
    {
        UploadMeshSection(PreviewMesh, SectionHull, ExteriorSection);
    }
    else
    {
        PreviewMesh->ClearMeshSection(SectionHull);
    }

    // ── Section 1: Outer envelope ring chain ──────────────────────────────────

    if (Asset->OuterEnvelopeRings.Num() >= 2)
    {
        TArray<Sub3DWave2::FGeneratedRingData> EnvSequence;
        TArray<FString> EnvErrors;

        // Reuse hull def for length/defaults; envelope rings drive the shape
        if (Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
                Hull, Asset->OuterEnvelopeRings, PreviewRingCount, EnvSequence, EnvErrors))
        {
            FCompiledMeshSection EnvSection;
            if (Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(
                    Hull, EnvSequence, Settings, EnvSection))
            {
                UploadMeshSection(PreviewMesh, SectionEnvelope, EnvSection);
            }
            else
            {
                PreviewMesh->ClearMeshSection(SectionEnvelope);
            }
        }
        else
        {
            PreviewMesh->ClearMeshSection(SectionEnvelope);
        }
    }
    else
    {
        PreviewMesh->ClearMeshSection(SectionEnvelope);
    }

    // ── Section 2: Sail ───────────────────────────────────────────────────────

    {
        FCompiledMeshSection SailSection;
        if (Sub3DWave9::FSubmarineAppendageBakeService::BakeSail(
                Hull, RingSequence, Asset->Sail, SailSection))
        {
            UploadMeshSection(PreviewMesh, SectionSail, SailSection);
        }
        else
        {
            PreviewMesh->ClearMeshSection(SectionSail);
        }
    }

    // ── Section 3: Bow dome ───────────────────────────────────────────────────

    {
        FCompiledMeshSection BowSection;
        if (Sub3DWave9::FSubmarineAppendageBakeService::BakeBowDome(
                Hull, RingSequence, Asset->BowSection, BowSection))
        {
            UploadMeshSection(PreviewMesh, SectionBowDome, BowSection);
        }
        else
        {
            PreviewMesh->ClearMeshSection(SectionBowDome);
        }
    }

    // ── Section 4: Stern fairing ──────────────────────────────────────────────

    {
        FCompiledMeshSection SternSection;
        if (Sub3DWave9::FSubmarineAppendageBakeService::BakeSternFairing(
                Hull, RingSequence, Asset->SternSection, SternSection))
        {
            UploadMeshSection(PreviewMesh, SectionStern, SternSection);
        }
        else
        {
            PreviewMesh->ClearMeshSection(SectionStern);
        }
    }
}

// ── ClearPreview ──────────────────────────────────────────────────────────────

void ASubmarinePreviewActor::ClearPreview()
{
    if (PreviewMesh)
    {
        PreviewMesh->ClearAllMeshSections();
    }
}
