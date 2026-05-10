# Tilt-Aware Multi-Compartment Water Simulation — Algorithm Comparison

Captured 2026-05-10 from research agent. Source for the post-FP "intra-compartment water redistribution" workstream noted in [post_fp_debt.md](../backlog/post_fp_debt.md).

User constraint reframe (from session 2026-05-10):

> Water plane horizontal in **world** (gravity dictates). 3 compartments at same deck closed = 3 distinct water planes. Open doors = communicating vessels in world-space, gravity redistributes until equilibrium. Sim must reject "door open but water doesn't physically reach door" — no transmission unless water level exceeds the door's world Z under current sub orientation.

## Recommendation summary

**Adopt Approach 1 (volume-as-scalar with world-horizontal evaluation) + Approach 4 (plane-clipped cap mesh) for First Playable continuation. Defer Approach 2 (heightfield + modal slosh) to post-FP polish. Discard 3 and 5.**

Estimate: **3–5 days** to land Approach 1+4 with tests. Matches the Phase 4 portage cadence in [2026-05-04_water_implementation_plan.md](../plans/2026-05-04_water_implementation_plan.md).

## Approach 1 — Augmented scalar with tilt-aware surface evaluation (RECOMMENDED)

Keep one scalar per compartment but interpret as **fluid volume** `V` instead of `WaterHeightCm`. Each tick: solve for the world-horizontal plane `z = z_world` such that the volume of `(boxes ∩ half-space z < z_world)` equals `V`. Inter-compartment flow uses `Δh_world` between pairs of compartment world-surface planes evaluated at door centers (existing Bernoulli/weir formula, just `Δh` in world Z).

For axis-aligned-to-sub boxes the volume-given-plane integral has a closed form: piecewise-cubic in `z_world` with breakpoints at corner Z values. 2–3 iterations of Newton or bisection on the cumulative cubic resolves `z_world` from `V` in microseconds.

| Aspect | Detail |
|---|---|
| Pros | Minimal data change (rename + reinterpret). Reuses existing flow solver. Single float per compartment replicated. Cap mesh becomes world-horizontal plane clipped by box corners → visually coherent with sim by construction. Communicating vessels correct. |
| Cons | Uniform fill within a compartment — wrong under sloshing or sudden tilt. For 0–30° steady-state piloting, acceptable. |
| Cost | ~1–5 µs/compartment/tick. 10 compartments × 60 Hz = trivial. |
| Complexity | 2–3 days incl. cumulative-volume LUT bake. |
| Replication | One float (volume) per compartment. Deterministic plane derived client-side from `V` + sub transform. |

## Approach 2 — Per-compartment 2D heightfield + tilt forcing

Each compartment carries a small grid (16×16) of water depths `h(x,y)` in sub-local XY. Each tick: integrate **shallow-water equations** with body-force = world-down vector projected to sub-local XY. Naturally produces sloshing, traveling waves, intra-compartment tilt response.

| Aspect | Detail |
|---|---|
| Pros | True sloshing + tilt redistribution. Heightfield is the structure already specced for P3.6 surface waves. Door coupling = pipe flow at boundary cells. Industry-validated (Müller 2007 GDC, Houdini). |
| Cons | 16×16 floats × 10 compartments to replicate is too much (~30 KB/tick). Mitigation = replicate only mean + small modal coefficients. |
| Cost | ~30 µs/compartment/tick CPU. 10 compartments = 300 µs. CFL stable at 60 Hz. |
| Complexity | 1–2 weeks. |
| Replication | Mean (1 float) + 4–8 modal coefficients per compartment. Client reconstructs heightfield. |

Use **post-FP** when intra-compartment dynamics matter (breach jets, fast tilts, deliberate slosh gameplay).

## Approach 3 — Niagara FLIP / particle / cellular automaton

Niagara FLIP is GPU-only with global-distance-field collisions. Multi-compartment isolation is hard. No clean server-authoritative path: particle state is not replicable. GTX 1660 Super doesn't fit even one FLIP grid at game-relevant resolution.

| Aspect | Detail |
|---|---|
| Cost | Niagara FLIP at 32³ per compartment ≈ 1–2 ms each. 10 compartments = 10–20 ms. Eats entire 16 ms budget. |
| Verdict | **Reject** — wrong tool, wrong budget. |

## Approach 4 — Plane-clipped surface mesh (RENDERER, pairs with #1)

No sim refinement. Each frame: build the surface polygon = world-horizontal plane `z = z_world` intersected with the union of CV boxes (Sutherland-Hodgman per box, merged for L-shapes). Procedural mesh component with vertices regenerated each tick.

This **is the renderer for Approach 1 or 2**, not a competing approach. Treat as a building block.

| Aspect | Detail |
|---|---|
| Pros | Mathematically exact world-horizontal surface. Cap mesh actually represents the water shape including tilt cut-off corners. |
| Cons | Pure render — must be paired with a sim approach. |
| Cost | ~10 µs/compartment for clip. 10 compartments = 100 µs. |
| Complexity | 2–3 days standalone, but really a sub-component of #1. |

## Approach 5 — Pressure-correction (Ruponen 2007 / Braidotti 2020)

Naval-engineering gold standard. Per tick: assemble linear system, unknowns = water level per compartment + flow velocity per opening. Enforces mass conservation + linearized Bernoulli at each opening. Solve iteratively or quasi-statically.

| Aspect | Detail |
|---|---|
| Pros | Validated against model-tank tests. Handles air compression, multi-level cross-flooding, partially submerged openings, attitude changes. |
| Cons | Solver overkill for 10 compartments. The level-vs-volume inner step is the **same volume integral as Approach 1** — Ruponen rigor on a 10-compartment graph reduces to what Approach 1 already solves implicitly. |
| Cost | ~50 µs/tick for sparse direct solve. |
| Complexity | 3–6 weeks. |
| Verdict | Defer until breach gameplay needs air compression / cross-flooding accuracy not provided by Approach 1. |

## Concrete delta from current code (Approach 1+4)

- `USubFloodComponent::WaterHeightCm` → `WaterVolumeM3` per compartment (replicated). Or keep scalar but reinterpret meaning.
- New baked artifact in `USubmarineDefinition`: per-compartment cumulative volume LUT or polynomial coefficients (computed from CV boxes).
- Helper `EvaluateWorldSurfaceZ(WaterVolume, SubTransform, BoxList) → float` — Newton/bisection over the cumulative cubic. ~50 lines.
- `AdvanceFlooding` rewrites `Δh` lines to call `EvaluateWorldSurfaceZ` for each side of each open edge.
- Cap mesh (`UFloodWaterPlaneComponent`) regenerates verts every Tick from box corners + `z_world`.

## Sources

- Ruponen P. (2007) *Progressive Flooding of a Damaged Passenger Ship*, PhD Helsinki UT, ISBN 978-951-22-9013-0.
- Braidotti L., Mauro F. (2020) *A Fast Algorithm for Onboard Progressive Flooding Simulation*, JMSE 8(5) 369. MDPI open access.
- Müller M. (2008) *Fast Water Simulation for Games Using Height Fields*, GDC 2008.
- Lee G. J. (2015) *Dynamic orifice flow model and compartment models for flooding simulation of a damaged ship*, Ocean Engineering.
- Bridson R., *Real Time Fluids in Games*.
- Houdini Shallow Water Solver — SideFX docs.
- Niagara Fluids — Unreal Engine 5.7 documentation.
- Barotrauma source: `Hull.cs`, `Gap.cs` (FakeFishGames/Barotrauma on GitHub).
