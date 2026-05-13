# Sub3D — Plan de refonte contrôlée du générateur worldgen vers cavernes + sous-réseaux PCE

**Date** : 2026-05-13  
**Statut** : plan technique, non implémenté  
**Auteur** : assistant technique  
**Scope** : `Source/Sub3D/WorldGen/` uniquement, sauf ajustements de data assets associés  
**Plan authority-max projet** : `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md` reste prioritaire pour le First Playable. Ce plan concerne le pivot environnement abyssal et ne doit pas réouvrir les systèmes sous-marin/flood/crew.

---

## 0. Décision

La bonne approche est de **modifier l'algorithme existant**, pas de repartir de zéro.

La chaîne actuelle a déjà les bons points d'ancrage :

```text
ATraversalRouteActor::RunPipeline
  C4 UTraversalTopologyGenerator
  C5 USkeletonResolver
  C6 UNavigableVolumeGenerator
  C7 UOrganicDeformationGenerator
  C8 URouteSemanticGenerator
  C9 URouteValidator
  C10 URouteMeshBuilder
```

Ce qu'il faut changer n'est pas l'orchestration générale. Il faut changer ce que produisent les étapes C4, C6, C7 et C9.

Objectif : passer de :

```text
route tubulaire sub-navigable + branches optionnelles
```

à :

```text
G_sub  = grandes chambres + galeries sub-navigables
G_PCE  = sous-réseau de passages cavités étroites non sub-navigables
Field  = morphologie organique + variation géologique
```

---

## 1. Analyse du code actuel

### 1.1 Ce que l'algo produit aujourd'hui

La route actuelle visible dans les screenshots correspond exactement au modèle actuel :

- C4 génère une topologie de route avec `spine`, `split`, `merge`, optional branches.
- C5 transforme les edges en segments Bézier.
- C6 transforme ces segments en capsules SDF.
- C6 ajoute des sphères aux jonctions.
- C7 applique un bruit qui agrandit le volume mais ne peut jamais le refermer.
- C10 extrait le mesh par Marching Cubes.

Conséquence visuelle : tubes continus, boucles très graphes, jonctions rondes, peu de sensation de roche/caverne.

### 1.2 Points précis dans le code

| Sujet | Fichier | Observation |
|---|---|---|
| Topologie route | `TraversalTopologyGenerator.cpp` | `BuildConstrainedGraphTopology()` construit un spine puis des optional branches. |
| Axe trop linéaire | `TraversalTopologyGenerator.cpp` | `ComputeNodePositionAtT()` pose `X = T * RouteLengthCm`. Le monde est d'abord une route le long de X. |
| Branches trop limitées | `WorldGenTypes.h` | `FTraversalComplexityBudget` limite `MaxOptionalBranchCount`, `MaxReconnectCount`, `MaxNodeBudget`. |
| Pas de PCE fins | `BranchProfileDataAsset.h` | `TargetRadiusCm` a `ClampMin="1000.0"`. |
| Volume tubulaire | `NavigableVolumeGenerator.cpp` | Segments convertis en `CapsuleCorridor`. |
| Jonctions sphériques | `NavigableVolumeGenerator.cpp` | `JunctionBrush` est un `SpherePocket` de rayon `NodeRadius * 1.1`. |
| Ellipsoïde non réel | `NavigableVolumeGenerator.cpp` | `EllipsoidChamber` est évalué comme sphère avec `HalfExtents.GetMax()`. |
| Organic pass unidirectionnel | `OrganicDeformationGenerator.cpp` | `NewD = FMath::Max(DBase, NoiseCave)`. Il agrandit uniquement. |
| Sonar/nav sub uniquement | `NavigableVolumeGenerator.cpp` | `SonarField` est rasterisé avec `GuaranteedBrushes` seulement. C'est utile à conserver pour le sub. |
| Validation sub globale | `RouteValidator.cpp` | La clearance utilise `Spec.Envelope.MinTurnRadius`, incompatible avec PCE. |
| Pipeline déjà bon | `TraversalRouteActor.cpp` | `RunPipeline()` est clair et ne doit pas être réécrit. |

---

## 2. Cause des formes actuelles

La forme actuelle vient principalement de C6, pas de C10.

`URouteMeshBuilder` extrait ce qu'on lui donne. Le Marching Cubes n'est pas la raison principale du look "tube". Le champ d'entrée est lui-même composé de capsules et de sphères.

Conclusion : **ne pas remplacer Marching Cubes en premier**. Il faut d'abord produire un meilleur champ.

Root cause détaillée :

1. **Les edges sont des capsules de rayon quasi constant.**
2. **Les chambres sont des sphères ou des sphères approximées.**
3. **Les junctions ajoutent encore des sphères.**
4. **Le bruit C7 ajoute des cavités secondaires mais ne contrôle pas la topologie.**
5. **Les optional branches sont pensées comme chemins ou poches, pas comme réseau EVA/PCE.**

---

## 3. Architecture cible

### 3.1 Garder l'orchestration

Ne pas casser :

```cpp
bool ATraversalRouteActor::RunPipeline(
	const FRouteGenSpec& Spec,
	const FRouteSeedCascade& Seeds,
	bool bSpawnVisualMesh);
```

Le pipeline garde :

- logs existants ;
- hash de build ;
- baked mesh path ;
- `UTunnelNavDataAsset` ;
- `USonarFieldComponent` ;
- validation ;
- debug route actor.

### 3.2 Ajouter un mode de génération cave

Ajouter un mode de topologie au niveau `URouteArchetypeDataAsset`, par exemple :

```cpp
UENUM(BlueprintType)
enum class ERouteGenerationPattern : uint8
{
	LegacyRoute,
	ConstrainedRoute,
	SplitMergeRoute,
	MultiStageSplitRoute,
	CaveSystem
};
```

Puis dans `URouteArchetypeDataAsset` :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Generation")
ERouteGenerationPattern GenerationPattern = ERouteGenerationPattern::ConstrainedRoute;
```

Raison : éviter d'empiler un nouveau booléen `bUseCaveSystemPattern` au-dessus des booléens déjà présents. Le data asset contient déjà beaucoup de modes legacy. Un enum rend le choix visible et testable.

Migration minimale : ne pas supprimer les booléens existants au premier patch. L'enum peut piloter le nouveau mode, les booléens restent pour compatibilité.

---

## 4. Changements de données nécessaires

### 4.1 Types de passage

Ajouter une distinction explicite entre navigation sub, PCE, squeeze et fissure décorative.

Dans `WorldGenTypes.h` :

```cpp
UENUM(BlueprintType)
enum class ECavePassageClass : uint8
{
	SubMain,
	SubChamber,
	PCE,
	Squeeze,
	DecorativeFissure
};
```

Dans `FTraversalTopologyNode` :

```cpp
UPROPERTY(BlueprintReadOnly)
ECavePassageClass PassageClass = ECavePassageClass::SubMain;
```

Dans `FTraversalSkeletonSegment` :

```cpp
UPROPERTY(BlueprintReadOnly)
ECavePassageClass PassageClass = ECavePassageClass::SubMain;
```

Dans `FVolumeBrushDef` :

```cpp
UPROPERTY(BlueprintReadOnly)
ECavePassageClass PassageClass = ECavePassageClass::SubMain;
```

Pourquoi : les booléens actuels `bGuaranteedPath`, `bIsOptionalSideContent`, `bDecorativeOnly` ne suffisent pas. Un PCE est traversable par crew, non traversable par sub. Ce n'est ni un simple optional branch, ni une cavité décorative.

### 4.2 Brush shapes

Étendre `EVolumeBrushType`.

Actuel :

```cpp
enum class EVolumeBrushType : uint8
{
	CapsuleCorridor,
	SpherePocket,
	EllipsoidChamber
};
```

Cible :

```cpp
UENUM(BlueprintType)
enum class EVolumeBrushType : uint8
{
	CapsuleCorridor,
	TaperedCapsuleCorridor,
	SpherePocket,
	EllipsoidChamber,
	CompoundChamber,
	FissureRibbon
};
```

Phase 1 peut se limiter à `TaperedCapsuleCorridor` et un vrai `EllipsoidChamber`.

### 4.3 Cave system settings

Ajouter un groupe de settings à `RouteArchetypeDataAsset.h`.

Snippet proposé :

```cpp
USTRUCT(BlueprintType)
struct FCaveSystemAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="3", ClampMax="24"))
	int32 SubChamberCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1200.0"))
	float SubGalleryRadiusCm = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="2000.0"))
	float MinChamberRadiusCm = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="3000.0"))
	float MaxChamberRadiusCm = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="256"))
	int32 PCENodeCount = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="50.0"))
	float PCERadiusCm = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="20.0"))
	float SqueezeRadiusCm = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0"))
	float PCECycleChance = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0"))
	float PCEBridgeChance = 0.45f;
};
```

Dans `URouteArchetypeDataAsset` :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|CaveSystem")
FCaveSystemAuthoringSettings CaveSystem;
```

---

## 5. C4 — nouvelle topologie cave

### 5.1 Nouveau chemin

Dans `TraversalTopologyGenerator.h` :

```cpp
bool BuildCaveSystemTopology(const FRouteGenSpec& Spec,
                             const FRouteSeedCascade& Seeds,
                             const URouteArchetypeDataAsset* Archetype,
                             TArray<FTraversalTopologyNode>& OutNodes) const;
```

Dans `GenerateTopology()` :

```cpp
if (Archetype && Archetype->GenerationPattern == ERouteGenerationPattern::CaveSystem)
{
	return BuildCaveSystemTopology(Spec, Seeds, Archetype, OutNodes);
}
```

### 5.2 Structure générée

`BuildCaveSystemTopology()` doit générer deux couches :

```text
G_sub:
  nodes = start + chambers + end
  edges = MST + quelques loops contrôlés
  radius = sub/chamber
  validation = sub clearance

G_PCE:
  nodes = points denses autour/entre chambers
  edges = k-nearest borné + cycle budget
  radius = PCE / squeeze
  validation = PCE, pas sub

bridges:
  edges rares entre chamber et PCE
```

### 5.3 Important : ne pas forcer PCE dans `bGuaranteedPath`

Pour les segments PCE :

```cpp
Node.bIsCanonicalPath = false;
Node.bIsOptionalSideContent = true;
Node.PassageClass = ECavePassageClass::PCE;
```

Pour les segments sub :

```cpp
Node.bIsCanonicalPath = true;
Node.bIsOptionalSideContent = false;
Node.PassageClass = ECavePassageClass::SubMain; // ou SubChamber
```

Le but est de conserver le sonar/nav sub garanti sans faire grossir les PCE.

### 5.4 Pseudo-code C4

```cpp
bool UTraversalTopologyGenerator::BuildCaveSystemTopology(
	const FRouteGenSpec& Spec,
	const FRouteSeedCascade& Seeds,
	const URouteArchetypeDataAsset* Archetype,
	TArray<FTraversalTopologyNode>& OutNodes) const
{
	FRandomStream Rng(Seeds.TopologySeed);
	OutNodes.Reset();

	// 1. Generate sparse sub chambers.
	TArray<int32> SubNodeIDs;
	GenerateSubChambers(Spec, Archetype->CaveSystem, Rng, OutNodes, SubNodeIDs);

	// 2. Connect G_sub with MST + limited loops.
	ConnectSubGraph(Archetype->CaveSystem, Rng, OutNodes, SubNodeIDs);

	// 3. Generate dense PCE nodes near chamber shells and between chambers.
	TArray<int32> PCENodeIDs;
	GeneratePCENodes(Spec, Archetype->CaveSystem, Rng, OutNodes, SubNodeIDs, PCENodeIDs);

	// 4. Connect PCE graph with k-nearest + cycle budget.
	ConnectPCEGraph(Archetype->CaveSystem, Rng, OutNodes, PCENodeIDs);

	// 5. Add rare bridges from chambers to PCE.
	AddPCEBridges(Archetype->CaveSystem, Rng, OutNodes, SubNodeIDs, PCENodeIDs);

	return OutNodes.Num() > 2;
}
```

Ces helpers peuvent d'abord vivre en `static` dans `TraversalTopologyGenerator.cpp`. Ne pas créer trop de classes avant que la forme soit validée.

---

## 6. C5 — propagation des classes de passage

Dans `USkeletonResolver::BuildSegment()` :

```cpp
Seg.PassageClass = A.PassageClass;
if (A.PassageClass != B.PassageClass)
{
	// Bridge: prefer the narrower class for geometry and validation behavior.
	Seg.PassageClass = B.PassageClass;
}
```

À préciser :

- Un segment `SubMain -> SubChamber` reste sub.
- Un segment `SubChamber -> PCE` devient bridge/PCE.
- Un segment `PCE -> PCE` reste PCE.
- Un segment `Squeeze -> PCE` peut rester squeeze si l'un des deux nodes est squeeze.

Snippet plus strict :

```cpp
static ECavePassageClass ResolveSegmentPassageClass(
	ECavePassageClass A,
	ECavePassageClass B)
{
	if (A == ECavePassageClass::DecorativeFissure || B == ECavePassageClass::DecorativeFissure)
	{
		return ECavePassageClass::DecorativeFissure;
	}
	if (A == ECavePassageClass::Squeeze || B == ECavePassageClass::Squeeze)
	{
		return ECavePassageClass::Squeeze;
	}
	if (A == ECavePassageClass::PCE || B == ECavePassageClass::PCE)
	{
		return ECavePassageClass::PCE;
	}
	if (A == ECavePassageClass::SubChamber || B == ECavePassageClass::SubChamber)
	{
		return ECavePassageClass::SubChamber;
	}
	return ECavePassageClass::SubMain;
}
```

---

## 7. C6 — changer les volumes

### 7.1 Ne plus représenter tout en capsules uniformes

Dans `UNavigableVolumeGenerator::GenerateBrushes()`, la conversion actuelle :

```cpp
Brush.BrushType = EVolumeBrushType::CapsuleCorridor;
Brush.Radius = MaxR;
```

doit devenir :

```cpp
Brush.PassageClass = Seg.PassageClass;

switch (Seg.PassageClass)
{
case ECavePassageClass::SubMain:
	Brush.BrushType = EVolumeBrushType::TaperedCapsuleCorridor;
	break;
case ECavePassageClass::SubChamber:
	Brush.BrushType = EVolumeBrushType::EllipsoidChamber;
	break;
case ECavePassageClass::PCE:
case ECavePassageClass::Squeeze:
	Brush.BrushType = EVolumeBrushType::TaperedCapsuleCorridor;
	Brush.bAffectsSonarField = false;
	break;
case ECavePassageClass::DecorativeFissure:
	Brush.BrushType = EVolumeBrushType::FissureRibbon;
	Brush.bAffectsSonarField = false;
	Brush.bGuaranteedTraversal = false;
	break;
}
```

### 7.2 Vrai ellipsoid SDF

Remplacer l'approximation actuelle :

```cpp
SDF = SphereSDF(P, Brush.CenterA, Brush.HalfExtents.GetMax());
```

par une approximation d'ellipsoïde.

Snippet :

```cpp
static float EllipsoidSDF(const FVector& P, const FVector& Center, const FVector& Radii)
{
	const FVector SafeRadii(
		FMath::Max(Radii.X, 1.f),
		FMath::Max(Radii.Y, 1.f),
		FMath::Max(Radii.Z, 1.f));

	const FVector Q = P - Center;
	const FVector K0(Q.X / SafeRadii.X, Q.Y / SafeRadii.Y, Q.Z / SafeRadii.Z);
	const FVector K1(Q.X / (SafeRadii.X * SafeRadii.X),
	                 Q.Y / (SafeRadii.Y * SafeRadii.Y),
	                 Q.Z / (SafeRadii.Z * SafeRadii.Z));

	const float K0Size = K0.Size();
	const float K1Size = K1.Size();
	if (K1Size < KINDA_SMALL_NUMBER)
	{
		return -SafeRadii.GetMin();
	}

	return K0Size * (K0Size - 1.f) / K1Size;
}
```

### 7.3 Tapered capsule SDF

Le code actuel évalue une capsule de rayon constant. Pour les galeries et PCE, il faut permettre des rayons Start/End ou une modulation.

Option minimale : ajouter `RadiusA` et `RadiusB` dans `FVolumeBrushDef`.

```cpp
UPROPERTY(BlueprintReadOnly)
float RadiusA = 3000.f;

UPROPERTY(BlueprintReadOnly)
float RadiusB = 3000.f;
```

Snippet SDF :

```cpp
static float TaperedCapsuleSDF(
	const FVector& P,
	const FVector& A,
	const FVector& B,
	float RadiusA,
	float RadiusB)
{
	const FVector AB = B - A;
	const float Len2 = AB.SizeSquared();
	const float T = Len2 > KINDA_SMALL_NUMBER
		? FMath::Clamp(FVector::DotProduct(P - A, AB) / Len2, 0.f, 1.f)
		: 0.f;

	const FVector Closest = A + AB * T;
	const float Radius = FMath::Lerp(RadiusA, RadiusB, T);
	return (P - Closest).Size() - Radius;
}
```

Phase 1 : `RadiusA = PrevRadius`, `RadiusB = CurR`.

### 7.4 Séparer RenderField, SonarField, PCEField

Le code a déjà :

```cpp
RenderField = toutes les brushes render
SonarField  = guaranteed-only
```

Conserver cette idée.

Ajouter plus tard :

```cpp
TMap<FFieldChunkCoord, FRouteFieldChunkData> PCEField;
```

Mais phase 1 peut éviter `PCEField`. On peut d'abord :

- inclure PCE dans `RenderField`;
- exclure PCE de `SonarField`;
- exclure PCE de la validation sub.

---

## 8. C7 — remplacer Organic-only par morphology pass

### 8.1 Garder l'invariant pour le sub

L'invariant actuel :

```cpp
const float NewD = FMath::Max(DBase, NoiseCave);
```

est bon pour ne pas bloquer la route sub. Il doit rester pour les zones garanties.

Mais il ne suffit pas pour la forme visuelle. Il ne peut pas créer :

- squeezes ;
- zones dures ;
- parois qui suivent des faults ;
- étranglements naturels ;
- PCE qui lisent comme des fissures.

### 8.2 Ajouter un mode de morphologie render-only

Renommer conceptuellement C7 :

```text
OrganicDeformationGenerator
→ CaveMorphologyGenerator
```

Pas besoin de renommer le fichier au premier patch. Ajouter une fonction nouvelle :

```cpp
bool ApplyCaveMorphology(const FRouteGenSpec& Spec,
                         const FRouteSeedCascade& Seeds,
                         const UBiomeFieldProfileDataAsset* Biome,
                         const TArray<FTraversalSkeletonSegment>& Skeleton,
                         FRouteFieldModel& InOutField) const;
```

Principe :

```cpp
if (SampleIsNearGuaranteedSubPath)
{
	Density = FMath::Max(Density, OrganicOpenSignal);
}
else
{
	Density += MorphologyDisplacement; // peut creuser ou durcir localement
}
```

Phase 1 prudente :

- ne pas fermer le sub path ;
- autoriser modulation render-only dans les chunks adjacents ;
- logs de min/max displacement.

### 8.3 Champ de conductance simple

Ajouter dans `UBiomeFieldProfileDataAsset` des paramètres simples :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave|Substrate")
float FaultFrequency = 1.f / 18000.f;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave|Substrate")
float FaultStrength = 0.35f;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave|Substrate")
float StratificationStrength = 0.25f;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave|Substrate")
float PCEConductanceBias = 0.5f;
```

Ne pas implémenter tout de suite un vrai modèle géologique complet. Pour la première passe, un champ simple suffit à pousser les PCE vers des bandes cohérentes.

---

## 9. C9 — validation séparée Sub / PCE

Le validateur actuel doit rester strict pour `G_sub`.

Ajouter à `FRouteValidationReport` :

```cpp
UPROPERTY(BlueprintReadOnly)
int32 PCENodeCount = 0;

UPROPERTY(BlueprintReadOnly)
int32 PCEEdgeCount = 0;

UPROPERTY(BlueprintReadOnly)
int32 PCEConnectedComponents = 0;

UPROPERTY(BlueprintReadOnly)
int32 PCECycleCount = 0;

UPROPERTY(BlueprintReadOnly)
float PCEAverageDegree = 0.f;

UPROPERTY(BlueprintReadOnly)
bool bPCEPass = true;
```

Validation PCE :

```text
PCE nodes > minimum
PCE components <= max allowed
PCE average degree between 2.0 and 3.8
PCE cycle count >= minimum
at least one bridge to G_sub
no sub clearance requirement
```

Important : ne pas faire échouer une route parce qu'un PCE a moins que `MinTurnRadius`. C'est voulu.

---

## 10. C10 — mesh extraction

Ne pas remplacer `URouteMeshBuilder` en première passe.

Raison :

- le défaut visuel principal vient des brushes ;
- Marching Cubes est déjà branché ;
- changer l'extracteur en même temps rendrait le diagnostic impossible.

À surveiller :

- voxel render actuel = 200 cm ;
- PCE à 80-180 cm ne peut pas être correctement représenté à 200 cm ;
- donc phase PCE mesh réel demandera une résolution locale plus fine.

Plan :

1. Phase 1 : PCE visible par debug lines / render-only grossier.
2. Phase 2 : chunk voxel size configurable par archetype.
3. Phase 3 : local high-res autour PCE si nécessaire.

Ajout possible dans `FRouteSurfaceBuildSettings` :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="25.0"))
float RenderVoxelSizeCm = 200.f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="25.0"))
float PCEVoxelSizeCm = 75.f;
```

Ne pas activer `PCEVoxelSizeCm` avant que la topologie PCE soit validée.

---

## 11. Ordre d'implémentation

### Phase A — Instrumentation et garde-fous

Fichiers :

- `WorldGenTypes.h`
- `TraversalRouteActor.cpp`
- `RouteValidator.cpp`

Actions :

1. Ajouter `ECavePassageClass`.
2. Ajouter compteurs PCE dans `FRouteValidationReport`.
3. Ajouter logs C4 :

```text
[CaveC4] SubNodes=N SubEdges=N PCENodes=N PCEEdges=N Bridges=N
[CaveC4] PCEAvgDegree=X Components=Y Cycles=Z
```

Validation :

- build compile ;
- route legacy inchangée ;
- logs affichent 0 PCE en mode legacy.

### Phase B — CaveSystem C4 sans changer C6

Fichiers :

- `RouteArchetypeDataAsset.h/.cpp`
- `TraversalTopologyGenerator.h/.cpp`
- `WorldGenTypes.h`
- `SkeletonResolver.cpp`

Actions :

1. Ajouter `ERouteGenerationPattern`.
2. Ajouter `FCaveSystemAuthoringSettings`.
3. Ajouter `BuildCaveSystemTopology()`.
4. Propager `PassageClass` dans le skeleton.
5. PCE peut être généré mais rendu avec les mêmes capsules au début.

Validation :

- le graphe ne ressemble plus à un simple spine X ;
- `G_sub` garde une route start/end ;
- PCE a des loops et bridges.

### Phase C — Brushes cavernes

Fichiers :

- `WorldGenTypes.h`
- `NavigableVolumeGenerator.h/.cpp`
- `TunnelNavDataBuilder.cpp`

Actions :

1. Ajouter vrai ellipsoid SDF.
2. Ajouter tapered capsule.
3. Changer les junctions : pour `SubChamber`, utiliser compound/ellipsoid, pas sphère unique.
4. Exclure PCE de `SonarField`.

Validation :

- la forme n'est plus une succession de tubes ;
- les chambers sont allongées/irrégulières ;
- le sub nav reste valide.

### Phase D — Morphology pass

Fichiers :

- `BiomeFieldProfileDataAsset.h`
- `OrganicDeformationGenerator.h/.cpp`
- `TraversalRouteActor.cpp`

Actions :

1. Ajouter paramètres substrate simples.
2. Ajouter `ApplyCaveMorphology()`.
3. Garder `ApplyDeformation()` legacy pour compatibilité.
4. Appeler la nouvelle fonction seulement en `CaveSystem`.

Validation :

- parois moins cylindriques ;
- aucune fermeture du chemin sub ;
- PCE visuellement mieux intégré.

### Phase E — Validation PCE

Fichiers :

- `RouteValidator.h/.cpp`
- `WorldGenTypes.h`

Actions :

1. Calculer composants PCE.
2. Calculer degré moyen.
3. Calculer cycles approximatifs : `E - V + Components`.
4. Ne pas appliquer `MinTurnRadius` aux PCE.

Validation :

- une route sans PCE fail si mode CaveSystem exige PCE ;
- une route PCE spaghetti fail ;
- une route PCE connectée passe.

---

## 12. Snippets clés

### 12.1 Génération PCE minimale

```cpp
struct FCandidateEdge
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;
	float DistanceCm = 0.f;
	float Cost = 0.f;
};

static float ScorePCEEdge(const FVector& A, const FVector& B, const FRandomStream& Rng)
{
	const float Distance = FVector::Dist(A, B);
	const float VerticalPenalty = FMath::Abs(A.Z - B.Z) * 0.25f;
	return Distance + VerticalPenalty;
}
```

Puis :

```cpp
// k-nearest borné, phase 1. Delaunay peut venir plus tard.
for (int32 I = 0; I < PCENodeIDs.Num(); ++I)
{
	TArray<FCandidateEdge> Candidates;
	for (int32 J = 0; J < PCENodeIDs.Num(); ++J)
	{
		if (I == J) continue;
		const FVector A = OutNodes[PCENodeIDs[I]].WorldPosition;
		const FVector B = OutNodes[PCENodeIDs[J]].WorldPosition;
		const float D = FVector::Dist(A, B);
		if (D > Settings.PCEMaxEdgeLengthCm) continue;
		Candidates.Add({PCENodeIDs[I], PCENodeIDs[J], D, D});
	}
	Candidates.Sort([](const FCandidateEdge& L, const FCandidateEdge& R)
	{
		return L.Cost < R.Cost;
	});

	for (int32 K = 0; K < FMath::Min(3, Candidates.Num()); ++K)
	{
		AddDirectedConnection(OutNodes, Candidates[K].A, Candidates[K].B);
	}
}
```

### 12.2 PCE cycle count

```cpp
PCECycleCount = FMath::Max(0, PCEEdgeCount - PCENodeCount + PCEConnectedComponents);
```

Ce n'est pas une preuve complète de qualité, mais c'est un bon indicateur de non-arbre.

### 12.3 Exclusion PCE du sonar field

Dans `GenerateBrushes()` :

```cpp
Brush.bAffectsSonarField =
	Seg.PassageClass == ECavePassageClass::SubMain ||
	Seg.PassageClass == ECavePassageClass::SubChamber;
```

Dans `BuildGuaranteedVolume()` :

```cpp
TArray<FVolumeBrushDef> SonarBrushes;
for (const FVolumeBrushDef& Brush : OutField.GuaranteedBrushes)
{
	if (Brush.bAffectsSonarField)
	{
		SonarBrushes.Add(Brush);
	}
}
RasterizeField(SonarBrushes, Bounds, 500.f, 8, OutField.SonarField);
```

---

## 13. Tests recommandés

Créer des automation tests dans `Source/Sub3DTests/Private/Automation/WorldGen/`.

Tests minimum :

1. `CaveSystem_GeneratesSubAndPCE`
   - mode CaveSystem ;
   - `PCENodeCount > 0` ;
   - `PCEEdgeCount > PCENodeCount`.

2. `CaveSystem_SubPathRemainsConnected`
   - start/end connectés par `SubMain/SubChamber` seulement.

3. `CaveSystem_PCEHasCycles`
   - `PCECycleCount >= 1` sur seed fixe.

4. `CaveSystem_PCEDoesNotRequireSubClearance`
   - PCE radius < `MinTurnRadius` ;
   - validation globale passe si sub path passe.

5. `LegacyRoute_Unchanged`
   - même seed legacy ;
   - node count / validation reste stable.

---

## 14. Risques

| Risque | Niveau | Mitigation |
|---|---:|---|
| Casser les routes legacy | Élevé | Enum mode + tests legacy. |
| PCE trop fin pour voxel 200 cm | Élevé | D'abord debug graph, puis voxel size configurable. |
| Validation mélange sub/PCE | Élevé | `ECavePassageClass` obligatoire avant C6/C9. |
| Performance build explose | Moyen | k-nearest borné d'abord, Delaunay plus tard. |
| Sonar/nav pollué par PCE | Moyen | Exclure PCE du `SonarField` au départ. |
| Trop gros refactor de data assets | Moyen | Ajouter champs, ne pas supprimer legacy au premier patch. |

---

## 15. Recommandation finale

Implémenter en premier :

```text
Phase A + Phase B
```

Ne pas toucher encore à Marching Cubes. Ne pas implémenter le vrai substrate complet. Ne pas faire Delaunay. Ne pas faire Manifold Dual Contouring.

La première preuve doit être :

```text
avec le pipeline existant,
une route CaveSystem génère une forme qui lit comme réseau de cavernes,
avec G_sub lisible pour Craniata,
et G_PCE séparé, intriqué, non sub-navigable.
```

Si cette preuve est bonne, C6/C7 peuvent ensuite transformer la silhouette en vraie caverne.

---

## 16. Définition de fini pour la première passe

La première passe est finie quand :

1. un `URouteArchetypeDataAsset` peut sélectionner `CaveSystem`;
2. une route se build sans toucher aux chemins legacy;
3. les logs affichent `SubNodes`, `SubEdges`, `PCENodes`, `PCEEdges`, `Bridges`;
4. le Craniata a une route sub claire;
5. le réseau PCE est visible en debug ou render-only;
6. la validation sub passe;
7. les PCE ne sont pas forcés à la clearance sub;
8. le résultat ne ressemble plus principalement à une ligne de tubes.

