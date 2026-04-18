# Audit complet : Sous-marin, Generateur, Meshes
**Date :** 2026-04-12

## Vue d'ensemble des deux pipelines

| | Pipeline Bake (legacy) | Pipeline Generator (actuel) |
|---|---|---|
| **Module** | Sub3DBake (Editor) | Sub3D (Runtime) |
| **Input** | `USubmarineAuthoringAsset` (hierarchique, multi-couches) | `USubmarineGeneratorSpec` + `USubmarineGeneratorEnvelopeDef` (2 assets plats) |
| **Process** | 9 waves, 12+ services de bake | 7 etapes, 1 generateur stateless |
| **Output** | `UCompiledSubmarineBaseAsset` -> `UCompiledSubmarineRuntimeAsset` | `USubmarineDefinition` (source de verite runtime) |
| **Mesh** | `FCompiledMeshSection` (donnees pre-bakees) | `FSubmarineMeshSectionData` -> `ProceduralMeshComponent` |
| **Airlock/SAS** | Aucun | Topologie complete, **visuel manquant** |
| **Statut** | Stable, complet dans son scope. Conserve pour Proto03/04 | Actif pour First Playable |

---

## 1. Pipeline Bake (legacy) -- Ce qui marchait bien

Le pipeline `SubmarineAuthoringAsset` -> `SubmarineBakeSubsystem` -> `CompiledSubmarineRuntimeAsset` est **stable et complet** dans son scope :

**9 waves fonctionnelles :**
1. Validation geometrie coque
2. Sequence d'anneaux (interpolation Hermite cubique) + bake coque
3. Resolution bays structurels + enveloppe exterieure
4. Niveaux de ponts + regions de sol
5. _(skip)_
6. Ouvertures, connecteurs, fermetures
7. Partitions (cloisons etanches, murs)
8. Graphe d'inondation (volumes derives des bays)

**Forces :**
- Profils parametriques (Myring, Series 58, Superellipse, Uniform)
- Validation exhaustive (walkabilite, clearance >180cm, largeur >100cm)
- Hash de staleness (detecte quand Layer B/C est invalide par un changement Layer A)
- 0 TODO/FIXME/HACK dans tout le module
- Mesh detaille : 32 segments radiaux (rendu), 12 (collision), caps bow/stern

**Limite fondamentale :** Aucune gestion d'airlock. L'enum `ESub3DRoomTag::Airlock` existe dans `Sub3DCanonicalEnums.h` mais n'est jamais assigne pendant le bake. `FDerivedFloodVolume::RoomTags` reste vide.

---

## 2. Pipeline Generator (actuel) -- Etat des lieux

Le pipeline `SubmarineGeneratorSpec` -> `SubmarineGenerator::Generate()` -> `SubmarineDefinition` est **fonctionnel et deterministe** :

**7 etapes :**
1. **ResolveEnvelope** -- Metriques coque (longueur, rayon, volume, masse)
2. **DeriveCompartments** -- Boundaries des cloisons -> compartiments (Helm, Engine, Crew)
3. **GenerateConnections** -- 1 porte par cloison entre compartiments adjacents
4. **GenerateAirlock** -- 1 compartiment + 2 connexions (inner + outer)
5. **BuildFloodGraph** -- Volumes 1:1 avec compartiments, edges par connexion
6. **PlaceStations** -- Helm/Engine/Crew stations dans leurs compartiments
7. **PlaceSpawns** -- Points d'apparition Pilot + Crew

Puis :
- `USubmarineMeshBuilder::BuildMeshData()` -> remplit `Definition.ExteriorHullMesh`, `InteriorMeshes[]`, `BulkheadMeshes[]`
- `USubmarineGeneratedGeometryComponent::BuildFromDefinition()` -> cree les PMC (render + collision)

**Ce qui fonctionne :**
- Generation deterministe bout-en-bout (meme Spec = meme Definition)
- Coque exterieure en superellipse avec caps bow/stern
- Interieurs par compartiment (murs, sols, caps)
- Cloisons (bulkheads) avec decoupes de portes
- Collision exterieure par decomposition convexe (N slices)
- Collision interieure (sols marchables, profil `SubInteriorWalkable`)
- Integration flood (`SubFlood->InitializeFromDefinition`)
- Spawn de portes (`SpawnDoorsFromDefinition` via `SpawnActorDeferred`)
- Spawn de stations depuis Definition

---

## 3. Le probleme du SAS -- Diagnostic precis

**Le SAS est architecturalement complet mais visuellement absent.**

| Couche | Statut | Detail |
|---|---|---|
| Specification | OK | `AirlockPositionNormalized`, `AirlockSide` dans le Spec |
| Topologie | OK | `GenerateAirlock()` cree compartiment + 2 connexions |
| Flood | OK | Le graphe d'inondation integre le SAS automatiquement |
| Mesh interieur | Partiel | Murs/sol du SAS generes (box 120x100x200cm) si `bBuildAirlock=true` |
| **Mesh exterieur** | **MANQUANT** | Le hull builder filtre explicitement le SAS (`SemanticType != Airlock`) |
| **Bulkhead** | **MANQUANT** | `BuildBulkheads()` skip explicitement les connexions du SAS |
| **Spawning visuel** | **MANQUANT** | Aucun code ne spawn un acteur visuel a la position du SAS |

**Le code qui cause le gap :**

`SubmarineMeshBuilder.cpp` -- le SAS est exclu de la coque :
```cpp
// Hull exterieur : skip airlock
for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
    if (Comp.SemanticType != ESubCompartmentType::Airlock)
        // ... genere la geometrie hull
```

`SubmarineMeshBuilder.cpp` -- les bulkheads du SAS sont skipes :
```cpp
// Bulkheads : skip airlock connections
if (CompA is Airlock || CompB is Airlock)
    continue;
```

**Consequence en PIE :** Le SAS est un volume invisible. Le flood le voit, mais le joueur ne peut ni le voir ni interagir avec.

---

## 4. Resume des differences Bake vs Procedural pour les meshes

| Aspect | Bake (legacy) | Procedural (actuel) |
|---|---|---|
| Vertices coque | ~2k-5k (32 radial x rings) | ~900-1000 (32 radial x subdivisions) |
| Collision | Tri-mesh proxy (12 segments) | Convex decomposition par slices (2-16) |
| UVs | U=longitudinal, V=circumferentiel | Idem |
| Materiaux | Non assignes | Non assignes (PMC blanc par defaut) |
| Sols | Bakes avec geometrie complete | Quad simple par compartiment |
| Cloisons | Non generees | Panels avec decoupes de portes |
| Airlock | Aucun | Topologie complete, mesh partiel |
