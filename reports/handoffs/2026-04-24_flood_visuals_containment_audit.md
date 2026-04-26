# Audit — Flood Visuals Water Plane Containment

**Date** : 2026-04-24
**Branche** : `scripting`
**Destinataires** : review externe (GPT / Gemini / second avis technique)
**Statut** : audit neutre — aucune position prise, décision reportée après review

---

## 0. Résumé exécutif

Le projet Sub3D (Unreal Engine 5.7, Substrate activé) rend l'eau intérieure d'un sous-marin via un `UFloodWaterPlaneComponent` (plan 80 m × 80 m horizontal) par compartiment. En PIE, le plan déborde visuellement hors de la silhouette du hull : il est visible depuis l'extérieur en pleine mer, traverse les cloisons intérieures quand regardé d'un compartiment voisin, et reste visible en vue spectateur aérienne.

Deux masks material existent (SceneDepth + DistanceToNearestSurface) mais aucun ne réalise un **test de containment spatial** "ce pixel est-il à l'intérieur d'un volume mesh fermé". Le plan documenté (`reports/plans/2026-04-23_flood_visuals_architecture.md`) affirme que le material porte toute l'intelligence visuelle, sans spécifier le mécanisme de containment.

Trois options ont été explorées en conversation, aucune n'a été consolidée en implémentation validée. Ce document inventorie l'état actuel et liste les questions ouvertes à soumettre à review externe.

---

## 1. Contexte projet

### 1.1 Stack

- Unreal Engine 5.7, Substrate activé (`r.Substrate=True`)
- Global Distance Fields activés (`r.GenerateMeshDistanceFields=True`, voxel density 0.200)
- Sub cible : Craniata — sub-marin authoré à la main (handmade, PM du FP), ~20–30 m de long
- 5 compartiments : `LowerDeckID`, `MainDeckID`, `MainDeck1ID`, `UpperDeck01`, `AL01`
- Cloisons intérieures présentes : `SM_BH_Lower_BallastAft`, `SM_BH_Lower_BallastFwd`, `SM_BH_Lower_TechPartition`, `SM_BH_Main_Control`, `SM_BH_Main_Fwd`, `SM_BH_Upper_Armory`, `SM_BH_UpperAirlock_Inner`
- Decks intérieurs : `SM_Deck_engine_lower/upper`, `SM_Deck_lower_main`, `SM_Deck_main`, `SM_Deck_upper`
- Hull extérieur : `SM_Hull`
- Airlock : `SM_Airlock_Cassette`, `SM_BH_UpperAirlock_Inner`, `SM_Door_Upper_Airlock_Inner_Frame`

### 1.2 Architecture flood visuals (plan validé)

Source : `reports/plans/2026-04-23_flood_visuals_architecture.md`.

```
USubFloodComponent (sim, authority)
    ↓ CompartmentStates[id].WaterHeightCm
UCompartmentVolumeComponent (data provider)
    ↓ GetWaterSurfaceWorldLocation(), GetWaterLevel01()
┌──────────────────────────────┐
│ UFloodWaterPlaneComponent     │   1 par UCompartmentVolume
│ C++ passif :                  │   - SetWorldLocation(Z=surface)
│  - SetVisibility(level > ε)   │
│  - BP events (level changed)  │
│ Material (SDF + SceneDepth)   │ ← "TOUTE l'intelligence visuelle"
└──────────────────────────────┘
```

Principe déclaré : **séparation stricte C++ (logique passive) / Material (intelligence visuelle)**.

Le CV (`UCompartmentVolumeComponent`) est un data-provider (water height, O2, CompartmentId). Par design, il ne doit pas définir la forme visuelle du plan.

---

## 2. Code en place

### 2.1 `UFloodWaterPlaneComponent`

Fichier : `Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp/.h`

- Dérive `USceneComponent`, possède un `UStaticMeshComponent` enfant créé en BeginPlay via `EnsurePlaneMesh`.
- Défaut mesh : `/Engine/BasicShapes/Plane.Plane` (1 m × 1 m).
- Scale uniforme appliqué : `PlaneWorldSizeCm / 100.f` — par défaut **80 m × 80 m** horizontal.
- World rotation forcée `ZeroRotator` chaque tick → plan **toujours horizontal world-space** (ignore roll/pitch du sub).
- Material assigné via `WaterMaterial` (`TObjectPtr<UMaterialInterface>`), MID créé au registration.
- Tick per-frame : met à jour `SetWorldLocation(X_sub, Y_sub, SurfaceZ)` + `SetVisibility(level > threshold)`.
- Paramètres scalaires du MID updated : `WaterLevel01`, `WaterHeightCm`.
- Pas de logique de containment côté C++.

### 2.2 `ASubmarineBase::BeginPlay`

Fichier : `Source/Sub3D/Submarine/SubmarineBase.cpp:326-362`

```cpp
TArray<UCompartmentVolumeComponent*> VisualVolumes;
GetComponents<UCompartmentVolumeComponent>(VisualVolumes);
for (UCompartmentVolumeComponent* Vol : VisualVolumes)
{
    UFloodWaterPlaneComponent* Plane = NewObject<UFloodWaterPlaneComponent>(this);
    Plane->SourceVolume = Vol;
    Plane->WaterMaterial = DefaultWaterMaterial;
    Plane->PlaneMesh = DefaultWaterPlaneMesh;
    Plane->PlaneWorldSizeCm = DefaultWaterPlaneWorldSizeCm;   // 8000.f par défaut
    Plane->SetupAttachment(Vol);
    Plane->RegisterComponent();
}
```

Spawn sur toutes les net roles (pas d'autorité requise), 1 plane par CV. `DefaultWaterMaterial` référence `/Game/Sub3D/Material/M_CompartmentWater`.

### 2.3 `M_CompartmentWater` — graph actuel

Asset : `Content/Sub3D/Material/M_CompartmentWater.uasset`.

Dump texte (extrait `CopyNodes` du graph) :

**Root node** (`MaterialGraphNode_Root_0`) :
- Blend Mode : `BLEND_TranslucentColoredTransmittance`
- Refraction Method : `RM_IndexOfRefraction`
- Translucency Pass : `MTP_AfterMotionBlur`
- Two Sided : true
- Pin `Front Material` ← `MaterialGraphNode_1.SubstrateWeight.Output`
- Pin `Refraction (IOR)` ← `MaterialGraphNode_0.Add.Output` (= `RefractionStrength × WaterLevel01 + 1.0`)

**Substrate Coverage Weight** (`MaterialGraphNode_1`, `MaterialExpressionSubstrateWeight_0`) :
- Pin `A` ← `MaterialGraphNode_24.SubstrateSlabBSDF` (slab PBR : WaterTint, Roughness=0.04, normal map animée)
- Pin `Weight` ← `MaterialGraphNode_38.MaterialExpressionMultiply_7.Output`
- Pin `Output` → `Root.FrontMaterial`

**Chaîne de Coverage** — reconstituée via le dump :

```
SceneDepth.Output ─┐
                    ├─► Subtract ─► Divide(HullMaskSoftness) ─► Saturate ─► HullMask
PixelDepth.Output ─┘                                                             │
                                                                                 ▼
AbsoluteWorldPosition ─► DistanceToNearestSurface ─► Divide(EdgeSofnessCm) ─► Saturate ─► SoftClip
                                                                                 │
                                                                                 ▼
                                          HullMask ─► Multiply ─► Multiply ─► Weight pin
                                          SoftClip ─┘            │
                                          WaterLevel01 ──────────┘
```

Parameters existants (ScalarParameter) :
- `HullMaskSoftness` (default 4.0)
- `EdgeSofnessCm` (default 5.0) — **typo** : manque un `t` (devrait être `EdgeSoftnessCm`). Aucun C++ n'écrit ce param donc pas d'effet runtime, à renommer pour propreté.
- `WaterLevel01` (default 1.0) — écrit par `UFloodWaterPlaneComponent::ApplyWaterState` via MID
- `RefractionStrength`, `WaveScale`, `WaveSpeedA`, `WaveSpeedB`, `NormalIntensity`, `Roughness` — params artistiques secondaires

Aucun test de containment spatial (pas de ray-march, pas de signed SDF, pas de stencil test, pas de CustomDepth lookup).

### 2.4 BP `BP_Submarine_Craniata`

- 5 `UCompartmentVolumeComponent` placés manuellement avec BoxExtents authorés pour matcher approximativement chaque compartiment interne.
- Slot `DefaultWaterMaterial` → `M_CompartmentWater` (asset base, pas d'instance)
- Slot `DefaultWaterPlaneMesh` → slot présent, valeur non vérifiée depuis le dump binaire
- Cloisons, decks, hull : static mesh components avec meshes listés en §1.1

---

## 3. Comportement observé (PIE)

Screenshots fournis par le dev (2026-04-24) :

1. **Vue extérieure, caméra latérale** : plan bleu rectangulaire visible flottant à la hauteur du water surface, dépassant largement la silhouette du sub en X et Y. Plan visible en pleine mer.
2. **Vue extérieure, angle plus rapproché** : on voit clairement deux plans bleus (probablement deux compartments différents) qui se chevauchent et qui dépassent la coque.
3. **Vue intérieure** : plan visible au-dessus de la porte (traverse le plafond d'un compartment et se voit dans le compartment voisin). Debug label visible : `[water] MainDeckID H=256cm L=0.82` → confirme que `USubFloodComponent` sim fonctionne et que `UFloodWaterPlaneComponent` met bien à jour le plan.

Conclusion observationnelle : le plan **n'est contenu ni par la coque extérieure ni par les cloisons intérieures**. Il s'étend sur ses 80 m × 80 m géométriques sans occlusion ni masking effectifs côté material.

---

## 4. Promesses du plan vs implémentation

Source : `reports/plans/2026-04-23_flood_visuals_architecture.md`, section 2 & 4.

| Promis | Implémenté | Écart |
|---|---|---|
| "Material = TOUTE l'intelligence visuelle" | Material fait look + edge softness | Manque le containment spatial |
| "`SceneDepth + PixelDepth` → discard pixels où hull est devant le plan" | Implémenté | Fait ce qu'il dit, mais ne suffit pas pour les pixels en pleine mer (pas de hull "devant") |
| "`DistanceToNearestSurface(WorldPos)` → soft clip au raccord hull/eau (Global DF)" | Implémenté | Fait ce qu'il dit, mais ce n'est PAS un containment — c'est un polish de bord. `DistanceToNearestSurface` est non-signée. |
| "Normal map scrolling + Fresnel + Refraction via SceneColor" | Partiellement (normals + refraction IOR présents, Fresnel non vérifié dans le dump) | Mineur |
| Water plane per-compartment fonctionnel | Oui, 1 plane × 5 compartments, Z updated | OK |
| "Séparation stricte : C++ passif, pas de décision d'affichage selon géométrie en C++" | Respecté | OK |

Le plan ne documente **nulle part** un mécanisme qui rendrait le plan invisible à l'extérieur de la coque quand aucun opaque ne le cache en front. La phrase "SceneDepth discard" pourrait être interprétée comme "containment" par un lecteur, mais l'opération mathématique décrite (`(SceneDepth − PixelDepth)`) fait de l'occlusion screen-space dépendante de la caméra, pas du containment world-space.

---

## 5. Diagnostic mécanique

### 5.1 Comportement des masks

| Mask | Opération | Résultat dans pixel intérieur à la coque | Résultat dans pixel extérieur en pleine mer |
|---|---|---|---|
| HullMask = `Saturate((SceneDepth − PixelDepth) / HullMaskSoftness)` | occlusion screen-space | Selon POV : si hull devant → 0 (caché) ; sinon → 1 (visible) | Toujours 1 (rien devant dans le ciel / l'océan ouvert) |
| SoftClip = `Saturate(DistanceToNearestSurface / EdgeSofnessCm)` | distance non-signée au Global DF | ~1 loin des surfaces, ~0 au contact d'une paroi | Toujours ~1 (pas de surface proche dans un océan vide) |
| Produit `HullMask × SoftClip × WaterLevel01` | | Variable selon POV + proximité walls | Proche de `WaterLevel01` ≈ 0.8 à 1.0 → **plan visible** |

### 5.2 Raison fondamentale

Le Global DF (`DistanceToNearestSurface`) expose uniquement la distance non-signée. Il ne distingue pas "ce point est dans un volume mesh fermé" vs "ce point est dans l'espace libre". Pour un containment spatial world-space invariant au POV, il faut :

- soit un signal signé (ex. ray-march custom HLSL pour compter les intersections de la DF le long d'un rayon)
- soit un buffer externe (CustomDepth/Stencil) rendu à partir des meshes concernés
- soit une limitation géométrique (forme du plan mesh = forme du compartment)

---

## 6. Options explorées en conversation

Aucune n'a été validée comme implémentation finale.

### 6.1 Option A — Plane scaled au CV extent (C++)

- Proposition : dans `UFloodWaterPlaneComponent::EnsurePlaneMesh`, scaler le plan à `sqrt(X²+Y²)` du `SourceVolume->GetScaledBoxExtent()` + marge.
- Appliquée → build OK → non testée en PIE avant revert.
- Violation du principe "CV = data-only, pas forme visuelle" → revertée à la demande du dev.
- **Statut** : revertée. Code retour à `PlaneWorldSizeCm / 100.f` fixe.

### 6.2 Option B — Plane mesh authoré par compartment

- Proposition : static mesh plat authoré en Blender par compartment, forme 2D = cross-section horizontale du compartment au niveau deck. Slot `WaterPlaneMeshOverride` sur `UCompartmentVolumeComponent`.
- Non retenue : le dev veut que le material porte le containment, pas l'authoring.
- **Statut** : écartée.

### 6.3 Option C — Signed SDF via Custom HLSL (retenue en principe)

- Proposition : Custom node HLSL dans `M_CompartmentWater` qui fait un double ray-march (haut + bas) contre le Global DF à partir de `AbsoluteWorldPosition`. Retourne 1 si les deux rayons touchent un opaque mesh dans une distance bornée (= enfermé dans un volume fermé), 0 sinon.
- Guide écrit : `reports/guides/2026-04-24_signed_sdf_water_containment.md` (HLSL + câblage UI + checklist meshes).
- **Non implémentée** : le Custom node n'a PAS été ajouté au material. Le guide est théorique, pas validé.
- **Statut** : guide livré, application manuelle par le dev pending.

---

## 7. État actuel (2026-04-24, fin de session)

| Élément | Statut |
|---|---|
| `UFloodWaterPlaneComponent` — scale 80 m fixe | ✅ (revertée à l'état initial) |
| Build `Sub3DEditor` | ✅ dernière compilation OK (30 s) |
| `M_CompartmentWater` — graph actuel | ⚠️ inchangé : HullMask + SoftClip + WaterLevel01, aucun containment |
| Option C (Signed SDF) — implémentée | ❌ guide écrit, Custom node non ajouté |
| DF generation sur meshes hull/bulkheads/decks | ❓ non vérifié par compartiment |
| Validation PIE cross-POV (intérieur/extérieur/spectateur/aérien) | ❌ pas effectuée |
| Le plan architectural | ⚠️ ne documente pas le containment mechanism ; potentielle amendment à faire |

---

## 8. Questions ouvertes pour review externe

### 8.1 Correctness technique

**Q1** — Le double ray-march HLSL proposé dans le guide (§3 de `2026-04-24_signed_sdf_water_containment.md`) est-il **correct** pour déterminer "ce point est à l'intérieur d'un volume mesh fermé" dans le cas d'un sous-marin (hull fermé extérieur, bulkheads solides intérieurs, decks horizontaux) ?

**Q2** — Cas limites à valider :
- Compartment avec porte ouverte vers un compartment voisin : le rayon depuis un pixel du plan dans le compartment A peut-il "fuir" par la porte et donner un faux négatif ?
- Pixel directement sous une porte ouverte entre deux compartments superposés : double hit mais inutile ?
- Sub en surface, plan à Z de la ligne de flottaison : ray-up atteint le ciel par l'airlock ouvert → containment = 0 → plan invisible. Désiré ou bug ?
- Sub complètement immergé dans un océan lui-même modélisé comme volume : tous les rayons hit de l'eau ambiante ?

**Q3** — `GetDistanceToNearestSurfaceGlobal` est-elle la bonne fonction HLSL à appeler dans un Custom node material ? L'include path `/Engine/Private/DistanceFieldLightingShared.ush` est-il correct pour UE 5.7 ? Y a-t-il une signature plus idiomatique (`GetDistanceToNearestSurface`, `SampleGlobalDistanceField`, etc.) ?

**Q4** — Le seuil de hit `d < 1.0` cm et le pas minimum `max(d, 4.0)` cm sont-ils raisonnables pour éviter à la fois les faux positifs (stepping à travers un wall fin) et les itérations infinies (stuck au contact) ?

### 8.2 Alternatives techniques

**Q5** — Une approche **CustomDepth Stencil** (hull + bulkheads + decks rendus en CustomDepth stencil=1, plan material discard si stencil != 1 au pixel écran) serait-elle plus robuste/moins coûteuse que le ray-march ? Quel est l'écueil classique de cette approche sur un volume 3D fermé (vs un contour 2D screen-space) ?

**Q6** — Le **Signed Mesh Distance Field** par mesh individuel (via `Distance To Object` material nodes ou via custom HLSL accédant aux MDFs per-mesh) donnerait-il un signal plus propre que le ray-march global ? Est-ce exploitable de manière BP-friendly dans UE 5.7 ?

**Q7** — Un cast de ray en shader contre la DF globale est-il **fiable** quand les meshes du volume (hull, bulkheads, decks) sont séparés (pas un mesh fermé unique) ? La DF globale est-elle "étanche" sur la réunion des meshes ou peut-elle avoir des trous aux raccords ?

### 8.3 Performance

**Q8** — 64 taps DF par pixel (32 iter × 2 rayons) × 500×500 pixels plan ≈ 16M taps DF/frame worst case à 60 Hz. Estimation réaliste à ~0.5 ms sur GPU moderne. Second avis ?

**Q9** — Y a-t-il une optimisation standard : Z-prepass, early-out si le pixel plan est en dessous d'un plan d'eau global océan (outside by default), caching DF temporellement ?

### 8.4 Architecture

**Q10** — Le plan `2026-04-23_flood_visuals_architecture.md` affirme "material carries all visual intelligence" mais ne documente pas de containment mechanism. Est-ce une omission à amender, ou le plan est-il correct et la responsabilité de containment doit être portée autrement (géométrie plane, Option B, etc.) ?

**Q11** — Si on commit à Option C (Signed SDF), quelle est la **dépendance implicite forte** que le plan devrait documenter ? (au minimum : "tous les meshes utilisés pour définir un volume étanche doivent avoir `Generate Mesh Distance Field = true`, doivent être fermés, et ne doivent pas utiliser Two-Sided DF Generation sauf cas particulier")

**Q12** — Y a-t-il une séparation d'asset à faire ? (`M_CompartmentWater_Base` avec le look, `MF_HullContainment` material function réutilisable, `MI_CompartmentWater` instance avec les params tuning)

### 8.5 Process

**Q13** — Les screenshots de debug (cyan slab `DrawDebugBox` à la bonne position, plan material 80 m qui déborde) ont été pris avant que le build Live Coding du fix Option A ne soit déclenché. Il n'existe donc **aucune trace** de ce que le fix Option A aurait donné visuellement. Faut-il refaire un test A vs C côte à côte avant de consolider ?

**Q14** — Le typo `EdgeSofnessCm` → `EdgeSoftnessCm` doit être corrigé en même temps que l'ajout du Custom node, ou séparément pour éviter la confusion ?

---

## 9. Fichiers référencés

| Rôle | Chemin |
|---|---|
| Plan architectural | `reports/plans/2026-04-23_flood_visuals_architecture.md` |
| Guide Signed SDF (non appliqué) | `reports/guides/2026-04-24_signed_sdf_water_containment.md` |
| Component plan | `Source/Sub3D/Submarine/FloodWaterPlaneComponent.{h,cpp}` |
| Data provider | `Source/Sub3D/Submarine/CompartmentVolumeComponent.{h,cpp}` |
| Sim flood | `Source/Sub3D/Submarine/SubFloodComponent.{h,cpp}` |
| Spawn planes | `Source/Sub3D/Submarine/SubmarineBase.cpp:326-362` |
| Material | `Content/Sub3D/Material/M_CompartmentWater.uasset` |
| BP sub | `Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata.uasset` |
| Settings engine | `Config/DefaultEngine.ini:65-66, 150-158` |

---

## 10. Sortie attendue de la review

1. Réponse ferme à Q1–Q4 : l'approche ray-march HLSL est-elle valide ou fragile ?
2. Recommandation entre Option C (Signed SDF ray-march) et alternatives Q5/Q6 (CustomDepth stencil, MDF signed).
3. Amendement éventuel à suggérer au plan architectural (Q10).
4. Liste de checks à faire avant implémentation (DF generation sur chaque mesh, fermeture mesh, etc.).
