# Option C v2 — Center-Visibility Flood Containment

**Date** : 2026-04-24
**Remplace** : `2026-04-24_water_cap_authoring_and_material_functions.md` (Option B cap mesh — abandonnée suite à objection "section XY varie en Z").
**Scope** : algorithme shader + architecture Material Functions + C++ binding pour contenir visuellement le water plane d'un compartment, indépendamment du POV, adaptatif à la forme variable de la coque en Z.

---

## 1. Principe en une ligne

**Water(P) = `InsideCV(P)` ∧ `CenterCanSee(P)` ∧ `WaterLevel01 > 0`**

où la ligne de vue `Center(CV) → P` ne doit traverser aucun static mesh opaque (hull, bulkhead, deck) pour que le pixel soit considéré dans le sous-volume habité par l'eau.

Voir le SVG [01_intersection_concept.svg](assets/2026-04-24_flood_algo/01_intersection_concept.svg) pour l'illustration.

## 2. Pourquoi C v2 résout les cas que Option B naive ne gère pas

| Cas | Option B naive (cap mesh unique) | **Option C v2** |
|---|---|---|
| Compartment tube O-shape (section étroite bas + haut, max milieu) | ❌ cap statique déborde aux Z non-médianes | ✅ ray-cast s'adapte à chaque Z du water level |
| Compartment avec hull courbé (Craniata LowerDeck/UpperDeck) | ❌ section XY varie avec Z | ✅ adaptatif |
| Ouverture airlock, porte, brèche | ambigu (cap avec ou sans le passage) | ✅ naturel (plan A calcule depuis Center A, plan B depuis Center B, chacun indépendant) |
| Compartments re-authorés (nouveau sub) | re-authorer tous les caps | ✅ zéro authoring |
| Porte fermée puis ouverte (état runtime) | cap statique, pas de changement | ✅ le ray voit la porte ou la traverse si elle devient un "trou" dans les meshes, chaque plan reste dans SON CV |

## 3. Architecture

```text
┌──────────────────────────────────────┐
│  USubFloodComponent (sim, authority) │
│  ↓ GetCompartmentWaterHeightCm       │
│  ↓                                    │
│  UCompartmentVolumeComponent (data)  │
│  ↓ GetWaterSurfaceWorldLocation      │
│  ↓                                    │
│  UFloodWaterPlaneComponent (C++)     │ ← SET MID vector params each tick:
│    - 1 per CV                         │   CV_Center_WS, CV_HalfExtent,
│    - updates plane Z + visibility     │   CV_W2L_Row0/1/2, WaterLevel01
│    - drives MID (containment data)   │
│  ↓                                    │
│  MID_CompartmentWater (per plane)    │
│  ↓                                    │
│  M_CompartmentWater                  │ ← combines 3 MFs
│    Weight = Contain × Edge × Level01 │
│    A      = Look.SubstrateSlabBSDF   │
└──────────────────────────────────────┘
```

Voir le SVG [03_material_architecture.svg](assets/2026-04-24_flood_algo/03_material_architecture.svg).

## 4. C++ — état

**Appliqué et buildé** :

- [FloodWaterPlaneComponent.cpp:136-210](../Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp#L136-L210) — `ApplyWaterState` drive 5 vector params + WaterLevel01/HeightCm dans le MID chaque tick :
  - `CV_Center_WS` = `SourceVolume->GetComponentTransform().GetLocation()`
  - `CV_HalfExtent` = `SourceVolume->GetScaledBoxExtent()`
  - `CV_W2L_Row0/1/2` = rows 0, 1, 2 de la matrice `Inverse` du transform du CV (world-to-local 4x3)

Le slot `WaterPlaneMeshOverride` sur `UCompartmentVolumeComponent` est **conservé mais dormant** — inutilisé en C v2, laissé en place au cas où un compartment atypique futur voudrait un cap authoré spécifique (fallback possible).

Le plan mesh reste à `PlaneWorldSizeCm` (80 m par défaut). La taille n'importe plus : le shader masque tous les pixels hors CV et hors ligne de vue du centre.

## 5. HLSL — algorithme Custom Node

À coller dans le node `Custom` de `MF_CompartmentWater_Containment` (création en §6).

### Node config

| Champ | Valeur |
|---|---|
| Output Type | `CMOT Float 1` |
| Description | `Compartment Containment` |
| Include File Paths | `/Engine/Private/DistanceField/GlobalDistanceFieldShared.ush` |

### Inputs du Custom node

| Input | Type | Connecte à |
|---|---|---|
| `WorldPos` | Float 3 | `AbsoluteWorldPosition.XYZ` |
| `CVCenter` | Float 3 | VectorParameter `CV_Center_WS`.RGB |
| `HalfExtent` | Float 3 | VectorParameter `CV_HalfExtent`.RGB |
| `W2LRow0` | Float 4 | VectorParameter `CV_W2L_Row0` (RGBA, car 4 composantes) |
| `W2LRow1` | Float 4 | VectorParameter `CV_W2L_Row1` |
| `W2LRow2` | Float 4 | VectorParameter `CV_W2L_Row2` |

### Code HLSL (validé par user — version corrigée 2026-04-24)

```hlsl
// Test 1 — InsideCV (AABB)
// Delta-based: pre-subtract the CV world position, then apply rotation-only part of W2L.
// C++ packs W2LRow0/1/2 as native rows of W2L (W2L.M[i][0..3]); the .w components
// carry the full 4x3 translation but are unused here — translation is handled by Delta.
float3 Delta = WorldPos - CVCenter;
float3 P_CVLocal;
P_CVLocal.x = Delta.x * W2LRow0.x + Delta.y * W2LRow1.x + Delta.z * W2LRow2.x;
P_CVLocal.y = Delta.x * W2LRow0.y + Delta.y * W2LRow1.y + Delta.z * W2LRow2.y;
P_CVLocal.z = Delta.x * W2LRow0.z + Delta.y * W2LRow1.z + Delta.z * W2LRow2.z;

float3 AbsDelta = abs(P_CVLocal);
if (any(AbsDelta > HalfExtent)) return 0.0;

// Test 2 — Ray-march (Center-Visibility)
float3 Dir = WorldPos - CVCenter;
float RayLen = length(Dir);
if (RayLen < 1.0) return 1.0;
Dir = Dir / RayLen;

// UE5 Translated World = AbsoluteWorld - CameraPosition.
// CameraPos is provided as a VectorParameter updated by the client each tick
// (or use ResolvedView.PreViewTranslation.xyz directly if you prefer the engine path).
float3 CenterTW = CVCenter - CameraPos;

float T = 0.0;
[loop]
for (int i = 0; i < 32; i++) {
    if (T >= RayLen) break;
    float3 SampleTW = CenterTW + Dir * T;
    float d = GetDistanceToNearestSurfaceGlobal(SampleTW);
    if (d < 5.0) return 0.0;
    T += max(d, 4.0);
}
return 1.0;
```

### Node config (mise à jour)

| Champ | Valeur |
|---|---|
| Output Type | `CMOT Float 1` |
| Description | `Compartment Containment` |
| Include File Paths | `/Engine/Private/DistanceField/GlobalDistanceFieldShared.ush` |

### Inputs du Custom node (mis à jour)

| Input | Type | Connecte à |
|---|---|---|
| `WorldPos` | Float 3 | `AbsoluteWorldPosition.XYZ` |
| `CVCenter` | Float 3 | VectorParameter `CV_Center_WS`.RGB |
| `HalfExtent` | Float 3 | VectorParameter `CV_HalfExtent`.RGB |
| `W2LRow0` | Float 4 | VectorParameter `CV_W2L_Row0` |
| `W2LRow1` | Float 4 | VectorParameter `CV_W2L_Row1` |
| `W2LRow2` | Float 4 | VectorParameter `CV_W2L_Row2` |
| `CameraPos` | Float 3 | VectorParameter `CameraPos_WS` — **à driver depuis C++ ou BP** (cf. §5.5) |

### §5.5 — Driver du paramètre `CameraPos_WS`

Le Custom node a besoin de la position world de la caméra pour convertir en translated world space (coordonnées attendues par `GetDistanceToNearestSurfaceGlobal`).

Deux options :

**Option A — BP sur le Pawn crew (recommandée FP)** : dans `BP_SubmarineCrew` (ou whichever pawn porte la caméra active), chaque tick, fais un `Get All Actors Of Class(ASubmarineBase)` puis itère `GetComponents(UFloodWaterPlaneComponent)` et appelle `Set Vector Parameter Value on Materials` avec `CameraPos_WS = GetWorldLocation(CameraComponent)`. Simple, BP-side.

**Option B — C++ client tick** : tu peux ajouter dans `UFloodWaterPlaneComponent::TickComponent` un update du MID si `bUseCameraPosParam = true` :

```cpp
if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
{
    FVector CamLoc; FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    MaterialMID->SetVectorParameterValue(TEXT("CameraPos_WS"), CamLoc);
}
```

Option B est plus robuste (pas d'itération BP par tick). À ajouter quand tu auras validé visuellement.

### Notes techniques

- **`GetDistanceToNearestSurfaceGlobal`** : signature `float(float3 TranslatedWorldPosition)`, [engine:214](C:/Program%20Files/Epic%20Games/UE_5.7/Engine/Shaders/Private/DistanceField/GlobalDistanceFieldShared.ush#L214). Retourne distance signée dans [-range, +range] où range dépend du clipmap courant. Pour le line-of-sight on teste `d < 5.0` — si on frappe un solide le signe devient négatif mais le seuil `< 5.0` tolère aussi le fait qu'on arrive tangentiellement près d'une surface.
- **Seuil `HitThresholdCm = 5.0`** : les voxels du GDF font typiquement 10–20 cm (`r.DistanceFields.DefaultVoxelDensity=0.200` = density 0.2 voxels/cm → 5 cm voxel, mais les clipmaps lointains sont plus gros). `5.0` est tolerant — ajuste à `8.0`–`10.0` si le compartment courant semble fuir visuellement.
- **`MinStepCm = 4.0`** : empêche de stepper infiniment petit près d'une surface (si la distance retournée est très petite mais > seuil). Peut être baissé à 2.0 pour une meilleure précision près des murs (au prix de plus d'itérations).
- **`[loop]` attribute** : force le compilateur HLSL à unroller en loop dynamique plutôt que de déplier statiquement (unrolling de 32 iter est coûteux pour le compilateur). Standard pour ce type de ray-march.
- **Early out `RayLen < 1.0`** : pixel au centre → pas besoin de ray-marching.
- **Budget perf réel** : pour un CV de 5 m × 3 m × 2 m (diagonale ~6 m = 600 cm), le ray moyen fait ~300 cm, avec step moyen ~30 cm → ~10 iterations. Worst case 32.

## 6. Material Functions — création manuelle dans UE

Dans `Content/Sub3D/Material/Functions/` (crée le dossier).

### 6.1 MF_CompartmentWater_Containment (NEW)

Porte le shader HLSL ci-dessus.

- **Inputs** (Material Function Input nodes) :
  - `WorldPos` (Float3)
  - `CVCenter` (Float3)
  - `HalfExtent` (Float3)
  - `W2LRow0` (Float4)
  - `W2LRow1` (Float4)
  - `W2LRow2` (Float4)
- **Output** (Material Function Output node) :
  - `Containment01` (Float1)
- **Body** : 1 seul node Custom HLSL configuré comme §5, ses 6 inputs branchés aux inputs de la MF, sa sortie branchée à l'output `Containment01`.

### 6.2 MF_CompartmentWater_EdgePolish (REFACTOR)

Déplace la chaîne HullMask + SoftClip existante de `M_CompartmentWater` vers cette MF.

- **Inputs** : aucun (utilise les ScalarParameter `HullMaskSoftness`, `EdgeSoftnessCm` directement)
- **Output** : `EdgeMask01` (Float1)
- **Body** :
  ```
  HullMask = Saturate((SceneDepth − PixelDepth) / HullMaskSoftness)
  SoftClip = Saturate(DistanceToNearestSurface(AbsoluteWorldPosition) / EdgeSoftnessCm)
  Output  = HullMask × SoftClip
  ```
- **Important** : c'est le moment de renommer le ScalarParameter `EdgeSofnessCm` (typo) en `EdgeSoftnessCm` (correct). Le param n'étant écrit par aucun C++, aucune autre modif requise.

### 6.3 MF_CompartmentWater_Look (REFACTOR)

Déplace le slab BSDF + chaîne normals + tint + roughness de `M_CompartmentWater` vers cette MF.

- **Inputs** : aucun (utilise ScalarParameter et VectorParameter directement : `WaveScale`, `WaveSpeedA`, `WaveSpeedB`, `NormalIntensity`, `Roughness`, `WaterTint`)
- **Output** : `WaterSlab` (Substrate)
- **Body** : les nodes actuels de `M_CompartmentWater` qui produisent le SubstrateSlabBSDF (les TextureSample + Panner + BlendNormals + VectorParameter + ScalarParameter existants).

## 7. Refactor de M_CompartmentWater

Après création des 3 MFs :

### 7.1 Retirer l'actuel Custom node HLSL erroné

Le Custom node ajouté précédemment (celui qui a le carré rouge) et tous les nodes du ray-march vertical : à supprimer.

### 7.2 Nettoyer les nodes absorbés par les MFs

Supprime (les nodes sont maintenant dans les MFs) :
- Le chain `SceneDepth` + `PixelDepth` + `Subtract` + `Divide(HullMaskSoftness)` + `Saturate` (absorbé dans `MF_EdgePolish`)
- Le chain `AbsoluteWorldPosition` + `DistanceToNearestSurface` + `Divide(EdgeSofnessCm)` + `Saturate` (absorbé dans `MF_EdgePolish`)
- Le node `SubstrateSlabBSDF` + WaterTint + Roughness + les 2 `Panner` + TextureSample normals + BlendNormals (absorbé dans `MF_Look`)

### 7.3 Nouveau graph de M_CompartmentWater

```text
[MaterialFunctionCall: MF_CompartmentWater_Containment]
  WorldPos   ← AbsoluteWorldPosition.XYZ
  CVCenter   ← VectorParameter CV_Center_WS.RGB
  HalfExtent ← VectorParameter CV_HalfExtent.RGB
  W2LRow0/1/2← VectorParameter CV_W2L_Row0/1/2 (full RGBA)
  → Containment01
                    ↓
                    ↓ ┌───────────────────────────────────────┐
                    ↓ │ Multiply                               │
                    ↓→│   A = Containment01                    │
[MF_EdgePolish] ─────→│   B = EdgeMask01                       │
                      │   C = WaterLevel01 (ScalarParameter)   │ ← triple Multiply
                      │   → Weight                              │
                      └───────────────────────────────────────┘
                                             │
                                             ▼
                             [Substrate Coverage Weight]
                                 A      ← MF_Look.WaterSlab
                                 Weight ← triple Multiply above
                                 → Output
                                             │
                                             ▼
                              Material Root.Front Material

[Refraction chain inchangée — reste en place]
  RefractionStrength × WaterLevel01 + 1.0 → Root.Refraction (IOR)
```

Material settings Root (inchangés) : Blend Mode `TranslucentColoredTransmittance`, Refraction Method `Index Of Refraction`, Two Sided `true`, Translucency Pass `After Motion Blur`, Shading Model `Default Lit` (Substrate override via SlabBSDF).

### 7.5 Refraction chain — à re-câbler explicitement

La pin `Refraction (IOR)` du Material Root a été câblée dans l'ancien graph via `Add(Multiply(RefractionStrength, WaterLevel01), 1.0)`. Si tu as effacé ces nodes pendant le refactor (comme semble-t-il le cas après tes edits), il faut les **re-créer dans `M_CompartmentWater`** — c'est une branche parallèle à la chaîne Coverage/Weight, pas dans une MF.

**Nodes à créer dans `M_CompartmentWater`** :

1. `ScalarParameter` → nom `RefractionStrength` (Default Value `0.05`). Category optionnelle `Water|Refraction`.
2. `ScalarParameter` → nom `WaterLevel01` (Default Value `1.0`). **Réutilise le ScalarParameter déjà en place** si il existe — il est aussi utilisé par `MF_CompartmentWater_Containment` (pas directement, mais via le Multiply final de Coverage). Un seul `WaterLevel01` param partagé.
3. `Multiply` → A = `RefractionStrength.Output`, B = `WaterLevel01.Output`.
4. `Constant` → Value `1.0` (c'est le base IOR de l'air/eau-à-l'interface).
5. `Add` → A = `Multiply.Output`, B = `Constant.Output`.
6. **Connecte `Add.Output` à la pin `Refraction (IOR)` du Material Root.**

**Semantics** : `IOR = 1.0 + RefractionStrength × WaterLevel01`.
- Quand le compartment est sec (`WaterLevel01 = 0`), IOR = 1.0 → aucune réfraction, le plan est complètement flat visuellement (de toute façon hidden par `VisibilityThreshold01` dans C++).
- Quand plein (`WaterLevel01 = 1.0`), IOR = 1.0 + `RefractionStrength` ≈ 1.05 → refraction légère mais visible.
- Tune `RefractionStrength` dans le `MI_CompartmentWater` pour ajuster le look sans recompile (0.02 subtil, 0.15 très marqué).

**Diagramme** :

```text
[RefractionStrength]─┐
                     ├─► Multiply ─┐
[WaterLevel01]───────┘             ├─► Add ─► Root.Refraction (IOR)
                     [Constant 1.0]─┘
```

**Note** : garde `WaterLevel01` comme ScalarParameter unique partagé entre la chaîne Refraction ET la chaîne Coverage (`Weight = Contain × Edge × WaterLevel01`). C'est le même param driven par le C++ via `MID->SetScalarParameterValue("WaterLevel01", ...)`.

### 7.4 MI_CompartmentWater

- Clic droit sur `M_CompartmentWater` → `Create Material Instance` → nom `MI_CompartmentWater`.
- Expose les params artistiques (WaveScale, Tint, Roughness, EdgeSoftnessCm, HullMaskSoftness) pour tuning PIE sans recompile.
- **Assigne `MI_CompartmentWater`** à `BP_Submarine_Craniata → DefaultWaterMaterial` (remplace le base `M_CompartmentWater`).

## 8. Checklist de validation PIE

Teste sur Craniata avec 5 compartments :

- [ ] Breach le LowerDeck → plan visible UNIQUEMENT à l'intérieur du LowerDeck
- [ ] Vue spectateur from above (free camera) → aucun plan visible hors silhouette sub
- [ ] Vue spectateur lateral à travers le hull → aucun débordement latéral
- [ ] Caméra intérieure dans MainDeck → plan MainDeck visible, plan LowerDeck NON visible (séparés par le deck)
- [ ] Porte ouverte entre LowerDeck et MainDeck → les 2 plans visibles **uniquement dans leur compartment respectif**, pas de "fuite" visuelle dans l'autre
- [ ] Water level variable (10% → 50% → 90%) → plan adapte sa forme à chaque Z sans modifier l'authoring
- [ ] Sub à pitch/roll → plan reste world-horizontal, containment toujours correct (le CV tourne avec le sub, le W2L matrix le suit, test AABB en CV-local reste valide)

## 9. Diagnostic si ça fuit

Si en PIE tu observes un débordement :

1. **Console `r.AOGlobalDistanceFieldVisualize 1`** — visualise le Global DF. Vérifie que SM_Hull, SM_BH_*, SM_Deck_* sont bien inclus (silhouettes pleines).
2. Si un mesh est absent : **Static Mesh Editor → Build Settings → Generate Mesh Distance Field = true** + rebuild mesh.
3. Si un bulkhead fin est digéré par les voxels (mesh trop fin pour la résolution GDF) : monter `Distance Field Resolution Scale` à 1.5–2.0 sur ce mesh, ou augmenter l'épaisseur du mesh en Blender.
4. Si la fuite survient au contact d'une porte/airlock : c'est attendu — la porte ouverte laisse une ligne de vue traverser. La containment logique ("chaque plan dans SON CV") tient car le CV bound bloque, mais visuellement le plan peut "fuir" à l'ouverture pour quelques pixels près du passage. Le `SoftClip` existant (edge fade) polir.
5. Monter `HitThresholdCm` du HLSL de 5 à 10 (plus tolérant aux voxels).

## 10. Perf

- **Instructions base pass** : +~40 instructions par rapport à l'actuel (ajout du Custom node), soit ~440 total estimés.
- **Samples GDF par pixel** : 1–32, moyen ~10.
- **Test AABB early-out** : tous les pixels hors CV retournent 0 immédiatement (pas de ray-march) — économise ~70% des pixels d'un plan 80m pour un CV de 5m.
- **Mesurer** : console `stat gpu`, `stat scenerendering`. Baseline avant refactor : 403 instructions.

## 11. Amendement plan

À acter dans `reports/plans/2026-04-23_flood_visuals_architecture.md` (amendement 2026-04-24 déjà en place, mais références cap-mesh à corriger).

---

## 12. Statut

- [x] C++ — MID vector params wiring dans `ApplyWaterState`
- [x] Build Sub3DEditor validé
- [x] SVGs générés (01, 02, 03)
- [ ] Création manuelle des 3 Material Functions (§6)
- [ ] Refactor `M_CompartmentWater` (§7)
- [ ] Création `MI_CompartmentWater` + assignation BP
- [ ] Validation PIE cross-cas (§8)
- [ ] Amendement plan (§11)
