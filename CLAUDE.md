# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Sub3D is an Unreal Engine 5.7 submarine simulation game on Windows. The submarine hull and interior are procedurally generated at runtime from a data-driven pipeline. The project targets a "First Playable" milestone.

## Hardware target

- Dev machine: i7-9700K, GTX 1660 Super, Win64.
- Use `-NullRHI` for all CLI automation tests (skips renderer, massive speedup).
- Avoid running multiple Lumen/MegaLights screenshot tests in parallel; serialize them.
- 4–16 player coop is the multiplayer target (host-listen architecture).

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

### Iterative code-change workflow (Claude must apply this automatically)

When Claude modifies C++ source and needs to verify the running editor picks up the change, the workflow is:

1. **Save all** in editor — not scriptable. Skip; the user pre-saves before approving the implementation step.
2. **Quit editor** — use PowerShell, not Bash: `Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force`. (Bash on Windows mangles `taskkill /IM …` because Git Bash interprets `/IM` as a path. Force kill is required because graceful kill of GUI apps on Windows is unreliable; the user accepts losing unsaved editor work as the cost of automation.)
3. **Build** — `Build.bat Sub3DEditor Win64 Development …`. Module additions, UCLASS additions, or any change touching reflection metadata require a full rebuild — Live Coding (`Ctrl+Alt+F11`) is not sufficient.
4. **Restart editor** — PowerShell: `Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "C:\Dev\Sub3D\Sub3D.uproject"`.
5. **Wait for editor to load** — typically 20–60 s. Continue with other prep work (writing test plan, reading other files) instead of polling.

When this workflow is **NOT** required:

- Pure text/markdown/asset changes (no compile needed).
- Live-Coding-safe changes (existing function bodies, new private members, log lines): user can `Ctrl+Alt+F11` themselves; no need to kill the editor.
- Build-only verification with no runtime check needed (the user only wants to know "does it compile?"): just run `Build.bat` while editor stays open. Build will succeed if Live Coding is off, or fail with "Live Coding active" message — in which case fall back to the full quit/build/restart workflow.

For sub-step validation gates that require the editor (e.g. "click this button, see this log line"): Claude executes steps 2–4 automatically before announcing the gate so the user can immediately validate without an extra round-trip.

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

## Test loop & AI workflow

This section is the **how** of testing; the **when** is in the "Automation tests cadence" subsection of Build & launch.

The AI iterates against a tiered ladder. Pick the cheapest tier that produces a deterministic signal. Escalate only when the lower tier cannot answer.

### Tier ladder

| Tier | Mechanism | Latency | Use when |
| --- | --- | --- | --- |
| 1 | CLI Automation Framework + JSON report | 5–30 s | Pure logic, math, isolated component behavior |
| 2 | MCP-driven PIE (UnrealClaude tools) | 30 s–2 min | Replication, physics ticking, world spawn, UI in PIE |
| 3 | Claude vision oracle on captured viewport | 3–10 s per call | Perceptual checks (lighting, water look, gauges legibility) |
| 4 | Human as oracle | — | Aesthetic/creative decisions only — must be requested explicitly with a screenshot package |

Tier 1 is the default. Never escalate to vision when a `TestEqual` or a log assertion would do.

### CLI commands (Tier 1)

```bash
# List all Sub3D-prefixed tests
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Dev/Sub3D/Sub3D.uproject" \
  -ExecCmds="Automation List;Quit" -unattended -NullRHI -nopause

# Run all Sub3D tests, write JSON report to Saved/Automation/Reports
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Dev/Sub3D/Sub3D.uproject" \
  -ExecCmds="Automation RunTests Sub3D.+;Quit" \
  -unattended -nopause -NullRHI -NOSPLASH \
  -testexit="Automation Test Queue Empty" \
  -ReportOutputPath="C:/Dev/Sub3D/Saved/Automation/Reports" \
  -log -stdout

# Smoke filter (fast subset)
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Dev/Sub3D/Sub3D.uproject" \
  -ExecCmds="Automation RunFilter Smoke;Quit" -unattended -NullRHI -nopause
```

`-testexit="Automation Test Queue Empty"` is **load-bearing** — without it the editor process never terminates and the AI hangs waiting on stdout.

The deterministic signal is `Saved/Automation/Reports/index.json`. Parse it directly — vision on stdout is unnecessary.

### Test naming convention

| Prefix | Backing macro / class | Purpose |
|---|---|---|
| `Sub3D.Unit.<System>.<Behavior>` | `IMPLEMENT_SIMPLE_AUTOMATION_TEST` | Pure logic, no PIE — millisecond-fast |
| `Sub3D.Spec.<System>` | `BEGIN_DEFINE_SPEC` / `Describe` / `It` | BDD across scenarios, no PIE |
| `Sub3D.Functional.<Feature>` | `AFunctionalTest` actor in map `FTEST_<Feature>` | Needs world, ticks, replication |
| `Sub3D.Visual.<Feature>` | `AFunctionalTest` + `Take Automation Screenshot` | Image diff against checked-in baseline |

Existing tests live in `Source/Sub3DTests/` (module `Sub3DTests`). New tests must follow the prefix scheme. Existing pre-convention tests are grandfathered until next refactor of the file they live in.

### MCP-driven PIE workflow (Tier 2)

UnrealClaude (`mcp__unrealclaude__*`) is the bridge. Canonical loop for a feature requiring a live PIE:

1. `unreal_status` — confirm editor up and which map is loaded.
2. Compile via `unreal_console_command("LiveCoding.Compile")` (Live Coding) or external `Build.bat` for full rebuild on header changes.
3. `unreal_console_command("WebControl.StartServer")` — opens Remote Control HTTP on `127.0.0.1:30010`.
4. `curl -X PUT http://127.0.0.1:30010/remote/object/property` to toggle a `UPROPERTY(EditAnywhere)`. **Object path prefix** is `UEDPIE_<InstanceID>_` once PIE is live (e.g. `/Game/Maps/UEDPIE_0_L_WaterProto_TwoRooms.L_WaterProto_TwoRooms:PersistentLevel.BP_SubDoor_C_1.DoorWaterBridge`).
5. **PIE start** — currently manual click. Phase B (mcp-unreal) closes this gap with `pie_control(start, map=...)`.
6. `unreal_capture_viewport` → image bytes for the AI.
7. Tier 3 vision call **only if** the assertion is perceptual; otherwise read structured output via `unreal_get_output_log` and assert in code.
8. **PIE stop** — currently manual. Phase B: `pie_control(stop)`.

### Logging convention

New log lines must use `UE_LOGFMT` (since UE 5.2):

```cpp
UE_LOGFMT(LogSub3D, Warning, "Hull breach at compartment {Comp} pressure={Pressure}",
          ("Comp", CompartmentName), ("Pressure", CurrentPressure));
```

Categories already declared in tree:

- `LogSub3D` — gameplay default.
- `LogSub3DNet` — replication, server/client mismatch.
- `LogSub3DWaterProto` — water proto module.

Other rules:

- `LogTemp` is exclusively for one-shot scratch — never check it into shipping code.
- For motion-chain visual validation, set `bLogPresentationChain = true` in `USub3DDebugSettings` (master switch).

### Anti-patterns (refuse these)

- AI "clicking" the editor via Anthropic Computer Use during inner-loop iteration. Slow, expensive, flaky. Use MCP.
- Vision call where a `TestEqual` or a log substring match would answer the question. Burns tokens, adds noise.
- Running tests on a packaged build during inner-loop. Editor context with `EditorContext | ProductFilter` flags is the inner-loop target; packaged-build runs are CI/nightly, not iteration.
- `Take Automation Screenshot` without a checked-in baseline in `Saved/Automation/Comparisons/<TestName>/Approved/`. The diff is meaningless without ground truth.
- Asking the human to launch PIE manually as part of a routine test. Acceptable only when MCP genuinely cannot reach the asserted state (rare).

## Naming conventions (enforced by .editorconfig)

Standard Unreal prefixes: `A` (actors), `U` (UObjects), `S` (Slate widgets), `F` (structs), `E` (enums), `T` (templates), `b` (booleans). All PascalCase.

## Planning & source of truth

- **Authority-max plan**: `reports/plans/2026-04-10_first_playable_strategic_analysis.md` — decision-making frame for FP scope.
- **Active execution plan**: `reports/plans/2026-05-04_water_implementation_plan.md` — internal water Phase 0–5. Current branch `water-proto-minitest-pt4` runs proto mini-tests P-T3/P-T4 before main-line Phase 3–4 portage.
- Older plan documents are subordinate references only. If they conflict, the 2026-04-10 strategic analysis takes precedence.
- Source of truth for code is always the repository (`Source/`, `Plugins/`, `Config/`), not memory or summaries.

### System status

| System | Status | Reference |
| --- | --- | --- |
| Submarine handmade (Craniata) | Production | `BP_Submarine_Craniata`, FP target |
| Crew embarked — Local Grid Space Authority | Done | Phase 1 complete (commits 24b1cb7, a95ce17, 716d1a2) |
| Crew environment axis | Done | Phases 1+2+3 (2026-04-22/23) |
| Helm cockpit redesign | Done | PIE-validated (memory `project_helm_cockpit_redesign_2026_04_18.md`) |
| Motion-chain jitter | Done | Project Settings → Use Fixed Frame Rate 60 (memory `project_motion_chain_jitter_root_cause_2026_04_27.md`) |
| Internal water rendering | Active | Phase 0 of water plan; proto on `water-proto-*` branches |
| Ladder climb system | Backlog | `reports/plans/2026-04-27_ladder_climb_system.md`, post-helm roadmap |
| Procedural crew animation | Backlog | `reports/plans/2026-04-23_procedural_crew_animation_architecture_and_execution_plan.md` |
| Hull breaking / breach reaction loop | Backlog | Post-helm roadmap (memory `project_post_helm_roadmap_2026_04_28.md`) |
| Generator pipeline (`Sub3DBuilder`) | Paused | FP pause; bugs go to `reports/backlog/post_fp_debt.md` |
| Bake pipeline (`Sub3DBake`) | Paused | Legacy Proto03/04 only |

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
