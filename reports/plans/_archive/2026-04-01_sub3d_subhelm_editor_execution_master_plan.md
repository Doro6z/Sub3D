# Sub3D - SUBHELM Editor Execution Master Plan

Date: 2026-04-01
Owner scope: `SUBHELM`
Status: runtime implementation advanced, full editor target build succeeded
Supersedes for current execution: `2026-04-01_sub3d_helm_gameplay_implementation_handoff.md`

---

## 1. Purpose

This document is the current execution reference for the playable submarine helm station.

It is written for:

- runtime implementation
- editor composition in `WBP_SubHelm`
- validation order
- handoff to the next editor pass

This is not a generic UI concept note.
Every important statement maps to code, current assets, or a concrete editor action.

---

## 2. Intent

The target is a helm station that is directly playable and readable.

The player must understand in one glance:

- what the sonar sees
- where the tunnel is going
- whether the hull fits
- what command is currently applied
- which automatic stabilization systems are active
- whether the boat is entering a stop-distance or commitment risk

The station is not meant to become a debug board.
The runtime physics remain visible.
The UI must support piloting, not replace the boat model.

---

## 3. Current Runtime Facts

## 3.1 Runtime ownership is now split correctly

The current split is:

- `USubSonarComponent` = raw ping source
- `USubSonarSystemComponent` = sonar range, focus, mode, tracks
- `UTunnelNavigationRuntimeComponent` = local route and tunnel geometry runtime
- `UHelmNavigationDisplayComponent` = interpreted nav display data
- `USubmarineSystemsComponent` = helm commands, ballast, pump, stabilization state
- `USubHelmWidget` = shell, routing, widget binding, panel data exposure

`USubHelmWidget` is still a shell.
It does not recalculate sonar or tunnel geometry.

## 3.2 Stable helm data contracts exist

`USubHelmWidget` now exposes:

- `FHelmPerceptionPanelData`
- `FHelmGuidancePanelData`
- `FHelmReconstructionPanelData`
- `FHelmControlPanelData`
- `FHelmAlertPanelData`

Each panel contract exposes:

- `bBound`
- `State`

Runtime state values are:

- `Unbound`
- `Warming`
- `Degraded`
- `Valid`

These are the canonical read surface for Blueprint and native child panels.

## 3.3 Gameplay routes now cover the helm control surface

The helm shell now exposes routes for:

- sonar ping and sonar mode/range/focus
- throttle
- rudder
- trim / planes
- ballast global target
- ballast circuit active state
- pump active state
- pump power
- stabilization master
- auto speed enable
- auto depth enable
- auto pitch enable
- target speed
- target depth
- target pitch

Controller-side server routes were added for stabilization toggles and targets.

---

## 4. Implemented Now

## 4.1 New native panel widgets

Two native widgets were added:

- `UHelmControlPanelWidget`
- `UHelmStatusStripWidget`

These widgets are functional now.

They are built in C++ and do not depend on a Blueprint layout graph.

## 4.2 Automatic shell injection

`USubHelmWidget` now supports:

- binding `ControlStackPanel` if provided in `WBP_SubHelm`
- binding `StatusStripPanel` if provided in `WBP_SubHelm`
- auto-creating both if they are missing

This means the station can gain the missing right-hand control panel and bottom strip without hand-editing the binary widget asset first.

## 4.3 Automatic docked layout

When `bDockInstrumentPanels = true`, the shell applies a fixed docked layout attempt for:

- `TacticalGraphView`
- `ReconstructionView`
- `SonarDisplay`
- `ControlStackPanel`
- `StatusStripPanel`

The docked layout assumes the root helm widget contains a `CanvasPanel`.

If the existing widget asset already uses that structure, the layout is applied directly at runtime.

## 4.4 Split-panel priority over legacy combined panel

When split navigation panels are present:

- the legacy `HelmNavigationDisplay` is hidden if `bHideLegacyHelmNavigationDisplayWhenSplitPanelsAvailable = true`

This keeps the station focused on:

- raw sonar
- tactical guidance
- clearance / reconstruction
- control stack
- status strip

## 4.5 Docked style on navigation panels

The shell now forces the split navigation widgets toward a fixed station presentation:

- drag disabled
- collapse disabled
- docked title sizes
- consistent frame colors
- consistent accent and warning colors

This moves them away from floating debug windows.

---

## 5. Constraints

These constraints remain active:

- no worldgen access from `USubHelmWidget`
- no sonar math in Blueprint
- no tunnel math in Blueprint
- no child widget cross-calls
- no binary `.uasset` patching by text replacement
- no unrelated refactor in submarine runtime

The current implementation deliberately avoids editing `WBP_SubHelm.uasset` directly because that asset is binary and not reviewable through source patching.

The runtime path added here is meant to keep progress unblocked.

---

## 6. Actual File-Level Scope

Modified runtime files:

- `Source/Sub3D/Submarine/SubHelmWidget.h`
- `Source/Sub3D/Submarine/SubHelmWidget.cpp`
- `Source/Sub3D/Submarine/SubPlayerController.h`
- `Source/Sub3D/Submarine/SubPlayerController.cpp`

New runtime files:

- `Source/Sub3D/Submarine/HelmControlPanelWidget.h`
- `Source/Sub3D/Submarine/HelmControlPanelWidget.cpp`
- `Source/Sub3D/Submarine/HelmStatusStripWidget.h`
- `Source/Sub3D/Submarine/HelmStatusStripWidget.cpp`

No content asset was modified in this tranche.

---

## 7. Current Station Composition

The current intended station composition is:

- center: `SonarDisplay`
- left top: `TacticalGraphView`
- left bottom: `ReconstructionView`
- right: `ControlStackPanel`
- bottom: `StatusStripPanel`

This is the current functional target.

It is acceptable if the visual skin is still editor-legacy in places, as long as:

- the panel responsibilities are correct
- the data is stable
- the controls are routed correctly
- the station is playable

---

## 8. What The New Panels Do

## 8.1 `UHelmControlPanelWidget`

This panel provides:

- throttle slider
- rudder slider
- planes slider
- ballast slider
- pump power slider
- stabilization master toggle
- auto speed toggle
- auto depth toggle
- auto pitch toggle
- ballast active toggle
- pump active toggle
- current motion summary
- current auto-target summary

Important behavior:

- it reads current data from `USubHelmWidget`
- it routes commands through `USubHelmWidget`
- it does not touch runtime components directly

## 8.2 `UHelmStatusStripWidget`

This panel provides:

- heading
- speed
- depth
- pitch
- roll
- sonar range and focus summary
- stabilization state summary
- stop-distance / commitment / degraded-state warning summary

It is intended as the player-facing compact strip, not a dev log.

---

## 9. Editor Plan

This is the exact editor plan from the current code state.

## Phase A - Accept the runtime fallback path first

Open `WBP_SubHelm` and verify:

- parent class is `USubHelmWidget`
- root contains or resolves to a `CanvasPanel`
- `SonarDisplay` still exists
- `ReconstructionView` still exists
- `TacticalGraphView` still exists

At this point, `ControlStackPanel` and `StatusStripPanel` do not need to exist in the asset because C++ can create them.

## Phase B - Make the shell asset explicit

If you want the editor asset to match the runtime shell explicitly, add these widgets inside `WBP_SubHelm`:

- a widget with instance name `ControlStackPanel`, parent `UHelmControlPanelWidget`
- a widget with instance name `StatusStripPanel`, parent `UHelmStatusStripWidget`

This is optional for functionality now.
It is recommended for long-term editor clarity.

## Phase C - Remove legacy layout noise

In `WBP_SubHelm`, remove or hide:

- legacy floating debug labels
- old mode buttons that are no longer used
- duplicate combined navigation display regions
- any manual display texts that conflict with the native status strip

The rule is simple:

- keep one authoritative place per panel responsibility

## Phase D - Final visual cleanup

After the shell works in PIE:

- tighten padding
- simplify panel borders
- reduce text density in non-critical areas
- keep sonar dominant
- keep right column readable under motion stress

This phase is visual only.
Do not reopen runtime routing while doing it.

---

## 10. Exact Editor Checklist

When opening the editor next, use this order:

1. Open `WBP_SubHelm`
2. Confirm parent class is still `USubHelmWidget`
3. Confirm root is a `CanvasPanel`, or add one as the root if needed
4. Confirm the center sonar widget instance is still present and named `SonarDisplay`
5. Confirm the guidance widget instance is named `TacticalGraphView`
6. Confirm the clearance widget instance is named `ReconstructionView`
7. Compile `WBP_SubHelm`
8. Run PIE
9. Enter helm station
10. Confirm that control stack and status strip appear, even if they were not present in the asset
11. Confirm sonar remains functional
12. Confirm throttle, rudder, trim, ballast, pump, and toggles actually route to the submarine

Only after that should you start cleaning the asset graph and layout.

---

## 11. Validation Plan

## 11.1 Deterministic validation already done

Done:

- full editor target build succeeded

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Sub3DEditor Win64 Development -Project='C:\Dev\Sub3D\Sub3D.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

- `Succeeded`

## 11.2 Required PIE validation next

The next validation pass is manual PIE.

Use this order:

### Gate 1 - Shell present

Expected:

- helm opens
- sonar visible
- tactical panel visible
- reconstruction panel visible
- control panel visible
- status strip visible

### Gate 2 - Manual command routing

Expected:

- throttle slider changes submarine power response
- rudder slider changes turning response
- planes slider changes pitch response
- ballast slider changes fill target
- pump toggle and power affect pump state

### Gate 3 - Stabilization routing

Expected:

- enabling stabilization master changes auto eligibility
- enabling auto speed captures current speed target
- enabling auto depth captures current depth target
- enabling auto pitch captures current pitch target
- manual throttle or trim input suspends corresponding auto axis as designed by `USubmarineSystemsComponent`

### Gate 4 - Readability

Expected:

- bottom strip reports heading / speed / depth correctly
- warning line changes when signal or navigation is degraded
- stop-distance and commitment warnings are visible without opening debug overlays

### Gate 5 - Legacy cleanup safety

Expected:

- no duplicate monolithic nav display remains visible
- no floating draggable debug windows remain active in normal play

---

## 12. Known Gaps

These are still open:

- no direct editor-authored final skin pass has been applied to `WBP_SubHelm`
- no direct `.uasset` layout refactor was performed in this tranche
- no PIE proof has been recorded yet for the new auto-created native panels
- the control panel currently exposes auto toggles and summaries, but the final product may still want dedicated target adjustment controls for speed/depth/pitch

These are not architecture failures.
They are the next controlled editor pass.

---

## 13. Recommended Next Execution Order

Follow this exact order:

1. Open `WBP_SubHelm`
2. Verify the root canvas assumption
3. Run PIE without editing the asset first
4. Confirm the new native panels appear and function
5. Only then add explicit `ControlStackPanel` and `StatusStripPanel` widget instances into the asset
6. Remove duplicate legacy elements
7. Run PIE again
8. Perform final visual cleanup

This order avoids mixing runtime uncertainty with editor cleanup.

---

## 14. Handoff Summary

The helm shell is no longer blocked on binary Blueprint editing for the missing right-hand and bottom panels.

Current practical state:

- runtime panel contracts are stable
- helm control routing is broader and correct
- two native player-facing panels now exist
- the shell can auto-create and dock them
- the full editor target build passes

The next step is not more runtime math work.
The next step is editor composition and PIE validation of the now-complete gameplay surface.
