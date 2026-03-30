# Sub3D - Master Program Planning Architecture

Date: 2026-03-28
Status: Active planning framework
Scope: whole Sub3D program, all future implementation sessions

---

## 1. Purpose

This document defines how to turn the current Sub3D document corpus into a coherent execution stack:
- product vision,
- architecture,
- track roadmaps,
- technical specs,
- implementation packets,
- test plans,
- autonomous coding/test workflows.

This is not a system spec.
It is the planning architecture for the whole project.

Goal:
- stop planning drift,
- stop re-deriving priorities every session,
- make each implementation session enter from a known document,
- allow agentic coding and agentic testing without losing scope discipline.

---

## 2. Problem Statement

Sub3D already has:
- GDD documents,
- proto handoffs,
- architecture notes,
- technical specs,
- local transition notes,
- implementation already in the repository.

But the corpus is fragmented across:
- product vision,
- runtime reality,
- proto-specific execution,
- local corrective notes.

This creates four problems:
- the same term means different things depending on the doc,
- some roadmap layers are more mature in code than in planning,
- some planning layers are more mature on paper than in runtime,
- implementation sessions risk opening the wrong front.

Sub3D now needs a planning stack, not more disconnected notes.

---

## 3. Canonical Source Hierarchy

All future planning should obey this hierarchy:

### Layer 0 - Product Canon

Source of truth for what the game is:
- [GDD_01_Vision_Univers_Monde.md](c:/ACC/Projects/Sub3D/Source/GDD_01_Vision_Univers_Monde.md)
- [GDD_02_Sous_Marin.md](c:/ACC/Projects/Sub3D/Source/GDD_02_Sous_Marin.md)
- [GDD_03_Gameplay.md](c:/ACC/Projects/Sub3D/Source/GDD_03_Gameplay.md)
- [GDD_04_IA_Multijoueur.md](c:/ACC/Projects/Sub3D/Source/GDD_04_IA_Multijoueur.md)
- [GDD_05_Art_Audio.md](c:/ACC/Projects/Sub3D/Source/GDD_05_Art_Audio.md)
- [GDD_06_Technical_Roadmap.md](c:/ACC/Projects/Sub3D/Source/GDD_06_Technical_Roadmap.md)

### Layer 1 - Program Canon

Source of truth for current project direction:
- [2026-03-21_sub3d_proto_program_and_proto03_execution_handoff.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-21_sub3d_proto_program_and_proto03_execution_handoff.md)
- [2026-03-28_sub3d_global_project_state_precise_reframing.md](c:/Dev/Sub3D/reports/plans/2026-03-28_sub3d_global_project_state_precise_reframing.md)

### Layer 2 - Active Architecture Canon

Source of truth for current technical shape:
- [2026-03-26_sub3d_proto04_submarine_compiler_architecture.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-26_sub3d_proto04_submarine_compiler_architecture.md)
- [2026-03-28_sub3d_proto04_consolidated_architecture.md](c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-28_sub3d_proto04_consolidated_architecture.md)

### Layer 3 - Execution Specs

Source of truth for implementation slices:
- proto-specific specs,
- system-specific technical specs,
- explicit transition notes where a spec has been corrected by runtime reality.

### Layer 4 - Repository Code

Final authority on what is implemented:
- `Source/`
- `Plugins/`
- `Config/`

No planning document can overrule repository reality without an explicit corrective spec.

---

## 4. Planning Stack Required For Sub3D

The full planning stack should be organized in the following layers.

### A. Global Program Layer

Purpose:
- define the project north star,
- define track priorities,
- define what is frozen, active, or deferred.

Required documents:
1. product north star
2. current project state
3. master track map
4. milestone map
5. risk register

### B. Domain Architecture Layer

Purpose:
- define the major subsystems of the whole game.

Required architecture documents:
1. core submarine runtime architecture
2. outer world and route generation architecture
3. sonar and sensor architecture
4. threats and combat architecture
5. mission and campaign architecture
6. crew role / character / status architecture
7. feedback / audio / VFX architecture
8. authoring / compiler / editor tooling architecture
9. networking and authority architecture
10. test infrastructure architecture

### C. Track Roadmap Layer

Purpose:
- translate architecture into implementable fronts.

Recommended tracks:
- Track A - Core Submarine Runtime
- Track B - Outer World / Sonar / Threats / Campaign
- Track C - Tooling / Authoring / Compiler / Editor
- Track D - Content, Assets, Feedback, Readability
- Track E - Validation, Testing, Automation, QA

Each track needs:
- objective,
- boundaries,
- dependencies,
- milestones,
- done criteria,
- freeze criteria.

### D. Technical Spec Layer

Purpose:
- define the execution contract of each major slice.

Each technical spec should include:
- scope,
- non-goals,
- source files,
- runtime contracts,
- data contracts,
- UI/debug contracts,
- networking constraints,
- test criteria,
- migration / compatibility notes,
- clear done criteria.

### E. Implementation Packet Layer

Purpose:
- turn one technical spec into one or more coding sessions.

Each packet should include:
- exact task boundary,
- exact files likely touched,
- assumptions,
- open questions already resolved,
- verification command,
- PIE protocol,
- rollback concern if any.

### F. Validation Layer

Purpose:
- ensure every spec is verified the same way.

Required validation documents:
- deterministic automation plan
- PIE manual validation plan
- multiplayer validation plan
- performance and profiling plan
- regression watchlist

### G. Agentic Execution Layer

Purpose:
- make the plan operable by coding agents and future test agents.

Required documents:
- coding session protocol
- planning session protocol
- test agent protocol
- bug triage and repro protocol
- observability/logging policy

---

## 5. Target Program Decomposition

Sub3D should now be run as five formal tracks.

### Track A - Core Submarine Runtime

Current maturity:
- highest

Contains:
- generated submarine runtime
- hull truth
- breaches
- flooding
- doors
- confinement
- movement coupling
- crew interior interaction baseline

Immediate remaining work:
- correct water equalization
- custom swim
- death baseline
- leak sync with visible hull skin
- then richer pumps and repairs

### Track B - Outer World / Sonar / Threats / Campaign

Current maturity:
- heavily specified
- lightly playable

Contains:
- route generation
- stratified world
- clearance metrics
- sonar readability
- first threats
- mission sockets
- campaign graph

This is a major GDD pillar and must be reopened after Track A stabilizes.

### Track C - Tooling / Authoring / Compiler / Editor

Current maturity:
- strong in the submarine compiler domain
- incomplete as a designer-wide pipeline

Contains:
- envelope definitions
- graph authoring
- solver
- geometry generation
- preview/build workflows
- editor affordances

Track C must not outrun runtime truth.

### Track D - Content / Feedback / Readability / Asset Rules

Current maturity:
- architecture exists
- content pipelines and placeholders exist
- final readability still emerging

Contains:
- alarms
- leak/flood audio
- door presentation
- station readability
- material rules
- VFX readability

### Track E - Validation / Automation / Agentic Test Infrastructure

Current maturity:
- some automation exists
- not yet a whole-program validation stack

Contains:
- subsystem automation
- PIE procedures
- multiplayer regression suites
- logging contracts
- autonomous or semi-autonomous test workflows

---

## 6. Required Master Documents To Produce

The following documents should be authored in order.

### Phase 0 - Canon Freeze

1. `sub3d_product_north_star.md`
2. `sub3d_master_track_map.md`
3. `sub3d_global_risk_register.md`

### Phase 1 - Domain Architecture Pack

1. `sub3d_core_submarine_runtime_architecture.md`
2. `sub3d_outer_world_and_route_generation_architecture.md`
3. `sub3d_sonar_and_sensor_architecture.md`
4. `sub3d_threats_and_combat_architecture.md`
5. `sub3d_mission_and_campaign_architecture.md`
6. `sub3d_crew_role_status_and_survival_architecture.md`
7. `sub3d_networking_and_authority_architecture.md`
8. `sub3d_testing_and_validation_architecture.md`

### Phase 2 - Track Master Specs

1. `trackA_core_submarine_runtime_master_spec.md`
2. `trackB_outer_world_sonar_campaign_master_spec.md`
3. `trackC_tooling_authoring_master_spec.md`
4. `trackD_feedback_assets_readability_master_spec.md`
5. `trackE_validation_and_test_automation_master_spec.md`

### Phase 3 - System Technical Specs

Examples:
- water equalization spec
- custom swim spec
- death baseline spec
- leak surface sync spec
- route generation clearance spec
- sonar field spec
- mission socket spec
- threat detection/combat spec
- campaign graph runtime spec

### Phase 4 - Implementation Packets

Every technical spec should then be broken into session packets.

### Phase 5 - Test Pack

1. automation suite roadmap
2. PIE scenario matrix
3. multiplayer regression matrix
4. agentic testing protocol

---

## 7. Session Method For Producing The Whole Corpus

This work should not be done as one giant doc dump.

It should be done as a sequence of controlled sessions:

### Session Type A - Canon Clarification

Objective:
- resolve ambiguity in product meaning and track priority.

Output:
- one clean canonical document.

### Session Type B - Architecture Drafting

Objective:
- define one domain architecture.

Output:
- one architecture reference document.

### Session Type C - Technical Spec Drafting

Objective:
- define one implementation-ready slice.

Output:
- one concrete technical spec.

### Session Type D - Packetization

Objective:
- cut one spec into coding packets.

Output:
- one or more session packets.

### Session Type E - Validation Planning

Objective:
- define how the slice will actually be tested.

Output:
- one test protocol document.

This makes the whole planning corpus grow cleanly instead of becoming another pile of overlapping notes.

---

## 8. Question-Driven Planning Workflow

Yes, this should be done with interactive clarification.

For each major document, planning should answer a fixed question set:

### Product questions

- what player fantasy is central here
- what is actually in scope now
- what is explicitly deferred
- what is the next playable proof

### Architecture questions

- what component owns runtime truth
- what is authoritative
- what is replicated
- what is derived presentation
- what is debug/observable

### Implementation questions

- what files are likely touched
- what data shape is required
- what contracts must remain stable
- what tests must pass before merge

### Planning questions

- is this really a top priority track
- what other track is blocked by this
- what should not be opened until this closes

This workflow is ideal for agentic coding because it reduces under-specified sessions.

---

## 9. How Agentic Coding Fits In

Once the planning stack exists, coding agents can work effectively because each packet can provide:
- exact scope,
- exact constraints,
- known files,
- explicit contracts,
- explicit verification.

That means:
- less drift,
- less accidental refactor,
- fewer wrong abstractions,
- easier multi-session continuity.

Coding agents should execute only from:
- a validated technical spec,
- or an implementation packet derived from it.

Not directly from loose brainstorm notes.

---

## 10. How Agentic Testing Fits In

Autonomous or semi-autonomous testing becomes useful only after the validation layer exists.

The future testing stack should include:
- subsystem automation tests
- deterministic runtime probes
- PIE scenario scripts
- multiplayer sync scenarios
- bug repro recipes
- observability conventions

Test agents should be able to answer:
- what scenario to run,
- what success signal to read,
- what logs to capture,
- what regressions matter.

Without that, test agents only generate noise.

---

## 11. Immediate Recommended Planning Order

The most useful next documentation order is:

1. `sub3d_product_north_star.md`
2. `sub3d_master_track_map.md`
3. `trackA_core_submarine_runtime_master_spec.md`
4. `trackB_outer_world_sonar_campaign_master_spec.md`
5. `sub3d_networking_and_authority_architecture.md`
6. `sub3d_testing_and_validation_architecture.md`

Why this order:
- it first locks what the game is,
- then what the active tracks are,
- then what the most advanced track must close,
- then how the project reconnects to the actual GDD outside world,
- then how to protect the runtime and validation stack while scaling.

---

## 12. Immediate Recommended Implementation Order

Before reopening too many new macro systems, Track A should continue in this order:

1. water equalization debug and correction
2. custom swim
3. death baseline
4. leak sync against visible compiled hull

Only then:
- reopen richer pumps and repairs,
- or push Track B aggressively,
- or let Track C run much farther.

---

## 13. Final Position

Yes, it is realistic and useful to build:
- the global architecture,
- every needed technical document,
- implementation session packets,
- and future agentic test protocols

for the whole Sub3D project.

But it must be done as a formal planning program, not as one giant unsorted writing pass.

The right approach is:
- canon first,
- architecture second,
- track master specs third,
- technical specs fourth,
- implementation packets fifth,
- validation/test protocols sixth.

This document is the framework for that process.
