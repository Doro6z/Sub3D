# Sub3D - FirstPlayableRun Editor Session Contract

Date: 2026-04-10
Status: Pre-editor execution contract
Scope: FirstPlayableRun setup and validation in editor

---

## 0. Why this document exists

We are changing work mode.

Until now, the project was in a code-heavy phase:
- main execution load on the agent
- human role mainly review, direction, arbitration
- outputs mainly code, build, review

The next phase is editor-heavy:
- main execution load shifts back to the human
- work becomes more fragmented and more tiring
- setup, asset wiring, level assembly, and PIE validation require direct editor actions

This change must be explicit.
It must not be treated as a simple continuation of the code phase.

The goal of this document is to make the editor session:
- bounded
- operational
- low ambiguity
- easy to stop cleanly

---

## 1. Session Goal

Assemble and validate the real FirstPlayableRun in editor with the generator path as the intended default path.

Concrete target:
- open a clean FP level
- place the FP submarine actor
- assign a valid `GeneratorSpec`
- run PIE
- verify that the generated runtime path works
- use `[LEGACY]` warnings as setup failure signals

This session is not for:
- new gameplay systems
- broad C++ work
- player-facing editor tooling
- large cleanup
- compile-time deprecation

---

## 2. Preparation Before Opening the Editor

Do not open the editor until this checklist is ready.

### Required decisions

1. Identify the FP level to use or create
   - target: one clean dedicated level for FirstPlayableRun
   - no old proto level reused as base

2. Identify the submarine actor or Blueprint to use
   - this must be the single FP submarine actor for the session

3. Identify the generator assets required
   - `USubmarineGeneratorSpec`
   - `USubmarineEnvelopeDef`
   - any required station class mappings already expected by runtime

4. Confirm the validation target
   - the FP submarine must use the generator path
   - the fallback `LayoutAsset` path is allowed to still exist in code, but it must not be the normal path for this FP submarine

### Required editor inputs

Before opening the editor, have the following names or paths ready:
- FP level name
- submarine Blueprint or placed actor name
- `GeneratorSpec` asset name
- `EnvelopeDef` asset name

### Out of scope before opening

Do not decide now:
- final art materials
- final lighting polish
- final VFX polish
- permanent campaign save/load
- player-facing submarine editor

---

## 3. Exact Actions in the Editor

Execute in this order.

### Step 1 - Open or create the FP level

Target shape:
- one clean dedicated level
- one submarine
- one player spawn
- minimal readable ocean setup

No reuse of a proto level as hidden foundation.

### Step 2 - Place or select the FP submarine actor

Use exactly one intended FP submarine actor.

Check on the actor:
- `GeneratorSpec`
- `GeneratedDefinition`
- `SubFlood`
- `GeneratedGeometry`
- station manager presence

### Step 3 - Assign the generator path

Required:
- assign a valid `GeneratorSpec`
- ensure the intended FP path does not depend on `LayoutAsset`

`GeneratedDefinition` may be:
- generated at runtime from `GeneratorSpec`
- or pre-assigned if that is the chosen test path

But the intended validation target is the generated path.

### Step 4 - Minimal scene setup

Create only what is required to validate gameplay behavior:
- player spawn
- camera/reference framing
- simple ocean plane or water volume
- simple fog / post-process
- simple readable dark lighting

This is validation-only setup, not final presentation.

### Step 5 - PIE validation

Run PIE and verify in order:

1. submarine appears
2. generated geometry appears
3. hull collision exists
4. floor is walkable
5. stations exist and are usable enough for validation
6. crew spawn is valid
7. `SubFlood` initializes
8. a hull impact can create a breach / inflow
9. a closed door blocks transfer
10. an open door allows transfer

### Step 6 - Log check

Watch the output log during PIE.

Critical rule:
- the FP submarine must not emit `[LEGACY]` warnings during normal generated-path setup

In particular, none of these should appear for the FP submarine:
- `ASubmarineBase::BeginPlay` fallback to `LayoutAsset`
- `USubFloodComponent::InitializeFromLayout`
- `UFloodWaterVisualsComponent` fallback to `LayoutAsset`
- `ASubCrewCharacter::ResolveCurrentCompartment` fallback to `LayoutAsset`

If one of those appears, the session is not a pass.

---

## 4. What We Ignore in This Session

Ignore all of this unless it blocks the session directly:
- final environment art
- final submarine materials
- final audio pass
- final UI pass
- editor tooling for player submarine creation
- large refactors
- broad legacy cleanup
- `UE_DEPRECATED` work
- campaign persistence
- non-FP levels
- polish unrelated to generator-path validation

---

## 5. When to Stop

Stop the session immediately when one of these conditions is met:

1. The FP submarine is in level with a valid `GeneratorSpec`, PIE runs, and the generated path is confirmed with no `[LEGACY]` warnings.

2. A concrete blocker is identified that cannot be resolved inside the editor session alone.

3. The session starts drifting into art polish, refactor work, or unrelated setup.

Do not extend the session just because the editor is already open.

---

## 6. What To Report Back After the Session

Return with a short structured report:

### Required report items

1. Level used
2. Submarine actor or Blueprint used
3. `GeneratorSpec` asset used
4. Whether `GeneratedDefinition` was runtime-generated or pre-assigned
5. Whether PIE succeeded
6. Whether any `[LEGACY]` warning appeared
7. Exact blocker, if any
8. Whether the FP submarine is ready for fallback removal

### Minimal result format

Use this exact structure:

```text
FP Editor Session Result

Level:
Submarine actor/BP:
GeneratorSpec:
GeneratedDefinition path: runtime-generated / pre-assigned

PIE result:
Legacy warnings seen: yes / no

Validated:
- ...
- ...

Blockers:
- ...

Ready for next step:
- yes / no
```

---

## 7. Exit Criteria For The Next Code Step

The next code step may begin only if all of the following are true:
- FP submarine uses the generator path in practice
- PIE succeeds
- no `[LEGACY]` warning is emitted for the FP submarine
- generated geometry, flood path, and walkable surfaces all validate

Only after that:
- remove runtime `LayoutAsset` fallbacks for the FP path
- then consider Phase 7B compile-time deprecation

Until then:
- keep 7B blocked
- keep the session editor-focused

