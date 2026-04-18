# Sub3D - Product North Star Implementation Plan

Date: 2026-03-29
Status: Active implementation reference
Parent vision: `C:\ACC\Projects\Sub3D\Plans\sub3d\sub3d_product_north_star.md`
Scope: implementation order for the first playable and the architecture decisions required to reach it

---

## 1. Purpose

The `North Star` document defines what Sub3D must feel like.

This document answers a different question:

How do we implement that product direction with the current repository and current proto state, without reopening too many major fronts at once?

This document is intentionally practical:
- it chooses what gets built now,
- what gets deferred,
- what runtime authority belongs to `GameMode`,
- what must live later in `GameInstance`,
- whether we should ship the first playable through a small baked traversal map or through a first real campaign runtime layer.

---

## 2. Core Decision Summary

### Decision 1 - Do NOT build the full campaign runtime now

For the first playable, the project should **not** implement the full campaign system immediately.

Do not build now:
- `UCampaignWorldManager`
- full graph streaming
- fog-of-war persistence
- seamless segment chaining across many runtime-loaded segments
- campaign save progression as a gating feature for the first playable

Reason:
- the interior submarine runtime is still not closed,
- the first playable loop is not yet proved,
- the current codebase already contains a usable route-generation and route-bake foundation,
- introducing the full campaign layer now would create a second critical runtime front before the first front is closed.

### Decision 2 - Build the first playable on a small baked traversal route

The first playable should use:
- one hand-selected baked route,
- one start dock,
- one end dock,
- one run-level,
- one submarine,
- one breach event,
- one docking completion sequence.

This is the shortest path to proving the product.

### Decision 3 - Keep the future campaign layer in the data contracts, not in runtime yet

We should still prepare the future campaign architecture by preserving:
- `RouteStartDockTransform`
- `RouteEndDockTransform`
- `CampaignSegmentID`
- route identity and build hash

But these should remain a **future integration seam**, not an immediate gameplay dependency.

---

## 3. Answer To The Main Question

### Is it simpler to go through a small baked traversal map first?

Yes.

It is not only simpler.
It is also strategically correct.

Why:
- it proves the first playable loop with the fewest new runtime systems,
- it reuses the most advanced existing worldgen/runtime pieces,
- it keeps focus on the submarine,
- it avoids splitting the team between "make the game fun" and "make the metagame shell exist" too early,
- it lets docking, sonar, breach, and interior crisis become the main validation targets.

### Should we implement a first campaign start/end traversal runtime now?

Not as a blocking runtime system for the first playable.

It is acceptable to define the campaign architecture and the future contracts now.
It is not healthy to make the first playable depend on them.

Recommended rule:
- campaign design may advance in documents,
- campaign runtime should remain data-only or stub-only until the first playable loop is closed.

---

## 4. Product Slice To Build First

The first playable should be implemented as this exact slice:

1. spawn at a start dock or underwater station
2. board or already start inside the submarine
3. free initial walk and readability pass inside the submarine
4. sit at helm
5. use sonar as the only reliable navigation tool
6. traverse one baked route segment or one baked route level
7. suffer one meaningful breach/flood event
8. either recover or limp forward
9. approach an end dock or destination station
10. perform a manual docking or docking confirmation sequence
11. succeed or fail the run

This is enough to prove:
- the fantasy,
- the loop,
- the readability,
- the submarine-first identity.

It is not enough to prove:
- campaign depth,
- broader world replayability,
- threat ecology,
- long-term progression.

That is fine.

---

## 5. Architecture Decision: GameMode / GameState / GameInstance / SaveGame

This must be explicit.

### `AGameMode` / `ASubGameMode`

Role now:
- authoritative owner of one run,
- spawn and boarding authority,
- first playable state machine owner,
- success/fail authority,
- docking completion authority,
- breach trigger authority if the event is scripted in the first playable.

Should own now:
- active submarine reference
- start dock reference
- end dock reference
- run state enum
- breach event trigger timing or trigger source
- run success/failure transitions

Should not own long-term:
- campaign persistence
- player profile meta progression
- inventory unlock history

### `AGameState`

Role now:
- replicated run-state mirror for clients.

Should own now:
- current run phase
- mission objective text/state
- docking completion state
- breach event state if needed for UI replication

This is useful even in small coop because it keeps the session readable without polluting `GameMode`.

### `UGameInstance`

Role now:
- session-wide shell only
- future home of profile/campaign entry points
- current menu-to-run transition data if needed

Should own now:
- very little
- maybe selected run configuration or seed

Should own later:
- campaign slot selection
- profile data
- unlocked codex / meta content
- long-term progression bridge

### `USaveGame`

Role:
- persistent campaign/profile storage

Not required to ship the first playable.

Can be deferred until after the run loop is validated.

---

## 6. Recommended Runtime Architecture For The First Playable

### 6.1 Level Topology

Use one level containing:
- one start dock area
- one baked traversal route
- one end dock area
- one active submarine

Do not make the first playable depend on segment streaming.

### 6.2 World Pieces

Required actors or systems:
- `ASubmarineBase` or compiled submarine actor
- one `ATraversalRouteActor` or equivalent baked route mesh actor
- one start dock / start station actor
- one end dock / end station actor
- optional simple approach corridor or final pocket around the destination

### 6.3 Sonar

The first playable should still treat sonar as non-negotiable.

Recommended:
- sonar reads from the route's existing field representation,
- sonar display is tied to helm,
- route geometry is authored/baked enough to make sonar actually useful.

### 6.4 Breach Event

The first playable breach should be deliberately staged:
- one clear trigger moment,
- one clear compartment or side hit,
- one clear visible consequence,
- one clear opportunity to react.

Do not randomize this yet.

### 6.5 Docking

Docking should be a deliberate final interaction.

It does not need to be a huge system.
It does need to be readable and manual enough that the player feels responsible for the arrival.

Minimal acceptable version:
- detect submarine near docking volume
- require alignment tolerance
- require explicit confirm/interact input
- trigger run success

---

## 7. What Already Exists That We Should Reuse

The repository already contains useful primitives:
- `ATraversalRouteActor`
- route generation and bake pipeline
- route start/end dock transforms
- `CampaignSegmentID`
- route hash/logging
- sonar field component on the route actor
- existing `ASubGameMode`
- active submarine spawn/boarding logic

This means we are not starting from zero.

The first playable should be built by reusing these parts, not by replacing them with a premature campaign runtime.

---

## 8. What Must Explicitly Wait

The following should remain deferred until after the first playable loop is closed:
- full campaign graph runtime
- multi-segment streaming manager
- fog-of-war persistence
- route-to-route seamless campaign traversal as a shipping dependency
- procedurally generated macro campaign graph in runtime
- long-term save/profile architecture

These are valid Track B topics.
They are not first playable blockers.

---

## 9. Ultra-Precise Implementation Plan

### Phase FP-0 - Lock The Product Contract

Deliverables:
- north star locked
- implementation plan locked
- explicit freeze list locked

Required outputs:
- no ambiguity on first playable scope
- no ambiguity on what is deferred

### Phase FP-1 - Build The Run Shell

Goal:
- one authoritative run state machine

Implementation targets:
- extend `ASubGameMode`
- add a small run state enum
- optionally add `ASubGameState`

Run states:
- `Boot`
- `Boarding`
- `Departure`
- `Traverse`
- `BreachCrisis`
- `Approach`
- `Docking`
- `Success`
- `Failure`

Validation:
- session can progress through phases without manual dev intervention

### Phase FP-2 - Build The First Playable Level

Goal:
- one testable run map

Implementation targets:
- place or generate one baked route
- place start dock
- place end dock
- place submarine at start
- confirm all traversal and collision are playable

Validation:
- player can start, move, helm, traverse, and reach the end area

### Phase FP-3 - Helm + Sonar Become Mandatory

Goal:
- make sonar truly central

Implementation targets:
- helm interaction path
- ping input
- readable sonar display
- no alternate easy navigation view

Validation:
- external tester can navigate using sonar, not vision cheats

### Phase FP-4 - Breach Crisis Becomes The Mid-Route Test

Goal:
- force a meaningful interior crisis

Implementation targets:
- deterministic breach trigger
- flooding consequence
- confinement consequence
- readable alarm/feedback

Validation:
- the breach creates a real decision and real consequence

### Phase FP-5 - Docking Closes The Loop

Goal:
- the run ends with a precise arrival interaction

Implementation targets:
- dock volume
- alignment check
- docking success logic
- basic end-of-run state

Validation:
- arrival is not an automatic cut
- player must actually perform the finish

### Phase FP-6 - Solo Closure

Goal:
- make the loop fully completable solo

Implementation targets:
- ensure helm can cover required actions
- ensure no mandatory second-player station
- simplify crisis actions where needed

Validation:
- full loop in under 15 minutes solo

### Phase FP-7 - Coop Closure

Goal:
- make the same loop better with 2 players

Implementation targets:
- shared state readability
- second player useful but not blocking
- no network regression in core run phases

Validation:
- 2-player run is smoother and richer, not more confusing

---

## 10. Concrete File-Level Direction

This is not an edit list.
It is the likely ownership map.

### `ASubGameMode`

Should become the first playable run authority:
- active run state
- active submarine
- route actor refs
- dock refs
- scripted breach trigger
- success/failure progression

### `ASubGameState`

Should be introduced if not already present when:
- client UI needs replicated run-state,
- docking state, breach state, or objective state must be visible to all clients cleanly.

### `UGameInstance`

Should remain light for first playable:
- selected run config
- future campaign entry point

Do not move first playable runtime there.

### `ATraversalRouteActor`

Should remain the outer-route truth for the first playable:
- baked route mesh
- route identity
- start/end transforms
- sonar field

### Dock Actors

Need lightweight dedicated actors:
- start dock or station
- end dock or station
- optional docking trigger/alignment helper

### Submarine Runtime

Must continue to mature independently:
- equalization
- flood readability
- custom swim
- death baseline
- leak sync

These are still gating the quality of the first playable.

---

## 11. Why This Is Better Than Opening Campaign Now

If we implement campaign runtime now:
- we split work between the run loop and the meta shell,
- we delay proof of fun,
- we increase debugging cost,
- we add more network state before the base loop is proven,
- we risk shipping a technically interesting shell around a half-closed core.

If we implement the first playable route now:
- we validate the product fantasy directly,
- we reuse existing route tech,
- we force sonar to matter,
- we keep the player loop readable,
- we can still preserve all future campaign seams.

This is the correct engineering decision and the correct product decision.

---

## 12. Recommended Document Order After This

After this plan, the next documents should be:

1. `sub3d_first_playable_run_spec.md`
2. `sub3d_first_playable_level_architecture.md`
3. `sub3d_helm_and_sonar_v1_spec.md`
4. `sub3d_docking_v1_spec.md`
5. `sub3d_trackB_campaign_runtime_stub_spec.md`

The important detail:
- campaign runtime should be documented next as a **stub/future seam**,
- not as the current main implementation target.

---

## 13. Final Recommendation

For Sub3D today:

- build the first playable on a small baked traversal route
- make `GameMode` the authority of one run
- keep `GameInstance` thin and future-facing
- defer the real campaign runtime
- preserve the future campaign contract in route and docking data

That is the shortest path to a real playable product proof while still preparing the long-term architecture correctly.
