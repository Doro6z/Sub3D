# Helm Cockpit — Full Redesign Proposal

**Status:** proposal, awaiting review by a second Agent and the user.
Nothing here is implemented. The current `UHelmControlPanelWidget` and
its checkbox-heavy layout will be replaced wholesale, not patched.

**Why now:** the player tested the current panel and reported it
unsatisfying — checkboxes for stabilization don't feel like piloting,
the override behavior is unclear, the rudder/plane sliders are
generic, and the layout reads as a config dialog instead of a bridge
console. We're starting clean.

**Scope:** Helm Control surface only. Sonar, status strip,
reconstruction, tactical views are out of scope here (handled in
[2026-04-17_submarine_physics_revision.md §7quater](2026-04-17_submarine_physics_revision.md)).

---

## 1. Design pillars

Five rules drive every decision below.

1. **Modal, not toggle.** Each domain (speed / steering / depth) is in
   exactly ONE mode at any instant. No "AutoSpeed AND Master AND
   Ballast Active" three-flag combinatorial gating. UX is "pick a
   mode", not "configure flags".
2. **Direct manipulation > config dialog.** Sliders and checkboxes are
   replaced by visual instruments the player operates like a real
   bridge surface (rotary yoke, telegraph dial, dive board).
3. **Vector primitives, no Unicode glyphs, no textures.** Every
   instrument is drawn via custom Slate `OnPaint` (lines, arcs, filled
   polygons, text). Crisp at any DPI, no asset dependency, animatable.
4. **State is observable.** Every active mode is colored / highlighted.
   Suspended autopilots dim instead of disappearing. Player always
   knows what the sub is doing without reading.
5. **One kill switch.** A single, large red strip at the bottom
   disables every autopilot in one press. No more "Master Enabled"
   that the player has to remember to turn on.

---

## 2. Layout overview

Vertical column of 4 sections. ~480 px wide, ~860 px tall (1080p
docked-right pattern). Each section is its own custom widget.

```
┌─ THROTTLE TELEGRAPH ───────────────────────┐
│  large dial / radial chooser, 7 presets    │
│  ────────  spool meter  ─────────          │
│  [DIRECT]  [HOLD CRUISE]  [STOP]           │
└────────────────────────────────────────────┘

┌─ STEERING YOKE ────────────────────────────┐
│       rotary wheel (-30°..+30° rudder)     │
│       HDG 045°    YAW RATE +5°/s           │
│  [DIRECT]  [HOLD RUDDER]  [AUTO HEADING]   │
└────────────────────────────────────────────┘

┌─ DIVE BOARD ───────────────────────────────┐
│  pitch ribbon (-30°..+30°)                 │
│  ballast tanks: vertical pipes w/ levels   │
│  [DIRECT]  [HOLD VERT]  [SURFACE]  [DIVE]  │
└────────────────────────────────────────────┘

┌─ KILL SWITCH ──────────────────────────────┐
│         [⏻  KILL ALL AUTOPILOTS]           │
└────────────────────────────────────────────┘
```

Bonus mini-strip at top with SHIP/CONTROL/ALARM (already exists as
`HelmStatusStripWidget` — keep, just move above the cockpit).

---

## 3. Throttle Telegraph (custom widget)

### Behavior

The telegraph is a **modal speed selector**. Three modes:

| Mode | Player action | What happens |
|---|---|---|
| **DIRECT** | Click a preset (FULL REV…FULL AHEAD) | `SetTargetSpeedCmS(preset)` + `SetAutoSpeedEnabled(true)`. Sub homes onto preset speed. Highlight the preset. |
| **HOLD CRUISE** | One press | Captures `CurrentForwardSpeedCmS` as target + AutoSpeed on. Telegraph dial points to the captured value (between presets). |
| **STOP** | One press | `SetTargetSpeedCmS(0)` + AutoSpeed on. Telegraph snaps to STOP preset. |

There is no "manual" mode in the new design — the slider goes away
entirely. Z/S keys still ramp the target value (existing
`ServerRouteHelmThrottleRamp` behavior); ramping while in DIRECT
preset mode shifts to HOLD CRUISE automatically (the captured value
becomes the new target).

Slamming Z or S to the floor → ramp to ±100% as today. No need for an
explicit MANUAL mode button.

### Visual

A semicircular dial (180°), like a real ship telegraph:

```
              ╭─────────╮
       FULL ╱             ╲ FULL
       REV ╱     ╭─────╮    ╲ AHEAD
          │     ╱   ●   ╲    │  ← dial sectors,
          │    │  needle │   │     active sector lit
       SLOW╲   ╲         ╱   ╱ STD
       REV  ╲   ╲───────╱   ╱
             ╲   STOP       ╱  HALF
              ╰─────────────╯
                  SLOW
```

- 7 colored sectors (red→green spectrum, same as current)
- A NEEDLE drawn over the sectors pointing to the **current sub
  speed** (not the target). So the needle moves toward the lit sector.
- Active sector flashes / glows when target ≠ current speed
  (transitioning).
- Click any sector to engage that preset (DIRECT mode).
- "HOLD CRUISE" button below: captures current needle position.
- "STOP" button: snaps target to 0.

### Sub-widget under the dial

Linear "spool meter" showing the **engine spool state** (existing
`SpooledPower` value, replicated). Two thin bars:
- Top: commanded throttle (HelmThrottleCmd, blue)
- Bottom: actually-applied power (SpooledPower, white)
- Gap between them = engine lag, visible feedback for "you can't
  brake instantly".

### Custom widget class

`UHelmThrottleTelegraphWidget : public UWidget`
- `NativePaint` draws sectors + needle + spool meter (vector)
- `NativeOnMouseButtonDown` hit-tests the dial sector under cursor
- Two sub-buttons (UButton) for HOLD CRUISE / STOP
- Reads from `FHelmControlPanelData` for current speed + target
- Writes via `RouteSetTargetSpeedCmS` + `RouteSetAutoSpeedEnabled`

---

## 4. Steering Yoke (custom widget)

### Behavior

Three modes:

| Mode | Player action | What happens |
|---|---|---|
| **DIRECT** | Drag yoke, A/D keys, gamepad stick | Direct rudder command via existing ramp RPC. Yoke springs back to center on release (current `RudderReturnRate`). |
| **HOLD RUDDER** | Click button | `bRudderHoldEnabled = true`. Yoke stays at last commanded value. |
| **AUTO HEADING** | Click button → set target heading via dial | (Future, not in FP scope) AutoYaw drives rudder to reach target. |

For FP: implement DIRECT and HOLD only. AUTO HEADING button shows but
is disabled with "FP+" tooltip.

### Visual

A **rotary wheel** that visually rotates with the rudder command:

```
        ●  ←  pointer (current rudder)
      ╱   ╲
     │     │
     │  ⊕  │   ← center spindle
     │     │
      ╲   ╱
        ●  ←  pointer reverse
```

- Wheel can rotate up to ±90° visual to represent ±100% rudder
  (exaggerated for readability).
- Tick marks at ±10°, ±25°, ±50%, full lock.
- A SECONDARY ARC traces the **actual yaw rate** of the sub
  (`YawRateDegPerSec`), so the player sees rudder authority vs
  effect (helps tune speed-rudder coupling intuitively).
- Clickable: drag horizontally to set rudder. LMB held + drag.
- Mouse over wheel without dragging: shows preview tick at that angle.

### Below the yoke

Two text rows + 3 buttons:
- "HDG 045°" (current heading)
- "YAW +5°/s" (current yaw rate, color-coded: green steady, orange high)
- Buttons: `[DIRECT]  [HOLD RUDDER]  [AUTO HEADING (FP+)]`

### Why a wheel and not a slider

- Sliders are linear, rudders are rotary in real subs. Visual mismatch.
- A wheel reads as "input device", a slider reads as "config setting".
- Wheel allows visual feedback (rotation) that a slider can't match.
- Drag interaction (LMB-drag arc) feels like pilot input.

### Custom widget class

`UHelmRudderYokeWidget : public UWidget`
- `NativePaint` draws wheel, ticks, pointer, secondary yaw-rate arc
- `NativeOnMouseButtonDown/Move/Up` for drag interaction
- 3 sub-buttons for mode selection
- Writes via existing `ServerRouteHelmRudderRamp` (drag-to-target
  becomes a continuous Intent stream)

---

## 5. Dive Board (custom widget)

The most novel section. Combines pitch + ballast in one cockpit
instrument because they share the same gameplay axis (vertical
control).

### Behavior

Four modes:

| Mode | Player action | What happens |
|---|---|---|
| **DIRECT** | Drag pitch ribbon (W/X keys), drag ballast bars | Manual hydroplane + manual ballast tanks. |
| **HOLD VERT** | Click | `AutoDepth` ON (zero vertical velocity). Ballast slider visibly self-trims. |
| **SURFACE** | Click | Drains ballast to 0% (rapid rise). Toggles AutoDepth off. |
| **DIVE** | Click + slider for target | Fills ballast to 100% OR Auto-depth target = current+10m (FP scope: just fill ballast). |

### Visual

**Pitch ribbon** (top) — horizontal strip with current pitch indicated:

```
┌────────────────────────────────────────┐
│  -30  -20  -10   0   +10  +20  +30     │
│   ║    ║    ║    ║    ║    ║    ║      │
│ ──────────────────●──────────────────  │  ← current pitch
│                   ↑                    │
│              commanded pitch ▼          │
└────────────────────────────────────────┘
```

- Tick marks every 10°
- BIG dot = current sub pitch
- Smaller arrow = commanded hydroplane input (target via ramp)
- Click anywhere on ribbon → instant hydroplane command at that angle
- Drag → ramp toward that angle (uses existing
  `ServerRouteHelmDivePlaneRamp`)

**Ballast tanks** (bottom) — vertical pipes with water level fill:

```
   Tank 1        Tank 2
   ┌────┐       ┌────┐
   │░░░░│       │░░░░│   ← target (semi-transparent)
   │▆▆▆▆│       │▆▆▆▆│   ← current fill (animated)
   │▆▆▆▆│       │▆▆▆▆│
   │    │       │    │
   └────┘       └────┘
   60%          60%
```

- Pipe outline + water column (colored by content level)
- Target fill drawn as ghost water level (so player sees AutoDepth
  acting in real time)
- Click + drag inside a pipe → manual fill target
- Numeric % below
- Tank state badge: pump health (Nominal=green, Degraded=orange,
  Dead=red)

### Buttons

`[DIRECT]  [HOLD VERT]  [SURFACE]  [DIVE]`

DIRECT means "no autopilot, manual ballast + manual planes".
HOLD VERT triggers velocity-target Auto Depth (existing).
SURFACE drains all tanks to 0% fill, AutoDepth off.
DIVE fills all tanks to 100%, AutoDepth off.

### Custom widget class

`UHelmDiveBoardWidget : public UWidget`
- `NativePaint` draws pitch ribbon + ballast pipes
- Hit-test ribbon → ramp dive plane
- Hit-test ballast pipes → set tank target
- 4 sub-buttons for modes

---

## 6. Kill Switch (custom widget)

Single big red bar at the bottom. One button.

```
┌──────────────────────────────────────┐
│  [⏻  KILL ALL AUTOPILOTS  ]          │
└──────────────────────────────────────┘
```

When pressed:
- `bStabilizationMasterEnabled = false`
- All autopilots stop driving controls (existing behavior, but now
  the player has a clear single action)
- Manual sliders / yoke / dive board are unaffected; the sub keeps
  whatever HelmThrottleCmd / HelmYawCmd / HelmTrimCmd / ballast
  target was last set
- Visual: button latches "DEAD" red until pressed again to reset
  (re-enables master)

Why not multiple kill modes (just speed, just depth, etc): emergency
kill is binary. The mode buttons in each section already let the
player drop OUT of an autopilot per axis.

### Custom widget class

`UHelmKillSwitchWidget : public UButton` (or a styled UWidget)
- `NativePaint` for the diagonal red stripes / state
- One click handler

---

## 7. State machine — replacing checkbox flags

### Old flags (gone)

- `bStabilizationMasterEnabled` — kept on backend (flipped to true by
  default already), exposed only as the kill switch state
- `bAutoSpeedEnabled` — backend, set by mode buttons, never directly
  toggled by user
- `bAutoDepthEnabled` — same
- `bAutoPitchEnabled` — same (FP+ for the AUTO PITCH button)
- `bRudderHoldEnabled` / `bPlaneHoldEnabled` — same
- `bBallastsActive` — keep but always true; remove from UI
- `bPumpActive` / `PumpPower01` — keep, surface as a tank-pumps row
  inside the dive board (or hide entirely for FP if not actively
  used)

### New per-axis modes (UI state)

```cpp
enum class EHelmSpeedMode : uint8  { Direct, HoldCruise, Stop };
enum class EHelmSteerMode : uint8  { Direct, HoldRudder, AutoHeading };
enum class EHelmDiveMode  : uint8  { Direct, HoldVert, Surface, DiveCommand };
```

These three enums live on the **client widget state** (transient, no
replication). The backend CommandState flags are derived:

| Speed mode | bAutoSpeedEnabled | TargetSpeedCmS |
|---|---|---|
| Direct (preset) | true | preset * MaxFwd |
| HoldCruise | true | captured value at toggle |
| Stop | true | 0 |

| Steer mode | bRudderHoldEnabled | rudder ramp |
|---|---|---|
| Direct | false | streamed from input |
| HoldRudder | true | streamed but doesn't recenter |
| AutoHeading (FP+) | true | server controller drives |

| Dive mode | bAutoDepthEnabled | ballast targets |
|---|---|---|
| Direct | false | manual |
| HoldVert | true | controller drives |
| Surface | false | force 0% on all tanks |
| DiveCommand | false | force 100% on all tanks |

So the backend flags are just an output of mode selection. The user
never sees the flags directly.

---

## 8. Visual technique — "vector cockpit"

No textures, no SVG (UE doesn't render SVG natively without a
plugin). All instruments rendered via custom `NativePaint` using
existing Slate primitives:

- **`FSlateDrawElement::MakeLines`** — ticks, pointers, ribbons, ring
  outlines, secondary arcs. The bread and butter.
- **`FSlateDrawElement::MakeBox`** — sectors (using
  `FSlateColorBrush(FLinearColor)` for solid fills), ballast water
  columns, button backgrounds.
- **`FSlateDrawElement::MakeText`** — numeric readouts.
- **`FSlateDrawElement::MakeRotatedBox`** — yoke wheel rotation
  (single image of wheel rotated by rudder angle).
- **Polygon arcs** — for the telegraph dial sectors, sample N points
  on a circle and use `MakeLines` with a closed path; fill with
  `MakeBox` clipped via stencil if needed (or use `MakeShapedText` /
  custom `FSlateBoxBrush` with rotated tessellation).

For animation (pulse, glow, smooth needle motion):
- Cache the "rendered" state (current needle angle, current pulse
  alpha) and update each tick with `FInterpTo` toward target.
- Pulse via `FApp::GetCurrentTime()` * frequency * sin().

For DPI: all positions / sizes scale with `AllottedGeometry.GetLocalSize()`.
At 4K the cockpit just looks bigger; no rasterization issues.

For polish (later): add a `UMaterialInstanceDynamic` background per
widget and animate via material parameters (CRT scanlines, glow
edge, etc.). Material lives on a `UImage` underlay; vector overlay
on top.

---

## 9. Code architecture — single pattern, no ambiguity

The earlier draft waffled between "custom `UWidget` + parent-owned
buttons" and "composite `UUserWidget` per section". Pick one. Going
with **composite UUserWidget per instrument**, with a nested custom
paint widget inside each.

Rationale:
- `UUserWidget` gives us `UPROPERTY(BindWidget)` for sub-buttons and
  text readouts placed in the designer, which beats hand-building
  buttons in C++.
- The actual vector drawing (dial, yoke wheel, pitch ribbon, tanks)
  lives in a child `UWidget` subclass (paint-only) that the user
  widget places as one of its children via `WidgetTree` construction.
- Event handling (button clicks, drag on the paint area) stays on the
  `UUserWidget`. The paint widget exposes hit-testing through
  overrides like `NativeOnMouseButtonDown`; the user widget either
  owns that directly or routes to its own handlers.

### 9.1 File layout

`Source/Sub3D/Submarine/Helm/`:

```
Helm/
├── HelmCockpitWidget.h/.cpp            ← UUserWidget, top-level container. Vertical box of 4 section widgets.
├── HelmCockpitState.h                  ← shared enums (EHelmSpeedMode etc.)
├── HelmThrottleTelegraphWidget.h/.cpp  ← §3, UUserWidget composite
├── HelmRudderYokeWidget.h/.cpp         ← §4, UUserWidget composite
├── HelmDiveBoardWidget.h/.cpp          ← §5, UUserWidget composite
├── HelmKillSwitchWidget.h/.cpp         ← §6, UUserWidget composite
└── Paint/                              ← pure vector paint widgets (UWidget subclasses)
    ├── HelmThrottleDialPaint.h/.cpp    ← draws the semicircular dial + needle
    ├── HelmRudderYokePaint.h/.cpp      ← draws the rotary wheel
    ├── HelmPitchRibbonPaint.h/.cpp     ← draws the pitch ribbon
    └── HelmBallastTankPaint.h/.cpp     ← draws one vertical pipe
```

### 9.2 Shared contract

All 4 instrument widgets (the composites) inherit a small base
interface so the cockpit container can iterate them:

```cpp
UCLASS(Abstract)
class UHelmInstrumentWidget : public UUserWidget
{
public:
    UFUNCTION(BlueprintCallable, Category = "Helm")
    virtual void InitForCockpit(UHelmCockpitWidget* InCockpit) { CachedCockpit = InCockpit; }

protected:
    virtual void NativeTick(const FGeometry&, float) override;  // polls GetControlPanelData
    virtual void RefreshFromData(const FHelmControlPanelData&) {};  // each instrument overrides

    UPROPERTY(Transient)
    TWeakObjectPtr<UHelmCockpitWidget> CachedCockpit;
};
```

The cockpit container caches a `TWeakObjectPtr<USubHelmWidget>` and
exposes it via `GetHelmShell()` so each instrument can route commands
without owning its own shell pointer.

### 9.3 Interaction responsibility

- **Click / drag on the paint area** (dial sector, yoke wheel, pitch
  ribbon, ballast pipe) → handled by the owning `UUserWidget` via
  `NativeOnMouseButtonDown` / `NativeOnMouseMove` / `NativeOnMouseButtonUp`
  overrides on the UUserWidget itself, with hit-test math delegated
  to a small helper on the paint widget (e.g., `FVector2D LocalToDialAngle`).
- **Mode buttons** (DIRECT / HOLD / STOP / etc.) are real UMG
  `UButton` children of the composite, bound via `BindWidget`. Their
  click handlers are UFUNCTIONs on the composite. No cross-widget
  wiring.
- **State changes** (mode switch, preset highlight) flow one-way:
  composite reads `FHelmControlPanelData` in Tick → writes internal
  display state on the paint widget via setters → paint widget
  invalidates.

One pattern, everywhere. No parent-knows-about-children-buttons
back-channel.

---

## 10. Backend changes required

**Not zero. The earlier draft of this doc oversold "100% UI". Review
found the following must-do backend work before the cockpit can be
wired.**

### 10.1 Existing and usable as-is

- `RouteSetTargetSpeedCmS` ✓ ([SubHelmWidget.h:347](../../Source/Sub3D/Submarine/SubHelmWidget.h))
- `RouteSetAutoSpeedEnabled` ✓
- `RouteSetAutoDepthEnabled` ✓
- `RouteSetStabilizationMasterEnabled` ✓ (kill switch)
- `RouteSetRudderHoldEnabled` / `RouteSetPlaneHoldEnabled` ✓
- `RouteSetGlobalBallast` ✓ (named this way, not `RouteSetGlobalBallastTarget`)
- `ServerRouteHelmRudderRamp` / `ServerRouteHelmDivePlaneRamp` ✓
  (added 2026-04-17)
- `FHelmControlPanelData.CurrentForwardSpeedCmS` ✓
- `FHelmControlPanelData.CurrentPitchDeg` ✓
- `FHelmControlPanelData.MaxForwardSpeedCmS` ✓ (added 2026-04-17)
- `FHelmControlPanelData.EffectivePowerInput` ✓ (usable as spool proxy)
- `FHelmControlPanelData.BallastTanks` ✓ (per-tank fill + target + pump state)

### 10.2 Missing — must add before implementation

**RPC wrappers on the shell** (`USubHelmWidget.h/.cpp`):

```cpp
// Per-tank ballast target for the DIVE / SURFACE / per-pipe drag.
UFUNCTION(BlueprintCallable, Category = "Helm|Ballast")
void RouteSetBallastByIndex(int32 TankIndex, float Target01);
```

Underlying RPC already exists on `ASubPlayerController`
(`ServerRouteBallastByIndex`, [SubPlayerController.h:148](../../Source/Sub3D/Submarine/SubPlayerController.h)).
Just the shell wrapper is missing. ~10 lines.

**Data struct fields** on `FHelmControlPanelData`
([SubHelmWidget.h:131](../../Source/Sub3D/Submarine/SubHelmWidget.h)):

```cpp
UPROPERTY(BlueprintReadOnly, Category = "Helm")
float CurrentYawRateDegPerSec = 0.f;     // for yoke secondary arc

UPROPERTY(BlueprintReadOnly, Category = "Helm")
float CurrentHeadingDeg = 0.f;           // for yoke "HDG 045°" readout

UPROPERTY(BlueprintReadOnly, Category = "Helm")
float CurrentSpooledPower = 0.f;         // for throttle spool meter
```

Populated in `USubHelmWidget::GetControlPanelData`. `YawRateDegPerSec`
comes from `Movement->GetYawRateDegPerSec()` (already exists,
[SubMovementComponent.h:292](../../Source/Sub3D/Submarine/SubMovementComponent.h)).
`Heading` from `Sub->GetActorRotation().Yaw` normalised. Spooled power
needs a getter added on `USubMovementComponent` (currently the
`SpooledPower` member is private, no accessor). ~5 lines on movement
component, ~10 on the shell.

### 10.3 Future (FP+, still not blocking)

- `ServerRouteTargetHeadingDeg` + AutoYaw controller — for AUTO HEADING
  button
- `ServerRouteTargetDepthMeters` actual DIVE TO command — the RPC
  exists but no controller drives ballast toward a target depth

### 10.4 Scope revision

So the redesign is **~30 lines of backend scaffolding** before the
cockpit widgets can be written, not zero. Do this in the same commit
as the first instrument (throttle telegraph) so the data flow is
proven before building the rest.

---

## 11. Migration / removal list

When implementing, **delete entirely** (no legacy):

- `UHelmControlPanelWidget.h/.cpp` — wholesale replacement
- All checkbox handlers (`HandleAutoSpeedChanged`,
  `HandleAutoDepthChanged`, `HandleAutoPitchChanged`,
  `HandleStabilizationMasterChanged`,
  `HandleBallastsActiveChanged`, `HandlePumpActiveChanged`,
  `HandleRudderHoldChanged`, `HandlePlaneHoldChanged`)
- The procedural `AddToggleRow` / `AddAxisSliderRow` helpers
- The `HelmYaw / HelmTrim / Pump / Ballast` slider sections
- The `bAutoDepthBallastGain` deprecated property (already deprecated,
  remove on the same pass)

Keep:
- `FHelmControlPanelData` (the data struct) — but extend with the
  three fields listed in §10.2
- All Server RPCs on `ASubPlayerController`
- The new `MaxForwardSpeedCmS` field added today

### 11.1 USubHelmWidget changes — not just TryBindControlStackPanel

The earlier draft said "only `TryBindControlStackPanel` needs to bind
the new widget". Review found the shell is typed against
`UHelmControlPanelWidget*` in several places; each has to change to
`UHelmCockpitWidget*` (or the new cockpit class name):

| Site | Current | What changes |
|---|---|---|
| [SubHelmWidget.h:388](../../Source/Sub3D/Submarine/SubHelmWidget.h) | `UPROPERTY(BindWidgetOptional) TObjectPtr<UHelmControlPanelWidget> ControlStackPanel;` | retype to new class |
| [SubHelmWidget.h:420](../../Source/Sub3D/Submarine/SubHelmWidget.h) | `TSubclassOf<UHelmControlPanelWidget> AutoCreatedControlStackPanelClass` | retype + default class asset |
| [SubHelmWidget.cpp:185](../../Source/Sub3D/Submarine/SubHelmWidget.cpp) | `DiscoverWidgetReferencesFromTree` cast chain | swap cast target |
| [SubHelmWidget.cpp:1085](../../Source/Sub3D/Submarine/SubHelmWidget.cpp) | `TryBindControlStackPanel` — constructs / resolves | swap construction + cast |
| [SubHelmWidget.cpp:1101](../../Source/Sub3D/Submarine/SubHelmWidget.cpp) | `InitForHelmShell` binding | swap call target |

Plan: rename the cockpit class to `UHelmCockpitWidget`, do a
find-and-replace of the old type name in the shell + header +
BindWidget, and update the default class asset set in BP
(`WBP_SubHelm`) to reference the new cockpit widget. ~20 lines
touched in the shell + one BP asset rewire.

### 11.2 Blueprint asset work

- `WBP_SubHelm` needs its `ControlStackPanel` reference retyped /
  reassigned to the new cockpit widget class (or the child
  WBP_HelmCockpit if we author it in BP rather than pure C++)
- `WBP_HelmControlPanel` is deleted (asset + BP graph)

---

## 12. Rollout steps (when implementing)

1. **Create the 5 new files** in `Helm/` folder, empty stubs.
2. **Wire `WBP_SubHelm`** to instantiate `UHelmCockpitWidget` as the
   ControlStackPanel content (in `USubHelmWidget::TryBindControlStackPanel`).
3. **Implement throttle telegraph** first (most isolated, validates
   the vector-paint approach end-to-end).
4. **Implement steering yoke** (validates drag interaction).
5. **Implement dive board** (validates compound instrument).
6. **Implement kill switch** (smallest, last).
7. **Delete `UHelmControlPanelWidget`** in the same commit, no
   coexistence period.
8. **PIE test** with the audit checklist (telegraph, yoke drag, dive
   modes, kill switch, persistence after pilot exit).
9. **Polish pass**: pulse animations, color tuning, tooltips on the
   FP+ buttons.

Estimated time: **2-3 days of focused work** for the C++ widgets,
plus 1 day of PIE iteration on feel. UMG-only since all the backend
exists.

---

## 13. Open questions for review

1. **Is the dial telegraph ergonomic enough vs the row of buttons?**
   The dial is more cinematic but requires hit-testing arc sectors.
   The row is simpler. Could also offer both (dial primary, row
   compact mode).

2. **Should the rudder yoke be horizontal (drag left/right) or
   rotational (drag in arc)?** Rotational is closer to a real wheel
   but harder to grok with a mouse. Horizontal might be more
   forgiving.

3. **Should DIVE/SURFACE be instant or ramped?** Instant fills are
   physically violent (a sub doesn't dump 60% ballast in 0.1s).
   Could ramp the target over 2-3s. Visual: ghost ballast level
   marches toward the new target.

4. **Should the kill switch latch (require explicit re-enable) or
   pulse (one-shot then auto-resume after N seconds)?** Latching is
   safer (player explicitly resumes); pulsing is more forgiving in
   panic.

5. **How much SHOULD AUTO HEADING be in the FP scope?** It's ONE new
   server RPC + a yaw P-controller, mirror of AutoSpeed. ~half a day
   of work. Decide if FP demo includes this.

6. **Pump controls**: do we even surface them in the cockpit? Today
   they're a slider + toggle. The new design hides them. If the FP
   gameplay requires explicit pump commands (mid-flood firefighting),
   they need a home — probably as a small icon in the dive board
   that expands to a per-tank pump control on click.

7. **Do we want gauges on the cockpit or rely on the StatusStrip
   above?** Currently the StatusStrip shows speed/depth/pitch/HDG.
   Could be redundant with cockpit instruments. Decide what lives
   where.

8. **Visual style: tactical green CRT vs warm amber bridge vs cold
   slate?** Sets the whole tone. Recommend: tactical green CRT
   (matches sonar) for the helm, amber for damage control panels
   (future).

---

## 14. Out of scope for this proposal

- Sonar / radar widget changes (separate doc)
- Tactical / Reconstruction view changes (separate doc)
- Status strip changes (already done in 2026-04-17)
- Damage control panel (post-FP)
- Crew assignments / station occupancy UI (post-FP)
- VR / gamepad-specific input (UI is mouse + keyboard for FP)

---

## 15. Review corrections (2026-04-18)

Documenting findings from the second-agent review so this doc stays
faithful to the code.

1. **Backend gaps corrected** in §10.2. The redesign needs a
   `RouteSetBallastByIndex` wrapper on the shell plus three new
   fields on `FHelmControlPanelData` (`CurrentYawRateDegPerSec`,
   `CurrentHeadingDeg`, `CurrentSpooledPower`) plus a public
   accessor on `USubMovementComponent` for `SpooledPower`. Earlier
   draft said "backend = none"; review found this was wrong.

2. **Migration scope expanded** in §11.1. Shell has 5 call sites
   typed against the old panel class (`ControlStackPanel` property,
   `AutoCreatedControlStackPanelClass`, discovery tree,
   `TryBindControlStackPanel`, `InitForHelmShell` call). All must
   be retyped to the new cockpit widget. Earlier draft pointed at
   only one.

3. **Architecture disambiguated** in §9. Earlier draft oscillated
   between `UWidget` paint-only instruments with parent-owned
   buttons and `UUserWidget` composites. Locked in on composites:
   each instrument is a `UUserWidget` that internally hosts a
   `UWidget` paint child for the vector drawing, plus `UButton`
   sub-widgets for modes. Handlers live on the composite only.

4. **Sonar silhouette scale** fix applied (out-of-doc): the
   silhouette draw now multiplies mesh extent by
   `ShapeSource->GetComponentScale()` so non-unit HullMesh /
   MovementCollisionProxy scales render at true size. Review
   noted the original pass read `GetStaticMesh()->GetBoundingBox()`
   without scale.

5. **Crew yaw recap in memory was stale**. The auto-memory entry
   that said `bIgnoreBaseRotation = true` as a permanent change
   overstates what the code does today. Current behaviour scopes
   the flag to the embarked state only, via
   [SubCrewMovementComponent.cpp:42](../../Source/Sub3D/Submarine/SubCrewMovementComponent.cpp),
   and the constructor still leaves it off. Memory file updated
   in the same pass.
