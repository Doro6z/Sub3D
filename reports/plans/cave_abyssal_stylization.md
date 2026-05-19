# Cave Abyssal — Stylization (Track B) — annexe

| | |
|---|---|
| **Date** | 2026-05-17 (split du main spec 2026-05-18) |
| **Statut** | Annexe — Track B Stylized DA |
| **Doc parent** | [cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md) |
| **Scope** | §14 du spec parent — Stylized hand-painted DA + MF_StylizePBR refactored v2 (per-step strengths) |

---

## 14. Stylized / hand-painted DA — sources & shader transformation

> 🎨 **TRACK B** — couche stylized par-dessus la spec technique (§§ 1-13 du parent).
> Les §§ 1-13 fonctionnent en PBR neutre photo-réaliste. Cette section ajoute la **transformation hand-painted** propre à ta DA.
> Indépendante : tu peux skipper §14 et avoir un résultat technique correct ; tu peux aussi appliquer juste §14 sans toucher le reste.

---

### Pourquoi une section dédiée

La spec du parent est neutre côté style (triplanar PBR standard fonctionne avec Megascans photo-réaliste). **Ton DA cible un look hand-painted stylized** (réf : Subnautica Below Zero crystal caves, Sea of Thieves stylized PBR, painterly brushstrokes visibles mais lit en PBR — PAS flat low-poly DRG/Lethal Company). Deux approches : sourcer des packs déjà stylisés, ou transformer du PBR photo en stylisé dans le shader. Mieux : combiner les deux.

### 14.0 LUT generator tool — déjà disponible

Outil HTML standalone : [`reports/tools/abyssal_palette_lut.html`](../tools/abyssal_palette_lut.html).
Ouvre dans Chrome → sélectionne preset → "Download" → import dans UE (Compression `VectorDisplacementMap (RGBA8)`, sRGB **décoché**).

**Presets disponibles** :
- `Abyssal v2 — Cool + warm highlights` (original)
- **`Abyssal Cold Pure`** — all cool, highlights restent gris-bleu (recommandé pour éviter wash)
- `Deep Sea` — navy → off-white
- `Ocean Breeze` — bright blues (trop clair pour cave abyssal)
- `Coastal Warm` — variante warm pour biome shallow

### 14.1 Sources de textures hand-painted / stylized PBR

#### Tier A — Payant ($10–40 par pack, qualité pro)

| Source | URL | Recherche |
|---|---|---|
| **ArtStation Marketplace** | artstation.com/marketplace | "stylized cave", "stylized rock pbr", "hand painted texture", "stylized cliff" |
| **Unity Asset Store** | assetstore.unity.com | "stylized rocks pbr", "hand painted environment" — importable UE via FBX+PNG |
| **Cubebrush** | cubebrush.co | section "Textures & Materials" → filtre "stylized" |
| **Gumroad** | gumroad.com | search "hand painted texture pack" — artists indé, $5–20 |
| **CGTrader** | cgtrader.com | "stylized cave" + filter "with textures" |

**Artistes/packs spécifiques utiles pour ton DA** :
- Lukas Walzer — stylized PBR rocks packs (ArtStation)
- Sergei Panin — hand-painted environment textures
- Krzysztof Czerwinski — stylized cave & cliff packs
- 3DWave — Cubebrush, stylized PBR séries

#### Tier B — Gratuit / CC

| Source | URL | Détail |
|---|---|---|
| **Sketchfab** | sketchfab.com | Filter : Downloadable + License "CC Attribution" or "CC0" + tag "stylized" |
| **itch.io** | itch.io | Tag "assets" + "stylized" — beaucoup pay-what-you-want avec option $0 |
| **OpenGameArt** | opengameart.org | Vieux mais beaucoup de tilesets hand-painted seamless CC0 / CC-BY |
| **PolyHaven** | polyhaven.com | Pas spécifiquement stylized, mais filter "stones" → sourcing brut à styliser au shader |
| **Kenney** | kenney.nl | Stylized mais souvent trop low-poly pour ton DA |
| **AmbientCG** | ambientcg.com | Pareil que PolyHaven — base PBR brute à styliser |

#### Tier C — Génération maison

| Outil | Usage |
|---|---|
| **Substance Painter** | Brush packs "hand painted smart materials" (sur Substance Source ou Gumroad) → peindre directement sur tes meshes Meshy |
| **Blender Texture Paint** | Mode peinture direct sur mesh + matcap pour preview rapide. Gratuit. |
| **Materialize** (FOSS) | Convertir photo → seamless PBR si tu as une ref que tu veux reproduire |
| **AI (Midjourney / DALL-E)** | Prompt "hand painted stylized seamless rock texture, abyssal, cyan biolum" + img2img tiling. Légalement à vérifier selon usage. |

---

### 14.2 Approche 2 — Transformer du PBR photo en stylized via shader

Au lieu de sourcer 30 packs hand-painted, prends 1 pack Megascans (ou ce que tu as déjà) et transforme-le via une **Material Function `MF_StylizePBR`** qui posterize + remap palette + flatten normals + edge enhance. Tu obtiens un look hand-painted cohérent sur n'importe quelle texture source.

#### MF_StylizePBR — spec (refactor v2 per-step strengths)

##### Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_StylizePBR` |
| **Location** | `/Game/Sub3D/MF/MF_StylizePBR` |
| **Output type** | 3 pins (BC_stylized, N_stylized_WS, R_stylized) |
| **Description** | Transforme PBR neutre en hand-painted : posterize + LUT remap + flatten normal + edge enhance + roughness clamp + brush overlay optionnel. |

##### Function Inputs

> ⚠️ **Refactor 2026-05-17 v2** : `In_StylizationStrength` master gate **retiré**. Remplacé par 5 strengths per-step indépendants. Motif : utilisateur signale qu'il aime l'effet edges + brush mais veut LUT minimal, impossible avec un seul master multiplier. Cette architecture (Solution B précédemment discutée) découple chaque étape.

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BC` | Vec3 | — | BC photo entrant |
| `In_NormalWS` | Vec3 | — | Normal world space entrante |
| `In_Roughness` | Scalar | — | Roughness entrante |
| `In_PaletteLUT` | Texture2DObject | T_AbyssalPalette_LUT | LUT 256×16 (généré via [tool](../tools/abyssal_palette_lut.html)) |
| `In_BrushTexture` | Texture2DObject | T_BrushStrokes_Grunge | Overlay grunge seamless |
| **`In_PosterizeStrength`** | Scalar | 0.0 | Force du posterize (0=continu, 1=full quantized 4 paliers). Si 0, posterize bypass. |
| **`In_LUTStrength`** | Scalar | 0.05 | Force du LUT remap palette. **Default faible** car LUT modifie agressivement les couleurs. |
| **`In_NormalFlatStrength`** | Scalar | 0.0 | Force flattening normals vers vertex normal (0=plein détail, 1=plat). |
| `In_EdgeIntensity` | Scalar | 0.6 | Force ink lines (0=invisible, 1.5=très marqué). Direct control sans compound. |
| **`In_RoughnessMatteStrength`** | Scalar | 0.0 | Force application roughness clamp matte (0=roughness source, 1=clamp full). |
| `In_BrushStrength` | Scalar | 0.2 | Force overlay grunge brush. Direct control sans compound. |
| `In_RoughnessFloor` | Scalar | 0.65 | Clamp matte min (utilisé seulement si RoughnessMatteStrength > 0) |
| `In_RoughnessCeiling` | Scalar | 0.80 | Clamp matte max |
| `In_PosterizeLevels` | Scalar | 4 | Paliers cible quand PosterizeStrength = 1 (3-16) |

##### Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | BC stylized |
| `Out_NormalWS` | Vec3 | Normal flattened world space |
| `Out_Roughness` | Scalar | Roughness clampé matte |

##### Étapes de création

###### Étape 1 — Posterize (color quantization) — gated par `In_PosterizeStrength`

- `Lerp` `Levels` : A ← Constant 255, B ← In_PosterizeLevels (Function Input default 4), Alpha ← `In_PosterizeStrength`
- `Multiply` `BC_Scaled` : A ← In_BC, B ← Levels
- `Floor` `BC_Stepped` : input ← BC_Scaled
- `Divide` `BC_Posterized` : A ← BC_Stepped, B ← Levels

> À `In_PosterizeStrength = 0`, Levels = 255 → posterize quasi inactif (256 niveaux ≈ continu). À 1 → 4 paliers durs.

###### Étape 2 — Palette remap via LUT — gated par `In_LUTStrength`

- `DotProduct` `Luminance` : A ← BC_Posterized, B ← Constant3Vector (0.299, 0.587, 0.114)
- `AppendVector` `LUT_UV` : A ← Luminance, B ← Constant 0.5
- `TextureSample` `LUT_Sample` : Tex ← In_PaletteLUT, UVs ← LUT_UV — set Sampler Type = "Color"
- `Lerp` `BC_Remapped` : A ← BC_Posterized, B ← LUT_Sample.RGB, Alpha ← Constant 0.7 *(internal blend conservé — affecte la rigidité du LUT vs source)*
- `Lerp` `BC_Palette` : A ← In_BC, B ← BC_Remapped, Alpha ← `In_LUTStrength`

> **LUT settings dans UE** : Compression = `VectorDisplacementMap (RGBA8)`, sRGB = **décoché**.

> **Default `In_LUTStrength = 0.05`** : effet subtil sans wash. À monter (0.2-0.4) si tu veux le remap palette plus dominant. >0.5 commence à faire blanchir les highlights vers la couleur "high" du LUT.

###### Étape 3 — Flatten normals — gated par `In_NormalFlatStrength`

- `VertexNormalWS` `VN_World`
- `Lerp` `N_Flat_Unnorm` : A ← In_NormalWS, B ← VN_World, Alpha ← `In_NormalFlatStrength`
- `Normalize` `N_Flat` : input ← N_Flat_Unnorm
- **Out_NormalWS** ← N_Flat

> **Default `In_NormalFlatStrength = 0.0`** : pas de flattening, garde le détail normal map full. Si tu veux un look "papier plié" stylized, monter à 0.3-0.6.

> Pas besoin de Tangent↔World conversion ici car `In_NormalWS` est déjà en world space (sortie de MF_Triplanar_PBR).

###### Étape 4 — Edge enhance (ink lines) — direct via `In_EdgeIntensity`

- `DDX` `Grad_X` : input ← BC_Palette
- `DDY` `Grad_Y` : input ← BC_Palette
- `Abs` `Grad_X_Abs` : input ← Grad_X
- `Abs` `Grad_Y_Abs` : input ← Grad_Y
- `Add` `Grad_Sum` : A ← Grad_X_Abs, B ← Grad_Y_Abs
- `VectorLength` `Grad_Mag` : input ← Grad_Sum
- `SmoothStep` `Edge_Raw` : Min ← Constant 0.05, Max ← Constant 0.15, Value ← Grad_Mag
- `Multiply` `Edge_Final` : A ← Edge_Raw, B ← `In_EdgeIntensity` *(plus de compound avec master, direct)*
- `OneMinus` `Edge_Factor` : input ← Edge_Final
- `Multiply` `BC_With_Edges` : A ← BC_Palette, B ← Edge_Factor

> **Default `In_EdgeIntensity = 0.6`** : ink lines visibles mais subtiles. À 1.0+ devient cel-shading agressif.

> **Pourquoi DDX/DDY** : variation de couleur entre pixels voisins → détecte les transitions automatiquement. Plus cheap que Sobel (4 samples sup) et acceptable pour ink line subtle.

###### Étape 5 — Roughness clamp matte — gated par `In_RoughnessMatteStrength`

- `Lerp` `R_Clamped` : A ← In_RoughnessFloor, B ← In_RoughnessCeiling, Alpha ← In_Roughness
- `Lerp` `R_Stylized` : A ← In_Roughness, B ← R_Clamped, Alpha ← `In_RoughnessMatteStrength`
- **Out_Roughness** ← R_Stylized

> **Default `In_RoughnessMatteStrength = 0.0`** : laisse roughness source intact. À monter (0.3-0.7) si tu veux le look matte hand-painted (réduit reflets photo-réalistes).

###### Étape 6 — Hand-paint overlay — direct via `In_BrushStrength`

- `WorldPosition`
- `Multiply` `Brush_UV_Raw` : A ← WorldPosition, B ← Constant 0.002
- `ComponentMask` `Brush_UV` (R+G) : input ← Brush_UV_Raw
- `TextureSample` `Brush_Sample` : Tex ← In_BrushTexture, UVs ← Brush_UV — set Sampler = "Grayscale"
- `Multiply` `Brush_Scaled` : A ← Brush_Sample.R, B ← Constant 0.3
- `Add` `Brush_Factor` : A ← Constant 0.85, B ← Brush_Scaled (range [0.85, 1.15])
- `Multiply` `Brush_Alpha` : A ← `In_BrushStrength`, B ← Constant 0.5 *(plus de compound avec master)*
- `Lerp` `Brush_Apply` : A ← Constant 1.0, B ← Brush_Factor, Alpha ← Brush_Alpha
- `Multiply` `BC_With_Brushes` : A ← BC_With_Edges, B ← Brush_Apply
- **Out_BC** ← BC_With_Brushes

> **Default `In_BrushStrength = 0.2`** : grain pictural subtle. À 0.5+ devient texture overlay agressive.

> **Note triplanar complet** : pour un look ultra-cohérent, remplace l'étape 6 par 3 samples + blend selon VertexNormalWS comme MF_Triplanar_PBR. Plus cher (3× samples) mais évite l'étirement aux faces verticales.

##### Paramètres exposés sur les MI dérivés

Les Function Inputs ci-dessus sont rebindés dans le master M_CaveAbyssal comme MaterialParameters (donc visibles dans les MI). Cohérence garantie.

| Param master M_CaveAbyssal | → Function Input MF_StylizePBR | Default master |
|---|---|---|
| `PosterizeStrength` | In_PosterizeStrength | 0.0 |
| `LUTStrength` | In_LUTStrength | 0.05 |
| `NormalFlatStrength` | In_NormalFlatStrength | 0.0 |
| `EdgeIntensity` | In_EdgeIntensity | 0.6 |
| `RoughnessMatteStrength` | In_RoughnessMatteStrength | 0.0 |
| `BrushOverlayStrength` | In_BrushStrength | 0.2 |
| `T_PaletteLUT` | In_PaletteLUT | T_AbyssalPalette_LUT |
| `T_BrushTexture` | In_BrushTexture | T_BrushStrokes_Grunge |
| `RoughnessMatteFloor` | In_RoughnessFloor | 0.65 |
| `RoughnessMatteCeiling` | In_RoughnessCeiling | 0.80 |
| `PosterizeLevels` | In_PosterizeLevels | 4 |

---

### 14.3 Intégration au master M_CaveAbyssal

Tu insères `MF_StylizePBR` **après** le triplanar + strata + sediment, **avant** le pin Base Color :

```
[triplanar + sediment + strata + fracture + wet]
        ↓ BC, N, R
[MF_StylizePBR]
        ↓ BC_stylized, N_stylized, R_stylized
[Gate_BC/N/R via StaticSwitch EnableStylization]
        ↓
[FinalColor → Base Color pin]
[N_stylized → TransformVector W→T → Normal pin]
[R_stylized → Roughness pin (after R_Range Lerp)]

// Le biolum n'est PAS stylisé (il garde son HDR cyan pur)
[MF_BiolumSpots] → Pin Emissive (inchangé, voir Group K)
```

**Astuce** : ajouter un `Static Switch Parameter` "EnableStylization" pour pouvoir basculer photo ↔ stylized en MI pour A/B comparer.

Détails master complets : [cave_abyssal_master_graph.md](cave_abyssal_master_graph.md) §3.6.9 Group H.

---

### 14.4 Recommandation pour Sub3D solo dev

| Priorité | Action |
|---|---|
| 1 | **Acheter 1 pack stylized rock** sur ArtStation ($15-30) comme **anchor visuel** — définit la palette cible et le niveau de stylisation |
| 2 | **Créer T_AbyssalPalette_LUT** via tool HTML (5 min) en choisissant preset Abyssal Cold Pure |
| 3 | **Implémenter MF_StylizePBR** refactor v2 (per-step strengths) une fois |
| 4 | **Sourcer tout le reste en PBR neutre** (Quixel Megascans, Poly Haven — gratuit ou inclus UE) et appliquer MF_StylizePBR via MI |
| 5 | **Hand-paint 2-3 overlays grunge** dans Substance/Photoshop pour casser le "trop propre" |

ROI : 1 pack acheté + 1 MF + 1 LUT + 3 overlays = look cohérent stylisé sur des dizaines de textures sources.

### 14.5 Pièges à éviter

- ❌ **Posterize seul** (sans LUT remap) — donne un look "réduit de couleurs" cheap, pas hand-painted
- ❌ **Flatten normal à 100%** — perd toute la lisibilité du mesh, plate comme du papier
- ❌ **Edge enhance trop fort** — devient cel-shading anime, pas painterly
- ❌ **Mélanger packs stylized + photo PBR non transformés** côte à côte — incohérence visuelle immédiate
- ❌ **LUT générique colorée** (genre rainbow LUT téléchargé) — doit être peint **pour ta DA spécifique**, pas générique

### 14.6 Ref visuelles pour calibrer le degré de stylisation

> **Note refactor v2** : avec per-step strengths, tu n'as plus un seul "StylizationStrength" global. Les valeurs ci-dessous sont indicatives pour le **LUTStrength** (puisque c'est lui qui pousse le plus vers stylized). Adapte aussi PosterizeStrength + EdgeIntensity en proportion.

| Référence | LUTStrength | PosterizeStrength | EdgeIntensity |
|---|---|---|---|
| Subnautica Below Zero | ~0.3 | 0.0 | 0.4 |
| Sea of Thieves | ~0.5 | 0.4 | 0.8 |
| World of Warcraft (récent) | ~0.7 | 0.6 | 1.0 |
| Spyro Reignited | ~0.85 | 0.85 | 1.2 |
| Borderlands | ~0.95 + cel shading | 1.0 | 1.5 |

**Ton DA cible probablement LUTStrength 0.1-0.3** (subtil, brushstrokes visibles via brush overlay + edges, mais couleurs source préservées). Tester en live dans MI.

---

## Cross-refs

- **Spec parent** : [2026-05-17_cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md)
- **MFs détaillées** : [cave_abyssal_mf_graphs.md](cave_abyssal_mf_graphs.md) (§3.4 step A→H)
- **Master graph** : [cave_abyssal_master_graph.md](cave_abyssal_master_graph.md) (§3.1/3.2/3.3 + §3.6 Group A→L)
- **LUT tool** : [reports/tools/abyssal_palette_lut.html](../tools/abyssal_palette_lut.html)
