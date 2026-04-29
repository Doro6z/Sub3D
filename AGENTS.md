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
- For UMG widgets and editor-bound assets, prefer editor-assigned widgets, explicit names, stable layout rules, and predictable asset wiring over runtime auto-spawn. This does NOT apply to runtime component spawning (flood water planes per compartment, doors from Definition, hull boundary components at breaches) — that is gameplay state materialization, allowed and expected.
- For First Playable scope, pragmatic hacks are acceptable IF flagged with `// TODO: post-FP` and added to `reports/backlog/post_fp_debt.md`. Long-term clarity is a post-FP concern; do not block FP iteration on it.
- Match the mode the user is in:
  - **Execution mode** ("do X", "fix Y", "apply the fix") — state the action decisively. No "if you want" / "you could" / hedging.
  - **Exploration mode** ("what do you suggest?", "should we X or Y?", "what do you think about Z?") — present 2-3 options with concrete tradeoffs. A single decisive answer is wrong here; the user is asking to choose.
  - When the mode is ambiguous, ask one clarifying question rather than guessing.

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
- Repository code is canonical (`Source/`, `Plugins/`, `Config/`).
- Memory entries and conversation summaries are valid context but **MUST be verified against current code** before any recommendation that names a specific file, function, flag, or asset path. Memories rot; refactors invalidate them silently.
- If memory states "X exists at file Y line Z", verify the file + grep before relying on it. Otherwise hedge.
- Execution must follow the current authority-max plan document:
  - `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`
- Older plan documents are subordinate references only. If an older plan conflicts with the 2026-04-10 strategic analysis, the 2026-04-10 strategic analysis takes precedence.
- If a plan document references code that no longer exists (post-refactor drift), state the drift explicitly rather than executing a stale step.

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

### Live editor introspection (UnrealClaude MCP)
- When the editor is running and the answer depends on runtime state, prefer `mcp__unrealclaude__*` tools over file reads.
- First call before any other MCP query: `mcp__unrealclaude__unreal_status` (confirms editor is alive + which level is loaded).
- Useful: `unreal_get_output_log` (live log diagnostics), `unreal_blueprint_query` (BP graph inspection), `unreal_asset_search` / `unreal_asset_referencers` (safe-to-delete checks), `unreal_capture_viewport` (visual confirmation).
- DO NOT use MCP tools when the editor is closed (they hang or fail silently).
- DO NOT use MCP tools to persist changes — those go through assets and code, not MCP.

### Automation tests
- Tests live in `Source/Sub3DTests/`. Runner command in `CLAUDE.md` "Build & launch" section. ~30 s with `-NullRHI`.
- Run before any commit touching `Source/Sub3D/Submarine/` core (movement, flood, sonar, replication contracts).
- Run after moving or renaming replicated UPROPERTYs.
- Add a new test alongside any new authoritative contract or replicated state.

### Workflow & state hygiene
- Commit cadence: a finished milestone = a commit, same day. Working tree should not accumulate beyond ~15 modified files. Split along features (Phase A, Phase C, Ladder, …), not files.
- Push to origin every day the branch has new commits. Local stash is **not a backup** — verified the hard way 2026-04-28 (locks during stash + accidental drop almost cost a day's untracked work).
- Close Unreal Editor before `git stash push -u`. Open .uasset locks cause partial stashes that leave the working tree in an ambiguous state.
- Stable tags: when the user names a commit "Stable" in its message, tag it (`git tag stable-YYYY-MM-DD <hash>`). Makes rollback trivial.
- Pre-commit sanity for new headers: build first. UHT manifest staleness on newly added .h files breaks future builds silently if not caught.
- If a stash entry is dropped accidentally, the commit hash stays in the object database for ~90 days. Recovery: `git archive <stash-hash>^3 | tar -x` extracts the untracked-files parent without touching the index.

### Do not touch
- `Binaries/`
- `Intermediate/`
- `DerivedDataCache/`
- `Saved/`
