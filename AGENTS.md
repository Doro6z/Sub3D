# AGENTS.md - Codex GPT Extension Instructions

## Global agent rules

### Scope discipline
- Stay strictly inside the requested scope.
- Prefer the smallest correct patch.
- Do not broaden the task unless explicitly asked.
- Do not perform large rewrites unless explicitly requested.

### Editing behavior
- Inspect the relevant code path before editing.
- Keep changes local, reviewable, and compile-safe.
- Do not silently refactor unrelated code.
- Do not revert unrelated user changes.
- Do not invent APIs, engine features, library calls, or behaviors.

### Output rules
- When modifying files, provide complete contents for every modified file unless explicitly asked for diffs only.
- Do not provide partial snippets only.
- Do not say "rest unchanged".
- Do not omit includes, macros, class members, declarations, or method bodies.

### Debugging and runtime work
- For runtime bugs, prefer instrumentation and observability before deep rewrites.
- Distinguish clearly between:
  - likely root cause
  - possible contributor
  - deferred concern
- Add logging and debug toggles when hidden runtime state is central to the problem.

### Multi-file coherence
- Keep header and cpp changes coherent.
- Preserve existing architecture unless structural change is explicitly requested.
- Avoid speculative abstractions.

### Verification
- Prefer deterministic verification over guesswork.
- State clearly what was verified and what remains unverified.

## Sub3D project rules

### Scope
- Modify only what is required.
- Keep changes minimal, local, and reviewable.
- Do not refactor unrelated systems.

### Source of truth
- Repository code only (`Source/`, `Plugins/`, `Config/`).
- Do not rely on memory, summaries, or assumptions.

### Architecture constraints
- Submarine = authoritative moving frame.
- Crew = traversal on that frame.
- Do not collapse movement, replication, and presentation layers.

### Runtime and movement
- Treat authority, replication, tick order, and movement ownership as critical.
- For movement bugs:
  - identify conflict between systems first
  - prefer removing conflicts over adding systems
- Do not assume stock UE behavior is sufficient.

### Unreal safety
- Do not rename reflected types without migration plan.
- Be careful with replication, serialization, and constructor defaults.
- Check `.Build.cs` when adding dependencies.

### Editing rules
- No drive-by refactors.
- No speculative abstractions.
- No invented Unreal APIs.
- Keep compile-safe code.

### Debug discipline
- Add logs before rewriting systems.
- Make debug toggles editor-accessible when possible.
- Distinguish:
  - root cause
  - possible contributor
  - out-of-scope issues

### Verification
- Prefer project scripts when available.
- State clearly:
  - what was verified
  - what remains uncertain

### Do not touch
- `Binaries/`
- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
