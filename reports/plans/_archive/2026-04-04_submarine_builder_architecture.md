# Submarine Builder — Architecture Complète

**Date:** 2026-04-04
**Objectif:** Remplacer le FSubmarineEditorToolkit (9 onglets Slate) par un actor-dans-le-level + widget UMG. Flux cible : ouvrir un level → voir le sous-marin → slider → bake → jouer.

---

## 1. Vue d'ensemble du flux

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                    BUILDER LEVEL                                │
  │                                                                 │
  │   ASubmarineBuilderActor                                        │
  │   ├── UProceduralMeshComponent  (hull live preview)             │
  │   ├── UProceduralMeshComponent  (envelope preview)              │
  │   ├── UStaticMeshComponent[]    (bow, stern, sail, propeller)   │
  │   ├── USubmarineBuilderComponent (logique de génération)        │
  │   └── USubmarineBuilderWidget   (UMG - panneau latéral)         │
  │                                                                 │
  │   Widget UMG (docked viewport gauche) :                         │
  │   ┌───────────────────────────────┐                             │
  │   │ [Kilo] [Barracuda] [Typhoon] │  ← Preset buttons           │
  │   │                               │                             │
  │   │ Length ════════════○ 73.0m    │  ← Slider → live rebuild    │
  │   │ Diameter ══════════○ 8.7m    │                              │
  │   │ Profile  [Myring ▼]          │                              │
  │   │ Midbody  ══════════○ 40%     │                              │
  │   │                               │                             │
  │   │ ── Rings ──────────────────── │                             │
  │   │ Count    ══════════○ 12      │  ← Auto-generates on change │
  │   │ [Ring 03] Pos:24.3 R:4.1     │  ← Sélectionnable, editable │
  │   │ [Ring 04] Pos:30.5 R:4.35    │     (highlight dans viewport)│
  │   │                               │                             │
  │   │ ── Structure ──────────────── │                             │
  │   │ Frame spacing ═════○ 55cm    │                              │
  │   │ [Auto-Structure]              │  ← Génère FrameRings + Bays │
  │   │                               │                             │
  │   │ ── Stations ───────────────── │                             │
  │   │ [+Helm] [+Ballast] [+Engine] │  ← Place dans le bay le +   │
  │   │ [+Turret]                     │     approprié               │
  │   │ Helm → Bay 03 (Command)      │  ← Liste des stations posées│
  │   │ Ballast → Bay 07 (Aft)       │                              │
  │   │                               │                             │
  │   │ ══════════════════════════════ │                             │
  │   │ [ Compile → Playable ]        │  ← Bake + spawn SubBase    │
  │   └───────────────────────────────┘                             │
  └─────────────────────────────────────────────────────────────────┘
```

---

## 2. Modules et fichiers

### Module Sub3DBuilder (NOUVEAU)

Ce module contient tout le builder. Il dépend de Sub3DCore, Sub3DBake, Sub3DRuntime, UMG, ProceduralMeshComponent.

```
Source/Sub3DBuilder/
├── Sub3DBuilder.Build.cs
├── Public/
│   ├── SubmarineBuilderActor.h
│   ├── SubmarineBuilderComponent.h
│   ├── SubmarineBuilderWidget.h
│   ├── Generation/
│   │   ├── HullMeshGenerator.h          ← ProceduralMesh depuis ControlRings
│   │   ├── FrameRingGenerator.h         ← Auto-dérivation des FrameRings
│   │   ├── BayGenerator.h               ← Auto-dérivation des Bays depuis FrameRings
│   │   ├── StationPlacer.h              ← Placement auto des stations dans les bays
│   │   └── HullProfileEvaluator.h       ← Évalue Myring/Series58/Superellipse à un X donné
│   └── Presets/
│       └── SubmarinePresetLibrary.h      ← DataAsset avec les presets (Kilo, Barracuda, etc.)
├── Private/
│   ├── SubmarineBuilderActor.cpp
│   ├── SubmarineBuilderComponent.cpp
│   ├── SubmarineBuilderWidget.cpp
│   ├── Generation/
│   │   ├── HullMeshGenerator.cpp
│   │   ├── FrameRingGenerator.cpp
│   │   ├── BayGenerator.cpp
│   │   ├── StationPlacer.cpp
│   │   └── HullProfileEvaluator.cpp
│   └── Presets/
│       └── SubmarinePresetLibrary.cpp
```

### Fichiers existants à conserver tels quels

| Fichier | Raison |
|---------|--------|
| `USub3DSubmarineAuthoringAsset` | Source de vérité des données — le Builder écrit dedans |
| `USubmarineBakeSubsystem` | Pipeline de compilation — le Builder l'appelle |
| `ASubmarineRuntimeActor` | Résultat du bake — utilisé par SubmarineBase |
| `ASubmarineBase` + tous les stations | Gameplay pur — ne change pas |
| Tous les types (Hull, Bay, Floor, etc.) | Structures de données — ne changent pas |

### Fichiers existants à RETIRER à terme

| Fichier | Remplacé par |
|---------|-------------|
| `FSubmarineEditorToolkit` (.h + .cpp) | `ASubmarineBuilderActor` + `USubmarineBuilderWidget` |
| `ASubmarineEditorActor` | `ASubmarineBuilderActor` (fusionne preview + editor) |
| `ASubmarinePreviewActor` | Intégré dans `ASubmarineBuilderActor` |
| `SSubmarineHullStatsBar` | Intégré dans `USubmarineBuilderWidget` |
| `SSubmarineAppendagesPanel` | Intégré dans `USubmarineBuilderWidget` |

---

## 3. Classes — Spécification détaillée

### 3.1 ASubmarineBuilderActor

```cpp
UCLASS(BlueprintType)
class SUB3DBUILDER_API ASubmarineBuilderActor : public AActor
{
    GENERATED_BODY()

public:
    ASubmarineBuilderActor();

    // ── Données ──────────────────────────────────────────────────

    /** L'asset authoring qui est édité. Créé automatiquement si null. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Builder")
    TObjectPtr<USub3DSubmarineAuthoringAsset> AuthoringAsset;

    /** Le preset actuellement appliqué (pour affichage dans le widget). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Builder")
    FName ActivePresetName;

    // ── Components ───────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USubmarineBuilderComponent> BuilderLogic;

    /** Hull preview mesh — mis à jour en temps réel. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UProceduralMeshComponent> HullPreviewMesh;

    /** Envelope preview mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UProceduralMeshComponent> EnvelopePreviewMesh;

    /** Bow mesh (pre-made ou procédural). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMeshComponent> BowMesh;

    /** Stern + propeller mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMeshComponent> SternMesh;

    /** Sail / kiosque mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMeshComponent> SailMesh;

    // ── API publique ─────────────────────────────────────────────

    /** Applique un preset. Réécrit l'AuthoringAsset, régénère tout. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder")
    void ApplyPreset(FName PresetName);

    /** Régénère le mesh preview depuis l'AuthoringAsset courant. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder")
    void RebuildPreview();

    /** Auto-structure : génère FrameRings + Bays depuis le hull profile. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder")
    void AutoStructure();

    /** Place une station du type donné dans le bay le plus approprié. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder")
    void PlaceStation(ESubStationType StationType);

    /** Full bake → spawn ASubmarineBase jouable dans ce level. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Builder")
    ASubmarineBase* CompileToPlayable();

    // ── Callbacks ────────────────────────────────────────────────

    /** Appelé par le widget quand un slider change. */
    void OnHullParameterChanged();

    /** Appelé par le widget quand un ring est sélectionné. */
    void OnRingSelected(int32 RingIndex);

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Crée l'AuthoringAsset si aucun n'est assigné. */
    void EnsureAuthoringAsset();

    /** Spawn le widget UMG et l'attache au viewport. */
    void SpawnBuilderWidget();

private:
    UPROPERTY()
    TObjectPtr<USubmarineBuilderWidget> ActiveWidget;

    /** Index du ring actuellement sélectionné (-1 = aucun). */
    int32 SelectedRingIndex = -1;
};
```

### 3.2 USubmarineBuilderComponent

Logique pure — pas de Slate, pas de mesh. Fonctions de génération réutilisables.

```cpp
UCLASS(BlueprintType)
class SUB3DBUILDER_API USubmarineBuilderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // ── Hull Profile Evaluation ──────────────────────────────────

    /** Évalue le rayon du hull à une position X donnée (0..LengthCm).
     *  Utilise le HullProfileParams de l'asset. */
    UFUNCTION(BlueprintCallable, Category="Builder|Hull")
    float EvaluateHullRadiusAtX(
        const USub3DSubmarineAuthoringAsset* Asset,
        float PositionX) const;

    /** Génère N ControlRings uniformément répartis depuis le hull profile. */
    UFUNCTION(BlueprintCallable, Category="Builder|Hull")
    void GenerateControlRings(
        USub3DSubmarineAuthoringAsset* Asset,
        int32 RingCount);

    // ── Frame Rings ──────────────────────────────────────────────

    /** Génère les FrameRings selon un espacement structurel.
     *  Règles :
     *  - Espacement nominal dans le midbody (ex: 55cm)
     *  - Compression de 60% dans les 15% bow et 15% stern
     *  - Un FrameRing tombe forcément sur chaque ControlRing position
     *  - Les FrameRings marqués bIsBayBoundary aux positions structurelles clés */
    UFUNCTION(BlueprintCallable, Category="Builder|Structure")
    void GenerateFrameRings(
        USub3DSubmarineAuthoringAsset* Asset,
        float NominalSpacingCm = 55.0f);

    // ── Structural Bays ──────────────────────────────────────────

    /** Génère les StructuralBays depuis les FrameRings marqués bIsBayBoundary.
     *  Assign un ESub3DRoomTag par défaut basé sur la position :
     *  - 0-10% : Ballast (bow trim)
     *  - 10-20% : Navigation/Sonar
     *  - 20-35% : Habitat (crew quarters)
     *  - 35-50% : Command
     *  - 50-65% : Machine (reactor/engine)
     *  - 65-80% : Storage/Auxiliary
     *  - 80-90% : Machine (propulsion)
     *  - 90-100% : Ballast (aft trim) */
    UFUNCTION(BlueprintCallable, Category="Builder|Structure")
    void GenerateStructuralBays(USub3DSubmarineAuthoringAsset* Asset);

    // ── Station Placement ────────────────────────────────────────

    /** Trouve le bay le plus approprié pour un type de station.
     *  Logique :
     *  - Helm → premier bay tagué Command
     *  - Ballast → bay tagué Ballast (ou Machine si absent)
     *  - Engine → bay tagué Machine
     *  - Turret → attache au Sail (pas un bay)
     *  Retourne l'index du bay, ou -1 si aucun trouvé. */
    UFUNCTION(BlueprintCallable, Category="Builder|Stations")
    int32 FindBestBayForStation(
        const USub3DSubmarineAuthoringAsset* Asset,
        ESubStationType StationType) const;

    // ── Mesh Generation ──────────────────────────────────────────

    /** Construit les vertices/indices pour le hull ProceduralMesh.
     *  Résolution adaptative :
     *  - Preview rapide : 8 rings interpolés × 12 radial segments
     *  - Preview final  : 1 segment par ControlRing × 32 radial segments */
    UFUNCTION(BlueprintCallable, Category="Builder|Mesh")
    void BuildHullMesh(
        const USub3DSubmarineAuthoringAsset* Asset,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals,
        TArray<FVector2D>& OutUVs,
        bool bHighQuality = false) const;

    /** Construit la géométrie de l'envelope/casing. */
    UFUNCTION(BlueprintCallable, Category="Builder|Mesh")
    void BuildEnvelopeMesh(
        const USub3DSubmarineAuthoringAsset* Asset,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector>& OutNormals,
        TArray<FVector2D>& OutUVs) const;

    // ── Compile ──────────────────────────────────────────────────

    /** Appelle le BakeSubsystem : Validate → BakeBase → BakeRuntime.
     *  Retourne true si tout a réussi. */
    UFUNCTION(BlueprintCallable, Category="Builder|Compile")
    bool CompileAuthoringAsset(
        USub3DSubmarineAuthoringAsset* Asset,
        UCompiledSubmarineBaseAsset*& OutBase,
        UCompiledSubmarineRuntimeAsset*& OutRuntime);
};
```

### 3.3 USubmarineBuilderWidget (UMG)

```cpp
UCLASS(BlueprintType)
class SUB3DBUILDER_API USubmarineBuilderWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Référence vers le BuilderActor qui possède ce widget. */
    UPROPERTY(BlueprintReadOnly, meta=(ExposeOnSpawn="true"))
    TObjectPtr<ASubmarineBuilderActor> OwningBuilder;

    // ── Bindings vers les contrôles UMG ──────────────────────────

    // Ces noms correspondent aux widgets dans le WBP_SubmarineBuilder Blueprint.
    // Le layout est défini dans le UMG Designer, pas en C++.

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_PresetKilo;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_PresetBarracuda;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_PresetTyphoon;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_PresetCompactAIP;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class USlider> Slider_Length;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class USlider> Slider_Diameter;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class USlider> Slider_Midbody;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UComboBoxString> Combo_Profile;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class USpinBox> Spin_RingCount;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class USlider> Slider_FrameSpacing;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_AutoStructure;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_AddHelm;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_AddBallast;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_AddEngine;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_AddTurret;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UVerticalBox> Box_StationList;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UVerticalBox> Box_RingList;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UTextBlock> Text_Stats;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<class UButton> Btn_CompilePlayable;

    // ── Lifecycle ────────────────────────────────────────────────

    virtual void NativeConstruct() override;

protected:
    // ── Handlers ─────────────────────────────────────────────────

    UFUNCTION() void OnPresetKilo();
    UFUNCTION() void OnPresetBarracuda();
    UFUNCTION() void OnPresetTyphoon();
    UFUNCTION() void OnPresetCompactAIP();

    UFUNCTION() void OnLengthChanged(float Value);
    UFUNCTION() void OnDiameterChanged(float Value);
    UFUNCTION() void OnMidbodyChanged(float Value);
    UFUNCTION() void OnProfileChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnRingCountChanged(float InValue);

    UFUNCTION() void OnAutoStructure();
    UFUNCTION() void OnAddHelm();
    UFUNCTION() void OnAddBallast();
    UFUNCTION() void OnAddEngine();
    UFUNCTION() void OnAddTurret();
    UFUNCTION() void OnCompilePlayable();

    /** Met à jour le texte stats (longueur, diamètre, nb rings, nb bays, nb stations). */
    void RefreshStatsDisplay();

    /** Rebuild la liste scrollable des rings (sélectionnables). */
    void RefreshRingList();

    /** Rebuild la liste des stations placées. */
    void RefreshStationList();
};
```

### 3.4 HullProfileEvaluator (logique mathématique pure)

```cpp
/** Fonctions statiques sans état — évaluent un profil hull à une position X. */
UCLASS()
class SUB3DBUILDER_API UHullProfileEvaluator : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Retourne le rayon (HalfWidth) à la position normalisée T (0..1) sur le hull.
     *  Dispatch selon le type de profil. */
    UFUNCTION(BlueprintCallable, Category="Hull|Profile")
    static float EvaluateRadius(
        const FHullProfileParams& Profile,
        const FSubmarineHullDef& Hull,
        float NormalizedT);

    /** Myring body-of-revolution radius at normalized position T.
     *  Nose: r(t) = MaxR * (1 - (1-t/tn)^n)^(1/n)
     *  Midbody: r = MaxR
     *  Tail: r(t) = MaxR * (1 - ((t-ts)/(1-ts))^2) * cos(theta) */
    UFUNCTION(BlueprintCallable, Category="Hull|Profile")
    static float EvaluateMyring(
        float NormalizedT,
        float MaxRadius,
        float NoseExponent,
        float NoseFraction,
        float TailFraction,
        float TailAngleDeg,
        float ParallelMidbodyFraction);

    /** Series 58 (DTMB Series 58) axisymmetric body. */
    UFUNCTION(BlueprintCallable, Category="Hull|Profile")
    static float EvaluateSeries58(
        float NormalizedT,
        float MaxRadius,
        float Fineness,
        float ParallelMidbodyFraction);

    /** Superellipse longitudinal. r(t) = MaxR * (1 - |2t-1|^n)^(1/n) */
    UFUNCTION(BlueprintCallable, Category="Hull|Profile")
    static float EvaluateSuperellipse(
        float NormalizedT,
        float MaxRadius,
        float Exponent,
        float ParallelMidbodyFraction);

    /** Uniform (cylinder with hemispherical caps). */
    UFUNCTION(BlueprintCallable, Category="Hull|Profile")
    static float EvaluateUniform(
        float NormalizedT,
        float MaxRadius,
        float ParallelMidbodyFraction);
};
```

---

## 4. Génération procédurale — Règles détaillées

### 4.1 ControlRings depuis Hull Profile

```
Input:  HullDef (LengthCm, MaxDiameterCm), HullProfileParams, int RingCount
Output: TArray<FControlRingDef>

Pour i = 0..RingCount-1 :
    T = i / (RingCount - 1)           // Position normalisée 0..1
    PositionX = T * LengthCm
    Radius = EvaluateRadius(Profile, Hull, T)

    Ring.ControlRingId = FName(*FString::Printf(TEXT("Ring_%02d"), i))
    Ring.PositionX = PositionX
    Ring.HalfWidthCm = Radius
    Ring.HalfHeightCm = Radius          // Circulaire par défaut
    Ring.SectionProfile = ESub3DSectionProfile::Circle
    Ring.WallThicknessCm = 12.0f        // Default structural
```

### 4.2 FrameRings depuis Hull Profile

```
Input:  HullDef, float NominalSpacingCm (default 55cm)
Output: TArray<FFrameRingDef>

Zones :
    BowZone   = 0.00 .. 0.15 * LengthCm   → spacing = NominalSpacing * 0.6
    MidZone   = 0.15 .. 0.85 * LengthCm   → spacing = NominalSpacing
    SternZone = 0.85 .. 1.00 * LengthCm   → spacing = NominalSpacing * 0.6

Bay boundaries (bIsBayBoundary = true) tous les 4-6 frames dans midbody,
    tous les 3 frames dans bow/stern.

Chaque FrameRing :
    FrameRingId = FName(*FString::Printf(TEXT("Frame_%03d"), Index))
    PositionX = position cumulée
    bIsBayBoundary = (compteur % BayInterval == 0)
    RoomTag = déduit de la position (voir 4.3)
```

### 4.3 Bays depuis FrameRings

```
Input:  TArray<FFrameRingDef> (filtrés bIsBayBoundary == true)
Output: TArray<FStructuralBayDef>

Pour chaque paire consécutive de FrameRings bay-boundary :
    Bay.BayId = FName(*FString::Printf(TEXT("Bay_%02d"), Index))
    Bay.ForwardFrameRingId = Frames[i].FrameRingId
    Bay.AftFrameRingId = Frames[i+1].FrameRingId
    Bay.LengthCm = Frames[i+1].PositionX - Frames[i].PositionX
    Bay.RoomTag = dérivé de la position moyenne :
        0-10%  → Ballast
        10-20% → Navigation
        20-35% → Habitat
        35-55% → Command (le bay central)
        55-70% → Machine
        70-85% → Storage
        85-95% → Machine
        95-100% → Ballast
```

### 4.4 Station Placement

```
Input:  TArray<FStructuralBayDef>, ESubStationType
Output: FTransform (position dans le bay) + bay index

Logique de placement :
    Helm    → Centre du premier bay tagué Command, Y=0, Z=+150cm (debout)
    Ballast → Centre du premier bay tagué Ballast, Y=0, Z=+80cm (console)
    Engine  → Centre du premier bay tagué Machine, Y=0, Z=+80cm
    Turret  → Attaché au Sail (position relative au SailDef)

La station est un ASubStationBase spawné comme child du BuilderActor,
    repositionné dans le bay. Au compile, les stations sont sauvegardées
    dans l'AuthoringAsset et re-spawnées par le SubmarineBase.
```

---

## 5. Pipeline Compile → Playable

```
User clique "Compile → Playable" :

1. BuilderComponent.GenerateControlRings(Asset, RingCount)     ← si pas déjà fait
2. BuilderComponent.GenerateFrameRings(Asset, FrameSpacing)    ← si pas déjà fait
3. BuilderComponent.GenerateStructuralBays(Asset)              ← si pas déjà fait
4. Asset->bHullGeometryConfirmed = true
5. Asset->HullGeometryHash = CRC32 des rings
6. BakeSubsystem->ValidateAuthoringAsset(Asset)
7. BakeSubsystem->FullBake(Asset)
   → Produit : UCompiledSubmarineBaseAsset + UCompiledSubmarineRuntimeAsset
8. Spawn ASubmarineRuntimeActor au même emplacement que le BuilderActor
   → RuntimeActor.InitializeFromRuntimeAsset()
9. Spawn ASubmarineBase (le pawn jouable) attaché au RuntimeActor
   → SubmarineBase possède le RuntimeActor
   → SubmarineBase.StationManager découvre les stations auto
10. Optionnel : cacher le BuilderActor (SetActorHiddenInGame)
11. Le joueur peut Possess le SubmarineBase et jouer

Résultat : un sous-marin jouable dans le level en UN clic.
```

---

## 6. Interaction Ring dans le Viewport

### Ring Selection
- Widget affiche la liste des rings (Ring_00, Ring_01, ...)
- Clic sur un ring dans la liste → `OnRingSelected(Index)` :
  - Highlight le ring dans le ProceduralMesh (couleur différente sur la section)
  - Le widget affiche les sliders du ring sélectionné (PositionX, HalfWidth, HalfHeight, Roundness)
  - Chaque changement → `RebuildPreview()` immédiat

### Ring Handles (Phase 2 — optionnel)
- `USubmarineRingGizmoComponent` : petits handles 3D dans le viewport sur chaque ring
- Drag horizontal → change PositionX
- Drag vertical → change HalfHeight
- Drag latéral → change HalfWidth
- Chaque drag → `OnHullParameterChanged()` → rebuild mesh

---

## 7. Données ajoutées à l'AuthoringAsset

Champs à ajouter dans `USub3DSubmarineAuthoringAsset` :

```cpp
// ─── Station Layout (écrit par le Builder, lu par le Bake) ───────

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[C] Stations")
TArray<FStationPlacementDef> StationPlacements;
```

Nouveau type :

```cpp
USTRUCT(BlueprintType)
struct SUB3DCORE_API FStationPlacementDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESubStationType StationType = ESubStationType::Helm;

    /** Bay dans lequel la station est placée. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName BayId;

    /** Transform local dans le bay. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform LocalTransform;

    /** Classe de station à spawner. Si null, utilise la classe par défaut du type. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftClassPtr<ASubStationBase> StationClass;
};
```

Ajout dans `FFrameRingDef` (Sub3DEnvelopeTypes.h) :

```cpp
/** Marque ce FrameRing comme frontière de bay. */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frame Ring")
bool bIsBayBoundary = false;
```

---

## 8. Bow / Stern / Propeller — Assets pré-faits

Au lieu de générer procéduralement les extrémités (complexe et moche) :

```
Content/Sub3D/Meshes/BowCaps/
    SM_Bow_Ogive.uasset          ← nez ogival classique (Kilo, 636)
    SM_Bow_Hemisphere.uasset     ← nez hémisphérique (SSBN)
    SM_Bow_Blunt.uasset          ← nez tronqué (Type 212)
    SM_Bow_SonarDome.uasset      ← nez avec dôme sonar (Los Angeles)

Content/Sub3D/Meshes/SternCaps/
    SM_Stern_Cone.uasset         ← cône simple
    SM_Stern_Torpedo.uasset      ← queue torpedo (ogive inversée)
    SM_Stern_Truncated.uasset    ← tronqué (pump-jet)

Content/Sub3D/Meshes/Propellers/
    SM_Prop_5Blade.uasset        ← 5 pales classique
    SM_Prop_7Blade.uasset        ← 7 pales silencieux (Akula)
    SM_Prop_PumpJet.uasset       ← pump-jet (Virginia, Astute)

Content/Sub3D/Meshes/Sails/
    SM_Sail_Soviet.uasset        ← kiosque soviétique (large, arrondi)
    SM_Sail_Western.uasset       ← kiosque OTAN (fin, incliné)
    SM_Sail_Compact.uasset       ← kiosque compact (diesel-électrique)
```

Le BuilderActor attache ces meshes aux bonnes positions :
- Bow → `PositionX = 0`, orienté -X, scalé pour matcher le rayon du premier ControlRing
- Stern → `PositionX = LengthCm`, orienté +X, scalé pour matcher le dernier ControlRing
- Propeller → `PositionX = LengthCm + PropellerOffsetCm`, centré
- Sail → Position depuis `SailDef.SailPositionFraction * LengthCm`, sur le dessus du hull

---

## 9. Ordre d'implémentation

### Phase 1 — Hull Live Preview (1-2 jours)
1. Créer module `Sub3DBuilder` avec Build.cs
2. `HullProfileEvaluator` — maths pures, testable standalone
3. `ASubmarineBuilderActor` — minimum : ProceduralMesh + RebuildPreview()
4. `GenerateControlRings()` — depuis le profil
5. Placer le BuilderActor dans un level, vérifier que le mesh apparaît

**Résultat : un actor dans un level qui affiche un hull procédural.**

### Phase 2 — Widget UMG (1-2 jours)
1. Créer `WBP_SubmarineBuilder` dans le UMG Designer (Blueprint widget)
2. Implémenter `USubmarineBuilderWidget` C++ (bindings)
3. Sliders : Length, Diameter, Midbody → `OnHullParameterChanged()` → live rebuild
4. Boutons preset → `ApplyPreset()`
5. Spinbox ring count → `GenerateControlRings()`
6. Ring list → sélection → highlight dans le viewport

**Résultat : widget latéral avec sliders qui modifient le hull en temps réel.**

### Phase 3 — Auto-Structure + Stations (1 jour)
1. `GenerateFrameRings()` avec règles d'espacement
2. `GenerateStructuralBays()` depuis les frame rings
3. `FindBestBayForStation()` + spawn des ASubStationBase
4. Boutons [+Helm] [+Ballast] [+Engine] [+Turret]

**Résultat : structure intérieure auto-générée, stations placées.**

### Phase 4 — Compile → Playable (1 jour)
1. Appeler BakeSubsystem.FullBake() depuis le BuilderComponent
2. Spawn ASubmarineRuntimeActor
3. Spawn ASubmarineBase, attacher les stations
4. Test : Possess → piloter le sous-marin

**Résultat : clic "Compile → Playable" → sous-marin jouable.**

### Phase 5 — Assets Bow/Stern/Propeller + Polish (2 jours)
1. Modéliser ou sourcer les meshes StaticMesh
2. Auto-scale et positionnement depuis le hull profile
3. Ring selection + highlight dans le viewport
4. Stats display (longueur, déplacement, nb bays, nb stations)

**Résultat : sous-marins visuellement complets.**

---

## 10. Ce qui ne change PAS

| Système | Status |
|---------|--------|
| `ASubmarineBase` + physics + movement | Inchangé — le Builder produit un sub que SubmarineBase consomme |
| Station system (Helm, Ballast, Engine, Turret) | Inchangé — les stations sont spawnées par le Builder |
| `USubmarineStationManagerComponent` | Inchangé — découvre les stations au BeginPlay |
| Tunnel Navigation | Inchangé — fonctionne sur le SubmarineBase |
| Network replication (`FSubmarineNetState`) | Inchangé |
| `USubmarineBakeSubsystem` | Inchangé — appelé par le Builder au lieu du Toolkit |
| Tous les types Core (Hull, Bay, Floor, etc.) | Inchangés sauf ajout `bIsBayBoundary` et `FStationPlacementDef` |

---

## 11. Résumé en une phrase

**Le Submarine Builder est un actor-dans-un-level avec un ProceduralMesh + un widget UMG latéral. Tu choisis un preset, tu tires 3 sliders, tu cliques "Auto-Structure", tu places tes stations, tu cliques "Compile → Playable", tu possèdes le pawn et tu joues.**
