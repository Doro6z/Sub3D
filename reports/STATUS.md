# Sub3D — First Playable Status

**Mis à jour :** 2026-04-25 | **Branche :** `scripting`

---

## DONE

| Système | Commit / Date |
|---|---|
| Local Grid Space Authority Phase 1 | 24b1cb7, a95ce17, 716d1a2 |
| Crew environment axis Phases 1+2+3 | 2026-04-22/23 |
| Game flow + multiplayer session architecture | 2026-04-23 |
| Physics revision (persistent cmds, ramp, Auto Depth, 60Hz) | 2026-04-17 |
| Helm cockpit redesign (UHelmCockpitWidget + 4 instruments) | 2026-04-18 |
| Character pipeline (SK_Crew_Basic, 26 bones, 5 mats) | 2026-04-14 |
| Flood visuals C++ — `UFloodWaterPlaneComponent::ApplyWaterState` | 2026-04-24 |
| Craniata BP rebuild (manual, collision script idempotent) | 2026-04-17 |

---

## IN PROGRESS / PENDING USER

| Item | Blocage |
|---|---|
| Water material authoring — 3 MFs + M_CompartmentWater refactor | User (authoring Unreal) — guide : `reports/guides/2026-04-24_flood_containment_option_c_v2.md` |
| Stairs → Ramps (SM_Stair_* → ramp meshes, simple collision) | User (asset swap dans BP_Submarine_Craniata) |

---

## PARKED (post-FP)

- Sub3DBake / Sub3DGenerator pipelines
- Fluidity roadmap (dual-buffer interp, camera late-update, VisualRoot)
- O2 simulation, audio/PP volume switching, Transitioning multi-tick
- M_CompartmentWater post-FP refinements (Fresnel, USub3DWaterSettings)

---

## RÉFÉRENCE AUTORITÉ

- Plan stratégique FP : `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
- Architecture dette technique : `reports/DEBT.md`
- Architecture locomotion : `reports/plans/2026-04-21_local_grid_space_authority_architecture.md`
- Architecture environment axis : `reports/plans/2026-04-22_crew_environment_axis.md`
- Architecture game flow : `reports/plans/2026-04-23_game_flow_and_multiplayer_session_architecture.md`
