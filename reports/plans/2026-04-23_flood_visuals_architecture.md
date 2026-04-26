# Architecture Flood Visuals — Water Plane + SDF + Post-Process Underwater

**Date** : 2026-04-23
**Statut** : Validé — prêt pour implémentation
**Scope** : Rendu de l'eau intérieure par compartment + effet underwater caméra. Pipeline BP-friendly pour art designer handoff.

**Amendement 2026-04-24** (rev 2) — Containment mechanism : **Option C v2 — center-visibility ray-march dans le material**.

- Le water plane reste 80 m par défaut. Le containment spatial est porté par un shader Custom HLSL qui, pour chaque pixel P, teste : (1) P est dans la box du CompartmentVolume (AABB en CV-local), (2) la ligne de vue `Center(CV) → P` ne traverse aucun static mesh opaque (ray-march contre Global DF). L'intersection des deux = zone eau visible.
- Approche Signed SDF vertical (Option C v1) **rejetée** (faux positifs/négatifs aux ouvertures, dépendance étanchéité).
- Approche cap mesh géométrique (Option B) **rejetée** : un cap 2D unique ne matche pas la section XY qui varie avec Z pour un compartment à hull courbe ou forme non-prismatique.
- Option C v2 est adaptative par nature (le shader teste à chaque water level la section courante).
- Architecture material : 3 Material Functions — `MF_CompartmentWater_Containment` (algo), `MF_CompartmentWater_EdgePolish` (HullMask + SoftClip existants), `MF_CompartmentWater_Look` (slab BSDF + normals). `M_CompartmentWater` devient thin combiner. `MI_CompartmentWater` instance pour tuning.
- C++ : `UFloodWaterPlaneComponent::ApplyWaterState` drive 5 vector params (CV_Center_WS, CV_HalfExtent, CV_W2L_Row0/1/2) + WaterLevel01 dans le MID chaque tick.
- Guide complet : `reports/guides/2026-04-24_flood_containment_option_c_v2.md`.
- SVGs : `reports/guides/assets/2026-04-24_flood_algo/{01_intersection_concept, 02_algorithm_flow, 03_material_architecture}.svg`.
- Guide cap-mesh obsolète : `2026-04-24_water_cap_authoring_and_material_functions.md` (à ignorer, conservé pour traçabilité historique).

---

## 1. Architecture

```
USubFloodComponent (authority, sim)
   │ CompartmentStates[id].WaterHeightCm
   ▼
UCompartmentVolumeComponent (data provider)
   │ GetWaterSurfaceWorldLocation(), GetWaterLevel01()
   ▼
┌─────────────────────────────────┐      ┌──────────────────────────────┐
│ UFloodWaterPlaneComponent        │      │ UCrewUnderwaterPPComponent    │
│  (1 par UCompartmentVolume)      │      │  (1 sur ASubCrewCharacter)    │
│                                  │      │                               │
│ C++ passif :                     │      │ C++ passif :                  │
│  - SetWorldLocation(Z=surface)   │      │  - CameraZ vs SurfaceZ        │
│  - SetVisibility(level > thresh) │      │  - Blend alpha                │
│                                  │      │  - BP events (enter/exit)     │
│ Material (SDF + SceneDepth)      │      │                               │
│  = TOUTE l'intelligence visuel   │      │ Material PP (tint, distortion,│
│                                  │      │  caustiques, fog)             │
└──────────────────────────────────┘      └───────────────────────────────┘
```

## 2. Séparation stricte C++ / Material

### C++ — Logique minimale, passif

- `UFloodWaterPlaneComponent` : met à jour Z du plan et sa visibilité. Point.
- `UCrewUnderwaterPPComponent` : compare camera Z à water surface Z, interpole blend alpha, fire events BP.
- Aucun calcul de clipping, aucune décision d'affichage selon géométrie en C++.

### Material — Toute l'intelligence visuelle

- `M_CompartmentWater` :
  - `SceneDepth` + `PixelDepth` → discard pixels où hull est devant le plan
  - `DistanceToNearestSurface(WorldPos)` → soft clip au raccord hull/eau (Global DF)
  - Normal map scrolling + Fresnel + Refraction via SceneColor
- `PP_Underwater` :
  - Tint couleur
  - Distortion UV
  - Caustiques projetées (DF + noise)
  - Fog exponential bleu
  - Optionnel : DOF léger, blur, grain

## 3. Responsabilités par composant

### `UCompartmentVolumeComponent` (data provider)

Nouvelles UFUNCTION BlueprintPure :
- `float GetWaterHeightCm() const` — lookup `SubFlood->GetCompartmentWaterHeightCm(CompartmentId)`
- `float GetWaterLevel01() const` — normalisé
- `FVector GetWaterSurfaceWorldLocation() const` — position world de la surface pour ce compartment (prend en compte rotation sub)
- `USubFloodComponent* GetFlood() const` — cache du lookup

### `UFloodWaterPlaneComponent` (visuel par compartment)

- Dérive `USceneComponent`, possède un `UStaticMeshComponent` enfant (le plan)
- Tick per-frame :
  ```
  SurfaceZ = SourceVolume->GetWaterSurfaceWorldLocation().Z
  SetWorldLocation(X_sub, Y_sub, SurfaceZ)
  SetVisibility(SourceVolume->GetWaterLevel01() > VisibilityThreshold01)
  ```
- `UPROPERTY` exposés pour art :
  - `PlaneMesh` (default `/Engine/BasicShapes/Plane.Plane`)
  - `WaterMaterial` (assigné par le designer)
  - `PlaneWorldSizeCm` (80m par défaut)
  - `VisibilityThreshold01` (0.02 par défaut)
- BP events :
  - `BP_OnWaterLevelChanged(float NewLevel01, float NewHeightCm)` — fire quand level change significativement
  - `BP_OnVisibilityChanged(bool bNowVisible)` — pour VFX/sfx

### `UCrewUnderwaterPPComponent` (effet caméra)

- Dérive `UActorComponent`, ajoute un `UPostProcessComponent` sub-component
- Tick per-frame (locally-controlled client uniquement) :
  ```
  SurfaceZ = CurrentCompartment ? CurrentCompartment->GetWaterSurfaceWorldLocation().Z : OceanSurfaceZ
  bIsUnderwater = (CameraZ < SurfaceZ) || EmbarkState == Outside
  TargetAlpha = bIsUnderwater ? 1.f : 0.f
  CurrentAlpha = FInterpTo(CurrentAlpha, TargetAlpha, dt, bIsUnderwater ? BlendInSpeed : BlendOutSpeed)
  PostProcessComp->BlendWeight = CurrentAlpha
  ```
- `UPROPERTY` exposés :
  - `UnderwaterPostProcessMaterial`
  - `BlendInSpeed`, `BlendOutSpeed`
  - `WaterlineCrossThresholdCm` (zone de proximité pour l'event waterline)
  - `OceanSurfaceZ` (default = 0, pour EVA)
- BP events :
  - `BP_OnEnterWater()` — camera passe au-dessous de la surface
  - `BP_OnExitWater()` — camera passe au-dessus
  - `BP_OnWaterlineProximity(float DistanceToSurfaceCm)` — fire chaque tick quand |CameraZ - SurfaceZ| < threshold
- BP queries :
  - `IsUnderwater()`, `GetBlendAlpha()`

### `ASubmarineBase` (intégration)

- Nouveaux UPROPERTY class-level (art designer configure sur le BP Craniata) :
  - `DefaultWaterMaterial`, `DefaultWaterPlaneMesh` (passés aux planes spawnés)
- Bootstrap post-flood-init : itère `UCompartmentVolumeComponent`, spawne 1 `UFloodWaterPlaneComponent` attaché à chacun

### `ASubCrewCharacter` (intégration)

- `CreateDefaultSubobject<UCrewUnderwaterPPComponent>` en constructor
- Nouveau UPROPERTY `DefaultUnderwaterPPMaterial` passé au component en BeginPlay

## 4. Accessibilité BP pour art designer

Tous les params artistiques sont `UPROPERTY(EditAnywhere, BlueprintReadWrite)` :

| Réglable | Où | Quoi |
|---|---|---|
| `M_CompartmentWater` | Craniata BP > `DefaultWaterMaterial` | Material d'eau surface |
| `PP_Underwater` | CrewCharacter BP > `DefaultUnderwaterPPMaterial` | Material PP underwater |
| Plan size | Craniata BP > `DefaultWaterPlaneSizeCm` | 80m par défaut |
| Blend speed | Crew > `BlendInSpeed` / `BlendOutSpeed` | Vitesse transition underwater |
| Visibility threshold | Plane > `VisibilityThreshold01` | 0.02 par défaut |
| Ocean surface Z | Crew > `OceanSurfaceZ` | Pour EVA (global) |

Events BP (bouchon pour audio/VFX) :
- `BP_OnWaterLevelChanged` — VFX bulles
- `BP_OnVisibilityChanged` — sfx loop d'eau démarre/stop
- `BP_OnEnterWater` — **gouttes sur l'écran** (fix waterline glitch)
- `BP_OnExitWater` — secouement caméra + splash
- `BP_OnWaterlineProximity` — modulation sfx surface

## 5. Waterline glitch (expert review)

Le passage caméra à travers la surface a un glitch visuel classique (on voit "sous" le plan 1 frame).

Mitigation : `BP_OnEnterWater` / `BP_OnExitWater` permettent au designer de spawner :
- VFX gouttes sur l'écran (Niagara émis par UI camera)
- Flash distortion brève (2-3 frames de chromatic aberration)
- Splash sfx

L'event `BP_OnWaterlineProximity` permet un effet continu quand la caméra est dans la zone critique.

## 6. Performance (expert review)

**Global Distance Fields** activés :
- `Project Settings > Rendering > Generate Mesh Distance Fields = true`
- `Project Settings > Rendering > Compress Mesh Distance Fields = true` (recommandé)

**Par mesh** :
- Sub hull mesh (intérieur + extérieur) : `Generate Mesh Distance Field = true` dans Static Mesh Editor
- **Distance Field Resolution Scale** : 1.0 par défaut. Si hull complexe, monter à 1.5-2.0 max. Au-delà = overkill.
- Meshes non-coque (petits props) : laisser off si pas utiles pour le DF.

**Water plane perf** :
- 1 plane × nombre de compartments. 5-10 compartments = négligeable.
- Material fetch SceneDepth + DistanceToNearestSurface : cheap.

**PP underwater perf** :
- Material PP fullscreen = ~0.5ms à 1080p.
- Activé uniquement si BlendWeight > 0 → zéro coût quand inactif (via `bEnabled` toggle).

## 7. Phases

### Phase A — Data provider (30 min)

Enrichit `UCompartmentVolumeComponent` avec les 4 getters.
Critère : compile, `GetWaterSurfaceWorldLocation()` log sanity sur un compartment avec un breach.

### Phase B — Water plane component (2-3h)

Crée `UFloodWaterPlaneComponent`. Spawn par `ASubmarineBase` bootstrap après flood init.
Critère : en PIE, 1 plane par compartment visible à la position attendue quand flood > threshold. Material placeholder (flat bleu semi-transparent).

### Phase C — Underwater PP component (1h)

Crée `UCrewUnderwaterPPComponent`. Sub-component sur `ASubCrewCharacter`. Driver de blend + BP events.
Critère : quand crew capsule traverse la water surface, BP events fire + BlendWeight anime. Material PP placeholder (tint bleu flat).

### Phase D — Assets materials (art designer, 2-4h)

`M_CompartmentWater` AAA avec SceneDepth discard + DF soft-clip + normals animées + fresnel + refraction.
`PP_Underwater` AAA avec tint + distortion + caustiques + fog.
Assignés sur le BP Craniata et BP SubmarineCrew respectivement.

## 8. Ordonnancement

```
[NEXT]  Phase A — Data provider
[THEN]  Phase B — Water plane component
[THEN]  Phase C — Underwater PP component
[THEN]  Phase D — Materials AAA (art designer)
[THEN]  Waterline glitch polish (BP events + VFX designer)
[THEN]  Perf pass : DF resolution tuning par mesh
```

## 9. Fichiers impactés

### Nouveaux
| Fichier | Rôle |
|---|---|
| `Source/Sub3D/Submarine/FloodWaterPlaneComponent.h/.cpp` | Plan visuel par compartment |
| `Source/Sub3D/Submarine/CrewUnderwaterPPComponent.h/.cpp` | Driver PP underwater |

### Modifiés
| Fichier | Nature |
|---|---|
| `Source/Sub3D/Submarine/CompartmentVolumeComponent.h/.cpp` | 4 getters BP (data provider) |
| `Source/Sub3D/Submarine/SubmarineBase.h/.cpp` | Spawn planes + UPROPERTY `DefaultWaterMaterial` / `DefaultWaterPlaneMesh` |
| `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` | Sub-component `UnderwaterPP` + UPROPERTY `DefaultUnderwaterPPMaterial` |

### Assets à créer (par le designer)
| Asset | Chemin |
|---|---|
| `M_CompartmentWater` | `Content/Sub3D/Materials/M_CompartmentWater.uasset` |
| `MI_CompartmentWater` | Instance assignée sur le BP Craniata |
| `PP_Underwater` | `Content/Sub3D/Materials/PP_Underwater.uasset` |
| `MI_PP_Underwater` | Instance assignée sur le BP SubmarineCrew |

### Project settings
- `Rendering > Generate Mesh Distance Fields = true`
- `Rendering > Compress Mesh Distance Fields = true`
- Sub hull mesh : `Generate Mesh Distance Field = true`, resolution scale 1.0-1.5

## 10. Notes de design

### Même material PP pour intérieur + EVA

Garantit la continuité visuelle au passage brèche/airlock. Le component active le blend quand :
- `EmbarkState == Outside` (toujours underwater en EVA, surface = `OceanSurfaceZ` global)
- OR `CurrentCompartment && CameraZ < CompartmentVolume->GetWaterSurfaceWorldLocation().Z`

### Plan unique vs par compartment

Par compartment validé — asymétrie nécessaire pour le gameplay (compartment A noyé, compartment B sec avec vue à travers une porte).

### Passivité stricte

Le component C++ ne connaît rien de la coque, des caustiques, ni de la réfraction. Si le designer veut changer le look, il modifie le material, pas le code.
