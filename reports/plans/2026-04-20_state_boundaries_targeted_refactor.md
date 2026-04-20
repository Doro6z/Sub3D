# Targeted State-Boundaries Refactor — Design — 2026-04-20

**Remplace** : [2026-04-20_crew_passenger_frame_refactor_design.md](./2026-04-20_crew_passenger_frame_refactor_design.md) (scope trop large, archivé comme référence pour un éventuel Round 2).

**Diagnostic** : les bugs qu'on a shippés ne sont pas des bugs isolés. Ce sont des symptômes d'une même cause : **absence de frontières claires entre 4 types d'état distincts**. Chaque fois qu'un état a fuité dans un autre, on a produit un bug.

**But de ce refactor** : rendre ces 4 frontières **explicites dans le code**, avec des contrats clairs sur ce qui peut lire/écrire quoi. Pas de réécriture des systèmes. Juste des frontières.

---

## 1. Les 4 états

### 1.1 SimState — vérité autoritaire

**Contenu** : ce que la sim physique produit à chaque step.

```
SimLocation, SimRotation       (post-SimulateStep pose)
SimVelocity                    (sim velocity vector)
SimYawRate, SimPitchRate       (sim angular rates)
SimFrameCounter                (monotonically increasing)
```

**Qui écrit** : `USubMovementComponent::SimulateStep` (authority only).
**Qui lit** : `RefreshRepState`, PresentedPose compositor, HUD (analytically).
**Invariant** : **jamais écrit depuis PresentedPose ou NetState**. Source of truth.

### 1.2 NetState — ce qui traverse le réseau

**Contenu** : subset sérialisable de SimState + CommandState.

```
FSubmarineNetState:
  WorldLocation, QuantizedRotation
  LinearVelocity, AngularVelocity
  ForwardSpeed, VerticalSpeed
  DepthMeters, FloodedMassKg, BallastGlobal01
  SimFrame
  MainTrim01, bPumpActive
```

**Qui écrit** : `ASubmarineBase::RefreshRepState` (server only, called after SimulateStep).
**Qui lit** : UE replication layer → client `OnRep_RepState`.
**Invariant** : jamais écrit depuis PresentedPose.

### 1.3 PresentedPose — ce que la caméra voit

**Contenu** : pose visuelle interpolée pour le render, côté authority ET côté remote.

```
PrevSim, CurrSim              (authority: lerp bounds between sim steps)
PrevSnapshot, TargetSnapshot  (remote: lerp bounds between net snapshots)
PresentedLocation, PresentedRotation  (last composed visual pose)
```

**Qui écrit** : `USubMovementComponent::TickComponent` (interp logic).
**Qui lit** : `Owner->SetActorTransform` (actor root), InteriorFrame (with caveats).
**Invariant** : **jamais source de vérité pour sim ou net**. Read-only output.

### 1.4 EmbarkState — contexte gameplay crew

**Contenu** : dans quel contexte gameplay le crew se trouve.

```
ECrewEmbarkState:
  OffSub
  OnFootInSub
  AtHelm
  AtStation(StationId)
CurrentSubmarine
CurrentStationId
```

**Qui écrit** : une fonction unique `TransitionTo(NewState, Sub, Station)`.
**Qui lit** : SubCrewMovementComponent, SubCrewCharacter, HUD, gameplay code.
**Invariant** : **un seul point d'entrée pour changer l'état**. Pas de modifications partielles.

---

## 2. Fuites actuelles (à fermer)

### Fuite 1 — SimState ← PresentedPose
Ancien α=0 skip (déjà fixé) : l'undo de début de tick restaurait la pose sim depuis... elle-même capturée post-sim, mais la fuite venait de l'écriture PresentedPose sur l'actor root qui polluait `GetActorLocation()` à la frame suivante. **Fix shipped** (Fix 1).

### Fuite 2 — NetState ← sim dérivée incomplète
`ForwardSpeedCmS` sur client : non écrit par `HandleReplicatedNetState` donc HUD lit 0 sur remote. **Fix shipped** (Fix 2).

### Fuite 3 — Derived signals ← PresentedPose instead of SimState
InteriorFrame dérivait vélocité de la pose visuelle (= PresentedPose). Symptôme : double dérivation = spikes. **Fix shipped** (S1 : lire `SimState.Velocity` directement).

### Fuite 4 — EmbarkState via 6 entry points → init partielle
`EnterOnFoot`, `Board`, `TakeHelm`, `ForceHelm`, `ReleaseHelm`, `Disembark`. Chaque fonction fait une init partielle qui suppose l'état précédent. Enchainés dans un ordre inattendu (ex : ForceHelm après un changement de base), l'état devient incohérent. **À fermer**.

### Fuite 5 — Walkable validation runtime ↔ EmbarkState
`IsAcceptedEmbarkedBase` (runtime) utilise un tag éditeur (`HandmadeWalkable`). Si tag manquant, `AcceptedBase=0` déclenche `bNeedsEmbarkedFloorRecovery`, boucle infinie dans `RefreshEmbarkedFlooring` avec `Velocity = FVector::ZeroVector`. **À fermer**.

### Fuite 6 — BasedMovement ← RefreshEmbarkedFlooring side-effects
Le recovery loop écrit Velocity=0 et force SetBaseFromFloor, ce qui perturbe `UpdateBasedMovement` stock qui applique des deltas de base incohérents. **Dépend de fermer Fuite 5**.

---

## 3. Changements proposés (tout le refactor)

### 3.1 Nommer explicitement les 4 zones dans `USubMovementComponent`

Actuellement les membres sont mélangés. Les grouper par zone avec commentaires explicites :

```cpp
// ────────────────────────────────────────────────────────────────
// ZONE 1 — SimState (authority, truth, written by SimulateStep)
// ────────────────────────────────────────────────────────────────
FVector Velocity;
float YawRateDegPerSec;
float PitchRateDegPerSec;
float CurrentDepth;
float FloodedMassKg;
float ForwardSpeedCmS;
int32 SimFrameCounter;

// ────────────────────────────────────────────────────────────────
// ZONE 2 — PresentedPose (derived, written by TickComponent interp)
// Authority path:
FVector PrevSimLocation;
FRotator PrevSimRotation;
FVector AuthoritativeLocation;  // renamed to CurrSimLocation for clarity
FRotator AuthoritativeRotation; // renamed to CurrSimRotation
bool bHasSimBuffer;
bool bHasVisualOffset;
// Remote path:
FSubmarineNetState PrevSnapshot;
FSubmarineNetState TargetSnapshot;
float InterpAlpha;
float InterpDuration;
bool bHasReceivedSnapshot;

// ────────────────────────────────────────────────────────────────
// ZONE 3 — NetState interface (written by RefreshRepState)
// FSubmarineNetState RepState lives on ASubmarineBase.
// This component only PROVIDES the values. Does not store NetState itself.
// ────────────────────────────────────────────────────────────────
```

Coût : **commentaires + rename mineur**. Pas de nouvelle logique. Le code existant marche pareil mais devient lisible.

### 3.2 Consolider `EmbarkState` sur le crew

Ajouter à `USubCrewMovementComponent` (ou `ASubCrewCharacter`, à décider selon l'architecture de réplication existante) :

```cpp
UENUM(BlueprintType)
enum class ECrewEmbarkState : uint8
{
    OffSub,
    OnFootInSub,
    AtHelm,
    AtStation,
};

UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|State")
ECrewEmbarkState CurrentEmbarkState = ECrewEmbarkState::OffSub;

UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|State")
FName CurrentStationId = NAME_None;

UFUNCTION(BlueprintCallable, Category = "Crew|State")
bool TransitionTo(ECrewEmbarkState NewState,
                  ASubmarineBase* TargetSubmarine = nullptr,
                  FName StationId = NAME_None,
                  const FTransform& DesiredTransform = FTransform::Identity);
```

**Règles** (matrice implémentée dans `TransitionTo`) :

| From ↓ / To → | OffSub | OnFootInSub | AtHelm | AtStation |
|---|---|---|---|---|
| **OffSub** | no-op | ✓ via EnterOnFoot | ✗ | ✗ |
| **OnFootInSub** | ✓ via Disembark | no-op / change sub | ✓ via TakeHelm | ✓ via TakeStation |
| **AtHelm** | ✗ (release first) | ✓ via ReleaseHelm | no-op | ✓ via TakeStation |
| **AtStation** | ✗ | ✓ via LeaveStation | ✓ | no-op / change station |

**Entry points existants deviennent des wrappers** :
- `EnterOnFootInSubmarine(Sub, Xform)` → `TransitionTo(OnFootInSub, Sub, NAME_None, Xform)`
- `BoardSubmarine(Sub)` → idem avec transform par défaut
- `TakeHelm()` → `TransitionTo(AtHelm, CurrentSubmarine, "Helm")`
- `ForceHelm()` → `TransitionTo(AtHelm, CurrentSubmarine, "Helm")` avec flag `bServerForced=true`
- `ReleaseHelm()` → `TransitionTo(OnFootInSub, CurrentSubmarine)`
- `DisembarkSubmarine()` → `TransitionTo(OffSub, nullptr)`

`TransitionTo` fait **toute l'init** : spawn transform, character movement mode, base, walking state, etc. Les wrappers ne font **rien de plus que setup les arguments**. Plus de divergence possible.

**Invariant** : "dommage permanent post-helm" disparaît car toute transition passe par le même init.

### 3.3 Déplacer la validation walkable hors runtime

Actuellement `IsAcceptedEmbarkedBase` filtre par tag à chaque tick. C'est la source de la boucle recovery sur stairs.

**Proposition** :

1. **BeginPlay du sub** : builder une liste cachée `AcceptedWalkableComponents` de tous les `UPrimitiveComponent` enfants du sub qui sont soit :
   - tagués `HandmadeWalkable`, **soit**
   - de type `UStaticMeshComponent` avec collision walkable (channel `ECC_Pawn` = Block).
   
   Cela garde le tag comme hint optionnel, mais **toute surface walkable par défaut UE est acceptée**.

2. **`IsAcceptedEmbarkedBase`** lit la liste cachée. O(log n) avec set, pas de trace.

3. **`bNeedsEmbarkedFloorRecovery`** déclenché uniquement si la base est `nullptr` (crew qui flotte), pas si "base not in accepted list". **Supprime le scénario "stairs rejetés"**.

4. **`RefreshEmbarkedFlooring`** garde sa logique mais cesse d'être appelée en boucle par un rejet walkable légitime.

Coût : **~30 lignes** dans `SubmarineBase.cpp` (cache builder) + ~10 lignes dans `SubCrewMovementComponent.cpp` (changer la condition recovery).

### 3.4 Documenter les invariants dans le code

Ajouter des `static_assert` / comment headers sur les fonctions qui touchent plusieurs zones :

```cpp
// INVARIANT: This function reads ONLY SimState. Never PresentedPose.
// If this assumption breaks, jitter spikes from double-derivation will return.
void ASubmarineBase::RefreshRepState()
{
    // Tick-order invariant: called before TickComponent's PresentedPose write.
    // Samples Owner->GetActorLocation() which at this point = CurrSim (post-SimulateStep, pre-interp).
    RepState.WorldLocation = GetActorLocation();
    // ...
}

// INVARIANT: bWroteInterp indicates actor root is at PresentedPose, not SimState.
// Next tick's Undo restores SimState before SimulateStep runs.
// RefreshRepState must not run between Undo and next SimulateStep output.
```

Coût : **commentaires**. Zéro runtime cost, énorme gain de maintenabilité.

---

## 4. Ce qui N'EST PAS dans ce refactor

**Explicitement hors scope** :

- **Passenger-frame / sub-local sim** : pas de changement dans la façon dont le crew simule. UE CMC stock reste utilisé, avec `BasedMovement` stock. C'est la différence fondamentale avec l'autre doc.
- **Camera sway / anim IK** : pas touché.
- **`USubInteriorFrameComponent`** : pas touché (S1 a déjà fait le fix "lire SimState").
- **Replication model** : pas changé (on garde RepState world-space).
- **Removal de compensations existantes** : `ApplyYawCompensation`, `bIgnoreBaseRotation`, etc. **conservés**. Ils marchent, on les laisse.
- **Math/physique sub** : pas touché.

**Ce n'est donc pas "passenger-frame light"**. C'est un refactor de **contrats d'état**, pas de sim.

---

## 5. Comment ça résout (ou pas) les bugs observés

| Bug | Résolu par ce refactor ? | Mécanisme |
|---|---|---|
| α=0 skip BACKWARD | Déjà fixé (Fix 1) | — |
| Double dérivation InteriorFrame | Déjà fixé (S1) | — |
| HUD SPD client | Déjà fixé (Fix 2) | — |
| InterpDuration long en coop | Déjà mitigé (clamp) | — |
| Boucle escaliers recovery | **OUI** | Section 3.3 : accepted walkables cachés, stairs auto-included si walkable collision |
| Helm path divergence / dommage permanent | **OUI** | Section 3.2 : `TransitionTo` unique |
| Jitter résiduel standalone haute FPS | **NON** | Nécessite passenger-frame ou camera late-update (hors scope) |
| 2-client jitter PIE | **NON** | Artefact PIE, pas un bug code |

**Le refactor répare les 2 bugs gameplay critiques** (stairs + helm) **par établissement de frontières claires**, pas par patch. Les jitters résiduels restent "bruit acceptable" en l'état, à traiter ultérieurement si bloquant.

---

## 6. Phases et budget

| Phase | Scope | Durée |
|---|---|---|
| P1 | Grouper/commenter les zones SimState/PresentedPose/NetState dans `USubMovementComponent` (3.1) | **0.5 j** |
| P2 | Ajouter `ECrewEmbarkState` + `TransitionTo` + wrappers (3.2) | **1 j** |
| P3 | Cached walkable list + recovery condition fix (3.3) | **0.5 j** |
| P4 | Documenter invariants (3.4) | **0.25 j** |
| P5 | Tests : matrice réduite (standalone + 2-PIE), focus stairs + helm handoff | **0.75 j** |

**Total : ~3 jours** de dev focalisé. Loin des 10-13 j du passenger-frame complet.

---

## 7. Tests de validation

### P2 (state machine) — non-régression embark
- Fresh PIE standalone : crew spawn off sub → board → walk → take helm → release → walk → disembark. Pas d'état cassé.
- Enchainements rapides : TakeHelm puis ForceHelm puis ReleaseHelm. Pas de dommage.
- 2-client PIE : Client 1 TakeHelm, Client 2 TakeHelm (refus), Client 1 Release, Client 2 TakeHelm (accepte). Smooth.

### P3 (walkables) — non-régression stairs
- PIE standalone : crew sur deck, monter stairs, redescendre. Pas de jitter. Pas de log `bNeedsEmbarkedFloorRecovery=true`.
- PIE avec sub en mouvement : idem en cruise. Crew walk through stairs sans freeze-cycles.
- Log target : plus aucune ligne `Recover=1` liée à stairs.

### P1 + P4 (doc/invariants) — 0 régression comportementale
- Les tests de non-régression des fixes précédents doivent toujours passer (0 BACKWARD, LocalAccel normal, SPD à jour).

---

## 8. Validation utilisateur — points à arbitrer

1. **Scope accepté** : ce refactor (3 j) à la place du passenger-frame (10-13 j). Tu confirmes ?
2. **Jitter résiduel left alone** : on accepte le bruit de fond haute FPS en standalone pour l'instant, à traiter plus tard si bloquant. OK ?
3. **`HandmadeWalkable` tag** : garder comme hint éditeur optionnel, **mais pas bloquant au runtime**. OK ?
4. **Ordre phases** : P1 → P2 → P3 → P4 → P5 séquentiel, validation test à chaque étape. OK ?
5. **Branche** : direct sur `scripting` ou nouvelle `feature/state-boundaries` ?

Si tu valides, j'attaque P1 immédiatement. Pas de code avant ton OK.
