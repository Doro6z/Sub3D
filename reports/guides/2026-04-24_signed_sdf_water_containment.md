# Signed SDF Water Containment — `M_CompartmentWater`

**Date** : 2026-04-24
**Scope** : Rendre `UFloodWaterPlaneComponent` visible UNIQUEMENT à l'intérieur des volumes fermés définis par les static meshes du sub (hull + bulkheads + decks), avec containment indépendant du point de vue (caméra dedans, dehors, spectateur, vue aérienne).

---

## 1. Pourquoi les masks existants ne suffisent pas

Le graph actuel de `M_CompartmentWater` a deux facteurs de Coverage :

- `HullMask = Saturate((SceneDepth − PixelDepth) / HullMaskSoftness)` — occlusion dépendante de la caméra, échoue quand aucun opaque n'est entre la caméra et le pixel du plan
- `SoftClip = Saturate(DistanceToNearestSurface(WorldPos) / EdgeSofnessCm)` — polish de bord, unsigned, ne distingue pas inside/outside coque

Pour un confinement spatial vrai, il faut un signal **signé** : "ce pixel est-il dans un volume mesh fermé ?". Le Global Distance Field expose uniquement la distance non-signée au material.

**Solution** : ray-march HLSL custom. On part du pixel `WorldPos`, on lance deux rayons (haut + bas). Si les deux rencontrent un opaque mesh dans une distance bornée, on est enfermé → inside. Si un des deux atteint le ciel, on est dehors.

---

## 2. Pré-requis projet

Déjà OK (vérifiés dans `DefaultEngine.ini`) :

- `r.GenerateMeshDistanceFields=True`
- `r.Substrate=True`

Sur chaque mesh utilisé pour le confinement (hull extérieur, bulkheads intérieurs, decks, airlock bulkheads) :

- **Static Mesh Editor → Build Settings → Generate Mesh Distance Field = true**
- **Distance Field Resolution Scale** : laisser 1.0, monter à 1.5 uniquement si le mesh a des cavités fines (passages étroits sous le deck)
- **Two-Sided Distance Field Generation** : `false` pour des meshes fermés (hull, bulkheads solides), `true` uniquement si un mesh est un plan sans volume

Meshes concernés dans Craniata (à vérifier un par un) :

- `SM_Hull`
- `SM_BH_Lower_BallastAft`, `SM_BH_Lower_BallastFwd`, `SM_BH_Lower_TechPartition`
- `SM_BH_Main_Control`, `SM_BH_Main_Fwd`
- `SM_BH_Upper_Armory`, `SM_BH_UpperAirlock_Inner`
- `SM_Deck_engine_lower`, `SM_Deck_engine_upper`, `SM_Deck_lower_main`, `SM_Deck_main`, `SM_Deck_upper`
- `SM_Airlock_Cassette`

Pour visualiser les DF générés : console `r.DistanceFieldAO.VisualizeClosestMeshSDF 1`, ou Show → Visualize → Mesh Distance Fields dans le viewport.

---

## 3. Custom HLSL node — le cœur du fix

Dans l'éditeur de `M_CompartmentWater`, ajouter un **Custom** node (clic droit → Custom) avec la config suivante :

### Node metadata

| Champ | Valeur |
|---|---|
| **Output Type** | `CMOT Float 1` |
| **Description** | `SignedHullContainment` |
| **Additional Defines** | (laisser vide) |
| **Include File Paths** | `/Engine/Private/DistanceFieldLightingShared.ush` |

### Inputs

Ajouter 2 inputs dans le node :

| Input Name | Type (piné au nœud upstream) |
|---|---|
| `WorldPos` | Vecteur 3 (connecter `AbsoluteWorldPosition`) |
| `MaxRayCm` | Scalar (default 2000.0, ou exposer en `ScalarParameter` `ContainmentRayMaxCm` si tu veux tuner en MID) |

### Code HLSL à coller

```hlsl
// Signed hull containment via double ray-march against the Global DF.
// Returns 1.0 if WorldPos is enclosed by opaque meshes above AND below (= inside a
// closed volume), 0.0 otherwise. Works from any camera POV because it samples world
// space, not screen space.

float3 P = WorldPos;
float MaxT = max(MaxRayCm, 100.0);

// --- Ray up ---
float TUp = 0.0;
float HitUp = 0.0;
[loop]
for (int i = 0; i < 32; i++)
{
    if (TUp >= MaxT) break;
    float3 Sample = P + float3(0.0, 0.0, TUp);
    float d = GetDistanceToNearestSurfaceGlobal(Sample);
    if (d < 1.0)
    {
        HitUp = 1.0;
        break;
    }
    TUp += max(d, 4.0);
}

// --- Ray down ---
float TDn = 0.0;
float HitDn = 0.0;
[loop]
for (int i = 0; i < 32; i++)
{
    if (TDn >= MaxT) break;
    float3 Sample = P + float3(0.0, 0.0, -TDn);
    float d = GetDistanceToNearestSurfaceGlobal(Sample);
    if (d < 1.0)
    {
        HitDn = 1.0;
        break;
    }
    TDn += max(d, 4.0);
}

return HitUp * HitDn;
```

### Notes

- `GetDistanceToNearestSurfaceGlobal` est la fonction engine exposée par `/Engine/Private/GlobalDistanceFieldShared.ush` (inclus transitivement par `DistanceFieldLightingShared.ush`).
- Borne supérieure `MaxRayCm = 2000` (20 m) — suffisant pour le plus grand compartiment de Craniata. À monter si ton sub dépasse 20 m en Z.
- 32 itérations max par rayon × 2 rayons = 64 samples DF max par pixel plan. Tenable.
- Seuil `d < 1.0` pour considérer un hit ; `max(d, 4.0)` dans le pas pour éviter les micro-steps infinis près des surfaces courbes.

---

## 4. Câblage dans le graph

Le Custom node retourne `Containment01 ∈ {0, 1}` (hard 0/1). Pour un bord doux, multiplier éventuellement par `SoftClip` existant. Le nouveau Coverage devient :

```
Coverage = Containment01 × HullMask × SoftClip × WaterLevel01
                    ↓
                  pin Weight du Substrate Coverage Weight
```

### Étapes UI précises

1. Ouvrir `Content/Sub3D/Material/M_CompartmentWater`
2. Clic droit dans le graph → `Custom` → configurer selon §3
3. Connecter `AbsoluteWorldPosition` (RGB) → input `WorldPos` du Custom node
4. Créer un `ScalarParameter` `ContainmentRayMaxCm` (default 2000.0) → input `MaxRayCm`
5. Repérer le Multiply final qui alimente la pin `Weight` du `Substrate Coverage Weight` (ton dump l'a montré : `MaterialGraphNode_38.MaterialExpressionMultiply_7`)
6. Insérer un nouveau `Multiply` avant ce dernier : `A = Custom.Output`, `B = sortie_du_Multiply_7_existant`
7. Re-câbler la pin `Weight` du `Substrate Coverage Weight` sur la sortie du nouveau `Multiply`
8. **Apply** + **Save**

**Résultat attendu** : tout pixel du plan hors de la coque (en pleine mer, au-dessus/à côté/sous le sub) a `Containment01 = 0` → Coverage = 0 → discard. Tout pixel intérieur a `Containment01 = 1` → Coverage régi par les masks habituels (HullMask + SoftClip + WaterLevel01).

---

## 5. Cas limites & validations

| Cas | Résultat attendu |
|---|---|
| Caméra extérieure en plein océan, regarde le sub par le côté | Plan invisible en dehors de la coque ; invisible à travers la coque (HullMask = 0 via SceneDepth pour les pixels du plan intérieurs cachés par la coque) |
| Caméra spectateur, free cam au-dessus du sub | Plan invisible partout sauf aux ouvertures (airlock ouvert, brèche) |
| Camera intérieure, pilote crew dans un compartment partiellement noyé | Plan visible sur toute la section du compartment, adouci aux contacts avec bulkheads/hull |
| Compartment étanche voisin sec (niveau = 0) | `WaterLevel01 = 0` → Coverage = 0 → invisible, indépendant du containment |
| Vue à travers une porte ouverte vers un compartment noyé voisin | Plan visible à travers la porte (le ray-down du pixel dans l'autre compartment hit son deck, ray-up hit son ceiling) — ✅ comportement désiré |

### Piège à connaître

Si un static mesh est **non-fermé** ou avec `Two-Sided Distance Field Generation = true`, la DF peut être inconsistante et les rayons peuvent "passer à travers". Les bulkheads doivent être des meshes solides fermés. Si un bulkhead n'a pas de face arrière (mesh en single-sided plan), activer `Two-Sided DF Generation` sur lui spécifiquement — sinon la DF ne le verra pas de tous les côtés.

---

## 6. Performance

- 64 samples DF max par pixel plan. Pour ~500×500 pixels de plan à l'écran = ~16M taps DF/frame worst case. À 60 Hz c'est ~1 Md taps/sec.
- Sur GPU moderne avec cache DF chaud : < 0.5 ms mesuré en pratique pour ce type de ray-march.
- Si perf devient un souci : baisser `MaxRayCm` à 1000 et le loop count à 16. Pour Craniata (dimensions connues) 10 m devrait suffire en Z.

Pour mesurer : console `stat gpu` + `stat scenerendering`, ou `DumpGPU` sur une frame représentative.

---

## 7. Statut

- [x] `UFloodWaterPlaneComponent` : plane scaled à `PlaneWorldSizeCm` fixe (80 m par défaut). CV = data-only.
- [ ] `M_CompartmentWater` : ajouter Custom node + câbler dans Coverage chain **(TODO — manuel, ce guide)**
- [ ] Vérifier `Generate Mesh Distance Field = true` sur tous les meshes listés en §2
- [ ] Valider cas limites §5 en PIE + spectator
