# Sub3D — Lighting, PostProcess & Fog Spec

| | |
|---|---|
| **Date** | 2026-05-17 |
| **Statut** | ⚠️ **SUPERSEDED par [2026-05-18_lighting_pp_fog_canonical_spec.md](2026-05-18_lighting_pp_fog_canonical_spec.md)** — conservé comme référence historique uniquement |
| **Scope** | Exposure system · Lights (units, intensity, perf) · Fog (vertical descent) · PostProcess settings |
| **Doc parallèle** | [cave_abyssal_material_spec](2026-05-17_cave_abyssal_material_spec.md) (matériaux) · [observation doc](2026-05-17_cave_abyssal_observation_doc.md) (frictions) |

---

## Sommaire

| # | Section | Lecture |
|---|---|---|
| 1 | [Problème observé — diagnostic](#1-problème-observé--diagnostic) | 1 min |
| 2 | [Exposure system UE5 — comprendre EV100](#2-exposure-system-ue5--comprendre-ev100) | 3 min |
| 3 | [Lights — units, intensité, attenuation](#3-lights--units-intensité-attenuation) | 5 min |
| 4 | [Fog — vertical descent problem & solutions](#4-fog--vertical-descent-problem--solutions) | 4 min |
| 5 | [PostProcess settings recommandés Sub3D](#5-postprocess-settings-recommandés-sub3d) | 3 min |
| 6 | [Workflow setup complet — starter cohérent](#6-workflow-setup-complet--starter-cohérent) | 2 min |
| 7 | [Optimisation perfs lights](#7-optimisation-perfs-lights) | 3 min |
| 8 | [Common pitfalls & fixes](#8-common-pitfalls--fixes) | 2 min |
| 9 | [Prompt ChatGPT recherche étendue](#9-prompt-chatgpt-recherche-étendue) | bonus |
| 10 | [PostProcessVolume — reference complète](#10-postprocessvolume--reference-complète) | 10 min |
| 11 | [Underwater UE5 patterns — research étendue](#11-underwater-ue5-patterns--research-étendue) | 4 min |
| 12 | [Diagnostic "drappé blanc" — checklist](#12-diagnostic-drappé-blanc--checklist) | 3 min |

---

## 1. Problème observé — diagnostic

**Symptômes utilisateur** :
- Lights doivent être à **1000000 lumens** pour être visibles
- Radius **5km** pour couvrir une scène cave
- PIE lag avec lights movable dans le sub
- Manual exposure : Compensation 4.48, Min/Max EV100 = 3, Chromatic 0.5

**Diagnostic** : système d'exposure mal calibré. UE5 utilise un système physique (Physical Light Units + EV100). Si tu cumules :
- Exposure manuelle trop **haute** (scène vue comme "jour"), pour compenser tu cranke les lumens
- Intensity à 1M lumens (≈ stadium light array) = irréaliste, mais ça marche **parce que** l'expo absorbe
- Résultat : tu te bats contre le moteur au lieu de l'utiliser

**Fix racine** : baisser l'exposure pour que la scène soit "physiquement sombre" (EV100 0-3, comme une caverne réelle), puis lights réalistes (50-3000 lumens) deviennent visibles avec naturalité.

---

## 2. Exposure system UE5 — comprendre EV100

### 2.1 Qu'est-ce que EV100

EV (Exposure Value) à ISO 100 — unité **photographique** standardisée. Chaque +1 EV100 = **2× plus de lumière** captée.

**Référence physique réelle** (à connaître pour calibrer Sub3D) :

| EV100 | Lumière équivalente | Contexte réel |
|---|---|---|
| -6 | nuit étoilée pure | hors propos |
| -4 | clair de lune | abysse profond cohérent |
| -2 | bougie unique | abysse mid avec biolum |
| 0 | bougie + ambient nuit | intérieur très sombre |
| 3 | crépuscule | intérieur dim |
| 5-6 | intérieur normal | bureau / chambre |
| 8-10 | jour overcast | dehors nuageux |
| 12-13 | jour clair ombragé | dehors ombre |
| 15-16 | soleil direct | dehors plein soleil |

### 2.2 Tes valeurs actuelles décodées

Tu as :
- **Min EV100 = 3, Max EV100 = 3** → expo locked à EV100 = 3 (crépuscule)
- **Exposure Compensation = 4.48** → on ajoute encore **+4.48 EV** au-dessus = effective EV100 = **7.48** (intérieur normal/lumineux)

**Conséquence** : la scène est rendue comme un intérieur normal. Pour qu'une cave abyssale **paraisse** abyssale, tu dois compenser par des lights ULTRA puissantes. C'est ça le 1M lumens.

### 2.3 Recommandation Sub3D abyssal

Pour une cave réellement abyssale **(option A — réaliste)** :

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Metering Mode** | Manual | Pas d'auto adaptation, immersion stable |
| **Exposure Compensation** | 0.0 | Pas de boost artificiel |
| **Min EV100** | 0 | = Max → locked |
| **Max EV100** | 0 | Scène "très sombre", ouvre la latitude pour les biolum HDR |

→ Avec ce setup, **lights réalistes** marchent :
- Sub headlight 5000 lumens éclaire fortement (équivalent flashlight tactical)
- Biolum cluster 200 lumens donne un glow doux
- PointLight ambient 10 lumens donne un soupçon de présence

Pour cave abyssale **(option B — pulse pour gameplay)**, si tu veux que le joueur voie un peu sans lampe :

| Setting | Valeur |
|---|---|
| **Min EV100** | -2 |
| **Max EV100** | 2 |
| (Auto adaptation entre -2 et 2 selon zone) |

> **Auto-exposure activée mais bornée** : ouvre la pupille du joueur dans les zones très sombres, ferme dans les zones biolum. Plus immersif mais moins prédictible visuellement.

### 2.4 Local Exposure (UE5 avancé)

Pour cave avec contraste extrême (biolum HDR très brillant à côté de zones quasi-noires), active **Local Exposure** dans le PP volume :

| Setting | Valeur |
|---|---|
| **Highlight Contrast Scale** | 0.7 | Adoucit les biolum trop éclatants |
| **Shadow Contrast Scale** | 1.3 | Renforce le contraste dans shadows |
| **Detail Strength** | 1.0 | Définition micro-contraste |

→ Donne un look "HDR cinema" : tu vois les détails sombres ET les biolum sans cramer.

---

## 3. Lights — units, intensité, attenuation

### 3.1 Intensity Units (très important)

Dans le **Details panel** de chaque light, dropdown `Intensity Units`. **Choix matters** :

| Type | Pour | Range typique |
|---|---|---|
| **Lumens** | PointLight, total omnidirectionnel | 50 (bougie) → 100000 (puissance industrielle) |
| **Candelas** | SpotLight focalisé | 50 → 20000 cd (flashlights, projecteurs) |
| **Lux** | DirectionalLight (sun) | 100 (overcast) → 120000 (soleil direct) |
| **Unitless (legacy)** | Évite, valeurs arbitraires | — |

**Recommandation Sub3D** :
- SpotLight (sub headlight, EVA flashlight) → **Candelas** (focalisé)
- PointLight (biolum, ambient) → **Lumens**
- DirLight → N/A (pas de soleil en abysse)

### 3.2 Valeurs réelles cibles Sub3D

**Avec exposure EV100 = 0** (recommandé) :

| Source | Type | Intensity | Unit | Cone (spot) | Range |
|---|---|---|---|---|---|
| Bougie / flame minuscule | PointLight | 12 | Lumens | — | 200 cm |
| Lampe d'urgence sub | PointLight | 150 | Lumens | — | 800 cm |
| Plafonnier sub corridor | PointLight | 800 | Lumens | — | 1200 cm |
| Sub headlight (faible) | SpotLight | 1500 | Candelas | 25° / 35° | 4000 cm |
| Sub headlight (fort) | SpotLight | 5000 | Candelas | 20° / 30° | 8000 cm |
| EVA flashlight | SpotLight | 2500 | Candelas | 18° / 30° | 5000 cm |
| Biolum cluster (small) | PointLight | 30 | Lumens | — | 400 cm |
| Biolum cluster (medium) | PointLight | 150 | Lumens | — | 800 cm |
| Biolum chasm massif | PointLight | 800 | Lumens | — | 2000 cm |
| Outpost lampe extérieure | SpotLight | 3000 | Candelas | 30° / 45° | 6000 cm |
| Flare lancée | PointLight | 2500 | Lumens | — | 3000 cm |

→ **Tu n'auras JAMAIS besoin de 1M lumens** avec ce setup.

### 3.3 Attenuation Radius — ce que ça fait vraiment

**Mythe** : "j'augmente le radius pour que la light éclaire plus loin"
**Réalité** : `AttenuationRadius` = distance max où la light est **calculée**. La brightness à distance dépend de :

1. **Intensity** (valeur)
2. **Inverse Square Falloff** (par défaut ON, physique réel) : brightness ÷ 4 quand on double la distance
3. **Source Radius / Soft Source Radius** : taille apparente de la source

Si une light n'éclaire pas assez à 10m, augmenter le radius à 100m ne change rien — tu dois augmenter l'**Intensity**.

**Recommandation** : radius = **2-3× la distance utile maximale**. Au-delà, c'est juste un cost perf pour zero impact visuel.

| Source | Distance utile | Radius recommandé |
|---|---|---|
| Plafonnier sub corridor | 5m | 1200 cm |
| Sub headlight | 30m | 8000 cm |
| Biolum cluster | 5m | 800 cm |
| Outpost lampe extérieure | 25m | 6000 cm |

### 3.4 Source Radius — pourquoi c'est important

**Source Radius** (en cm) = taille de l'émetteur. Petit (1 cm) = source ponctuelle dure, grandes shadows. Grand (50 cm) = source douce, soft shadows.

| Source | Source Radius | Note |
|---|---|---|
| LED tactical flashlight | 1-2 cm | Cone net |
| Ampoule sub | 5-10 cm | Diffuse normal |
| Biolum cluster | 20-50 cm | Soft glow, pas de hard shadow |
| Flare diffuse | 30 cm | |

Cast Volumetric Shadows = TRUE pour interaction avec Volumetric Fog (shafts visibles).

---

## 4. Fog — vertical descent problem & solutions

### 4.1 Le problème ExponentialHeightFog (HF)

Par design, HF a une formule : `density(z) = base × exp(-falloff × (z - origin))`.

- Si **`Height Falloff > 0`** : densité varie selon Z → en abyssal descent (joueur passe de -200m à -2000m), la fog change drastically.
- Si **`Height Falloff = 0`** : densité est uniforme → c'est ce que tu veux.

→ **Setup baseline Sub3D : Height Falloff = 0.** Tu as déjà ça dans le cave material spec §6. Re-confirmé ici.

### 4.2 Mais comment varier la fog par biome alors ?

Tu veux :
- Coastal (0-120m) : fog claire, visibilité 50m
- Pelagic (120-300m) : fog medium, visibilité 30m
- Bathyal (300-1500m) : fog dense, visibilité 15m
- Abyssal (>1500m) : fog opaque, visibilité 8m

**Solution** : **ne pas utiliser Height Falloff**. À la place, **changer les params HeightFog dynamiquement** via C++ quand le joueur change de biome.

> ⚠️ **2026-05-18 — Décision verrouillée : fog controller FUSIONNÉ dans `UAtmosphereStateController`**
>
> Le `UBiomeFogController` standalone évoqué historiquement (cf cave material spec §6.4) **n'existe plus comme classe séparée**. La logique fog (density / inscatter / extinction / albedo) est intégrée dans `UAtmosphereStateController` (UActorComponent sur `ASubCrewCharacter`) qui pilote aussi le `UPostProcessComponent` de la camera.
>
> → Source de vérité : [`2026-05-18_postprocess_state_controller_spec.md`](2026-05-18_postprocess_state_controller_spec.md) §5 (Q5 — Intégration Fog Controller, Option C fusion) + §7.2 header skeleton.
>
> Conséquence : la cible de transition biome (40 m smoothstep, cf décision 6 du controller spec) est appliquée par le même tick qui pilote PP. Pas de race condition possible entre 2 acteurs (PP volume vs fog actor) — un seul writer.

Approche concrète (post-fusion) :

1. `UAtmosphereStateController::ComputeBiomeFromDepth(WorldZ)` détermine le biome courant.
2. Sur changement de biome, le controller lerp en interne `FogTarget` (DA_Fog_Biome) sur 40 m de profondeur traversée (smoothstep, pas seconde).
3. Le tick écrit directement les params sur `AExponentialHeightFog` cached (cf §5.2 controller spec).
4. Aucun event BP `OnBiomeChanged` requis — c'est continu sur l'axe Z.

### 4.3 Volumetric Fog — recommandé activé

**Active Volumetric Fog** sur ton HeightFog (toggle dans Details). Ça :
- Permet les **light shafts** depuis sources locales (cf cave material spec §7.4)
- Rend les Volumetric Scattering Intensity des lights réactives
- Coûte ~0.5-2ms GPU sur GTX 1660

| Setting | Valeur | Note |
|---|---|---|
| **Volumetric Fog** | ✅ true | obligatoire pour shafts |
| **Scattering Distribution** | 0.3 | anisotropy Henyey-Greenstein |
| **Albedo** | (0.4, 0.5, 0.6) | gris-bleu froid |
| **Emissive** | (0, 0, 0) | aucune lumière émise par fog |
| **Extinction Scale** | 1.5 | densité volumetric |
| **View Distance** | 6000 cm | au-delà tombe en height fog |

### 4.4 Alternative — PostProcess fog material (avancé)

Si HeightFog limite ne te suffit pas, tu peux faire un **PP material fog** custom (post-process material avec scene depth). Plus de contrôle mais plus complexe.

**Quand l'utiliser** :
- Tu veux fog basé sur une **mask 3D** custom (zones de fog densité variable)
- Tu veux fog couleur localement variable (transition warm sub ↔ cool cave par exemple)

**Coût** : développement custom + ~1-2ms perf. Hors scope FP. Backlog.

---

## 5. PostProcess settings recommandés Sub3D

### 5.1 PostProcessVolume baseline

Crée un **PostProcessVolume**, set **Unbound = true** (affecte tout le niveau).

**Settings essentiels** (ne touche que ce qui est listé, laisse le reste défaut) :

#### Lens > Exposure

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Metering Mode** | Manual | Cohérence visuelle Sub3D |
| **Exposure Compensation** | 0.0 | Pas de boost compensatoire |
| **Min EV100** | 0 | Cave abyssale dark |
| **Max EV100** | 0 | = Min, locked |
| **Apply Physical Camera Exposure** | ✅ | physique cohérent |

#### Lens > Bloom

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Method** | Standard | qualité/perf bon |
| **Intensity** | 0.5 | subtil, juste biolum bloomeer |
| **Threshold** | 1.0 | seules les sources HDR (Emissive > 1) bloomeent |
| **Size** | 4.0 | défaut |

#### Lens > Chromatic Aberration

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Intensity** | 0.2 | très subtil, suggère refraction eau (pas vertigo) |
| **Start Offset** | 0.0 | apparait dès le centre |

> Ton 0.5 est trop fort, ça donne le mal de tête. Reste à 0.1-0.3.

#### Lens > Vignette

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Intensity** | 0.4 | tunnel vision abyssale, augmente immersion |

### 5.2 Color Grading

Cette section est sensible — affecte tout le look. Sub3D abyssal :

#### Global

| Setting | Valeur | Note |
|---|---|---|
| **Saturation** | (1.0, 1.0, 1.0, 1.18) | légèrement boost saturation global (canal Y) |
| **Contrast** | (1.05, 1.05, 1.05, 1.0) | légèrement crunché |
| **Gamma** | (0.95, 0.95, 0.95, 1.0) | shadows un poil plus sombres |
| **Gain** | (1.0, 1.0, 1.0, 1.0) | défaut |
| **Offset** | (0.0, 0.005, 0.01, 0.0) | léger bleu en shadows |

> **Important** : la 4ème valeur (Y dans HSV/Y) est la **luminosité**. Saturation Y > 1 = boost.

#### Shadows (zones sombres)

| Setting | Valeur | Note |
|---|---|---|
| **Saturation** | (0.4, 0.4, 0.5, 1.0) | désature shadows → quasi mono-bleu |
| **Contrast** | (1.1, 1.1, 1.0, 1.0) | renforce contraste shadows |

#### Midtones

| Setting | Valeur | Note |
|---|---|---|
| **Saturation** | (0.7, 0.7, 0.8, 1.0) | un peu désaturé |

#### Highlights

| Setting | Valeur | Note |
|---|---|---|
| **Saturation** | (0.6, 0.6, 0.7, 1.0) | shadows < midtones < highlights en désaturation graduelle |

> Tu n'as **pas besoin** de toucher tout, mais ces 4 sections données ensemble créent l'identité abyssale.

### 5.3 Rendering Features

| Setting | Valeur | Pourquoi |
|---|---|---|
| **Ambient Occlusion Intensity** | 0.8 | beaucoup pour casser le plat (cave PMC) |
| **Ambient Occlusion Radius** | 80 cm | proche surface |
| **Indirect Lighting Intensity** | 0.3 | très peu (abyssal) |
| **Indirect Lighting Color** | (0.05, 0.10, 0.15) | tint bleu nuit |
| **Local Exposure Highlight Contrast** | 0.7 | adoucit biolum |
| **Local Exposure Shadow Contrast** | 1.3 | renforce contraste shadow |

---

## 6. Workflow setup complet — starter cohérent

À faire dans cet ordre dans ta map test :

### 6.1 Cleanup

1. Supprime tout **SkyAtmosphere** existant
2. Supprime toute **DirectionalLight** existante
3. Supprime tout **SkyLight HDRI** existant
4. Si tu as un BP_Sky_Sphere, supprime aussi

### 6.2 Add baseline atmosphere

1. **PostProcessVolume** :
   - Unbound = true
   - Settings §5.1 + §5.2 + §5.3
2. **ExponentialHeightFog** :
   - Fog Density = 0.08
   - Fog Height Falloff = **0.0** (critique)
   - Fog Inscattering Color = (0.02, 0.04, 0.06)
   - Volumetric Fog = true
   - Settings §4.3 pour volumetric
3. **SkyLight** (oui mais sans HDRI) :
   - Source Type = Specified Cubemap
   - Cubemap = null
   - Lower Hemisphere Color = (0.0, 0.005, 0.01)
   - Upper Hemisphere Color = (0.0, 0.005, 0.01)
   - Intensity Scale = 0.05
   - → donne juste assez d'ambient pour silhouettes

### 6.3 Add diegetic lights

1. **Sub headlight** (sur Quinn ou submarine BP) :
   - SpotLight, Intensity 5000 Candelas, Cone 20°/30°, Range 8000cm, Color #FFB46A
2. **Biolum cluster** (sur paroi cave) :
   - PointLight, Intensity 150 Lumens, Range 800cm, Color #2DC4B0 (teal)
   - Cast Shadows = false (perf)
3. **Test : passe en Lit mode** → tu devrais voir cave éclairée par sub headlight, biolum subtil

### 6.4 Iterate

Joue avec :
- Sub headlight intensity 1500 ↔ 5000 (jamais 1M)
- Fog Density 0.05 ↔ 0.20
- Vignette 0.3 ↔ 0.6 selon goût

---

## 7. Optimisation perfs lights

### 7.1 Coût général

| Type light | Cost approximatif (GTX 1660) |
|---|---|
| PointLight static (no shadows) | ~0.05 ms |
| PointLight movable + shadows | ~0.3-0.5 ms |
| SpotLight movable + shadows | ~0.4-0.7 ms |
| RectLight movable + shadows | ~0.6-1.0 ms |
| Volumetric Fog contribution per light | +0.1-0.2 ms |

**Budget cible Sub3D** : viser <80 lights actives simultanément, dont **<20 avec Cast Shadows = true**.

### 7.2 Règles d'optimisation

1. **Cast Shadows = false sur décoratif** : biolum, ambient corridor, faune n'ont pas besoin d'ombres
2. **Mobility = Stationary** pour lights fixes : utilise dynamic shadows uniquement pour dynamic geom
3. **Use Inverse Squared Falloff = true** : physique correct, plus rapide
4. **Source Radius petit (1-5 cm)** : évite soft shadow expensive
5. **Distance Field Shadows** : active pour les lights cave (cheap shadow approx)
6. **Attenuation Radius proche du minimum utile** : pas 2x ou 3x si pas besoin
7. **Light Function Material** : pour pattern flicker/anim, plus cheap que multiple lights

### 7.3 Lag PIE avec movable lights sub

**Symptôme utilisateur** : lights movable dans le sub → lag en PIE.

**Causes probables** :
1. **Cast Shadows = true sur trop de lights** : chaque light ombré = dynamic shadow pass
2. **Bounded shadows** : si shadows débordent (range trop grand), GPU loade plus
3. **Volumetric Scattering = 1 sur tous** : Volumetric Fog × N lights = explose
4. **Lumen GI active** : si Lumen Surface Cache rebuild à chaque tick (movable interior) c'est cher

**Fix concret** :
- Sur lights décoratives sub (corridor) → **Cast Shadows = false**, **Volumetric Scattering = 0**
- Seules lights "héros" (headlight, station lights) → shadows + volumetric
- Vérifie **Project Settings > Rendering > Lumen** : si tu peux passer en SSGI/legacy pour tests, fais-le ; Lumen est cher

### 7.4 MegaLights (UE5.5+, optionnel)

Si tu veux plein de small biolum lights (50-100 simultanées), active **MegaLights** dans Project Settings > Rendering. Permet beaucoup de dynamic lights cheap au coût d'un voxel cache (1-2ms fixed).

Tester **après** que tu sois confortable avec setup baseline. Hors scope FP first iteration.

---

## 8. Common pitfalls & fixes

| Pitfall | Symptôme | Fix |
|---|---|---|
| Lights à 1M lumens | Scène cramée même avec exposure haute | Baisser EV100 à 0, lights réalistes ensuite |
| Exposure auto unbounded | Scène fluctue selon orientation caméra | Manual EV100 ou bornes Min/Max strictes |
| HeightFog avec falloff > 0 | Fog disparaît en descendant | Falloff = 0 strict |
| ChromaticAberration > 0.5 | Vertigo, mal de tête joueur | Reste 0.1-0.3 |
| Lights movable + Cast Shadows partout | FPS tank en PIE | Cast Shadows false sur décoratif |
| Pas de SkyLight | Tout noir absolu hors lights | SkyLight intensity 0.05 ambient subtil |
| Émissive > 1 sans Bloom | Pas de glow biolum | Bloom Threshold = 1.0 strict |
| Volumetric Fog Extinction trop haut | Fog opaque, plus rien visible | Extinction 1.0-2.0 selon biome |
| Indirect Lighting Intensity = 1 | Scène trop éclairée d'ambient | 0.2-0.4 max pour abyssal |
| Saturation globale > 1.2 sur Sub3D | Look pas abyssal, trop coloré | Saturation = 1.0 ou désature shadows |

---

## 9. Prompt ChatGPT recherche étendue

Si tu veux creuser un sujet précis (par ex Lumen GI vs SSGI cost détaillé, ou MegaLights setup), voici un prompt template :

```
J'utilise UE 5.7 pour un jeu sous-marin abyssal (Sub3D).
Setup actuel :
- PostProcessVolume Unbound, Exposure Manual EV100 = 0
- ExponentialHeightFog : Density 0.08, Height Falloff = 0, Volumetric Fog ON
- Pas de DirectionalLight, pas de SkyAtmosphere, SkyLight ambient à 0.05
- Lights diégétiques uniquement (sub headlight SpotLight 5000 Candelas, biolum PointLights 150 Lumens)
- Cible perf GTX 1660 Super, FPS >50

Question : [TON SUJET PRÉCIS]

Précise STP :
- Si applicable : valeurs concrètes (pas "moyen" mais 0.5 ou 1.2)
- Coût perf approximatif sur GTX 1660 Super
- Compatibilité UE 5.7 (certaines features comme MegaLights ont changé d'API)
- Trade-offs entre qualité et perf
- Alternatives si la solution principale est trop coûteuse
```

**Exemples de sujets précis à creuser** :
- "Comment activer MegaLights en UE 5.7 et quel est son coût"
- "Quelle est la meilleure approche pour fog variable par profondeur sans HeightFalloff"
- "Setup Light Function Material pour pattern biolum pulse économique"
- "Auto-exposure local vs global pour cave avec biolum HDR"
- "Distance Field Shadows vs Dynamic Shadows pour PMC procedural cave"

---

## Quick reference — values cheatsheet

À garder sous la main pendant tweak :

```
EXPOSURE
─ Min/Max EV100 = 0 (Manual, locked) pour abyssal
─ Compensation = 0
─ Local Exposure : HL=0.7, SH=1.3 si contraste extrême

LIGHTS (avec EV100=0)
─ Sub headlight    : 1500-5000 cd, cone 20/30, range 8000cm
─ Biolum cluster   : 50-200 lm, range 400-800cm
─ Outpost lights   : 800-3000 cd, cone 30/45
─ Flare lancée     : 2500 lm, range 3000cm
─ Ambient SkyLight : intensity 0.05, color (0,0.005,0.01)

FOG
─ HeightFalloff = 0 STRICT
─ Density = 0.05 (Coastal) → 0.25 (Trenches)
─ Inscatter color = sombre bleu-vert
─ Volumetric Fog ON, Extinction 1.5

POSTPROCESS
─ Bloom 0.5, threshold 1.0
─ Chromatic 0.2
─ Vignette 0.4
─ AO 0.8, radius 80cm
─ Indirect 0.3, color (0.05, 0.10, 0.15)
─ Saturation shadows 0.4, midtones 0.7, highlights 0.6
```

---

## 10. PostProcessVolume — reference complète

J'ai initialement listé Lens (Exposure, Bloom, Vignette, Chromatic) et Color Grading et 4 settings Rendering Features. Voici l'exhaustif des sections de `FPostProcessSettings` (UE5.7) avec recommandation Sub3D abyssal.

### 10.1 Lens > Camera (physical camera simulation)

| Setting | Sub3D recommandé | Note |
|---|---|---|
| **ISO** | 100 | défaut, pas touche |
| **Aperture (F-Stop)** | f/2.8 | grande ouverture = peu de DOF |
| **Shutter Speed** | 1/60 | combine avec ISO pour exposure physique |
| **Focal Length** | 35 mm | équivalent FOV humain, naturel |
| **Sensor Width** | 24.89 mm | défaut Super35, pas touche |

**Activer** : `Apply Physical Camera Exposure` = true dans Exposure section. Cohérent avec EV100 manual.

### 10.2 Lens > Depth of Field

Pour cinematique uniquement (cutscenes, planning shots). En gameplay continu = mal de tête.

| Setting | Gameplay | Cinematic |
|---|---|---|
| **Focal Distance** | 0 (off) | 800 cm (cible) |
| **Aperture (Sensor Size)** | — | 2.8 |
| **Sensor Width** | — | 36 mm |

→ Skip pour FP. Backlog post-FP pour cinematiques.

### 10.3 Lens > Lens Flares

Très subtil, **pas pour underwater** classique. Lens flares = optique caméra "sèche". En subaquatique, c'est plutôt particulates et caustiques.

| Setting | Sub3D | Note |
|---|---|---|
| **Intensity** | 0.0 | désactivé |
| **Tint** | — | N/A |
| **Threshold** | 8.0 | N/A si Intensity=0 |
| **Bokeh Shape** | — | N/A |

→ Si tu veux un effet "lampe sub aveugle la cam" : Intensity 0.3, tint warm #FFB46A. Sinon laisse off.

### 10.4 Lens > Image Effects

| Setting | Sub3D | Note |
|---|---|---|
| **Vignette Intensity** | 0.4 | tunnel vision abyssale |
| **Chromatic Aberration** | 0.2 | subtle, NOT 0.5 (vertigo) |
| **CA Start Offset** | 0.0 | apparait dès le centre |
| **Grain Intensity** | 0.15 | film grain léger, stylized hand-painted compatible |
| **Grain Jitter** | 0.0 | défaut |

### 10.5 Color Grading > Temperature

| Setting | Sub3D | Note |
|---|---|---|
| **Type** | White Balance | défaut |
| **Temp** | 5500 | neutre — on tinte ailleurs |
| **Tint** | 0 | défaut |

### 10.6 Color Grading > Misc (souvent oubliés)

| Setting | Sub3D | Note |
|---|---|---|
| **Blue Correction** | 0.6 | corrige les blues désaturés UE par défaut (subtil) |
| **Expand Gamut** | 0.0 | pas besoin |
| **Tone Curve Amount** | 1.0 | applique courbe filmic |
| **Scene Color Tint** | (1.0, 1.0, 1.0, 1.0) | overall tint final — laisse neutre |

### 10.7 Color Grading > LUT

| Setting | Sub3D | Note |
|---|---|---|
| **Color Grading LUT Intensity** | 0.0 | pas de LUT global pour l'instant |
| **Color Grading LUT** | (none) | pourrait être utilisé plus tard pour stylize global |

> **Différence avec `T_AbyssalPalette_LUT`** : le LUT du shader (MF_StylizePBR) remap les couleurs material par material. Le LUT PostProcess remap toute l'image finale. Tu peux utiliser **les deux** en complément, ou juste un. Pour MVP, fais shader LUT seul.

### 10.8 Film (tone curve filmic)

Tonemapping HDR → SDR. **Très important** pour HDR comme tes biolum.

| Setting | Sub3D | Default UE | Note |
|---|---|---|---|
| **Slope** | 0.88 | 0.88 | défaut OK |
| **Toe** | 0.55 | 0.55 | défaut OK |
| **Shoulder** | 0.26 | 0.26 | défaut OK |
| **Black Clip** | 0.0 | 0.0 | jamais clip black |
| **White Clip** | 0.04 | 0.04 | léger clip pour éviter pure white |

→ Tu peux pas toucher. Filmique défaut UE5 = ACES tone mapping cohérent abyssal.

### 10.9 Global Illumination — Lumen

**Section critique perf.** Lumen est cher mais beau. Pour cave abyssale procedural :

| Setting | Sub3D recommandé | Coût GTX 1660 |
|---|---|---|
| **Method** | Lumen (ou Screen Space si <30 fps) | Lumen ~3-5ms |
| **Lumen Scene Lighting Quality** | 1.0 | défaut |
| **Lumen Scene Detail** | 1.0 | défaut |
| **Lumen Scene View Distance** | 4000 cm | réduit du défaut 20000 — gain perf sans impact visible cave |
| **Lumen Final Gather Quality** | 1.0 | défaut |
| **Lumen Final Gather Update Speed** | 1.0 | défaut |
| **Lumen Surface Cache Resolution** | 1.0 | défaut |
| **Use Hardware RT when available** | false | software OK |
| **Diffuse Color Boost** | 1.0 | défaut |
| **Skylight Leaking** | 0.0 | important : pas de leak SkyLight artificiel |

**Si perf insuffisante** → switcher en **Screen Space GI** : Method = Screen Space. Plus de réflexions globales mais moins cher.

### 10.10 Reflections — Lumen Reflections

| Setting | Sub3D | Note |
|---|---|---|
| **Method** | Lumen | défaut UE5 |
| **Lumen Reflection Quality** | 1.0 | défaut |
| **Screen Space Reflection Intensity** | 0.0 | si Lumen actif, désactive SSR (double calcul) |
| **Screen Space Reflection Quality** | — | N/A |

→ Cave abyssale wet rock → reflections subtiles via Lumen. Si Lumen OFF, active SSR Intensity 0.6, Quality 50.

### 10.11 Rendering Features > Ambient Occlusion

| Setting | Sub3D | Note |
|---|---|---|
| **Intensity** | 0.8 | beaucoup pour casser plat PMC |
| **Radius** | 80 cm | proche surface |
| **Quality** | 75 | défaut OK |
| **Mip Blend** | 0.4 | défaut |
| **Fade Out Distance** | 8000 cm | au-delà skip AO |

### 10.12 Rendering Features > Ambient Cubemap

**Trick underwater** : utilise un cubemap dégradé bleu sombre pour simuler le "wash" abyssal directionnel sans DirLight.

| Setting | Sub3D | Note |
|---|---|---|
| **Intensity** | 0.05 | très subtil |
| **Tint** | (0.05, 0.10, 0.18) | bleu nuit |
| **Texture** | (optionnel : un cubemap blue gradient) | si null, intensity n'a aucun effet |

→ Si tu n'as pas envie de créer un cubemap, ignore cette section, le SkyLight ambient à 0.05 fait le job.

### 10.13 Rendering Features > Motion Blur

| Setting | Sub3D | Note |
|---|---|---|
| **Amount** | 0.2 | subtil, sub bouge donc utile |
| **Max** | 5.0 | défaut |
| **Per Object Size** | 0.5 | défaut |

→ Si lag PIE, désactive (Amount = 0).

### 10.14 Rendering Features > Anti-Aliasing

| Setting | Sub3D | Note |
|---|---|---|
| **Method** | TSR (Temporal Super Resolution) | UE5 défaut, **recommandé** |

→ TSR gère bien le marine snow + particulates volumétriques. TAA en remplacement sur hardware faible.

### 10.15 Rendering Features > Local Exposure (UE5 avancé)

| Setting | Sub3D | Note |
|---|---|---|
| **Highlight Contrast Scale** | 0.7 | adoucit les biolum trop éclatants |
| **Shadow Contrast Scale** | 1.3 | renforce le contraste dans shadows |
| **Detail Strength** | 1.0 | définition micro-contraste |
| **Blurred Lum Blend** | 0.6 | défaut |
| **Middle Grey Bias** | 0.0 | défaut |

→ Donne look "HDR cinema" — détails sombres visibles ET biolum sans cramer.

### 10.16 Translucency

| Setting | Sub3D | Note |
|---|---|---|
| **Translucency Type** | Raster | défaut |

→ Pour effets translucents (poissons translucides, bulles), garde défaut. Pas critique FP.

### 10.17 Convolution Bloom (alternative au Standard)

**Optionnel mais intéressant pour Sub3D** :

| Setting | Sub3D | Note |
|---|---|---|
| **Method** | Convolution (au lieu de Standard) | qualité bloom HDR plus naturelle |
| **Convolution Texture** | T_DefaultConvolutionTexture (UE built-in) | OK |
| **Scatter Dispersion** | 1.0 | défaut |
| **Center UV** | (0.5, 0.5) | défaut |

→ Convolution Bloom est ~2× plus cher que Standard mais donne un glow halo plus diffus, parfait pour biolum hero clusters. Tester en final pass.

---

## 11. Underwater UE5 patterns — research étendue

### 11.1 Refraction screen-space (sans surface)

UE5 a un **Material Domain = Post Process** capable de refraction screen-space. Tu peux faire un PP material qui sample SceneTexture::PostProcessInput0 avec offset noise → effet "voir à travers l'eau".

Implémenté dans le cave material spec §5.5 (`M_PP_UnderwaterDistortion`). Subtile et efficace.

**Alternative avancée** : `Material Domain = Surface` avec **Refraction Mode = Index Of Refraction**. Donne refraction physique mais nécessite des proxies water (hors scope Sub3D ground game).

### 11.2 Caustics (sans surface)

Caustiques = motifs de lumière par convergence (au fond d'une piscine). Sans surface, en abyssal **réel** elles n'existent pas. Mais tu peux les simuler **artistique** pour beauty :

**Pattern UE5** :
- `Light Function Material` sur tes lights principales (sub headlight, biolum hero)
- Sample une **caustics texture** (motif noise scrolling)
- Multiply la light contribution par ce pattern
- Donne un effet "lumière dansante" sur les surfaces sans surface réelle

**Coût** : 0.1-0.3ms par light avec function. Acceptable pour 3-5 lights.

→ Backlog Sub3D : `M_LightFunction_Caustics` à créer après MVP.

### 11.3 Water density / visibility (au-delà du fog)

Vraie eau profonde absorbe les longueurs d'onde selon distance :
- Rouge disparaît à ~5m
- Orange à ~10m
- Jaune à ~25m
- Vert à ~70m
- Bleu pénètre jusqu'à ~200m

Pour Sub3D abyssal, c'est nuit total → **pas de soleil pour donner ce gradient**. Mais tu peux tinter la lumière des sources locales (sub headlight) selon la distance traversée.

**Pattern** : dans PP material custom, ré-tinter selon SceneDepth :
```
At pixel distance D:
  red_factor   = exp(-D × 0.02)   // rouge perdu rapide
  green_factor = exp(-D × 0.005)
  blue_factor  = exp(-D × 0.002)  // bleu pénètre loin
  tinted = scene_color × (red, green, blue)
```

→ Hors scope FP. Backlog. Donne un look "vraiment underwater".

### 11.4 Particulates volumétriques (marine snow)

Tu en as déjà spec : `M_PP_UnderwaterParticulate` dans cave material §5.5.

**Pattern avancé Niagara** :
- `NS_AmbientParticulates` Niagara System (point cloud lifetime infinite)
- Spawn rate variable selon profondeur (plus dense en abyssal)
- Random tiny size sprites, additive blend
- Lit par les lights ambiantes (réacts à PointLight biolum quand passe à côté)

→ Beau mais **expensif** : 5000-10000 particules visibles simultanées = +1-2ms. À benchmarker.

### 11.5 Bubble streams (immersion)

`NS_BubbleStream` Niagara — bulles s'élevant de tubes endommagés / vents / créatures.

| Setting | Recommandé |
|---|---|
| **Sprite** | T_Bubble_Translucent (custom) ou particle built-in |
| **Lifetime** | 2-4 sec |
| **Initial Velocity** | (0, 0, 50-150) cm/s vers le haut |
| **Drag** | 0.3 |
| **Size** | 0.5-3 cm |
| **Spawn rate** | 5-20 par spawn point |

→ Backlog Sub3D, place via PCG ou manuellement.

### 11.6 Underwater shader pattern : Color desaturation by depth

Au-delà de la palette LUT, tu peux désaturer par distance dans un Post Process Material :
```
luminance = dot(sceneColor, (0.299, 0.587, 0.114))
desaturated = lerp(sceneColor, vec3(luminance), saturate(sceneDepth × 0.0001))
output = lerp(sceneColor, desaturated, depthDesatStrength)
```

→ Plus la surface est loin, plus elle tend vers gris-bleu. Réalisme underwater.

---

## 12. Diagnostic "drappé blanc" — checklist

Symptôme : scène apparaît voilée blanche/bleu pâle, biolum visibles mais le reste lavé.

### 12.1 Causes possibles classées par probabilité

| Probabilité | Cause | Check |
|---|---|---|
| 🔴 80% | **SkyLight intensity trop haute** (>0.1) | Outliner → SkyLight → Intensity Scale = 0.05 |
| 🟡 60% | **Fog Inscattering Color trop clair** | ExpHeightFog → InscatteringColor ≤ (0.05, 0.08, 0.10) |
| 🟡 50% | **DirectionalLight oubliée dans la scène** | Outliner search "Directional" → delete si existe |
| 🟢 30% | **SkyAtmosphere active** | Outliner search "SkyAtmosphere" → delete si existe |
| 🟢 25% | **Indirect Lighting Intensity trop haut** | PP → Indirect Lighting Intensity ≤ 0.3 |
| 🟢 20% | **Lumen Scene leaking ambient** | PP → Lumen → Skylight Leaking = 0 |
| 🟢 15% | **Cubemap Sky par défaut activé** | SkyLight → Source Type = Specified Cubemap, Cubemap = null |

### 12.2 Procédure diagnostic (5 min)

1. **Outliner search** : type successivement `Directional`, `SkyAtmosphere`, `BP_Sky` → si présent et non-désiré, supprime
2. **Click SkyLight** dans Outliner :
   - Source Type → **Specified Cubemap**
   - Cubemap → **None**
   - Intensity Scale → **0.05**
   - Lower Hemisphere Color → **(0.0, 0.005, 0.01)**
   - Lower Hemisphere is Solid Color → ✅
   - **Recapture Sky** (bouton dans Details)
3. **Click ExpHeightFog** dans Outliner :
   - Fog Inscattering Color → **(0.005, 0.02, 0.04)** (très sombre)
   - Fog Density → 0.08 (test)
   - Volumetric Fog → ✅
4. **Click PostProcessVolume** :
   - Indirect Lighting Intensity → **0.3**
   - Indirect Lighting Color → **(0.05, 0.10, 0.15)**
   - Lumen Skylight Leaking → **0.0** (si visible dans le panel)
5. **Save** et regarde le viewport

Si encore drappé blanc après ces 5 steps : **active `Visualize > Buffer Visualization > Base Color`** (View > Buffer Visualization) pour voir si c'est ton material qui est lavé. Si BaseColor lavé → problème côté material (RockTint trop clair, ou bug LUT). Sinon → problème côté éclairage / fog.

### 12.3 Si tu veux pousser le diagnostic

Active dans le viewport :
- **Show > Lighting Only** → tu vois la contribution éclairage seule (sans BC). Si le drappé persiste → c'est l'éclairage qui leak. Si la scène devient noir → c'est le material qui est lavé.
- **Show > Visualize > Indirect Lighting** → spécifiquement la composante GI
- **Show > Visualize > Reflections** → si reflections "leak" du blanc partout

### 12.4 Fix probable pour ta screenshot

D'après le rendu :
- Fond bleu-blanc uniforme = **fog inscatter trop clair** ou **SkyLight pas à 0.05**
- Bord de cave visible avec gradient = **fog density élevée + couleur trop bright**

**Action immédiate** :
1. PP → exposure check : Min/Max EV100 = 0
2. SkyLight intensity = 0.05 (CRITIQUE)
3. ExpHeightFog → Inscatter (0.005, 0.02, 0.04)
4. Re-screenshot et compare

---

## Cross-references

- **Atlas Asset** `underwater-post-process` doit pointer ici
- **Cave Material Spec** §5/§6/§7 contient settings overlap — ce doc est plus complet, le cave material doit linker ici
- **Memory** `project_abyssal_setting_2026_05_07` : confirme "pas de SkyAtmosphere, pas de DirLight"
