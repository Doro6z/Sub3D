# Proto04C Transition Note

Date: 2026-03-28

## Validated

- The runtime chain `damage -> breach cluster -> flooding -> movement response` is active in PIE.
- `CreateDebugBreachOnFirstExteriorSheet` is enough to test `Proto04C` without turret gameplay.
- `UBreachVfxManagerComponent` and `UFloodWaterVisualsComponent` are viable placeholder visuals for `C.3/C.4`.

## Explicit Placeholders

- Flood water is represented by one plane per compartment. It is a readability tool, not a fluid volume.
- Breach VFX are attached to breach cluster centers and still need placement polish against the inner hull.
- Niagara systems and camera shake assets are bootstrap assets, not final tuned content.

## System Debts Exposed By PIE

- Breach VFX can appear on or through the shell because the current placement uses sheet-space center without an inward offset.
- Flood planes intersect rounded shell geometry and bulkheads because they use compartment bounds rather than enclosed fluid volume.
- Flooding changes submarine motion in a way that is physically plausible in intent, but still too abrupt in presentation.
- The current hydrostatic model is intentionally simplified:
  - flooded liters feed dynamic mass,
  - buoyancy volume remains mostly fixed,
  - no coherent internal fluid redistribution exists yet.

## Architecture Notes

- The movement code does not currently apply a fake downward force; it recomputes total mass and derives vertical acceleration from buoyancy minus gravity.
- The issue is not "wrong layer", but an under-modeled physical chain:
  - flood mass,
  - buoyancy response,
  - pump authority,
  - client interpolation.
- A richer internal water simulation is compatible later, but should remain client-side visual only. Server gameplay truth should stay compartment-based.

## Short-Term Decisions

- Keep the current compartment flooding simulation as gameplay truth.
- Keep flood planes as the `Proto04C` placeholder visual.
- Do not open the big hydrostatic rewrite yet.
- Proceed with `C.5 Feedback Joueur` now, then revisit:
  - inward breach VFX offset,
  - flooded-mass smoothing for tests,
  - targeted movement/network observability.
- Feedback placement and anchor follow-up now live in:
  - `reports/plans/2026-03-28_sub3d_feedback_second_pass_notes.md`

## Deferred Work

- Multi-day tuning pass for:
  - ballast capacities,
  - flooded mass influence,
  - emergency pumping,
  - neutral buoyancy defaults,
  - damage-to-flood escalation.
- Possible later visual layer:
  - richer client-only water surface or local fluid fake,
  - without replacing server-side compartment flooding logic.
