# Sub3D - PIE Validation Guide - ReconstructionView + TacticalGraphView

## 1. Purpose

This document is the canonical PIE validation guide for the new Helm UI architecture:

- `ReconstructionView`
- `TacticalGraphView`

It replaces the old monolithic HelmNav validation flow.

This is a gameplay validation document.
It is not a final art or polish document.

## 2. Scope

This validation covers:

- explicit widget binding
- runtime data flow from `UHelmNavigationDisplayComponent`
- reconstruction readability
- tactical graph usefulness
- separation between sonar raw view and secondary interpreted tools

This validation does not cover:

- final visual polish
- in-world monitor props
- unlock/crafting/module gameplay
- final threat-classification UX

## 3. Canonical Setup

### 3.1 Required widgets in `WBP_SubHelm`

The widget tree must contain explicit named widgets:

- `SonarDisplay`
- `ReconstructionView`
- `TacticalGraphView`

Do not rely on auto-create.
Keep `bAllowWidgetTreeFallbackDiscovery = false` unless debugging a broken bind path.

### 3.2 Required runtime components on the submarine

The current submarine instance must expose:

- `Sonar`
- `SonarSystem`
- `TunnelNavigationRuntime`
- `HelmNavigationDisplay`

### 3.3 Required map state

Use the shell map with:

- valid `TraversalRouteActor`
- valid spawned submarine
- valid helm station flow

## 4. Pre-PIE Checklist

Before pressing PIE, verify:

- old `HelmNavigationDisplay` widget is removed from `WBP_SubHelm`
- `ReconstructionView` has explicit size in the layout
- `TacticalGraphView` has explicit size in the layout
- `TunnelNavigationRuntime.bEnableDebugDraw` is enabled if you want world comparison
- `HelmNavigationDisplay.bEnableDebugLogs` is enabled for projection sanity checks

## 5. PIE Validation Order

### Gate 1 - Bind Stability

Expected:

- helm opens without crash
- `SonarDisplay` is visible
- `ReconstructionView` is visible
- `TacticalGraphView` is visible
- no `UNBOUND` state on the two new views

GO:

- all three widgets are bound immediately after taking the helm

NO-GO:

- one of the widgets stays `UNBOUND`
- one of the widgets only binds after fallback/scanning hacks

### Gate 2 - Reconstruction Header Sanity

Expected:

- header shows `RECON`
- `AGE` updates coherently
- `CONF` is non-zero in valid route traversal
- state reads `STABLE`, `STALE`, or `SUSPECT`

GO:

- header reacts to runtime state and is believable

NO-GO:

- header values stay frozen
- `CONF` is always zero
- state is misleading compared to actual runtime behavior

### Gate 3 - Cross-Section Reading

Test cases:

- centered in corridor
- drift left/right
- rise/fall in tunnel
- near-wall approach

Expected:

- ownship marker moves in the section
- heading and velocity vectors diverge when drift exists
- `safe above / safe below / safe left / safe right` react coherently

GO:

- the section helps pilot centering and vertical control

NO-GO:

- values change but ownship marker does not
- section is visually static while the submarine clearly moves relative to the tunnel

### Gate 4 - Forward Anticipation Reading

Test cases:

- straight corridor
- soft turn
- sharp turn
- narrowing
- large open zone / transition

Expected:

- ownship origin marker is visible on the left side
- ahead profile changes before the obstacle/turn is visually obvious
- stop line is understandable
- turn trend is readable

GO:

- the panel helps anticipate motion, not just report raw numbers

NO-GO:

- profile appears almost static while traversing meaningful geometry
- no readable relation between corridor shape and displayed profile

### Gate 5 - Reconstruction Interaction

Expected:

- hold + drag changes blend between section-dominant and forward-dominant reading
- interaction is stable
- no bind loss, no redraw glitch, no collapse into invalid state

GO:

- interaction changes emphasis without breaking readability

NO-GO:

- drag does nothing
- drag breaks the panel state
- the view becomes less interpretable than before interaction

### Gate 6 - Tactical Graph Reading

Expected:

- current route context is readable
- local graph reacts to progression
- hubs/splits/merges are distinguishable when present
- sonar-derived overlay is useful but remains secondary to route structure

GO:

- tactical graph improves local route awareness

NO-GO:

- graph stays visually static over meaningful progression
- sonar overlay overwhelms route readability

### Gate 7 - Role Separation

Expected:

- `SonarDisplay` remains the raw live screen
- `ReconstructionView` is clearly interpreted and secondary
- `TacticalGraphView` behaves as a structured aid, not a sonar duplicate

GO:

- each screen has a clear gameplay role

NO-GO:

- player can no longer tell what belongs to sonar vs interpreted navigation

## 6. GO / NO-GO for This Phase

Global GO if:

- explicit binding is stable
- ReconstructionView is useful for centering and anticipation
- TacticalGraphView improves local awareness
- no external camera is required for normal piloting
- sonar remains the raw primary perception tool

Global NO-GO if:

- one panel is mostly decorative
- one panel lies about geometry or motion
- sonar and navigation views duplicate each other confusingly
- routing still depends on legacy widget behavior

## 7. Debug Notes to Record

For each failed gate, record:

- exact gate number
- reproduction steps
- widget affected
- whether the numbers changed
- whether the drawing changed
- whether the issue is likely:
  - data issue
  - binding issue
  - rendering issue
  - interpretation issue

## 8. Next Step After GO

Only after this document passes:

- refine the design language of both widgets
- move toward reusable instrument windows for other stations
- design the future scan-to-tactical-map workflow
