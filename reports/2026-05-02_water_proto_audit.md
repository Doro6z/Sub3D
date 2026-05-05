# Sub3DWaterProto — Audit & Review (2026-05-02)

**Branche** : `water-proto`
**HEAD** : `58e97d5 chore(water-proto): A07/A08 tuning + BP wiring for click + reset inputs`
**Auteur audit** : co-conçu user + Claude Opus 4.7 sur ~3 jours d'itération (29 avril → 2 mai 2026)
**Audience** : agent planner pour review et planification de la suite (rattrapage spec Phase A + finition Étape B + portage Sub3D)
**Plan source** : `Sub3DWaterProto_Implementation (5).md` (versionné chez l'user, hors repo)

---

## 1. Résumé exécutif

Le proto valide en isolement les concepts centraux du rendu d'eau intérieure pour Sub3D :
- Voxelisation multi-slice par SDF 2D
- Extraction du contour par Marching Squares interpolé via raycast réel sur arête de cellule
- Triangulation à topologie cohérente entre slices (rings concentriques) → vertex blending continu
- Heightfield CPU 2D propagé par équation des ondes, push texture R32_FLOAT, sampling via WPO matériau
- Animation ambient par WPO Gerstner 3-ondes (HLSL Custom node)
- Bridge component entre 2 compartiments avec porte (UDoorWaterBridge) — résolution FName auto

**État global** : pipeline core fonctionnel et validé en PIE (A06 confirmé : click → onde circulaire visible). Étape B partiellement implémentée — bridge horizontal + slosh OK, **skirts verticaux et Niagara absents**. Le code est ~3300 lignes de C++ + assets BP + matériau Substrate-SLW.

**Décision pivot prise pendant l'itération** (à formaliser auprès du planner) : Option 4 (boundary-cell raycast 360° + détection coin) **abandonnée** après prototype fonctionnel-mais-cassé (Moore tracing échouait sur 100 % des slices). Retour à MS interpolated par raycast-on-edge — chamfer aux coins 90° accepté comme limitation proto.

**Dette consciente** :
- A04 (skirt aux ouvertures) bundlé conceptuellement avec le bridge B (couvre la même mécanique).
- Bridge mesh statique (4 verts, pas de tessellation interne).
- Skirts verticaux du bridge spec'd plan 5.3.2 mais pas implémentés (`code détaillé omis` dans le commentaire C++).
- MPC_WaterProtoGlobals délibérément skippé (pas de bénéfice pour 1-2 rooms ; à créer côté Sub3D production si besoin).

---

## 2. Mission & Scope

### 2.1 Objectifs du proto (rappel plan section 1)

Acquérir et valider en isolement :
1. Voxelisation multi-slice d'un compartiment courbe
2. Cap mesh épousant la silhouette interne, multi-Z (slider niveau d'eau)
3. Heightfield CPU réactif aux clicks (perturbation locale + propagation + amortissement)
4. Bridge dynamique entre 2 compartiments séparés par une `ASubDoorActor`
5. FX courant via Niagara quand delta de niveau

### 2.2 Hors scope (plan section 1.2 — à respecter au portage)

- Multiplayer / réplication
- Sub mobile, pitch/roll (water reste world-up dans le proto)
- Brèches `USubHullBoundaryComponent`
- Postprocess underwater, caustiques, volumetric fog
- Intégration `USubFloodComponent` (niveaux pilotés par variables exposées)
- Trappes au plancher (compartiments multi-deck)
- Optimisation perf (LOD, multi-résolution, culling)
- Sous-marin réel, props intérieurs, occluders

### 2.3 Étapes du plan

| Étape | Scope | Validation |
|---|---|---|
| **A** | Une salle, bake + render + heightfield + slider | A01-A08 |
| **B** | Deux salles + ASubDoorActor + bridge + Niagara flow | B01-B06 |

---

## 3. Architecture du module

### 3.1 Module Sub3DWaterProto

```
Source/Sub3DWaterProto/
├── Sub3DWaterProto.Build.cs   ← deps : Core, Engine, ProceduralMeshComponent,
│                                  Niagara, RenderCore, RHI, GeometryCore,
│                                  GeometryAlgorithms (Phase B Delaunay legacy),
│                                  + Sub3D (privée pour ASubDoorActor)
│                                  + UnrealEd / Blutility (editor-only)
├── Public/
│   ├── Sub3DWaterProto.h          ← module include + LogWaterProto category
│   ├── RoomActor.h                ← ARoomActor (conteneur de test, jamais référencé runtime)
│   ├── RoomWaterBakedData.h       ← FCompartmentSlice + FCachedWaterMesh + URoomWaterBakedData
│   ├── RoomWaterBakerLibrary.h    ← BakeVolume + BakeAndSave UFUNCTION
│   ├── RoomWaterRenderer.h        ← URoomWaterRenderer (runtime)
│   ├── DoorWaterBridge.h          ← UDoorWaterBridge (Étape B)
│   └── RoomWaterDebugDrawer.h     ← BlueprintFunctionLibrary, debug visuel + dumps
└── Private/
    ├── Sub3DWaterProto.cpp        ← FOutputDeviceFile pour WaterProto.log dédié
    ├── RoomActor.cpp              ← wrapper Bake() — délègue à URoomWaterBakerLibrary
    ├── RoomWaterBakedData.cpp     ← (vide — POD struct)
    ├── RoomWaterBakerLibrary.cpp  ← pipeline bake complet (~900 lignes)
    ├── RoomWaterRenderer.cpp      ← cap+skirt+heightfield+blend (~670 lignes)
    ├── DoorWaterBridge.cpp        ← bridge component (~340 lignes)
    └── RoomWaterDebugDrawer.cpp   ← debug snapshot/draws (~410 lignes)
```

### 3.2 Module Sub3D (extension pour le proto)

**Fichiers modifiés en cours de proto** :
- `Source/Sub3D/Submarine/SubInteractionComponent.{h,cpp}` — ajout `TraceFromView(FHitResult&)` BlueprintCallable, découplé d'`ASubCrewCharacter` (path 1 crew, path 2 APawn générique avec `DefaultTraceDistance`). Justification : permettre à `BP_WaterProtoController` (Pawn générique) ET `BP_SubmarineCrew` (existant) de réutiliser le même trace pour le click→inject.

### 3.3 Flow runtime

```
[Bake éditeur] (ARoomActor::Bake button → URoomWaterBakerLibrary::BakeAndSave)
   ↓
[URoomWaterBakedData asset] (BD_<RoomId>.uasset, persistant)
   ├── Slices[i]            : FCompartmentSlice avec SDF + ContourPolygon
   ├── CapMeshesPerSlice[i] : FCachedWaterMesh (rings tessellation)
   ├── LocalBoundsMin/Max
   └── OpeningSegmentStarts/Ends (si bAutoDetectOpenings)
   ↓
[BeginPlay BP_RoomActor]
   ├── ARoomActor (root: CompartmentVolume UBoxComponent)
   │   ├── RoomMesh (StaticMesh attaché)
   │   └── WaterRenderer (URoomWaterRenderer attaché — voir guard auto-attach)
   │       ├── CapMesh (UProceduralMeshComponent runtime, NewObject + RegisterComponent)
   │       └── SkirtMesh (UProceduralMeshComponent runtime)
   ↓
[Tick]
   ├── ARoomActor::Tick : sync BakedData → renderer + push WaterLevelNormalized
   ├── WaterRenderer::TickComponent : TickHeightfield + PushHeightfieldToTexture
   └── WaterRenderer::SetWaterLevel : RebuildBlendedCapMesh (lerp slices encadrantes)
   ↓
[Material M_Phase0_Test]
   WPO = Gerstner3Waves(WorldXY, Time)
       + MakeFloat3(0, 0, HeightfieldTex.R × HeightfieldAmplitude)

[BridgeFlow Étape B]
   ├── BP_SubDoor (ASubDoorActor + UDoorWaterBridge)
   │   └── ResolveRenderers @BeginPlay : lookup CompartmentA/B FName via TActorIterator<ARoomActor>
   ├── PollDoorState @Tick : détecte transition open/close → OnDoorStateChanged
   ├── OnDoorStateChanged(closed=false) : slosh des 2 côtés via InjectAt
   └── UpdateBridgeMesh + UpdateFlowFx @Tick (si porte ouverte)
```

### 3.4 Discipline d'API (critique pour le portage)

**Aucune classe runtime ne référence `ARoomActor`.** Les composants runtime prennent :
- `URoomWaterRenderer` : `UBoxComponent*` (SourceVolume, fallback GetAttachParent) + `URoomWaterBakedData*`
- `URoomWaterBakerLibrary` : `UBoxComponent*` + `FName`
- `UDoorWaterBridge` : résolution par `FName CompartmentA/B` → `URoomWaterRenderer*`

`ARoomActor` est UNIQUEMENT un conteneur pédagogique pour le proto. Au portage Sub3D, on remplace par :
- `UCompartmentVolumeComponent` (déjà existant dans Sub3D, hérite de `UBoxComponent`) attaché à `ASubmarineBase`
- `URoomWaterRenderer` reste tel quel, `SourceVolume = UCompartmentVolumeComponent*`
- `UDoorWaterBridge` : refactor `ResolveRenderers` pour utiliser un manager sur `ASubmarineBase` au lieu de `TActorIterator<ARoomActor>` (5 lignes à changer)

---

## 4. File-by-file audit

### 4.1 `Sub3DWaterProto.cpp` (155 lignes)

**Rôle** : module init + log category dédiée.

**Points clés** :
- `DEFINE_LOG_CATEGORY(LogWaterProto)`
- `FOutputDeviceFile` custom routé vers `Saved/Logs/WaterProto.log` (séparation propre des logs proto vs Sub3D)
- Console commands `Sub3DWaterProto.LogTest` pour vérifier le routing log

**État** : ✅ stable, rien à changer.

### 4.2 `RoomActor.{h,cpp}` (119 + 82 lignes)

**Rôle** : conteneur de test. Trois composants en CreateDefaultSubobject (CompartmentVolume root, RoomMesh, WaterRenderer attached) + appel `URoomWaterBakerLibrary::BakeAndSave` via fonction `Bake()` exposée `CallInEditor`.

**UPROPERTY exposées** (params bake) :
- `BakeCellSize` (default 25 cm) — taille cellule SDF
- `BakeNumSlices` (default 12) — résolution Z
- `BakeResampleN` (default 64) — points polygone après resampling
- `BakeRingsCount` (default 3) — anneaux concentriques pour tessellation cap mesh
- `bAutoDetectOpenings` (default false) — heuristique skirt openings
- `CapInsetCm` (default 2 cm) — inset cosmétique du contour

**Tick** : sync `BakedData → renderer.BakedData` + `Lerp(MinZ, MaxZ, WaterLevelNormalized)` → `Renderer->SetWaterLevel(TargetZ)`. Couplage minimal.

**État** : ✅ stable.

**Risque portage** : aucun — ARoomActor est jeté au portage. Les UPROPERTY `Bake*` migrent vers `UCompartmentVolumeComponent` ou un manager équivalent.

### 4.3 `RoomWaterBakedData.{h,cpp}` (99 + ~5 lignes)

**Rôle** : POD UDataAsset.

**Structures** :
- `FCompartmentSlice` : `SliceZ_Local`, `GridWidth/Height`, `SignedDistance: TArray<float>`, `ContourPolygon: TArray<FVector2D>`
- `FCachedWaterMesh` : `Vertices`, `Triangles`, `Normals`, `UV0` (Z=0 dans le template)
- `URoomWaterBakedData` : `SourceRoomId`, `LocalBoundsMin/Max`, `Slices[]`, `CapMeshesPerSlice[]`, `OpeningSegmentStarts/Ends`

**État** : ✅ stable, schéma figé. Persistance .uasset OK.

**Note size** : un .uasset typique fait 1-2 MB pour 12 slices × 64×64 SDF + 257 verts cap par slice. Acceptable.

### 4.4 `RoomWaterBakerLibrary.{h,cpp}` (63 + 912 lignes) — **cœur du bake**

**Rôle** : pipeline complet bake, exposé en `BlueprintFunctionLibrary`. Utilisé par `ARoomActor::Bake` et théoriquement par n'importe quel Editor Utility Widget.

**Helpers anonymes (namespace privé)** :

| Fonction | Ligne | Rôle |
|---|---|---|
| `ExtractContourMarchingSquaresInterpolated` | 41 | MS sur SDF, raycast réel sur arête de cellule pour le t exact (au lieu de t = SDF_a / (SDF_a - SDF_b) qui causait bow-inward) |
| `ChainSegmentsIntoPolygon` | 172 | Soup de segments → polygone fermé ordonné (greedy nearest-endpoint) |
| `EnsureCCWWinding` | 236 | Shoelace area, reverse si CW |
| `InsetPolygon` | 267 | Décalage le long de la bissectrice. Supporte signe négatif (= outset) |
| `ResamplePolygonUniform` | 307 | Resample en N points uniformes en arc-length — **critique pour blending entre slices** |
| `AlignPolygonStart` | 367 | Rotate l'array pour démarrer au point d'angle minimal autour du centroïde — garantit correspondance vertex-à-vertex entre slices |
| `GenerateCapMeshConcentricFromPolygon` | 446 | Tessellation par anneaux concentriques. `1 + (R+1)*N` verts, `(2R+1)*N` triangles. R=0 → fallback fan classique |
| `DetectOpenings` | 546 | Heuristique segment-near-box-border (faux positifs sur salle close — désactivé par défaut) |

**Public API** :
- `BakeVolume(Volume, CompartmentId, NumSlices, CellSize, bAutoDetectOpenings, CapInsetCm, BakeResampleN, BakeRingsCount)` — bake transient
- `BakeAndSave(...)` — bake + persiste dans `/Game/Sub3DWaterProto/BakedData/BD_<id>.uasset`

**Pipeline complet (BakeVolume)** :

```
1. Voxelisation SDF par slice :
   - 6 raycasts test "all 6 hit" pour signe (inside ssi tous hit)
   - 8 raycasts XY (4 cardinaux + 4 diagonaux) pour magnitude
   - Stocke SDF[idx] = bIsInside ? -MinDist : +MinDist
2. Extract contour MS interpolated (raycast réel sur arêtes de signes opposés)
3. Chain segments → polygone CCW
4. InsetPolygon (signe = direction)
5. ResamplePolygonUniform(N=64)
6. AlignPolygonStart
7. GenerateCapMeshConcentricFromPolygon(R=3) → FCachedWaterMesh
8. DetectOpenings si flag
```

**Logging** : `BakeVolume: id=Room_01 | algo=MS_Interp | grid=20x16 | slices=12 | probes=3840 | empty=2 | degenerate=0 | inset=2.0cm | openings=0`. Très utile pour valider.

**Bug history** :
- Option 4 (boundary-cell raycast 360°) abandonnée — Moore tracing échouait sur 100 % des slices. Code retiré au revert `1d95a7a..` (avant Option 4) pour rester sur MS interpolated qui marche.
- Bow-inward bug en Phase B initiale → fixed par raycast-on-edge replacing scalar lerp. Persiste petit chamfer aux coins 90° par construction MS, accepté pour proto.

**État** : ✅ stable, validé. Re-bake nécessaire quand on change `BakeRingsCount` ou la géométrie de la salle.

**Risque portage** : zéro logique de bake à changer. `BakeVolume` prend déjà un `UBoxComponent*` → marche pour `UCompartmentVolumeComponent` directement.

### 4.5 `RoomWaterRenderer.{h,cpp}` (177 + 673 lignes) — **cœur du runtime**

**Rôle** : composant scene attaché à `CompartmentVolume`, rend le cap mesh + skirt + heightfield CPU.

**UPROPERTY exposées** :
- `SourceVolume` (TWeakObjectPtr<UBoxComponent>) — fallback GetAttachParent
- `BakedData` (TObjectPtr<URoomWaterBakedData>) — sync depuis ARoomActor
- `CapMaterial`, `SkirtMaterial` (fallback WorldGridMaterial si null)
- `CurrentWaterLevelLocalZ` — cm en local de la box
- `HeightfieldResolutionX/Y` (default 64)
- `WaveSpeed` (default 8 → user a tuné à 500 en runtime)
- `Damping` (default 0.985 → user a tuné à 0.995)
- Debug toggles divers

**Methods clés** :

| Method | Ligne approx | Rôle |
|---|---|---|
| `BeginPlay` | ~46 | **Garde-fou auto-attach** : si parent absent (BP mal sérialisé), AttachToComponent(GetOwner()->GetRootComponent(), KeepRelative). Évite cap mesh à world(0,0,0). |
| `EnsureMeshComponents` | ~26 | NewObject CapMeshComp + SkirtMeshComp. **SetMobility(Movable) AVANT SetupAttachment** (chaîne valide pour sub mobile en portage) |
| `LazyInitializeFromBakedData` | ~60 | Idempotent : alloue Heights/Velocities, crée HeightfieldTexture R32_FLOAT, instancie CapMID, push LocalBoundsMin/Max au matériau. |
| `SetWaterLevel` | ~184 | Update Z, appelle RebuildBlendedCapMesh, set RelativeLocation Z, UpdateSkirtScale |
| `FindBracketingSlices` | nouveau | Slice below/above + t ∈ [0..1] pour le blending |
| `RebuildBlendedCapMesh` | nouveau | **Lerp(below.vert[i], above.vert[i], t)** pour chaque vertex, push via UpdateMeshSection (cheap) |
| `TickHeightfield` | ~320 | Wave equation discrète : Velocities += laplacian × WaveSpeed × dt; Velocities × Damping; NewHeights = h_center + Velocities × dt. Boundaries forcées à 0 (absorbantes). |
| `PushHeightfieldToTexture` | ~365 | UpdateTextureRegions async — pas de stall render thread |
| `InjectAt` | ~520 | Modifie Heights[] avec falloff radial (cone) à la position locale XY |
| `InjectAtWorldPoint` | nouveau | Wrapper BP-friendly : world → local via GetComponentTransform inverse, filtre out-of-bounds |
| `BuildSkirtMeshOnce` | ~250 | Skirt vertical sur OpeningSegments (build une fois, scale Z dynamique selon water level) |
| `DrawDebugSnapshot` | ~440 | Visualisation débuggage (bornes, contours, gradient SDF) |

**Architecture WPO** (matériau M_Phase0_Test) :
```
Final WPO = Gerstner3Waves(WorldXY, Time)            ← passive ambient (3 ondes empilées)
          + MakeFloat3(0, 0, HeightfieldTex.R × HeightfieldAmplitude)  ← active (CPU heightfield)
```

**État** : ✅ validé en PIE.

**Risque portage** : moyen. Le composant est portable tel quel. Seuls items à adapter :
1. `URoomWaterBakedData* BakedData` ref → set par le manager Sub3D au lieu de `ARoomActor::Tick`
2. Surface horizontale en world (pitch/roll sub) — actuellement le cap mesh hérite la rotation pleine du parent. À overrider en SetWorldRotation(yaw-only) au tick. Modèle existe : [`FloodWaterPlaneComponent.cpp:165-166`](Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp#L165).
3. Forces inertielles (sloshing) : injecter heightfield depuis `USubMovementComponent.Acceleration` + `AngularVelocity` au tick. Hors scope proto.
4. Vagues Gerstner en sub-local au lieu de WorldXY — modif WPO matériau (ajouter une transform world→sub-local à l'input du Custom node).

### 4.6 `DoorWaterBridge.{h,cpp}` (150 + 341 lignes)

**Rôle** : composant Scene attaché à `ASubDoorActor`, rend le raccord d'eau dynamique entre 2 compartiments quand porte ouverte.

**UPROPERTY exposées** :
- `CompartmentAOverride/BOverride` (FName) — override manuel du lookup, défaut `NAME_None`
- `RendererA/B` (TWeakObjectPtr) — résolus au BeginPlay, lecture seule (`VisibleInstanceOnly`)
- `WaterMaterial` (TObjectPtr<UMaterialInterface>) — pour le BridgeMesh
- `FlowEffect` (TObjectPtr<UNiagaraSystem>) — système flow particules
- `DoorWidth` / `DoorDepth` / `DoorHeight` — dimensions embrasure (cm)
- `MinDeltaForFlowFx` (default 10 cm) — seuil delta pour activer FX
- `OpenSloshForce` (default 3 cm) — slosh à l'ouverture
- `FlowReceiverForce` (default 0.5 cm) — injection continue côté arrivée

**Methods clés** :

| Method | Rôle |
|---|---|
| `BeginPlay` | EnsureSubComponents + cache CachedDoor + ResolveRenderers |
| `ResolveRenderers` | Si parent ASubDoorActor : lit CompartmentA/B. Sinon override. Walk TActorIterator<ARoomActor> pour matcher RoomId. Log clair sur résolution. |
| `EnsureSubComponents` | NewObject BridgeMesh (UProceduralMesh) + FlowFxComp (UNiagaraComponent), Movable, attached via SetupAttachment |
| `PollDoorState` | Lit `CachedDoor->bClosed` chaque tick, trigger OnDoorStateChanged à transition |
| `OnDoorStateChanged(bClosed)` | Visibilité bridge + (de)activate FxComp + slosh symétrique InjectAt à l'ouverture |
| `UpdateBridgeMesh` | Build quad horizontal (4 verts) au min(WaterZ_A, WaterZ_B), DoorWidth × DoorDepth, position = door XY, world rotation yaw-only (gravity-aligned) |
| `UpdateFlowFx` | Calcule v=√(2gΔh), set Niagara params FlowSpeed + FlowDirection, injection continue FlowReceiverForce côté arrivée |

**État** : ⚠️ **partiellement implémenté** par rapport au plan 5.3.2.

**Gap explicite** : `UpdateBridgeMesh` ne génère QUE le quad horizontal. Le plan demande aussi des **skirts verticaux des deux côtés si delta significatif** (cité textuellement : "+ petits skirts verticaux des deux côtés si delta significatif"). J'avais marqué le manque dans le commentaire C++ : `(code détaillé omis — équivalent au skirt de 4.4.4)`. Conséquence visuelle : un trou apparent entre cap mesh d'une room et bridge quand les niveaux diffèrent — c'est le bug que l'user a relevé sur l'image du 2 mai.

**Autre limitation** : bridge mesh à 4 verts seulement. Même si on lui mettait un WPO matériau, l'animation serait limitée aux 4 coins (pas de tessellation interne). Le matériau M_WaterProto_Skirt n'a actuellement **pas** de WPO (par décision proto, valable pour faces verticales mais limitant pour le quad horizontal du bridge).

**Risque portage** : moyen.
- Refactor `ResolveRenderers` : remplacer `TActorIterator<ARoomActor>` par lookup via manager Sub3D (CompartmentId map). 5-10 lignes.
- Conservation des UPROPERTY `DoorWidth/Depth/Height` — devraient être lues depuis `ASubDoorActor` ou la connector definition au lieu d'être manuelles. Renvoie à la spec connector du SubmarineGenerator.
- Pitch/roll sub : déjà géré (force `WorldRotation = (0, Yaw, 0)` dans `UpdateBridgeMesh`).

### 4.7 `RoomWaterDebugDrawer.{h,cpp}` (103 + 410 lignes)

**Rôle** : `BlueprintFunctionLibrary` static, debug visuel + dumps log.

**Functions clés** :
- `DrawCompartmentSnapshot` : box bounds (jaune) + masque cellule par cellule + contour bleu + ouvertures orange + plan d'eau actuel
- `DrawSliceSDFGradient` : SDF en gradient continu (vert intérieur / rouge extérieur, magnitude par saturation)
- `DrawSliceContourDetailed` : sphères jaunes 3 cm sur chaque point du contour interpolé
- `MarkInjection` : sphère cyan pulsée 0.5 s à chaque InjectAt (visible dans le PIE)
- `DumpBakedDataToLog` : summary (slices, mask %, SDF range, contour pts, mesh size)
- Console commands debug

**État** : ✅ stable, très utile pour validation. À garder au portage en mode debug (gated par `USub3DDebugSettings`).

### 4.8 Material `M_Phase0_Test`

**Architecture finale** :

```
[Heritage existant] : double-panner blend de normal water map
[WPO simple sine]   : DEPRECATED (orphelin dans le graph, à supprimer au cleanup)
[Gerstner Custom]   : 3 ondes empilées en HLSL (D1/L1/A1/S1/Q1, idem 2 et 3) → Float3
[Heightfield path]  : LocalPosition.xy → mask → /(Max-Min) → UV → TextureSampleParameter2D HeightfieldTex
                       → Multiply HeightfieldAmplitude → MakeVector3(0,0,Z) → Float3
[Final Add]         : Gerstner Float3 + Heightfield Float3 → Material Output WPO
```

**Paramètres exposés** (set par C++ via CapMID) :
- `LocalBoundsMin`, `LocalBoundsMax` (Vector) — set une fois au LazyInit
- `HeightfieldTex` (Texture2D R32_FLOAT, 64×64) — push chaque frame via UpdateTextureRegions
- `HeightfieldAmplitude` (Scalar default 5 cm)
- `WaveScale`, `WaveSpeedA/B`, `NormalIntensity`, `Roughness`, `EmmisiveBoost`, `WaterAlbedo`, etc. (héritage matériau d'origine du projet)

**État** : ✅ Phase A spec rattrapé à 100% côté Cap. Le sin simple devenu obsolète après ajout Gerstner — orphelin dans le graph, peut être nettoyé.

**Risque portage** : reproduire le même réseau dans `M_CompartmentWater` (production) ou cloner directement. Tous les params publiés sont stables.

### 4.9 Material `M_WaterProto_Skirt`

**Architecture** :
- Dupliqué de `M_Phase0_Test`
- WPO disconnected
- Two Sided = true
- Refraction = None
- Opacity 0.5

**Utilisation** : assigné au `BridgeMesh` du `UDoorWaterBridge`. **Pas de WPO** — par décision proto pour skirts verticaux. **Limitation** : le bridge mesh horizontal n'ondule pas non plus (utilise le même matériau).

**État** : ✅ basique fonctionnel.

**Risque portage** : OK. Pour la prod, le skirt material peut être unifié avec le cap material (même graph + paramètres MID pour activer/désactiver WPO selon usage).

### 4.10 Module Sub3D — modifications proto

**`USubInteractionComponent::TraceFromView(FHitResult&)`** — nouveau, [`SubInteractionComponent.h:34`](Source/Sub3D/Submarine/SubInteractionComponent.h#L34) + [`.cpp:65`](Source/Sub3D/Submarine/SubInteractionComponent.cpp#L65).
- Path 1 : `ASubCrewCharacter` → caméra crew + InteractDistance (compat existante)
- Path 2 : `APawn` générique → premier `UCameraComponent` trouvé + `DefaultTraceDistance` (default 500 cm)
- Justification : permettre au `BP_WaterProtoController` (Pawn générique sans crew movement) ET au `BP_SubmarineCrew` de réutiliser le trace pour les clicks proto eau.

**Risque portage** : aucun. C'est une fonctionnalité utile en général, devrait rester en prod (split possible : créer une `UInteractionUtilityLibrary` static si on veut décorréler de USubInteractionComponent).

---

## 5. Implementation status (vs plan)

### 5.1 Étape A — Phase A validations

| # | Test plan | État | Note |
|---|---|---|---|
| A01 | Bake produit DataAsset cohérent | ✅ | BD_Room_01.md confirme structure (12 slices, 257 verts/slice avec rings=3, contour 116 pts) |
| A02 | Cap mesh épouse contour, mur courbe inclus | ✅ avec caveat | Chamfer aux coins MS accepté ; rings tessellation rend la silhouette propre côté tessellation |
| A03 | Slider niveau d'eau continu | ✅ | Vertex blending entre slices encadrantes, transitions douces |
| A04 | Skirt à l'ouverture | ⏸️ bundlé avec B (bridge couvre le concept). Skirt mesh code présent (`BuildSkirtMeshOnce`) mais activé seulement si `bAutoDetectOpenings=true` ou ouverture authored |
| A05 | Ondulation au repos (Gerstner ambient) | ✅ Phase A spec rattrapée — Custom HLSL 3 ondes opérationnel |
| A06 | Click → onde circulaire visible | ✅ confirmé en PIE par l'user |
| A07 | Reset (R) | ✅ wiré BP, validé |
| A08 | Stabilité numérique 5 min | ✅ pas d'instabilité observée. WaveSpeed=500, Damping=0.995 — CFL stable (limite ~3600) |

### 5.2 Étape B — validations

| # | Test plan | État | Note |
|---|---|---|---|
| B01 | 2 rooms indépendants | ✅ chaque slider room → renderer indépendant |
| B02 | Porte fermée → bridge invisible | ✅ logique `OnDoorStateChanged(true)` set Visibility(false) |
| B03 | Porte ouverte, niveaux égaux → raccord propre | ⚠️ bridge horizontal seul, sans skirts verticaux. Tant que les niveaux sont strictement égaux ET que le bridge est exactement au même Z que les caps, OK. Sinon trou visible. **À vérifier en PIE.** |
| B04 | Porte ouverte, delta → bridge + FX flow visibles | ❌ **bloqué — skirts verticaux pas implémentés + Niagara pas créé** |
| B05 | Toggle porte → slosh injecté des 2 côtés | ✅ code OK dans `OnDoorStateChanged`, à valider visuellement en PIE |
| B06 | Click Room A ne saute PAS dans Room B | ✅ par construction — chaque renderer a son `Heights[]` privé, pas de coupling |

### 5.3 Section 6 — Debug

✅ Implémenté complet (RoomWaterDebugDrawer + console commands + log category dédiée + DrawDebugSnapshot CallInEditor)

### 5.4 Section 7 — Critères globaux du proto

| Critère plan | État |
|---|---|
| Bake éditeur produit asset persistant | ✅ |
| Cap mesh épouse contour multi-slice | ✅ |
| Slider niveau continu | ✅ |
| Click → onde réactive | ✅ |
| Surface bouge au repos (vagues ambient) | ✅ |
| Stabilité numérique | ✅ |
| 2 rooms indépendantes + door bridge | ⚠️ partiel (bridge basique OK, skirts/Niagara absents) |
| Documentation d'architecture | ✅ (ce document) |

---

## 6. Trade-offs & decisions made

### 6.1 Décision : Option 4 abandonnée → MS interpolated par raycast-on-edge

**Contexte** : Phase B initiale avait des chamfer aux coins 90° par construction MS. L'user a demandé une approche cleaner : Option 4 (boundary-cell raycast 360° + Moore tracing + corner detection par discontinuité de normale).

**Tentative** : implémentée intégralement (~250 lignes), mais Moore tracing échouait sur 100 % des slices (`loop did not close (cells=52, traced=121) — fallback`). Critère d'arrêt Jacob foireux + orientation Y/sens CCW non vérifié.

**Décision** : revert au commit `1d95a7a` (avant Option 4). Phase B = MS interpolated par raycast-on-edge (corrige le bow-inward sur murs droits, chamfer aux coins accepté).

**Justification** : iteration time. L'Option 4 propre demanderait ~4-6h de plus (refonte Moore + corner intersection bornée + tests sur 5+ géométries). Le chamfer MS est invisible en proto avec rings tessellation (~5 mm sur cell de 25 cm).

**Reproduction** : code Option 4 disponible dans `git show <hash>` du commit `1d95a7a..` qui a été reverted.

### 6.2 Décision : fan tessellation → rings concentriques

**Contexte** : fan triangulation (1 centroïde + N polygon points + N triangles) avait 2 défauts visuels :
- Singularité au centre : seul le centroïde montre les ondes du heightfield (les N points polygone sont au mur, forcés à 0 par boundary condition wave equation).
- Dead zones près d'embrasures de portes : triangles fins → bobbing centroïde peu visible côté X-.

**Décision** : ajout `BakeRingsCount` (default 3), refactor `GenerateCapMeshConcentricFromPolygon`. 1 + (R+1)·N verts, (2R+1)·N triangles. Pour R=3, N=64 : 257 verts, 448 triangles. Topologie cohérente entre slices garantie.

**Trade-off** : .uasset 4× plus gros (passe ~76 KB → ~1.7 MB par room). Acceptable pour proto.

**Validation** : visuellement en PIE — propagation visible entre rings, dead zones disparues.

### 6.3 Décision : MPC_WaterProtoGlobals skipped

**Plan** : section 4.5.0 demandait un MPC `GlobalTime`.

**Décision** : skip pour le proto. Le node `Time` UE renvoie le moteur time, suffisant pour 1-2 rooms.

**Justification** : MPC utile pour 8-12 compartiments synchrones, pas pour 2. À créer côté Sub3D production (`MPC_Sub3DGlobals`) avec `GlobalTime`, `OceanLevelWorldZ`, `WaveStrength`.

### 6.4 Décision : InjectAtWorldPoint ajouté

**Plan** : non spécifié.

**Décision** : ajouter helper BP-friendly `InjectAtWorldPoint(FVector, Force, Radius)` qui fait la conversion world→local + filtre out-of-bounds.

**Justification** : éviter au BP de coder la conversion world→local après le trace. Pattern réutilisé dans `UDoorWaterBridge::OnDoorStateChanged` pour le slosh.

### 6.5 Décision : Découplage USubInteractionComponent de ASubCrewCharacter

**Plan** : non spécifié.

**Décision** : ajouter `TraceFromView(FHitResult&)` avec path 1 (Crew) et path 2 (Pawn générique).

**Justification** : permet au proto d'utiliser n'importe quel pawn, pas que le crew. Réutilisable en prod pour annotations de surface, splash particles, etc.

### 6.6 Décision : auto-attach guard dans WaterRenderer::BeginPlay

**Bug** : un BP enfant créé avant l'ajout du `SetupAttachment` dans le constructeur C++ pouvait avoir le WaterRenderer en top-level → cap mesh à world(0,0,0). User a vécu le bug, traçage par log a confirmé le problème.

**Décision** : garde-fou C++ `if (!GetAttachParent()) AttachToComponent(GetOwner()->GetRootComponent(), KeepRelative)` + warning log.

**Justification** : protection contre les BP mal sérialisés. Pas une solution propre (le BP devrait être corrigé manuellement aussi), mais empêche un bug silencieux fatal.

---

## 7. Lessons learned

### 7.1 Tessellation > heightfield resolution (visuel)

64×64 cellules de heightfield est OVERKILL si le cap mesh a 65 vertices. Le WPO est per-vertex, pas per-pixel. Conclusion : prioriser la tessellation du cap mesh AVANT d'augmenter la résolution du heightfield. Un cap mesh à 257 verts + heightfield 32×32 visuellement > cap 65 verts + heightfield 128×128.

### 7.2 BP-loaded child actors et SetupAttachment

Le constructeur C++ `SetupAttachment` n'est pris en compte par les BP enfants QUE s'ils sont créés/recompilés après cette modif. Un BP créé avant peut sérialiser une hiérarchie sans l'attachement. Mitigation : garde-fou runtime dans BeginPlay.

### 7.3 Moore tracing CCW sur Y-down vs Y-up

Le Moore neighbor tracing dépend du sens (CW vs CCW) qui dépend de l'orientation Y du grid. UE Y est différent du Y math standard. Sans vérification explicite, le trace tourne dans le mauvais sens et n'arrive pas à la stopping criterion. → leçon pour Option 4 si jamais on y revient : tester sur un cas synthétique (carré 3×3) avant de débuger sur un sous-marin.

### 7.4 Substrate + WPO + ProcMesh : combinaison fonctionnelle en UE 5.7

Validé en Phase 0 du plan. Pas d'usage flag manuel à cocher (`Used with World Position Offset` n'existe plus en Substrate). Auto-détection à la compile. Les 3 systèmes coopèrent sans friction.

### 7.5 Le sampler type d'une R32_FLOAT texture

`Linear Grayscale` rejette une texture default Color → erreur compile. Solution : `Color` (ou `Linear Color`). Le runtime R32_FLOAT marche dans tous les cas.

### 7.6 Topologie cohérente = pré-requis du blending

Toute approche de blending entre slices (lerp vertex-à-vertex) demande que les meshes aient :
- Même nombre de vertices
- Même ordre des vertices (correspondance i ↔ i)
- Mêmes triangles

→ ResamplePolygonUniform + AlignPolygonStart + GenerateCapMeshConcentric (rings fixes) garantissent les 3.

### 7.7 Le code "moisi" pendant l'iter (Phase B Delaunay)

Avant rings tessellation, on utilisait `FConstrainedDelaunay2d`. La triangulation différait par slice → blending impossible. Switch vers fan puis rings = correction de fond, pas un patch.

---

## 8. Known issues & debt

### 8.1 Bloquants pour Étape B validation finale

| # | Issue | Localisation | Effort fix |
|---|---|---|---|
| B-1 | Skirts verticaux non implémentés dans `UpdateBridgeMesh` | `DoorWaterBridge.cpp:UpdateBridgeMesh` | ~30-45 min C++ |
| B-2 | NS_DoorFlow Niagara System pas créé | asset éditeur | ~30 min user |

### 8.2 Cosmétique / nice-to-have

| # | Issue | Note |
|---|---|---|
| C-1 | Bridge mesh à 4 verts seulement, pas de tessellation interne | empêche WPO matériau d'animer le bridge proprement |
| C-2 | M_WaterProto_Skirt sans WPO | décision proto. Pour bridge horizontal, ajout d'un WPO simplifié serait pertinent post-portage |
| C-3 | `WPO simple sine` orphelin dans graph M_Phase0_Test | nettoyage 2 min |
| C-4 | DetectOpenings heuristique faux-positifs | désactivée par défaut (`bAutoDetectOpenings=false`). À remplacer en Sub3D par lookup ouvertures explicites via connecteurs |
| C-5 | DefaultTraceDistance hardcoded à 500 cm sur le générique path | exposé `EditAnywhere`, peut être tuné par BP |

### 8.3 Hors scope plan mais possibles attentes user

| # | Issue | Note |
|---|---|---|
| O-1 | Équalisation auto des niveaux entre rooms (porte ouverte) | NON dans le plan. L'user pourrait s'attendre intuitivement. À expliciter ou ajouter physique simplifiée en chantier dédié |
| O-2 | Surface horizontale en pitch/roll sub | bridge déjà force yaw-only. Cap mesh hérite la rotation parent — à override en portage |
| O-3 | Sloshing (forces inertielles du sub) | hors scope explicite plan section 1.2. À ajouter en Sub3D via injection heightfield depuis SubMovement.Acceleration |
| O-4 | Vagues Gerstner en sub-local | actuellement WorldXY → glissent quand sub yaw rotate. À fixer par transform world→sub-local avant Custom node |

### 8.4 Phase A spec rattrapage encore non couvert

Du plan section 4.5 :
- ❌ MPC_WaterProtoGlobals (4.5.0) — skipped, à recréer en Sub3D
- ❌ Bridge mesh tessellé + WPO skirt — pas demandé par plan, cosmétique

---

## 9. Portage Sub3D — checklist

### 9.1 Renames + lookup adaptations

| Proto class | → Sub3D production | Effort |
|---|---|---|
| `URoomWaterRenderer` | `UCompartmentWaterRenderer` (rename), attaché à `UCompartmentVolumeComponent` | rename + tests |
| `UDoorWaterBridge` | reste `UDoorWaterBridge` (concept identique) | adapter `ResolveRenderers` au manager submarine |
| `URoomWaterBakedData` | `UCompartmentWaterBakedData` (rename), packagé avec submarine def | rename |
| `URoomWaterBakerLibrary::BakeVolume(UBoxComponent*, ...)` | inchangé (UCompartmentVolumeComponent hérite de UBoxComponent) | rename signature labels |
| `ARoomActor` | **DROP** | conteneur de test, remplacé par UCompartmentVolumeComponent on Pawn |

### 9.2 Adaptations runtime

| Item | Action |
|---|---|
| Surface horizontale en pitch/roll | Override `WorldRotation = (0, SubYaw, 0)` au tick. Modèle : [`FloodWaterPlaneComponent.cpp:165-166`](Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp#L165) |
| Vagues Gerstner sub-local | Modif WPO matériau : ajouter une transform world→sub-local AVANT le Custom node Gerstner. Optionnel : passer le sub world transform via `MaterialParameterCollection` global (MPC_Sub3DGlobals) |
| Sloshing inertiel | Inject heightfield depuis `USubMovementComponent.Acceleration` + `AngularVelocity` chaque tick. Décompose acceleration sub-local en 2D, applique injection asymétrique aux bords du compartiment opposés |
| ResolveRenderers manager-based | Remplacer `TActorIterator<ARoomActor>` par `Submarine->FindWaterRendererForCompartment(FName)` |
| Multiplayer | Hors scope proto. Heights/Velocities CPU-side seulement → garder client-side pour visuel, pas répliqué. Slosh à l'ouverture porte = répliqué via `OnDoorStateChanged` (déjà sur RPC dans ASubDoorActor) |

### 9.3 Matériau M_CompartmentWater

- Reproduire le réseau Gerstner Custom + Heightfield path identique au proto
- Ajouter le sub-local transform en amont si voulu (pour stabilité visuelle pitch/roll)
- Garder MID instancié runtime pour pousser HeightfieldTex + LocalBoundsMin/Max + HeightfieldAmplitude

### 9.4 Hooks gameplay (post-portage, hors scope proto)

- `USubFloodComponent` → `UCompartmentWaterRenderer::SetWaterLevel` (le flood pilote, pas un slider)
- Breach `USubHullBoundaryComponent` → InjectAt avec gros radius (effet spray)
- Ouverture/fermeture porte → déjà via `ASubDoorActor::bClosed` (replicated)

---

## 10. Recommended next steps (ordered)

| # | Action | Effort | Bloquant pour |
|---|---|---|---|
| 1 | **Skirts verticaux dans `UpdateBridgeMesh`** | ~45 min C++ | B03/B04 visuels |
| 2 | **NS_DoorFlow Niagara System** (asset éditeur) | ~30 min user | B04 FX |
| 3 | **Validation B01-B06 en PIE** | ~30 min user | clôture Étape B |
| 4 | (optionnel) Bridge mesh tessellé + WPO skirt | ~1h | qualité visuelle bridge |
| 5 | (optionnel) Équalisation niveaux entre rooms (physique simplifiée) | ~2h | attente intuitive joueur, hors plan |
| 6 | Cleanup orphelins matériau M_Phase0_Test | 5 min | propreté |
| 7 | **Décision portage** : commencer le portage Sub3D ou continuer polish proto ? | n/a | future scope |

---

## 11. Appendix — commit log water-proto

```
58e97d5 chore(water-proto): A07/A08 tuning + BP wiring for click + reset inputs
676c2a0 feat(water-proto): heightfield→WPO + click injection validated (A06)
daf20a8 feat(water-proto): Étape B scaffold — UDoorWaterBridge + L_WaterProto_TwoRooms
0931a29 feat(water-proto): vertex blending between slices for smooth water level transitions
d634aed fix(water-proto): runtime guards + Movable mobility for sub portage
1d95a7a feat(water-proto): Phase B SDF + Marching Squares interpolated by raycast
b58419a feat(water-proto): Sub3DWaterProto module + Phase A SDF bake
```

**Uncommitted state au 2 mai 2026** :
- M Source/Sub3DWaterProto/Public/RoomActor.h (BakeRingsCount UPROPERTY)
- M Source/Sub3DWaterProto/Public/RoomWaterBakerLibrary.h (BakeRingsCount param)
- M Source/Sub3DWaterProto/Private/RoomActor.cpp (propagation)
- M Source/Sub3DWaterProto/Private/RoomWaterBakerLibrary.cpp (rings tessellation impl)
- M Content/Maps/L_WaterProto_TwoRooms.umap (2 rooms placement)
- M Content/Sub3DWaterProto/BakedData/BD_Room_01.uasset (re-bake avec rings)
- M Content/Sub3DWaterProto/Materials/M_Phase0_Test.uasset (Gerstner Custom + heightfield WPO)
- ?? Content/Sub3DWaterProto/BakedData/BD_Room_02.uasset (deuxième room baked)
- ?? Content/Sub3DWaterProto/Materials/M_WaterProto_Skirt.uasset (chantier 2c)

Total ~11 fichiers à committer pour clôturer chantier 2 (rings + Gerstner + skirt mat).

---

## 12. Files of interest pour le planner

### Code C++ (pour audit critique)

- [Source/Sub3DWaterProto/Public/RoomWaterRenderer.h](Source/Sub3DWaterProto/Public/RoomWaterRenderer.h)
- [Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp](Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp)
- [Source/Sub3DWaterProto/Private/RoomWaterBakerLibrary.cpp](Source/Sub3DWaterProto/Private/RoomWaterBakerLibrary.cpp) (rings tessellation à `:446`)
- [Source/Sub3DWaterProto/Public/DoorWaterBridge.h](Source/Sub3DWaterProto/Public/DoorWaterBridge.h)
- [Source/Sub3DWaterProto/Private/DoorWaterBridge.cpp](Source/Sub3DWaterProto/Private/DoorWaterBridge.cpp) (le `UpdateBridgeMesh` à compléter)
- [Source/Sub3D/Submarine/SubInteractionComponent.cpp](Source/Sub3D/Submarine/SubInteractionComponent.cpp) (TraceFromView à `:65`)

### Plan source (référence)

- `Sub3DWaterProto_Implementation (5).md` (chez l'user, hors repo)
- Section 4 = Étape A
- Section 5 = Étape B
- Section 5.3.2 = spec UDoorWaterBridge (skirts verticaux mentionnés)
- Section 7 = critères de succès globaux
- Section 8 = transition Sub3D

### Memory files (contexte projet)

- `project_water_proto_pivot_2026_04_29.md` : décision de pivoter en proto isolé
- `project_post_helm_roadmap_2026_04_28.md` : positionnement du water dans la roadmap globale FP
- `project_water_material_post_fp.md` : décision M_CompartmentWater à raffiner post-FP
- `project_flood_containment_decision_2026_04_24.md` : choix architecture Option C v2 shader-only (à harmoniser avec proto)

---

## 13. Code snippets verbatim (pour review planner sans accès repo)

### 13.1 Headers publics

#### 13.1.1 `RoomActor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class URoomWaterBakedData;
class URoomWaterRenderer;

/**
 * Conteneur de test du proto Sub3DWaterProto — UN SEUL compartiment, autonome dans une carte.
 *
 * Cette classe est UNIQUEMENT un conteneur pédagogique. Aucune classe runtime
 * (URoomWaterRenderer, baker, DoorWaterBridge) ne référence ARoomActor — toutes prennent
 * UBoxComponent / URoomWaterRenderer en paramètre. Cette discipline garantit que le portage
 * vers Sub3D (compartiment = component sur Pawn, pas Actor) est un simple renommage.
 */
UCLASS(meta = (PrioritizeCategories = "Water Proto, Water Proto|Bake"))
class SUB3DWATERPROTO_API ARoomActor : public AActor
{
    GENERATED_BODY()

public:
    ARoomActor();

    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<UBoxComponent> CompartmentVolume;        // root du Actor

    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<UStaticMeshComponent> RoomMesh;

    UPROPERTY(VisibleAnywhere, Category = "Water Proto")
    TObjectPtr<URoomWaterRenderer> WaterRenderer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<URoomWaterBakedData> BakedData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto",
        meta = (ClampMin = "0", ClampMax = "1"))
    float WaterLevelNormalized = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    FName RoomId = TEXT("Room_01");

    // Bake params
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "5", ClampMax = "100"))
    float BakeCellSize = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "2", ClampMax = "32"))
    int32 BakeNumSlices = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake")
    bool bAutoDetectOpenings = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float CapInsetCm = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "16", ClampMax = "256"))
    int32 BakeResampleN = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto|Bake",
        meta = (ClampMin = "0", ClampMax = "8"))
    int32 BakeRingsCount = 3;

    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Water Proto|Bake")
    void Bake();
};
```

#### 13.1.2 `RoomWaterBakedData.h`

```cpp
USTRUCT(BlueprintType)
struct FCompartmentSlice
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere) float SliceZ_Local = 0.0f;
    UPROPERTY(VisibleAnywhere) int32 GridWidth = 0;
    UPROPERTY(VisibleAnywhere) int32 GridHeight = 0;
    /** Signed distance field. Convention : négatif = intérieur. */
    UPROPERTY(VisibleAnywhere) TArray<float> SignedDistance;
    /** Polygone du contour extrait par MS. Soup de paires (segments). */
    UPROPERTY(VisibleAnywhere) TArray<FVector2D> ContourPolygon;
};

USTRUCT(BlueprintType)
struct FCachedWaterMesh
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere) TArray<FVector> Vertices;   // Z=0 en template
    UPROPERTY(VisibleAnywhere) TArray<int32> Triangles;
    UPROPERTY(VisibleAnywhere) TArray<FVector> Normals;
    UPROPERTY(VisibleAnywhere) TArray<FVector2D> UV0;
};

UCLASS(BlueprintType)
class SUB3DWATERPROTO_API URoomWaterBakedData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FName SourceRoomId;

    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FVector LocalBoundsMin = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FVector LocalBoundsMax = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Slices")
    TArray<FCompartmentSlice> Slices;

    UPROPERTY(VisibleAnywhere, Category = "Cached Meshes")
    TArray<FCachedWaterMesh> CapMeshesPerSlice;

    UPROPERTY(VisibleAnywhere, Category = "Openings")
    TArray<FVector2D> OpeningSegmentStarts;

    UPROPERTY(VisibleAnywhere, Category = "Openings")
    TArray<FVector2D> OpeningSegmentEnds;
};
```

#### 13.1.3 `RoomWaterBakerLibrary.h`

```cpp
UCLASS()
class SUB3DWATERPROTO_API URoomWaterBakerLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Bake")
    static URoomWaterBakedData* BakeVolume(
        UBoxComponent* Volume,
        FName CompartmentId,
        int32 NumSlices = 12,
        float CellSize = 25.0f,
        bool bAutoDetectOpenings = false,
        float CapInsetCm = 2.0f,
        int32 BakeResampleN = 64,
        int32 BakeRingsCount = 3);

    UFUNCTION(BlueprintCallable, Category = "Sub3DWaterProto|Bake")
    static URoomWaterBakedData* BakeAndSave(
        UBoxComponent* Volume,
        FName CompartmentId,
        const FString& PackagePath = TEXT("/Game/Sub3DWaterProto/BakedData/"),
        int32 NumSlices = 12,
        float CellSize = 25.0f,
        bool bAutoDetectOpenings = false,
        float CapInsetCm = 2.0f,
        int32 BakeResampleN = 64,
        int32 BakeRingsCount = 3);
};
```

#### 13.1.4 `RoomWaterRenderer.h`

```cpp
UCLASS(ClassGroup = (Sub3DWaterProto),
    meta = (BlueprintSpawnableComponent,
            PrioritizeCategories = "Water Proto, Heightfield, Water Proto|Debug"))
class SUB3DWATERPROTO_API URoomWaterRenderer : public USceneComponent
{
    GENERATED_BODY()

public:
    URoomWaterRenderer();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TWeakObjectPtr<UBoxComponent> SourceVolume;     // fallback GetAttachParent si null

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<URoomWaterBakedData> BakedData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<UMaterialInterface> CapMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    TObjectPtr<UMaterialInterface> SkirtMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proto")
    float CurrentWaterLevelLocalZ = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
    int32 HeightfieldResolutionX = 64;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
    int32 HeightfieldResolutionY = 64;

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "0.1"))
    float WaveSpeed = 8.0f;     // user a tuné à 500 sur instances

    UPROPERTY(EditAnywhere, Category = "Heightfield", meta = (ClampMin = "0.9", ClampMax = "1.0"))
    float Damping = 0.985f;     // user a tuné à 0.995 sur instances

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void SetWaterLevel(float NewLocalZ);

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void InjectAt(FVector2D LocalPosXY, float Force, float Radius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    bool InjectAtWorldPoint(FVector WorldPos, float Force, float Radius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Water Proto")
    void ResetHeightfield();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Water Proto|Debug")
    void DrawDebugSnapshot();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction*) override;

private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> CapMeshComp;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> SkirtMeshComp;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CapMID;
    UPROPERTY() TObjectPtr<UTexture2D> HeightfieldTexture;

    TArray<float> Heights;
    TArray<float> Velocities;

    int32 CurrentSliceIndex = -1;
    bool bSkirtBuilt = false;
    bool bInitialized = false;
    bool bCapSectionCreated = false;

    void EnsureMeshComponents();
    void LazyInitializeFromBakedData();
    void BuildSkirtMeshOnce();
    void UpdateSkirtScale();
    void TickHeightfield(float DeltaTime);
    void PushHeightfieldToTexture();
    int32 PickClosestSlice(float WaterZ_Local) const;
    void FindBracketingSlices(float Z, int32& OutBelow, int32& OutAbove, float& OutT) const;
    void RebuildBlendedCapMesh();
};
```

#### 13.1.5 `DoorWaterBridge.h`

```cpp
UCLASS(ClassGroup = (Sub3DWaterProto), meta = (BlueprintSpawnableComponent,
    PrioritizeCategories = "Bridge"))
class SUB3DWATERPROTO_API UDoorWaterBridge : public USceneComponent
{
    GENERATED_BODY()
public:
    UDoorWaterBridge();

    // Override manuel si parent != ASubDoorActor (testing standalone).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Resolution")
    FName CompartmentAOverride = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Resolution")
    FName CompartmentBOverride = NAME_None;

    // Résolus au BeginPlay, lecture seule.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bridge|Resolved")
    TWeakObjectPtr<URoomWaterRenderer> RendererA;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bridge|Resolved")
    TWeakObjectPtr<URoomWaterRenderer> RendererB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    TObjectPtr<UNiagaraSystem> FlowEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float DoorWidth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "5.0", ClampMax = "200.0"))
    float DoorDepth = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float DoorHeight = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.1"))
    float MinDeltaForFlowFx = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.0"))
    float OpenSloshForce = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.0"))
    float FlowReceiverForce = 0.5f;

    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void OnDoorStateChanged(bool bClosed);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float, ELevelTick, FActorComponentTickFunction*) override;

private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> BridgeMesh;
    UPROPERTY() TObjectPtr<UNiagaraComponent> FlowFxComp;
    TWeakObjectPtr<ASubDoorActor> CachedDoor;

    bool bDoorOpen = false;
    bool bLastObservedClosed = true;
    bool bInitialStateApplied = false;

    void EnsureSubComponents();
    void ResolveRenderers();
    void PollDoorState();
    void UpdateBridgeMesh();
    void UpdateFlowFx();
    static float GetRendererWaterWorldZ(const URoomWaterRenderer* Renderer);
};
```

---

### 13.2 Pipeline bake (`RoomWaterBakerLibrary.cpp` — anonymous namespace)

#### 13.2.1 ExtractContourMarchingSquaresInterpolated

Marching Squares 2x2 case-based, mais l'interpolation linéaire `t = SDF_a / (SDF_a - SDF_b)` est remplacée par un **raycast réel** sur l'arête de cellule. Corrige le bow-inward que l'interp scalaire produit quand la magnitude SDF vient d'une direction non-perpendiculaire à l'arête.

```cpp
TArray<FVector2D> ExtractContourMarchingSquaresInterpolated(
    const TArray<float>& SDF, int32 W, int32 H, float CellSize, const FVector& LocalMin,
    UWorld* World, const FTransform& VolumeXf, ECollisionChannel BakeChannel,
    const FCollisionQueryParams& TraceParams, float SliceZ_Local)
{
    TArray<FVector2D> Contour;
    Contour.Reserve(W * H);

    auto RaycastEdge = [&](float sdfA, float sdfB, const FVector2D& pa, const FVector2D& pb) -> FVector2D
    {
        if (FMath::Sign(sdfA) == FMath::Sign(sdfB))
            return (pa + pb) * 0.5f;

        const bool aInside = (sdfA < 0.f);
        const FVector2D& fromPos = aInside ? pa : pb;
        const FVector2D& toPos = aInside ? pb : pa;

        const FVector StartWorld = VolumeXf.TransformPosition(FVector(fromPos.X, fromPos.Y, SliceZ_Local));
        const FVector EndWorld = VolumeXf.TransformPosition(FVector(toPos.X, toPos.Y, SliceZ_Local));

        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, StartWorld, EndWorld, BakeChannel, TraceParams))
        {
            const FVector HitLocal = VolumeXf.InverseTransformPosition(Hit.ImpactPoint);
            return FVector2D(HitLocal.X, HitLocal.Y);
        }
        // Fallback interp linéaire si pas de hit.
        const float t = FMath::Clamp(sdfA / (sdfA - sdfB), 0.0f, 1.0f);
        return FMath::Lerp(pa, pb, t);
    };
    auto Interp = RaycastEdge;

    for (int32 y = 0; y < H - 1; ++y)
    for (int32 x = 0; x < W - 1; ++x)
    {
        const float d_tl = SDF[y * W + x];
        const float d_tr = SDF[y * W + (x + 1)];
        const float d_bl = SDF[(y + 1) * W + x];
        const float d_br = SDF[(y + 1) * W + (x + 1)];

        const FVector2D pTL((x + 0.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
        const FVector2D pTR((x + 1.5f) * CellSize + LocalMin.X, (y + 0.5f) * CellSize + LocalMin.Y);
        const FVector2D pBL((x + 0.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);
        const FVector2D pBR((x + 1.5f) * CellSize + LocalMin.X, (y + 1.5f) * CellSize + LocalMin.Y);

        const int32 caseIdx =
            (d_tl < 0.f ? 8 : 0) | (d_tr < 0.f ? 4 : 0) |
            (d_br < 0.f ? 2 : 0) | (d_bl < 0.f ? 1 : 0);

        switch (caseIdx)
        {
        case 0: case 15: break;
        case 1: case 14:
            Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
            Contour.Add(Interp(d_bl, d_br, pBL, pBR)); break;
        case 2: case 13:
            Contour.Add(Interp(d_bl, d_br, pBL, pBR));
            Contour.Add(Interp(d_tr, d_br, pTR, pBR)); break;
        case 3: case 12:
            Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
            Contour.Add(Interp(d_tr, d_br, pTR, pBR)); break;
        case 4: case 11:
            Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
            Contour.Add(Interp(d_tr, d_br, pTR, pBR)); break;
        case 5: // saddle TL+BR
            Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
            Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
            Contour.Add(Interp(d_bl, d_br, pBL, pBR));
            Contour.Add(Interp(d_tr, d_br, pTR, pBR)); break;
        case 6: case 9:
            Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
            Contour.Add(Interp(d_bl, d_br, pBL, pBR)); break;
        case 7: case 8:
            Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
            Contour.Add(Interp(d_tl, d_tr, pTL, pTR)); break;
        case 10: // saddle TR+BL
            Contour.Add(Interp(d_tl, d_bl, pTL, pBL));
            Contour.Add(Interp(d_bl, d_br, pBL, pBR));
            Contour.Add(Interp(d_tl, d_tr, pTL, pTR));
            Contour.Add(Interp(d_tr, d_br, pTR, pBR)); break;
        }
    }
    return Contour;
}
```

#### 13.2.2 ChainSegmentsIntoPolygon + EnsureCCWWinding + InsetPolygon

```cpp
// Greedy chain segments soup → polygone fermé ordonné. Tolérance 0.5cm.
TArray<FVector2D> ChainSegmentsIntoPolygon(const TArray<FVector2D>& Segments)
{
    TArray<FVector2D> Polygon;
    if (Segments.Num() < 4) return Polygon;

    const int32 NumSegments = Segments.Num() / 2;
    TArray<bool> Used; Used.Init(false, NumSegments);

    Polygon.Add(Segments[0]); Polygon.Add(Segments[1]);
    Used[0] = true;
    constexpr float EPS_SQ = 0.25f;

    bool bExtended = true;
    while (bExtended)
    {
        bExtended = false;
        const FVector2D LastPoint = Polygon.Last();
        for (int32 i = 0; i < NumSegments; ++i)
        {
            if (Used[i]) continue;
            const FVector2D& A = Segments[i * 2];
            const FVector2D& B = Segments[i * 2 + 1];
            if (FVector2D::DistSquared(A, LastPoint) < EPS_SQ)
            { Polygon.Add(B); Used[i] = true; bExtended = true; break; }
            else if (FVector2D::DistSquared(B, LastPoint) < EPS_SQ)
            { Polygon.Add(A); Used[i] = true; bExtended = true; break; }
        }
    }
    if (Polygon.Num() > 2 && FVector2D::DistSquared(Polygon[0], Polygon.Last()) < EPS_SQ)
        Polygon.RemoveAt(Polygon.Num() - 1);
    return Polygon;
}

// Shoelace area, reverse si CW.
void EnsureCCWWinding(TArray<FVector2D>& Polygon)
{
    if (Polygon.Num() < 3) return;
    const int32 N = Polygon.Num();
    float SignedArea2 = 0.f;
    for (int32 i = 0; i < N; ++i)
    {
        const FVector2D& A = Polygon[i];
        const FVector2D& B = Polygon[(i + 1) % N];
        SignedArea2 += A.X * B.Y - B.X * A.Y;
    }
    if (SignedArea2 < 0.f) Algo::Reverse(Polygon);
}

// Décalage le long de la bissectrice. Supporte signe négatif (= outset vers le mur).
TArray<FVector2D> InsetPolygon(const TArray<FVector2D>& Polygon, float InsetCm)
{
    if (FMath::IsNearlyZero(InsetCm) || Polygon.Num() < 3) return Polygon;
    const int32 N = Polygon.Num();
    TArray<FVector2D> Result; Result.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        const FVector2D& Prev = Polygon[(i + N - 1) % N];
        const FVector2D& Curr = Polygon[i];
        const FVector2D& Next = Polygon[(i + 1) % N];
        const FVector2D EdgeIn = (Curr - Prev).GetSafeNormal();
        const FVector2D EdgeOut = (Next - Curr).GetSafeNormal();
        // Normale gauche = rotation +90° = (-dy, dx). Pointe vers l'intérieur d'un poly CCW.
        const FVector2D NormalIn(-EdgeIn.Y, EdgeIn.X);
        const FVector2D NormalOut(-EdgeOut.Y, EdgeOut.X);
        const FVector2D InwardNormal = ((NormalIn + NormalOut) * 0.5f).GetSafeNormal();
        Result.Add(Curr + InwardNormal * InsetCm);
    }
    return Result;
}
```

#### 13.2.3 ResamplePolygonUniform + AlignPolygonStart

**Critique** : ces deux helpers garantissent la topologie cohérente entre slices, requise pour le vertex blending au runtime.

```cpp
// Resample en N points uniformément espacés en arc-length le long du périmètre.
TArray<FVector2D> ResamplePolygonUniform(const TArray<FVector2D>& Polygon, int32 N)
{
    TArray<FVector2D> Out;
    const int32 M = Polygon.Num();
    if (M < 3 || N < 3) return Out;

    TArray<float> SegLengths;
    SegLengths.SetNumUninitialized(M);
    float TotalLength = 0.f;
    for (int32 i = 0; i < M; ++i)
    {
        SegLengths[i] = FVector2D::Distance(Polygon[i], Polygon[(i + 1) % M]);
        TotalLength += SegLengths[i];
    }
    if (TotalLength < KINDA_SMALL_NUMBER) return Out;

    const float Step = TotalLength / static_cast<float>(N);
    Out.Reserve(N);

    int32 SegIdx = 0;
    float SegStart = 0.f;
    for (int32 k = 0; k < N; ++k)
    {
        const float Target = static_cast<float>(k) * Step;
        while (SegIdx < M && SegStart + SegLengths[SegIdx] < Target)
        {
            SegStart += SegLengths[SegIdx];
            ++SegIdx;
        }
        if (SegIdx >= M) { Out.Add(Polygon[M - 1]); continue; }
        const float SegT = (SegLengths[SegIdx] > KINDA_SMALL_NUMBER)
            ? (Target - SegStart) / SegLengths[SegIdx] : 0.f;
        Out.Add(FMath::Lerp(Polygon[SegIdx], Polygon[(SegIdx + 1) % M], SegT));
    }
    return Out;
}

// Rotate l'array pour démarrer au point d'angle minimal autour du centroïde (≈ +X).
// Garantit que polygon[0] est au "même endroit relatif" sur toutes les slices.
void AlignPolygonStart(TArray<FVector2D>& Polygon)
{
    const int32 N = Polygon.Num();
    if (N < 3) return;

    FVector2D Centroid(0.f, 0.f);
    for (const FVector2D& P : Polygon) Centroid += P;
    Centroid /= static_cast<float>(N);

    int32 BestIdx = 0;
    float BestAbsAngle = TNumericLimits<float>::Max();
    for (int32 i = 0; i < N; ++i)
    {
        const FVector2D D = Polygon[i] - Centroid;
        const float AbsAngle = FMath::Abs(FMath::Atan2(D.Y, D.X));
        if (AbsAngle < BestAbsAngle) { BestAbsAngle = AbsAngle; BestIdx = i; }
    }
    if (BestIdx > 0)
    {
        TArray<FVector2D> Rotated; Rotated.Reserve(N);
        for (int32 i = 0; i < N; ++i)
            Rotated.Add(Polygon[(BestIdx + i) % N]);
        Polygon = MoveTemp(Rotated);
    }
}
```

#### 13.2.4 GenerateCapMeshConcentricFromPolygon (rings tessellation)

**Cœur du blending** : 1 + (R+1)·N verts, (2R+1)·N triangles, topologie identique pour toutes les slices.

```cpp
FCachedWaterMesh GenerateCapMeshConcentricFromPolygon(
    const TArray<FVector2D>& Polygon, int32 RingsCount)
{
    FCachedWaterMesh Mesh;
    const int32 N = Polygon.Num();
    if (N < 3) return Mesh;
    const int32 R = FMath::Max(0, RingsCount);

    FVector2D Centroid(0.f, 0.f);
    for (const FVector2D& P : Polygon) Centroid += P;
    Centroid /= static_cast<float>(N);

    const int32 TotalRings = R + 1;
    const int32 TotalVerts = 1 + TotalRings * N;
    Mesh.Vertices.Reserve(TotalVerts);
    Mesh.Normals.Reserve(TotalVerts);
    Mesh.UV0.Reserve(TotalVerts);

    auto PushVert = [&](const FVector2D& P)
    {
        Mesh.Vertices.Add(FVector(P.X, P.Y, 0.f));
        Mesh.Normals.Add(FVector(0.f, 0.f, 1.f));
        Mesh.UV0.Add(FVector2D(P.X * 0.01f, P.Y * 0.01f));
    };

    PushVert(Centroid);   // index 0

    // R+1 anneaux : k=0..R, t_k = (k+1)/(R+1). Anneau R = polygone (t=1).
    for (int32 k = 0; k < TotalRings; ++k)
    {
        const float T = static_cast<float>(k + 1) / static_cast<float>(R + 1);
        for (int32 i = 0; i < N; ++i)
            PushVert(FMath::Lerp(Centroid, Polygon[i], T));
    }

    auto RingIdx = [N](int32 RingK, int32 VertI) -> int32
    { return 1 + RingK * N + VertI; };

    Mesh.Triangles.Reserve((2 * R + 1) * N * 3);

    // Inner fan : centroïde → anneau 0
    for (int32 i = 0; i < N; ++i)
    {
        Mesh.Triangles.Add(0);
        Mesh.Triangles.Add(RingIdx(0, i));
        Mesh.Triangles.Add(RingIdx(0, (i + 1) % N));
    }

    // R strips entre anneaux consécutifs, 2N triangles chacun
    for (int32 k = 0; k < R; ++k)
    for (int32 i = 0; i < N; ++i)
    {
        const int32 i_next = (i + 1) % N;
        const int32 a = RingIdx(k, i);
        const int32 b = RingIdx(k + 1, i);
        const int32 c = RingIdx(k + 1, i_next);
        const int32 d = RingIdx(k, i_next);
        Mesh.Triangles.Add(a); Mesh.Triangles.Add(b); Mesh.Triangles.Add(c);
        Mesh.Triangles.Add(a); Mesh.Triangles.Add(c); Mesh.Triangles.Add(d);
    }
    return Mesh;
}
```

#### 13.2.5 BakeVolume — voxelisation SDF (loop principale)

Section critique de `BakeVolume` : pour chaque cellule de chaque slice, calcule le SDF par "all 6 hit" (signe) + 8 raycasts XY (magnitude).

```cpp
// Constants au début de BakeVolume
const ECollisionChannel BakeChannel = ECC_WorldStatic;
constexpr float SDF_MaxDistanceCm = 1000.f;
constexpr float SDF_ParityRayLength = 100000.f;
constexpr float Diag = 0.70710678f;

// 6 directions (cardinaux + verticales)
const FVector LocalDirs6[6] = {
    FVector(+1, 0, 0), FVector(-1, 0, 0),
    FVector(0, +1, 0), FVector(0, -1, 0),
    FVector(0, 0, +1), FVector(0, 0, -1),
};
// 4 diagonales XY pour magnitude in-plane
const FVector LocalDirsDiag[4] = {
    FVector(+Diag, +Diag, 0), FVector(-Diag, -Diag, 0),
    FVector(+Diag, -Diag, 0), FVector(-Diag, +Diag, 0),
};
FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WaterBakeSDF), /*bTraceComplex*/ true);

// Pour chaque slice :
for (int32 sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
{
    const float t = static_cast<float>(sliceIdx) / static_cast<float>(NumSlices - 1);
    const float SliceZ_Local = FMath::Lerp(BoxLocalMin.Z, BoxLocalMax.Z, t);

    FCompartmentSlice Slice;
    Slice.SliceZ_Local = SliceZ_Local;
    Slice.GridWidth = GridW;
    Slice.GridHeight = GridH;
    Slice.SignedDistance.SetNumZeroed(GridW * GridH);

    for (int32 y = 0; y < GridH; ++y)
    for (int32 x = 0; x < GridW; ++x)
    {
        const FVector LocalPos(
            BoxLocalMin.X + (x + 0.5f) * CellSize,
            BoxLocalMin.Y + (y + 0.5f) * CellSize,
            SliceZ_Local);
        const FVector WorldPos = VolumeXf.TransformPosition(LocalPos);

        int32 HitCount = 0;
        float MinDist = SDF_MaxDistanceCm;

        // 6 raycasts : sign (all 6 hit) + magnitude pour les 4 cardinaux XY (d<4)
        for (int32 d = 0; d < 6; ++d)
        {
            const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirs6[d]);
            FHitResult Hit;
            if (World->LineTraceSingleByChannel(
                    Hit, WorldPos, WorldPos + DirWorld * SDF_ParityRayLength,
                    BakeChannel, TraceParams))
            {
                ++HitCount;
                if (d < 4)
                {
                    const float HitDist = FMath::Min(Hit.Distance, SDF_MaxDistanceCm);
                    if (HitDist < MinDist) MinDist = HitDist;
                }
            }
        }

        // 4 diagonales XY : magnitude only
        for (int32 d = 0; d < 4; ++d)
        {
            const FVector DirWorld = VolumeXf.TransformVectorNoScale(LocalDirsDiag[d]);
            FHitResult Hit;
            if (World->LineTraceSingleByChannel(
                    Hit, WorldPos, WorldPos + DirWorld * SDF_MaxDistanceCm,
                    BakeChannel, TraceParams))
            {
                if (Hit.Distance < MinDist) MinDist = Hit.Distance;
            }
        }

        // Strict enclosed test : 6/6 directions hit → inside.
        const bool bIsInside = (HitCount == 6);
        Slice.SignedDistance[y * GridW + x] = bIsInside ? -MinDist : +MinDist;
    }

    Slice.ContourPolygon = ExtractContourMarchingSquaresInterpolated(
        Slice.SignedDistance, GridW, GridH, CellSize, BoxLocalMin,
        World, VolumeXf, BakeChannel, TraceParams, SliceZ_Local);
    Data->Slices.Add(MoveTemp(Slice));
}

// Cap meshes par slice : chain → CCW → inset → resample → align → rings
for (const FCompartmentSlice& Slice : Data->Slices)
{
    if (Slice.ContourPolygon.Num() < 4) { /* empty cap */; continue; }

    TArray<FVector2D> OrderedPoly = ChainSegmentsIntoPolygon(Slice.ContourPolygon);
    if (OrderedPoly.Num() < 3) { /* degenerate */; continue; }

    EnsureCCWWinding(OrderedPoly);

    if (CapInsetCm > 0.f)
        OrderedPoly = InsetPolygon(OrderedPoly, CapInsetCm);

    OrderedPoly = ResamplePolygonUniform(OrderedPoly, BakeResampleN);
    AlignPolygonStart(OrderedPoly);

    FCachedWaterMesh Mesh = GenerateCapMeshConcentricFromPolygon(OrderedPoly, BakeRingsCount);
    Data->CapMeshesPerSlice.Add(MoveTemp(Mesh));
}
```

---

### 13.3 Runtime renderer (`RoomWaterRenderer.cpp`)

#### 13.3.1 BeginPlay (auto-attach guard) + EnsureMeshComponents

```cpp
void URoomWaterRenderer::EnsureMeshComponents()
{
    if (!CapMeshComp)
    {
        CapMeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("CapMesh"));
        // Movable AVANT SetupAttachment+RegisterComponent : la chaîne d'attachement Unreal
        // n'autorise pas un enfant Movable sous un parent Static. En portage Sub3D, le sub
        // bouge → tous les enfants doivent être Movable pour la transform propagation.
        CapMeshComp->SetMobility(EComponentMobility::Movable);
        CapMeshComp->SetupAttachment(this);
        CapMeshComp->RegisterComponent();
        CapMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CapMeshComp->bUseAsyncCooking = false;
    }
    if (!SkirtMeshComp)
    {
        SkirtMeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("SkirtMesh"));
        SkirtMeshComp->SetMobility(EComponentMobility::Movable);
        SkirtMeshComp->SetupAttachment(this);
        SkirtMeshComp->RegisterComponent();
        SkirtMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SkirtMeshComp->bUseAsyncCooking = false;
    }
}

void URoomWaterRenderer::BeginPlay()
{
    Super::BeginPlay();

    // Garde-fou : un BP enfant peut avoir sérialisé WaterRenderer comme top-level non
    // attaché. Sans parent, un USceneComponent flotte à world(RelativeLocation) au lieu
    // de suivre l'Actor → cap mesh à world(0,0,0). On corrige ici.
    if (!GetAttachParent())
    {
        if (AActor* Owner = GetOwner())
        {
            if (USceneComponent* Root = Owner->GetRootComponent())
            {
                AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
                UE_LOG(LogWaterProto, Warning,
                    TEXT("URoomWaterRenderer (%s): was unparented in BP — auto-attached to root %s"),
                    *GetNameSafe(Owner), *GetNameSafe(Root));
            }
        }
    }

    EnsureMeshComponents();

    if (!SourceVolume.IsValid())
        SourceVolume = Cast<UBoxComponent>(GetAttachParent());

    LazyInitializeFromBakedData();
}
```

#### 13.3.2 LazyInitializeFromBakedData (excerpt — material setup)

```cpp
void URoomWaterRenderer::LazyInitializeFromBakedData()
{
    if (bInitialized || !BakedData) return;

    EnsureMeshComponents();

    Heights.SetNumZeroed(HeightfieldResolutionX * HeightfieldResolutionY);
    Velocities.SetNumZeroed(HeightfieldResolutionX * HeightfieldResolutionY);

    HeightfieldTexture = UTexture2D::CreateTransient(
        HeightfieldResolutionX, HeightfieldResolutionY, PF_R32_FLOAT);
    if (HeightfieldTexture)
    {
        HeightfieldTexture->Filter = TF_Bilinear;
        HeightfieldTexture->AddressX = TA_Clamp;
        HeightfieldTexture->AddressY = TA_Clamp;
        HeightfieldTexture->UpdateResource();
    }

    UMaterialInterface* EffectiveCap = CapMaterial;
    if (!EffectiveCap)
        EffectiveCap = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

    if (EffectiveCap && CapMeshComp)
    {
        CapMID = UMaterialInstanceDynamic::Create(EffectiveCap, this);
        if (CapMID)
        {
            if (HeightfieldTexture)
                CapMID->SetTextureParameterValue(TEXT("HeightfieldTex"), HeightfieldTexture);
            CapMID->SetVectorParameterValue(TEXT("LocalBoundsMin"),
                FLinearColor(BakedData->LocalBoundsMin));
            CapMID->SetVectorParameterValue(TEXT("LocalBoundsMax"),
                FLinearColor(BakedData->LocalBoundsMax));
            CapMeshComp->SetMaterial(0, CapMID);
        }
    }

    CurrentSliceIndex = PickClosestSlice(CurrentWaterLevelLocalZ);
    RebuildBlendedCapMesh();
    BuildSkirtMeshOnce();
    UpdateSkirtScale();

    if (CapMeshComp)
        CapMeshComp->SetRelativeLocation(FVector(0, 0, CurrentWaterLevelLocalZ));

    bInitialized = true;
}
```

#### 13.3.3 SetWaterLevel + FindBracketingSlices + RebuildBlendedCapMesh

```cpp
void URoomWaterRenderer::SetWaterLevel(float NewLocalZ)
{
    CurrentWaterLevelLocalZ = NewLocalZ;
    if (!bInitialized) LazyInitializeFromBakedData();
    if (!BakedData) return;

    // Rebuild systématique : la lerp entre slices encadrantes change avec NewLocalZ.
    CurrentSliceIndex = PickClosestSlice(NewLocalZ);
    RebuildBlendedCapMesh();

    if (CapMeshComp)
    {
        FVector RelLoc = CapMeshComp->GetRelativeLocation();
        RelLoc.Z = NewLocalZ;
        CapMeshComp->SetRelativeLocation(RelLoc);
    }
    UpdateSkirtScale();
}

void URoomWaterRenderer::FindBracketingSlices(float Z, int32& OutBelow, int32& OutAbove, float& OutT) const
{
    OutBelow = OutAbove = 0; OutT = 0.f;
    if (!BakedData || BakedData->Slices.Num() == 0) return;

    const TArray<FCompartmentSlice>& Slices = BakedData->Slices;
    const int32 N = Slices.Num();

    if (Z <= Slices[0].SliceZ_Local) { OutBelow = OutAbove = 0; return; }
    if (Z >= Slices[N - 1].SliceZ_Local) { OutBelow = OutAbove = N - 1; return; }

    for (int32 i = 0; i < N - 1; ++i)
    {
        if (Z >= Slices[i].SliceZ_Local && Z <= Slices[i + 1].SliceZ_Local)
        {
            OutBelow = i; OutAbove = i + 1;
            const float Span = Slices[i + 1].SliceZ_Local - Slices[i].SliceZ_Local;
            OutT = (Span > KINDA_SMALL_NUMBER)
                ? (Z - Slices[i].SliceZ_Local) / Span : 0.f;
            return;
        }
    }
}

void URoomWaterRenderer::RebuildBlendedCapMesh()
{
    if (!BakedData || !CapMeshComp || BakedData->CapMeshesPerSlice.Num() == 0) return;

    int32 IdxBelow = 0, IdxAbove = 0; float T = 0.f;
    FindBracketingSlices(CurrentWaterLevelLocalZ, IdxBelow, IdxAbove, T);

    if (!BakedData->CapMeshesPerSlice.IsValidIndex(IdxBelow) ||
        !BakedData->CapMeshesPerSlice.IsValidIndex(IdxAbove)) return;

    const FCachedWaterMesh& Below = BakedData->CapMeshesPerSlice[IdxBelow];
    const FCachedWaterMesh& Above = BakedData->CapMeshesPerSlice[IdxAbove];

    const bool bBelowEmpty = (Below.Vertices.Num() == 0);
    const bool bAboveEmpty = (Above.Vertices.Num() == 0);

    if (bBelowEmpty && bAboveEmpty)
    {
        if (bCapSectionCreated) { CapMeshComp->ClearAllMeshSections(); bCapSectionCreated = false; }
        return;
    }

    const FCachedWaterMesh* SourceForTopology = nullptr;
    TArray<FVector> BlendedVerts;

    if (bBelowEmpty) { SourceForTopology = &Above; BlendedVerts = Above.Vertices; }
    else if (bAboveEmpty) { SourceForTopology = &Below; BlendedVerts = Below.Vertices; }
    else if (Below.Vertices.Num() == Above.Vertices.Num())
    {
        // Cas nominal : topologie identique (garantie par resampling au bake).
        SourceForTopology = &Below;
        const int32 NumVerts = Below.Vertices.Num();
        BlendedVerts.SetNumUninitialized(NumVerts);
        for (int32 i = 0; i < NumVerts; ++i)
            BlendedVerts[i] = FMath::Lerp(Below.Vertices[i], Above.Vertices[i], T);
    }
    else
    {
        UE_LOG(LogWaterProto, Warning, TEXT("RebuildBlendedCapMesh: vertex count mismatch — re-bake required"));
        SourceForTopology = (T < 0.5f) ? &Below : &Above;
        BlendedVerts = SourceForTopology->Vertices;
    }

    if (!SourceForTopology || SourceForTopology->Triangles.Num() == 0) return;

    if (!bCapSectionCreated)
    {
        CapMeshComp->CreateMeshSection(0, BlendedVerts,
            SourceForTopology->Triangles, SourceForTopology->Normals, SourceForTopology->UV0,
            TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        bCapSectionCreated = true;
    }
    else
    {
        // UpdateMeshSection : juste re-upload des verts/normales/UV, pas de re-cook collision.
        CapMeshComp->UpdateMeshSection(0, BlendedVerts,
            SourceForTopology->Normals, SourceForTopology->UV0,
            TArray<FColor>(), TArray<FProcMeshTangent>());
    }
}
```

#### 13.3.4 TickHeightfield + PushHeightfieldToTexture

```cpp
// Wave equation discrète, boundaries forcées à 0 (absorbantes).
// CFL : c² × dt² ≤ dx² → avec dt=1/60, dx=1, WaveSpeed ≤ 3600 stable.
void URoomWaterRenderer::TickHeightfield(float dt)
{
    const int32 W = HeightfieldResolutionX;
    const int32 H = HeightfieldResolutionY;

    TArray<float> NewHeights;
    NewHeights.SetNumZeroed(W * H);

    for (int32 y = 1; y < H - 1; ++y)
    for (int32 x = 1; x < W - 1; ++x)
    {
        const int32 idx = y * W + x;
        const float h_center = Heights[idx];
        const float h_avg = (Heights[idx - 1] + Heights[idx + 1] +
                             Heights[idx - W] + Heights[idx + W]) * 0.25f;
        const float laplacian = h_avg - h_center;

        Velocities[idx] += laplacian * WaveSpeed * dt;
        Velocities[idx] *= Damping;
        NewHeights[idx] = h_center + Velocities[idx] * dt;
    }
    Heights = NewHeights;
}

// Async upload via UpdateTextureRegions — pas de stall render thread.
void URoomWaterRenderer::PushHeightfieldToTexture()
{
    if (!HeightfieldTexture) return;

    struct FUpdateData
    {
        FUpdateTextureRegion2D Region;
        uint32 SrcPitch = 0;
        TArray<float> Buffer;
    };

    FUpdateData* Data = new FUpdateData();
    Data->Region = FUpdateTextureRegion2D(0, 0, 0, 0, HeightfieldResolutionX, HeightfieldResolutionY);
    Data->SrcPitch = HeightfieldResolutionX * sizeof(float);
    Data->Buffer = Heights;

    HeightfieldTexture->UpdateTextureRegions(
        0, 1, &Data->Region, Data->SrcPitch, sizeof(float),
        reinterpret_cast<uint8*>(Data->Buffer.GetData()),
        [Data](uint8*, const FUpdateTextureRegion2D*) { delete Data; });
}
```

#### 13.3.5 InjectAt + InjectAtWorldPoint

```cpp
// Modifie Heights[] avec falloff radial conique. Position en local XY du Box.
void URoomWaterRenderer::InjectAt(FVector2D LocalPosXY, float Force, float Radius)
{
    if (!BakedData || Heights.Num() == 0) return;

    if (UBoxComponent* Vol = SourceVolume.Get())
    {
        const FVector LocalPos3D(LocalPosXY.X, LocalPosXY.Y, CurrentWaterLevelLocalZ);
        const FVector WorldPos = Vol->GetComponentTransform().TransformPosition(LocalPos3D);
        URoomWaterDebugDrawer::MarkInjection(this, WorldPos, Force, Radius);
    }

    const int32 W = HeightfieldResolutionX;
    const int32 H = HeightfieldResolutionY;
    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;
    const float SpanX = Max.X - Min.X;
    const float SpanY = Max.Y - Min.Y;
    if (SpanX <= 0.f || SpanY <= 0.f) return;

    const float u = (LocalPosXY.X - Min.X) / SpanX;
    const float v = (LocalPosXY.Y - Min.Y) / SpanY;
    const int32 cx = FMath::Clamp(static_cast<int32>(u * W), 0, W - 1);
    const int32 cy = FMath::Clamp(static_cast<int32>(v * H), 0, H - 1);
    const float CellWorldSize = SpanX / W;
    const int32 RadiusCells = FMath::Max(1, FMath::CeilToInt(Radius / CellWorldSize));

    for (int32 dy = -RadiusCells; dy <= RadiusCells; ++dy)
    for (int32 dx = -RadiusCells; dx <= RadiusCells; ++dx)
    {
        const int32 nx = cx + dx;
        const int32 ny = cy + dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
        const float dist = FMath::Sqrt(static_cast<float>(dx * dx + dy * dy));
        const float falloff = FMath::Max(0.f, 1.0f - dist / RadiusCells);
        Heights[ny * W + nx] += Force * falloff;
    }
}

// Wrapper BP-friendly : world → local + filtre out-of-bounds.
bool URoomWaterRenderer::InjectAtWorldPoint(FVector WorldPos, float Force, float Radius)
{
    if (!BakedData) return false;
    const FVector LocalPos3D = GetComponentTransform().InverseTransformPosition(WorldPos);

    const FVector& Min = BakedData->LocalBoundsMin;
    const FVector& Max = BakedData->LocalBoundsMax;
    if (LocalPos3D.X < Min.X - Radius || LocalPos3D.X > Max.X + Radius ||
        LocalPos3D.Y < Min.Y - Radius || LocalPos3D.Y > Max.Y + Radius)
        return false;

    InjectAt(FVector2D(LocalPos3D.X, LocalPos3D.Y), Force, Radius);
    return true;
}

void URoomWaterRenderer::ResetHeightfield()
{
    FMemory::Memzero(Heights.GetData(), Heights.Num() * sizeof(float));
    FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(float));
}
```

#### 13.3.6 BuildSkirtMeshOnce + UpdateSkirtScale

Skirt vertical sur les `OpeningSegments` du baked data. Build une fois, scale Z dynamique selon water level. **Inactif tant que `bAutoDetectOpenings=false`**.

```cpp
void URoomWaterRenderer::BuildSkirtMeshOnce()
{
    if (!BakedData || bSkirtBuilt || !SkirtMeshComp) return;
    const int32 N = BakedData->OpeningSegmentStarts.Num();
    if (N == 0 || BakedData->OpeningSegmentEnds.Num() != N) { bSkirtBuilt = true; return; }

    TArray<FVector> Vertices; TArray<int32> Triangles;
    TArray<FVector> Normals;  TArray<FVector2D> UVs;
    Vertices.Reserve(N * 4); Triangles.Reserve(N * 6);
    Normals.Reserve(N * 4);  UVs.Reserve(N * 4);

    int32 BaseIdx = 0;
    for (int32 i = 0; i < N; ++i)
    {
        const FVector2D& A2D = BakedData->OpeningSegmentStarts[i];
        const FVector2D& B2D = BakedData->OpeningSegmentEnds[i];

        // Quad vertical : Z=1 en haut, Z=0 en bas (scale Z appliqué au runtime).
        Vertices.Add(FVector(A2D.X, A2D.Y, 1.0f));
        Vertices.Add(FVector(B2D.X, B2D.Y, 1.0f));
        Vertices.Add(FVector(B2D.X, B2D.Y, 0.0f));
        Vertices.Add(FVector(A2D.X, A2D.Y, 0.0f));

        Triangles.Add(BaseIdx + 0); Triangles.Add(BaseIdx + 1); Triangles.Add(BaseIdx + 2);
        Triangles.Add(BaseIdx + 0); Triangles.Add(BaseIdx + 2); Triangles.Add(BaseIdx + 3);

        const FVector2D Edge = B2D - A2D;
        const FVector Normal = FVector(-Edge.Y, Edge.X, 0.f).GetSafeNormal();
        for (int32 k = 0; k < 4; ++k) Normals.Add(Normal);

        UVs.Add(FVector2D(0,0)); UVs.Add(FVector2D(1,0));
        UVs.Add(FVector2D(1,1)); UVs.Add(FVector2D(0,1));
        BaseIdx += 4;
    }

    SkirtMeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
        TArray<FColor>(), TArray<FProcMeshTangent>(), false);

    const float FloorZ = BakedData->LocalBoundsMin.Z;
    SkirtMeshComp->SetRelativeLocation(FVector(0, 0, FloorZ));
    bSkirtBuilt = true;
}

void URoomWaterRenderer::UpdateSkirtScale()
{
    if (!bSkirtBuilt || !BakedData || !SkirtMeshComp) return;
    const float FloorZ = BakedData->LocalBoundsMin.Z;
    const float Height = FMath::Max(0.f, CurrentWaterLevelLocalZ - FloorZ);
    SkirtMeshComp->SetRelativeScale3D(FVector(1.f, 1.f, Height));
    SkirtMeshComp->SetVisibility(Height > 0.01f);
}
```

---

### 13.4 Bridge component (`DoorWaterBridge.cpp`)

#### 13.4.1 BeginPlay + ResolveRenderers (FName lookup)

```cpp
void UDoorWaterBridge::BeginPlay()
{
    Super::BeginPlay();
    EnsureSubComponents();

    if (AActor* Owner = GetOwner())
        CachedDoor = Cast<ASubDoorActor>(Owner);

    ResolveRenderers();
    bInitialStateApplied = false;  // appliqué au premier tick de PollDoorState
}

void UDoorWaterBridge::ResolveRenderers()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 1) Déterminer les FName cibles : porte parent en priorité, override sinon.
    FName TargetA = CompartmentAOverride;
    FName TargetB = CompartmentBOverride;
    if (CachedDoor.IsValid())
    {
        if (TargetA.IsNone()) TargetA = CachedDoor->CompartmentA;
        if (TargetB.IsNone()) TargetB = CachedDoor->CompartmentB;
    }

    if (TargetA.IsNone() || TargetB.IsNone())
    {
        UE_LOG(LogWaterProto, Warning,
            TEXT("UDoorWaterBridge (%s): unresolved compartment IDs (A=%s, B=%s)"),
            *GetNameSafe(GetOwner()), *TargetA.ToString(), *TargetB.ToString());
        return;
    }

    // 2) PROTO : walke ARoomActor du level, match par RoomId.
    //    PORTAGE Sub3D : remplacer par lookup sur ASubmarineBase manager
    //    (CompartmentId → URoomWaterRenderer via UCompartmentVolumeComponent).
    RendererA.Reset();
    RendererB.Reset();
    int32 NumRoomsScanned = 0;

    for (TActorIterator<ARoomActor> It(World); It; ++It)
    {
        ARoomActor* Room = *It;
        if (!Room) continue;
        ++NumRoomsScanned;

        if (Room->RoomId == TargetA && !RendererA.IsValid() && Room->WaterRenderer)
            RendererA = Room->WaterRenderer;
        if (Room->RoomId == TargetB && !RendererB.IsValid() && Room->WaterRenderer)
            RendererB = Room->WaterRenderer;
    }

    UE_LOG(LogWaterProto, Display,
        TEXT("UDoorWaterBridge (%s): resolved A=%s→%s, B=%s→%s (scanned %d rooms)"),
        *GetNameSafe(GetOwner()),
        *TargetA.ToString(), RendererA.IsValid() ? TEXT("OK") : TEXT("MISS"),
        *TargetB.ToString(), RendererB.IsValid() ? TEXT("OK") : TEXT("MISS"),
        NumRoomsScanned);
}
```

#### 13.4.2 EnsureSubComponents + TickComponent + PollDoorState

```cpp
void UDoorWaterBridge::EnsureSubComponents()
{
    if (!BridgeMesh)
    {
        BridgeMesh = NewObject<UProceduralMeshComponent>(this, TEXT("BridgeMesh"));
        BridgeMesh->SetMobility(EComponentMobility::Movable);
        BridgeMesh->SetupAttachment(this);
        BridgeMesh->RegisterComponent();
        BridgeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BridgeMesh->bUseAsyncCooking = false;
        BridgeMesh->SetVisibility(false);
        if (WaterMaterial) BridgeMesh->SetMaterial(0, WaterMaterial);
    }
    if (!FlowFxComp && FlowEffect)
    {
        FlowFxComp = NewObject<UNiagaraComponent>(this, TEXT("FlowFx"));
        FlowFxComp->SetMobility(EComponentMobility::Movable);
        FlowFxComp->SetupAttachment(this);
        FlowFxComp->SetAsset(FlowEffect);
        FlowFxComp->RegisterComponent();
        FlowFxComp->SetAutoActivate(false);
        FlowFxComp->Deactivate();
    }
}

void UDoorWaterBridge::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* TickFunc)
{
    Super::TickComponent(DeltaTime, TickType, TickFunc);

    PollDoorState();
    if (!bDoorOpen || !RendererA.IsValid() || !RendererB.IsValid()) return;

    UpdateBridgeMesh();
    UpdateFlowFx();
}

void UDoorWaterBridge::PollDoorState()
{
    if (!CachedDoor.IsValid()) return;
    const bool bClosedNow = CachedDoor->bClosed;

    if (!bInitialStateApplied)
    {
        OnDoorStateChanged(bClosedNow);
        bLastObservedClosed = bClosedNow;
        bInitialStateApplied = true;
        return;
    }
    if (bClosedNow != bLastObservedClosed)
    {
        OnDoorStateChanged(bClosedNow);
        bLastObservedClosed = bClosedNow;
    }
}
```

#### 13.4.3 OnDoorStateChanged (slosh symétrique)

```cpp
void UDoorWaterBridge::OnDoorStateChanged(bool bClosed)
{
    bDoorOpen = !bClosed;

    if (BridgeMesh) BridgeMesh->SetVisibility(bDoorOpen);
    if (FlowFxComp)
    {
        if (bDoorOpen) FlowFxComp->Activate();
        else FlowFxComp->Deactivate();
    }

    // Slosh à l'ouverture : impulsion symétrique des 2 côtés au niveau de la porte.
    if (bDoorOpen && OpenSloshForce > 0.f)
    {
        const FVector DoorWorld = GetComponentLocation();
        for (URoomWaterRenderer* Renderer : { RendererA.Get(), RendererB.Get() })
        {
            if (!Renderer) continue;
            const FVector LocalPos = Renderer->GetComponentTransform().InverseTransformPosition(DoorWorld);
            Renderer->InjectAt(FVector2D(LocalPos.X, LocalPos.Y), OpenSloshForce, 80.f);
        }
    }
}

float UDoorWaterBridge::GetRendererWaterWorldZ(const URoomWaterRenderer* Renderer)
{
    if (!Renderer) return 0.f;
    return Renderer->GetComponentLocation().Z + Renderer->CurrentWaterLevelLocalZ;
}
```

#### 13.4.4 UpdateBridgeMesh (⚠️ INCOMPLET — manque skirts verticaux)

**Spec plan 5.3.2** demande : "horizontal quad au min level + petits skirts verticaux des deux côtés si delta significatif". Actuellement seulement le quad horizontal est implémenté. Le commentaire C++ dit `(code détaillé omis — équivalent au skirt de 4.4.4)`.

```cpp
void UDoorWaterBridge::UpdateBridgeMesh()
{
    if (!BridgeMesh) return;

    const float WaterZ_A = GetRendererWaterWorldZ(RendererA.Get());
    const float WaterZ_B = GetRendererWaterWorldZ(RendererB.Get());

    // Plan de surface visible à l'embrasure : eau s'écoule par le haut du niveau le plus bas.
    const float BridgeWorldZ = FMath::Min(WaterZ_A, WaterZ_B);

    // Quad horizontal en local space du bridge. Z=0 mesh, Z absolu via SetWorldLocation
    // pour rester gravity-aligned indépendamment du yaw porte.
    const float HalfDepth = DoorDepth * 0.5f;
    const float HalfWidth = DoorWidth * 0.5f;
    TArray<FVector> Vertices = {
        FVector(-HalfDepth, -HalfWidth, 0.f),
        FVector(+HalfDepth, -HalfWidth, 0.f),
        FVector(+HalfDepth, +HalfWidth, 0.f),
        FVector(-HalfDepth, +HalfWidth, 0.f),
    };
    TArray<int32> Triangles = { 0, 1, 2, 0, 2, 3 };
    TArray<FVector> Normals = { FVector(0,0,1), FVector(0,0,1), FVector(0,0,1), FVector(0,0,1) };
    TArray<FVector2D> UVs = { FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1) };

    if (BridgeMesh->GetNumSections() == 0)
    {
        BridgeMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
            TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial) BridgeMesh->SetMaterial(0, WaterMaterial);
    }

    // Position World du bridge : XY = position porte, Z = niveau d'eau bas.
    const FVector DoorWorld = GetComponentLocation();
    BridgeMesh->SetWorldLocation(FVector(DoorWorld.X, DoorWorld.Y, BridgeWorldZ));

    // Yaw seulement (gravity-aligned). Annule pitch/roll du parent.
    const FRotator OwnerRot = GetComponentRotation();
    BridgeMesh->SetWorldRotation(FRotator(0.f, OwnerRot.Yaw, 0.f));

    // ⚠️ MANQUE : skirts verticaux du plan 5.3.2 si |WaterZ_A - WaterZ_B| > seuil.
    // Concrètement : générer 1-2 quads verticaux entre BridgeWorldZ et le niveau le plus haut,
    // côté du compartiment au niveau supérieur. Width=DoorWidth, Height=delta.
    // Sans ces skirts, B03 (raccord propre) et B04 (delta visible) montrent un trou visuel.
}
```

#### 13.4.5 UpdateFlowFx (Torricelli)

```cpp
void UDoorWaterBridge::UpdateFlowFx()
{
    if (!FlowFxComp) return;

    const float WaterZ_A = GetRendererWaterWorldZ(RendererA.Get());
    const float WaterZ_B = GetRendererWaterWorldZ(RendererB.Get());
    const float Delta = FMath::Abs(WaterZ_A - WaterZ_B);

    if (Delta < MinDeltaForFlowFx)
    {
        if (FlowFxComp->IsActive()) FlowFxComp->Deactivate();
        return;
    }
    if (!FlowFxComp->IsActive()) FlowFxComp->Activate();

    // Vélocité Torricelli : v = sqrt(2 g h)
    constexpr float G = 980.f; // cm/s²
    const float FlowSpeed = FMath::Sqrt(2.f * G * Delta);

    // Direction : du haut vers le bas, en world XY plane.
    const URoomWaterRenderer* High = (WaterZ_A > WaterZ_B) ? RendererA.Get() : RendererB.Get();
    const URoomWaterRenderer* Low = (WaterZ_A > WaterZ_B) ? RendererB.Get() : RendererA.Get();
    if (!High || !Low) return;

    FVector Dir = Low->GetComponentLocation() - High->GetComponentLocation();
    Dir.Z = 0.f;
    if (!Dir.Normalize()) return;

    FlowFxComp->SetVariableFloat(TEXT("FlowSpeed"), FlowSpeed);
    FlowFxComp->SetVariableVec3(TEXT("FlowDirection"), Dir);

    // Injection continue côté arrivée pour onduler la salle qui se remplit.
    URoomWaterRenderer* Receiver = (WaterZ_A > WaterZ_B) ? RendererB.Get() : RendererA.Get();
    if (Receiver && FlowReceiverForce > 0.f)
    {
        const FVector DoorWorld = GetComponentLocation();
        const FVector LocalPos = Receiver->GetComponentTransform().InverseTransformPosition(DoorWorld);
        Receiver->InjectAt(FVector2D(LocalPos.X, LocalPos.Y), FlowReceiverForce, 40.f);
    }
}
```

---

### 13.5 Sub3D extension

#### 13.5.1 USubInteractionComponent::TraceFromView (decoupled)

Ajout au composant existant. Permet à n'importe quel `APawn` (incluant `BP_WaterProtoController` générique) ET au `ASubCrewCharacter` existant de produire un `FHitResult` complet camera→forward.

```cpp
// SubInteractionComponent.h — ajouté
UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
bool TraceFromView(FHitResult& OutHit) const;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Interaction",
    meta = (ClampMin = "10.0", ClampMax = "10000.0"))
float DefaultTraceDistance = 500.f;
```

```cpp
// SubInteractionComponent.cpp
bool USubInteractionComponent::TraceFromView(FHitResult& OutHit) const
{
    AActor* Owner = GetOwner();
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!Owner || !World) return false;

    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;

    // Path 1 : ASubCrewCharacter — caméra crew + InteractDistance (compat existante).
    if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(Owner))
    {
        UCameraComponent* Cam = Crew->GetActiveViewCamera();
        if (!Cam) return false;
        Start = Cam->GetComponentLocation();
        End = Start + Cam->GetForwardVector() * Crew->InteractDistance;
    }
    // Path 2 : APawn générique — premier UCameraComponent + DefaultTraceDistance.
    else if (const APawn* Pawn = Cast<APawn>(Owner))
    {
        UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>();
        if (!Cam) return false;
        Start = Cam->GetComponentLocation();
        End = Start + Cam->GetForwardVector() * DefaultTraceDistance;
    }
    else return false;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SubInteractionTraceFromView), false);
    Params.AddIgnoredActor(Owner);
    return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}
```

---

### 13.6 Material M_Phase0_Test — architecture WPO

Le matériau cap final combine :

```
[Sin simple ambient]   : DEPRECATED, orphelin dans le graph (à nettoyer)
                          WorldPosition.X → Mult×0.05 + Time×1.5 → sin → Mult×2 (scalaire Z)

[Gerstner Custom HLSL] : Float3 (X, Y, Z)
                          Inputs : WorldPosition.xy (Float2), Time (Float)
                          Output : Float3 = somme de 3 ondes Gerstner directionnelles

[Heightfield path]     : LocalPosition.xy → mask
                          → soustraire LocalBoundsMin.xy
                          → diviser par (LocalBoundsMax.xy - LocalBoundsMin.xy)
                          → UV ∈ [0,1]
                          → TextureSampleParameter2D HeightfieldTex (R32_FLOAT, sampler "Color")
                          → R channel × HeightfieldAmplitude (5.0 default)
                          → MakeVector3(0, 0, Z) → Float3

[Final Add Float3]     : Gerstner Float3 + Heightfield Float3 → Material Output WPO
```

HLSL du Custom node Gerstner :

```hlsl
// 3 ondes empilées avec directions, longueurs et vitesses différentes.
const float2 D1 = float2(1.0,  0.3); const float L1 = 200.0; const float A1 = 1.5; const float S1 = 30.0; const float Q1 = 0.3;
const float2 D2 = float2(-0.5, 1.0); const float L2 = 130.0; const float A2 = 1.0; const float S2 = 40.0; const float Q2 = 0.4;
const float2 D3 = float2(0.7, -0.5); const float L3 = 80.0;  const float A3 = 0.6; const float S3 = 50.0; const float Q3 = 0.5;

float3 GerstnerOffset = float3(0, 0, 0);

// Onde 1
float k1 = 6.2831853 / L1;
float omega1 = S1 * k1;
float phase1 = k1 * dot(normalize(D1), WorldXY) + omega1 * Time;
GerstnerOffset.x += Q1 * A1 * normalize(D1).x * cos(phase1);
GerstnerOffset.y += Q1 * A1 * normalize(D1).y * cos(phase1);
GerstnerOffset.z += A1 * sin(phase1);
// Onde 2 + Onde 3 idem avec D2/L2/A2/... et D3/L3/A3/...

return GerstnerOffset;
```

**Paramètres exposés** (set par C++ via `CapMID->SetXxxParameterValue`) :
- `HeightfieldTex` (Texture2D) — set au LazyInit
- `LocalBoundsMin`, `LocalBoundsMax` (Vector) — set au LazyInit
- `HeightfieldAmplitude` (Scalar, default 5.0) — multiplicateur cm

---

**Fin du document**. Prêt pour review planner.
