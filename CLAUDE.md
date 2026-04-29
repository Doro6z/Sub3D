# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Sub3D is an Unreal Engine 5.7 submarine simulation game on Windows. The submarine hull and interior are procedurally generated at runtime from a data-driven pipeline. The project targets a "First Playable" milestone.

## Build & launch

```bash
# Open the project in Unreal Editor
C:/Dev/sub3d/LaunchEditor.bat

# Build from command line (Development Editor, Win64)
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" Sub3DEditor Win64 Development "C:/Dev/Sub3D/Sub3D.uproject"

# Run automation tests from command line
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Dev/Sub3D/Sub3D.uproject" -ExecCmds="Automation RunTests Sub3D" -Unattended -NullRHI -NoSound -Log
```

The solution file `Sub3D.sln` is gitignored — regenerate it from the .uproject if needed.

### Automation tests cadence

Tests live in `Source/Sub3DTests/` (module `Sub3DTests`). Run them:
- Before any commit touching `Source/Sub3D/Submarine/` core (movement, flood, sonar, replication contracts).
- After moving or renaming replicated UPROPERTYs (`FSubmarineNetState`, `USubmarineDefinition`, etc.).
- After any change to crew rebase, environment axis, or hull boundary handoff.

Add a new test alongside any new authoritative contract or replicated state. The runner is fast (~30 s with `-NullRHI`); use it as a pre-commit sanity check.

## Module architecture

Seven C++ modules, layered by responsibility:

```
Sub3DCore (Runtime)        — Pure data types and enums. No gameplay logic. No dependencies beyond Core/Engine.
Sub3DRuntime (Runtime)     — Runtime data assets and components (flood, breach, door). Depends on Sub3DCore.
Sub3D (Runtime)            — All gameplay: submarine pawn, crew character, movement, generation, weapons, HUD.
                             Depends on Sub3DCore, Sub3DRuntime, EnhancedInput, UMG, Niagara, PCG, GameplayTags.
Sub3DBake (Editor)         — LEGACY authoring/bake pipeline (Phase 7A). Superseded by SubmarineGenerator for First Playable.
                             Kept for Proto03/04 asset authoring only.
Sub3DBuilder (Editor)      — Hull profile evaluation and submarine builder components.
Sub3DEditor (Editor)       — Custom asset type actions, Slate debug panels, editor toolkit.
Sub3DTests (Editor)        — Automation test framework.
```

When adding module dependencies, always update the corresponding `.Build.cs` file.

## Generation pipeline (PAUSED for First Playable)

**Status (decision 2026-04-18, see memory `project_pipelines_status_2026_04_18.md`):**

- Production submarine for First Playable is `BP_Submarine_Craniata` — handmade, Blender-script + manual BP wiring. **Anything new targets Craniata.**
- The runtime generator pipeline below is in **pause** during FP. Code remains in tree but no active feature targets it.
- Bake pipeline (`Sub3DBake/`) is also in pause (legacy Proto03/04).
- Bugs in Generator or Bake during FP go to `reports/backlog/post_fp_debt.md`, not to fix.

Generator pipeline (kept as architectural reference, not active):

```
USubmarineGeneratorSpec          — Editor-authored data asset (input)
        ↓
USubmarineGenerator::Generate()  — Derives compartments, airlock, flood graph, stations, spawns
        ↓
USubmarineDefinition             — RUNTIME SOURCE OF TRUTH (output)
        ↓
        ├→ USubmarineMeshBuilder         → USubmarineGeneratedGeometryComponent (ProceduralMeshComponent)
        ├→ USubmarineStationManagerComponent → Spawns helm/engine/ballast stations
        ├→ USubFloodComponent            → Reads compartment topology for flood simulation
        └→ ASubDoorActor                 → Spawns from connector definitions
```

`USubmarineDefinition` is the single source of truth at runtime. All downstream systems read from it. Do not bypass it.

## Key gameplay classes

- **ASubmarineBase** — Main submarine pawn (not possessed). Houses all subsystems as components.
- **ASubCrewCharacter** — Crew member pawn. FPS camera, boarding, helm assignment, environmental effects.
- **ASubPlayerController** — Input routing, station management, control modes (OnFoot, HelmDriving, StationUI).
- **ASubGameMode** — Authoritative run phase state machine (ESubBootstrapPhase, ESubRunPhase).
- **USubMovementComponent** — Math-based physics (no Chaos), server-authoritative.
- **USubFloodComponent** — Flood simulation with replication.
- **ASubDoorActor** — Door/hatch with state machine and interaction.

## Architecture constraints

- **Submarine = authoritative moving frame.** Crew = traversal on that frame. Do not collapse movement, replication, and presentation layers.
- **Server authority** — Submarine movement, flood state, door state are authoritative on server.
- **No Chaos physics** — Movement is math-based via USubMovementComponent.
- **No runtime auto-spawn for UMG widgets** — HUDs, helm panels and station UIs are editor-assigned with explicit names and stable layout rules. Component-level runtime spawning (flood water planes per compartment, doors from Definition, hull boundary components at breaches) is fine — that is gameplay state materialization, not UI.

## Crew embarked movement — Local Grid Space Authority

Authoritative architecture: `reports/plans/2026-04-21_local_grid_space_authority_architecture.md`.

**Principle** — each crew tick when embarked:

1. **REBASE** (pre-CMC): teleport character to `SubTransform * GridSpaceTransform` via `UpdatedComponent->SetWorldLocationAndRotation(..., bSweep=false, ETeleportType::TeleportPhysics)`. Rotation is **Yaw-only** (Pitch=Roll=0 on the capsule).
2. **SIMULATE**: `Super::TickComponent(...)` — CMC runs natively against the sub's world-space geometry, which is static from its point of view.
3. **EXTRACT** (post-CMC): `GridSpaceTransform = SubTransform.Inverse() * Character->GetActorTransform()` (yaw-only extraction for rotation).

`GridSpaceTransform` (FTransform, on USubCrewMovementComponent) is the authoritative pose. The rebase runs in **all** movement modes (Walking, Falling, Swimming, Flying, Custom).

**Invariants:**

- Use `UpdatedComponent->SetWorldLocationAndRotation`, never `SetActorLocation`/`SetActorTransform` for the rebase.
- `bSweep=false` + `ETeleportType::TeleportPhysics`.
- `bIgnoreBaseRotation = IsGridAuthoritative()` — disable CMC's built-in base-rotation carry.
- `UpdateBasedMovement` and `UpdateBasedRotation` are no-ops when `IsGridAuthoritative()`.
- Controller yaw delta is applied in `TickComponent` (pre-CMC) from `SubRot.Yaw - LastSubWorldTransform.Rotator().Yaw`.
- Tick prereqs: `SubFlood → SubMovement → CrewMovement`. `SubFlood → SubMovement` is set in `USubMovementComponent::BeginPlay`; the crew-side chain is set in `InitializeForSubmarine`. (`USubInteriorFrameComponent` was deleted in Phase C of the unified motion-chain refactor — no middleman between SubMovement and CrewMovement.)

## Flood → sub movement coupling

`USubFloodComponent` is the authority for interior water mass. Each server tick, after `AdvanceFlooding`, it writes `GetTotalWaterMassKg()` into `USubMovementComponent::FloodImpactKg` via `SetFloodImpactKg`. `SimulateStep` (fixed-tick 60 Hz) samples `FloodImpactKg` into `FloodedMassKg`, which is folded into `ComputeTotalMass` via `FloodedMassInfluence`.

**Invariants:**

- `USubMovementComponent` must not reach into `USubFloodComponent` or `USubmarineCompartmentComponent` directly. Flood is an **input**, not a pull.
- Tick prereq `AddTickPrerequisiteComponent(SubFlood)` on `SubMovement` (set in `BeginPlay`) guarantees the fixed-tick integrator reads a freshly-advanced value each render frame.
- No brute paths: `USubFloodComponent` does not call `SetActorLocation`/`SetMass`/`AddForce` on the sub. Buoyancy and gravity remain inside `ApplyPhysics`.

**Kept as validation harness:**

- `USub3DDebugSettings` (Project Settings > Game > Sub3D Debug) — all debug toggles live here.
- `bLogCrewJitter` + related fields in Crew category — per-tick diagnostic log for verifying the new architecture holds. The SPIKE threshold (`CrewJitterWarnVelocityCmPerSec`, default 1200 cm/s) should now stay clean.

**Status:** Phase 1 completed (commits 24b1cb7, a95ce17, 716d1a2). Network phase absorbed into environment axis Phase 3 (see below).

## Crew environment axis — Compartment pointer + EVA handoff

Orthogonal to the locomotion axis above. Authoritative architecture: `reports/plans/2026-04-22_crew_environment_axis.md`.

**Principle** — crew state is a tuple `(MovementState, EnvironmentContext)` :

- **MovementState** = `ECrewEmbarkState { Outside, Embarked, Transitioning }` on `USubCrewMovementComponent`. Replaces the former `bool bIsGridSpaceAuthority`.
- **EnvironmentContext** = `TWeakObjectPtr<UCompartmentVolumeComponent> CurrentCompartment` on `ASubCrewCharacter`. `nullptr` = ocean.

The locomotion component does NOT know compartments. The compartment volume does NOT know about rebase. `ASubCrewCharacter` is the single coupling point.

**Detection** — managed overlap via dedicated collision channel `ECC_CompartmentProbe`. `UCompartmentVolumeComponent` (existing, enriched with `CompartmentId`, `O2Level01`, `LinkedAudioVolume`, `LinkedPostProcessVolume`) fires begin/end overlap with the crew capsule. Tiebreak on multi-overlap = nearest box center.

**EVA handoff** — `USubHullBoundaryComponent` (new) placed at hull openings (airlock, breach). Detects capsule crossing by dot product against a sub-local plane. On crossing, `ASubCrewCharacter::HandleHullCrossing` applies velocity blending:

- `Embarked → Outside`: `CrewMov->Velocity += V_sub_world`. State = Outside. CurrentCompartment = nullptr.
- `Outside → Embarked`: seed `GridSpaceTransform` from world pose, `CrewMov->Velocity -= V_sub_world`. State = Embarked. CurrentCompartment = InsideCompartmentId.

Breach path reuses the same component — `USubFloodComponent::CreateBreach` spawns a `USubHullBoundaryComponent{Kind=Breach}`.

**Status:** Phases 1+2+3 completed (2026-04-22/23). Spawn slots + PlayerController init fixed (3.5).

**Stubs for post-FP:**

- `O2Level01 = 1.f` — real simulation post-FP.
- `LinkedAudioVolume` / `LinkedPostProcessVolume` — refs populated, live switching post-FP.
- `Transitioning` state — reserved in enum, instant flip for FP (no multi-tick blend yet).
- Breach aspiration force — handoff event only, no force application.

## Custom plugins

- **LevelSwitcher** — Editor-only level switching widget.
- **RuntimeSyncDiagnostics** — Runtime diagnostics for replication debugging.
- **UnrealClaude** — Claude AI MCP bridge to the Unreal Editor (see "MCP tooling" section).
- **Sub3DDebugPanel** — Editor-only Slate dockable panel exposing all `USub3DDebugSettings` toggles in one place.

## MCP tooling (UnrealClaude)

The UnrealClaude plugin exposes MCP tools that let you query the **live editor state** instead of inferring from files. Prefer them over file reads when the editor is running and the answer depends on runtime state.

| Tool | Use when |
|---|---|
| `mcp__unrealclaude__unreal_status` | Need to know if the editor is responsive and which level is loaded. First call before any other MCP query. |
| `mcp__unrealclaude__unreal_get_output_log` | Diagnose a runtime bug — read the live log instead of guessing from code. |
| `mcp__unrealclaude__unreal_blueprint_query` | Inspect a BP graph (variables, functions, pins) without opening the editor. |
| `mcp__unrealclaude__unreal_asset_search` | Find assets by name pattern across the content tree. |
| `mcp__unrealclaude__unreal_asset_referencers` / `unreal_asset_dependencies` | Decide if it is safe to delete or rename an asset. |
| `mcp__unrealclaude__unreal_get_level_actors` | List actors in the current level — useful when a BP is misplaced or missing. |
| `mcp__unrealclaude__unreal_capture_viewport` | Capture a viewport screenshot for visual confirmation. |
| `mcp__unrealclaude__unreal_set_property` | Edit a UPROPERTY on an actor instance live. Use sparingly — the change does not persist unless saved. |
| `mcp__unrealclaude__unreal_spawn_actor` / `unreal_move_actor` / `unreal_delete_actors` | Live edits during a debug session. |

**When NOT to use MCP tools:**
- For source code reads (use `Read`, `Grep`, `Glob` — faster, no editor dependency).
- When the editor is closed (the tools will hang or fail).
- For any change you intend to persist — those go through assets/code, not MCP.

## Naming conventions (enforced by .editorconfig)

Standard Unreal prefixes: `A` (actors), `U` (UObjects), `S` (Slate widgets), `F` (structs), `E` (enums), `T` (templates), `b` (booleans). All PascalCase.

## Planning & source of truth

- Authority-max plan: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
- Older plan documents are subordinate references only. If they conflict, the 2026-04-10 strategic analysis takes precedence.
- Source of truth for code is always the repository (`Source/`, `Plugins/`, `Config/`), not memory or summaries.

## Utility scripts

Located in `Source/scripts/`:
- `create_proto04c_assets.py` — Asset creation for proto04c
- `create_proto_assets.py` — Prototype asset generation
- `create_test_sub.py` — Test submarine creation

## Scope and editing discipline

- Stay strictly inside the requested scope. Do not broaden tasks.
- Do not omit includes, macros, class members, declarations, or method bodies.
- No optional functionality in the current implementation path.
- No drive-by refactors or speculative abstractions.
- Do not invent Unreal APIs or engine features.
- Do not rename reflected types without a migration plan.
- Add logs before rewriting systems. Make debug toggles editor-accessible.
- Distinguish: root cause, possible contributor, out-of-scope issues.
- **When the user flags unfixed state or known debt, do NOT ask them to confirm the broken state.** State the explicit fix: which asset to swap, which file to edit, which step to take. "Confirming broken state" turns a plan into silent debt — if the plan is to do X, say "replace Y with X" explicitly and track it as a TODO. Applies to assets, collision, maps, BP wiring, content and code alike.

## Known environment debt (must be fixed, not worked around)

Formalized TODOs for the First Playable test stage. These are the ONLY acceptable reasons for the described symptoms — do not invent other explanations while these are unresolved.

(no open environment debt — stair complex-collision was replaced with simple box ramp collision; stair traversal is no longer an asset issue.)

## Required project config

These engine settings are load-bearing for FP and must remain set:

| Setting | Value | Why |
|---|---|---|
| `Engine.UseFixedFrameRate` | **True** | Eliminates 2-week motion-chain jitter at trigger events. PIE without it batches 1-3 sim steps per snapshot, causing visible stutter (memory `project_motion_chain_jitter_root_cause_2026_04_27.md`). Runtime guardrail in `SubMovementComponent.cpp:118` warns at BeginPlay if False. |
| `Engine.FixedFrameRate` | 60.0 | Matches `USubMovementComponent::FixedSimulationHz`. |

`USub3DDebugSettings` (Project Settings > Game > Sub3D Debug) holds all debug toggles. `bLogPresentationChain` is the master switch for motion-chain instrumentation.

## Workflow & state hygiene

- **Commit cadence**: a finished milestone = a commit, in the same day. Do not let the working tree accumulate beyond ~15 modified files. Split commits along features (Phase A, Phase C, Ladder, …), not files.
- **Push to origin every day** the branch has new commits. Local stash is **not a backup** — verified the hard way 2026-04-28 (locks during stash + accidental drop almost cost a day's untracked work).
- **Stash discipline**: close the Unreal Editor before `git stash push -u` (open .uasset locks cause partial stashes that leave the working tree in an ambiguous state). If a stash entry drops accidentally, the commit hash stays in the object database for ~90 days; recovery: `git archive <stash-hash>^3 | tar -x` extracts the untracked-files parent without touching the index.
- **Stable tags**: when the user names a commit "Stable" in its message, tag it: `git tag stable-2026-04-18 917489b`. Makes rollback trivial.
- **Pre-commit sanity**: build + automation tests before committing changes to `Source/Sub3D/Submarine/`. Cheap insurance against UHT manifest staleness on newly added headers.

## Writing quality

- Use plain technical language. No hybrid labels or AI-shaped wording (e.g., `debug-but-durable`).
- Every important adjective must map to a concrete, verifiable property.
- Prefer explicit terms: `temporary`, `persistent`, `editor-assigned`, `validation-only`, `runtime`, `debug`.
- Prefer direct statement over polished prose.

## Do not modify

- `Binaries/`
- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
