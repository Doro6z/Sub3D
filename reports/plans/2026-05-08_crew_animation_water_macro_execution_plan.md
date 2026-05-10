# Crew animation, debug, and water locomotion macro execution plan

Date: 2026-05-08
Status: macro roadmap, current gate is Milestone 1 PIE validation

Authority-max plan:

- `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

Detailed plans:

- `reports/plans/2026-05-08_crew_animation_procedural_debug_plan.md`
- `reports/plans/2026-05-07_crew_water_locomotion_architecture.md`
- `reports/2026-05-04_crew_animation_audit.md`

Visual tracking diagrams:

- `reports/diagrams/2026-05-08_crew_anim_water_debug_system.svg`
- `reports/diagrams/2026-05-08_crew_anim_debug_ui_concept.svg`

## Purpose

This document is the low-cognitive-load roadmap.

Use it to answer:

- what is the next step;
- what must not be started yet;
- what counts as done;
- which detailed plan section to open only when needed.

## Main rule

Implement debug visibility before deep water locomotion changes.

Reason:

- The water locomotion work touches `EmbarkState`, native `MovementMode`, local/world reference frames, swim, IK, animation, and hull boundary handoff.
- Without the debug snapshot, failures look similar: falling, wrong swim state, stale flood state, wrong AnimBP wiring, or wrong frame.
- With the snapshot, each failure has a named warning and a concrete source.

## Current position - 2026-05-08

You are in Milestone 1, text debug core.

Already in code:

- `FCrewAnimDebugSnapshot` and stable warning ids.
- `UCrewAnimDebugComponent` as a crew-owned debug component.
- `CrewAnimDump` console command.
- Central debug settings for crew animation debug sampling.
- Base snapshot fields for movement, reference frame, water contact, anim swim state, IK weights, requested bone rotations, and applied bone transforms.

Not validated yet:

- PIE execution of `CrewAnimDump`.
- Whether every expected bone name resolves on the current crew skeletal mesh.
- Whether the warning list matches the actual falling/swimming cases.
- Whether the component tick order is late enough for the applied bone transform read to match the current frame.

Autonomous work before the next user test:

- Add missing Movement debug hooks for last hull handoff, last movement mode change, and swim fallback reason.
- Add AnimInstance debug hooks for walk/swim/breath phases and requested pose array access.
- Add procedural node health export: resolved bone count, missing expected bones, and pose snapshot copied flag.
- Write the editor validation handoff for `ABP_Crew`, the procedural anim node, IK variables, and expected mesh/ABP assignment.
- Keep the panel, 3D draw debug, Gameplay Debugger, CSV trace, and water locomotion resolver untouched until the text snapshot is validated.

User test gate:

- Open PIE.
- Possess the crew.
- Run `CrewAnimDump`.
- Cross a hull boundary or airlock.
- Confirm the dump reports `Embark=Outside`, native movement `Swimming`, `ReferenceFrame=WorldSpace`, and `Water=Ocean`.
- If the bug reproduces, confirm the warning is `OutsideNotSwimming` rather than a silent `Falling`.

## Current north star

One crew state snapshot feeds every validation surface:

```text
UCrewAnimDebugComponent
  -> CrewAnimDump
  -> Gameplay Debugger
  -> UMG debug panel
  -> 3D draw debug
  -> CSV trace
```

The snapshot must expose:

- `TraversalDomain`
- `ReferenceFrameState`
- raw `EmbarkState`
- raw native `MovementMode`
- `WaterContactState`
- `ImmersionSample`
- requested procedural pose
- applied skeletal pose
- IK computed / wired / applied status
- warnings

## Milestone 0 - Align documents

Status: current document pass.

Goal:

- Make the animation debug plan and water locomotion plan use the same vocabulary.

Do:

- Patch the debug plan with `TraversalDomain`, `ReferenceFrameState`, `WaterContactState`, `ImmersionSample`, requested/applied pose split.
- Cross-link this macro plan from the water locomotion plan.

Done when:

- The two detailed plans no longer describe separate debug vocabularies.
- `git diff --check` passes for edited docs.

## Milestone 1 - Text debug core

Detailed reference:

- `2026-05-08_crew_animation_procedural_debug_plan.md`, Phases A to E.

Goal:

- Get deterministic runtime truth before building UI.

Do:

- Add `CrewAnimDebugTypes`.
- Add `UCrewAnimDebugComponent`.
- Add `CrewAnimDump`.
- Add debug settings.
- Add movement/environment/anim requested pose snapshot fields.
- Add warnings.
- Add procedural node health.

Do not do yet:

- Do not build the UMG panel.
- Do not refactor water movement decisions.
- Do not migrate `CrewAnimDebugWidget`.

Done when:

- `CrewAnimDump` logs one complete local crew snapshot.
- Falling/swimming mismatches produce named warnings.
- Missing AnimInstance or missing procedural node does not crash.
- Debug disabled path does not change gameplay.

## Milestone 2 - Editor validation and diagnostic surfaces

Detailed reference:

- `2026-05-08_crew_animation_procedural_debug_plan.md`, Phases F to J.

Goal:

- Make the text snapshot visible and useful in PIE.

Do:

- Validate `ABP_Crew` wiring in editor.
- Record whether hand IK and foot IK are computed, wired, and applied.
- Add `UCrewAnimDebugPanelWidget`.
- Keep `UCrewAnimDebugWidget` as a temporary legacy tuner.
- Add 3D draw debug for frame axes, IK rays, and selected bones.
- Add CSV trace.
- Extend Gameplay Debugger with the compact anim block.

Do not do yet:

- Do not convert the legacy tuner during this pass.
- Do not implement final player HUD art.

Done when:

- PIE shows the same warning ids in dump, panel, and Gameplay Debugger.
- The system can distinguish requested pose from applied pose.
- A 10-second repro can be exported to CSV.

## Milestone 3 - Water contact contract

Detailed reference:

- `2026-05-07_crew_water_locomotion_architecture.md`, Phase B.

Goal:

- Separate water sampling from movement decisions.

Do:

- Add `ECrewWaterContactState`.
- Add `FCrewImmersionSample`.
- Add `UCrewWaterContactComponent` or equivalent local owner.
- Derive dry, shallow wade, deep wade, swimming, and ocean states.
- Feed the same state into the debug snapshot.

Do not do yet:

- Do not centralize every movement mode write until the samples are stable.
- Do not implement custom swim mode yet.

Done when:

- Inside flooded compartments remain `Embarked`.
- Outside ocean reports ocean contact.
- Debug shows water contact without reading raw compartment state in the panel.

## Milestone 4 - Locomotion resolver and handoff cleanup

Detailed reference:

- `2026-05-07_crew_water_locomotion_architecture.md`, Phase C.

Goal:

- Remove scattered movement mode decisions.

Do:

- Add one resolver for crew locomotion from frame + water + ladder + handoff state.
- Keep `Outside => Swimming` invariant.
- Keep inside water as submarine-local movement.
- Add `FCrewHullBoundaryHandoff`.
- Route outgoing/incoming velocity conversion through one helper.

Do not do yet:

- Do not introduce custom swim movement unless `MOVE_Swimming` remains unstable after the resolver.

Done when:

- Search shows one primary path for water movement mode decisions.
- Boundary handoff logs before/after local and world velocities.
- Debug warnings stop being the only thing keeping outside swim correct.

## Milestone 5 - Custom Sub3D swim movement

Detailed reference:

- `2026-05-07_crew_water_locomotion_architecture.md`, Phase E.

Goal:

- Remove dependency on Unreal water `PhysicsVolume`.

Do:

- Add `CMOVE_Sub3DSwim`.
- Implement `PhysCustom` swim path.
- Support `Embarked + Sub3DSwim`.
- Support `Outside + Sub3DSwim`.
- Keep grid rebase active for interior swimming.

Do not do yet:

- Do not tune final swim feel before debug traces are clean.

Done when:

- No outside swim case falls back to `MOVE_Falling`.
- Interior swim remains local to the submarine frame.
- Debug clearly reports why the crew is swimming.

## Milestone 6 - Player feedback and gameplay extensions

Detailed references:

- `2026-05-07_crew_water_locomotion_architecture.md`, HUD requirements.
- `2026-05-08_crew_animation_procedural_debug_plan.md`, future gameplay extensions.

Goal:

- Turn validated state into player-facing feedback and later gameplay.

Do:

- Minimal oxygen, pressure, and water contact HUD.
- Underwater PP/audio hooks.
- Water contact foam/ripples as visual polish.
- Creature attach/latch prototypes only after the crew state is stable.

Do not do yet:

- Do not build creature AI before crew reference-frame and water state are stable.
- Do not build final UI art before the debug values are trusted.

Done when:

- Player HUD uses validated state, not duplicate sampling.
- Debug panel and player-facing signals agree.

## Session checklist

At the start of a work session:

1. Pick exactly one milestone.
2. Open only the detailed plan sections listed for that milestone.
3. Define one runtime scenario to validate.
4. Make the smallest code/doc change that advances that milestone.
5. Run `git diff --check`.
6. Record what was verified and what remains unverified.

## Stop rules

Stop and reassess if:

- a change requires touching movement, animation, and HUD at the same time;
- a warning cannot say which system owns the bad state;
- a fix hides a mismatch instead of exposing it;
- UMG work starts before the snapshot fields are stable;
- water movement changes are made before `WaterContactState` exists in debug.
