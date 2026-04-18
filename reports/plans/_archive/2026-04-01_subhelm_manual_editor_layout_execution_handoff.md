# Sub3D - SUBHELM Manual Editor Layout Execution Handoff

Date: 2026-04-01
Scope: `WBP_SubHelm`, `USubHelmWidget`, native helm panels, gameplay-facing helm controls
Status: runtime shell updated for manual editor setup

---

## 1. Intent

The target path is now:

- manual widget creation in the editor
- stable manual placement in a 16:9 reference frame
- no runtime auto-create as the expected workflow
- no runtime re-docking as the expected workflow

The helm shell must remain readable and modular, but the authoritative layout is now the editor asset.

---

## 2. Rules For This Phase

These rules are active for the helm station:

- no optional functionality in the current implementation path
- always plan for long-term editor clarity
- prefer editor-assigned widgets over runtime-created widgets
- prefer exact widget names and stable asset wiring over implicit discovery
- do not let PIE rewrite a manually authored helm layout unless explicitly re-enabled

Runtime fallback code still exists in some places for backward compatibility, but it is not the target workflow for this phase.

---

## 3. What Changed In Code

## 3.1 Shell defaults changed

`USubHelmWidget` now defaults to:

- `bAutoCreateControlStackPanelIfMissing = false`
- `bAutoCreateStatusStripPanelIfMissing = false`
- `bDockInstrumentPanels = false`

This means the shell no longer treats runtime auto-spawn and runtime re-layout as the default path.

## 3.2 Manual widget binding is more explicit

The shell now tries to resolve these widget names directly from `WBP_SubHelm` before generic type discovery:

- `SonarDisplay`
- `HelmNavigationDisplay`
- `ReconstructionView`
- `TacticalGraphView`
- `ControlStackPanel`
- `StatusStripPanel`

If `ControlStackPanel` or `StatusStripPanel` is still missing, the log now states the expected exact widget name and class.

## 3.3 Native helm child widgets can now self-resolve the shell

`UHelmControlPanelWidget` and `UHelmStatusStripWidget` now try to recover a parent `USubHelmWidget` through their outer chain.

This is a safety path for editor-assigned widgets when the explicit shell init call did not happen early enough.

This specifically addresses the observed PIE state:

- `SONAR UNBOUND`
- `No helm shell bound`

when auto-create was disabled.

## 3.4 Ballast display data is now correct

`FHelmControlPanelData.GlobalBallastFill01` is now the average current ballast fill, not the target.

The target already remains in:

- `CommandState.GlobalBallastTarget01`

This means the control panel can now display:

- target ballast fill
- current ballast fill

without showing the same value twice.

## 3.5 Ballast tank detail is now exposed

New per-tank control panel data now exists:

- `FHelmBallastTankPanelData`

Each tank exposes:

- index
- current fill
- target fill
- pump state

This allows the control panel to display a real ballast section based on the submarine runtime array, not a fake fixed layout.

## 3.6 Rudder and planes hold behavior now exists

`FSubmarineCommandState` now includes:

- `bRudderHoldEnabled`
- `bPlaneHoldEnabled`

`USubmarineSystemsComponent` now supports:

- `SetRudderHoldEnabled`
- `SetPlaneHoldEnabled`

and applies slow return-to-zero when hold is disabled.

Current behavior:

- if rudder hold is disabled, `HelmYawCmd` drifts back toward `0`
- if planes hold is disabled and auto-pitch is not active, `HelmTrimCmd` drifts back toward `0`
- if hold is enabled, the last manual command remains applied

This is the first gameplay implementation for the requested:

- lock the rudder
- or let it slowly stabilize toward zero

---

## 4. Native Panel Behavior Now

## 4.1 Control panel

`UHelmControlPanelWidget` now provides:

- neutral marker on each axis slider
- throttle hints: `S` on reverse side, `Z` on forward side
- rudder hints: `L` and `R`
- planes hints: `DOWN` and `UP`
- rudder hold toggle
- planes hold toggle
- ballast target slider
- ballast tank rows with current fill, target fill, and pump state

## 4.2 Status strip

`UHelmStatusStripWidget` now displays:

- shell fallback binding more robustly
- hold mode summary for rudder and planes
- current ballast fill from runtime, not only target state

---

## 5. Manual Editor Layout Is Now The Expected Path

The recommended approach is to author `WBP_SubHelm` around a fixed 16:9 reference frame.

The shell should not be composed as free-floating windows in the shipping path.

## 5.1 Root composition

Use this hierarchy in `WBP_SubHelm`:

1. `CanvasPanel` as root
2. `SizeBox` named `HelmReferenceFrame`
3. inside it, a `CanvasPanel` named `HelmReferenceCanvas`

`HelmReferenceFrame` settings:

- width override: `1280`
- height override: `720`
- anchors: center
- alignment: `0.5, 0.5`
- position: `0, 0`

This creates one stable authoring frame for both editor and PIE.

The outer root canvas can then scale or center this frame, but the authored positions remain fixed.

## 5.2 Exact widget instances to place manually

Place these widgets directly in the reference canvas with these exact instance names:

- `SonarDisplay`
- `TacticalGraphView`
- `ReconstructionView`
- `ControlStackPanel`
- `StatusStripPanel`

Optional legacy widget:

- `HelmNavigationDisplay`

It should stay hidden in the normal player layout once the split panels are in place.

## 5.3 Exact native classes or Blueprint classes

For manual editor setup, you may use either:

- the native class directly
- a Blueprint subclass of the native class

For the shell binding to remain clear, the instance name must still match the expected property name.

Examples:

- a Blueprint child of `UHelmControlPanelWidget` is valid if the placed widget is still named `ControlStackPanel`
- a Blueprint child of `UHelmStatusStripWidget` is valid if the placed widget is still named `StatusStripPanel`

---

## 6. Recommended 1280x720 Reference Layout

Use these positions as the baseline manual composition.

All coordinates below are relative to `HelmReferenceCanvas`.

## 6.1 Center sonar

- widget: `SonarDisplay`
- position: `320, 20`
- size: `560 x 540`

This remains the dominant visual panel.

## 6.2 Left top navigation

- widget: `TacticalGraphView`
- position: `24, 24`
- size: `272 x 220`

## 6.3 Left bottom clearance

- widget: `ReconstructionView`
- position: `24, 264`
- size: `272 x 276`

## 6.4 Right control stack

- widget: `ControlStackPanel`
- position: `904, 20`
- size: `352 x 540`

## 6.5 Bottom status strip

- widget: `StatusStripPanel`
- position: `320, 580`
- size: `936 x 92`

This keeps the station composition aligned with the intended hierarchy:

- center = raw perception
- left = interpreted local navigation
- right = command stack
- bottom = status and warnings

---

## 7. Exact Editor Setup Steps

Follow this order:

1. Open `WBP_SubHelm`
2. Confirm parent class is `USubHelmWidget`
3. Set shell defaults in Details:
   - `bAutoCreateSonarDisplayIfMissing = false`
   - `bAutoCreateHelmNavigationDisplayIfMissing = false`
   - `bAutoCreateControlStackPanelIfMissing = false`
   - `bAutoCreateStatusStripPanelIfMissing = false`
   - `bDockInstrumentPanels = false`
4. Add the `HelmReferenceFrame` size box
5. Add `HelmReferenceCanvas` inside it
6. Manually place the five panel widgets with the exact names listed above
7. Compile the widget blueprint
8. Verify the Bind Widgets panel:
   - `SonarDisplay` should bind
   - `TacticalGraphView` should bind
   - `ReconstructionView` should bind
   - `ControlStackPanel` should bind
   - `StatusStripPanel` should bind
9. Run PIE
10. Enter the helm station

If `ControlStackPanel` or `StatusStripPanel` still shows as unbound, check:

- the exact instance name
- the class of the widget
- whether the widget is actually inside the same widget tree as `WBP_SubHelm`

---

## 8. Validation Gates

## Gate 1 - Shell structure

Expected:

- the same authored 16:9 composition appears in editor preview and PIE
- no panel jumps to a new runtime position
- no window behaves like a floating debug panel in normal play

## Gate 2 - Binding

Expected:

- `ControlStackPanel` no longer reports `No helm shell bound`
- `StatusStripPanel` no longer reports `No helm shell bound`
- sonar state reflects actual helm runtime binding

## Gate 3 - Controls

Expected:

- throttle slider changes submarine power response
- rudder slider changes turning response
- planes slider changes pitch response
- ballast slider changes target ballast fill
- ballast rows show current and target per tank
- pump toggle changes pump state
- pump power changes pump rate

## Gate 4 - Hold behavior

Expected:

- with `RUDDER HOLD` disabled, rudder slowly returns to zero
- with `RUDDER HOLD` enabled, rudder keeps the commanded offset
- with `PLANES HOLD` disabled, planes slowly return to zero when auto-pitch is not active
- with `PLANES HOLD` enabled, planes keep the commanded offset

## Gate 5 - Status strip

Expected:

- heading, speed, depth, pitch, and roll are readable
- ballast display is based on current fill, not only target
- warnings still report stop-distance or commitment when navigation runtime raises them

---

## 9. Verified In Code

Verified:

- manual shell path is the default path now
- runtime auto-create defaults are disabled for the new helm panels
- runtime re-layout is disabled by default
- manual widget exact-name lookup exists
- child native helm widgets can now self-resolve the parent shell
- ballast current fill is now derived from real tank fill data
- ballast per-tank panel data exists
- rudder hold and planes hold gameplay state now exists in runtime code

Not yet verified in this tranche:

- final PIE after manual editor re-placement
- final editor bind state after the user re-saves `WBP_SubHelm`
- final visual pass on the widget asset itself
- input mapping changes for `Z / S` in Enhanced Input assets

---

## 10. Known Remaining Work

These items are still open:

- the final `WBP_SubHelm` asset composition is still a manual editor task
- the native control panel has neutral markers, but the final shipping skin may still want custom art for the slider rails
- the input asset mapping for `Z / S` was not changed in this tranche
- the ballast panel currently shows tank rows in the control stack, not a dedicated large ballast panel

These are the next correct tasks after the current runtime fix.

---

## 11. Files For This Tranche

Modified runtime:

- `Source/Sub3D/Submarine/SubHelmWidget.h`
- `Source/Sub3D/Submarine/SubHelmWidget.cpp`
- `Source/Sub3D/Submarine/HelmControlPanelWidget.h`
- `Source/Sub3D/Submarine/HelmControlPanelWidget.cpp`
- `Source/Sub3D/Submarine/HelmStatusStripWidget.h`
- `Source/Sub3D/Submarine/HelmStatusStripWidget.cpp`
- `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.h`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp`
- `Source/Sub3D/Submarine/SubPlayerController.h`
- `Source/Sub3D/Submarine/SubPlayerController.cpp`

Modified project rules:

- `AGENTS.md`

New handoff:

- `reports/plans/2026-04-01_subhelm_manual_editor_layout_execution_handoff.md`

---

## 12. Bottom Line

The helm shell is now aligned with a manual editor-authored layout.

The next correct step is:

- recompose `WBP_SubHelm` on a fixed 1280x720 reference frame
- bind the five named widgets manually
- run PIE and validate the new control and status panels in the manual path

Do not re-enable runtime auto-create or runtime re-layout as the normal workflow for this phase.
