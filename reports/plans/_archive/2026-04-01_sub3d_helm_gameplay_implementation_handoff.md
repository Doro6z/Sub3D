# Sub3D - Helm Gameplay Implementation Handoff

Date: 2026-04-01
Scope: Consolidated runtime + editor handoff after helm data contract exposure in `USubHelmWidget`
Status: Build complete succeeded for `Sub3DEditor Win64 Development`

---

## 1. Purpose

This document is the current implementation handoff for the playable helm station.

It is focused on:

- stable runtime data sources
- editor hookup in `WBP_SubHelm`
- migration from debug-style widget composition to final helm panel composition
- gameplay-facing implementation order

This document does not describe a temporary validation-only path.
It is the current durable route toward gameplay implementation.

---

## 2. Current Runtime State

### 2.1 Locked responsibilities

The runtime split is now:

- `USubSonarComponent` = raw acoustic perception source
- `USubSonarSystemComponent` = sonar runtime logic, range, focus, tracks, modes
- `UTunnelNavigationRuntimeComponent` = route projection and tunnel geometry queries
- `UHelmNavigationDisplayComponent` = interpreted navigation and reconstruction data
- `USubmarineSystemsComponent` = helm command state, ballast, pump, stabilization
- `USubHelmWidget` = assembly shell and Blueprint-facing read surface

`USubHelmWidget` still routes commands.
It does not recalculate sonar or navigation geometry.

### 2.2 New stable panel contracts now exposed

`USubHelmWidget` now exposes Blueprint-pure panel snapshots:

- `FHelmPerceptionPanelData`
- `FHelmGuidancePanelData`
- `FHelmReconstructionPanelData`
- `FHelmControlPanelData`
- `FHelmAlertPanelData`

It also exposes a common panel runtime state:

- `EHelmPanelRuntimeState::Unbound`
- `EHelmPanelRuntimeState::Warming`
- `EHelmPanelRuntimeState::Degraded`
- `EHelmPanelRuntimeState::Valid`

These contracts are intended to become the canonical BP read surface for `WBP_SubHelm`.

---

## 3. What Was Implemented

### 3.1 `USubHelmWidget` now exposes panel data directly

The shell widget can now provide:

- perception panel data from `Sonar` + `SonarSystem`
- guidance panel data from `HelmNavigationDisplay`
- reconstruction panel data from `HelmNavigationDisplay`
- control panel data from `Systems` + `SubMovement`
- alert/status panel data from `SubMovement` + `HelmNavigationDisplay` + `SonarSystem`

### 3.2 Panel state logic is explicit

Current state resolution rules:

- perception:
  - `Unbound` if sonar runtime is missing
  - `Warming` if sonar range is not initialized
  - `Degraded` if signal is unstable
  - `Valid` otherwise
- navigation-derived panels:
  - `Unbound` if `UHelmNavigationDisplayComponent` is missing
  - `Warming` if runtime is not ready yet
  - `Degraded` if stale, suspect, or invalid
  - `Valid` otherwise
- control:
  - `Unbound` if systems or movement are missing
  - `Valid` otherwise

### 3.3 Existing bind path remains intact

The current explicit runtime binding path remains:

- `SonarDisplay`
- `HelmNavigationDisplay`
- `ReconstructionView`
- `TacticalGraphView`

No gameplay logic was moved into child widgets.
No sonar or tunnel math was moved into Blueprint.

---

## 4. Build Status

### 4.1 Command executed

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Sub3DEditor Win64 Development -Project='C:\Dev\Sub3D\Sub3D.uproject' -WaitMutex -NoHotReloadFromIDE
```

### 4.2 Result

Build result:

- `Succeeded`

This is a full editor target build.
The code changes compile and link successfully in the current workspace state.

### 4.3 Important context

There are many unrelated modified assets and source files already present in the worktree.
They were not reverted and were not folded into this tranche.

---

## 5. Editor Handoff

## 5.1 Root rule

`WBP_SubHelm` must now treat `USubHelmWidget` as the single Blueprint-facing shell API.

The Blueprint should:

- read panel snapshots from `self`
- compose panels
- style panels
- route controls through `self`

The Blueprint should not:

- query world generation directly
- recompute route or sonar interpretation
- make panel widgets talk to each other

## 5.2 Current data entry points to use in Blueprint

Use these getters on `WBP_SubHelm`:

- `GetPerceptionPanelData()`
- `GetGuidancePanelData()`
- `GetReconstructionPanelData()`
- `GetControlPanelData()`
- `GetAlertPanelData()`

These should replace ad-hoc direct reads where possible.

## 5.3 Runtime command routing to keep

Continue to route helm actions through `USubHelmWidget` methods:

- `RouteSonarPing`
- `RouteSonarPingHeldStart`
- `RouteSonarPingHeldStop`
- `RouteSetSonarMode`
- `RouteSetSonarFocusBearing`
- `RouteSetSonarRangePreset`
- `RouteSetSonarRangeNormalized`
- `RouteMarkPriorityTrack`

Do not bypass this path in Blueprint by directly reaching into the player controller unless there is a specific missing route.

---

## 6. Required Editor Layout Direction

Use the helm shell as a six-panel station:

- Panel A: raw perception
- Panel B: local guidance
- Panel C: reconstruction / clearance
- Panel D: helm controls
- Panel E: stability / commitment
- Panel F: status / alarms

Recommended hierarchy:

- center = raw perception
- left = interpreted navigation
- right = controls and stabilization
- top or bottom strip = status and alerts

Do not return to a floating multi-window debug board as the default player layout.

Use custom paint only where needed:

- raw sonar
- tactical graph
- reconstruction

Use UMG for:

- shell layout
- labels
- control stack
- toggles
- alert/status composition

---

## 7. Concrete Editor Wiring Steps

### Step 1

Open `WBP_SubHelm`.

Verify parent class:

- `USubHelmWidget`

### Step 2

Keep the current explicit widget references valid:

- `SonarDisplay`
- `ReconstructionView`
- `TacticalGraphView`

If the legacy `HelmNavigationDisplay` widget instance still exists only as a transitional panel, keep it only until the shell has replaced its role fully.
Do not build new gameplay UI on top of that legacy combined widget.

### Step 3

Add or reorganize UMG panels around the shell.

Recommended structure inside `WBP_SubHelm`:

- root canvas or overlay
- central sonar region
- left guidance stack
- right control stack
- bottom status strip

### Step 4

For each UMG panel:

- bind visibility, labels, and values from the new snapshot structs
- branch on `State`
- provide a clean fallback presentation for:
  - `Unbound`
  - `Warming`
  - `Degraded`
  - `Valid`

No panel should collapse the whole station if its own data is degraded.

### Step 5

Implement the control panel first.

Reason:

- it is pure assembly from already available runtime state
- it does not require changing sonar or tunnel geometry rendering
- it is the fastest path from debug station to gameplay station

### Step 6

After controls, wire:

- status / alarms strip
- guidance summary
- reconstruction summary

Only then decide whether the old transitional navigation widget can be removed from the default layout.

---

## 8. Gameplay Implementation Order

This is the recommended order from current code state to playable helm gameplay.

### Phase 1 - Shell conversion

Goal:

- make `WBP_SubHelm` consume the new panel contracts
- stop spreading runtime reads across unrelated child widgets

Deliverables:

- control stack panel
- status / alarms strip
- state-aware panel placeholders

### Phase 2 - Replace control UX

Goal:

- make propulsion, rudder, trim, ballast, and stabilization readable and intentional

Deliverables:

- command sliders or levers
- dampener indicators
- target readouts
- current command readouts

Source contract:

- `FHelmControlPanelData`

### Phase 3 - Add gameplay commitment read

Goal:

- expose stopping distance, commitment pressure, and machine state without debug noise

Deliverables:

- stop distance read
- commitment severity read
- navigation confidence read
- signal instability read

Source contracts:

- `FHelmAlertPanelData`
- `FHelmGuidancePanelData`

### Phase 4 - Refine guidance and reconstruction panels

Goal:

- present interpreted navigation as dedicated player instruments rather than dev plots

Deliverables:

- tactical guidance panel fed from `FHelmGuidancePanelData`
- clean reconstruction panel fed from `FHelmReconstructionPanelData`

### Phase 5 - Remove legacy combined panel from default shell

Goal:

- stop relying on the old monolithic transitional display in the default station layout

Condition before removal:

- the shell can render all player-needed panel reads from the snapshot contracts

---

## 9. Rules To Keep During Gameplay Implementation

Do not break these rules:

- `USubHelmWidget` remains a shell and route layer
- no widget recomputes sonar logic
- no widget recomputes route geometry
- no direct worldgen access from `SubHelmWidget`
- no child widget cross-calls
- no Blueprint recomputation of business logic

If a new read is needed, add it as runtime data surfaced through a stable contract.
Do not solve missing data by reading half the submarine directly in random bindings.

---

## 10. Immediate Next Editor Tasks

If the next person opens the editor now, the recommended sequence is:

1. Open `WBP_SubHelm`
2. Add a clean right-hand control stack driven only by `GetControlPanelData()`
3. Add a bottom status strip driven only by `GetAlertPanelData()`
4. Add a left guidance summary driven by `GetGuidancePanelData()`
5. Add state labels or visual fallback for `Unbound`, `Warming`, `Degraded`, `Valid`
6. Keep raw sonar central and visually dominant
7. Run PIE and verify that all panel states are readable without external debug views

---

## 11. Verification Done

Verified:

- full editor target build succeeded
- new helm panel data structs are reflected and compiled
- `USubHelmWidget` exposes Blueprint-pure panel getters
- the shell still keeps existing sonar route methods and bind flow

Not verified in this tranche:

- final `WBP_SubHelm` panel composition in editor
- final player-facing readability in PIE
- removal of the transitional combined navigation widget from the shell

---

## 12. Canonical Files For This Tranche

Primary runtime shell changes:

- `Source/Sub3D/Submarine/SubHelmWidget.h`
- `Source/Sub3D/Submarine/SubHelmWidget.cpp`

Relevant existing runtime sources:

- `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.h`
- `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.h`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineBase.h`
- `Source/Sub3D/Submarine/SubmarineBase.cpp`

---

## 13. Bottom Line

The codebase now has a usable shell-level data contract for the helm station.

The next work should happen mainly in editor composition and gameplay-facing panel replacement.

Do not reopen sonar math or tunnel projection math before the shell layout is rebuilt around these contracts.
