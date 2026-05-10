# Progressive Flooding Simulation — Sub3D Design Doc

**Status:** Working document, in progress. Write-as-we-think, iterate before code.
**Started:** 2026-05-10
**Owner:** Corentin (creative director + programmer) + Claude (implementer)
**Triggered by:** flood transmission needs to be physically coherent for FP gameplay; current model has a non-physical "100% overflow" shortcut and a binary door state. Going further requires alignment on what we're simulating before more C++ patches.

This doc is the **shared mental model**. Everything that ends up in code should have a justification rooted here. The references at the end are the source of truth for the underlying physics.

---

## 1. System framing

The submarine is an **I/O system** with three node types:

| Node type | Role | Examples |
|---|---|---|
| **Input** | Adds water mass to a compartment | Hull breach (exterior), interior leak from above-water pipe rupture |
| **Passage** | Couples two compartments (or compartment ↔ exterior) | Door, hatch, open vertical shaft, internal bulkhead breach |
| **Output** | Removes water mass from a compartment | Pump, drain, restored hull integrity (post-repair) |

The simulation tracks water mass per compartment over time. Each tick:
1. **Inputs** add water (breach inflow rate × dt).
2. **Passages** transfer water between compartments per their physics (gated by door open ratio).
3. **Outputs** remove water (pump rate × dt).
4. Per-compartment state (volume, height) is recomputed.
5. Mass conservation must hold within float precision.

This framing is the consensus in maritime engineering literature (Ruponen 2007 Ch. 2, Braidotti & Mauro 2020 §2). It maps cleanly to our existing `FFloodCompartmentState` / `FFloodEdgeState` / `FBreachInflow`.

## 2. Compartment topology

### 2.1 Geometric variants we ship

| Variant | Example in Craniata | Authoring |
|---|---|---|
| **Horizontal** (same Z floor) | C_Main_Bow ↔ C_Main_Hub ↔ C_Main_Aft | One CV per compartment, doors with bottom 10-20cm above floor |
| **Vertical stack** (different Z floor) | C_Main_Hub ↑ C_Upper_Hub, ↓ C_Lower_Hub | Hatch/staircase Z-aligned with floor of upper |
| **L-shape / multi-volume** | **C_Main_Aft** (currently 2 CV components sharing the CompartmentId) | Multiple CV components share a `CompartmentId`; bake integrates total capacity across all CVs |
| **Mixed adjacency** | C_Lower_Hub ↔ C_Lower_BallastFwd (same Z) ↔ C_Lower_Deep (below) | Combination of above |

### 2.2 What the sim needs from each compartment

- `CapacityLiters` — total floodable volume. **Bake-derived** (✓ implemented in current commit, voxel-integrated).
- `MaxWaterHeightCm` — Z extent above floor where water can rise. **Bake-derived**.
- `WalkableFloorZCm` — sub-local Z of compartment floor. **Bake-derived**. Critical for absolute-Z surface comparison.
- `CurrentWaterLiters` — sim state, integrated from inputs/passages/outputs each tick.
- `WaterHeightCm` — derived: `MaxWaterHeightCm * (CurrentWaterLiters / CapacityLiters)`. Assumes **uniform cross-section** (true for rectangular CV; approximate for L-shapes).
- **Surface absolute Z** = `WalkableFloorZCm + WaterHeightCm` (sub-local, world-Y once sub transform is applied).

### 2.3 What the sim does NOT need

- The 3D voxel grid from the bake. The sim runs on the integrated capacity + height column model. The voxel detail lives in `UCompartmentWaterBake` and is consumed only by the cap mesh / heightfield rendering.
- Per-CV resolution. L-shape with two CVs is one compartment with one capacity / one water level. The fact that the water surface visually crosses an L-bend is a rendering concern (cap mesh handles it via the bake's slice contours), not a sim concern.

## 3. Behavioral specification (from gameplay)

The user's expected behaviors, formalized:

### B1 — Horizontal adjacent (same floor Z)

> *"Le déversement entre deux compartiments adjacents horizontaux est simple : ça se déverse jusqu'à ce que la surface des deux compartiments soit à la même hauteur. Puis les deux se comportent 'comme un'. Le déversement doit être rapide, l'eau se déverse & se stabilise très rapidement."*

**Model:** communicating vessels. Water flows from higher surface to lower until equalization. Flow rate proportional to head difference and orifice area. Stabilization should happen within ~1-2 seconds at game scale.

**Formal:** `dV/dt = sign(Z_a - Z_b) · C_orifice · A_passage · √(2g · |Z_a - Z_b|)` (Bernoulli, see Lee 2015 eq. 1).

Sub3D simplification: replace √ with linear for stability (Barotrauma does this) → `dV/dt = K · A · (Z_a - Z_b) · open`. Tune K so stabilization is "fast but not infinite" (≤ 1s for typical compartments).

### B2 — Vertical drain (open hatch under)

> *"Le cas d'une trappe ouverte sous un compartiment, l'eau du compartiment supérieur est 'aspirée' rapidement vers le compartiment inférieur. S'il y a assez de capacité en dessous alors le compartiment supérieur est vidé."*

**Model:** gravity-driven drain. As long as upper has water AND lower has headroom, water transfers downward at a high rate. The "aspirée rapidement" suggests this rate dominates over horizontal flow.

**Formal:** classic orifice flow under gravity. `dV/dt = C_d · A · √(2g · h_upper)` where `h_upper` = water column above the hatch. With `C_d ≈ 0.6` (standard discharge coefficient) and `g = 981 cm/s²`, drain rate is high.

Sub3D shortcut: if exact Bernoulli is too unstable, use kinematic fixed-rate (Barotrauma): `dV/dt = K_vert · A · open`, source-clamped. Tune K_vert so a typical compartment drains in ~5-10s.

### B3 — Vertical fill (breach in upper compartment, water reaches lower passages)

> *"De même, pour un compartiment supérieur adjacent avec une brèche/ouverture, l'eau remplit le compartiment/les compartiments supérieurs par les brèches connectés."*

**Model:** if breach is in an upper compartment, water enters that compartment (B1 input flow). It then propagates DOWN via vertical passages (B2 drain). It does NOT propagate UP to a still-higher compartment unless source rises above the higher's floor (which requires source to be near full first).

**Formal:** B1 + B2 combined. The "100% overflow" shortcut becomes unnecessary when B2 is implemented correctly: water always drains down, so a breached upper compartment fills lower ones first, only retaining water once lower are full.

### B4 — Vertical fill from below (water rising up via stairwell)

This is **not** in the user's bullets but follows from the system framing: if a lower compartment is fully flooded and its surface exceeds the floor of an upper open-passage compartment, water should spill UP.

**Formal:** B1 horizontal model, but with the passage Z taken into account. Water flows up only when source surface > destination floor (= passage's bottom edge).

### B4.5 — Door spill threshold (oval doors)

Sub3D doors are oval with their bottom edge at **10-20 cm above the compartment floor**. Water below the door's bottom edge cannot flow horizontally. This means even with a fully open door, a low water level in compartment A simply doesn't cross until it rises above the door sill.

**Formal:** define `SpillZ_SubLocal` per edge = sub-local Z of the passage's bottom edge. Flow only happens when at least one side's surface exceeds `SpillZ`. See §6 weir-vs-orifice formula.

This is what gives the "puddle stays in the source room until it rises to door level" behavior expected by gameplay.

**Authoring convention:** `Connection.LocalTransform.Translation.Z` carries this value. Verified against current Craniata DA — connections are already authored at `floor_Z + 10` (e.g. `N_Main_Hub` at Z=-10 with Main floor at Z=-20). No DA change needed; sim just reads what's already there.

### B5 — Air pockets (deferred)

When a compartment is sealed (all doors closed) and water enters via breach, trapped air resists further flooding. Real progressive flooding models this via air pressure correction (Ruponen 2007 Ch. 4, Lee 2015 §3.2).

**Sub3D FP:** ignore. Compartments with sealed doors still flood to capacity. Post-FP refinement.

### B6 — Pump output

Pumps remove water from a specific compartment at a configurable rate. Flow direction always out (to exterior). Rate independent of water level (a real pump's curve depends on level + back-pressure, but FP simplifies to constant rate).

**Formal:** `dV/dt = -PumpRate · bActive`, clamped to `Min(CurrentWaterLiters / dt, PumpRate)`.

Already partially implemented (`bPumpActive`, `PumpRateLitersPerSec`), but no UI/gameplay hook yet.

### B7 — Sub orientation (roll/pitch)

When the sub rolls or pitches, the water surface follows world-up, not sub-local axes. A breach on the starboard side floods the starboard half of a compartment first.

**Sub3D FP:** ignore. Treat each compartment as a stable bucket aligned with sub-local Z. Post-FP requires per-compartment surface plane orientation tracking.

### B8 — Visual coupling: cap mesh + heightfield + FX

The sim drives three rendering systems. The contract:

1. **Cap mesh visibility** — at `WaterLevelNormalized ≥ 1.0`, the compartment is fully submerged. **Hide the cap mesh** (no surface to render) but **keep the post-process** so the player sees "underwater" while inside. Implementation: `UFloodWaterPlaneComponent::SetVisibility(false)` when level ≥ 0.99, plus the existing `bIsCrewSubmerged` post-process gate stays driven by water column height vs crew Z.

2. **Heightfield force injection** — when an edge has flow `Q > 0`, inject a localized perturbation on the destination compartment's heightfield at the edge's spill-side world position. Magnitude proportional to `Q · dt`. This makes the destination water visibly slosh on door open. The source side gets a smaller suction perturbation. Already partially implemented (`InjectAtWorldPoint`); needs to be auto-fired from `AdvanceFlooding` per-edge.

3. **Niagara FX intensity** — same per-edge `Q` drives the spawn rate / velocity / spray amount of the door-flow Niagara system. Existing `UDoorFloodVfxComponent` already exists (P3.7+) and reads `CurrentFlowRateLitersPerSec`. The sim must expose this on `FFloodEdgeState`, replicated.

**Required new field on `FFloodEdgeState`:** `float CurrentFlowRateLitersPerSec` (signed, sign indicates direction A→B / B→A). Replicated. Read by VFX component each tick.

### B9 — Door pressure dynamics (gameplay layer)

A door submerged on one side and dry on the other faces a hydrostatic force `F = ρ · g · h̄ · A` where `h̄` is the average head and `A` is door area. Realistically:

- **Trying to open** while pressure pushes the door open: easy (force assists).
- **Trying to open** while pressure pushes against your direction: hard or impossible.
- **Trying to close** while water flows against you: hard / impossible until the side drains.

**Sub3D model:**
```
hydrostatic_resistance = K_pressure · ρ · g · h̄ · A
crew_force = K_crew (constant, e.g. 500 N equivalent)
net_force = crew_intent_sign · (crew_force - hydrostatic_resistance)
OpenSpeed = net_force / Mass_door  (clamped, can be 0 if outmatched)
```

If `hydrostatic_resistance > K_crew · OpenIntent`, the door is **stuck**. UI feedback: "Cannot close — water pressure too high". Crew has to drain the source side first (close another door upstream, activate pump).

This is gameplay design layered on top of the sim. Implementation:
- `ASubDoorActor::Tick` reads source-side water column from `USubFloodComponent`
- Computes pressure resistance, applies to OpenAlpha tween speed
- Exposes `bIsStuck` to BP for UI feedback
- Strictly post-FP-flood-sim: implement after §6 model is in.

## 4. State model

### Compartment state (per compartment, per tick)

```cpp
struct FFloodCompartmentState
{
    FName CompartmentId;
    float CapacityLiters;           // bake-derived, immutable
    float MaxWaterHeightCm;         // bake-derived, immutable
    float WalkableFloorZCm;         // bake-derived, immutable
    float CurrentWaterLiters;       // integrated each tick
    float WaterHeightCm;            // derived = MaxH * (Cur / Cap)
    float WaterLevelNormalized;     // derived = Cur / Cap, clamped [0, 1.05] (5% overpressure)
    // ... existing pump / breach fields
};
```

The 1.05× soft cap on `WaterLevelNormalized` (Barotrauma trick) prevents flow stalls when two near-full compartments try to equalize.

### Edge state (per passage, per tick)

```cpp
struct FFloodEdgeState
{
    FName ClosureId;
    FName VolumeA, VolumeB;         // compartments. VolumeB == None means exterior.
    float PassageAreaCm2;
    float OpenRatio;                // ∈ [0, 1] — REPLACES bClosed bool. Door writes this each tick.
    float SpillZ_SubLocal;          // sub-local Z of passage's bottom edge (= where water starts to flow over)
    bool  bExteriorEdge;            // true if VolumeB == None (open hatch to ocean)
};
```

The `SpillZ` field is new. Populated from `Connection.LocalTransform.Translation.Z` at init. Required for B4 (surface must exceed passage Z to flow).

`OpenRatio` replacing `bClosed`: 0 = closed, 1 = fully open, intermediate = animated transition. `ASubDoorActor::OpenAlpha` writes this in `ApplyDoorState` (or per-tick if alpha is interpolating).

## 5. Algorithmic options

Five candidates, ordered by ambition:

### Option A — Barotrauma-grade explicit per-edge

Per tick, per edge: compute desired flow from current state, apply with mass conservation budget pass. No equilibrium solve. Stabilization happens over many ticks.

- ✓ Simple, fast, mostly what we have
- ✓ Easy to reason about
- ✗ Stabilization is slow (10-100 ticks for full equalization)
- ✗ Fixed flow constants are tuned, not derived

Currently committed: roughly Option A with absolute-Z + a hack for vertical (the "100% overflow rule").

### Option B — Bernoulli orifice (Lee 2015)

Same per-tick explicit, but rates derived from physical orifice formula:
`Q = C_d · A · √(2g · Δh)`

Tuning collapses to one knob (C_d ≈ 0.6 universal). Stabilization is correct but the explicit timestep needs care: high Δh + small dt → blow up. Stabilizer: clamp per-tick transfer to `min(SourceVolume, DestHeadroom, MaxRate · dt)`.

- ✓ Physical, no magic constants
- ✓ Vertical drain is "free" — same formula handles it
- ✗ √ is non-linear → stiff, may need sub-stepping
- ✗ Stability requires careful clamps

### Option C — Quasi-static pressure correction (Ruponen 2007 / Braidotti 2020)

Each tick, **solve** for the new equilibrium pressures via linearized Bernoulli + mass conservation. Adaptive timestep based on rate-of-change.

`Σ Q_in,i - Σ Q_out,i = dV_i / dt`
`Q_ij = C_d · A · sign(p_i - p_j) · √(2 |p_i - p_j| / ρ)`

Linearize Q in p via Newton iteration. Solve sparse linear system per tick.

- ✓ Industry standard for naval engineering
- ✓ Stable at large dt
- ✓ Handles air pockets (Ruponen Ch. 4)
- ✗ Requires linear solver
- ✗ Overkill for FP

### Option D — Energy minimization (Levi / Besançon)

Each tick, solve for water distribution that minimizes total potential energy subject to mass conservation. Equivalent to communicating vessels at equilibrium.

- ✓ Mathematically elegant
- ✓ Naturally handles N-way connections
- ✗ Not time-dependent — gives equilibrium directly, no transient flooding feel
- ✗ Wrong for gameplay (no dramatic "watching the water rise")

### Option E — Hybrid

Use Option B (Bernoulli explicit) for transient flow, snap to Option D equilibrium when |dV/dt| < threshold for N consecutive ticks.

- ✓ Best of both: dramatic transient + stable equilibrium
- ✗ Complexity

## 6. Recommendation for Sub3D FP

**Use Option B (Bernoulli orifice with weir / submerged-orifice split) with these constraints:**

### 6.1 Unified flow formula (weir vs submerged orifice)

For each open edge with `OpenRatio > 0`, compute heads above the spill threshold:

```
h_a = max(0, SurfaceA - SpillZ)   // A's head above the door sill
h_b = max(0, SurfaceB - SpillZ)   // B's head above the door sill
```

Then:

```
if h_a == 0 && h_b == 0:
    Q = 0                                                       // both below sill — no flow
else if h_a > 0 && h_b > 0:
    // SUBMERGED ORIFICE — door fully drowned both sides
    Δh   = SurfaceA - SurfaceB                                  // signed
    Q    = sign(Δh) · C_d · A_eff · sqrt(2g · |Δh|)             // standard orifice
else:
    // FREE WEIR / OUTFLOW — one side dry above sill, other side wet
    h_src = max(h_a, h_b)
    sign_q = (h_a > 0) ? +1 : -1                                // +: A→B, -: B→A
    Q     = sign_q · C_d · A_eff · sqrt(2g · h_src)             // free outflow
```

Where:
- `C_d = 0.6` (sharp-edged orifice default; per-Connection override possible later)
- `A_eff = PassageAreaCm2 · OpenRatio` (partial open = partial area)
- `g = 981 cm/s²`

This single formula handles **all** behaviors without special-casing:
- **Horizontal equalize (B1):** both sides above sill → submerged orifice → fast equalize, exact stop at Δh=0
- **Vertical drain (B2):** upper above hatch, lower below → free outflow, high rate (large `h_src`)
- **Breach in upper (B3):** B1 + B2 chained → propagation downward natural
- **Spill-up at high source (B4):** source surface eventually exceeds dst's spillZ → submerged orifice kicks in
- **Door sill 10-20cm above floor (B4.5):** built into the spillZ check; no flow until water rises above sill

### 6.2 Per-tick stability clamps

Apply, in order, to each computed `Q`:

```
Q = clamp(Q, -Q_max, +Q_max)                              // global rate cap (MaxInternalFlowLitersPerSec)
Q = clamp(Q, -SourceWater_AvailLps, +DestHeadroom_AvailLps) // mass conservation per edge
```

Where:
- `SourceWater_AvailLps = SourceVolume · CapacityLiters / dt`
- `DestHeadroom_AvailLps = (DestCapacity · 1.05 - DestCurrent) / dt`  (1.05× soft cap for overpressure)

Then the existing two-pass budget pass (already in `AdvanceFlooding`) scales all outflows from one source so they sum to ≤ source water. Keep it.

### 6.3 Auxiliary constraints

1. **OpenRatio float** replaces `bClosed`. `ASubDoorActor::ApplyDoorState` writes `OpenRatio = OpenAlpha` (continuous). Connection-type=Open hard-locks `OpenRatio=1.0`.
2. **Soft overpressure cap 1.05×** prevents stalls when two near-full compartments try to equalize.
3. **CurrentFlowRateLitersPerSec** stored on edge state, replicated, signed (+ = A→B). Drives VFX (B8) and audio.
4. **Drop the forced overflow shortcut**. The weir/orifice combo above replaces it cleanly.
5. **Sub-stepping**: not needed for FP. The 60Hz tick + clamps suffice. If validation surfaces stiffness, add internal N=4 sub-step locally (transparent to caller).

### 6.4 What stays / what changes vs current code

| Component | Current | After §6 |
|---|---|---|
| `FFloodEdgeState.bClosed` | bool | Replace with `float OpenRatio` |
| `FFloodEdgeState` | no spillZ, no flow rate | Add `SpillZ_SubLocal`, `CurrentFlowRateLitersPerSec` |
| `AdvanceFlooding` Δh formula | abs-Z surface compare + 100% shortcut | Weir/orifice formula above |
| `ApplyDoorState` flood call | `SetDoorState(bClosed)` | `SetDoorOpenRatio(alpha)` writing OpenRatio |
| Two-pass budget | ✓ keep | ✓ keep |
| Absolute-Z surface compare | ✓ keep | ✓ keep, used inside the weir formula |

This is roughly 100-150 lines of edits, no architectural change.

## 7. Decisions (resolved 2026-05-10)

| Question | Resolution |
| --- | --- |
| **Discharge coefficient C_d** | **0.6 default** (sharp-edged orifice). Per-Connection override later if some passages need different values. |
| **Tuning multiplier** | Single global `FloodFlowMultiplier` on `USub3DDebugSettings`, start 1.0, EditAnywhere for live PIE tuning. |
| **Sub-stepping** | **Skip for FP.** The 60Hz tick + per-edge `min(Q, SourceAvail, DestHeadroom)` clamp suffices. Revisit only if PIE shows oscillation. |
| **Pump UI** | **Defer.** Sim API stays (`SetPumpActive`). UI/actors come with the gameplay loop work, after pump assets exist. |
| **Breach repair** | **Out of scope here** — separate gameplay loop (post-helm roadmap). |
| **OpenRatio timing** | **Continuous per-tick.** `ASubDoorActor::Tick` writes `OpenAlpha → SetDoorOpenRatio(alpha)`. Partial-open = partial flow naturally. |
| **Air pockets (B5)** | **Defer post-FP.** Sealed-compartment behavior: floods to capacity. |
| **Sub roll/pitch (B7)** | **Defer post-FP.** Surface always sub-local Z. |
| **Acceptance** | **Both.** Unit tests T1-T7 in `Source/Sub3DTests/` for the math. PIE smoke for T3 + T8. |
| **L-shape compartments** | **Already supported** — C_Main_Aft uses 2 CVs sharing CompartmentId. Bake integrates total capacity correctly. No work needed. |
| **SpillZ source** | `Connection.LocalTransform.Translation.Z`. Already authored at `floor + 10cm` for current Craniata doors. **No DA edit needed.** |
| **Door pressure dynamics (B9)** | **Implement in same pass** — `ASubDoorActor::Tick` reads pressure from SubFlood, modulates OpenAlpha tween speed. ~30 lines. |
| **100% cap-mesh hide (B8.1)** | `UFloodWaterPlaneComponent` hides cap when `WaterLevelNormalized ≥ 0.99`. PP stays per existing crew submersion check. |
| **Heightfield force injection (B8.2)** | `AdvanceFlooding` calls `InjectAtWorldPoint` per active edge with magnitude proportional to `Q`. New code in the existing per-edge loop. |
| **CurrentFlowRateLitersPerSec replication** | Add to `FFloodEdgeState`, replicated. Already on the P3.7 todo list — fold into this pass. |

## 8. Test plan / acceptance criteria

Once code is written, validate with these scenarios:

| Test | Setup | Expected behavior |
|---|---|---|
| **T1 horizontal equalize** | Two adjacent compartments, same Z floor. Inject 50% in A. Open door. | Both equalize to 25% within 1-2 sim seconds. |
| **T2 vertical drain** | Upper full, Lower empty, hatch open. | Upper drains to 0%, Lower fills to (Upper.cap / Lower.cap). Time scale: ~5-10 sim seconds. |
| **T3 vertical fill from below (breach in lower)** | Lower has breach. Hatch above is open. | Lower fills first. Once Lower is full, water spills up through the open hatch into Upper. |
| **T4 mass conservation** | Closed scenario, no exterior flow. Total water across all compartments must be constant ± float epsilon over 1000 ticks. |
| **T5 multi-destination split** | One source, three open destinations of different size. | Flow splits proportional to destination headroom × open. Total outflow = sum of inflows. |
| **T6 partial open ratio** | Door at OpenRatio = 0.5. | Flow rate is half of full-open value. |
| **T7 closing a door mid-flood** | Door open, flood passing. Close door (OpenRatio → 0). | Flow stops within one tick. |
| **T8 breach repair** | Breach active, then `RemoveBreach` called. | Inflow stops within one tick. |

T1-T7 are unit tests (`Source/Sub3DTests/`). T8 requires PIE.

## 9. References

The papers below define the actual physics. We're not reinventing — we're picking the right level of fidelity for FP.

| Reference | Contribution |
|---|---|
| Ruponen, P. (2007). *Progressive Flooding of a Damaged Passenger Ship*. Helsinki UT. ISBN 978-951-22-9013-0. | Pressure-correction equation from mass conservation + linearized Bernoulli on staggered unstructured grid. Air pockets Ch. 4. |
| Braidotti, L. & Mauro, F. (2019). *A new calculation technique for onboard progressive flooding simulation*. | Setup for fast onboard variant. |
| Braidotti, L. & Mauro, F. (2020). *A Fast Algorithm for Onboard Progressive Flooding Simulation*. J. Mar. Sci. Eng. 8(5), 369. MDPI open access. | Free-surface plates, adaptive timestep, quasi-static. |
| Dankowski, H. & Krüger, S. (2012). *A Fast, Direct Approach for the Simulation of Damage Scenarios in the Time Domain*. | Direct time-domain solve, no iteration. |
| Lee, G. J. (2015). *Dynamic orifice flow model and compartment models for flooding simulation of a damaged ship*. Ocean Eng. | Bernoulli orifice equation + air pressure correction equation (the model we're proposing for Option B + B5). |
| Levi / Besançon. *Communicating vessels as energy minimization*. matbesancon.xyz/post/2020-05-09-volumes/. | Equilibrium-as-optimization framing. Useful intuition. |
| Barotrauma source. github.com/FakeFishGames/Barotrauma `Hull.cs` / `Gap.cs`. | What suffices for a shipped game. See [companion notes](../research/2026-05-10_barotrauma_flood_implementation.md). |

## 10. Next steps (sequenced)

§6 + §7 signed off 2026-05-10. Implementation order:

1. **Acceptance tests first** (T1-T7 unit, fail today). Lock the spec by writing red tests in `Source/Sub3DTests/Private/Automation/FloodSimWeirTests.cpp`.
2. **State changes**:
   - Add `float SpillZ_SubLocal` to `FFloodEdgeState`. Populate from `Connection.LocalTransform.Translation.Z` in `InitializeFromDefinition`.
   - Add `float CurrentFlowRateLitersPerSec` to `FFloodEdgeState`, replicated.
   - Replace `bool bClosed` with `float OpenRatio` (0..1). Update all readers/writers.
3. **Sim core** (`USubFloodComponent::AdvanceFlooding`):
   - Replace the current `HeightDelta + 100% shortcut` block with the §6.1 weir/orifice formula.
   - Drop `ForcedOverflowDeltaCm`, `VerticalThresholdCm`, `bAHigher / bBHigher` branches — all subsumed by the unified formula.
   - Apply §6.2 clamps; keep the existing two-pass budget.
   - Soft cap headroom at `1.05 × Capacity`.
   - Write `CurrentFlowRateLitersPerSec` per edge each tick.
4. **Door integration**:
   - `ASubDoorActor::ApplyDoorState` → `SetDoorOpenRatio(OpenAlpha)` instead of `SetDoorState(bClosed)`. Keep `SetDoorState` as backward-compat shim that calls the float version.
   - `ASubDoorActor::Tick` adds the §B9 pressure resistance computation, modulating tween speed. Skip first iteration if it makes scope creep — gate behind `bUseDoorPressureDynamics` debug toggle.
5. **Visual coupling**:
   - `UFloodWaterPlaneComponent` hides cap at `Level ≥ 0.99` (§B8.1).
   - `AdvanceFlooding` injects heightfield perturbation per active edge, magnitude `f(Q)` (§B8.2).
   - `UDoorFloodVfxComponent` reads `CurrentFlowRateLitersPerSec` from the matching edge (§B8.3).
6. **Settings**:
   - `USub3DDebugSettings` gains `FloodFlowMultiplier (default 1.0)`, `FloodOverpressureMaxNormalized (default 1.05)`, `FloodDischargeCoefficient (default 0.6)`.
7. **PIE validation**: scenarios T1-T8 from §8. Iterate K/multipliers until "feel" matches the expectations in §3.
8. **Commit + push** as `feat(water): progressive flooding sim — weir/orifice + OpenRatio + visual coupling`.

The whole block is roughly **300-400 lines of code** + tests, no architectural change. Estimate: one session.

Parallel tracks (§11) can run independently and merge later.

## 11. Parallel work tracks

These are independent of the sim implementation and can be worked in parallel by Corentin while sim refinement happens. Each tracks its own scope.

### 11.1 FloodView widget (terminal-style integrity overview)

**Goal:** a single widget showing the global flood / integrity state of the sub. Two display modes, same underlying widget:

- (a) **Console-mounted screen** in-world: rendered to a `UTextureRenderTarget2D` driven by a `USceneCaptureComponent2D` pointing at the widget. Crew walks up to a console, looks at the screen.
- (b) **Standalone HUD overlay** for debug / spectator: same widget on the player HUD.

**Content per compartment:**

- Compartment name + status (Dry / Flooding / Flooded / Sealed)
- Water level bar 0-100% (color: blue → orange → red as it climbs)
- Breach indicator (icon if `BreachInflowLitersPerSec > 0`)
- Active flow indicators on connections (small arrow + rate Lps if `|CurrentFlowRateLps| > threshold`)
- Door state (open ratio glyph: full / partial / closed / stuck)

**Schematic layout:** vertical-section profile of the sub. 3 deck rows (Upper / Main / Lower), each row split fwd-to-aft. Compartments boxed in with their state. Connections drawn as lines; thickness/color encodes flow.

**Data path:**

- Reads `USubFloodComponent::CompartmentStates` + `EdgeStates` (already replicated).
- No new server logic needed — pure presentation.

**Naming candidates:** `FloodView`, `IntegrityView`, `DamageControl`, `BulkheadStatus`. Decide once mockup feels right.

**Independence:** can ship before §6 sim refinement; will look better with §6 because OpenRatio gives partial-open arrows. Start with mockup HTML in `reports/ui-references/`, then build BP/UMG widget.

### 11.2 Pump assets (defer until sim ships)

API already exists (`SetPumpActive`, `PumpRateLitersPerSec`). Pending:

- Pump SK_Mesh asset
- BP_Pump_Actor with interaction
- Hookup to `USubFloodComponent::SetPumpActive(CompartmentId, true, RateLps)`

Trigger: after §6 sim is in PIE and pump becomes the natural counter to flooding.

### 11.3 Breach repair gameplay

Out of this doc's scope — see post-helm roadmap (memory `project_post_helm_roadmap_2026_04_28.md`). Mentioned here only because it closes the loop with §6 (sim) + §11.1 (FloodView surfaces "breach active" state).
