# Architecture : Crew Environment Axis + EVA Velocity Blending

**Date** : 2026-04-22
**Statut** : Validé — prêt pour implémentation
**Axe couvert** : **Environnement du crew** (compartiment, intérieur/extérieur, EVA) — orthogonal à l'axe locomotion de [2026-04-21_local_grid_space_authority_architecture.md](2026-04-21_local_grid_space_authority_architecture.md).
**Scope First Playable** : Phase 1 + Phase 2 (solo). Phase 3 (réseau) après validation solo.

---

## 1. Principe — Orthogonalité stricte

L'état du crew est désormais un **tuple** :

```
CrewState = (MovementState, EnvironmentContext)
```

| Axe | Type | Owner | Description |
|---|---|---|---|
| **MovementState** | `ECrewEmbarkState` (enum) | `USubCrewMovementComponent` | Référentiel spatial (Outside / Embarked / Transitioning) |
| **EnvironmentContext** | `TWeakObjectPtr<UCompartmentVolumeComponent>` | `ASubCrewCharacter` | Compartiment courant (ou nullptr = océan). Les props env sont dérivées |

Le système de mouvement ne connaît PAS les compartiments. Le système d'environnement ne connaît PAS le rebase. Le seul point de couplage est `ASubCrewCharacter`, qui reçoit les events et orchestre.

---

## 2. Axe A — Locomotion : `ECrewEmbarkState`

Remplace l'actuel `bool bIsGridSpaceAuthority`.

```cpp
UENUM(BlueprintType)
enum class ECrewEmbarkState : uint8
{
    Outside,        // World Space, CMC natif (marche sol monde, nage océan)
    Embarked,       // Local Grid Space, rebase + extract actif
    Transitioning   // Handoff en cours (blend de vélocité / interp rebase)
};
```

### Helpers

```cpp
// USubCrewMovementComponent.h
FORCEINLINE bool IsGridAuthoritative() const
{
    return EmbarkState == ECrewEmbarkState::Embarked
        || EmbarkState == ECrewEmbarkState::Transitioning;
}
```

Le rebase (TickComponent, UpdateBasedMovement, UpdateBasedRotation) lit `IsGridAuthoritative()` au lieu de `bIsGridSpaceAuthority`.

### Transitioning — comportement FP

Pour First Playable : **instant flip**, `Transitioning` est sauté (Outside ↔ Embarked direct).
Le state existe dans l'enum pour que Phase 3 réseau + Phase post-FP puissent y ajouter un blend multi-ticks sans refactor.

---

## 3. Axe B — Environnement : `CurrentCompartment`

### 3.A — `UCompartmentVolumeComponent` enrichi

Le composant existant ([CompartmentVolumeComponent.cpp](../../Source/Sub3D/Submarine/CompartmentVolumeComponent.cpp)) est un `UBoxComponent` dédié aux compartiments. On l'enrichit **sans** ajouter de nouvelle classe sibling :

```cpp
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class UCompartmentVolumeComponent : public UBoxComponent
{
    GENERATED_BODY()
public:
    UCompartmentVolumeComponent();

    /** Id du compartiment dans USubFloodComponent (jonction flood ↔ env). */
    UPROPERTY(EditAnywhere, Category = "Compartment")
    FName CompartmentId = NAME_None;

    /** Stub FP : toujours 1.0. Future simulation O2. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment|Env", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float O2Level01 = 1.f;

    /** Audio volume optionnel à activer quand le crew entre. FP : non câblé. */
    UPROPERTY(EditAnywhere, Category = "Compartment|Audio")
    TObjectPtr<AAudioVolume> LinkedAudioVolume = nullptr;

    /** Post-process volume optionnel. FP : non câblé. */
    UPROPERTY(EditAnywhere, Category = "Compartment|FX")
    TObjectPtr<APostProcessVolume> LinkedPostProcessVolume = nullptr;

    /** Couleur existante (éditeur only). Kept. */
    UPROPERTY(EditAnywhere, Category = "Compartment|Debug")
    FColor VolumeColor = FColor(50, 180, 220, 255);

    UPROPERTY(EditAnywhere, Category = "Compartment|Debug", meta = (ClampMin = "0.0"))
    float LineThickness = 2.f;
};
```

### 3.B — Pointer compartment sur crew

```cpp
// ASubCrewCharacter.h
UPROPERTY(Transient, Replicated)
TWeakObjectPtr<UCompartmentVolumeComponent> CurrentCompartment;

UFUNCTION(BlueprintPure, Category = "Crew|Env")
bool IsInWater() const;

UFUNCTION(BlueprintPure, Category = "Crew|Env")
bool HasOxygen() const;
```

`nullptr` = océan ouvert (infiniment inondé, pas d'O2).

### 3.C — Canal de collision dédié

Nouveau canal `ECC_CompartmentProbe` déclaré dans `Config/DefaultEngine.ini` → `[/Script/Engine.CollisionProfile]`.

- Type : `ECC_GameTraceChannel?` (prochain slot libre)
- Default response : `Ignore`
- `UCompartmentVolumeComponent` : `SetCollisionObjectType(ECC_CompartmentProbe)`, `SetCollisionResponseToAllChannels(Ignore)`, `SetGenerateOverlapEvents(true)`.
- `USubHullBoundaryComponent` : même canal.
- `ASubCrewCharacter::CapsuleComponent` : `SetCollisionResponseToChannel(ECC_CompartmentProbe, ECR_Overlap)`.

Zéro interférence avec gameplay (projectiles, sub sweeps, physics).

### 3.D — Managed overlap + tiebreak

```cpp
// ASubCrewCharacter.cpp — bind OnRegister / BeginPlay
Capsule->OnComponentBeginOverlap.AddDynamic(this, &ASubCrewCharacter::OnCompartmentOverlapBegin);
Capsule->OnComponentEndOverlap.AddDynamic(this, &ASubCrewCharacter::OnCompartmentOverlapEnd);

UPROPERTY() TSet<TWeakObjectPtr<UCompartmentVolumeComponent>> ActiveCompartmentOverlaps;

void ASubCrewCharacter::OnCompartmentOverlapBegin(UPrimitiveComponent* Overlapped, AActor*, UPrimitiveComponent* Other, ...)
{
    if (UCompartmentVolumeComponent* Vol = Cast<UCompartmentVolumeComponent>(Other))
    {
        ActiveCompartmentOverlaps.Add(Vol);
        RecomputeCurrentCompartment();
    }
}

void ASubCrewCharacter::RecomputeCurrentCompartment()
{
    // Tiebreak : plus proche du centre de la capsule.
    UCompartmentVolumeComponent* Best = nullptr;
    float BestDist = FLT_MAX;
    const FVector CapCenter = GetActorLocation();
    for (const auto& W : ActiveCompartmentOverlaps)
    {
        if (UCompartmentVolumeComponent* V = W.Get())
        {
            const float D = FVector::DistSquared(V->GetComponentLocation(), CapCenter);
            if (D < BestDist) { BestDist = D; Best = V; }
        }
    }
    CurrentCompartment = Best;
}
```

---

## 4. Handoff EVA — Velocity Blending au plan de coque

### 4.A — `USubHullBoundaryComponent`

Nouveau composant. **N'est pas** un compartiment — c'est un plan de coque orienté qui détecte le franchissement de la capsule crew.

```cpp
UENUM(BlueprintType)
enum class EHullBoundaryKind : uint8
{
    Airlock,    // Transition prévue (sub en bon état)
    Breach      // Transition chaotique (coque endommagée)
};

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class USubHullBoundaryComponent : public UBoxComponent
{
    GENERATED_BODY()
public:
    USubHullBoundaryComponent();

    /** Normale sortante du plan de coque, en espace local du submarine. */
    UPROPERTY(EditAnywhere, Category = "HullBoundary")
    FVector LocalPlaneNormal = FVector::ForwardVector;

    /** Point sur le plan, en espace local du submarine. */
    UPROPERTY(EditAnywhere, Category = "HullBoundary")
    FVector LocalPlaneOrigin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "HullBoundary")
    EHullBoundaryKind Kind = EHullBoundaryKind::Airlock;

    /** Compartiment du côté intérieur (pour seed du CurrentCompartment au handoff Outside→Embarked). */
    UPROPERTY(EditAnywhere, Category = "HullBoundary")
    FName InsideCompartmentId = NAME_None;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCapsuleCrossedHull, ASubCrewCharacter*, Crew, bool, bOutgoing);
    UPROPERTY(BlueprintAssignable) FOnCapsuleCrossedHull OnCapsuleCrossedHull;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

private:
    /** Side tracking par crew : frame N-1 = +1 outside / -1 inside. Détecte le crossing. */
    TMap<TWeakObjectPtr<ASubCrewCharacter>, int8> LastSideByCrew;
};
```

### 4.B — Détection du crossing

Chaque tick du composant, pour chaque crew actuellement en overlap :

```cpp
const FVector WorldOrigin = GetOwner()->GetActorTransform().TransformPosition(LocalPlaneOrigin);
const FVector WorldNormal = GetOwner()->GetActorTransform().TransformVectorNoScale(LocalPlaneNormal).GetSafeNormal();
const float Dot = FVector::DotProduct(Crew->GetActorLocation() - WorldOrigin, WorldNormal);
const int8 Side = (Dot >= 0.f) ? +1 : -1;

int8& Prev = LastSideByCrew.FindOrAdd(Crew, Side);
if (Side != Prev)
{
    const bool bOutgoing = (Prev == -1 && Side == +1);  // inside → outside
    OnCapsuleCrossedHull.Broadcast(Crew, bOutgoing);
    Prev = Side;
}
```

### 4.C — Formule velocity blending

Appelée par `ASubCrewCharacter::HandleHullCrossing(USubHullBoundaryComponent*, bool bOutgoing)`.

```cpp
void ASubCrewCharacter::HandleHullCrossing(USubHullBoundaryComponent* Boundary, bool bOutgoing)
{
    USubCrewMovementComponent* CrewMov = GetCrewMovement();
    ASubmarineBase* Sub = CurrentSubmarine;
    if (!CrewMov || !Sub || !Sub->InteriorFrame) return;

    const FTransform SubXf = Sub->InteriorFrame->GetSubTransform();
    const FVector V_sub_world = Sub->SubMovement ? Sub->SubMovement->Velocity : FVector::ZeroVector;
    const FVector V_crew_world = CrewMov->Velocity;

    if (bOutgoing)  // Embarked → Outside
    {
        // La capsule EST déjà au bon endroit monde (rebase l'y a mise).
        // On injecte la vélocité de transport pour continuité.
        CrewMov->Velocity = V_crew_world + V_sub_world;
        CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
        SetCurrentCompartment(nullptr);
    }
    else           // Outside → Embarked
    {
        // Seed GridSpaceTransform depuis la position monde actuelle.
        const FVector LocalPos = SubXf.InverseTransformPosition(GetActorLocation());
        const float LocalYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw - SubXf.Rotator().Yaw);
        CrewMov->GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
        CrewMov->LastSubWorldTransform = SubXf;
        CrewMov->Velocity = V_crew_world - V_sub_world;
        CrewMov->SetEmbarkState(ECrewEmbarkState::Embarked);

        // Seed du compartiment depuis l'ID porté par la boundary.
        if (UCompartmentVolumeComponent* Vol = FindCompartmentById(Boundary->InsideCompartmentId))
        {
            SetCurrentCompartment(Vol);
        }
    }
}
```

### 4.D — Breach = même path, trigger dynamique

`USubFloodComponent::CreateBreach(CompartmentId, InflowRate, LocalCenter)` est étendu pour spawner/attacher un `USubHullBoundaryComponent{Kind=Breach, InsideCompartmentId=CompartmentId, LocalPlaneOrigin=LocalCenter, LocalPlaneNormal=<normal approximée>}` sur le submarine.

`RemoveBreach` le détruit.

Force d'aspiration **hors scope FP** — on ne câble que le handoff (crossing → velocity blend). L'event sera suffisant pour tester que le crew aspiré par une brèche passe en Outside avec vélocité continue.

---

## 5. Mapping des piliers design

| Pillar | Mécanique | État FP |
|---|---|---|
| **Flood** | `IsInWater()` compare `CapZ_local` avec `Flood->GetCompartmentWaterHeightCm(CurrentCompartment->CompartmentId)`. Si dehors (nullptr) → toujours dans l'eau | Câblé |
| **O2** | `HasOxygen()` = `CurrentCompartment && CurrentCompartment->O2Level01 > 0` | Stub (O2=1) |
| **Pressure/Current** | `Outside` : hérite `V_sub` au handoff, la vélocité monde porte. World currents → post-FP | Partiel (inheritance au handoff oui, courants non) |
| **Audio/Visual** | Refs `LinkedAudioVolume`/`LinkedPostProcessVolume` stockées. Activation live → post-FP | Refs en place |

---

## 6. Fichiers impactés

### Nouveaux

| Fichier | Contenu |
|---|---|
| `Source/Sub3D/Submarine/SubHullBoundaryComponent.h/.cpp` | `USubHullBoundaryComponent` + `EHullBoundaryKind` |

### Modifiés

| Fichier | Nature |
|---|---|
| `Source/Sub3D/Submarine/SubCrewMovementComponent.h/.cpp` | Refactor `bool bIsGridSpaceAuthority` → `ECrewEmbarkState EmbarkState`. Helper `IsGridAuthoritative()`. Setter `SetEmbarkState(...)`. Tout `bIsGridSpaceAuthority` lu remplacé par le helper |
| `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` | `CurrentCompartment` (TWeakPtr). `ActiveCompartmentOverlaps`. Overlap handlers + tiebreak. `HandleHullCrossing`. `IsInWater`/`HasOxygen`. Seed `EmbarkState = Embarked` dans `EnterOnFootInSubmarine` |
| `Source/Sub3D/Submarine/CompartmentVolumeComponent.h/.cpp` | Enrichissement : `CompartmentId`, `O2Level01`, `LinkedAudioVolume`, `LinkedPostProcessVolume`. `SetGenerateOverlapEvents(true)`, canal `ECC_CompartmentProbe` |
| `Source/Sub3D/Submarine/SubFloodComponent.h/.cpp` | `CreateBreach` spawne/attache un `USubHullBoundaryComponent{Breach}`. `RemoveBreach` le détruit |
| `Source/Sub3D/Debug/Sub3DDebugSettings.h` | `bDrawCrewGridAuthority` toggle pour debug screen env + locomotion |
| `Config/DefaultEngine.ini` | Déclaration canal `ECC_CompartmentProbe` |
| `CLAUDE.md` | Sous-section "Crew environment axis" |

### Non modifiés

| Fichier | Raison |
|---|---|
| `SubInteriorFrameComponent.*` | Toujours correct |
| `SubMovementComponent.*` | Mouvement sub inchangé |
| `SubmarineCompartmentComponent.*` | Manager state (doors, mass totale) — rôle distinct de `UCompartmentVolumeComponent` |

---

## 7. Scope First Playable

### Phase 1 — Infrastructure env (solo) — **COMPLETED 2026-04-22**

| # | Tâche | État |
|---|---|---|
| 1.1 | Enum `ECrewEmbarkState` + refactor `bIsGridSpaceAuthority` → `EmbarkState` + helper `IsGridAuthoritative()` | ✓ |
| 1.2 | `UCompartmentVolumeComponent` enrichi (fields) | ✓ |
| 1.3 | Canal `ECC_CompartmentProbe` déclaré + config collision compartiment + capsule crew | ✓ |
| 1.4 | `CurrentCompartment` sur crew + `ActiveCompartmentOverlaps` + tiebreak plus-proche-centre | ✓ |
| 1.5 | Helpers `IsInWater()` / `HasOxygen()` (stub O2=1) | ✓ |
| 1.6 | Debug screen on-screen : `EmbarkState`, `Mode`, `CompartmentId`, `GridSpaceTransform`, WorldPos, SubPos | ✓ (+ MovementMode field) |
| 1.7 | BP Craniata manuel : 1 `UCompartmentVolumeComponent` par pièce + renseignement `CompartmentId` | ✓ (MainDeck + autres) |

### Phase 2 — EVA handoff (solo) — **COMPLETED 2026-04-22**

| # | Tâche | État |
|---|---|---|
| 2.1 | `USubHullBoundaryComponent` + détection crossing | ✓ (API simplifiée : plane = GetComponentLocation + GetForwardVector, pas de LocalPlaneOrigin/Normal) |
| 2.2 | `HandleHullCrossing` sur crew + velocity blending formule §4.C | ✓ |
| 2.3 | Seed `EmbarkState = Outside` + `CurrentCompartment = nullptr` au disembark | ✓ |
| 2.4 | `USubFloodComponent::CreateBreach` spawne `USubHullBoundaryComponent{Breach}` | ✓ |
| 2.5 | BP Craniata manuel : 1 `USubHullBoundaryComponent{Airlock}` au sas extérieur | ✓ |
| 2.6 | PIE solo : crew sort par le sas → Outside + Flying stub, rentre → Embarked retrouvé | ✓ |
| 2.7 | PIE solo + breach triggered → passe en Outside | À tester complètement |

### Phase 2.5 — Ajustements post-validation solo — **COMPLETED 2026-04-22**

Correctifs appliqués après retours PIE :

| # | Fix | Raison |
|---|---|---|
| 2.5.1 | `MOVE_Flying` + `GravityScale=0` au crossing Embarked→Outside | Pas de volume d'eau océan ; stub en attendant vraie sim ocean |
| 2.5.2 | `MaxFlySpeed=250`, `BrakingDecelerationFlying=1200` sur CMC | Feel heavy-water pour EVA stub |
| 2.5.3 | Hull boundary : single-fire arm/disarm per overlap | Évite ping-pong Walking-collision au crossing inward |
| 2.5.4 | Lazy latch rising-edge only (`bWasEmbarkedLastTick`) | Empêche re-écriture de `EmbarkState=Outside` par le latch après EVA |
| 2.5.5 | `bImpartBaseVelocityX/Y/Z` + `bImpartBaseAngularVelocity` = false sur crew CMC | CMC injectait V_sub à chaque base change (ladder→deck), fight le rebase |
| 2.5.6 | `InteriorFrame::LocalLinearAcceleration` calculé à sim rate (via `GetSimFrameCounter()`) | Évite spike 60Hz render-rate sur la caméra sway |
| 2.5.7 | `ActiveCompartmentOverlaps` seed au BeginPlay via `GetOverlappingComponents` | Overlap begin fire uniquement sur transition → spawn-in-compartment ratait l'init |
| 2.5.8 | Visual interp sub confirmée NÉCESSAIRE (pose lissée entre sim steps 60Hz) | Sans lerp, sub avance par chunks discrets 0/12/24cm au render → stutter perceptible |

### Phase 3 — Réseau unifié (absorbe Phase 2 de 2026-04-21) — **COMPLETED 2026-04-23**

**Prérequis** : Phases 1 + 2 validées en solo. ✓

| # | Tâche | État |
|---|---|---|
| 3.1 | Surcharge `FSavedMove_SubCrew` avec `GridSpaceTransform` + `EmbarkStateByte` + handoff event | ✓ [SubCrewNetTypes.h/.cpp](../../Source/Sub3D/Submarine/SubCrewNetTypes.h) |
| 3.2 | `FCharacterNetworkMoveData_SubCrew` sérialise grid-space en payload ServerMove ; `MoveAutonomous` override lit et applique côté serveur | ✓ |
| 3.3 | Réplication `GridSpaceTransform` + `EmbarkState` (`COND_SkipOwner`) | ✓ |
| 3.4 | Réplication `CurrentCompartmentId` (FName + `OnRep_CurrentCompartmentId` résout le pointeur) | ✓ |
| 3.5 | Handoff prédit : `HandleHullCrossing` client fire `SetPendingHandoff`, capturé dans FSavedMove, appliqué serveur | ✓ |
| 3.6 | Test Play-as-Client × 2 | ✓ Perso synchronisé, peer crew cohérent sur les deux clients |

### Phase 3.5 — Spawn slots & PlayerController init — **COMPLETED 2026-04-23**

Corrige le bug "un seul joueur spawn dans le sub" (spawn au même world transform → collision).

| # | Fix | Fichier |
|---|---|---|
| 3.5.1 | `AssignedSpawnSlot` (Replicated int32) sur `ASubPlayerController` | [SubPlayerController.h/.cpp](../../Source/Sub3D/Submarine/SubPlayerController.h) |
| 3.5.2 | `CrewSpawnSlotOffsetsLocal` (TArray<FVector> sub-local) sur `ASubGameMode` | [SubGameMode.h](../../Source/Sub3D/Submarine/SubGameMode.h) |
| 3.5.3 | `PostLogin` assigne slot incrémental (max+1) | [SubGameMode.cpp](../../Source/Sub3D/Submarine/SubGameMode.cpp) |
| 3.5.4 | `ResolveCrewSpawnTransform(SlotIndex)` applique offset sub-local rotaté | idem |
| 3.5.5 | `RestartPlayer` + `InitializePlayerCrewState` + `SpawnAndEmbarkPendingControllers` utilisent le slot du PC | idem |

---

## 8. Ordonnancement & dépendances

```
[DONE]  2026-04-21 · Phase 1 (rebase solo)          — commits 24b1cb7, a95ce17, 716d1a2
[DONE]  2026-04-22 · Phase 1 (env axis infra solo)  — ce doc
[DONE]  2026-04-22 · Phase 2 (EVA handoff solo)     — ce doc
[DONE]  2026-04-22 · Phase 2.5 (tuning post-PIE)    — bImpartBase, InteriorFrame sim-rate accel, overlap seed, visual interp restored
[DONE]  2026-04-23 · Phase 3 (réseau unifié)        — FSavedMove_SubCrew + ServerMove grid-space + spawn slots — validé Play-as-Client × 2
[DONE]  2026-04-23 · Phase 4 (flood FP)             — InitializeFromCompartmentVolumes priorité, DevCheat Server RPC, log centralisé, crew anim HUD retiré
[NEXT]  Flood visuals (water planes depuis volumes) OU edges entre compartments (propagation)
[THEN]  Stairs → Ramps asset swap (dette CLAUDE.md)
```

Règle d'or : **pas de réseau tant que les deux axes solo ne sont pas stabilisés**. Évite 2 refactors FSavedMove évitables.

### Dette résiduelle (hors code, flaggée dans CLAUDE.md)

- **Stairs → Ramps** : les `SM_Stair_*` du BP Craniata utilisent toujours collision complex. Le jitter micro à la transition `Deck ↔ Stair` est un problème d'asset (step-edge discontinuities + CMC StepUp + slope walkable angle), pas du rebase. Solution stricte : remplacer par rampes à collision simple.
- **O2 simulation** : stub `O2Level01 = 1.f`. Production/consommation post-FP.
- **Audio/PP live switching** : refs stockées dans `UCompartmentVolumeComponent`, activation au changement de compartiment différée post-FP.
- **Breach aspiration force** : l'event handoff est câblé, la force d'aspiration vers le trou n'est pas appliquée. Event-only pour FP.
- **Ocean swim proper** : stub via `MOVE_Flying`. `APhysicsVolume{bWaterVolume=true}` + buoyancy + courants mondiaux post-FP.

---

## 9. Décisions verrouillées

- **Q-A** : On évite le dédoublement (pas de `USubCompartmentComponent` séparé) mais on ne mélange pas la logique. Enrichissement de `UCompartmentVolumeComponent`, séparation stricte locomotion/environnement au niveau des owners.
- **Q-B** : Canal de collision dédié `ECC_CompartmentProbe`. Un seul canal pour `UCompartmentVolumeComponent` ET `USubHullBoundaryComponent`. Capsule crew répond `Overlap`, tout le reste ignore.
- **Q-C** : FP scope = Phase 1 + Phase 2. Audio/PP live switching **non câblé** (refs en place). EVA **inclus** dans FP (stress test architecture). Breach **inclus** (crossing + velocity blend, sans force d'aspiration).
- **Transitioning state** : instant flip pour FP, placeholder dans l'enum pour Phase 3+ (blend multi-ticks si nécessaire).
- **O2 simulation** : stub `O2Level01 = 1` pour FP. Modélisation future hors scope.

---

## 10. Risques identifiés

| Risque | Mitigation |
|---|---|
| `SetWorldLocationAndRotation` au handoff Outside→Embarked peut briser CMC Velocity interne | On utilise `Crew->CrewMov->Velocity` directement (pas `SetActorLocationAndRotation`). Le seed de `GridSpaceTransform` est purement data |
| Double overlap (crew au bord entre deux compartiments) → flip-flap de `CurrentCompartment` | Tiebreak distance au centre (§3.D). Audio/PP activation différée post-FP évite un jitter audio |
| `CreateBreach` spawne un boundary mais la normale approximée est fausse | FP : normale calculée depuis `(BreachLocalCenter - SubCenter).Normalize()`. Approximatif, suffisant pour tester le handoff. Amélioration post-FP via vraie géométrie coque |
| L'enum refactor touche 30+ call-sites | Helper `IsGridAuthoritative()` pour minimiser churn. Grep strict de `bIsGridSpaceAuthority` avant et après |
| `TWeakObjectPtr` replication → pas standard | Phase 3 seulement. Alternative : répliquer `FName CurrentCompartmentId` et résoudre côté receveur |
