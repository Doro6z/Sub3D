# Sub3D - Helm Full Editor & Gameplay Setup Guide

## 1. Purpose

This document is the durable editor and gameplay setup guide for the full Helm station UI.

It covers:

- the raw sonar screen
- `ReconstructionView`
- `TacticalGraphView`
- explicit widget binding
- draggable instrument behavior
- runtime/component prerequisites
- troubleshooting
- PIE validation order

This document is the canonical setup path.

It replaces:

- auto-create workflows
- ad-hoc widget tree discovery
- temporary legacy Helm display assumptions

---

## 2. First Principle

There are **two different systems** on the helm now.

### 2.1 Raw sonar

This is the live tactical perception screen.

It is:

- direct
- mechanical
- noisy
- not classified by default
- the main perception tool

It is **not** the place for structured clearance/navigation help.

### 2.2 Secondary navigation instruments

These are interpreted secondary tools.

They are:

- `ReconstructionView`
- `TacticalGraphView`

They are not the same thing as sonar.
They are fed from navigation/runtime interpretation.

---

## 3. Where the bind really happens

The bind is **not** created in the Blueprint Event Graph.

The bind is created in C++ by `USubHelmWidget`.

Important files:

- [SubHelmWidget.h](/c:/Dev/Sub3D/Source/Sub3D/Submarine/SubHelmWidget.h)
- [SubHelmWidget.cpp](/c:/Dev/Sub3D/Source/Sub3D/Submarine/SubHelmWidget.cpp)

### 3.1 Binding model

`USubHelmWidget` exposes these widget references:

- `SonarDisplay`
- `ReconstructionView`
- `TacticalGraphView`

These are declared with `BindWidgetOptional`.

That means:

- the widgets must exist **inside `WBP_SubHelm`**
- the widget **instance names** in the designer must match exactly
- the widget **class type** must also match

You do **not** create Blueprint graph nodes like:

- Set SonarDisplay
- Set ReconstructionView
- Set TacticalGraphView

That is not the canonical path.

You also do **not** route helm actions by manually reading the `Owner Controller` variable and casting it in the Blueprint graph.

Canonical action routing from `WBP_SubHelm` is:

- call `RouteSonarPing()` on `self`
- call `RouteSonarPingHeldStart()` / `RouteSonarPingHeldStop()` on `self`
- call `RouteSetSonarMode(...)` on `self`
- call `RouteSetSonarFocusBearing(...)` on `self`
- call `RouteSetSonarRangePreset(...)` on `self`
- call `RouteMarkPriorityTrack(...)` on `self`

Why:

- `USubHelmWidget` resolves runtime references before routing
- direct Blueprint access to `Owner Controller` can be stale or null
- the `Route*` methods are the durable API surface for helm UI actions

### 3.2 Consequence

If a panel shows `UNBOUND`, the first thing to check is not the Blueprint graph.

The first thing to check is:

- wrong widget instance name
- wrong parent class of the widget Blueprint
- widget not actually inside `WBP_SubHelm`

---

## 4. Required runtime components

Your submarine actor must expose all of these:

- `Sonar`
- `SonarSystem`
- `TunnelNavigationRuntime`
- `HelmNavigationDisplay`

Relevant code:

- [SubmarineBase.h](/c:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.h)
- [SubmarineBase.cpp](/c:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp)
- [HelmNavigationDisplayComponent.h](/c:/Dev/Sub3D/Source/Sub3D/Submarine/HelmNavigationDisplayComponent.h)
- [TunnelNavigationRuntimeComponent.h](/c:/Dev/Sub3D/Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.h)

If `ReconstructionView` says `NO RUNTIME DATA`, it usually means:

- the widget bind succeeded
- but `HelmNavigationDisplay` is not receiving valid route/nav data

That is a runtime data issue, not a widget bind issue.

---

## 5. Required Blueprint classes

## 5.1 `WBP_SubHelm`

Parent class:

- `USubHelmWidget`

This is the canonical helm widget root.

If `WBP_SubHelm` is not parented to `USubHelmWidget`, the whole durable bind path is wrong.

## 5.2 Sonar widget Blueprint

Use your current sonar widget Blueprint if it already works.

Its parent must be:

- `USubSonarDisplayWidget`

The asset name can stay whatever you want.

What matters is the **instance name inside `WBP_SubHelm`**:

- `SonarDisplay`

## 5.3 `WBP_ReconstructionView`

Create a Widget Blueprint with parent:

- `UReconstructionViewWidget`

Important:

- the Blueprint can remain visually empty
- drawing is done in C++ via `NativePaint`
- you do not need a custom Blueprint graph for the basic version

## 5.4 `WBP_TacticalGraphView`

Create a Widget Blueprint with parent:

- `UTacticalGraphViewWidget`

Important:

- the Blueprint can remain visually empty
- drawing is done in C++ via `NativePaint`
- no custom graph is needed for the basic version

## 5.5 Optional base window Blueprint

You do **not** need to create a separate Blueprint for `UDraggableInstrumentWindowWidget` to make the current setup work.

That class is already the base class of:

- `UReconstructionViewWidget`
- `UTacticalGraphViewWidget`

You may create a style Blueprint later if you want shared exposed theme data, but it is not required for the canonical setup.

---

## 6. Exact widget tree setup in `WBP_SubHelm`

## 6.1 Root

Use a `CanvasPanel` root.

This is the cleanest way to:

- position the sonar panel
- position the reconstruction panel
- position the tactical panel
- keep explicit sizes

## 6.2 Add the three widgets

Inside `WBP_SubHelm`, add:

1. your sonar widget Blueprint
2. `WBP_ReconstructionView`
3. `WBP_TacticalGraphView`

## 6.3 Rename the widget instances exactly

This is critical.

The **instance names in the `WBP_SubHelm` hierarchy** must be:

- `SonarDisplay`
- `ReconstructionView`
- `TacticalGraphView`

The asset names may be:

- `WBP_SubRadar`
- `WBP_ReconstructionView`
- `WBP_TacticalGraphView`

That is fine.

But the **instance names** in the hierarchy must still be:

- `SonarDisplay`
- `ReconstructionView`
- `TacticalGraphView`

If you leave a name like:

- `WBP_TacticalGraphView_0`

the C++ bind will not use the canonical `BindWidgetOptional` path.

## 6.4 Recommended first-pass sizes

Recommended first-pass panel sizes:

- `SonarDisplay`: keep your current working size
- `ReconstructionView`: `600 x 300`
- `TacticalGraphView`: `360 x 300`

These are not final product sizes.
They are just stable gameplay validation sizes.

## 6.5 Recommended first-pass layout

Suggested temporary layout:

- `SonarDisplay` centered or right-weighted
- `ReconstructionView` left side
- `TacticalGraphView` right of Reconstruction or below it

The main rule:

- sonar remains visually dominant
- navigation instruments remain secondary

---

## 7. What to put inside the child widget Blueprints

## 7.1 `WBP_ReconstructionView`

For the current durable implementation:

- keep it visually empty
- do not add inner controls yet
- do not add overlay text blocks yet

Why:

- the rendering is already done in C++
- the interaction is already implemented in C++
- extra children can intercept input and create confusion

## 7.2 `WBP_TacticalGraphView`

For now:

- keep it visually empty
- no graph logic in Blueprint
- no manual tick/event graph logic

## 7.3 Sonar widget

If your current sonar widget already works:

- keep it
- do not refactor it in the same tranche

Only ensure that inside `WBP_SubHelm`, its instance is named `SonarDisplay`.

---

## 8. How draggable behavior works

Relevant base class:

- [DraggableInstrumentWindowWidget.h](/c:/Dev/Sub3D/Source/Sub3D/Submarine/DraggableInstrumentWindowWidget.h)
- [DraggableInstrumentWindowWidget.cpp](/c:/Dev/Sub3D/Source/Sub3D/Submarine/DraggableInstrumentWindowWidget.cpp)

The drag behavior is already implemented in C++.

### 8.1 Window drag

To move the window:

- click and hold on the title bar
- drag the window

The title bar is the top strip of the instrument window.

### 8.2 Collapse

To collapse:

- click the small `-` / `+` control in the top-right corner

### 8.3 Reconstruction interaction

`ReconstructionView` has a special content interaction.

Inside the plot area:

- hold left mouse
- drag horizontally

This changes the reconstruction emphasis between:

- section-dominant
- forward-dominant

Important:

- title bar drag = move the window
- content drag = change reconstruction blend

### 8.4 Tactical graph interaction

Current tactical graph:

- title bar drag only
- no content interaction yet

---

## 9. Canonical component settings

On the submarine instance or Blueprint, check:

## 9.1 `HelmNavigationDisplay`

Recommended:

- `bAutoBindTunnelNavigationRuntime = true`
- `RefreshPeriodS = 0.08`
- `ForwardLookaheadCm = 12000`
- `TacticalGraphRadiusCm = 18000`
- `bEnableDebugLogs = true` for validation

## 9.2 `TunnelNavigationRuntime`

Recommended for validation:

- `bEnableDebugDraw = true`

This helps compare widget behavior with world-space debug.

## 9.3 `USubHelmWidget`

Recommended:

- `bAllowWidgetTreeFallbackDiscovery = false`

Why:

- this forces the durable explicit bind path
- if something breaks, you see it immediately

Do not rely on fallback for canonical production setup.

---

## 10. Reading current failure states

## 10.1 `UNBOUND`

Meaning:

- the widget is present on screen
- but `USubHelmWidget` did not bind a runtime source to it

Check in this order:

1. Is the widget actually inside `WBP_SubHelm`?
2. Is the parent class correct?
3. Is the widget instance name exact?
4. Is `WBP_SubHelm` still parented to `USubHelmWidget`?

## 10.2 `NO RUNTIME DATA`

Meaning:

- the widget bind succeeded
- but runtime nav data is invalid or unavailable

Check in this order:

1. Does the submarine have `TunnelNavigationRuntime`?
2. Does it have `HelmNavigationDisplay`?
3. Is the route actor present and built?
4. Is the submarine projected onto the route at runtime?
5. What does the log say with `bEnableDebugLogs = true`?

Important:

`NO RUNTIME DATA` is **not** the same as `UNBOUND`.

## 10.3 Sonar dead / blank after the refactor

Most likely cause:

- the sonar widget instance inside `WBP_SubHelm` is not named `SonarDisplay`

Secondary causes:

- wrong parent class for the sonar widget Blueprint
- widget removed from `WBP_SubHelm`
- runtime sub references not resolving
- sonar buttons in `WBP_SubHelm` are still wired manually through `Owner Controller` instead of the `Route*` functions on `self`

---

## 11. Exact editor workflow

## Step 1

Open `WBP_SubHelm`.

Verify parent class:

- `USubHelmWidget`

## Step 2

Remove the old monolithic Helm navigation widget instance from the hierarchy.

You do not want the old `HelmNavigationDisplay` widget in the canonical new layout.

## Step 3

Add your sonar widget Blueprint to the widget tree.

Rename the widget instance:

- `SonarDisplay`

## Step 4

Create `WBP_ReconstructionView` if not already done.

Parent class:

- `UReconstructionViewWidget`

Keep it empty.

## Step 5

Add `WBP_ReconstructionView` to `WBP_SubHelm`.

Rename the widget instance:

- `ReconstructionView`

Set explicit size.

## Step 6

Create `WBP_TacticalGraphView` if not already done.

Parent class:

- `UTacticalGraphViewWidget`

Keep it empty.

## Step 7

Add `WBP_TacticalGraphView` to `WBP_SubHelm`.

Rename the widget instance:

- `TacticalGraphView`

Set explicit size.

## Step 8

Do not add manual Blueprint graph binding logic for these widgets.

Do not call init functions manually from Blueprint.

Let `USubHelmWidget` do the runtime binding.

For helm actions, use the widget API on `self`.

Examples:

- sonar ping button -> `RouteSonarPing`
- sonar hold pressed -> `RouteSonarPingHeldStart`
- sonar hold released -> `RouteSonarPingHeldStop`
- mode buttons -> `RouteSetSonarMode`
- focus controls -> `RouteSetSonarFocusBearing`
- range controls -> `RouteSetSonarRangePreset`

Do **not** build button logic like:

- `Owner Controller` -> cast to `PC_SubPlayerController` -> call controller event

That path bypasses the canonical runtime ref resolution in `USubHelmWidget`.

## Step 9

Compile and save:

- `WBP_ReconstructionView`
- `WBP_TacticalGraphView`
- `WBP_SubHelm`

## Step 10

Open `BP_Submarine_Compiler`.

Verify components:

- `Sonar`
- `SonarSystem`
- `TunnelNavigationRuntime`
- `HelmNavigationDisplay`

## Step 11

Enable debug options during validation:

- `TunnelNavigationRuntime.bEnableDebugDraw = true`
- `HelmNavigationDisplay.bEnableDebugLogs = true`

## Step 12

Run PIE.

Take the helm.

Verify in this order:

- sonar bound
- reconstruction bound
- tactical bound
- no `UNBOUND`
- no `NO RUNTIME DATA`

---

## 12. Gameplay reading model

### 12.1 Sonar screen

Use it for:

- raw topological perception
- moving echoes
- rough immediate awareness

Do not treat it as a structured classified tactical map.

### 12.2 Reconstruction view

Use it for:

- immediate clearance reading
- up/down safety
- drift reading
- forward route anticipation

This is a derived instrument.
It should feel like a machine reconstruction, not a perfect truth panel.

### 12.3 Tactical graph

Use it for:

- local route awareness
- branch/hub understanding
- future interpreted echo placement

At this stage it is still partially fed from current sonar tracks.
That is transitional.

---

## 13. PIE validation order for full Helm gameplay

### Gate 1 - Binding

Expected:

- all 3 widgets present
- no `UNBOUND`
- sonar works

### Gate 2 - Reconstruction

Expected:

- ownship marker in section
- safe up/down reacts
- forward marker visible
- reconstruction header updates

### Gate 3 - Tactical graph

Expected:

- graph visible
- graph reacts to progression
- not visually dead

### Gate 4 - Role separation

Expected:

- sonar = raw perception
- reconstruction = piloting aid
- tactical = structured route awareness

### Gate 5 - No cheat camera dependence

Expected:

- you can pilot with helm data, not by relying on external view

---

## 14. GO / NO-GO

## GO

If all are true:

- widgets bind explicitly
- sonar works again
- ReconstructionView is readable
- TacticalGraphView is readable
- no fallback hacks are required
- no old monolithic HelmNav widget remains

## NO-GO

If any are true:

- one widget only works through fallback discovery
- sonar breaks after widget replacement
- ReconstructionView says `NO RUNTIME DATA` in normal route traversal
- TacticalGraphView remains `UNBOUND`
- the layout works only through manual Blueprint graph patching

---

## 15. Immediate diagnosis for your current screenshot

Your current screenshot most likely means:

- `ReconstructionView` is bound, because it no longer says `UNBOUND`
- but `HelmNavigationDisplay` is not producing valid runtime navigation data
- `TacticalGraphView` is still not bound, which strongly suggests:
  - wrong widget instance name
  - or wrong parent class
  - or it is not actually the widget instance named `TacticalGraphView` inside `WBP_SubHelm`
- sonar likely broke for the same reason on the `SonarDisplay` side

So the first concrete fixes are:

1. In `WBP_SubHelm`, check the exact hierarchy instance names.
2. Make sure they are exactly:
   - `SonarDisplay`
   - `ReconstructionView`
   - `TacticalGraphView`
3. Keep fallback discovery disabled.
4. Re-run PIE.
5. If `ReconstructionView` still says `NO RUNTIME DATA`, inspect runtime components and logs, not widget binding.
