# Sub3D Proto 03 - DA, Spatiality, and Asset Realization Plan

Date: 2026-03-20  
Project: `C:/Dev/Sub3D`

Companion documents:

- `C:/Dev/Sub3D/reports/plans/20260320-proto03-first-sub-art-brief.md`
- `C:/Dev/Sub3D/reports/plans/20260320-proto03-first-sub-blockout-sheet.md`

## 1. What Is Already Clear About The DA

The submarine direction is not "naval military sim" by default.

What is already stable from the current discussion:

- Submarines are not only weapons or exploration craft. They are also habitat, cargo tool, checkpoint runner, work machine, and status-bearing asset.
- They are hard to acquire, precious, robust, customized over time, and usually more functional than refined.
- Piloting is skilled labor. Bad navigation can mean disappearance, not a mild setback.
- The world is organized around hostile traversal between settlements, checkpoints, and larger cities.
- Different captains and factions should bias ships toward transport, exploration, combat, salvage, or hybrids.

This points to a DA family closer to:

- industrial habitat
- pressure caravan
- underwater convoy vehicle
- working machine with living quarters

Not this:

- clean military submarine
- sleek luxury sci-fi yacht
- pure NASA lab aesthetic
- tiny arcade mini-sub as the baseline

## 2. DA Direction Recommended For Proto 03

For the prototype, the best anchor is:

**"Independent utility habitat / checkpoint runner"**

Why this is the right proto baseline:

- it supports helm gameplay
- it supports repair and breach gameplay
- it supports cargo or storage logic later
- it can belong to civilian, militia, trader, scavenger, or mixed factions with only surface changes
- it fits your lore better than a pure attack sub

The proto ship should feel:

- heavy
- pressurized
- lived-in
- improvised but reliable
- industrial
- asymmetrical in use, not in structural readability

## 3. What The Current Structure Imposes On Design

The current implementation does not force the art style, but it does impose strong spatial and structural constraints.

### Spatial constraints

- Third-person traversal needs one main clear movement spine.
- Repair, panic movement, flooding, and suction all need readable circulation.
- Walls, ceiling, corners, and floors can all become breach sites.
- If the player can be pulled toward ceiling or side breaches, the room must remain navigable under stress.

### Structural constraints

- The current hull logic uses logical sheets, not pre-cut mesh panels.
- Exterior and interior do not need literal engineering-grade matching, but they must be broadly believable as one pressure vessel.
- You can use simple continuous hull surfaces; you do not need visual segmentation to match runtime damage.
- Internal bulkheads should stay mostly planar or gently curved for readable authoring and repair gameplay.

### What that means for asset design

- Do not fill every wall with deep furniture.
- Keep a "service lane" and a "player lane".
- Prefer wall-mounted machinery, overhead cable trays, shallow lockers, exposed pipe runs.
- Avoid hyper-fragmented interior geometry for the prototype.
- Large continuous hull surfaces are good. They help VFX, decals, damage masks, and readability.

## 4. Spatiality Is Part Of The Asset Plan

Yes. It is not separate. Spatiality is one of the main drivers of the asset plan.

For Proto 03, the submarine is not just a mesh set. It is:

- a circulation layout
- a repair arena
- a flooding volume
- a line-of-sight and landmarking problem
- a support for panic navigation

The first asset decision is therefore not "what prop do I model first?"

It is:

**"What is the navigable spatial grammar of the first submarine?"**

## 5. Recommended Spatial Grammar For The First Ship

Use the current `submarine layout proto03` image as the canonical gameplay base.

That layout is the best of the references because it gives:

- one obvious main corridor
- a readable helm zone
- a readable engine/pump zone
- enough side surface for repair events
- clean third-person movement

Recommended adjustments:

- keep the long central spine
- add one asymmetric side nook in the mid or rear section
- keep the helm slightly widened, not cramped
- make the engine area denser than the corridor, but never maze-like
- use one or two bulkheads only, not many

## 6. Recommended Dimensions For Blockout

These are prototype gameplay dimensions, not naval realism targets.

- Overall exterior length: `14m` to `16m`
- Overall exterior width: `5m` to `6m`
- Overall exterior height: `4.5m` to `5.5m`
- Main corridor clear width: `2.2m` to `2.4m`
- Secondary nook clear width: `1.6m` to `2.0m`
- Clear standing space in front of console: `1.4m` to `1.8m`
- Clear repair frontage on critical walls: at least `1.2m`
- Door clear width: `1.2m` to `1.4m`
- Door clear height: `2.1m` to `2.2m`
- Helm room usable diameter: `3.2m` to `3.8m`
- Engine/pump room length: `3.0m` to `4.0m`

If a space is narrower than this, it may still look believable, but it will fight the intended gameplay.

## 7. Reading Of The Provided Images

### Image 1 - small rusted utility sub with turret

Good:

- communicates "working machine"
- faceted pressure-hull language is usable
- grime and protected outer rails fit the world

Bad:

- too close to militia or armed escort as a default baseline
- scale reads small and cramped
- turret pushes the read toward combat-first
- too object-like, not enough habitat-like

Verdict:

- good as one faction or cheap escort variant
- not the best base for the first proto ship

### Image 2 - "rust bucket" surface craft with front fan

Good:

- rough utility mood
- broad, chunky front mass

Bad:

- too many AI nonsense details
- front propulsion read is weak for your world
- surface-water presentation fights the deep hostile traversal fantasy
- not a good guide for interior organization

Verdict:

- discard as structural reference
- keep only as "dirty utility" mood, if at all

### Image 3 - larger industrial cargo sub with bright front windows

Good:

- strongest "habitat + machine" read
- bigger scale feels right for your lore
- multiple systems and appendages support a multi-role vessel
- silhouette suggests cargo, route-running, and faction identity

Bad:

- too many windows for a hostile pressure world if used literally
- AI clutter risk on the upper deck and appendages
- front glazing is visually strong but may become overdesigned fast

Verdict:

- strong macro inspiration
- use as silhouette and tone reference
- simplify heavily before modeling

### Image 4 - cutaway futuristic interior

Good:

- good density reference
- useful for "machine + habitation" layering
- good proof that a long internal volume can read clearly

Bad:

- too polished and premium
- too diagrammatic to trust as direct build data
- lighting and finish are more premium sci-fi than rough industrial

Verdict:

- use for mood and density only
- do not copy literally

### Image 5 - top-down `submarine layout proto03`

Good:

- best gameplay readability
- best circulation logic
- best alignment with repair/flooding/breach design
- immediately blockout-able

Bad:

- still needs faction flavor and external silhouette work
- risks feeling too clean if copied without asymmetry and lived-in layering

Verdict:

- this should be the canonical proto blockout

## 8. Immediate Direction To Avoid Scope Drift

Do not build an entire modular submarine editor asset pack now.

For the prototype, build:

- one bespoke submarine shell
- one bespoke interior layout
- one small modular dressing kit

That is enough to unblock:

- navigation
- helm use
- breaches
- repair
- flooding
- suction
- first art direction validation

If you jump straight to full modular authoring, you will delay the prototype for very little validation value.

## 9. Asset Production Plan

### Phase 0 - lock the art sentence

Deliverable:

- one written DA sentence
- one page of keywords
- one page of anti-keywords

Recommended DA sentence:

**"A precious industrial undersea habitat-machine: heavy, functional, lived-in, repairable, and customized by its crew."**

### Phase 1 - gameplay blockout first

Deliverable:

- top view layout
- side view silhouette
- rough outer hull around real interior volume

Work in Blender or Unreal, not in image generation first.

Rules:

- block out the interior first
- derive the exterior from it after
- keep one spine corridor
- include one sleeping/storage nook
- include one engine/pump zone
- include one exposed repair wall in each major zone

### Phase 2 - proto asset set

Build only the assets required to run the loop.

#### Structural assets

- `SM_Sub_Proto03_Hull_A`
- `SM_Sub_Proto03_InteriorShell_A`
- `SM_Sub_Proto03_BulkheadDoor_A`
- `SM_Sub_Proto03_Hatch_A`

#### Gameplay station assets

- `SM_HelmConsole_A`
- `SM_PumpConsole_A`
- `SM_EngineBlock_A`
- `SM_RepairPanel_A`
- `SM_PowerBox_A`

#### Lived-in utility assets

- `SM_BunkFrame_A`
- `SM_LockerBank_A`
- `SM_CrateStack_A`
- `SM_ShelfIndustrial_A`

#### Dressing kit

- `SM_Pipe_Straight_A`
- `SM_Pipe_Elbow_A`
- `SM_Pipe_T_A`
- `SM_ValveWheel_A`
- `SM_CableTray_A`
- `SM_Lamp_Industrial_A`
- `SM_Bracket_A`
- `SM_Handrail_A`

#### Material set

- `M_HullPaintedSteel_A`
- `M_InteriorSteel_A`
- `M_RubberFloor_A`
- `M_GlassHeavy_A`
- `MI_FactionStripe_A`
- `MI_HazardMarking_A`

### Phase 3 - UE dressing

Do cables, hoses, and some pipe runs in Unreal where possible.

Recommended in Unreal:

- spline cables
- spline hoses
- repeated pipe runs
- warning lights
- loose clutter placement

Recommended as modeled assets:

- main consoles
- engine mass
- bunk and storage units
- hull shell
- bulkhead doors

Reason:

- spline-driven dressing is cheaper to iterate
- it avoids overcommitting Blender time too early

## 10. AI Workflow Recommendation

For the main submarine, do **not** start with Text-to-3D as the primary source.

### For the first ship

Best workflow:

1. manual gameplay blockout
2. render clean clay orthographic and 3/4 views from that blockout
3. use those images as paintover or concept guidance
4. manually model the actual gameplay ship

### Where AI is useful

- concept overpaints
- silhouette exploration
- mood exploration
- secondary props
- material idea exploration

### Where AI is weak for this task

- coherent inside/outside relationship
- precise gameplay blockout
- reliable topology for a navigable hero ship
- maintaining one readable faction language across a full kit

## 11. Text-to-3D vs Image-to-3D

### Text-to-3D

Use for:

- secondary props
- fast ideation
- throwaway exploratory shapes

Do not use as the main source for the first playable submarine.

### Image-to-3D

Better than Text-to-3D only if the input images are already disciplined.

That means:

- neutral background
- one clear object
- minimal atmospheric effects
- top, side, front, and 3/4 reference if possible
- no underwater fog, no cinematic bloom, no heavy motion cues

If you want to use Image-to-3D well, create the blockout first and render a clean concept plate from it.

## 12. AI Tool Budget Snapshot

This section is a dated pricing snapshot and should be rechecked before purchase.

### Meshy

- Free: `100` monthly credits, limited downloads
- Pro: `20 USD/month` or `192 USD/year`
- API pricing uses credits; official docs list per-call credit costs that vary by model and mode

Use case for you:

- good for quick prop ideation
- acceptable for throwaway exploration
- not my first choice for the main playable submarine

### Tripo

- Basic: free tier
- Professional: `19.9 USD/month` or `143.28 USD/year`
- Advanced: `49.9 USD/month` or `359.28 USD/year`

Use case for you:

- good candidate for cleaner hero prop exploration
- better than text-only ideation when you already have image guidance

### Hyper3D / Rodin

- Creator: `24 USD/month`
- Business: `120 USD/month` after discounted first month shown on pricing page
- API docs currently expose credit-based pricing, including `0.5` credit per Rodin generation and `0.5` credit per texture generation; Gen-2 API requires Business tier

Use case for you:

- high interest if you want higher-quality controlled experiments
- overkill for the first playable blockout unless you already know the pipeline

### Sloyd

- Plus: `15 USD/month`
- Pro: `50 USD/month`
- unlimited exports on paid tiers according to pricing page

Use case for you:

- strongest value for modular props and controllable game-ready support
- better fit than pure generative 3D for your prototype asset kit

## 13. Recommended Spend Strategy

If budget is tight:

- do not subscribe to three tools at once
- block out manually
- use one controllable tool for props

Recommended tiers:

- cheapest pragmatic path: `Sloyd Plus` only
- balanced path: `Sloyd Plus + Meshy Pro`
- quality experiment path: `Sloyd Plus + Tripo Professional`

I do **not** recommend paying for Rodin Business yet unless you specifically decide to test API-driven or higher-fidelity generation as a production path.

## 14. Questions That Actually Matter Now

These are the questions worth answering before serious asset production.

### Must-answer now

- Is the first ship solo/duo crew scale or small crew scale?
- Is the first ship more "route runner" or more "combat escort"?
- Do you want rare small windows, or almost no windows?
- Is the baseline faction improvised civilian, industrial contractor, or checkpoint authority?

### Can wait

- exact faction color coding
- late-game ship class tree
- full editor-grade modular grammar
- full city docking language

## 15. Recommended Answers For The Prototype

To unblock production now, I recommend locking these:

- Crew scale: `small crew`
- Function: `small exploration route-runner with repair-heavy internals`
- Window policy: `few small reinforced windows only`, with rare exceptions if they strengthen mood and remain pressure-believable
- Faction baseline: `independent civilian / industrial`
- Interior tone: `workable and inhabited, not luxurious`
- Exterior tone: `pressure-rated workhorse, not navy sleek`

Locked from user follow-up:

- Intended occupancy supports `coop or AI crew navmesh`
- Primary fantasy is `small exploration submarine`, not escort combat
- Windows are allowed, but they must read as `rare, thick, reinforced, and expensive`

## 16. Final Recommendation

Build one submarine first as a **habitable industrial route-runner**, not a war sub.

Use the `submarine layout proto03` top-down as the gameplay canon.

Block out manually before any AI generation.

Use AI only for:

- prop ideation
- mood boards
- paintover support
- secondary shapes

This is the fastest path that still preserves gameplay truth.

## 17. Sources For Pricing Snapshot

- Meshy pricing: `https://www.meshy.ai/pricing/`
- Meshy pricing help article: `https://help.meshy.ai/en/articles/12062933-what-are-your-prices-and-plans-offered-and-do-you-have-monthly-annual-plans`
- Meshy API pricing: `https://docs.meshy.ai/api/pricing`
- Tripo pricing: `https://www.tripo3d.ai/pricing`
- Tripo API page: `https://www.tripo3d.ai/api`
- Hyper3D pricing: `https://hyper3d.ai/subscribe`
- Hyper3D API pricing docs: `https://developer.hyper3d.ai/api-specification/rodin-generation`
- Hyper3D texture API pricing docs: `https://developer.hyper3d.ai/api-specification/generate-texture`
- Sloyd pricing: `https://www.sloyd.ai/pricing`
