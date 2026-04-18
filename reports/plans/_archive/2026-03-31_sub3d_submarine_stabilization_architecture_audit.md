# Sub3D - Audit Architecture & Plan de Stabilisation Sous-Marin

Date: 2026-03-31
Statut: Document de reference pour implementation

---

## 1. DIAGNOSTIC DE L'EXISTANT

### 1.1 Arbre de composants runtime

```
ASubmarineCompilerActor (herite ASubmarineBase : APawn)
 |
 +-- HullMesh (UStaticMeshComponent) .............. Root component, profil SubmarineHull
 |    +-- ExteriorCollisionProxy (UBoxComponent) .. Profil SubmarineHull, sweep de mouvement
 |    +-- HelmSocket (USceneComponent) ............ Point de reference helm
 |    +-- CrewSpawnSocketP1 (USceneComponent) ..... Point de spawn crew
 |    +-- TurretHardpoint (USceneComponent) ....... Point de montage tourelle
 |
 +-- SubMovement (USubMovementComponent) .......... Physique math-based, fixed-tick 30Hz
 +-- InteriorFrame (USubInteriorFrameComponent) ... Referentiel interieur, delta par frame
 +-- SubHull (USubHullComponent) .................. Verite structurelle / flooding
 +-- Systems (USubmarineSystemsComponent) ......... Command state hub
 +-- Compartments (USubmarineCompartmentComponent)  Runtime flooding
 +-- StationManager ............................... Stations discovery
 +-- Radar, Sonar, SonarSystem .................... Capteurs
 +-- TunnelNavigationRuntime ...................... Navigation tunnel
 +-- HelmNavigationDisplay ........................ Affichage navigation helm
 +-- BreachVfxManager, FloodWaterVisuals .......... VFX
 +-- FeedbackManager .............................. Feedback haptic/audio
 |
 +-- [Runtime generated components]
 |    +-- PMC_ExteriorHull (ProceduralMesh) ....... Coque exterieure, profil SubmarineHull, QueryOnly
 |    +-- PMC_Interior_* (ProceduralMesh) ......... Murs + sol par compartiment, profil SubInteriorWalkable, QueryOnly
 |    +-- PMC_Bulkhead_* (ProceduralMesh) ......... Cloisons entre compartiments, profil SubInteriorWalkable, QueryOnly
 |
 +-- [Attached child actors]
      +-- ASubDoorActor* .......................... Portes, profil BlockAll + ECC_GTC1 Ignore
      +-- ASubCompilerPlaceholderStation* ......... Stations, QueryOnly, ignore all sauf Visibility/Camera
```

### 1.2 Pipeline de compilation du sous-marin

```
EnvelopeDef + FunctionalGraph
       |
       v
  LayoutSolver.Solve()
       |
       v
  FSubmarineLayoutSolution (compartments, bulkheads, stations, doors, metrics)
       |
       v
  BuildCompiler.CompileToLayoutAsset() --> USubmarineLayoutAsset
       |
       v
  GeometryBuilder.BuildInteriorMeshes() --> PMC_Interior_*, PMC_Bulkhead_*
  GeometryBuilder.BuildExteriorMesh()   --> PMC_ExteriorHull
       |
       v
  RefreshExteriorCollisionProxy() --> UBoxComponent fitted
  BuildGeneratedDoors()           --> ASubDoorActor* spawned + attached
  BuildGeneratedStations()        --> ASubCompilerPlaceholderStation* spawned + attached
```

### 1.3 Systeme de collision actuel

#### Channels (DefaultEngine.ini)

| Channel | Nom | Default |
|---|---|---|
| ECC_GameTraceChannel1 | Submarine | ECR_Block |
| ECC_GameTraceChannel2 | SubInterior | ECR_Ignore |

#### Profils

| Profil | Enabled | ObjectType | Blocks | Ignores |
|---|---|---|---|---|
| SubmarineHull | QueryAndPhysics | Submarine | WorldStatic, WorldDynamic, Visibility, Camera, Submarine | **Pawn, SubInterior** |
| SubInteriorWalkable | QueryOnly | SubInterior | WorldStatic, WorldDynamic, Pawn, Visibility, Camera | **Submarine, SubInterior** |
| SubInteriorVisual | QueryOnly | SubInterior | WorldStatic, WorldDynamic, Pawn, Visibility, Camera | **Submarine, SubInterior** |

#### Contrat collision par composant

| Composant | Profil | Collision | Role |
|---|---|---|---|
| ExteriorCollisionProxy | SubmarineHull | QueryAndPhysics | Sweep de mouvement du sub |
| PMC_ExteriorHull | SubmarineHull | QueryOnly | Visuel coque + traces |
| PMC_Interior_* | SubInteriorWalkable | QueryOnly | Sol/murs walkable crew |
| PMC_Bulkhead_* | SubInteriorWalkable | QueryOnly | Cloisons walkable crew |
| Capsule crew | Pawn | QueryAndPhysics | Ignore Submarine, Block SubInterior |
| DoorMesh | BlockAll + GTC1=Ignore | QueryOnly (ferme) / NoCollision (ouvert) | Bloque crew si ferme |

### 1.4 Mouvement du sous-marin (SubMovementComponent)

Architecture: physique math-based custom, pas Chaos/PhysX.

- **Fixed-tick**: 30 Hz (SimAccumulator + FixedSimDt)
- **Modele**: buoyancy + ballast + thrust + drag + rudder + pitch (hydroplanes)
- **Drag custom**: DynamicDragX rampe a haute valeur quand idle pour stabiliser
- **Keel effect**: transfert lateral -> forward pour ne pas s'arreter net en virage
- **Sweep**: MoveWithSlide lambda, horizontal + vertical separes
- **Client**: interpolation snapshot (PrevSnapshot -> TargetSnapshot, alpha-based)

**Fix applique cette session**: `IsInternalHit` guard dans MoveWithSlide pour ignorer les hits contre les composants/acteurs enfants du sous-marin. Empeche la depenetration violente qui propulsait le sub lateralement au Departure.

### 1.5 Crew character (SubCrewCharacter + SubCrewMovementComponent)

- **ASubCrewCharacter** : herite ACharacter, FPS camera, capsule Pawn
- **USubCrewMovementComponent** : herite UCharacterMovementComponent (stock CMC)
- **Embark**: `EnterOnFootInSubmarine()` teleporte + stop + init + refresh floor + safety snap
- **Floor snap de secours**: `FindInteriorWalkableHit()` trace vers le bas sur ECC_GTC2, filtre par `GetInteriorWalkableComponents()`
- **Support state**: `bHasValidEmbarkedFloor`, `bHasAcceptedEmbarkedBase`, `SupportQuality01`, `bNeedsEmbarkedFloorRecovery`
- **Floor recovery**: periodique (`FloorRecoveryIntervalSeconds = 0.2s`) si floor perdu
- **Brace support**: trace laterale pour detecter un mur proche (embodiment feel)
- **Inertial state**: lecture linear/angular velocity/acceleration depuis InteriorFrame
- **Yaw compensation**: applique delta yaw du sub au controller rotation
- **Based movement**: stock CMC `UpdateBasedMovement` + `UpdateBasedRotation` re-actives

### 1.6 InteriorFrame

- Tick apres SubMovement (prerequisite)
- Compute delta location/rotation par frame
- Derive linear/angular velocity et acceleration en espace local
- Conversions World <-> Local

### 1.7 Bootstrap (GameMode)

Pipeline strict ESubBootstrapPhase:
```
None -> WorldReady -> SubResolved -> SubValidated -> CrewSpawned -> CrewEmbarked -> Ready
```

- Separation pipeline A (world/sub) et pipeline B (crew)
- Freeze sub pendant Boarding
- BeginDeparture unfreeze puis advance a Departure

---

## 2. PROBLEMES MAJEURS IDENTIFIES

### P1. Enveloppe exterieure — Geometrie degradee [CRITIQUE]

**Constat (cf. screenshots)**:
- Facettage visible tres prononce sur la coque exterieure
- Double coque: on voit la shell interieure a travers la coque exterieure
- Bow cap et stern cap avec artefacts geometriques
- Normals incoherents sur certaines faces (faces noires vs eclairees)
- L'offset entre coque interieure et exterieure est de seulement 5 cm (`ExteriorHullOffsetCm = 5.f`)

**Cause racine dans SubmarineGeometryBuilder.cpp**:

1. **Exterior hull**: les vertex normals sont calcules per-vertex dans `AppendExteriorRing()` comme la direction outward de la superellipse. C'est correct localement mais les triangles entre rings adjacents ne partagent PAS de vertices — chaque ring genere ses propres points. Le smooth shading depend du fait que les normals sont coherents. Actuellement, les normals per-ring sont corrects mais le **winding order des triangles** peut causer des faces inversees selon l'orientation de la camera.

2. **Double hull visible**: la coque exterieure (PMC_ExteriorHull) et les murs interieurs (PMC_Interior_*) sont rendus simultanement. L'offset de 5 cm est insuffisant — a distance, les deux surfaces z-fight. De plus, les murs interieurs sont visibles depuis l'exterieur car il n'y a pas de backface culling force.

3. **Bow/Stern caps**: le cap est un simple fan de triangles convergeant vers un point central. Les normals de ce point sont tous identiques (-Forward ou +Forward) alors que les vertex autour ont des normals radiaux — discontinuite nette.

4. **Bulkheads**: `AppendBoxPrism()` genere un prisme avec des normals cardinaux (ForwardVector, -ForwardVector, etc.). La jointure avec les murs courbes du compartiment cree un gap visible — pas de vertex sharing entre bulkhead et wall mesh.

5. **Floor double-sided**: le floor genere 6 triangles (0,2,1 + 1,2,3 + 0,1,2 + 1,3,2) — c'est intentionnellement double-face, mais ca genere du z-fighting avec lui-meme sous certains angles.

### P2. Interieur walkable — Verite implicite [IMPORTANT]

**Constat**: le sol walkable est un sous-produit du mesh visuel interieur. Le mesh section 1 (FloorSection) dans chaque PMC_Interior_* sert a la fois de:
- surface visuelle
- surface de collision walkable (via bUseComplexAsSimpleCollision)

**Probleme**: la geometrie walkable EST la geometrie visuelle. Si on corrige le visuel (par ex. ajouter des details, courber le sol, supprimer le double-side), on casse potentiellement le support crew.

**Solution requise**: decouplage walkable vs visuel. Le walkable doit etre une couche logique deduit du layout compile, pas un effet secondaire du mesh.

### P3. Camera en referentiel monde [MOYEN]

**Constat**: `FPSCamera` est attachee au root component du character (la capsule), avec `bUsePawnControlRotation = true`. Le controller rotation est en referentiel monde.

**Compensation actuelle**: `ApplyYawCompensation()` dans SubCrewMovementComponent applique le delta yaw du sub au controller. Mais cela ne couvre que le yaw — le pitch et roll du sub ne sont PAS compenses.

**Consequence**: quand le sub pitch (hydroplanes), la camera semble basculer par rapport a l'interieur. L'horizon du joueur ne correspond plus au sol du sub.

### P4. Character feel FPS — Pas encore assume [MOYEN]

**Constat**: les controles sont stock CMC. Pas de:
- Headbob
- Step sounds
- Interaction feedback visuel
- Lean / peek
- Sprint / crouch modulaire
- Camera smoothing dans le referentiel sub

Ces elements ne sont pas bloquants mais essentiels pour un gameplay "jouable et lisible".

### P5. Spawn crew — Fiabilite residuelle [MINEUR]

Le socket Z est fixe a `FloorOffsetCm + 92`. Le floor snap de secours existe et fonctionne via `FindInteriorWalkableHit()`. Cependant:
- Si le trace ne trouve rien (mesh pas encore genere, timing), le crew spawne dans le vide
- Le contrat n'est pas garanti formellement: le GameMode valide `ValidateCrewBootstrap()` mais ne verifie que `GetMovementBase() != nullptr`

---

## 3. ARCHITECTURE CIBLE

### 3.1 Les 4 verites

| # | Verite | Composant | Profil | Collision | Consumers |
|---|---|---|---|---|---|
| V1 | Mouvement exterieur | ExteriorCollisionProxy | SubmarineHull | QueryAndPhysics | SubMovement sweep |
| V2 | Walkable interieur | WalkableFloor components (dedies) | SubInteriorWalkable | QueryOnly | Crew CMC FindFloor |
| V3 | Structure / flooding | SubHullComponent + LayoutAsset | n/a | n/a | Flooding, breach, compartments |
| V4 | Support crew | Crew capsule + V2 + InteriorFrame | Pawn + SubInterior | QueryAndPhysics | Character movement |

**Regle**: chaque systeme ne consomme QUE sa verite.
- SubMovement ne touche que V1
- Crew CMC ne touche que V2/V4
- SubHull ne touche que V3
- Le visuel est separable de toutes les verites

### 3.2 Separation mesh visuel vs collision

Architecture cible pour la geometrie compilee:

```
GeometryBuilder produit:
  1. ExteriorVisualMesh   — Visuel seulement, profil SubInteriorVisual ou NoCollision
  2. InteriorVisualMeshes — Murs/plafond visuels seulement
  3. InteriorFloorMeshes  — Sol uniquement, profil SubInteriorWalkable, QueryOnly
  4. BulkheadMeshes       — Cloisons visuelles + collision walkable si besoin
  5. ExteriorCollisionProxy — Box approchee, deja en place
```

Le point cle: le **floor walkable** doit etre un composant separe du mesh visuel des murs. Aujourd'hui, murs + floor sont dans le meme PMC (section 0 = walls, section 1 = floor). Le floor doit devenir son propre composant, ou au minimum rester identifiable comme surface walkable verifiee.

### 3.3 Camera en referentiel sous-marin

Architecture cible:

```
Option A (recommandee): Camera control en espace sub-local
  - Le controller rotation est converti en sub-local avant application
  - Le pitch/roll du sub sont automatiquement compenses
  - Le joueur regarde toujours "droit" par rapport au sol du sub

Option B (plus simple): Compensation pitch/roll en plus du yaw
  - Etendre ApplyYawCompensation() pour aussi compenser pitch et roll
  - Plus simple mais peut accumuler de la derive
```

Recommandation: Option A pour la stabilite long terme. La rotation du controller devrait etre exprimee dans le referentiel de l'InteriorFrame, pas dans le referentiel monde.

### 3.4 Enveloppe exterieure propre

Objectifs geometriques:
1. **Plus de double hull visible** — l'offset entre coque interieure et exterieure doit etre suffisant OU les murs interieurs ne doivent pas etre rendus depuis l'exterieur
2. **Normals lisses sur la coque** — vertex sharing entre rings adjacents, smooth normals
3. **Caps propres** — remplacer le fan naif par une tessellation progressive
4. **Facettage reduit** — augmenter les subdivisions radiales (15 -> 24-32) et longitudinales
5. **Winding coherent** — toutes les faces exterieures doivent pointer outward, verifiable par dot(normal, view) test

### 3.5 Hierarchy de tick

```
SubMovementComponent.Tick()
    |
    v
SubInteriorFrameComponent.Tick()  (prerequisite: SubMovement)
    |
    v
SubCrewMovementComponent.Tick()   (prerequisite: InteriorFrame + SubMovement)
```

Deja en place. Ne pas toucher.

---

## 4. PLAN D'IMPLEMENTATION

### Phase 1 — Verite collision exterieure [FAIT / EN COURS]

**Fichiers**: SubMovementComponent.cpp, SubmarineBase.h/.cpp, SubmarineCompilerActor.h/.cpp, DefaultEngine.ini

**Changements faits**:
- [x] IsInternalHit guard dans MoveWithSlide
- [x] GetInteriorWalkableComponents() API virtuelle
- [x] GetCrewEmbarkTransform() API virtuelle
- [x] Override dans SubmarineCompilerActor
- [x] Profil SubInteriorVisual ajoute dans DefaultEngine.ini
- [ ] Build non verifie

**Critere de fin**: le sub ne se cogne plus a son propre interieur au Departure.

### Phase 2 — Enveloppe exterieure propre

**Fichiers**: SubmarineGeometryBuilder.h/.cpp, SubmarineCompilerActor.cpp

**Changements a faire**:

1. **Augmenter ExteriorHullOffsetCm** de 5 a 15-20 cm pour eliminer le z-fight interieur/exterieur
2. **Smooth normals exterieurs**: partager les vertex normals entre rings adjacents au lieu de normals per-ring isoles
3. **Defaults preview**: passer `PreviewExteriorRadialSegments` de 15 a 24-32 pour reduire le facettage
4. **Bow/Stern caps**: remplacer le fan simple par une tessellation concentrique (2-3 rings de convergence)
5. **Floor section**: supprimer le double-sided (garder seulement les 2 premiers triangles, normal Up)
6. **Backface masking**: soit forcer le material sur les murs interieurs a ne pas rendre backface, soit ajouter un flag `bRenderFromExterior = false`

**Critere de fin**: le sub est visuellement propre vu de l'exterieur — pas de double coque, pas de faces inversees, pas de facettage excessif.

### Phase 3 — Decouplage walkable / visuel

**Fichiers**: SubmarineGeometryBuilder.h/.cpp, SubmarineCompilerActor.cpp, SubmarineLayoutAsset.h

**Changements a faire**:

1. **Separer le floor mesh**: `BuildInteriorMeshes()` produit un PMC dedie par compartiment pour le floor seulement (PMC_Floor_*), profil SubInteriorWalkable. Les murs restent dans PMC_Interior_* avec profil SubInteriorVisual ou NoCollision.
2. **Exploiter FWalkableSurfaceDef**: deja dans SubmarineLayoutAsset. Le BuildCompiler doit peupler `WalkableSurfaces` pour chaque compartiment a la compilation.
3. **Override GetInteriorWalkableComponents()**: ne retourner QUE les PMC_Floor_*, pas les murs.
4. **Floor simplification**: le floor walkable n'a pas besoin d'etre un mesh complexe. Un simple quad plat par compartiment (4 vertices, 2 triangles) suffit pour la collision. La geometrie visuelle du sol peut etre plus riche separement.

**Critere de fin**: le crew marche sur un composant dedie, pas sur le mesh visuel des murs.

### Phase 4 — Camera referentiel sous-marin

**Fichiers**: SubCrewMovementComponent.h/.cpp, SubCrewCharacter.h/.cpp

**Changements a faire**:

1. **Compensation pitch/roll**: etendre `ApplyYawCompensation()` pour compenser les 3 axes de rotation du sub
2. **Alternative**: convertir le controller rotation en espace local du sub avant de l'appliquer a la camera
3. **Test**: le joueur doit voir l'interieur du sub comme stable meme quand le sub pitch fortement

**Critere de fin**: la camera ne bascule plus quand le sub change de pitch/roll.

### Phase 5 — Embark et floor support durci

**Fichiers**: SubCrewCharacter.cpp, SubCrewMovementComponent.cpp, SubGameMode.cpp

**Changements a faire**:

1. **Embark canonique**: utiliser `GetCrewEmbarkTransform()` dans GameMode au lieu de `GetPrimaryCrewSpawnTransform()` + offset
2. **Validation floor obligatoire**: le bootstrap ne passe pas a CrewEmbarked si `bHasValidEmbarkedFloor == false`
3. **Recovery robuste**: si floor perdu pendant Boarding, forcer un re-snap depuis `GetCrewEmbarkTransform()`
4. **Log clair**: chaque etape du contrat embark doit logguer sa validation

**Critere de fin**: plus de spawn sous le sol, plus de spawn flottant, plus de perte de base silencieuse.

### Phase 6 — Feel FPS

**Fichiers**: SubCrewCharacter.h/.cpp, SubCrewMovementComponent.h/.cpp

**Changements a faire** (suggestions, selon vision gameplay):

1. **Headbob**: leger mouvement camera en Walking lie a la vitesse du character
2. **Footstep sounds**: declenches par distance parcourue, pas par animation
3. **Sprint/Crouch**: augmenter MaxWalkSpeed en sprint, reduire capsule en crouch
4. **Interaction visuelle**: crosshair change quand un interactable est en range
5. **Camera smoothing**: interpolation douce quand le sub bouge (attenuer micro-jitter)
6. **Embodiment inertiel**: utiliser `LocalSubLinearAcceleration` pour un leger camera tilt quand le sub accelere/decelere (deja en preparation avec le brace support)

**Critere de fin**: le joueur "sent" qu'il est dans un sous-marin. Pas avant que les phases 1-5 soient stables.

---

## 5. DECISIONS GAMEPLAY EN ATTENTE

Les questions suivantes doivent etre tranchees avant ou pendant l'implementation. Elles sont documentees dans un fichier separe.

Voir: `reports/plans/2026-03-31_sub3d_gameplay_vision_questions.md`

---

## 6. RESUME DES PRIORITES

```
[CRITIQUE] Phase 2 — Enveloppe exterieure propre
[CRITIQUE] Phase 3 — Decouplage walkable / visuel
[IMPORTANT] Phase 4 — Camera referentiel sous-marin
[IMPORTANT] Phase 5 — Embark et floor support durci
[SECONDAIRE] Phase 6 — Feel FPS
```

Phase 1 est faite (en attente de build).
Phases 2 et 3 sont les vrais bloquants pour un sous-marin "propre et jouable".
Phases 4 et 5 rendent l'experience stable.
Phase 6 est du polish — ne pas commencer avant la stabilisation.

---

## 7. GO / NO-GO CHECKLIST

### GO — On peut avancer quand:
- [ ] Le sub ne sweep plus contre ses propres composants internes
- [ ] La coque exterieure est visuellement propre (pas de double hull, normals coherents)
- [ ] Le crew spawn sur un vrai floor dedie
- [ ] La movement base reste stable en mouvement
- [ ] La camera reste stable par rapport a l'interieur du sub
- [ ] Le floor interieur est un composant distinct du mesh visuel

### NO-GO — Ne pas faire:
- Toucher au feel FPS avant d'avoir resolu support/collision/camera
- Garder un mesh interieur ambigu comme verite walkable
- Laisser le sub sweep contre ses propres composants interieurs
- Melanger structure, visuel et support dans une seule couche de collision
- Travailler le polish avant la stabilite

---

## 8. FICHIERS DE REFERENCE

### Tier 1 — Pipeline enveloppe + collision

| Fichier | Role |
|---|---|
| SubmarineGeometryBuilder.h/.cpp | Generation geometrie (LE fichier cle pour le cleanup enveloppe) |
| SubmarineCompilerActor.h/.cpp | Orchestrateur compilation + proxy collision |
| SubmarineLayoutSolver.h/.cpp | Placement compartiments |
| SubmarineBuildCompiler.h/.cpp | Compilation vers LayoutAsset |
| SubmarineEnvelopeDef.h/.cpp | Definition enveloppe parametrique |
| SubmarineLayoutAsset.h | Asset compile (compartiments, doors, stations, walkables) |
| SubCompilerTypes.h | Types shared (FCompartmentPlacement, FBulkheadPlacement, etc.) |

### Tier 2 — Mouvement + character

| Fichier | Role |
|---|---|
| SubMovementComponent.h/.cpp | Physique sub + sweep |
| SubCrewCharacter.h/.cpp | Character FPS + embark |
| SubCrewMovementComponent.h/.cpp | CMC crew + floor support + inertial |
| SubInteriorFrameComponent.h/.cpp | Referentiel interieur |

### Tier 3 — GameMode + run flow

| Fichier | Role |
|---|---|
| SubGameMode.h/.cpp | Bootstrap + run phases |
| SubRunPhase.h | Enum des phases de run |
| SubmarineBase.h/.cpp | Facade runtime sub |

### Tier 4 — Config

| Fichier | Role |
|---|---|
| Config/DefaultEngine.ini | Collision channels et profils |
