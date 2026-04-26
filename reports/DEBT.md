# Sub3D — Technical Debt Tracker

Tracked items with explicit fix. Do not invent other explanations for their symptoms while these are unresolved.

---

## Code legacy — à supprimer

Ces chemins existent encore dans le code mais sont des no-ops ou du dead code depuis Local Grid Space Authority.

| Symbole | Fichier | Fix |
|---|---|---|
| `ApplyYawCompensation()` | SubCrewMovementComponent | Supprimer la méthode et tous ses call sites. Rebase gère la rotation. |
| `bEnableCrewTether` + tether logic | SubCrewMovementComponent | Supprimer le flag et le bloc world-snap. Rebase ne drift pas. |
| `UpdateBasedMovement` / `UpdateBasedRotation` overrides | SubCrewMovementComponent | Garder les overrides (no-op quand `IsGridAuthoritative()`) mais les commenter "suppression prévue post-FP". |
| `MovementBase` comme mécanisme de transport | SubCrewMovementComponent | Aucune action FP — MovementBase reste pour le floor-finding CMC. Supprimer post-FP si confirmé inutile. |

---

## Asset debt

| Symptôme | Asset | Fix |
|---|---|---|
| Micro-jitter sur transitions Deck ↔ Stair | `SM_Stair_*` meshes dans `BP_Submarine_Craniata` (ex. `SM_Stair_UpperToMain_UpperAccess`) — complex collision | Remplacer par des meshes ramp (pente plane, simple collision). Jusqu'à résolution : symptôme = asset, pas régression architecture. |

---

## Infrastructure non créée

| Item | Fix |
|---|---|
| `reports/ui-references/` + `reports/ui-pillars.md` | Créés. Voir `reports/ui-pillars.md`. |
| Water material authoring (3 MFs + M_CompartmentWater refactor) | Pending user — créer `MF_CompartmentWater_Containment`, `MF_CompartmentWater_EdgePolish`, `MF_CompartmentWater_Look`, refactor MI. Guide : `reports/guides/2026-04-24_flood_containment_option_c_v2.md`. |

---

## Post-FP — ne pas traiter avant milestone

- Fluidity roadmap : dual-buffer interp, camera late-update, VisualRoot mesh decouple, client prediction.
- O2 simulation réelle (actuellement `O2Level01 = 1.f`).
- LinkedAudioVolume / LinkedPostProcessVolume live switching.
- `Transitioning` state multi-tick blend (instant flip pour FP).
- Breach aspiration force (handoff event only pour FP).
- M_CompartmentWater post-FP : Fresnel horizontal mix, USub3DWaterSettings data asset.
- Sub3DBake / Sub3DGenerator pipelines.
