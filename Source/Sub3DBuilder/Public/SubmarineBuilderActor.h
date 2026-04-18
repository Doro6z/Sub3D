#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineBuilderActor.generated.h"

class USub3DSubmarineAuthoringAsset;
class USubmarineBuilderComponent;
class UProceduralMeshComponent;

/**
 * Place this actor in a level to build submarines interactively.
 * Provides live procedural mesh preview and one-click compilation to a playable submarine.
 *
 * Workflow:
 *   1. Place ASubmarineBuilderActor in level
 *   2. Assign or auto-create an AuthoringAsset
 *   3. ApplyPreset() or tweak hull parameters → mesh updates live
 *   4. AutoStructure() → generates FrameRings + Bays
 *   5. CompileToPlayable() → bake + spawn runtime actor
 */
UCLASS(BlueprintType, Blueprintable)
class SUB3DBUILDER_API ASubmarineBuilderActor : public AActor
{
    GENERATED_BODY()

public:
    ASubmarineBuilderActor();

    virtual void Tick(float DeltaTime) override;
    virtual bool ShouldTickIfViewportsOnly() const override;

    // ── Data ─────────────────────────────────────────────────────

    /** The authoring asset being edited. Created automatically if null. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Builder")
    TObjectPtr<USub3DSubmarineAuthoringAsset> AuthoringAsset;

    /** Active preset name (display only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder")
    FName ActivePresetName;

    /** Number of control rings for generation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Builder|Hull", meta=(ClampMin="2", ClampMax="64"))
    int32 RingCount = 12;

    /** Radial segments for preview mesh (12=fast, 32=quality). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Builder|Preview", meta=(ClampMin="6", ClampMax="64"))
    int32 PreviewRadialSegments = 16;

    // ── Components ───────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USubmarineBuilderComponent> BuilderLogic;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UProceduralMeshComponent> HullPreviewMesh;

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<class USubmarineRingHandlesComponent> RingHandles;
#endif

    // ── Actions (CallInEditor for details panel buttons) ─────────

    /** Apply a hull preset and rebuild preview. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Presets")
    void ApplyPreset(FName PresetName);

    /** Rebuild the procedural mesh preview from current ControlRings. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Preview")
    void RebuildPreview();

    /** Generate ControlRings from hull profile and rebuild preview. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Tooling")
    void GenerateRingsAndPreview();

    /** Full auto-structure: ControlRings + FrameRings + Bays, then rebuild preview. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Structure")
    void AutoStructure();

    // ── Quick Preset Buttons (CallInEditor) ──────────────────────

    UFUNCTION(CallInEditor, Category="Builder|Quick Presets")
    void Preset_Kilo() { ApplyPreset(FName("Kilo")); }

    UFUNCTION(CallInEditor, Category="Builder|Quick Presets")
    void Preset_Barracuda() { ApplyPreset(FName("Barracuda")); }

    UFUNCTION(CallInEditor, Category="Builder|Quick Presets")
    void Preset_Typhoon() { ApplyPreset(FName("Typhoon")); }

    UFUNCTION(CallInEditor, Category="Builder|Quick Presets")
    void Preset_CompactAIP() { ApplyPreset(FName("CompactAIP")); }

    // ── Compile ──────────────────────────────────────────────────

    /** Bake the current AuthoringAsset into a runtime-ready DataAsset. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Compile")
    void CompileToPlayable();

    /** Save the current AuthoringAsset as a persistent asset in /Game/Sub3D/Submarines/. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder|Compile")
    void SaveAuthoringAsset();

    // ── Status ───────────────────────────────────────────────────

    /** Last operation status message (shown in details panel). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    FString LastStatus = TEXT("Ready.");

    /** Number of ControlRings in the current asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    int32 NumControlRings = 0;

    /** Number of FrameRings in the current asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    int32 NumFrameRings = 0;

    /** Number of StructuralBays in the current asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    int32 NumBays = 0;

    /** Number of DeckLevels in the current asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    int32 NumDecks = 0;

    /** Number of FloorRegions in the current asset. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder|Status")
    int32 NumFloors = 0;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    uint32 LastAssetHash = 0;
    
    /** Create a transient authoring asset if none is assigned. */
    void EnsureAuthoringAsset();

    /** Refresh the stats counters from the authoring asset. */
    void UpdateStats();
};
