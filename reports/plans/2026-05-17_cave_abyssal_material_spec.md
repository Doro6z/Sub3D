# Cave Abyssal — Material + Asset Harmonization + Atmosphere Spec

| | |
|---|---|
| **Date** | 2026-05-17 (split 2026-05-18) |
| **Statut** | Spec actif — prêt pour implémentation. Split en 4 docs pour fluidité IDE. |
| **Scope** | M_CaveAbyssal · M_CaveRockAsset · PostProcess Underwater · Fog · Lighting · Stylized DA |
| **Outil associé** | [`reports/tools/abyssal_palette_lut.html`](../tools/abyssal_palette_lut.html) (palette LUT generator) |
| **Tracking** | Atlas Asset `cave-procedural-material` (Sub3D-Atlas → Assets) |

---

## Architecture documentaire (split 2026-05-18)

Ce doc principal contient : header, TOC, TL;DR, §1 Vision, §2 Architecture, §4 Rocks, **§5 PostProcess / §6 Fog / §7 Lighting / §7-bis Color Tokens** (canonical-aligned), §8 Order tracker, §8-bis backlog, §9-13 reference.

**Annexes** :

| Annexe | Contenu | Quand l'ouvrir |
|---|---|---|
| [cave_abyssal_master_graph.md](cave_abyssal_master_graph.md) | §3.1 settings · §3.2 pins · §3.3 params · §3.5 perf · §3.6 Group A→L master câblage · §3.7 test workflow · §3.8 LUT procedure | Quand tu câbles le master M_CaveAbyssal ou tweaks les params MI |
| [cave_abyssal_mf_graphs.md](cave_abyssal_mf_graphs.md) | §3.4 Material Functions step-by-step (A=Triplanar, B=Sediment, C=Strata, D=Fracture, E=Final color, F=Final roughness, G=Biolum, H=WetSurface) | Quand tu crées ou modifies une MF spécifique |
| [cave_abyssal_stylization.md](cave_abyssal_stylization.md) | §14 Track B Stylized DA : LUT presets · sources textures · **MF_StylizePBR refactor v2** (per-step strengths) · intégration master · pièges · calibration | Quand tu travailles le look stylized hand-painted |
| [cave_abyssal_observation_doc.md](2026-05-17_cave_abyssal_observation_doc.md) | Frictions / décisions / lock historique | Référence rétroactive |

**Specs canoniques liées (créées 2026-05-18)** :

| Spec canonique | Quand consulter |
|---|---|
| [lighting_pp_fog_canonical_spec.md](2026-05-18_lighting_pp_fog_canonical_spec.md) | Source de vérité PP/Fog/Lights. §5/§6/§7 de ce doc sont alignés mais le canonical a les valeurs complètes copy-paste UE5.7 |
| [project_settings_canonical_spec.md](2026-05-18_project_settings_canonical_spec.md) | Source de vérité Project Settings (Lumen→SSGI, Substrate off, etc.) |

---

## Sommaire ce doc principal

| # | Section | Type | Lecture |
|---|---|---|---|
| — | [TL;DR — 1 page récap](#tldr--1-page-récap) | summary | 2 min |
| 1 | [Vision & Goal](#1-vision--goal) | strategy | 1 min |
| 2 | [Architecture matériaux (vue d'ensemble)](#2-architecture-matériaux--vue-densemble) | overview | 2 min |
| 3 | [M_CaveAbyssal — Master Material spec](#3-m_caveabyssal--master-material-spec) | **annexe** | renvoi |
| 4 | [Rocks assets harmonization](#4-rocks-assets-harmonization) | spec | 5 min |
| 5 | [PostProcess — PP_Underwater_Abyssal](#5-postprocess--pp_underwater_abyssal) | spec aligné canonical | 4 min |
| 6 | [Fog — ExpHeightFog config](#6-fog--exponentialheightfog-config) | spec aligné canonical | 3 min |
| 7 | [Lighting setup](#7-lighting-setup) | spec aligné canonical | 3 min |
| 7-bis | 🎨 [**Cohérence palette diégétique sub vs cave**](#7-bis-cohérence-palette-diégétique-sub-vs-cave) (Color Tokens) | DA canon | 3 min |
| 8 | 🎯 [**Order of implementation**](#8-order-of-implementation-priority) | plan exécutable | 2 min |
| 8-bis | [Backlog Phase 2](#8-bis-backlog-phase-2--enrichissement-géométrie-post-fp) | backlog | 2 min |
| 9 | [Validation criteria](#9-validation-criteria) | checklist | 1 min |
| 10 | [Risques connus & contournements](#10-risques-connus--contournements) | reference | 2 min |
| 11 | [Files à créer](#11-files-à-créer) | reference | 1 min |
| 12 | [Liens utiles](#12-liens-utiles) | reference | — |
| 13 | [À ne PAS faire](#13-à-ne-pas-faire) | guardrails | 1 min |
| 14 | [Track B Stylized](#14-stylized--hand-painted-da--track-b-annexe) | **annexe** | renvoi |

**Total ce doc** : ~25 min lecture · ~10 min pour TL;DR + §8 + §5/§6/§7 si focus PP/Fog/Lights.

---

## TL;DR — 1 page récap

### Objectif

Look cohérent **abyssal stylized hand-painted** (réf : Subnautica Below Zero · Sea of Thieves) sur **toute la cave** : mesh procédural PMC + rocks/boulders posés en PCG + atmosphère/fog/PP. À partir d'assets PBR neutres (Megascans / Poly Haven) transformés en shader, OU de packs stylized sourcés.

### Architecture en 3 couches

```
┌─────────────────────────────────────────────────────────────────┐
│  COUCHE 1 — 7 Material Functions partagées (foundation)         │
│                                                                 │
│  Triplanar · StrataBanding · BiolumSpots · SedimentOverlay      │
│  FractureMask · WetSurface · StylizePBR (§14)                   │
└─────────────────────────────────────────────────────────────────┘
                          ↓ utilisées par ↓
┌─────────────────────────────────────────────────────────────────┐
│  COUCHE 2 — 3 Master materials                                  │
│                                                                 │
│  M_CaveAbyssal       (PMC procedural, triplanar, pas d'UVs)     │
│  M_CaveRockAsset     (assets PCG, UV mesh + mêmes MFs overlay)  │
│  M_CaveBiolumCluster (cristaux émissifs HDR)                    │
└─────────────────────────────────────────────────────────────────┘
                          ↓ instances ↓
┌─────────────────────────────────────────────────────────────────┐
│  COUCHE 3 — MIs paramétrées par strate / variante               │
│                                                                 │
│  MI_CaveAbyssal_Coastal | _Pelagic | _Bathyal | _Abyssal | …    │
│  MI_CaveRock_Default | _Sediment | _Biolum | _Stalactite | …    │
└─────────────────────────────────────────────────────────────────┘
```

### Le secret de cohérence

**Toutes les variations (strata, biolum, fracture, stylization) lisent `WorldPosition`**, pas les UVs locales. Conséquence : un boulder posé sur la paroi cave hérite des mêmes bandes strates et tâches biolum que le mesh procédural à cet endroit → **le boulder paraît taillé dans la même roche, pas plaqué dessus**.

### Atmosphère en 4 settings critiques

| Asset | Setting clé | Valeur |
|---|---|---|
| `ExponentialHeightFog` | **Fog Height Falloff** | `0.0` → fog uniforme par distance (pas par hauteur) |
| `ExponentialHeightFog` | **Volumetric Fog** | **activé** (§6.3 — nécessaire pour shafts depuis sources locales) |
| `PostProcessVolume` | **Exposure Min/Max EV100** | `12.0 / 12.0` → expo manuelle fixe |
| Scene | **DirectionalLight** | **absente** — pas de soleil en abysse |

### Cohérence palette diégétique (§7-bis)

**Le DA Sub3D repose sur 2 palettes contrastées** — c'est la mécanique visuelle clé :

- **Intérieur sub** = tungstène chaud (ambre #FF9D60, cuivre #A57212) — refuge habité
- **Cave abyssale** = cool violet/teal (shadows #1F1C2D, biolum #2DC4B0) — hostile vide
- **Transitions sas** = contraste warm↔cool fort, à exploiter narrativement

Light shafts dans la cave = **sources locales** (failles biolum massives, sub headlight, EVA flashlight, cristaux émissifs HDR). **Jamais surface**. Volumetric Fog les rend visibles.

### Stylized DA (§14)

**Outil prêt** : [`reports/tools/abyssal_palette_lut.html`](../tools/abyssal_palette_lut.html) → ouvre dans Chrome → "Download T_AbyssalPalette_LUT.png" → import UE5 (`VectorDisplacementMap`, sRGB décoché).

**Recommandation solo dev** : 1 pack stylized anchor ($15-30 ArtStation) + `MF_StylizePBR` qui transforme tout le reste (Megascans gratuit, Poly Haven CC0) en cohérent stylized via le LUT. ROI maximal — pas besoin de sourcer 30 packs hand-painted.

### Order of implementation — 4 premières étapes pour résultat visible

1. **`MF_Triplanar_PBR`** (foundation)
2. **`MF_BiolumSpots`** (gros impact visuel, Voronoi3D)
3. **`M_CaveAbyssal`** master + `MI_CaveAbyssal_Default` → **assigne sur `AGeologicalCaveActor.MeshMaterial`** → cave avec look basique mais cohérent immédiat
4. **`T_AbyssalPalette_LUT`** + **`MF_StylizePBR`** → passage au look stylized (gros saut visuel)

Suite des steps (5-10) en §8. PP/Fog/RockAsset/Strata/Fracture viennent après.

---

## 1. Vision & Goal

Produire un **look cohérent abyssal** pour toute la cave : le mesh procédural (PMC d'`AGeologicalCaveActor`), les rocks/boulders/stalactites/stalagmites posés en PCG, ET le post-process/fog ambiant, doivent tous partager le **même langage visuel** :

- **Palette** : noir-bleuté profond + bandes sédimentaires brun-vert + ponctuations cyan biolum
- **Surface** : humide (roughness basse + variation), érodée (pas plate)
- **Lumière** : pas de DirLight ; PointLights biolum + EVA flashlight uniquement
- **Atmosphère** : fog distance-uniforme, particulates volumétriques, contraste écrasé

**Anti-objectif** : ne PAS reproduire un look "spelunking surface" (cave terrestre éclairée par lampe frontale jaune sur roche sèche orange). On est en abysse.

---

## 2. Architecture matériaux — vue d'ensemble

```
┌──────────────────────────────────────────────────────────────────────┐
│  Material Functions (réutilisables — toutes dans /Game/Sub3D/MF/)    │
├──────────────────────────────────────────────────────────────────────┤
│  MF_Triplanar_PBR        ← input: 3 textures (BC+N+R), tiling, blend │
│  MF_StrataBanding        ← input: WorldPos, params strates           │
│  MF_BiolumSpots          ← input: WorldPos, Voronoi → emissive       │
│  MF_SedimentOverlay      ← input: WorldNormal, params seuil          │
│  MF_FractureMask         ← input: WorldPos, noise → multiply BC      │
│  MF_WetSurface           ← input: roughness, dampen + spec tweak     │
│  MF_StylizePBR           ← input: BC/N/R + LUT, posterize+remap+edge │
│                            (voir §14 — transformation hand-painted)  │
└──────────────────────────────────────────────────────────────────────┘
                              ↓ utilisées par ↓
┌──────────────────────────────────────────────────────────────────────┐
│  Master materials                                                    │
├──────────────────────────────────────────────────────────────────────┤
│  M_CaveAbyssal           ← pour le PMC (triplanar, pas d'UVs)        │
│  M_CaveRockAsset         ← pour les rocks PCG (UVs mesh + procedural)│
│  M_CaveStalactiteAsset   ← variante stalactite (Z-bias biolum tip)   │
│  M_CaveBiolumCluster     ← cristaux émissifs, emissive HDR           │
└──────────────────────────────────────────────────────────────────────┘
                              ↓ instances ↓
┌──────────────────────────────────────────────────────────────────────┐
│  MI_CaveAbyssal_Coastal | _Pelagic | _Bathyal | _Abyssal | _Trenches │
│  MI_CaveRock_Default | _Sediment | _Biolum                           │
└──────────────────────────────────────────────────────────────────────┘
```

Tous les masters utilisent les **mêmes Material Functions** → cohérence garantie. Les MI varient seulement les paramètres exposés (couleur, intensité biolum, scale strates).

---

## 3. M_CaveAbyssal — Master Material spec

> 📑 **Spec détaillée extraite en annexes** (split 2026-05-18 pour fluidité IDE).
> Le contenu §3 complet est maintenant dans **2 annexes** :

| Annexe | Contenu |
|---|---|
| [cave_abyssal_master_graph.md](cave_abyssal_master_graph.md) | §3.1 Material settings · §3.2 Pins · §3.3 Parameters exposés · §3.5 Performance · §3.6 Group A→L master graph câblage complet · §3.7 Workflow de test · §3.8 Procédure T_AbyssalPalette_LUT |
| [cave_abyssal_mf_graphs.md](cave_abyssal_mf_graphs.md) | §3.4 Material Functions step-by-step (A=Triplanar, B=Sediment, C=Strata, D=Fracture, E=Final color, F=Final roughness, G=Biolum, H=WetSurface) |

**Pour le step 5 (PP/Fog/Lights canonical)** : continue sur ce doc principal §5/§6/§7 ci-dessous.

**Pour reprendre l'implémentation** :
- Refactor MF_StylizePBR v2 (per-step strengths) → [cave_abyssal_stylization.md](cave_abyssal_stylization.md) §14.2
- Re-tag MIs avec Color Tokens linear → §7-bis ci-dessous


## 4. Rocks assets harmonization

### 4.1 Le problème

Les rocks/boulders/stalactites sourcés (Megascans, Poly Haven, Meshy) viennent avec :
- Leur propre BaseColor / Normal / Roughness en textures **UV-mapped**
- Une teinte qui dépend du pack source (rarement du basalte abyssal humide)
- Pas de strates, pas de biolum

→ Posés tels quels, ils auront l'air **plaqués** sur le mesh procédural, comme deux mondes.

### 4.2 Solution : M_CaveRockAsset master

Un master material **dédié aux assets PCG**, qui combine :
- Les textures du mesh (UV-mapped, pas triplanar)
- Les MÊMES Material Functions procedural overlays que M_CaveAbyssal (strata, biolum, sediment, fracture)

```
┌────────────────────────────────────────────────────────────────────┐
│  M_CaveRockAsset                                                   │
├────────────────────────────────────────────────────────────────────┤
│  Inputs : T_AssetBC (UV), T_AssetN (UV), T_AssetR (UV)             │
│                                                                    │
│  Step 1 : Sample asset textures via UVs (standard)                 │
│  Step 2 : Apply RockTint multiply → unify color avec M_CaveAbyssal│
│  Step 3 : Apply MF_StrataBanding (WorldPos) → bandes traversent   │
│           l'asset comme si elles étaient géologiques               │
│  Step 4 : Apply MF_FractureMask (WorldPos) → cracks cohérents     │
│  Step 5 : Apply MF_BiolumSpots (WorldPos, lower density)          │
│  Step 6 : Apply MF_WetSurface → roughness dampen                  │
└────────────────────────────────────────────────────────────────────┘
```

**Magie** : comme strata + biolum + fracture lisent **WorldPosition**, les bandes traversent à la fois la paroi de la cave ET les boulders posés dessus → l'asset paraît avoir été « taillé dans la même roche ». Pas de discontinuité visuelle.

### 4.3 Parameters M_CaveRockAsset (MI override par asset type)

| Param | Default | Note |
|---|---|---|
| `T_AssetBC` | (par mesh) | texture BC du pack |
| `T_AssetN` | (par mesh) | normal du pack |
| `T_AssetR` | (par mesh) | roughness du pack |
| `RockTint` | (0.50, 0.55, 0.60) | **MÊME couleur que M_CaveAbyssal** |
| `TintStrength` | 0.6 | 0 = texture pure, 1 = full tint |
| `StrataPeriod` | 800 | **MÊME valeur que M_CaveAbyssal** |
| `StrataDarkenAmount` | 0.25 | légèrement moins fort sur asset |
| `BiolumDensity` | 0.03 | moins dense sur boulder (le gros vient de la paroi) |
| `BiolumNoiseScale` | 0.0005 | **MÊME** |
| `FractureIntensity` | 0.4 | |
| `WetSurfaceDampen` | 0.15 | |

> **Règle d'or** : si un param est partagé entre cave et asset (strata, biolum), il doit avoir la **même valeur**. Sinon les bandes ne s'alignent plus → effet de couture.

### 4.4 Variantes MI à créer

| MI | Usage | Override notable |
|---|---|---|
| `MI_CaveRock_Default` | Boulders standards | RockTint = palette globale |
| `MI_CaveRock_Sediment` | Rocks au sol couverts de silt | + sediment overlay |
| `MI_CaveRock_Biolum` | Cluster cristaux | BiolumIntensity = 8.0, FractureIntensity = 0 |
| `MI_CaveStalactite_Default` | Pendants plafond | StrataPeriod = 400 (bandes plus fines), tip emissive |
| `MI_CaveStalagmite_Default` | Pointes au sol | identique à stalactite mais inversé |

### 4.5 Pipeline de transformation des assets sourcés

Quand tu importes un rock de Megascans/PolyHaven :

1. **Importer** mesh + textures dans `Content/Sub3D/Meshes/Cave/Rocks/<pack_name>/`
2. **Créer MI** dérivée de `M_CaveRockAsset` :
   - Override `T_AssetBC`, `T_AssetN`, `T_AssetR` avec les textures du pack
   - Garder tous les autres params au default → cohérence cave
3. **Assigner MI** au mesh slot dans le Static Mesh Editor
4. **Tester en context** : poser le mesh sur la cave actor, vérifier que les bandes strates s'alignent

**Test rapide de cohérence** : pose 3 boulders différents côte à côte sur la paroi cave. Si l'œil distingue immédiatement "c'est plaqué", revoir RockTint / TintStrength jusqu'à intégration.

---

## 5. PostProcess — PP_Underwater_Abyssal

> ⚠️ **2026-05-18 — Valeurs alignées sur [canonical spec lighting/PP/fog](2026-05-17_lighting_postprocess_fog_spec.md)**.
> Le canonical est la source de vérité pour cette section. Les valeurs ci-dessous sont synchronisées.
> **8 décisions verrouillées** (cf canonical §564) — toute modification doit faire l'objet d'un nouveau spec daté qui supersede explicitement.
>
> 🔁 **State-driven runtime (2026-05-18 controller lock)** — Les valeurs PP listées ci-dessous décrivent **le baseline statique** (PostProcessVolume Unbound, ambiance cave abyssal "dry default"). À runtime, l'override dynamique est piloté par [`UAtmosphereStateController`](2026-05-18_postprocess_state_controller_spec.md) via composition séquentielle de DA D2 (Medium × Location × Biome) appliquée sur `UPostProcessComponent` de la camera. Les transitions (Medium 0.4 s ease-out, Location 1.0 s ease-in-out, Biome 40 m smoothstep) sont la spec du controller, **pas du volume**. Décision 1+2+3 controller spec.

### 5.1 Volume

| Setting | Valeur |
|---|---|
| Bounds | **Unbound** = true (affecte tout le niveau) |
| Priority | 0 |
| Blend Weight | 1.0 |

### 5.2 Lens — Exposure (auto bornée, pas manual locked)

> **Décision 1+2 canonical** : auto-exposure avec bornes serrées + `bApplyPhysicalCameraExposure = false` (sinon Min/Max EV100 ignoré).

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Metering Mode** | **Auto Exposure Histogram** | Adaptation joueur naturelle, jamais 100% black |
| **Apply Physical Camera Exposure** | **false** | ⚠️ Critique — sinon Min/Max ignorés |
| **Min EV100** | **-0.5** | Plancher bas pour ambiance abyssale |
| **Max EV100** | **+1.5** | Headroom pour biolum HDR |
| **Exposure Compensation** | **-0.3** | Push global vers le sombre (signature abyssale) |
| **Exposure Speed Up** | 4.0 | Adaptation rapide quand on entre dans le noir |
| **Exposure Speed Down** | 2.0 | Adaptation lente quand on sort vers la lumière |
| **Histogram Log Min** | -8 | Pour gérer la dynamique cave noire / biolum HDR |
| **Histogram Log Max** | 4 | |

### 5.2-bis Lens — Local Exposure (HDR cinema)

| Setting | Valeur | Note |
|---|---|---|
| **Highlight Contrast Scale** | 0.85 | |
| **Shadow Contrast Scale** | 1.15 | |
| **Detail Strength** | **1.0** | ⚠️ corrigé 2026-05-18 (était 4.0 — divergeait canonical, **> 2 crée halos abherants sur ULightComponent** ; biolum-only scenes tolèrent jusqu'à 4-6 mais sub headlamp + EVA flashlight présents = 1.0–2.0 max) |

### 5.2-ter Lens — Bloom / CA / Vignette

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Bloom Intensity** | **0.6** | Biolum doit pop |
| **Bloom Threshold** | **0.85** | Plus de halo sur sources moyennes |
| **Bloom Size Scale** | 1.2 | Plus diffus, plus "wet" |
| **Vignette Intensity** | **0.55** | Pousse le claustrophobe abyssal |
| **Chromatic Aberration** | **0.4** | Suggère "hublot épais" |
| **Chromatic Aberration Start Offset** | **0.0** | ⚠️ corrigé 2026-05-18 (était 0.5 — crée stries horizontales visibles en bord écran, cf friction observation doc) |
| **Lens Flares Intensity** | 0.1 | Subtil sur sub headlight |

### 5.3 Color grading — saturation LIFT (pas crush)

> **Décisions 3+4 canonical** : Custom Tone Curve + saturation lift au lieu de crush (le crush était mon erreur initiale).

#### Tone Curve (Film)

| Param | ACES default | **Sub3D recommandé** | Effet |
|---|---|---|---|
| Slope | 0.91 | **0.88** | Réduit légèrement contraste global |
| Toe | 0.53 | **0.42** | Soulève les ombres (less crush) |
| Shoulder | 0.23 | **0.30** | Roll-off plus tendre sur biolum HDR |
| Black Clip | 0.0 | 0.0 | Garde true black possible |
| White Clip | 0.04 | **0.08** | Plus de headroom HDR avant clip |

#### Color Grading par range

| Range | Saturation | Contrast | Gamma | Gain | Offset |
|---|---|---|---|---|---|
| **Global** | 1.0 | 1.0 | 1.0 | 1.0 | — |
| **Shadows** | **0.85** | 1.05 | 0.95 | (0.95, 0.92, 1.0) | (-0.005, -0.005, +0.005) |
| **Midtones** | **1.05** | 1.0 | 1.0 | 1.0 | — |
| **Highlights** | **0.90** | 0.95 | 1.02 | (1.05, 1.02, 0.98) | — |

| Bordure | Valeur |
|---|---|
| **ShadowsMax** | 0.09 |
| **HighlightsMin** | 0.5 |

### 5.4 Rendering features

| Setting | Valeur | Note |
|---|---|---|
| **Global Illumination Method** | **Screen Space (Beta) — SSGI** | Pas Lumen, voir décision 2 canonical |
| **SSGI Quality** | 50 | |
| **Reflection Method** | **Lumen** | Reflections gardé (cheap, utile wet rock) |
| **Lumen Reflections Quality** | 0.5 | |
| **Lumen Final Gather Quality** | 0.5 | |
| **Lumen Scene Detail** | 0.5 | |
| **Lumen Scene Lighting Update Speed** | 0.5 | |
| **Indirect Lighting Intensity** | **0.45** | Compense l'absence de Lumen GI |
| **Indirect Lighting Color** (linear) | (0.05, 0.10, 0.15) | tinte froide subtile |
| **Ambient Occlusion Intensity** | 0.8 | Beaucoup pour casser le plat |
| **Ambient Occlusion Radius** | 80 cm | Proche surface |
| **Ambient Occlusion Power** | 1.5 | Renforce le falloff |
| **Ambient Occlusion Fade Out Distance** | 4000 | cm |
| **Anti-Aliasing Method** | **TSR Medium** | Stable en cave avec marine snow |
| **Motion Blur Amount** | **0.0** | **OFF strict** en cave |

### 5.5 PP Materials (Post Process Materials)

À ajouter dans le **Post Process Materials** array du volume, dans cet ordre :

1. **M_PP_UnderwaterParticulate** — marine snow / silt en suspension
2. **M_PP_UnderwaterDistortion** — refraction subtile (sin time × scene UV offset)
3. **M_PP_UnderwaterVignetteRadial** (optionnel) — vignettage paramétrique

#### M_PP_UnderwaterParticulate spec

##### Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `M_PP_UnderwaterParticulate` |
| **Location** | `/Game/Sub3D/Materials/PostProcess/M_PP_UnderwaterParticulate` |
| **Material Domain** | **Post Process** |
| **Blendable Location** | Before Tonemapping |
| **Blendable Priority** | 0 |
| **Description** | Marine snow / silt visible flottant en suspension dans la cave. Effet de profondeur, sensation aquatique. |

##### Material Parameters

| Param | Type | Default | Range | Rôle |
|---|---|---|---|---|
| `T_NoiseTexture` | Texture2D | T_VolNoise_RG | — | Noise grayscale tileable 512² |
| `ScrollSpeed` | Scalar | 0.02 | 0–0.1 | Vitesse défilement vertical |
| `NoiseScale` | Scalar | 4.0 | 1–16 | Tiling du noise sur l'écran |
| `ParticleThreshold` | Scalar | 0.85 | 0.7–0.95 | Seuil pour faire émerger les particules |
| `ParticleIntensity` | Scalar | 0.04 | 0–0.2 | Force visuelle ajoutée à la scène |

##### Étapes de création

###### Étape 1 — UV scrolling

- `ScreenAlignedUVs` (catégorie Coordinates) — ou `ViewportUV` selon UE version
- `Multiply` `UV_Scaled` : A ← ScreenAlignedUVs, B ← NoiseScale (param)
- `Time`
- `Multiply` `Time_Scroll` : A ← Time, B ← ScrollSpeed (param)
- `Constant2Vector` `Scroll_Direction` : value = (0, 1) — vertical only
- `Multiply` `Offset_Vec` : A ← Time_Scroll, B ← Scroll_Direction
- `Add` `UV_Final` : A ← UV_Scaled, B ← Offset_Vec

###### Étape 2 — Sample noise + 2 octaves

- `TextureSampleParameter2D` `Noise_1` : Texture ← T_NoiseTexture, UVs ← UV_Final
- `Multiply` `UV_Octave2` : A ← UV_Final, B ← Constant 2.3 (decorrelation)
- `TextureSampleParameter2D` `Noise_2` : Texture ← T_NoiseTexture, UVs ← UV_Octave2
- `Multiply` `Combined_Raw` : A ← Noise_1.R, B ← Noise_2.R (combine octaves)

###### Étape 3 — Threshold + composition

- `SmoothStep` `Particles_Mask` : Min ← ParticleThreshold (param), Max ← Constant 1.0, Value ← Combined_Raw
- `Multiply` `Particles_Final` : A ← Particles_Mask, B ← ParticleIntensity (param)
- `SceneTexture:PostProcessInput0` `Scene` : pin "Color" → recover scene RGB
- `Add` `Output_Color` : A ← Scene, B ← Particles_Final (white particles added)
- **Emissive Color** ← Output_Color

> **Astuce density** : si trop dense visuellement (50+ particules visibles), monte `ParticleThreshold` à 0.90. Si trop rare, descends à 0.80.

> **Coût** : 2 TextureSample + quelques math = ~10 instructions PP. Négligeable.

#### M_PP_UnderwaterDistortion spec

##### Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `M_PP_UnderwaterDistortion` |
| **Location** | `/Game/Sub3D/Materials/PostProcess/M_PP_UnderwaterDistortion` |
| **Material Domain** | **Post Process** |
| **Blendable Location** | Before Tonemapping |
| **Blendable Priority** | 1 (après Particulate) |
| **Description** | Refraction subtile (sin Time × scene UV) — sensation "voir à travers l'eau". |

##### Material Parameters

| Param | Type | Default | Range | Rôle |
|---|---|---|---|---|
| `T_NoiseTexture` | Texture2D | T_VolNoise_RG | — | Noise pour pattern de distortion |
| `ScrollSpeed` | Scalar | 0.05 | 0–0.2 | Vitesse animation noise |
| `DistortionAmount` | Scalar | 0.002 | 0–0.01 | Amplitude offset UV (très petit) |
| `NoiseScale` | Scalar | 2.0 | 0.5–8 | Tiling du noise |

##### Étapes de création

###### Étape 1 — UV noise animé

- `ScreenAlignedUVs`
- `Multiply` `UV_Scaled` : A ← ScreenAlignedUVs, B ← NoiseScale (param)
- `Time`
- `Multiply` `Time_Scroll` : A ← Time, B ← ScrollSpeed (param)
- `Constant2Vector` `Scroll_Direction` : value = (0.7, 1.0) — diagonal pour variation
- `Multiply` `Offset` : A ← Time_Scroll, B ← Scroll_Direction
- `Add` `UV_Animated` : A ← UV_Scaled, B ← Offset
- `TextureSampleParameter2D` `Noise_Sample` : Texture ← T_NoiseTexture, UVs ← UV_Animated

###### Étape 2 — Conversion noise [0,1] → offset signé [-1,1]

- `Multiply` `Noise_Half` : A ← Noise_Sample.RG, B ← Constant 2.0 (RG car 2D offset)
- `Subtract` `Noise_Signed` : A ← Noise_Half, B ← Constant 1.0 (range [-1, 1])
- `Multiply` `UV_Offset` : A ← Noise_Signed, B ← DistortionAmount (param)

###### Étape 3 — Échantillonner la scène à UV décalées

- `ScreenAlignedUVs` (nouveau, pour l'échantillonnage scène)
- `Add` `Scene_UV_Distorted` : A ← ScreenAlignedUVs, B ← UV_Offset
- `SceneTexture:PostProcessInput0` `Scene_Distorted` : UVs ← Scene_UV_Distorted, pin "Color"
- **Emissive Color** ← Scene_Distorted

> **Tuning** : si refraction trop visible (vertigo), réduit `DistortionAmount` à 0.001. Si invisible, monte à 0.005.

> **Coût** : 1 TextureSample noise + 1 SceneTexture refetch = ~15 instructions PP. Acceptable.

---

## 6. Fog — ExponentialHeightFog config

> ⚠️ **2026-05-18 — Valeurs alignées sur [canonical spec](2026-05-17_lighting_postprocess_fog_spec.md)**.
>
> 🔁 **State-driven runtime (2026-05-18 controller lock)** — Le `UBiomeFogController` standalone évoqué en §6.4 **n'existe plus comme classe séparée**. La logique fog (density, inscatter, vol albedo, vol extinction) est **fusionnée dans [`UAtmosphereStateController`](2026-05-18_postprocess_state_controller_spec.md)** (décision 3 controller spec). Un seul tick écrit PP + fog en cohérence — pas de race condition entre 2 writers. La table de variation par biome (§6.4) reste valide comme **cible de DA_Fog_Biome_X**, lerpée par le controller sur 40 m smoothstep.

### 6.1 Setup principal (Bathyal default)

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Fog Density** | **0.10** | Bathyal default ; varie par biome via `UBiomeFogController` (§6.4) |
| **Fog Height Falloff** | **0.0** | **Critique** : fog uniforme par distance, pas par hauteur |
| **Fog Inscattering Color** (linear RGB) | **(0.003, 0.015, 0.035)** | Bathyal ; varie par biome |
| **Fog Max Opacity** | 0.92 | Slight cap |
| **Start Distance** | 0 | commence dès la caméra |
| **Fog Cutoff Distance** | 0 | désactivé |
| **Second Fog Data** | désactivé | pas besoin |

### 6.2 Directional Inscattering

| Setting | Valeur |
|---|---|
| **Directional Inscattering** | **désactivé** (toggle off) |

Pourquoi : on n'a pas de DirLight (décision 7 canonical). Si on l'active sans DirLight, le moteur projette quand même un gradient horizon qui n'a aucun sens en abysse.

### 6.3 Volumetric Fog (**activé, calibré 1660 Super**)

| Setting | Valeur | Note |
|---|---|---|
| **Volumetric Fog** | true | active le mode volumetric |
| **Scattering Distribution** | **0.2** | Henyey-Greenstein anisotropy modéré |
| **Albedo** (linear RGB, Bathyal) | **(0.02, 0.06, 0.10)** | très sombre cohérent abyssal |
| **Emissive** | (0, 0, 0) | pas d'émission |
| **Extinction Scale** | **1.5** | densité volumetric Bathyal |
| **View Distance** | **8000** | cm (80m) — au-delà tombe en height fog |
| **Static Lighting Scattering Intensity** | 1.0 | |

**CVars à set dans `DefaultEngine.ini`** :

```ini
r.VolumetricFog.GridPixelSize=8
r.VolumetricFog.GridSizeZ=128
r.VolumetricFog.HistoryWeight=0.9
r.VolumetricFog.DepthDistributionScale=32
r.LocalFogVolume.ApplyOnTranslucent=1
```

**Coût perf** : ~1.6-2.4 ms sur GTX 1660 Super. Budget validé dans §6.2 canonical.

### 6.4 Variations par biome — fusionné dans `UAtmosphereStateController`

> **Implémentation 2026-05-18 (controller lock)** : la classe standalone `UBiomeFogController` n'existe plus. La logique fog par biome est intégrée dans [`UAtmosphereStateController`](2026-05-18_postprocess_state_controller_spec.md) §5 (Q5 Option C fusion) + §7.2 header skeleton.
>
> Le controller lerp les params `AExponentialHeightFog` cached (cf controller spec §5.2) selon `OwnerCharacter->GetActorLocation().Z`. Transition 40 m smoothstep (décision 6 controller). La table ci-dessous reste la cible de chaque `DA_Fog_Biome_X` (data asset composé séquentiellement via `bOverride_X`).

| Biome | Z range (cm) | Fog Density | Inscattering (linear RGB) | Vol. Albedo (linear) | Vol. Extinction Scale |
|---|---|---|---|---|---|
| **Coastal** | 0 → -12000 | 0.04 | (0.012, 0.040, 0.080) | (0.06, 0.12, 0.18) | 1.0 |
| **Pelagic** | -12000 → -30000 | 0.06 | (0.006, 0.025, 0.055) | (0.04, 0.10, 0.16) | 1.2 |
| **Bathyal** | -30000 → -150000 | **0.10** | **(0.003, 0.015, 0.035)** | **(0.02, 0.06, 0.10)** | **1.5** ← default |
| **Abyssal** | -150000 → -400000 | 0.15 | (0.0015, 0.006, 0.018) | (0.015, 0.04, 0.06) | 1.8 |
| **Trenches** | < -400000 | 0.22 | (0.0006, 0.0025, 0.010) | (0.008, 0.020, 0.035) | 2.0 |

**LocalFogVolume** : utilisé en complément pour hero spots locaux (faille biolum, anomalies). Pas pour la variation globale par biome. Voir canonical §1.6. Note : les `ALocalFogVolume` ne sont **pas** pilotés par le controller — ils restent des actors statiques placés en niveau.

**Pourquoi C++ et pas BP** : BP tick flotte avec le frame rate, le lerp paraît saccadé. C++ + `FInterpTo` reste frame-rate independent. Implem détaillée dans [`UAtmosphereStateController` spec](2026-05-18_postprocess_state_controller_spec.md) §7.3 (Tick implementation skeleton, TG_PostPhysics).

---

## 7. Lighting setup

> ⚠️ **2026-05-18 — Valeurs alignées sur [canonical spec](2026-05-17_lighting_postprocess_fog_spec.md) §3.2**. Light intensities significativement augmentées (sub headlight 35 000 cd vs 30 000 avant) car cohérentes avec exposure auto bornée §5.2.
>
> 🔁 **State-driven (2026-05-18 controller lock)** — **Les lights restent des actors statiques**, le controller **ne les touche pas**. Décision 6 controller spec : `Headlight/Flare = aucun PP override`. Les sources diégétiques (sub headlight, EVA flashlight, biolum point lights) sont placées dans la map ou spawnées par leur owner BP, leurs UPROPERTYs (intensity / radius / cone / color / vol scattering) restent éditeur-fixed. L'overlay PP qui réagit à "sub light on/off" passe par le DA Medium (cf [controller spec](2026-05-18_postprocess_state_controller_spec.md) §4.5 composition canonique), pas par une mutation des `ULightComponent`.

### 7.1 Pas de DirLight, pas de SkyAtmosphere — confirmé (décision 7 canonical)

Tout l'éclairage est artificiel et diégétique. Memory `project_abyssal_setting_2026_05_07`.

### 7.2 SkyLight — Specified Cubemap OBLIGATOIRE (jamais null)

> **Décision 8 canonical** : `Cubemap = null` est un BUG (force capture de sky inexistant → gris-sale). Doit pointer vers un cubemap explicite.

**Asset à créer** : `T_Skybox_Abyssal_Dark` — cubemap 6-face, teal sombre uniforme, ~30 min Photoshop ou CubemapGen. Couleur cohérente avec `TKN_FOG_INSCATTER_ABYSSAL`.

| Setting | Valeur |
|---|---|
| **Mobility** | Stationary (ou Movable) |
| **Source Type** | Specified Cubemap |
| **Cubemap** | **T_Skybox_Abyssal_Dark** (à créer) |
| **Intensity Scale** | **0.08** |
| **Sky Distance Threshold** | 150000 |
| **Volumetric Scattering Intensity** | 0.0 |
| **Lower Hemisphere Is Black** | false |

→ Donne juste assez d'ambient pour silhouettes, cohérent avec teal abyssal.

### 7.3 Sources de lumière diégétiques — valeurs cohérentes (canonical §3.2)

> Toutes ces valeurs supposent exposure auto §5.2 et `bApplyPhysicalCameraExposure=false`.

| Source | Type | Unit | Intensity | Atten. Radius | Cone (spot) | Color | Vol. Scattering | Cast Shadows |
|---|---|---|---|---|---|---|---|---|
| **Sub headlight (hero)** | SpotLight | **Candelas** | **35 000** | 15000 cm | 12° / 28° | TKN_SUB_HEADLIGHT_AMBER | 1.2 | ✅ Dynamic |
| **Sub headlight (filler)** | SpotLight | Candelas | 8 000 | 8000 cm | 25° / 50° | TKN_SUB_HEADLIGHT_AMBER | 0.6 | ❌ |
| **EVA flashlight** | SpotLight | Candelas | 12 000 | 6000 cm | 10° / 26° | `#FFE0B0` (1.0, 0.78, 0.45) linear | 1.0 | ✅ Dynamic |
| **Sub interior tungsten (ceiling)** | PointLight | **Lumens** | 400 | 600 cm | — | TKN_SUB_HEADLIGHT_AMBER | 0.3 | ❌ Static/Stat |
| **Sub interior tungsten (panel)** | PointLight | Lumens | 150 | 250 cm | — | TKN_SUB_HEADLIGHT_AMBER | 0.0 | ❌ |
| **Biolum cristal hero (cluster)** | PointLight | Lumens | 120 | 800 cm | — | TKN_BIOLUM_TEAL_MAIN | 0.8 | ❌ |
| **Biolum filler (paroi)** | PointLight | Lumens | 35 | 350 cm | — | TKN_BIOLUM_TEAL_MAIN | 0.4 | ❌ |
| **Biolum micro (algue/spore)** | PointLight | Lumens | 8 | 120 cm | — | `#5EE0C0` (0.10, 0.75, 0.55) linear | 0.2 | ❌ |
| **Outpost extérieur (spot)** | SpotLight | Candelas | 8 000 | 5000 cm | 18° / 42° | `#FFC080` (1.0, 0.50, 0.22) linear | 0.8 | ✅ |
| **Flare lancée** | PointLight | Lumens | 2 000 | 1500 cm | — | `#FF6030` (1.0, 0.10, 0.02) linear | 1.2 | ✅ |
| **Cristal émissif (material seul)** | Emissive | — | 25-60 | — | — | — | — | — |

### 7.3-bis Règles canoniques lights

1. **Cast Shadows = false** sur 80%+ des lights (biolum, ambient, micro). Hero shadows max **4-6 lights simultanées**.
2. **Volumetric Scattering Intensity > 0** uniquement sur lights qui doivent générer light shafts visibles (sub headlight, biolum hero, flare).
3. **Color en linear RGB strict** — pas sRGB. Utilise les tokens §7-bis.4-bis.
4. **Unit cohérent au type** : Candelas pour Spot (intensité directionnelle), Lumens pour Point (flux total).
5. **Pas de Attenuation Radius géant** — 2-3× la distance utile max. Au-delà = waste GPU.
6. **Pas de Intensity 1M+** — c'était mon erreur initiale due à `bApplyPhysicalCameraExposure=true`. Fix exposure d'abord.

### 7.3-ter Total lights actives simultanées

**Budget GTX 1660 Super** (canonical §6.2) :
- 5-8 lights shadowed maximum
- 30-40 lights non-shadowed
- 8-10 lights avec Volumetric Scattering > 0 simultanément visibles

Au-delà : drop fps significatif. MegaLights (UE5.5+) peut aider mais à benchmarker.

### 7.4 Light shafts — sources locales (pas surface)

**Décision 2026-05-17** : la cave a des light shafts visuels, mais ils viennent **exclusivement de sources locales**, jamais de la surface (la surface n'existe pas dans le worldspace jouable).

Sources qui génèrent des shafts via Volumetric Fog :

| Source | Mécanisme | Setup |
|---|---|---|
| **Faille avec biolum dense** | Cluster de PointLights biolum massifs dans un goulot étroit | `BP_BiolumChasm` — PointLight intensity 50000, attenuation 2000cm, posé dans une fissure étroite. Le volumetric fog rend le shaft naturellement. |
| **Sub headlight in fog** | SpotLight cone du sub vu de côté | SpotLight `IES Profile` propre, intensity 30000, Cast Volumetric Shadows = true |
| **EVA flashlight** | SpotLight cone EVA vu de côté | Pareil que sub headlight mais intensity 20000 |
| **Outpost vu de loin** | PointLights warm 3500K filtrant par sas ouvert | Light shaft via la géométrie du sas |
| **Cristaux émissifs massifs** | Mesh émissif HDR (Emissive > 5) + bloom + volumetric | `BP_BiolumCluster` Large variant |

**Implication critique** : `Volumetric Fog` passe de "optionnel §6.3" à **NÉCESSAIRE**. Sans lui, les shafts ne sont pas visibles → le visuel des concepts n'est pas atteint.

→ Voir §6.3 maintenant marquée recommandée.

---

## 7-bis. Cohérence palette diégétique sub vs cave

**Référence canonique** : `C:/ACC/Projects/Sub3D/Image & Concept/subcolors.png` (palette officielle).

### 7-bis.1 La mécanique visuelle clé

Le DA Sub3D repose sur **deux palettes contrastées** :

| Zone | Palette | Émotion |
|---|---|---|
| **Intérieur submarine** | Tungstène chaud — ambre #FF9D60, cuivre/laiton #A57212, rouge minium #80363F, blanc néon chaud #FCEFCF | Refuge, habité, contrôlé, humanité |
| **Cave abyssale extérieure** | Cool violet-bleu shadows + teal biolum + beige-warm où la lampe sub frappe | Hostile, inconnu, vide, profondeur |
| **Transition au sas** | Contraste fort warm↔cool, brève superposition | Lisibilité narrative : tu sors du refuge |

**Cette dualité EST le langage visuel du jeu.** Ne pas la diluer. Le moment où tu passes du sas vers la cave doit donner un frisson palpable de température chromatique.

### 7-bis.2 Couleurs canoniques (subcolors.png)

#### Intérieur sub — matières

| Nom | Hex | Usage |
|---|---|---|
| Acier brossé | `#4A4E52` | Parois principales |
| Acier oxydé | `~#B05B43` | Rouille, usure, points chauds |
| Vert hôpital | `~#7AA386` | Zones médicales, vert pâle d'hygiène |
| Jaune sécurité | `~#C8A640` | Marquages, hazard stripes |
| Rouge minium | `#80363F` | Stencils pipes, marquages militaires |
| Caoutchouc | `~#1A1A1A` | Joints, gaines, sols |
| Cuivre/laiton | `#A57212` | Pipework, boutons bakelite, instruments |

#### Intérieur sub — émissifs

| Nom | Hex | Usage |
|---|---|---|
| Blanc néon chaud | `#FCEFCF` | Plafonniers tungstène 2700K |
| Ambre tungstène | `#FF9D60` | Lampes individuelles, ambiances |
| LED rouge alarme | `~#C03A2A` | Indicateurs danger |
| CRT phosphor vert | `~#5CD89A` | Écrans instruments |

#### Cave abyssale — palette LUT v2

Voir [`abyssal_palette_lut.html`](../tools/abyssal_palette_lut.html). 9 stops violet→beige + accents :
- Biolum teal `#2DC4B0` (recalibré v2)
- Crystal biolum bright `#3DFFC4`
- Sub light ambre `#FFB46A` (diégétique, source warm dans la cave — vient de la lampe sub, PAS du material)

### 7-bis.3 Règles d'usage

- **Aucune couleur warm de la palette sub** (ambre, cuivre, rouge minium) **ne doit apparaître sur les surfaces géologiques de la cave**, sauf en réponse à un éclairage diégétique (lampe sub qui frappe la roche → la roche reflète warm momentanément). C'est géré naturellement par PBR lighting, pas par le material.
- **Aucune couleur cool de la cave** (violet shadows, teal biolum) **ne doit apparaître sur les surfaces intérieures du sub**, sauf via un hublot qui montre l'extérieur ou un éclairage diégétique externe (flare lancée près d'un hublot).
- **Le contraste warm/cool est volontairement amplifié**. Pas de tons neutres "qui pourraient marcher des 2 côtés" — choisir un camp pour chaque asset.
- **Les transitions (sas, hublots, éclairages traversants)** sont des moments visuels privilégiés. Les souligner avec des matériaux qui jouent volontairement les 2 palettes côte à côte (ex : sas avec joint warm visible depuis la cave cool).

### 7-bis.4 Cross-references

- `cave concept submarine stylized handpaint.png` — violet rock + teal biolum (cave pure)
- `cave concept reparation submarine.png` — sub posé dans la cave, **on voit la dualité** : warm ambre du sub vs cool des parois
- `flare illumination cave concept.png` — EVA avec flare blanc, sub avec lampe ambre, biolum teal — toutes les sources visibles ensemble
- `In_a_digital_painting_style_a_dark_cavernous_7eOFk_Z8.png` — biolum + light shafts depuis sources locales

### 7-bis.4-bis Sub3D Color Tokens — source of truth canonique

> Décision 2026-05-17 (suite friction utilisateur "RockTint / SedimentTint sont défault, ils devraient venir d'une palette DA") :
> définir un set fixe de **Color Tokens** Sub3D que tous les material params, lights, PP settings doivent référencer.
> Le LUT est UNE expression de ces tokens (les 9 stops). Mais les RockTint, SedimentTint, BiolumColor, StrataBandColor, etc. doivent venir des MÊMES tokens, pas de valeurs improvisées.

> 🔧 **Correction 2026-05-18** : les colonnes "RGB linéaire" étaient en réalité des valeurs sRGB normalisées. UE5 LinearColor attend du **vrai linéaire**. Tous les tokens recalculés avec formule sRGB→linear standard. Si tu avais déjà set des params avec les anciennes valeurs, **re-tagge avec les nouvelles**.

#### Tokens — cave / abyssal (palette cool)

| Token | Hex sRGB | **RGB linéaire (vrai)** | Usage canonique |
|---|---|---|---|
| `TKN_ABYSSAL_SHADOW_DEEPEST` | `#07060C` | **(0.0021, 0.0018, 0.0039)** | LUT stop L=0, deepest shadow |
| `TKN_ABYSSAL_SHADOW_VIOLET` | `#1F1C2D` | **(0.0144, 0.0119, 0.0262)** | LUT stop L=64, dark slate |
| `TKN_ABYSSAL_MID_VIOLET` | `#2D2D3D` | **(0.0262, 0.0262, 0.0467)** | LUT stop L=96 |
| `TKN_ABYSSAL_MID_NEUTRAL` | `#3A3941` | **(0.0422, 0.0408, 0.0545)** | LUT stop L=128 |
| `TKN_ROCK_TINT_DEFAULT` | `#808C99` | **(0.2159, 0.2623, 0.3186)** | Default `RockTint` master M_CaveAbyssal |
| `TKN_SEDIMENT_TINT_DEFAULT` | `#666156` | **(0.1329, 0.1195, 0.0931)** | Default `SedimentTint` master |
| `TKN_STRATA_BAND_INDIGO` | `#1A1F35` | **(0.0102, 0.0144, 0.0350)** | Default `StrataBandColor` (indigo abyssal coherent) |
| `TKN_STRATA_BAND_RUST` | `#6E3F24` | **(0.1560, 0.0513, 0.0181)** | Variante warm rust (pour biomes Coastal) |

#### Tokens — biolum / hero émissifs

| Token | Hex sRGB | **RGB linéaire (vrai)** | Usage |
|---|---|---|---|
| `TKN_BIOLUM_TEAL_MAIN` | `#2DC4B0` | **(0.0262, 0.5457, 0.4442)** | Default `BiolumColor` master |
| `TKN_BIOLUM_BRIGHT_CRYSTAL` | `#3DFFC4` | **(0.0467, 1.0000, 0.5457)** | Accent cristaux émissifs (HDR ×4-8) |
| `TKN_BIOLUM_VIOLET_ACCENT` | `#7D4FCC` | **(0.2053, 0.0790, 0.6038)** | Variante rare violet (cf concept In_a_digital_painting) |

#### Tokens — light diégétique (warm, à ne pas mélanger avec material BC abyssal)

| Token | Hex sRGB | **RGB linéaire (vrai)** | Usage canonique |
|---|---|---|---|
| `TKN_SUB_HEADLIGHT_AMBER` | `#FFB46A` | **(1.0000, 0.4564, 0.1447)** | Couleur SpotLight sub headlight + ViewportHeadlamp |
| `TKN_TUNGSTEN_AMBRE` | `#FF9D60` | **(1.0000, 0.3374, 0.1170)** | Lampes intérieures sub |
| `TKN_LED_RED_ALARM` | `#C03A2A` | **(0.5249, 0.0422, 0.0232)** | Indicateurs danger sub |
| `TKN_CRT_PHOSPHOR_GREEN` | `#5CD89A` | **(0.1070, 0.6867, 0.3231)** | Écrans instruments CRT |

#### Tokens — fog / ambient (très sombre)

| Token | Hex sRGB | **RGB linéaire (vrai)** | Usage |
|---|---|---|---|
| `TKN_FOG_INSCATTER_ABYSSAL` | `#01050A` | **(0.0003, 0.0015, 0.0030)** | Fog Inscattering Color |
| `TKN_FOG_VOLUMETRIC_ALBEDO` | `#050D14` | **(0.0015, 0.0043, 0.0068)** | Volumetric Fog Albedo |
| `TKN_SKYLIGHT_AMBIENT` | `#000103` | **(0.0000, 0.0003, 0.0009)** | SkyLight Lower/Upper Hemisphere Color |

#### Règle d'usage

**Quand tu choisis une valeur Color pour un nouveau material/light/PP** :
1. Cherche dans cette table un token qui correspond au rôle
2. Si trouvé → utilise ce token (set le param avec ce hex)
3. Si pas trouvé → c'est une nouvelle catégorie : **ajoute un token ici** avant de l'utiliser ailleurs
4. **Jamais** d'invent une couleur ad-hoc qui ne référence pas un token

→ Garantit cohérence visuelle Sub3D à travers tous les systèmes (cave, sub, outpost, future creatures, UI…).

#### Liaison avec le LUT generator

Le LUT (`abyssal_palette_lut.html`) consomme un sous-ensemble de ces tokens (les 9 stops pour le gradient). **Mais les autres tokens (biolum, sub light, fog) sont aussi canoniques** — ils ne passent pas dans le LUT mais doivent être utilisés tels quels dans leurs contextes.

Si tu changes un token ici → met à jour les MIs / lights / PP volumes qui l'utilisent. La memory `feedback_collaboration_ui_method` peut aider à tracker les usages.

### 7-bis.5 Validation runtime

Pour vérifier que ta palette in-game est alignée :
1. Screenshot du sas vu depuis l'intérieur sub (warm dominant)
2. Screenshot du même sas vu depuis la cave (cool dominant)
3. Screenshot du sub posé dans une cavité (les deux palettes visibles, contraste fort)
4. Color picker (Photoshop/Krita) sur les screenshots → vérifier que les RGB tombent dans les ranges des palettes ci-dessus

Si tu trouves du beige-warm dans une zone sans éclairage diégétique sub → leak palette à corriger (probablement RockTint trop chaud sur MI_CaveAbyssal).

---

## 8. Order of implementation (priority)

> 📍 **Tracker exécution** — édite la colonne Status au fil de l'avancement.
> Légende : ✅ done · ⏳ in progress · ⏸ pending · ⏭ skipped (revisité plus tard)

| Step | Status | Asset | Lien vers procédure | Why first |
|---|---|---|---|---|
| 1 | ✅ | `MF_Triplanar_PBR` | [§3.4 step A](#a-triplanar-projection-mf_triplanar_pbr) | foundation pour tout le reste |
| 2 | ✅ | `MF_Biolum` (sans pulse d'abord) | [§3.4 step G](#g-biolum-emissive-mf_biolumspots) | gros impact visuel |
| 3 | ✅ | `M_CaveAbyssal` master + `MI_CaveAbyssal_Default` | [§3.6 graph master](#36-m_caveabyssal--graph-du-master-material) + [§3.7 workflow test](#37-workflow-de-test) | assigner sur AGeologicalCaveActor.MeshMaterial → look immédiat (PBR neutre) |
| 3.5 | ⏳ | `MF_SedimentOverlay` + `MF_StrataBanding` (anticipé vs §8 original) | [§3.4 step B](#b-sediment-layer-mf_sedimentoverlay) + [§3.4 step C](#c-strata-banding-mf_stratabanding) | enrichissement avant stylization |
| 4 | ⏳ | `T_AbyssalPalette_LUT` (texture) + **`MF_StylizePBR` refactor v2** (per-step strengths) | [§3.8 procédure LUT](#38-procédure--générer--importer-t_abyssalpalette_lut) + [§14.2 MF_StylizePBR refactored](#142-approche-2--transformer-du-pbr-photo-en-stylized-via-shader) | LUT done, refactor v2 en cours (architecture per-step strengths) |
| **5** | **⏸ NEXT** | **PP + Fog + Lights + Project Settings setup** (canonical 2026-05-18) | [§5 PostProcess](#5-postprocess--pp_underwater_abyssal) + [§6 Fog](#6-fog--exponentialheightfog-config) + [§7 Lighting](#7-lighting-setup) + [canonical lighting spec](2026-05-18_lighting_pp_fog_canonical_spec.md) + [canonical project settings spec](2026-05-18_project_settings_canonical_spec.md) | **16 décisions canoniques locked 2026-05-18** (8 lighting/PP/fog + 8 project settings). Apply project settings d'abord (restart editor required pour certains), puis canonical lighting §8.1-8.5, create T_Skybox_Abyssal_Dark, re-tag lights, re-tag MI Color Tokens linear |
| 6 | ⏸ | `M_CaveRockAsset` + `MI_CaveRock_Default` | [§4 Rocks harmonization](#4-rocks-assets-harmonization) | quand on a 1-2 rock packs sourcés ; partage MF_StylizePBR |
| 7 | ⏸ | `MF_FractureMask` | [§3.4 step D](#d-fracture-mask-mf_fracturemask--optionnel) | détail final |
| 8 | ⏸ | PP materials (particulate, distortion) | [§5.5 PP Materials](#55-pp-materials-post-process-materials) | polish |
| 9 | ⏸ | Variantes MI par strate (Coastal/Pelagic/Abyssal) | — | post-FP |
| 10 | ⏸ | `MF_WetSurface` (optionnel) | [§3.4 step H](#h-wet-surface-mf_wetsurface--optionnel) | enrichissement optionnel humidité |

**Smoke test à chaque étape** : ouvrir map cave proto + screenshot → vérifier que la prochaine étape ajoute sans casser ce qui marche.

### Mini-procedure : passer au step suivant

1. Marquer le step actuel ✅
2. Lire la section linkée (col "Lien vers procédure") du prochain step en ⏸
3. Marquer prochain step ⏳
4. Implémenter → tester → re-marquer ✅ si validé
5. Goto 1

---

## 8-bis. Backlog Phase 2 — enrichissement géométrie post-FP

> Décisions 2026-05-17 utilisateur — pas dans le scope FP, à rappeler pour planification post-FP.

### 8-bis.1 Relief / micro-géométrie cave (look "années 1990-2000 cube terrain")

**Observation utilisateur** : le rendu actuel sur PMC procedural cave fait très cube/terrain old-school. Le normal map seul ne suffit pas à donner du relief perçu sur une géométrie de marching cubes assez lisse.

**Options Phase 2 envisagées** :

1. **Layer PCG complexe sur parois** : génération PCG dense qui pose des micro-rocks/échardes/strates physiques sur les parois pour casser le côté lisse. Coûteux mais artistique max.
2. **Displacement + 2e passe de generation** : tessellation runtime + displacement map dérivée des paramètres géologiques DA. Plus performant, look plus fin et naturel.
3. **Hybride** : displacement de base sur le mesh + PCG pour les "hero pieces" (gros rochers proéminents, formations remarquables).

**À évaluer** quand on aura plus de visuel cible et qu'on aura mesuré les perfs de référence sur GTX 1660 avec le master complet.

### 8-bis.2 Passe nouvelle generator pour strates

**Idée utilisateur** : ajouter une passe dans `AGeologicalCaveActor` qui sculpte la **géométrie** elle-même selon les strates (pas juste le shader qui darken horizontalement, mais le mesh qui a des recess physiques aux frontières de strate).

**Bénéfice** : strates **vraiment** visibles type cliff réel (cf image ref user — falaise avec bandes pierreuses physiques), pas juste tinted via shader.

**Coût** : modifier le marching cubes / sculpting pass dans le generator. Hors scope material spec.

**Action** : backlog cave generator. Lié à 8-bis.1.

### 8-bis.3-bis Biolum lens flare selective system (decoupled from global PP)

**Observation 2026-05-18** : `LocalExposureDetailStrength = 4.0` donnait un pop biolum HDR magnifique mais créait simultanément des halos abherants sur les ULightComponent (cf friction 2026-05-18 observation doc). Le tradeoff vient de l'algo PP UE5 qui est **global** : tout pixel haute-luminance est traité pareil, pas de natif "biolum-only flare".

**3 options évaluées 2026-05-18** :

| Option | Quoi | Coût | Quand |
|---|---|---|---|
| **A — Rebalance BloomThreshold + BiolumIntensity** | Calibrer biolum emissive > BloomThreshold > ULight illuminated surfaces. Bloom natif capture biolum, skip rocks éclairés. | 5 min tuning | ❌ **testée 2026-05-18 — insuffisante** (ne récupère pas le pop HDR halo que DetailStrength 4 donnait). FP accepte biolum subtle en attendant C. |
| **B — Niagara lens flare per biolum patch** | Spawn `N_BiolumFlare` à chaque cluster biolum, billboard sprite additif décorrélé du PP. Couplé à cave generator + PCG biolum placement. | ~1-2 jours, perf monitoring obligatoire (LOD distance) | Post-FP polish |
| **C — Custom Stencil PP material** ⭐ **recommandé long terme** | Master material écrit `CustomStencilValue = 100` sur pixels biolum. `M_PP_BiolumFlare` lit Stencil + applique HDR halo additif sur stencil==100 only. Compatible composition controller DA (active via DA_PP_Medium_Cave ou DA_PP_Biome). | ~4-6h impl + ~0.2 ms GPU | Post-FP polish, intègre avec controller spec décision 7 (PP material deferred) |

**Option D rejetée** : Bloom Convolution custom kernel — coût perf trop élevé (~+1 ms GPU GTX 1660) pour résultat cosmétique seul.

**Action FP** : Option A (rebalance). **Action post-FP** : Option C (Custom Stencil), s'intègre comme material `M_PP_BiolumFlare` plugged dans le DA_PP_Medium_Cave du controller.

### 8-bis.3 Strates multi-texture (vraies couches géologiques)

Réf image 3 utilisateur (cliff multi-color banding). Aujourd'hui MF_StrataBanding fait darken + rust tint, pas multi-texture.

**Extension Phase 2** : `MF_StrataBanding` v2 avec 2-3 BC textures + blend selon la bande active (cf §C plus haut). Limite UE : 16 texture samples par shader → triplanar × 3 textures × 3 strates = 27 samples (dépasse). Solution : 2 strates max ou drop triplanar pour les bandes.

**Action** : décision Phase 2.

---

## 9. Validation criteria

Avant de marquer "Done" :

- [ ] L'œil ne distingue pas immédiatement un rock asset PCG-placed d'un pic de la paroi procédurale
- [ ] Les bandes strates traversent visuellement le mesh procédural ET les boulders
- [ ] Les biolum cyan sont la seule chose lumineuse de la scène (en l'absence de lampe EVA)
- [ ] Le fog s'atténue avec la distance, pas avec la hauteur
- [ ] Pas de chromatic aberration douloureuse (test sur écran 24" à 60cm)
- [ ] FPS > 50 sur GTX 1660 dans une zone moyennement dense
- [ ] Aucun warning "Shading model SubSurface non supporté par PMC" dans le log
- [ ] Screenshot side-by-side avec ta ref visuelle (Subnautica Below Zero crystal cave) montre un look apparenté

---

## 10. Risques connus & contournements

| Risque | Symptôme | Mitigation |
|---|---|---|
| Triplanar tilling visible | bandes parallèles évidentes sur grandes parois | augmenter `TriplanarBlendSharpness`, varier `WorldTilingScale` via noise faible |
| Bandes strates trop régulières | look "barré, pas géologique" | augmenter `StrataNoiseWarp`, casser via noise basse fréquence en multiply sur `StrataDarkenAmount` |
| Biolum pulsing trop régulier | clignotement comme une LED 1Hz | déphaser via `cell_id` (Voronoi.cell_id) — pulses asynchrones |
| Fog trop dense → on voit rien | navigation impossible | réduire density à 0.04, augmenter Sub headlight |
| Volumetric Fog tank le FPS | drop 60 → 30 | désactiver, garder seulement HeightFog |
| Rocks PCG-placed cassent l'illusion | look "stickers" | augmenter TintStrength, vérifier que les Material Functions partagent les WorldPos |
| Roughness trop uniforme | rocaille plastique | augmenter range `RoughnessMin`/`RoughnessMax`, ajouter variation noise sur roughness |

---

## 11. Files à créer

| Path | Type |
|---|---|
| `Content/Sub3D/MF/MF_Triplanar_PBR.uasset` | Material Function |
| `Content/Sub3D/MF/MF_StrataBanding.uasset` | Material Function |
| `Content/Sub3D/MF/MF_BiolumSpots.uasset` | Material Function |
| `Content/Sub3D/MF/MF_SedimentOverlay.uasset` | Material Function |
| `Content/Sub3D/MF/MF_FractureMask.uasset` | Material Function |
| `Content/Sub3D/MF/MF_WetSurface.uasset` | Material Function |
| `Content/Sub3D/MF/MF_StylizePBR.uasset` | Material Function (stylized DA — §14) |
| `Content/Sub3D/Materials/Cave/M_CaveAbyssal.uasset` | Material Master |
| `Content/Sub3D/Materials/Cave/MI_CaveAbyssal_Default.uasset` | Material Instance |
| `Content/Sub3D/Materials/Cave/M_CaveRockAsset.uasset` | Material Master |
| `Content/Sub3D/Materials/Cave/MI_CaveRock_Default.uasset` | Material Instance |
| `Content/Sub3D/Materials/Cave/M_CaveBiolumCluster.uasset` | Material Master |
| `Content/Sub3D/Materials/Cave/Palettes/T_AbyssalPalette_LUT.png` | Texture LUT 256×16 (généré via [tool](../tools/abyssal_palette_lut.html)) |
| `Content/Sub3D/Materials/Cave/Brushes/T_BrushStrokes_Grunge.png` | Texture overlay hand-paint (Photoshop/Substance, seamless 1024²) |
| `Content/Sub3D/Materials/PostProcess/M_PP_UnderwaterParticulate.uasset` | PP Material |
| `Content/Sub3D/Materials/PostProcess/M_PP_UnderwaterDistortion.uasset` | PP Material |
| `Content/Sub3D/Atmosphere/PP_Underwater_Abyssal_Preset.uasset` | Data asset (params) |

---

## 12. Liens utiles

- **Triplanar mapping in UE5** : [Ben Cloward triplanar tutorial](https://www.youtube.com/results?search_query=ben+cloward+triplanar+ue5)
- **Reoriented Normal Mapping** : [Self Shadow blog](https://blog.selfshadow.com/publications/blending-in-detail/)
- **Voronoi noise nodes UE5** : doc officielle "Material Expression Voronoi"
- **PostProcess Materials** : doc officielle "Custom Post Process Materials"

---

## 13. À ne PAS faire

- ❌ Activer **SkyAtmosphere** "pour voir" — c'est calibré pour la surface, ça met du jaune solaire partout
- ❌ Mettre une **Directional Light** "juste pour les ombres" — pas de soleil en abysse, et les ombres directionnelles trahissent immédiatement
- ❌ Utiliser des textures **photo-réalistes desert/canyon** (couleurs chaudes orange/rouge) — incohérent avec abyssal
- ❌ Activer **Lumen Surface Cache** sur le PMC tant que le mesh n'est pas stable — recalcul à chaque génération = freeze
- ❌ Mettre **bUseDebugVertexColorMaterial = true** en production — c'est pour debug brush role, pas pour le look final
- ❌ Mettre des **textures de coraux tropicaux colorés** ; les abysses sont sobres

---

## 14. Stylized / hand-painted DA — Track B (annexe)

> 🎨 **Track B extrait en annexe** (split 2026-05-18) :
> [cave_abyssal_stylization.md](cave_abyssal_stylization.md)

Contenu de l'annexe :
- §14.0 LUT generator tool (5 presets palettes)
- §14.1 Sources textures hand-painted / stylized PBR (3 tiers)
- §14.2 **MF_StylizePBR refactor v2** (per-step strengths : Posterize, LUT, NormalFlat, Edge, RoughnessMatte, Brush)
- §14.3 Intégration au master M_CaveAbyssal
- §14.4 Recommandation pour Sub3D solo dev
- §14.5 Pièges à éviter
- §14.6 Calibration LUTStrength par référence (Subnautica BZ, Sea of Thieves, etc.)

