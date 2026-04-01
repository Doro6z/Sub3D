# Sub3D Tunnel Navigation Phase 2

## Editor Validation Handoff

Date: 2026-03-30

Scope of this handoff:
- validate the runtime query layer only
- do not build final helm UI
- do not validate against marching-cubes triangles as logic source
- validate on the real submarine actor in PIE

Runtime truth:
- `ATraversalRouteActor` remains route authority
- `UTunnelNavDataAsset` remains a derived sidecar
- `UTunnelNavigationRuntimeComponent` is the helm-navigation query layer

## Implemented runtime layer

The component now exposes:
- projection onto route
- local cross-section query
- forward anticipation profile
- local graph window
- class-aware restriction aggregation
- stop-distance warning
- commitment / no-turn warning
- heading vs velocity drift state
- runtime obstacle overlay
- debug draw world overlays
- `CallInEditor` logs:
  - `LogCurrentProjection`
  - `LogCurrentRestrictions`

Component path:
- `Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.h`
- `Source/Sub3D/Submarine/TunnelNavigationRuntimeComponent.cpp`

Attached on submarine:
- `ASubmarineBase::TunnelNavigationRuntime`

## Current known build state

The nav runtime patch compiles through UHT and native compile scheduling.

The full editor build is still blocked by unrelated existing `SubCompiler` errors:
- duplicate `AddValidationMessage`
- files involved:
  - `SubmarineFunctionalGraph.cpp`
  - `SubmarineBuildCompiler.cpp`
  - `SubmarineLayoutSolver.cpp`

This is outside TunnelNav scope.

## Prerequisites in the editor

Open the playable shell map used for route traversal tests.

Required actors:
- one `ATraversalRouteActor` already built and valid
- one submarine actor using `ASubmarineBase` or `BP_Submarine_Compiler`

Required sidecar condition:
- the route must have `bBuildTunnelNavData = true`
- the route must have been rebuilt so `GeneratedTunnelNavData` exists

Submarine component settings:
- select the submarine actor
- open `TunnelNavigationRuntime`
- set:
  - `ActiveSubClass` to the class under test
  - `bEnableDebugDraw = true`
  - `DebugLookaheadCm = 10000` to start
  - `ProjectionSearchRadiusCm = 25000`
  - `ProjectionCacheSampleWindow = 18`

Optional for logs:
- set `bEnableDebugLogs = true`
- use `LogCurrentProjection`
- use `LogCurrentRestrictions`

## What debug draw means

World debug overlays currently produced:
- yellow sphere: projected route sample / closest route center sample
- magenta line: submarine world location to projected route sample
- green/cyan axes: local cross-section clearances
- white sphere in section: projected sub position inside local cross-section
- blue line chain: forward lookahead samples considered safe
- red line chain: restricted lookahead samples
- silver line chain: cavern / hub-like section in lookahead
- white line from sub: heading
- orange line from sub: real velocity
- purple ghost spheres: projected drift / inertia ghosts
- orange obstacle spheres: runtime observed obstacles

## Validation sequence

Run the tests in this exact order.

### Test 1 - Projection stability

Goal:
- confirm route projection stays coherent in real traversal

Procedure:
1. Place the submarine in a straight corridor section.
2. Call `LogCurrentProjection`.
3. Start PIE.
4. Move slowly forward.
5. Repeat in:
   - before a bend
   - inside a bend
   - near a narrowing
   - near a split / branch approach
   - near a hub-like or large cavity zone

Check:
- `SampleIndex` advances smoothly
- `EdgeIndex` only changes when expected
- `RouteDistanceCm` is monotonic while advancing
- projection does not jump between unrelated edges
- `SpaceMode` is plausible:
  - corridor in narrow transit
  - transition in widening pockets
  - cavern hub in large cavity / hub-like spaces

Reject if:
- projection oscillates between edges while the submarine is stable
- route distance jumps backward/forward without movement cause

### Test 2 - Local cross-section fidelity

Goal:
- confirm the section reflects actual piloting margins

Procedure:
1. Keep debug draw enabled.
2. In PIE, drift the submarine toward the right wall.
3. Repeat toward the left wall.
4. Repeat with vertical offset if your route supports climb/descent.

Check:
- white sub marker inside section shifts in the correct direction
- local margin shrinks on the side approached
- `bNearWallWarning` behavior matches visual proximity
- `bHardClearanceViolation` only appears when truly too close

Use:
- `LogCurrentProjection`
- `LogCurrentRestrictions`

Reject if:
- lateral drift is inverted
- clearance values stay flat while approaching a wall

### Test 3 - Heading vs velocity drift

Goal:
- validate one of the core gameplay pillars: inertia and drift are visible

Procedure:
1. Enter helm.
2. Build forward speed.
3. Apply heading correction or lateral drift situation.
4. Observe white heading line vs orange velocity line.

Check:
- heading line follows hull orientation
- velocity line follows actual movement
- drift angle increases in gliding turns
- ghost markers move ahead in the actual travel direction

Reject if:
- heading and velocity remain visually identical under drift
- drift angle stays near zero during obvious lateral slide

### Test 4 - Forward anticipation

Goal:
- confirm lookahead follows route progression, not naive world straight-line logic

Procedure:
1. Test in:
   - straight corridor
   - gentle curve
   - sharp curve
   - narrowing
   - approach to large cavity / hub
2. Drive slowly, then faster.

Check:
- lookahead line follows tunnel progression
- approaching a bend moves the profile along the bend, not through rock
- red segments appear before the physical danger becomes immediate
- `Stop / FirstCritical` debug text becomes tighter as you approach restriction

Reject if:
- anticipation behaves like a single world-space ray
- line leaks through topology shortcuts

### Test 5 - Class-aware restrictions

Goal:
- prove the system is materially different across sub classes

Procedure:
1. Stop PIE.
2. Set `ActiveSubClass = ClassS`.
3. Repeat tests 1-4 in one tight section.
4. Repeat with `ClassM`.
5. Repeat with `ClassL` or `ClassXL`.

Check:
- warnings appear earlier for larger classes
- `CommitmentNoTurn` risk appears sooner for larger classes
- recommended stop margin is harsher for larger classes
- same corridor may be acceptable for `ClassS` and risky for `ClassL`

Reject if:
- class changes barely affect output

## Runtime obstacle overlay test

Goal:
- verify static mapped knowledge vs live runtime overlay distinction

Procedure:
1. In PIE, call `AddRuntimeObservedObstacle(...)` from BP or temporary debug script.
2. Place it ahead on the current corridor.

Check:
- orange obstacle sphere appears
- lookahead turns red where the obstacle blocks the corridor envelope
- restrictions log contains `RuntimeObstacle`

## Suggested debug workflow

For each test point:
1. stop movement
2. call `LogCurrentProjection`
3. call `LogCurrentRestrictions`
4. capture screenshot
5. note:
   - map section
   - active sub class
   - expected result
   - observed result
   - pass / fail

## GO / NO-GO gate

GO to next phase only if:
- projection is stable in corridors and acceptable in hubs
- cross-section tracks real local margin changes
- heading vs velocity drift is clearly distinct
- anticipation follows route progression
- class-aware restrictions are materially different

NO-GO if any of these fail:
- projection jumps unpredictably
- cross-section does not track piloting offset
- drift is unreadable
- anticipation behaves like world-space raycast
- large/small class outputs are too similar

## Next phase after validation

If this pass is green:
- build the debug helm panels only
- still no final polish
- target the three panels:
  - Front Cross-Section
  - Forward Anticipation
  - Tactical Graph

If this pass is not green:
- fix runtime query correctness first
- do not move to helm UI
