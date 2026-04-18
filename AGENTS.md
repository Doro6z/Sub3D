# AGENTS.md - Codex GPT Extension Instructions

## Global agent rules

### Scope discipline
- Stay strictly inside the requested scope.
- Do not broaden the task unless explicitly asked.
- Do not perform large rewrites unless explicitly requested, but suggest reasonable improvements. If needed, ask the user for clarification. 

### Editing behavior
- Inspect the relevant code path before editing.
- Keep changes local, reviewable, and compile-safe.
- Do not silently refactor unrelated code.
- Do not revert unrelated user changes.
- Do not invent APIs, engine features, library calls, or behaviors. But suggest when missing.

### Output rules
- When modifying files, tell which files.
- Default to concise change summaries after edits.
- Do not include full file contents in responses unless explicitly requested.
- Do not omit critical declarations or implementation details when explaining code changes.
- No optional functionality in the current implementation path. Do not add or keep parallel convenience behavior when a single target behavior is already defined for this phase.
- Always plan for long-term editor clarity. Prefer editor-assigned widgets, explicit names, stable layout rules, and predictable asset wiring over runtime auto-spawn or implicit fallback.
- Do not use optional phrasing such as "if you want" or "you could". State the required action clearly.

### Writing quality
- For plans, specs, handoffs, and editor setup guides, use plain technical language.
- For user execution tasks, use clear and concise language. & clear objectives.
- Do not invent hybrid labels or stylish compounds such as `debug-but-durable`, `production-adjacent`, or similar AI-shaped wording.
- Every important adjective must map to a concrete property that can be observed or verified in code, assets, editor setup, or runtime behavior.
- Prefer explicit terms such as `temporary`, `persistent`, `editor-assigned`, `validation-only`, `non-final`, `runtime`, or `debug`.
- If a screen or widget is not final UI, state what it is for in operational terms, for example `used to validate gameplay behavior in PIE`.
- Avoid pseudo-precision and inflated wording such as `instrument`, `slice`, `layer`, or `reconstruction` unless the term is already defined and technically necessary.
- Prefer direct statement even if it sounds less polished.
- Prefer requirement lines that are testable, editor-checkable, or implementation-checkable.

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
- Execution must follow the current authority-max plan document:
  - `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`
- Older plan documents are subordinate references only.
- If an older plan conflicts with the 2026-04-10 strategic analysis, the 2026-04-10 strategic analysis takes precedence.

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
- No optional functionality in the current implementation path.
- Prefer editor-assigned setup and explicit asset wiring over runtime-created widget structure.

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

### Planning and handoff docs
- Plans and handoffs must optimize for execution clarity, not style.
- Do not compress multiple meanings into one adjective. Split them into separate statements.
- When describing intermediate UI or tooling, specify whether it is temporary or persistent, debug-only or player-facing, asset-bound or runtime-spawned, and validation-only or expected to ship.
- Replace vague phrases with concrete ones. Example:
  - bad: `Three debug-but-durable panels`
  - good: `Three validation panels, implemented as persistent editor-assigned widgets for this phase`
- Execution steps must reference `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md` as the authority-max plan.
- Do not invent a parallel plan or alternate workflow.

### Do not touch
- `Binaries/`
- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
