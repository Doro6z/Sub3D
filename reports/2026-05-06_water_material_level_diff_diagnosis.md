# M_CompartmentWater — Diagnostic du delta visuel entre maps

**Date** : 2026-05-06
**Branche** : water-proto
**Symptôme** : `M_CompartmentWater` (`/Game/Sub3D/Material/M_CompartmentWater.M_CompartmentWater`) — material identique, MID créé runtime via `URoomWaterRenderer::Refresh()` (`Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp:111-135`) — rend différemment selon la map :

- `Content/Maps/Proto03_Sub_HullPrecision.umap` → aspect A (incorrect)
- `Content/Maps/L_WaterProto_TwoRooms.umap` → aspect B (référence visuelle)

Le pipeline de matérialisation est strictement le même dans les deux maps : `URoomWaterRenderer::Refresh()` → `UMaterialInstanceDynamic::Create(CapMaterial, this)` → `SetVectorParameterValue(LocalBoundsMin/Max)` → `CapMeshComp->SetMaterial(0, CapMID)`. Le delta provient donc nécessairement de **l'environnement de rendu** dans lequel le cap mesh est inséré.

## 1. Diff factuel — actors par catégorie

Source : `mcp__unrealclaude__unreal_get_level_actors` filtré par `class_filter` sur les deux maps consécutivement.

| Catégorie | Proto03_Sub_HullPrecision | L_WaterProto_TwoRooms | Δ |
|---|---|---|---|
| **Actor count total** | 1971 | 13 | gigantesque (Proto03 = océan + 1900+ planes) |
| **DirectionalLight** | 1 (`pitch=-89.20, yaw=-155.70, roll=-136.10`, scale 2.5×2.5×2.5) | 1 (rot/scale par défaut) | ⚠ rotation extrême + scale != 1 |
| **SkyLight** | 1 (`SkyLight_0` à 820,0,360) | **0** | **présent vs absent** |
| **SkyAtmosphere** | 1 (à 600,260,0) | 1 (à origine) | ~équivalent |
| **PostProcessVolume** | 1 (`PostProcessVolume2` à 6870,-1580,-30, scale 3.91×3.55×1) | **0** | **présent vs absent** |
| **ExponentialHeightFog** | 0 | 0 | identique |
| **VolumetricCloud** | 0 | 0 | identique |
| **PointLight / SpotLight / RectLight** | 0 | 1 PointLight (`PointLight_1`) | **éclairage intérieur seulement dans L_WaterProto** |
| **AudioVolume / PhysicsVolume** | 0 | 0 | identique |
| **Submarine** | BP_Submarine_Craniata à `(-772, 2846, 11200)` | (rooms isolés, pas de sub) | sub à Z=11200 dans Proto03 → **très haut** |

Les actors planes massifs de Proto03 (z=-1660) sont des dalles de sea-bed/seafloor placeholders, hors zone du sub. Ils ne touchent pas le cap mesh.

## 2. Lecture des deux viewports

- **Proto03** : caméra orientée vers l'horizon → ciel bleu, océan sombre. Le sub n'est pas dans la frame éditeur capturée — la caméra est loin. Pas d'info utile sur le rendu du cap dans cette capture.
- **L_WaterProto_TwoRooms** : caméra sur les deux rooms vues de l'extérieur, ciel bleu, lumière directionnelle frontale, ombres très contrastées (typique d'un setup sans SkyLight et sans PP volume → tout vient du SkyAtmosphere + DirLight + PointLight intérieur).

Note : le delta visuel signalé par l'utilisateur concerne le **cap mesh translucent en intérieur**, pas la silhouette extérieure. Le cap mesh n'est visible qu'en PIE/embarquement.

## 3. Material et MID — pas de divergence côté pipeline

`URoomWaterRenderer::Refresh()` (`Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp:107-136`) charge `CapMaterial` (UPROPERTY `EditAnywhere`, par défaut `WorldGridMaterial` si nul, donc dans les deux maps c'est la valeur configurée sur le BP_RoomActor / BP_Submarine_Craniata). Les seuls paramètres MID positionnés sont `LocalBoundsMin` et `LocalBoundsMax` — purement géométriques. **Ni l'eau-level, ni la teinte, ni la profondeur ne sont set côté MID**, donc `M_CompartmentWater` lit ses valeurs par défaut.

→ Si la valeur de `CapMaterial` diffère entre `BP_Submarine_Craniata.URoomWaterRenderer` et `BP_RoomOpen.URoomWaterRenderer`, ce serait LA cause directe. À vérifier (voir §6 plan d'action).

## 4. Hypothèse principale

**Confiance : medium**.

Hypothèse H1 (la plus probable, deux contributeurs) :

1. **Le PostProcessVolume de Proto03 est probablement `bUnbound=true`** ou couvre l'origine via son scale. Si Auto Exposure est activée, l'exposition s'adapte aux 1971 actors d'océan ouvert (très sombre + ciel saturé) — résultat : le HDR moyen de la scène est très différent de celui de L_WaterProto (deux rooms, ciel dominant). `M_CompartmentWater` étant probablement Substrate Translucent ou SLW, son rendu **dépend du tonemap/exposure final**. Une compensation auto-exposée différente change la perception de transparence et de teinte de la lame d'eau.
2. **L'absence de SkyLight dans L_WaterProto** force le cap à n'utiliser que (a) le SkyAtmosphere comme env reflection, (b) la PointLight intérieure pour le direct lighting. Dans Proto03, le SkyLight ajoute une cubemap-based ambient au cap qui peut le délaver si la cubemap a été capturée à un moment où la scène avait un état lumineux différent. Combine avec l'exposition altérée → rendu lavé / désaturé.

Le DirLight de Proto03 a une rotation aberrante (`roll=-136°`) et un scale 2.5× — soit un setup hérité d'un autre niveau (le scale d'un DirLight n'a aucun effet sur la lumière elle-même, mais le `roll=-136°` combiné à `pitch=-89°` donne une direction quasi-verticale dans une orientation chelou). Ça affecte la teinte du SkyAtmosphere (position du soleil) et la direction de l'ombre directionnelle sur le cap.

## 5. Action correctrice — alignement Proto03 → L_WaterProto

Plan d'action **dans Proto03, sans toucher L_WaterProto**, par ordre de coût croissant. À exécuter une étape à la fois et capture viewport entre chaque pour observer le delta.

### Étape 1 — Neutraliser le PostProcessVolume (test isolant, 30 s)

1. World Outliner → sélectionner `PostProcessVolume2`.
2. Details → décocher `Enabled` (Settings → `bEnabled`).
3. Ré-entrer en PIE / re-capturer le cap mesh en intérieur.

Si le rendu s'aligne sur L_WaterProto, le coupable est confirmé : auto-exposure + tonemapping du PP. Continuer en étape 1b.

**Étape 1b** : Plutôt que de désactiver tout le PP, ouvrir ses Settings et :
- décocher `Auto Exposure → Metering Mode` override (ou forcer `Manual` avec `ExposureCompensation=0`).
- décocher tous les overrides de `Color Grading`, `Bloom`, `Lens Flares`.
- vérifier `bUnbound`. Si `true`, soit le passer `false`, soit redimensionner la box pour ne couvrir que l'extérieur du sub.

### Étape 2 — Tester sans SkyLight (test isolant, 30 s)

1. World Outliner → sélectionner `SkyLight_0`.
2. Details → `Visible = false` (juste hide, ne pas supprimer).
3. Ré-évaluer.

Si l'aspect s'aligne maintenant, la cause est l'apport ambient cubemap du SkyLight. Soit force `SkyLight → Real Time Capture = true` (recapture chaque frame, plus fidèle au SkyAtmosphere courant), soit baisse `Intensity Scale` à 0.3–0.5.

### Étape 3 — Reset du DirectionalLight (5 min)

1. World Outliner → `DirectionalLight_1`.
2. Reset rotation : `(pitch=-45, yaw=45, roll=0)` (valeurs neutres, sun à 45° au-dessus de l'horizon).
3. Reset scale : `(1, 1, 1)`.
4. Vérifier `Atmosphere Sun Light = true` (sinon SkyAtmosphere ne tracke pas le sun).

Le scale d'un DirLight n'a aucun effet rendu, mais le `roll=-136°` combiné au `pitch=-89°` est mathématiquement non-standard et peut provoquer un comportement bizarre côté `SkyAtmosphere::SetSunLightDirection`.

### Étape 4 — Ajouter une PointLight intérieure dans Proto03 (5 min)

Pour aligner totalement sur L_WaterProto, le cap a besoin d'éclairage intérieur direct. Le BP_Submarine_Craniata embarque-t-il déjà des SubLightBase ? À vérifier — si oui, c'est peut-être une question de toggle `bAffectTranslucentLighting` sur ces lights. Sinon, ajouter une PointLight dans le BP, attached sub root, à mi-hauteur du compartiment.

### Étape 5 — Vérifier `CapMaterial` est bien le même asset

`mcp__unrealclaude__unreal_blueprint_query` sur `BP_Submarine_Craniata` et `BP_RoomOpen` — chercher la variable `CapMaterial` du component `URoomWaterRenderer` et confirmer le SoftObjectPath. Si l'un référence `M_CompartmentWater` et l'autre une variante (`M_FloodWater_DF` modifié ?), la divergence est triviale.

## 6. Étape de validation supplémentaire (si Étapes 1–5 insuffisantes)

Confiance low → tests additionnels à faire :

- **Project Settings → Rendering** : confirmer que `r.Substrate` est identique (même fichier `DefaultEngine.ini`, donc cohérent par construction — mais une override par-map via `WorldSettings.PerWorldRenderSettings` reste possible). World Settings de chaque map : `mcp__unrealclaude__unreal_set_property` ne suffit pas pour un read complet ; ouvrir manuellement et comparer onglet "Rendering" vs "World".
- **Capture viewport en PIE** des deux maps avec caméra **dans le sub à hauteur du cap mesh**. Les captures actuelles sont éditeur, hors compartiment. Sans vue identique, on compare deux scènes externes.
- **Console** : `r.SubstrateDebugView 1` en PIE pour voir quels passes Substrate sont actifs.
- **Console** : `Stat SceneRendering` pour comparer le coût des passes translucent dans les deux maps — un delta significatif révélerait des features rendu différentes (Lumen, Reflection Captures, etc.).

## 7. TL;DR

| | |
|---|---|
| Cause la plus probable | **PostProcessVolume2 de Proto03** + **SkyLight_0**, qui altèrent l'exposition et l'env-reflection du cap mesh translucent. L_WaterProto, plus minimaliste, laisse le matériau lu "tel quel". |
| Confiance | medium |
| Action première | désactiver `PostProcessVolume2` (Étape 1), capturer, comparer. Coût : 30 s. |
| Action secondaire si Étape 1 insuffisante | hide `SkyLight_0` (Étape 2) puis reset DirLight rotation (Étape 3). |
| Risque | faible — toutes les étapes sont réversibles (Visibility / décochage) sans modifier le matériau. |

## 8. Outils MCP UnrealClaude utilisés

- `unreal_status` — confirmer connexion + version 5.7.4.
- `unreal_open_level` × 2 — basculer entre Proto03_Sub_HullPrecision et L_WaterProto_TwoRooms.
- `unreal_get_level_actors` × 13 (par class_filter : `PostProcessVolume`, `DirectionalLight`, `SkyLight`, `SkyAtmosphere`, `ExponentialHeightFog`, `VolumetricCloud`, `PointLight`, `SpotLight`, `RectLight`, `AudioVolume`, `PhysicsVolume`, name `Submarine`, brief).
- `unreal_capture_viewport` × 2 (un par map).
- `unreal_asset_search` × 3 — confirmer existence des deux maps + M_CompartmentWater.
- `unreal_ue` (asset / get_asset_info) × 1 — confirmer M_CompartmentWater est bien class `Material`.

Lecture source `Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp` pour confirmer le pipeline de matérialisation runtime.

Aucune modification effectuée — diagnostic read-only conformément à la consigne.

---

## 9. Post-mortem (2026-05-06, après confirmation utilisateur)

**Cause réelle : SkyAtmosphere.** Hypothèse H1 (PP volume + SkyLight) **invalidée**.

Erreur de diagnostic : le tableau §1 a marqué SkyAtmosphere "~équivalent" sur la base d'un simple count (1 vs 1), sans lire les propriétés de chaque actor. Les positions différentes (`(600, 260, 0)` vs origine) auraient dû déclencher une comparaison property-by-property, pas un classement "équivalent".

Leçon pour les diagnostics futurs : un compteur d'actors n'est pas un diff. Pour un actor unique mais central au rendu (SkyAtmosphere, ExponentialHeightFog, DirectionalLight), lire au minimum les propriétés clés (transform, intensity, et pour SkyAtmosphere : `BottomRadius`, `TransmittanceMin/MaxSampleLOD`, `MultiScatteringFactor`, `RayleighScattering*`, `MieScattering*`) avant de conclure.

Action correctrice à appliquer : à confirmer avec l'utilisateur (suppression du SkyAtmosphere de Proto03 ? alignement des paramètres sur celui de L_WaterProto ? changement de transform ?). Cette étape sera tracée séparément.
