# Meshy Text-to-3D Prompts — Crew FP

4 prompts text-to-3d, ≤600 chars chacun, base nue (zero accessoires).

Format ancré sur le benchmark `captain_front` (style DRG × TF2, hand-painted PBR low-poly stylized, proportions 1:7 gamifiées).

---

## Crew (générique) — base réutilisable

```
male crew worker, 25yo, fit athletic build, fresh young face clean skin, short brown hair, clean shaven, alert focused eyes, charcoal #3A3D40 one-piece coverall, yellow #C9A646 collar piping, two symmetric empty chest pockets, olive belt empty, octagonal leather knee patches, black rubber mid-calf boots, bare hands, head:body 1:7 gamified, broad shoulders thick neck, hands oversized FPS, NO tools NO hat NO harness NO bag NO weapon NO pouches, hand-painted PBR low-poly stylized DRG meets TF2, NOT photoreal NOT cartoon, T-pose, 10000 tris, 2048 atlas, FBX UE5
```

## Captain

```
male submarine captain, 58yo, angular face, strong jaw, slicked back grey hair, clean shaven, deep-set authoritative eyes, weathered creases, navy #1A2238 officer coverall, raised collar piped gold #B8732C with small symmetric gold collar pips, two empty chest pockets, black leather belt brass buckle empty, octagonal leather knee patches, black leather mid-calf boots, bare hands, head:body 1:7 gamified, broad shoulders thick neck, hands oversized FPS, NO tools NO hat NO harness NO bag NO weapon, hand-painted PBR low-poly stylized DRG meets TF2, NOT photoreal, T-pose, 10000 tris, 2048 atlas, FBX UE5
```

## Engineer / Mécano

```
male submarine mechanic, 47yo, compact stocky muscular build, strong jaw, thick BLACK MUSTACHE handlebar, short black hair, weathered tanned face, dark circles, stern look, charcoal #3A3D40 coverall, yellow #C9A646 collar piping, two empty chest pockets, olive belt empty, octagonal leather knee patches, black rubber mid-calf boots, calloused bare hands, head:body 1:7 gamified, broad shoulders thick neck, hands oversized FPS, NO tools NO hat NO beanie NO harness NO bag NO weapon, hand-painted PBR low-poly stylized DRG meets TF2, NOT photoreal, T-pose, 10000 tris, 2048 atlas, FBX UE5
```

## Concierge / Artisan

```
male submarine handyman, 53yo, solid build, long face, hollowed cheeks, salt-pepper grey slicked back hair, clean shaven, tired alert eyes, weathered skin, charcoal #3A3D40 coverall, yellow #C9A646 collar piping, two empty chest pockets, olive belt empty, octagonal leather knee patches, black rubber mid-calf boots, bare hands, head:body 1:7 gamified, broad shoulders thick neck, hands oversized FPS, NO tools NO hat NO harness NO bag NO weapon NO pouches, hand-painted PBR low-poly stylized DRG meets TF2, NOT photoreal, T-pose, 10000 tris, 2048 atlas, FBX UE5
```

---

## Paramètres API communs

| Param | Valeur | Pourquoi |
|---|---|---|
| `mode` | `preview` puis `refine` | Workflow 2 étapes text-to-3d |
| `ai_model` | `meshy-6` | Stable, bon sur low-poly stylisé |
| `pose_mode` | `t-pose` | Critique pour rig downstream |
| `model_type` | `lowpoly` | Game-ready, pas hi-poly subdivision |
| `topology` | `quad` | Quads se skinnent propre |
| `target_polycount` | `10000` | Cible personnage (user constraint) |
| `symmetry_mode` | `on` | Mirror clean (les détails identitaires asymétriques iront en modulaire Unreal) |
| `enable_pbr` | `true` (refine) | Sortie metallic/roughness/normal |
| `remove_lighting` | `true` (refine) | On applique nos shaders UE5 |
| `target_formats` | `["fbx", "glb"]` | FBX pour rig+UE5, GLB pour preview |

## Crédits estimés

4 personnages × (preview 20 + refine 10) = **120 crédits** Meshy.

## Lancement

Voir `launch_meshy.py` à côté de ce fichier. Requiert `MESHY_API_KEY` en variable d'environnement.
