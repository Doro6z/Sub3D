# Sub3D — Asset Library

Album navigable des assets 3D générés via Meshy pipeline. Pendant de [`reports/ui-references/`](../ui-references/) pour les UI.

## Sources canoniques de DA — autorité absolue

À consulter avant tout prompting :

- **Color sheet** : `C:/ACC/Projects/Sub3D/Image & Concept/ChatGPT Image 5 mai 2026, 10_40_11.png`
  Palette principale, éclairages émissifs, références matériaux, ambiance, applications par zone, slider dégradation.
- **Crew lineup** : `C:/ACC/Projects/Sub3D/Image & Concept/ChatGPT Image 5 mai 2026, 10_40_15.png`
  5 archétypes : pilot, engineer, medic, EVA diver, captain.
- **Hostile creatures** : `C:/ACC/Projects/Sub3D/Image & Concept/Mobs/`
  Stalker (annotated design sheet), Y Hunter, HostileTiers2Baser, hunter_shadow_threat.
- **Sub geometry brief** : [`../Sub3D_Submarine_Blockout_Brief.md`](../Sub3D_Submarine_Blockout_Brief.md)
  Surcouf 1929, 44m × 8m, 10 compartiments + SAS, 3 ponts, doors 90×185cm.
- **Plan complet pipeline** : [`../plans/2026-05-09_meshy_asset_pipeline_plan.md`](../plans/2026-05-09_meshy_asset_pipeline_plan.md)

## Visual target résumé

Hand-painted PBR low-poly stylized, game-ready UE5. Realistic proportions (NOT cartoon, NOT Marvel-hero, NOT anime). Slim shoulders. Body diversity réaliste autorisée. Faces évocatrices. Submarine inspiré Surcouf class 1929.

## Workflow

1. **Mood board** (priorité 1) : générer M01-M06 via [`moodboard_prompts.md`](moodboard_prompts.md). Sauvegarder dans `moodboard/M0X_*.png`.
2. **Item concept** : pour chaque item, générer image text-to-image en référençant la palette + style d'un mood M0X. Sauvegarder dans `concepts/{item_id}.png`.
3. **Item mesh** : passer l'image dans Meshy image-to-3D. Mesh livré → `Content/Sub3D/Assets/Meshy/{category}/{item_id}/`.
4. **Status update** : éditer `assets-data.js` pour passer `status` de `draft` → `concept_done` → `model_done` → `in_game`.
5. **UE integration** : ajouter row dans `Content/Sub3D/DataTables/DT_ItemUIRequests` (post-FP).

## Naming conventions

IDs lowercase snake_case, préfixés par catégorie :

| Préfixe | Catégorie |
| --- | --- |
| `tool_` | Held tool (interactif handheld) |
| `wear_` | Wearable (porté) |
| `dress_` | Set dressing (décoration non-interactive) |
| `fix_` | Sub fixture (fixe coque/intérieur) |
| `char_` | Character (body, head, morph) |
| `creature_` | Hostile / abyss exterior creature |

Exemples : `tool_pipe_wrench`, `wear_helmet_engineer`, `fix_valve_handwheel_M`, `char_head_pilot_male`, `creature_stalker_juvenile`.

## Visiter l'album

Ouvrir [`index.html`](index.html) dans un navigateur. React UMD + Babel-standalone + Tailwind CDN. No build, no install. Marche en `file://` direct.

## Statuts items

- **draft** — entrée créée, pas encore d'image concept
- **concept_done** — image concept générée et sauvée dans `concepts/`
- **model_done** — mesh Meshy validé et importable
- **in_game** — importé dans UE et utilisé en gameplay

## Pour ajouter un item

Éditer `assets-data.js` directement, ajouter une row au tableau `items[]`. Voir le schema dans [`../plans/2026-05-09_meshy_asset_pipeline_plan.md`](../plans/2026-05-09_meshy_asset_pipeline_plan.md) §3.
