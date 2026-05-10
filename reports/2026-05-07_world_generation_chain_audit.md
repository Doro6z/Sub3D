# Audit chaine generation world / traversal

Date: 2026-05-07  
Mode: audit statique code + docs, sans PIE  
Fichier produit: `reports/2026-05-07_world_generation_chain_audit.md`

---

## Mise a jour 2026-05-09

Le shell de run C++ audite dans ce document a ete supprime apres decision de scope:

- `ASubGameState`
- `ESubRunPhase`
- `ASubRunPhaseVolume`
- `ASubBreachTriggerVolume`
- les tests automation associes aux phases de run

`ASubGameMode` reste present, mais uniquement pour le bootstrap prototype: resolution du sous-marin actif, spawn crew, embark crew, watchdog de bootstrap.

Les sections qui decrivent les phases de run, les volumes Departure/Approach, le breach trigger scriptable et le mirror `ASubGameState` sont maintenant historiques. Les points d'audit encore valides sont ceux qui concernent `ATraversalRouteActor`, la pipeline de route, `UTunnelNavigationRuntimeComponent`, `UHelmNavigationDisplayComponent`, la replication de recette de route et les risques de divergence client/serveur.

Refs d'assets binaires a nettoyer/resauver dans l'editeur apres suppression C++:

- `/Game/Sub3D/UI/WBP_SubHelm`
- `/Game/Sub3D/Blueprint/PlayerBP/PC_SubPlayerController`
- `/Game/Maps/Proto02_Traversal`
- `/Game/Maps/L_FP01_RunShell`

---

## 0. Preambule - comprehension actuelle

Ce que j'ai compris du fonctionnement:

- La "world generation" actuelle n'est pas un open world streamer complet. C'est surtout une chaine de generation de routes sous-marines traversables: un graphe de campagne choisit un segment, un `FRouteGenSpec` + `FRouteSeedCascade` determinent une route, `ATraversalRouteActor` construit topologie, squelette, volumes, deformation organique, semantics, validation, mesh et donnees de navigation.
- La route produite est le cadre monde exterieur dans lequel le sous-marin se deplace. Elle est separee de la chaine de mouvement: le mouvement ne lit pas directement le mesh de route, mais le mesh/collision de route bloque le sous-marin, et `UTunnelNavigationRuntimeComponent` lit un sidecar `UTunnelNavDataAsset` pour projeter le sous-marin sur la route et alimenter les aides helm/navigation.
- La chaine "traversal" a deux sens dans le projet. Pour cet audit, le sujet principal est le traversal monde du sous-marin dans les tunnels generes. Le traversal crew interne reste la LGA auditee ailleurs. Ici, il apparait seulement comme consommateur indirect du run flow, pas comme generation world.
- Le chemin First Playable documente ne veut pas un campaign runtime complet. Il veut une seule route baked, un start, un end, des volumes Departure/Approach, et un breach trigger. Le code de campagne existe, mais les docs disent explicitement que `CampaignWorldManager` ne doit pas etre critique pour le FP.
- La replication de route repose sur une idee propre mais fragile: le serveur replique une recette (`FRouteNetSpec`) et les clients reconstruisent localement le mesh/la nav data ou reutilisent un baked static mesh. Cela suppose que les clients disposent localement des memes assets/config actor que le serveur.

Etat potentiel:

- La pipeline route C4-C10 est assez avancee pour du tooling/editor et pour une route baked unique. Elle a des logs, un hash, une validation, du bake static mesh, du tunnel nav sidecar, et une couche runtime helm.
- La partie "campaign world" est un outil de construction/preview plus qu'un runtime de production. Elle peut auto-build en BeginPlay, mais elle n'a pas de state replication robuste, pas de streaming, pas de persistance de progression, et les fonctions "InEditor" sont parfois appelees en runtime.
- Le support multiplayer de la route est partiel: `ATraversalRouteActor` replique `RouteNetSpec`, mais pas les assets d'archetype/biome, pas les `CampaignExternalUnionBrushes`, pas les settings de surface, pas le `GeneratedTunnelNavData`. Pour une route placee/baked identique dans la map, ca peut marcher. Pour une campagne runtime dynamique, le risque de divergence client/serveur est reel.
- Le sonar field existe mais `SampleOcclusionAlongRay()` est un stub. La navigation tunnel est plus mature que le sonar world proprement dit.

Amelioration potentielle:

- Formaliser deux chemins distincts: `FP baked single route` et `post-FP dynamic campaign route`. Aujourd'hui ils partagent beaucoup de code, mais pas le meme niveau de garanties.
- Faire de la recette network un contrat complet, ou bien imposer officiellement le baked mesh cooke comme source client. Le contrat actuel replique trop peu pour garantir une reconstruction dynamique identique.
- Ajouter des tests automation purs pour C3-C10: determinisme hash, route validation pass/fail, tunnel nav sample count, endpoint transforms, client rebuild without editor-only data.
- Decoupler la nav runtime du besoin de rerun toute la pipeline quand seules les donnees sidecar manquent. Actuellement `EnsureRuntimeNavigationDataForCurrentSpec()` rappelle `RunPipeline`.
- Corriger ou encadrer les APIs de vitesse dans `TunnelNavigationRuntime`: elles utilisent `OwnerActor->GetVelocity()`, alors que le sous-marin a un mouvement math-based custom. A valider en PIE, car une vitesse nulle fausse drift, stopping distance et helm display.

---

## 1. Scope audite

Inclus:

- `Source/Sub3D/WorldGen/*`
- `Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.{h,cpp}`
- `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.{h,cpp}`
- `Source/Sub3D/Submarine/SubGameMode.{h,cpp}`
- `Source/Sub3D/Submarine/SubGameState.{h,cpp}`
- `Source/Sub3D/Submarine/SubRunPhase.h`
- `Source/Sub3D/Submarine/SubRunPhaseVolume.{h,cpp}`
- `Source/Sub3D/Submarine/SubBreachTriggerVolume.{h,cpp}`
- `Source/Sub3D/Submarine/TraversalLevelManager.{h,cpp}` comme residue legacy
- `Source/Sub3D/Submarine/SubmarineBase.{h,cpp}` uniquement pour l'attachement des composants TunnelNav/HelmNav
- `Config/DefaultEngine.ini`
- `Sub3D.uproject`
- docs archivees pertinentes sur First Playable route, TunnelNav et HelmNav

Non audite en runtime:

- Etat reel des assets `.uasset`.
- Acteurs effectivement places dans les maps.
- PIE, Standalone, client/serveur live.
- Qualite visuelle du mesh genere.
- Collision effective dans le moteur.

Consequence: tout ce qui depend d'une map placee ou d'un asset reference est marque "a valider en editor/PIE".

---

## 2. Resume executif

- Le coeur de generation world est `ATraversalRouteActor`: acteur sans tick, replicant, qui construit la route via `RunPipeline()` et expose mesh, endpoints, sonar field et tunnel nav data.
- La pipeline principale observee est C4-C10: topology -> skeleton -> guaranteed volume -> organic deformation -> semantics -> validation -> marching cubes mesh -> sonar field -> procedural mesh components.
- Le niveau "campaign" existe via `UCampaignGraphAsset` + `ACampaignWorldManager`, mais il est encore proche d'un outil editor/runtime proto. Il build le premier chemin du graphe et peut aligner plusieurs routes par leurs docks, puis ajouter des brushes de jonction.
- Le First Playable documente prefere une route baked unique. Cela correspond mieux a l'etat actuel que le dynamic campaign runtime.
- `UTunnelNavDataAsset` est une sidecar derivee pendant la pipeline. Elle contient nodes, edges, samples, clearances radiales et validation. Elle n'est pas repliquee; elle est regeneree localement.
- `UTunnelNavigationRuntimeComponent` est la couche query: projection du sous-marin sur la route, cross-section, lookahead, restrictions, graph window, obstacle runtime, drift heading/velocity.
- `UHelmNavigationDisplayComponent` ne genere rien: il transforme les queries TunnelNav en donnees UI/caches helm.
- `ASubGameMode` ne genere pas de monde. Depuis la coupe du 2026-05-09, il ne porte plus les phases de run: il sert au bootstrap prototype du sous-marin et du crew.
- Point critique MP: `RouteNetSpec` replique seulement spec + seeds + hash. Or `RunPipeline()` depend aussi de `ArchetypeAsset`, `BiomeAsset`, `SurfaceBuildSettings` et `CampaignExternalUnionBrushes`. Si ces donnees ne sont pas identiques localement sur le client, la reconstruction client peut diverger.
- Point critique gameplay: `USonarFieldComponent::SampleOcclusionAlongRay()` est explicitement stub, donc le sonar field world n'est pas encore un gameplay sonar fiable.

Niveau de maturite estime:

| Sous-systeme | Maturite statique | Commentaire |
|---|---:|---|
| Route baked single actor | Moyen/bon | Pipeline complete + bake + endpoints + validation. A valider sur asset/map. |
| Dynamic campaign multi-route | Fragile | Preview/path tooling present, mais pas de contrat runtime/MP complet. |
| Mesh generation C10 | Moyen | Marching cubes CPU + batching; backend PMC experimental/fragile mais utilise ailleurs. |
| Tunnel nav runtime | Moyen/bon | API riche, mais vitesse source a valider et cout de queries a surveiller. |
| Sonar field world | Fragile | Stocke occupancy, mais raycast sonar stub. |
| Multiplayer dynamic route | Fragile | Recipe repliquee incomplete pour rebuild deterministe complet. |

---

## 3. Diagramme global

```text
[Authoring assets]
  UCampaignGraphAsset
  URouteArchetypeDataAsset
  UBiomeFieldProfileDataAsset
  UBranchProfileSetDataAsset / UBranchProfileDataAsset
  URouteMotifDataAsset
        |
        v
[Campaign selection / compile]
  ACampaignWorldManager
    -> UCampaignRouteCompiler::BuildRouteSpec
    -> FRouteGenSpec + FRouteSeedCascade
        |
        v
[Route authority]
  ATraversalRouteActor::BuildRouteFromSpec
    -> RunPipeline
        C4 UTraversalTopologyGenerator
        C5 USkeletonResolver
        C6 UNavigableVolumeGenerator
        C7 UOrganicDeformationGenerator
        C8 URouteSemanticGenerator
        C9 URouteValidator
        C10 URouteMeshBuilder
        SonarField InitializeFromField
        Spawn PMCs or reuse baked static mesh
        Build UTunnelNavDataAsset sidecar
        |
        +--> RouteNetSpec replication
        +--> endpoint transforms
        +--> mesh collision/visuals
        +--> GeneratedTunnelNavData
        +--> USonarFieldComponent

[Runtime traversal/navigation]
  ASubmarineBase
    -> UTunnelNavigationRuntimeComponent
       -> Resolve first route actor
       -> Read GeneratedTunnelNavData
       -> Project submarine to route
       -> Cross-section / lookahead / graph / restrictions
    -> UHelmNavigationDisplayComponent
       -> UI-ready cached nav panels

[Run phase gameplay]
  ASubGameMode
    -> Resolve ActiveRoute
    -> Read RouteStart/RouteEnd transforms
    -> Departure/Traverse/BreachCrisis/Approach
  ASubRunPhaseVolume
  ASubBreachTriggerVolume
```

Tick/runtime shape:

```text
Generation:
  No per-frame route generation tick.
  ACampaignWorldManager: PrimaryActorTick disabled.
  ATraversalRouteActor: PrimaryActorTick disabled.
  Route builds happen on explicit calls or BeginPlay fallback.

Runtime query:
  UTunnelNavigationRuntimeComponent ticks.
    - cleanup runtime obstacles
    - retry route resolve
    - draw debug if enabled

  UHelmNavigationDisplayComponent ticks.
    - refresh cached view data every RefreshPeriodS, min 0.02s
```

---

## 4. Authoring data and source of truth

### 4.1 Route spec

Source: `Source/Sub3D/WorldGen/WorldGenTypes.h:164`.

`FRouteGenSpec` contient le seed de campagne, l'identite route, les checkpoints, longueur, profondeur, biome, difficulte, complexite, archetype, envelope de navigation et flags d'interface start/end.

Snippet:

```cpp
USTRUCT(BlueprintType)
struct FRouteGenSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  CampaignSeed       = 12345;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  RouteID            = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  StartCheckpointID  = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  EndCheckpointID    = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  RouteLengthMeters  = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  StartDepthMeters   = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float  EndDepthMeters     = 1600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  BiomeID            = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  DifficultyTier     = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETraversalComplexityTier ComplexityTier = ETraversalComplexityTier::Moderate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32  CrewSizeHint       = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ETraversalRouteArchetype Archetype = ETraversalRouteArchetype::MainTransit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FNavigationEnvelopeSpec  Envelope;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bForceStartInterfaceOnly = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bForceEndInterfaceOnly = false;
};
```

Observation:

- `FNavigationEnvelopeSpec::HardClearance` vaut 250 cm par defaut, alors que `MinTurnRadius` vaut 2200 cm (`WorldGenTypes.h:145-156`).
- La validation et le tunnel nav utilisent `Spec.Envelope.MinTurnRadius` comme clearance requise (`RouteValidator.cpp:257`, `TunnelNavDataBuilder.cpp:38`, `TunnelNavDataBuilder.cpp:223`).
- Cela peut etre intentionnel pour forcer un espace de demi-tour, mais le nom `MinTurnRadius` utilise comme "clearance" est une ambiguite forte.

### 4.2 Seed cascade

Source: `Source/Sub3D/WorldGen/CampaignRouteCompiler.cpp:13`.

`UCampaignRouteCompiler::DeriveSeedCascade()` derive les seeds par hash FNV-like:

```cpp
Seeds.RouteSeed      = HashInts(Spec.CampaignSeed, Spec.RouteID);
Seeds.TopologySeed   = HashInts(Seeds.RouteSeed, 1);
Seeds.VolumeSeed     = HashInts(Seeds.RouteSeed, 2);
Seeds.OrganicSeed    = HashInts(Seeds.RouteSeed, 3);
Seeds.SemanticSeed   = HashInts(Seeds.RouteSeed, 4);
Seeds.PopulationSeed = HashInts(Seeds.RouteSeed, 5);
```

Le compiler lui-meme est minimal:

- `BuildRouteSpec()` remplit seulement `CampaignSeed` et `RouteID`.
- Le caller doit override longueur, biome, difficulte, archetype, etc.

Reference: `CampaignRouteCompiler.cpp:25-31`.

### 4.3 Route archetype data asset

Source: `Source/Sub3D/WorldGen/RouteArchetypeDataAsset.h`.

`URouteArchetypeDataAsset` est le principal authoring asset pour la forme de route:

- `FRouteFlowAuthoringSettings`: spine nodes, split anchors, fanout, rejoin.
- `FRouteCheckpointAuthoringSettings`: shape/radius start/end.
- `FRouteConnectionAuthoringSettings`: dock throat length/radius, campaign overlap.
- `FRouteComplexityAuthoringSettings`: hubs, optional branches, pocket depth.
- `FRouteVolumeAuthoringSettings`: trunk/branch/merge/hub radii, junction transition.
- `FRouteSpatialShapeAuthoringSettings`: separation, vertical offset, curvature.
- `FRouteRhythmAuthoringSettings`: branch density, hub/pocket chance.

References:

- grouped authoring fields: `RouteArchetypeDataAsset.h:116-126`
- constrained graph legacy fields: `RouteArchetypeDataAsset.h:127-143`
- split/multi-stage fields: `RouteArchetypeDataAsset.h:144+`
- migration: `RouteArchetypeDataAsset.cpp:23-154`

Fragilite observee:

- Le data asset garde des champs legacy et des champs groupes.
- `MigrateGroupedAuthoringIfNeeded()` remplit des valeurs groupees depuis les anciennes (`RouteArchetypeDataAsset.cpp:45-154`).
- `TraversalTopologyGenerator` a encore des fallbacks `LegacyNarrow`, `LegacyWide`, `LegacyStandard` si aucun `BranchProfileSet` n'est charge (`TraversalTopologyGenerator.cpp:145-157`).

### 4.4 Biome data asset

Source: `Source/Sub3D/WorldGen/BiomeFieldProfileDataAsset.h`.

Le biome fournit:

- `BiomeID`
- noise frequencies / threshold / amplitude pour organic caves
- material principal
- `PreferredBranchProfileSet`
- tags / complexity defaults

Points importants:

- `UOrganicDeformationGenerator` ne modifie que le `RenderField`; le `SonarField` reste guaranteed-only (`OrganicDeformationGenerator.cpp:109-112`).
- Si `BiomeAsset` est null, C7 est skip sans erreur (`OrganicDeformationGenerator.cpp:73`).

### 4.5 Branch profiles et motifs

Sources:

- `BranchProfileDataAsset.h`
- `BranchProfileSetDataAsset.h`
- `RouteMotifDataAsset.h`

Les profiles de branche encodent:

- intent (`CanonicalBypass`, `OptionalResourceDetour`, `OptionalDangerBranch`, etc.)
- allowed canonical/optional/hub/checkpoint
- radius, length scale, curvature scale, verticality scale
- rejoin/pocket/secondary split chances
- placement window
- semantic tags

Etat:

- Les profiles sont lus synchronously (`LoadSynchronous`) dans `BuildResolvedBranchProfiles()` (`TraversalTopologyGenerator.cpp:106-144`).
- Si rien n'est present, le code fabrique trois profiles legacy (`TraversalTopologyGenerator.cpp:154-156`).
- Les motifs existent comme data assets, et `ComputeBuildHash()` les inclut si charges (`TraversalRouteActor.cpp:1305-1324`), mais l'audit statique n'a pas trouve d'integration forte des motifs dans la generation topologique courante. A valider si des assets les utilisent.

---

## 5. Campaign graph et selection de route

### 5.1 UCampaignGraphAsset

Source: `Source/Sub3D/WorldGen/CampaignGraphAsset.{h,cpp}`.

Fonctions:

- `FindSegmentByID`
- `FindSegmentIndexByID`
- `CountIncomingConnections`
- `IsValidGraph`

`IsValidGraph()` valide:

- segments non vides
- root non none
- terminal non none
- IDs uniques
- root/terminal presents
- next IDs existants

References:

- validation no segments/root/terminal: `CampaignGraphAsset.cpp:25-44`
- duplicate IDs: `CampaignGraphAsset.cpp:46-65`
- next ID missing: `CampaignGraphAsset.cpp:77-89`

Limites:

- Ne valide pas que le terminal est reachable depuis root.
- Ne valide pas les cycles sauf indirectement dans `BuildPreviewPathDescriptors()` qui stoppe sur visited.
- Ne valide pas une politique de branchement multiple pour gameplay.

### 5.2 ACampaignWorldManager

Source: `Source/Sub3D/WorldGen/CampaignWorldManager.{h,cpp}`.

Role observe:

- Acteur sans tick (`CampaignWorldManager.cpp:67`).
- Peut auto-build en BeginPlay si `bAutoBuildCurrentSegmentOnBeginPlay` est true (`CampaignWorldManager.cpp:72-83`).
- Resout ou spawn un `ATraversalRouteActor` target/preview/path (`CampaignWorldManager.cpp:203-323`).
- Compile une route par segment via `UCampaignRouteCompiler`.
- Assigne `ArchetypeAsset`, `BiomeAsset`, `DebugSpec`, `CampaignSegmentID`, `CampaignPathIndex` au route actor.
- Appelle `RouteActor->BuildRouteFromSpec()`.
- Peut aligner une route suivante sur le dock de la precedente.
- Peut build un connector mesh debug entre deux routes.
- Peut build tout le premier chemin root -> first next -> ... via `BuildCampaignPathPreviewInEditor()`.
- Peut ajouter des union brushes entre segments adjacents puis reconstruire les routes.

Snippet important:

```cpp
RouteActor->ArchetypeAsset = Descriptor.Archetype;
RouteActor->BiomeAsset = Descriptor.Biome;
RouteActor->DebugSpec = Spec;
RouteActor->CampaignSegmentID = Descriptor.SegmentID;
RouteActor->CampaignPathIndex = SegmentIndex;
RouteActor->ClearCampaignExternalUnionBrushes();

const bool bBuilt = RouteActor->BuildRouteFromSpec(Spec, Seeds);
```

Reference: `CampaignWorldManager.cpp:409-416`.

### 5.3 Build segment flow

```text
BuildSegmentByID(SegmentID)
  ValidateCampaignGraph
  Find descriptor + index
  ResolveTargetRouteActor
  BuildDescriptorToActor
    Hash SegmentID + SeedOffset + SegmentIndex
    UCampaignRouteCompiler::BuildRouteSpec
    ConfigureSpecForCampaignSegment
      RouteLengthMeters = PreferredLengthMeters
      ComplexityTier = descriptor tier
      bForceStartInterfaceOnly if incoming exists
      bForceEndInterfaceOnly if outgoing exists
    Assign archetype/biome/debug/campaign metadata
    ClearCampaignExternalUnionBrushes
    BuildRouteFromSpec
    Update ProgressState
```

References:

- configure spec: `CampaignWorldManager.cpp:337-350`
- build descriptor: `CampaignWorldManager.cpp:373-447`
- build by id: `CampaignWorldManager.cpp:455-472`

### 5.4 Preview / connector / multi-segment

`PreviewSelectedNextSegmentAndConnector()`:

- builds preview route for selected next segment
- aligns preview start dock to current end dock
- records `PreviewConnectorStart` / `PreviewConnectorEnd`
- optionally builds `ARouteConnectorActor`

References:

- preview build: `CampaignWorldManager.cpp:491-599`
- align route: `CampaignWorldManager.cpp:353-369`
- connector build call: `CampaignWorldManager.cpp:573`
- connector actor: `RouteConnectorActor.cpp:17-57`
- connector generator Bezier rings: `RouteConnectorGenerator.cpp:25-113`

`BuildCampaignPathPreviewInEditor()`:

- suit uniquement `NextSegmentIDs[0]`.
- build chaque route.
- aligne chaque route sur la precedente.
- detruit les extras inutiles.
- applique `ApplyCrossSegmentUnionToBuiltPath()`.

References:

- first-next path: `CampaignWorldManager.cpp:603-626`
- path preview: `CampaignWorldManager.cpp:629-706`
- cross-segment union: `CampaignWorldManager.cpp:722-831`

Fragilites:

- Les methodes avec suffixe `InEditor` sont parfois `BlueprintCallable`, pas toutes `CallInEditor`, et `BeginPlay()` peut appeler `BuildRootSegmentInEditor()` si auto-build true.
- `ACampaignWorldManager` ne replique pas son `ProgressState`.
- `CampaignExternalUnionBrushes` sont appliquees au `ATraversalRouteActor`, incluses dans `ComputeBuildHash()`, mais pas repliquees comme donnees de recette.
- `BuildPreviewPathDescriptors()` ignore toutes les branches sauf le premier `NextSegmentIDs[0]`.

---

## 6. Route authority: ATraversalRouteActor

### 6.1 Role

Source: `Source/Sub3D/WorldGen/TraversalRouteActor.h:20-24`.

Commentaire du code:

```cpp
// C11 — Runtime container for a generated route.
// Server: builds, validates, holds collision + semantic data.
// Clients: receive FRouteNetSpec via replication and rebuild mesh locally.
```

Responsabilites reelles observees:

- Owns replicated `FRouteNetSpec`.
- Owns designer references: archetype, biome, debug spec.
- Owns surface/bake settings.
- Owns endpoint transforms and dock transforms.
- Owns `GeneratedTunnelNavData`.
- Owns `USonarFieldComponent`.
- Builds PMCs or resolves baked static mesh.
- Writes route generation logs.
- Bakes generated PMCs to static mesh asset in editor.

### 6.2 Constructor / replication

References:

- `PrimaryActorTick.bCanEverTick = false`: `TraversalRouteActor.cpp:457`
- `bReplicates = true`: `TraversalRouteActor.cpp:459`
- `DOREPLIFETIME(ATraversalRouteActor, RouteNetSpec)`: `TraversalRouteActor.cpp:512`

Seule propriete repliquee explicitement:

```cpp
UPROPERTY(ReplicatedUsing=OnRep_RouteNetSpec, BlueprintReadOnly)
FRouteNetSpec RouteNetSpec;
```

Reference: `TraversalRouteActor.h:36-37`.

### 6.3 BeginPlay behavior

`BeginPlay()`:

1. refresh endpoint markers
2. active/desactive baked static mesh runtime
3. tente resolve baked asset from hash si option active
4. reconstruit liste de PMCs managed
5. si PMCs baked presentes: reuse visuals, ensure runtime nav data
6. sinon si baked static mesh present: reuse static mesh, ensure runtime nav data
7. sinon si authority + spec valide: `BuildRouteFromSpec`

Reference: `TraversalRouteActor.cpp:515-548`.

Important:

- Meme quand les visuels sont reutilises, `EnsureRuntimeNavigationDataForCurrentSpec(true)` peut rerun la pipeline sans respawn mesh pour regenerer la nav data.
- Le route actor ne tick pas; il build a BeginPlay ou par appel explicite.

### 6.4 BuildRouteFromSpec

Flow:

```text
BuildRouteFromSpec
  if !HasAuthority -> false
  hide incompatible baked mesh
  ClearMeshComponents
  reset validation / triangle count
  bOk = RunPipeline
  RouteNetSpec.GenSpec = Spec
  RouteNetSpec.Seeds = Seeds
  RouteNetSpec.BuildHash = ComputeBuildHash
  RouteNetSpec.BuildVersion = 1
  fill LastSavedRecipe
  log / write route generation log if ok
```

Reference: `TraversalRouteActor.cpp:1346-1402`.

Point important:

- `RouteNetSpec` est mis a jour apres `RunPipeline()`, meme si `RunPipeline()` echoue. Le retour `bOk` signale l'echec, mais l'etat spec/hash peut quand meme changer.
- En cas d'echec validation, le log ne sort que la premiere fail reason.

### 6.5 RunPipeline complet

Reference: `TraversalRouteActor.cpp:1405-1608`.

Ordre exact:

```text
RunPipeline(Spec, Seeds, bSpawnVisualMesh)
  GeneratedTunnelNavData = nullptr
  if bBuildTunnelNavData:
    GeneratedTunnelNavData = NewObject<UTunnelNavDataAsset>(..., RF_Transient)
    TunnelNavBuilder = NewObject<UTunnelNavDataBuilder>

  C4 Topology:
    UTraversalTopologyGenerator::GenerateTopology

  C5 Skeleton:
    USkeletonResolver::ResolveSkeleton
    UpdateRouteEndpointTransforms
    save SelectedBranchProfileIDs
    TunnelNavBuilder->InitializeFromC5

  C6 Volume:
    UNavigableVolumeGenerator::BuildGuaranteedVolume
    TunnelNavBuilder->PopulateFromC6

  C7 Organic:
    UOrganicDeformationGenerator::ApplyDeformation

  C8 Semantics:
    URouteSemanticGenerator::BuildSemantics

  C9 Validation:
    URouteValidator::Validate
    TunnelNavBuilder->StampFromC9
    if !bPass return false

  C10 MeshBuild:
    URouteMeshBuilder::BuildMeshChunks

  SonarField:
    SonarField->InitializeFromField(FieldModel)

  if bSpawnVisualMesh:
    SpawnMeshComponents(Chunks)
  else:
    reuse existing visuals
```

Snippet cle:

```cpp
GeneratedTunnelNavData = nullptr;
const int32 RouteBuildHash = static_cast<int32>(ComputeBuildHash(Spec, Seeds));
UTunnelNavDataBuilder* TunnelNavBuilder = nullptr;
if (bBuildTunnelNavData)
{
	GeneratedTunnelNavData = NewObject<UTunnelNavDataAsset>(this, TEXT("GeneratedTunnelNavData"), RF_Transient);
	TunnelNavBuilder = NewObject<UTunnelNavDataBuilder>(this);
}
```

Reference: `TraversalRouteActor.cpp:1407-1414`.

Implications:

- `GeneratedTunnelNavData` est toujours transient.
- En client runtime, elle est regeneree, pas repliquee.
- Toute route baked qui veut la nav runtime doit avoir un `RouteNetSpec` suffisant pour rerun la pipeline.

---

## 7. C4 Topology

Source: `Source/Sub3D/WorldGen/TraversalTopologyGenerator.{h,cpp}`.

### 7.1 GenerateTopology order

Reference: `TraversalTopologyGenerator.cpp:1194-1244`.

Ordre:

1. `BuildConstrainedGraphTopology`
2. `BuildMultiStageSplitTopology`
3. `BuildSplitMergeTopology`
4. fallback linear main path + branch nodes

Snippet:

```cpp
if (BuildConstrainedGraphTopology(Spec, Seeds, Archetype, OutNodes))
{
	return true;
}

if (BuildMultiStageSplitTopology(Spec, Seeds, Archetype, OutNodes))
{
	return true;
}

if (BuildSplitMergeTopology(Spec, Seeds, Archetype, OutNodes))
{
	return true;
}
```

### 7.2 Positionnement des nodes

Reference: `TraversalTopologyGenerator.cpp:287-306`.

La route avance en X:

- `X = T * RouteLengthCm`
- `Z` interpole `StartDepthMeters` vers `EndDepthMeters` en negatif.
- variation verticale random scalee par `VerticalityBias`.
- `Y` random dans une enveloppe dependant de `RouteLengthMeters`.

Snippet:

```cpp
const float RouteLengthCm = Spec.RouteLengthMeters * 100.f;
const float X = T * RouteLengthCm;

const float StartZ = -Spec.StartDepthMeters * 100.f;
const float EndZ = -Spec.EndDepthMeters * 100.f;
const float BaseZ = FMath::Lerp(StartZ, EndZ, T);
...
return FVector(X, Y, Z);
```

### 7.3 Constrained graph

Reference: `TraversalTopologyGenerator.cpp:420+`.

Le constrained graph consomme:

- grouped authoring si `bUseGroupedConstrainedAuthoring`
- complexity tier via `ResolveComplexityTier`
- `ResolveComplexityBudget`
- branch profiles
- hub/split/optional budgets
- checkpoint shapes / interface-only flags

Logs importants:

- `[C4Settings]`
- `[C4Budget]`

Fragilite:

- Fallback legacy si `MaxOptionalBranches == 0` mais branch density > 0 (`TraversalTopologyGenerator.cpp:485-488`).
- Le code a beaucoup de paths historiques: constrained graph, split/merge, multi-stage, fallback linear.

---

## 8. C5 Skeleton

Source: `Source/Sub3D/WorldGen/SkeletonResolver.{h,cpp}`.

Role:

- Convertit les nodes + `NextNodeIDs` en segments Bezier.
- Chaque edge topologique donne un `FTraversalSkeletonSegment`.
- Propage branch stage/index/profile/intent/logical role.
- Calcule rayons start/end et transitions aux jonctions.

References:

- Bezier eval: `SkeletonResolver.cpp:34-52`
- radius eval: `SkeletonResolver.cpp:54-75`
- build segment: `SkeletonResolver.cpp:77-160`
- resolve skeleton: `SkeletonResolver.cpp:162-184`

Invariants observes:

- Sans curvature/jitter: P1/P2 sont a 35% du chord.
- Avec curvature/jitter: support point aleatoire deterministe par node IDs, avec role scale.
- `bGuaranteedPath = A.bIsCanonicalPath && B.bIsCanonicalPath`.

Fragilite:

- Le random de curvature utilise `HashCombine(NodeID A, NodeID B)`, pas le seed route. Cela reste deterministe pour une topologie donnee, mais pas directement derive de `Seeds`.
- Les roles/nodes historiques influencent fortement les transitions de rayon; une erreur d'authoring logique devient une erreur de volume.

---

## 9. C6 Volume / fields

Source: `Source/Sub3D/WorldGen/NavigableVolumeGenerator.{h,cpp}`.

### 9.1 Representation

Types:

- `FVolumeBrushDef`: capsule/sphere/ellipsoid + radius/smoothness + flags render/sonar/guaranteed/decorative.
- `FRouteFieldModel`:
  - `GuaranteedBrushes`
  - `RenderOnlyBrushes`
  - `OrganicBrushes`
  - `RenderField`: 200 cm voxels
  - `SonarField`: 500 cm voxels

References:

- brush type: `WorldGenTypes.h:305-322`
- field model: `WorldGenTypes.h:359-369`

### 9.2 SDF

References:

- `SmoothMin`: `NavigableVolumeGenerator.cpp:113-119`
- `CapsuleSDF`: `NavigableVolumeGenerator.cpp:121-130`
- `SphereSDF`: `NavigableVolumeGenerator.cpp:132-135`
- density convention: `NavigableVolumeGenerator.cpp:137-164`

Convention:

```text
density >= 0 -> water / navigable
density < 0  -> rock
```

### 9.3 Brush generation

`GenerateBrushes()`:

- Transforme chaque Bezier segment en 8 capsule brushes.
- Les guaranteed segments vont dans `GuaranteedBrushes`.
- Les optional/decorative peuvent aller dans `RenderOnlyBrushes`.
- Ajoute des sphere pockets aux jonctions split/merge/hub.
- Ajoute checkpoint spaces et dock throat brushes.

References:

- segment capsules: `NavigableVolumeGenerator.cpp:180-230`
- junction sphere: `NavigableVolumeGenerator.cpp:233-258`
- checkpoint/dock brushes: `NavigableVolumeGenerator.cpp:240-270`
- optional hub/pocket render-only: `NavigableVolumeGenerator.cpp:273-321`

### 9.4 RasterizeField

Reference: `NavigableVolumeGenerator.cpp:332-421`.

Algorithme:

- Collecte seulement les chunks touches par les brushes.
- Chaque chunk stocke `(S+1)^3` samples pour fixer les seams.
- Culling local de brushes par chunk AABB.
- Evalue density pour chaque sample.
- Marque `bContainsGuaranteedPath` si au moins un sample water.

Parametres hardcodes:

- Render field: voxel 200 cm, `SamplesPerChunkAxis = 16`.
- Sonar field: voxel 500 cm, `SamplesPerChunkAxis = 8`.

Reference: `NavigableVolumeGenerator.cpp:453-456`.

### 9.5 BuildGuaranteedVolume

Reference: `NavigableVolumeGenerator.cpp:424-459`.

Flow:

```text
GenerateBrushes
Append ExternalGuaranteedBrushes if present
Compute bounds from skeleton + brushes
Rasterize render field from guaranteed + render-only brushes
Rasterize sonar field from guaranteed brushes only
```

Fragilites:

- `ExternalGuaranteedBrushes` sont une API utile pour multi-segment, mais pas repliquee.
- Les valeurs voxel/S sont hardcodees; pas exposees dans `FRouteSurfaceBuildSettings`.
- Le nom `BuildGuaranteedVolume` masque le fait qu'il construit aussi render-only field.

---

## 10. C7 Organic deformation

Source: `Source/Sub3D/WorldGen/OrganicDeformationGenerator.cpp`.

Role:

- Applique une deformation noise sur le render field.
- Ne ferme jamais le guaranteed path: `NewD = Max(DBase, NoiseCave)`.
- Ne modifie pas le sonar field.

References:

- value noise: `OrganicDeformationGenerator.cpp:8-55`
- skip si no biome: `OrganicDeformationGenerator.cpp:73`
- active coords autour guaranteed chunks: `OrganicDeformationGenerator.cpp:109-124`
- invariant enlarge-only: `OrganicDeformationGenerator.cpp:150-153`

Snippet:

```cpp
// INVARIANT: only enlarge, never close guaranteed path
const float NewD = FMath::Max(DBase, NoiseCave);
Chunk.DensitySamples[Idx] = NewD;
```

Fragilites:

- Deformation organique depend de `BiomeAsset`. Si client dynamic rebuild sans `BiomeAsset`, il genere une route moins organique/differente.
- `LargeScaleWarpAmplitude` et `MediumNoiseAmplitude` sont dans `BiomeFieldProfileDataAsset`, inclus dans hash, mais non observes comme utilises dans C7 actuel.

---

## 11. C8 Semantics

Source: `Source/Sub3D/WorldGen/RouteSemanticGenerator.{h,cpp}`.

Role:

- Cree des `FRouteSemanticZone` a partir des node types.
- Cree des `FMissionSocketDef` pour `WreckPocket` et `ResourcePocket`.

References:

- zones: `RouteSemanticGenerator.cpp:3-113`
- mission sockets: `RouteSemanticGenerator.cpp:116-149`
- build: `RouteSemanticGenerator.cpp:151-159`

Observations:

- Les decorative disconnected nodes sont ignores dans les zones.
- `StartCheckpointSpace` cree `StartCheckpointDock`.
- `EndCheckpointSpace` cree `EndCheckpointDock`.
- `ExitAnchor` cree `ExitGate`.
- Wreck/resource pockets generent des gameplay tags via `FGameplayTag::RequestGameplayTag`.

Fragilites:

- `FRouteSemanticModel` est local a `RunPipeline()` et n'est pas stocke sur le route actor apres validation.
- Les semantics ne semblent pas exposees aux runtime systems actuels, hors validation/log potentiel.

---

## 12. C9 Validation

Source: `Source/Sub3D/WorldGen/RouteValidator.{h,cpp}`.

Role:

- Mesure clearance min.
- Valide connectivity skeleton et sonar field.
- Valide branches split/merge, multi-stage, constrained graph.
- Produit `FRouteValidationReport`.

References:

- `Validate`: `RouteValidator.cpp:250-598`
- report struct: `WorldGenTypes.h:509-533`
- sonar connectivity check: `RouteValidator.cpp:162`

Point critique:

```cpp
const float RequiredClearance = Spec.Envelope.MinTurnRadius;
```

Reference: `RouteValidator.cpp:257`.

Cela signifie que la validation compare `MinObservedClearance` a `MinTurnRadius`, pas a `HardClearance`.

Validation branches:

- constrained graph: canonical start -> canonical exit + sonar connectivity (`RouteValidator.cpp:405-420`)
- multi-stage: start/exit count, split/merge count, start->exit path, sonar (`RouteValidator.cpp:423-462`)
- linear/no split: derive start/exit from sequential node IDs + sonar (`RouteValidator.cpp:465-507`)
- split/merge: one start, one split, one merge, one exit, each branch >= 2 segments + clearance + path split->merge (`RouteValidator.cpp:512-598`)

Fragilites:

- Validation depend de categories topologiques heuristiques.
- `Semantic` est passe a `Validate()` mais pas observe comme utilise dans la section lue.
- Les fail reasons sont strings; pas d'enum machine-readable pour tooling.

---

## 13. C10 Mesh generation / collision / bake

Source: `Source/Sub3D/WorldGen/RouteMeshBuilder.{h,cpp}` + `TraversalRouteActor.cpp`.

### 13.1 Mesh builder

Role:

- CPU Marching Cubes depuis `RenderField` vers `FRouteMeshChunkData[]`.
- Filtre les chunks uniformes.
- Process les chunks avec brushes locales.
- BFS flood fill depuis guaranteed chunks pour garder les shells connectes.
- Marque `MeshChunk.bServerCollision = Pair.Value.bContainsGuaranteedPath`.

References:

- header comment: `RouteMeshBuilder.h:10`
- `BuildMeshChunks`: `RouteMeshBuilder.cpp:426-672`
- BFS shell connectivity: `RouteMeshBuilder.cpp:578-627`
- output add: `RouteMeshBuilder.cpp:633-654`

Hard facts:

- `OutChunks.Reset()` a chaque build.
- `AllRenderBrushes = GuaranteedBrushes + RenderOnlyBrushes`.
- `LocalBrushes.Reserve(32)` par chunk.
- Pas de `ParallelFor` observe.

### 13.2 Spawn PMCs

`ATraversalRouteActor::SpawnMeshComponents()` batch les chunks selon `SurfaceBuildSettings.RuntimeMeshSectionBatchSize`, defaut 24.

References:

- `FRouteSurfaceBuildSettings`: `WorldGenTypes.h:446-460`
- `SpawnMeshComponentSection`: `TraversalRouteActor.cpp:1655-1744`
- `SpawnMeshComponents`: `TraversalRouteActor.cpp:1746-1765`

Collision generated PMC:

```cpp
PMC->CreateMeshSection_LinearColor(
	0,
	Vertices,
	Triangles,
	Normals,
	UVs,
	Colors,
	Tangents,
	HasAuthority() && bSectionCollision && ResolveCollisionModeForStrategy(SurfaceBuildSettings, false) != ERouteMeshCollisionMode::None);
```

Reference: `TraversalRouteActor.cpp:1719-1727`.

Observation:

- Collision cooking des generated PMCs est explicitement authority-only.
- `PMC->SetCollisionEnabled(...)` est ensuite appele quel que soit authority (`TraversalRouteActor.cpp:1729`), mais sans collision mesh cooke cote client.
- Route collision authoritative est coherent avec mouvement serveur-authoritative, mais a valider pour client-side camera/trace/visual interactions.

### 13.3 Collision strategy

`ERouteBoundsCollisionStrategy::CanonicalProxyOnly` renvoie `SimpleAndComplex` avec commentaire proto fallback:

```cpp
case ERouteBoundsCollisionStrategy::CanonicalProxyOnly:
	return ERouteMeshCollisionMode::SimpleAndComplex; // proto fallback until dedicated proxy exists
```

Reference: `TraversalRouteActor.cpp:150-151`.

Dette claire:

- Le mode "canonical proxy only" n'a pas encore son proxy dedie.

### 13.4 Baked static mesh

`ATraversalRouteActor` peut:

- resoudre un baked asset par hash (`TryResolveBakedAssetForHash`)
- appliquer un `UStaticMeshComponent`
- bake les PMCs en static mesh asset sous `/Game/GeneratedRoutes` par defaut

References:

- baked settings: `TraversalRouteActor.h:62-83`
- resolve baked asset: `TraversalRouteActor.cpp:772-806`
- baked component activation: `TraversalRouteActor.cpp:587-617`
- bake current route: `TraversalRouteActor.cpp:1818+`

First Playable:

- Les docs FP preferent une route baked unique.
- Ce chemin est plus credible que dynamic campaign runtime en MP, parce que les assets et actor properties existent localement dans la map/cook.

---

## 14. Tunnel nav sidecar

Source: `Source/Sub3D/WorldGen/TunnelNavDataAsset.{h,cpp}` + `TunnelNavDataBuilder.{h,cpp}`.

### 14.1 Data asset

`UTunnelNavDataAsset` stocke:

- schema version
- build hash
- spec + seeds
- build settings
- endpoint snapshot
- nodes
- edges
- samples
- required clearance
- validation report

References:

- build settings: `TunnelNavDataAsset.h:9-27`
- endpoint snapshot: `TunnelNavDataAsset.h:30-49`
- node record: `TunnelNavDataAsset.h:52-73`
- edge record: `TunnelNavDataAsset.h:76-105`
- sample record: `TunnelNavDataAsset.h:108-143`
- asset fields: `TunnelNavDataAsset.h:146-186`

Default build settings:

| Setting | Default |
|---|---:|
| SampleSpacingCm | 1000 |
| RadialSampleCount | 16 |
| CrossSectionProbeMaxCm | 30000 |
| BinarySearchIterations | 10 |
| CoarseProbeSteps | 24 |

### 14.2 Builder C5/C6/C9

`InitializeFromC5()`:

- reset data
- copy hash/spec/seeds/build settings/endpoints
- set required clearance to `Spec.Envelope.MinTurnRadius`
- copy topology nodes to nav records
- create edge records from skeleton
- sample each Bezier segment by length / sample spacing
- create frame forward/right/up
- init radial clearances array

References: `TunnelNavDataBuilder.cpp:24-151`.

`PopulateFromC6()`:

- for each sample, ray/probe radial directions against guaranteed brushes
- fill min/max/right/up/left/down clearances
- fill edge min clearance

References: `TunnelNavDataBuilder.cpp:153-221`.

`StampFromC9()`:

- stamps required clearance, pass flag and validation report
- flags samples/edges below required clearance
- sets branch validation pass per branch index

References: `TunnelNavDataBuilder.cpp:223-266`.

Cost model:

```text
samples ~= sum(segment_length / SampleSpacingCm)
per sample clearance probes = RadialSampleCount
per radial probe = coarse steps + binary iterations, each with brush evals
```

At defaults: 16 radial probes * (24 coarse + 10 binary worst-ish) * brush evals per sample. One-time build, but expensive if rerun frequently.

---

## 15. Sonar field

Source: `Source/Sub3D/WorldGen/SonarFieldComponent.{h,cpp}`.

State:

- `USonarFieldComponent` caches `Field.SonarField`.
- `CollectCoarseWaterSurfacePoints()` can collect boundary water points around a center.
- `GetSonarVoxelCount()` counts occupancy samples.
- `SampleOcclusionAlongRay()` is a stub.

Snippet:

```cpp
bool USonarFieldComponent::SampleOcclusionAlongRay(const FVector& Start, const FVector& End, float& OutBlockage) const
{
	// Proto 04 STUB. Implement in Proto 05 using CachedSonarField.
	OutBlockage = 0.f;
	return false;
}
```

Reference: `SonarFieldComponent.cpp:12-18`.

Implication:

- Le field sonar est utilisable comme donnees grossieres, mais pas comme occlusion gameplay.
- Les docs FP notaient deja ce point: `SampleOcclusionAlongRay()` always returns false (`reports/plans/_archive/2026-03-29_sub3d_first_playable_run_spec.md:118`).

---

## 16. Runtime traversal: UTunnelNavigationRuntimeComponent

Source: `Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.{h,cpp}`.

### 16.1 Role

La composante est attachee au sous-marin dans `ASubmarineBase`:

```cpp
TunnelNavigationRuntime = CreateDefaultSubobject<UTunnelNavigationRuntimeComponent>(TEXT("TunnelNavigationRuntime"));
HelmNavigationDisplay = CreateDefaultSubobject<UHelmNavigationDisplayComponent>(TEXT("HelmNavigationDisplay"));
```

Reference: `SubmarineBase.cpp:221-222`.

Elle expose:

- projection submarine/world to route
- local cross-section
- forward anticipation
- graph window
- active restrictions
- stopping distance warning
- commitment warning
- heading vs velocity state
- runtime observed obstacles
- debug logs/draw

References:

- public API: `TunnelNavigationRuntimeComponent.h:654-727`
- tick: `TunnelNavigationRuntimeComponent.cpp:102-121`

### 16.2 Tick

```cpp
void UTunnelNavigationRuntimeComponent::TickComponent(...)
{
	Super::TickComponent(...);

	CleanupExpiredRuntimeObstacles();

	if (bAutoResolveRouteActor && (!IsValid(CachedRouteActor) || (bAutoBindTunnelNavAssetFromRoute && !IsValid(CachedTunnelNavData))))
	{
		TimeSinceLastResolveAttemptS += DeltaTime;
		if (TimeSinceLastResolveAttemptS >= FMath::Max(0.05f, RouteResolveRetryPeriodS))
		{
			TimeSinceLastResolveAttemptS = 0.f;
			ResolveRouteActorFromWorld();
		}
	}

	if (GetDefault<USub3DDebugSettings>()->bDrawTunnelNavigation)
	{
		DrawDebugOverlay();
	}
}
```

Reference: `TunnelNavigationRuntimeComponent.cpp:102-121`.

### 16.3 Route resolution

`ResolveRouteActorFromWorld()`:

- itere `TActorIterator<ATraversalRouteActor>`
- retient le premier route actor comme fallback
- prefere le premier qui a `GeneratedTunnelNavData` valide avec samples
- bind `CachedTunnelNavData` depuis `CandidateRoute->GetTunnelNavData()`
- log si `bLogTunnelNavigation`

Reference: `TunnelNavigationRuntimeComponent.cpp:932-1001`.

Fragilite:

- Si plusieurs route actors existent, le runtime prend le premier avec samples, pas necessairement le segment actif ni le plus proche du sous-marin.
- Le `ACampaignWorldManager` peut avoir target, preview et path actors. Sans binding explicite, la navigation du sous-marin peut se lier au mauvais actor.

### 16.4 Projection

`ProjectWorldLocationToRouteInternal()`:

- ensure nav data
- find best sample index
- calc offset right/up depuis sample frame
- remplit `FTunnelNavProjectionResult`
- update projection cache

Reference: `TunnelNavigationRuntimeComponent.cpp:177-222`.

Cost:

- `FindProjectedSampleIndex()` utilise cache/neighbor puis fallback full scan.
- A valider sur routes longues: full scan peut devenir cher si la cache saute.

### 16.5 Cross-section

`GetLocalCrossSection()`:

- lit sample clearances
- soustrait offset local du sous-marin
- compare a `NavProfile.HardClearanceCm` et `PreferredClearanceCm`
- signale wall warning, hard violation, branch validation, no-turn, runtime obstacle

Reference: `TunnelNavigationRuntimeComponent.cpp:240-281`.

### 16.6 Forward anticipation

`GetForwardAnticipationProfile()`:

- calcule `EffectiveLookaheadCm`
- boucle sur tous les samples
- filtre distance [current, current+lookahead]
- filtre optional selon `bIncludeOptionalBranchesInLookahead`
- sort par distance
- subsample avec `AnticipationSamplingStepCm`
- calcule curvature/grade/restrictions/no-turn/stop distance/recommended speed

References:

- start: `TunnelNavigationRuntimeComponent.cpp:299`
- full sample loop: `TunnelNavigationRuntimeComponent.cpp:321-359`
- sort/subsample: `TunnelNavigationRuntimeComponent.cpp:361-405`
- output speed/stop: `TunnelNavigationRuntimeComponent.cpp:406-423`

Performance:

- Cette fonction alloue `TArray<FTunnelNavAnticipationPoint> GatheredPoints` et boucle tous les samples a chaque appel.
- `UHelmNavigationDisplayComponent` peut l'appeler a 50 Hz si `RefreshPeriodS` est bas.

### 16.7 Runtime obstacles

APIs:

- `AddRuntimeObservedObstacle`
- `ClearRuntimeObservedObstacles`
- `GetRuntimeObservedObstacles`

Reference: `TunnelNavigationRuntimeComponent.cpp:840-862`.

Utilite:

- Point d'extension clair pour obstacles dynamiques: debris, creature, breach cloud, mines.
- Non replique dans la composante observee. Si un obstacle doit etre gameplay-authoritative, il faudra un owner server/replication explicite.

### 16.8 Heading / velocity issue

Plusieurs fonctions utilisent `OwnerActor->GetVelocity()`:

- `GetHeadingVsVelocityState`: `TunnelNavigationRuntimeComponent.cpp:804`, `1084`
- `ComputeForwardSpeedCmS`: `TunnelNavigationRuntimeComponent.cpp:1447`
- `ComputeLateralSpeedCmS`: `TunnelNavigationRuntimeComponent.cpp:1459`
- `ComputeVerticalSpeedCmS`: `TunnelNavigationRuntimeComponent.cpp:1471`
- Helm display: `HelmNavigationDisplayComponent.cpp:118`

Risque:

- Le sous-marin utilise un movement math-based custom, pas un `UMovementComponent` standard.
- Si `AActor::GetVelocity()` ne renvoie pas la velocity custom reelle, tout le drift/stopping/recommended speed est faux.
- A valider en PIE avec logs TunnelNav + motion logs.

---

## 17. Helm navigation display

Source: `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.{h,cpp}`.

Role:

- Resolve `UTunnelNavigationRuntimeComponent`.
- Refresh data a intervalle.
- Project submarine.
- Build cross-section view.
- Build forward anticipation view.
- Build stopping/commitment warning.
- Build tactical graph view.
- Build reconstruction view.
- Cache `FHelm*ViewData` pour widget/UI.

References:

- tick refresh: `HelmNavigationDisplayComponent.cpp:38-49`
- `RefreshViewData`: `HelmNavigationDisplayComponent.cpp:62-210`
- projection failure log: `HelmNavigationDisplayComponent.cpp:90-108`
- suspect projection warning: `HelmNavigationDisplayComponent.cpp:124-145`
- forward profile: `HelmNavigationDisplayComponent.cpp:154-160`
- graph view: `HelmNavigationDisplayComponent.cpp:164-185`

Fragilites:

- Meme velocity issue que TunnelNav.
- Si `CachedTunnelRuntime->ProjectSubmarineToRoute()` echoue, tout le display devient invalid.
- Le component est une couche de presentation/query, pas une source de truth.

---

## 18. Run phase traversal integration

### 18.1 ASubGameMode

Source: `Source/Sub3D/Submarine/SubGameMode.{h,cpp}`.

Role observe:

- Autorite run phase.
- Resout `ActiveSubmarine`.
- Resout `ActiveRoute` comme premier `ATraversalRouteActor` en world.
- Lit campaign seam data depuis route actor.
- Gere Boot/Boarding/Departure/Traverse/BreachCrisis/Approach/Docking/Success/Failure.
- Consomme volumes de depart/approche et breach trigger.

References:

- `ResolveActiveRoute`: `SubGameMode.cpp:514-532`
- `RefreshCampaignSeamData`: `SubGameMode.cpp:570-583`
- bootstrap route optional: `SubGameMode.cpp:888-900`
- run transitions: `SubGameMode.cpp:644-680`

Point important:

```cpp
// Route is required only on production maps that opt in. Test/proto maps
// can boot the gameplay loop without one.
if (bRequireActiveRoute && !ActiveRoute)
{
	return false;
}
```

Reference: `SubGameMode.cpp:890-898`.

Implication:

- Par defaut, `bRequireActiveRoute = false` (`SubGameMode.h:70`).
- Des maps proto peuvent tourner sans route.
- Une map production doit explicitement opt-in pour ne pas masquer l'absence de route.

### 18.2 Run phase enum / replication

`ESubRunPhase`:

- Boot
- Boarding
- Departure
- Traverse
- BreachCrisis
- Approach
- Docking
- Success
- Failure

Reference: `SubRunPhase.h`.

`ASubGameState` replique:

- `CurrentPhase`
- `bBreachActive`
- `BreachedCompartmentId`
- `bDockingAligned`

References:

- properties: `SubGameState.h:17-29`
- `DOREPLIFETIME`: `SubGameState.cpp:11-18`

### 18.3 Departure / approach volumes

`ASubRunPhaseVolume`:

- box trigger
- authority-only
- accepts `ASubmarineBase` only
- `DepartureGate` -> `NotifySubmarineClearedDepartureGate`
- `ApproachZone` begin/end -> `NotifyApproachZoneStateChanged`

References:

- constructor: `SubRunPhaseVolume.cpp:9-18`
- begin overlap: `SubRunPhaseVolume.cpp:28-69`
- end overlap: `SubRunPhaseVolume.cpp:78-109`

### 18.4 Breach trigger volume

`ASubBreachTriggerVolume`:

- box trigger 500 cm
- authority-only
- accepts `ASubmarineBase`
- calls `ASubGameMode::TriggerBreachEvent()`
- consumes itself if configured

References:

- constructor: `SubBreachTriggerVolume.cpp:9-17`
- overlap: `SubBreachTriggerVolume.cpp:27-67`

### 18.5 World generation relationship

`ASubGameMode` does not build route generation.

It assumes:

- active route exists or is optional
- route endpoints exist if needed
- departure/approach volumes are placed in map
- breach trigger is placed in map

So the world chain currently has two separate tracks:

```text
Generation track:
  ATraversalRouteActor / ACampaignWorldManager build route.

Gameplay traversal track:
  ASubGameMode + volumes consume a placed/built route.
```

There is no code observed that automatically places `ASubRunPhaseVolume` or `ASubBreachTriggerVolume` from generated route data.

---

## 19. Legacy residue: TraversalLevelManager

Source: `Source/Sub3D/Submarine/TraversalLevelManager.{h,cpp}`.

Header says:

```cpp
/**
 * Manages the traversal session in Proto 02.
 */
```

Reference: `TraversalLevelManager.h:11`.

Behavior:

- Actor without tick.
- Has an `EndTrigger`.
- On BeginPlay, after 0.2s, finds first `ASubmarineBase` and teleports it to `SpawnTransformA`.
- On overlap, prints traversal duration on screen.

Reference: `TraversalLevelManager.cpp`.

Risk:

- If this actor is still placed in any current map, it can fight `ASubGameMode` bootstrap/spawn and route start logic by teleporting the submarine.
- It is not referenced as current FP authority. Treat as legacy/proto residue unless a map intentionally uses it.

---

## 20. Network / multiplayer audit

### 20.1 What replicates

`ATraversalRouteActor`:

- `bReplicates = true`.
- Replicates only `RouteNetSpec`.

`FRouteNetSpec`:

```cpp
USTRUCT(BlueprintType)
struct FRouteNetSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FRouteGenSpec     GenSpec;
	UPROPERTY(BlueprintReadOnly) FRouteSeedCascade Seeds;
	UPROPERTY(BlueprintReadOnly) int32             BuildVersion = 1;
	UPROPERTY(BlueprintReadOnly) int32             BuildHash    = 0;
};
```

Reference: `WorldGenTypes.h:468-476`.

`OnRep_RouteNetSpec()`:

```cpp
if (TryResolveBakedAssetForHash(RouteNetSpec.BuildHash))
{
	EnsureRuntimeNavigationDataForCurrentSpec(true);
	return;
}

RebuildManagedMeshComponentList();
if (MeshComponents.Num() > 0)
{
	EnsureRuntimeNavigationDataForCurrentSpec(true);
	return;
}

ClearMeshComponents();
RunPipeline(RouteNetSpec.GenSpec, RouteNetSpec.Seeds, true);
```

Reference: `TraversalRouteActor.cpp:1787-1811`.

### 20.2 What does not replicate

Not explicitly replicated in `ATraversalRouteActor`:

- `ArchetypeAsset`
- `BiomeAsset`
- `SurfaceBuildSettings`
- `TunnelNavBuildSettings`
- `TunnelDebugOptions`
- `bBuildTunnelNavData`
- `GeneratedTunnelNavData`
- endpoint transforms
- `CampaignSegmentID`
- `CampaignPathIndex`
- `CampaignExternalUnionBrushes`
- baked static mesh asset pointer

Some of these can exist identically on clients if the actor is placed in the map and cooked with same properties. They are not guaranteed by the replicated runtime recipe.

### 20.3 Determinism gap

`ComputeBuildHash()` includes more than `RouteNetSpec`:

- spec + seeds
- surface build settings
- campaign external union brushes
- archetype asset path and many archetype fields
- branch profile set hash
- motifs
- biome asset path and fields
- material path/tags

References:

- surface settings hash: `TraversalRouteActor.cpp:1231-1241`
- campaign brushes: `TraversalRouteActor.cpp:1243-1258`
- archetype asset: `TraversalRouteActor.cpp:1260-1327`
- biome asset: `TraversalRouteActor.cpp:1329-1341`

But `FRouteNetSpec` contains none of these extra asset/settings payloads except `BuildHash`.

Critical implication:

- A client can receive hash H and spec/seeds, then run `RunPipeline()` with null/default `ArchetypeAsset` or `BiomeAsset` and generate a route that does not match the server.
- The log will still say it rebuilt route hash H, because `OnRep` logs `RouteNetSpec.BuildHash`, not a recomputed local hash.

This is acceptable only if:

1. The route actor is preplaced with identical archetype/biome/surface settings on every client, or
2. Clients always resolve a baked static mesh asset by hash, and nav data rebuild uses equivalent local actor properties, or
3. The network recipe is extended later.

### 20.4 Campaign dynamic MP blocker

`ACampaignWorldManager::BuildDescriptorToActor()` assigns route actor data at runtime:

```cpp
RouteActor->ArchetypeAsset = Descriptor.Archetype;
RouteActor->BiomeAsset = Descriptor.Biome;
RouteActor->DebugSpec = Spec;
RouteActor->CampaignSegmentID = Descriptor.SegmentID;
RouteActor->CampaignPathIndex = SegmentIndex;
```

Reference: `CampaignWorldManager.cpp:409-413`.

Those assignments are not replicated by `ATraversalRouteActor`.

So dynamic runtime campaign generation is not currently a complete multiplayer contract.

### 20.5 GameMode / GameState MP

`ASubGameMode` is server-only authority; `ASubGameState` replicates phase and breach state.

This part is more coherent than campaign route generation:

- phase replicated via GameState
- route mesh/recipe replicated via RouteActor
- volumes authority-only

But route selection itself is weak:

- `ASubGameMode::ResolveActiveRoute()` picks first `ATraversalRouteActor`.
- `UTunnelNavigationRuntimeComponent::ResolveRouteActorFromWorld()` also picks first route with samples.
- No explicit replicated "active route actor" pointer observed.

---

## 21. Project settings / modules / hidden dependencies

### 21.1 Collision channels

`DefaultEngine.ini` defines:

- `ECC_GameTraceChannel1` = `Submarine`
- `ECC_GameTraceChannel2` = `SubInterior`
- `ECC_GameTraceChannel3` = `CompartmentProbe`

References: `Config/DefaultEngine.ini:249-255`.

`TraversalRouteActor` explicitly sets:

```cpp
PrimitiveComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
PrimitiveComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
```

Reference: `TraversalRouteActor.cpp:76-85`.

Fragilite:

- Si ces channels changent, route collision vs submarine/interior behavior casse silencieusement.
- `ECC_GameTraceChannel1/2` sont hard references C++, pas lookup par name.

### 21.2 Fixed frame rate

`DefaultEngine.ini`:

- `bUseFixedFrameRate=True`
- `FixedFrameRate=60.000000`

References: `Config/DefaultEngine.ini:245-246`.

Ce setting appartient surtout a la motion chain, mais il touche indirectement le traversal world: le sous-marin doit traverser la route avec la meme cadence serveur/client que la motion chain.

### 21.3 Modules

`Source/Sub3D/Sub3D.Build.cs` inclut:

- `NetCore`
- `ProceduralMeshComponent`
- `GameplayTags`
- `Niagara`
- `PCG`
- `PhysicsCore`
- `DeveloperSettings`
- `Sub3DRuntime`
- `Sub3DCore`

Reference: `Sub3D.Build.cs:14-29`.

WorldGen dependances visibles:

- `ProceduralMeshComponent` pour route mesh et connector.
- `GameplayTags` pour semantics/sockets.
- `NetCore` indirectement pour replication.

### 21.4 Plugins

`Sub3D.uproject` active notamment:

- `PCG`
- `RuntimeSyncDiagnostics`
- `Niagara`
- `UnrealClaude`
- `Sub3DDebugPanel`
- `EditorScriptingUtilities`
- `GeometryProcessing`

Reference: `Sub3D.uproject`.

Le monde route actuel n'utilise pas directement PCG dans les fichiers audites, mais le module `Sub3D` depend de `PCG`.

### 21.5 Debug settings

`USub3DDebugSettings` expose:

- `bLogHelmNavigationDisplay`
- `bDrawTunnelNavigation`
- `bLogTunnelNavigation`

Reference: `Source/Sub3D/Debug/Sub3DDebugSettings.h:128-143`.

WorldGen route a aussi ses propres `FRouteTunnelDebugOptions` par route actor:

- `bEnableTunnelDebug`
- chunk window
- logs zero verts/high rejects/kept shell chunks

Reference: `WorldGenTypes.h:373-386`.

---

## 22. Performance statique

### 22.1 Build-time one-shot costs

| Stage | Cost statique |
|---|---|
| C4 Topology | O(nodes + branch attempts), low. |
| C5 Skeleton | O(edges), low. |
| C6 Rasterize | O(active chunks * samples per chunk * local brushes). Render chunk = 17^3 samples; sonar chunk = 9^3 samples. |
| C7 Organic | O(active render chunks near guaranteed * 17^3 samples). |
| C8 Semantics | O(nodes). |
| C9 Validation | Clearance + graph connectivity + sonar connectivity. Details sonar check can be nontrivial. |
| C10 Mesh | O(render chunks * 16^3 cubes + local brushes + smoothing/normals). |
| TunnelNavDataBuilder | O(samples * radial samples * probe steps * brushes). |
| Spawn PMCs | alloc components + copy merged arrays per batch. |

### 22.2 Runtime tick costs

`UTunnelNavigationRuntimeComponent::TickComponent`:

- `CleanupExpiredRuntimeObstacles`: `RemoveAll` over obstacle array.
- retry route resolve every `RouteResolveRetryPeriodS` if needed.
- debug draw only if enabled.

`UHelmNavigationDisplayComponent::TickComponent`:

- refresh every `RefreshPeriodS`, min 0.02.
- `RefreshViewData()` can call projection, cross-section, forward profile, graph window.

### 22.3 Per-refresh allocations

Observed likely allocations in query path:

- `GetForwardAnticipationProfile()` creates/reserves `GatheredPoints`, fills `OutProfile.Points`.
- `GetLocalGraphWindow()` builds arrays in `OutWindow`.
- `GetActiveRestrictionsForSubClass()` fills output array.
- Helm display builds cached view structs/arrays.

These are probably fine for debug/helm if sample count is modest, but need profiling if route samples grow.

### 22.4 Red flags

- No `ParallelFor` in mesh build; CPU build is synchronous.
- `RunPipeline()` uses multiple `NewObject` allocations and large transient arrays/maps. Do not run during gameplay hitch-sensitive moments unless intentionally loading.
- Client `OnRep_RouteNetSpec()` can rebuild the entire route mesh locally if no baked asset/PMC exists.
- `EnsureRuntimeNavigationDataForCurrentSpec()` reruns whole `RunPipeline()` to get nav data, even if visuals are reused.

---

## 23. Dette / fragilites connues

### 23.1 Severe / architecture

1. Dynamic route replication recipe incomplete.
   - Evidence: only `RouteNetSpec` replicated (`TraversalRouteActor.cpp:512`).
   - But hash/pipeline depend on archetype/biome/surface/union brushes (`TraversalRouteActor.cpp:1231-1341`).

2. Active route selection by "first actor".
   - GameMode: `SubGameMode.cpp:514-532`.
   - TunnelNav runtime: `TunnelNavigationRuntimeComponent.cpp:932-1001`.
   - Risk with target/preview/path routes.

3. Campaign runtime state not replicated.
   - `ACampaignWorldManager` has no `bReplicates` and `ProgressState` is visible instance only.
   - OK for editor tool; fragile for runtime campaign.

4. `CampaignExternalUnionBrushes` not in network recipe.
   - Set at `TraversalRouteActor.cpp:640-647`.
   - Used in volume build at `TraversalRouteActor.cpp:1503`.
   - Included in hash at `TraversalRouteActor.cpp:1243-1258`.

5. Sonar occlusion stub.
   - `SonarFieldComponent.cpp:12-18`.

### 23.2 Medium / gameplay integration

1. No observed automatic placement of departure/approach/breach volumes from route endpoints.
   - Volumes are map-authored actors.

2. `bRequireActiveRoute=false` by default.
   - Good for proto maps, risky for production if not set.

3. `TraversalLevelManager` legacy can teleport sub if still placed.

4. TunnelNav velocity source possibly wrong for math-based submarine.

5. `FRouteSemanticModel` not retained/exposed after validation.

6. `RouteConnectorActor` is debug/preview quality: builds a tube mesh, no replication contract observed, no nav sidecar integration.

### 23.3 Medium / code hygiene

1. Legacy migration fields in `URouteArchetypeDataAsset`.
2. Legacy branch profiles fallback.
3. `CanonicalProxyOnly` collision strategy is a proto fallback.
4. Several docs reference old debug fields (`bEnableDebugDraw`, `bEnableDebugLogs`) while current code uses `USub3DDebugSettings` toggles and `FRouteTunnelDebugOptions`.
5. `SampleOcclusionAlongRay()` TODO explicitly deferred to Proto 05.

### 23.4 Potential bugs / a valider

1. On client rebuild, local `ArchetypeAsset`/`BiomeAsset` may be null for runtime spawned route actors.
2. Client `OnRep_RouteNetSpec()` does not check return of `RunPipeline()`.
3. `BuildRouteFromSpec()` writes `RouteNetSpec` even if `RunPipeline()` failed.
4. `ResolveRouteActorFromWorld()` can bind preview route instead of active route.
5. `RequiredClearance = MinTurnRadius` naming/semantics can confuse future tuning.

---

## 24. Points d'extension

### 24.1 Production single baked route

Existing:

- baked static mesh support
- hash-based asset resolution
- endpoint transforms/dock transforms
- tunnel nav sidecar rebuild
- route generation logs

Missing / useful:

- deterministic validation command/test for one baked route asset.
- explicit active route binding in `ASubGameMode` and `UTunnelNavigationRuntimeComponent`.
- map validation utility: exactly one active route, departure volume near start, approach near end, breach trigger on canonical path.

### 24.2 Dynamic campaign runtime

Existing:

- graph segments
- segment compile
- target/preview/path route actors
- align docks
- cross-segment union brushes
- connector debug mesh

Missing:

- replicated campaign state.
- replicated complete route recipe or baked asset manifest.
- route actor lifecycle/streaming policy.
- active segment selection and handoff.
- nav data across connectors/seams.
- branch choice UX/gameplay.
- persistence/save.

### 24.3 Runtime obstacles / hazards

Existing:

- `UTunnelNavigationRuntimeComponent::AddRuntimeObservedObstacle`.
- restrictions can report runtime obstacles.

Missing:

- authoritative obstacle source.
- replication of obstacles if they affect gameplay.
- tie-in from sonar/damage/AI/debris systems.

### 24.4 Sonar

Existing:

- route actor owns `USonarFieldComponent`.
- sonar occupancy field is generated.
- coarse surface point query exists.

Missing:

- `SampleOcclusionAlongRay()`.
- integration with `USubSonarSystemComponent` not observed in this audit.
- clear contract: route field as sonar truth vs physics trace vs hybrid.

### 24.5 Missions / semantic sockets

Existing:

- `FMissionSocketDef` generated for wreck/resource pockets.
- `FRouteSemanticZone` generated by node type.

Missing:

- storage/exposure of `FRouteSemanticModel` on route actor.
- runtime APIs to query semantic zones/sockets.
- replication or save of mission socket state.

---

## 25. Validation / test bench

### 25.1 Existing automation tests

`Source/Sub3DTests/` exists and contains many automation tests, but grep did not find worldgen/traversal route tests.

Observed test domains:

- hull bake/profile/envelope
- floor mesh/bake
- ring sequence
- runtime actor flood/breach/door components
- submarine base rebuild

No route generation tests found for:

- `ATraversalRouteActor`
- `UTraversalTopologyGenerator`
- `URouteMeshBuilder`
- `UTunnelNavDataBuilder`
- `UTunnelNavigationRuntimeComponent`
- `ACampaignWorldManager`

References:

- file list: `Source/Sub3DTests/Private/Automation/*`
- grep result no worldgen route tests.

### 25.2 Runtime debug knobs

Available:

- `USub3DDebugSettings.bDrawTunnelNavigation`
- `USub3DDebugSettings.bLogTunnelNavigation`
- `USub3DDebugSettings.bLogHelmNavigationDisplay`
- per-route `FRouteTunnelDebugOptions`
- `UTunnelNavigationRuntimeComponent::LogCurrentProjection`
- `UTunnelNavigationRuntimeComponent::LogCurrentRestrictions`
- `ATraversalRouteActor::LogRouteEndpointDebugSummary`
- route generation logs written to `Saved/RouteGenLogs/Routes`
- campaign logs written to `Saved/RouteGenLogs/Campaign`

### 25.3 Static validation checklist suggested for current state

For a route actor intended for FP:

1. Exactly one active `ATraversalRouteActor` in map, or explicit `ActiveRoute` assigned.
2. `bBuildTunnelNavData = true`.
3. `RouteNetSpec.GenSpec.RouteLengthMeters > 0`.
4. `GeneratedTunnelNavData` exists after BeginPlay/rebuild.
5. `GeneratedTunnelNavData.Samples.Num() > 0`.
6. `LastValidationReport.bPass = true`.
7. Baked static mesh asset exists/cooked if using `bUseBakedStaticMeshAtRuntime`.
8. `bRequireActiveRoute = true` on production GameMode subclass.
9. No `ATraversalLevelManager` legacy actor in production map.
10. Departure/Approach/Breach volumes placed and authority overlap tested.

### 25.4 What I did not validate

- Did not run UE editor.
- Did not inspect placed map actors.
- Did not verify cooked baked mesh assets.
- Did not run network PIE.
- Did not profile route build time.
- Did not run automation tests because this audit only changed a Markdown report.

---

## 26. Docs / memory alignment

### CLAUDE.md

Relevant current status:

- `Generation pipeline (PAUSED for First Playable)` refers to submarine generator, not this outer route generation.
- It says production submarine FP is handmade Craniata.
- It says target multiplayer is 4-16 coop host-listen.

Status vs code:

- True for submarine generator context.
- Outer route generation code remains active/present.
- Need avoid conflating submarine generator pause with route generation availability.

### First Playable route docs

Docs:

- `reports/plans/_archive/2026-03-29_sub3d_first_playable_run_spec.md`
- `reports/plans/_archive/2026-03-29_sub3d_first_playable_level_architecture.md`

Alignment:

- Docs say FP should use one baked traversal route and not depend on full campaign runtime. Code supports a single `ATraversalRouteActor` better than dynamic campaign runtime.
- Docs mention `CampaignWorldManager` exists but should not gate FP. Code confirms it is not required by GameMode.
- Docs mention `USonarFieldComponent::SampleOcclusionAlongRay()` stub. Code confirms stub.

### TunnelNav Phase 2 handoff

Doc:

- `reports/plans/_archive/2026-03-30_sub3d_tunnel_nav_phase2_editor_validation_handoff.md`

Alignment:

- Route authority = `ATraversalRouteActor`: yes.
- Sidecar = `UTunnelNavDataAsset`: yes.
- Query layer = `UTunnelNavigationRuntimeComponent`: yes.
- Component attached on `ASubmarineBase`: yes.

Drift:

- Doc references old-style component debug fields like `bEnableDebugDraw` / `bEnableDebugLogs`; current code uses `USub3DDebugSettings.bDrawTunnelNavigation`, `bLogTunnelNavigation`, and per-route `FRouteTunnelDebugOptions`.

### HelmNav durable guide

Doc:

- `reports/plans/_archive/2026-03-31_sub3d_helmnav_durable_implementation_guide.md`

Alignment:

- `UHelmNavigationDisplayComponent` as adapter/cache: yes.
- Widget reads prepared data: likely yes, not deeply audited here.
- TunnelNav remains query source: yes.

---

## 27. Main conclusions

The current world/traversal generation chain is best described as:

```text
Production-usable for one baked traversal route after editor validation.
Architecturally promising but incomplete for dynamic multi-segment campaign runtime.
Partially network-aware, but not yet a complete multiplayer deterministic generation contract.
```

Most important risks:

1. Dynamic route replication is incomplete because clients rebuild from a recipe that omits assets/settings used by the generator.
2. Runtime systems auto-resolve "first route actor", which is unsafe once target/preview/path actors coexist.
3. Sonar field is not yet gameplay sonar because ray occlusion is a stub.
4. CampaignWorldManager is an editor/proto builder, not yet a replicated runtime world manager.
5. TunnelNav's velocity-dependent warnings may be wrong if `OwnerActor->GetVelocity()` is not wired to custom sub movement.

Most useful consolidation target:

- For FP: freeze on one baked `ATraversalRouteActor`, explicit active route binding, map validation, and no dependency on `ACampaignWorldManager`.
- For post-FP: either replicate/cook a complete route recipe manifest, or make baked route assets the network contract and treat generation as offline/editor-only.
