# Cave Abyssal — Master Graph & Test Workflow (annexe)

| | |
|---|---|
| **Date** | 2026-05-17 (split du main spec 2026-05-18) |
| **Statut** | Annexe — spec master M_CaveAbyssal + workflow test + LUT procedure |
| **Doc parent** | [cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md) |
| **Scope** | §3.1/3.2/3.3 (settings/pins/params) + §3.5 Performance + §3.6 Group A→L master graph + §3.7 Workflow test + §3.8 LUT procedure |

---

## 3.1 Material settings (Details panel)

| Param | Valeur | Note |
|---|---|---|
| Material Domain | Surface | |
| Blend Mode | Opaque | |
| Shading Model | Default Lit | Pas SubSurface (pas besoin) |
| Two Sided | **False** | |
| Used with Procedural Meshes | **True** | obligatoire pour PMC |
| Tangent Space Normal | True | |
| Allow Negative Emissive | False | |

---

## 3.2 Pins branchés

```
Base Color  ← FinalColor (étape 3.6 Group L)
Normal      ← FinalNormal (étape 3.6 Group J)
Metallic    ← 0
Specular    ← 0.5
Roughness   ← FinalRoughness (étape 3.6 Group I)
Emissive    ← FinalEmissive (étape 3.6 Group K)
Ambient Occlusion ← AO sortie triplanar (si dispo)
```

---

## 3.3 Parameters exposés (pour MI)

| Param | Type | Default | Range | Rôle |
|---|---|---|---|---|
| `T_Rock_BC` | Texture2D | T_Rock_Basalt_BC | — | Base color rock |
| `T_Rock_N` | Texture2D | T_Rock_Basalt_N | — | Normal rock |
| `T_Rock_R` | Texture2D | T_Rock_Basalt_R | — | Roughness rock |
| `T_Sediment_BC` | Texture2D | T_Sediment_Silt_BC | — | Base color sediment |
| `T_Sediment_N` | Texture2D | T_Sediment_Silt_N | — | Normal sediment |
| `WorldTilingScale` | Scalar | 0.001 | 0.0001–0.01 | 1 / tiling distance en cm |
| `TriplanarBlendSharpness` | Scalar | 4.0 | 1–16 | dureté des transitions triplanar |
| `RockTint` | Color | TKN_ROCK_TINT_DEFAULT linear (0.216, 0.262, 0.319) | — | teinte rock (cf §7-bis tokens) |
| `SedimentTint` | Color | TKN_SEDIMENT_TINT_DEFAULT linear (0.133, 0.120, 0.093) | — | teinte sédiment |
| `StrataPeriod` | Scalar | 800 | 200–4000 | hauteur d'une bande (cm) |
| `StrataDarkenAmount` | Scalar | 0.35 | 0–1 | profondeur des bandes |
| `StrataNoiseWarp` | Scalar | 200 | 0–1000 | déformation noise des bandes |
| `StrataBandColor` | VectorParameter | TKN_STRATA_BAND_INDIGO linear (0.010, 0.014, 0.035) | — | couleur des bandes strates (cf §3.6.6-tris) |
| `StrataBandStrength` | Scalar | 0.3 | 0-1 | force tint strata (cf §3.6.6-tris) |
| `BiolumIntensity` | Scalar | 3.0 | 0–10 | multiplicateur HDR du biolum |
| `BiolumColor` | Color | TKN_BIOLUM_TEAL_MAIN linear (0.026, 0.546, 0.444) | — | teal biolum |
| `BiolumDensity` | Scalar | 0.06 | 0–0.3 | fraction de surface biolum |
| `BiolumNoiseScale` | Scalar | 0.0005 | 0.0001–0.005 | fréquence Voronoi |
| `SedimentUpThreshold` | Scalar | 0.55 | 0–1 | dot(N, up) > seuil → sediment |
| `SedimentBlendSharpness` | Scalar | 8.0 | 1–32 | dureté transition rock→sediment |
| `RoughnessMin` | Scalar | 0.55 | 0–1 | min wet rock |
| `RoughnessMax` | Scalar | 0.85 | 0–1 | max dry highlight |
| `EmissiveAmbient` | Color | (0.005, 0.012, 0.018) | — | très subtile émission cave |
| `FractureIntensity` | Scalar | 0.6 | 0–1 | profondeur des cracks |
| `FractureScale` | Scalar | 0.0008 | — | fréquence des cracks |
| `PosterizeStrength` | Scalar | 0.0 | 0-1 | Stylization step (cf §14.2 refactor v2) |
| `LUTStrength` | Scalar | 0.05 | 0-1 | Stylization step LUT remap |
| `NormalFlatStrength` | Scalar | 0.0 | 0-1 | Stylization step flatten normals |
| `EdgeIntensity` | Scalar | 0.6 | 0-1.5 | Stylization step edge enhance |
| `RoughnessMatteStrength` | Scalar | 0.0 | 0-1 | Stylization step roughness clamp |
| `BrushOverlayStrength` | Scalar | 0.2 | 0-1 | Stylization step brush overlay |
| `EnableStylization` | StaticSwitch | true | — | bypass total stylization si false |
| `T_PaletteLUT` | Texture2D | T_AbyssalPalette_LUT | — | LUT remap palette (cf §3.8 + tool HTML) |
| `T_BrushTexture` | Texture2D | T_BrushStrokes_Grunge | — | Brush overlay grunge |

---

## 3.5 Performance

- **Triplanar = 3× le coût des samples**. Garde les textures en 1024² max pour rock/sediment, c'est suffisant en triplanar (le détail vient des 3 projections croisées).
- **Voronoi3D coûte ~30 instructions**. Si tu vois des chutes de FPS sur GTX 1660, désactive le pulse + simplifie biolum en Perlin3D thresholded.
- **Static Switch Parameters** pour activer/désactiver les MF coûteuses (Fracture, BiolumPulse). Permet 3-4 niveaux de qualité.

---

## 3.6 M_CaveAbyssal — Graph du master material

Cette section décrit le câblage de **toutes les MFs ensemble** dans le master `M_CaveAbyssal`. Une fois fait, tu peux assigner le material sur n'importe quel mesh et voir le résultat dans l'éditeur ou en jeu.

### 3.6.0 Material settings (rappel)

Crée `M_CaveAbyssal` dans `/Game/Sub3D/Materials/Cave/`. Settings du Material Result : voir §3.1 ci-dessus.

### 3.6.1 Vue d'ensemble du graph

```
┌───────────────────────────────────────────────────────────────────┐
│  Group A : Paramètres (textures + scalars du master)              │
└───────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────┐        ┌──────────────────────┐
│ Group B :            │        │ Group C :            │
│ Rock Triplanar       │        │ Sediment Triplanar   │
│ (MF call #1)         │        │ (MF call #2)         │
└──────────────────────┘        └──────────────────────┘
        BC_rock, N_rock, R_rock         BC_sed, N_sed
                              ↓
┌───────────────────────────────────────────────────────────────────┐
│  Group D : Sediment Overlay blend (MF call #3)                    │
└───────────────────────────────────────────────────────────────────┘
        BC_blended, N_blended, R_blended, SedMask
                              ↓
┌───────────────────────────────────────────────────────────────────┐
│  Group E : Strata Banding (MF call #4)                            │
│  Group F : Fracture Mask (MF call #5)                             │
│  Group G : Wet Surface optionnel (MF call #6)                     │
└───────────────────────────────────────────────────────────────────┘
        BC_detailed, R_detailed
                              ↓
┌───────────────────────────────────────────────────────────────────┐
│  Group H : Stylization optionnelle (MF call #7 via StaticSwitch)  │
└───────────────────────────────────────────────────────────────────┘
        BC_final, N_final_WS, R_final
                              ↓
┌──────────────────────┐  ┌──────────────────────┐  ┌────────────┐
│  Group I :           │  │  Group J :           │  │  Group K : │
│  BC + Roughness pins │  │  Normal W→T + pin    │  │  Biolum    │
└──────────────────────┘  └──────────────────────┘  │  branch    │
                                                    │  (Emissive)│
                                                    └────────────┘
                              ↓
                         Material Result
                       (BC + N + R + Emissive)
```

### 3.6.2 Group A — Paramètres du master

Crée ces nodes en haut-gauche du graph. Tous sont des **paramètres exposés** (visibles dans les MIs).

#### Textures (5 paramètres)

| Node | Type UE | Set | Default |
|---|---|---|---|
| `T_Rock_BC` | `TextureObjectParameter` | Texture = AP2_Rocks_BaseColor (ou ce que tu veux) | — |
| `T_Rock_N` | `TextureObjectParameter` | Texture = AP2_Rocks_Normal_DX | — |
| `T_Rock_R` | `TextureObjectParameter` | Texture = AP2_Rocks_Roughness | — |
| `T_Sediment_BC` | `TextureObjectParameter` | Texture = (à sourcer plus tard, peut être nulle au début) | — |
| `T_Sediment_N` | `TextureObjectParameter` | Texture = (idem) | — |

> **`TextureObjectParameter`** (pas `TextureSampleParameter2D`) — c'est important. La version "Object" renvoie juste la **référence texture**, pas un sample. C'est ce que MF_Triplanar_PBR attend via ses Function Inputs `In_BaseColor` (qui sont de type Texture2DObject).

#### Scalars (11 paramètres)

Tous sont des `ScalarParameter`. Groupe-les visuellement en bas-gauche.

| Node | Default | Range | Groupe (Details) |
|---|---|---|---|
| `WorldTilingScale` | 0.001 | 0.0001–0.01 | "01 Triplanar" |
| `TriplanarBlendSharpness` | 4.0 | 1–16 | "01 Triplanar" |
| `SedimentUpThreshold` | 0.55 | 0–1 | "02 Sediment" |
| `SedimentBlendSharpness` | 8.0 | 1–32 | "02 Sediment" |
| `StrataPeriod` | 800 | 200–4000 | "03 Strata" |
| `StrataDarkenAmount` | 0.35 | 0–1 | "03 Strata" |
| `FractureIntensity` | 0.6 | 0–1 | "04 Fracture" |
| `FractureScale` | 0.0008 | — | "04 Fracture" |
| `Wetness` | 0.3 | 0–1 | "05 Wet" |
| `RoughnessMin` | 0.55 | 0–1 | "06 Roughness" |
| `RoughnessMax` | 0.85 | 0–1 | "06 Roughness" |

#### Color params (3 paramètres)

| Node | Type | Default | Groupe |
|---|---|---|---|
| `RockTint` | `VectorParameter` | TKN_ROCK_TINT_DEFAULT (0.216, 0.262, 0.319) linear | "02 Sediment" |
| `SedimentTint` | `VectorParameter` | TKN_SEDIMENT_TINT_DEFAULT (0.133, 0.120, 0.093) linear | "02 Sediment" |
| `BiolumColor` | `VectorParameter` | TKN_BIOLUM_TEAL_MAIN (0.026, 0.546, 0.444) linear | "07 Biolum" |

#### Biolum scalars (3 paramètres)

| Node | Default | Range | Groupe |
|---|---|---|---|
| `BiolumIntensity` | 3.0 | 0–10 | "07 Biolum" |
| `BiolumDensity` | 0.06 | 0–0.3 | "07 Biolum" |
| `BiolumNoiseScale` | 0.0005 | 0.0001–0.005 | "07 Biolum" |

#### Static switches (1 paramètre)

| Node | Type | Default |
|---|---|---|
| `EnableStylization` | `StaticBoolParameter` | true |

> **Tip** : utilise le champ "Group" du Details panel pour ranger les paramètres visuellement dans les MIs (catégories collapsibles).

### 3.6.3 Group B — Rock Triplanar (1er MF call)

- `MaterialFunctionCall` `MF_Triplanar_Rock` — Function = `MF_Triplanar_PBR`
  - In_BaseColor ← T_Rock_BC, In_Normal ← T_Rock_N, In_Roughness ← T_Rock_R
  - In_TilingScale ← WorldTilingScale, In_BlendSharpness ← TriplanarBlendSharpness
- Out → Out_BC, Out_NormalWS, Out_Roughness (utilisés Groups C/D/I)

### 3.6.4 Group C — Sediment Triplanar (2e MF call)

Mêmes scalars que Group B (partage tiling/sharpness pour éviter cassure visuelle aux transitions).

- `MaterialFunctionCall` `MF_Triplanar_Sediment` — Function = `MF_Triplanar_PBR`
  - In_BaseColor ← T_Sediment_BC, In_Normal ← T_Sediment_N, In_Roughness ← T_Rock_R (réutilisé)
  - In_TilingScale ← WorldTilingScale, In_BlendSharpness ← TriplanarBlendSharpness
- Out → Out_BC, Out_NormalWS

### 3.6.5 Group D — Sediment Overlay blend (3e MF call)

- `MaterialFunctionCall` `MF_SedimentBlend` — Function = `MF_SedimentOverlay`
  - In_BC_Rock ← MF_Triplanar_Rock.Out_BC, In_N_Rock_WS ← MF_Triplanar_Rock.Out_NormalWS
  - In_BC_Sediment ← MF_Triplanar_Sediment.Out_BC, In_N_Sediment_WS ← MF_Triplanar_Sediment.Out_NormalWS
  - In_RockTint ← RockTint, In_SedimentTint ← SedimentTint
  - In_UpThreshold ← SedimentUpThreshold, In_BlendSharpness ← SedimentBlendSharpness
- Out → Out_BC, Out_NormalWS, Out_SedimentMask (mask utilisé Group I pour roughness boost)

### 3.6.6 Group E — Strata Banding (4e MF call)

- `MaterialFunctionCall` `MF_StrataAdd` — Function = `MF_StrataBanding`
  - In_BC ← MF_SedimentBlend.Out_BC
  - In_Period ← StrataPeriod, In_DarkenAmount ← StrataDarkenAmount
  - In_NoiseWarp ← Constant 200, In_NoiseScale ← Constant 0.0005
  - In_RustColor ← `StrataBandColor` (VectorParameter, voir 3.6.6-tris), In_RustStrength ← `StrataBandStrength` (ScalarParameter)
- Out → Out_BC, Out_StrataMask

### 3.6.6-tris Group E — exposition couleur strates

> Décision 2026-05-17 : `In_RustColor` et `In_RustStrength` exposés comme params master pour tweak depuis MI.

**Au lieu de** :
- `Constant3Vector (0.43, 0.25, 0.14)` → `MF_StrataAdd.In_RustColor`
- `Constant 0.3` → `MF_StrataAdd.In_RustStrength`

**Utilise** :
- `VectorParameter` `StrataBandColor` (default TKN_STRATA_BAND_INDIGO linear, group "04 Strata") → `MF_StrataAdd.In_RustColor`
- `ScalarParameter` `StrataBandStrength` (default 0.3, range 0-1, group "04 Strata") → `MF_StrataAdd.In_RustStrength`

→ Tu peux maintenant tweaker la couleur des strates depuis MI sans modifier le master. Utile pour adapter par biome (rust dans Coastal warm, indigo dans Abyssal froid).

### 3.6.6-bis Group E-prep — Exclusion strata sur sediment

> Décision 2026-05-17 : ne pas faire apparaître le darken/rust strata sur les zones sediment (silt sableux n'a pas de bandes géologiques visibles).

Sub-graph à ajouter APRÈS MF_StrataAdd, AVANT MF_FractureAdd dans la chaîne BC :

- `Lerp` `Strata_Masked` :
  - A ← MF_SedimentBlend.Out_BC (BC pré-strata, sediment dominant)
  - B ← MF_StrataAdd.Out_BC (BC post-strata, rock avec bandes)
  - Alpha ← `SedMask_Inv` (réutilise le node créé pour biolum §3.6.11-bis, ou crée un `OneMinus` sur `MF_SedimentBlend.Out_SedimentMask`)
- La chaîne BC continue depuis `Strata_Masked.output`

**Effet** : strates darkening/rust visibles uniquement sur les zones rock. Sur sediment, BC reste celui du sediment blend pré-strata.

> Pattern réutilisable : si tu ajoutes plus tard d'autres effets "rock-only", suis le même pattern Lerp(pre, post, SedMask_Inv).

### 3.6.7 Group F — Fracture Mask (5e MF call, optionnel)

- `MaterialFunctionCall` `MF_FractureAdd` — Function = `MF_FractureMask`
  - In_BC ← MF_StrataAdd.Out_BC
  - In_Scale ← FractureScale, In_Intensity ← FractureIntensity
  - In_EdgeWidthMin ← Constant 0.02, In_EdgeWidthMax ← Constant 0.06
- Out → Out_BC

### 3.6.8 Group G — Wet Surface (6e MF call, optionnel)

- `MaterialFunctionCall` `MF_WetApply` — Function = `MF_WetSurface`
  - In_BC ← MF_FractureAdd.Out_BC
  - In_Roughness ← MF_Triplanar_Rock.Out_Roughness
  - In_Wetness ← Wetness
  - In_DarkenAmount ← Constant 0.15, In_RoughnessReduce ← Constant 0.25
- Out → Out_BC, Out_Roughness

### 3.6.9 Group H — Stylization (7e MF call, gated)

> ⚠️ **Refactor 2026-05-17 v2** : `StylizationStrength` master retiré, remplacé par **5 strengths per-step indépendants**. Voir [cave_abyssal_stylization.md](cave_abyssal_stylization.md) §14.2 refactored.

**Paramètres master à exposer** (Group "14 Stylization") :

| Param | Type | Default | Range | Rôle |
|---|---|---|---|---|
| `PosterizeStrength` | ScalarParameter | 0.0 | 0-1 | Force quantization |
| `LUTStrength` | ScalarParameter | 0.05 | 0-1 | Force LUT palette remap (default faible) |
| `NormalFlatStrength` | ScalarParameter | 0.0 | 0-1 | Force flattening normals |
| `EdgeIntensity` | ScalarParameter | 0.6 | 0-1.5 | Force ink lines bordures |
| `RoughnessMatteStrength` | ScalarParameter | 0.0 | 0-1 | Force clamp matte roughness |
| `BrushOverlayStrength` | ScalarParameter | 0.2 | 0-1 | Force overlay brush texture |
| `T_PaletteLUT` | TextureObjectParameter | T_AbyssalPalette_LUT | — | LUT remap |
| `T_BrushTexture` | TextureObjectParameter | T_BrushStrokes_Grunge | — | Brush overlay |

**Câblage MF call** :

- `MaterialFunctionCall` `MF_StylizeApply` — Function = `MF_StylizePBR`
  - In_BC ← MF_WetApply.Out_BC
  - In_NormalWS ← MF_SedimentBlend.Out_NormalWS
  - In_Roughness ← MF_WetApply.Out_Roughness
  - In_PaletteLUT ← T_PaletteLUT (param)
  - In_BrushTexture ← T_BrushTexture (param)
  - In_PosterizeStrength ← PosterizeStrength
  - In_LUTStrength ← LUTStrength
  - In_NormalFlatStrength ← NormalFlatStrength
  - In_EdgeIntensity ← EdgeIntensity
  - In_RoughnessMatteStrength ← RoughnessMatteStrength
  - In_BrushStrength ← BrushOverlayStrength
- Out → Out_BC, Out_NormalWS, Out_Roughness

**Gates static switch (optionnel)** — bypass complet stylization quand `EnableStylization = false` :

- `StaticSwitch` `Gate_BC` : Value ← EnableStylization, True ← MF_StylizeApply.Out_BC, False ← MF_WetApply.Out_BC
- `StaticSwitch` `Gate_N`  : Value ← EnableStylization, True ← MF_StylizeApply.Out_NormalWS, False ← MF_SedimentBlend.Out_NormalWS
- `StaticSwitch` `Gate_R`  : Value ← EnableStylization, True ← MF_StylizeApply.Out_Roughness, False ← MF_WetApply.Out_Roughness

### 3.6.10 Group I — Roughness finale + pin Roughness

Mini sous-graph pour ajuster la roughness :

| # | Node | Set | In | Out → |
|---|---|---|---|---|
| 1 | `Lerp` `R_Range` | — | A ← RoughnessMin, B ← RoughnessMax, Alpha ← R_final (Gate_R output) | → #2 |
| 2 | `Add` `R_Boost` | — | A ← R_Range, B ← Constant 0.1 | → #3 |
| 3 | `Lerp` `R_With_Sed` | — | A ← R_Range, B ← R_Boost, Alpha ← MF_SedimentBlend.Out_SedimentMask | → #4 |
| 4 | `Saturate` `R_Out` | — | input ← R_With_Sed | → **Roughness pin (Material Result)** |

### 3.6.11 Group J — Normal World→Tangent + pin Normal

UE5 attend une normale en **tangent space** sur le pin Normal (Material Result). Nos MFs produisent du world space. Conversion :

**`TransformVector` — name `N_W_to_T`**
- Set: `Source` = World Space, `Destination` = Tangent Space
- In: `Input` ← N_final_WS (Gate_N output, ou directement MF_SedimentBlend.Out_NormalWS si pas de stylization)
- Out → **Normal pin (Material Result)**

### 3.6.11-bis Group K-prep — Exclusion biolum sur sediment ET strates

> Décision 2026-05-17 : ne pas faire apparaître les spots biolum sur les zones sediment (silt au sol) ni dans les bandes strata. Les biolum sont des cristaux qui poussent sur rock dur exposé.

Sub-graph à ajouter avant le pin Emissive (entre MF_BiolumApply et le pin) :

- `OneMinus` `SedMask_Inv` : input ← `MF_SedimentBlend.Out_SedimentMask`
- `Multiply` `Biolum_NoSed` : A ← `MF_BiolumApply.Out_Emissive`, B ← `SedMask_Inv`
- `OneMinus` `StrataMask_Inv` : input ← `MF_StrataAdd.Out_StrataMask`
- `Multiply` `Biolum_NoSedNoStrata` : A ← `Biolum_NoSed.output`, B ← `StrataMask_Inv`
- **Pin Emissive Color** ← `Biolum_NoSedNoStrata.output`

**Effet** : biolum visible uniquement sur rock pur (entre strates, hors sediment).

### 3.6.12 Group K — Biolum branch + Emissive pin

Branche **indépendante** des Groups B→I. Lit aussi WorldPosition (cohérent avec rock pipeline).

**`MaterialFunctionCall` — name `MF_BiolumApply`**
- Set: `MaterialFunction` = `MF_BiolumSpots`
- In:
  - `In_Color` ← BiolumColor (param)
  - `In_Intensity` ← BiolumIntensity (param)
  - `In_Density` ← BiolumDensity (param)
  - `In_NoiseScale` ← BiolumNoiseScale (param)
  - `In_DensityNoiseScale` ← Constant 0.001
  - `In_EnablePulse` ← StaticBoolParameter `EnableBiolumPulse` (default true)
  - `In_PulseSpeed` ← Constant 1.5
  - `In_EmissiveAmbient` ← Constant3Vector (0.005, 0.012, 0.018)
- Out → `Out_Emissive` → utilisé par Group K-prep avant pin Emissive

### 3.6.13 Group L — Pin Base Color (final)

**Connect** : `Gate_BC` output → **Base Color pin (Material Result)**

(Pas de node intermédiaire — connexion directe.)

### 3.6.14 Connection summary (vérification rapide)

| Pin Material Result | Vient de |
|---|---|
| **Base Color** | Group H — `Gate_BC` output |
| **Normal** | Group J — `N_W_to_T` (TransformVector) output |
| **Roughness** | Group I — `R_Out` (Saturate) output |
| **Emissive Color** | Group K-prep — `Biolum_NoSedNoStrata` output |
| **Metallic** | Constant 0 |
| **Specular** | Constant 0.5 (défaut UE) ou expose `SpecParam` |

### 3.6.15 Save + Apply

1. **Save** le master (Ctrl+S)
2. UE va compiler le shader — peut prendre 30-90 secondes la première fois (beaucoup de MFs imbriqués)
3. Vérifie l'onglet **Stats** en haut à droite du Material Editor :
   - Instructions count : viser <200 (acceptable jusqu'à 400 sur GTX 1660)
   - Texture samples : 18 (9 rock + 9 sediment) — élevé, normal pour triplanar
   - Warnings : doit être vide

Si tu vois "Texture sample limit reached" → tu dépasses 16 samples sur certains shading models. Solution : passer le sediment en sub-MF qui n'est appelée que si SedimentMask > 0 (lazy eval), ou réduire les triplanar à 1-2 projections.

---

## 3.7 Workflow de test

Le problème : les MFs ne montrent rien en preview (juste une sphère grise par défaut). Pour vraiment voir le résultat tu as besoin :

### 3.7.1 Créer MI_CaveAbyssal_Test

1. Click-droit sur `M_CaveAbyssal` dans Content Browser → "Create Material Instance"
2. Renomme en `MI_CaveAbyssal_Test`
3. Ouvre `MI_CaveAbyssal_Test`
4. Override les paramètres :
   - **T_Rock_BC** : cocher la case ☐, assigner `AP2_Rocks_BaseColor`
   - **T_Rock_N** : cocher ☐, assigner `AP2_Rocks_Normal_DX`
   - **T_Rock_R** : cocher ☐, assigner `AP2_Rocks_Roughness`
   - **T_Sediment_BC, T_Sediment_N** : peuvent rester nulls pour le moment (le sediment ne fera rien faute de texture, mais ça n'arrêtera pas le shader)

### 3.7.2 Créer une map de test

1. File → New Level → Empty Level
2. Save As → `/Game/Maps/Test/L_CaveMatTest`
3. Ajoute :
   - Une `BP_Sky_Sphere` désactivée (delete-la, on est en abysse)
   - Une **PostProcessVolume** unbound avec settings canonical (cf [canonical lighting/PP/fog](2026-05-18_lighting_pp_fog_canonical_spec.md) §8.1)
   - 1 **PointLight** : Intensity 5000, Range 1500, posée à 200cm au-dessus
   - 1 **Sphere** (Geometry) ou un mesh rock importé, assigne lui le `MI_CaveAbyssal_Test`
4. Sauvegarde la map

> **Mieux** : utilise le **ViewportHeadlamp C++ tool** (menu Window > Sub3D > Toggle Viewport Headlamp) qui crée une SpotLight suivant ta caméra editor. Cf canonical lighting spec ou cave abyssal observation doc.

### 3.7.3 Itérer sur les paramètres

Avec le MI ouvert pendant que tu es en éditeur :

- Modifie `WorldTilingScale` : 0.001 → 0.005 → tu dois voir le tiling devenir plus serré
- Modifie `RockTint` : passe en rouge pour debug → tu dois voir la sphère devenir rouge
- Modifie `BiolumIntensity` : monte à 8 → les spots biolum doivent devenir très brillants
- Modifie `LUTStrength` (cf refactor v2) : 0 → 0.5 → tu dois voir le look passer photo→stylized

**Si rien ne change** quand tu modifies un param :
- Vérifie que la case "Override" est cochée dans le MI
- Vérifie que le param est bien rebindé au master (pas un nom différent)

**Si le shader ne compile pas** ou crash → regarde les messages dans la fenêtre **Output Log** (Window → Developer Tools → Output Log) avec filtre "Material"

### 3.7.4 Smoke test des MFs individuelles

Avant d'attaquer le master, tu peux tester chaque MF isolément dans le Material Editor :

1. Ouvre la MF (ex `MF_Triplanar_PBR`)
2. Le preview en haut-gauche montre... rien d'utile (sphère grise) parce que la MF n'a pas de pin Result connecté
3. **Astuce** : crée un material temporaire `M_TestMF` qui appelle juste cette MF et plug `Out_BC` sur Base Color → preview marche
4. Tu peux supprimer `M_TestMF` après validation

> **Alternative** : dans le Material Editor de la MF, click-droit sur n'importe quel pin output → "Start Previewing Node" → le preview affiche la valeur de ce node. Pratique pour debug.

---

## 3.8 Procédure — Générer & importer T_AbyssalPalette_LUT

1. Ouvre [`reports/tools/abyssal_palette_lut.html`](../tools/abyssal_palette_lut.html) dans Chrome (double-click)
2. Sélectionne le preset palette (dropdown) — recommandé : **`Abyssal Cold Pure`** (highlights stay cool, pas de wash beige)
3. Click le bouton **"Download T_AbyssalPalette_LUT.png"** (PNG 256×16)
4. Dans UE5 Content Browser, drag-drop le PNG dans `/Game/Sub3D/Material/CaveGenerator/Palettes/` (créer le dossier Palettes si absent)
5. Double-click la texture → ouvre Texture Editor
6. Dans **Details panel**, set :
   - **Compression Settings** : `VectorDisplacementMap (RGBA8)`
   - **sRGB** : **décoché** (couleurs déjà sRGB stockage, on les veut linéaires au sample)
   - **Filter** : `Bilinear` (gradient lisse) OU `Nearest` (9 bandes nettes)
   - **Texture Group** : `World` (ou laisse défaut)
7. Save (Ctrl+S)

**Validation visuelle dans Texture Editor** :
- Tu dois voir un gradient horizontal de 9 stops selon le preset choisi
- Si tu vois du rouge/jaune/vert vif → tu as téléchargé un mauvais fichier
- Si tu vois "gradient flou unique" au lieu de bandes distinctes → Filter probablement sur Trilinear, repasse Bilinear

**Use ensuite** : la texture est prête pour MF_StylizePBR (cf [stylization annexe](cave_abyssal_stylization.md) §14.2 refactored).

---

## Cross-refs

- **Spec parent** : [2026-05-17_cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md)
- **MFs détaillées** : [cave_abyssal_mf_graphs.md](cave_abyssal_mf_graphs.md) (§3.4 step A→H)
- **Stylization** : [cave_abyssal_stylization.md](cave_abyssal_stylization.md) (§14 Track B + MF_StylizePBR refactor v2)
- **Canonical lighting/PP/fog** : [2026-05-18_lighting_pp_fog_canonical_spec.md](2026-05-18_lighting_pp_fog_canonical_spec.md)
- **Canonical project settings** : [2026-05-18_project_settings_canonical_spec.md](2026-05-18_project_settings_canonical_spec.md)
