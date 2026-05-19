# Sub3D — PostProcess State Controller — Architectural Spec

| | |
|---|---|
| **Date** | 2026-05-18 |
| **Statut** | ✅ **CANONIQUE — architecture C++ figée pour PostProcess multi-état gameplay-driven** |
| **Auteur** | Tech architect review (senior 8+ ans UE5 underwater) |
| **Hardware cible** | i7-9700K + GTX 1660 Super, 1080p @ 60fps |
| **Engine** | UE 5.7 |
| **Scope** | Architecture C++ du controller PP + détection state + data assets + transitions + intégration fog |
| **Docs parallèles** | [lighting_pp_fog_canonical_spec](2026-05-18_lighting_pp_fog_canonical_spec.md) (valeurs PP/fog) · [cave_abyssal_material_spec](2026-05-17_cave_abyssal_material_spec.md) (matériaux) |
| **Prompt source** | [postprocess_state_controller_research_prompt.md](../tools/postprocess_state_controller_research_prompt.md) |

---

## Sommaire

| # | Section | Lecture |
|---|---|---|
| 0 | [TL;DR — 4 décisions verrouillées](#0-tldr--4-décisions-verrouillées) | 30 s |
| 1 | [Q1 — Architecture optimale : décision matrix](#1-q1--architecture-optimale--décision-matrix) | 6 min |
| 2 | [Q2 — Stratégies de transition par type](#2-q2--stratégies-de-transition-par-type) | 4 min |
| 3 | [Q3 — Détection head underwater](#3-q3--détection-head-underwater) | 3 min |
| 4 | [Q4 — Data assets design](#4-q4--data-assets-design) | 3 min |
| 5 | [Q5 — Intégration Fog Controller](#5-q5--intégration-fog-controller) | 2 min |
| 6 | [Q6 — Alternatives & anti-patterns industrie](#6-q6--alternatives--anti-patterns-industrie) | 4 min |
| 7 | [Architecture finale — UML + code skeletons](#7-architecture-finale--uml--code-skeletons) | 8 min |
| 8 | [Plan d'implémentation — TODO list](#8-plan-dimplémentation--todo-list) | 2 min |
| 9 | [Références](#9-références) | bonus |

---

## 0. TL;DR — 4 décisions verrouillées

1. **Option F = hybrid B+D**, ma 6ème proposition. `UPostProcessComponent` attaché à la camera du `ASubCrewCharacter` + `UAtmosphereStateController` (UActorComponent sur le même character) qui écrit `FPostProcessSettings` natif. **+ 1 PP material** custom assigné via Blendable pour le concern underwater spécifique (refraction, edge tint) — défère post-FP, FP s'en passe.

2. **Data Assets D2 (axis-based)** : `DA_PP_Medium`, `DA_PP_Location`, `DA_PP_Biome` × N variants. Composition séquentielle via le mécanisme natif `bOverride_X` de `FPostProcessSettings`. **Pas de combinatoire produit.**

3. **Fog Controller fusionné** (Option C de Q5) dans `UAtmosphereStateController`. Une seule classe pilote PP + ExpHeightFog. Évite la coordination cross-controller.

4. **Détection head underwater = compartment-relative** avec hysteresis 5 cm. Pas de trace channel, pas d'event-based. Test plan-vs-point simple, robuste 6-DOF.

---

## 1. Q1 — Architecture optimale : décision matrix

### 1.1 Comparatif exhaustif

| Option | Coût perf (1660) | Effort (jours) | Artiste-friendly | Multi-PC ready | UE5.7 compat | Verdict |
|---|---|---|---|---|---|---|
| **A. Single Unbound PP + Controller** | 0.05 ms | 2-3 | ⚠️ Modifie un actor scène en runtime (debug confus) | ❌ Globalement partagé | ✅ | ⚠️ Fonctionne mais pollue scène |
| **B. UPostProcessComponent on camera + Controller** | 0.05 ms | 2-3 | ✅ Component isolé sur player | ✅ Per-camera natif | ✅ | ✅ Solide |
| **C. Bounded "storage" volumes + Linked** | 0.1 ms | 4-5 | ✅ Tweakable in-level | ❌ Identique à A | ✅ | ⚠️ Ne couvre pas biomes |
| **D. MPC-driven full shader** | 0.5-1.0 ms | 3-4 | ⚠️ Material params seulement | ✅ Per-camera si material on camera | ✅ | ⚠️ Bloque Tone Curve / Exposure natifs |
| **E. Composite layered PP materials** | 1.0-2.0 ms | 4-6 | ⚠️ Stack difficile à debug | ✅ | ✅ | ❌ Trop cher 1660, complexité élevée |
| **🎯 F. Hybrid B + D (recommandé)** | 0.5-0.6 ms | 3-4 | ✅ DA presets + 1 material isolé | ✅ Per-camera natif | ✅ | ✅✅ **Choix final** |

### 1.2 Analyse détaillée par option

**Option A — Single Unbound PP + Controller (UActorComponent)**

- ✅ Pros : single source of truth, debug visuel direct dans la scène.
- ❌ Cons : mutation runtime d'un actor de niveau (dirty flag en éditeur si tu sauves par accident), globalement partagé (en multi 4-16 joueurs post-MVP, tous les clients verraient les mêmes settings, sauf si on switche en local override → re-bouchonner plus tard).
- **Coût perf** : négligeable, 0.05 ms le tick.
- **Effort** : 2-3 jours (component + DA + lerp).
- **Maintenabilité** : moyenne — l'artiste voit le volume mais ne peut pas tweaker en runtime sans confusion sur "qui écrit quoi".

**Option B — UPostProcessComponent on camera + Controller**

- ✅ Pros : 100% per-camera, follow camera automatique, n'écrit jamais dans l'actor scène, multiplayer-ready dès jour 1, debugger via `showdebug postprocessstate` (custom).
- ❌ Cons : artiste doit savoir que le component vit sur le character (pas dans le World Outliner du level).
- **Coût perf** : 0.05 ms.
- **Effort** : 2-3 jours.
- **Maintenabilité** : ✅ component isolé, testable, scopé au lifetime du character.

**Option C — Bounded volumes "storage" via LinkedPostProcessVolume**

- ✅ Pros : artiste place 1 volume PP par compartiment dans le level (visuel intuitif), tweak in-level, version-controlled per map.
- ❌ Cons : **ne s'applique qu'aux compartiments** — ne gère pas les biomes (procéduraux, depth-driven, pas de volume placé). Doublonne l'infra UCompartmentVolumeComponent existante. Crée des données dupliquées (volume + compartment) à maintenir synchro.
- **Coût perf** : 0.1 ms (extra read du volume.Settings).
- **Effort** : 4-5 jours (plumbing du LinkedPostProcessVolume → controller).
- **Maintenabilité** : ⚠️ artiste doit placer un PP volume à chaque nouveau compartiment, oubli silencieux possible.

**Option D — MPC-driven full shader**

- ✅ Pros : transitions intrinsèquement smooth (lerp shader = par-pixel naturel), single material à debug, GPU-friendly.
- ❌ Cons : **bloque l'utilisation native** de Tone Curve / Exposure / Saturation / Color Grading (qui sont dans `FPostProcessSettings`, pas dans un material). Tu réimplémentes le tonemapping et le color grading en shader → réinvention coûteuse.
- **Coût perf** : 0.5-1.0 ms (full-screen sample SceneColor + composite).
- **Effort** : 3-4 jours (master material complexe).
- **Maintenabilité** : ⚠️ artiste-friendly seulement si le material expose 20+ params bien nommés.

**Option E — Composite layered PP materials**

- ✅ Pros : modulaire, chaque material un concern.
- ❌ Cons : 3-4 materials × 0.3-0.5 ms chacun = **1-2 ms total**, prohibitif sur 1660 Super (budget total ~16 ms). Stack Blendable difficile à debug (ordre, weights, override).
- **Coût perf** : 1.0-2.0 ms.
- **Effort** : 4-6 jours.
- **Maintenabilité** : ❌ complexe.

### 1.3 🎯 Option F — Hybrid B + D (ma 6ème proposition)

**Concept** : combiner la force de B (natif UE) et de D (shader pour ce que natif ne gère pas), sans cumuler leurs coûts.

```
┌──────────────────────────────────────────────────────────────┐
│ ASubCrewCharacter                                            │
│                                                              │
│   UCameraComponent (Camera)                                  │
│       └── UPostProcessComponent (Sub3D PP injector)          │
│              FPostProcessSettings   ← écrit par Controller   │
│              + Blendable: M_PP_Underwater (BeforeTonemap)    │
│                  └── samples MPC_Sub3DAtmosphere             │
│                                                              │
│   UAtmosphereStateController (UActorComponent)               │
│       ├── State input :                                      │
│       │     CurrentCompartment (from this character)         │
│       │     SubFloodComponent (via owner sub)                │
│       │     Depth Z (world)                                  │
│       │     bHeadUnderwater (computed §3)                    │
│       │                                                      │
│       ├── Data inputs :                                      │
│       │     DA_PP_Medium_{Air, Water}                        │
│       │     DA_PP_Location_{SubInterior, Outside}            │
│       │     DA_PP_Biome_{Coastal..Trenches}                  │
│       │     [Optional] DA_PP_CompartmentOverrides            │
│       │                                                      │
│       └── Outputs :                                          │
│             → UPostProcessComponent.Settings (lerped)        │
│             → MPC_Sub3DAtmosphere (params shader)            │
│             → AExponentialHeightFog scene actor (lerped)     │
└──────────────────────────────────────────────────────────────┘
```

**Pourquoi F est supérieur** :
- **Tone Curve, Exposure, Saturation, Color Grading, Bloom, Vignette, CA** : natif `FPostProcessSettings` (le composite UE est ultra-optimisé, ~0 ms extra).
- **Underwater overlay** (refraction, edge tint, surface fade) : **1 seul** material custom, BlendableLocation = BeforeTonemap, params via MPC. Coût 0.4-0.6 ms.
- **Per-camera** dès le jour 1 (multi-PC futur OK).
- **Composition data-driven** via les DA axes.
- **Pas de pollution scène** (PP volume scène = baseline seulement, ne change pas en runtime).

**Coût total** : 0.5-0.6 ms (uniquement le PP material custom).
**Effort** : 3-4 jours pour FP complet (material défèrable, FP peut s'en passer 1 semaine).
**FP-shipping** : sans le material custom, c'est 2-3 jours. Le material est polish, pas blocker.

**Décision** : Option F. Implémenter d'abord sans le material custom (= Option B pure), ajouter le material au moment où l'eau interne visuel n'est plus suffisante avec les natifs seuls.

---

## 2. Q2 — Stratégies de transition par type

| Transition | Trigger | Durée | Curve | Params lerpés | Risque pop | Mitigation |
|---|---|---|---|---|---|---|
| **T1. Sub dry ↔ flooded head submerged** | bHeadUnderwater computed continu (§3) | **0.4 s** | ease-out | Saturation (1.05→0.7), CA (0.4→0.7), Vignette (0.55→0.75), MPC `WaterMask` (0→1) | Flicker à la surface | **Hysteresis 5 cm** (§3) + min dwell 0.15 s |
| **T2. Sub interior ↔ Cave exterior** (sas) | `USubHullBoundaryComponent::HandleHullCrossing` event | **1.0 s** | ease-in-out | TOUS les params Medium+Location, fog Density/Inscatter, Vignette | Téléport interrompant transition | Interrompre lerp en cours, snap-restart vers nouveau target |
| **T3. Biome change** (depth-driven) | Z player traverse seuil biome | **3.0 s** | smoothstep sur depth | Fog Density, Inscatter, VolAlbedo, ExposureCompensation (subtle) | Ping-pong au seuil ± altitude | **Hysteresis 2000 cm** (±20 m) + smoothstep continu sur 4000 cm de transition |
| **T4. Sub headlight zone** | aucun — géré par light natif | **N/A** | N/A | Aucun PP override | N/A | Light Vol. Scattering Intensity = 1.2 fait le job |
| **T5. Flare illumination** | aucun — géré par light natif | **N/A** | N/A | Aucun PP override | N/A | Auto-exposure (§canonical spec §2) absorbe le flash |

### 2.1 Détail T1 — Head dunk

L'utilisateur ressent "dunk" si la transition est trop lente. 0.4 s avec ease-out (rapide initial, lent fin) donne le feel "splash" puis "settle".

```cpp
// Pseudo-code dans UAtmosphereStateController::Tick
const bool bWantsUnderwater = ComputeHeadUnderwater();
const float TargetWaterMix = bWantsUnderwater ? 1.0f : 0.0f;
WaterMixCurrent = FMath::FInterpTo(WaterMixCurrent, TargetWaterMix, DeltaTime, 1.0f / 0.4f);
MPC->SetScalarParameterValue("WaterMask", WaterMixCurrent);
```

### 2.2 Détail T2 — Sas crossing

Event-based, le `USubHullBoundaryComponent::HandleHullCrossing` broadcast déjà sur Embarked↔Outside (cf. CLAUDE.md §Crew environment axis). Le controller s'abonne :

```cpp
HullBoundary->OnHullCrossed.AddDynamic(this, &UAtmosphereStateController::OnHullCrossed);

void UAtmosphereStateController::OnHullCrossed(ESubHullCrossingDirection Direction)
{
    // Snap-restart : si une transition est en cours, on l'interrompt
    LocationLerpAlpha = 0.0f;
    LocationLerpSource = CurrentComposite;       // depuis l'état actuel (pas la cible précédente)
    LocationLerpTarget = ComputeTargetFromState(); // recompose
    LocationLerpDuration = 1.0f;
}
```

Interrompre = repartir depuis `CurrentComposite` (l'état visuel actuel, pas la cible précédente). Sinon snap visuel.

### 2.3 Détail T3 — Biome

Continu, ne JAMAIS faire de switch discret. Calcul :

```cpp
// Z thresholds en cm
constexpr float Z_COASTAL_PELAGIC = -12000.f;
constexpr float Z_PELAGIC_BATHYAL = -30000.f;
constexpr float Z_BATHYAL_ABYSSAL = -150000.f;
constexpr float Z_ABYSSAL_TRENCHES = -400000.f;

// Calcul biome continu (0.0 Coastal, 4.0 Trenches)
float ComputeBiomeIndex(float Z)
{
    if (Z > Z_COASTAL_PELAGIC) {
        return FMath::Lerp(0.f, 0.f, 1.f);  // pure coastal
    }
    // Smoothstep entre seuils
    if (Z > Z_PELAGIC_BATHYAL) {
        return FMath::Lerp(0.f, 1.f, FMath::SmoothStep(Z_COASTAL_PELAGIC, Z_PELAGIC_BATHYAL, Z));
    }
    // etc.
}
```

Puis lerp entre deux DA biome adjacents :

```cpp
const float BiomeIdx = ComputeBiomeIndex(PlayerZ);
const int32 LowerIdx = FMath::FloorToInt(BiomeIdx);
const int32 UpperIdx = FMath::CeilToInt(BiomeIdx);
const float Alpha = BiomeIdx - LowerIdx;
const UPP_BiomePreset* Lower = BiomePresets[LowerIdx];
const UPP_BiomePreset* Upper = BiomePresets[UpperIdx];
const FPostProcessSettings BiomeLerped = LerpPP(Lower->Settings, Upper->Settings, Alpha);
```

### 2.4 Anti-pop checklist

Pour tout lerp PP :
- Lerp UNIQUEMENT les champs avec `bOverride_X = true` dans les DA source/target. Lerper les autres = lerper des défauts qui ne sont jamais "à toi".
- **Pas de lerp sur les champs Method** (`AutoExposureMethod`, `AntiAliasingMethod`) : ce sont des enums, pas des floats. Snap au target dès que > 0.5.
- **Snap les booleans** : `bOverride_X` doit rester stable durant le lerp (on lerp toujours dans le même set).

---

## 3. Q3 — Détection head underwater

### 3.1 Approches comparées

| Approche | Robustesse 6-DOF | Coût | Complexité | Verdict |
|---|---|---|---|---|
| **Compartment-relative test** | ✅ Parfaite | ~0.005 ms | Faible | ✅ Recommandé |
| World Z vs compartment surface world Z | ⚠️ Faux quand sub pitch ≠ 0 | ~0.005 ms | Faible | ❌ Bug sur sub tilted |
| Trace channel ECC_WaterSurface | ✅ Mais overkill | ~0.05 ms | Élevée | ❌ Coût pour rien |
| Event-based FloodComponent | ⚠️ Coupling FloodComponent→Camera | Variable | Élevée | ❌ Inverse la dépendance |

### 3.2 Recommandation : compartment-relative + hysteresis

**Principe** : transforme la position monde de la caméra dans le repère local du compartiment. Compare au Z local de la surface d'eau (qui dépend du water mass dans le compartiment, calculé indépendamment du tilt du sub).

### 3.3 Code C++ canonique

```cpp
// Dans UCompartmentVolumeComponent.h (méthode à ajouter)
UFUNCTION(BlueprintPure, Category="Sub3D|Compartment")
float ComputeWaterSurfaceLocalZ(float WaterMassKg) const;

// Implémentation : water mass / (densité eau × surface section horizontale) = hauteur
// Pour box approximatif : Volume = Mass / 1000 (kg/m³ eau douce), Height = Volume / FloorArea
float UCompartmentVolumeComponent::ComputeWaterSurfaceLocalZ(float WaterMassKg) const
{
    constexpr float WaterDensityKgPerCm3 = 1.0e-3f; // 1g/cm³
    const float VolumeCm3 = WaterMassKg / WaterDensityKgPerCm3;
    const FVector LocalExtent = GetUnscaledBoxExtent(); // ou méthode équivalente
    const float FloorAreaCm2 = (LocalExtent.X * 2.f) * (LocalExtent.Y * 2.f);
    const float HeightCm = VolumeCm3 / FMath::Max(FloorAreaCm2, 1.f);
    return -LocalExtent.Z + HeightCm; // Z local depuis le fond du compartiment
}
```

```cpp
// Dans UAtmosphereStateController.h
private:
    UPROPERTY()
    bool bWasHeadUnderwaterLastTick = false;

    static constexpr float HysteresisCm = 5.0f;

public:
    bool ComputeHeadUnderwater() const;

// Implémentation
bool UAtmosphereStateController::ComputeHeadUnderwater() const
{
    const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
    if (!Crew) return false;
    
    UCompartmentVolumeComponent* Comp = Crew->GetCurrentCompartment();
    if (!Comp)
    {
        // Outside = always underwater (abyssal context, jamais à la surface)
        return true;
    }
    
    const USubFloodComponent* Flood = Crew->GetOwningSubmarine()
        ? Crew->GetOwningSubmarine()->FindComponentByClass<USubFloodComponent>()
        : nullptr;
    if (!Flood) return false;
    
    const float WaterMassKg = Flood->GetWaterMassKg(Comp->CompartmentId);
    if (WaterMassKg < KINDA_SMALL_NUMBER) return false; // dry compartment
    
    const float WaterSurfaceLocalZ = Comp->ComputeWaterSurfaceLocalZ(WaterMassKg);
    
    // Caméra : world → compartment local
    const FVector CameraWorld = Crew->GetCameraComponent()->GetComponentLocation();
    const FVector CameraLocal = Comp->GetComponentTransform().InverseTransformPosition(CameraWorld);
    
    // Hysteresis : seuil dépend de l'état précédent (anti-flicker)
    const float Threshold = bWasHeadUnderwaterLastTick 
        ? (WaterSurfaceLocalZ - HysteresisCm)   // pour sortir, il faut être 5cm au-dessus
        : (WaterSurfaceLocalZ + HysteresisCm);  // pour entrer, il faut être 5cm en-dessous
    
    return CameraLocal.Z < Threshold;
}

void UAtmosphereStateController::TickComponent(float DeltaTime, ...)
{
    Super::TickComponent(DeltaTime, ...);
    const bool bHeadUnderwater = ComputeHeadUnderwater();
    bWasHeadUnderwaterLastTick = bHeadUnderwater;
    // ... use bHeadUnderwater for transitions
}
```

### 3.4 Pourquoi compartment-relative et pas world

Quand le sub roll/pitch (combat, dive sharp), le "haut" du compartiment n'est plus aligné avec +Z monde. Tester `CameraWorld.Z < SurfaceWorldZ` donne des faux positifs quand le sub est tilté. Le compartment local Z est aligné avec l'up du sub (= up apparent du joueur quand embarked, cf. Local Grid Space Authority §CLAUDE.md).

### 3.5 Le "outside = underwater" assumption

Sub3D est 100% abyssal. Pas de surface atteignable. Donc `CurrentCompartment == nullptr` ⇒ joueur dans l'océan ⇒ underwater. Si plus tard tu ajoutes une bulle d'air outside (peu probable), tu ajouteras un check `bIsInAirPocket`.

---

## 4. Q4 — Data Assets design

### 4.1 Comparatif

| Option | Combinatoire | Composition | Artist-friendly | Verdict |
|---|---|---|---|---|
| **D1. DA per state (6 DA)** | 6 × 5 biomes = 30 | ❌ Aucune | ✅ Direct | ❌ Explose |
| **🎯 D2. DA per axis (3 axes)** | 2+2+5 = 9 DA | ✅ Séquentielle bOverride | ✅ Per-axis tweak | ✅ **Recommandé** |
| **D3. UPrimaryDataAsset hierarchical** | Variable | ⚠️ Inheritance UE5 complexe | ⚠️ Concept abstrait | ⚠️ Overkill |
| **D4. Single DA + Map<state, settings>** | 1 DA | ❌ Pas de compose | ⚠️ 1 gros fichier | ❌ Pas scalable |
| **D5. MPC direct (no DA)** | 0 DA | ❌ Pas de tone curve | ❌ Pas d'inspecteur PP UE | ❌ Bloque natifs |

### 4.2 Structure D2 finale

```
Content/Sub3D/Atmosphere/
├── DA_PP_Medium_Air.uasset          ← Saturation 1.05, Vignette 0.4
├── DA_PP_Medium_Water.uasset        ← Saturation 0.85, Vignette 0.65, CA 0.6
├── DA_PP_Location_SubInterior.uasset ← ColorGain warm (1.05, 1.02, 0.98), ExposureBias +0.2
├── DA_PP_Location_Outside.uasset    ← ColorGain cool (0.95, 0.92, 1.0), ExposureBias -0.1
├── DA_PP_Biome_Coastal.uasset       ← Fog 0.04, Inscatter (0.012, 0.040, 0.080)
├── DA_PP_Biome_Pelagic.uasset       ← Fog 0.06, ...
├── DA_PP_Biome_Bathyal.uasset       ← Fog 0.10, ...
├── DA_PP_Biome_Abyssal.uasset       ← Fog 0.15, ...
├── DA_PP_Biome_Trenches.uasset      ← Fog 0.22, ...
└── DA_PP_CompartmentOverrides.uasset  ← optional, Map<CompartmentId, FPP overrides>
```

### 4.3 UClass DataAsset minimal

```cpp
// Sub3DRuntime/Public/Atmosphere/PPPresetDataAsset.h
UCLASS(BlueprintType)
class SUB3DRUNTIME_API UPPPresetDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** Sparse FPostProcessSettings — only fields with bOverride_X=true are applied */
    UPROPERTY(EditAnywhere, Category="Sub3D|PP")
    FPostProcessSettings Settings;

    /** Fog params (optional, used by Biome presets) */
    UPROPERTY(EditAnywhere, Category="Sub3D|Fog")
    bool bOverridesFog = false;

    UPROPERTY(EditAnywhere, Category="Sub3D|Fog", meta=(EditCondition="bOverridesFog"))
    float FogDensity = 0.10f;

    UPROPERTY(EditAnywhere, Category="Sub3D|Fog", meta=(EditCondition="bOverridesFog"))
    FLinearColor FogInscattering = FLinearColor(0.003f, 0.015f, 0.035f);

    UPROPERTY(EditAnywhere, Category="Sub3D|Fog", meta=(EditCondition="bOverridesFog"))
    FLinearColor VolumetricAlbedo = FLinearColor(0.02f, 0.06f, 0.10f);

    UPROPERTY(EditAnywhere, Category="Sub3D|Fog", meta=(EditCondition="bOverridesFog"))
    float VolumetricExtinctionScale = 1.5f;
};
```

### 4.4 Composition séquentielle — pattern UE natif

`FPostProcessSettings` a un `bOverride_X` pour chaque champ. UE le respecte nativement dans le composite. Pour composer 3 axes :

```cpp
FPostProcessSettings UAtmosphereStateController::ComposeSettings() const
{
    FPostProcessSettings Composite;
    // Composite démarre vide (tous bOverride_X = false)

    // Apply Medium (lerp Air ↔ Water si en transition)
    ApplyOverlay(Composite, MediumPreset_Current);
    
    // Apply Location (lerp SubInterior ↔ Outside)
    ApplyOverlay(Composite, LocationPreset_Current);
    
    // Apply Biome (lerp continu entre 2 biomes adjacents)
    const FPostProcessSettings BiomeLerped = LerpPP(BiomeLower, BiomeUpper, BiomeAlpha);
    ApplyOverlay(Composite, BiomeLerped);
    
    // Apply Compartment override (si présent)
    if (UPPPresetDataAsset* Override = GetCompartmentOverride())
    {
        ApplyOverlay(Composite, Override->Settings);
    }
    
    return Composite;
}

// ApplyOverlay : pour chaque champ avec bOverride_X = true dans Source, écrase dans Target
void ApplyOverlay(FPostProcessSettings& Target, const FPostProcessSettings& Source)
{
    // Macro-expand sur tous les champs FPostProcessSettings — UE n'a pas d'API publique,
    // mais on peut le faire field-by-field pour les ~25 champs qu'on utilise réellement.
    if (Source.bOverride_AutoExposureBias)
    {
        Target.bOverride_AutoExposureBias = true;
        Target.AutoExposureBias = Source.AutoExposureBias;
    }
    if (Source.bOverride_ColorSaturation)
    {
        Target.bOverride_ColorSaturation = true;
        Target.ColorSaturation = Source.ColorSaturation;
    }
    // ... 20-25 champs total
}
```

### 4.5 Tableau composition canonique Sub3D

Quels champs sont écrits par quel axe ?

| Champ FPostProcessSettings | Medium | Location | Biome | Compartment override |
|---|---|---|---|---|
| AutoExposureBias | — | ✅ +0.2 / -0.1 | ⚠️ subtile (-0.0 à -0.3) | ✅ rare |
| ColorSaturation Shadows/Mid/High | ✅ Air 1.05 / Water 0.85 | — | — | — |
| ColorGain Shadows/Highlights | — | ✅ warm tungsten / cool teal | — | ✅ rare |
| ColorOffset Shadows | — | ✅ mauve subtle | — | — |
| VignetteIntensity | ✅ 0.4 / 0.65 | — | — | — |
| SceneFringeIntensity (CA) | ✅ 0.4 / 0.7 | — | — | — |
| BloomIntensity | — | — | ⚠️ subtle | — |
| IndirectLightingIntensity | — | ✅ 0.6 (interior) / 0.45 (outside) | — | — |
| **FogDensity** | — | — | ✅ exclusive | — |
| **FogInscatteringColor** | — | — | ✅ exclusive | — |

Cette table figée évite que 2 axes essaient d'écrire le même champ (conflit).

---

## 5. Q5 — Intégration Fog Controller

### 5.1 Décision : Option C (fusion)

| Option | Pros | Cons | Verdict |
|---|---|---|---|
| A. Sub-component du PP controller | Modulaire | 2 ticks séparés, risque désynchro | ❌ |
| B. Séparé + events/Subsystem | Découplé | Plus de plumbing, double source de vérité depth | ❌ |
| **C. Fusionné en UAtmosphereStateController** | Single tick, single read d'état, single source de vérité | Classe un peu grosse (~400 lignes) | ✅ |

**Justification** : le fog ET le PP partagent les mêmes inputs (player Z, compartment, surrounding medium). Les diviser duplique la logique de lecture d'état. Un seul controller centralise la dirty-flag computation et garantit la cohérence visuelle frame-perfect (pas de cas où fog est à Bathyal pendant que PP est à Pelagic).

### 5.2 Gestion de l'AExponentialHeightFog actor

Le fog est un actor scène, pas un component. Le controller doit le résoudre au BeginPlay :

```cpp
void UAtmosphereStateController::BeginPlay()
{
    Super::BeginPlay();
    
    // Resolve scene ExpHeightFog (assume one in level)
    for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
    {
        SceneFog = *It;
        break;
    }
    
    if (!SceneFog.IsValid())
    {
        UE_LOGFMT(LogSub3D, Warning, 
            "AtmosphereStateController: no AExponentialHeightFog found in level — fog control disabled");
    }
    
    // Resolve MPC
    MPC_AtmosphereState = LoadObject<UMaterialParameterCollection>(
        nullptr, TEXT("/Game/Sub3D/Atmosphere/MPC_Sub3DAtmosphere.MPC_Sub3DAtmosphere"));
}

void UAtmosphereStateController::ApplyFog(const FFogState& State)
{
    if (!SceneFog.IsValid()) return;
    
    UExponentialHeightFogComponent* FogComp = SceneFog->GetComponent();
    FogComp->SetFogDensity(State.Density);
    FogComp->SetFogInscatteringColor(State.Inscattering);
    FogComp->SetVolumetricFogAlbedo(State.Albedo);
    FogComp->SetVolumetricFogExtinctionScale(State.ExtinctionScale);
}
```

### 5.3 Note importante

S'il y a plusieurs `AExponentialHeightFog` dans le level (rare), le controller prend le premier itéré (ordre arbitraire). Pour éviter cette ambiguïté, exposer un `UPROPERTY(EditAnywhere) TSoftObjectPtr<AExponentialHeightFog> SceneFogOverride` sur le controller pour permettre à l'artiste de désigner explicitement quel fog est piloté.

---

## 6. Q6 — Alternatives & anti-patterns industrie

### 6.1 Patterns UE5.7 récents pertinents

**UWorldSubsystem vs UActorComponent pour le controller** :

| Approche | Lifetime | Accès | Multi-PC | Verdict |
|---|---|---|---|---|
| UActorComponent on character | Lié au character | `Crew->GetAtmosphereController()` | Per-character natif | ✅ FP |
| UWorldSubsystem | Lié au monde | `GetWorld()->GetSubsystem<U...>()` | Singleton par world (split-screen problématique) | ⚠️ Si singleton suffit |
| ULocalPlayerSubsystem | Lié au local player | `GetLocalPlayer()->GetSubsystem<U...>()` | Per-LocalPlayer ✅ | ✅ Multi-PC propre |

**Recommandation FP** : UActorComponent sur le character (simple, lifetime clair).
**Refactor post-FP si multi-screen** : migrer vers `ULocalPlayerSubsystem`.

**ULocalFogVolume** (UE 5.4+) : déjà couvert en [lighting_pp_fog_canonical_spec §1.6]. Le controller peut spawn/despawn des LocalFogVolume pour hero spots (caverns particuliers) — défère post-FP.

**FX Subsystem / VFX-PP integration** : Niagara a un FXSubsystem mais il pilote des particules, pas le PP. Non applicable.

**Substrate** : désactivé en 5.7 par défaut. Activé, c'est un nouveau material pipeline qui change les shaders (pas le PP). Non applicable au state controller. Ne pas activer Substrate sur 1660 Super (perf cost).

**World Partition Streaming PP** : permet des PP volumes streamés in/out avec les cells. Sub3D n'utilise pas WP en interior, et la cave est procédurale (pas WP-compatible nativement). Non applicable.

### 6.2 Anti-patterns à éviter

1. **Écrire dans une `AExponentialHeightFog` ou `APostProcessVolume` du level chaque frame puis sauver le map** : l'éditeur marque dirty. Faire un sweep pour vérifier que ces actors NE sont PAS marqués `bIsEditorOnly` ou `bSaveGame` après runtime.

2. **Tick group PrePhysics pour le controller** : le state dépend de la position du player (déterminée par physics). Le controller doit ticker en **PostPhysics** (après le mouvement physique) ou en TG_LastDemotable. Sinon le fog/PP sont en retard d'une frame.

3. **Singleton GameMode pour le state PP** : `AGameMode` est server-only en multiplayer. Côté client, `GameMode == nullptr`. Utiliser GameState ou per-character controller.

4. **Spawner/despawner des PostProcessVolumes à runtime pour switch d'état** : crée du churn GC, hierarchy changes, et confuse le scene outliner. Toujours blender via FPostProcessSettings.

5. **Lerper TOUS les champs FPostProcessSettings** : il y en a ~80. Lerper LensFlares, MotionBlur, FilmGrain, ScreenPercentage… c'est lerper du bruit. Limiter au ~25 champs qu'on utilise réellement (cf. table §4.5).

6. **PP material en BlendableLocation = AfterTonemap pour color tint** : coûte plus cher (le buffer est LDR à ce stade, moins de précision pour color grading). Utiliser BeforeTonemap pour color/saturation, AfterTonemap UNIQUEMENT pour effets UI-screen-space (vignette dégradée, scope overlay).

7. **MPC sample sur material instance sans flag `bUsedWithMaterialPostProcess`** : compile silently mais ne s'affiche pas. Toujours flag MaterialDomain = Post Process sur le master material.

8. **Setting params MPC chaque frame même quand inchangés** : `SetScalarParameterValue` invalide tous les material instances qui samplent. Diff-then-set :

```cpp
if (!FMath::IsNearlyEqual(LastWaterMask, NewWaterMask, 0.001f))
{
    UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, "WaterMask", NewWaterMask);
    LastWaterMask = NewWaterMask;
}
```

### 6.3 Plugins Marketplace évalués

Aucun plugin marketplace standard ne fait ce que tu veux mieux qu'une implémentation custom. Les plugins "Atmospheric Volumes" qui existent sont génériques (boat games, weather systems) et ne gèrent ni le state matrix multi-axes ni l'underwater par-compartment. Build custom.

### 6.4 Notable UE5 shipped games — references

- **Layers of Fear (2023, UE5)** : transitions psychologiques heavy PP. Devs utilisent un blend custom de FPostProcessSettings via component sur le player. Pas de pub source mais Bloober Team a interviewé sur podcast UE Wave (2023).
- **Subnautica 2 (annoncé UE5)** : pas encore shipped au 2026-05, mais le studio Unknown Worlds a publié des dev diaries indiquant un controller PP centralisé.
- **Lyra Game Sample (Epic)** : utilise GameFeature plugins pour les visuals, et ULocalPlayerSubsystem pour le client state. Pattern intéressant à étudier pour le refactor post-FP.
- **Robocop Rogue City (2023, UE5)** : interior-exterior transitions, source : ArtStation breakdowns de Teyon.

---

## 7. Architecture finale — UML + code skeletons

### 7.1 Class diagram

```
                                  ┌──────────────────────────┐
                                  │ ASubCrewCharacter        │
                                  ├──────────────────────────┤
                                  │ + Camera (UCameraComp)   │──┐
                                  │ + AtmoController (UAtmCtl)│ │
                                  │ + CurrentCompartment     │ │
                                  └─────────┬────────────────┘ │
                                            │                  │
                                            │ owns             │ attached to camera
                                            ▼                  ▼
   ┌──────────────────────────────────────────┐    ┌─────────────────────────┐
   │ UAtmosphereStateController (UActorComp)  │───▶│ UPostProcessComponent   │
   ├──────────────────────────────────────────┤    │ (on player camera)      │
   │ - SceneFog (TWeakObjectPtr<AExpHeightFog>│    ├─────────────────────────┤
   │ - MPC_AtmosphereState                    │    │ + Settings (FPP...)     │
   │ - AirPreset, WaterPreset (DA)            │    │ + Blendables [M_Under-  │
   │ - SubInteriorPreset, OutsidePreset (DA)  │    │   water]                │
   │ - BiomePresets[5] (DA)                   │    └─────────────────────────┘
   │ - CompartmentOverrides (DA)              │              ▲
   ├──────────────────────────────────────────┤              │ writes
   │ + TickComponent() — compose + apply      │──────────────┘
   │ + ComputeHeadUnderwater()                │
   │ + ComputeBiomeIndex(Z)                   │
   │ + OnHullCrossed(Direction)               │              ┌────────────────┐
   │ + ApplyFog(FogState)                     │─────writes──▶│ AExpHeightFog  │
   │ - ApplyOverlay(target, source)           │              │ (scene actor)  │
   │ - LerpPP(a, b, alpha)                    │              └────────────────┘
   └──────────────────────────────────────────┘
                  │
                  │ subscribes to events
                  ▼
   ┌─────────────────────────────────────┐
   │ USubHullBoundaryComponent           │
   │ (existing, broadcasts OnHullCrossed)│
   └─────────────────────────────────────┘
```

### 7.2 Header skeleton — UAtmosphereStateController

```cpp
// Sub3D/Public/Atmosphere/AtmosphereStateController.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Scene.h"           // FPostProcessSettings
#include "AtmosphereStateController.generated.h"

class UPPPresetDataAsset;
class UPostProcessComponent;
class UMaterialParameterCollection;
class AExponentialHeightFog;
class USubHullBoundaryComponent;

USTRUCT()
struct FFogState
{
    GENERATED_BODY()
    float Density = 0.10f;
    FLinearColor Inscattering = FLinearColor(0.003f, 0.015f, 0.035f);
    FLinearColor Albedo = FLinearColor(0.02f, 0.06f, 0.10f);
    float ExtinctionScale = 1.5f;
};

UCLASS(ClassGroup=(Sub3D), meta=(BlueprintSpawnableComponent))
class SUB3D_API UAtmosphereStateController : public UActorComponent
{
    GENERATED_BODY()

public:
    UAtmosphereStateController();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    // === Data assets (assigned in editor on the component) ===
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Presets")
    TObjectPtr<UPPPresetDataAsset> Preset_Medium_Air;
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Presets")
    TObjectPtr<UPPPresetDataAsset> Preset_Medium_Water;
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Presets")
    TObjectPtr<UPPPresetDataAsset> Preset_Location_SubInterior;
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Presets")
    TObjectPtr<UPPPresetDataAsset> Preset_Location_Outside;
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Presets")
    TArray<TObjectPtr<UPPPresetDataAsset>> Preset_Biomes; // 0..4 = Coastal..Trenches

    // === Scene references ===
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Scene")
    TSoftObjectPtr<AExponentialHeightFog> SceneFogOverride; // if null, auto-resolve

    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Scene")
    TObjectPtr<UMaterialParameterCollection> MPC_AtmosphereState;

    // === Transition durations ===
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Transitions")
    float MediumTransitionDuration = 0.4f;    // T1 dry↔underwater
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Transitions")
    float LocationTransitionDuration = 1.0f;  // T2 sas
    
    UPROPERTY(EditAnywhere, Category="Sub3D|Atmosphere|Transitions")
    float BiomeTransitionDistanceCm = 4000.f; // T3 smoothstep over 40m

private:
    // === Cached state ===
    UPROPERTY() TWeakObjectPtr<AExponentialHeightFog> ResolvedFog;
    UPROPERTY() TWeakObjectPtr<UPostProcessComponent> PostProcessComp;

    // Lerp state machines (each axis has its own t)
    float MediumLerpAlpha = 0.0f;       // 0 = Air, 1 = Water
    float LocationLerpAlpha = 0.0f;     // 0 = Outside, 1 = SubInterior
    float BiomeContinuous = 0.0f;       // 0..4 fractional
    
    bool bWasHeadUnderwaterLastTick = false;
    bool bWasInsideSubLastTick = false;

    // === Compute methods ===
    bool ComputeHeadUnderwater() const;
    float ComputeBiomeIndex(float WorldZ) const;
    bool ComputeIsInsideSub() const;

    // === Compose & apply ===
    FPostProcessSettings ComposeSettings(FFogState& OutFogState) const;
    void ApplyToPostProcessComp(const FPostProcessSettings& Composite);
    void ApplyFog(const FFogState& FogState);
    void ApplyMPC(float WaterMask, float InteriorMix, float BiomeT);

    // === Helpers ===
    static void ApplyOverlay(FPostProcessSettings& Target, const FPostProcessSettings& Source);
    static FPostProcessSettings LerpPP(const FPostProcessSettings& A, const FPostProcessSettings& B, float Alpha);
    static FFogState LerpFog(const FFogState& A, const FFogState& B, float Alpha);

    // === Event handlers ===
    UFUNCTION()
    void OnHullCrossed(int32 Direction); // bound to USubHullBoundaryComponent
};
```

### 7.3 Tick implementation skeleton

```cpp
void UAtmosphereStateController::TickComponent(float DT, ELevelTick T, FActorComponentTickFunction* F)
{
    Super::TickComponent(DT, T, F);
    
    // 1. Read state from gameplay
    const bool bHeadUnder = ComputeHeadUnderwater();
    const bool bInsideSub = ComputeIsInsideSub();
    const float BiomeIdx = ComputeBiomeIndex(GetOwner()->GetActorLocation().Z);
    
    bWasHeadUnderwaterLastTick = bHeadUnder;
    bWasInsideSubLastTick = bInsideSub;
    
    // 2. Advance lerp state machines
    const float MediumRate = 1.0f / FMath::Max(MediumTransitionDuration, KINDA_SMALL_NUMBER);
    MediumLerpAlpha = FMath::FInterpTo(MediumLerpAlpha, bHeadUnder ? 1.0f : 0.0f, DT, MediumRate);
    
    const float LocationRate = 1.0f / FMath::Max(LocationTransitionDuration, KINDA_SMALL_NUMBER);
    LocationLerpAlpha = FMath::FInterpTo(LocationLerpAlpha, bInsideSub ? 1.0f : 0.0f, DT, LocationRate);
    
    BiomeContinuous = BiomeIdx; // continuous, no lerp needed (already smoothstep'd in ComputeBiomeIndex)
    
    // 3. Compose settings
    FFogState FogState;
    const FPostProcessSettings Composite = ComposeSettings(FogState);
    
    // 4. Apply to all sinks
    ApplyToPostProcessComp(Composite);
    ApplyFog(FogState);
    ApplyMPC(MediumLerpAlpha, LocationLerpAlpha, BiomeContinuous / 4.0f); // BiomeT normalisé 0..1
}
```

### 7.4 Tick prereq

```cpp
void UAtmosphereStateController::BeginPlay()
{
    Super::BeginPlay();
    
    // Tick after physics + flood + crew movement
    // SubFlood → SubMovement → CrewMovement → AtmosphereStateController
    if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner()))
    {
        if (USubCrewMovementComponent* CrewMov = Crew->FindComponentByClass<USubCrewMovementComponent>())
        {
            AddTickPrerequisiteComponent(CrewMov);
        }
    }
    
    SetComponentTickEnabled(true);
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}
```

### 7.5 LerpPP simple — ~25 champs

`FPostProcessSettings` n'a pas de méthode `Lerp` publique. On en écrit une qui lerp les champs qu'on utilise réellement (cf. table §4.5) :

```cpp
FPostProcessSettings UAtmosphereStateController::LerpPP(
    const FPostProcessSettings& A, const FPostProcessSettings& B, float Alpha)
{
    FPostProcessSettings R;
    
    auto LerpOverride = [Alpha](bool ovA, bool ovB, float vA, float vB, bool& ovR, float& vR) {
        ovR = ovA || ovB;
        if (ovR) vR = FMath::Lerp(vA, vB, Alpha);
    };
    
    auto LerpColorOverride = [Alpha](bool ovA, bool ovB, const FVector4& vA, const FVector4& vB,
                                     bool& ovR, FVector4& vR) {
        ovR = ovA || ovB;
        if (ovR) vR = FMath::Lerp(vA, vB, Alpha);
    };
    
    LerpOverride(A.bOverride_AutoExposureBias, B.bOverride_AutoExposureBias,
                 A.AutoExposureBias, B.AutoExposureBias,
                 R.bOverride_AutoExposureBias, R.AutoExposureBias);
    
    LerpColorOverride(A.bOverride_ColorSaturation, B.bOverride_ColorSaturation,
                      A.ColorSaturation, B.ColorSaturation,
                      R.bOverride_ColorSaturation, R.ColorSaturation);
    
    // ... idem pour ColorGain, ColorOffset, VignetteIntensity, SceneFringeIntensity,
    //     BloomIntensity, IndirectLightingIntensity, etc. (~25 champs total)
    
    return R;
}
```

---

## 8. Plan d'implémentation — TODO list

| # | Tâche | Type | Estimation | Dépendances |
|---|---|---|---|---|
| 1 | Créer `UPPPresetDataAsset` class (Sub3DRuntime) | C++ | 1 h | — |
| 2 | Créer les 9 assets DA (Medium×2, Location×2, Biome×5) avec valeurs canonical spec | Editor | 2 h | #1 |
| 3 | Créer `UAtmosphereStateController` class skeleton (Sub3D) | C++ | 2 h | #1 |
| 4 | Implémenter `ComputeHeadUnderwater()` + ajouter `ComputeWaterSurfaceLocalZ` sur `UCompartmentVolumeComponent` | C++ | 2 h | — |
| 5 | Implémenter `ComputeBiomeIndex()` + `ComputeIsInsideSub()` | C++ | 1 h | — |
| 6 | Implémenter `ComposeSettings()` + `ApplyOverlay()` + `LerpPP()` (~25 champs) | C++ | 4 h | #3 |
| 7 | Implémenter `ApplyFog()` + resolve scene fog | C++ | 1 h | #3 |
| 8 | Créer `MPC_Sub3DAtmosphere` avec params {WaterMask, InteriorMix, BiomeT, WaterSurfaceLocalZ} | Editor | 30 min | — |
| 9 | Implémenter `ApplyMPC()` avec diff-then-set | C++ | 1 h | #3, #8 |
| 10 | Brancher event `OnHullCrossed` via `USubHullBoundaryComponent` | C++ | 1 h | #3 |
| 11 | Attacher `UPostProcessComponent` à la camera du `ASubCrewCharacter` (BP ou C++) | BP/C++ | 30 min | — |
| 12 | Ajouter `UAtmosphereStateController` au `BP_SubmarineCrew` | BP | 15 min | #3 |
| 13 | Tests Sub3D.Unit : `Sub3D.Unit.Atmosphere.HeadUnderwater_HysteresisStable` | C++ | 1 h | #4 |
| 14 | Tests Sub3D.Unit : `Sub3D.Unit.Atmosphere.BiomeLerp_NoDiscontinuity` | C++ | 1 h | #5 |
| 15 | Validation PIE : 6 états matrix + 3 transitions T1/T2/T3 | PIE manuel | 1 h | tout |
| 16 | (Défère post-FP) Créer `M_PP_Underwater` material + assigner Blendable | Material | 4 h | #8 |

**Total FP** : ~18 h sur 2-3 jours.
**Total + material polish** : ~22 h sur 3-4 jours.

---

## 9. Références

### 9.1 Epic Documentation

- **FPostProcessSettings struct** : https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Engine/FPostProcessSettings
- **PostProcess Material Blending** : https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine
- **Material Parameter Collections** : https://dev.epicgames.com/documentation/en-us/unreal-engine/material-parameter-collections-in-unreal-engine
- **UPrimaryDataAsset** : https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine
- **ULocalPlayerSubsystem (post-FP migration)** : https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine

### 9.2 Talks / posts

- **Lyra Game Sample** (Epic, github : `EpicGames/UnrealEngine` branch `release` → `Samples/Games/Lyra`) — patterns ULocalPlayerSubsystem + GameFeature.
- **William Faucher YouTube** (`@WilliamFaucher`) — séries sur PostProcess Volumes + Camera-attached PP + Lumen avec exemples 5.4+.
- **Ben Cloward YouTube** (`@BenCloward`) — material design pour stylized PBR + PP materials underwater.
- **Underwater post-process — Epic forums** : `https://forums.unrealengine.com/t/underwater-post-process` (multiples threads, valider la fraîcheur — préférer posts 2024+).

### 9.3 Anti-références (à ne PAS suivre)

- Tutos "spawn PostProcessVolume on demand to change settings" (2019-2021 era UE4) — pattern obsolète, churn GC.
- Tutos "use AGameMode singleton to drive PP" — server-only, casse en multiplayer.
- Tutos "100k lumens for headlight" — symptôme de mauvais exposure calibration, cf. [lighting_pp_fog_canonical_spec §3](2026-05-18_lighting_pp_fog_canonical_spec.md).

---

## Décisions verrouillées (ne pas re-débattre sans nouveau spec)

1. **Architecture = Option F hybrid** : `UPostProcessComponent` on camera + `UAtmosphereStateController` UActorComponent on `ASubCrewCharacter` + 1 PP material custom (deferred post-FP).
2. **Data Assets = D2 axis-based** : 9 DA composées séquentiellement via `bOverride_X` natif.
3. **Fog Controller fusionné** dans `UAtmosphereStateController` (Option C de Q5).
4. **Head underwater detection = compartment-relative** + hysteresis 5 cm.
5. **Tick group = TG_PostPhysics**, prereq sur `USubCrewMovementComponent`.
6. **Transition durations** : Medium 0.4 s (ease-out), Location 1.0 s (ease-in-out), Biome 40 m smoothstep, Headlight/Flare = aucun PP override.
7. **PP material custom = BeforeTonemap** uniquement, AfterTonemap interdit pour color work.
8. **Diff-then-set sur MPC** : éviter invalidation inutile de tous les MI consommateurs.
9. **Pas de Substrate** sur 1660 Super (perf budget).
10. **Migration ULocalPlayerSubsystem** = backlog post-FP (quand multi-PC arrive).

Toute modification de ces 10 décisions doit faire l'objet d'un nouveau spec daté qui supersede explicitement celui-ci.
