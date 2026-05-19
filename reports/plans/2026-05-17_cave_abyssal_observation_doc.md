# CaveAbyssal ObservationDoc : Workflow & Friction

> Ce doc couvre l'observation de l'implémentation du CaveAbyssal et du workflow IA Humain.
> Ce doc vise le rapportage et sera utilisé en fin d'implémentation du système borné par le document.
> Ce document vise l'amélioration, la statistique, et la persistance du savoir.

| | |
|---|---|
| **Date début** | 2026-05-17 |
| **Doc bornant** | [`2026-05-17_cave_abyssal_material_spec.md`](2026-05-17_cave_abyssal_material_spec.md) |
| **Statut** | Living document — édité au fil de l'implémentation |
| **Cible** | Synthèse finale + extraction de patterns workflow IA-Humain réutilisables |

---

## Sommaire

1. [Observations workflow positives](#observations-workflow-positives)
2. [Frictions rencontrées](#frictions-rencontrées) (catégorisé)
3. [Frictions en cours](#frictions-en-cours)
4. [Frictions anticipées](#frictions-anticipées)
5. [Améliorations workflow identifiées](#améliorations-workflow-identifiées)
6. [Statistiques de session](#statistiques-de-session) (à remplir)

---

## Observations workflow positives

### Aboutissement du workflow IA-Humain (verbatim utilisateur, 2026-05-17)

> "La qualité du doc, et la densité qu'on à réussi à créer me semble être un aboutissement clair de mon expérience workflow IA Humain. Ca permet de garder un cap clair, travailler éfficacement et jusqu'ici, étape 3 ça me semble bien marcher."

### Patterns workflow qui ont bien marché

- **Spec doc dense et itératif** comme contrat partagé IA-Humain ; ré-éditable en cours d'exécution sans casser la cohérence
- **Sommaire + TL;DR + tracks A/B** permettent navigation rapide selon l'usage (lecture exhaustive vs référence ponctuelle)
- **Format node-by-node condensé** (après itération v1 verbeux → v2 condensé) pour reproduire un graph UE5 sans avoir à traduire mentalement
- **Outil HTML standalone** (palette LUT generator) à côté du doc — assets opérationnels immédiatement utilisables
- **Vérification croisée code ↔ spec** avant de prétendre cohérence ("Q3" passé sur AGeologicalCaveActor.MeshMaterial, etc.)
- **Sources DA canoniques consultées** avant d'inventer (cave concepts → recalibration LUT v2)
- **Cleanup explicite après expérimentation** (suppression code scatter C++ après pivot PCG)
- **Atlas Systems comme backlog visuel** centralisé, lié au spec via références cliquables

### Décisions structurantes validées en cours

- Pivot scatter C++ → PCG (cleanup propre, pas de code mort)
- LUT v2 recalibré sur concepts cave canoniques (mauve shadows + teal biolum)
- Light shafts = sources locales uniquement (failles biolum, sub headlight, etc.)
- Palette diégétique sub (warm) vs cave (cool) comme mécanique visuelle clé
- Material Functions partagées entre PMC procedural et rocks PCG via WorldPosition (cohérence garantie)

---

## Frictions rencontrées

### Catégorie 1 — Spec & plan

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 1.1 | Plan initial avait palette LUT générique non-alignée sur DA canon | Recalibration complète après audit | LUT v2 + section §7-bis cohérence diégétique |
| 1.2 | Format graph node-by-node trop verbeux v1 (Add X. Set Y. Connect Z → W) | Lecture lourde | Refactor v2 en bullet condensé `Type Name : pin ← source` |
| 1.3 | Naming `_n` (W_X_n) trop cryptique, friction de compréhension utilisateur | Perte de temps reconstruction sens | Refactor v3 `_Norm` explicite + memory `feedback_material_graph_doc_style` |
| 1.4 | Spec décrit MFs internes mais pas explicitement le câblage master M_CaveAbyssal | Utilisateur bloqué pour assembler | Ajout §3.6 entier (Group A→L + connection summary) + §3.7 workflow test |
| 1.5 | Confusion asset MF vs instance MaterialFunctionCall (MF_Triplanar_Rock paraît être un asset à créer) | Question utilisateur "pourquoi j'ai 2 noms différents" | Explication asset vs instance ; clarifier dans §3.6 |
| 1.6 | Sampler Type des TextureSample pas mentionné dans la spec initiale | 6/9 samples wrong type chez utilisateur (LinearColor partout) | Ajout note explicite §3.4 step A étape 2 |
| 1.7 | Normal intensity via Lerp simple ne donne pas le contrôle attendu | Visuellement pas d'effet sur cave | Switch vers méthode Subtract+Multiply+Add+Normalize (2026-05-17) |

### Catégorie 2 — Outils & API UE5

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 2.1 | Script Python `generate_M_CaveAbyssal.py` : tous les `connect_material_expressions` échouent | Master créé vide, manuel obligatoire | Abandon script, retour manuel |
| 2.2 | `unreal.MaterialExpressionTransformVector` n'existe pas (vrai nom : `MaterialExpressionTransform`) | RuntimeError dans script | Documented (n'a pas servi car script abandonné) |
| 2.3 | API Python ne permet pas de set `bUsedWithProceduralMesh` de manière stable | Manuel obligatoire post-script | Noté dans header script + workflow test |
| 2.4 | Compilation Substrate (UE 5.7) cause warnings sur certaines MFs | Preview MF ne compile pas | Workaround : ouvrir le MF tel quel, vérifier dans M réel |
| 2.5 | "Missing Preview connection" sur Function Input StaticBool | Preview MF compile pas | Set "Preview Value" sur le Function Input |

### Catégorie 3 — Asset naming & cohérence

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 3.1 | Path `/Game/Sub3D/MF/` (spec) ≠ `/Game/Sub3D/Material/CaveGenerator/` (utilisateur) | Script Python échoue à charger MFs | CONFIG block en tête de script |
| 3.2 | `MF_BiolumSpots` (spec) → `MF_Biolum` (utilisateur, plus court) | MFs absentes au load | Mapping logique dans script |
| 3.3 | Typo `MF_Segiment` initial → renommé `MF_SedimentOverlay` | Asset à corriger après création | Renaming utilisateur post-flagging |
| 3.4 | Typo `TillingScale` (double L) dans MF_Triplanar_PBR | Param exposé avec mauvais nom | Flag déjà donné, à fixer utilisateur |
| 3.5 | ScalarParameter dans MF vs FunctionInput Scalar | Divergence architecture spec vs implémentation utilisateur | Validé : les deux marchent, ScalarParameter plus simple |
| 3.6 | `Out_BC` missing sur MF_StrataBanding (output non créé ou nommé différemment) | Compilation MF échoue | Vérification utilisateur, à corriger |

### Catégorie 4 — Asset sourcing

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 4.1 | `AP2_Rocks_BaseColor` ne fit pas le DA abyssal (vert clair pavé, pas paroi rock) | Rendu visuel hors cible | Recommandation : Megascons / Sketchfab CC0 / ArtStation stylized |
| 4.2 | Pas de texture sediment sourcée → sediment apparaît noir/transparent | Visuellement incomplet | Sourcing reporté, fonctionnel pour test rock seul |

### Catégorie 5 — Spécificités cave generator (hors scope material mais impactent visuel)

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 5.1 | Vertex normals de cave pointent vers rock (interior mesh) → sediment apparaît sur plafond au lieu du sol | Sediment inversé visuellement | Ajouter `In_InvertUp` StaticBool à MF_SedimentOverlay |
| 5.2 | Wireframe inégal entre chunks (smoothing s'applique pas partout) | Visuel pas uniforme | **Backlog cave generator** — out of scope material |
| 5.3 | Mesh seams / petits trous / fissures entre chunks | Visuel cassé visible | **Backlog cave generator** — out of scope material |
| 5.4 | Biolum patches forment des stries au lieu de blobs sur surfaces inclinées | Visuel pas conforme attente | Tuning paramètres BiolumNoiseScale + Density |

### Catégorie 6 — Communication & format

| # | Friction | Impact | Résolution |
|---|---|---|---|
| 6.1 | IA suppose paths/noms par défaut sans vérifier (cf. script Python) | Script échoue au premier run | Toujours demander/vérifier paths réels avant scripting |
| 6.2 | IA suggère solutions complexes (Python script) là où manuel marche mieux | Temps perdu sur outil cassé | Trade-off : automatisation seulement quand l'effort de fix < manual |
| 6.3 | IA construit doc cohérent mais l'utilisateur le découvre au fur et à mesure → certaines décisions changent (palette, scope, etc.) | Doc à re-éditer en cours | Acceptable : le doc est vivant, c'est sa nature |

---

## Frictions en cours

| Date | Friction | Statut |
|---|---|---|
| 2026-05-17 | Sediment inversé sur cave interior (normals flipped) | Fix MF_SedimentOverlay avec In_InvertUp à implémenter |
| 2026-05-17 | Biolum scarse + stries au lieu de blobs | Tuning params, peut-être revoir Voronoi Quality |
| 2026-05-17 | Rendu PMC fait "cube terrain années 1990-2000" — manque de relief perçu | Backlog Phase 2 : PCG complex layer + displacement + generator pass strate (§8-bis) |
| 2026-05-17 | Step navigation §8 pas assez claire — utilisateur cherchait T_AbyssalPalette_LUT sans le trouver | Ajout §3.8 procédure dédiée + §8 enrichi avec tracker ✅⏳⏸ + liens cliquables vers sections |
| 2026-05-17 | **Normals à RockNormalIntensity=3.0 créent effet "plastic wavy" bizarre sur certaines géométries inclinées** (cf image 4 user). Au plafond cave avec NormalIntensity=1, peu de relief perçu — geometry insuffisamment denses ou normals trop subtiles. | À adresser plus tard. **Pistes** : (1) clamp range 0-2 max au lieu de 0-5, (2) saturate après normalize pour éviter déviations extrêmes, (3) alternative : utiliser FlattenNormal MF built-in pour intensity < 1 et garder deviation scaling > 1, (4) investigate orientation cave PMC normals vs lighting direction sur plafond |
| 2026-05-17 | LUT remap force highlights vers couleur "high" du LUT — même avec Cold Pure (highlight #8A95A8), à StylizationStrength > 0.2 les strates sombres deviennent CLAIRES par remap luminance | Architecture limit : LUT remap-by-luminance ne préserve pas l'intent "dark zones stay dark". Workaround court terme : exposer `StrataBandColor` comme VectorParameter (§3.6.6-tris) pour contrôler la couleur des bandes indépendamment du LUT. Long terme : refactor MF_StylizePBR avec per-step strengths (Solution B précédemment discutée) pour découpler edge effect de LUT remap. |
| 2026-05-18 | **Chromatic Aberration `StartOffset = 0.5` crée stries horizontales visibles en bord de cave** (Step 5 baseline test, screenshots `reports/observations/2026-05-18_step5_pp_baseline/chromab_offset05.png` vs `..._offset00.png`). À `StartOffset = 0.0` les stries disparaissent. | Décision sur la fly : **set `StartOffset = 0.0`** (override valeur cave material spec §5.2-ter si elle était 0.5). Update spec à faire si la valeur canonical est ≠ 0.0. À ranger comme tunable "feel-tune" (cf playbook ci-dessous). |
| 2026-05-18 | **Bloom + lens flares saturent excessivement sur biolum punctuelles** (image `..._bloom_lens_biolum.png`) — sources biolum jaune/vert très lumineuses non polish, halos écrasants. | Différencier deux niveaux : (1) placement biolum / patch density à raffiner (post-FP), (2) bloom threshold + flare tints à modérer **après** validation complete Step 5 + controller. Pas de changement spec maintenant — c'est polish. Voir playbook PP tunable knobs ci-dessous. |
| 2026-05-18 | **Matériaux paraissent très sombres en cave** Step 5 baseline test — sensation potentiellement claustrophobique | **Aucun changement impl docs maintenant** (user explicit). Idée à valider après Step 5 + controller actifs + ambient + headlamp warm full setup → réévaluer. Si toujours trop sombre, ajustement à faire dans **DA_PP_Location_Cave** (decision: ExposureCompensation -0.3 → -0.1, ou SkyLight Intensity 0.08 → 0.12). Tunable, pas structural. |
| 2026-05-18 | **Explosion des params PostProcess + Color Grading** — `UPostProcessSettings` expose ~200 propriétés, impossible de tout tuner à la main sans perdre du temps. User demande tooling ou guidance "quand puis-je tuner au feeling sans peur". | Réponse → nouvelle section "Playbook PP tunable knobs vs locked knobs" ci-dessous. Court terme : table locked / feel-tune / care-tune. Mid terme : éventuel outil HTML similaire à `abyssal_palette_lut.html` pour générer DA presets avec sliders. Long terme : DA composition controller (Medium/Location/Biome) absorbe la majorité — feel-tune se fait DANS les DA, pas sur le PostProcessVolume. |
| 2026-05-18 | **`LocalExposure.DetailStrength = 4.0` (cave material spec §5.2-bis) crée halos abherants sur `ULightComponent`** — découverte empirique user en plaçant SpotLight 35k cd sub headlamp. Au-delà de 2.0 l'enhancement local de luminance amplifie le falloff attenuation curve du light en énorme blob HDR. | **Correction immédiate canonical** : cave material spec §5.2-bis Detail Strength **4.0 → 1.0** (aligné canonical lighting spec qui était déjà à 1.0 — divergence pré-existante résolue). Note documentaire : biolum-only scenes (emissive surface punctuelle) tolèrent jusqu'à 4-6 mais dès qu'un ULight est présent, range max 1.0–2.0. **Insight général** : Local Exposure interagit fortement avec le type de source — emissive surface ≠ point/spot light pour cet algo. Update playbook FEEL-TUNE range Detail Strength : `2.0-6.0` → `1.0-3.0`. |
| 2026-05-18 | **Canonical light intensity values (35k cd hero etc.) testées en scène vide saturent l'auto-expo au ceiling +1.5 EV100** — bien que valeurs nominales prévues pour scène équilibrée. Place un seul SpotLight 35k cd dans cave proto vide → camera était déjà à expo max → light rendu ~6.5× over-bright + bloom + volumetric scattering empilent. | **Insight test ordering** : ne pas valider canonical light intensities en isolation. Workflow validé : (1) lock temporairement `Min/Max EV100 = 0.0` pour test isolé, (2) ou réduire intensity à 10-15k cd dans cave proto vide, re-bump canonical 35k quand scène peuplée. **Pattern à propager** : tout param dont la cible suppose "scène équilibrée" doit être documenté avec condition de validation. À noter dans canonical lighting spec §3.2 (TODO). |
| 2026-05-18 | **Tradeoff Local Exposure / Bloom : globaux → impossible séparer biolum vs ULight nativement** — user discovered post-fix DetailStrength 1.0 : le pop biolum HDR halo qu'il aimait est perdu en même temps que le blob ULight, parce que les deux effets viennent du même algo PP UE5 global. | **Architecture limit UE5** : PostProcess (Local Exposure, Bloom, LensFlare) ne discrimine pas par object/material. 3 options évaluées : **A (FP)** rebalance `BiolumIntensity` × `BloomThreshold` discriminant (**TESTÉE 2026-05-18 — insuffisante**, ne récupère pas le pop HDR halo que DetailStrength 4 donnait), **B (post-FP)** Niagara lens flare per biolum patch (~1-2j), **C (post-FP) ⭐ confirmée** Custom Stencil PP material `M_PP_BiolumFlare` (4-6h, +0.2ms GPU, compatible DA composition controller). Backlog Phase 2 cave material spec §8-bis.3-bis. Implication playbook : `BloomThreshold` reste tunable mais discriminant biolum/ULight = chemin partiel seulement, le pop HDR vrai nécessite stencil mask. FP accepte le visuel "biolum subtle" en attendant Option C post-FP. |

## Frictions résolues

| Date | Friction | Résolution |
|---|---|---|
| 2026-05-17 | Normal intensity Lerp ne donne pas le contrôle | Switch deviation scaling (Subtract+Multiply+Add+Normalize) ; ET découverte parallèle : effet pas visible en Unlit, demande PointLight intensity 100000+ pour voir l'impact sur PMC plat |
| 2026-05-17 | Tiling rock trop fin pour le visuel cible | À ajuster via WorldTilingScale (default 0.001 → tester 0.0005) — pas de changement de spec nécessaire |
| 2026-05-17 | Biolum apparaît sur sediment (incohérent : cristaux ne poussent pas sur silt) | Sub-graph master OneMinus(SedMask) × BiolumEmissive (§3.6.11-bis) |
| 2026-05-17 | Strata banding apparaît aussi sur sediment (incohérent : silt sableux n'a pas de strates géologiques) | Pattern Lerp(pre_strata, post_strata, SedMask_Inv) — réutilise le node SedMask_Inv déjà créé pour biolum (§3.6.6-bis). Pattern réutilisable pour futures couches "rock-only" |
| 2026-05-17 | "Drappé blanc" en viewport — scène pas vraiment abyssale, ambient/fog leak | Diagnostic en 5 steps : SkyLight intensity 0.05 + cubemap=None, ExpHeightFog Volumetric Albedo (0.02, 0.05, 0.08), Fog Inscatter (0.005, 0.02, 0.04), Indirect Lighting Intensity 0.3, Lumen Skylight Leaking 0. Validé par utilisateur, rendu abyssal cohérent obtenu |
| 2026-05-17 | Setup lighting/PP/fog initial fragile (lights à 1M lumens, EV100 à 7.48 effective) | Doc dédié [2026-05-17_lighting_postprocess_fog_spec](2026-05-17_lighting_postprocess_fog_spec.md) avec valeurs concrètes calibrées Sub3D + diagnostic "drappé blanc" §12. Setup EV100=0 + lights réalistes (5000 cd headlight) valide visuellement |
| 2026-05-17 | Test material en Unlit ne révèle rien sur le relief / normals / shading | Solution C++ : `AViewportHeadlamp` actor + menu Window > Toggle Viewport Headlamp (Sub3DEditor module). SpotLight 30000 lm warm tungsten qui suit la caméra editor en temps réel |
| 2026-05-17 | Attente utilisateur "LUT change all colors to unify" ≠ implémentation réelle (luminance remap, préserve structure dark/light) | Documentation comportement clarifiée + 5 presets palettes ajoutés au LUT generator (Abyssal v2 / Abyssal Cold Pure / Deep Sea / Ocean Breeze / Coastal Warm) pour permettre exploration sans recoder shader. Friction de design persistante : workflow LUT-based ≠ hue-shift global, à valider en playtest |

---

## Frictions anticipées

| Phase | Friction probable | Mitigation préventive |
|---|---|---|
| Group H Stylization | MF_StylizePBR utilisera dérivées (DDX/DDY) — UE 5.7 peut avoir restrictions sur Material Domain Surface | Avoir un fallback sans edge enhance |
| Group K Biolum | Voronoi 3D peut être très lent sur cave dense (PMC énorme) | Profile à activer, fallback Perlin si <30fps |
| MI per strate | Le mesh PMC mono-bloc rend impossible le swap MI par section | Solution future : modulation in-shader OU sous-meshes par strate |
| Asset sourcing stylized | Aucun pack hand-painted abyssal cave PBR connu n'existe vraiment | Plan B : AI generation + Materialize pour Normal/Roughness |
| Cave generator seams | Si pas fixés avant production : visuel cassé permanent | Backlog explicite + fix avant FP |
| PCG biolum cluster (Approche D) | Setup PCG complexe pour spawn rules par biome | Spec dédiée à écrire avant impl |
| Transition palette sub↔cave aux sas | Risque d'effet "stickers" si pas géré | Documenter dans spec sas/airlock material |

---

## Améliorations workflow identifiées

### Pour Claude (process IA)

1. **Vérifier les paths réels avant tout scripting** — demander ou inspecter avant d'inventer
2. **Préférer manuel guidé vs script automatisé** quand l'API est instable (UE Material Editing en particulier)
3. **Toujours croiser code ↔ spec** avant de prétendre cohérence
4. **Sources DA avant inventions** — consulter `C:/ACC/Projects/Sub3D/Image & Concept/` systématiquement
5. **Format graph stable** — appliquer memory `feedback_material_graph_doc_style` v3 sans variation

### Pour l'utilisateur (process humain)

1. **Naming convention assets dès création** — éviter typos rétroactifs (TillingScale, MF_Segiment)
2. **Sampler Type dès création TextureSample** — pas attendre la 9e instance pour découvrir le problème
3. **Test minimal après chaque MF** — éviter de découvrir un bug en assemblant tout
4. **Backup avant scripts destructifs** — le script delete+recreate du master a écrasé l'asset

### Pour le doc (process partagé)

1. **TL;DR + table de navigation** systématique sur tout spec >300 lignes
2. **Lien doc ↔ atelier (Atlas Asset)** systématique pour traçabilité
3. **Sections "À ne pas faire" explicites** comme guardrails
4. **Outils HTML standalone** quand pertinent (LUT generator pattern réutilisable)

---

## Statistiques de session

> À remplir en fin d'implémentation. Métriques cibles :

| Métrique | Valeur | Note |
|---|---|---|
| Lignes de spec produites | ~2000+ | Doc spec + observation + tool |
| Material Functions créées | 4/7 | (Triplanar, Biolum, Sediment, Strata) — manque Fracture, Wet, Stylize |
| Master materials créés | 1 (M_CaveAbyssal en cours) | |
| Itérations de format graph doc | 3 (verbeux → condensé → naming explicite) | |
| Scripts Python tentés / abandonnés | 1 / 1 | API UE5 trop instable pour ce cas |
| Outils HTML produits | 1 (LUT generator) | |
| Recalibrations majeures | 1 (LUT v1 → v2 DA-aligned) | |
| Memories créées | 2 | `feedback_material_graph_doc_style` |
| Sessions de validation | TBD | |
| Total temps spec → résultat visuel valide | TBD | |

---

## Patterns réutilisables identifiés

### Pattern 1 : Spec dense avec tracks parallèles
Track A = technique baseline, Track B = surcouche optionnelle (stylized). Permet à l'IA et l'humain de progresser sur des layers indépendants.

### Pattern 2 : Outil HTML standalone à côté du spec
Un fichier `.html` autonome qui produit un asset concret immédiatement utilisable (LUT generator). Évite la dépendance à l'IA pour itérer sur cet asset précis plus tard.

### Pattern 3 : Atlas Asset entry comme "tableau de bord" du système
Référence vers spec + tool + concept canonique + rules. Permet de retrouver tout le contexte d'un système en un point d'entrée.

### Pattern 4 : Memory feedback explicite après friction
Quand l'utilisateur corrige un format/naming/process IA, immédiatement créer une memory pour persister la règle. Évite la re-correction en session future.

### Pattern 5 : Cleanup destructif documenté
Pivot tech (scatter C++ → PCG) traité comme cleanup explicite, pas juste "ignorer l'ancien code". Évite le legacy silencieux.

---

## Playbook PP tunable knobs vs locked knobs (2026-05-18)

> Réponse à la question utilisateur "à quel moment je peux régler au feeling sans peur ?".
> Source : audit `UPostProcessSettings` UE5.7 + 8 décisions canonical lighting/PP/fog + 10 décisions controller spec + 8 décisions project settings.

### 🔒 LOCKED — ne pas toucher sans nouveau spec daté

Ces params ont un impact structural (cohérence physique, perf, anti-pattern shipping). Modifier = nouveau spec qui supersede.

| Param | Valeur locked | Source décision |
|---|---|---|
| `bApplyPhysicalCameraExposure` | **false** | canonical PP/fog §2 |
| `AutoExposureMethod` | **Histogram** | canonical §2 |
| `AutoExposureMin/MaxBrightness` (EV100) | **-0.5 / +1.5** | canonical §2 |
| `ToneCurveAmount` / film tone curve type | **Custom (Slope 0.88, Toe 0.42, Shoulder 0.30, WhiteClip 0.08)** | canonical §5.2 |
| `Saturation` shadows / mid / high | **0.85 / 1.05 / 0.90 (LIFT pas crush)** | canonical §5.2 |
| `bOverride_DynamicGlobalIlluminationMethod` | **SSGI** (pas Lumen) | project settings + canonical §10.9 |
| `bOverride_ReflectionMethod` | **Lumen Reflections ON** | canonical §10.10 |
| `FogHeightFalloff` (ExpHeightFog actor) | **0.0** | canonical §4.1 |
| `bEnableVolumetricFog` | **true** | canonical §4.3 |
| Substrate | **OFF** | project settings + controller décision 9 |
| Forward Shading | **OFF** | project settings |
| Fixed Frame Rate | **60** | project settings |

### 🎯 FEEL-TUNE — pas de peur, tune en PIE, copie dans DA

Ces params sont à expression artistique, dans des ranges raisonnables. Itère librement en PIE puis fige dans le DA approprié (Medium / Location / Biome) une fois content.

| Param | Range raisonnable | Quand tuner |
|---|---|---|
| `ExposureCompensation` | -1.0 → +0.5 | Si scène trop sombre / trop claire après baseline. Par Location DA. |
| `ChromaticAberrationStartOffset` | 0.0 → 0.7 | Décision sur la fly : 0.0 = pas de stries, 0.5+ = visible. Default 0.0 confirmé 2026-05-18. |
| `SceneFringeIntensity` (ChromAb intensity) | 0.0 → 0.4 | Effet abyssal "deep pressure" subtil. 0.2 confirmé. |
| `VignetteIntensity` | 0.3 → 0.6 | Ambiance claustrophobique. Par Location DA. |
| `BloomIntensity` | 0.4 → 0.9 | Selon density biolum scène. Par Location/Biome DA. |
| `BloomThreshold` | 0.7 → 2.0 | ⚠️ **tune par paire avec `BiolumIntensity` (MI param)** — pas seul. Discriminant biolum vs ULight : threshold haut catch que les highs HDR (biolum), threshold bas catch tout. Cf friction 2026-05-18 sur Local Exposure / Bloom global. |
| `BiolumIntensity` (MI Color Token MaterialInstance param) | 5 → 30 | Calibre conjointement avec BloomThreshold. Si BloomThreshold = 1.5, BiolumIntensity > 20 pour pop, < 10 pour subtle. |
| `LocalExposureHighlightContrastScale` | 0.7 → 1.0 | Cinema HDR feel. |
| `LocalExposureShadowContrastScale` | 1.0 → 1.3 | Push shadow detail. |
| `LocalExposureDetailStrength` | **1.0 → 3.0** | ⚠️ **range corrigée 2026-05-18** (était 2.0-6.0). Avec ULight présent : max 2.0. Biolum-only scene tolère 4-6 mais risk halo HDR si ULight ajouté plus tard. |
| Fog `FogDensity` (DA_Fog_Biome_X target) | 0.03 → 0.25 | Per biome (Coastal clair / Trenches opaque). |
| Fog `FogInscatteringColor` (linear RGB) | teal/blue dark range | Per biome ; respecter palette teal (cf Color Tokens). |
| Fog Volumetric `Albedo` | (0.005, 0.02, 0.05) → (0.08, 0.15, 0.20) | Selon ambiance biome. |
| Color Tokens `TKN_BIOLUM_*` linear RGB | shifts ±20% sur saturation/teinte | Variations par espèce biolum. |
| Color Tokens `TKN_ROCK_TINT_*` | shifts ±15% saturation | Par strate géologique. |
| Light `Intensity` (Candelas) | sub headlight 25k–45k, EVA 8k–18k | Cohérent avec exposure auto bornée. Si tu doubles, vérifie tone curve. |
| Light `OuterConeAngle` / `InnerConeAngle` | spot 8°/22° → 18°/40° | Ergonomie cinematic vs gameplay. |
| Light `VolumetricScatteringIntensity` | 0.0 → 2.0 | Shafts intensity. |

### ⚠️ CARE-TUNE — tunable mais avec sanity check

Ces params changent l'équilibre global. Tune mais vérifie ensuite que (a) HDR auto-expo reste stable, (b) GPU ms ne saute pas, (c) le visuel des autres biomes reste cohérent.

| Param | Range | Sanity check |
|---|---|---|
| SkyLight `Intensity Scale` | 0.04 → 0.15 | Si > 0.15, risque "drappé blanc" (cf friction résolue 2026-05-17) |
| `LocalExposureMethod` (None / Bilateral / Fusion) | rester sur Bilateral default | Fusion = ~+0.3 ms GPU GTX 1660 |
| `BloomMethod` (Standard / Convolution) | Standard | Convolution = ~+1 ms GPU + asset Convolution Texture |
| `MotionBlurAmount` | 0.0 → 0.3 | > 0.3 = écœurement player FPS |
| `AmbientOcclusionIntensity` (SSAO) | 0.3 → 0.7 | > 0.7 + SSGI = double-darkening contact |
| `LumenSceneDetail` | 0.5 → 2.0 | Hors scope car SSGI mais reflections Lumen utilisent ces params |
| MPC `BiomeFogDensity` (controller-driven) | per-biome | Si tu ajustes hors controller, conflit transition |

### Tooling — court / mid / long terme

**Court terme (maintenant)** :
- Reste sur tune-by-feel directement dans PIE → PostProcessVolume Details panel
- Pour chaque valeur figée, **note-la dans le DA Medium/Location/Biome correspondant** (pas dans le volume scene)
- Cette table playbook = ta référence "puis-je tuner ce param ?"

**Mid terme (post-Step 6 controller actif)** :
- Petit outil HTML similaire à `abyssal_palette_lut.html` : `pp_da_preset_generator.html`
- 3 sliders par DA axis (Medium / Location / Biome)
- Output = JSON des `bOverride_X` + valeurs → paste dans DA UE5
- Permet exploration rapide sans toucher le code

**Long terme (post-FP)** :
- Slate dockable panel similar au Sub3DDebugPanel
- Live diff sur le DA actif dans le PIE
- Auto-save vers DA asset quand satisfait
- Build sur le pattern `mcp__unrealclaude__unreal_set_property` + remote control

### Règle générale "feel-tune without fear"

1. Si le param est dans 🔒 LOCKED → **ne touche pas**, ouvre un nouveau spec.
2. Si le param est dans 🎯 FEEL-TUNE → **vas-y**, screenshot avant/après, note la valeur, push dans DA.
3. Si le param est dans ⚠️ CARE-TUNE → tune **mais lance `stat gpu`** + check les 4 biomes en PIE rapide.
4. Si le param n'apparaît pas dans cette table → **demande-moi**, je classifie + ajoute à la table.

---

## Décisions verrouillées

| Date | Décision | Source | Impact |
|---|---|---|---|
| 2026-05-18 | **8 décisions PP/Fog/Lighting locked** (canonical 2026-05-18) | [Canonical spec lighting/PP/fog](2026-05-18_lighting_pp_fog_canonical_spec.md) §564 + ChatGPT research | Source-of-truth pour PP/Fog/Lighting. Cave abyssal spec §5/§6/§7 alignés. Exposure auto bornée + SSGI au lieu de Lumen + Custom Tone Curve + Saturation lift au lieu crush + Sub headlight 35000cd + SkyLight cubemap obligatoire |
| 2026-05-18 | **Color Tokens recalculés vrai linear RGB** (bug fix) | Audit utilisateur 2026-05-18 | §7-bis.4-bis updated. Toutes valeurs sRGB stockées comme "linéaire" corrigées via formule sRGB→linear. Tous les MI overrides existants doivent être re-taggés |
| 2026-05-18 | **8 décisions Project Settings locked** | [Project Settings canonical 2026-05-18](2026-05-18_project_settings_canonical_spec.md) §décisions verrouillées | Substrate OFF, Forward Shading OFF, DX12 RHI, HW RT OFF, GI=SSGI, ShadowMap=VSM, AutoExpo OFF, Fixed Frame Rate 60. Restart editor requis après changes Rendering/Lighting |
| 2026-05-18 | **10 décisions PostProcess State Controller locked** (Opus 4.7 agent) | [PostProcess State Controller canonical 2026-05-18](2026-05-18_postprocess_state_controller_spec.md) §décisions verrouillées | Architecture = Option F hybrid : `UPostProcessComponent` on camera + `UAtmosphereStateController` (UActorComponent) sur `ASubCrewCharacter`. DA design D2 axis-based (9 DA composés via `bOverride_X`). **Fog controller fusionné** dans le controller (le `UBiomeFogController` standalone n'existe plus). Head underwater = compartment-relative + hysteresis 5cm. Tick TG_PostPhysics, prereq `USubCrewMovementComponent`. Transitions : Medium 0.4s ease-out, Location 1.0s ease-in-out, Biome 40m smoothstep. PP material custom = BeforeTonemap only, **deferred post-FP**. Diff-then-set MPC. Substrate OFF (1660 Super). Migration `ULocalPlayerSubsystem` = backlog post-FP |

## Mises à jour de ce doc

| Date | Auteur | Changement |
|---|---|---|
| 2026-05-17 | IA (init) | Création du doc avec catégorisation initiale frictions |
| 2026-05-18 | IA + user | Lock 9 décisions canoniques. Aligne cave abyssal spec sur canonical lighting/PP/fog. Color Tokens corrigés linear RGB |
| 2026-05-18 | IA + user (Opus 4.7) | Lock 10 décisions PostProcess State Controller. `UBiomeFogController` standalone supprimé (fusionné dans `UAtmosphereStateController`). Cave material spec §5/§6/§7 obtient cross-refs vers controller pour state-driven behavior |
| 2026-05-18 | IA + user | Step 5 baseline test observations : 5 frictions (ChromAb StartOffset stries, bloom/lens biolum overload, matériaux dark, params explosion PP/color grading). Nouvelle section "Playbook PP tunable knobs vs locked knobs" avec table 🔒 LOCKED / 🎯 FEEL-TUNE / ⚠️ CARE-TUNE + règle "feel-tune without fear" + tooling court/mid/long terme |
| 2026-05-18 | IA + user | Step 5 baseline test (suite) : 2 frictions ajoutées (LocalExposure DetailStrength 4.0 wrong sur ULight, canonical light intensities saturent auto-expo en scène vide). Corrections canonical : cave material spec §5.2-bis DetailStrength 4.0→1.0 + §5.2-ter ChromAb StartOffset 0.5→0.0. Playbook FEEL-TUNE range DetailStrength updated 2.0-6.0 → 1.0-3.0. |
| 2026-05-18 | IA + user | Friction "Tradeoff Local Exposure / Bloom globaux empêchent biolum-only pop". 3 options évaluées (A rebalance FP / B Niagara post-FP / C Custom Stencil PP material post-FP ⭐). Backlog cave material spec §8-bis.3-bis ajouté. Playbook updated : BloomThreshold tune par paire avec BiolumIntensity, ajout entry BiolumIntensity dans table FEEL-TUNE. |
