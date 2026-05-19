# Cave Abyssal — Material Functions Graphs (annexe)

| | |
|---|---|
| **Date** | 2026-05-17 (split du main spec 2026-05-18) |
| **Statut** | Annexe — spec graphs MFs détaillés |
| **Doc parent** | [cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md) |
| **Scope** | §3.4 du spec parent — tous les Material Functions step-by-step (Triplanar, Sediment, Strata, Fracture, Biolum, WetSurface) + sub-graphs Final color / Final roughness |

---

### 3.4 Graphe — étape par étape

> **Naming convention** : nodes intermédiaires nommés avec rôle explicite, suffixes complets (`_Norm` = normalized, `_Final`, `_Tinted`, `_Inv`, `_Raw`, `_Scaled`, `_Step1`). Pas d'abréviations cryptiques type `_n`. Quand tu crées le node dans UE, renomme-le via le panneau Details → "Description" pour faciliter le câblage.

**Navigation interne §3.4** :

| Step | MF / sous-graph | Rôle | Lignes approx |
|---|---|---|---|
| A | `MF_Triplanar_PBR` | Sample 3 textures PBR en 3 projections perpendiculaires | ↓ 6 étapes |
| B | `MF_SedimentOverlay` | Blend rock ↔ sediment selon orientation normal | ↓ 3 étapes |
| C | `MF_StrataBanding` | Bandes horizontales géologiques via sin(WorldPos.Z) | ↓ 3 étapes |
| D | `MF_FractureMask` | Lignes sombres en réseau via Voronoi3D edges | ↓ 3 étapes |
| E | Final color (sous-graph master) | Combine sorties vers pin Base Color | mini |
| F | Final roughness (sous-graph master) | Lerp + sediment boost + saturate | mini |
| F-bis | `MF_StylizePBR` (track B) | Renvoi vers §14.2 du spec parent — couche stylized | renvoi |
| G | `MF_BiolumSpots` | Tâches biolum teal HDR via Voronoi3D + density mask + pulse | ↓ 4 étapes |
| H | `MF_WetSurface` (optionnel) | Darken BC + reduce roughness pour ambiance humide | ↓ 2 étapes |

**Ordre d'application dans le master M_CaveAbyssal** :

```
MF_Triplanar_PBR (A)
        ↓ BC_rock, N_rock_WS, R_rock
MF_SedimentOverlay (B)
        ↓ BC_blended, N_blended_WS, SedimentMask
MF_StrataBanding (C)
        ↓ BC_with_strata, StrataMask
MF_FractureMask (D)
        ↓ BC_with_fractures
MF_WetSurface (H, optionnel)
        ↓ BC_wet, R_wet
[Sous-graph Final color (E) + Final roughness (F)]
        ↓ BC_final, R_final
MF_StylizePBR (F-bis, si EnableStylization)
        ↓ BC_stylized, N_stylized_WS, R_stylized
TransformVector (World → Tangent) sur N_stylized
        ↓
Pins du Material Result : Base Color · Normal · Roughness

[En parallèle, indépendant]
MF_BiolumSpots (G)
        ↓ Emissive_HDR → Pin Emissive Color
```

---

#### A. Triplanar projection (MF_Triplanar_PBR)

##### A.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_Triplanar_PBR` |
| **Location** | `/Game/Sub3D/MF/MF_Triplanar_PBR` |
| **Output type** | 3 pins individuels (BC_World, N_World, R) — pas MaterialAttributes |
| **Description** | Sample 3 textures PBR (BC/N/R) en 3 projections perpendiculaires (XY/XZ/YZ) et blend selon la normale du vertex. Utilisé pour les meshes sans UVs (PMC procedural). |

##### A.1 Function Inputs (panneau Inputs de la MF)

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BaseColor` | Texture2DObject | — | Texture BC à sampler en triplanar |
| `In_Normal` | Texture2DObject | — | Normal map (compression "Normal map") |
| `In_Roughness` | Texture2DObject | — | Roughness map (R channel utilisé) |
| `In_TilingScale` | Scalar | 0.001 | 1 / distance de tiling en cm (0.001 = 1 tile / 1000cm) |
| `In_BlendSharpness` | Scalar | 4.0 | Dureté des transitions entre les 3 plans (1=très smooth, 16=très net) |
| `In_NormalIntensity` | Scalar | 1.0 | 0 = flat, 1 = full normal map, >1 = exagération (deviation scaling, §A.4 étape 5-bis) |

> **Comment ajouter un Function Input** : dans le graph de la MF, click-droit → "Add Input Node". Sélectionne le type via le panneau Details → Input Type.

##### A.2 Material Parameters (aucun ici — la MF expose via Inputs, les params sont dans le master)

Les valeurs `WorldTilingScale` et `TriplanarBlendSharpness` sont des **paramètres du master material** `M_CaveAbyssal` (cf §3.3 du spec parent) qui se plugguent dans les inputs `In_TilingScale` et `In_BlendSharpness` de cette MF.

##### A.3 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | Base Color blendée |
| `Out_NormalWS` | Vec3 | Normal en **world space** (à reconvertir tangent space dans le master) |
| `Out_Roughness` | Scalar | Roughness blendée |

> **Comment ajouter un Output** : click-droit dans le graph → "Add Output Result". Renomme-le via Details.

##### A.4 Étapes de création du graph

###### Étape 1 — UVs triplanaires

- `WorldPosition` — set "World Position Origin" = Absolute
- `Multiply` : A ← WorldPosition, B ← In_TilingScale
- `ComponentMask` × 3 — input ← Multiply :
  - `UV_XY` mask R+G
  - `UV_XZ` mask R+B
  - `UV_YZ` mask G+B

###### Étape 2 — Sampler les 3 textures × 3 projections (9 TextureSample)

- `TextureSample` × 3 (BC) — Tex ← In_BaseColor, **Sampler Type = `Color`** :
  - `TS_BC_XY` : UVs ← UV_XY
  - `TS_BC_XZ` : UVs ← UV_XZ
  - `TS_BC_YZ` : UVs ← UV_YZ
- `TextureSample` × 3 (N) — Tex ← In_Normal, **Sampler Type = `Normal`**, UVs idem (TS_N_XY/XZ/YZ)
- `TextureSample` × 3 (R) — Tex ← In_Roughness, **Sampler Type = `Linear Color`**, UVs idem (TS_R_XY/XZ/YZ)

> ⚠️ **Sampler Type critique** : à régler dans le Details panel de chaque TextureSample. Quand le texture vient via Function Input (pas assigné direct), UE ne peut pas deviner le type — défaut LinearColor = BC apparaîtra trop sombre, Normal mal décompressé. Set explicitement.

> Astuce optionnelle : créer une sub-MF `MF_SamplePBR_OneAxis(UV, Tex_BC, Tex_N, Tex_R)` → returns 3 pins. Instanciée 3 fois ici à la place de 9 TextureSample. Plus propre.

###### Étape 3 — Weights de blend depuis la normale

- `VertexNormalWS` (Vertex Attributes → World Space)
- `Abs` : input ← VertexNormalWS
- `Power` : Base ← Abs, Exp ← In_BlendSharpness
- `ComponentMask` × 3 — input ← Power :
  - `W_X` mask R (= poids plan YZ)
  - `W_Y` mask G (= poids plan XZ)
  - `W_Z` mask B (= poids plan XY)
- `Add` `Sum_AB` : A ← W_X, B ← W_Y
- `Add` `Sum_W` : A ← Sum_AB, B ← W_Z
- `Divide` × 3 — B ← Sum_W :
  - `W_X_Norm` : A ← W_X
  - `W_Y_Norm` : A ← W_Y
  - `W_Z_Norm` : A ← W_Z

###### Étape 4 — Blend BC

- `Multiply` × 3 :
  - `Mul_BC_YZ` : A ← TS_BC_YZ.RGB, B ← W_X_Norm
  - `Mul_BC_XZ` : A ← TS_BC_XZ.RGB, B ← W_Y_Norm
  - `Mul_BC_XY` : A ← TS_BC_XY.RGB, B ← W_Z_Norm
- `Add` `Sum_BC_1` : A ← Mul_BC_YZ, B ← Mul_BC_XZ
- `Add` `Sum_BC_final` : A ← Sum_BC_1, B ← Mul_BC_XY
- **Out_BC** ← Sum_BC_final

###### Étape 5 — Blend Normales (RNM via built-in)

- `BlendAngleCorrectedNormals` `BlendN_1` : BaseNormal ← TS_N_XY.RGB, AdditionalNormal ← TS_N_XZ.RGB
- `BlendAngleCorrectedNormals` `BlendN_2` : BaseNormal ← BlendN_1, AdditionalNormal ← TS_N_YZ.RGB

> **Pourquoi pas Multiply+Add comme pour BC** : les normales ne sont pas linéaires — additionner directement les vecteurs normalisés donne un résultat incorrect aux frontières. `BlendAngleCorrectedNormals` (built-in UE5, cherche dans MaterialFunctions) implémente RNM (Barré-Brisebois) qui corrige ça.
>
> **Alternative si pas dispo** : `Add` des 3 normales tangent + `Normalize` (méthode UDN). Moins correct mais marche.

###### Étape 5-bis — Normal Intensity (deviation scaling)

> ⚠️ Historique : la première version utilisait un `Lerp(VertexNormal, NormalMap, intensity)`. Cassé en pratique (interpole 2 vecteurs unitaires sur 3 axes simultanément, dégradation au milieu). Remplacé 2026-05-17 par deviation scaling.

Après BlendN_2, avant l'output, intercale :

- `VertexNormalWS` `VN_Flat`
- `Subtract` `N_Deviation` : A ← BlendN_2, B ← VN_Flat
- `Multiply` `N_Dev_Scaled` : A ← N_Deviation, B ← In_NormalIntensity
- `Add` `N_Adjusted` : A ← VN_Flat, B ← N_Dev_Scaled
- `Normalize` `N_Final` : input ← N_Adjusted
- **Out_NormalWS** ← N_Final

**Effet réel** (validé utilisateur 2026-05-17) :
- Visible **en mode Lit** uniquement (Unlit ne calcule pas l'éclairage donc ne montre pas le relief)
- Demande une source lumineuse forte (testé avec PointLight intensity 100000) pour voir l'impact sur PMC procedural cave
- Sur surfaces très planes du PMC actuel : impact visuel limité car la géométrie elle-même n'a pas assez de variation pour que le normal map sculpte du détail visible

###### Étape 6 — Blend Roughness

- `ComponentMask` (R only) × 3 sur les TS_R pour extraire le R channel :
  - `R_XY` ← TS_R_XY, `R_XZ` ← TS_R_XZ, `R_YZ` ← TS_R_YZ
- `Multiply` × 3 :
  - `Mul_R_YZ` : A ← R_YZ, B ← W_X_Norm
  - `Mul_R_XZ` : A ← R_XZ, B ← W_Y_Norm
  - `Mul_R_XY` : A ← R_XY, B ← W_Z_Norm
- `Add` `Sum_R_1` : A ← Mul_R_YZ, B ← Mul_R_XZ
- `Add` `Sum_R_final` : A ← Sum_R_1, B ← Mul_R_XY
- **Out_Roughness** ← Sum_R_final

##### A.5 Connection summary (vérification rapide)

| Step | Node | Pin source | → Pin destination |
|---|---|---|---|
| 1 | WorldPosition | XYZ | Multiply_Scale.A |
| 1 | In_TilingScale | — | Multiply_Scale.B |
| 1 | Multiply_Scale | output | CMask_XY.input, CMask_XZ.input, CMask_YZ.input |
| 2 | CMask_XY/XZ/YZ | output | TextureSample_BC/N/R_XY/XZ/YZ.UVs (9 connexions) |
| 2 | In_BaseColor / In_Normal / In_Roughness | — | TextureSample.Tex pin |
| 3 | VertexNormalWS | output | Abs.input |
| 3 | Abs | output | Power.Base |
| 3 | In_BlendSharpness | — | Power.Exp |
| 3 | Power | output | W_X/Y/Z masks |
| 3 | W_X/Y/Z | output | Sum_W (Add×2) |
| 3 | Divide×3 | A=W_*, B=Sum_W | → W_X_Norm, W_Y_Norm, W_Z_Norm |
| 4 | TextureSample_BC_*.RGB × W_*_Norm | — | Mul_BC_*, puis Add×2 |
| 4 | Add2 | output | **Out_BC** |
| 5 | TextureSample_N_XY × N_XZ × N_YZ | — | BlendAngleCorrectedNormals×2 |
| 5-bis | BlendN_2 - VertexNormalWS × Intensity + VertexNormalWS → Normalize | — | **Out_NormalWS** |
| 6 | TextureSample_R_*.R × W_*_Norm | — | Mul_R_*, puis Add×2 → **Out_Roughness** |

##### A.6 Notes & pièges

- **Performance** : 9 TextureSample = coûteux. Garde les textures source à 1024² max (le détail vient des 3 projections croisées de toute façon).
- **Normal en world space** : `Out_NormalWS` est en world space. Le master `M_CaveAbyssal` doit le reconvertir en tangent space via `TransformVector (World → Tangent)` avant de le brancher sur le pin Normal du Result, **OU** activer "Tangent Space Normal = false" dans les Details du Result et brancher en world direct. Préférer la première option (cohérent avec autres MFs).
- **Texture compression** : les `In_Normal` textures doivent avoir Compression Settings = `Normal map (DXT5, BC5 on D3D11+)` et **sRGB décoché**. Les BC/R standard sRGB pour BC, no-sRGB pour R.
- **Tiling visible aux transitions** : si tu vois des bandes parallèles évidentes, augmente `In_BlendSharpness` (essaie 6 ou 8) et/ou ajoute un noise de break-up sur `In_TilingScale` (multiplier par un Perlin basse fréquence).
- **Coût RNM** : `BlendAngleCorrectedNormals` ajoute ~12 instructions par appel × 2 appels = 24 instructions. Acceptable pour qualité du résultat.

---

#### B. Sediment layer (MF_SedimentOverlay)

##### B.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_SedimentOverlay` |
| **Location** | `/Game/Sub3D/MF/MF_SedimentOverlay` |
| **Output type** | 3 pins (BC_blended, N_blended_WS, SedimentMask) |
| **Description** | Blend rock ↔ sediment selon orientation normal (dot avec up). Sol/paliers = sédiment, parois/plafond = rock pur. |

##### B.1 Function Inputs

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BC_Rock` | Vec3 | — | BC rock (sortie de MF_Triplanar_PBR sur textures rock) |
| `In_N_Rock_WS` | Vec3 | — | Normal rock world space |
| `In_BC_Sediment` | Vec3 | — | BC sediment (autre triplanar avec textures silt) |
| `In_N_Sediment_WS` | Vec3 | — | Normal sediment world space |
| `In_RockTint` | Vec3 | (0.50, 0.55, 0.60) | Teinte appliquée au rock avant blend |
| `In_SedimentTint` | Vec3 | (0.40, 0.38, 0.32) | Teinte appliquée au sédiment |
| `In_UpThreshold` | Scalar | 0.55 | dot(N, up) > seuil → sediment |
| `In_BlendSharpness` | Scalar | 8.0 | Dureté transition (plus = plus net) |

##### B.2 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | BC blendé rock ↔ sediment |
| `Out_NormalWS` | Vec3 | Normal blendée world space |
| `Out_SedimentMask` | Scalar | 0 = rock pur, 1 = sediment pur (pour roughness adjust en aval) |

##### B.3 Étapes de création

###### Étape 1 — Calcul du sediment weight depuis l'orientation

- `VertexNormalWS`
- `Constant3Vector` `Up_Vec` — set value = (0, 0, 1)
- `DotProduct` `DotUp` : A ← VertexNormalWS, B ← Up_Vec
- `Subtract` `Edge_Min` : A ← In_UpThreshold, B ← Constant 0.05
- `Add` `Edge_Max` : A ← In_UpThreshold, B ← Constant 0.05
- `SmoothStep` `Weight_Raw` : Min ← Edge_Min, Max ← Edge_Max, Value ← DotUp
- `Power` `Weight_Final` : Base ← Weight_Raw, Exp ← In_BlendSharpness
- **Out_SedimentMask** ← Weight_Final

###### Étape 2 — Application des tints

- `Multiply` `BC_Rock_Tinted` : A ← In_BC_Rock, B ← In_RockTint
- `Multiply` `BC_Sed_Tinted` : A ← In_BC_Sediment, B ← In_SedimentTint

###### Étape 3 — Blend BC et N selon weight

- `Lerp` `BC_Blended` : A ← BC_Rock_Tinted, B ← BC_Sed_Tinted, Alpha ← Weight_Final
- `Lerp` `N_Blended` : A ← In_N_Rock_WS, B ← In_N_Sediment_WS, Alpha ← Weight_Final
- **Out_BC** ← BC_Blended
- **Out_NormalWS** ← N_Blended

> **Pourquoi 2 edges sur le SmoothStep** : transition douce sur 0.10 d'amplitude (Threshold ± 0.05) puis durcie par Power. Ça évite une cassure brutale rock↔sediment et permet `In_BlendSharpness` de contrôler finement la netteté.

---

#### C. Strata banding (MF_StrataBanding)

##### C.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_StrataBanding` |
| **Location** | `/Game/Sub3D/MF/MF_StrataBanding` |
| **Output type** | 2 pins (BC_with_strata, StrataMask) |
| **Description** | Bandes horizontales géologiques via sin(WorldPos.Z) avec warp noise. Lit WorldPos donc traverse PMC + rocks PCG cohérent. |

##### C.1 Function Inputs

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BC` | Vec3 | — | BC entrant (sortie de SedimentOverlay) |
| `In_Period` | Scalar | 800 | Hauteur d'une bande en cm |
| `In_DarkenAmount` | Scalar | 0.35 | Profondeur du darkening (0=invisible, 1=noir) |
| `In_NoiseWarp` | Scalar | 200 | Amplitude de la déformation noise des bandes |
| `In_NoiseScale` | Scalar | 0.0005 | Fréquence du noise de warp (1 / longueur en cm) |
| `In_RustColor` | Vec3 | (0.43, 0.25, 0.14) | Teinte rust des bandes (canon `#6e3f24`) — optionnel |
| `In_RustStrength` | Scalar | 0.3 | 0 = darken seul, 1 = full rust tint |

##### C.2 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | BC avec strates appliquées |
| `Out_StrataMask` | Scalar | 0 entre bandes, 1 au coeur d'une bande (pour usage aval) |

##### C.3 Étapes de création

###### Étape 1 — Warp noise sur WorldPos.Z

- `WorldPosition` — Absolute
- `Multiply` `WPos_NoiseUV` : A ← WorldPosition, B ← In_NoiseScale
- `Noise` `Warp_Noise` — set Function = "Simplex" (ou Perlin), Quality=2 : Position ← WPos_NoiseUV
- `Multiply` `Warp_Amount` : A ← Warp_Noise, B ← In_NoiseWarp
- `ComponentMask` `WPos_Z` (B only) : input ← WorldPosition
- `Add` `Z_Warped` : A ← WPos_Z, B ← Warp_Amount

###### Étape 2 — Génération des bandes via sin

- `Divide` `Phase` : A ← Z_Warped, B ← In_Period
- `Multiply` `Phase_2Pi` : A ← Phase, B ← Constant 6.2832 (= 2π)
- `Sine` `Bands_Raw` : input ← Phase_2Pi (output range [-1, 1])
- `Multiply` `Bands_Half` : A ← Bands_Raw, B ← Constant 0.5
- `Add` `Bands_0_1` : A ← Bands_Half, B ← Constant 0.5 (output range [0, 1])
- `Power` `Bands_Sharp` : Base ← Bands_0_1, Exp ← Constant 3.0 (concentre l'énergie aux pics → bandes étroites foncées)
- **Out_StrataMask** ← Bands_Sharp

###### Étape 3 — Application darken + rust tint

- `Multiply` `Darken_Amount` : A ← Bands_Sharp, B ← In_DarkenAmount
- `OneMinus` `Darken_Factor` : input ← Darken_Amount (= 1 - darken)
- `Multiply` `Rust_Alpha` : A ← Darken_Amount, B ← In_RustStrength
- `Lerp` `Rust_Tinted` : A ← In_BC, B ← In_RustColor, Alpha ← Rust_Alpha
- `Multiply` `BC_Darkened` : A ← Rust_Tinted, B ← Darken_Factor
- **Out_BC** ← BC_Darkened

> **Pourquoi sin + Power** : le sin pur donne des bandes lisses de 50% d'amplitude. Power(3) écrase les zones claires et garde les pics → bandes étroites foncées plus géologiques (un strate = une couche fine).

> **Effet visuel** : bandes horizontales sombres répétées tous les `In_Period` cm, ondulées par le warp noise. Augmente `In_NoiseWarp` si trop régulier.

---

#### D. Fracture mask (MF_FractureMask) — optionnel

##### D.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_FractureMask` |
| **Location** | `/Game/Sub3D/MF/MF_FractureMask` |
| **Output type** | 2 pins (BC_with_fractures, FractureMask) |
| **Description** | Lignes sombres en réseau (cracks géologiques) via Voronoi3D edges. Lit WorldPos. |

##### D.1 Function Inputs

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BC` | Vec3 | — | BC entrant (sortie StrataBanding) |
| `In_Scale` | Scalar | 0.0008 | Fréquence Voronoi (1 / espacement cells en cm) |
| `In_Intensity` | Scalar | 0.6 | Profondeur du darkening (0 = invisible, 1 = noir total) |
| `In_EdgeWidthMin` | Scalar | 0.02 | Début transition edge (smoothstep min) |
| `In_EdgeWidthMax` | Scalar | 0.06 | Fin transition edge (smoothstep max) — différence = largeur visible |

##### D.2 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | BC avec fractures appliquées |
| `Out_FractureMask` | Scalar | 0 ailleurs, 1 sur les fractures |

##### D.3 Étapes de création

###### Étape 1 — Voronoi 3D

- `WorldPosition` — Absolute
- `Multiply` `Voronoi_UV` : A ← WorldPosition, B ← In_Scale
- `Noise` `Voronoi_Noise` — set Function = "Voronoi", Quality=2 : Position ← Voronoi_UV
  - Output range [0, 1] = distance au centre de la cell la plus proche, faible aux centres, 1 aux edges

###### Étape 2 — Extraction des edges

- `SmoothStep` `Edge_Falloff` : Min ← In_EdgeWidthMin, Max ← In_EdgeWidthMax, Value ← Voronoi_Noise
- `OneMinus` `Fracture_Lines` : input ← Edge_Falloff (inverse : 1 = edge, 0 = inside cell)
- **Out_FractureMask** ← Fracture_Lines

###### Étape 3 — Application darken

- `Multiply` `Darken` : A ← Fracture_Lines, B ← In_Intensity
- `OneMinus` `Darken_Factor` : input ← Darken
- `Multiply` `BC_Fractured` : A ← In_BC, B ← Darken_Factor
- **Out_BC** ← BC_Fractured

> **Tuning** : pour des fractures plus larges, augmente `In_EdgeWidthMax` (ex: 0.10). Pour fractures plus rares, augmente `In_EdgeWidthMin` (ex: 0.04).

---

#### E. Final color

Combine les sorties précédentes vers le pin Base Color du Result :

- `Out_BC` du MF_FractureMask **→ Base Color** (via MF_StylizePBR si stylization active, voir step F-bis)

---

#### F. Final roughness

Mini sous-graph dans le master M_CaveAbyssal (pas une MF dédiée — trop petit) :

- `Lerp` `R_Lerped` : A ← In_RoughnessMin (param), B ← In_RoughnessMax (param), Alpha ← Out_Roughness de MF_Triplanar_PBR
- `Add` `R_Sediment_Boost` : A ← R_Lerped, B ← Constant 0.1
- `Lerp` `R_With_Sediment` : A ← R_Lerped, B ← R_Sediment_Boost, Alpha ← Out_SedimentMask de MF_SedimentOverlay
- `Saturate` `R_Final` : input ← R_With_Sediment
- → **Roughness pin** (via MF_StylizePBR si stylization active)

> **Pourquoi le boost +0.1 en sediment** : le silt/sable est plus diffus que la roche → roughness légèrement plus haute. Subtil mais ajoute à la lisibilité.

---

#### F-bis. Stylization (MF_StylizePBR) — voir [stylization annexe](cave_abyssal_stylization.md)

Avant les pins finaux, **si stylized DA active** (Static Switch `EnableStylization`), passe BC/N/R à travers `MF_StylizePBR` qui posterize + remap palette LUT + flatten normal + edge enhance + roughness clamp. Le biolum (step G) reste hors stylization pour garder son HDR pur.

```
BC_with_fractures, N_combined, R_combined
        ↓
[MF_StylizePBR] (params : LUT, per-step strengths, voir refactor v2)
        ↓
BC_stylized, N_stylized, R_stylized → pins
```

Détails complets : [cave_abyssal_stylization.md](cave_abyssal_stylization.md) §14.2.

---

#### G. Biolum emissive (MF_BiolumSpots)

##### G.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_BiolumSpots` |
| **Location** | `/Game/Sub3D/MF/MF_BiolumSpots` |
| **Output type** | 2 pins (Emissive_Final, BiolumMask) |
| **Description** | Tâches biolum teal HDR via Voronoi3D + density mask + pulse asynchrone. Lit WorldPos donc traverse PMC + rocks PCG cohérent. |

##### G.1 Function Inputs

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_Color` | Vec3 | (0.026, 0.546, 0.444) linear | Teal `#2DC4B0` (palette v2 DA-aligned, vrai linear RGB) |
| `In_Intensity` | Scalar | 3.0 | Multiplicateur HDR (peut aller jusqu'à 10) |
| `In_Density` | Scalar | 0.06 | Fraction de surface occupée par biolum (0–0.3) |
| `In_NoiseScale` | Scalar | 0.0005 | Fréquence Voronoi (espacement des spots) |
| `In_DensityNoiseScale` | Scalar | 0.001 | Fréquence du mask de regroupement (clusters) |
| `In_EnablePulse` | StaticBool | true | Active pulse asynchrone (coûte 1 sin + Time) |
| `In_PulseSpeed` | Scalar | 1.5 | Hz du pulse |
| `In_EmissiveAmbient` | Vec3 | (0.005, 0.012, 0.018) | Émission ambiante très subtile (visible en blackout total) |

##### G.2 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_Emissive` | Vec3 | HDR pour pin Emissive du master |
| `Out_BiolumMask` | Scalar | 0 ailleurs, 1 sur les spots (pour usage aval, ex bloom mask) |

##### G.3 Étapes de création

###### Étape 1 — Voronoi 3D pour spots de base

- `WorldPosition` — Absolute
- `Multiply` `Voronoi_UV` : A ← WorldPosition, B ← In_NoiseScale
- `Noise` `Voronoi_Cells` — set Function = "Voronoi", Quality=2, Output Min = 0, Output Max = 1 : Position ← Voronoi_UV
- `OneMinus` `Voronoi_Inv` : input ← Voronoi_Cells (= 1 = cell center, 0 = edge)
- `SmoothStep` `Spots_Raw` : Min ← Constant 0.6, Max ← Constant 0.8, Value ← Voronoi_Inv

> Spots concentrés au cœur des cells Voronoi, transition douce. Range typique : 5–10% de la surface devient bright.

###### Étape 2 — Density mask pour grouper en clusters

- `Multiply` `Density_UV` : A ← WorldPosition, B ← In_DensityNoiseScale
- `Noise` `Density_Noise` — set Function = "Simplex", Output Min = 0, Output Max = 1 : Position ← Density_UV
- `OneMinus` `Density_Threshold` : input ← In_Density (= 1 - density)
- `Step` `Density_Mask` : Y (threshold) ← Density_Threshold, X (value) ← Density_Noise (output 1 si X > Y)
- `Multiply` `Biolum_Mask` : A ← Spots_Raw, B ← Density_Mask
- **Out_BiolumMask** ← Biolum_Mask

###### Étape 3 — Pulse asynchrone (conditionnel via Static Switch)

- `Time` (catégorie Constants)
- `Multiply` `Time_Speed` : A ← Time, B ← In_PulseSpeed
- `Multiply` `Cell_Offset` : A ← Voronoi_Cells, B ← Constant 6.2832 (déphasage par cell — pulses indépendants)
- `Add` `Pulse_Phase` : A ← Time_Speed, B ← Cell_Offset
- `Sine` `Pulse_Sin` : input ← Pulse_Phase (range [-1, 1])
- `Multiply` `Pulse_Half` : A ← Pulse_Sin, B ← Constant 0.3
- `Add` `Pulse_Final` : A ← Pulse_Half, B ← Constant 0.7 (range [0.4, 1.0])
- `StaticSwitch` `Pulse_Gate` : Value ← In_EnablePulse, True ← Pulse_Final, False ← Constant 1.0

###### Étape 4 — Combinaison finale HDR

- `Multiply` `Color_Times_Intensity` : A ← In_Color, B ← In_Intensity
- `Multiply` `Mask_Times_Pulse` : A ← Biolum_Mask, B ← Pulse_Gate
- `Multiply` `Biolum_HDR` : A ← Color_Times_Intensity, B ← Mask_Times_Pulse
- `Add` `Emissive_With_Ambient` : A ← Biolum_HDR, B ← In_EmissiveAmbient
- **Out_Emissive** ← Emissive_With_Ambient

> **Coût perf** : Voronoi3D ~30 instructions + Sine ~5 instructions = ~35-40 instructions par pixel. Si chute FPS sur GTX 1660, set `In_EnablePulse = false` (économise 8 instructions) et/ou remplace Voronoi par Simplex 3D thresholded (moins joli mais ~15 instructions de moins).

> **Effet visuel** : taches teal HDR éparpillées (~6% surface), regroupées en clusters par le density mask, chaque cluster pulse à un rythme légèrement différent. Reproduit observation des concepts cave (`flare illumination`, `cave handpaint`).

---

#### H. Wet surface (MF_WetSurface) — optionnel

##### H.0 Metadata

| Champ | Valeur |
|---|---|
| **Nom** | `MF_WetSurface` |
| **Location** | `/Game/Sub3D/MF/MF_WetSurface` |
| **Output type** | 2 pins (BC_Wet, R_Wet) |
| **Description** | Assombrit légèrement la BC et réduit la roughness pour simuler une surface mouillée. Peut être appliqué après StrataBanding+Fracture, avant StylizePBR. |

##### H.1 Function Inputs

| Nom | Type | Default | Description |
|---|---|---|---|
| `In_BC` | Vec3 | — | BC entrant |
| `In_Roughness` | Scalar | — | Roughness entrant |
| `In_Wetness` | Scalar | 0.3 | 0=sec, 1=trempé |
| `In_DarkenAmount` | Scalar | 0.15 | Force d'assombrissement BC |
| `In_RoughnessReduce` | Scalar | 0.25 | Force de réduction roughness |

##### H.2 Outputs

| Nom | Type | Description |
|---|---|---|
| `Out_BC` | Vec3 | BC plus sombre |
| `Out_Roughness` | Scalar | Roughness plus basse |

##### H.3 Étapes de création

###### Étape 1 — Darken BC

- `Multiply` `Darken_Factor` : A ← In_Wetness, B ← In_DarkenAmount
- `OneMinus` `BC_Mult` : input ← Darken_Factor
- `Multiply` `BC_Wet` : A ← In_BC, B ← BC_Mult
- **Out_BC** ← BC_Wet

###### Étape 2 — Reduce roughness

- `Multiply` `R_Reduce` : A ← In_Wetness, B ← In_RoughnessReduce
- `Subtract` `R_Wet` : A ← In_Roughness, B ← R_Reduce
- `Saturate` `R_Final` : input ← R_Wet
- **Out_Roughness** ← R_Final

> **Usage in master** : applique entre MF_FractureMask (BC sortie) et MF_StylizePBR (entrée BC). Le wetness peut être un Scalar Param global du master pour ambiance "fraîchement immergé" vs "stable".

> **Note** : pour effet plus réaliste, ajouter aussi un boost de Specular ou une SSR layer. Hors scope MF basique.

---

## Cross-refs

- **Spec parent** : [2026-05-17_cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md) (vision, architecture overview, §4 rocks, §5/6/7 atmosphère, §7-bis tokens, §8 tracker)
- **Master graph annexe** : [cave_abyssal_master_graph.md](cave_abyssal_master_graph.md) (§3.1/3.2/3.3 + §3.6 Group A→L + §3.7 test + §3.8 LUT procedure)
- **Stylization annexe** : [cave_abyssal_stylization.md](cave_abyssal_stylization.md) (§14 Track B Stylized DA + MF_StylizePBR refactor v2)
