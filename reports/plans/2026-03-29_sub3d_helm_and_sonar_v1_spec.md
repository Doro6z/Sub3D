# Sub3D — Helm & Sonar V1 Spec

**Date**: 2026-03-29
**Status**: Authoritative technical spec — Phase FP-3 entry point
**Parent documents**:
- `C:\ACC\Projects\Sub3D\Plans\sub3d\sub3d_product_north_star.md` (Section 4 — Sonar V1)
- `2026-03-29_sub3d_first_playable_run_spec.md` (P0 prerequisite)

---

## 1. Scope

This spec defines Sonar V1 as a P0 prerequisite for the First Playable.

It covers:
- Verdict on `USonarFieldComponent` (existing stub)
- `USubSonarComponent` — new component on `ASubmarineBase`
- Ping cycle and wave propagation model
- Point cloud data contract + replication
- Input routing through `ASubPlayerController`
- CRT display architecture (world-space widget + lean mode)
- `USubHelmWidget` and `USubSonarDisplayWidget` extension plan
- Source files
- Done criteria

This spec does **not** cover:
- Sonar art polish or phosphor shader (Track D)
- Creature/contact detection by sonar (Track B, post-FP)
- Sonar passive mode
- Macro-world sonar coverage (Track B)

---

## 2. Non-Goals

| Out of scope | Reason |
|---|---|
| Passive sonar mode | Not in V1 spec — active ping only |
| Creature / contact discrimination | Track B, not required for terrain navigation |
| Sonar countermeasures | Post-FP |
| `USonarFieldComponent` voxel query path | See section 3 — not used for V1 sonar |
| Multi-ping overlap or interference | Not required to prove navigation loop |
| Phosphor CRT shader with real scanlines | Track D art pass |
| Audio ping / return sound timing | Track D, stub acceptable for V1 |

---

## 3. SonarFieldComponent Audit and Verdict

### 3.1 What it is

`USonarFieldComponent` lives on `ATraversalRouteActor`.

It stores a `TMap<FFieldChunkCoord, FRouteFieldChunkData>` populated from the route's field model build pipeline.

Its `SampleOcclusionAlongRay()` is a **confirmed stub** — always returns false.

### 3.2 What it was designed for

The component was designed for the macro-world layer:
- route clearance metrics during authoring
- coarse obstacle awareness for route generation passes
- future long-range passive sonar read (Track B)

Its voxel resolution is matched to route-level chunks, not submarine-scale precision navigation.

### 3.3 Verdict: Not used for Sonar V1

`USonarFieldComponent` is **not a dependency of Sonar V1**.

Reason:
- Voxel resolution is route-generation scale, not navigation-precision scale
- `SampleOcclusionAlongRay()` is a stub with no timeline to fill before FP
- Direct world geometry raycasts against the baked route mesh are simpler, more accurate, and immediately correct
- Using the voxel field would require implementing the voxel query path before proving the sonar loop — wrong priority order

### 3.4 Disposition

`USonarFieldComponent` remains on `TraversalRouteActor` unchanged.

It is not deleted, deprecated, or modified by this spec.

Its future is a **Track B decision**:
- It may be repurposed as a long-range low-resolution passive echo source
- It may be replaced by a dedicated macro-awareness component
- It may remain an authoring-only tool

Until Track B is reopened, it is a dormant stub.

---

## 4. Sonar V1 Architecture

### 4.1 Core Design

Sonar V1 is built on:
- **`USubSonarComponent`** — new component on `ASubmarineBase`, owns ping logic and point cloud
- **`USubSonarDisplayWidget`** — new UMG widget, CRT display rendered in world-space on the helm
- **Lean mode** — a full-screen HUD version of the same widget active when pilot looks into the CRT

The sonar does NOT use `USonarFieldComponent`.

It uses **UE5 world raycasts** (`LineTraceSingleByChannel` in batches) against the baked route mesh collision.

### 4.2 What the pilot experiences

1. Pilot presses `[ESPACE]` (Action: `IA_SonarPing`)
2. Sonar component fires a burst of rays from the sub, constrained to a forward hemisphere
3. Hits are returned as a point cloud with travel-time metadata
4. The CRT display on the helm console reveals hit points progressively as the simulated wave would reach them (`hit.DistanceCm / PropagationSpeedCmS`)
5. Points are visible at full intensity for `PointPeakDurationS` (~2s) then fade over `PointFadeDurationS` (~7s)
6. After full fade, points are removed from the display
7. A second ping refreshes any overlapping geometry — fade timer resets on re-ping
8. Geometry behind occlusion is never revealed (ray cannot pass through solid — raycast stops at first hit)

### 4.3 What the sonar does NOT do

Per North Star section 4:
- Does not provide a real-time world view
- Does not reveal anything outside the ray's line of sight
- Does not distinguish creatures from terrain in V1
- Does not persist between runs

---

## 5. USubSonarComponent

### 5.1 Location

New file: `Source/Sub3D/Submarine/SubSonarComponent.h/.cpp`

Attached to `ASubmarineBase` as:
```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
USubSonarComponent* Sonar;
```

### 5.2 Properties

```cpp
// Angular resolution of the ping burst — total rays = HRays * VRays
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
int32 PingRayCountHorizontal = 24;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
int32 PingRayCountVertical = 12;

// Max range of each ray
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PingMaxRangeCm = 15000.f;

// Simulated propagation speed (not physics — controls reveal delay)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PropagationSpeedCmS = 3000.f;

// How long a hit point stays at full intensity before fading
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PointPeakDurationS = 2.f;

// How long the fade takes after peak
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PointFadeDurationS = 7.f;

// Minimum cooldown between pings (prevents spam)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PingCooldownS = 1.5f;

// Forward-hemisphere half-angle (degrees) — 90 = full hemisphere, 60 = narrower cone
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
float PingHalfAngleDeg = 85.f;

// Replicated point cloud
UPROPERTY(ReplicatedUsing = OnRep_SonarPoints, BlueprintReadOnly, Category = "Sonar")
TArray<FSonarHitPoint> SonarPoints;
```

### 5.3 Key Methods

```cpp
// Called by SubPlayerController (server-side)
UFUNCTION(BlueprintCallable, Category = "Sonar")
bool TryFirePing();

// Fires synchronously on server — fills SonarPoints, marks NetDirty
void ExecutePingRaycasts();

// Called on tick (server only) — culls expired points, triggers replication if changed
void TickCullExpiredPoints(float DeltaTime);

// Blueprint event for widget binding
UFUNCTION(BlueprintImplementableEvent, Category = "Sonar")
void OnPingSonarPointsUpdated();

UFUNCTION()
void OnRep_SonarPoints();
```

### 5.4 Tick behavior

`USubSonarComponent` ticks on server only.

Each tick:
1. Decrement `LastPingAge += DeltaTime`
2. Remove from `SonarPoints` any point where `(WorldTime - PingTime) > PeakDuration + FadeDuration`
3. If any point was removed → mark replicated

---

## 6. Sonar Hit Point Data Contract

### 6.1 Struct: FSonarHitPoint

New struct in `Source/Sub3D/Submarine/SubSonarTypes.h`:

```cpp
USTRUCT(BlueprintType)
struct FSonarHitPoint
{
    GENERATED_BODY()

    // World position of the hit
    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize100 WorldLocation = FVector::ZeroVector;

    // Distance from sub origin at time of ping (cm)
    // Used by display to compute reveal delay: RevealDelay = DistanceCm / PropagationSpeedCmS
    UPROPERTY(BlueprintReadOnly)
    float DistanceCm = 0.f;

    // Server world time when ping was fired (seconds)
    UPROPERTY(BlueprintReadOnly)
    float PingTimestamp = 0.f;

    // Surface normal at hit (for future specular/intensity shading — V1: unused by display)
    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize Normal = FVector::UpVector;
};
```

### 6.2 Why distance not timestamp

The widget uses `DistanceCm` to compute a display reveal delay independently of server time:

```
RevealTimeS = PingTimestamp + (DistanceCm / PropagationSpeedCmS)
```

The widget knows `PingTimestamp` (received via `OnRep_SonarPoints`), knows the current client time, and knows the distance. This keeps the display correct even with minor clock drift between server and client.

### 6.3 Replication size

Default parameters: 24 × 12 = 288 rays × (roughly 50–80% hit rate on dense terrain) = ~150–230 points per ping.

Each `FSonarHitPoint` is approximately 24 bytes quantized. At 230 points: ~5.5 KB per ping.

Pings are infrequent (minimum 1.5s cooldown). Acceptable bandwidth for 2-player session.

---

## 7. Input Routing

### 7.1 New action

New Enhanced Input action: `IA_SonarPing`

Mapped to `[ESPACE]` (default) when `CurrentControlMode == HelmDriving`.

**Important**: `[ESPACE]` must not conflict with jump/on-foot interaction. The input mapping context for `HelmDriving` must suppress on-foot actions while active.

### 7.2 New controller method

Add to `ASubPlayerController.h`:

```cpp
UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
void ServerRouteSonarPing();
```

Implementation:
```cpp
void ASubPlayerController::ServerRouteSonarPing_Implementation()
{
    if (CurrentControlMode != ECrewControlMode::HelmDriving) return;

    ASubmarineBase* Sub = ResolveCurrentSubmarine();
    if (!Sub || !Sub->Sonar) return;

    Sub->Sonar->TryFirePing();
}
```

### 7.3 Lean mode input

Lean mode is a **local-only** camera action — not routed to server.

New Enhanced Input action: `IA_SonarLean` (hold to lean, release to return).

`ASubPlayerController` (or its BP) handles camera interpolation to a lean target in local space. No server RPC needed. The lean does not affect `CurrentControlMode`.

---

## 8. CRT Display Architecture

### 8.1 Design intent

From North Star section 4:
> "Terminal CRT bombé, encastré dans le pupitre helm. Phosphore vert. Rémanence, flicker, distorsion de bord. Hologramme wireframe du sous-marin au centre."

### 8.2 V1 rendering approach

V1 uses a **UMG widget rendered to a Render Target**, displayed on the helm console mesh via a dynamic material:

```
USubSonarDisplayWidget (UMG canvas)
        ↓
UTextureRenderTarget2D (sonar_crt_rt, 512×512)
        ↓
MI_SonarCRT (dynamic material, WidgetComponent or Decal mesh on helm)
        ↓
Visible as CRT screen in FPS world-space view
```

**Why render target and not a world-space WidgetComponent directly:**
- A `UWidgetComponent` in world space has no built-in post-process (no CRT curvature, no phosphor glow)
- A render target allows a simple post-process material pass (scan-line, curvature, phosphor color) on the final texture before display
- V1 can skip the post-process material entirely and just display the raw widget render — Track D will add the visual layer

For V1: raw render target without CRT material. The screen just displays the dot canvas.

### 8.3 Lean mode

When `IA_SonarLean` is held:
- Camera interpolates to a fixed local-space lean position (designed in helmet BP)
- A full-screen HUD widget (`USubSonarLeanWidget`) becomes visible — it reads from the same `USubSonarDisplayWidget` logic or the same render target
- `IA_SonarPing` remains active during lean
- Movement inputs (`HelmThrust`, `HelmSteer`, `HelmDive`) remain active during lean — the North Star says "losing access to piloting controls during lean". **This is a design choice that the spec does not mandate for V1** — V1 can allow all inputs during lean; the design constraint can be enforced by the BP when the camera is locked forward

For V1: lean = camera zoom + HUD overlay. Movement input not blocked.

### 8.4 USubSonarDisplayWidget

New widget class: `USubSonarDisplayWidget : UUserWidget`

New file: `Source/Sub3D/Submarine/SubSonarDisplayWidget.h/.cpp`

**Responsibilities:**
- Receives `USubSonarComponent*` reference on init
- Draws the sub wireframe outline at canvas center (static pre-built mesh projection or simple hardcoded silhouette)
- On `OnPingSonarPointsUpdated`: projects each `FSonarHitPoint.WorldLocation` into the local CRT view space (sub-relative, top-down or forward-facing perspective)
- Draws each point as a dot with alpha = `ComputePointAlpha(Point, CurrentTime)`

**Alpha computation:**
```
float RevealTime = Point.PingTimestamp + (Point.DistanceCm / PropagationSpeedCmS);
float Age = CurrentTime - RevealTime;
if (Age < 0) return 0.f;                        // not yet revealed
if (Age < PeakDuration) return 1.f;             // full intensity
float FadeT = (Age - PeakDuration) / FadeDuration;
return FMath::Clamp(1.f - FadeT, 0.f, 1.f);     // linear fade
```

**CRT view projection:**
For V1, the CRT displays a **forward-facing plan view** centered on the submarine:
- X axis (screen horizontal) = submarine local Y (port-starboard)
- Y axis (screen vertical) = submarine local Z (up-down)
- Depth (submarine local X, fore-aft) is encoded as dot brightness or size: nearer = brighter (optional V1 enhancement)

Alternative: true 3D projection onto the screen plane. Both are acceptable V1 implementations. The projection type is a Blueprint-side decision — the spec only mandates that the widget reads from `SonarPoints` and displays reveal-timed dots.

### 8.5 Sub wireframe outline at CRT center

The North Star says "hologramme wireframe du sous-marin au centre."

For V1: a **static pre-drawn silhouette** of the compiled submarine shape, rendered as a thin overlay at canvas center. Not a live mesh projection.

This is sufficient for FP. A live wireframe projected from the compiled hull geometry is a Track D enhancement.

---

## 9. Source Files

### New Files

| File | Content |
|---|---|
| `Source/Sub3D/Submarine/SubSonarTypes.h` | `FSonarHitPoint` struct |
| `Source/Sub3D/Submarine/SubSonarComponent.h/.cpp` | `USubSonarComponent` — ping, raycasts, point cloud, replication |
| `Source/Sub3D/Submarine/SubSonarDisplayWidget.h/.cpp` | `USubSonarDisplayWidget` — CRT canvas, point projection, alpha fade |

### Modified Files

| File | Change |
|---|---|
| `Source/Sub3D/Submarine/SubmarineBase.h/.cpp` | Add `USubSonarComponent* Sonar` component, wire to `GetLifetimeReplicatedProps` |
| `Source/Sub3D/Submarine/SubPlayerController.h/.cpp` | Add `ServerRouteSonarPing()`, `IA_SonarLean` local handler |
| `Source/Sub3D/Submarine/SubHelmWidget.h/.cpp` | Expose reference to `USubSonarDisplayWidget` or delegate ping events |

### Not Modified

| File | Reason |
|---|---|
| `WorldGen/SonarFieldComponent.h/.cpp` | Not used for V1 sonar — left as Track B stub |
| `WorldGen/TraversalRouteActor.h/.cpp` | Not modified — sonar raycasts hit its mesh but don't call it |
| `SubMovementComponent.h/.cpp` | No change |
| `SubmarineCompartmentComponent.h/.cpp` | No change |

---

## 10. Raycast Execution

### 10.1 Batch strategy

Rays are fired synchronously in `ExecutePingRaycasts()` on server.

Total rays: `PingRayCountHorizontal × PingRayCountVertical` (default: 288).

Distributed over the ping half-angle cone using spherical Fibonacci sampling to avoid polar clustering.

Each ray: `LineTraceSingleByChannel(ECC_Visibility)` from sub actor origin, along the computed direction, max distance = `PingMaxRangeCm`.

### 10.2 Trace channel

Trace channel: `ECC_Visibility` (default).

The baked route mesh must have:
- `CollisionEnabled` = Query and Physics
- `ObjectType` = WorldStatic
- `Visibility` channel = Block

This is already correct for standard baked landscape/procedural mesh. No new channel required.

### 10.3 Performance note

288 raycasts per ping on server. At 1.5s minimum cooldown, this is not a frame-rate concern.

If future performance profiling identifies an issue, the first mitigation is async trace batch (`World->AsyncLineTraceByChannel` + completion delegate) — not required for V1.

### 10.4 Occlusion is real

A ray stops at the first hit. Geometry behind an obstacle is never revealed. This is a free byproduct of standard raycast behavior — no special implementation required.

---

## 11. Runtime Contracts

### 11.1 Authority

- `TryFirePing()` is server-only
- `SonarPoints` is replicated from server to all clients via `ReplicatedUsing = OnRep_SonarPoints`
- Point culling (expired points) happens server-side only — clients never mutate `SonarPoints`

### 11.2 Propagation reveal timing

The propagation speed is a **display simulation** — it has no effect on when raycasts fire. All raycasts fire in one frame on ping. The display widget introduces the artificial delay on the client side using `RevealTime = PingTimestamp + DistanceCm / PropagationSpeedCmS`.

This means:
- Gameplay truth (sub position vs terrain) is always correct at ping time
- Display reveal is visual fidelity only — the player cannot gain frame-perfect advantage from the artificial delay

### 11.3 Cross-session replay

`PingTimestamp` uses `GetWorld()->GetTimeSeconds()`. Between runs (Boot transition), `SonarPoints` is cleared server-side. No stale points persist across sessions.

---

## 12. Helm Station Interaction Path

Sonar V1 does not require changes to the existing helm station enter/exit flow.

The existing flow:
1. Player walks to helm BP actor
2. Player interaction → `ServerEnterHelm()` → `CurrentControlMode = HelmDriving`
3. PC begins routing `ServerRouteHelmThrust/Steer/Dive`

Sonar adds:
4. `IA_SonarPing` becomes active when `CurrentControlMode == HelmDriving`
5. Input binding in the `HelmDriving` IMC fires `ServerRouteSonarPing()`

No changes to enter/exit logic.

---

## 13. Test Criteria

### Automation (EditorContext)

- `FSubSonarPingFiresRaycasts`: call `TryFirePing()` on a component with a known static mesh obstacle 5000cm ahead — assert `SonarPoints.Num() > 0` and at least one point has `DistanceCm` within ±500cm of 5000
- `FSubSonarCooldownEnforced`: fire two pings within `PingCooldownS` — assert second ping returns false and `SonarPoints` count does not change
- `FSubSonarPointsCulledAfterLifetime`: fire ping, manually advance `PingTimestamp` by `PeakDurationS + FadeDurationS + 1s` — assert `SonarPoints` is empty after cull tick
- `FSubSonarNoHitBehindOccluder`: place two meshes in line — one closer, one further — assert no point from the further mesh is present in `SonarPoints` (occluder blocks the ray)

### PIE Manual

- [ ] Pilot at helm, presses `[ESPACE]` — point cloud appears on CRT display
- [ ] Points are not visible until simulated wave would have reached them (near points appear before far points)
- [ ] Points fade out after ~9s (peak + fade window)
- [ ] Second ping before full fade refreshes overlapping points without duplication
- [ ] Geometry behind a wall section is not revealed
- [ ] Lean mode (`IA_SonarLean` held) expands CRT view to full-screen
- [ ] Navigation is possible using only sonar — no external view helps more

---

## 14. Done Criteria

This spec is closed when all of the following are true:

1. `USubSonarComponent` compiles and is attached to `ASubmarineBase`
2. `TryFirePing()` fires 288 rays and populates `SonarPoints`
3. `SonarPoints` replicates to clients
4. `USubSonarDisplayWidget` renders points with correct reveal delay and fade
5. Points from geometry behind an occluder are never shown
6. `ServerRouteSonarPing()` exists and is bound to `IA_SonarPing` action
7. Lean mode activates full-screen CRT view
8. All 4 automation tests pass
9. PIE manual checklist passes
10. `USonarFieldComponent` is not modified and not called by any sonar path

---

## 15. SonarFieldComponent Future Note

`USonarFieldComponent` is not deleted. It is a dormant Track B stub.

When Track B is reopened (post-FP), the decision to be made is:
- **Option A (repurpose)**: extend `SonarFieldComponent` to serve as a long-range low-resolution passive echo source for the submarine's passive sonar (ambient terrain awareness, no active ping)
- **Option B (replace)**: implement a proper macro sonar field from route data, separate from the voxel field built for route generation clearance
- **Option C (remove)**: if the macro sonar layer is implemented via runtime raycasts at a higher level, the voxel field becomes redundant

This decision is deferred. The Track B spec will answer it.
