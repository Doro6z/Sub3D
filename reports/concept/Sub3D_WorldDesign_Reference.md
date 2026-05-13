# Sub3D — World Design Reference

**Date** : 2026-05-13
**Statut** : Document de référence Étape 1 — produit AVANT toute manipulation UE5
**Phase** : pivot env abyssal Strate 1 (mémoire `project_env_proto_pivot_2026_05_13`)
**Auteur** : assistant technique, pour validation du créatif (DXR)
**Documents amont** :

- [2026-05-12_sub3d_concept_v2_final.md](2026-05-12_sub3d_concept_v2_final.md) — Concept V2 ratifié, Strate 1, ton Metro + Subnautica
- [2026-05-12_submarine_v5_palette_and_gates.md](2026-05-12_submarine_v5_palette_and_gates.md) — palette subs + V5a production FP
- Mémoires : `project_abyssal_setting_2026_05_07`, `project_meshy_asset_pipeline_2026_05_09`, `project_niagara_fluids_cascade_paused_2026_05_11`

**But du doc** : poser la stack technique, l'inventaire d'assets, la stratégie master-materials + FX shaders, et les options d'architecture pour le futur générateur de monde — **avant** que le créatif construise des scènes de référence. Ne fixe pas la DA (déjà cadrée Concept V2). Ne définit pas la composition des scènes (travail du créatif).

---

## Sommaire

- [0. Contexte et état existant Sub3D](#0-contexte-et-état-existant-sub3d)
- [1. État de l'art technique env abyssal UE 5.7](#1-état-de-lart-technique-env-abyssal-ue-57)
- [2. Inventaire stack assets — Megascans / Fab / alternatives](#2-inventaire-stack-assets--megascans--fab--alternatives)
- [3. Master materials + FX shaders — stratégie](#3-master-materials--fx-shaders--stratégie)
- [4. Architectures mi-procédurales — options non implémentées](#4-architectures-mi-procédurales--options-non-implémentées)
- [5. Questions ouvertes pour le créatif](#5-questions-ouvertes-pour-le-créatif)

---

## 0. Contexte et état existant Sub3D

### 0.1 Cible Strate 1 (rappel synthétique)

| Axe | Valeur |
|---|---|
| **Profondeur** | 100-500 m (coastal abyss) |
| **Échelle** | ~3×3 km zone ouverte + tunnels resserrés + cavités sporadiques (certaines géantes, certaines EVA-only) |
| **Densité humaine** | Avant-postes 2-4 km en ligne droite |
| **Lumière** | Pas de lumière solaire. Sources possibles : artificielles (phares sub, balises, néons stations, émissifs man-made), bioluminescence (faune, plankton, microbes), réactions chimiques (hydrothermal, oxydation, fluorescence minérale). Aucune lumière de ciel. |
| **Atmosphère** | Bleu profond → ténèbres ; pas de soleil, pas de ciel, pas de surface |
| **Tonalité** | Industriel sombre, "usé mais entretenu" — frontière industrielle analog-tech (**PAS** Cold War littéral, pas soviétique) |
| **Style** | Low-poly stylisé (silhouette) + matériaux PBR réalistes — philosophie matériau Deep Rock Galactic |

### 0.2 Cible hardware + budget

- UE 5.7, Win64
- Dev box i7-9700K + GTX 1660 Super (6 GB VRAM) — **pas de RT cores**
- `Engine.UseFixedFrameRate = true`, `FixedFrameRate = 60.0` (load-bearing pour anti-jitter — mémoire `project_motion_chain_jitter_root_cause_2026_04_27`)
- Coop 4 joueurs FP, host-listen architecture
- Budget frame cible : **16.6 ms** (60 Hz fixed) — toute techno qui consomme > 4 ms doit se justifier

### 0.3 État existant Sub3D pertinent

Audit du repo 2026-05-13 + correction créatif :

**Précision importante** — Sub3D est **entièrement immergé** (mémoire `abyssal_setting`). L'environnement n'a **pas de surface d'eau** (pas de flaque, pas de pool, pas de lac intérieur dans les cavités). Les cavités sont elles-mêmes sous l'eau ; le crew y nage en EVA. La seule eau "à surface" du jeu est le **flood interne au sub** (breach / compartment flooding).

**Non réutilisable pour l'env (clarifié 2026-05-13)** :

- `M_Craniata_Master` — essai abandonné côté créatif, ne pas en hériter
- `M_Decal_Sub_GrimeMaster` — usage strictement sub
- Light Functions `M_LF_Sub_*` — strictement intérieur sub
- Niagara `NS_LeakDrip`, `NS_DoorCascade`, `NS_Breach_WaterJet` — strictement flood interne sub
- `M_FloodWater_Base` / `M_CompartmentWater_v2` — **uniquement** pour le flood intérieur au sub. Pas d'analogue env (cf. précision ci-dessus). Aucun master `M_AbyssalWater_*` à concevoir.

**Pertinent pour la suite** :

- Biome data assets : `DA_BiomeField_Abyss`, `DA_BiomeField_RockyShelf`, `DA_BiomeField_Auto_DeepFault`, `DA_BiomeField_Auto_Labyrinth`, `DA_BiomeField_Auto_UpperShelf` — données proc-gen pré-existantes (lignée marching cubes). **L'algo et le format de ces données peuvent être réécrits intégralement** si la nouvelle architecture le justifie. Pas de contrainte de réutilisation, pas de contrainte de temps. Cf. §4.
- Concept assets : `C:/ACC/Projects/Sub3D/Image & Concept/` (concept art mobs)
- Pipeline Meshy ([[project_meshy_asset_pipeline_2026_05_09]]) — l'authoring path principal pour props bespoke

**Lacunes identifiées :**

1. Aucun master material rock/basalt/sédiment abyssal
2. Aucun master material émissif/bioluminescent
3. Aucun preset post-process abyssal dédié (PostProcessVolume "Strate1Abyss")
4. Aucun Niagara marine snow / sédiment en suspension
5. Aucune library mesh tunnels/cavités abyssales (les proto maps usent gen procédurale, pas d'assets statiques)
6. Aucun prefab modulaire avant-poste industriel
7. **Aucune map de référence `RefScene_Strate1_*`** dédiée

**À nettoyer / migrer :**

- `Source/Sub3D/Submarine/DepthPostProcessActor.cpp` — utilise une `ADirectionalLight` qui fade avec la profondeur. **Contradiction directe avec le doctrinal abyssal** (mémoire `abyssal_setting` : pas de DirLight, pas de soleil). À refactorer ou supprimer avant la phase scènes. Pattern correct : 2 `APostProcessVolume` (InsideHull + OpenWater) + volumetric fog + lumières placées.
- Mémoire `project_meshy_asset_pipeline_2026_05_09` : pipeline asset Meshy → PBR low-poly. **Aligné avec philosophie DRG** ; cette pipeline reste l'authoring path principal pour assets bespoke (avant-postes industriels, props man-made).

---

## 1. État de l'art technique env abyssal UE 5.7

Format : pour chaque techno — principe (1 ligne), pour/contre dans le contexte Sub3D, recommandation, citations.

### 1.1 Nanite vs LOD traditionnel

**Principe** — Nanite virtualise la géométrie en clusters streamés par taille écran. Remplace les LOD manuels.

**Pour Sub3D** :

- Hero assets rock/cliff/boulder = canonique Nanite (high tri, opaque, rigide)
- Supprime l'authoring LOD pour les sorties Meshy
- Instances massives (cailloux scatter) deviennent draw-call cheap

**Contre sur GTX 1660 Super** :

- Pas de RT cores → Nanite tourne en compute software-raster, avec ~2-5 ms d'overhead per-frame qui ne s'amortit pas en dessous d'un certain seuil de triangles (rapports forum sur GPU Turing pré-RTX). Le break-even est haut.
- **Translucent non supporté** — eau, verre, jellies, membranes biolum doivent rester non-Nanite (limite dure, pas perf cliff). Cf. [UE Nanite roadmap](https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1155-nanite).
- **Masked materials = quasi coût opaque** ([Nanite Foliage docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage)) — seaweed/coral à alpha mask paient plein tarif Nanite ; les décals contournent.
- Nanite displacement (5.4+) trop cher sur 1660 Super — désactiver.
- VRAM : streaming pool default 512 MB est trop pour 6 GB. Clamper `r.Nanite.Streaming.StreamingPoolSize=256` (cf. [streaming budgets](https://medium.com/@GroundZer0/nanite-streaming-and-memory-budgets-managing-geometry-at-scale-4c54bfa5d5b1)).

**Recommandation (confiance moyenne)** : Nanite **opt-in par catégorie d'asset** — hero rocks/cliffs/tunnels oui, petits props non. Tester sur 1660 Super avant de décider widely. Décals plutôt que foliage masked photogrammétrique pour coral/algae.

**Décision créatif 2026-05-13** : **skip Nanite** par défaut. Si le coût perf n'est pas un gain net sur 1660 Super, on n'investit pas. Le projet n'a probablement pas besoin de Nanite (style cible = stylé, pas photogrammétrie hi-poly). Re-tester uniquement si un asset spécifique le réclame.

**Sources** : [UE Nanite docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine), [Nanite + translucent forum](https://forums.unrealengine.com/t/will-nanite-ever-support-translucent-and-masked-materials/239729).

### 1.2 World Partition vs single persistent level

**Principe** — WP slice le monde en cells auto-streamés + HLOD pour les proxies distance. Single level charge tout.

**Pour Sub3D** :

- 3×3 km largement au-dessus du seuil où single level devient ingérable
- One File Per Actor (OFPA) supprime les conflits sur `.umap` — précieux en solo dev
- Data Layers mappent naturellement les tunnels/cavités (show/hide par mission ou par strate)

**Contre sur host-listen 4P** :

- **Listen server WP = point faible documenté** ([Epic KB — Listen Servers + WP](https://dev.epicgames.com/community/learning/knowledge-base/D7lL/unreal-engine-issues-with-using-listen-servers-with-world-composition-and-world-partition))
- Default : le serveur force-load **toutes** les cells. Mitigation : `wp.Runtime.EnableServerStreaming=1` (introduit en 5.1, **non-default**, easy to miss).
- Avec 4P + host, le host paye streaming **et** serving cost.
- HLOD landscape a connu plusieurs régressions 5.5→5.7 ; à monitorer.
- Hitches à la frontière des cells au cruise speed possible — loading range généreuse (~2-3× cell size).

**Recommandation (confiance haute)** : utiliser WP. Activer `wp.Runtime.EnableServerStreaming=1` **dès jour 1**. Cell size : commencer à 25600 cm (default), ne shrink que si hitches visibles. Data Layers pour tunnels et cavités EVA-only. HLOD uniquement sur la zone ouverte, pas sur les tunnels.

**Sources** : [WP Server Streaming KB](https://dev.epicgames.com/community/learning/knowledge-base/Xdj9/unreal-engine-world-partition-server-streaming), [Server Streaming roadmap](https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1235-server-streaming), [WP HLOD tips](https://dev.epicgames.com/community/learning/tutorials/z050/unreal-engine-5-world-partition-hlods-tips-tricks).

### 1.3 Lumen vs baked lighting

**Principe** — Lumen = GI + reflections temps réel (SDF + screen-space en software, RT cores en hardware). Baked = lightmass + lights stationary + reflection captures.

**Pour Sub3D** :

- Lumen marche en scène 100 % émissive (Matrix Awakens night mode existence proof)
- Dynamique par nature : breach, doors, headlights, sub bouge — pas de re-bake
- Look "moderne" out-of-the-box

**Contre sur GTX 1660 Super @ 60 Hz fixed** :

- **Pas de RT cores → Lumen software uniquement** (SDF + screen-space). Coût rapporté ~4-8 ms en 1080p sur Turing pré-RTX. C'est **la moitié du budget 16.6 ms.**
- Epic warn explicitement contre l'émissif comme source primaire ([Lumen Performance Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine)) : "results will be noisy, inconsistent, screen-space" — small bright sources (LED balise) sont le worst case.
- Mitigation Epic recommandée : émissifs larges + ternes + compléter avec point/spot lights réelles.

**Recommandation (confiance moyenne → hybride baked)** : **baked GI + Lumen Reflections seulement** (pattern DRG / Tekken 8) :

1. Baked GI + stationary lights pour les intérieurs statiques (couloirs sub, balises fixes, intérieurs stations). Reflection captures par compartment (déjà pratique Sub3D documentée dans `abyssal_setting`).
2. Lumen Reflections layered au-dessus pour wet hull / metal — fraction du coût full Lumen.
3. Dynamic lights (headlights, breach sparks) movable + cast dynamic shadows par-dessus le bake.

Pure Lumen-SW reste défendable si le créatif accepte un budget 8 ms de lighting et de la noise sur LED.

**Décision créatif 2026-05-13** : par défaut **baked GI + stationary lights**. **Cas spécial du sub** (grid local mobile) à traiter à part — un sub qui bouge avec son lighting baké pose un problème de cohérence lumineuse en mouvement. Options à considérer : (a) lighting baked relatif au sub (acceptable si le crew accepte des "ghost shadows" en virage rapide), (b) lighting fully dynamic dans le sub (lights movable + Lumen Reflections), (c) hybride — bake les surfaces internes pour AO, dynamic pour les lights. À tester en PIE.

**Sources** : [Lumen GI docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine), [Lumen Performance Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine), [To bake or not to bake 2024](https://lucaslabstudio.wordpress.com/2024/07/03/to-bake-or-not-to-bake-unreal-engine-5-lumen/), [Lumen optimization 60 FPS](https://www.strayspark.studio/blog/lumen-optimization-masterclass-60fps).

### 1.4 Volumetric Fog + Exponential Height Fog

**Principe** — EHF = fog analytique altitude-driven (quasi-gratuit). VF activé sur EHF = participating media per-froxel avec scattering per-light.

**Pour Sub3D** :

- VF gère le scattering depuis n'importe quelle light (point/spot/rect) — **c'est ce qui fait les cônes de phares sub dans la murk**.
- EHF seul ne fait pas de god rays — VF est obligatoire pour l'effet "lumière dans le brouillard".
- Local Fog Volumes (5.3+) permettent variation par compartment (intérieur sec ≠ flooded ≠ breach plume).

**Contre** :

- VF scale avec froxel grid (`r.VolumetricFog.GridPixelSize`, `r.VolumetricFog.GridSizeZ`). Default config ~2-4 ms sur 1660 Super.
- Chaque light avec scattering enabled = coût. Cap à 5-10 hero lights par view.

**Recommandation (confiance haute)** : **les deux**.

- EHF global pour le falloff bleu → noir (draw distance abyssal 30-80 m).
- VF activé, grid tunée (`GridPixelSize=16`, `GridSizeZ=64`), extinction modérée.
- `Volumetric Scattering Intensity` activé **uniquement sur lights hero** (phares sub, balises stations, intérieurs).
- Local Fog Volumes pour compartiments à densité différente.

**Sources** : [Volumetric Fog docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine), [Local Fog Volumes docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine).

### 1.5 Niagara recipes (sans FLIP)

**FLIP Niagara Fluids reste pausé** (mémoire `niagara_fluids_cascade_paused_2026_05_11`). Substitut adopté : foam sprites + heightfield. Pour env Strate 1, on a besoin de 4 effets :

| Effet | Type | Count | Notes |
|---|---|---|---|
| **Marine snow** | GPU sprite, additive soft | ~5k-20k | Vélocité descendante + jitter, density par compartment context. Camera-attached. |
| **Bioluminescence sparks** | **CPU** (besoin Light Renderer pour éclairer la scène) | < 200 | Trigger par proximity event creature BP |
| **Suspended sediment / murk** | **Pas Niagara — volumetric fog (§1.4)** | n/a | Volumétrique, pas particles |
| **Near-camera glitter** | GPU sprite | 500-2000 | Spawn box camera-locked, brightness modulée par light vector dans le material — c'est l'effet qui fait 80 % du "feel" sous-marin |

**Caveats** : 6 GB VRAM → cap projet ~50k particles GPU total. GPU emitters ne drive pas les `UPointLightComponent` (le bioluminescence qui éclaire **doit** rester CPU).

**Sources** : [Niagara optimization scalability](https://dev.epicgames.com/community/learning/tutorials/15PL/unreal-engine-optimizing-niagara-scalability-and-best-practices), [Niagara perf measuring](https://dev.epicgames.com/community/learning/tutorials/0qPO/unreal-engine-optimizing-niagara-measuring-performance).

### 1.6 Post-process abyssal

Stack recommandé pour 2 `APostProcessVolume` ("InsideHull" mild, "OpenWater" full) :

| # | Effet | Réglage | Note |
|---|---|---|---|
| 1 | Color grading | Shift bleu-cyan shadows, cooler whites, lifted blacks crushed légèrement, saturation -15 à -30 avec profondeur | LUT par strate (Strate 1 = moins agressif que Strate 2+) |
| 2 | Exponential Height Fog | Voir §1.4 | Densité dégressive avec profondeur (DepthPostProcessActor existant peut être recyclé, **mais virer la DirLight associée**) |
| 3 | Chromatic aberration | **Boostée** à 0.5-1.0 (forum UE consensus, pas la valeur "tasteful" 0.05) | Drive le feel "pression/refraction" |
| 4 | Vignette | Modérée 0.4-0.6, tintée bleu (pas noire) | Sells "claustrophobie pression" |
| 5 | Bloom | Restreint, threshold haut | Garde instruments lisibles dans la murk |
| 6 | Film grain | 0.1-0.2 subtil | "Low-light sensor camera" |
| 7 | DoF | **Off** | Cher + confusion spatiale (Subnautica évite) |
| 8 | Caustics | **Off** | Pas de surface → pas de caustics (free perf) |
| 9 | Lens distortion | **Off** | Cassé navigation (Subnautica 2 perf cost noté) |

**Budget** : ~1.5-2.5 ms PP stack complet sur 1660 Super, à profiler avec `ProfileGPU`.

**Toggle InsideHull/OpenWater** : déjà supporté par `crew environment axis` (mémoire CLAUDE.md "Crew environment axis") — chaque crew a un `CurrentCompartment` ; le switch PP volume peut piggy-back sur cet état.

**Sources** : [Subnautica rendering — Game Developer](https://www.gamedeveloper.com/design/how-i-subnautica-i-plunges-deeper-into-rendering-realistic-water), [Underwater lighting UE5 — 80.lv](https://80.lv/articles/here-is-how-to-set-up-underwater-lighting-in-unreal-engine-5), [UE5 PP chromatic blur](https://medium.com/@hussaindabhiya/postprocess-radial-chromatic-blur-ue-5-0-8e366ee82860).

### 1.7 PCG Framework (UE 5.2+) — note d'opportunité

Pas dans le brief original mais pertinent. PCG (Procedural Content Generation) UE-native permet de scatter rocks/sédiments/lighting probes par règles **sans** code custom. Mature en 5.5+. Utile pour §4 (architectures mi-procédurales) — leverager UE-native plutôt que construire un système from scratch.

**Sources** : [PCG Framework docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview).

---

## 2. Inventaire stack assets — Megascans / Fab / alternatives

### 2.1 Vérité honnête en préambule

Il n'existe **pas** de pack Megascans curé "abyssal" ou "deep-sea". Megascans est massivement terrestre. La stratégie réaliste pour Strate 1 :

1. **Repurposer du terrestre** (cliffs nordiques, lava rock, mud floors) sous un PP + fog volume unifié
2. **Packs Fab dédiés underwater** pour le bagage spécifique (corail, épaves, kit station)
3. **Meshy bespoke** pour la couche industrielle humaine (avant-postes, props man-made) — **c'est déjà la pipeline principale Sub3D**

Le "feel abyssal" sera porté **bien plus** par le lighting + water shader + post-process que par les assets sources.

### 2.2 Géologie — rocks, cliffs, boulders

Coastal-abyss = Nordic / dark wet stone, **pas** tropical ni désert.

| Pack | Type | Nanite | Pourquoi | URL |
|---|---|---|---|---|
| **Nordic Coastal Cliff** (Massive / Huge / individual) | Cliff faces | Oui | Dark wet stone, strata érodés — référence visuelle directe undersea cliff | [Quixel](https://quixel.com/megascans/home?assetId=ulmiccava) |
| **Quarry Slate Wall Cliff Rock** | Strata cliff | Oui | Stratification horizontale lit comme dépôt sédimentaire | [Quixel](https://quixel.com/megascans/home?assetId=xccibbi) |
| **Cave Stone Wall** | Cave interior | Oui | Tunnel + cavité dressing | [Quixel](https://quixel.com/megascans/home?assetId=tltncfpg) |
| **Sand Rock** (3D scans) | Boulder | Oui | Scatter seabed sand-embedded | [Quixel](https://quixel.com/megascans/home?category=3D+asset&search=rocks&assetId=vd5scco) |
| **Dark Ruins Sample Project** | Mixed cave+ruins kit | Oui | Reference lighting dark cave, mine pour assets individuels | [Fab](https://www.fab.com/listings/836ed2f8-e2d6-49be-98d3-59d104bd351e) |

**Flag créatif** : Nordic Coastal Cliff a des surfaces mousse/herbe top. Soit override material (strip moss channel + décal barnacle), soit cache sous fog dense.

### 2.3 Sédiment / sable / mud floors

| Pack | Type | Pourquoi | URL |
|---|---|---|---|
| **Mud Floor** | Wet mud surface | Seabed abyssal soft | [Quixel](https://quixel.com/megascans/home?search=muddy&assetId=tkjkfb0cw) |
| **Canyon Sandy Mud** | Sand/mud blend | "Plaine abyssale muddy" | [Quixel](https://quixel.com/megascans/home?category=surface&search=sand&assetId=wdvcade) |
| **Sand Debris** | Surface avec coquillages/pierres embedded | Lit "ocean floor deposition" pas "désert" | [Quixel](https://quixel.com/megascans/home?category=surface&category=sand&category=beach&assetId=phlvsop0) |
| **Coral Beach Sand** | Sable coarse + fragments coquilles | Meilleur tag "ocean" du catalogue | [Quixel](https://quixel.com/megascans/home?assetId=tgzleibcw) |

### 2.4 Underwater-specific kits (Fab — pas Megascans)

| Pack | Notes | URL |
|---|---|---|
| **Underwater World — Modular** | Kit modulaire abyssal scene-building | [Fab](https://www.fab.com/listings/552df954-0bf2-4c2d-bb7d-c624805262ba) |
| **Underwater Sunken Ship Environment** | Nanite+Lumen sample, épave + récif | [Fab](https://www.fab.com/listings/a8085275-4f89-43d5-a188-bf3b9366d488) |
| **Underwater Sea Station** | Intérieurs modulaires station — **utiliser comme comp reference**, pas direct import (Sub3D = Meshy auth) | [Fab](https://www.fab.com/listings/fa294f21-8259-4df9-9470-0eaaf6e5b69c) |
| **Deep Underwater** | Sub + épave WW2-style — industrial-frontier closer que tropical reef | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/deep-underwater) |

**Flag créatif** : tous ces packs lean stylized-tropical-saturated (corail bright). Pour Strate 1 ("industrial dark, used"), strip saturation dans le master instance + re-tinte muted blue-green-rust.

### 2.5 Hydrothermal / volcanique (Strate 2-3 prep, Strate 1 foreshadow)

| Pack | Notes | URL |
|---|---|---|
| **Megascans Lava Field** (curated) | Pack canonique Quixel volcanic — cooler les émissifs pour "vents dormants" Strate 1 | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/megascans-lava-field) |
| **Icelandic Lava Rock** (3D + surface) | Basalt poreux noir = substrate parfait chemosynthesis biome | [Quixel](https://quixel.com/megascans/home?assetId=tfyhfgzl) |
| **Rippled Lava Rock** | Pahoehoe surface | [Quixel](https://quixel.com/megascans/collections?assetId=vlzheg0n) |

### 2.6 Industrial debris / wrecks / structures

**Catégorie la plus dense Megascans** — easy win pour "présence humaine".

| Pack | Notes | URL |
|---|---|---|
| **Modular Metal Pipe Pack** | Plomberie avant-poste, scrap breach-zone | [Quixel](https://quixel.com/megascans/home?category=3D+asset&search=industrial&grid-type=trending&assetId=ubrqeehhx) |
| **Rusty Shipping Container** | Cargo coulé près outposts | [Quixel](https://quixel.com/megascans/home?category=3D+asset&category=industrial&search=rust&assetId=vl3iaiedy) |
| **Metal Containers Pack** | Bulk dressing | [Quixel](https://quixel.com/megascans/home?search=boxes&assetId=ubjhdfgva) |
| **Rust Debris** | Surface tile rust — applicable sur tout asset Meshy pour vieillir | [Quixel](https://quixel.com/megascans/home?assetId=ugxhbjph) |
| **Rubble Pack** | Concrete/rock chunks clutter | [Quixel](https://quixel.com/megascans/home?search=pieces&assetId=tj4mcccla) |
| **Burnt Debris Pack** | Scraps charrés — sites "récente breach" | [Quixel](https://quixel.com/megascans/home?category=3D+asset&search=rubble&assetId=ulrldcmiw) |
| **Abandoned Factory** (collection) | Whole-kit ref "used but maintained" | [Quixel collection](https://quixel.com/megascans/collections?category=environment&category=industrial&category=abandoned-factory) |

### 2.7 Marine growth / barnacle / decals

| Pack | Type | Notes | URL |
|---|---|---|---|
| **Barnacle Covered Rock** | Hero rock | Asset marine-growth rare, hero prop | [Quixel](https://quixel.com/megascans/home?assetId=uddkdbuya) |
| **Moss Patch** (decal) | Décal | Recolor → algue. **Décal contourne Nanite cost masked.** | [Quixel](https://quixel.com/megascans/home?category=decal&search=natural&assetId=tjxpg3h) |
| **Spanish Moss** (decal) | Décal | Réinterprété en drift-algae | [Quixel](https://quixel.com/megascans/home?category=decal&assetId=thpmevh) |
| **Ocean Seaweed** | Plante masked | **Cher Nanite** — préférer décals + Meshy low-poly opaque | [Quixel](https://quixel.com/megascans/home?assetId=sdDkn) |

### 2.8 Stylisé hand-painted PBR (direction DRG)

Pour la couche silhouette low-poly de Strate 1, voir §3.2.

| Pack | Notes | URL |
|---|---|---|
| **Low-Poly Mine and Caves — Modular Assets** | Le plus proche du vocabulaire DRG cave | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/low-poly-mine-and-cave-assets) |
| **45 Stylized Rock Pack** | PBR-stylized direct fit | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/45-stylized-rock-pack) |
| **Sculpted Rock Pack** (9 stones) | Hero rocks smaller-volume | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/sculpted-rock-pack) |
| **Stylized PBR Materials Pack** | Tile materials painterly-PBR | [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/stylized-pbr-textures-pack) |

### 2.9 Sources gratuites / CC0 (gap-fillers)

| Source | Force pour Sub3D | URL |
|---|---|---|
| **Poly Haven** | 8K CC0 PBR — `Coast Sand Rocks 02` seabed | [polyhaven.com](https://polyhaven.com/a/coast_sand_rocks_02) |
| **ambientCG** | 2000+ CC0 PBR, large rock/metal/rust | [ambientcg.com](https://ambientcg.com/) |
| **Sketchfab CC0** | Meshes spot CC0 (props marine, wreckage) | [sketchfab.com](https://sketchfab.com/) |
| **Quixel via Fab** | Tout Megascans free pour UE projects | [Quixel license](https://quixel.com/en-US/license) |

### 2.10 Philosophie matériau Deep Rock Galactic (synthèse)

DRG marche visuellement parce que (1) silhouette low-poly facet-shaded fait la lecture **identité** ; (2) PBR underneath donne biome-coded palettes serrés (Magma Core orange/red, Glacial cyan, Dense Biozone green) avec **specular contrast élevé** sur facets (wet/icy/mineralized highlights pop sur base dark) ; (3) **couleur = lever de readability dominant** — biomes reconnaissables silhouette-only par palette ; (4) painterly via **baked AO + edge highlights** aux facets, pas via noise procédural. Choix Ghost Ship pour **content velocity** (cheap to author, fast to ship more biomes) — contrainte que Sub3D partage en solo.

**Pas de GDC talk public sur le pipeline DRG** trouvé. Substitut viable : [Agents of Mayhem stylized PBR open world](https://gdcvault.com/play/1024690/-Agents-of-Mayhem-Physically) (GDC Vault).

**Sources** : [UE spotlight Ghost Ship](https://www.unrealengine.com/en-US/spotlights/how-ghost-ship-games-found-success-with-deep-rock-galactic), [DRG Wiki Biome Features](https://deeprockgalactic.wiki.gg/wiki/Biome_Features).

---

## 3. Master materials + FX shaders — stratégie

C'est la cible explicite du proto. La direction "low-poly stylisé + PBR réaliste" demande une architecture material claire : 3-4 masters paramétriques couvrent 90 % des surfaces ; chaque master expose les paramètres exposés (`Tint`, `RoughnessMult`, `WetnessMix`, `EdgeHighlight`, `EmissiveMask`...) pour instances rapides.

### 3.1 Inventaire des masters à produire

**Note** — Sub3D env entièrement immergé : pas de master `M_AbyssalWater_*` (pas de surface d'eau dans l'env). Le "feel eau" passe intégralement par post-process + volumetric fog + marine snow (§1.4-1.6).

| Master | Usage cible | Paramètres clés exposés | Priorité |
|---|---|---|---|
| **`M_AbyssalRock_Master`** | Toutes surfaces rocheuses (cliff, boulder, tunnel wall, seabed cliff) | `BaseColor`, `BaseColorVariation`, `Roughness`, `WetnessAmount`, `EdgeBrightness`, `BarnacleDecalMask`, `MossDecalMask`, `SedimentOverlay` | **Haute** |
| **`M_AbyssalSediment_Master`** | Sols sédiments (mud, sand, layered floor) | `BaseColor`, `Layered01Mix`, `Roughness`, `DebrisMaskOverlay`, `WetnessAmount` | **Haute** |
| **`M_AbyssalEmissive_Master`** | Bioluminescence (créatures, plankton, marker lights, fungal growth) | `EmissiveColor`, `EmissiveIntensity`, `PulseSpeed`, `PulsePhase`, `Opacity` | **Haute** |
| **`M_AbyssalIndustrial_Master`** | Outpost panels, pipes, scaffolds, structures man-made | `BaseColor`, `RustOverlay`, `WearMaskFromVertex`, `EmissiveMaskForLED`, `Roughness` | **Haute** |
| **`M_AbyssalDecal_Master`** | Décals shared (rust, barnacle, leak, grime, sediment splash, biolum patch) | `DecalAlbedo`, `DecalNormal`, `DecalRoughness`, `DecalOpacityMask`, `Tint`, `BlendMode` | **Moyenne** |
| **`MF_TriPlanar`** (fonction matériau) | Helper UV-free pour rocks/cliff arbitraires | Inputs (BaseColor, Normal, Roughness, ProjectionScale) | **Haute** (consommé par `M_AbyssalRock_Master`) |
| **`MF_WetnessLayer`** (fonction matériau) | Ajout uniforme wetness à n'importe quel master | Inputs (BaseColor, Roughness, WetnessAmount) | **Moyenne** |
| **`MF_EdgeHighlight`** (fonction matériau) | Approx painterly "edge highlight" facets via curvature ou per-vertex AO | Inputs (BaseColor, Normal, Strength, EdgeColor) | **Moyenne** (clé DRG-look) |

**Note d'architecture** : chaque master doit déclarer ses Material Quality Switches (Low / Medium / High / Epic) — pour scaler sur GTX 1660 Super, désactiver triplanar + edge highlight en Low ; conserver baseline PBR.

### 3.2 Stratégie shaders stylés (décision créatif 2026-05-13)

**Pas de PBR réaliste.** La DA cible est **stylée**, pas photogrammétrique. Le "feel" du jeu vient des lights + caves + mouvement, pas du détail de surface. **Un master unificateur** applique le même traitement à tous les assets, peu importe leur origine (Meshy bespoke ou Megascans cliff repurposé) — c'est ce qui consolide l'identité visuelle.

Conséquences :

1. **Source géométrie** = Meshy low-poly (props bespoke) + cliff Megascans pour hero rocks, **les deux passent par le même master unificateur stylé** (pas deux pipelines parallèles).
2. **Master unificateur** = `M_AbyssalRock_Master` + `M_AbyssalIndustrial_Master` portent la couche stylée commune : tinte saturée biome-coded (philosophie DRG), `MF_EdgeHighlight` pour le rendu painterly, roughness biasée (high contrast). Le PBR existe mais est stylisé, pas réaliste — pas d'albédo photogrammétrique tel quel ; chaque texture passe par une quantification de couleur ou un re-color ramp.
3. **Décal pass** = rust / barnacle / grime / biolum stickés par-dessus pour densifier sans alourdir mesh.
4. **Lighting hybride baked (§1.3)** — c'est ce qui finit de consolider l'identité visuelle.

Conséquence sur §1 et §2 : Nanite skip (§1.1) ; Megascans hero rocks **toujours utilisables** comme silhouette source mais leurs textures sont **retraitées via le master unificateur** (re-color ramp, edge highlight, etc.). Pas d'import "as is".

### 3.3 FX shaders à produire

| Shader / system | Usage | Implé | Priorité |
|---|---|---|---|
| **PostProcess Volume preset "PP_Strate1_OpenWater"** | Stack §1.6 complet | Material PostProcess + paramètres exposés en BP | **Haute** |
| **PostProcess Volume preset "PP_Strate1_InsideHull"** | Stack §1.6 mild | Idem | **Haute** |
| **Volumetric Fog driver BP** | Density / inscatter color par profondeur OU par compartment context | BP utilisant `UExponentialHeightFogComponent` + Local Fog Volumes | **Haute** |
| **`NS_MarineSnow_GPU`** | Marine snow GPU, camera-attached | Niagara CPU spawner + GPU simulation (§1.5) | **Haute** |
| **`NS_NearCameraGlitter_GPU`** | Glitter par-cone-phare, brightness via light vector | Niagara GPU + material light vector sampling | **Haute** |
| **`NS_BioluminescenceSpark_CPU`** | Sparks bioluminescence avec Light Renderer | Niagara CPU (besoin lights réelles) | **Moyenne** |
| **`NS_AbyssalSediment_GPU`** | Sédiments en suspension distance, density modulée | Niagara GPU | **Moyenne** |
| **`MF_HeadlightCone_FakeGodRay`** | Approche cheap god-ray sub-headlight (si VF coûte trop) | Material function billboard cone avec attenuation | **Basse** (fallback si VF over budget) |
| **`M_AbyssalCausticPanel`** | Fake caustic projection projeté depuis source artificielle (réinterprétation cohérente "pas de soleil") | Material translucent panel avec animated UV | **Basse** (cosmétique) |

### 3.4 Refactor / nettoyage prérequis

- **`DepthPostProcessActor.cpp`** : virer la `ADirectionalLight` qui fade. Remplacer par 2 `APostProcessVolume` (InsideHull + OpenWater) + Volumetric Fog driver. Mémoire `abyssal_setting` : **pas de DirLight**.
- **Examiner si `DA_BiomeField_*` data assets (§0.3) restent pertinents** ou si une refonte vers PCG-driven (§4) les obsolète. Probablement refonte — voir §4.6.

---

## 4. Architectures pour systèmes de caves — analyse approfondie

> **Aucune implé dans cette phase.** Le but est de poser **l'architecture qui servira de base**, pas de coder. Pas de contrainte de temps sur l'algo — on veut la meilleure structure, pas la moins chère.

### 4.0 Recadrage 2026-05-13

La première itération listait 5 options A-E avec une grille FP-friendly. Trois corrections :

1. **Pas de contrainte de temps pour l'algo** — la meilleure architecture, pas la moins chère.
2. **Option C (Tile + Wave Function Collapse) rejetée** par le créatif. Sortie de l'analyse.
3. **Lignée marching cubes existante peut être abandonnée intégralement** si une autre approche structure mieux le code. Pas d'attachement au code existant.
4. **Objectif explicite** : produire des **systèmes de caves intéressants**. Pas "qui marchent" ; pas "génériques" ; **intéressants** — mémorables, lisibles cartographiquement, géologiquement plausibles, navigation-rich.
5. **Correction après lecture des screenshots/proto** : le proto actuel a une valeur forte comme **référence de morphologie** (grosses masses organiques, rayons dominants ~3200 u, rétrécissements très fins). Il ne doit pas devenir seul la source de topologie. La bonne base doit conserver cette qualité de silhouette, mais déplacer l'intelligence réseau dans une couche graph/math séparée.

### 4.1 Critères de qualité d'un système de caves intéressant

Articulés explicitement pour cadrer l'évaluation :

| # | Critère | Pourquoi pour Sub3D |
|---|---|---|
| C1 | **Topologie lisible** — le joueur peut construire un mental map | Cartographie = pillar signature (Concept V2 §4.1-4.2) |
| C2 | **Plausibilité géologique** — formes lisent comme du vrai cave | Crédibilité narrative (Subnautica + Metro) |
| C3 | **Variété de profils** — chambres, squeeze tunnels, chimneys, galeries | Variation d'expérience EVA / sub |
| C4 | **Différentiation sub/EVA** — branches inaccessibles au sub (forcent EVA) | Mission 1 = balisage EVA tunnel annexe (Concept V2 §6.3) |
| C5 | **Surprise** — branchings cachés, breakthroughs inattendus | Récompense exploration |
| C6 | **Story embedded** — la géométrie suggère une histoire (ère d'érosion, fault tectonique, hydrothermal) | Cohérence ton Metro + Subnautica + future horror |
| C7 | **Hand-authored regions** — outposts et cavités hero authored exactement | Pas d'uncanny valley industrielle |
| C8 | **Déterministe par seed** — même seed = même monde | Persistance cartographie + multi (B.5 Concept V2) |
| C9 | **Streamable** — coexiste avec WP | Coop 4P host-listen sur 1660 Super |

Une approche pertinente adresse explicitement ces 9 critères. Une approche qui n'en couvre que 3-4 même bien échoue sur le reste.

### 4.2 Boîte à outils mathématique disponible

État de l'art des familles d'outils pour structures caverneuses 3D :

| Famille | Outils | Force | Faiblesse |
|---|---|---|---|
| **Bruit continu** | Perlin, Simplex, Worley, FBM | Smooth, multi-fréquence, gratuit | Pas de topologie, pas d'interest structurel |
| **Voronoi 3D** | Tessellation cellulaire, cells anisotropes, fault patterns | Boundaries lisent comme fractures, blocky-natural | Cells convexes par défaut |
| **Cellular Automata** | Conway-like rules, 3D CA, drunkard walk | Émergence organique, swiss-cheese natural | Pas de topologie globale, hard à contraindre |
| **Diffusion-Limited Aggregation (DLA)** | Witten-Sander 1981, particle walk + stick | Dendritic / karst natural | Heavy compute, hard à contrôler |
| **Reaction-Diffusion** | Turing patterns, Gray-Scott | Organic patterns scale-invariant | Heavy compute, mapping flou |
| **L-systems / grammaires** | Lindenmayer recursive rewriting | Branching hiérarchique naturel | 3D heavy, alignment hard |
| **Graph theory** | Nodes + edges, spatial embedding, MST, Voronoi-Delaunay | Topologie explicite, easy à authorer / contraindre | Geometry pass séparée |
| **Random walks** | Lévy flights, Brownian motion, biased walks | Meandering naturel | Pur random = boring |
| **SDF + CSG** | Signed distance fields, boolean ops, smooth-min | Smooth, mathematical, mixable | Need primitives |
| **Marching Cubes / Dual Contouring** | Lorensen-Cline 1987 / Ju 2002 / Schaefer 2007 | Voxel→mesh standard | MC flou ; DC sharper |
| **Erosion simulation** | Anisotropic erosion, water/sediment particles | Physically grounded, beaux résultats | Heavy compute |

Sources : [Inigo Quilez SDF articles](https://iquilezles.org/articles/distfunctions/), [Charles Boivin GDC procedural caves](https://www.gdcvault.com/play/1027382/), [Schaefer-Warren Manifold Dual Contouring 2007](https://www.cs.rice.edu/~jwarren/papers/dmc.pdf), [Spelunky cave algorithm postmortem](https://tinysubversions.com/spelunkyGen/), [Caves of Qud WFC notes](https://www.gridsagegames.com/blog/2014/06/procedural-map-generation/).

### 4.3 Pourquoi une approche single-pass échoue

Toute approche **single-pass** (un algorithme produit la géométrie finale en une étape) bute sur un trade-off structurel :

- Si l'algorithme est **topologique** (graph, agents, grammaire) → la géométrie est pauvre (cylindres aboutés, junctions cassées).
- Si l'algorithme est **géométrique** (CA, SDF, noise, erosion) → la topologie est émergente, donc impossible à contrôler explicitement.
- Si l'algorithme est **authored** → ne scale pas à 5+ strates.

Single-pass = OK pour caves génériques. Pour Sub3D où cartographie ET plausibilité géologique ET différentiation sub/EVA ET 5+ strates ET authoring override sont tous obligatoires, le single-pass laisse au moins une dimension orpheline. C'est ce qui rendait les options A-E originales toutes incomplètes (chacune sur un axe différent).

### 4.4 Architecture proposée — pipeline multi-étapes découplées

Conséquence directe du §4.3 : **plusieurs étapes orthogonales**, chacune avec son algorithme propre, composant un output unifié. La séparation permet d'itérer une étape sans toucher les autres, et de substituer l'algorithme d'une étape sans casser les autres.

```text
┌─────────────────────────────────────────────────────────────────────┐
│  Stage 1 — TOPOLOGY GRAPH                                            │
│  Output : G = (V, E) où V = chambers semantically typed,             │
│  E = passages with profile + diameter + sub/EVA accessibility tag.   │
│  Math : graph theory + spatial embedding                             │
│  → Adresse C1 (cartographie), C3 (variété), C4 (sub/EVA), C7 (auth)  │
└──────────────────────────────┬──────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│  Stage 2 — GEOLOGICAL SUBSTRATE                                      │
│  Output : champ scalaire 3D S(x,y,z) ∈ [0,1] = hardness rock         │
│  + weakness planes (faults, joints, stratification).                 │
│  Math : superposition Voronoi + stratification + faults + noise      │
│  → Adresse C2 (plausibilité), C6 (story embedded)                    │
└──────────────────────────────┬──────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│  Stage 3 — GEOMETRIC REALIZATION                                     │
│  Input : G + S.  Output : mesh surface extraite.                     │
│  Math : SDF (topologie via skeleton) modulé par S (substrate),       │
│  extraction via Manifold Dual Contouring.                            │
│  → Adresse C2 (forme realistic), C5 (surprises micro), C9 (stream)   │
└──────────────────────────────┬──────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│  Stage 4 — DETAIL DECORATOR PASS                                     │
│  Input : surface + curvature + wetness/heat fields.                  │
│  Output : scatter stalactites/stalagmites/sediment/biolum/algae.     │
│  Math : Poisson disk + curvature-driven + field-driven rules         │
│  → Adresse C2 (richesse), C3 (texture variation), C5 (rewards)       │
└──────────────────────────────┬──────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│  Stage 5 — AUTHORED OVERRIDE                                         │
│  Sublevels hand-authored override Stages 1-4 dans certaines régions  │
│  (outposts, cavité Mission 1, signature landmarks).                  │
│  → Adresse C7 (control absolu) et C9 (WP streaming)                  │
└─────────────────────────────────────────────────────────────────────┘
```

**Propriétés du pipeline** :

- **Modules indépendants côté code** : 1 module C++ Sub3DRuntime par stage. Inputs/outputs sérialisables (debugging trivial). Substitution algorithmique d'un stage = no impact sur les autres.
- **Bake offline** : pipeline tournée 1 fois par strate × seed, output cached. WP cells consomment du pre-baked. Aucun coût runtime.
- **Composable** : Stage 5 override = lookup spatial ; les régions non-overridées passent par Stages 1-4 normalement.

### 4.5 Algorithmes candidats par étape

#### 4.5.1 Stage 1 — Topology graph (deux couches superposées)

**Décision architecturale 2026-05-13** — le graph topologique Sub3D est **deux couches** : un trunk clairsemé sub-navigable + un réseau dense de passages cavités étroites non-navigables. Pas une couche unique avec tags d'accessibility et dead-ends. Le PCE network a sa propre topologie interconnectée — c'est lui qui produit l'intrication réseau que les caves intéressantes demandent.

**Seconde analyse 2026-05-13** — la première démo HTML montre bien l'idée, mais son `G_PCE` Poisson + k-nearest uniforme est seulement une visualisation pédagogique. Comme architecture finale, il est trop "graphe de routes" et pas assez "cave". Les passages cavités étroites doivent être générés comme un **réseau contraint par la géologie**, puis réalisés par SDF/smooth-min pour retrouver les formes organiques du proto.

##### Vocabulaire

| Terme | Définition | Remplace |
|---|---|---|
| **PCE** | Passages Cavités Étroites non-navigables (par le sub). Tunnels traversables EVA. **Forment un réseau dense, pas des dead-ends.** | "EVA-only" (terme deprecated) |
| **G_sub** | Trunk graph clairsemé. Outposts + chambers + sub-tunnels. C'est la route principale du sub. | — |
| **G_PCE** | Réseau dense des passages étroits. Sa propre topologie, ses propres loops, joinings, sa propre lecture cartographique. | — |
| **Bridge** | Edge ponctuel reliant un node PCE à un chamber G_sub. Point d'accès depuis la route principale vers le réseau étroit. | — |
| **Squeeze** | Sous-set des edges PCE — passages très resserrés où le crew doit lâcher de l'équipement. | (inchangé) |

##### Structure mathématique

```text
G = G_sub ∪ G_PCE ∪ E_bridge

G_sub = (V_sub, E_sub)
    V_sub = { outposts (authored fixés) ∪ chambers (Poisson disk Δ≈100 m) }
    E_sub = k-nearest k=3, distance max 220 m, + MST + k_loop extra edges
    Edge.tag = 'sub' uniformément
    Edge.radius : grand (200-800 u équivalent)

G_PCE = (V_pce, E_pce)
    V_pce = { Poisson disk biaisé par C(x,y,z), exclure coeur des chambers sauf entrées }
    E_candidates = Delaunay(V_pce) filtré par longueur, pente, coût géologique
    E_pce = MST(E_candidates, coût) + cycles contrôlés + reconnects locaux
    Edge.tag = 'pce' (majority) | 'squeeze' (18% stochastique)
    Edge.radius : petit (50-200 u) | très petit pour squeeze (10-50 u)

E_bridge ⊂ V_pce × V_sub
    Pour chaque v_pce, candidat = chamber le plus proche
    Garde si distance < 85 m ET tirage rng < 0.55
    Edge.tag = 'bridge'

C(x,y,z) = champ de conductance de cave
         = a1 · (1 - S_hardness)
         + a2 · faultWeakness
         + a3 · stratLayerBand
         + a4 · distanceToChamberShell
         - a5 · protectedAuthoringMask

cost(edge path γ) = ∫γ [ baseCost / max(C(p), ε)
                      + bendPenalty · κ(p)^2
                      + pitchPenalty · max(0, |slope(p)| - slopeLimit)^2 ] dp
```

##### Propriétés émergentes

- **Densité** : G_PCE a ~4× à 8× plus de nodes que G_sub, selon le biome. Ce ratio est un paramètre de lisibilité, pas une constante absolue.
- **Loops** : les cycles ne viennent pas d'un simple `k=4`; ils sont ajoutés par cycle budget. On garde les cycles qui améliorent la navigation (`shortestPathGain`) ou la surprise (`hiddenBySubstrate`) et on retire les loops inutiles.
- **Interconnexions** : un PCE node moyen vise degré 2.4-3.6. Moins = couloir avec dead-ends ; plus = spaghetti illisible. Les bridges vers chambers sont rares et nommables.
- **Lecture cartographique** : G_sub est la "table des matières" du monde — les outposts et chambers principales. G_PCE est le "texte" — l'exploration intriquée que le joueur cartographie à l'EVA. **Les deux apparaissent sur la carte** mais avec des niveaux d'opacité / agrandissement différents.

##### Algorithmes candidats (ré-évaluation)

| Algo | Principe | Couche | Adopté ? |
|---|---|---|---|
| **1.1** Authored + perturbation | Graphe hand-designed perturbé | G_sub primarily | Oui pour outposts (nodes fixes) |
| **1.2** Poisson disk + k-nearest | Nodes scatter, edges k-nearest | Debug / démo | **Non final seul** — trop régulier, pas assez géologique |
| **1.3** L-system | Branchements grammaire récursive | PCE local | Non comme source principale ; utile pour motifs locaux récurrents |
| **1.4** Stochastic agent walks | Agents wander random walks dans `C(x)` | Routage PCE | Oui comme outil local si les chemins A* sont trop droits |
| **1.5** Hybrid 2-layer — outposts authored + G_sub + G_PCE + bridges | Architecture canonique | Les deux | **Oui** |
| **1.6** **Substrate-biased PCE graph** — Poisson pondéré + Delaunay candidates + MST + cycles + routage anisotrope | Réseau PCE final | G_PCE | **Oui — remplace le kNN uniforme comme cible finale** |

**Recommandation Stage 1 = 1.5 hybrid 2-layer + 1.6 substrate-biased PCE graph**.

##### Mapping radius vers les meshes existants (Sub3D mesh aesthetic)

Les meshes procéduraux que tu génères actuellement (grosses cavités organiques ~3200 u rayon, rétrécissements visibles jusqu'à ~10 u) correspondent à une **morphologie implicite** : capsules/sphères, rayons variables, smooth-min, bruit organique, puis extraction de surface. Cette logique est saine pour la forme.

Lecture technique du proto actuel vérifiée dans `Source/Sub3D/WorldGen/` :

- `UNavigableVolumeGenerator` construit un champ SDF à partir de capsules/sphères et `SmoothMin`.
- `UOrganicDeformationGenerator` ouvre des formes organiques via `Threshold - (N1² + N2²)`, restreint aux chunks proches du chemin garanti.
- `URouteMeshBuilder` extrait actuellement par Marching Cubes sur un champ 200 cm.
- Les `BranchProfileDataAsset` actuels imposent `TargetRadiusCm >= 1000`, donc ils ne peuvent pas représenter de vrais PCE fins sans nouvelle famille de profils ou nouveau type de couche.

Conclusion : **ne pas jeter l'intuition visuelle du proto**. Il faut la déplacer en Stage 3 comme style de réalisation : radius variable, smooth-min, bruit organique, déformation anisotrope. En revanche, la topologie PCE ne doit pas dépendre du bruit `N1²+N2²` seul. Le bruit est excellent pour la peau et les renflements ; il est insuffisant pour produire un réseau EVA intriqué, connecté, lisible et testable.

Important sur les unités : si `10 u` signifie 10 cm UE, ce n'est pas traversable par un personnage EVA. Il faut distinguer :

| Famille | Rayon indicatif | Usage |
|---|---:|---|
| Sub trunk / grande galerie | 2400-5500 cm | Sub Craniata proto |
| Chamber / bulbe organique | 3200-8000+ cm | Repère visuel, composition |
| PCE EVA | 120-300 cm | Traversable par crew en EVA |
| Squeeze EVA | 60-120 cm | Tension, équipement contraint |
| Fissure décorative | 10-60 cm | Visuel, biolum, écoulement, non traversable |

Le générateur doit pouvoir créer toutes ces familles, mais elles ne doivent pas toutes être dans la même couche de navigation.

##### Architecture PCE recommandée en détail

1. **Générer un champ de conductance `C(x,y,z)`** depuis le substrate Stage 2 : soft rock, fault planes, interfaces de strates, distance au shell des chambers, masques authored.
2. **Sampler les nodes PCE** par Poisson disk pondéré : plus dense dans les bands tendres/faults, moins dense dans hard rock. Le sampling reste seed-locked.
3. **Construire des edges candidats** par Delaunay 3D ou k-nearest borné, mais les scorer par coût géologique, pente, courbure attendue et distance aux chambers.
4. **Forcer la connectivité** par MST pondéré sur les composants proches. Le MST donne le minimum lisible, pas le résultat final.
5. **Ajouter des cycles contrôlés** : choisir des edges non-MST qui réduisent la distance de parcours, reconnectent deux branches proches, ou créent un raccourci entre deux chambers. Budget de cycles par cell.
6. **Router chaque edge dans le champ** avec A* 3D / Fast Marching / agent walk biaisé par `C(x)`. L'edge final devient une polyligne ou spline, pas un segment droit.
7. **Attribuer les rayons** par fonction `r(t) = r_base · m_substrate(t) · m_squeeze(t) · m_junction(t)`. Les PCE peuvent enfler près des junctions et se pincer entre deux zones dures.
8. **Réaliser la géométrie** via SDF/smooth-min, avec un bruit organique proche du proto actuel.

Cette architecture donne des passages longs quand le coût géologique les y pousse, mais aussi des rejoins, mailles, raccourcis et zones intriquées. C'est le point qui manque à un système "branche EVA longue" simple.

**Adresse explicitement C3 (variété) + C4 (sub vs PCE) + C5 (surprise via loops PCE)** en encodant la richesse réseau dans deux graphs distincts plutôt qu'un seul tagué.

#### 4.5.2 Stage 2 — Geological substrate

| Algo | Principe |
|---|---|
| **2.1** Pure stratification + Perlin | Layers Y + 3D noise low-frequency |
| **2.2** Anisotropic Voronoi 3D | Cells stretched along stratification → block-fault model |
| **2.3** Multi-fault planes superposed | Fault planes explicites, chaque ajoute une weakness band |
| **2.4** Multi-octave FBM with directional bias | Fractal brownian motion biaisé horizontalement |
| **2.5** **Composition pondérée** — 2.1 ⊕ 2.2 ⊕ 2.3 ⊕ 2.4 | Stratification dominante + Voronoi blocks + faults sharply localisées + noise low-amp |

**Recommandation Stage 2 = 2.5 composition pondérée**. La géologie réelle est multi-scale et multi-process. Un seul noise ne peut pas capter ça. Coût compute négligeable (champ scalaire pré-computé une fois par cell, ~1 sec / km³ sur CPU).

**Détail mathématique** :

```
S(x,y,z) = w1 · S_strat(y)                # stratification horizontale
        + w2 · S_voronoi(x,y,z)            # blocks tectoniques
        + w3 · S_faults({plane_i}, x,y,z)  # fault planes explicites
        + w4 · FBM(x,y,z, octaves=4)       # rugosité multi-scale
        + w5 · S_hydrothermal(heat_sources) # zones de chaleur post-FP
```

Poids `w1..w5` paramétrables par strate (Strate 1 = stratification + Voronoi modérés, Strate 4+ = faults + hydrothermal plus forts). **Encode l'âge géologique** : superposer 2-3 "ères" de fault planes à orientations différentes crée une histoire visible (fault récente coupe à travers fault ancienne). Adresse C6.

#### 4.5.3 Stage 3 — Geometric realization

| Algo | Principe | Surface quality | Sharp features | Manifold | Coût |
|---|---|---|---|---|---|
| **3.1** Pure marching cubes | Voxel grid + iso-surface | Smooth flou | Non | Non garanti | Modéré |
| **3.2** Dual Contouring (Ju 2002) | Voxel + hermite data | Smooth + sharp | Oui | Non garanti | Modéré |
| **3.3** Surface Nets | Voxel + minimize quadric | Smooth | Limited | Oui | Faible |
| **3.4** **Manifold Dual Contouring** (Schaefer 2007) | DC + manifold guarantee | Smooth + sharp | Oui | **Oui** | Modéré |
| **3.5** Pure SDF + ray-march | Function-only, no voxel | Smooth perfect | Oui | N/A | High runtime |
| **3.6** Hybrid SDF→DC mesh bake | SDF skeleton + DC discretization | Sharp + smooth | Oui | Oui | Modéré bake, free runtime |

**Recommandation Stage 3 = 3.4 Manifold Dual Contouring**, avec **input SDF** dérivé du graph G (Stage 1) + modulation par S (Stage 2).

**Construction du SDF**:

```
F(x,y,z) = max(  -F_tunnel(x,y,z, G, accessibility) ,  # carving (negative inside)
                  S_modulated(x,y,z)                  ) # substrate hardness
```

où `F_tunnel` est la distance signée au skeleton du graph (radius variable par edge), modulée par `AccessibilityTag` (EVA tunnels = small radius, sub tunnels = large radius). `S_modulated = S - threshold_strate` détermine où le rocher cède naturellement (faults → érodé même hors tunnel = formes complexes parois).

**Pourquoi ça produit des caves intéressantes** : un tunnel sub-capable qui passe à travers un Voronoi block boundary verra ses parois **suivre le boundary** (fault visible) ; un tunnel EVA qui passe dans du soft sediment verra son diamètre fluctuer (squeeze sections naturelles). C2 + C5 + C6 émergent automatiquement.

#### 4.5.4 Stage 4 — Detail decorator pass

| Algo | Principe |
|---|---|
| **4.1** Poisson disk sampling on surface | Uniform-but-not-regular scatter |
| **4.2** Curvature-driven placement | Stalactites à top concave, sediment à bottom flat |
| **4.3** Wetness / heat field driven | Bioluminescence dans zones humides ; hydrothermal accretion près chaleur |
| **4.4** **Composition 4.1 + 4.2 + 4.3** | Layered scatter rules |

**Recommandation Stage 4 = 4.4 composition** via **PCG Framework UE5.7** (cf. §1.7) — outil natif. Pas besoin de réinventer ce stage.

#### 4.5.5 Stage 5 — Authored override

Pas d'algorithme. Sublevels UE classiques + spatial lookup (BP / data asset) qui déclare quel cell est override. Stage 1-4 sont court-circuités sur ces cells, le mesh authored est streamé tel quel.

### 4.6 Comparaison réévaluation A / B / D / E + pipeline F

Critères C1-C9 explicitement appliqués (★ = 1 / 5) :

| Approche | C1 Carto | C2 Plausi | C3 Variété | C4 Sub/EVA | C5 Surprise | C6 Story | C7 Author | C8 Détermin | C9 Stream |
|---|---|---|---|---|---|---|---|---|---|
| **A** Authored + PCG fill | ★★★★★ | ★★★ | ★★★ | ★★★★★ | ★★ | ★★★ | ★★★★★ | ★★★★★ | ★★★★ |
| **B** Voxel + worm | ★★ | ★★ | ★★ | ★ | ★★★ | ★ | ★ | ★★★★ | ★★★ |
| **D** Graph + spline tunnels | ★★★★ | ★★ | ★★★ | ★★★★ | ★★★ | ★★ | ★★★★ | ★★★★★ | ★★★★ |
| **E** WP + PCG | ★★★ | ★★ | ★★ | ★★★ | ★★ | ★★ | ★★★ | ★★★★ | ★★★★★ |
| **F v1** Pipeline multi-étapes §4.4, PCE kNN uniforme | ★★★★★ | ★★★★★ | ★★★★ | ★★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★★ | ★★★★ |
| **F v2** Pipeline multi-étapes + PCE substrate-biased §4.5.1 | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★ |

**Lecture** :

- **A** ne couvre pas C2 / C5 / C6 — trop authored, manque de surprises géologiques, manque de plausibilité multi-scale.
- **B** ne couvre pas la cartographie, l'authoring, ni la story. C'est l'approche "Minecraft caves" — fonctionnelle mais générique.
- **D** est proche mais manque de substrate géologique → formes pauvres (cylindres aboutés). Les splines tunnels sont ce que **F-Stage-3** remplace par SDF+DC.
- **E** = D mais UE-native, mêmes limites.
- **F v1** était cohérent comme pédagogie, mais son PCE kNN uniforme peut produire un réseau trop abstrait.
- **F v2** couvre les 9 critères parce que **chaque étape adresse explicitement un sous-ensemble** et que le PCE est maintenant lié au substrate, pas seulement au hasard spatial.

### 4.7 Recommandation finale

**Adopter F v2 — pipeline multi-étapes découplées (§4.4) + PCE substrate-biased (§4.5.1)** avec algorithmes :

| Stage | Algo retenu |
|---|---|
| 1 — Topology | **1.5 + 1.6** Authored outposts + `G_sub` clairsemé + `G_PCE` substrate-biased, MST/cycles/routage anisotrope |
| 2 — Substrate | **2.5** Composition pondérée (stratification + Voronoi + faults + FBM) |
| 3 — Geometry | **3.4** Manifold Dual Contouring driven by SDF(G) ⊕ S |
| 4 — Detail | **4.4** Composition Poisson + curvature + wetness/heat (via PCG) |
| 5 — Override | Sublevels UE authored |

**Lignée marching cubes existante** : ne pas la prendre comme architecture finale, mais ne pas la supprimer aveuglément pendant la phase de référence. Elle sert de comparaison visuelle et de preuve que le SDF/smooth-min produit des formes organiques valides. Pour le générateur final, `DA_BiomeField_*` doit plutôt évoluer vers des profils Stage 2/3 : substrate, conductance PCE, rayons par famille, bruit morphologique.

**Mode d'orchestration** :

- 5 modules C++ dans `Sub3DRuntime` (ou nouveau module `Sub3DWorldGen`)
- Pipeline tournée **offline** au premier chargement d'un seed (5-30 min selon strate)
- Outputs cachés en `Saved/WorldBake/<seed>/<strate>/<cell>.bin`
- WP cells consomment les bakes au runtime

### 4.8 Heuristiques pour caves intéressantes (au-delà du pipeline)

Le pipeline §4.4 est le **squelette structurel**. Les caves intéressantes émergent aussi de paramètres de tuning. Liste de heuristiques à appliquer dans la config Stages 1-2 :

1. **Degré contrôlé par couche.** `G_sub` doit rester lisible (degré moyen ~2-3). `G_PCE` peut monter à ~2.4-3.6, mais au-dessus il devient illisible en carte.
2. **Variance dimensionnelle des chambers.** Ratio largest:smallest dimension d'une chamber doit varier dans Stage 1.5 (tirage stochastique par node type). Toutes égales = boring.
3. **Squeeze sections obligatoires mais rares.** Encoder comme sous-segments PCE, pas comme toute une branche. Un squeeze est une contrainte locale, pas une catégorie entière de tunnel.
4. **Vertical drops / chimneys.** Les produire par routage anisotrope dans `C(x,y,z)` quand une fault verticale domine. Cartographie 3D non triviale, exploration verticale.
5. **Loops dans le graph** (pas que tree). Le `k extra edges` du Stage 1.5 produit cette propriété. Multiplie choix nav, replay, surprises.
6. **Dead-end alcoves récompensantes.** Autorisées, mais budgetées. Un PCE intéressant est d'abord un réseau ; les dead-ends sont des accents, pas la structure.
7. **Geological coherence locale.** Une chamber dans un block Voronoi (Stage 2.2) doit lire comme la même "ère" géologique que ses voisines proches. Le substrate field garantit ça naturellement.
8. **Hydrothermal influence locale.** Certaines chambres = heat source → Stage 4 modifie scatter (mineral accretion + biolum density).
9. **Erosion age layering.** Stage 2 encode plusieurs ères (2-3 sets de fault planes à orientations différentes). Visuel riche, story emergent.
10. **Sub-trunk G_sub + réseau PCE G_PCE superposés** — décision architecturale §4.5.1. Le sub trace une route lisible ; le réseau PCE dense est l'exploration intriquée. Les deux apparaissent sur la cartographie avec opacité différenciée. Voir aussi démo `Sub3D_CavePipeline_Demo.html` Stage 1 mise à jour.

### 4.9 Points d'attention implémentation (non-décisifs mais à noter)

- **Manifold Dual Contouring** : implémentations open-source disponibles (C++/Rust). Pas à coder from scratch.
- **SDF library** : composer Inigo Quilez primitives (`smin`, `opUnion`, `opSubtraction`) + skeleton tunnels = pattern standard.
- **Voronoi 3D** : pas implémenté natif UE ; implémentation Lloyd-relaxed sur grid ou via lib CGAL.
- **Bake time** : 5-30 min par cell × strate × seed est acceptable (1-time per seed). Si trop long, voxel resolution dégradable.
- **WP cell size** : aligner sur la grille SDF (1 cell = N voxels d'un côté ; default 25600 cm UE / 256 voxels à 1 m = 256 voxels — confortable).
- **Mesh collision** : MDC produit du triangle soup ; collision proxy via convex decomposition (V-HACD lib) ou voxel collision direct.

---

## 5. Questions ouvertes pour le créatif

À trancher avant l'Étape 2 (scènes de référence). Numérotation pour facilité de retour.

### Q-1 — Stratégie lighting (load-bearing)

§1.3 propose **baked GI + Lumen Reflections** (pattern DRG, défensif sur 1660 Super @ 60 Hz). Alternative : **Lumen-SW pure** (~8 ms budget, noise sur LED, mais full dynamique).

→ **Réponse créatif 2026-05-13** : (c) à tester ; choix par défaut = **bake GI + stationary lights**. **Cas du sub** (grid local mobile) à traiter à part — voir §1.3 décision tranchée.

### Q-2 — Scope Nanite

Opt-in par catégorie (hero cliffs + tunnels oui, petits props non) — vs opt-in projet-wide. Tester sur 1660 Super avec un cliff Megascans avant d'arbitrer ?

→ **Réponse créatif 2026-05-13** : **skip Nanite** par défaut. Si le coût perf n'est pas un gain net, on ne part pas dessus. Pas nécessaire pour le style cible (stylé, pas photogrammétrie). Voir §1.1 décision tranchée.

### Q-3 — Résolution conflit "low-poly stylisé + PBR réaliste"

§3.2 propose : Meshy low-poly + Megascans cliff hero, **pass uniforme via masters Sub3D** (tinte, wetness, `MF_EdgeHighlight`). Tu confirmes que les masters doivent porter cette "couche painterly" pour unifier, ou tu veux séparer (deux sets de masters distincts par origine d'asset) ?

→ **Réponse créatif 2026-05-13** : **pas de PBR réaliste** ; shaders stylés. **Master unificateur retenu** (pas deux sets). L'ambiance du jeu vient des lights + caves + mouvement, pas du détail photogrammétrique. Voir §3.2 réécrit.

### Q-4 — Refactor `DepthPostProcessActor.cpp`

§3.4 + §0.3 : ce composant utilise une `ADirectionalLight` qui fade — contredit `abyssal_setting`. Je propose de le démanteler et remplacer par 2 PP volumes + Volumetric Fog driver. Tu valides le démantèlement ou tu veux que je documente d'abord pourquoi il était là ?

→ **Réponse créatif 2026-05-13** : **démanteler** (jamais vraiment utilisé). 2 PP volumes pas tranché — le créatif a déjà **un PP qui s'applique sous l'eau**. Comme l'env entier est sous l'eau, ce PP couvre tout l'extérieur du sub. À reclarifier : 1 PP global "underwater" + 1 PP intérieur sub airy plutôt que la division InsideHull/OpenWater initialement proposée. **Question ouverte** : ce PP existant est-il déjà calibré sur le stack 9-éléments §1.6 ou faut-il l'enrichir ?

### Q-5 — Cartographie et seed

Pillar signature §4 contrainte. Choix : (a) Strate 1 fully authored (FP) + post-FP idem ; (b) Strate 1 authored, Strates suivantes seed-locked ; (c) seed-locked dès Strate 1 (full generator early). Concept V2 ne tranche pas.

→ **Réponse créatif 2026-05-13** : **reporté**. Focus actuel = scènes de test. Indéterminé si traversées Strate 1 sont **semi-open persistant avec checkpoints** (modèle Subnautica) ou **levels procéduraux start-to-end** roguelite-style (modèle FTL / Hardspace). Pipeline F §4 supporte les deux, mais l'integration WP diffère sensiblement. À retrancher quand la direction missions/loop sera tranchée.

### Q-6 — Validation pipeline F (architecture caves §4.4)

Tu valides le **pipeline multi-étapes** comme architecture de base (Stages 1 → 5) ? Et les algorithmes retenus par stage (§4.7 — 1.5 / 2.5 / 3.4 / 4.4) ? Ou tu veux qu'on creuse une étape en particulier avant de la fixer ? Notamment Stage 3 (MDC vs SDF pure runtime) est l'arbitrage le plus lourd : MDC = bake offline mesh, SDF pure = ray-march runtime ; MDC est ma reco mais SDF reste théoriquement supérieur côté qualité de surface.

→ **Réponse créatif 2026-05-13** : **pipeline F validé en principe**, puis corrigé en **F v2** après lecture des screenshots/proto. Changement principal : `G_PCE` ne doit pas être une simple couche Poisson/kNN uniforme ; il doit être un réseau PCE substrate-biased, avec cycles contrôlés et routage anisotrope. Démo HTML `Sub3D_CavePipeline_Demo.html` illustre chaque stage visuellement, avec comparaison proto organique.

### Q-6bis — Tags accessibility Stage 1 (et autres layers de placement)

Taxonomie proposée pour les edges du graph : `Sub` / `EVA` / `Squeeze`. Tu en veux d'autres ? Candidates possibles : `ClimbOnly` (vertical chimneys non-EVA-flottants), `SwimmableOnly` (passages où le crew doit lâcher l'équipement), `HydrothermalHazard` (passages avec dégâts chaleur), `CollapsedBypassable` (passage temporairement bloqué jusqu'à breach). Lié au game design plus qu'à l'archi.

→ **Réponse créatif 2026-05-13** : tags candidats validés, **d'autres viendront**. Question ouverte étendue : **ressources à miner, spawn mobs, hazards environnementaux** — où vivent-ils ?
- (a) tags sur les edges (tunnel "contient" du minerai X) ;
- (b) tags sur les nodes (chamber "contient" mob Y) ;
- (c) layer indépendant superposé au graph (placement pass séparé, ex. Stage 4.5 = "Population pass" qui consomme topology + substrate).

Recommandation tech : **(c) layer indépendant**. Garde Stage 1 focused sur la géométrie / accessibility ; les ressources / mobs / hazards = pass séparée qui peut être re-tirée sans reflood Stage 1-3. Permet aussi de faire varier la population au sein d'un même monde déterministe (rééquilibrage, events) sans rebake le mesh.

### Q-6ter — Ères géologiques Stage 2

§4.5.2 propose 2-3 ères de fault planes superposées par strate pour encoder de la "story embedded" (C6). Combien d'ères par strate te paraît justifié ? 2 = simple ; 3 = riche ; >3 = visuellement bruité.

→ **Réponse créatif 2026-05-13** : **3 ères**. Implémenté dans la démo HTML Stage 2 (3 groupes de fault planes à orientations distinctes, codés par tinte différente).

### Q-7 — Devenir des `DA_BiomeField_*` existants

Trois options : (a) wipe complet, replace par nouveaux DA Stage 2 config ; (b) garder le format, refactor le contenu pour servir Stage 2 ; (c) garder ligne marching cubes en parallèle comme legacy non-utilisée pour future référence. **Pas de contrainte de temps** — tu peux trancher (a) pour propreté du code si tu préfères.

→ **Réponse créatif 2026-05-13, révisée après screenshots/proto** : ne pas conserver l'ancien système comme architecture finale, mais ne pas le supprimer tant qu'il sert de référence visuelle. Le proto actuel prouve que SDF + smooth-min + bruit organique produit des silhouettes intéressantes. Action recommandée : figer des captures/réglages de référence, puis migrer les `DA_BiomeField_*` vers nouveaux profils Stage 2/3. Suppression du code legacy seulement après équivalence visuelle vérifiée dans la nouvelle pipeline.

### Q-8 — Sub3D vs DRG pondération visuelle

Le Concept V2 cite **Subnautica + horror plus tard** comme ton ; **DRG** comme philosophie matériau. Tension : Subnautica = full Lumen dynamic + heavy volumetric (water game) ; DRG = baked + minimal volumetric (cave game). Sub3D est plus proche DRG en tone (industrial dark) mais Subnautica en setting (water). Où mettre le curseur (60 % DRG / 40 % Subnautica par défaut) ?

→ **Réponse créatif 2026-05-13** : **reporté**. Pas pertinent ici, à trancher plus tard. Direction par défaut = **DRG-leaning** (pondération exacte à figer plus tard).

### Q-9 — Première scène de référence — biome

Avant d'attaquer Étape 2, quel biome précis Strate 1 pour la première scène : plaine abyssale ouverte (test fog + scale) / champ hydrothermal foreshadow Strate 2 / canyon vertical / zone d'épave près avant-poste / autre ?

→ **Réponse créatif 2026-05-13** : **canyon vertical avec cavités**. Démo HTML composition finale (`Sub3D_CavePipeline_Demo.html` § "Canyon vertical") montre la cible.

### Q-10 — Filename + location du doc

J'ai placé ce doc en `reports/concept/Sub3D_WorldDesign_Reference.md` (à côté du Concept V2). Si tu préfères `reports/world-design/` ou autre, dis-le maintenant pour éviter un rename downstream.

---

## Annexe — sources techniques principales

- **UE5.7** — [Nanite](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine), [Nanite Foliage](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage), [Lumen](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine), [Lumen Perf Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine), [Volumetric Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine), [Local Fog Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine), [Exponential Height Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine), [PCG Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- **World Partition** — [Listen Servers KB](https://dev.epicgames.com/community/learning/knowledge-base/D7lL/unreal-engine-issues-with-using-listen-servers-with-world-composition-and-world-partition), [Server Streaming KB](https://dev.epicgames.com/community/learning/knowledge-base/Xdj9/unreal-engine-world-partition-server-streaming), [HLOD tips](https://dev.epicgames.com/community/learning/tutorials/z050/unreal-engine-5-world-partition-hlods-tips-tricks)
- **Niagara** — [Optimization scalability](https://dev.epicgames.com/community/learning/tutorials/15PL/unreal-engine-optimizing-niagara-scalability-and-best-practices), [Measuring perf](https://dev.epicgames.com/community/learning/tutorials/0qPO/unreal-engine-optimizing-niagara-measuring-performance)
- **Post-process underwater** — [Subnautica rendering / Game Developer](https://www.gamedeveloper.com/design/how-i-subnautica-i-plunges-deeper-into-rendering-realistic-water), [Underwater lighting UE5 — 80.lv](https://80.lv/articles/here-is-how-to-set-up-underwater-lighting-in-unreal-engine-5), [Lumen optimization 60 FPS — StraySpark](https://www.strayspark.studio/blog/lumen-optimization-masterclass-60fps), [To bake or not to bake 2024](https://lucaslabstudio.wordpress.com/2024/07/03/to-bake-or-not-to-bake-unreal-engine-5-lumen/)
- **Megascans / Fab** — [Quixel licence (free pour UE)](https://quixel.com/en-US/license), [Megascans March 2025 update](https://forums.unrealengine.com/t/megascans-update-march-2025/2419561), [Nanite optimization Medium](https://medium.com/@GroundZer0/nanite-optimizations-in-unreal-engine-5-diving-into-nanite-performance-a5e6cd19920c)
- **DRG art reference** — [UE spotlight Ghost Ship](https://www.unrealengine.com/en-US/spotlights/how-ghost-ship-games-found-success-with-deep-rock-galactic), [DRG Wiki Biome Features](https://deeprockgalactic.wiki.gg/wiki/Biome_Features), [GDC Vault Agents of Mayhem stylized PBR (substitut)](https://gdcvault.com/play/1024690/-Agents-of-Mayhem-Physically)
- **Free / CC0** — [Poly Haven](https://polyhaven.com/textures), [ambientCG](https://ambientcg.com/)

---

**Fin du doc.** Étape 2 (accompagnement scènes) démarre après validation par le créatif des Q-1 à Q-10.
