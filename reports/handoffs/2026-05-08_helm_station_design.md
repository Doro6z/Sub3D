# Helm Station — Design Brief & Blender Modeling Spec

Date: 2026-05-08
Workspace: `C:\Dev\Sub3D`
Status: design proposal — modeling brief pour Blender 5.0 pipeline
References:
- DA · `reports/plans/2026-05-08_crew_hud_unified_da.md` (L4 stations + terminals)
- Pillars · `reports/ui-pillars.md` P14 (Station ≠ Terminal) · P16 (palette)
- Visual mockup · `reports/ui-references/helm_station_layout.html`
- Existing widgets · `Source/Sub3D/Submarine/Helm/`
- Blender pipeline · memory `reference_blender_version.md`
- BP submarine · `BP_Submarine_Craniata`

---

## 1. Design intent

Le **Helm Station** est le poste de pilotage du sous-marin. C'est un **prop physique 3D** dans la coque interne, où le joueur s'assied pour piloter. Il héberge **3 terminaux** (UI) et **4 instruments physiques** (props avec animations).

### 1.1 Pillar alignment

| Pillar | Application |
|---|---|
| GDD `humanisation` rule | "L'objet existe dans l'espace, son contenu se gère en menu" — chaque terminal est un **écran physique posé sur la console**, pas un overlay libre. |
| P14 Station ≠ Terminal | Helm Station = chaise + console 3D. Terminaux = écrans qui vivent sur la console. Instruments = props animés (telegraph, yoke, dive levers, kill switch). |
| P15 Context auto-reveal | Approche → L5 prompt `STATION · HELM` → `E · Sit / Mount`. Une fois assis : les terminaux deviennent visibles, L5 affiche `ESC · Leave`. |
| Project setting · abyssal | Lumière artificielle uniquement. Le poste a ses propres spots intégrés + screens emissive. |

### 1.2 Style references (no copy)

- Submarine pilots seats : industriel, sangles, repose-pieds, métal peint.
- Sous-marins militaires russes/américains de classe Akula/Los Angeles : panneaux rivetés, gauges encastrés.
- Cockpit Boeing 747 : organisation par zones fonctionnelles, instruments rondsdistinguables.
- Hardspace Shipbreaker · Barotrauma : aesthetic industriel utilitaire.

**Évite :** futurisme propre (Star Trek), sci-fi propre (Mass Effect), wood-and-brass (Captain Nemo). Sub3D reste industriel maritime moderne avec patine.

---

## 2. Form factor

**Configuration** : siège fixe orienté avant, console en U entourant le pilote (~270°).

```
                    Periscope shaft
                         │
                  ┌──────┴──────┐
                  │  CANOPY     │     <- Spots, status LEDs, periscope mount
                  └──┬───────┬──┘
                     │       │
       ┌─────────────┴───────┴─────────────┐
       │                                   │
       │        FRONT CONSOLE              │   <- Helm Controls terminal + instruments
       │   ┌────────────────────────┐     │
       │   │   [HELM CONTROLS UI]   │     │
       │   └────────────────────────┘     │
       │   ╭───────╮ ╭──╮ ╭─────╮ ╭──╮  │
       │   │TELEGR │ │YK│ │DIVE │ │KS│  │   <- 4 instruments physiques
       │   ╰───────╯ ╰──╯ ╰─────╯ ╰──╯  │
       │                                   │
   ┌───┘     ┌─[ CHAIR ]─┐                 └───┐
   │ SENSOR  │            │  HULL INTEGRITY   │
   │ TERM    │   pilot    │  TERM             │
   │ (left)  │   sits     │  (right)          │
   └─────────┘            └────────────────────┘
                  ┌────────────┐
                  │ FOOT REST  │
                  └────────────┘
```

**Footprint** : 2.4 m largeur × 1.8 m profondeur × 2.2 m hauteur.
**Player sit point** : centré, 1.4 m du sol au niveau des yeux quand assis.

---

## 3. Composants (modular Blender pieces)

Vert budget total : **~4 800 verts** avant LOD. LOD0 ship-ready, LOD1 à -50%, LOD2 à -75%.

### 3.1 Structure principale

| ID | Nom | Dimensions (m) | Verts | Notes |
|---|---|---|---|---|
| 1 | `SM_HelmFloorPlate` | 1.5 × 1.5 × 0.05 | ~200 | Plaque rivetée surélevée 5 cm. Conduit câble dessous. Mat M_RubberMatting top, M_Industrial_Metal bottom. |
| 2 | `SM_HelmChair` | 0.7 × 0.7 × 1.1 | ~600 | Siège pilote industriel. Frame métal, coussin cuir patiné. Mount 5-point au floor plate. Pneumatique central (détail visuel). Optionnel : repose-pieds intégré. |
| 3 | `SM_HelmConsoleFront` | 1.6 × 0.4 × 0.9 | ~800 | Console frontale tiltée 30° vers le pilote. Sub-divisée : zone screen (haut) + zone instruments (bas). Rivetée sur le pourtour. |
| 4 | `SM_HelmConsoleLeft` | 0.7 × 0.3 × 0.7 | ~400 | Panneau gauche, angle 45° vers le pilote. Surface dédiée au screen Sensor + 2-3 boutons décoratifs. |
| 5 | `SM_HelmConsoleRight` | 0.7 × 0.3 × 0.7 | ~400 | Panneau droit, miroir du gauche. Screen Hull Integrity + boutons. |
| 6 | `SM_HelmCanopy` | 1.4 × 0.8 × 0.3 | ~300 | Arche au-dessus du pilote. Mount du périscope, status LEDs. Peut servir de référence visuelle "tu es à un poste". |
| 7 | `SM_HelmFrameStruts` | — | ~200 | 2-3 montants verticaux reliant le canopy à la base. Esthétique structurelle. |

**Sous-total structure** : ~2 900 verts.

### 3.2 Instruments physiques (animés)

Chaque instrument est un asset séparé avec une armature 1-bone pour son axe d'animation. Driven par BP variables.

| ID | Nom | Dimensions (m) | Verts | Animation | Mappe à |
|---|---|---|---|---|---|
| 8 | `SM_HelmTelegraph` | 0.30 × 0.20 × 0.30 | ~250 | Bone `Lever` rotation X (-135° → +135°) | `HelmThrottleTelegraphWidget` |
| 9 | `SM_HelmYoke` | 0.40 × 0.10 × 0.40 | ~350 | Bone `Wheel` rotation Y (-90° → +90°) | `HelmRudderYokeWidget` |
| 10 | `SM_HelmDiveBoard` | 0.35 × 0.25 × 0.50 | ~350 | 2 bones `LeverFwd`, `LeverAft` translation Z (±0.1 m) | `HelmDiveBoardWidget` |
| 11 | `SM_HelmKillSwitch` | 0.15 × 0.15 × 0.18 | ~200 | Bone `Cover` rotation X (0° → 90°) hinge + bone `Button` translation Z (-0.02 m) | `HelmKillSwitchWidget` |

**Sous-total instruments** : ~1 150 verts.

### 3.3 Terminal screens (placeholders pour les WBP)

Chaque screen = plan plat avec un material slot dédié au render UMG. Géométrie minimale, biseau leger sur les bords pour le rendu metalisé du bezel.

| ID | Nom | Dimensions (m) | Verts | Material slot | WBP source |
|---|---|---|---|---|---|
| 12 | `SM_HelmScreen_HelmControls` | 0.45 × 0.02 × 0.20 | ~80 | `MI_TerminalScreen_HelmControls` | `WBP_TerminalHelmControls` (à créer, fragment haut du `HelmCockpitWidget`) |
| 13 | `SM_HelmScreen_Sensor` | 0.40 × 0.02 × 0.30 | ~80 | `MI_TerminalScreen_Sensor` | `WBP_TerminalSensor` (NEW) |
| 14 | `SM_HelmScreen_HullIntegrity` | 0.40 × 0.02 × 0.30 | ~80 | `MI_TerminalScreen_HullIntegrity` | `WBP_TerminalHullIntegrity` (NEW) |

**Sous-total screens** : ~240 verts.

### 3.4 Périscope (post-FP — modulable)

| ID | Nom | Dimensions (m) | Verts | Notes |
|---|---|---|---|---|
| 15 | `SM_HelmPeriscope` | 0.18 × 0.18 × 1.6 | ~400 | Cylindre vertical depuis canopy. Bone `Pivot` rotation Z + `LiftZ` translation Z (0.5 m). Eyepiece + 2 handles. **Stub pour le FP** — modèle présent mais non interactif. Devient `Terminal Navigation` post-FP. |

**Sous-total périscope** : ~400 verts (LOD2 à 100 si pas en focus).

### 3.5 Détails ambient (humanisation rule)

Petits assets qui donnent vie au poste. Optionnels mais ils transforment le prop "fonctionnel" en "habité".

| ID | Nom | Verts | Notes |
|---|---|---|---|
| 16 | `SM_HelmCoffeeCup` | ~60 | Tasse sur un porte-gobelet à droite. GDD §humanisation : "la cafétière est l'humanisation du métal." |
| 17 | `SM_HelmNameplate` | ~40 | Plaque brass gravée "HELM · STATION" sur le canopy. |
| 18 | `SM_HelmPipes` | ~150 | 2-3 tuyaux qui descendent du canopy vers la base, en arrière-plan. Détail silhouette. |
| 19 | `SM_HelmStatusLEDs` | ~50 (×8) | 8 petites LEDs sur le canopy avec emissive. Indiquent l'état basique sans terminal. |
| 20 | `SM_HelmCableBundle` | ~80 | Câbles qui sortent du floor plate vers la console. |
| 21 | `SM_HelmWarningLabels` | ~20 | Decals (pas geometry) sur les panneaux : "DIVE", "EMERG STOP", numéros de circuit. |

**Sous-total ambient** : ~150 verts geometry + decals.

### Total assets : ~21 meshes · ~4 800 verts LOD0

---

## 4. Materials & textures

### 4.1 Master materials (réutilisables)

| Master | Usage | Sub3D existing? |
|---|---|---|
| `M_Industrial_Metal_Painted` | Body panels, console outer | À créer si absent. Reusable hull-wide. |
| `M_Industrial_Metal_Brass` | Telegraph handle, accents, nameplates | À créer. Reusable. |
| `M_Industrial_Metal_Riveted` | Trims sur les bords des panneaux | Possiblement existant pour la coque. |
| `M_LeatherSeat` | Coussin chaise, sangles | Nouveau. Cuir brun usé. |
| `M_RubberMatting` | Top du floor plate | Nouveau. Texture rugueuse. |
| `M_TerminalScreen` | Master pour les 3 screens. Param texture = render UMG via `WidgetTo3D`. | À créer. Critique. |
| `M_GlassPanel_Sub3D` | Cover du kill switch (transparent) | Possiblement réutilisable depuis assets existants. |
| `M_PeriscopeOptic` | Optique du périscope (réfractif) | Post-FP. |

### 4.2 Material instances

- `MI_HelmConsole_BodyPainted` (gris foncé `#2a2c30` avec wear)
- `MI_HelmConsole_AccentBrass` (handles, plates)
- `MI_HelmChair_Cushion` (cuir `#3a2a18` patiné)
- `MI_HelmFloor_Mat` (caoutchouc noir/gris)
- `MI_TerminalScreen_HelmControls`
- `MI_TerminalScreen_Sensor`
- `MI_TerminalScreen_HullIntegrity`
- `MI_HelmKillSwitch_Glass`
- `MI_HelmStatusLED_Green` / `MI_HelmStatusLED_Amber` / `MI_HelmStatusLED_Red`

### 4.3 UV strategy

- **UV0 albedo** : tile material sur les body panels (1 m² grid). Petites zones unique pour les nameplates et instruments.
- **UV1 lightmap** : pas critique pour FP (lighting moveable acceptable). Si static lighting voulu, UV1 dédié 256×256 ou 512×512.
- Modular trim sur les bords (atlas trim sheet) pour ne pas avoir à dépier chaque rivet.

### 4.4 Wear & detailing

- **Macro detail** : painted texture base.
- **Mid detail** : streaks de rouille sur les bords inférieurs, peinture chipped autour des handles fréquemment touchés.
- **Micro detail** : decals (warning labels, numbers, scratches) appliqués via `DecalActor` ou texture overlay.

---

## 5. UI ↔ physique mapping

Chaque écran physique est un emplacement où une `WBP_*` Widget3D viendra se rendre. Mode UE5 : `UWidgetComponent` avec `Space = World`, attaché au socket du screen.

```
Component / Socket                       → WBP_*
─────────────────────────────────────────────────────────────────────
SM_HelmConsoleFront/sk_ScreenHelmCtrl    → WBP_TerminalHelmControls
SM_HelmConsoleLeft /sk_ScreenSensor      → WBP_TerminalSensor
SM_HelmConsoleRight/sk_ScreenHullInt     → WBP_TerminalHullIntegrity
SM_HelmTelegraph /sk_LeverPivot          → BP drives the bone "Lever" from HelmThrottleTelegraphWidget
SM_HelmYoke      /sk_WheelPivot          → BP drives "Wheel" from HelmRudderYokeWidget
SM_HelmDiveBoard /sk_LeverFwd, sk_LeverAft → BP drives translations from HelmDiveBoardWidget
SM_HelmKillSwitch/sk_CoverHinge,sk_Button  → BP drives from HelmKillSwitchWidget
```

**Sockets dans Blender** : Empty objects nommés `sk_*` à exporter. UE5 les détecte automatiquement comme sockets si nommés correctement.

---

## 6. Player ergonomics

### 6.1 Sit point

- Empty `sk_SitPoint` au niveau du coussin de la chaise.
- Position assise (camera) : `world_pos = sk_SitPoint + (0, 0, 0.7)` (yeux à 70 cm au-dessus du coussin).
- Forward facing : `+Y` (assumant Blender `-Y` forward exporté).

### 6.2 Reach zones

```
                  ↑ (head freedom)
                30° pan L/R, 15° pitch
                  
       ┌─────────────────────┐
       │  Sight cone (90°)   │   <- vision libre vers l'avant
       │                     │
       └──┐               ┌──┘
          │  reach zones  │
   ┌──────┤               ├──────┐
   │ left │  hands rest   │right │
   │ side │  on yoke      │ side │
   └──────┘               └──────┘
   Sensor                  Hull Int
   screen                  screen
```

- Yoke à 0.45 m devant le pilote, hauteur épaule.
- Telegraph à droite à 0.30 m.
- Dive board juste à droite du yoke à 0.20 m.
- Kill switch top-right à 0.40 m, hauteur épaule.
- Sensor screen à 45° gauche, glance.
- Hull Integrity screen à 45° droite, glance.

### 6.3 Sit-down animation hooks

- `Anim_HelmSit` : transition 1 s, 1 frame de "approach" → final pose seated.
- `Anim_HelmStand` : reverse, 1 s.
- `Anim_HelmYokeIdle` : idle subtle hands-on-yoke loop.
- `Anim_HelmTelegraphReach` : 0.4 s reach, déclenche la rotation du levier.

Animations placeholders en FP : sit/stand pop instantané. Anim properes post-FP.

---

## 7. Collision & triggers

### 7.1 Collision

- **1 mesh wrapper** `UCX_HelmStation` simple (chair + console + canopy en un seul box compound). Crew doit pouvoir contourner mais pas traverser.
- **Pas de collision sur les instruments** (déjà dans le wrapper).
- Floor plate a une collision séparée (le crew peut marcher dessus).

### 7.2 Triggers

- `BoxComponent_Approach` : 1.5 m × 1.0 m × 1.8 m devant la chaise. Active le L5 prompt `E · Sit / Mount`.
- `BoxComponent_Seated` : 0.5 m × 0.5 m × 0.5 m centré sur le sit point. Active les terminaux + L5 prompt `ESC · Leave`.

### 7.3 Interaction sockets

- `sk_InteractTelegraph` : pour le L5 prompt sur le levier individuel (post-FP, pour réglage manuel).
- `sk_InteractYoke`, `sk_InteractDiveBoard`, `sk_InteractKillSwitch` : same.
- En FP, on n'expose pas chaque instrument séparément ; tout passe par le `Terminal Helm Controls` quand le pilote est assis.

---

## 8. Lighting integration

### 8.1 Sources de lumière

- **2 spots overhead** dans le canopy (`Light_HelmSpot_L`, `Light_HelmSpot_R`). Cône large, intensity moderate, color warm `#e8c898`.
- **1 spot above seat back** rétroéclairant le pilote (`Light_HelmRimBack`). Optional.
- **Status LEDs** comme point lights minuscules (intensity < 5, radius 0.1 m). Couleurs Green/Amber/Red selon état.
- **Screens emissive** (auto via material).

### 8.2 Niveaux et atmosphère

- Lumière artificielle uniquement (cohérent avec memory `project_abyssal_setting_2026_05_07.md`).
- Ambiance globale dim, le poste devient un îlot de lumière dans le compartiment.
- Quand alarme → spots flashing rouge (anim post-FP).

---

## 9. Blender pipeline

### 9.1 File organization

```
Art/Submarine/Helm/Helm_Station.blend
  ├ Collection "Structure"        (1, 3-7)
  ├ Collection "Chair"            (2)
  ├ Collection "Instruments"      (8-11) — armatures parented
  ├ Collection "Screens"          (12-14)
  ├ Collection "Periscope_PostFP" (15) — disabled by default
  ├ Collection "Ambient"          (16-21)
  ├ Collection "Sockets"          empties only (sk_*)
  ├ Collection "Reference"        ref images, hidden in export
  └ Collection "Helpers"          shrinkwrap, modifiers, hidden
```

### 9.2 Mesh prep

- **Apply** all transforms before export (Object → Apply All Transforms).
- **Origin** at world center for the whole station; sub-meshes centered on their own pivot for instruments (telegraph base, yoke center, etc).
- **Normals** facing out (Mesh → Recalculate Outside).
- **Smooth shading** + edge mark for hard edges (Edge → Mark Sharp). Use Bevel with limit method = angle.
- **Vert merge** (Merge by Distance 0.0001 m).

### 9.3 Modifiers strategy

- **Mirror modifier** sur les pieces L/R (console L/R, status LEDs).
- **Bevel modifier** (segments 2, weight 1) sur les corners — apply au préalable de l'export pour les LOD0.
- **Boolean** pour les screen cutouts dans la console front (apply immediately).
- **Solidify** pour les panels minces (apply).
- **Subdivision** : éviter, on cible game-ready directement. Sculpt details vont dans la normal map.

### 9.4 Export FBX (per `reference_blender_version.md`)

```python
# Recommended FBX export params (Blender 5.0)
axis_forward = '-Y'
axis_up = 'Z'
apply_scale_options = 'FBX_SCALE_ALL'
add_leaf_bones = False
bake_anim = True  # for instruments with armatures
bake_anim_use_all_bones = False
bake_anim_simplify_factor = 1.0
mesh_smooth_type = 'EDGE'  # propage le sharp marquage
use_tspace = True
```

Export par groupes :
- `SK_HelmTelegraph.fbx` (skeletal mesh, 1 bone)
- `SK_HelmYoke.fbx`
- `SK_HelmDiveBoard.fbx`
- `SK_HelmKillSwitch.fbx`
- `SM_HelmStation.fbx` (static mesh, structure complète sans les 4 instruments)
- `SM_HelmAmbient.fbx` (cup, pipes, cables — séparé pour pouvoir les omettre si perf)

Ou : un seul `SK_HelmStation.fbx` avec armature multi-bone si tu préfères tout en un. Recommandation : **séparé** pour modularité.

### 9.5 Sockets (empties)

Empties Blender exportés comme sockets UE5 en nommant `sk_*` :

```
sk_SitPoint
sk_ScreenHelmCtrl
sk_ScreenSensor
sk_ScreenHullInt
sk_LeverPivot       (sur SK_HelmTelegraph, bone-attached)
sk_WheelPivot       (sur SK_HelmYoke, bone-attached)
sk_LeverFwd, sk_LeverAft  (sur SK_HelmDiveBoard)
sk_CoverHinge, sk_Button  (sur SK_HelmKillSwitch)
sk_InteractTelegraph, sk_InteractYoke, ...   (interaction prompts post-FP)
sk_LightHelmSpot_L, sk_LightHelmSpot_R       (light source positions)
sk_StatusLED_01 .. sk_StatusLED_08
```

---

## 10. Ordered modeling task list (pour ta session Blender)

Recommandation d'ordre (chaque tâche peut être un commit Blender) :

1. **Block-out** : 5 cubes scaled aux bonnes proportions (chair, front console, L console, R console, canopy). Pas de détail. Valider proportions via la maquette HTML.
2. **Floor plate + chair frame** : forme + sangles + coussin block-out.
3. **Console front** : forme angulaire avec sub-divisions zone screen / zone instruments. Cutout pour le screen Helm Controls.
4. **Console L et R** : mirror modifier. Cutout pour les screens.
5. **Canopy + struts** : structure overhead, pas encore les détails.
6. **Telegraph** : base demi-cercle + lever + handle. Armature 1-bone.
7. **Yoke / wheel** : steering wheel + central hub + 2 handles. Armature 1-bone.
8. **Dive board** : panneau vertical + 2 levers translation. Armature 2-bones.
9. **Kill switch** : box + glass cover hinged + bouton. Armature 2-bones.
10. **Screens** : 3 plans plats + bezel biseauté.
11. **Périscope** (optional FP, recommended block-out only) : cylindre + handles + eyepiece. Armature 2-bones.
12. **Ambient details** : cup, pipes, cables, nameplates.
13. **Status LEDs** : 8 petits cylindres avec emissive, mirror sur le canopy.
14. **Sockets** : empties pour tous les sk_*.
15. **Beveling pass** : edges mark sharp, bevel modifier, apply.
16. **UV unwrap** : bodies tile, instruments unique, screens unique.
17. **Bake high-to-low normals** (optionnel pour la patine).
18. **Texture pass** : painted metal, wear, decals.
19. **Export FBX** : par groupe (cf §9.4).
20. **UE import** : test in PIE, attach `WBP_*` to sockets.

---

## 11. Variants & post-FP extensions

### 11.1 Captain variant

Plus tard, si on veut différencier le poste capitaine du poste navigateur :
- Captain : ajouter accoudoirs avec mini-screens (tactical map, comms).
- Navigator : configuration plus simple, focus sur les terminaux Sensor + Navigation.

### 11.2 Periscope full

Post-FP, le périscope devient interactif :
- Click sur les handles → "use periscope".
- Animation de baisse (translation Z).
- Vue switch sur un Render Target qui montre la vue extérieure.
- Devient le `Terminal Navigation` source de truth.

### 11.3 Modular kit

Idéalement, tous les meshes sont assez modulaires pour qu'on puisse re-mixer la station Helm avec d'autres configurations (Sonar, Mécanique). Le canopy + chaise + floor plate sont génériques, seuls les instruments et screens diffèrent.

### 11.4 Damaged state

Post-FP, ajouter des variantes de damage :
- Material instances avec sparking, smoke decals.
- Sub-meshes optionnels : casse, brisé.
- Driven par hull integrity ENG-bridge < seuil.

---

## 12. Validation checkpoints

Quand tu auras modelé, valide ces points avant texture :

- [ ] Sit point est à la bonne hauteur (camera 1.4 m du sol assis).
- [ ] Yoke est reachable (0.45 m du sit point).
- [ ] Les 3 screens sont lisibles depuis la position assise (angle, distance).
- [ ] Pas de collision capsule du crew avec les instruments quand il s'assied.
- [ ] Les 4 sockets de lever pivot sont au bon endroit (rotation propre, pas de clipping).
- [ ] L5 approach trigger n'overlap pas avec d'autres stations dans le même compartiment.
- [ ] Vert count LOD0 < 5 500 (marge confortable).
- [ ] Compatible `BP_Submarine_Craniata` interior dimensions (compartment helm).

---

## 13. Open questions

À trancher quand tu auras commencé à modéliser :

| # | Question | Default si non décidé |
|---|---|---|
| Q1 | Yoke ou wheel rond ? | **Wheel** rond (classique submarine). Yoke = post-FP. |
| Q2 | Périscope dans FP ou post-FP ? | **Block-out only en FP** (visible mais non-interactif). Plein post-FP. |
| Q3 | Captain variant en FP ? | Non, single Helm Station pour tout pilotage en FP. |
| Q4 | Accoudoirs avec mini-screens ? | Non FP. Post-FP si captain variant. |
| Q5 | Floor plate intégrée ou décor ? | **Décor** (pas de gameplay impact). Le crew step dessus comme tout autre sol. |
| Q6 | Canopy en mesh ou structuré actor (avec spots placés à part) ? | **Mesh + sockets** pour les lights. Lights placées comme components attachés en BP. |
| Q7 | Coffee cup statique ou physique simulé (GDD humanisation) ? | **Statique en FP**. Physique post-FP (la tasse glisse pendant les manœuvres = humanisation). |

---

## 14. Companion files

- Visual mockup multi-vues : `reports/ui-references/helm_station_layout.html`
- DA architecturale : `reports/plans/2026-05-08_crew_hud_unified_da.md`
- Pillars : `reports/ui-pillars.md` (P14 station ≠ terminal)

---

## 15. Pour la prochaine itération

Si tu modèles ce design et qu'il y a des soucis :
1. Mets une photo blocking-out dans `reports/ui-references/` (PNG ou screenshot Blender).
2. Note les dimensions effectives qui divergent du brief.
3. Ajuste les sockets si l'ergonomie ne tient pas.
4. Commit le `.blend` dans `Art/Submarine/Helm/`.
5. Ouvre une rev de ce doc avec un decision log au bas.
