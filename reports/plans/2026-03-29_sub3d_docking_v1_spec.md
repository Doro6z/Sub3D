# Sub3D — Docking V1 Spec

**Date**: 2026-03-29
**Status**: Authoritative technical spec — Phase FP-5 entry point
**Parent documents**:
- `C:\ACC\Projects\Sub3D\Plans\sub3d\sub3d_product_north_star.md` (First Playable sequence — step 8/9)
- `2026-03-29_sub3d_first_playable_run_spec.md` (Docking V1 contract, section 8)

---

## 1. Scope

This spec defines docking as a deliberate, manual closure of the First Playable run.

It covers:
- `ASubDockActor` — new external world actor (start dock + end dock)
- Alignment check (position + heading tolerances)
- `IA_DockingConfirm` input routing
- Post-success airlock stub
- `ESubStationType` extension for docking
- GameMode integration (bridge to run spec section 8)
- Source files
- Done criteria

This spec does **not** cover:
- Full pressurized airlock simulation (equalization, suit change)
- Campaign-side dock registry or port network
- Dock art or geometry realization (level architecture spec)
- Dock damage or blockage
- Start dock departure flow (covered by run spec, Phase FP-2)

---

## 2. Non-Goals

| Out of scope | Reason |
|---|---|
| Airlock equalization simulation | Post-FP — North Star explicit freeze |
| Dock-side crew disembark to underwater station | Post-FP |
| Dock occupancy for multiple submarines | Not required for FP |
| Dock as submarine station (`ASubStationBase`) | Wrong parent — dock is external, not onboard |
| Automatic docking | North Star requires manual + precision |
| Docking camera animation or cinematic | Track D, not required for loop proof |

---

## 3. Architectural Decision: Dock is Not a Submarine Station

The existing station architecture (`ASubStationBase`, `ISubStationInterface`) is designed for **onboard crew stations** — helm, ballast, engine, turret.

A dock is an **external world actor**: the submarine aligns to it, not the crew.

`ASubDockActor` does **not** inherit from `ASubStationBase`.

It is a standalone `AActor` with:
- a trigger volume (submarine overlap detection)
- an alignment socket (position + heading reference)
- a type flag (start or end dock)

This avoids coupling the dock to the station occupancy/ownership system, and keeps the dock's responsibility narrow: detect submarine proximity, expose alignment data, notify GameMode.

---

## 4. ASubDockActor

### 4.1 Location

New file: `Source/Sub3D/Submarine/SubDockActor.h/.cpp`

### 4.2 Enum: EDockType

New enum in `SubDockActor.h`:

```cpp
UENUM(BlueprintType)
enum class EDockType : uint8
{
    Start  UMETA(DisplayName = "Start Dock"),
    End    UMETA(DisplayName = "End Dock"),
};
```

### 4.3 Properties

```cpp
// Dock type — determines which GameMode reference slot this dock fills
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dock")
EDockType DockType = EDockType::End;

// The point toward which the submarine must align
// Forward vector of this component = expected submarine approach heading (sub bow faces this)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
USceneComponent* DockSocket;

// Overlap volume — triggers Approach phase and Docking phase checks
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
UBoxComponent* DockingVolume;

// Optional visual mesh (placeholder for V1)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dock")
UStaticMeshComponent* DockMesh;

// Docking tolerances — override GameMode defaults if > 0
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dock|Alignment")
float AlignmentToleranceCm = 0.f;  // 0 = use GameMode default (300cm)

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dock|Alignment")
float AlignmentAngleDeg = 0.f;     // 0 = use GameMode default (20°)
```

### 4.4 Methods

```cpp
// Returns true if the given submarine passes alignment check
UFUNCTION(BlueprintCallable, Category = "Dock")
bool IsSubmarineAligned(const ASubmarineBase* Sub, float ToleranceCm, float AngleDeg) const;

// Expose socket world location for GameMode polling
UFUNCTION(BlueprintPure, Category = "Dock")
FVector GetDockSocketLocation() const;

// Expose expected approach heading (DockSocket forward = direction sub should face)
UFUNCTION(BlueprintPure, Category = "Dock")
FVector GetApproachDirection() const;
```

### 4.5 Overlap callbacks

```cpp
// Server-only
UFUNCTION()
void OnDockingVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult);

UFUNCTION()
void OnDockingVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
```

`BeginOverlap`: if `OtherActor` is `ASubmarineBase` and `DockType == End` → notify GameMode (`ASubGameMode::OnSubmarineEnterEndDock`).

`EndOverlap`: if same sub exits during `Docking` phase → `GameMode->TriggerRunFailure()`.

`BeginOverlap` for `DockType == Start` is used only for departure detection → notified but GameMode handles that separately (Phase FP-2 concern, not this spec).

---

## 5. Alignment Check

### 5.1 Position tolerance

```
float Dist = FVector::Dist(Sub->GetActorLocation(), DockSocket->GetComponentLocation());
bool bPositionOk = Dist <= EffectiveToleranceCm;
```

`EffectiveToleranceCm` = dock's `AlignmentToleranceCm` if > 0, else GameMode's `DockingAlignmentToleranceCm` (default: 300cm).

### 5.2 Heading tolerance

The `DockSocket`'s forward vector defines the expected approach direction — the direction the submarine bow should be facing when docking.

```
FVector SubForward = Sub->GetActorForwardVector();
FVector ExpectedApproach = DockSocket->GetForwardVector();
float DotProduct = FVector::DotProduct(SubForward.GetSafeNormal(), ExpectedApproach.GetSafeNormal());
float AngleDelta = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.f, 1.f)));
bool bHeadingOk = AngleDelta <= EffectiveAngleDeg;
```

`EffectiveAngleDeg` = dock's `AlignmentAngleDeg` if > 0, else GameMode's `DockingAlignmentAngleDeg` (default: 20°).

### 5.3 Combined result

```cpp
bool ASubDockActor::IsSubmarineAligned(const ASubmarineBase* Sub, float ToleranceCm, float AngleDeg) const
{
    if (!Sub || !DockSocket) return false;
    float EffTol = (AlignmentToleranceCm > 0.f) ? AlignmentToleranceCm : ToleranceCm;
    float EffAng = (AlignmentAngleDeg > 0.f) ? AlignmentAngleDeg : AngleDeg;
    float Dist = FVector::Dist(Sub->GetActorLocation(), DockSocket->GetComponentLocation());
    FVector SubFwd = Sub->GetActorForwardVector().GetSafeNormal();
    FVector DockFwd = DockSocket->GetForwardVector().GetSafeNormal();
    float Dot = FVector::DotProduct(SubFwd, DockFwd);
    float AngleDelta = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));
    return (Dist <= EffTol) && (AngleDelta <= EffAng);
}
```

### 5.4 GameMode polling

`ASubGameMode` polls alignment every tick when in `Approach` or `Docking` phase:

```cpp
void ASubGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (RunPhase != ESubRunPhase::Approach && RunPhase != ESubRunPhase::Docking) return;
    if (!ActiveSubmarine || !EndDock) return;

    bool bAligned = EndDock->IsSubmarineAligned(ActiveSubmarine,
        DockingAlignmentToleranceCm, DockingAlignmentAngleDeg);

    ASubGameState* GS = GetGameState<ASubGameState>();
    if (GS && GS->bDockingAligned != bAligned)
    {
        GS->bDockingAligned = bAligned;
        GS->ForceNetUpdate();
    }
}
```

`bDockingAligned` on GameState drives the helm widget alignment indicator.

---

## 6. Docking Confirm Input

### 6.1 Who can trigger docking

Any crew member can trigger the docking confirm — not just the pilot.

Rationale: in solo, the player is always the pilot. In coop, a dedicated crew member could handle docking while the pilot holds position. Both are valid.

### 6.2 New action

`IA_DockingConfirm` — mapped to `[F]` or an interact key (to be decided in BP input mapping).

Active input context: `IMC_OnFoot` and `IMC_HelmDriving` — valid in both modes.

The action is gated server-side (not by input context alone):
- `RunPhase == Approach`
- `bDockingAligned == true`

If either condition fails, the attempt is rejected silently.

### 6.3 New controller method

Add to `ASubPlayerController.h`:

```cpp
UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
void ServerRouteDockingConfirm();
```

Implementation:
```cpp
void ASubPlayerController::ServerRouteDockingConfirm_Implementation()
{
    ASubGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASubGameMode>() : nullptr;
    if (GM) GM->AttemptDocking();
}
```

`AttemptDocking()` already validates `RunPhase` and `bDockingAligned` before accepting (defined in run spec).

### 6.4 Helm widget alignment indicator

`USubHelmWidget` (or its BP) reads `ASubGameState::bDockingAligned` and shows a simple alignment cue:
- Aligned: green indicator or text "DOCKING READY"
- Not aligned: no indicator (or subtle red if the dock is in proximity)

This is a BP-side concern — the spec only mandates that `bDockingAligned` is readable from widget.

---

## 7. Post-Success Airlock Stub

### 7.1 North Star intent

The North Star says "Accostage manuel + interaction sas" — the run closes with a docking AND an airlock interaction.

For V1, the airlock interaction is a **stub**: it is triggered by crew interacting with the submarine's forward hatch after `Success`. It does not simulate pressure equalization or crew transfer.

### 7.2 Sub forward hatch interactable

After `Success`:
- A `UInteractableComponent` on the submarine's forward hatch becomes active
- Crew approaches and interacts (existing `ASubCrewCharacter::Interact()` flow)
- `OnInteract` fires a `BlueprintImplementableEvent` on the hatch BP: `BP_OnAirlockTriggered`
- No C++ logic beyond the event — the BP handles the feedback (sound, brief hold, fade)

This does **not** require a new C++ class. The existing `UInteractableComponent` + a hatch BP actor is sufficient.

### 7.3 What V1 does not simulate

- No pressure equalization sequence
- No crew health effect from exiting
- No camera transition to dock side
- No suit change or decompression timer

These are all Track A / Track D concerns, post-FP.

### 7.4 Relationship to run state

The airlock interaction fires **after** `Success`. It does not gate the run success — success is already declared on docking confirm. The airlock is a narrative closure, not a gameplay condition.

`ESubRunPhase` does not add a `Disembark` state for FP.

---

## 8. ESubStationType Extension

Docking confirm is not a station action. No new `ESubStationType` value is needed for V1.

However, `ESubStationType` should receive a `Docking` entry as a future-seam value for when the docking panel becomes a proper interactive station (post-FP):

```cpp
// In SubmarineTypes.h — add to ESubStationType enum:
Docking UMETA(DisplayName = "Docking", Hidden),
```

`Hidden` keeps it out of editor dropdowns for now. It is just a reserved seam.

---

## 9. GameMode Integration Summary

This spec bridges to run spec section 8 as follows:

| Run spec contract | Where it lives |
|---|---|
| `DockingAlignmentToleranceCm` | `ASubGameMode` property |
| `DockingAlignmentAngleDeg` | `ASubGameMode` property |
| `EndDock` reference | `ASubGameMode` property, set to `ASubDockActor` in editor |
| `AttemptDocking()` | `ASubGameMode` method — called by `ServerRouteDockingConfirm()` |
| `bDockingAligned` | `ASubGameState` replicated property — written by GameMode tick |
| `DockingVolume` overlap → `Approach` | `ASubDockActor::OnDockingVolumeBeginOverlap` → `GameMode->OnSubmarineEnterEndDock()` |
| `DockingVolume` exit during `Docking` → `Failure` | `ASubDockActor::OnDockingVolumeEndOverlap` → `GameMode->TriggerRunFailure()` |

New GameMode method (not in run spec — added here):

```cpp
// Called by ASubDockActor when submarine enters end dock volume
UFUNCTION()
void OnSubmarineEnterEndDock();
```

Implementation:
```cpp
void ASubGameMode::OnSubmarineEnterEndDock()
{
    if (RunPhase == ESubRunPhase::Traverse || RunPhase == ESubRunPhase::BreachCrisis)
    {
        AdvanceRunPhase(ESubRunPhase::Approach);
    }
}
```

---

## 10. Source Files

### New Files

| File | Content |
|---|---|
| `Source/Sub3D/Submarine/SubDockActor.h/.cpp` | `ASubDockActor` — dock trigger, socket, alignment check, overlap routing |

### Modified Files

| File | Change |
|---|---|
| `Source/Sub3D/Submarine/SubmarineTypes.h` | Add `Docking` (Hidden) to `ESubStationType` |
| `Source/Sub3D/Submarine/SubGameMode.h/.cpp` | Add `EndDock` ref, `StartDock` ref, `OnSubmarineEnterEndDock()`, alignment tick |
| `Source/Sub3D/Submarine/SubPlayerController.h/.cpp` | Add `ServerRouteDockingConfirm()` |

### Not Modified

| File | Reason |
|---|---|
| `SubStationBase.h/.cpp` | Dock is not a station |
| `SubStationInterface.h` | Not applicable |
| `SubCrewCharacter.h/.cpp` | Existing `Interact()` handles airlock stub |
| `SubmarineBase.h/.cpp` | No change |

---

## 11. Runtime Contracts

### Authority

- `ASubDockActor` overlap callbacks are **server-only** (`SetIsReplicated(false)` on the dock, or ensure overlap events only fire on server)
- `bDockingAligned` is written by server (GameMode tick) and replicated to clients
- `AttemptDocking()` is server-only — always called via `ServerRouteDockingConfirm()`

### Idempotence

- `OnSubmarineEnterEndDock()` is only accepted in `Traverse` or `BreachCrisis` phase — safe to ignore duplicate calls
- `AttemptDocking()` is only accepted in `Approach` phase — safe to ignore if called in wrong phase
- `TriggerRunFailure()` from dock exit is only accepted in `Docking` phase — safe to ignore if sub exits during `Approach`

### Placement contract

For correct behavior:
- `DockingVolume` must be large enough to contain the submarine during the full alignment and docking sequence — recommended: `400 × 400 × 200 cm` minimum at dock socket center
- `DockSocket` forward vector must face the expected submarine bow direction (the direction the sub is pointing when correctly aligned)
- Start dock volume overlap fires departure detection — this is a GameMode-level concern, not part of this spec

---

## 12. Test Criteria

### Automation (EditorContext)

- `FSubDockAlignmentPass`: place `ASubDockActor` at origin, `DockSocket` facing +X. Place sub at 200cm ahead, facing +X (dot product ≈ 1). Assert `IsSubmarineAligned(Sub, 300f, 20f) == true`
- `FSubDockAlignmentFailDistance`: same setup but sub at 400cm. Assert `IsSubmarineAligned == false`
- `FSubDockAlignmentFailAngle`: sub at 200cm but rotated 30° from expected heading. Assert `IsSubmarineAligned == false`
- `FSubDockConfirmRejectsOutsideApproach`: call `AttemptDocking()` when `RunPhase == Traverse`. Assert phase remains `Traverse`

### PIE Manual

- [ ] Sub enters end dock volume — `RunPhase` transitions to `Approach`
- [ ] `bDockingAligned` becomes true when sub is positioned and aligned within tolerances
- [ ] Helm widget shows alignment indicator when `bDockingAligned == true`
- [ ] Press docking confirm while aligned → `RunPhase == Docking` → `Success` after 2s
- [ ] Press docking confirm while NOT aligned → nothing happens (no phase change)
- [ ] Sub exits dock volume during `Docking` → `RunPhase == Failure`
- [ ] After `Success`, sub forward hatch becomes interactable, crew can interact with it
- [ ] Full docking sequence completable solo in under 2 minutes once in `Approach`

---

## 13. Done Criteria

This spec is closed when all of the following are true:

1. `ASubDockActor` compiles with `DockSocket`, `DockingVolume`, `IsSubmarineAligned()`
2. End dock volume overlap triggers `Approach` phase transition
3. `bDockingAligned` updates correctly on GameState each tick
4. `ServerRouteDockingConfirm()` exists and routes to `AttemptDocking()`
5. `AttemptDocking()` only accepts when `Approach` + `bDockingAligned == true`
6. Dock exit during `Docking` triggers `Failure`
7. Forward hatch interactable fires `BP_OnAirlockTriggered` after `Success`
8. All 4 automation tests pass
9. PIE manual checklist passes
10. `ASubStationBase` and station interface not modified

---

## 14. Post-FP Docking Extensions

These are explicitly deferred:

| Extension | Track |
|---|---|
| Pressure equalization sequence on airlock | Track A |
| Dock registry in campaign (named ports, known docks) | Track B |
| Docking panel as proper `ASubStationBase` subclass | Track C (station tooling) |
| Docking camera / cinematic pass | Track D |
| Crew disembark to underwater station environment | Track B / Post-FP |
