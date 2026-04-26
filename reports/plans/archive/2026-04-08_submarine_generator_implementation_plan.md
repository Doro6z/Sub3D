# Sub3D — Submarine Generator Implementation Plan

Date: 2026-04-08  
Input: `2026-04-08_submarine_generator_architecture_unified.md`  
Status: Actionable implementation plan, file-by-file.

---

## 0. Clarifications demandées

### USubmarineEnvelopeDef — Adapter ou extraire ?

**Decision : utiliser directement, sans adapter.**

`USubmarineEnvelopeDef` ([SubmarineEnvelopeDef.h](Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h)) est un `UDataAsset` avec des fonctions pures de profil (`EvaluateRadius`, `EvaluateSectionPoint`, `EvaluateBowSternTaper`). Il dépend de `SubCompilerTypes.h` uniquement pour `EBowSternProfile`. Cette dépendance est un enum de 5 valeurs, pas un pipeline.

Plan :
1. Extraire `EBowSternProfile` de `SubCompilerTypes.h` vers un header standalone `Source/Sub3D/Submarine/Generator/SubmarineEnvelopeEnums.h` (ou le laisser dans `SubCompilerTypes.h` pour l'instant — le générateur inclura ce seul enum, pas le reste).
2. Le nouveau `SubmarineGeneratorSpec` pointe vers `USubmarineEnvelopeDef*` directement. Pas de wrapper, pas d'adapter.
3. Plus tard (post-FP), si nécessaire, on peut déplacer `USubmarineEnvelopeDef` dans `Generator/` et casser la dépendance SubCompiler. Pour le FP, ce n'est pas bloquant.

**Risque** : Le `#include "SubCompilerTypes.h"` dans `SubmarineEnvelopeDef.h` tire tout le header. Acceptable car le générateur n'utilise aucun type de layout solver ; il crée ses propres types.

### Bow/Stern — Compartiments ou zones structurelles ?

**Decision : zones structurelles uniquement.**

Bow et Stern sont des segments de coque fermée (caps) sans espace intérieur marchable. Le générateur les traite comme géométrie de coque, pas comme compartiments. Ils n'apparaissent pas dans `USubmarineDefinition::Compartments` ni dans le flood graph. Ils servent uniquement à :
- Fermer le mesh extérieur (bow cap, stern cap).
- Définir les limites du premier et du dernier compartiment marchable.

### Sas (Airlock) — Implémentation FP

**Decision : compartiment explicite minimal.**

Le sas est un `FGeneratedCompartmentDef` avec `CompartmentSemanticType = Airlock`. Il a exactement deux `FGeneratedConnectionDef` :
1. Inner door → connexion vers un compartiment intérieur (type `Door`, `bStartsClosed = true`).
2. Outer hatch → connexion extérieure (type `ExteriorHatch`, `bStartsClosed = true`, `bExteriorEdge = true`).

Pas de boolean "is airlock" sur un compartiment générique. Pas de remeshing runtime. Le sas est une excroissance géométrique simple (boîte soudée au hull) avec sa propre géométrie.

### Level FP — Création propre

Un nouveau level `L_FP01_Generated` sans héritage des maps proto existantes. Les maps legacy (`Proto03_Sub_HullPrecision`, `L_FP01_RunShell`, etc.) restent intactes.

---

## 1. Architecture cible consolidée

```
USubmarineGeneratorSpec (UDataAsset, editor-authored)
    │
    │  USubmarineEnvelopeDef* (hull profile, reused as-is)
    │  Bulkhead positions, passage types
    │  Airlock position/side
    │  Requested stations
    │
    ▼
USubmarineGenerator (UObject, stateless)
    │
    │  Step 1: Hull envelope → closed exterior mesh + collision + volume
    │  Step 2: Interior space → walkable floor + walls
    │  Step 3: Bulkheads → mesh with door cutouts
    │  Step 4: Compartments → derived from bulkhead segments
    │  Step 5: Airlock → explicit compartment + 2 connections
    │  Step 6: Flood graph → FCompiledFloodGraph (Sub3DCore type)
    │  Step 7: Stations & spawns → placed transforms in compartments
    │
    ▼
USubmarineMeshBuilder (UObject, stateless)
    │
    │  Reads Definition compartments + EnvelopeDef profile
    │  Populates Definition.ExteriorHullMesh
    │  Populates Definition.InteriorMeshes[]
    │  Populates Definition.BulkheadMeshes[]
    │
    ▼
USubmarineDefinition (UDataAsset, runtime single source of truth)
    │
    │  Hull metrics, compartments, connections, flood graph,
    │  station slots, spawn points, mesh data arrays
    │
    ▼
USubmarineGeneratedGeometryComponent (UActorComponent)
    │  Converts mesh data arrays → UProceduralMeshComponent instances
    │  Render + collision separation
    │
    ▼
Runtime Components (read from USubmarineDefinition)
    ├── USubFloodComponent (new) — flood simulation
    ├── USubHullComponent (existing) — damage only
    ├── USubmarineCompartmentComponent (existing) — façade, reads SubFlood
    ├── FloodWaterVisualsComponent — reads SubFlood
    ├── DoorFloodVfxComponent — reads SubFlood
    ├── SubMovementComponent — reads SubFlood for mass
    └── SubCrewCharacter — reads SubFlood for immersion
```

---

## 2. Plan fichier par fichier

### Fichiers à CRÉER

| # | Fichier | Module | Description |
|---|---------|--------|-------------|
| 1 | `Source/Sub3D/Submarine/Generator/SubmarineDefinitionTypes.h` | Sub3D | Enums et structs : `ESubCompartmentType`, `EConnectionType`, `ESpawnRole`, `FGeneratedCompartmentDef`, `FGeneratedConnectionDef`, `FGeneratedStationSlotDef`, `FGeneratedSpawnPointDef`, `FBulkheadPassageDef`, `EAirlockSide` |
| 2 | `Source/Sub3D/Submarine/Generator/SubmarineDefinition.h` | Sub3D | `USubmarineDefinition : UDataAsset` — hull metrics, compartments array, connections array, flood graph, station slots, spawn points |
| 3 | `Source/Sub3D/Submarine/Generator/SubmarineDefinition.cpp` | Sub3D | Fonctions utilitaires : `FindCompartment`, `FindConnection`, `GetCompartmentIdsForStation`, validation |
| 4 | `Source/Sub3D/Submarine/Generator/SubmarineGeneratorSpec.h` | Sub3D | `USubmarineGeneratorSpec : UDataAsset` — envelope ref, bulkhead positions, passages, airlock config, requested stations |
| 5 | `Source/Sub3D/Submarine/Generator/SubmarineGeneratorSpec.cpp` | Sub3D | Validation editor-time |
| 6 | `Source/Sub3D/Submarine/Generator/SubmarineGenerator.h` | Sub3D | `USubmarineGenerator : UObject` — `Generate(Spec) → Definition` |
| 7 | `Source/Sub3D/Submarine/Generator/SubmarineGenerator.cpp` | Sub3D | Topologie : hull metrics → compartments → airlock → flood graph → stations → spawns |
| 8 | `Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.h` | Sub3D | `USubmarineMeshBuilder : UObject` — `BuildMeshData(Definition, Envelope)` peuple les mesh arrays |
| 9 | `Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp` | Sub3D | `BuildExteriorHull`, `BuildInteriorCompartments`, `BuildBulkheads` — math superellipse autonome |
| 10 | `Source/Sub3D/Submarine/SubFloodComponent.h` | Sub3D | `USubFloodComponent : UActorComponent` — flood simulation standalone |
| 11 | `Source/Sub3D/Submarine/SubFloodComponent.cpp` | Sub3D | Tick flood : inflow externe, transferts inter-compartiments, pompes, door state → edge modulation |
| 12 | `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.h` | Sub3D | `USubmarineGeneratedGeometryComponent : UActorComponent` — instancie ProceduralMeshComponents depuis USubmarineDefinition |
| 13 | `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.cpp` | Sub3D | BuildFromDefinition : exterior hull, interior compartments, bulkheads — render + collision séparés |

### Fichiers à MODIFIER

| # | Fichier | Changement |
|---|---------|------------|
| 1 | `Source/Sub3D/Submarine/SubmarineBase.h` | Ajouter `USubmarineGeneratorSpec* GeneratorSpec` (EditAnywhere) + `USubFloodComponent* SubFlood` + `USubmarineDefinition* GeneratedDefinition` + `USubmarineGeneratedGeometryComponent* GeneratedGeometry` |
| 2 | `Source/Sub3D/Submarine/SubmarineBase.cpp` | Créer SubFlood et GeneratedGeometry dans constructeur. BeginPlay : si `GeneratedDefinition` est null et `GeneratorSpec` est set, appeler `Generate(Spec)` puis `BuildMeshData()`. Initialiser SubFlood et GeneratedGeometry depuis `GeneratedDefinition`. |
| 3 | `Source/Sub3D/Submarine/SubMovementComponent.h/.cpp` | Ajouter getter pour masse d'eau depuis SubFlood au lieu de SubHull |
| 4 | `Source/Sub3D/Submarine/FloodWaterVisualsComponent.h/.cpp` | Lire niveaux d'eau depuis SubFlood |
| 5 | `Source/Sub3D/Submarine/DoorFloodVfxComponent.h/.cpp` | Lire depuis SubFlood |
| 6 | `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` | Résolution spatiale via `GeneratedDefinition` si `SubHull` est absent, puis lecture flood depuis SubFlood |
| 7 | `Source/Sub3D/Submarine/SubmarineSystemsComponent.h/.cpp` | Router les pompes vers SubFlood et résoudre le compartiment pompe depuis `GeneratedDefinition` |
| 8 | `Source/Sub3D/Submarine/SubmarineFeedbackDirectorComponent.h/.cpp` | Lire depuis SubFlood |
| 9 | `Source/Sub3D/Submarine/SubDoorActor.cpp` | Bridge ouverture/fermeture des portes vers `SubFlood::SetDoorState` |
| 10 | `Source/Sub3D/Submarine/SubmarineStationManagerComponent.h/.cpp` | Spawn des stations depuis `GeneratedDefinition::StationSlots` quand aucune station attachée n'est déjà découverte |
| 11 | `Source/Sub3D/Submarine/SubHullComponent.h/.cpp` | Phase 6 : retirer AdvanceFlooding du Tick, garder damage/breach/flow fields uniquement |
| 12 | `Source/Sub3D/Sub3D.Build.cs` | Ajouter `"Sub3D/Submarine/Generator"` et `"Sub3D/Submarine/GeneratedGeometry"` aux PublicIncludePaths si nécessaire |

### Fichiers à NE PAS TOUCHER au début

| Fichier | Raison |
|---------|--------|
| `Source/Sub3D/SubCompiler/*` | Pipeline legacy. Ne pas modifier, ne pas supprimer. Extraction de fonctions utilitaires par copie, pas par refactoring. |
| `Source/Sub3DBake/*` | Pipeline bake. Le nouveau générateur ne passe pas par Sub3DBake. |
| `Source/Sub3DBuilder/*` | Pipeline builder legacy. |
| `Source/Sub3DEditor/*` | Outils editor parked. |
| `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h` | Réutilisé tel quel (`FCompiledFloodGraph`). Ne pas modifier la structure. |
| `Source/Sub3DRuntime/*` | `USubmarineFloodRuntimeComponent` reste intact. Le nouveau `USubFloodComponent` est dans Sub3D, pas Sub3DRuntime. |
| `Content/Maps/Proto*` | Maps proto existantes. Pas de suppression. |
| `Content/Maps/L_FP01_RunShell.umap` | Map FP existante. Le nouveau level est distinct. |

---

## 3. Ordre d'exécution par phases

### Phase 1 — Contrats de données (aucun runtime, aucune dépendance legacy)

**Objectif** : types compilables, testables dans l'éditeur.

**Fichiers** :
1. `SubmarineDefinitionTypes.h` — tous les structs et enums
2. `SubmarineDefinition.h/.cpp` — le data asset cible
3. `SubmarineGeneratorSpec.h/.cpp` — le data asset d'entrée

**Validation** :
- [ ] Le projet compile.
- [ ] On peut créer un `USubmarineGeneratorSpec` dans l'éditeur et y assigner un `USubmarineEnvelopeDef`.
- [ ] On peut créer un `USubmarineDefinition` vide dans l'éditeur.

**Durée estimée** : non applicable. Complexité faible, aucune logique runtime.

---

### Phase 2 — SubFloodComponent standalone

**Objectif** : simulation flood testable en isolation, sans dépendre du générateur.

**Fichiers** :
1. `SubFloodComponent.h/.cpp`

**API minimale** :
```cpp
void InitializeFromDefinition(const USubmarineDefinition* Definition);
void SetDoorState(FName ConnectionId, bool bClosed);
void CreateBreach(FName CompartmentId, float InflowRateLps);
void RemoveBreach(FName CompartmentId);
void SetPumpActive(FName CompartmentId, bool bActive, float RateLps);
void SetCompartmentFloodDirect(FName CompartmentId, float Level01); // debug
float GetCompartmentFloodLevel01(FName CompartmentId) const;
float GetCompartmentWaterLiters(FName CompartmentId) const;
float GetTotalWaterLiters() const;
float GetTotalWaterMassKg() const;
void ExportCompartmentStates(TArray<FCompartmentState>& OutStates) const;
```

**Test** : Créer un `USubmarineDefinition` à la main en C++ (2 compartiments, 1 porte, 1 edge extérieur sur le sas). Appeler `CreateBreach`, vérifier que l'eau monte. Ouvrir la porte, vérifier le transfert. Activer la pompe, vérifier la baisse.

**Validation** :
- [ ] `SetCompartmentFloodDirect` change le niveau d'eau.
- [ ] `CreateBreach` génère un inflow continu.
- [ ] Porte fermée bloque le transfert entre compartiments.
- [ ] Porte ouverte permet le transfert.
- [ ] Pompe active réduit le niveau.
- [ ] `GetTotalWaterMassKg` retourne la somme correcte.

---

### Phase 3 — Wiring minimal SubmarineBase

**Objectif** : SubFlood tourne dans le pawn, coexiste avec SubHull.

**Fichiers** :
1. `SubmarineBase.h/.cpp` — ajouter `GeneratorSpec`, `SubFlood`, `GeneratedDefinition`, `GeneratedGeometry`
2. `SubMovementComponent.cpp` — lire masse d'eau depuis `SubFlood` si disponible, sinon fallback SubHull

**Invocation runtime** :

`SubmarineBase::BeginPlay` :
1. Résolution de la définition (toutes machines — pipeline déterministe)
   a. Si `GeneratedDefinition != null` : utiliser la définition assignée.
   b. Sinon si `GeneratorSpec != null` :
      - `USubmarineGenerator* Gen = NewObject<USubmarineGenerator>()`
      - `GeneratedDefinition = Gen->Generate(GeneratorSpec)`
      - `USubmarineMeshBuilder* MeshBuilder = NewObject<USubmarineMeshBuilder>()`
      - `MeshBuilder->BuildMeshData(GeneratedDefinition, GeneratorSpec->Envelope)`
   Note : la génération n'est PAS authority-only. Le pipeline Spec → Definition est pur et déterministe. Chaque machine produit la même Definition depuis le même Spec asset.
2. Initialisation runtime
   a. Si `HasAuthority()` et `GeneratedDefinition != null` :
      - `SubFlood->InitializeFromDefinition(GeneratedDefinition)`
   b. Si `GeneratedDefinition != null` (toutes machines) :
      - `GeneratedGeometry->BuildFromDefinition(GeneratedDefinition)`
      - `RefreshMovementCollisionBinding()` — rebind OnHullHit sur les PMCs hull collision générés
   Le build geometry et le rebind collision doivent exister sur toute machine qui affiche le sous-marin.

**Validation** :
- [ ] En PIE, le sous-marin a SubFlood actif.
- [ ] SubHull continue de fonctionner (pas de régression).
- [ ] SubMovement lit la masse d'eau depuis SubFlood.

---

### Phase 4 — Rewire visuels et systèmes vers SubFlood

**Objectif** : tous les consommateurs de flood data lisent SubFlood.

**Fichiers** : `FloodWaterVisualsComponent`, `DoorFloodVfxComponent`, `SubCrewCharacter`, `SubmarineSystemsComponent`, `SubmarineFeedbackDirectorComponent`.

**Stratégie** : chaque composant cherche `SubFlood` sur le Owner. S'il existe, il lit depuis SubFlood. Sinon, comportement actuel (SubHull). Ce dual-read est temporaire (Phase 3-4 uniquement) et sera retiré en Phase 6.

**Validation** :
- [ ] Le water plane visual monte quand SubFlood reçoit de l'eau.
- [ ] Les VFX de porte réagissent à l'état flood SubFlood.
- [ ] Le crew character passe en immersion via SubFlood.
- [ ] Le feedback director lit les bonnes valeurs.

---

### Phase 5A — Generator topology

**Objectif** : le générateur produit une `USubmarineDefinition` structurelle complète, sans matérialiser la géométrie runtime.

**Fichiers** :
1. `SubmarineGenerator.h/.cpp` — topology (compartments, connections, flood graph, stations, spawns)

**Pipeline interne** :

```
SubmarineGenerator::Generate(USubmarineGeneratorSpec* Spec) → USubmarineDefinition*
│
├─ 1. ResolveEnvelope(Spec->Envelope)
│     → HullLengthCm, HullBeamCm, HullHeightCm, BaseMassKg, SubmergedVolumeLiters
│
├─ 2. DeriveCompartments(BulkheadPositions, Envelope)
│     → TArray<FGeneratedCompartmentDef>
│     → CapacityLiters, HydroBounds, WalkableFloorZCm
│
├─ 3. GenerateAirlock(Spec->AirlockPosition, Spec->AirlockSide)
│     → 1 FGeneratedCompartmentDef (type Airlock)
│     → 2 FGeneratedConnectionDef (inner door + outer hatch)
│
├─ 4. BuildFloodGraph(Compartments, Connections)
│     → FCompiledFloodGraph
│
├─ 5. PlaceStations(RequestedStations, Compartments)
│     → TArray<FGeneratedStationSlotDef>
│
└─ 6. PlaceSpawns(Compartments)
      → TArray<FGeneratedSpawnPointDef>
```

**Validation** :
- [ ] `Generate()` retourne un `USubmarineDefinition` non-null.
- [ ] `Compartments.Num()` == nombre de segments entre bulkheads + 1 airlock.
- [ ] `Connections.Num()` == nombre de bulkheads avec passage + 2 (airlock inner + outer).
- [ ] `FloodGraph.Volumes.Num()` == `Compartments.Num()`.
- [ ] `FloodGraph.Edges.Num()` == `Connections.Num()`.
- [ ] Chaque station est dans les bounds d'un compartiment valide.

---

### Phase 5B — Mesh data generation

**Objectif** : générer les mesh data dans `USubmarineDefinition`, sans encore créer les `UProceduralMeshComponent`.

**Fichiers** :
1. `SubmarineMeshBuilder.h/.cpp` — mesh data (exterior hull, interiors, bulkheads)

**Pipeline interne** :

```
SubmarineMeshBuilder::BuildMeshData(Definition, Envelope)
│
├─ 1. BuildExteriorHull(Definition, Envelope)
│     → Definition.ExteriorHullMesh (closed, with bow/stern caps)
│
├─ 2. BuildInteriorCompartments(Definition, Envelope)
│     → Definition.InteriorMeshes[] (walls, floor, caps par compartiment)
│
└─ 3. BuildBulkheads(Definition, Envelope)
      → Definition.BulkheadMeshes[] (panel avec cutout si passage)
```

**Extraction depuis SubmarineGeometryBuilder** :

La logique de mesh (superellipse, arc segments, bow/stern caps) est dans `SubmarineMeshBuilder`.
Elle a été extraite/adaptée depuis `USubmarineGeometryBuilder` (legacy SubCompiler).
`SubmarineMeshBuilder` ne dépend pas de `FSubmarineLayoutSolution`. Il lit `USubmarineDefinition + USubmarineEnvelopeDef` directement.

**Validation** :
- [ ] `Definition->ExteriorHullMesh` est peuplé.
- [ ] `Definition->InteriorMeshes.Num()` == `Definition->Compartments.Num()` moins les compartiments non supportés par la coque principale.
- [ ] `Definition->BulkheadMeshes.Num()` couvre les cloisons générées.
- [ ] Le mesh extérieur est fermé (caps présents).

---

### Phase 5C — Runtime gameplay integration

**Objectif** : fermer les ponts runtime gameplay entre `GeneratedDefinition`, `SubFlood` et les systèmes existants.

**Fichiers** :
1. `SubDoorActor.cpp` — bridge portes → `SubFlood::SetDoorState`
2. `SubmarineBase.h/.cpp` — bridge breaches/damage → `SubFlood`, fallback spawn, génération conditionnelle
3. `SubCrewCharacter.cpp` — requête spatiale via `GeneratedDefinition` quand `SubHull` est absent
4. `SubmarineStationManagerComponent.h/.cpp` — spawn des stations depuis `StationSlots`
5. `SubmarineSystemsComponent.cpp` — résolution du compartiment pompe depuis la définition

**Validation** :
- [ ] Ouvrir/fermer une porte met à jour l'edge correspondant dans `SubFlood`.
- [ ] Un impact coque crée un inflow dans `SubFlood`, avec ou sans structural sheets sur `SubHull`.
- [ ] Le crew résout son compartiment via la définition quand `SubHull` est absent.
- [ ] Les spawns et stations peuvent être alimentés depuis `GeneratedDefinition`.

---

### Phase 5D — Runtime Mesh Materialization

**Objectif** : Matérialiser `USubmarineDefinition` en géométrie visible, marchable et collidable au runtime. Ferme la chaîne Spec → Definition → Visible Runtime Submarine. Permet un FP01 purement généré sans dépendance au runtime actor legacy.

**Pipeline corrigé** :
```
Spec → Definition → Runtime Geometry → Runtime Systems → Migration Legacy
```

**Architecture** : Composant `USubmarineGeneratedGeometryComponent` sur `ASubmarineBase`.

Raisons :
- `ASubmarineBase` possède déjà `GeneratedDefinition`, `SubFlood`, `SubHull`, les bridges 5C.
- Pas de duplication d'ownership runtime.
- Cohérent avec le reste de l'architecture en composants.

**Fichiers à créer** :
1. `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.h`
2. `Source/Sub3D/Submarine/GeneratedGeometry/SubmarineGeneratedGeometryComponent.cpp`

**Fichiers à modifier** :
1. `SubmarineBase.h` — ajouter `USubmarineGeneratedGeometryComponent* GeneratedGeometry`, `BoundHullCollisionComponents`
2. `SubmarineBase.cpp` — créer le composant, appeler `BuildFromDefinition(GeneratedDefinition)` en BeginPlay, rebind collision. `GetMovementCollisionComponent()` préfère les PMCs hull générés. `RefreshMovementCollisionBinding()` bind `OnHullHit` sur tous les PMCs hull. `GetInteriorWalkableComponents()` retourne les floor collision PMCs.

**API** :
```cpp
UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
bool BuildFromDefinition(const USubmarineDefinition* Definition);
```

**Entrée** : mesh data peuplée par `SubmarineMeshBuilder::BuildMeshData()` dans `USubmarineDefinition` :
- `ExteriorHullMesh` — `FSubmarineMeshSectionData`
- `InteriorMeshes[]` — `FSubmarineInteriorCompartmentMeshData` (Wall, Floor, BowCap, SternCap)
- `BulkheadMeshes[]` — `FSubmarineBulkheadMeshData` (Panel)

**Sortie** : `UProceduralMeshComponent` instances sur l'acteur.

| Catégorie | Render (visible) | Collision (caché) |
|-----------|-------------------|-------------------|
| Coque extérieure | 1 PMC render | N PMC collision (1 convex hull par slice longitudinal via `AddCollisionConvexMesh`, profil `SubmarineHull` avec Pawn→Ignore, `SetNotifyRigidBodyCollision(true)`, `HullCollisionSlices` = 4 par défaut) |
| Intérieur par compartiment | Walls + Floor + BowCap + SternCap renders | Floor collision (marchable, profil `SubInteriorWalkable`, object type `SubInterior`/`ECC_GameTraceChannel2`) |
| Bulkhead par cloison | Panel render | Panel collision (BlockAll) |

**Tagging** :
- `GeneratedRender` / `GeneratedCollision` — rôle
- `CompartmentId=<id>` — quand applicable

**Matériaux** : défaut UE ou assignation post-construction en Blueprint. Pas de material pipeline dans cette phase.

**Prérequis** : Phase 5B (mesh data) et Phase 3 (SubFlood + GeneratedDefinition + invocation runtime). Phase 5C est requise pour le chemin FP gameplay complet.

**Validation** :
- [ ] Le sous-marin généré est visible en jeu.
- [ ] La coque bloque correctement les collisions extérieures.
- [ ] Le crew peut marcher sur les floors intérieurs.
- [ ] Les bulkheads bloquent le passage.
- [ ] Les portes visibles restent cohérentes avec les collisions.
- [ ] Aucun asset/runtime actor legacy requis pour afficher le FP01.

---

### Phase 6 — Migration SubHull (flood removal)

**Objectif** : SubHull ne simule plus le flood. SubFlood est l'unique simulateur.

**Fichiers** :
1. `SubHullComponent.h/.cpp` — retirer `AdvanceFlooding`, `UpdateCompartmentDerivedState` du Tick. Garder : damage, breach clusters, flow fields. Les breaches créent des `CreateBreach` sur SubFlood quand des structural sheets existent. Le chemin pure generator route directement `damage -> résolution de compartiment via GeneratedDefinition -> SubFlood`.
2. Retirer le dual-read des composants visuels (Phase 4).

**Prérequis** : Phase 2-5D validées.

**Validation** :
- [ ] SubHull ne ticke plus de flood.
- [ ] Un impact hull crée un breach cluster dans SubHull ET un inflow dans SubFlood quand des structural sheets existent.
- [ ] Un impact hull crée un inflow direct dans SubFlood quand `SubHull` n'a pas de structural sheets et que `GeneratedDefinition` est active.
- [ ] Pas de régression sur les visuels de damage.
- [ ] Pas de régression sur la simulation de masse/mouvement.

---

### Phase 7 — Deprecation

Marquer comme `DEPRECATED` (commentaire + UE_DEPRECATED) :
- `USubmarineLayoutAsset`
- `EnsureFallbackLayout()` dans SubHull
- `BakeLayoutFromVolumes()` dans SubmarineBase
- L'usage FP de `Sub3DBake` pipeline
- `SubCompilerMvpFactory`

Ne pas supprimer. Ne pas modifier le code legacy.

**Execution note - Phase 7A / 7B** :

La Phase 7 s'execute en deux sous-phases :

- **Phase 7A - Legacy marking non-cassant**
  - Ne pas ajouter `UE_DEPRECATED` sur les cibles encore load-bearing :
    - `USubmarineLayoutAsset`
    - `EnsureFallbackLayout()`
    - `BakeLayoutFromVolumes()`
    - `SubCompilerMvpFactory`
    - l'usage FP de `Sub3DBake`
  - Ajouter seulement :
    - commentaires `LEGACY` explicites
    - warnings runtime quand un fallback legacy est pris
  - Points de warning minimum :
    - fallback `SubmarineBase::BeginPlay` vers `LayoutAsset`
    - `SubFlood::InitializeFromLayout(...)`
    - fallback `FloodWaterVisualsComponent`
    - fallback `SubCrewCharacter`

- **Phase 7B - Deprecation compile-time**
  - Ajouter `UE_DEPRECATED` seulement quand le chemin generated est le chemin par defaut reel et que les fallbacks `LayoutAsset` ont disparu du runtime normal.
  - Aucun pragma de contournement pour forcer la deprecation.
  - Le code legacy reste present, mais il n'est plus load-bearing pour le FP.

---

## 4. Types/classes à créer (liste complète)

### Enums

```cpp
// SubmarineDefinitionTypes.h
enum class ESubCompartmentType : uint8 { Generic, Helm, Crew, Engine, Airlock };
enum class EConnectionType : uint8 { Door, Hatch, ExteriorHatch, Open };
enum class ESpawnRole : uint8 { Pilot, Crew };
enum class EAirlockSide : uint8 { Port, Starboard, Top };
```

### Structs

```cpp
// SubmarineDefinitionTypes.h
FBulkheadPassageDef        // BulkheadIndex, PassageType, DoorWidthCm, DoorHeightCm
FGeneratedCompartmentDef   // Id, DisplayName, SemanticType, CapacityLiters, HydroBounds, WalkableFloorZ
FGeneratedConnectionDef    // Id, CompartmentA, CompartmentB, FlowAreaCm2, LocalTransform, Type, bStartsClosed
FGeneratedStationSlotDef   // Id, StationType, CompartmentId, LocalTransform
FGeneratedSpawnPointDef    // Id, LocalTransform, Role
```

### Classes

```cpp
// SubmarineDefinition.h
UCLASS() USubmarineDefinition : UDataAsset

// SubmarineGeneratorSpec.h
UCLASS() USubmarineGeneratorSpec : UDataAsset

// SubmarineGenerator.h
UCLASS() USubmarineGenerator : UObject

// SubmarineMeshBuilder.h
UCLASS() USubmarineMeshBuilder : UObject

// SubFloodComponent.h
UCLASS() USubFloodComponent : UActorComponent

// SubmarineGeneratedGeometryComponent.h
UCLASS() USubmarineGeneratedGeometryComponent : UActorComponent
```

---

## 5. Fichiers à modifier (récapitulatif)

| Fichier | Phase | Nature du changement |
|---------|-------|---------------------|
| `SubmarineBase.h/.cpp` | 3 | Ajout GeneratorSpec + SubFlood + GeneratedDefinition + GeneratedGeometry. BeginPlay : génération conditionnelle + init flood + init geometry |
| `SubMovementComponent.cpp` | 3 | Lire masse depuis SubFlood |
| `FloodWaterVisualsComponent.cpp` | 4 | Source flood → SubFlood |
| `DoorFloodVfxComponent.cpp` | 4 | Source flood → SubFlood |
| `SubCrewCharacter.cpp` | 4 / 5C | Requête spatiale via GeneratedDefinition si `SubHull` est absent, puis source immersion → SubFlood |
| `SubmarineSystemsComponent.cpp` | 4 / 5C | Pompes routées vers SubFlood, compartiment résolu depuis GeneratedDefinition |
| `SubmarineFeedbackDirectorComponent.cpp` | 4 | Source flood → SubFlood |
| `SubDoorActor.cpp` | 5C | Bridge porte → `SubFlood::SetDoorState` |
| `SubmarineStationManagerComponent.h/.cpp` | 5C | Spawn stations depuis `StationSlots` |
| `SubHullComponent.h/.cpp` | 6 | Retirer flood du Tick, bridge breach → SubFlood |
| `Sub3D.Build.cs` | 1 | Ajout include path Generator/ |

---

## 6. Fichiers à ne pas toucher (récapitulatif)

| Dossier/Fichier | Raison |
|-----------------|--------|
| `Source/Sub3D/SubCompiler/*` | Legacy pipeline. Lecture seule pour extraction de math. |
| `Source/Sub3DBake/*` | Bake pipeline (editor v2, parked). |
| `Source/Sub3DBuilder/*` | Builder pipeline (editor v2, parked). |
| `Source/Sub3DEditor/*` | Editor tools (parked). |
| `Source/Sub3DCore/Public/Types/*` | Types canoniques. Réutilisés, pas modifiés. |
| `Source/Sub3DRuntime/*` | Runtime module existant. SubFlood vit dans Sub3D, pas ici. |
| `Content/Maps/Proto*` | Maps proto, pas de suppression. |
| `Content/Maps/L_FP01_RunShell.umap` | Map FP existante, le nouveau level est distinct. |

---

## 7. Risques techniques et mitigations

| # | Risque | Impact | Mitigation |
|---|--------|--------|------------|
| 1 | **SubmarineGeometryBuilder couplé à FSubmarineLayoutSolution** | Le nouveau pipeline ne peut pas appeler directement les fonctions existantes sans créer une solution layout | Extraire/copier la logique de mesh dans `SubmarineMeshBuilder`. Les fonctions math (superellipse, arc) sont pures et petites (~100-200 lignes chacune). Pas de wrapper intermédiaire. |
| 2 | **SubHull flood + SubFlood flood en parallèle (Phase 3-4)** | Double simulation d'eau, valeurs incohérentes | Pendant Phase 3-4, SubFlood est la source de vérité pour les visuels. SubHull continue de tourner mais ses valeurs de flood ne sont plus consommées par les visuels. Phase 6 supprime le double. |
| 3 | **Replication : SubFlood compartment states** | SubHull réplique déjà les compartment states. SubFlood doit aussi répliquer. | SubFlood réplique ses propres `CompartmentStates` via `DOREPLIFETIME`. Pendant la coexistence, seuls les states SubFlood sont lus par les visuels. |
| 4 | **Airlock mesh : soudure au hull** | L'excroissance géométrique du sas doit s'intégrer au mesh extérieur sans trous | Pour le FP, le sas est une boîte simple (6 faces) soudée au hull. Le trou dans le hull est fait au moment de la génération (cutout dans les triangles du hull au point d'attache). Acceptable visuellement pour le FP. |
| 5 | **Collision du mesh généré** | Un ProceduralMesh n'a pas de collision physics par défaut | Convex decomposition : 1 convex hull par segment longitudinal pour la coque, 1 convex par floor de compartiment. Suffisant pour le FP, pas de triangle-level collision nécessaire. |
| 6 | **USubmarineEnvelopeDef inclut SubCompilerTypes.h** | Le générateur hérite transitoirement de tous les types legacy | Acceptable pour le FP. Le générateur n'utilise aucun type de SubCompilerTypes.h directement (il a ses propres types). L'inclusion transitive ne crée pas de couplage fonctionnel. |
| 7 | **SubmarineDefinition.h inclut SubmarineGeometryBuilder.h** | Le nouveau type central dépend du header legacy pour `FSubmarineMeshSectionData`, `FSubmarineInteriorCompartmentMeshData`, `FSubmarineBulkheadMeshData` | Acceptable pour le FP. Post-FP : déplacer ces 3 structs dans `SubmarineDefinitionTypes.h` et retirer l'include legacy. |

---

## 8. Checklist de validation PIE

### Validation structurelle (peut être faite dans l'éditeur, sans PIE)
- [ ] `USubmarineGeneratorSpec` créable dans le Content Browser.
- [ ] `USubmarineDefinition` créable dans le Content Browser.
- [ ] Assigner un `USubmarineEnvelopeDef` au Spec, remplir les bulkheads.
- [ ] Appeler `Generate()` → obtenir un Definition non-null.
- [ ] Inspecter les compartments, connections, flood graph dans l'éditeur.

### Validation PIE — Flood
- [ ] `SetCompartmentFloodDirect("Helm", 0.5)` → le water plane monte dans le compartiment Helm.
- [ ] `CreateBreach("Engine", 500.f)` → l'eau monte dans Engine progressivement.
- [ ] Ouvrir la porte Helm↔Engine → l'eau se transfère.
- [ ] Fermer la porte → le transfert s'arrête.
- [ ] Ouvrir le hatch extérieur du sas (porte intérieure fermée) → seul le sas se remplit.
- [ ] Ouvrir la porte intérieure du sas → l'eau entre dans le compartiment adjacent.
- [ ] Activer la pompe dans un compartiment → le niveau baisse.
- [ ] La masse d'eau affecte le mouvement du sous-marin (vérifier via SubMovement).

### Validation PIE — Crew
- [ ] Le crew character spawn au point défini dans SpawnPoints.
- [ ] Le crew peut traverser les compartiments via les portes.
- [ ] Le crew détecte l'immersion dans un compartiment inondé.

### Validation PIE — Hull intégrité
- [ ] Un impact crée un breach cluster dans SubHull.
- [ ] Le breach cluster déclenche un inflow dans SubFlood.
- [ ] Réparer le breach réduit/stoppe l'inflow.

### Validation PIE — Mesh
- [ ] Le mesh extérieur est visible et fermé (pas de trous aux caps).
- [ ] La collision fonctionne (le sous-marin ne tombe pas à travers le monde).
- [ ] L'intérieur est marchable (le crew character ne tombe pas à travers le sol).

---

## 9. Level First Playable + VFX

### 9.1 Level FP — `L_FP01_Generated`

**Création** : nouveau level dans `Content/Maps/L_FP01_Generated.umap`.

**Contenu minimal** :

| Élément | Détail |
|---------|--------|
| **Submarine spawn** | Un `ASubmarineBase` placé à l'origine (0, 0, -5000) = 50m de profondeur |
| **Player spawn** | `APlayerStart` attaché conceptuellement au spawn point défini dans `USubmarineDefinition` |
| **GameMode override** | `SubGameMode` (existant) |
| **Sky/lighting** | Directional light très faible (intensity 0.05-0.1, bleu profond) ou absente |
| **Post Process Volume** | Unbounded, color grading sous-marin, exposure basse |
| **Fog** | Exponential Height Fog (bleu-noir, density élevée, start distance courte) |
| **Water** | Pas de water plane global — le sous-marin est supposé entièrement immergé. L'eau est l'environnement par défaut. |
| **Terrain** | Optionnel : un plan ou landscape simple très en dessous pour donner une référence de fond |

**Ce qui n'est PAS dans ce level** :
- Aucun `TraversalRouteActor` ou tunnel de world gen.
- Aucun `SubmarineCompilerActor` ou `SubmarineEditorActor`.
- Aucun BP legacy (`BP_SubmarineBase_Proto01`, `BP_Submarine_Compiler`).
- Aucune référence aux assets de `Content/Sub3D/Proto03/` ou `Content/Sub3D/Proto04C/`.

### 9.2 VFX sous-marine — première passe

**Objectif** : lisibilité gameplay, pas rendu final. Le joueur doit comprendre qu'il est sous l'eau, voir où il va, distinguer les obstacles.

#### Fog / Atmosphère

| Paramètre | Valeur indicative | Notes |
|-----------|-------------------|-------|
| Exponential Height Fog | Density 0.02, Height Falloff 0.01 | Fog uniforme (pas de surface) |
| Fog Inscattering Color | Bleu très sombre (#0A1520) | Abysse profonde |
| Volumetric Fog | Enabled, Scattering 0.01 | Subtil, pour light shafts |
| Max Opacity | 0.95 | Jamais full black, toujours un minimum de lisibilité |

#### Post Process

| Paramètre | Valeur indicative |
|-----------|-------------------|
| Color Grading → Global Saturation | 0.6 (désaturer légèrement) |
| Color Grading → Shadows Tint | Bleu (#1A2A3A) |
| Auto Exposure → Min/Max Brightness | 0.5 / 2.0 (range serré, pas de flash) |
| Vignette Intensity | 0.3 |
| Bloom → Intensity | 0.2 (subtil, pour bioluminescence) |

#### Lumière

| Source | Config |
|--------|--------|
| Directional Light | Intensity 0.05 lux, angle rasant, couleur bleu-vert pâle. Simule une lumière résiduelle filtée. Optionnel : absente si profondeur > 200m. |
| Sub interior lights | Point lights dans chaque compartiment, blanc chaud (3200K), intensity modérée. Gameplay-driven : le joueur voit où il marche. |
| Sub exterior lights | 1-2 spot lights montés sur le hull, blanc froid, portée ~30m. Éclairent devant le sous-marin. |

#### Niagara — Faune ambiante (non-interactive)

| Système | Description |
|---------|-------------|
| `NS_AmbientParticles` | Particules de plancton/sédiment. GPU particles, très petites, dérive lente. Donne du volume au fog. |
| `NS_BioluminescentFauna` | Petits points lumineux (jellyfish/plankton bioluminescent). Spawn autour du sous-marin dans un rayon de 50-100m. Émissifs, pas d'ombre. Mouvement organique simple (sine wave + noise). |

**Pas pour le FP** :
- Pas de caustics.
- Pas de god rays complexes.
- Pas de simulation de particules interactives (réaction au sous-marin).
- Pas de faune animée (poissons, créatures). Juste des particules.

#### Implémentation

1. Créer un `BP_UnderwaterAtmosphere` (Actor Blueprint) qui contient :
   - Exponential Height Fog component
   - Post Process component
   - Directional Light (optionnel)
2. Placer cet actor dans `L_FP01_Generated`.
3. Créer les systèmes Niagara (`NS_AmbientParticles`, `NS_BioluminescentFauna`) dans `Content/Sub3D/VFX/`.
4. Les attacher au sous-marin ou les spawner dans le level.

---

## 10. Résumé de l'ordre d'exécution

```
Phase 1  : Types & contrats          → compile, zéro runtime
Phase 2  : SubFloodComponent         → testable en isolation
Phase 3  : Wiring SubmarineBase      → SubFlood tourne, coexiste avec SubHull
Phase 4  : Rewire visuels            → tous les consommateurs lisent SubFlood
Phase 5A : Generator data            → produit USubmarineDefinition (layout, flood graph, stations)
Phase 5B : Mesh data generation      → produit mesh data dans USubmarineDefinition
Phase 5C : Runtime gameplay wiring   → bridges portes/breaches/crew/stations/pompes
Phase 5D : Runtime geometry          → matérialise les meshes en ProceduralMeshComponents
Phase 6  : Migration SubHull         → flood retiré de SubHull, damage-only
Phase 7  : Deprecation markers       → legacy marqué, pas supprimé
```

Pipeline cible : `Spec → Definition → Runtime Geometry → Runtime Systems → Migration Legacy`

Chaque phase est indépendamment testable. Si une phase échoue, les précédentes restent fonctionnelles.

### Note : Persistence (hors scope phases actuelles)

La séparation canonique pour la suite :
- **Spec** = entrée design (éditeur)
- **Definition** = structure compilée (runtime, immutable)
- **PersistentState** = état mutable persistant (eau, dégâts, portes, position, modules)

Le générateur produit une `USubmarineDefinition` en mémoire au runtime. La sauvegarde durable ne passe pas par l'écriture d'un nouvel asset sur disque, mais par un `FSubmarinePersistentState` séparé qui capture l'état dynamique.
