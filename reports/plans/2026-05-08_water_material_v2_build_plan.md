# Review corrigee - `M_CompartmentWater_v2`

**Date** : 2026-05-08  
**Branche** : `water-proto`  
**Statut** : plan corrige apres review, execution bloquee par un gate SLW court  
**Autorite projet** : `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

Ce plan remplace le plan initial du 2026-05-08. Le plan initial n'est pas executable tel quel.

---

## 0. Verdict de review

Le plan original avait la bonne intuition : ne pas continuer a empiler des patches dans `M_Phase0_Test` / `M_CompartmentWater`. Mais il etait trop confiant sur `Single Layer Water` et faux sur plusieurs details UE.

### P0 - A corriger avant tout asset work

1. **Setup SLW faux dans le plan initial.**  
   La doc Epic de `Single Layer Water` indique un material en Blend Mode `Opaque` ou `Masked`, pas `Translucent`. La ligne "SLW exige Translucent" est fausse.

2. **Pins du `Single Layer Water Material Output` faux dans le plan initial.**  
   Le plan listait `ScatteringDistance`. Le node standard expose `Scattering Coefficients`, `Absorption Coefficients`, `PhaseG`, `Color Scale Behind Water`. Ne pas inventer un pin.

3. **Le contrat runtime n'etait pas assez explicite.**  
   `UFloodWaterPlaneComponent` pousse deja des parametres precis sur le MID. Le nouveau materiau doit respecter ces noms, ou le code doit etre corrige dans le meme jalon.

4. **Le pourquoi de la reecriture etait trop superficiel.**  
   La vraie cause n'est pas seulement "ancien graph sale". La cause structurelle est que les anciens materiaux essaient de faire de la topologie/containment en shader. Cette responsabilite doit rester dans la geometrie du cap mesh et les donnees de bake.

5. **Il existe un mismatch runtime sur l'amplitude.**  
   `FloodWaterPlaneComponent.h` expose `HeightfieldAmplitudeParamName = "HeightfieldAmplitude"`, mais `FloodWaterPlaneComponent.cpp` pousse aussi `HeightfieldAmplitudeCm` en dur a chaque tick. Avant de valider v2, aligner le code pour utiliser un seul nom canonique.

### P1 - A resserrer

- Ne pas supprimer `M_Phase0_Test`, `MI_Phase0_Test`, `M_CompartmentWater` ni le module `Sub3DWaterProto` dans ce jalon.
- Ne pas promettre que SLW regle le flicker/TAA. Le verifier en PIE.
- Ne pas utiliser `Absolute World Position` pour l'animation des vagues. Utiliser `Local Position` du cap mesh.
- Ne pas ajouter de normal map "au cas ou". v2 lit le heightfield et genere sa normale depuis lui.

---

## 1. Pourquoi reecrire le materiau

On reecrit le materiau pour retirer de la dette de responsabilite, pas pour rendre le graph plus joli.

### Cause racine

Les anciens graphs melangent quatre sujets :

- **Look de l'eau** : teinte, roughness, reflets, refraction.
- **Animation de surface** : Gerstner, heightfield, WPO.
- **Containment** : GDF, OBB, SceneDepth, DistanceToNearestSurface.
- **Debug/proto** : noms de params temporaires, nodes orphelins, essais de Phase0.

Le containment shader est le mauvais proprietaire. Le runtime actuel a deja un meilleur proprietaire : `UFloodWaterPlaneComponent` rend un cap mesh par compartiment depuis `UCompartmentWaterBake`. La silhouette de l'eau vient du mesh. Le materiau doit lire le heightfield et faire le look, pas deviner ou sont les murs.

### Decision

`M_CompartmentWater_v2` est un nouveau master propre. Il ne refactor pas node par node l'ancien graph.

Il conserve le contrat runtime actuel :

| Parametre materiau | Type | Producteur runtime | Usage v2 |
|---|---|---|---|
| `HeightFieldTex` | Texture2D | `UFloodWaterPlaneComponent::EnsureHeightfieldInitialized` | WPO Z + normale derivee |
| `HeightfieldAmplitude` | Scalar | `HeightfieldAmplitudeParamName` | echelle WPO en cm |
| `LocalBoundsMin` | Vector | bake courant | UV heightfield |
| `LocalBoundsMax` | Vector | bake courant | UV heightfield |
| `WaterLevel01` | Scalar | `RefreshFromFlood` | debug/tuning, pas containment |
| `WaterHeightCm` | Scalar | `RefreshFromFlood` | debug/tuning, pas containment |

Patch code requis avant l'asset final :

```cpp
// FloodWaterPlaneComponent.cpp
// Remplacer le write hardcode HeightfieldAmplitudeCm par le FName configurable.
BakeCapMID->SetScalarParameterValue(HeightfieldAmplitudeParamName, HeightfieldAmplitudeCm);
```

Ne pas renommer les params dans l'asset tant que ce patch n'est pas fait.

---

## 2. Gate obligatoire - SLW ou stop

SLW est une hypothese, pas une verite acquise. On ne bascule pas `BP_Submarine_Craniata.DefaultWaterMaterial` tant que ce gate n'est pas vert.

### 2.1 Asset de spike

Creer un materiau temporaire :

`/Game/Sub3D/Material/_Spike/M_Spike_CompartmentWater_SLW`

Settings :

| Property | Valeur |
|---|---|
| Material Domain | Surface |
| Blend Mode | `Opaque` |
| Shading Model | `Single Layer Water` |
| Two Sided | false |
| Disable Depth Test | false |
| Used with Procedural Mesh | true |
| Refraction input/root chain | non connecte |

Graph minimal :

- `Local Position` RG -> UV normalisee via `LocalBoundsMin/Max`
- `HeightFieldTex.R * HeightfieldAmplitude` -> `World Position Offset.z`
- Flat normal `(0,0,1)` -> `Normal`
- `Roughness = 0.06`
- `Specular = 0.5`
- `Opacity = 0.5`
- `Single Layer Water Material Output` :
  - `Scattering Coefficients`
  - `Absorption Coefficients`
  - `PhaseG`
  - `Color Scale Behind Water`

Valeurs de depart :

| Param | Default |
|---|---|
| `WaterScatteringCoeff` | `(0.002, 0.006, 0.010)` |
| `WaterAbsorptionCoeff` | `(0.006, 0.002, 0.001)` |
| `WaterPhaseG` | `0.0` |
| `ColorScaleBehindWater` | `(0.7, 0.9, 1.0)` |

Ces valeurs sont des coefficients de depart, pas une direction artistique finale.

### 2.2 Validation du gate

Dans PIE sur Craniata, sur un compartiment avec bake cap actif :

- [ ] Le material compile sans erreur.
- [ ] Le cap mesh est visible quand `WaterLevel01 > VisibilityThreshold01`.
- [ ] `Dump MID params` montre `HeightFieldTex`, `HeightfieldAmplitude`, `LocalBoundsMin`, `LocalBoundsMax`.
- [ ] `InjectAt` deforme visiblement la surface.
- [ ] Les vagues restent ancrees au sous-marin quand il avance.
- [ ] Pas d'ecran noir, pas de disparition selon l'angle camera.
- [ ] Pas de flicker stationnaire visible pendant 20 secondes.

Si un de ces points echoue, stop. Ne pas construire un fallback dans ce plan. Ecrire un diagnostic court et faire un plan separe pour une v2 Substrate/Translucent basee sur le chemin deja valide par les tests du 2026-05-04.

---

## 3. Materiau final si le gate passe

Creer :

- `/Game/Sub3D/Material/Functions/MF_WaterCompartmentUVs`
- `/Game/Sub3D/Material/M_CompartmentWater_v2`
- `/Game/Sub3D/Material/MI_CompartmentWater_v2`

Ne pas modifier les assets legacy dans ce jalon.

### 3.1 `MF_WaterCompartmentUVs`

Inputs :

| Input | Type |
|---|---|
| `LocalPosXY` | Float2 |
| `LocalBoundsMin` | Float3 |
| `LocalBoundsMax` | Float3 |

Output :

| Output | Type |
|---|---|
| `UV` | Float2 |

Graph :

```text
SpanXY = Max(LocalBoundsMax.xy - LocalBoundsMin.xy, (1,1))
UV = Saturate((LocalPosXY - LocalBoundsMin.xy) / SpanXY)
```

Raison : clamp defensif contre une bake invalide, sans crash shader.

### 3.2 Bloc heightfield inline

Decision d'implementation validee pendant l'execution : ne pas creer de
`MF_WaterHeightfieldSurface`. Le bloc heightfield reste directement dans
`M_CompartmentWater_v2`.

Graph inline minimal :

```text
h_center = Sample(HeightFieldTex, UV).r
WPOZ = h_center * HeightfieldAmplitude
Normal = (0,0,1)
```

Ne pas ajouter Gerstner ou sine ambient dans v2. Le heightfield, les breaches, `InjectAt` et le slosh modal suffisent pour ce jalon.

### 3.3 `M_CompartmentWater_v2`

Settings :

| Property | Valeur |
|---|---|
| Material Domain | Surface |
| Blend Mode | `Opaque` |
| Shading Model | `Single Layer Water` |
| Two Sided | false |
| Used with Procedural Mesh | true |
| Refraction Method | laisser par defaut, ne pas cabler la pin Refraction |

Parameters :

| Param | Type | Default |
|---|---|---|
| `LocalBoundsMin` | Vector | `(0,0,0,0)` |
| `LocalBoundsMax` | Vector | `(1,1,1,0)` |
| `HeightFieldTex` | Texture | DefaultBlack |
| `HeightfieldAmplitude` | Scalar | `10` |
| `WaterRoughness` | Scalar | `0.06` |
| `WaterSpecular` | Scalar | `0.5` |
| `WaterOpacity` | Scalar | `0.5` |
| `WaterScatteringCoeff` | Vector | `(0.002,0.006,0.010,0)` |
| `WaterAbsorptionCoeff` | Vector | `(0.006,0.002,0.001,0)` |
| `WaterPhaseG` | Scalar | `0.0` |
| `ColorScaleBehindWater` | Vector | `(0.7,0.9,1.0,1)` |

Wiring :

```text
Local Position -> mask R,G
LocalPosXY + LocalBoundsMin/Max -> MF_WaterCompartmentUVs -> UV

HeightFieldTex + UV
    -> Sample.R * HeightfieldAmplitude
    -> WPOZ -> MakeFloat3(0,0,WPOZ) -> World Position Offset

Constant3(0,0,1) -> Normal

WaterRoughness -> Roughness
WaterSpecular -> Specular
WaterOpacity -> Opacity

WaterScatteringCoeff -> Single Layer Water Material Output.Scattering Coefficients
WaterAbsorptionCoeff -> Single Layer Water Material Output.Absorption Coefficients
WaterPhaseG -> Single Layer Water Material Output.PhaseG
ColorScaleBehindWater -> Single Layer Water Material Output.Color Scale Behind Water
```

Base Color peut rester noir ou tres sombre. Le look vient du node SLW.

---

## 4. Integration

1. Appliquer le patch code d'amplitude decrit en section 1.
2. Creer le spike SLW.
3. Passer le gate section 2.
4. Creer `MF_WaterCompartmentUVs`.
5. Creer `M_CompartmentWater_v2`.
6. Creer `MI_CompartmentWater_v2`.
7. Assigner `MI_CompartmentWater_v2` dans `BP_Submarine_Craniata.DefaultWaterMaterial`.
8. PIE validation section 5.

Ne pas toucher :

- `Sub3DWaterProto`
- `Sub3DWaterBake`
- `UCompartmentWaterBake`
- `USubFloodComponent`
- `UFloodWaterPlaneComponent` hors patch d'amplitude
- assets legacy tant que v2 n'est pas valide

---

## 5. Validation finale

Validation sur Craniata, cap mesh bake actif :

- [ ] `Dump MID params` confirme les params runtime.
- [ ] Surface visible a `WaterLevel01 > 0.02`, invisible sous le seuil.
- [ ] `Open heightfield texture` montre une R32F vivante.
- [ ] `InjectAt` produit une onde qui se propage.
- [ ] Breach detecte -> injection initiale visible.
- [ ] Slosh -> tilt/offset leger pendant acceleration/virage.
- [ ] Sub en mouvement -> la texture ne defile pas en world-space.
- [ ] Sub pitch/roll -> surface reste gravity-aligned, avec slosh seul en tilt.
- [ ] Vue exterieure -> pas de plan 80 m visible. Si fuite visible, verifier bake/cap mesh, pas le shader.
- [ ] Vue interieure -> eau lisible, sol/props perceptibles au travers.
- [ ] Stationnaire 20 s -> pas de flicker evident.

Si un echec concerne la silhouette ou une fuite hors compartiment, ne pas ajouter de raymarch shader. Corriger la bake, le cap mesh ou l'assignation de bake.

---

## 6. Cleanup apres validation

Quand les validations section 5 sont vertes :

1. Garder `M_Phase0_Test` et `M_CompartmentWater` pendant au moins un commit de stabilisation.
2. Noter dans `reports/backlog/post_fp_debt.md` :
   - supprimer les assets proto apres verification de referencers,
   - nettoyer les docs Phase0 obsoletes,
   - decider post-FP si SLW reste le chemin final ou si une variante Substrate est preferable.
3. Commit separe :

```text
feat(water): add M_CompartmentWater_v2 SLW material
```

Ne pas archiver ou deplacer `Source/Sub3DWaterProto/Materials/` dans ce commit.

---

## 7. TL;DR execution

1. Patch param amplitude runtime.
2. Spike SLW minimal, sans assignation definitive.
3. Gate PIE sur un cap mesh Craniata.
4. Si vert : construire `MF_WaterCompartmentUVs`, `M_CompartmentWater_v2`, `MI_CompartmentWater_v2`.
5. Assigner l'instance dans `BP_Submarine_Craniata`.
6. Valider avec Water Debug Panel.
7. Commit asset + petit patch code. Pas de cleanup legacy dans le meme jalon.
