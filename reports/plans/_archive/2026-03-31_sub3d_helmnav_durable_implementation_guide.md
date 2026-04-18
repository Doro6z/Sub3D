# Sub3D — Helm Navigation Durable Implementation Guide

## 1. Purpose

This document is the durable implementation and editor setup guide for the **Helm Navigation Suite**.

It replaces any "auto-create" or temporary hookup workflow.

The canonical goal is:

- keep the **sonar main screen frozen**
- add a **separate Helm Navigation layer**
- bind it durably through widget assets placed explicitly in the editor
- validate the three debug panels in PIE before any final UI polish

This document is intentionally practical.

---

## 2. Runtime Context

### 2.1 What is already locked

The sonar main screen is considered functionally frozen for this phase.

It remains responsible for:

- global tactical topology
- active ping / hold ping
- passive tracks
- acoustic context

It must **not** absorb:

- clearance guidance
- centering assistance
- front cross-section
- stop-distance navigation help

### 2.2 What HelmNav is

HelmNav is a separate navigation layer built as a **diegetic reconstruction** from submarine-captured data.

It is not a debug omniscient truth.

It is built on:

- `UTunnelNavigationRuntimeComponent`
- `UTunnelNavDataAsset`
- submarine transform / velocity
- route-derived query results

### 2.3 What HelmNav must provide now

Three debug-but-durable panels:

- `A — Front Cross-Section`
- `B — Forward Anticipation`
- `C — Tactical Graph`

These are not final art/UI screens.
They are gameplay validation instruments.

---

## 3. Objectives

### 3.1 Primary objectives

- Provide a **separate HelmNav display path**, independent from the sonar CRT.
- Expose **UI-ready data** from the TunnelNav runtime query layer.
- Make the panels usable in PIE without external cheat camera.
- Keep the implementation durable and explicit in assets/Blueprints.
- Keep sonar and HelmNav responsibilities cleanly separated.

### 3.2 Secondary objectives

- Make widget ownership and binding obvious in the editor.
- Avoid runtime widget spawning tricks for canonical usage.
- Allow later visual redesign without changing runtime query logic.

---

## 4. Non-objectives

This phase does **not** try to do the following:

- no final polished UI
- no diegetic props/meshes/screens in-world
- no submarine class differentiation
- no upgrade/module gameplay
- no sonar redesign
- no route generator redesign
- no dependency on marching-cubes mesh triangles for HelmNav logic
- no replacement of the sonar tactical screen

---

## 5. Canonical Architecture

## 5.1 Source of truth

### Route authority

`ATraversalRouteActor` remains the route authority.

It owns:

- route build
- validation
- transforms
- sidecar generation

### Navigation data source

`UTunnelNavDataAsset` remains read-only runtime input.

### Runtime query layer

`UTunnelNavigationRuntimeComponent` remains the place where route projection and navigation queries are computed.

It already exposes:

- projection
- local cross-section
- forward anticipation
- graph window
- heading vs velocity
- stop warning
- commitment warning

## 5.2 New HelmNav layer

### Adapter

`UHelmNavigationDisplayComponent`

Responsibility:

- read TunnelNav runtime queries
- transform them into UI-ready data
- cache the current panel payloads
- stay off the sonar path

### Widget

`UHelmNavigationDisplayWidget`

Responsibility:

- draw the three debug panels
- read already-prepared data
- avoid recomputing route logic

### Existing Helm widget

`USubHelmWidget`

Responsibility:

- bind the sonar display
- bind the HelmNav display
- route station-level interactions

---

## 6. Current Code Shape

## 6.1 New code introduced

### `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.h`

Defines:

- `FHelmCrossSectionViewData`
- `FHelmForwardAnticipationSampleViewData`
- `FHelmForwardAnticipationViewData`
- `FHelmTacticalGraphNodeViewData`
- `FHelmTacticalGraphEdgeViewData`
- `FHelmTacticalGraphViewData`
- `UHelmNavigationDisplayComponent`

### `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.cpp`

Implements:

- runtime binding to `UTunnelNavigationRuntimeComponent`
- refresh/caching
- transformation from TunnelNav structs into UI-ready data

### `Source/Sub3D/Submarine/HelmNavigationDisplayWidget.h`

Defines:

- `UHelmNavigationDisplayWidget`

### `Source/Sub3D/Submarine/HelmNavigationDisplayWidget.cpp`

Implements:

- native debug painting for the 3 panels
- panel framing
- cross-section drawing
- anticipation strip drawing
- tactical graph drawing

## 6.2 Existing files updated

### `Source/Sub3D/Submarine/SubmarineBase.h`

Adds:

- `UHelmNavigationDisplayComponent* HelmNavigationDisplay`

### `Source/Sub3D/Submarine/SubmarineBase.cpp`

Creates:

- `HelmNavigationDisplay = CreateDefaultSubobject<UHelmNavigationDisplayComponent>(TEXT("HelmNavigationDisplay"));`

### `Source/Sub3D/Submarine/SubHelmWidget.h`

Adds:

- `HelmNavigationDisplay` widget reference
- `IsHelmNavigationDisplayBound()`
- editor-facing properties for optional lookup/autocreate path

### `Source/Sub3D/Submarine/SubHelmWidget.cpp`

Adds:

- `TryBindHelmNavigationDisplay()`
- runtime binding to the submarine display component

---

## 7. Durable Editor Workflow

## 7.1 Canonical rule

**Do not use auto-create for HelmNav.**

The canonical path is explicit widget placement in assets.

### Why

Likely root cause of the crash you saw:

- the auto-create path instantiates a widget at runtime and adds it directly to viewport
- this competes with the durable widget tree ownership of the helm widget
- if the helm widget is reconstructed/destructed while the auto-created widget still exists, lifetime/order bugs become much easier

Possible contributor:

- multiple helm enter/exit cycles and viewport ownership conflicts

Deferred concern:

- the auto-create path still exists in code for debugging symmetry with sonar, but it is **not the recommended production path**

## 7.2 Canonical asset creation flow

### Step 1 — Create the dedicated HelmNav display widget asset

Create a Blueprint Widget:

- name: `WBP_HelmNavigationDisplay`
- parent class: `UHelmNavigationDisplayWidget`

This Blueprint is the durable asset used by the helm station.

You do **not** create it dynamically from code.

Important:

- the Blueprint can stay visually empty
- the rendering comes from native paint in `UHelmNavigationDisplayWidget`
- do not add gameplay logic inside this widget Blueprint

### Step 2 — Open the main helm widget

Open:

- `WBP_SubHelm`

Its native parent is `USubHelmWidget`.

### Step 3 — Place the HelmNav display widget explicitly

Inside `WBP_SubHelm`, place:

- the existing sonar display widget
- the new `WBP_HelmNavigationDisplay`

Recommended layout:

- sonar tactical screen on one side or upper area
- HelmNav display on a dedicated region, large enough to read 3 debug panels

Critical sizing rule:

- `WBP_HelmNavigationDisplay` must receive an explicit size from layout
- if you place it in a `CanvasPanel`, give it explicit slot size
- if you place it in an `Overlay`, `HorizontalBox`, or `VerticalBox`, wrap it in a `SizeBox`

Recommended initial size:

- width: `900`
- height: `300`

Reason:

- this widget draws through native paint
- it has no internal child content driving desired size
- without explicit size, it may collapse to zero and appear invisible

Important:

- only place **one** `UHelmNavigationDisplayWidget`-derived widget in the tree
- only place **one** sonar display widget in the tree

Reason:

`USubHelmWidget` finds the first matching widget of each type in the widget tree.

### Step 4 — Keep auto-create disabled

On the root `USubHelmWidget` instance / BP defaults:

- `bAutoCreateHelmNavigationDisplayIfMissing = false`
- `bAutoCreateSonarDisplayIfMissing = false` if you are also using a durable sonar widget asset

### Step 5 — Compile and save the widget assets

Compile:

- `WBP_HelmNavigationDisplay`
- `WBP_SubHelm`

Save both.

Expected editor behavior:

- in designer preview, the widget may look empty or only show placeholder state
- the useful content appears once it is bound at runtime to `UHelmNavigationDisplayComponent`

---

## 8. Durable Submarine Setup

## 8.1 Submarine component presence

On `BP_Submarine_Compiler`, verify these components exist:

- `Sonar`
- `SonarSystem`
- `TunnelNavigationRuntime`
- `HelmNavigationDisplay`

`HelmNavigationDisplay` is created natively on `ASubmarineBase`.

## 8.2 Recommended defaults for HelmNav

In `HelmNavigationDisplay`:

- `RefreshPeriodS = 0.08`
- `ForwardLookaheadCm = 12000`
- `TacticalGraphRadiusCm = 18000`
- `bAutoBindTunnelNavigationRuntime = true`

### CanonicalNavigationProfile

For now, this is a single canonical profile.

Recommended initial values:

- `HullLengthCm = 2700`
- `HullBeamCm = 450`
- `HullHeightCm = 450`
- `HardClearanceCm = 2500`
- `PreferredClearanceCm = 3000`
- `MinTurningBasinDiameterCm = 8400`
- `ServiceDecelerationCmS2 = 160`
- `EmergencyDecelerationCmS2 = 260`
- `CommitmentLookaheadCm = 12000`
- `DriftWarningAngleDeg = 18`
- `LateralSpeedWarningCmS = 90`

Do not introduce class-based variants in this phase.

---

## 9. Panel Intent

## 9.1 Panel A — Front Cross-Section

Purpose:

- immediate left/right/up/down reading
- immediate `safe above / safe down`
- immediate lateral/vertical offset
- heading vs velocity drift cue

Interpretation rule:

- this is a reconstructed instrument slice, not a literal real-time mesh camera

## 9.2 Panel B — Forward Anticipation

Purpose:

- read upcoming narrowing
- read ceiling/floor envelope ahead
- read stop distance against what is ahead
- read commitment / no-turn pressure

Interpretation rule:

- this follows route progression, not a naive world raycast

## 9.3 Panel C — Tactical Graph

Purpose:

- local macro understanding
- current edge awareness
- next hub / split / merge awareness

Interpretation rule:

- this is not the sonar map
- this is a structural route awareness view

---

## 10. File-by-file Impact Summary

## 10.1 Runtime query source files

### `Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.h/.cpp`

Role:

- unchanged as the route-query core
- remains the place for projection and route-aligned logic

Impact in this phase:

- consumed, not rewritten

## 10.2 New HelmNav adapter

### `Source/Sub3D/Submarine/HelmNavigationDisplayComponent.h/.cpp`

Role:

- converts TunnelNav runtime outputs into UI-ready payloads

Contains:

- cross-section payload
- anticipation payload
- tactical graph payload
- cached warnings

## 10.3 New HelmNav widget

### `Source/Sub3D/Submarine/HelmNavigationDisplayWidget.h/.cpp`

Role:

- temporary durable debug renderer
- no gameplay logic
- no route queries

## 10.4 Submarine owner

### `Source/Sub3D/Submarine/SubmarineBase.h/.cpp`

Role:

- owns the new display component

Impact:

- one new component added
- no sonar behavior change

## 10.5 Helm station widget

### `Source/Sub3D/Submarine/SubHelmWidget.h/.cpp`

Role:

- binds both sonar and HelmNav widgets

Impact:

- new `TryBindHelmNavigationDisplay()`
- new editor-visible reference for the durable HelmNav widget

---

## 11. Implementation Order

The correct order is:

### Phase A — Runtime stable source

Already done:

- `UTunnelNavigationRuntimeComponent`

### Phase B — UI-ready transformation layer

Already done:

- `UHelmNavigationDisplayComponent`

### Phase C — Debug widget

Already done:

- `UHelmNavigationDisplayWidget`

### Phase D — Durable asset hookup

To do in editor:

- create `WBP_HelmNavigationDisplay`
- place it explicitly in `WBP_SubHelm`
- keep auto-create disabled

### Phase E — PIE validation

To do next:

- validate all 3 panels on the real traversal shell

---

## 12. GO / NO-GO Criteria

## 12.1 GO

This phase is GO only if:

- `WBP_HelmNavigationDisplay` is explicitly placed in `WBP_SubHelm`
- no auto-create path is used
- entering helm does not crash
- all 3 panels render in PIE
- cross-section gives usable `up/down/left/right`
- `safe above / safe down` are readable without external camera
- anticipation helps read narrowing and stop distance
- tactical graph helps identify local route structure
- sonar remains untouched in its role

## 12.2 NO-GO

This phase is NO-GO if:

- HelmNav depends on viewport auto-create
- entering/exiting helm creates duplicate widgets
- panel data is empty while `TunnelNavigationRuntime` has valid projection
- the widget is placed without explicit size and therefore never paints visibly
- panel logic starts re-querying route geometry inside the widget
- sonar begins to absorb HelmNav responsibilities
- the player still needs out-of-body camera to read clearance reliably

---

## 13. PIE Validation Checklist

Test in this order:

### 1. Bind stability

- enter helm
- verify sonar widget is still present
- verify HelmNav widget is present
- exit helm
- re-enter helm
- confirm no duplicate widget, no crash

### 2. Cross-section usefulness

- center in corridor
- move up/down
- drift left/right
- confirm `safe above / safe down` reacts coherently

### 3. Forward anticipation usefulness

- run straight corridor
- approach narrowing
- approach bend
- confirm stop marker and critical marker remain readable

### 4. Tactical graph usefulness

- enter wider zone / branch approach
- confirm current edge and next structure awareness are readable

### 5. Product sanity

- confirm sonar tactical screen still feels like sonar
- confirm HelmNav feels like a navigation instrument

---

## 14. Explicit Recommendation

For this project phase:

- use **durable widget assets**
- keep **auto-create off**
- treat `UHelmNavigationDisplayWidget` as the canonical debug surface
- validate in PIE before any diégèse/polish pass

The next clean step after this document is not more architecture.

It is:

- create `WBP_HelmNavigationDisplay`
- place it in `WBP_SubHelm`
- run the 5 validation blocks above
