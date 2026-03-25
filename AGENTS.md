# AGENTS.md

## Purpose
This repository contains the Unreal Engine project `Sub3D`.

Primary goals for agents working here:
- keep the project buildable,
- keep changes local and reviewable,
- avoid breaking Blueprint or asset references without an explicit migration plan,
- prefer deterministic verification over guesswork.

## Source Of Truth
The source of truth is the repository itself, in this order:
- `Source/`
- `Plugins/` that belong to this project
- `Config/`
- `Sub3D.uproject`
- project build scripts under `scripts/`

Memory systems, chat history, and generated summaries are secondary context only.

## Project Shape
Current project areas include:
- `Source/Sub3D/Submarine/`: submarine gameplay systems, stations, movement, interaction, runtime components
- `Source/Sub3D/ProcGen/`: procedural generation, traversal generation, chunk assembly, validation
- `Source/Sub3D/WorldGen/`: world graph, routes, topology, connectors, navigable volumes, campaign world systems
- `Plugins/`: project-local plugins and integrations
- `Config/`: Unreal project configuration

Agents should preserve existing module boundaries unless the task explicitly requires restructuring.

## Write Scope
Agents may modify:
- `Source/`
- `Plugins/` owned by this project
- `Config/`
- `scripts/`
- repository documentation

Agents must not modify unless explicitly requested:
- `Binaries/`
- `DerivedDataCache/`
- `Intermediate/`
- `Saved/`
- `.vs/`
- generated IDE state, cache directories, or machine-local settings outside the repo

## Working Rules
- Start by inspecting the relevant code path before editing.
- Keep changes narrow. Avoid drive-by refactors.
- Do not revert unrelated user changes.
- Prefer ASCII unless the file already uses Unicode and there is a reason to keep it.
- Add brief comments only when the code would otherwise be hard to parse.
- Preserve public behavior unless the task explicitly changes behavior.

## Git Discipline
Before editing:
- check the relevant files and local git status

After editing:
- review a focused diff
- summarize user-visible behavior changes, risks, and verification status

Never do the following unless explicitly requested:
- destructive resets
- unrelated formatting sweeps
- mass renames across gameplay and Blueprint-facing classes

## Standard Verification Commands
Agents should prefer repository scripts over ad hoc commands.

Expected standard commands:
- `pwsh -File scripts/build-editor.ps1`
- `pwsh -File scripts/build-game.ps1`
- `pwsh -File scripts/test.ps1`
- `pwsh -File scripts/gen-compile-db.ps1`

If a script is missing, create or update it instead of repeating long shell commands in prompts.

## Unreal Engine Rules
- Respect Unreal reflection requirements for `UCLASS`, `USTRUCT`, `UENUM`, `UPROPERTY`, and `UFUNCTION`.
- Avoid renaming reflected types or properties that may be referenced by assets or Blueprints without an explicit redirect or migration plan.
- Be careful with constructor defaults, component initialization, and replication-related behavior.
- Check `*.Build.cs` when introducing new engine modules or plugin dependencies.
- Treat serialization-sensitive changes as high risk.
- Prefer small changes that keep editor and cooked builds viable.

## C++ Rules
- Prefer compile-safe, local edits.
- Use forward declarations where appropriate, but not where Unreal header tooling requires full includes.
- Minimize header bloat and unnecessary transitive includes.
- Keep public interfaces stable unless the task requires a breaking change.
- Do not introduce large utility abstractions without proving repeated need in this codebase.

## Refactoring Rules
For medium or large refactors:
1. identify the owning subsystem,
2. map Blueprint and asset-facing risk,
3. make the smallest coherent code move,
4. run at least one deterministic verification step,
5. report residual risk clearly.

If a refactor touches multiple gameplay systems, agents should prefer staged changes over one large rewrite.

## Clangd And Compile Database
For C++ navigation, diagnostics, and refactoring quality, agents should rely on `clangd` with a valid `compile_commands.json`.

Rules:
- regenerate the compile database after build configuration changes,
- do not assume IntelliSense diagnostics are equivalent to clangd diagnostics,
- if clangd and Unreal build output disagree, treat Unreal build output as final.

Preferred location:
- `compile_commands.json` at repo root when feasible

## MCP And Tooling Policy
Recommended tool roles:
- Filesystem MCP: read and write only inside this repository
- Memory MCP: store architectural decisions, conventions, and active workstreams only
- Unreal MCP: editor automation and inspection only

Not recommended as primary truth sources:
- long-lived memory entries describing code that may have changed
- tool summaries that are not backed by current repository state

## Memory Policy
Good memory entries:
- subsystem ownership notes
- architecture decisions
- naming conventions
- current milestone goals
- known risky files or migration constraints

Bad memory entries:
- copied code
- stale API descriptions
- exact behavior claims not validated against current source

## Expected Workflow
Default workflow for code tasks:
1. inspect relevant files
2. identify constraints and impact radius
3. edit narrowly
4. verify with scripts or the smallest reliable command
5. report what changed, what was verified, and what remains unverified

## High-Risk Areas In This Repo
Extra caution is required in:
- traversal and route generation code under `ProcGen/` and `WorldGen/`
- Blueprint-facing gameplay classes under `Submarine/`
- plugin boundaries and module dependencies
- config changes affecting input, gameplay tags, maps, or engine settings

## Definition Of Done
A task is not done until:
- the requested code or config change is present,
- the change has been reviewed against surrounding code,
- at least one relevant verification step has run when feasible,
- unverified risk is stated explicitly if verification could not be completed

## Communication
Agent responses should be concise and factual.

When reporting results:
- lead with findings or outcome,
- include concrete file references,
- state verification performed,
- state blockers or residual risk without padding.
