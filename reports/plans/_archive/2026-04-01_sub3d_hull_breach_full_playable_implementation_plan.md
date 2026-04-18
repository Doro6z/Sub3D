# Sub3D - Full Playable Hull Breach Implementation Plan

Date: 2026-04-01
Status: Active implementation plan
Scope: Complete and robust hull damage, breach, flooding, repair, dewatering, and visual rupture path for the first playable submarine run

---

## 1. Purpose

This document defines the full implementation plan for the hull gameplay track needed to make the submarine loop genuinely playable.

The target is not a debug-only breach.
The target is a full gameplay chain:

- impact or scripted strike
- hull damage
- visible rupture
- breach growth if damage continues
- flooding and containment consequence
- repair action
- patch state
- pump-out and recovery
- continue the run or fail the run

This plan is aligned to the first playable and must not reopen scope beyond that product slice.

---

## 2. Canon And Scope Defense

Keep this priority order:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-03-29_sub3d_product_north_star_implementation_plan.md`
3. `reports/plans/2026-03-31_sub3d_gameplay_vision_questions.md`
4. this document

Scope rule:

- this plan owns the complete hull / breach / flood / repair track
- it does not replace the first playable run spec
- it must serve the run spec

Hard limit:

- no full campaign runtime dependency
- no runtime mesh boolean destruction of the entire submarine if a cheaper and more robust first playable path exists

---

## 3. Product Objective

For the first playable, the breach crisis must become a real mid-run gameplay event.

The player must be able to:

1. collide or suffer a scripted breach
2. understand where the submarine is damaged
3. see that the hull is visually opened at the damage site
4. understand that water ingress is ongoing
5. move to the damaged area
6. apply a real repair action
7. stop exterior inflow
8. remove remaining water with pumps and containment
9. recover enough submarine viability to continue

The player must also be able to fail because they:

- hit too hard
- let the breach grow
- fail to contain flooding
- fail to dewater in time
- reach a submarine state that is no longer recoverable

---

## 4. Done Definition For This Track

This track is complete when all of the following are true in the first playable:

1. High-speed collision or scripted damage can create a visible hull rupture.
2. Repeated impacts on the same area enlarge the visible rupture and increase gameplay consequence.
3. Hull visuals, breach logic, flood state, and repair state all agree.
4. Repair stops exterior inflow only when the breach is actually sealed.
5. Pump-out removes already ingressed water after sealing.
6. The run shell can detect that the designated breach crisis is resolved.
7. The player can finish the first playable after successful crisis handling.
8. The player can fail the first playable from hull-state-driven catastrophe.

---

## 5. Current Base

The repository already provides a strong base.

Already present:

- `USubHullComponent` structural sheets and per-cell damage
- breach cluster rebuild
- flood runtime and compartment mirror
- repair APIs
- pump APIs
- `BreachVfxManager`
- `FloodWaterVisuals`
- `USubInteractionComponent` repair routing
- speed-gated hull collision damage in `ASubmarineBase`
- `USubmarineSystemsComponent` pump state routing

Not closed yet:

- exterior hull opening visuals
- rupture growth visuals
- repair patch visuals
- full pressure gameplay
- first playable breach objective closure on a real repair state
- final catastrophe criteria

This means the implementation plan must build on the current runtime, not replace it.

---

## 6. Target Architecture

The complete robust solution should use five truths.

### 6.1 Truth A - Structural damage

Owner:

- `USubHullComponent`

Responsibilities:

- per-sheet damage
- per-cell open or closed state
- breach clusters
- flood rates
- repair state

Rule:

- no visual system owns breach truth

### 6.2 Truth B - Exterior visual rupture

Owner:

- new `USubHullVisualDamageComponent`

Responsibilities:

- render visible hole state on the exterior hull
- render growth of the hole
- render patch state after repair
- keep visuals attached to structural sheets and breach clusters

Rule:

- this component reads structural truth
- it does not decide gameplay breach state

### 6.3 Truth C - Interior water and viability

Owners:

- `USubHullComponent`
- `USubmarineCompartmentComponent`
- `UFloodWaterVisualsComponent`

Responsibilities:

- actual ingressed water
- compartment state
- dewatering state
- internal readability

### 6.4 Truth D - Player action

Owners:

- `USubInteractionComponent`
- repair tool state on crew side

Responsibilities:

- detect repairable target
- apply repair over time
- feed repair visuals and gameplay state

### 6.5 Truth E - Run objective

Owners:

- `ASubGameMode`
- `ASubGameState`

Responsibilities:

- designate the breach objective for the first playable
- decide when the breach crisis is resolved
- decide when the run fails from hull state

---

## 7. Chosen Visual Strategy

The robust first playable solution should not use runtime boolean remeshing of the full hull mesh.

That path is too expensive, too brittle, and not required to prove the product loop.

The robust first playable path should be:

- structural damage remains sheet and cell based
- visible hull opening is material-driven and sheet-addressed
- rupture border meshes and patch meshes are spawned and attached locally
- VFX continues to come from `BreachVfxManager`

### 7.1 Required visual result

At a breach location, the player should see:

- a real dark opening in the hull surface
- a rupture rim or torn metal border
- water spray and suction VFX
- a patch visual after repair

### 7.2 Required technical result

For each structural sheet that can visually rupture:

- the exterior visual mesh must expose a local visual frame
- the material must support one or more local hole masks
- the runtime must be able to update visible hole center and radius
- repeated impacts on a nearby area must enlarge or merge the visible mask

### 7.3 Preferred implementation

Preferred first playable implementation:

1. Exterior hull sections remain stable geometry.
2. Each rupture-capable sheet gets a dynamic material instance.
3. That material supports local hole masks in sheet space.
4. `USubHullVisualDamageComponent` converts breach clusters to sheet-space masks.
5. A rupture border mesh actor is attached at each active cluster.
6. A repair patch actor replaces or overlays the rupture border when sealed.

This is robust because:

- it matches the existing sheet-based hull truth
- it does not require full geometry rebuild every impact
- it is authorable in editor
- it can scale with repeated damage

---

## 8. Required Data Additions

The current structural types are enough for logic, but not enough for the full visual solution.

The plan requires adding editor-authored visual data on the hull side.

### 8.1 Extend structural sheet authoring

Each rupture-capable exterior sheet should additionally define:

- whether it supports visible rupture
- which exterior visual material slot or section it maps to
- its local visual basis for mask projection
- optional maximum visible rupture radius
- optional preferred rupture border mesh scale

### 8.2 Add breach visual runtime state

New runtime state should exist for visuals only:

- active breach visual id
- owning structural sheet id
- local center
- current visible radius
- target visible radius
- patch state

This state should be derived from structural truth and not replicated independently unless required for client-only visual timing.

### 8.3 Add repair patch runtime state

Repair should expose:

- patch placed or not
- patch integrity
- patch visual actor state
- whether the patch is holding against current damage

Repeated impacts must be able to damage or destroy a patch.

---

## 9. Repair Contract

The robust solution must distinguish three different things:

1. Hull sealed
2. Water contained
3. Water removed

These are not the same.

### 9.1 Repair action

The player repair action should:

- target a visible breach or damaged hull area
- apply repair over time
- require line of sight and usable range
- drive a patch state, not just instantly zero damage

### 9.2 Repair result

A successful repair should:

- stop or sharply reduce exterior inflow
- convert the breach to a sealed patch state
- leave existing water in the compartment until dewatered

### 9.3 Reopening behavior

If the patched area is hit again:

- the patch can fail
- the visible rupture can reopen
- flooding can resume

### 9.4 First playable resolution rule

The first playable should exit `BreachCrisis` only when all of the following are true for the designated breach:

- the designated exterior breach is sealed
- the designated compartment is below a safe water threshold
- the submarine is not in catastrophic state

This is the robust shipping rule.
Do not end the crisis just because a patch was placed while the compartment is still not viable.

---

## 10. Pressure, Flooding, And Containment Closure

Current pressure fields exist in runtime data but are not behaviorally closed.

The full robust solution must finish this.

### 10.1 Exterior pressure

Exterior pressure must be derived from depth and fed into the hull model.

### 10.2 Internal pressure

Internal pressure must react to:

- free air volume
- flooding
- open breach area
- door state between compartments

### 10.3 Crew consequence

Pressure and flooding should feed:

- suction danger
- overpressure risk
- movement degradation in flooded compartments
- failure criteria when the submarine is no longer recoverable

### 10.4 Pump consequence

Pump efficiency should depend on:

- available power
- water amount
- pressure delta
- target compartment

Current pump penalty fields in `USubHullComponent` should become fully meaningful rather than placeholder tuning only.

---

## 11. Feedback And Readability Contract

The player must never have to guess whether the breach is sealed.

The robust solution needs explicit readability on:

- breach active
- breach severity
- patch placed
- patch integrity
- compartment flooding trend
- pump active
- pump effectiveness
- crisis resolved or not

Use existing systems where possible:

- `USubmarineFeedbackDirectorComponent`
- `BreachVfxManager`
- `FloodWaterVisuals`
- helm status panels
- alarms and local station readouts

Do not hide essential repair state only inside debug logs.

---

## 12. Complete Implementation Packets

## Packet H1 - Runtime Hardening Baseline

Goal:

- close the current logical hull loop and keep it authoritative

Scope:

- finish A4 collision behavior
- keep repair and pump runtime coherent
- remove contradictions between structural state and compartment state

Primary files:

- `SubmarineBase.h/.cpp`
- `SubHullComponent.h/.cpp`
- `SubmarineSystemsComponent.h/.cpp`
- `SubmarineCompartmentComponent.h/.cpp`

Done when:

- high-speed collision damages hull
- low-speed scrape does not
- repair can seal
- pump can reduce remaining water

## Packet H2 - Run Objective Integration

Goal:

- make hull crisis a real first playable objective

Scope:

- `ASubGameMode` designates one breach objective
- `ASubGameState` mirrors crisis state
- objective exits only on sealed + safe compartment rule
- catastrophe can fail the run

Primary files:

- `SubGameMode.h/.cpp`
- `SubGameState.h/.cpp`
- `SubRunPhase.h`

Done when:

- `BreachCrisis` has a clear authoritative enter and exit path

## Packet H3 - Structural Sheet Visual Authoring

Goal:

- make exterior hull visuals addressable by structural sheet

Scope:

- extend layout and geometry authoring for rupture-capable sheets
- expose sheet-to-visual mapping
- define local visual basis for hole masks

Primary files:

- `StructuralHullTypes.h`
- `SubmarineLayoutAsset.h`
- `SubmarineGeometryBuilder.h/.cpp`
- `SubmarineCompilerActor.h/.cpp`

Done when:

- each rupture-capable exterior sheet can drive a local visual opening

## Packet H4 - Hull Visual Damage Component

Goal:

- render visible hull openings and growth

Scope:

- new `USubHullVisualDamageComponent`
- dynamic material instances per rupture-capable sheet
- breach-to-mask conversion
- rupture border actor spawning
- patch actor spawning

Primary files:

- new `SubHullVisualDamageComponent.h/.cpp`
- possible new `SubHullPatchVisualActor.h/.cpp`
- `SubmarineBase.h/.cpp`
- `SubmarineCompilerActor.h/.cpp`

Done when:

- an active breach is visible on the hull surface
- repeated damage enlarges the visible opening

## Packet H5 - Repair Tool Gameplay

Goal:

- turn repair into a real player action

Scope:

- hold-to-repair or repeated application path
- repair target filtering
- repair strength over time
- patch placement state
- patch failure on new impact

Primary files:

- `SubInteractionComponent.h/.cpp`
- `SubCrewCharacter.h/.cpp`
- possible new repair tool component or state holder
- `SubHullComponent.h/.cpp`

Done when:

- the player can deliberately seal a breach through interaction

## Packet H6 - Pressure And Containment Closure

Goal:

- make breach consequence physically and gameplay coherent

Scope:

- real exterior pressure by depth
- internal pressure equalization
- door-driven containment consequence
- suction and crew consequence updates
- pump efficiency fully tied to state

Primary files:

- `SubHullComponent.h/.cpp`
- `SubCrewCharacter.h/.cpp`
- `SubmarineFeedbackDirectorComponent.h/.cpp`

Done when:

- the player can feel the difference between open breach, sealed breach, and dewatered compartment

## Packet H7 - Readability And Alarms

Goal:

- make the crisis understandable without debug tools

Scope:

- hull breach alarms
- flood trend alarms
- patch state feedback
- pump effectiveness feedback
- helm and local station state exposure

Primary files:

- `SubmarineFeedbackDirectorComponent.h/.cpp`
- helm widget or station data surfaces
- local repair station widgets if used

Done when:

- the player can tell what needs to be done and whether it is working

## Packet H8 - Catastrophe And Failure Closure

Goal:

- make failure come from hull state, not only scripted loss

Scope:

- catastrophe thresholds
- unrecoverable flood state
- loss of submarine viability
- run failure integration

Primary files:

- `SubGameMode.h/.cpp`
- `SubGameState.h/.cpp`
- `SubHullComponent.h/.cpp`
- `SubMovementComponent.h/.cpp`

Done when:

- the run can fail from real hull mismanagement

## Packet H9 - Editor Authoring And Content Contract

Goal:

- make the system authorable and repeatable in editor

Scope:

- rupture-capable sheet authoring rules
- patch asset rules
- material slot rules
- debug utilities for breach placement and reset
- compartment naming and objective mapping

Done when:

- a level designer can place, preview, and validate the breach setup without C++ changes

## Packet H10 - First Playable Balance Pass

Goal:

- make the crisis fun, readable, and completable

Scope:

- collision thresholds
- breach growth rates
- leak rates
- repair speed
- pump speed
- safe water thresholds for crisis resolution

Done when:

- the breach event creates a real decision but remains solo-completable inside the first playable time budget

---

## 13. Editor Handoff Requirements For The Final Solution

The final editor handoff, once this plan is implemented, must include:

1. exact actor and component setup on the submarine prefab
2. exact material and rupture asset hookup
3. exact repair tool setup
4. exact designated breach objective setup in the first playable level
5. exact pump and compartment naming contract
6. exact PIE protocol for collision, scripted breach, repair, and recovery

That future handoff is not this document.
This document is the implementation plan that leads to it.

---

## 14. Explicit No-Go Rules

Do not do the following:

1. Do not make visuals the source of truth for breach state.
2. Do not let repair instantly delete ingressed water.
3. Do not exit the crisis while the compartment is still not viable.
4. Do not build full runtime hull CSG if a sheet-based visual rupture path already closes the product need.
5. Do not make campaign runtime a dependency for this track.
6. Do not build a second damage system separate from `USubHullComponent`.

---

## 15. Final Recommendation

The correct next execution order for this track is:

1. `H2 Run Objective Integration`
2. `H3 Structural Sheet Visual Authoring`
3. `H4 Hull Visual Damage Component`
4. `H5 Repair Tool Gameplay`
5. `H6 Pressure And Containment Closure`
6. `H7 Readability And Alarms`
7. `H8 Catastrophe And Failure Closure`
8. `H9 Editor Authoring And Content Contract`
9. `H10 First Playable Balance Pass`

Reason:

- the logical hull loop already exists
- the next blocker is not raw data
- the next blocker is turning that data into a visible, repairable, first-playable crisis

The North Star and first playable both point to the same rule:

- keep the submarine as the center of the game
- make the breach crisis real
- make it readable
- make recovery feel earned
