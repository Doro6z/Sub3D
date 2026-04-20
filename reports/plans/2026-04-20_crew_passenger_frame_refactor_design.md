# Crew Passenger-Frame Refactor — Design Document — 2026-04-20

**Status** : design, pas de code écrit. À revoir et valider avant implémentation.

**Contexte** : après 4 fixes structurels (α=0 skip, double-dérivation InteriorFrame, ForwardSpeedCmS repli, InterpDuration clamp), les bugs restants (boucle recovery escaliers, divergence helm paths, jitter résiduel inhérent) sont tous des symptômes d'un mismatch fondamental : UE `CharacterMovementComponent` stock n'est pas conçu pour un base mobile rapide avec crew walking dessus. Ce doc propose un refactor qui remplace la sim crew en repère monde + `BasedMovement` par une sim en repère sub-local + composition au render.

**But de ce doc** : énoncer précisément l'architecture cible, la math, les états, les ruptures, les tests. Pas d'ambiguïté avant de commencer à coder.

---

## 1. Problème à résoudre

### 1.1 Symptômes observés
- Jitter résiduel haute FPS en standalone après tous les fixes (variance inhérente du dual-buffer interp, amplifié par CMC stock + chaîne d'attachement).
- Boucle infinie `RefreshEmbarkedFlooring` sur escaliers (`FloorWalkable=1, AcceptedBase=0` → `Recover=1`, `Velocity=0` cyclique, jitter positionnel).
- 6+ points d'entrée pour "crew ↔ sub" (`SetCurrentSubmarine`, `EnterOnFootInSubmarine`, `BoardSubmarine`, `TakeHelm`, `ForceHelm`, `DisembarkSubmarine`) avec init partielle divergente. Dommage permanent observé quand chemins mélangés.
- Compensation manuelle (`ApplyYawCompensation`, `UpdateRelativeState`, `UpdateInertialState`, `CameraSway`) empilée en couches pour corriger la dérive monde-base mobile. Chaque couche introduit ses propres artefacts.

### 1.2 Cause racine commune
UE stock `CharacterMovementComponent` + `UpdateBasedMovement` suppose :
- Base statique ou en mouvement lent (ascenseur, plateforme).
- Floor trace en monde, collision en monde, intégration velocity en monde.
- Correction de delta transform de la base appliquée au character chaque tick.

Pour un sub à 750 cm/s qui rotate, avec crew walking sur walkable mesh mobile :
- Le delta de base par tick est ~12 cm, non-négligeable.
- La vélocité du character en monde = vélocité input + vélocité base. Les deux se mélangent.
- Le floor trace en monde sur une surface mobile produit des hits instables.
- La compensation yaw du base sur l'actor du crew introduit du jitter rotationnel.

**Chaque bug qu'on a shippé était un pansement sur ce mismatch. Passenger-frame le supprime à la racine.**

### 1.3 Ce que "passenger-frame" signifie
Le crew est simulé en repère **sub-local** quand il est dans le sub. Sa pose monde est dérivée par composition à chaque frame :

```
CrewWorldTransform = CrewLocalTransform * SubWorldTransform
```

Le sub bouge → la pose monde du crew bouge automatiquement via la composition. Aucune compensation nécessaire. Le crew est "rigide" par rapport au sub sans effort.

---

## 2. Principes de design

1. **Local-frame comme vérité** pour le crew quand embarked. Le monde n'est qu'une dérivée.
2. **Un seul point d'entrée d'état** : `TransitionTo(Context, Sub, Station)`. Remplace tous les `Board/TakeHelm/EnterOnFoot`.
3. **Composition explicite** : pas de `BasedMovement` implicite. On écrit `CrewWorld = CrewLocal * SubWorld` nous-mêmes.
4. **État observable** : state machine visible, `DebugDraw` par contexte, log transitions.
5. **Pas de dérivation numérique de signaux visuels** — leçon des 4 agents. Tout signal dérivé vient de la sim.
6. **Garder UE stock pour ce qu'il fait bien** : capsule shape, anim graph, input binding, camera attach. On swap uniquement la sim movement.

---

## 3. State machine `ECrewFrameContext`

### 3.1 Énumération

```cpp
UENUM(BlueprintType)
enum class ECrewFrameContext : uint8
{
    WorldSpace      UMETA(DisplayName = "World Space"),     // default, crew en monde (hors sub)
    SubLocal        UMETA(DisplayName = "Sub Local"),       // embarked, walking in sub
    StationLocked   UMETA(DisplayName = "Station Locked"),  // assigné à une station (helm, engine, ballast)
};
```

### 3.2 Diagramme d'états

```
                          TransitionTo(SubLocal, Sub)
                          +--------------------------+
                          |                          v
                +------------------+        +------------------+
                |   WorldSpace     |        |    SubLocal      |
                |  (stock UE CMC)  |        | (custom sim)     |
                +------------------+        +------------------+
                          ^                    ^         |
                          |                    |         | TransitionTo(StationLocked, Sub, "Helm")
                          |                    |         v
                          |                    |  +------------------+
                          |                    |  | StationLocked    |
                          |                    |  | (fixed transform)|
                          |                    |  +------------------+
                          |                    |         |
                          |                    +---------+
                          |     TransitionTo(SubLocal, Sub)
                          |
                          +----------------------+
                                TransitionTo(WorldSpace, nullptr)
                                (depuis SubLocal uniquement)
```

### 3.3 Règles de transition

| From ↓ / To → | WorldSpace | SubLocal | StationLocked |
|---|---|---|---|
| **WorldSpace** | — (no-op) | ✓ si Submarine valide + proche | ✗ (passer par SubLocal) |
| **SubLocal** | ✓ (disembark) | — (no-op ou change sub) | ✓ si StationId valide |
| **StationLocked** | ✗ (release station d'abord) | ✓ (release station) | ✓ change station |

### 3.4 API unique

```cpp
UFUNCTION(BlueprintCallable, Category = "Crew")
bool TransitionTo(ECrewFrameContext NewContext,
                  ASubmarineBase* Submarine = nullptr,
                  FName StationId = NAME_None,
                  const FTransform& DesiredLocalTransform = FTransform::Identity);
```

**Comportement** :
- Valide la transition (retourne false si interdite).
- Capture l'état courant (pose monde, vélocité).
- Calcule la nouvelle vérité (pose locale si SubLocal/StationLocked, pose monde si WorldSpace).
- Initialise les variables pertinentes (reset Velocity, base tracking, IK hints).
- Met à jour `CurrentSubmarine` et `CurrentFrameContext`.
- Log la transition avec from/to/sub/station.
- Replicate le nouveau contexte au client.

**Thin wrappers existants** à garder pour compat mais implémentés via `TransitionTo` :
- `EnterOnFootInSubmarine(Sub, Xform)` → `TransitionTo(SubLocal, Sub, NAME_None, WorldToLocal(Xform))`
- `BoardSubmarine(Sub)` → idem avec spawn transform par défaut
- `TakeHelm()` → `TransitionTo(StationLocked, CurrentSubmarine, "Helm")`
- `ForceHelm()` → idem mais bypass côté serveur
- `ReleaseHelm()` → `TransitionTo(SubLocal, CurrentSubmarine)`
- `DisembarkSubmarine()` → `TransitionTo(WorldSpace, nullptr)`

**`ForceHelm` cesse d'être un chemin parallèle**. C'est juste `TransitionTo` avec un flag serveur.

---

## 4. Math : composition et décomposition

### 4.1 Notation

- `SubWorld` : `FTransform` du sub en monde (actor transform du `ASubmarineBase`).
- `CrewLocal` : `FTransform` du crew en repère sub-local (vérité quand SubLocal).
- `CrewWorld` : `FTransform` du crew en monde (pose rendue).

### 4.2 Formules

**Composition (SubLocal → World)** :
```
CrewWorld = CrewLocal * SubWorld
```

(UE convention : `FTransform A * B` applique A puis B. Donc `CrewLocal * SubWorld` = "d'abord CrewLocal relatif à l'origine, puis transformé par SubWorld". Ce qui équivaut à "CrewLocal exprimé dans le repère de SubWorld". ✓)

**Décomposition (World → Local)** :
```
CrewLocal = CrewWorld * SubWorld.Inverse()
```

### 4.3 Velocity

Le crew a deux vélocités distinctes à garder en tête :
- `LocalVelocity` (repère sub) : ce que le crew fait de son plein gré (walk, jump).
- `WorldVelocity` (repère monde) : apparente, incluant la motion du sub.

Relation :
```
WorldVelocity = SubWorldVelocity 
              + Cross(SubWorldAngularVelocity, CrewLocalPosition_rotated_to_world) 
              + LocalVelocity_rotated_by_sub
```

En pratique, **on n'a pas besoin de WorldVelocity dans la sim crew**. On simule en local. Au render, la pose est composée ; la vélocité apparente émerge naturellement.

Exception : si un autre système externe (ex : animation physique, ragdoll impact) demande la vélocité monde, on la calcule à la demande via la formule ci-dessus.

### 4.4 Floor trace

Le crew veut savoir "qu'est-ce qu'il y a sous moi dans le sub". Le trace doit se faire dans le repère où le sub ne bouge pas — donc local.

Mais UE physics scene est en monde. On ne peut pas trace en local directement. Approche :

```cpp
// Local-space trace inputs
const FVector LocalStart = CrewLocal.GetLocation() + FVector(0, 0, CapsuleHalfHeight);
const FVector LocalEnd   = LocalStart - FVector(0, 0, CapsuleHalfHeight + FloorTraceDistance);

// Transform to world for physics query
const FVector WorldStart = SubWorld.TransformPosition(LocalStart);
const FVector WorldEnd   = SubWorld.TransformPosition(LocalEnd);

// World-space physics query
FHitResult Hit;
GetWorld()->LineTraceSingleByChannel(Hit, WorldStart, WorldEnd, ECC_Pawn, Params);

// Filter: only accept hits on sub-owned components
if (Hit.GetActor() == Submarine)
{
    // Transform hit back to local
    const FVector LocalHitLocation = SubWorld.InverseTransformPosition(Hit.Location);
    const FVector LocalHitNormal = SubWorld.InverseTransformVectorNoScale(Hit.Normal);
    // Apply as floor in local
}
```

**Clé** : la requête physique passe par le monde, mais l'interprétation du résultat reste en sub-local. Le sub peut bouger d'un tick à l'autre sans invalider nos données locales.

### 4.5 Intégration locale

À chaque tick quand `SubLocal` :

```cpp
// 1. Input: local-frame desired velocity
const FVector LocalInputDir = ComputeInputDirInLocalFrame();  // from axis inputs + crew local yaw
const FVector LocalDesiredVel = LocalInputDir * WalkSpeed;

// 2. Gravity in local frame (up is +Z in sub-local)
LocalVelocity.Z -= GravityZ * DeltaTime;

// 3. Apply input with acceleration
LocalVelocity = FMath::VInterpTo(LocalVelocity, LocalDesiredVel, DeltaTime, Accel);

// 4. Integrate local position
CrewLocal.SetLocation(CrewLocal.GetLocation() + LocalVelocity * DeltaTime);

// 5. Floor trace (see 4.4)
if (FloorFoundInLocal)
{
    CrewLocal.SetLocation(CrewLocal.GetLocation() snapped to floor);
    LocalVelocity.Z = FMath::Max(LocalVelocity.Z, 0.f);
}

// 6. Collision in local (see 4.6)
ResolveLocalCollision();

// 7. Compose world pose and set on actor
CrewWorld = CrewLocal * Submarine->GetActorTransform();
CharacterOwner->SetActorTransform(CrewWorld, /*bSweep=*/false, nullptr, ETeleportType::None);
```

### 4.6 Collision locale

Même principe que floor trace : capsule sweep en monde (UE ne sait pas faire autrement) mais inputs en sub-local transformés en monde, résultats retransformés en local.

Capsule shape reste en monde (physics engine). Le mouvement du capsule est piloté par notre sim locale composée.

---

## 5. Refactor `USubCrewMovementComponent`

### 5.1 Membres ajoutés

```cpp
UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|Context")
ECrewFrameContext CurrentFrameContext = ECrewFrameContext::WorldSpace;

UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RelativeTransform, Category = "Crew|SubLocal")
FTransform RelativeTransform = FTransform::Identity;

UPROPERTY(BlueprintReadOnly, Category = "Crew|SubLocal")
FVector LocalVelocity = FVector::ZeroVector;

UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|Station")
FName CurrentStationId = NAME_None;

UFUNCTION()
void OnRep_RelativeTransform();  // recompose world pose on client
```

### 5.2 Membres retirés / dépréciés

- `RelativeLinearVelocity`, `LocalSubLinearVelocity`, `LocalSubLinearAcceleration`, `LocalSubAngularVelocityDegrees`, `LocalSubAngularAcceleration` — **plus dérivés** de InteriorFrame. Si nécessaires pour anim/FX, lire depuis `Sub->SubMovement->Velocity` directement (approche S1 InteriorFrame).
- `bHasValidEmbarkedFloor`, `bHasAcceptedEmbarkedBase`, `bNeedsEmbarkedFloorRecovery`, `SupportQuality01`, `AttemptEmbarkedFloorRecovery` — **toute la logique "floor recovery"** saute. En sub-local, le floor trace est trivial et ne boucle jamais.
- `ApplyYawCompensation` — **retiré entièrement**. Crew rigide par composition.
- `UpdateInertialState`, `UpdateRelativeState` — **retiré ou refactorisé**. L'état relatif est la vérité, pas dérivé.
- `LastKnownBase`, `LastEmbarkedFloorComponent`, `CheckAndLogBaseChange` — **retiré**. Le concept de "base" UE n'est plus utilisé en SubLocal.
- `bIgnoreBaseRotation = IsEmbarked()` — **retiré**. Pas de rotation de base appliquée.

### 5.3 TickComponent refactorisé

```cpp
void USubCrewMovementComponent::TickComponent(float DeltaTime, ...)
{
    switch (CurrentFrameContext)
    {
    case ECrewFrameContext::WorldSpace:
        // Stock UE CMC path. Leave everything to Super::TickComponent.
        Super::TickComponent(DeltaTime, ...);
        break;

    case ECrewFrameContext::SubLocal:
        // Custom sim in sub-local frame. Super::TickComponent NOT called.
        TickSubLocal(DeltaTime);
        break;

    case ECrewFrameContext::StationLocked:
        // Actor transform = fixed RelativeTransform composed with sub.
        // No simulation runs. Just compose and write.
        ComposeAndApplyWorldPose();
        break;
    }
}
```

### 5.4 Override `UpdateBasedMovement`

```cpp
void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
    if (CurrentFrameContext == ECrewFrameContext::SubLocal
        || CurrentFrameContext == ECrewFrameContext::StationLocked)
    {
        // Our custom sim handles world pose. Skip stock BasedMovement entirely.
        return;
    }

    Super::UpdateBasedMovement(DeltaSeconds);  // WorldSpace uses stock
}
```

### 5.5 Override `FindFloor` pour SubLocal

```cpp
void USubCrewMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
    if (CurrentFrameContext == ECrewFrameContext::SubLocal && GetCurrentSubmarine())
    {
        FindFloorInSubLocal(CapsuleLocation, OutFloorResult);
        return;
    }

    Super::FindFloor(CapsuleLocation, OutFloorResult, bCanUseCachedLocation, DownwardSweepResult);
}
```

`FindFloorInSubLocal` fait le trace décrit en 4.4.

---

## 6. Impact anim IK

### 6.1 FootIK (`USubCrewAnimInstance`)

Actuellement : trace descendant en monde pour trouver le sol, rotate bone pour matcher slope.

Nouvelle approche :
- Si `CurrentFrameContext == SubLocal`, le floor trace est déjà fait par `USubCrewMovementComponent::FindFloorInSubLocal`.
- L'anim instance lit la hit info **en sub-local** via une API que le CMC expose : `GetFootFloorInfoLocal(FootBoneName, OutLocalNormal, OutLocalHeight)`.
- Pour la rotation de bone, on travaille en **local-local** (relatif au root bone du crew). Si le bone IK natif est en local-au-crew, rien ne change fondamentalement.
- Si le code IK est en world-space, on transforme le hit point/normal à la demande avant de l'utiliser.

**Risque** : si l'anim BP existant fait des traces via un `Control Rig` world-space, il faudra adapter. À vérifier au moment du code.

### 6.2 HandIK (grip sur rails)

Les targets de grip sont des scene components sub-children. Leur world transform = composé via sub. Quand le sub bouge, les targets bougent avec.

Anim IK lit leur world transform (ou local au crew) — les deux marchent. **Pas de changement en sub-local.**

### 6.3 Stumble / posture reactions

Le `ComputeSubMotion` dans `SubCrewAnimInstance` utilise `LocalSubLinearAcceleration`. En passenger-frame, la "motion ressentie" par le crew = vélocité du SUB (pas du crew). Donc lire `Submarine->SubMovement->Velocity` et ses dérivées analytiques.

Si on veut garder du stumble, on le recalcule :
```cpp
// Dérivée analytique stockée sim-side (à ajouter à USubMovementComponent si pas déjà là)
FVector SubAcceleration = Submarine->SubMovement->GetAccelerationCmS2();  
// Transform to crew's local frame
FVector LocalSubAccel = Crew->GetActorTransform().InverseTransformVectorNoScale(SubAcceleration);
```

**Pas de double dérivation**. Consommation directe.

---

## 7. Camera

### 7.1 FPS camera

Attachée à la capsule. Capsule transform = écrite par notre sim (compose `CrewLocal * SubWorld`). Camera suit via attachement standard. Smooth par construction.

Le `CameraSway` actuel (basé sur `LocalSubLinearAcceleration` dérivé double) :
- **Retiré** (le signal source n'existe plus dans le refactor).
- Si on veut le ressenti "accélération du sub" visible à la caméra, on le remet mais avec `Submarine->SubMovement->Velocity` différencié UNE fois (dérivée analytique sur signal lisse).

### 7.2 TPS camera + SpringArm

Inchangé. SpringArm child de crew, follow via attachement. SpringArm fait un collision test (c'est son job). Pas de modification nécessaire.

---

## 8. Replication

### 8.1 Ce qui est répliqué

```cpp
// ASubCrewCharacter
UPROPERTY(ReplicatedUsing = OnRep_CurrentSubmarine)
ASubmarineBase* CurrentSubmarine;  // unchanged

// USubCrewMovementComponent
UPROPERTY(Replicated)
ECrewFrameContext CurrentFrameContext;

UPROPERTY(ReplicatedUsing = OnRep_RelativeTransform)
FTransform RelativeTransform;  // set only in SubLocal/StationLocked

UPROPERTY(Replicated)
FName CurrentStationId;  // set only in StationLocked
```

### 8.2 Ce qui n'est PAS répliqué directement

- Crew world position : **calculé localement sur chaque client** à partir de `RelativeTransform * Sub->GetActorLocation()`.
- Sub world position : déjà répliqué via `RepState` (inchangé).

### 8.3 Bénéfices

- **Bandwidth** : `RelativeTransform` est borné par la taille intérieure du sub (~20m). Quantization plus efficace qu'une `WorldLocation` qui peut dériver à grandes coordonnées.
- **Cohérence** : la pose monde du crew est toujours en phase avec la pose monde du sub (composition déterministe). Plus de "crew qui glisse par rapport au sub" dû à timing d'interpolation.
- **Moins de snaps** : en coop, le crew "suit" le sub même pendant les snaps/interp-hiccups du sub lui-même.

### 8.4 OnRep_RelativeTransform

```cpp
void USubCrewMovementComponent::OnRep_RelativeTransform()
{
    if (CurrentFrameContext == ECrewFrameContext::SubLocal 
        || CurrentFrameContext == ECrewFrameContext::StationLocked)
    {
        ComposeAndApplyWorldPose();
    }
}
```

Sur client, quand serveur envoie un update `RelativeTransform`, on recompose et applique.

---

## 9. Interaction avec systèmes existants

### 9.1 Doors (`ASubDoorActor`)

- Doors sont des child actors du sub, déjà attachés.
- Leur world transform = transform du sub * relative.
- Interaction component (proximity detect) : fonctionne en world. Le crew's world position est composée chaque frame → proximity query valide.
- **Aucun changement nécessaire.**

### 9.2 Stations (`USubmarineStationManagerComponent`)

- Stations enregistrent des slots sub-local (`Slot.LocalTransform`).
- Actuellement, station spawn actor à `Slot.LocalTransform * Sub.World`.
- En passenger-frame, le crew qui prend la station → `TransitionTo(StationLocked, Sub, StationId)` avec `RelativeTransform = Slot.LocalTransform`.
- Le crew est fixe à cette transform locale. Sub bouge → crew suit par composition.
- **Refactor léger** : `TakeHelm` / `ForceHelm` / `TakeStation` deviennent des appels à `TransitionTo(StationLocked, ...)`.

### 9.3 Floors (`HandmadeWalkable` tag)

- Actuellement, `GetInteriorWalkableComponents` filtre par tag. `IsAcceptedEmbarkedBase` valide que la base UE est dans la liste.
- En passenger-frame, le floor trace en sub-local hit des composants enfants du sub. Tous sont acceptés (par construction — c'est le sub).
- **Le tag `HandmadeWalkable` devient optionnel** pour le gameplay walking. Il peut rester comme hint de validation éditeur si on veut, mais ne bloque plus le support crew.
- **Le bug escaliers disparaît** : `FloorWalkable=1, AcceptedBase=0` est impossible en passenger-frame — il n'y a plus de concept `AcceptedBase`.

### 9.4 Helm widget

Lit `Sub->GetActorRotation`, `Sub->SubMovement->Velocity`, `RepState.ForwardSpeed`. Inchangé.

### 9.5 Breach / flood

Les breach volumes et flood cells sont sub-local. Leurs interactions avec le crew (damage par pressure, wade speed) utilisent la position sub-local du crew. **Simplification** : `RelativeTransform.GetLocation()` est direct et fiable.

### 9.6 AI / NPCs (future)

Même contexte : un NPC dans le sub est un crew en `SubLocal`. Sa nav mesh est sub-local. Path find en sub-local.

---

## 10. Breaking changes

### 10.1 Appels à retirer/adapter

Audit à faire pour tous les call-sites :

| Pattern code | Avant | Après |
|---|---|---|
| `Crew->SetActorLocation(WorldLoc)` when embarked | Ignorait `CurrentSubmarine` | Doit appeler `TransitionTo(...)` OU utiliser `SetRelativeTransform` |
| `Crew->GetActorLocation()` | OK, retourne world | OK (composée), retourne world |
| `Crew->AddActorWorldOffset(Delta)` when embarked | Écrasait BasedMovement | Invalide, doit passer par input ou `SetRelativeTransform` |
| Nouveau : `Crew->GetRelativeLocationInSub()` | N'existait pas | Retourne `RelativeTransform.GetLocation()` si SubLocal, sinon NaN/error |

### 10.2 Fichiers à réviser

| Fichier | Impact estimé |
|---|---|
| `SubCrewCharacter.cpp/h` | API transitions, retrait CameraSway dérivé, `EnterOnFoot` → wrap `TransitionTo` |
| `SubCrewMovementComponent.cpp/h` | **Refactor majeur**. Retire 50% du code existant, ajoute la sim locale. |
| `SubCrewAnimInstance.cpp/h` | Retire lecture `LocalSubLinearAcceleration`. Ajoute lecture directe `Submarine->SubMovement->Velocity/Acceleration`. |
| `SubPlayerController.cpp` | Simplifie logique helm/embark. Remplace `TakeHelm` / `ForceHelm` par appels à `TransitionTo`. |
| `SubmarineBase.cpp` | `GetInteriorWalkableComponents` devient optionnel, `HandmadeWalkable` tag reste informatif. |
| `SubInteriorFrameComponent.cpp` | **Peut être retiré** (plus utilisé par crew). Ou gardé si d'autres consumers existent (debug, HUD). |

### 10.3 Sauvegardes / compat

Les `.uasset` des BP_SubmarineCrew et BP_Submarine_Craniata peuvent référencer des méthodes C++ existantes. Audit :
- Si `BoardSubmarine` / `TakeHelm` sont appelés depuis BP, garder les wrappers.
- Si BP inspecte `RelativeLocation` ou autre membre retiré, exposer l'équivalent nouveau.

---

## 11. Migration en phases

### Phase 0 — Design doc (ce doc)
**Livrable** : ce fichier, validé.

### Phase 1 — Core composition + context machine (1-2 j)
**Scope** :
- Ajouter `ECrewFrameContext` et `TransitionTo` sur `USubCrewMovementComponent`.
- Implémenter composition `ComposeAndApplyWorldPose`.
- En mode `SubLocal`, DÉSACTIVER la sim UE stock (`UpdateBasedMovement` return early, floor find no-op temporaire).
- Le crew est RIGIDE sur le sub (pas de walking pour l'instant).

**Test** : crew placé via `TransitionTo(SubLocal, Sub, transform)` reste collé au sub qui bouge. Aucun jitter.

### Phase 2 — Walking en sub-local (2 j)
**Scope** :
- Implémenter `TickSubLocal` : input → LocalVelocity → intégrer RelativeTransform.
- Implémenter `FindFloorInSubLocal` (trace world, résultat local).
- Crew peut walk sur SM_Deck_main normalement, sub qui bouge n'affecte pas la mécanique de walking.

**Test** : walking sur deck main pendant que sub cruise, pas de drift, pas de jitter.

### Phase 3 — Stairs + vertical (1 j)
**Scope** :
- Collision capsule en sub-local (sweep transformé).
- Floor snap sur stairs (SM_Stair_*).
- **Vérifier que le bug escalier est mort** (plus de FloorWalkable=1/AcceptedBase=0 loop).

**Test** : traverser stairs pendant cruise, pas de jitter, walking fluide.

### Phase 4 — Anim IK adaptation (1-2 j)
**Scope** :
- Expose `GetFootFloorInfoLocal()` depuis CMC.
- Adapter anim instance pour utiliser les infos locales.
- Validation visuelle FootIK + HandIK.

**Test** : IK visible correct en standalone pendant que sub bouge + tourne.

### Phase 5 — Replication (1 j)
**Scope** :
- Répliquer `CurrentFrameContext`, `RelativeTransform`, `CurrentStationId`.
- Implémenter `OnRep_RelativeTransform` → recompose world.
- Serveur autoritaire.

**Test** : 2-client PIE, crew du Client 2 visible sur Client 1 à la bonne position relative au sub. Plus de "crew qui dérive par rapport au sub" en coop.

### Phase 6 — State machine unifiée helm/station (1 j)
**Scope** :
- Refactor `TakeHelm`, `ForceHelm`, `ReleaseHelm`, `BoardSubmarine`, `EnterOnFoot`, `Disembark` en wrappers sur `TransitionTo`.
- `StationLocked` mode pour les stations.
- Helm devient une station comme les autres.

**Test** : prendre/rendre helm depuis différents states (OnFoot, SubLocal walking), pas de dommage permanent. Prendre helm client 2, rendre, Client 1 prend — smooth.

### Phase 7 — Cleanup (1-2 j)
**Scope** :
- Retirer `ApplyYawCompensation`.
- Retirer `UpdateBasedMovement` stock usage en SubLocal (déjà fait en Phase 1, confirmer).
- Retirer `IsAcceptedEmbarkedBase`, `GetInteriorWalkableComponents` tag-based logic (ou garder comme hint validation).
- Retirer `LocalSubLinearAcceleration` et la chaîne dérivation double (remplacée par lecture directe SubMov.Velocity).
- Retirer `CameraSway` basé sur accélération dérivée (optionnel : ré-ajouter proprement avec dérivée analytique si on veut le feel).

### Phase 8 — Tests + polish (2 j)
**Matrice de test** :

| # | Scénario | Standalone | 2-client PIE | Réseau réel |
|---|---|---|---|---|
| 1 | Crew stationnaire, sub cruise | ✓ | ✓ | à tester LAN |
| 2 | Crew walking, sub cruise | ✓ | ✓ | à tester LAN |
| 3 | Crew walking, sub qui tourne (rudder) | ✓ | ✓ | — |
| 4 | Crew traversant stairs | ✓ | ✓ | — |
| 5 | Crew prend helm, release, reprend | ✓ | ✓ | — |
| 6 | Crew 1 pilote, Crew 2 observe | — | ✓ | — |
| 7 | Handoff helm entre crews | — | ✓ | — |
| 8 | Crew 1 disembark (hors sub) | ✓ | ✓ | — |
| 9 | FPS sweep 30/60/90/144 | ✓ | ✓ | — |
| 10 | Sub qui hit wall pendant walking | ✓ | ✓ | — |

**Critères acceptation** :
- 0 jitter ressenti perceptuel en standalone à 144 FPS.
- 0 `BACKWARD` dans les logs.
- 0 boucle `RefreshEmbarkedFlooring` (supprimée).
- Helm ↔ OnFoot transitions propres, pas de dommage d'état.
- Cohérence world pose crew ↔ sub en coop.

---

## 12. Sort des fixes récents

| Fix | Sort |
|---|---|
| Fix 1 (α=0 skip removal) | **KEEP**. L'interp sub authority-side reste utile pour le rendu du sub lui-même. Authority path inchangé. |
| Fix 2 (ForwardSpeedCmS repli) | **KEEP**. HUD SPD lit ça. Pas touché. |
| S1 (InteriorFrame → SubMov.Velocity) | **KEEP mais mostly unused**. InteriorFrame devient peut-être retirable si personne ne lit plus ses signaux dérivés. Garder pour debug/telemetry. |
| InterpDuration clamp (coop) | **KEEP**. Protège le remote client interp du sub. Inchangé. |

Tous les 4 fixes restent pertinents — ils fixent des bugs légitimes sur le path du sub lui-même. Passenger-frame concerne le CREW, pas le SUB.

---

## 13. Risques et questions ouvertes

### 13.1 Risques techniques

**R1 — Anim IK complexity** : si les control rigs sont profondément world-space, la conversion sub-local peut prendre plus de 1-2 jours. Mitigation : phase 4 isolée, prototyper un seul foot d'abord avant de tout convertir.

**R2 — Capsule sweep en monde avec input local** : pathologies potentielles si capsule se retrouve "entre deux murs sub-local" après rotation rapide du sub. Mitigation : gérer explicitement la friction sub-local + sweep itératif comme le sub lui-même fait.

**R3 — Physics overlap events timing** : les overlap triggers UE firent en world. Avec notre sim custom, les timings peuvent différer du stock. Mitigation : test exhaustif des triggers (breach volumes, station proximity).

**R4 — BP_SubmarineCrew asset references** : si des fonctions C++ retirées sont référencées en BP, compile errors. Mitigation : audit BP d'abord, garder wrappers déprécié-avec-log plutôt que retirer brutalement.

**R5 — Régression subtile** : un système en aval (sonar, breach, compartment state) qui dépendait d'un side-effect de l'ancienne sim. Mitigation : tests matrice phase 8.

### 13.2 Questions ouvertes

**Q1 — CameraSway en passenger-frame : on le garde ou on le retire ?**
- Pour : feel d'accélération du sub, immersion
- Contre : c'est une compensation perceptuelle non-physique
- **Proposition** : retirer dans le refactor, ré-ajouter en post si le feel manque, avec source = `Sub->SubMovement.GetAcceleration()` analytique (pas dérivé de pose).

**Q2 — Crew physique (ragdoll, impact) en sub-local ?**
- Si le crew se fait éjecté par un breach violent : ragdoll en world (stock UE) ? Ou ragdoll sub-local ?
- **Proposition** : à la transition vers ragdoll, `TransitionTo(WorldSpace, nullptr)` — le crew devient world-space et retombe dans la sim UE stock, physics engine gère.

**Q3 — Sub-à-sub interaction (crew saute d'un sub à un autre) ?**
- Hors scope First Playable probablement. Mais le modèle passenger-frame supporte : transition `SubLocal(SubA)` → `WorldSpace` brief → `SubLocal(SubB)`.

**Q4 — NPC AI en sub-local ?**
- Le modèle est identique : NPC est un character avec `ECrewFrameContext::SubLocal`. Leur nav mesh doit être sub-local. Hors scope pour le refactor crew joueur immédiat.

**Q5 — Walkable detection totalement implicite ?**
- Est-ce qu'on garde `HandmadeWalkable` tag comme ceinture-bretelles (validator éditeur) ou on le retire entièrement ?
- **Proposition** : garder comme hint éditeur pour la logique "où spawner le crew initialement", mais retirer toute dépendance runtime.

---

## 14. Ce que ce refactor N'EST PAS

Pour éviter la dérive de scope :

- **Pas une refonte du sub** : `ASubmarineBase`, `USubMovementComponent`, `USubInteriorFrameComponent` (presque) intacts.
- **Pas une réécriture de l'anim graph** : on adapte seulement ce qui doit l'être pour les traces locales.
- **Pas une refonte du HUD** : les widgets lisent des valeurs existantes.
- **Pas une refonte du gameplay** : le feel des inputs (walk, jump, helm) reste identique à l'utilisateur.
- **Pas une refonte des assets** : BP_Submarine_Craniata, SM_* meshes intacts.

---

## 15. Validation du doc par toi

Points sur lesquels j'aimerais ton input avant de commencer le code :

1. **Scope Phase 1** (rigid on sub, no walking) : tu es OK qu'on passe 1-2 jours juste sur "crew rigide sur sub qui bouge" avant d'ajouter la sim walking ? C'est le fondement.
2. **CameraSway** : retirer dans le refactor, ré-ajouter si feel manque (Q1). OK ?
3. **Risque anim IK** (R1) : si je découvre en Phase 4 que c'est plus gros que 1-2 j, je te remonte un flag avant d'engager la semaine.
4. **`HandmadeWalkable` tag** : garder comme hint éditeur ou retirer complètement ? (Q5)
5. **Ordre des phases** : phase 1-3 en série me semble nécessaire (chaque dépend de la précédente). Phase 4 (anim) pourrait être parallélisée si un autre dev s'y met. Toi, tu préfères séquentiel ?
6. **Budget** : estimation 10-13 j me semble honnête. Tu as une contrainte ou un drop-dead date ?

---

## 16. Prochaine étape

Une fois validé :
1. Créer branche dédiée `feature/crew-passenger-frame` (ou direct sur `scripting` selon ta préférence).
2. Commencer Phase 1 : écrire `ECrewFrameContext` + `TransitionTo` + `ComposeAndApplyWorldPose`. Build, tester "crew rigide sur sub" en standalone.
3. Remonter un log du test pour vérifier que le fondement tient.
4. Phase 2 seulement après Phase 1 validée.

Pas de code en parallèle de plusieurs phases — on avance en séquentiel avec validation à chaque étape.
