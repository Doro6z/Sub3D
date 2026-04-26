# Corrections du plan submarine_generator_implementation_plan.md

Date: 2026-04-09  
Input: `2026-04-08_submarine_generator_implementation_plan.md` + audit code  
Status: 11 corrections a appliquer au plan d'implementation.

---

## Contexte

Le plan (2026-04-08) a ete implemente jusqu'a Phase 5. Une Phase 5D a ete inseree pour la materialisation mesh.
L'audit a trouve : references cassees (5B/5C), fichier existant absent du plan (SubmarineMeshBuilder),
point d'invocation runtime absent, collision contradictoire, comptage de phases faux.

---

## Correction 1 — Architecture diagram : ajouter MeshBuilder + GeneratedGeometry

**Fichier cible** : Section 1, le diagramme ASCII

Remplacer le diagramme par :

```
USubmarineGeneratorSpec (UDataAsset, editor-authored)
    |
    |  USubmarineEnvelopeDef* (hull profile, reused as-is)
    |  Bulkhead positions, passage types
    |  Airlock position/side
    |  Requested stations
    |
    v
USubmarineGenerator (UObject, stateless)
    |
    |  Step 1: Hull envelope -> closed exterior mesh + collision + volume
    |  Step 2: Interior space -> walkable floor + walls
    |  Step 3: Bulkheads -> mesh with door cutouts
    |  Step 4: Compartments -> derived from bulkhead segments
    |  Step 5: Airlock -> explicit compartment + 2 connections
    |  Step 6: Flood graph -> FCompiledFloodGraph (Sub3DCore type)
    |  Step 7: Stations & spawns -> placed transforms in compartments
    |
    v
USubmarineMeshBuilder (UObject, stateless)
    |
    |  Reads Definition compartments + EnvelopeDef profile
    |  Populates Definition.ExteriorHullMesh
    |  Populates Definition.InteriorMeshes[]
    |  Populates Definition.BulkheadMeshes[]
    |
    v
USubmarineDefinition (UDataAsset, runtime single source of truth)
    |
    |  Hull metrics, compartments, connections, flood graph,
    |  station slots, spawn points, mesh data arrays
    |
    v
USubmarineGeneratedGeometryComponent (UActorComponent)
    |  Converts mesh data arrays -> UProceduralMeshComponent instances
    |  Render + collision separation
    |
    v
Runtime Components (read from USubmarineDefinition)
    +-- USubFloodComponent (new) -- flood simulation
    +-- USubHullComponent (existing) -- damage only
    +-- USubmarineCompartmentComponent (existing) -- facade, reads SubFlood
    +-- FloodWaterVisualsComponent -- reads SubFlood
    +-- DoorFloodVfxComponent -- reads SubFlood
    +-- SubMovementComponent -- reads SubFlood for mass
    +-- SubCrewCharacter -- reads SubFlood for immersion
```

**Raison** : SubmarineMeshBuilder existe dans le code mais pas dans le diagramme. GeneratedGeometryComponent est dans Phase 5D mais absent du diagramme.

---

## Correction 2 — Table fichiers a creer : ajouter SubmarineMeshBuilder

**Fichier cible** : Section 2, table "Fichiers a CREER"

Inserer apres la ligne 7 (SubmarineGenerator.cpp) :

```
| 8 | Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.h | Sub3D | USubmarineMeshBuilder : UObject — BuildMeshData(Definition, Envelope) popule les mesh arrays |
| 9 | Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp | Sub3D | BuildExteriorHull, BuildInteriorCompartments, BuildBulkheads — math superellipse autonome |
```

Renumeroter les lignes suivantes :
- SubFloodComponent.h -> 10
- SubFloodComponent.cpp -> 11
- SubmarineGeneratedGeometryComponent.h -> 12
- SubmarineGeneratedGeometryComponent.cpp -> 13

**Raison** : Ce fichier existe dans le code, il doit etre dans le plan.

---

## Correction 3 — Table fichiers a modifier : ajouter GeneratorSpec sur SubmarineBase

**Fichier cible** : Section 2, table "Fichiers a MODIFIER", ligne 1

Ancienne ligne 1 :
```
| 1 | Source/Sub3D/Submarine/SubmarineBase.h | Ajouter USubFloodComponent* SubFlood + USubmarineDefinition* GeneratedDefinition + USubmarineGeneratedGeometryComponent* GeneratedGeometry |
```

Nouvelle ligne 1 :
```
| 1 | Source/Sub3D/Submarine/SubmarineBase.h | Ajouter USubmarineGeneratorSpec* GeneratorSpec (EditAnywhere) + USubFloodComponent* SubFlood + USubmarineDefinition* GeneratedDefinition + USubmarineGeneratedGeometryComponent* GeneratedGeometry |
```

Ancienne ligne 2 :
```
| 2 | Source/Sub3D/Submarine/SubmarineBase.cpp | Creer SubFlood et GeneratedGeometry dans constructeur, initialiser depuis GeneratedDefinition dans BeginPlay |
```

Nouvelle ligne 2 :
```
| 2 | Source/Sub3D/Submarine/SubmarineBase.cpp | Creer SubFlood et GeneratedGeometry dans constructeur. BeginPlay : si GeneratedDefinition est null et GeneratorSpec est set, appeler Generate(Spec) puis BuildMeshData(). Initialiser SubFlood et GeneratedGeometry depuis GeneratedDefinition. |
```

**Raison** : Le plan ne dit nulle part quand Generate() est appele. Ca ferme le trou d'invocation.

---

## Correction 4 — Phase 5 : separer mesh generation du generateur

**Fichier cible** : Section 3, Phase 5

Remplacer la section "Fichiers" :

Ancien :
```
**Fichiers** :
1. SubmarineGenerator.h/.cpp
```

Nouveau :
```
**Fichiers** :
1. SubmarineGenerator.h/.cpp — topology (compartments, connections, flood graph, stations, spawns)
2. SubmarineMeshBuilder.h/.cpp — mesh data (exterior hull, interiors, bulkheads)
```

Remplacer le pipeline interne :

Ancien :
```
Generate(USubmarineGeneratorSpec* Spec) -> USubmarineDefinition*
|
+- 1. ResolveEnvelope(...)
+- 2. GenerateHullMesh(...)     -> Exterior mesh
+- 3. GenerateInterior(...)     -> Floor mesh, walls
+- 4. GenerateBulkheads(...)    -> Bulkhead meshes
+- 5. DeriveCompartments(...)
+- 6. GenerateAirlock(...)
+- 7. BuildFloodGraph(...)
+- 8. PlaceStations(...)
+- 9. PlaceSpawns(...)
```

Nouveau :
```
=== SubmarineGenerator::Generate(Spec) -> USubmarineDefinition* ===
|
+- 1. ResolveEnvelope(Spec->Envelope)
|     -> HullLengthCm, HullBeamCm, HullHeightCm, BaseMassKg, SubmergedVolumeLiters
|
+- 2. DeriveCompartments(BulkheadPositions, Envelope)
|     -> TArray<FGeneratedCompartmentDef>
|
+- 3. GenerateAirlock(Spec->AirlockPosition, Spec->AirlockSide)
|     -> 1 FGeneratedCompartmentDef (type Airlock) + 2 FGeneratedConnectionDef
|
+- 4. BuildFloodGraph(Compartments, Connections)
|     -> FCompiledFloodGraph
|
+- 5. PlaceStations(RequestedStations, Compartments)
|     -> TArray<FGeneratedStationSlotDef>
|
+- 6. PlaceSpawns(Compartments)
|     -> TArray<FGeneratedSpawnPointDef>

=== SubmarineMeshBuilder::BuildMeshData(Definition, Envelope) ===
|   Appele apres Generate(). Lit les compartments et hull metrics du Definition.
|
+- 1. BuildExteriorHull(Definition, Envelope)
|     -> Definition.ExteriorHullMesh (closed, with bow/stern caps)
|
+- 2. BuildInteriorCompartments(Definition, Envelope)
|     -> Definition.InteriorMeshes[] (walls, floor, caps par compartiment)
|
+- 3. BuildBulkheads(Definition, Envelope)
|     -> Definition.BulkheadMeshes[] (panel avec cutout si passage)
```

Remplacer la section "Extraction depuis SubmarineGeometryBuilder" :

Ancien :
```
Les fonctions suivantes de USubmarineGeometryBuilder contiennent la math utile. Le generateur copie la logique pertinente (...) dans des fonctions privees internes.
```

Nouveau :
```
La logique de mesh (superellipse, arc segments, bow/stern caps) est dans SubmarineMeshBuilder.
Elle a ete extraite/adaptee depuis USubmarineGeometryBuilder (legacy SubCompiler).
SubmarineMeshBuilder ne depend pas de FSubmarineLayoutSolution. Il lit USubmarineDefinition + USubmarineEnvelopeDef directement.
```

**Raison** : Le plan disait que la mesh generation etait interne a SubmarineGenerator. En realite SubmarineMeshBuilder est un fichier separe. Le pipeline doit refleter cette separation.

---

## Correction 5 — Phase 5D : references cassees 5B/5C

**Fichier cible** : Section 3, Phase 5D

Ancien :
```
**Prerequis** : Phase 5B (mesh data), Phase 5C (wiring runtime).
```

Nouveau :
```
**Prerequis** : Phase 5 (Definition avec mesh data popule par SubmarineMeshBuilder), Phase 3 (SubFlood wire dans SubmarineBase).
```

**Raison** : Phase 5B et 5C n'existent pas dans le plan.

---

## Correction 6 — Phase 5D : entree corrigee

**Fichier cible** : Section 3, Phase 5D, sous-section "Entree"

Ancien :
```
**Entree** : mesh data deja generee en 5B dans USubmarineDefinition :
```

Nouveau :
```
**Entree** : mesh data popule par SubmarineMeshBuilder::BuildMeshData() dans USubmarineDefinition :
```

**Raison** : Meme reference cassee a 5B.

---

## Correction 7 — Phase 5D : collision coherente

**Fichier cible** : Section 3, Phase 5D, table collision

Ancien :
```
| Coque exterieure | 1 PMC render | 1 PMC collision (BlockAll, bUseComplexAsSimple) |
```

Nouveau :
```
| Coque exterieure | 1 PMC render | 1 PMC collision (BlockAll, convex decomposition — 1 convex par segment longitudinal) |
```

Et dans la Section 7 (Risques), Risque 5, remplacer :

Ancien :
```
UE5 supporte bUseComplexAsSimpleCollision sur ProceduralMesh, mais pour le FP un convex approximatif suffit.
```

Nouveau :
```
Convex decomposition : 1 convex hull par segment longitudinal pour la coque, 1 convex par floor de compartiment. Suffisant pour le FP, pas de triangle-level collision necessaire.
```

**Raison** : Le plan disait bUseComplexAsSimple a un endroit et convex a l'autre. Decision : convex pour le FP.

---

## Correction 8 — Comptage de phases

**Fichier cible** : Partout ou "10 phases" apparait

Ancien : `10 phases`
Nouveau : `8 phases (1, 2, 3, 4, 5, 5D, 6, 7)`

**Raison** : 1+2+3+4+5+5D+6+7 = 8, pas 10.

---

## Correction 9 — Ajouter note sur la dependance mesh structs

**Fichier cible** : Section 7, apres le Risque 6

Ajouter :

```
| 7 | **SubmarineDefinition.h inclut SubmarineGeometryBuilder.h** | Le nouveau type central depend du header legacy pour FSubmarineMeshSectionData, FSubmarineInteriorCompartmentMeshData, FSubmarineBulkheadMeshData | Acceptable pour le FP. Post-FP : deplacer ces 3 structs dans SubmarineDefinitionTypes.h et retirer l'include legacy. |
```

**Raison** : Le plan dit "ne pas toucher SubCompiler" mais SubmarineDefinition inclut un header SubCompiler. C'est un couplage qui doit etre documente.

---

## Correction 10 — Ajouter section invocation runtime a Phase 3

**Fichier cible** : Section 3, Phase 3

Ajouter apres "Fichiers" :

```
**Invocation runtime** :

SubmarineBase::BeginPlay :
1. Si GeneratedDefinition == null ET GeneratorSpec != null :
   a. USubmarineGenerator* Gen = NewObject<USubmarineGenerator>()
   b. GeneratedDefinition = Gen->Generate(GeneratorSpec)
   c. USubmarineMeshBuilder* Mesh = NewObject<USubmarineMeshBuilder>()
   d. Mesh->BuildMeshData(GeneratedDefinition, GeneratorSpec->Envelope)
2. Si GeneratedDefinition != null :
   a. SubFlood->InitializeFromDefinition(GeneratedDefinition)
   b. GeneratedGeometry->BuildFromDefinition(GeneratedDefinition)  // Phase 5D

Le GeneratorSpec est EditAnywhere sur SubmarineBase.
Le GeneratedDefinition peut aussi etre pre-assigne dans l'editeur (skip generation).
```

**Raison** : Le plan ne definissait nulle part quand le generateur est appele. C'est le trou central du plan.

---

## Correction 11 — Section 5 recap : ajouter GeneratorSpec

**Fichier cible** : Section 5, table recap modifications

Modifier la ligne SubmarineBase :

Ancien :
```
| SubmarineBase.h/.cpp | 3 | Ajout SubFlood + GeneratedDefinition |
```

Nouveau :
```
| SubmarineBase.h/.cpp | 3 | Ajout GeneratorSpec + SubFlood + GeneratedDefinition + GeneratedGeometry. BeginPlay : generation conditionnelle + init flood + init geometry |
```

---

## Resume des 11 corrections

| # | Type | Description |
|---|------|-------------|
| 1 | Diagramme | Ajouter MeshBuilder + GeneratedGeometryComponent au diagramme architecture |
| 2 | Table fichiers | Ajouter SubmarineMeshBuilder.h/.cpp, renumeroter |
| 3 | Table modifs | Ajouter GeneratorSpec sur SubmarineBase |
| 4 | Phase 5 | Separer mesh generation (MeshBuilder) du generateur (topology) |
| 5 | Phase 5D | Corriger references 5B/5C -> Phase 5 + Phase 3 |
| 6 | Phase 5D | Corriger reference 5B dans "Entree" |
| 7 | Phase 5D + Risque 5 | Unifier collision strategy : convex decomposition |
| 8 | Global | Corriger 10 phases -> 8 phases |
| 9 | Risques | Ajouter risque 7 : dependance mesh structs dans header legacy |
| 10 | Phase 3 | Ajouter invocation runtime (BeginPlay chain) |
| 11 | Section 5 recap | Mettre a jour la ligne SubmarineBase |

---

## Verification post-application

- [ ] Aucune reference a Phase 5B ou 5C dans le document
- [ ] SubmarineMeshBuilder apparait dans le diagramme ET dans la table fichiers
- [ ] Le point d'invocation (quand Generate + BuildMeshData sont appeles) est explicite
- [ ] La strategie collision est coherente entre Phase 5D et Risque 5
- [ ] Le comptage de phases est correct (8)
- [ ] Toute la chaine est tracable : Spec -> Generate -> BuildMeshData -> BuildFromDefinition -> PMC visibles
