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

## Generation pipeline (core architecture)

This is the central system. All submarine geometry and gameplay layout flows through it:

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
- **No runtime auto-spawn for UI** — Prefer editor-assigned widgets, explicit names, stable layout rules, and predictable asset wiring.

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
- Tick prereqs: `SubFlood → SubMovement → InteriorFrame → CrewMovement`. `SubFlood → SubMovement` is set in `USubMovementComponent::BeginPlay`; the crew-side chain is set in `InitializeForSubmarine`.

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
- **UnrealClaude** — Claude AI integration for Unreal Editor.

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

- **Stairs → Ramps.** `SM_Stair_*` meshes inside `BP_Submarine_Craniata` (e.g. `SM_Stair_UpperToMain_UpperAccess`) still use complex collision. The step-edge discontinuity produces micro-jitter on walk transitions (Deck ↔ Stair). **Fix:** replace stair meshes with ramp meshes (single planar slope, simple collision). Until done, this jitter is an asset problem, NOT a rebase architecture regression.

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
