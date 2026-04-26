# Sub3D — Submarine Generator : Plan Authoritaire (Shape & GeneratorEditor)

Date : 2026-04-08 (dernière révision : 2026-04-10)
Statut : **AUTHORITAIRE**. Ce document remplace les variantes antérieures du plan de génération.
Portée : **correction de la forme uniquement** (hull, caps bow/stern, sol, portes, séparation render/collision). Aucune autre correction, aucune refonte de flood, stations, airlock, spawn, LayoutSolver, StructuralSheets.

Référence visuelle cible : `/Game/Sub3D/Proto04C/CompilerV2/DA_SubmarineAuthoringAsset_Proto04C_1`.

---

## 1. État PIE au 2026-04-10

### 1.1 Ce qui fonctionne et doit rester intact
- Génération de la coque extérieure (forme globale sortie, pas la qualité).
- Sas externe présent sur le flanc.
- Stations spawnées depuis `USubmarineDefinition` : `BP_BallastStation0`, `BP_EngineStation0`, `BP_HelmStation0`.
- `BP_Submarine_FPRun` présent dans le niveau, spawn géré dans `SubmarineBase`.
- Pipeline `USubmarineGeneratorSpec → USubmarineGenerator::Generate → USubmarineDefinition → USubmarineMeshBuilder::BuildMeshData`.
- Types de contrats (`SubmarineDefinitionTypes.h`, `FGeneratedCompartmentDef`, `FGeneratedConnectionDef`, `FGeneratedStationSlotDef`, `FCompiledFloodGraph`).

### 1.2 Ce qui est cassé et entre dans le scope « forme »
1. **Coque extérieure non visible depuis l'extérieur.** Le matériau n'est pas two-sided et/ou le winding des triangles envoie les normales vers l'intérieur sur certaines faces. Observable par : on voit l'intérieur à travers la coque en PIE.
2. **Sol visible uniquement par le dessous.** La section sol dans `BuildInteriorCompartments` n'émet qu'un seul quad orienté `-Z`. Rendre le matériau two-sided ou doubler les triangles.
3. **Portes deviennent pleines dès que `FloorDropBiasCm > 0` ou que `FloorZ > 0`.** Root cause : dans [SubmarineMeshBuilder.cpp](Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp) `BuildBulkheads`, la boucle de cutout filtre les points de l'arc de la section avec `Pt.Y > DoorTop || FMath::Abs(Pt.X) > DoorHalfWidth`. Quand le sol remonte, les points inférieurs de l'arc ne satisfont plus ce filtre et la triangulation fan-from-pivot dégénère, produisant une paroi pleine au lieu d'un cutout porte.
4. **Bow / stern aplatis, incontrôlables.** Le cap est un hard-clamp `FMath::Min(BowRadius * 0.8f, 80.f)` avec un décroissance `cos(T * π/2)` appliquée sur 6 anneaux. Aucun contrôle utilisateur. La forme ne ressemble pas au sous-marin de référence.
5. **Pollution visuelle intérieure.** En vue wireframe, les volumes intérieurs apparaissent triangulés solidement. Cause la plus probable : meshes de collision générés visibles dans l'éditeur (flag visibility sur la proc mesh), ou bulkheads/caps émis deux fois (exterior + interior), ou l'intérieur émet des triangles que la vue gameplay n'est pas supposée voir (ex : dessous du sol, parois extérieures de l'arc intérieur). À investiguer avant de réécrire. La séparation render/collision n'est pas propre.

### 1.3 Ce qui reste hors scope dans ce document
- Correction du LayoutSolver / constraints (`SubCompiler/SubmarineLayoutSolver.cpp`).
- StructuralSheets, rupture visuelle, décks multiples.
- Simulation hydrodynamique avancée.
- Éditeur de sous-marin in-game (joueur).
- Toute migration `SubFloodComponent`, `SubHullComponent`, `SubmarineCompartmentComponent`.

---

## 2. Règles dures (à ne pas discuter)

Ces règles viennent directement de l'utilisateur et sont non-négociables dans ce scope.

1. **Pas de noms de profils d'architecture navale réels.** Pas de `Myring`, `Series58`, `SuperellipseLongitudinal` dans les enums ou les types. On reste sur `EGenBowSternProfile { Rounded, Needle, Blunt, Bulbous, Tapered }` ou une variante générique équivalente. Les fonctions internes peuvent s'inspirer des mathématiques navales mais ne doivent pas exposer ces noms à l'éditeur.
2. **Pas d'appendages dans ce scope.** Pas de sail, pas de bow dome, pas de stern fairing, pas de control surfaces, pas de propulsor. Le contenu de [Sub3DCore/Public/Types/Sub3DAppendageTypes.h](Source/Sub3DCore/Public/Types/Sub3DAppendageTypes.h) reste non utilisé.
3. **Pas de curves exposées à l'éditeur.** `FRuntimeFloatCurve RadiusProfile` dans [SubmarineGeneratorEnvelopeDef.h](Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.h) **est retiré**. Le contrôle de la forme passe par des rings et des paramètres scalaires, pas par des courbes.
4. **Pas de nouvel import depuis `SubCompiler/`, `Sub3DBake/`, `Sub3DCore/AppendageTypes`.** Le nouveau code de cette phase vit uniquement dans `Source/Sub3D/Submarine/Generator/`. Les mathématiques bow/stern sont réécrites localement, pas importées depuis `SubmarineHullProfileService.cpp` ou `SubmarineAppendageBakeService.cpp`. Le couplage déjà existant de [SubmarineDefinition.h](Source/Sub3D/Submarine/Generator/SubmarineDefinition.h) vers `SubCompiler/SubmarineGeometryBuilder.h` pour les structs mesh reste une dette temporaire connue ; il n'est pas étendu dans cette phase.
5. **On ne corrige que la forme.** Tout ce qui touche au flood, à la logique métier stations/spawns/airlock, à la topologie des compartiments, reste strictement identique.
6. **Pas de wording « hybride ».** Termes explicites : `temporaire`, `éditeur`, `runtime`, `debug`, `validation-only`. Pas de « shipyard-adjacent », « editor-grade », etc.

---

## 3. Nouveau contrat d'envelope (shape only)

On garde **exactement** l'enum `EGenBowSternProfile` existant. On garde la paire `BowProfile` / `SternProfile`. On retire `RadiusProfile`. On ajoute un petit groupe de paramètres qui rendent la forme contrôlable sans courbes.

### 3.1 Modifications dans [SubmarineGeneratorEnvelopeDef.h](Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.h)

| Action | Champ | Valeur / Type | Raison |
|---|---|---|---|
| KEEP | `SpineLengthCm` | `float`, default 1520 | Longueur totale du sub. |
| KEEP | `DefaultRadiusCm` | `float`, default 180 | Rayon de référence du corps cylindrique. |
| KEEP | `ExteriorHullOffsetCm` | `float`, default 12 | Offset coque extérieure par rapport à l'intérieur. |
| KEEP | `SectionExponent` | `float`, default 2.0 | Superellipse. |
| KEEP | `WidthToHeightRatio` | `float`, default 1.0 | Aplatissement horizontal/vertical. |
| KEEP | `BowProfile` / `SternProfile` | `EGenBowSternProfile` | Profils génériques, pas de noms navals réels. |
| KEEP | `BowTaperFraction` / `SternTaperFraction` | `float`, default 0.12 / 0.15 | Proportion de la spine dédiée au taper. |
| **REMOVE** | `FRuntimeFloatCurve RadiusProfile` | — | L'utilisateur ne veut pas de curves dans l'éditeur. |
| **ADD** | `BowCapLengthCm` | `float`, default 240, ClampMin 0, ClampMax 2000 | Longueur explicite du cap avant. Remplace le hardcode `FMath::Min(BowRadius * 0.8f, 80.f)`. |
| **ADD** | `SternCapLengthCm` | `float`, default 320, ClampMin 0, ClampMax 2000 | Longueur explicite du cap arrière. |
| **ADD** | `BowSharpness` | `float`, default 1.0, ClampMin 0.1, ClampMax 4.0 | Exposant de décroissance du rayon le long du cap avant. 1.0 = cosinus, >1 = plus pointu, <1 = plus rond. |
| **ADD** | `SternSharpness` | `float`, default 1.0, ClampMin 0.1, ClampMax 4.0 | Idem pour l'arrière. |
| **ADD** | `BodyLengthFraction` | `float`, default 0.55, ClampMin 0.1, ClampMax 0.9 | Fraction de la spine occupée par le corps parallèle (rayon quasi constant) entre les deux tapers. |
| **ADD** | `ControlRingCount` | `int32`, default 9, ClampMin 5, ClampMax 17 | Nombre de rings de contrôle générés par le GeneratorEditor (voir section 7). Pair ou impair accepté. |

Note : `BowCapLengthCm` et `SternCapLengthCm` sont exprimés en cm, pas en fraction, pour que l'utilisateur ait un contrôle direct en unités du monde (cohérent avec le style de l'asset de référence qui expose `Length Cm 7200`, `Max Outer Diameter Cm 1200`).

### 3.2 Modifications dans `SubmarineGeneratorEnvelopeDef.cpp`

- `EvaluateRadius(NormalizedPosition)` : ne lit plus `RadiusProfile`. La formule canonique devient :
  - `RemainingFraction = max(0, 1 - BodyLengthFraction)`.
  - `WeightSum = max(0.001, BowTaperFraction + SternTaperFraction)`.
  - `BowShapeFraction = RemainingFraction * BowTaperFraction / WeightSum`.
  - `SternShapeFraction = RemainingFraction * SternTaperFraction / WeightSum`.
  - `BodyStart = BowShapeFraction`.
  - `BodyEnd = 1 - SternShapeFraction`.
  - Retourne `DefaultRadiusCm` sur toute la portion `[BodyStart, BodyEnd]`.
  - Dans les zones `[0, BodyStart]` et `[BodyEnd, 1]`, applique le profil `EGenBowSternProfile` avec la sharpness correspondante.
  - `BodyLengthFraction` est donc le contrôle autoritaire de la longueur du corps parallèle. `BowCapLengthCm` et `SternCapLengthCm` ne pilotent pas `EvaluateRadius` ; ils pilotent uniquement la géométrie des caps dans `BuildExteriorHull`.
- `EvaluateBowSternTaper(NormalizedPosition)` : applique un multiplicateur `[0..1]` basé sur `BowProfile`, `SternProfile`, `BowSharpness`, `SternSharpness`. Formules internes :
  - `Rounded` : `sin(pi/2 * t)` (où `t` est la distance normalisée du début du taper vers le corps).
  - `Needle` : `t^BowSharpness` (courbe concave, rayon petit longtemps).
  - `Blunt` : `1 - (1-t)^BowSharpness` (courbe convexe, rayon grand rapidement).
  - `Bulbous` : `sin(pi/2 * t) + 0.15 * sin(pi * t)` clampé.
  - `Tapered` : `pow(t, 1/BowSharpness)` (transition linéaire avec biais).
  - Ces formules restent génériques, pas de référence à des profils navals réels.

### 3.3 Modifications dans [SubmarineGeneratorSpec.h](Source/Sub3D/Submarine/Generator/SubmarineGeneratorSpec.h)

Aucune modification du contrat gameplay. Le spec continue d'exposer `Envelope`, `BulkheadPositionsNormalized`, `Passages`, `AirlockPositionNormalized`, `AirlockSide`, `RequestedStations`, `WallThicknessCm`, `FloorDropBiasCm`. Le fix de forme passe entièrement par l'envelope. Les toggles debug d'isolation ne doivent pas être ajoutés ici : `USubmarineGeneratorSpec` reste une donnée de génération, pas un panneau de debug runtime.

---

## 4. Réécriture de `BuildExteriorHull`

Fichier : [SubmarineMeshBuilder.cpp](Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp), fonction `USubmarineMeshBuilder::BuildExteriorHull` (lignes 150–413).

### 4.1 Bugs à corriger

| ID | Ligne(s) | Bug | Fix |
|---|---|---|---|
| EXT-1 | 291, 352 | Cap length hardcodé à `min(radius*0.8, 80)` | Lire `Envelope->BowCapLengthCm` / `SternCapLengthCm`. |
| EXT-2 | 299, 361 | Shrink `cos(T*pi/2)` fixe | Appliquer un profil paramétré par `BowProfile` + `BowSharpness` et idem stern. |
| EXT-3 | 281–282 vs 328–329, 390–391 | Winding hull body vs caps potentiellement incohérent | Auditer et harmoniser : le corps utilise `(A0,A1,B0)(B0,A1,B1)` ; les caps utilisent l'ordre opposé. Valider en PIE avec un matériau one-sided avant d'activer two-sided. |
| EXT-4 | 213 | `InterpEaseInOut` sur le rayon entre knot rings | Remplacer par une interpolation linéaire si on veut respecter fidèlement les rings de contrôle du GeneratorEditor (voir section 7). Un easing de lissage peut rester mais doit être désactivable. |
| EXT-5 | 333–337, 395–399 | Bow et stern centers sont des points isolés avec normale `-X` / `+X` | Corrects, mais vérifier qu'aucun triangle ne vise la mauvaise direction. |

### 4.2 Nouvelle structure des rings du cap

Remplacer la boucle actuelle par une construction paramétrée :

```text
Pour N = CapRings (par ex. 8) :
  t = (ring+1) / (N+1)                             // [0..1] vers la pointe
  CapX = BowX - t * Envelope->BowCapLengthCm
  ShrinkMultiplier = ApplyBowProfile(t, BowProfile, BowSharpness)
  Radius = BowRadius * ShrinkMultiplier
  Émettre EffRadial vertices sur la section superellipse
Émettre un vertex central à (BowX - BowCapLengthCm, 0, 0)
Trianguler ring-to-ring puis triangle-fan sur le vertex central.
```

`ApplyBowProfile` reproduit exactement les formules déjà utilisées dans `EvaluateBowSternTaper` (section 3.2) pour garantir la cohérence entre le body et le cap.

### 4.3 Matériau

- Le matériau coque extérieure **doit être two-sided** (propriété matériau) pendant toute la phase de validation. C'est le moyen le plus rapide d'éliminer la classe de bugs « on voit à travers ». Un audit winding pourra suivre si nécessaire, mais n'est pas prioritaire dans ce scope.

---

## 5. Correction du sol et des portes

### 5.1 Sol (`BuildInteriorCompartments`)

Fichier : [SubmarineMeshBuilder.cpp](Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp), fonction `BuildInteriorCompartments` (lignes 417–711).

- Le sol est actuellement un simple quad `(0,2,1)(1,2,3)` avec normale `FVector::UpVector`.
- **Fix** : matériau sol two-sided. Pas de double émission de triangles (économie de vertices, lisibilité), la solution matériau couvre les deux cas.

### 5.2 Portes (`BuildBulkheads`)

Fichier : [SubmarineMeshBuilder.cpp](Source/Sub3D/Submarine/Generator/SubmarineMeshBuilder.cpp), fonction `BuildBulkheads` (lignes 715–950).

**Root cause** : le cutout porte est construit en ajoutant manuellement 4 coins à un polygone `SolidOutline`, puis tous les autres points de l'arc sont ajoutés conditionnellement avec le filtre `Pt.Y > DoorTop || FMath::Abs(Pt.X) > DoorHalfWidth`. Quand `FloorZ > 0`, certains points de l'arc qui devraient former le contour haut du bulkhead se retrouvent exclus à tort, produisant un polygone dégénéré. Le fan-from-pivot triangulation au centre `PivotPt = (0, min(HalfH-1, DoorTop+1))` n'est valable que pour un polygone étoile, ce qui cesse d'être vrai dans ces cas.

**Fix minimal dans le scope** :

1. Construire l'outline du bulkhead dans l'ordre angulaire de la section, sans filtrage arbitraire. L'outline est simplement la portion d'arc comprise entre l'angle minimum (juste au-dessus du sol côté gauche) et l'angle maximum (juste au-dessus du sol côté droit), plus les deux segments verticaux descendant jusqu'au sol.
2. Appliquer le cutout porte **après** avoir l'outline fermé, par soustraction polygonale : couper un rectangle centré `[-DoorHalfWidth, DoorHalfWidth] x [FloorZ, FloorZ + DoorHeight]` du polygone solide.
3. Utiliser `FGeomTools2D::TriangulatePoly` (déjà disponible dans UE) sur le polygone résultant avec trou. Abandonner le fan-from-pivot qui ne marche que pour les polygones étoile.

**Alternative plus simple** (si `FGeomTools2D::TriangulatePoly` ne supporte pas trivialement les trous ici) :
- Découper le bulkhead en 3 sous-rectangles : bande gauche (x ∈ [−HalfW, −DoorHalfWidth]), bande droite (x ∈ [DoorHalfWidth, HalfW]), bande haute (y ∈ [DoorTop, HalfH]). Chaque bande est intersectée avec la superellipse de la section. Triangulation indépendante par bande. Simple, robuste, sans polygon clipping.

La deuxième approche est retenue par défaut parce qu'elle élimine toute ambiguïté polygon-étoile et supporte `FloorZ` arbitraire sans condition.

---

## 6. Pollution visuelle intérieure : investigation avant réécriture

L'utilisateur a signalé en vue wireframe des volumes triangulés solidement qui ressemblent à des convex hulls ou des slices de collision plutôt qu'à de la géométrie de rendu intentionnelle.

### 6.1 Hypothèses par ordre de probabilité

1. **Collision visible dans l'éditeur PIE.** Le `UProceduralMeshComponent` affiche ses collisions en mode `Collision`, `Collision Wireframe`, ou lorsqu'un console command a activé `show Collision`. Vérification : ouvrir le viewport, `Show > Collision`, puis `stat Collision`. Si les volumes bizarres disparaissent en décochant Collision, le root cause est identifié.
2. **Double émission bulkhead / interior wall.** `BuildBulkheads` et `BuildInteriorCompartments` émettent potentiellement des triangles qui se chevauchent dans la même section. Vérification : compter les vertices de chaque `FSubmarineMeshSectionData` après `BuildMeshData` et comparer visuellement en isolant chaque section (hull, interior, bulkheads) via un flag debug.
3. **Triangulation interior compartment dégénérée.** Si l'arc intérieur émet des triangles avec normales pointant vers l'extérieur de la pièce (au lieu de l'intérieur), l'effet visuel en wireframe est un volume solide.
4. **Airlock émis en box par `BuildBulkheads` ou `BuildExteriorHull`.** Currently, l'airlock est généré comme une box séparée (lignes 658–708). Vérifier qu'il n'y a pas de double émission dans plusieurs fonctions.

### 6.2 Étapes d'investigation (ordre strict)

1. **Ajouter des logs.** Dans `BuildMeshData`, logger le nombre de vertices et de triangles de chaque section (`ExteriorHullMesh`, `InteriorMesh`, `BulkheadMesh`, `AirlockMesh`) après chaque sous-étape. Format : `[SubMeshBuilder] Exterior V=%d T=%d Interior V=%d T=%d ...`. Logs avant toute réécriture.
2. **Toggle debug éditeur-accessible.** Exposer des flags validation-only sur `USubmarineGeneratedGeometryComponent` ou, si nécessaire, sur `ASubmarineBase` : `bBuildExteriorHull`, `bBuildInterior`, `bBuildBulkheads`, `bBuildAirlock`, default `true`. Permet d'isoler chaque section en PIE sans recompiler sans polluer `USubmarineGeneratorSpec`.
3. **Vérifier Show > Collision.** Si désactiver Collision en viewport supprime la pollution, ajouter `ProceduralMesh->SetCollisionEnabled(QueryAndPhysics)` explicit et vérifier que `bUseComplexAsSimpleCollision` est bien réglé. La pollution sera alors due à l'auto-génération de convex hulls par UE.
4. **Si la pollution persiste sans collision visible**, l'investigation continue dans `BuildInteriorCompartments` — c'est probablement une issue de double-face ou de normales.

### 6.3 Decision gate

- Si l'hypothèse 1 est confirmée (collision visible), la correction est de **séparer render et collision** : render mesh = `ExteriorHullMesh` + `InteriorMesh` + `BulkheadMesh` ; collision mesh = simplifié, généré après coup, pas le même mesh. Ce travail est dans le scope « correction de la forme » parce que l'utilisateur a explicitement cité la séparation render/collision comme non propre.
- Si les hypothèses 2, 3 ou 4 sont confirmées, corriger directement dans la fonction concernée. Pas de refonte globale.

---

## 7. Espace intermédiaire `GeneratorEditor` (concept, pas d'implémentation immédiate)

L'utilisateur demande un espace intermédiaire inspiré d'un chantier naval : bake d'une preview, modification des rings via gizmos, puis bake final du sub gameplay. Ce document fige le concept. L'implémentation est hors scope de ce plan et sera traitée dans un plan d'implémentation dédié.

### 7.1 Principe

```text
[USubmarineGeneratorSpec (asset)]
        |
        v
[USubmarineGenerator::GeneratePreview]  --> USubmarineDefinition (transient, preview)
        |
        v
[GeneratorEditor mode in PIE]
   - Affiche le sub preview
   - N rings de contrôle le long de la spine, chacun avec un gizmo
   - L'utilisateur déplace/scale un ring → la spec est mise à jour localement
   - Rebuild incrémental de la preview (pas un rebuild complet à chaque frame)
        |
        v
[User clicks "Bake Gameplay Sub"]
        |
        v
[USubmarineGenerator::GenerateFinal] --> USubmarineDefinition (persistant / baked)
        |
        v
[BP_Submarine_FPRun utilise ce USubmarineDefinition baké]
```

### 7.2 Règles d'acceptation du concept

1. **L'éditeur vit dans l'éditeur, pas dans le jeu.** C'est un mode PIE ou un tab d'asset editor. Pas de UI exposée au joueur final.
2. **Les rings de contrôle ne sont pas des curves.** Chaque ring a : position `X` le long de la spine, rayon `R`, multiplicateur horizontal/vertical (hérité de `WidthToHeightRatio`). Pas de points de Bézier, pas de tangentes, pas de courbes exposées.
3. **Le preview bake et le final bake utilisent le même `USubmarineGenerator` et le même `USubmarineMeshBuilder`.** Pas de pipeline parallèle. La seule différence est que le preview est transient et peut être rejeté.
4. **Les rings de contrôle sont dérivés automatiquement du spec initial.** À l'ouverture de l'éditeur, `ControlRingCount` rings sont générés à partir de `Envelope->EvaluateRadius` aux positions équi-réparties. L'utilisateur peut ensuite les ajuster. Les modifications sont stockées comme **override** dans une structure locale au spec, pas dans un asset séparé.
5. **Pas de gizmos freeform.** Les gizmos ne déplacent que le rayon du ring et éventuellement son offset X. Pas de rotation, pas de scale non-uniforme en dehors de Y/Z couplés par `WidthToHeightRatio`.

### 7.3 Sortie du concept

Le GeneratorEditor n'est pas implémenté dans ce plan. Ce document fige uniquement les règles ci-dessus pour que toute future implémentation reste cohérente. Le fix de forme (sections 3 à 6) doit être complété et validé en PIE avant même de commencer l'éditeur.

---

## 8. Plan d'exécution séquentiel

1. **Logs & isolation** (section 6.2, étapes 1–2). Aucune modification de géométrie. Permet de diagnostiquer la pollution intérieure.
2. **Envelope def** (section 3). Retrait `RadiusProfile`, ajout des champs cap length / sharpness / body length fraction / control ring count. Mise à jour `EvaluateRadius` / `EvaluateBowSternTaper`.
3. **Bow/stern cap paramétrés** (section 4). Réécriture des deux blocs cap dans `BuildExteriorHull`. Validation PIE : forme cigare visible, longueur cap contrôlable, sharpness fonctionnelle.
4. **Portes** (section 5.2). Réécriture `BuildBulkheads` avec approche 3-bandes. Validation PIE : `FloorDropBiasCm = 40` ne casse plus les portes.
5. **Matériaux two-sided** (sections 4.3 et 5.1). Fix hull et sol. Validation PIE : plus de transparence indésirable.
6. **Séparation render/collision si nécessaire** (section 6.3). Si les logs confirment que la pollution vient de la collision, ajuster `SetCollisionEnabled` et l'auto-gen convex hulls.
7. **Validation finale en PIE.** Reproduire exactement les scénarios de l'analyse PIE 2026-04-10 et confirmer qu'ils sont résolus.

Chaque étape est validée individuellement en PIE avant de passer à la suivante. Pas de commit batch.

---

## 9. Checklist de vérification

**Envelope**
- [ ] `RadiusProfile` retiré de `SubmarineGeneratorEnvelopeDef.h` et plus aucune référence dans `.cpp`.
- [ ] `BowCapLengthCm`, `SternCapLengthCm`, `BowSharpness`, `SternSharpness`, `BodyLengthFraction`, `ControlRingCount` ajoutés avec les clamps indiqués en section 3.1.
- [ ] `EvaluateRadius` retourne une valeur cohérente sans curve.
- [ ] `EvaluateBowSternTaper` applique les formules génériques de section 3.2.

**BuildExteriorHull**
- [ ] Cap length lu depuis l'envelope, plus de hardcode.
- [ ] Shrink du cap paramétré par profil + sharpness.
- [ ] Winding corps et caps audité, cohérent.
- [ ] Forme cigare observable en PIE avec bow long et stern long.

**BuildInteriorCompartments**
- [ ] Matériau sol two-sided.
- [ ] Vertices count et triangle count logués.

**BuildBulkheads**
- [ ] Portes visibles avec `FloorDropBiasCm = 0`.
- [ ] Portes visibles avec `FloorDropBiasCm = 40`.
- [ ] Portes visibles avec `FloorDropBiasCm = 80`.
- [ ] Pas de polygone dégénéré, pas de triangle manquant au-dessus de la porte.

**Pollution visuelle intérieure**
- [ ] Logs verts : `Exterior V=… T=…`, `Interior V=… T=…`, `Bulkhead V=… T=…`, `Airlock V=… T=…`.
- [ ] `Show > Collision` désactivé en viewport : pollution présente/absente documentée.
- [ ] Root cause identifié parmi les 4 hypothèses de section 6.1.
- [ ] Fix appliqué uniquement sur la cause identifiée, pas de refonte globale.

**Hors scope validé comme intact**
- [ ] `SubmarineGenerator::GenerateAirlock` non modifié.
- [ ] `SubmarineGenerator::BuildFloodGraph` non modifié.
- [ ] `SubmarineGenerator::PlaceStations` non modifié.
- [ ] `SubmarineGenerator::PlaceSpawns` non modifié.
- [ ] `FGeneratedCompartmentDef`, `FGeneratedConnectionDef` inchangés.

---

## 10. Liste d'exclusion explicite (ce document refuse)

- Aucun enum `Myring`, `Series58`, `SuperellipseLongitudinal`, `Uniform` exposé.
- Aucun import depuis `Source/Sub3DBake/Private/Bake/SubmarineHullProfileService.cpp`.
- Aucun import depuis `Source/Sub3DBake/Private/Bake/SubmarineAppendageBakeService.cpp`.
- Aucun `BakeBowDome`, `BakeSternFairing`, `FGeneratedRingData`.
- Aucun appendage (sail, sonar dome, planes, torpedo, propulsor, control surfaces, towed array).
- Aucune `FRuntimeFloatCurve` dans l'envelope ou le spec.
- Aucun usage de `SubmarineLayoutSolver` ou `SubmarineBuildCompiler` dans le flux de génération.
- Aucun changement de comportement de `SubFloodComponent`, `SubHullComponent`, `SubmarineCompartmentComponent`.
- Aucun changement de l'airlock (`GenerateAirlock` reste tel quel).
- Aucune UI in-game, aucun éditeur de sub exposé au joueur.
- Aucun label hybride (« editor-grade », « production-adjacent », « debug-but-durable »).

---

## 11. Autorité et suite

Ce document est authoritaire pour la correction de forme du générateur de sous-marin. Toute modification dans `Source/Sub3D/Submarine/Generator/` doit se conformer aux sections 3 à 6. Toute modification qui sort du scope « forme » est rejetée jusqu'à ce que ce plan soit complété et validé en PIE.

Le plan d'implémentation détaillé (ordre des edits, commit-by-commit) sera produit après validation de ce document par l'utilisateur.
