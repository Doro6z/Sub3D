# Sub3D Editor Completion Execution Contract

## Purpose

This document defines the execution path from the current state to a complete submarine authoring editor implementation.

It replaces ad hoc editor setup, manual fine tuning of `USub3DSubmarineAuthoringAsset`, and partial validation loops.

The target is a single durable workflow:

`USub3DSubmarineAuthoringAsset -> Submarine Editor toolkit -> persistent compiled assets -> spawned runtime actor`

This contract is subordinate to the Plan Directeur:

- `C:\ACC\Projects\Sub3D\ProtoEditorV3\Sub3D_Plan_Directeur_Unique_Final_Clean.md`

If any statement here conflicts with the Plan Directeur, the Plan Directeur is the source of truth.

## Current Decision

From now until completion:

- continue implementing the planned editor and toolchain
- do not spend time on fine manual tuning inside raw asset properties
- do not treat intermediate geometry oddities as a tuning task
- do not run broad validation/debug passes after each small step
- keep focus on completing the authoring path first

Build verification after meaningful code changes remains allowed and required.

Large debug and validation passes are deferred to the end.

## Working Rules

### 1. Single authoring path

The only intended path is:

1. select a `USub3DSubmarineAuthoringAsset`
2. open the `Submarine Editor` toolkit
3. edit through the toolkit
4. bake through the toolkit
5. spawn or update the runtime actor through the toolkit

No parallel workflow should be introduced that depends on:

- direct actor details panel usage as the main path
- manual subsystem calls from utility blueprints as the main path
- temporary transient assets
- manual runtime actor setup disconnected from the authoring asset

### 2. Authoring asset policy

`USub3DSubmarineAuthoringAsset` is the source data container, but it is not the final user experience.

Until the dedicated phase UIs are in place:

- the asset may remain broad and low-level
- manual data entry is accepted only to unblock implementation
- manual data entry is not a completion target

Completion means the toolkit guides the authoring flow and exposes the right properties in the right phase.

### 3. Validation policy

Validation is split into two periods:

- now: compile safety and minimal runtime/editor continuity
- end: full validation, bake review, runtime review, and gap closing

This means:

- no time should be spent now on authoring perfect sample submarines
- no time should be spent now on repeated manual geometry tweaking
- no time should be spent now on broad automation cleanup unless a failing build blocks progress

### 4. Smoke asset policy

There is no need to invest in a polished smoke asset now.

If a minimal authoring asset is needed to continue implementation, it should remain:

- temporary
- simple
- structurally valid enough to exercise the pipeline
- not treated as production content

## Execution Order To Completion

## Phase 1. Lock the editor entry flow

Goal:

Make the toolkit the obvious and durable entry point for the submarine authoring workflow.

Required outcome:

- user selects an authoring asset
- user opens the toolkit
- user edits the asset in the toolkit
- user bakes from the toolkit
- user spawns or updates the runtime actor from the toolkit

Implementation focus:

- toolkit wiring
- persistent asset wiring
- editor actor resolution and reuse
- removal of redundant editor actions
- clearer labels and clear phase ownership

This phase is complete when the workflow is understandable without using raw subsystem calls.

## Phase 2. Replace generic raw editing with phase-oriented authoring panels

Goal:

Stop forcing the user to browse the whole `USub3DSubmarineAuthoringAsset` as one generic property list.

Required outcome:

- `Pressure Hull` editing is presented as a dedicated phase
- `Control Rings` and `Frame-Rings` are presented as a dedicated phase
- `Outer Envelope` is presented as a dedicated phase
- `Structural Bays` are presented as a dedicated phase
- `Deck Levels` and `Floor Regions` are presented as a dedicated phase
- `Openings`, `Connectors`, and `Closures` are presented as a dedicated phase
- `Pressure Bulkheads` and `Internal Walls` are presented as a dedicated phase

Implementation focus:

- per-phase details views or custom Slate panels
- stable grouping
- explicit names
- no hidden fallback logic
- no duplicate editing surfaces

This phase is complete when the toolkit structure matches the canonical authoring phases from the plan.

## Phase 3. Put the contracts in the UI

Goal:

Prevent invalid authoring by making the UI respect the data contracts instead of relying on late failure.

Required outcome:

- canonical vocabulary is used in labels and sections
- invalid ordering is harder to produce
- important constraints are visible at author time
- actions fail clearly when required data is absent or incoherent

Implementation focus:

- field ordering and grouping
- informative phase guidance
- warnings and errors in the toolkit
- removal of ambiguous actions
- explicit handling of missing or invalid prerequisites

Examples of contracts to expose clearly:

- `Control Rings` must be strictly increasing by `PositionX`
- `Frame-Rings` intended to split `Structural Bays` must be marked as bay boundaries
- `Outer Envelope` is a secondary baked volume, not a final raccord surface
- `Derived Flood Volume` is baked runtime data, not author-entered geometry

This phase is complete when common invalid configurations are caught or discouraged in the editor flow.

## Phase 4. Add phase-appropriate editor feedback

Goal:

Give the toolkit enough visual feedback to support authoring decisions without treating final debugging as ongoing work.

Required outcome:

- `Structural Bays` can be drawn from the toolkit
- bake outputs are understandable from the toolkit
- the user can tell what the toolkit produced without browsing internal assets blindly

Implementation focus:

- targeted debug draw actions
- clear status text after validate and bake
- direct links or visibility of created compiled assets where useful
- clear distinction between authored data and baked data

This phase is complete when the toolkit helps the user understand structural outputs without resorting to raw internal inspection.

## Phase 5. Finish the runtime handoff path

Goal:

Make the editor output reliably consumable as a spawned runtime submarine actor.

Required outcome:

- the spawned runtime actor is created from compiled runtime data only
- the runtime actor is updated by the toolkit path
- the runtime actor is not using authoring data directly at runtime
- the actor can be used for later gameplay validation

Implementation focus:

- runtime actor update path
- asset persistence
- component rebuild behavior
- collision/runtime system initialization

This phase is complete when the editor produces a runtime actor through one stable path.

## Phase 6. Final validation pass

Goal:

Run the broad validation only once the implementation path is materially complete.

Required outcome:

- compile clean enough for the current branch
- automation pass reviewed
- bake path reviewed with representative authoring assets
- runtime spawn path reviewed in editor and PIE
- remaining gaps recorded precisely

Implementation focus:

- global compile
- automation execution
- toolkit flow review
- bake determinism and structural output review
- final editor UX cleanup

This phase is where the large debug pass belongs.

## What Not To Do Before Phase 6

Do not spend time on:

- polishing a hand-authored submarine asset
- correcting every geometric imperfection by tuning values manually
- treating the current sample asset as production content
- broad automation cleanup that does not block implementation
- repeated manual subsystem invocation outside the intended toolkit path
- parallel editor workflows that compete with the toolkit

## What Counts As Completion

The editor side is complete enough for the plan when all of the following are true:

- the toolkit is the normal authoring entry point
- the toolkit exposes the authoring phases in a controlled way
- the toolkit can validate, bake, and spawn the submarine through a single path
- the runtime actor is produced from compiled assets, not authoring assets
- the user no longer needs to manually assemble the flow from raw properties, subsystem calls, and loose actors

The content side is not required for this completion.

Examples of content that remain external to the generator:

- textures
- materials
- unique station meshes
- machinery meshes
- propeller meshes
- storage meshes
- other non-generated authored assets

The editor is expected to generate the structural submarine data and the structural runtime actor setup.

It is not expected to generate every unique art asset.

## Immediate Next Step

The next work should stay on the editor/tooling side.

Priority:

1. continue turning the toolkit tabs into real authoring panels instead of generic placeholders
2. keep the toolkit as the only intended execution path
3. postpone broad validation and sample asset cleanup until the end

## Operator Summary

From this point:

- implement the editor workflow first
- do not tune submarine content manually unless required to unblock code work
- do not branch into final debugging now
- do not treat current generated geometry as final content quality
- finish the authoring path, then run the global validation pass
