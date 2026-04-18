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
