# Water Cap Authoring + Material Functions — Flood Visuals

**Date** : 2026-04-24
**Scope** : Implémentation de Option B — containment géométrique par compartment via water cap meshes authorés, et refactor de `M_CompartmentWater` en Material Functions réutilisables. Remplace Option C (Signed SDF ray-march) invalidée par review GPT.
**Statut** : Code C++ appliqué + build OK. Authoring Blender + refactor material : à faire manuellement en suivant ce guide.

---

## 0. Principe en une phrase

**Chaque compartiment a son propre mesh "cap" plat dont la forme 2D épouse la section horizontale au niveau du deck.** Ce mesh porte le containment spatial. Le material ne fait plus de test de containment — il fait juste du look (normals, fresnel, refraction) et du polish de bord (SceneDepth + DF edge fade).

---

## 1. Vue d'ensemble visuelle

### 1.1 Où le water cap se place dans la scène

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 720 340" width="720" height="340">
  <style>
    .hull { fill: #3a4750; stroke: #1e2a33; stroke-width: 2; }
    .deck { fill: #6b7a85; stroke: #2a363f; stroke-width: 1.5; }
    .water { fill: #4a90e2; fill-opacity: 0.35; stroke: #2c7cc9; stroke-width: 1; }
    .cap { fill: #5bc0ff; stroke: #0066aa; stroke-width: 2.5; stroke-dasharray: 6 3; fill-opacity: 0.55; }
    .label { font-family: monospace; font-size: 13px; fill: #111; }
    .axis { stroke: #888; stroke-width: 1; stroke-dasharray: 4 3; }
    .arrow { fill: #d33; stroke: #d33; stroke-width: 1.5; }
    .title { font-family: monospace; font-size: 15px; font-weight: bold; fill: #111; }
  </style>
  <text x="10" y="22" class="title">Side view — water cap placement in a partially flooded compartment</text>

  <!-- Hull outline (rounded sub silhouette) -->
  <path class="hull" d="M 50 80 Q 120 50 360 50 Q 600 50 670 80 Q 680 180 670 260 Q 600 290 360 290 Q 120 290 50 260 Q 40 180 50 80 Z"/>

  <!-- Decks (horizontal slabs) -->
  <rect class="deck" x="90" y="110" width="540" height="10"/>
  <rect class="deck" x="90" y="180" width="540" height="10"/>
  <rect class="deck" x="90" y="250" width="540" height="10"/>

  <!-- Compartment labels -->
  <text x="640" y="100" class="label">UpperDeck</text>
  <text x="640" y="170" class="label">MainDeck</text>
  <text x="640" y="240" class="label">LowerDeck</text>

  <!-- Water fill in MainDeck (partial) -->
  <rect class="water" x="96" y="150" width="528" height="30"/>

  <!-- Water cap mesh (at water surface Z, horizontal flat plane, shape matches deck cross-section) -->
  <line class="cap" x1="96" y1="150" x2="624" y2="150"/>

  <!-- Arrow + label pointing to cap -->
  <path class="arrow" d="M 200 115 L 310 147" fill="none"/>
  <polygon class="arrow" points="310,147 305,140 300,146"/>
  <text x="110" y="108" class="label" fill="#c22">Water cap mesh</text>
  <text x="110" y="125" class="label" fill="#c22">(horizontal, authored)</text>

  <!-- Deck label -->
  <path class="arrow" d="M 450 215 L 400 187" fill="none"/>
  <polygon class="arrow" points="400,187 406,193 411,187"/>
  <text x="430" y="230" class="label">Deck mesh (SM_Deck_main)</text>

  <!-- Water surface Z label -->
  <line class="axis" x1="625" y1="150" x2="700" y2="150"/>
  <text x="635" y="146" class="label">surface Z</text>

  <!-- Z axis indicator -->
  <line class="axis" x1="20" y1="80" x2="20" y2="290"/>
  <polygon class="arrow" points="20,75 16,85 24,85" fill="#888" stroke="#888"/>
  <text x="5" y="75" class="label" fill="#555">Z</text>
</svg>
```

- Le cap mesh est **plat**, **horizontal**, et sa forme 2D **matche la section intérieure** du compartiment au niveau du deck.
- Il est positionné en Z par `UFloodWaterPlaneComponent` à chaque tick (`SurfaceZ = deck + WaterHeightCm`).
- Quand le compartiment est sec (`WaterLevel01 = 0`), le cap est hidden par le component.
- Quand partiellement inondé, le cap apparaît à la hauteur calculée.

### 1.2 Vue de dessus — forme du cap

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 720 360" width="720" height="360">
  <style>
    .hull-wall { fill: none; stroke: #1e2a33; stroke-width: 3; }
    .bulkhead { stroke: #5a4c3c; stroke-width: 4; fill: none; }
    .cap-outer { fill: #5bc0ff; fill-opacity: 0.35; stroke: #0066aa; stroke-width: 2; stroke-dasharray: 6 3; }
    .cap-inner { stroke: #c22; stroke-width: 1.5; fill: none; stroke-dasharray: 3 2; }
    .label { font-family: monospace; font-size: 13px; fill: #111; }
    .title { font-family: monospace; font-size: 15px; font-weight: bold; fill: #111; }
    .arrow { fill: #d33; stroke: #d33; stroke-width: 1.5; }
    .margin-label { font-family: monospace; font-size: 11px; fill: #c22; }
  </style>
  <text x="10" y="22" class="title">Top view — water cap shape of MainDeck compartment</text>

  <!-- Hull outer boundary (elliptical sub top-down) -->
  <ellipse class="hull-wall" cx="360" cy="195" rx="300" ry="110"/>

  <!-- Bulkheads (divide sub into 3 compartments along X) -->
  <line class="bulkhead" x1="180" y1="125" x2="180" y2="265"/>
  <line class="bulkhead" x1="540" y1="125" x2="540" y2="265"/>

  <!-- Water cap outer boundary (MainDeck compartment, between 2 bulkheads, inside hull) -->
  <path class="cap-outer" d="M 188 140 Q 250 128 360 128 Q 470 128 532 140 L 532 250 Q 470 262 360 262 Q 250 262 188 250 Z"/>

  <!-- Ideal cap boundary (the compartment's inner wall line — what we try to match) -->
  <path class="cap-inner" d="M 185 135 Q 250 123 360 123 Q 470 123 535 135 L 535 255 Q 470 267 360 267 Q 250 267 185 255 Z"/>

  <!-- Label for cap -->
  <text x="340" y="198" class="label" fill="#055">Water cap mesh</text>
  <text x="330" y="215" class="label" fill="#055">(flat 2D polygon)</text>

  <!-- Bulkhead labels -->
  <text x="135" y="115" class="label">SM_BH_Main_Fwd</text>
  <text x="500" y="115" class="label">SM_BH_Main_Aft</text>

  <!-- Hull wall label -->
  <text x="30" y="195" class="label">SM_Hull</text>
  <text x="30" y="210" class="label">(inner)</text>

  <!-- Margin indicator -->
  <line x1="185" y1="303" x2="188" y2="303" stroke="#c22" stroke-width="2"/>
  <line x1="188" y1="299" x2="188" y2="307" stroke="#c22" stroke-width="1.5"/>
  <line x1="185" y1="299" x2="185" y2="307" stroke="#c22" stroke-width="1.5"/>
  <text x="195" y="307" class="margin-label">2 cm margin (edge fade)</text>

  <!-- Arrow to show offset -->
  <path class="arrow" d="M 550 295 L 535 268" fill="none"/>
  <polygon class="arrow" points="535,268 542,273 541,265"/>
  <text x="540" y="312" class="label" fill="#c22">inset cap 2 cm from walls</text>
  <text x="540" y="327" class="label" fill="#c22">→ SoftClip band can fade</text>
</svg>
```

- La forme du cap = **polygon plat 2D** épousant la section horizontale intérieure du compartiment.
- **Offset de 2 cm vers l'intérieur** depuis les walls → ça laisse 2 cm pour que le `SoftClip` (DistanceToNearestSurface) puisse faire son fade de bord propre.
- Pas besoin de subdivision interne — un seul N-gon ou quelques triangles suffisent (le cap ne porte pas de normal variée, c'est tout plat).

---

## 2. Changements code (déjà appliqués)

### 2.1 `UCompartmentVolumeComponent`

Nouveau slot `WaterPlaneMeshOverride` en [CompartmentVolumeComponent.h:65-73](Source/Sub3D/Submarine/CompartmentVolumeComponent.h#L65-L73) :

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Flood")
TObjectPtr<UStaticMesh> WaterPlaneMeshOverride = nullptr;
```

### 2.2 `UFloodWaterPlaneComponent::EnsurePlaneMesh`

Sélection du mesh avec priorité au cap authoré, sinon fallback engine plane 80 m ([FloodWaterPlaneComponent.cpp:81-110](Source/Sub3D/Submarine/FloodWaterPlaneComponent.cpp#L81-L110)).

Le cap authoré est utilisé à scale `(1, 1, 1)`. Ses dimensions authorées deviennent authoritatives.

**Rien d'autre ne change** : `UFloodWaterPlaneComponent::ApplyWaterState` continue de positionner le plan en Z et toggler sa visibilité, quel que soit le mesh.

---

## 3. Authoring Blender — étape par étape

### 3.1 Workflow d'ensemble

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 440" width="760" height="440">
  <style>
    .step { fill: #f0f4f8; stroke: #334; stroke-width: 2; rx: 6; }
    .step-title { font-family: monospace; font-size: 13px; font-weight: bold; fill: #111; }
    .step-body { font-family: monospace; font-size: 11px; fill: #333; }
    .arrow { stroke: #667; stroke-width: 2; fill: none; }
    .arrow-head { fill: #667; }
    .note { font-family: monospace; font-size: 11px; fill: #c22; font-style: italic; }
    .title { font-family: monospace; font-size: 15px; font-weight: bold; fill: #111; }
  </style>
  <text x="10" y="22" class="title">Blender workflow — create a water cap mesh from a deck mesh</text>

  <!-- Step 1 -->
  <rect class="step" x="20" y="50" width="180" height="60"/>
  <text x="30" y="72" class="step-title">1. Open deck mesh</text>
  <text x="30" y="90" class="step-body">Import SM_Deck_main.fbx</text>
  <text x="30" y="104" class="step-body">(or reuse source .blend)</text>

  <!-- Arrow -->
  <path class="arrow" d="M 110 115 L 110 145"/>
  <polygon class="arrow-head" points="110,150 106,140 114,140"/>

  <!-- Step 2 -->
  <rect class="step" x="20" y="150" width="180" height="70"/>
  <text x="30" y="172" class="step-title">2. Duplicate + separate</text>
  <text x="30" y="190" class="step-body">Select all → Shift+D →</text>
  <text x="30" y="204" class="step-body">Esc → P → "Selection"</text>
  <text x="30" y="218" class="step-body">(new object: Cap_MainDeck)</text>

  <!-- Arrow -->
  <path class="arrow" d="M 110 225 L 110 255"/>
  <polygon class="arrow-head" points="110,260 106,250 114,250"/>

  <!-- Step 3 -->
  <rect class="step" x="20" y="260" width="180" height="70"/>
  <text x="30" y="282" class="step-title">3. Flatten in Z</text>
  <text x="30" y="300" class="step-body">Edit mode → select all</text>
  <text x="30" y="314" class="step-body">S → Z → 0 → Enter</text>
  <text x="30" y="328" class="step-body">(all verts same Z now)</text>

  <!-- Arrow -->
  <path class="arrow" d="M 110 335 L 110 370"/>
  <polygon class="arrow-head" points="110,375 106,365 114,365"/>

  <!-- Step 4 -->
  <rect class="step" x="20" y="380" width="180" height="50"/>
  <text x="30" y="402" class="step-title">4. Apply + origin center</text>
  <text x="30" y="420" class="step-body">Object → Origin → Geometry</text>

  <!-- Arrow column 1 → column 2 -->
  <path class="arrow" d="M 210 405 L 260 405"/>
  <polygon class="arrow-head" points="265,405 255,401 255,409"/>

  <!-- Step 5 -->
  <rect class="step" x="270" y="380" width="200" height="50"/>
  <text x="280" y="402" class="step-title">5. Keep only boundary</text>
  <text x="280" y="420" class="step-body">Select interior verts → X → Vertices</text>

  <!-- Arrow up from 5 -->
  <path class="arrow" d="M 370 375 L 370 340"/>
  <polygon class="arrow-head" points="370,335 366,345 374,345"/>

  <!-- Step 6 -->
  <rect class="step" x="270" y="260" width="200" height="70"/>
  <text x="280" y="282" class="step-title">6. Fill face</text>
  <text x="280" y="300" class="step-body">Select all boundary verts</text>
  <text x="280" y="314" class="step-body">F (fill) or Alt+F (beautify)</text>
  <text x="280" y="328" class="step-body">Single N-gon or triangulated</text>

  <!-- Arrow up from 6 -->
  <path class="arrow" d="M 370 255 L 370 225"/>
  <polygon class="arrow-head" points="370,220 366,230 374,230"/>

  <!-- Step 7 -->
  <rect class="step" x="270" y="150" width="200" height="70"/>
  <text x="280" y="172" class="step-title">7. Inset boundary 2 cm</text>
  <text x="280" y="190" class="step-body">Select boundary loop</text>
  <text x="280" y="204" class="step-body">I (inset) → type 0.02 → Enter</text>
  <text x="280" y="218" class="step-body">Delete outer ring (keep inner)</text>

  <!-- Arrow up from 7 -->
  <path class="arrow" d="M 370 145 L 370 110"/>
  <polygon class="arrow-head" points="370,105 366,115 374,115"/>

  <!-- Step 8 -->
  <rect class="step" x="270" y="50" width="200" height="60"/>
  <text x="280" y="72" class="step-title">8. Rename + cleanup</text>
  <text x="280" y="90" class="step-body">Rename: SM_WaterCap_MainDeck</text>
  <text x="280" y="104" class="step-body">Remove doubles (M → By Distance)</text>

  <!-- Arrow column 2 → column 3 -->
  <path class="arrow" d="M 480 80 L 530 80"/>
  <polygon class="arrow-head" points="535,80 525,76 525,84"/>

  <!-- Step 9 -->
  <rect class="step" x="540" y="50" width="210" height="80"/>
  <text x="550" y="72" class="step-title">9. FBX export</text>
  <text x="550" y="90" class="step-body">File → Export → FBX</text>
  <text x="550" y="104" class="step-body">Scale 1.0, Apply Transform: ON</text>
  <text x="550" y="118" class="step-body">Selected objects only</text>

  <!-- Arrow down from 9 -->
  <path class="arrow" d="M 640 135 L 640 160"/>
  <polygon class="arrow-head" points="640,165 636,155 644,155"/>

  <!-- Step 10 -->
  <rect class="step" x="540" y="170" width="210" height="80"/>
  <text x="550" y="192" class="step-title">10. UE import</text>
  <text x="550" y="210" class="step-body">Drop .fbx in Content/Sub3D/</text>
  <text x="550" y="224" class="step-body">FirstPlayableRun/Meshes/WaterCaps/</text>
  <text x="550" y="238" class="step-body">Collision: None. Nanite: No.</text>

  <!-- Arrow down from 10 -->
  <path class="arrow" d="M 640 255 L 640 290"/>
  <polygon class="arrow-head" points="640,295 636,285 644,285"/>

  <!-- Step 11 -->
  <rect class="step" x="540" y="300" width="210" height="80"/>
  <text x="550" y="322" class="step-title">11. Assign to CV in BP</text>
  <text x="550" y="340" class="step-body">Open BP_Submarine_Craniata</text>
  <text x="550" y="354" class="step-body">Select CV MainDeck</text>
  <text x="550" y="368" class="step-body">Set WaterPlaneMeshOverride = SM</text>

  <!-- Final note -->
  <text x="280" y="420" class="note">Repeat for each of 5 compartments (LowerDeck, MainDeck, MainDeck1, UpperDeck, Airlock)</text>
</svg>
```

### 3.2 Détail de chaque étape

**Étape 1 — Ouvrir le deck mesh source**
- Si tu as le `.blend` source de Craniata : ouvre-le et prends l'objet `SM_Deck_main` (ou équivalent par compartment).
- Sinon : importe le `.fbx` depuis `Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/SM_Deck_main.fbx` (si exporté côté git) ou recrée via l'authoring pipeline.
- Le deck mesh te donne la forme XY de référence : la water cap doit matcher sa cross-section.

**Étape 2 — Dupliquer et séparer**
- En Object mode, sélectionne le deck mesh.
- `Shift+D` → `Esc` (annule le déplacement, garde la duplication en place).
- `P` → `Selection` → le duplicate devient un objet séparé.
- Renomme : `Cap_MainDeck` (on finalisera le nom en étape 8).

**Étape 3 — Flatten en Z**
- Enter Edit mode (`Tab`).
- `A` (select all).
- `S` → `Z` → `0` → `Enter`. Tous les vertices s'alignent sur le Z moyen du deck → le mesh devient plat.
- Le deck mesh d'origine peut avoir des normales orientées vers le haut (sol) ou vers le bas (plafond vu de dessous) — peu importe, on ne s'en sert que pour la forme XY.

**Étape 4 — Origin to geometry**
- Object mode.
- `Object → Set Origin → Origin to Geometry`. Centre l'origin du mesh sur sa médiane. Important pour que le placement en scene dans UE soit prévisible.

**Étape 5 — Garder uniquement le boundary loop**
- Edit mode, Face ou Edge select.
- `Alt+click` sur une edge du contour extérieur pour sélectionner le loop.
- `Ctrl+I` (invert selection) → tu as tous les vertices intérieurs sélectionnés.
- `X` → `Vertices` → delete. Tu ne gardes que le boundary.

Astuce : si le deck a plusieurs "trous" (ex. escalier, passage), garde aussi les boundaries internes — la water cap aura des trous là où il n'y a pas d'eau.

**Étape 6 — Fill face**
- Select all boundary verts (`A`).
- `F` → ça fait un seul N-gon plat.
- Alternative : `Alt+F` (beautify fill) qui triangule en triangles bien formés. Les deux marchent, le N-gon est plus simple.

**Étape 7 — Inset 2 cm vers l'intérieur**
- En Face select, sélectionne le N-gon.
- `I` (inset) → tape `0.02` (= 2 cm, Blender unit = 1 m par défaut) → `Enter`.
- Tu as maintenant deux boundaries : extérieur (original) et intérieur (offset de 2 cm).
- Select le ring intérieur → `Ctrl+I` → inverse → tu as le boundary extérieur + la face centrale.
- Supprime le boundary extérieur : `X` → `Vertices`.
- Re-fill la face centrale : `A` → `F`.

Résultat : ton cap mesh fait 2 cm de marge par rapport aux walls originaux → ça laisse la place au `SoftClip` du material de faire son fade de bord.

**Étape 8 — Rename + cleanup**
- Object mode.
- `F2` (rename) ou double-click dans l'outliner.
- Nom recommandé (doit matcher la convention UE) : `SM_WaterCap_MainDeck`, `SM_WaterCap_LowerDeck`, etc.
- Edit mode → `A` → `M` → `By Distance` → merge les vertex doublons (threshold 0.0001).
- Vérifie que les normales pointent vers le haut : `Alt+N` → `Recalculate Outside`.

**Étape 9 — FBX export**
- `File → Export → FBX (.fbx)`.
- Settings :
  - **Selected Objects** : ON (seulement ton cap)
  - **Scale** : 1.0
  - **Apply Transform** : ON (évite le pb d'orientation UE)
  - **Forward** : -Z Forward (convention UE)
  - **Up** : Y Up
  - **Mesh → Smoothing** : Face
  - **Mesh → Apply Modifiers** : ON si tu as des modifiers
- Sauvegarde comme `SM_WaterCap_MainDeck.fbx`.

**Étape 10 — Import dans UE**
- Drop le `.fbx` dans `Content/Sub3D/FirstPlayableRun/Meshes/WaterCaps/` (crée le dossier s'il n'existe pas).
- Import settings :
  - **Auto Generate Collision** : OFF (le cap est purement visuel, aucune collision)
  - **Nanite Enabled** : OFF (mesh trivial, nanite overhead inutile)
  - **Generate Lightmap UVs** : OFF (pas de lightmap, c'est de la translucence)
  - **Build Simple Collision** : OFF
  - **LODs** : none
- Post-import : ouvre le Static Mesh Editor, vérifie dans **Build Settings** que **Collision Complexity** = `Default` et que **Distance Field Resolution Scale** peut rester à 0 (pas de DF nécessaire pour le cap).

**Étape 11 — Assignation dans BP Craniata**
- Ouvre `Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata`.
- Components panel → sélectionne `CV MainDeck`.
- Details panel → section `Compartment|Flood` → slot **Water Plane Mesh Override** → assigne `SM_WaterCap_MainDeck`.
- Compile + Save.
- Répète pour les 4 autres CV : `CV LowerDeck → SM_WaterCap_LowerDeck`, etc.

### 3.3 Cas particulier — compartments avec trous (escaliers, passages)

Si le MainDeck a un trou pour un escalier descendant vers le LowerDeck : ton cap doit avoir le même trou, sinon l'eau s'affichera sur l'escalier.

- En étape 5, quand tu sélectionnes le boundary extérieur, sélectionne aussi le boundary du trou.
- En étape 6, après `F`, Blender fait un N-gon entre les deux boundaries → forme "donut" avec trou interne.
- Reste cohérent pour l'inset : inset sur le boundary extérieur ET sur le boundary intérieur (pour que l'eau fade aussi au bord du trou).

### 3.4 Validation dans Blender avant export

- `Tab` en edit mode, `A` (select all), stats en haut à droite doivent dire :
  - **Verts** : ~8–50 (mesh simple)
  - **Faces** : 1 N-gon OU quelques triangles après beautify fill
- Visualise de côté (`Numpad 1` ou `3`) : toutes les faces doivent être **strictement horizontales**. Aucune variation en Z.

---

## 4. Architecture Material Functions

### 4.1 Pourquoi refactorer en MFs maintenant

Avant (état actuel) : tout est inline dans `M_CompartmentWater` → 403 instructions, graph chargé, pas réutilisable, toute édition de look impacte le graph entier.

Après (target) : 3 MFs séparées, chacune testable/éditable isolément, et `M_CompartmentWater` devient un thin combiner :

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 780 360" width="780" height="360">
  <style>
    .mf-box { fill: #e8f0fe; stroke: #2d6cdf; stroke-width: 2; rx: 8; }
    .main-box { fill: #fff4e0; stroke: #c8841c; stroke-width: 2; rx: 8; }
    .output-box { fill: #d4f0d4; stroke: #2e7d32; stroke-width: 2; rx: 8; }
    .mf-title { font-family: monospace; font-size: 13px; font-weight: bold; fill: #111; }
    .mf-sub { font-family: monospace; font-size: 10.5px; fill: #333; }
    .pin { fill: #fff; stroke: #333; stroke-width: 1; rx: 2; }
    .wire { stroke: #667; stroke-width: 2; fill: none; }
    .wire-label { font-family: monospace; font-size: 10px; fill: #556; }
    .title { font-family: monospace; font-size: 15px; font-weight: bold; fill: #111; }
  </style>
  <text x="10" y="22" class="title">Material architecture — M_CompartmentWater becomes a thin combiner of 3 MFs</text>

  <!-- MF 1: Look -->
  <rect class="mf-box" x="20" y="60" width="220" height="110"/>
  <text x="30" y="82" class="mf-title">MF_CompartmentWater_Look</text>
  <text x="30" y="102" class="mf-sub">• normal scrolling (waves)</text>
  <text x="30" y="118" class="mf-sub">• water tint (VectorParameter)</text>
  <text x="30" y="134" class="mf-sub">• roughness, IOR, fresnel</text>
  <text x="30" y="150" class="mf-sub">output: SubstrateSlabBSDF</text>

  <!-- MF 2: Edge Polish -->
  <rect class="mf-box" x="20" y="190" width="220" height="90"/>
  <text x="30" y="212" class="mf-title">MF_CompartmentWater_EdgePolish</text>
  <text x="30" y="232" class="mf-sub">• HullMask (SceneDepth/PixelDepth)</text>
  <text x="30" y="248" class="mf-sub">• SoftClip (DistToNearestSurface)</text>
  <text x="30" y="264" class="mf-sub">output: scalar mask [0..1]</text>

  <!-- MF 3: Level Gate -->
  <rect class="mf-box" x="20" y="300" width="220" height="50"/>
  <text x="30" y="322" class="mf-title">MF_WaterLevelGate</text>
  <text x="30" y="340" class="mf-sub">output: WaterLevel01 (pass-through)</text>

  <!-- M_CompartmentWater -->
  <rect class="main-box" x="320" y="130" width="220" height="160"/>
  <text x="330" y="152" class="mf-title">M_CompartmentWater</text>
  <text x="330" y="170" class="mf-sub">(thin combiner)</text>
  <text x="330" y="192" class="mf-sub">A (slab)  ←  Look</text>
  <text x="330" y="208" class="mf-sub">Weight  ←  Multiply(</text>
  <text x="345" y="224" class="mf-sub">EdgePolish,</text>
  <text x="345" y="240" class="mf-sub">WaterLevelGate)</text>
  <text x="330" y="264" class="mf-sub">Substrate Coverage Weight</text>

  <!-- Output root -->
  <rect class="output-box" x="600" y="170" width="170" height="80"/>
  <text x="612" y="192" class="mf-title">Material Root</text>
  <text x="612" y="212" class="mf-sub">Front Material ← Weight</text>
  <text x="612" y="228" class="mf-sub">Refraction (IOR)</text>
  <text x="612" y="244" class="mf-sub">BlendMode, TwoSided, ...</text>

  <!-- Wires -->
  <path class="wire" d="M 240 115 Q 280 115 320 175" fill="none"/>
  <polygon fill="#667" points="320,175 315,168 322,166"/>
  <text x="250" y="100" class="wire-label">SubstrateSlabBSDF</text>

  <path class="wire" d="M 240 240 Q 280 240 320 225" fill="none"/>
  <polygon fill="#667" points="320,225 312,220 312,228"/>
  <text x="255" y="230" class="wire-label">scalar mask</text>

  <path class="wire" d="M 240 325 Q 280 325 320 265" fill="none"/>
  <polygon fill="#667" points="320,265 313,268 315,260"/>
  <text x="250" y="340" class="wire-label">scalar 0..1</text>

  <path class="wire" d="M 540 205 L 600 205" fill="none"/>
  <polygon fill="#667" points="605,205 595,201 595,209"/>
  <text x="545" y="195" class="wire-label">FrontMaterial</text>
</svg>
```

### 4.2 Création des 3 Material Functions

Dans `Content/Sub3D/Material/Functions/` (crée le dossier) — clic droit dans le Content Browser → `Material & Textures → Material Function`.

#### MF_CompartmentWater_Look

- **Purpose** : tout ce qui fait le look de l'eau (hors containment).
- **Outputs** :
  - `SubstrateMaterial` (Substrate Slab BSDF)
- **Inputs** (configurables pour tuning) :
  - `TintColor` (Vector3, default `0.12, 0.28, 0.60`)
  - `Roughness` (Scalar, default `0.04`)
  - `WaveScale` (Scalar, default `0.5`)
  - `WaveSpeedA`, `WaveSpeedB` (Scalar)
  - `NormalIntensity` (Scalar, default `20.0`)
- **Implémentation** : déplace les nodes actuels qui font le slab BSDF (les MaterialGraphNode_3, 4, 5, 6, 7, 8, et la chaîne de normals avec les 2 Panner + TextureSample + BlendNormals) dans cette MF.

#### MF_CompartmentWater_EdgePolish

- **Purpose** : mask de polish de bord uniquement (PAS de containment — le cap mesh porte le containment).
- **Outputs** :
  - `EdgeMask01` (Scalar 0..1)
- **Inputs** :
  - `HullMaskSoftness` (Scalar, default `4.0`)
  - `EdgeSoftnessCm` (Scalar, default `5.0`) — **corrige le typo** `EdgeSofnessCm` ici
- **Implémentation** :
  ```
  HullMask = Saturate((SceneDepth − PixelDepth) / HullMaskSoftness)
  SoftClip = Saturate(DistanceToNearestSurface(AbsoluteWorldPosition) / EdgeSoftnessCm)
  Output = HullMask × SoftClip
  ```
- **Note importante** : maintenant que le cap mesh porte le containment, le `SoftClip` n'a **plus besoin de faire disparaître le plan en pleine mer** — il fait juste le fade au contact des walls internes. La formule actuelle `DistToNearestSurface / soft → saturate` marche bien pour ça puisqu'elle va vers 0 au contact des walls et vers 1 au milieu du compartment.

#### MF_WaterLevelGate

- **Purpose** : isoler le level threshold pour pouvoir le tuner sans toucher au reste.
- **Outputs** :
  - `Level01` (Scalar)
- **Inputs** :
  - (aucun — ça lit le `WaterLevel01` ScalarParameter directement)
- **Implémentation** :
  ```
  Output = WaterLevel01 (ScalarParameter)
  ```
- (MF minimale, peut évoluer post-FP avec un fade-in progressif, une courbe de transition, etc.)

### 4.3 Refactor de `M_CompartmentWater`

Après création des 3 MFs, le graph de `M_CompartmentWater` devient :

1. **Supprime le Custom node HLSL ajouté précédemment** (celui avec le carré rouge). Reconnecte ce qui était avant lui.
2. **Supprime tous les nodes** qui font maintenant partie des MFs (normal chain, slab BSDF, scene depth, dist to nearest surface).
3. **Ajoute 3 nodes `MaterialFunctionCall`** pointant vers les MFs créées (clic droit → `Material Function`).
4. **Câblage final** :
   - `MF_CompartmentWater_Look.SubstrateMaterial` → pin `A` du `Substrate Coverage Weight`
   - `Multiply(MF_EdgePolish.EdgeMask01, MF_WaterLevelGate.Level01)` → pin `Weight` du `Substrate Coverage Weight`
   - `Substrate Coverage Weight.Output` → pin `Front Material` du Root
   - (Refraction chain : `RefractionStrength × WaterLevel01 + 1.0` → pin `Refraction (IOR)` du Root — reste inchangée)
5. **Material settings Root** inchangés : Blend Mode `TranslucentColoredTransmittance`, Refraction Method `Index Of Refraction`, Two Sided `true`, Translucency Pass `After Motion Blur`.

### 4.4 Material Instance `MI_CompartmentWater`

Une fois `M_CompartmentWater` refactorisé et clean :

- Clic droit sur `M_CompartmentWater` → `Create Material Instance`.
- Nom : `MI_CompartmentWater`.
- Expose les scalars et vectors des MFs dans l'instance pour tuning runtime (tous hérités automatiquement).
- Dans `BP_Submarine_Craniata`, change le slot `Default Water Material` : `M_CompartmentWater` → `MI_CompartmentWater`.

Avantage : designer peut tuner tous les params (WaveScale, TintColor, EdgeSoftnessCm, etc.) sans toucher au base material ni recompiler le shader à chaque changement.

---

## 5. Checklist de validation PIE

Après authoring + refactor + assignation :

- [ ] Les 5 CV ont chacun un `WaterPlaneMeshOverride` assigné
- [ ] `MI_CompartmentWater` est assigné à `Default Water Material` de la BP
- [ ] En PIE, breach d'un compartment → water visible uniquement dans ce compartment
- [ ] Vue extérieure du sub en plein océan → aucun plan visible qui dépasse la silhouette
- [ ] Vue spectateur free-cam → même comportement
- [ ] Vue à travers une porte ouverte entre 2 compartments → water du compartment voisin visible à travers la porte (comportement désiré)
- [ ] Bord du water au contact des walls → fade doux (pas de ligne dure)

---

## 6. Corrections associées (à faire en commits séparés)

### 6.1 Typo `EdgeSofnessCm`

Actuel dans `M_CompartmentWater` : `EdgeSofnessCm` (missing `t`). Renommer en `EdgeSoftnessCm` quand tu crées `MF_CompartmentWater_EdgePolish` (le nom devient un input de la MF, propre).

### 6.2 Naming `DefaultWaterPlaneWorldSizeCm` vs plan

Le plan `2026-04-23_flood_visuals_architecture.md` ligne 123 dit `DefaultWaterPlaneSizeCm`, le code en `SubmarineBase.h:216` dit `DefaultWaterPlaneWorldSizeCm`. **Décision** : garder le code (`World` est plus explicite) et amender le plan pour cohérence.

### 6.3 Amendement plan flood-visuals

Ajouter section "Containment mechanism" dans `reports/plans/2026-04-23_flood_visuals_architecture.md` :

> **Containment** : porté par la géométrie du mesh cap assigné à chaque `UCompartmentVolumeComponent.WaterPlaneMeshOverride`. Le material ne fait PAS de test de containment (SDF ray-march rejeté suite à review 2026-04-24). Le material est responsable du look + du polish de bord via `MF_CompartmentWater_EdgePolish`.

---

## 7. Status

- [x] C++ `WaterPlaneMeshOverride` slot sur `UCompartmentVolumeComponent`
- [x] C++ consumption dans `UFloodWaterPlaneComponent::EnsurePlaneMesh`
- [x] Build Sub3DEditor validé
- [ ] Authoring Blender des 5 caps (§3)
- [ ] Refactor MFs + refactor `M_CompartmentWater` + `MI_CompartmentWater` (§4)
- [ ] Assignation dans BP Craniata (§3.2 étape 11)
- [ ] Validation PIE (§5)
- [ ] Fix typo `EdgeSofnessCm` (§6.1)
- [ ] Amendement plan flood-visuals (§6.3)
