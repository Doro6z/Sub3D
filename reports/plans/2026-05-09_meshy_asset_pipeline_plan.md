# Sub3D — Meshy Asset Pipeline Plan

**Date** : 2026-05-09
**Branch** : water-proto
**Scope** : FP / démo. Production-ready si la qualité tient, sinon placeholders élégants.
**Status** : Proposition — en attente validation user.

---

## 1. Objectif

Pipeline parallèle de génération d'assets 3D via **Meshy** (text/image → 3D mesh) pour peupler Sub3D en cohérence visuelle sans dépendre d'un modeleur humain dédié.

Deux issues acceptables :

- **Qualité tient** → assets gardés en production FP.
- **Qualité ne tient pas** → assets servent de placeholders / vidéos démo / références jusqu'à un vrai modeleur.

Ce pipeline alimente directement le système **item-driven UI** ([feedback_item_driven_ui.md](../../C:/Users/coren/.claude/projects/c--Dev-Sub3D/memory/feedback_item_driven_ui.md), pillar P1) : chaque item généré → row dans `FItemUIRequest` data table.

---

## 2. Direction artistique — alignée sur sources canoniques existantes

**Setting locked** : 100% abysses, lumière artificielle, bioluminescence sur créatures hostiles ([project_abyssal_setting_2026_05_07.md](../../C:/Users/coren/.claude/projects/c--Dev-Sub3D/memory/project_abyssal_setting_2026_05_07.md)).

**Visual target** : low-poly stylisé hand-painted PBR, game-ready UE5. Realistic proportions (NOT cartoon, NOT Marvel-hero, NOT anime). Thin athletic builds avec body diversity réaliste autorisée. Faces évocatrices, proportions réelles, no exaggerated stylization. Slim shoulders, not oversized.

**Submarine inspiration** : **Surcouf class 1929 (French diesel-electric)** + Barotrauma layout (cf. `reports/Sub3D_Submarine_Blockout_Brief.md`). PAS Cold War Soviet, PAS photoreal modern.

### Sources canoniques (déjà en place — autorité absolue)

| Fichier | Contenu |
| --- | --- |
| `C:/ACC/Projects/Sub3D/Image & Concept/ChatGPT Image 5 mai 2026, 10_40_11.png` | **Référence couleurs Sub3D** — palette principale, éclairages émissifs, références matériaux, ambiance générale, utilisation par zone, slider dégradation/usure |
| `C:/ACC/Projects/Sub3D/Image & Concept/ChatGPT Image 5 mai 2026, 10_40_15.png` | **Crew lineup** — 5 archétypes : pilot, engineer, medic, EVA diver, captain |
| `C:/ACC/Projects/Sub3D/Image & Concept/Mobs/` | **Hostile creatures** — Stalker (annotated design sheet), Y Hunter (4 iter), HostileTiers2Baser, hunter_shadow_threat |
| `reports/Sub3D_Submarine_Blockout_Brief.md` | Sub geometry brief (Surcouf-inspired, 44m × 8m, 10 compartments + SAS, 3 decks, doors 90×185cm) |

Toutes les images générées via Meshy doivent référencer la palette du color sheet et le style/proportions du crew lineup.

### Palette canonique (extraite du color sheet)

**Palette principale :**

| Token | Hex (estimé du PNG) | Usage |
| --- | --- | --- |
| Acier brossé | `#4A4E52` | Coque intérieure, consoles, structures |
| Acier oxydé | tons rouille orangée | Patches de wear, valves anciennes |
| Vert hôpital | `#8FB80C` | **Zone médicale uniquement**, saturated NOT muted (différent du `--leaf` UI) |
| Jaune sécurité | `#C8AA45` | Marquages, casques engineer, signalisation |
| Rouge minium | `#80363F` | Peinture anti-rouille tuyauterie, accents danger |
| Caoutchouc | `#1A1A1A` | Joints, gaines, sols antidérapants |
| Cuivre / laiton | `#A57212` | Tuyauterie haute pression, valves, fittings |

**Éclairages émissifs :**

- Blanc néon — éclairage général
- Ambre chaud — tungstène 2700K, ambient cabine
- LED rouge — alarmes, indicators critiques
- CRT phosphore vert — instruments cockpit, monitors
- **Cyan bioluminescent** — créatures hostiles uniquement (PAS sur le sub habité)

### Wear-state cible

Slider du color sheet : default à **1/3 vers usé** pour le sub habité (entretenu mais utilisé). Patches de rouille OK, peinture craquelée OK, abandoned-grunge non. Pour les épaves extérieures abyssales : 3/3 (full corroded).

### Materials par zone

| Zone | Surfaces dominantes | Lighting target |
| --- | --- | --- |
| **Pont inférieur** | Acier brossé, plancher caoutchouc, rivets exposés | Tungstène 2700K |
| **Salle des machines** | Acier oxydé + cuivre/laiton patiné + valves rouge minium | Tungstène + alarmes LED rouge |
| **Cockpit / Bridge** | Console acier brossé + bakélite ambre + CRT phosphore vert | Mix tungstène + CRT |
| **Zone médicale** | Vert hôpital `#8FB80C` (murs) + acier brossé (fixtures) | Néon blanc froid |
| **Extérieur abyssal** | Coque acier oxydé patiné + projecteurs jaunes saturés | Projecteurs sub + bioluminescence cyan créatures |

### Character body template (male base)

Base body male : ~1.78m, **head:body ratio 1:6.5** (slightly stylized, PAS réaliste 1:7.5, PAS Marvel 1:8). Compact muscular **worker** body — broad shoulders, thick neck, solid chest. **NOT slim athletic, NOT gym-muscular**, NOT Marvel-hero. Default age **40-55 ans** weathered submarine worker, natural body fat distribution.

**Hands +10% oversized** pour FPS readability. Knuckle detail visible, calloused worker hands.

**Skin** — weathered dark, fatigue visible, pores at face/hands. Roughness 0.72-0.80, metallic 0.0.

**Persona** — *character over beauty*. NOT generic, NOT young, NOT idealized. Strong jaw, deep-set tired eyes, short grey stubble, weathered creases. Submarine workers marqués qui ont vu du service.

**Spec technique** (Meshy/UE5 input) :

- Stylized low-poly, sharp defined edge loops
- ~3500 tris max base body, ~2500 tris max head
- T-pose, UE5 Epic skeleton bone naming
- Socket points neck/wrists/ankles pour clothing attachment
- Single 2048×2048 atlas, PBR albedo+roughness+metallic+normal
- Minimal thermal undershirt visible only at neck/wrists/ankles (dark grey `#1A2028`)
- FBX export

**Prompt format adopté** : section-based (PROPORTIONS / SKIN / FACE / HANDS / TECHNICAL) au lieu de paragraphes flat — plus précis, plus reproducible. Source : user template 2026-05-09.

Female base body : à valider scope FP (le crew lineup canonique a 1 medic femme). Si oui, même template adapted (~1:6.5 femme = compact mature worker, 40-55 ans, NOT idealized).

5 archétypes locked au lineup canonique (scope FP) — tous attachent au char_body_male via sockets, têtes distinctes par archétype :

| # | Archétype | Look | Rôle gameplay |
| --- | --- | --- | --- |
| 1 | **Pilot / Helm** | Dark blue jumpsuit, headset, slim athletic | Pilotage, cockpit |
| 2 | **Engineer** | Orange jumpsuit weathered, hard hat jaune sécurité, heavyset OK | Réparation, salle machines |
| 3 | **Medic** | Cream jumpsuit + Red Cross arm patch, slim athletic | Soins, zone médicale |
| 4 | **EVA / Diver** | Dark wetsuit + yellow stripes, athletic | Sortie extérieure, breach repair |
| 5 | **Captain** | Olive coat, beard, pipe, calm authority | Command, navigation |

Têtes : 4-5 distinctes (ages, ethnicités, physionomies variées), morph targets per-head pour customisation (jaw_width, brow, cheekbones, weight, age) — Blender step post-Meshy.

### Hostile creature aesthetic

Référence canonique = `Mobs/Stalker (1).png` annotated design sheet + `Y HUNTER` series.

Style :

- Bio-mechanical fusion (chitin armor + organic flesh)
- **Cyan bioluminescent veins** running through body (signature visual hostile = cyan, opposé du sub habité ambre/jaune)
- Articulated scythe-limbs / segmented tails / dorsal armor plates
- Dark grey-black base + cyan accents
- Sleek-and-deadly silhouette, predatory

Le scope FP des créatures dans l'asset library = silhouettes + 1-2 créatures finalisées si Meshy convergent. La conception existe déjà — on prompte vers ces concepts existants en référence directe.

---

## 3. Album structure

### Stack technique

- **HTML + React UMD + Babel-standalone** (CDN, no build step, no node_modules — cohérent avec philosophie reports/).
- **Tailwind CDN** — même config que `reports/ui-references/index.html`.
- **Source unique** : `assets-data.js` qui définit `window.SUB3D_ASSETS`, chargé via `<script>` tag (marche en file://, contrairement à `fetch('assets.json')` qui est bloqué CORS). Format interne JSON-compatible, exportable vers data tables UE.

### Localisation

`reports/asset-library/` — sibling de `reports/ui-references/`.

```text
reports/asset-library/
├── index.html              # SPA React, point d'entrée
├── assets-data.js          # source de vérité items (window.SUB3D_ASSETS)
├── moodboard_prompts.md    # 6 prompts mood board M01-M06
├── moodboard/              # images concept générées (à créer par user)
│   ├── M01_compartment_corridor.png
│   ├── M02_helm_cockpit.png
│   └── ...
└── README.md               # rappel workflow + naming conventions
```

### Tabs

| # | Tab | Contenu |
| --- | --- | --- |
| 1 | **Overview** | DA synthesis, ancres, palette canonique, links vers ui-pillars, status global |
| 2 | **Scale & proportions** | Comparateur visuel : porte 1.85m, crew 1.78m, hand grip 8cm, valve 25cm, console 1.2m. Règles "fits through door", "two-handed vs one-handed" |
| 3 | **Characters** | Base body (1) + têtes (4-5) + morph targets + variations physionomie |
| 4 | **Equipment & wearables** | Items portés : helmet variants, jacket, boots, gloves, harness, mask, weight belt |
| 5 | **Held items / tools** | Interactifs handheld : wrench, lampe, extincteur, medkit, valve handle, repair gun, weapon, food, hydration |
| 6 | **Set dressing** | Non-interactifs : crates, books, photos, mugs, papers, cables, hooks, towels, signs |
| 7 | **Sub fixtures** | Built-in : console, valves, doors, lights, pipes, ladders, hatches, chairs, beds |
| 8 | **Abyss exterior** | Wreck pieces, rocks, créatures basiques, kelp, mineral nodes |

### Schéma d'un item card

```json
{
  "id": "tool_pipe_wrench",
  "category": "held_tool",
  "name": "Pipe Wrench",
  "name_fr": "Clé à pipe",
  "scale": {
    "length_cm": 35,
    "weight_g": 1200,
    "two_handed": false,
    "fits_through_door": true
  },
  "tags": ["repair", "metal", "rust_patina", "industrial"],
  "materials": ["forged_steel", "rubber_grip"],
  "concept_prompt": "Heavy industrial pipe wrench, forged steel head with patina rust, rubber-wrapped handle, top-down still life on dark workshop bench, single tungsten lamp lighting, 35cm length, photorealistic, 4k, neutral background",
  "meshy_3d_prompt": "Industrial pipe wrench with adjustable jaw, weathered steel patina, black rubber grip, mid-poly game-ready, single mesh",
  "ui_request": {
    "panel_id": "wrench_repair_overlay",
    "priority": 60,
    "fade_in_ms": 200,
    "fade_out_ms": 300
  },
  "status": "draft",
  "references": ["A2", "A5"],
  "notes": "Used for valve repair and pipe welding interactions"
}
```

Status workflow : `draft` → `concept_done` (image générée) → `model_done` (mesh Meshy validé) → `in_game` (importé UE).

### Card visual (par tab)

Layout repris de `reports/ui-references/index.html` :

- Card preview 16:9 (image concept ou SVG placeholder si `draft`)
- Header : ID mono + status tag
- Title + name_fr + scale chip ("35cm · 1.2kg · ☑ fits door")
- Tags pills
- Click → modal détail avec : full prompts, ui_request, references, notes, regen button

---

## 4. Inventaire — counts FP

| Catégorie | Cible FP | Détail |
| --- | --- | --- |
| Characters · base body | 1-2 | Male base 1:6.5 worker proportions ; female base à valider scope FP (1 medic femme dans lineup) |
| Characters · têtes | 4-5 | 40-55 weathered workers default, distinct meshes, character over beauty (NOT generic NOT young NOT idealized) |
| Characters · morph targets | 6-8 | Per-head sliders : jaw_width, brow, nose, cheekbones, weight, fatigue |
| Characters · wearables | 8-10 | Helmet (3 variants : naval, repair, EVA pressure), jacket (2), boots, gloves (2), harness, mask, weight belt |
| Held items / tools | 12-18 | Wrench, multitool, lampe torche, extincteur, medkit, valve handle, repair gun, food can, hydration flask, oxygen tank, flare, weapon (1-2), torpedo arm tool, clipboard, radio handset, camera, log book |
| Set dressing | 25-35 | Crates (3 sizes), books, photos, mugs, papers, cables (3), hooks, towels, ropes, signs, posters, food cans empty, bottles, ashtray, lamp portable, framed map, calendar, pin-up, plants, blanket, pillow |
| Sub fixtures | 8-12 | Console générique, valves (3 sizes), doors (2 variants : crew + airlock), ceiling lights, wall lights, pipes (4 variants), ladders, hatches, chairs (2), beds, locker |
| Abyss exterior | 8-12 | Wreck hull pieces, rocks (3 sizes), créatures basiques (silhouette, taille), kelp, mineral nodes, sediment cloud, anchor débris |
| **Total** | **~75-105** | Manageable en 8-12 sessions Meshy |

Le count est **plafond** — on rapatrie agressivement si Meshy converge mal sur certains items.

---

## 5. Workflow

### Étapes

1. **DA validation** (cette session) — user valide ancres + synthesis + scale rules. Plan committed.
2. **Album skeleton** — je build `reports/asset-library/index.html` avec tabs vides + 2-3 cards d'exemple + assets.json schema initialisé.
3. **Mood board generation** — 6 prompts mood key générés par Meshy en text-to-image :
   - M01 — Compartment ambiant (couloir crew, tungstène + crimson alarme)
   - M02 — Helm cockpit ambient (instruments allumés, hublot abyssal noir)
   - M03 — Crew quarters off-duty (couchettes, photo familles, lampe basse)
   - M04 — Breach moment (eau jaillissante, alarme rouge, vapeur)
   - M05 — Abyss exterior (sub vue extérieure, projecteurs traversant noir d'encre)
   - M06 — Item still life (établi avec outils, mood reference table)

   Ces 6 images deviennent les references "in the style of {M0X}" pour tous les prompts d'items ultérieurs.

4. **Scale rules locked** — comparateur dans tab 2 généré, image scale-card published.
5. **Inventaire item par item** — par batchs de 5-10 items, je prépare :
   - ID + nom + catégorie
   - Scale + tags + materials
   - Concept prompt (text-to-image, ref M0X)
   - Meshy 3D prompt (image-to-3D refinements)
   - UI request stub (panel_id + priority)
6. **Génération en lots** — user lance Meshy par batch, importe les images dans `moodboard/` et les meshes dans `Content/Sub3D/Assets/Meshy/{category}/`.
7. **Review + iterate** — par item : accepté / re-prompt / sortir du scope.
8. **Import UE** — quand un mesh est validé, import → assignation matériaux → row data table → in_game.

### Ordre proposé des batchs

1. **Tools** (3-5 items) — plus simple à valider la qualité Meshy + premier feedback prompting
2. **Set dressing** (5-8 items) — diversifie, teste les matériaux variés
3. **Characters** (head + 1 wearable) — le plus structurant, mais plus risqué — on attaque après validation tools
4. **Sub fixtures** (3-5 items) — réutilise les conventions characters/tools
5. **Abyss exterior** — dernier, scope optionnel selon time budget

---

## 6. Stubs / scope-out FP

- **Morph targets pipeline** : Meshy ne génère pas les morph targets — Blender step après import (shape keys puis FBX export). Listé en post-Meshy task per character head.
- **Animation rigs** : pas de génération automatique. Skeleton existant `SK_Crew_Basic` ([project_character_pipeline_2026_04_14.md](../../C:/Users/coren/.claude/projects/c--Dev-Sub3D/memory/project_character_pipeline_2026_04_14.md)) reste autorité.
- **LODs** : Meshy génère un seul LOD. Auto-gen UE5 LODs au moment de l'import.
- **Collision shapes** : custom simple primitives (UE auto-convex souvent suffit pour items). Listé per-item dans data row.
- **Materials post-Meshy** : Meshy livre PBR baked. Nous gardons la possibilité de swap pour matériaux Sub3D canoniques (M_Phase0_Test, futur M_CompartmentInterior) si nécessaire.

---

## 7. Décisions verrouillées (validation user 2026-05-09)

| # | Décision | Statut |
| --- | --- | --- |
| Q1 | DA aligned sur sources canoniques existantes (color sheet + crew lineup + creatures concepts + Surcouf brief). Style hand-painted PBR low-poly, realistic proportions. | Locked |
| Q2 | Scale : porte 1.85m / crew 1.78m / tout-passe-par-la-porte. Confirmé par le brief blockout (90×185cm). | Locked |
| Q3 | Ordre : **mood board first** (6 images key M01-M06), ensuite batch tools (5 items). Characters après validation Meshy quality. | Locked |
| Q4 | Mood board prompts détaillés écrits directement dans `reports/asset-library/moodboard_prompts.md`. | Locked |
| Q5 | React stack : React + ReactDOM UMD + Babel-standalone (CDN), Tailwind CDN, single-page, no build. | Locked |
| Q6 | Counts FP : ~75-105 items, réaliste pour FP/démo. | Locked |

---

## 8. Livrables — état

1. Plan committé (ce doc) — DA alignée sur sources canoniques.
2. `reports/asset-library/` scaffolded :
   - `index.html` — React SPA avec 8 tabs + cards items + modal détail
   - `assets-data.js` — 6 items d'exemple répartis sur les catégories clés (`window.SUB3D_ASSETS`)
   - `README.md` — sources canoniques + workflow + naming conventions
   - `moodboard_prompts.md` — 6 prompts mood board (M01-M06) prêts pour Meshy
3. Premier batch tools (~5 items détaillés) — à écrire après que mood board M01-M06 soit validé visuellement.

---

## 9. Décisions à logger en memory si validation

- `project_meshy_asset_pipeline_2026_05_09.md` — pipeline established, anchors locked, scope FP locked, stack React+esm.sh
- Update `feedback_collaboration_ui_method.md` — ajouter référence `reports/asset-library/` comme entry point pour assets 3D (en parallèle de `reports/ui-references/` pour UI)

---

*Plan en attente validation. Aucune ligne de code/asset générée à ce stade.*
