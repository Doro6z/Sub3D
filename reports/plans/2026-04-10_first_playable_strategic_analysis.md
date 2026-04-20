# Sub3D — Analyse stratégique vers First Playable

**Date** : 2026-04-10
**Input** : `2026-04-08_submarine_generator_architecture_unified.md` (authoritaire form-only) + audit runtime code + lecture gameplay vision
**Ton** : production, tranchée, solo-dev-aware
**Destination** : sera enregistré dans `reports/plans/` après validation comme `2026-04-10_first_playable_strategic_analysis.md`

---

## Contexte

Le projet Sub3D a une chaîne runtime générateur qui fonctionne bout en bout : `SubmarineGeneratorSpec → SubmarineGenerator::Generate → USubmarineDefinition → SubmarineMeshBuilder::BuildMeshData → GeneratedGeometryComponent::BuildFromDefinition → PMC visibles en PIE`. Les stations spawnent, le flood simule, le bridge breach→flood est câblé, SubDoorActor existe.

Mais le projet n'est pas jouable. Entre "ça tourne" et "ça se joue", il manque : forme correcte (hull/caps/doors/floor), airlock visible, spawn de doors depuis la Definition, mécanique de repair, validation FP testable.

Le doc authoritaire du 08/04 ne couvre que la correction de forme. Cette analyse couvre la trajectoire complète vers un First Playable respectant la gameplay vision : **circulation dans le sous-marin, stations, brèches, inondation, réparation, reprise de contrôle**.

---

## 1. Diagnostic global franc

Ton projet est dans la zone grise la plus dangereuse pour un solo dev : **l'architecture marche, mais le jeu ne se joue pas encore**. Cette zone est piégée parce que :
- Chaque bug donne envie d'un fix architectural plutôt qu'un fix opportuniste
- Chaque feature manquante invite à construire un système plutôt qu'une solution locale
- Le tooling devient attirant parce qu'il promet de résoudre le problème "une fois pour toutes"
- Tu es toujours à quelques heures d'une refactorisation qui "rendrait tout plus simple"

Tu as raison de t'en inquiéter. Ton propre CLAUDE.md dit : *"je refuse qu'une architecture élégante empêche d'obtenir rapidement quelque chose de jouable"*. Ton instinct est bon. Mais ton plan actuel (le doc authoritaire + les phases 1-5 implémentées + Phase 5D insérée) a toutes les caractéristiques d'une trajectoire qui va continuer de tourner autour du générateur pendant des mois sans fermer le FP si tu ne bornés pas fermement le scope.

Le doc authoritaire est scopé correctement (form only), mais il n'y a aucun doc qui dit "ce qu'il faut en plus pour que ce soit jouable". Résultat : l'attention est sur la forme, les gaps gameplay sont invisibles, et le FP s'éloigne.

**Signal d'alerte principal** : tu as passé du temps à insérer une Phase 5D dans le plan d'implémentation alors que `USubmarineGeneratedGeometryComponent` existait déjà. Cela veut dire que tu n'avais pas une vision claire de l'état réel du code au moment de planifier. C'est un symptôme typique de projet qui se disperse entre plan et réalité.

**Franc** : ton approche est viable, mais tu es à un cheveu d'entrer dans le cycle qui tue les projets solo — la refonte permanente d'un générateur qui est déjà bon assez.

---

## 2. Ce qui tient déjà debout

Ces éléments sont solides, testés par le fait qu'ils produisent un résultat observable en PIE. **Ne les touche pas pour FP.**

1. **Chaîne runtime complète**
   - `SubmarineBase::BeginPlay` ligne 114-124 appelle `Generator::Generate` + `MeshBuilder::BuildMeshData`
   - `SubmarineBase::BeginPlay` ligne 172-178 appelle `GeneratedGeometry::BuildFromDefinition`
   - Le hull sort en PIE (preuve que la chaîne est wireable et wirée)

2. **Contrats de données**
   - `USubmarineDefinition` comme source unique runtime
   - `FGeneratedCompartmentDef`, `FGeneratedConnectionDef`, `FGeneratedStationSlotDef`, `FCompiledFloodGraph`
   - Pas d'ambiguïté sur qui possède quoi

3. **Simulation flood**
   - `USubFloodComponent` avec API complète : `CreateBreach`, `RemoveBreach`, `SetDoorState`, `SetPumpActive`, queries
   - Bridge `HandleBreachesUpdatedForFlood` câblé dans `SubmarineBase.cpp:167` et `:875`
   - Replication des `CompartmentStates`

4. **Station system**
   - `USubmarineStationManagerComponent::SpawnStationsFromDefinition` (SubmarineBase.cpp:136)
   - Stations spawnent effectivement en PIE (`BP_HelmStation0`, `BP_EngineStation0`, `BP_BallastStation0`)

5. **Door infrastructure**
   - `ASubDoorActor` existe avec replication, InteractableComponent, state machine
   - `SetDoorClosed`, `ToggleDoor`, `HandleInteract` implémentés
   - `FindAttachedDoorById` dans SubmarineBase pour lookup

6. **Generator topology complète**
   - `DeriveCompartments`, `GenerateAirlock` (produit les 2 connections), `BuildFloodGraph`, `PlaceStations`, `PlaceSpawns` : tout fonctionne et le Definition est consommé par les systèmes downstream

Ces 6 points forment une plate-forme stable. L'erreur serait de les "nettoyer" avant d'avoir validé le FP.

---

## 3. Ce qui est fragile, incomplet ou trompeur

Par ordre de criticité pour le FP.

### Fragile — qualité observable en PIE

1. **Winding hull + matériau non two-sided** (doc §1.2 point 1, §4.3)
   - On voit à travers la coque en PIE
   - Fix prescrit : matériau two-sided. **Bon pour FP, dette cosmétique documentée.**
   - Ne pas lancer d'audit winding exhaustif avant FP.

2. **Sol one-sided visible par dessous** (doc §1.2 point 2, §5.1)
   - Quad unique avec normale Z, le crew voit le sol disparaître depuis certains angles
   - Fix prescrit : matériau sol two-sided. **Bon pour FP.**

3. **Door cutout dégénéré si `FloorDropBiasCm > 0`** (doc §1.2 point 3, §5.2)
   - Root cause identifié : fan-from-pivot + filtre `Pt.Y > DoorTop || FMath::Abs(Pt.X) > DoorHalfWidth` ligne 804 de SubmarineMeshBuilder.cpp
   - Fix prescrit : approche 3-bandes (gauche / droite / haute). **Bon, pas d'alternative plus simple.**
   - **Piège à éviter** : ne pas tenter `FGeomTools2D::TriangulatePoly` avec trous. C'est un rabbit hole.

4. **Bow/stern caps hardcodés** (doc §1.2 point 4, §4)
   - `FMath::Min(BowRadius * 0.8f, 80.f)` ligne 291 et 352
   - `cos(T * pi/2)` fixe sur 6 anneaux
   - Fix prescrit : envelope refactor + caps paramétriques. **Bon, périmètre borné.**

5. **Pollution visuelle intérieure non diagnostiquée** (doc §1.2 point 5, §6)
   - 4 hypothèses listées, aucune investigation encore
   - **Pré-requis dur** : les logs V/T + toggles d'isolation du doc §6.2 étape 1-2 doivent être faits AVANT toute modif mesh, sinon tu corriges à l'aveugle.

### Incomplet — gaps gameplay non couverts par le doc

6. **Airlock sans mesh visible**
   - `GenerateAirlock` (SubmarineGenerator.cpp:384-490) crée un `FGeneratedCompartmentDef` + 2 `FGeneratedConnectionDef` mais **n'émet aucune section mesh**.
   - `BuildBulkheads` skip explicitement l'airlock (ligne 744)
   - Conséquence PIE : le sas est un volume flood invisible collé au hull
   - **Bloquant pour FP** : la gameplay vision inclut le cycle sas, donc le sas doit être visible, interactable, et fonctionnel.

7. **Doors pas spawnées depuis `FGeneratedConnectionDef`**
   - `ASubDoorActor::InitializeFromDoorDef` prend un `FDoorDef` (legacy), pas un `FGeneratedConnectionDef`
   - `FindAttachedDoorById` cherche dans `AttachedActors` → suppose qu'elles ont été attachées, mais par qui ?
   - Pas trouvé de code qui itère `GeneratedDefinition->Connections` pour spawner des doors
   - **Bloquant pour FP** : sans spawn programmatique depuis la Definition, les doors n'existent pas à la bonne place.

8. **Repair gameplay inexistant**
   - Il y a `OnBreachesUpdated` sur SubHull, `CreateBreach`/`RemoveBreach` sur SubFlood, mais aucune fonction joueur qui "répare"
   - **Bloquant pour FP** : la vision gameplay dit "réparation" explicitement.

9. **Création de brèche joueur-déclenchable inexistante**
   - Il faut un moyen de créer une brèche pendant le test (combat, damage event, ou debug cheat)
   - Pas de console command ni de système de dommage exécutable
   - **Bloquant pour FP** : sans ça, tu ne peux pas tester les scénarios 1-14 du protocole FP.

10. **Lighting intérieur inexistant**
    - Pas de light spawnée dans les compartiments
    - L'intérieur sera noir en PIE
    - **Bloquant mineur** : si noir complet, le crew ne peut pas voir où il marche. Fix trivial : 1 point light par compartiment.

### Fragile — dépendance backend mesh

13. **`UProceduralMeshComponent` est marqué EXPÉRIMENTAL en UE 5.7**
    - Ton chemin runtime actuel repose entièrement sur PMC via `CreateMeshSection_LinearColor` (ligne 343 de `SubmarineGeneratedGeometryComponent.cpp`)
    - Risque : Epic peut déprécier, changer l'API, ou le tagger deprecated entre deux versions mineures
    - Alternative plus supportée : `UDynamicMeshComponent` (ecosystème Geometry Script + Modeling Tools)
    - **Décision pour FP** : rester sur PMC. Changer de backend coûterait des jours et ne change rien au gameplay. Mais documenter cette dette.
    - **Règle qui en découle** : le gameplay ne lit JAMAIS la mesh directement. Il lit `USubmarineDefinition`. La mesh est un détail d'implémentation interchangeable. Si tu vois un composant runtime qui fait `ProceduralMesh->GetVertex...` au lieu de `Definition->GetCompartment...`, c'est un bug architectural à corriger.

### Trompeur — pièges conceptuels

11. **Les warnings [LEGACY] dans le log**
    - Ton doc de session FirstPlayableRun dit : *"les warnings [LEGACY] sont des signaux d'échec de setup"*
    - Tant que les fallbacks coexistent avec le path generator, tu as un risque permanent de "réparer" un fallback au lieu du générateur. C'est le chemin le plus court pour gaspiller des heures.
    - **Décision tranchée** : les fallbacks doivent être retirés du path principal (ou marqués DEPRECATED + UE_LOG bruyant) avant M3. Pas après.

12. **Le concept GeneratorEditor (doc §7)**
    - Le doc dit "concept, pas d'implémentation immédiate". Respecte cette ligne.
    - C'est le piège le plus séduisant de ton projet actuel. Tu vas vouloir l'implémenter parce qu'il promet de résoudre les itérations de forme. **Il va te coûter semaines voire mois, et il n'est pas nécessaire pour FP.**
    - Alternative minimale : §5.C de cette analyse (CallInEditor + Refresh).

13. **Le plan d'implémentation corrigé (Phase 5D)**
    - Tu as inséré Phase 5D dans un plan d'implémentation alors que le composant `GeneratedGeometryComponent` existait déjà et fonctionnait. Tu étais en train de planifier quelque chose qui existait.
    - **Signal** : ta vision du code réel n'est pas toujours à jour. Avant de planifier une phase, va toujours lire le code.

---

## 4. Ce qu'il faut absolument simplifier

Décisions tranchées, non négociables pour fermer le FP.

### 4.1 — Airlock : pas de génération procédurale
- **NE PAS** essayer de souder un mesh airlock au hull par opération booléenne
- **NE PAS** exposer une géométrie de sas paramétrable dans l'envelope
- **À LA PLACE** : un `BP_Airlock` préfabriqué avec son propre mesh, ses 2 doors enfants, sa collision, ses zones d'interaction. Spawné dans `SubmarineBase::BeginPlay` à `AirlockCenter` (lu depuis `FGeneratedCompartmentDef` de type Airlock), attaché à `SubmarineRoot`.
- Le "trou" dans le hull où s'attache le sas est caché visuellement par le mesh du sas qui recouvre la zone. Personne ne regarde dessous pour FP.
- Blending visuel : une "flange" sur le BP_Airlock couvre la jointure. Si jointure visible sous certains angles, tant pis, backlog post-FP.

### 4.2 — Doors : spawn programmatique simple
- Itérer `GeneratedDefinition->Connections` dans `SubmarineBase::BeginPlay` après `BuildFromDefinition`
- Pour chaque connection de type `Door`, `Hatch`, `ExteriorHatch` : `SpawnActor<ASubDoorActor>` à `Conn.LocalTransform`
- Nouvelle surcharge : `ASubDoorActor::InitializeFromConnectionDef(const FGeneratedConnectionDef&, ASubmarineBase*)`. Ne pas toucher à `InitializeFromDoorDef` (legacy, laisser tomber).
- Attach au `SubmarineRoot`
- Chaque door, quand togglée, appelle `SubFlood->SetDoorState(DoorId, bClosed)`

### 4.3 — Repair : one-button, no-minigame
- Nouvelle UFUNCTION sur `USubHullComponent` : `ClearBreachNearLocation(const FVector& WorldLocation, float Radius)`
- Quand une brèche est créée par `HandleBreachesUpdatedForFlood`, spawn un `AInteractablePoint` temporaire au point de brèche avec prompt "Repair"
- Input → `ClearBreachNearLocation` → cascade automatique vers `SubFlood::RemoveBreach`
- Zero UI, zero ressources, zero timer. Si le joueur stand devant, clique, ça répare.
- **Ne pas faire** : minigame de réparation, wrench wielding, resource spend. Tout ça est post-FP.

### 4.4 — Editor interne : CallInEditor + Rebuild
- `USubmarineGeneratorEnvelopeDef` et `USubmarineGeneratorSpec` sont déjà des `UDataAsset`. Leurs propriétés sont déjà éditables dans le Content Browser.
- Ajouter sur `ASubmarineBase` : `UFUNCTION(CallInEditor, Category="Debug") void RebuildFromSpec();`
- Cette fonction reset `GeneratedDefinition`, rappelle `Generate` + `BuildMeshData` + `BuildFromDefinition`, respawn doors + airlock + stations.
- Tu cliques un bouton dans les Details panel → la forme se met à jour.
- **C'est ton éditeur pour FP.** Pas plus.
- **Ne pas faire** : `FAssetEditorToolkit`, Slate panel, gizmos 3D custom, control rings draggables, preview/bake split. Tous post-FP si même nécessaires.

### 4.5 — Collision : render et collision séparés, décisivement
- **1 PMC dédié collision**, jamais visible (`SetHiddenInGame(true)` + `SetVisibility(false)`), collision profile `BlockAll`, 1 convex par segment longitudinal pour la coque + 1 convex par floor de compartiment pour le marchable
- **N PMC dédiés render** (hull, intérieurs, bulkheads), collision `NoCollision`
- **Pas de** `bUseComplexAsSimpleCollision`. **Pas de** double-rôle.
- Ceci résout probablement l'hypothèse 1 de la pollution visuelle du doc §6.1. À valider après investigation.

### 4.6 — Tests : console cheats d'abord, features ensuite
- Avant d'implémenter breach gameplay, implémenter `DevCheat_CreateBreach [CompartmentId] [RateLps]`
- Avant d'implémenter door interact, implémenter `DevCheat_SetDoorClosed [ConnectionId] [bool]`
- Avant d'implémenter repair, implémenter `DevCheat_RepairAllBreaches`
- Ces cheats sont dans `SubPlayerController` ou un `USubCheatManager`
- **Raison** : sans ça, tu ne peux tester un scénario qu'en jouant le jeu normalement, ce qui coûte 10x plus cher que taper une commande.

---

## 5. Analyse détaillée des sujets critiques

### 5.A — Géométrie jouable

**Position** : le doc authoritaire §4-5 est correct sur le scope. Mais il faut deux précisions supplémentaires.

**Précision 1 — Render/collision séparation est OBLIGATOIRE avant le reste.**

Tant que render et collision sont dans le même PMC, tu ne sais pas si les bugs visuels viennent de la géométrie ou de l'autogen de convex hulls. Le doc §6.1 hypothèse 1 est très probablement la vraie cause de la pollution visuelle intérieure. **Investigue ça en premier** (logs V/T + Show > Collision) avant toute autre modif mesh.

**Précision 2 — Two-sided material est une dette acceptée, pas une solution.**

Le doc §4.3 prescrit two-sided pour le hull. C'est correct pour FP. Mais ça crée :
- Une dette visuelle : le backface lighting est incorrect, les ombres peuvent paraître étranges
- Une dette de production : quand tu auras des matériaux production, tu devras refaire l'audit winding

Accepter ces dettes dans un backlog post-FP explicite : `backlog/post_fp_debt.md` avec la ligne "hull et floor materials two-sided temporaire — audit winding requis avant lighting final".

**Règles opérationnelles pour la forme** :

| Aspect | FP | Post-FP |
|---|---|---|
| Hull exterior | PMC unique, two-sided material, forme cigare paramétrée | Winding audité, one-sided, material production |
| Interior walls | PMC par compartiment, two-sided floor, normals valides | UV mapping, textures, subdivision |
| Bulkheads | Approche 3-bandes, cutouts porte fonctionnels | Frames, rivets, détails |
| Bow/stern caps | Paramétrés par `BowCapLengthCm`/`BowSharpness`, formes basiques | Profils personnalisés, appendages |
| Collision | 1 convex / segment hull, 1 convex / compartment floor | Collision précise par composant mesh |
| Marchable | Crew marche sur les floors de compartment | Escaliers, ladders, déclivités |
| Doors cutouts | Fonctionnels pour `FloorDropBiasCm` ∈ {0, 40, 80} | Cutouts avec geometry précise (frame, hinges) |

**Validation PIE** : chaque étape du doc §8 est validée individuellement. Ne pas batch. Ne pas "optimiser en fin de phase". Si un truc ne marche pas à la fin de l'étape N, c'est cette étape qui a introduit la régression, pas la N+1.

**Principe architectural dur — backend mesh interchangeable**

Conséquence du fait que `UProceduralMeshComponent` est expérimental en UE 5.7 : ton architecture doit être capable de changer de backend mesh sans réécrire un seul système gameplay.

Règles concrètes :
- `USubFloodComponent`, `USubmarineStationManagerComponent`, `USubmarineCompartmentComponent`, `ASubCrewCharacter`, `ASubDoorActor` lisent **uniquement** `USubmarineDefinition`. Jamais `GeneratedGeometry->GetMeshSection(...)`.
- `USubmarineGeneratedGeometryComponent` est la seule classe autorisée à parler au PMC. C'est la couche "materialization".
- Si un jour tu dois passer à `UDynamicMeshComponent` ou à un bake vers Static Mesh, tu réécris `GeneratedGeometryComponent::BuildFromDefinition` et rien d'autre.
- Aucune classe runtime ne doit avoir `#include "ProceduralMeshComponent.h"` sauf `GeneratedGeometryComponent` elle-même.

**À vérifier en M1** : grep `ProceduralMeshComponent` dans `Source/Sub3D/Submarine/` et confirmer que seul `GeneratedGeometry/` l'inclut. Si d'autres classes l'incluent, c'est une fuite architecturale à corriger avant M3.

### 5.B — Sas / airlock / blending (critique)

**Diagnostic**

Le sas est le point de friction maximal entre "tout généré" et "pragmatique". Le doc authoritaire laisse `GenerateAirlock` inchangé (§10 dit "aucun changement de l'airlock"). Résultat : le sas existe en topologie mais pas en géométrie. En PIE, c'est un volume invisible flood-only. **Injouable pour FP.**

**Comparaison des options**

| Option | Avantage | Coût | Risque | Verdict FP |
|---|---|---|---|---|
| A. Mesh soudé au hull, cutout booléen | Integrité visuelle unique, tout généré | Très haut : boolean 2D sur mesh triangulé | Toxique : chaque touche au générateur risque de casser le cutout | REJETÉ |
| B. Cylindre généré séparé, overlap avec hull | Moins de boolean, reste dans le pipeline generator | Haut : normales à gérer, Z-fighting aux intersections | Moyen : mais tu dois solver l'intersection | REJETÉ pour FP |
| C. BP_Airlock préfab attaché à un socket | Zéro remeshing, zéro boolean, itération artistique libre | Bas : 1 BP à créer, 1 socket, 1 spawn | Cosmétique : jointure visible sous certains angles | **RECOMMANDÉ** |
| D. Mesh statique importé + instancing | Idem C mais encore plus léger | Moyen : perd le lien dynamique | Pas extensible pour variations | REJETÉ : C est meilleur |

**Implémentation concrète pour FP — Option C**

1. **Le générateur garde `GenerateAirlock` inchangé** (la Definition contient toujours le compartiment airlock + les 2 connections flood)

2. **Création d'un `BP_Airlock`** :
   - Root scene
   - StaticMesh (ou ProceduralMesh simple) : un cylindre ou une boîte arrondie représentant le sas
   - Une "flange" en collerette à la base (couvre visuellement la jointure avec le hull)
   - 2 `ASubDoorActor` enfants positionnés sur les faces inner (vers hull) et outer (vers mer)
   - 1 collision volume pour empêcher le crew de traverser les parois du sas
   - 1 BoxComponent "floor collision" pour marchable
   - 1 PointLight interne basique

3. **Dans `SubmarineBase.h`** :
   ```cpp
   UPROPERTY(EditDefaultsOnly, Category = "Airlock")
   TSubclassOf<AActor> AirlockActorClass;

   UPROPERTY()
   TObjectPtr<AActor> SpawnedAirlock = nullptr;
   ```

4. **Dans `SubmarineBase::BeginPlay`**, après `GeneratedGeometry->BuildFromDefinition` :
   ```cpp
   SpawnAirlockFromDefinition();
   ```
   Cette fonction :
   - Trouve le `FGeneratedCompartmentDef` avec `SemanticType == ESubCompartmentType::Airlock`
   - Lit `HydroBoundsMin/Max` pour calculer le centre local
   - Lit `FGeneratedConnectionDef` avec `CompartmentA/B == AirlockId` pour trouver les 2 connections
   - `SpawnActor<AActor>(AirlockActorClass, LocalTransformAsWorld, ...)`
   - `AttachToComponent(SubmarineRoot, KeepRelative)`
   - Initialise les 2 doors internes du BP avec `ConnectionId`, `CompartmentA/B`, et les bind à `SubFlood`

5. **Les 2 doors internes de l'airlock appellent `SubFlood->SetDoorState` normalement**. Le flood graph les voit comme des edges ordinaires. Cycle sas fonctionnel.

**Blending visuel pour FP**

- La flange (aussi appelée "collar" dans la littérature UE) du BP_Airlock recouvre la jointure
- Le matériau de la collar est le même que celui de la coque (ou un blend neutre gris foncé)
- **Aucun procedural stitching.** Si ça a l'air cheap, tant pis. C'est un FP.
- Angle camera : éviter de pointer la camera vers la jointure dans les prises de vue démo du FP. Tu ne tricherais pas, tu bornés le pitch.

**Variante plus procédurale (post-FP si jamais nécessaire)**

Une alternative documentée est "collar + cut intérieur" : l'airlock intersecte géométriquement la coque (sans boolean union runtime), une collerette procédurale masque la jointure visible, et un "cut" spécifique est ajouté côté intérieur pour créer le passage physique. Geometry Script dispose d'`Apply Mesh Boolean` sur Dynamic Mesh qui peut faire ça, mais c'est à garder pour un **bake editor-only**, jamais en runtime. **Post-FP uniquement** et uniquement si le BP_Airlock full-prefab montre ses limites artistiques.

**Pourquoi c'est une bonne décision solo dev**
- Zero risque de casser le générateur
- Itération artistique via BP (pas de recompile C++)
- Le jour où tu veux des sas variés : plusieurs BP_Airlock_X sélectionnables via Spec
- Quand plus tard tu voudras un mesh soudé vraiment propre, tu auras appris ce qui ne marche pas avant de faire l'investissement

**Ce qui devient toxique à moyen terme**
- Option A (mesh soudé généré) devient toxique dès que tu veux des variations de sas. Chaque variation = nouveau code de cutout = nouveau bug mesh.
- Si tu commences avec A, tu auras toujours des bugs cutout prioritaires sur les bugs gameplay. Spirale mort.

**La vraie question à te poser** : est-ce que ton joueur va jamais se dire "ce sas est magnifiquement soudé à la coque" ? Non. Il va se dire "j'ai pu cycler le sas et sortir nager". Focus là.

### 5.C — Outil/éditeur interne

**Position tranchée** : **NE PAS CONSTRUIRE DE GENERATOR EDITOR POUR FP.**

Le doc §7 décrit un système avec rings de contrôle, gizmos, preview/bake split. C'est un projet à part entière. Pour un solo dev en phase FP, ça n'a pas de ROI.

**Le plus petit éditeur crédible qui t'aide vraiment pour FP**

Rien. Utilise l'asset editor standard d'Unreal + un bouton Rebuild. C'est tout.

**Implémentation** :

1. **`USubmarineGeneratorEnvelopeDef`** est déjà un `UDataAsset`. Double-clic → panel de propriétés standard avec tous les paramètres exposés en `UPROPERTY(EditAnywhere)`. C'est déjà un "éditeur". Tu modifies `BowCapLengthCm`, tu sauves, c'est pris en compte.

2. **`USubmarineGeneratorSpec`** idem. Tu modifies les `BulkheadPositionsNormalized`, tu sauves.

3. **Sur `ASubmarineBase`, ajouter** :
   ```cpp
   UFUNCTION(CallInEditor, Category = "Debug|Submarine")
   void RebuildFromSpec();

   UFUNCTION(CallInEditor, Category = "Debug|Submarine")
   void ClearGeneratedState();
   ```
   
   `RebuildFromSpec` :
   - Clear `GeneratedDefinition`
   - Destroy `SpawnedAirlock`, tous les `ASubDoorActor` spawnés
   - Clear les PMCs via `GeneratedGeometry->ClearGeometry()`
   - Rappelle toute la chaîne `Generate` → `BuildMeshData` → `BuildFromDefinition` → `SpawnDoors` → `SpawnAirlock` → `SpawnStations`

4. **Dans les Details panel** de l'actor en éditeur, tu vois 2 boutons "Rebuild From Spec" et "Clear Generated State". Un clic, la forme se met à jour.

**Ce que tu obtiens pour 1h de code** :
- Itération complète sur la forme sans quitter le Content Browser
- Modifier Spec ou Envelope → sauvegarder → cliquer Rebuild → voir le résultat
- Pas de recompile C++ sur les paramètres
- Zero widget custom, zero Slate, zero gizmo

**Ce qui ne doit PAS entrer dans l'éditeur FP** :
- Gizmos 3D pour control rings → **post-FP**
- Preview transient vs baked Definition → **post-FP**
- Asset editor custom avec viewport → **post-FP**
- UMG editor utility widget → **seulement si CallInEditor devient réellement limitant, et pas avant**

**Structure d'UI lisible (si un jour tu fais l'UMG widget post-FP)** :

```
+-------------------------------------+
| Submarine Quick Tune                |
+-------------------------------------+
| Spec: [DA_Spec_FP01 v]    [Reload]  |
|                                     |
| Hull                                |
|   Spine Length [____1520] cm        |
|   Body Fraction [=====0.55]         |
|   Bow Cap Len [____240] cm          |
|   Bow Sharp [====1.0]               |
|   Stern Cap Len [____320] cm        |
|   Stern Sharp [====1.0]             |
|                                     |
| Layout                              |
|   Bulkhead 1 [====0.25]             |
|   Bulkhead 2 [====0.50]             |
|   Bulkhead 3 [====0.75]             |
|                                     |
| [Rebuild]  [Save Spec]              |
+-------------------------------------+
```

Pas de 3D. Pas de gizmos. Pas de control rings. Des sliders bindés à des `UPROPERTY` via `Widget Reflector` + `Property Binding`. Un bouton Rebuild. C'est tout. Si tu en arrives là, tu seras probablement déjà post-FP et tu auras appris lesquels des paramètres sont réellement itérés fréquemment.

**Path d'escalade documenté (si FP clos et tu veux vraiment un meilleur éditeur)**

Escalade progressive, pas de saut :

| Niveau | Technique UE | Coût | Quand l'envisager |
|---|---|---|---|
| 0 | Details panel standard + `CallInEditor` | 1h | **Pour FP. Ne pas dépasser.** |
| 1 | `UEditorUtilityWidget` (EUW) + `UDetailsView` | ~1 jour | Post-FP, si tu itères >10x par jour sur la forme |
| 2 | EUW + Component Visualizers pour dessiner des handles 2D dans le viewport | ~2-3 jours | Si niveau 1 est utilisé quotidiennement et insuffisant |
| 3 | Editor Mode (`UEdMode` / Scriptable Tools / ITF) avec gizmos 3D | 1-3 semaines | Si plusieurs personnes construisent des subs, ou si tu sais précisément ce que tu dois manipuler |
| 4 | Asset Editor custom (`FAssetEditorToolkit`) avec son propre viewport | 3+ semaines | Projet complet dédié |
| 5 | `GeneratedDynamicMeshActor` pour preview temps réel editor-only | Variable | Si tu passes à Dynamic Mesh backend, cet actor existe spécifiquement pour ça |

**Règle** : ne jamais sauter plus d'un niveau. Ne jamais monter avant d'avoir épuisé le niveau courant. La plupart des solo devs n'ont besoin que du niveau 0, certains du niveau 1. Les niveaux 2+ sont pour des cas très spécifiques.

**Gotcha connu pour niveau 1+** : les rebuilds procéduraux en Blueprint/EUW peuvent être coûteux et casser l'interactivité éditeur. Toujours throttler les rebuilds (débounce 0.2-0.5s après dernière modif slider, pas rebuild à chaque tick).

**Réponse à "quel est le plus petit éditeur crédible"** : **niveau 0** — le Details panel d'Unreal + `CallInEditor`. Coût : 1h. ROI : immédiat. Tous les autres chemins sont des dévieurs pour FP.

### 5.D — Runtime path et ordre

L'architecture runtime actuelle est bonne. Le seul ajustement est d'ajouter deux étapes dans `SubmarineBase::BeginPlay` après `BuildFromDefinition` :

```cpp
// SubmarineBase.cpp BeginPlay (pseudo)
1. Create components (existing)
2. if (!GeneratedDefinition && GeneratorSpec):
     Generate() + BuildMeshData()           // existing
3. GeneratedGeometry->BuildFromDefinition()   // existing
4. SpawnDoorsFromDefinition()                 // NEW
5. SpawnAirlockFromDefinition()               // NEW
6. StationManager->SpawnStationsFromDefinition() // existing (but verify ordering after doors)
7. SubFlood->InitializeFromDefinition()       // existing
8. SubHull->OnBreachesUpdated.AddDynamic(...) // existing
```

**Order matters** :
- Doors et airlock doivent être spawnés AVANT que le crew character essaie de les trouver pour l'interaction
- Stations peuvent spawner en dernier ou en premier, peu importe
- SubFlood doit être initialisé APRÈS que GeneratedDefinition est complète (pour lire le flood graph)

**NavMesh / AI crew — décision maintenant**

Si ton FP inclut des **AI crew members** qui pathfind dans le sous-marin, tu dois prévoir la génération NavMesh sur la géométrie dynamique. UE documente "Navigation Invokers" et la navmesh runtime generation.

**Décision tranchée** : le FP Sub3D tel que décrit dans la gameplay vision (circulation, stations, brèches, flood, repair, regain control) est un FP **joueur-unique-en-intérieur**. **Pas d'AI crew pour FP.** Donc pas de NavMesh dynamique dans M1-M4. Si tu as besoin plus tard, c'est du travail M5+ post-FP avec Navigation Invokers.

Si à un moment tu te surprends à penser "il faudrait que les NPCs puissent se déplacer", stop, reporter.

**Retrait progressif des fallbacks legacy** :
1. **D'abord** : avoir M1→M4 qui passent sans toucher au legacy
2. **Ensuite** : ajouter `UE_LOG(LogSub3D, Warning, TEXT("[LEGACY] ..."))` sur chaque branche fallback touchée
3. **Puis** : retirer les fallbacks un par un, vérifier FP passe encore
4. **Jamais** : retirer les fallbacks en premier

Ordre inverse = tu casses la baseline, tu ne peux plus tester, tu passes des heures à trouver la régression.

### 5.E — Validation FP (critères de sortie)

**Définition du "First Playable"** : un joueur peut lancer PIE, spawner dans le sous-marin, et exécuter le scénario suivant de bout en bout sans crash ni warning [LEGACY] sur le path principal.

**Les 14 critères du protocole FP** (cocher chacun avant de déclarer FP clos) :

1. Sous-marin apparaît en PIE avec la forme correcte (caps contrôlables, pas de transparence backface visible, doors visibles dans les bulkheads)
2. Le crew character spawne dans un compartiment walkable depuis `SpawnPoint.Crew`
3. Le crew peut marcher à l'intérieur sans tomber à travers le sol
4. Le crew peut s'approcher d'une door, voir le prompt "interact", et toggler la door
5. La door togglée met à jour `SubFlood->SetDoorState`
6. Le crew atteint le `BP_HelmStation0` et peut interagir avec
7. L'interaction helm permet au moins une translation ou rotation du sous-marin
8. Commande console `DevCheat_CreateBreach <CompartmentId> 500` crée une brèche observable
9. L'eau monte progressivement dans le compartiment (visual water plane)
10. Si une door vers un compartiment adjacent est ouverte, l'eau se transfère
11. Une pompe activée (console cheat) réduit le niveau
12. Le crew s'approche de la brèche, voit le prompt "Repair", et la répare → l'eau arrête de monter
13. Le sas : crew entre, ferme la porte intérieure, ouvre la porte extérieure → le sas se remplit d'eau. Ferme la porte extérieure, active la pompe → le sas se vide. Ouvre la porte intérieure → le crew revient dans le sub.
14. Aucun warning `[LEGACY]` dans le log sur le path principal pendant toute l'exécution

**Qualification "pass"** : les 14 critères passent en une seule session PIE consécutive. Si tu dois redémarrer PIE entre deux critères, ce n'est pas un pass.

**Protocole de test** :
- Console cheats tapés dans l'ordre
- Pas d'automatisation nécessaire pour FP (trop cher pour solo dev)
- Enregistrer un screencast pour chaque pass pour documenter la progression
- Si un critère échoue : identifier cause, fix, **rejouer tout le protocole depuis le début**. Pas de "je sais que les autres marchent". Les régressions sont insidieuses.

---

## 6. Proposition d'architecture resserrée

**L'architecture actuelle est bonne. Aucune refonte.** Les ajustements sont localisés.

### Changements structurels minimaux

1. **SubmarineBase.h** : 2 nouvelles UPROPERTY
   ```cpp
   UPROPERTY(EditDefaultsOnly, Category = "Submarine|Doors")
   TSubclassOf<ASubDoorActor> DoorActorClass;

   UPROPERTY(EditDefaultsOnly, Category = "Submarine|Airlock")
   TSubclassOf<AActor> AirlockActorClass;
   ```

2. **SubmarineBase.cpp** : 3 nouvelles fonctions
   - `void SpawnDoorsFromDefinition();`
   - `void SpawnAirlockFromDefinition();`
   - `UFUNCTION(CallInEditor) void RebuildFromSpec();`
   Le tout appelé depuis `BeginPlay` après `BuildFromDefinition`.

3. **SubDoorActor.h** : nouvelle surcharge
   ```cpp
   UFUNCTION(BlueprintCallable, Category = "Door")
   void InitializeFromConnectionDef(
       const FGeneratedConnectionDef& Conn,
       ASubmarineBase* InOwningSubmarine);
   ```
   Ne pas toucher à `InitializeFromDoorDef` (legacy, laisser mourir).

4. **SubHullComponent.h** : nouvelle UFUNCTION
   ```cpp
   UFUNCTION(BlueprintCallable, Category = "Hull|Repair")
   bool ClearBreachNearLocation(const FVector& WorldLocation, float Radius);
   ```

5. **Nouveau : `USubCheatManager`** (ou extension de `SubPlayerController`)
   ```cpp
   UFUNCTION(Exec) void DevCheat_CreateBreach(FName CompartmentId, float RateLps);
   UFUNCTION(Exec) void DevCheat_SetDoorClosed(FName ConnectionId, bool bClosed);
   UFUNCTION(Exec) void DevCheat_SetFloodLevel(FName CompartmentId, float Level01);
   UFUNCTION(Exec) void DevCheat_RepairAllBreaches();
   UFUNCTION(Exec) void DevCheat_TeleportToCompartment(FName CompartmentId);
   ```

6. **Nouveau BP_Airlock** (contenu, pas code)
   - Créé dans le Content Browser
   - Assigné dans `SubmarineBase::AirlockActorClass` via `DefaultGameMode` ou l'asset Submarine

### Ce que l'architecture resserrée N'introduit PAS
- Pas de nouveau module C++
- Pas de nouveau composant
- Pas de nouveau manager
- Pas de nouvelle interface
- Pas de nouveau path runtime
- Pas de nouvelle delegation / message bus

Tu ajoutes 3 fonctions, 2 UPROPERTY, 1 BP, 1 cheat manager. Total : ~400 lignes de code, peut-être moins.

---

## 7. Ordre d'implémentation recommandé

Chaque étape est testable individuellement en PIE. Pas de commit batch. Pas d'étape qui démarre avant la validation de la précédente.

### Milestone 1 — Form correct (scope du doc authoritaire)

**Étape 1.1** : Investigation pollution visuelle (doc §6.2 étapes 1-2)
- Ajouter logs V/T dans `BuildMeshData` (sections)
- Ajouter toggles `bBuildExteriorHull`, `bBuildInterior`, `bBuildBulkheads` sur `USubmarineGeneratedGeometryComponent`
- Test `Show > Collision` en viewport
- **Pass** : root cause parmi les 4 hypothèses identifié et documenté dans le backlog

**Étape 1.2** : Envelope refactor (doc §3)
- Retirer `RadiusProfile` de `SubmarineGeneratorEnvelopeDef.h`
- Ajouter `BowCapLengthCm`, `SternCapLengthCm`, `BowSharpness`, `SternSharpness`, `BodyLengthFraction`, `ControlRingCount`
- Réécrire `EvaluateRadius`, `EvaluateBowSternTaper`
- **Pass** : projet compile, `Generate()` produit un Definition avec `HullLengthCm` cohérent

**Étape 1.3** : Bow/stern caps paramétriques (doc §4)
- Remplacer le hardcode `FMath::Min(BowRadius * 0.8f, 80.f)` lignes 291, 352
- Appliquer `ApplyBowProfile` / `ApplySternProfile`
- Audit winding corps vs caps
- **Pass** : forme cigare visible en PIE, `BowCapLengthCm` et `BowSharpness` ont un effet observable

**Étape 1.4** : Door cutout 3-bandes (doc §5.2)
- Réécrire `BuildBulkheads` avec l'approche 3 sous-rectangles
- Abandonner fan-from-pivot
- **Pass** : doors visibles avec `FloorDropBiasCm ∈ {0, 40, 80}`

**Étape 1.5** : Matériaux two-sided (doc §4.3, §5.1)
- Matériau hull two-sided
- Matériau sol two-sided
- **Pass** : pas de transparence indésirable en PIE sous angles divers

**Étape 1.6** : Render/collision separation
- 1 PMC collision global avec convex decomposition, `SetVisibility(false)`
- N PMC render sans collision (`NoCollision`)
- **Pass** : `Show > Collision` OFF → pas de pollution visuelle ; ON → collision visible

**Milestone 1 clos quand** : toutes les étapes 1.1-1.6 passées, form correct en PIE, pas de régression sur stations/flood/crew spawn.

### Milestone 2 — Interactions de base

**Étape 2.1** : Spawn de doors depuis Definition
- `SpawnDoorsFromDefinition()` dans `SubmarineBase::BeginPlay`
- Nouvelle surcharge `ASubDoorActor::InitializeFromConnectionDef`
- **Pass** : en PIE, chaque connection non-airlock a un ASubDoorActor à la bonne place, le prompt interact apparaît, appuyer sur la touche toggle l'état, `SubFlood::SetDoorState` est bien appelé

**Étape 2.2** : BP_Airlock + spawn
- Créer `BP_Airlock` avec 2 doors enfants, mesh, collision, flange, light
- `SpawnAirlockFromDefinition()` dans `BeginPlay`
- Initialiser les 2 doors enfants avec les `FGeneratedConnectionDef` correspondantes
- **Pass** : en PIE, sas visible sur le flanc, 2 doors fonctionnelles, crew peut cycler (entrer, fermer inner, ouvrir outer, voir eau monter)

**Étape 2.3** : Console cheats
- `USubCheatManager` avec les 5 exec UFUNCTION
- **Pass** : chaque cheat fonctionne individuellement en PIE

**Étape 2.4** : Repair mechanism
- `USubHullComponent::ClearBreachNearLocation`
- Spawn dynamique d'`AInteractablePoint` au point de brèche
- Prompt interact + key → clear breach → `SubFlood::RemoveBreach` cascade
- **Pass** : `DevCheat_CreateBreach → voir eau monter → s'approcher → prompt repair → interact → eau arrête`

**Milestone 2 clos quand** : scénario breach→flood→repair testable avec console cheats, cycle sas fonctionnel, doors interactables.

### Milestone 3 — Validation FP

**Étape 3.1** : `RebuildFromSpec` et `ClearGeneratedState` (`CallInEditor`)
- **Pass** : modif Spec → Rebuild → forme mise à jour sans recompile

**Étape 3.2** : Nettoyage warnings [LEGACY] sur path principal
- Identifier les fallbacks touchés en exécution normale
- Les marquer DEPRECATED + UE_LOG bruyant, ou les retirer du path principal
- **Pass** : log PIE silencieux sauf warnings intentionnels

**Étape 3.3** : Protocole FP 14 critères (section 5.E)
- Exécution complète en une session PIE
- **Pass** : 14 critères cochés en une seule passe

**Milestone 3 clos quand** : les 14 critères passent, et tu peux les refaire passer demain sans ajustement.

### Milestone 4 — Polish minimum

**Étape 4.1** : Lighting intérieur basique
- 1 PointLight spawnée au centre de chaque compartment dans `SubmarineBase::BeginPlay`
- **Pass** : intérieurs lisibles en PIE

**Étape 4.2** : Backlog post-FP documenté
- `reports/backlog/post_fp_debt.md` listant : winding audit, UV mapping, materials production, GeneratorEditor, StructuralSheets, etc.
- **Pass** : toutes les dettes connues listées

**Milestone 4 clos quand** : le FP est jouable ET le backlog post-FP est explicite.

---

## 8. Plan exécutable vers FP

Formatté pour envoi à un implémenteur (humain ou agent).

### Plan M1 — Form correct

| # | Titre | Fichiers | Entrée | Sortie | Pass criterion |
|---|---|---|---|---|---|
| 1.1 | Logs V/T + toggles isolation | `SubmarineMeshBuilder.cpp`, `GeneratedGeometryComponent.h/.cpp` | État actuel | Logs dans `BuildMeshData`, toggles sur component | Root cause pollution identifié |
| 1.2 | Envelope refactor | `SubmarineGeneratorEnvelopeDef.h/.cpp` | État actuel | `RadiusProfile` retiré, 6 nouveaux champs, `EvaluateRadius` réécrit | Projet compile, `Generate()` OK |
| 1.3 | Caps paramétriques | `SubmarineMeshBuilder.cpp` (lignes 291, 352, 150-413) | 1.2 | `BuildExteriorHull` utilise envelope caps | Forme cigare en PIE, caps contrôlables |
| 1.4 | Door cutout 3-bandes | `SubmarineMeshBuilder.cpp` (lignes 715-951) | 1.2 | `BuildBulkheads` réécrit avec 3 sous-rectangles | Doors visibles pour FloorDropBias ∈ {0, 40, 80} |
| 1.5 | Materials two-sided | Content (materials hull, floor) | 1.3, 1.4 | Matériaux two-sided | Pas de transparence indésirable |
| 1.6 | Render/collision split | `GeneratedGeometryComponent.cpp` | 1.1-1.5 | PMC collision séparé, invisible, convex | `Show > Collision` OFF → pas de pollution |

### Plan M2 — Interactions

| # | Titre | Fichiers | Entrée | Sortie | Pass criterion |
|---|---|---|---|---|---|
| 2.1 | Door spawn | `SubmarineBase.h/.cpp`, `SubDoorActor.h/.cpp` | M1 | `SpawnDoorsFromDefinition`, `InitializeFromConnectionDef` | Doors spawnées aux bons transforms, interact OK |
| 2.2 | BP_Airlock + spawn | `SubmarineBase.h/.cpp`, Content/`BP_Airlock` | 2.1 | `SpawnAirlockFromDefinition`, BP créé | Cycle sas fonctionnel |
| 2.3 | Console cheats | `SubPlayerController.cpp` ou `SubCheatManager.h/.cpp` | M1 | 5 exec UFUNCTION | Chaque cheat testable en PIE |
| 2.4 | Repair | `SubHullComponent.h/.cpp`, `AInteractablePoint` (existant ?) | 2.3 | `ClearBreachNearLocation`, spawn interact | breach→flood→repair testable |

### Plan M3 — Validation

| # | Titre | Fichiers | Entrée | Sortie | Pass criterion |
|---|---|---|---|---|---|
| 3.1 | RebuildFromSpec | `SubmarineBase.h/.cpp` | M2 | `UFUNCTION(CallInEditor)`, full rebuild chain | Modif Spec → clic → forme mise à jour |
| 3.2 | [LEGACY] cleanup | Path principal | M2 | Fallbacks marqués DEPRECATED ou retirés | Log PIE silencieux |
| 3.3 | Protocole FP | Aucun fichier | 3.1, 3.2 | 14 critères cochés | Passe complète en une session PIE |

### Plan M4 — Polish

| # | Titre | Fichiers | Entrée | Sortie | Pass criterion |
|---|---|---|---|---|---|
| 4.1 | Lighting intérieur | `SubmarineBase.cpp` (spawn lights) | M3 | PointLight par compartment | Intérieurs lisibles |
| 4.2 | Backlog post-FP | `reports/backlog/post_fp_debt.md` | M3 | Document créé | Dettes listées |

**Règles d'exécution** :
- Chaque étape = 1 commit (ou 1 PR si tu en fais)
- Aucune étape ne dépasse 2 jours ouvrés sans réévaluation
- Si blocage technique > 4h sur une étape, stop, re-scoper, poser des questions
- Pas de refactor non-planifié pendant l'exécution d'une étape
- Un agent implémenteur reçoit ce plan étape par étape, jamais en batch

---

## 9. Liste explicite de ce qui doit être repoussé

### POST-FP obligatoire (pas maintenant)

1. **GeneratorEditor avec gizmos/control rings** (doc §7) — projet à part entière, ne jamais démarrer avant FP clos
2. **Blending procédural airlock↔hull** — toxique pour solo dev, BP_Airlock suffit indéfiniment
3. **Asset editor custom pour USubmarineDefinition** — UPROPERTY standard suffit
4. **UV mapping propre pour texturing** — two-sided materials en attendant
5. **Winding audit exhaustif** — two-sided couvre ça
6. **Material pipeline production** — default UE suffit pour FP
7. **LODs** — n'existe pas de crise de perf pour un solo dev en phase FP
8. **StructuralSheets / fine grain damage** — damage model actuel suffit
9. **Multiple decks** — un deck suffit pour FP
10. **Simulation hydrodynamique navale avancée** — simple mass+thrust suffit
11. **Narrative hooks, NPCs, objectives** — gameplay loop suffit pour FP
12. **Variantes de sous-marins** — un seul sub suffit pour FP
13. **Réseau multiplayer >1 client** — replication existe déjà mais tester seul pour FP
14. **Éditeur de sous-marin exposé au joueur** — hors scope définitif de FP

### POST-M1 mais peut-être PRE-M4 (si budget)

1. Ambient sound par compartment (1 audio source par compartiment)
2. VFX jet d'eau au breach
3. HUD compartment indicator (texte en haut à droite : "HELM - Dry")
4. Pump UI (bouton interact sur les pumps)

### Prématuré quoi qu'il arrive (ne jamais faire pour FP)

1. Refonte `USubmarineEnvelopeDef` dépendance `SubCompilerTypes.h` — dette acceptée
2. Déplacement de `FSubmarineMeshSectionData` hors de `SubmarineGeometryBuilder.h` — dette acceptée
3. Suppression du module SubCompiler — c'est de la lecture seule, laisser
4. Refactor `SubHullComponent` pour retirer flood — déjà planifié en Phase 6 d'un autre plan, POST-FP
5. Migration `SubmarineCompartmentComponent` en façade lecture SubFlood — POST-FP

---

## 10. Red flags / dérives à surveiller

Si tu te retrouves dans une de ces situations, c'est un signal que tu es en dérive. Stop, recentre, reprends le plan.

### Signaux de dérive architecture/outils

1. **Tu ouvres un fichier pour fix un bug, tu finis par refactoriser 3 classes** → STOP, revert, fix localisé
2. **Tu écris un nouveau header "pour garder les choses propres"** alors qu'un `EditCondition` dans un UDataAsset existant aurait suffi → STOP, re-scope
3. **Tu te retrouves à justifier un système par "on en aura besoin plus tard"** → flag rouge, reporter en post-FP
4. **Tu penses à écrire un asset editor custom** → STOP, relire §5.C de cette analyse
5. **Tu remarques qu'un fix a cassé le fallback legacy et tu veux "le bien faire"** → STOP, le fallback doit être retiré pas réparé
6. **Tu passes 2h+ sur un problème de winding sur un cap cosmétique** → STOP, two-sided en attendant, reporter après FP
7. **Tu commences à lire du Slate pour comprendre comment ajouter un slider** → STOP, utiliser UPROPERTY + CallInEditor
8. **Tu te retrouves à penser "il faut que le générateur supporte X avant de pouvoir tester Y"** → STOP, hardcoder Y temporairement, noter la dépendance
9. **Tu commences à écrire de la doc architecture sur un sous-système en dehors du scope M1-M4** → STOP, le doc n'est pas le blocker, le code l'est
10. **Tu te retrouves à modifier plus de 5 fichiers dans la même étape du plan** → STOP, l'étape est trop large, re-découper
11. **Un composant runtime inclut `ProceduralMeshComponent.h` alors qu'il n'est pas `GeneratedGeometryComponent`** → fuite architecturale, la seule classe autorisée à parler au mesh est la couche materialization. Refactor immédiatement.
12. **Une fonction gameplay lit des données mesh directement** (`GetVertex`, `GetMeshSection`, etc.) au lieu de lire `USubmarineDefinition` → idem, fuite. Le gameplay lit le Definition, jamais la mesh.
13. **Tu te dis "il faudrait passer à Dynamic Mesh pour éviter l'expérimental"** pendant M1-M4 → NON. Rester sur PMC pour FP. Migration post-FP si vraiment nécessaire.

### Erreurs typiques à éviter

1. **Refactorer pendant qu'on développe** : décorréler. Un refactor = un commit dédié, jamais "pendant que j'y suis"
2. **Laisser le path legacy et le nouveau coexister longtemps** : chaque semaine de coexistence double le coût de la migration
3. **Tester en prod sans scénarios reproductibles** : console cheats sont obligatoires AVANT les features gameplay
4. **Se laisser attirer par features secondaires** (lighting, sound, VFX) avant d'avoir le core testable
5. **Écrire des fonctions helper "génériques"** qui ne sont utilisées qu'une fois : toujours la fonction spécifique d'abord, généraliser SEULEMENT quand le 3ème usage arrive
6. **Fixer un problème en mettant un matériau two-sided PARTOUT** : la règle est par asset, documenter chaque cas
7. **Planifier Phase N+1 avant d'avoir validé Phase N** : tu finis par planifier dans le vide (cas de Phase 5D inserée alors que le composant existait)
8. **"Juste une petite amélioration"** qui prend 2h au lieu des 15 minutes annoncées : STOP après 30 minutes, si pas fini, noter et passer

### Où l'IA / agents aident

1. **Refactoring mécanique avec pattern clair** (rename, move, add param cohérent) : rapide, faible risque, donne au solo dev un boost 3-5x
2. **Génération de boilerplate console cheats** : rapide, fiable
3. **Écriture de tests unitaires sur fonctions math pures** (mesh builder, envelope evaluation) : utile post-FP
4. **Lecture/audit de code pour identifier des drifts** (comme cette session) : excellent
5. **Génération de logs debug boilerplate** : utile
6. **Explication d'un fichier legacy inconnu** : très utile pour reprendre contact

### Où l'IA / agents aggravent le problème

1. **Design décisions larges** : les agents proposent des solutions "élégantes" sur-dimensionnées. **Tu dois arbitrer toi-même**, ou leur dire explicitement "réponds en mode FP minimum, solution la plus simple qui marche, rien de plus"
2. **Multi-fichier refactors simultanés** : ils perdent la cohérence inter-fichiers et introduisent des bugs subtils qui sortent en PIE 2 jours plus tard
3. **Écriture de nouveaux systèmes from scratch sans contrainte d'intégration** : ils inventent des APIs qui ne matchent pas le reste
4. **Écriture d'éditeur custom** : ils adorent ça, c'est le piège absolu
5. **Décisions de scope** ("il faut aussi X pour être propre") : **ne jamais les laisser décider du scope**. Le scope, c'est toi qui le tiens.
6. **Planification de phases sans lire l'état réel du code** : symptôme vu dans cette session, à éviter. Avant toute planification, un agent doit lire l'état actuel.

**Règle opérationnelle** : utiliser les agents pour exécution, jamais pour arbitrer. Les arbitrages tu les fais après lecture de leurs analyses. Pour chaque proposition d'un agent, te demander : "est-ce que ça ferme mon FP ou est-ce que ça l'éloigne ?". Si éloigne, reject.

**Pattern d'utilisation recommandé pour agents** :
- Explore agents : lecture/audit, très bon
- Plan agents : génération de plans d'exécution détaillés à partir d'analyses humaines, bon
- Implémentation agents : exécution étape par étape d'un plan déjà validé, bon
- Jamais : "analyse et décide le scope", "choisis entre A et B", "propose l'architecture qui convient"

---

## 11. Verdict final

**APPROCHE VIABLE SOUS CONDITIONS.**

Ton architecture est bonne. La chaîne runtime fonctionne. Les composants clés existent et sont testés par leur fonctionnement observable en PIE. Le projet est significativement plus avancé que tes inquiétudes ne le suggèrent. La majorité du travail restant pour un FP est du branchement, pas de la conception.

### Les conditions (non négociables)

1. **Respecter le scope du doc authoritaire comme M1** (form only), et le faire en entier avant de passer à autre chose. Pas de "petite amélioration" hors scope pendant M1.

2. **Ne jamais démarrer le GeneratorEditor §7** avant d'avoir un FP clos, joué, et validé par le protocole 14 critères.

3. **Sas = BP_Airlock préfab attaché, pas mesh généré.** Cette décision ferme une surface de régression énorme.

4. **Accepter les dettes temporaires** (two-sided materials, fallbacks legacy coexistants, pas de winding audit, pas d'UV propre) et les documenter en post-FP backlog explicite.

5. **Console cheats AVANT features gameplay complexes.** Ce n'est pas un choix, c'est une pré-condition à l'itération rapide.

6. **Jalonnement strict M1 → M2 → M3 → M4 dans l'ordre.** Pas de parallélisation, pas de "je prends de l'avance sur M3 pendant que M1 marine".

7. **Quand tu hésites entre "architecture plus propre maintenant" et "hack qui marche maintenant" → toujours le hack, noter la dette, passer à la suite.**

8. **Chaque étape du plan = testable en PIE individuellement. Si une étape ne l'est pas, elle est mal découpée.**

9. **Backend mesh interchangeable.** Le gameplay ne lit jamais la mesh directement. Seul `GeneratedGeometryComponent` parle à PMC. Quand Epic dépréciera PMC (ou le mettra non-experimental, ou sortira une version 3), tu pourras migrer sans toucher au gameplay.

10. **Pas d'AI crew, pas de NavMesh dynamique avant FP clos.** Si la gameplay vision glisse vers "il faudrait des NPCs", c'est du M5+ post-FP strict.

### Ce qui n'est PAS le problème

- Ton architecture. Elle tient.
- Ton découpage de types. Il est correct.
- Ta séparation topologie/géométrie (Generator vs MeshBuilder). Elle est justifiée.
- Ta vision gameplay. Elle est réaliste et resserrée.
- Ton choix de tech (C++ + data-driven + procedural mesh). Il est bon pour le problème.

### Ce qui EST le problème

- Le temps passé à perfectionner le générateur au lieu de fermer les gaps gameplay (doors spawn, airlock mesh, repair).
- La tentation récurrente de tooling (GeneratorEditor, asset editors custom, gizmos) qui n'apporte aucun ROI avant FP.
- Le risque psychologique d'une refonte permanente qui bloque le projet dans sa zone grise "architecture-qui-marche-mais-jeu-qui-ne-se-joue-pas".
- L'écart entre le code réel et les plans (exemple : Phase 5D insérée alors que GeneratedGeometryComponent existait déjà).

### La vérité dure

**Ton FP est à 4 jalons de distance, pas à 4 architectures.** Chaque fois que tu démarres une architecture au lieu d'un jalon, tu repousses le FP d'une semaine minimum. Chaque fois que tu fermes un jalon même imparfait, tu te rapproches.

Tu as tout ce qu'il faut pour fermer M1-M4 en temps raisonnable. Le seul variable qui compte maintenant est la discipline de scope.

---

## Verification (quand le plan est exécuté)

Pour vérifier que le plan mène réellement au FP :

1. **Après M1** : lancer PIE, vérifier que le sous-marin est visible avec la bonne forme, les doors sont visibles dans les bulkheads (cutouts fonctionnels), pas de pollution visuelle, pas de transparence backface.

2. **Après M2** : en PIE, taper `DevCheat_CreateBreach <Id> 500` dans la console → voir l'eau monter → marcher vers la brèche → voir le prompt repair → appuyer → l'eau arrête.

3. **Après M3** : exécuter les 14 critères du protocole FP (section 5.E) en une session PIE unique. Screencast pour documentation.

4. **Après M4** : relire `reports/backlog/post_fp_debt.md` pour s'assurer que toutes les dettes connues sont listées et non oubliées.

**Commandes de vérification rapide** (à placer dans un console cheat ou un skill dev) :
- `DevCheat_RunFPProtocol` qui automatise les scénarios 8-13 du protocole FP dans l'ordre avec timings
- `stat flood` pour monitorer les niveaux d'eau en direct
- `show Collision` pour vérifier la séparation render/collision

---

## Addendum — Intégration du rapport deep research (2026-04-10)

Cette analyse a été enrichie par lecture d'un rapport de recherche externe (`deep-research-report submarine.md`). Les points suivants proviennent du rapport et ont été intégrés aux sections ci-dessus :

**Findings intégrés (modifient le plan)**

1. **`UProceduralMeshComponent` est expérimental en UE 5.7** (confirmé par docs Epic citées dans le rapport). Implication : architecture doit abstraire le backend mesh. Voir section 3 point 13 et section 5.A sous-section "backend mesh interchangeable".
2. **`UDynamicMeshComponent` comme alternative future** alignée Geometry Script / Modeling Tools. Pas pour FP, mais documenté comme voie de migration post-FP.
3. **`GeneratedDynamicMeshActor`** (editor-only) existe spécifiquement pour rebuilds procéduraux en éditeur. Référencé au niveau 5 du path d'escalade éditeur en section 5.C.
4. **EUW + `UDetailsView` comme step-up concret** entre CallInEditor et editor mode custom. Intégré comme niveau 1 du path d'escalade en section 5.C.
5. **Component Visualizers** comme technique "cheap" pour dessiner des handles sans mode éditeur complet. Intégré comme niveau 2.
6. **Rebuild throttling obligatoire** (débounce 0.2-0.5s) pour tout rebuild lié à des sliders live. Intégré dans le gotcha en section 5.C.
7. **NavMesh / AI crew** comme décision explicite : pas pour FP. Intégré en section 5.D.
8. **Geometry Script `Apply Mesh Boolean`** existe pour bake editor-only, pas runtime. Intégré en section 5.B variante post-FP.
9. **Terminologie "collar"** au lieu de "flange" pour la collerette airlock. Aligné avec la littérature UE.

**Findings du rapport NON intégrés (écarts de position assumés)**

1. **Rapport suggère EUW au niveau initial, pas CallInEditor.** Je maintiens CallInEditor pour FP. Justification : EUW ajoute une couche Slate/UMG, même minimale, qui crée une surface de maintenance. Pour un solo dev en phase FP, Details panel + bouton clic-droit suffit pendant M1-M4. Le rapport est correct que EUW est rapide à construire, mais "rapide" ≠ "nécessaire maintenant".

2. **Rapport propose "collar + cut intérieur" comme option viable FP** pour l'airlock. Je maintiens "BP_Airlock full prefab" pour FP. Justification : le "cut intérieur" réintroduit la complexité polygon-clipping que le doc §5.2 essaie précisément d'éviter via l'approche 3-bandes. Faire un cut procédural ailleurs que dans les bulkheads, c'est maintenir deux algorithmes de cut différents. Trop cher pour FP solo dev.

3. **Rapport mentionne option de bake vers Static Mesh** comme backend alternatif stable. Je ne l'intègre pas car : (a) ça ajoute une étape de bake dans le pipeline, (b) ça complique l'itération forme, (c) ça n'est nécessaire que si PMC devient vraiment problématique, et (d) c'est une décision M5+ post-FP.

4. **Rapport évoque Navigation Invokers** comme solution pour navmesh sur géométrie dynamique. Je le reporte strictement post-FP en excluant l'AI crew du scope FP.

**Points où ma position est plus tranchée que le rapport**

1. Le rapport dit "approche viable sous conditions" mais les conditions sont plus abstraites (focus, EUW minimal, backend abstrait). Ma version resserre à 8 conditions opérationnelles et non négociables (section 11).

2. Le rapport liste les red flags comme "signaux de dérive". Ma version ajoute 13 signaux opérationnels avec actions correctives immédiates.

3. Le rapport propose un plan d'exécution par thèmes. Ma version propose 4 milestones M1-M4 avec pass criteria binaires par étape et critères de sortie par milestone.

4. Le rapport ne s'engage pas sur l'ordre exact des étapes gameplay (doors, airlock, repair, cheats). Ma version tranche l'ordre dans le plan M2.

**Dette documentée du rapport à traiter post-FP**

Les points suivants sont légitimes mais différés explicitement :
- Migration PMC → Dynamic Mesh ou Static Mesh bake
- GeneratorEditor avec niveau 1+ (EUW, visualizers, editor mode)
- Navigation Invokers pour AI crew
- Apply Mesh Boolean pour airlock stitching procédural
- Audit backend fuite architecturale (à faire en M3, pas plus tard)

---

## Critical files (pour implémenteurs)

À lire dans cet ordre par tout implémenteur avant de toucher à quoi que ce soit :

1. `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubmarineBase.h` + `.cpp` — wiring runtime principal
2. `C:\Dev\Sub3D\Source\Sub3D\Submarine\Generator\SubmarineDefinition.h` — contrat runtime
3. `C:\Dev\Sub3D\Source\Sub3D\Submarine\Generator\SubmarineGenerator.cpp` — Generate() pipeline
4. `C:\Dev\Sub3D\Source\Sub3D\Submarine\Generator\SubmarineMeshBuilder.cpp` — bugs à fixer lignes 291, 352, 804
5. `C:\Dev\Sub3D\Source\Sub3D\Submarine\GeneratedGeometry\SubmarineGeneratedGeometryComponent.cpp` — ApplySectionToPMC, CreateMeshSection_LinearColor
6. `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubFloodComponent.h` — API flood
7. `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubDoorActor.h` — door actor (ajouter InitializeFromConnectionDef)
8. `C:\Dev\Sub3D\Source\Sub3D\Submarine\SubHullComponent.h` — ajouter ClearBreachNearLocation
9. `C:\Dev\Sub3D\reports\plans\2026-04-08_submarine_generator_architecture_unified.md` — scope authoritaire M1

Tout agent implémenteur doit lire au moins les points 1, 4, 5 et 9 avant de proposer du code sur ce projet.

---

## Décisions post-rédaction

### 2026-04-18 — Pipelines submarine

- **Bake pipeline** (`Sub3DBake/`) : **en pause**, legacy Proto03/04. Ne pas toucher. Ne pas ajouter de dette dessus.
- **Generator pipeline** (`Sub3D/Submarine/Generator/`, `SubmarineGeneratedGeometryComponent`) : **en pause** jusqu'à post-FP. Le code reste, on n'investit plus dessus pendant le First Playable.
- **Craniata scripted** (sub handmade généré par script Blender + FBX → BP manuel) : **pipeline de production** pour le First Playable et tout travail immédiat. Toute nouvelle feature runtime (damage, feedback, outils, mob) cible Craniata.

Règle pratique : si un bug se manifeste dans le Generator ou le Bake pendant la phase FP, ne pas le corriger. Le noter dans `reports/backlog/post_fp_debt.md` (à créer si absent) et passer à autre chose.
