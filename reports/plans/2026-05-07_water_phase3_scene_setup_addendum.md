# Addendum Phase 3 — Scene setup abyssal & unification volumes par compartiment

**Date** : 2026-05-07
**Branche** : `water-proto`
**Statut** : addendum à `reports/plans/2026-05-04_water_implementation_plan.md` Phase 3
**Contrainte directrice** : Sub3D se passe **entièrement sous l'eau, dans les abysses**. Pas de soleil, pas de ciel, pas de surface. (cf. memory `project_abyssal_setting_2026_05_07.md`)

---

## 1. Contexte et déclencheur

L'audit P3.1 (matériau `M_Phase0_Test`) et le diagnostic du delta visuel `Proto03_Sub_HullPrecision` vs `L_WaterProto_TwoRooms` (cf. `reports/2026-05-06_water_material_level_diff_diagnosis.md`) ont mis en lumière trois problèmes orthogonaux à la qualité du shader d'eau :

1. **Refraction sous-dimensionnée** : `RefractionStrength` par défaut à `0.020` → IOR `1.02` au lieu de `1.33` (eau réelle).
2. **Setup scène divergent entre maps** : `SkyAtmosphere`, `SkyLight`, `PostProcessVolume`, `DirectionalLight` configurés différemment entre Proto03 et L_WaterProto, causant un rendu cap-mesh different sur le même MID.
3. **Pas de stratégie de scene-lighting cohérente avec le design** : le setup actuel imite un préréglage UE "ciel diurne extérieur" alors que **Sub3D est 100% abysse**.

Ce addendum formalise les actions correctrices et l'unification des volumes par compartiment AVANT les sous-tâches P3.4 → P3.9 (heightfield/slosh/flow). Sans scene cohérente, le rendu du cap mesh translucent ne peut pas être validé visuellement.

---

## 2. Principes directeurs (abysse)

1. **Pas de soleil, pas de ciel atmosphérique terrestre.** Aucun `ADirectionalLight` "sun", aucun `ASkyAtmosphere` configuré comme ciel diurne.
2. **Tout l'éclairage est artificiel et local** : lampes intérieures du sub, projecteurs externes coque, bioluminescence (post-FP). Aucune source globale type sun.
3. **Brouillard dense bleu-noir abyssal** comme fade par défaut vers le noir au-delà de la portée des lumières du sub.
4. **AutoExposure désactivée** : sans soleil, l'auto-exposition UE surcompense vers le ciel/bloom non-existant et écrase l'intérieur. Manual avec `ExposureCompensation=0` ou valeur tunée.
5. **Reflections par compartiment obligatoires** (pas de SkyLight cubemap fallback crédible) : un `UBoxReflectionCaptureComponent` par `UCompartmentVolumeComponent`.
6. **PostProcess par compartiment** (post-FP polish) : tint sous-marin léger, contrôlable par compartiment via `UPostProcessComponent` attaché au volume.

---

## 3. Tâches Phase 3 (renumérotation et insertion)

Insertion entre P3.3 (extension matériau Gerstner) et P3.4 (port heightfield CPU) du plan d'origine. Justification : le rendu du cap mesh translucent ne peut être validé sans scene cohérente, donc il faut fixer la scène avant d'investir dans heightfield/slosh.

### P3.3.A — Fix `RefractionStrength` (matériau)

- **Asset** : `MI_Phase0_Test` (instance) ou `M_Phase0_Test` (default).
- **Action** : passer `RefractionStrength` de `0.020` → `0.330` (IOR 1.33, eau réelle).
- **Validation** : capture viewport en PIE intérieur cap mesh ; le fond doit visiblement plier à travers la lame d'eau. Si "trop fort" subjectivement (parce que l'effet Substrate amplifie), descendre à `0.20` et tuner.
- **Coût** : 1 minute. Editor-only, pas de compile.
- **Critère** : refraction visible et plausible.

### P3.3.B — Unification volumes par compartiment (auto-spawn)

Étendre la boucle d'auto-spawn `UFloodWaterPlaneComponent` dans `ASubmarineBase::BeginPlay` ([SubmarineBase.cpp:430-456](../../Source/Sub3D/Submarine/SubmarineBase.cpp#L430-L456)) pour ajouter, par compartiment, **deux composants frères** attachés au même `UCompartmentVolumeComponent` :

- `UBoxReflectionCaptureComponent` — box-projeté, scale = volume scale, capture l'intérieur du compartiment.
- `UPostProcessComponent` — sphérique (pas de bounds box natif sur le component), priorité haute, BlendRadius ≈ 50cm. **Settings overrides désactivés en FP** (création vide, polish post-FP).

#### Fichiers touchés

- `Source/Sub3D/Submarine/SubmarineBase.cpp` (boucle d'auto-spawn).
- `Source/Sub3D/Submarine/SubmarineBase.h` (includes si nécessaire).

#### Code à insérer

Dans `ASubmarineBase::BeginPlay`, à la fin de la boucle qui spawn les `UFloodWaterPlaneComponent` (~ligne 456), pour chaque `Vol` sélectionné :

```cpp
// ── Per-compartment Box Reflection Capture ─────────────────────────────
UBoxReflectionCaptureComponent* Refl = NewObject<UBoxReflectionCaptureComponent>(this);
if (Refl)
{
    Refl->Brightness = 1.0f;
    Refl->ReflectionSourceType = EReflectionSourceType::CapturedScene;
    Refl->BoxTransitionDistance = 25.f;        // soft 25 cm transition at compartment edges
    Refl->SetupAttachment(Vol);
    Refl->SetRelativeLocation(FVector::ZeroVector);
    Refl->SetRelativeRotation(FRotator::ZeroRotator);
    Refl->SetRelativeScale3D(Vol->GetRelativeScale3D());
    Refl->RegisterComponent();
    Refl->SetCaptureIsDirty();                 // queue first capture next frame
}

// ── Per-compartment Post-Process (FP : empty settings, polish post-FP) ──
UPostProcessComponent* PP = NewObject<UPostProcessComponent>(this);
if (PP)
{
    PP->bUnbound = false;
    PP->BlendRadius = 50.f;                    // 50 cm soft transition (sphérique)
    PP->BlendWeight = 1.f;
    PP->Priority = 1.f;                        // beats world master PP volume
    // Settings overrides : NONE in FP. Filled later (post-FP polish).
    PP->SetupAttachment(Vol);
    PP->RegisterComponent();
}
```

**Includes à ajouter dans `SubmarineBase.cpp`** :
```cpp
#include "Components/BoxReflectionCaptureComponent.h"
#include "Components/PostProcessComponent.h"
```

#### Notes

- **`BoxReflectionCaptureComponent` est box-projeté nativement** : son influence box = `RelativeScale3D × 100 cm`. Copier le scale du `UCompartmentVolumeComponent` donne pile la même région — c'est la propriété clé qui rend l'unification propre.
- **`UPostProcessComponent` est sphérique**, pas box. Pour un boxage strict il faudrait un `APostProcessVolume` (actor) spawné via `World->SpawnActor` + attach. Tradeoff : actor séparé vs component attaché. **FP : on reste sur component sphérique** ; si on observe du PP-leak entre compartiments adjacents en PIE, on switche post-FP.
- Pas de Niagara/effet attaché ici — orthogonal au scope.

#### Validation

- Build clean (UHT + module link).
- En PIE, ouvrir World Outliner → `BP_Submarine_Craniata` → component tree : voir 3 components par `UCompartmentVolumeComponent` (Plane, BoxReflCapture, PostProcess).
- Console : `r.ReflectionCaptureUpdateEveryFrame 1` (debug only) — vérifier que les captures ont été générées (pas de cubemap noire).
- Coût : ~2h dev + test.

#### Critère

Build + 1 capture viewport intérieur cap-mesh montre un reflet de l'intérieur (mur, lampe), pas du ciel/noir.

### P3.3.C — Setup scène abyssale (purge maps + actors fondateurs)

Action **par map**, à appliquer à `Proto03_Sub_HullPrecision.umap` ET `L_WaterProto_TwoRooms.umap` (et toute future map de jeu).

#### Actors à supprimer

| Actor | Raison |
|---|---|
| `ASkyAtmosphere` | Pas de ciel atmosphérique. |
| `ADirectionalLight` (en tant que sun) | Pas de soleil. Si besoin d'une fill-light artistique très basse intensité, OK, mais désactiver `AtmosphereSunLight`. |
| `AVolumetricCloud` (s'il y en a) | Pas de nuages. |

#### Actors à configurer (créer si absents)

| Actor | Property | Valeur |
|---|---|---|
| **`AExponentialHeightFog`** (un, à origine) | `FogDensity` | `0.08` |
| | `FogHeightFalloff` | `0.0` (uniforme — pas de gradient surface→fond, on est tout au fond) |
| | `FogInscatteringColor` | `(0.005, 0.015, 0.03)` linear (bleu-noir abyssal) |
| | `bEnableVolumetricFog` | `true` (cônes phares sub visibles) |
| | `VolumetricFogExtinctionScale` | `1.5` |
| | `StartDistance` | `0` |
| **`ASkyLight`** (optionnel — uniquement pour ambient minimal anti-pure-black) | `SourceType` | `SLS_SpecifiedCubemap` |
| | `Cubemap` | `T_Cube_Abyss` (asset à créer §3.3.D) ou laisser nul |
| | `Intensity` | `0.1` |
| | `Mobility` | `Stationary` |
| | `bRealTimeCapture` | `false` (pas de skyatmo à capturer) |
| **`APostProcessVolume`** (un, master, `bUnbound=true`) | `Settings.AutoExposure → MeteringMode` | `Manual` |
| | `Settings.AutoExposure → ExposureCompensation` | `0.0` |
| | Bloom, Vignette, ColorGrading, LensFlare overrides | tous décochés (FP) |
| | `Priority` | `0` (les PP par compartiment ont priority `1`, ils gagnent) |

**Note critique** : `AutoExposure → Manual` est non-négociable. En setting abysse sans soleil, le default `Auto Exposure Histogram` sur-expose dramatiquement le moindre point lumineux, écrase tout, et donne un rendu instable selon ce que la caméra cadre.

#### Validation

Capture viewport éditeur de chaque map :
- Distance : tout fade vers `(0.005, 0.015, 0.03)` à ~10–20m (selon density).
- Pas de gradient ciel-bleu visible (atmosphère supprimée).
- Sub visible si lights externes présentes ; sinon silhouette quasi-noire.

#### Critère

Les deux maps rendent le cap mesh **identiquement** (à la position du sub près). Re-test du diagnostic `2026-05-06_water_material_level_diff_diagnosis.md` § post-mortem doit montrer aspect aligné.

### P3.3.D — (Optionnel) Cubemap abysse pour SkyLight ambient

- **Asset à créer** : `T_Cube_Abyss` (UTextureCube 64×64).
- **Contenu** : 6 faces toutes en couleur unie `(0.005, 0.01, 0.02)` linear (ou gradient léger top-clair `(0.008, 0.015, 0.025)` → bottom-noir `(0.001, 0.002, 0.005)`).
- **Méthode rapide** : Content Browser → Add → Cubemap → fill solid color.
- **Usage** : assigné à `ASkyLight.Cubemap` quand `SourceType=SLS_SpecifiedCubemap`.
- **Skip si** : suppression complète du SkyLight, on s'appuie 100% sur PointLights + ReflectionCaptures intérieurs. Tester d'abord sans SkyLight ; ajouter seulement si l'ambient pure-black gêne sur les normales d'objets sub-extérieurs.

### P3.3.E — Lights extérieures coque sub (manuel BP, hors scope code)

Hors scope du code mais documenté ici pour cohérence du setup :

- **2–4 SpotLight** sur la coque (proue, sides) : `Movable`, cône 60–90°, `IntensityUnits=Lumens`, `Intensity=50000–200000`.
- **1 PointLight** "interior glow" autour des fenêtres cockpit pour visibilité externe du sub.
- **Lights intérieures par compartiment** : déjà placeable dans `BP_Submarine_Craniata` ; au moins 1 par compartiment pour que `BoxReflectionCapture` capture quelque chose.

Ces lights sont **manuelles** dans le BP — l'utilisateur a explicitement validé ce choix.

---

## 4. Risques et points d'attention

### 4.1 Lumen vs ReflectionCapture priority

Si Sub3D utilise Lumen Reflections (à confirmer dans `DefaultEngine.ini` → `r.DynamicGlobalIlluminationMethod` et `r.ReflectionMethod`), les `BoxReflectionCapture` deviennent **secondaires** : Lumen prend la priorité sur le cap mesh translucent via screen-trace + cubemap fallback.

- **Si Lumen actif** : box captures servent de fallback hors écran. Toujours utiles, mais le test "reflet visible dans cap mesh" peut sembler quand même OK sans elles.
- **Si Lumen non-actif** (probably standard SSR + pre-baked captures) : box captures sont la **seule** source de specular intérieur. Critiques.

À déterminer après build P3.3.B en testant avec/sans Lumen via `r.Lumen.Reflections.Allow 0/1`.

### 4.2 Rebuild des reflection captures à chaque BeginPlay

`SetCaptureIsDirty()` queue une capture, mais l'exécution prend ~1 frame. En PIE, le 1er frame post-spawn voit le cap mesh avec une cubemap noire. Pas un blocker (frame 2+ OK), mais à savoir pour les captures viewport automatisées.

Si on veut un build packagé "pre-baked", il faudra `BuildReflectionCapturesOnly` en éditeur sur la map, ce qui suppose les volumes `UCompartmentVolumeComponent` placés en BP-time (pas seulement runtime). À ce stade c'est le cas via `EnsureCompartmentVolumesFromDefinition` mais en runtime — donc en packagé on aura toujours un capture-on-spawn. Acceptable FP.

### 4.3 PostProcessComponent sphérique → leak entre compartiments adjacents

Si deux compartiments sont à <2× BlendRadius (50 cm + 50 cm = 100 cm) l'un de l'autre, leurs PP overlap et se moyennent. Sur Craniata les compartiments sont assez espacés (>2m typiquement), donc OK. Mais si on observe un tint qui flotte entre 2 zones, switcher vers `APostProcessVolume` actor avec brush exact (post-FP).

### 4.4 No-sun ne signifie pas no-DirLight forever

Si plus tard on veut une "fill light" directionnelle artistique (genre "courant lumineux distant") pour donner du modelé aux objets externes, un `ADirectionalLight` avec `AtmosphereSunLight=false`, `Mobility=Movable`, intensité très basse (<1.0) reste utilisable. C'est juste pas un sun terrestre.

---

## 5. Ordre d'application

1. **P3.3.A** (matériau `RefractionStrength`) — 1 min, zéro risque.
2. **P3.3.B** (code auto-spawn BoxReflCapture + PostProcess) — ~2h dev + build + test PIE.
3. **P3.3.C** (purge + reconfig actors fondateurs des deux maps) — ~10 min/map.
4. **P3.3.D** (cubemap abysse) — skip d'abord, tester sans SkyLight.
5. **PIE validation** : entrer dans le sub, comparer visuel cap mesh sur les deux maps, vérifier reflets intérieurs (pas ciel, pas noir).
6. Si OK : reprendre Phase 3 normale à **P3.4** (port heightfield CPU).

---

## 6. Critères de validation Phase 3.3 (gate avant P3.4)

- ✅ Cap mesh translucent rendu identique sur `Proto03_Sub_HullPrecision` et `L_WaterProto_TwoRooms` (dans la limite des positions de sub différentes).
- ✅ Refraction visible et plausible (IOR ~1.33).
- ✅ Reflets intérieurs dans le cap mesh (mur, lampe, plancher), pas ciel/noir.
- ✅ Distance externe (>20m) fade vers bleu-noir abyssal, pas vers gris/bleu ciel.
- ✅ Pas de surexposition : entrer dans une zone sombre ne fait pas pomper l'auto-exposure.
- ✅ Build clean, pas de warning UHT/link.

---

## 7. Commit prévu

```
feat(water-scene): abyssal scene setup + per-compartment box reflection captures

P3.3.A: bump RefractionStrength to 0.33 (IOR 1.33 = real water)
P3.3.B: auto-spawn UBoxReflectionCaptureComponent + UPostProcessComponent
        per UCompartmentVolumeComponent in ASubmarineBase::BeginPlay
P3.3.C: scene actors purged (SkyAtmosphere, DirLight) + ExpHeightFog dense
        bleu-noir + master PostProcessVolume manual exposure on
        Proto03_Sub_HullPrecision.umap and L_WaterProto_TwoRooms.umap
```

---

## 8. Références croisées

- Plan principal : `reports/plans/2026-05-04_water_implementation_plan.md` § Phase 3.
- Diagnostic delta maps : `reports/2026-05-06_water_material_level_diff_diagnosis.md`.
- Design abysse : memory `project_abyssal_setting_2026_05_07.md`.
- M_Phase0_Test dump : `reports/handoffs/mat M_Phase0_Test dump.md`.
- Code à étendre : `Source/Sub3D/Submarine/SubmarineBase.cpp:430-456`.
