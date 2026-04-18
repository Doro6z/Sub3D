# Sub3D - Global Project State And Precise Reframing

Date: 2026-03-28
Status: Active working reference
Scope: whole project, not only current runtime patchwork

---

## 1. Purpose

This document recenters the whole Sub3D project at project scale:
- what the GDD says the game is,
- what the prototypes really proved,
- what is actually implemented in code today,
- where the project drifted,
- what must be finished first,
- what must be delayed on purpose.

This is not a feature spec.
This is a project-level state document.

---

## 2. Canonical References

Product and world vision:
- [GDD_01_Vision_Univers_Monde.md](c:/ACC/Projects/Sub3D/Source/GDD_01_Vision_Univers_Monde.md)
- [GDD_02_Sous_Marin.md](c:/ACC/Projects/Sub3D/Source/GDD_02_Sous_Marin.md)
- [GDD_03_Gameplay.md](c:/ACC/Projects/Sub3D/Source/GDD_03_Gameplay.md)
- [GDD_04_IA_Multijoueur.md](c:/ACC/Projects/Sub3D/Source/GDD_04_IA_Multijoueur.md)
- [GDD_05_Art_Audio.md](c:/ACC/Projects/Sub3D/Source/GDD_05_Art_Audio.md)
- [GDD_06_Technical_Roadmap.md](c:/ACC/Projects/Sub3D/Source/GDD_06_Technical_Roadmap.md)

Project proto references:
- [README.md](c:/ACC/Projects/Sub3D/Plans/sub3d/README.md)
- [2026-03-21_sub3d_proto_program_and_proto03_execution_handoff.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-21_sub3d_proto_program_and_proto03_execution_handoff.md)
- [2026-03-26_sub3d_proto04_submarine_compiler_architecture.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-26_sub3d_proto04_submarine_compiler_architecture.md)
- [2026-03-28_sub3d_proto04_consolidated_architecture.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-28_sub3d_proto04_consolidated_architecture.md)

Recent local reframing notes:
- [2026-03-28_sub3d_proto04c_transition_note.md](c:/Dev/Sub3D/reports/plans/2026-03-28_sub3d_proto04c_transition_note.md)
- [2026-03-28_sub3d_feedback_shakes_and_sounds_architecture.md](c:/Dev/Sub3D/reports/plans/2026-03-28_sub3d_feedback_shakes_and_sounds_architecture.md)
- [2026-03-28_sub3d_proto04D_spec_technique.md](c:/Dev/Sub3D/reports/plans/2026-03-28_sub3d_proto04D_spec_technique.md)
- [2026-03-28_sub3d_proto04D_04E_next_session_note.md](c:/Dev/Sub3D/reports/plans/2026-03-28_sub3d_proto04D_04E_next_session_note.md)

---

## 3. The Actual Product Promise

Sub3D is not "just a submarine builder" and not "just a cave generator".

The long-term game promise remains:
- submarine-first,
- interior-state-first,
- most play perceived from inside the submarine,
- co-op capable,
- sonar-centric reading of the outside world,
- semi-open layered world,
- threats, missions, repairs, breaches, survival, pressure, navigation and crisis management.

This matters because the codebase is currently much more advanced on submarine runtime than on the rest of the game loop.

---

## 4. What Each Proto Really Did

### Proto01

What it proved:
- first playable submarine fantasy,
- first ballast / helm / drag intuition,
- first sense that piloting the vehicle can feel right.

What it did not lock:
- interior traversal,
- stable collision,
- interior crisis loop,
- durable authoring pipeline.

### Proto02

What it proved:
- outer world traversal and generation direction,
- chaining exterior route segments,
- campaign/world structure direction became clearer.

What it did not lock:
- playable interior,
- breach -> flood -> repair loop,
- durable link between submarine authoring and submarine runtime.

Current status:
- maintenance only,
- not the active development priority.

### Proto03

Original ambition:
- playable interior,
- movement inside a moving submarine,
- collisions,
- breaches,
- repairs,
- first real crisis loop.

What Proto03 actually became in its stabilized form:
- network baseline,
- coop baseline,
- interior traversal baseline,
- base UI,
- substations,
- moving frame consistency.

This is important:
- Proto03 is not "the whole interior game" yet,
- but it is the authority baseline for movement/network/interior frame.

Current status:
- foundational and still critical,
- must not be casually destabilized.

### Proto04

Proto04 in the real codebase became:
- submarine compiler,
- runtime hull structure,
- breach generation,
- flooding,
- feedback,
- doors,
- confinement,
- advanced envelope/silhouette work.

This is strong work, but it is not identical to the original GDD interpretation of "Proto 4" in the roadmap, which was closer to:
- outer route generation,
- sonar route readability,
- clearance and playable navigation outside.

This mismatch is one of the main project drifts.

---

## 5. Actual Implementation Status Today

### 5.1 Strongly implemented

These parts are real and materially usable:
- submarine runtime truth through hull/layout/runtime components,
- interior structural sheets and breach logic,
- flood basics,
- generated doors and minimal confinement,
- feedback layer foundations,
- compiler pipeline A/B,
- hydro volume distinction from pure structural shell,
- movement reading authoritative flood mass,
- coop/network baseline inherited from Proto03 stabilization.

### 5.2 Partially implemented / in redress

These parts exist but are not yet behaviorally correct enough:
- water equalization between compartments,
- crew swim behavior,
- death pipeline,
- visual breach placement vs visible hull skin,
- full gameplay use of compartment hydraulic states,
- final interaction between envelope evolution and runtime leak truth.

### 5.3 Mostly specified, not yet truly playable

These parts are more advanced in docs than in gameplay:
- sonar-centered outer navigation,
- threat gameplay,
- mission sockets and mission loop,
- campaign graph,
- broader worldgen stack,
- long-term suit / pressure / survival ladder,
- full outer-world tactical gameplay.

---

## 6. Project Drift

The project currently has a real structural drift:

### Drift A - Runtime is ahead of macro-game

The submarine itself is becoming sophisticated:
- compiler,
- doors,
- breaches,
- flood,
- local feedback,
- generated runtime layout.

But the broader game loop is still behind:
- sonar as central play,
- real outside route gameplay,
- threats,
- mission loop,
- campaign progression.

### Drift B - Proto04 means two different things

In code reality, Proto04 means:
- "compiled submarine + damage/flooding runtime"

In roadmap/GDD reading, Proto04 means:
- "outer route generation and sonar-ready world layer"

These are not the same milestone.

### Drift C - Too many valid fronts at once

The following are all meaningful, but they do not all deserve equal priority now:
- hydraulic water correctness,
- custom swim,
- death baseline,
- leak sync with advanced envelope,
- advanced compiler/editor work in Proto04E,
- outer route generation,
- sonar readability,
- campaign architecture.

Without a forced order, the project risks staying impressive-but-unclosed on every front.

---

## 7. The Project Is Not Lost

The current state is not failure.

The project actually has a stronger submarine core than many prototypes ever reach:
- authority/runtime separation exists,
- moving-frame traversal exists,
- generated interior pipeline exists,
- breaches and flooding are real,
- door/confinement loop exists,
- feedback layer has an architecture,
- documentation quality is high.

The problem is not lack of progress.
The problem is lack of project-scale closure order.

---

## 8. What Must Be Treated As The Current Core Product Slice

The true current core slice is:
- one reference submarine,
- stable moving-frame interior traversal,
- generated interior layout,
- breach -> flood -> confinement -> repairable crisis direction,
- enough feedback to read the situation,
- enough runtime truth to support future outer gameplay.

This means:
- the submarine interior machine must be finished before the rest of the game can be honestly judged.

It does not mean:
- outer world, sonar and campaign are unimportant.

It means:
- they should not steal priority before the submarine core becomes robust and readable.

---

## 9. What Must Be Finished Before Opening More Major Fronts

### Priority 1 - Finish the Submarine Core Slice

This is the main redress track.

What remains:
- correct water equalization between compartments,
- stable and readable water debug/observability,
- custom swim instead of stock UE swimming,
- death baseline,
- leak sync with visible compiled hull skin,
- then only later: richer pumps, pressure, suits, advanced damage states.

This is the only track that should currently have the right to create deep runtime changes.

### Priority 2 - Keep Proto04E On A Short Leash

Proto04E is useful because:
- envelope quality matters,
- the submarine shape matters,
- compiler/editor quality matters.

But Proto04E should not run too far ahead of runtime truth.

Allowed now:
- shape quality,
- envelope refinements,
- compiler improvements that do not break runtime contracts.

Not healthy yet:
- pushing advanced authoring/editor complexity while leak/runtime/surface contracts are still unresolved.

### Priority 3 - Reopen The Real Outer Game After The Core Slice Holds

Only after the submarine core is genuinely solid:
- outer route generation,
- sonar readability,
- threat gameplay,
- mission loop,
- campaign graph execution.

This is the point where the project reconnects with the actual GDD promise.

---

## 10. Current Freeze / Defer Decisions

These should be treated as intentionally deferred, not forgotten:
- no big return to compartment pressure simulation right now,
- no big pump pass right now,
- no deep suit ladder right now,
- no broad reopening of campaign/worldgen before submarine core closure,
- no major visual-destructible mesh architecture before leak truth and surface patch direction are fixed.

This freeze is healthy.
It reduces project noise.

---

## 11. Recommended Project Order From Now

### Phase 1 - Close The Inner Crisis Slice

Order:
1. water equalization correctness
2. custom swim
3. death baseline
4. leak sync / surface patch truth

Exit condition:
- interior crisis loop is readable, testable, and not lying visually.

### Phase 2 - Stabilize The Compiler/Runtime Contract

Order:
1. keep hydro volumes coherent
2. keep generated doors/layout coherent
3. align envelope evolution with runtime hit/leak truth

Exit condition:
- compiler work and runtime damage/flood systems stop fighting each other.

### Phase 3 - Reconnect To The Real Proto04 GDD Layer

Order:
1. outer route generation
2. submarine clearance metrics
3. sonar readability
4. mission sockets / mission structure

Exit condition:
- the game is no longer only an interior machine, but a submarine in a readable world.

### Phase 4 - Threats / Campaign / Full Loop

Order:
1. threats
2. mission pressure
3. campaign routing and persistence
4. broader role depth

Exit condition:
- the project approaches its actual product identity, not just its subsystem identity.

---

## 12. Honest Current Status Summary

If forced to summarize Sub3D today in one paragraph:

Sub3D currently has a strong and unusually serious submarine runtime foundation, especially around moving-frame interior traversal, generated submarine layout, hull damage, breaches, doors and flooding. It does not yet have the broader game loop maturity promised by the GDD, especially on sonar-first outer navigation, threats, missions and campaign structure. The correct strategic move is not to spread wider, but to finish the inner submarine crisis slice cleanly, keep Proto04E under control, and then reconnect the project to the larger outer-world promise.

---

## 13. Project Health Verdict

### The project is healthy if:
- the team accepts that the submarine core is the current mainline,
- movement/network baseline stays protected,
- Proto04D is finished before opening too many new fronts,
- Proto04E is advanced carefully,
- the macro-game is reopened only after the submarine core holds.

### The project is unhealthy if:
- every interesting subsystem stays half-open,
- Proto04E and runtime drift further apart,
- worldgen/sonar/campaign are reopened before the submarine core stops lying,
- the team confuses "many specs" with "closed milestones".

Current verdict:
- promising,
- structurally advanced,
- not yet project-coherent enough,
- recoverable with a strict closure order.

---

## 14. Immediate Recommended Next Working Order

For the next sessions:
1. use `LogSubHullWater` and fix equalization behavior
2. implement custom swim
3. implement death baseline
4. define leak/surface patch truth against the compiled hull
5. only then decide how far Proto04E can advance next

This is the shortest path to making the whole project coherent again.
