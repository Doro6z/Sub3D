# Sub Jitter — Audit Architecture Findings — 2026-04-18

Document de synthèse des Phases 1-11 de la procédure d'audit ([2026-04-18_jitter_full_architecture_audit_procedure.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_full_architecture_audit_procedure.md)).

**Statut** : Phases statiques (1-11) terminées. Phase 12 (matrice empirique) **bloquée sur exécution PIE par utilisateur** — voir handoff [2026-04-18_jitter_debug_handoff.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_debug_handoff.md).

**Toutes les conclusions ci-dessous sont conditionnelles** aux résultats de Phase 12. Aucun fix appliqué.

---

## Résumé exécutif

Trois résultats nets de l'audit statique :

1. **Aucun writer externe identifiable de la pose actor du sub** sur le path standalone, en dehors de `USubMovementComponent::TickComponent`. L'hypothèse H1 est **affaiblie mais pas écartée** — la matrice empirique (Phase 12) doit confirmer via `EXTERNAL_WRITE` log absent.

2. **Identification d'un amplificateur perceptuel majeur** : `ASubCrewCharacter::Tick` ([SubCrewCharacter.cpp:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235)) applique une camera sway proportionnelle à `LocalSubLinearAcceleration` et `LocalSubAngularVelocityDegrees`, avec clamp à 3 cm. Ces valeurs sont des **dérivées de la pose visuelle du sub** calculées par InteriorFrame. Toute oscillation pose, même infime, génère des accélérations spike (vues à -192 246 cm/s² dans le log) qui saturent le clamp et **transforment 0.3 cm de jitter pose en 3 cm de jitter caméra** ressenti.

3. **Pipeline de motion derivation fragile** : la chaîne `[Sub Pose Jitter] → [InteriorFrame velocity = (loc_now - loc_prev)/dt] → [accel = (vel_now - vel_prev)/dt] → [Camera Sway clamped 3cm]` est une **dérivation finie deux fois** sur la pose visuelle. Sensible à toute non-monotonie même au cm. C'est de la **différenciation numérique sur signal jittery** — mathématiquement instable par construction.

Hypothèse principale post-audit : **H2 + un amplificateur**. Le jitter pose du sub est probablement petit (sub-cm), invisible à l'œil sur l'actor lui-même, mais la chaîne crew-camera l'amplifie en oscillation 3 cm visible. La preuve définitive vient de Phase 12.

---

## Phase 1 — Composants sur ASubmarineBase

Tous les `UPROPERTY()` Component sur la classe ([SubmarineBase.h:54-128](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.h:54)) :

| Composant | Class | Parent | Tick? | Role |
|---|---|---|---|---|
| `SubmarineRoot` | USceneComponent | ROOT | non | Root scene (vide) |
| `HullMesh` | UStaticMeshComponent | SubmarineRoot | non | Visual hull (NoCollision) |
| `MovementCollisionProxy` | UStaticMeshComponent | HullMesh | non | Collision sweep shape |
| `SubMovement` | USubMovementComponent | actor (non-scene) | **oui (TG_PrePhysics)** | Sim physique |
| `SubHull` | USubHullComponent | actor | ? | État hull |
| `SubFlood` | USubFloodComponent | actor | oui | Sim flood |
| `Systems` | USubmarineSystemsComponent | actor | oui | État systèmes (lit `Sub->GetActorRotation().Pitch` à [:212](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:212)) |
| `Compartments` | USubmarineCompartmentComponent | actor | ? | État compartiments |
| `StationManager` | USubmarineStationManagerComponent | actor | ? | Spawn stations |
| `Radar` | USubmarineRadarComponent | actor | oui | Radar contacts (lit pose du sub) |
| `InteriorFrame` | USubInteriorFrameComponent | actor | **oui (prereq SubMov)** | Delta tracking + WorldToLocal |
| `BreachVfxManager` | UBreachVfxManagerComponent | actor | ? | VFX breach |
| `FloodWaterVisuals` | UFloodWaterVisualsComponent | actor | ? | VFX water |
| `HullVisualDamage` | USubHullVisualDamageComponent | actor | ? | Visual damage |
| `DoorFloodVfx` | UDoorFloodVfxComponent | actor | ? | VFX door (lit DoorActor.GetActorTransform au tick à [:225](/C:/Dev/Sub3D/Source/Sub3D/Submarine/DoorFloodVfxComponent.cpp:225)) |
| `FeedbackManager` | USubmarineFeedbackDirectorComponent | actor | ? | Feedback haptique |
| `Sonar` | USubSonarComponent | actor | ? | Sonar |
| `SonarSystem` | USubSonarSystemComponent | actor | ? | Sonar UI |
| `TunnelNavigationRuntime` | UTunnelNavigationRuntimeComponent | actor | ? | Nav |
| `HelmNavigationDisplay` | UHelmNavigationDisplayComponent | actor | oui | Navigation widget data (lit pose à [:116](/C:/Dev/Sub3D/Source/Sub3D/Submarine/HelmNavigationDisplayComponent.cpp:116)) |
| `HelmSocket` | USceneComponent | HullMesh | non | Anchor station |
| `CrewSpawnSocketP1` | USceneComponent | HullMesh | non | Anchor spawn crew |
| `TurretHardpoint` | USceneComponent | HullMesh | non | Anchor turret |
| `GeneratedGeometry` | USubmarineGeneratedGeometryComponent | actor | non | Géométrie procédurale (pas de tick) |

**Observations** :
- Le **root est un USceneComponent vide**, pas un UPrimitiveComponent. Les SetActorLocation/Rotation sur le sub bougent SubmarineRoot, qui propage à HullMesh (et MovementCollisionProxy via lui).
- `SetReplicateMovement(false)` confirmé à [SubmarineBase.cpp:163](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:163) — UE n'auto-réplique pas la pose. C'est notre `RepState` qui est répliqué via `RefreshRepState`.
- `ASubmarineBase::Tick` à [SubmarineBase.cpp:798](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:798) ne fait que `Super::Tick` — pas de logique custom donc pas de writer per-frame depuis l'actor.
- Le sub n'est **pas possédé** (CLAUDE.md), donc pas de `FaceRotation` automatique d'APawn même si `bUseControllerRotationYaw` est true par défaut.

---

## Phase 2 — Composants sur ASubCrewCharacter + chaîne caméra

Composants explicites ([SubCrewCharacter.h:34-44](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.h:34)) :

| Composant | Class | Parent | Tick? |
|---|---|---|---|
| (root) | UCapsuleComponent | ROOT (default ACharacter) | non |
| Mesh | USkeletalMeshComponent | Capsule | oui (anim) |
| `FPSCamera` | UCameraComponent | (probablement Mesh ou Capsule, à vérifier en BP) | non |
| `TPSCameraBoom` | USpringArmComponent | Mesh | oui |
| `TPSCamera` | UCameraComponent | TPSCameraBoom | non |
| `InteractionComponent` | USubInteractionComponent | actor | oui |

**Chaîne d'attachement caméra (estimée)** :
- FPS : ActorRoot (Capsule) → Mesh → FPSCamera (relative location modifiée chaque tick, voir Phase 7)
- TPS : ActorRoot → Mesh → SpringArm (collision-test) → TPSCamera

**Crew rotation** : `bUseControllerRotationYaw = true` à [SubCrewCharacter.cpp:134](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:134). Le yaw du crew suit le controller. `ApplyYawCompensation` ajuste à la fois ActorRotation et ControlRotation pour compenser la rotation du sub.

---

## Phase 3 — Writers de la pose du sub (path standalone)

**Inventaire complet** des appels qui modifient la pose actor du sub :

| Fichier:Ligne | Caller | Fréquence | TeleportType | Notes |
|---|---|---|---|---|
| [SubMovementComponent.cpp:165](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:165) | TickComponent → undo | chaque tick (auth) | TeleportPhysics | Restaure CurrSim |
| [SubMovementComponent.cpp:265](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:265) | TickComponent → interp write | chaque tick si interp pas suppress | None | Lerp(PrevSim, CurrSim, α) |
| [SubMovementComponent.cpp:552](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:552) | ApplyPhysics → fallback no-sweep horizontal | sim step seulement | None | AddActorWorldOffset |
| [SubMovementComponent.cpp:567](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:567) | ApplyPhysics → fallback no-sweep vertical | sim step seulement | None | AddActorWorldOffset |
| [SubMovementComponent.cpp:615](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:615) | ApplyPhysics sweep → no hit | sim step | None | AddActorWorldOffset (RemainingDelta) |
| [SubMovementComponent.cpp:627](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:627) | ApplyPhysics sweep → advance to hit | sim step si hit | None | AddActorWorldOffset (AdvanceDelta) |
| [SubMovementComponent.cpp:633](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:633) | ApplyPhysics sweep → depen if start penetrating | sim step si bStartPenetrating | None | AddActorWorldOffset (Depen along Normal) |
| [SubMovementComponent.cpp:686](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:686) | ApplyPhysics → set rotation final | sim step | None | SetActorRotation(NewRotation) |
| [SubPlayerController.cpp:1023](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubPlayerController.cpp:1023) | DevCheat_TeleportToCompartment | manuel (cheat) | TeleportPhysics | Hors test cruise |
| [TraversalLevelManager.cpp:34](/C:/Dev/Sub3D/Source/Sub3D/Submarine/TraversalLevelManager.cpp:34) | Timer 0.2s post-BeginPlay | une fois | TeleportPhysics | Hors test cruise après 0.2s |

**Writers exclus** (path remote) :
- [SubMovementComponent.cpp:858](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:858), :892, :922 — `HandleReplicatedNetState` + `InterpolateClient`. Path client distant uniquement (gardé par `!HasAuthority()` à [:184](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:184)).

**Conclusion Phase 3** : **aucun writer externe identifié** sur le path standalone authority. Tous les writes proviennent de `USubMovementComponent::TickComponent` ou `SimulateStep` qui est appelé par `TickComponent`. **L'hypothèse H1 (writer externe entre nos ticks) est statiquement écartée**. Reste à confirmer empiriquement (Phase 12) — si `EXTERNAL_WRITE` ne fire jamais en log, H1 est définitivement écartée.

---

## Phase 4 — Readers critiques de la pose du sub

Sélection des readers qui lisent la pose **per-tick** et propagent en aval :

| Fichier:Ligne | Caller | Read | Propagation |
|---|---|---|---|
| [SubInteriorFrameComponent.cpp:63-64](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:63) | TickComponent (post SubMov) | `Owner->GetActorLocation/Rotation()` | Calcule FrameLocationDelta, LocalLinearVelocity, LocalLinearAcceleration → exposé à crew |
| [SubCrewMovementComponent.cpp:168-171](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:168) | UpdateInertialState (post InteriorFrame) | Frame→GetLocalLinearAcceleration etc | Stocke dans LocalSubLinearAcceleration → lu par camera sway |
| [SubCrewMovementComponent.cpp:184](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:184) | UpdateRelativeState | `Frame->WorldToLocal(CharacterOwner->GetActorLocation())` | Suit position relative crew→sub |
| [SubCrewAnimInstance.cpp:123](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:123) | Anim tick | `Crew->CurrentSubmarine->GetActorTransform()` | Convertit vélocité relative en world (animation) |
| [SubmarineSystemsComponent.cpp:212](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:212) | tick | `Sub->GetActorRotation().Pitch` | TargetPitchDeg pour helm command |
| [SubHelmWidget.cpp:390-432](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubHelmWidget.cpp:390) | widget paint/tick | `Submarine->GetActorRotation()` | Affichage HUD |
| [SubmarineRadarComponent.cpp:57](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineRadarComponent.cpp:57) | tick | `GetOwner()->GetActorLocation()` | Origine radar sweep |

**Aucun reader n'écrit en retour**. Tous purs observateurs.

**Critique** : InteriorFrame tick est garanti AFTER SubMovement par `AddTickPrerequisiteComponent(SubMov)` à [SubInteriorFrameComponent.cpp:41](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:41). Donc lit la pose **post-interp**, pas une pose intermédiaire. ✓

---

## Phase 5 — Tick order (auth path standalone)

Reconstitué via `AddTickPrerequisiteComponent` + `TickGroup` defaults :

```
[TG_PrePhysics]
   ├─ ASubmarineBase::Tick (no-op)
   ├─ USubMovementComponent::TickComponent  ← undo + sim + RefreshRepState + interp write
   │     │
   │     ├ prereq for: USubInteriorFrameComponent
   │     └ prereq for: USubCrewMovementComponent (via SubMov)
   │
   ├─ USubInteriorFrameComponent::TickComponent  ← reads sub pose, computes deltas
   │     │
   │     └ prereq for: USubCrewMovementComponent
   │
   ├─ USubCrewMovementComponent::TickComponent  ← UpdateInertialState (reads Frame), ApplyYawCompensation
   │
   ├─ ACharacter::Tick → ASubCrewCharacter::Tick  ← UpdateCameraMode, CameraSway (reads CMC.LocalSubLinearAcceleration)
   │
   ├─ USubInteractionComponent (probably)
   ├─ USubmarineRadarComponent::TickComponent (reads sub pose)
   ├─ USubmarineSystemsComponent::TickComponent (reads sub rotation pitch)
   ├─ UDoorFloodVfxComponent::TickComponent (reads doors)
   ├─ UHelmNavigationDisplayComponent::TickComponent (reads sub pose)
   └─ ... autres composants sub sans prerequisite explicite
```

**Tous les readers ticking sur le sub ou le crew ticken APRÈS SubMovement** par construction (TG_PrePhysics est le tick group le plus tôt + prereq explicite pour les critiques). Aucune ambiguïté problématique identifiée.

---

## Phase 6 — OnRep callbacks (relevant en standalone)

`ASubmarineBase` a un seul OnRep relevant : `OnRep_RepState` à [SubmarineBase.cpp:1199](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:1199), qui appelle `SubMovement->HandleReplicatedNetState(RepState)`. Cette dernière écrit la pose actor à [:858](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:858) si la distance dépasse `InterpSnapDistanceCm`.

**En standalone** : `RepState` est marqué `ReplicatedUsing = OnRep_RepState`. Mode REPNOTIFY par défaut = `REPNOTIFY_OnChanged`. Les OnRep callbacks **ne fire pas sur l'authority** (la machine qui a écrit la valeur). En standalone, on est l'authority pour tout. **Donc OnRep_RepState ne fire pas.** ✓

Vérification : grep absent de `MARK_PROPERTY_DIRTY` sur RepState ou de `RPC.RepNotifyCondition = REPNOTIFY_Always`. Sécurité confirmée.

**Conclusion Phase 6** : OnRep n'est pas un writer sur le path standalone. ✓

---

## Phase 7 — Systèmes de compensation

Trois systèmes identifiés :

### 7.1 `ApplyYawCompensation` ([SubCrewMovementComponent.cpp:381-430](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:381))

**But** : faire tourner le crew sur lui-même quand le sub tourne, pour qu'il garde son cap relatif à l'intérieur du sub.

**Lit** : `Frame->GetFrameRotationDelta().Yaw` — delta yaw du sub entre deux ticks d'InteriorFrame.

**Écrit** :
- `CharacterOwner->SetActorRotation(NewActorRotation)` — sur le crew, pas le sub
- `Controller->SetControlRotation(...)` — pour synchroniser la caméra

**État persisté** : aucun (purement réactif au delta du frame).

**Risque jitter** : si le sub jitter en yaw, le crew yaw inherits le jitter. Mais pour le cas du test cruise (Yaw=0 stable), pas de delta yaw, ApplyYawCompensation noop. **Pas la cause du jitter X observé.**

### 7.2 Camera Sway ([SubCrewCharacter.cpp:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235)) — **AMPLIFICATEUR MAJEUR**

```cpp
Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale;  // 0.002
Sway.Y = CMC->LocalSubLinearAcceleration.Y * CameraSwayAccelScale;
Sway.Z = CMC->LocalSubAngularVelocityDegrees.Y * CameraSwayAngularScale;  // 0.05
Sway = Sway.GetClampedToMaxSize(CameraSwayMaxCm);  // 3 cm
FPSCamera->SetRelativeLocation(FVector(Sway.X, Sway.Y, PostureZ + Sway.Z));
```

**Lit** : `CMC->LocalSubLinearAcceleration` (cm/s²) qui dérive de `(velocity_now - velocity_prev) / dt`, qui dérive de `(loc_now - loc_prev) / dt`. **Double dérivation finie sur la pose visuelle du sub.**

**Écrit** : `FPSCamera->SetRelativeLocation` — n'écrit pas l'actor, écrit le composant caméra.

**Pathologie identifiée** :
- Si la pose du sub jitter de seulement 0.3 cm sur 11ms, vélocité = 27.3 cm/ms = 27 300 cm/s. Si la frame d'avant la vélocité était 750 cm/s, accel = (27300 - 750) / 0.011 = ~**2.4 millions cm/s²**.
- Avec scale 0.002, sway raw = 4 800 cm. Clamp à 3 cm → camera sway saturée à ±3 cm.
- Le user voit "le monde bouge de 3 cm" en FPS view.

**Le log utilisateur le confirme** : `LocalAccel=V(X=-192246.16)` — accélération de -1.9 millions m/s² sur une seule frame, complètement non-physique. C'est purement le résultat du double-derivative numérique.

**C'est l'amplificateur perceptuel du jitter.** Sans lui, le sub jitter serait sub-perceptible.

### 7.3 BasedMovement (UE stock)

`USubCrewMovementComponent::UpdateBasedMovement` [SubCrewMovementComponent.cpp:101](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:101) délègue à `Super::UpdateBasedMovement` (UE stock). Le crew est basé sur `SM_Deck_main` (composant du sub). UE déplace le crew chaque tick selon le delta de la base.

**Risque** : si la base (= composant sub) jitter, le crew jitter aussi via UpdateBasedMovement. **Confirmé par le log** où `WorldLoc` du crew suit le pattern `+20.84 / -3.74` du sub. C'est un comportement attendu de UE BasedMovement (le crew suit fidèlement la base, jitter inclus).

---

## Phase 8 — Side-effects de SetActorLocation

**OnComponentHit listeners sur le sub** :
- `OnHullHit` à [SubmarineBase.cpp:902](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:902) bind à PMC.OnComponentHit ([:542](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:542)) et CollisionComponent.OnComponentHit ([:552](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:552)). N'écrit que `SubHull->ApplyHullImpact()` et `SubFlood`. **N'écrit pas la pose actor.** ✓

**OnComponentBeginOverlap listeners sur le sub** : aucun (les triggers volumes externes sont sur leur propre actor, pas sur le sub).

**Side-effects de SetActorLocationAndRotation(..., bSweep=false, ETeleportType::None)** :
- Update transform du root + propage aux enfants attachés. ✓
- Skip overlap recompute (TeleportType=None court-circuite cette part).
- Skip physics rebody (bSweep=false).
- Émission de delegate `OnActorMoved` — aucun listener identifié sur le sub.

**Conclusion Phase 8** : pas de side-effect identifié qui pourrait re-écrire la pose actor en réponse à mes writes.

---

## Phase 9 — Mesh visuel + skeletal mesh + MovementCollisionProxy

`HullMesh` (UStaticMeshComponent, NoCollision per memory `project_interior_collision_invariant_2026_04_18.md`) — pas de simulation physique, attaché statiquement à SubmarineRoot. Suit la transform du root.

`MovementCollisionProxy` (UStaticMeshComponent, attaché à HullMesh) — utilisé comme shape de sweep par `ApplyPhysics`. Pas de simulation physique non plus. Suit HullMesh donc SubmarineRoot.

**Pas de USkeletalMeshComponent sur le sub.** Donc pas de root motion ni d'anim qui puisse écrire la pose actor.

`SubmarineGeneratedGeometryComponent` — pas de tick override, donc pas de write per-frame.

**Conclusion Phase 9** : aucun writer mesh/skeletal identifié. ✓

---

## Phase 10 — Caméra

### Chaîne FPS
ActorRoot (Capsule) → Mesh (USkeletalMesh, anim) → FPSCamera

**Position de FPSCamera** : `SetRelativeLocation` chaque tick par CameraSway (Phase 7.2). Donc la position relative de FPSCamera change chaque frame en fonction des dérivées de la pose du sub.

**Late update / SpringArm** : pas de SpringArm sur FPSCamera (uniquement TPS).

**CalcCamera override** : pas de override identifié (Phase 1 du handoff "Point 2 caméra late-update" reste à faire).

### Chaîne TPS
ActorRoot → Mesh → SpringArm → TPSCamera. SpringArm fait un sweep de collision (lag possible). Pertinent uniquement en TPS, le user a testé en FPS.

### Conclusion Phase 10
**FPSCamera est la chaîne pertinente pour le test cruise.** Camera sway est l'amplificateur perceptuel principal.

---

## Phase 11 — Attachement crew → sub

Code embark : `EnterOnFootInSubmarine` ([SubCrewCharacter.cpp:282-302](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:282)) :

```cpp
DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
SetCurrentSubmarine(Sub);
SetActorTransform(SpawnXform, false, nullptr, ETeleportType::TeleportPhysics);
GetCharacterMovement()->StopMovementImmediately();
GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
```

**Pas d'AttachToActor explicite**. Le crew n'est PAS parent-attaché au sub. La liaison se fait via `MovementBase` (le floor static mesh devient la base), géré par UE BasedMovement standard.

Conséquence : le crew n'hérite pas du transform du sub directement (pas d'attach). Il suit via `UpdateBasedMovement` qui calcule le delta de la base entre frames et l'applique au crew.

**Si la base jitter** (= les composants du sub jittent quand l'actor sub jitter), `UpdateBasedMovement` propage proprement le jitter au crew.

---

## Phase 12 — Matrice empirique (BLOQUÉ)

Cette phase requiert l'exécution PIE par utilisateur via les CVars du handoff précédent :
- `sub.LogVisualInterp 1` + `sub.DisableVisualInterp 0|1`
- `t.MaxFPS 30|90|144`
- Observation jitter visuel + extraction logs `EXTERNAL_WRITE`, `BACKWARD`, `SUB_TRACE`

**Sans ces données, impossible de**  :
- Confirmer/écarter H1 (writer externe) — bien qu'écartée statiquement, l'empirique est requise pour preuve.
- Confirmer/écarter H3 (math interp non-monotone) — verra si `BACKWARD` log fire.
- Quantifier le jitter pose résiduel quand `sub.DisableVisualInterp 1`.

**Action requise utilisateur** : exécuter Tests A/B/C du handoff `2026-04-18_jitter_debug_handoff.md`.

---

## Causes identifiées (preliminaire, sous réserve Phase 12)

### Cause confirmée par audit statique : double dérivation numérique pour camera sway

`ASubCrewCharacter::Tick` [SubCrewCharacter.cpp:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235) utilise `LocalSubLinearAcceleration` calculé par double dérivation numérique sur la pose visuelle du sub. **Mathématiquement instable** sur tout signal pose qui a la moindre non-monotonie ou bruit.

**Impact** : transforme un jitter pose sub-cm en oscillation caméra ±3 cm visible en FPS.

**Niveau** : root cause de la **perception** du jitter. La cause de la non-monotonie pose elle-même reste à identifier en Phase 12.

### Hypothèse principale post-statique : H2 (signal pose imparfait amplifié)

L'audit statique n'a pas trouvé de writer externe (H1) ni de bug math évident dans mon interp (H3). La théorie restante :

- Mon interp produit des deltas par-tick qui sont **techniquement positifs** mais avec des "marches" (ex: tick à δα=0.1 puis tick à δα=0.7 puis tick à δα=0.2). En valeur absolue toujours forward, mais avec **amplitude oscillante**.
- Cette amplitude oscillante est lue par InteriorFrame qui en dérive une **vélocité oscillante** (deltas grands/petits/grands).
- La dérivée seconde (acceleration) est alors massivement spike.
- Camera sway saturée → 3 cm visible.

**Contre-preuve** : le log montre `+20.84 / -3.74`. C'est une vraie inversion de signe, pas une oscillation d'amplitude. **Cette observation ne s'explique pas par H2 pur.** Reste H1 ou H3.

### Hypothèses non confirmées

- **H1** (writer externe) : audit statique négatif. Phase 12 doit confirmer via `EXTERNAL_WRITE` log absent.
- **H3** (bug math) : code interp inspecté plusieurs fois, math monotone par construction. Phase 12 doit confirmer via `BACKWARD` log absent. Si présent, il y a un edge case dans `Lerp(FRotator)` ou un wrap-around à investiguer.

---

## Fixes proposés (ordonnés par impact / coût)

### Fix #1 — Remplacer la double dérivation par accès direct aux quantités sim (P0)

**Problème** : Camera sway dépend de `LocalSubLinearAcceleration` dérivé numériquement de la pose visuelle. Instable.

**Fix** :
```cpp
// Au lieu de :
Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale;

// Utiliser :
Sway.X = LocalSubAccelFromSim.X * CameraSwayAccelScale;
// où LocalSubAccelFromSim est dérivé du Velocity / dt SIM, pas du transform visuel
```

**Source** : `USubMovementComponent::Velocity` est le vrai vecteur sim, mis à jour par les forces. Sa dérivée (= sum of forces / mass) est calculable analytiquement dans SimulateStep, sans différenciation finie.

**Coût** : ~30 lignes. Modifier `USubMovementComponent` pour exposer une `LinearAccelerationCmS2` mise à jour en SimulateStep, et pointer InteriorFrame ou crew dessus.

**Bénéfice** : élimine l'amplificateur perceptuel principal. Même si le jitter pose reste, le user ne le verra plus en FPS view.

**Risque** : faible. L'accel sim est plus "lisse" mais correct physiquement. Le feel sera plus calme.

### Fix #2 — Désactiver `bUseControllerRotationYaw` puis utiliser un yaw différentiel propre (P1)

**Problème** : `bUseControllerRotationYaw = true` sur le crew + `ApplyYawCompensation` qui modifie la control rotation après chaque rotation du sub. Risque d'allers-retours entre input controller et compensation.

**Fix** : refactor pour que le yaw du crew soit **always** dans le frame du sub (RelativeYaw), et que la projection world se fasse au render via override de `CalcCamera`. Plus propre, mais demande refactor (Phase 2 du roadmap fluidity).

**Coût** : 200-300 lignes.

### Fix #3 — Ajouter Point 2 du roadmap fluidity (caméra late-update) (P1)

**Problème** : la caméra sample la pose du sub via le composant attaché. Pas de late-update.

**Fix** : override `ASubCrewCharacter::CalcCamera` pour que la caméra sample la pose interpolée fraîche au moment du render, plutôt que d'utiliser la pose figée par tick.

**Coût** : ~50 lignes.

**Note** : c'était déjà au plan ([Point 2 du sub_fluidity_architecture](/C:/Dev/Sub3D/reports/analysis/2026-04-18_sub_fluidity_architecture.md#L98)). Vraisemblablement nécessaire après Fix #1.

### Fix #4 — Si Phase 12 montre `BACKWARD` ou `EXTERNAL_WRITE` (conditionnel)

Investigation ciblée selon la donnée empirique. Patterns de fix :
- `BACKWARD` répété → bug edge case dans `Lerp(FRotator)` ou inconsistance PrevSim/CurrSim, à fixer dans SubMovement.
- `EXTERNAL_WRITE` répété → un writer caché qu'on n'a pas vu en static, à tracker via `OnActorMoved` listener.

---

## Risques résiduels après fix

- **Latence visuelle de 16.6 ms** introduite par mon interp (Point 1). Acceptable pour coop chill mais à surveiller pour le pilote (point 5 du roadmap fluidity = client prediction).
- **CameraSway physique correct** mais user pourrait trouver le ressenti trop "calme" si Fix #1 enlève les spikes. À tester en feel.
- **Si la cause profonde est H3 (math)** non écartée : risque de réintroduction du jitter au prochain refactor sim.

---

## Prochaines étapes

1. **Utilisateur** : exécuter Tests A/B/C ([handoff](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_debug_handoff.md)) et remonter les logs.
2. **Moi** : intégrer les résultats Phase 12 dans ce doc, finaliser la cause profonde.
3. **Implémenter Fix #1** (camera sway sim-based) en priorité — gain perceptuel immédiat.
4. **Si Fix #1 résout perceptuellement** : Fix #3 (caméra late-update) en suivant.
5. **Si jitter pose réel persiste après Fix #1+#3** : investigation H3 ciblée.
