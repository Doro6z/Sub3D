# Sub3D — Agent Rules

## Scope
- Modify only what is required.
- Keep changes minimal, local, and reviewable.
- Do not refactor unrelated systems.

## Source of truth
- Repository code only (`Source/`, `Plugins/`, `Config/`)
- Do not rely on memory, summaries, or assumptions.

## Architecture constraints
- Submarine = authoritative moving frame.
- Crew = traversal on that frame.
- Do not collapse movement, replication, and presentation layers.

## Runtime / Movement
- Treat authority, replication, tick order, and movement ownership as critical.
- For movement bugs:
  - identify conflict between systems first
  - prefer removing conflicts over adding systems
- Do not assume stock UE behavior is sufficient.

## Unreal safety
- Do not rename reflected types without migration plan.
- Be careful with replication, serialization, and constructor defaults.
- Check `.Build.cs` when adding dependencies.

## Editing rules
- No drive-by refactors.
- No speculative abstractions.
- No invented Unreal APIs.
- Keep compile-safe code.

## Output rules
- Full files for every modified file.
- No partial snippets.
- No "rest unchanged".
- No omitted sections.

## Debug discipline
- Add logs before rewriting systems.
- Make debug toggles editor-accessible when possible.
- Distinguish:
  - root cause
  - possible contributor
  - out-of-scope issues

## Verification
- Prefer project scripts when available.
- State clearly:
  - what was verified
  - what remains uncertain

## Do not touch
- Binaries/, Intermediate/, DerivedDataCache/, Saved/