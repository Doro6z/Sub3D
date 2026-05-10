# Post-FP Debt Backlog

Date created: 2026-05-08

## Crew animation debug

- Remove the temporary legacy `UCrewAnimDebugWidget` tuner after `UCrewAnimDebugPanelWidget` ships with equivalent tuning controls. Until then, keep the legacy tuner isolated from the diagnostic panel and mark touched code with `// TODO: post-FP`.

## Flood — intra-compartment water redistribution under tilt

Added 2026-05-10. Sub3D's flood sim runs entirely in **sub-local Z** for both the simulation (`USubFloodComponent::AdvanceFlooding`) and the visual (`UFloodWaterPlaneComponent` cap mesh + heightfield). When the sub tilts, water should physically redistribute toward the low end of each compartment in **world-horizontal** space — neither the sim nor the visual does this today.

Symptoms at >10° tilt:

- Visual: cap mesh stays sub-local-flat, so water appears tilted with the sub instead of pooling at the low corner.
- Sim: surface delta computed sub-local, so water can transmit through doors that visually don't have water near them — and inversely, doors near actual water pools don't transmit because the sub-local average says the surface is far away.

The fix is a coupled sim + render rework. **Both must change in the same pass** to avoid the sim/visual divergence we briefly had on 2026-05-10 (world-Z sim + sub-local visual = worse than both-sub-local).

Plan:

1. **Sim** — surface elevation = world-Z. Per compartment per tick:
   - Get sub world transform.
   - Get compartment box (CV bounds) corners; transform to world; track min/max world-Z.
   - Surface_world_Z ≈ MinZ + Fraction · (MaxZ − MinZ) (linear approx; correct for fully-immersed range).
   - Per edge: Spill_world_Z = transform(Edge.LocalSpillPosition).Z.
   - Heads above sill in world-Z. Bernoulli unchanged.
2. **Visual cap mesh** — render the world-horizontal plane intersection with the compartment shape. In sub-local rendering frame, the cap is a tilted plane (perpendicular to world-up-in-sub-local) at the surface elevation, clipped by the compartment box. Probably means rebuilding the cap mesh procedurally each tick or supplying a tilt parameter to the existing builder.
3. **Heightfield** — same surface plane drives the height texture; perturbations stay on this plane.
4. **L-shape compartments** — bounding box approximation degrades at L-shapes (water "fills" outside the actual L volume). Acceptable if the bounding box area ≈ L footprint area. Otherwise, store per-compartment volume-vs-surface-Z function from the bake.

Cost: several days. Foundation work for high-tilt gameplay realism. Out of FP scope.

Until then, FP ships with sub-local sim + sub-local visual — both wrong above ~10° tilt but **coherent with each other**. The `LocalSpillPosition` (FVector) field already on `FFloodEdgeState` is in place for the future tilt work; only the Z component is read currently.

