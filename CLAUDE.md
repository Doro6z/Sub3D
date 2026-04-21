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
- `bIgnoreBaseRotation = bIsGridSpaceAuthority` — disable CMC's built-in base-rotation carry.
- `UpdateBasedMovement` and `UpdateBasedRotation` are no-ops when `bIsGridSpaceAuthority`.
- Controller yaw delta is applied in `TickComponent` (pre-CMC) from `SubRot.Yaw - LastSubWorldTransform.Rotator().Yaw`.
- Tick prereqs: `SubMovement → InteriorFrame → CrewMovement` (set in `InitializeForSubmarine`).

**Legacy to be removed — do NOT extend:**

- `ApplyYawCompensation()` — obsolete, rebase handles rotation.
- Crew tether (`bEnableCrewTether`, world-snap safety net added 2026-04-21) — obsolete, rebase does not drift.
- `UpdateBasedMovement`/`UpdateBasedRotation` as transport path — no-op'd in grid mode.
- Treating `MovementBase` as the carry mechanism — it still exists for CMC's floor-finding, but no longer transports the crew.

**Kept as validation harness:**

- `USub3DDebugSettings` (Project Settings > Game > Sub3D Debug) — all debug toggles live here.
- `bLogCrewJitter` + related fields in Crew category — per-tick diagnostic log for verifying the new architecture holds. The SPIKE threshold (`CrewJitterWarnVelocityCmPerSec`, default 1200 cm/s) should now stay clean.

**Implementation phases:**

- **Phase 1** — solo PIE validation. No FSavedMove override yet; network off. Jitter must reach zero in solo before Phase 2.
- **Phase 2** — override `FSavedMove_Character` to carry `GridSpaceTransform` in the net payload, `ServerMove` validates in local space, replicate `GridSpaceTransform` with `COND_SkipOwner`.

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
