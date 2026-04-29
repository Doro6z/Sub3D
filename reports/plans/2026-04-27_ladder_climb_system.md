# Sub3D — Ladder Climb System (FP scope)

**Date**: 2026-04-27
**Status**: PROPOSED, awaiting approval
**Authority-max reference**: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

---

## 1. Purpose

Add vertical traversal between sub decks via climbable ladders, using the existing
`UInteractableComponent` pattern. Reuses the LGA architecture: ladders are children of
the sub, crew rebases each tick, ladder climb is a state inside the embarked locomotion.

---

## 2. Hard constraints

1. LGA architecture preserved — ladders are sub children, crew rebases against sub
   every tick. Climb is an embarked sub-state, NOT EVA.
2. No new replication contract — climb state replicates via existing crew movement
   stream (`GridSpaceTransform` already covers pose).
3. `UCharacterMovementComponent` not subclassed for a custom mode — climb is a layer
   on top of the existing rebase that overrides the input-to-pose mapping.
4. Single Actor `ABP_Ladder` template, hand-authored mesh + 2 interactables.
5. Authority on server — ServerRPC gates entry/exit; predicted on owning client for
   responsiveness.

---

## 3. Architecture

```
ABP_Ladder (Actor in BP_Submarine_Craniata hierarchy)
├── StaticMeshComponent (the visual ladder mesh)
├── ULadderClimbComponent (NEW)
│   ├── ClimbStartLocal (sub-local FVector — bottom of ladder)
│   ├── ClimbEndLocal (sub-local FVector — top of ladder)
│   ├── ClimbDirection = (End - Start).Normalize()
│   ├── ClimbLengthCm = (End - Start).Size()
│   ├── ServerRequestEnterClimb(ASubCrewCharacter* Crew, bool bFromBottom)
│   └── ServerRequestExitClimb(ASubCrewCharacter* Crew)
├── BottomInteractable (UInteractableComponent at ClimbStart)
│   └── OnInteract → ULadderClimbComponent::OnBottomInteract
└── TopInteractable (UInteractableComponent at ClimbEnd)
    └── OnInteract → ULadderClimbComponent::OnTopInteract
```

```
ASubCrewCharacter
├── ECrewClimbState { NotClimbing, ClimbingUp, ClimbingDown } — local + replicated
├── ULadderClimbComponent* CurrentLadder (weak ref)
├── float ClimbProgress01 (0 = bottom, 1 = top)
└── float ClimbSpeedPerSec = 0.5 (FP default = 2 sec to climb full ladder)

USubCrewMovementComponent::TickComponent extension:
  if (Crew->IsClimbing && Crew->CurrentLadder)
  {
    // Drive ClimbProgress01 from forward input axis (W = up, S = down).
    // Compute target GridSpaceTransform.Location = ladder bottom + direction * (progress * length)
    // Override the rebase target with this computed pose.
    // Skip CMC sim (or set MOVE_None) to disable horizontal walking.
    // Exit when progress hits 0 or 1, or interact pressed again.
  }
```

---

## 4. State transitions

### Enter climb (from bottom)
1. Crew walks to bottom interactable.
2. Player presses interact.
3. `USubInteractionComponent::ServerTryPrimaryInteract` → `BottomInteractable.TriggerInteract(Crew)`.
4. `OnInteract` BP delegate calls `ULadderClimbComponent::ServerRequestEnterClimb(Crew, bFromBottom=true)`.
5. Server validates crew is locally embarked + within range, sets `Crew->CurrentLadder`, `Crew->ClimbState = ClimbingUp`, `Crew->ClimbProgress01 = 0.f`.
6. Replicates state to clients via existing crew rep.

### During climb
- Forward input axis (W/S) drives `ClimbProgress01 += ClimbSpeedPerSec * DeltaTime * (W=+1, S=-1)`.
- Owning client predicts; server authoritative; ServerMove sends ClimbProgress01 (extend FCharacterNetworkMoveData_SubCrew).
- Crew capsule pose = `Sub->GetActorTransform().TransformPosition(ClimbStartLocal + ClimbDirection * (Progress * ClimbLengthCm))`.
- Crew yaw = facing the ladder (perpendicular to ClimbDirection projected on horizontal).
- CMC mode forced to MOVE_Flying (no gravity, no walk) during climb.

### Exit climb
- **Reach top** (`Progress >= 1.0`): server snaps crew to ClimbEndLocal + step-off offset (e.g. +50cm forward), sets `ClimbState = NotClimbing`, restores MOVE_Walking.
- **Reach bottom** (`Progress <= 0.0`): same with ClimbStartLocal.
- **Interact pressed again**: same as reach top.
- **EVA boundary cross**: defensive, force exit + clear state.

---

## 5. Files to create / modify

**New**:
- `Source/Sub3D/Submarine/LadderClimbComponent.h`
- `Source/Sub3D/Submarine/LadderClimbComponent.cpp`

**Modified**:
- `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` — add `ECrewClimbState`, `CurrentLadder`, `ClimbProgress01`, replicate.
- `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp` — TickComponent fork for climb mode (override rebase target, freeze CMC).
- `Source/Sub3D/Submarine/SubCrewNetTypes.h/.cpp` — add `ClimbProgress01` to `FCharacterNetworkMoveData_SubCrew` (1 float, NetQuantize).

**Asset (user authoring after code lands)**:
- `Content/Sub3D/Crew/BP_Ladder.uasset` — Actor BP with StaticMesh + LadderClimbComponent + 2 Interactables wired via OnInteract.

---

## 6. Out of scope (post-FP)

- Climb animation graph (anim BP listens to `bIsClimbing` + `ClimbProgress01`, climb anim cycle authored in editor).
- Multiple crew on same ladder (FP: one at a time, server gate rejects if occupied).
- Sliding down (FP: only stepped climb).
- Asymmetric ladders (curved, multi-segment) — FP ladders are single-segment straight.

---

## 7. Acceptance

1. Crew walks to bottom interactable, interact, ascends to top of ladder smoothly.
2. Crew at top, interact, descends to bottom.
3. While climbing, no horizontal walk input (gravity off, capsule glued to ladder line).
4. Exit at top steps crew off onto upper deck.
5. Exit at bottom steps crew off onto lower deck.
6. Peer J2 sees J1 climbing (replicated state + pose).
7. EVA boundary cross during climb forces exit defensively.
8. None of the 5 stable scenarios regresses.

---

## 8. Risk

- CMC freeze during climb: setting `MOVE_Flying` + zero velocity + manual pose override may
  fight CMC base handling. Mitigation: same pattern as LGA rebase
  (`UpdatedComponent->SetWorldLocationAndRotation(..., bSweep=false, TeleportPhysics)`).
- Anim BP isn't authored yet: visual will be the static mesh sliding up the ladder
  without arm/leg animation. Acceptable for FP, anim authoring is post-FP work.
