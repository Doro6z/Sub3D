# Proto04D / Proto04E - Next Session Note

Date : 2026-03-28  
Status : session handoff / direction note

## Decision

Proto04E can continue in parallel only for compiler-side shape work that does not move runtime contracts again:

- E.1 sections transversales
- E.2 locks
- E.3 bow/stern profiles

Proto04E should pause before deeper editor/runtime coupling work until Proto04D stabilizes the interior hydraulic contract:

- hold E.4 partial rebuild if it starts depending on runtime-facing compartment payloads
- hold E.5 drydock/editor UX until the interior hydro volume and leak anchoring contracts are fixed

Reason:

- Proto04D currently lacks a stable interior hydraulic volume distinct from the structural/envelope hull
- Proto04E increases the divergence between rendered hull skin and runtime breach anchoring
- pushing more editor/runtime work before fixing those contracts would multiply rework

## Current Problems Observed In PIE

1. Flood water starts from the structural/envelope compartment bounds instead of the true interior habitable floor.
2. D.7 immersion inherits the same bad vertical reference.
3. Leak VFX can drift outside the visible hull because runtime breach anchors do not yet share a stable visual surface contract with Proto04E geometry.
4. Default UE swimming is not a good fit for interior FPS traversal.
5. Pressure effects exist, but there is no real death baseline yet.
6. Environment debug logs are too chatty when enabled.

## Immediate Direction

### Pass 1 - Interior HydroVolume

Goal:

- compile a true interior hydraulic volume per compartment
- use it as the single source for:
  - flood water min/max height
  - player immersion
  - future flood audio / slosh anchors

Implementation direction:

- compiler writes compartment hydro bounds from solved floor/width/clearance
- runtime prefers hydro bounds over structural sheet bounds
- flood visuals and D.7 read those hydro bounds

Status:

- started in current session

### Pass 2 - Custom Swim

Goal:

- replace stock swimming feel with controlled interior swim traversal

Direction:

- use custom movement mode instead of raw `MOVE_Swimming`
- add explicit input:
  - `SwimUp`
  - `SwimDown`
- forward movement aligned with camera
- stronger damping
- sprint pass and walk-speed retune bundled here

### Pass 3 - Death Baseline

Goal:

- add a real death state for pressure / drowning / severe environment failure

Phase split:

1. baseline death
- lose control
- camera detach with collision-safe retreat
- ragdoll

2. spectacle later
- body breakup / gibs / explosion

### Pass 4 - Surface Patch Destructible / Leak Sync

Goal:

- align runtime breaches, leak VFX and future visual damage with the visible compiled hull skin

Direction retained:

- do not use the raw runtime mesh as gameplay truth
- do not base gameplay on booleans over the rendered mesh
- instead, compiler emits stable surface patch data shared by:
  - runtime integrity
  - leak anchoring
  - future decals / visual tearing

This is the correct bridge point between Proto04D and Proto04E.

## Logging Direction

Introduce a dedicated environment log category later:

- `LogSubCrewEnvironment`

Logging should be event-oriented or throttled:

- compartment change
- walk/wade/swim state change
- pressure danger entered / exited
- pressure damage applied

Never spam every tick by default.

## Recommended Next Execution Order

1. finish HydroVolume propagation and validate in PIE
2. custom swim pass
3. death baseline
4. surface patch / leak sync contract
5. then resume deeper Proto04E editor/runtime coupling
