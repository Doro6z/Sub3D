# Sub3D — Guide éditeur FirstPlayableRun

Date : 2026-04-10
Scope : actions manuelles éditeur restantes après fermeture M1+M2 côté C++
Contrats d'autorité :
- `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
- `reports/plans/2026-04-10_firstplayablerun_editor_session_contract.md`

UE : 5.7
Workspace : `C:\Dev\Sub3D`

---

## 0. Blockers

Points constatés dans le code réel qui divergent du rapport de session autonome. Traiter avant d'exécuter le guide.

### Blocker 1 — Les PMCs générés n'ont aucun matériau assigné dans le code

**Observation** : `SubmarineGeneratedGeometryComponent.cpp` crée les `UProceduralMeshComponent` via `CreateMeshSection_LinearColor` sans appeler `SetMaterial`. Aucune autre partie du code (`SubmarineBase.cpp`, `SubmarineMeshBuilder.cpp`, `SubmarineGenerator.cpp`) n'assigne de matériau aux PMCs générés.

**Conséquence** : en PIE, les PMCs affichent le matériau par défaut du PMC (fallback UE, généralement `WorldGridMaterial`). Aucun asset `M_Hull*`, `M_Floor*`, `M_Submarine*` n'existe dans `Content/` (vérifié par Glob).

**Impact sur l'étape « matériaux two-sided »** du rapport de session : elle ne peut pas être exécutée telle qu'écrite. Il n'y a aucun matériau coque ni sol à ouvrir pour activer le two-sided.

**Ce que le guide fait à la place** : la section 4 documente une action minimale purement éditeur (création d'un matériau trivial two-sided + assignation via construction script sur `BP_Submarine_FPRun`). Si tu refuses de modifier la logique BP, marque la dette « materials post-FP » et saute l'étape en acceptant l'aspect grille par défaut.

### Blocker 2 — BP_SubDoor : parenté non confirmée binairement

**Observation** : l'asset `Content/Sub3D/Blueprint/SubBP/BP_SubDoor.uasset` existe. Le code C++ ne peut pas révéler sa classe parente à travers le .uasset binaire depuis le terminal.

**Conséquence** : on ne peut pas affirmer avec certitude que `BP_SubDoor` hérite de `ASubDoorActor`. Si le BP hérite d'une autre classe (legacy), il ne sera pas assignable à `ASubmarineBase::GeneratorDoorActorClass` qui est typé `TSubclassOf<ASubDoorActor>`.

**Ce que le guide fait** : la section 7 ouvre le BP dans l'éditeur et inspecte le Class Defaults > Details > Parent Class. Si la parenté est bonne, on l'utilise. Si elle ne l'est pas, le guide passe au fallback « créer BP_Door depuis `ASubDoorActor` ».

### Blocker 3 — Instance legacy dans L_FP_GeneratorRun : non vérifiable hors éditeur

**Observation** : l'asset `Content/Sub3D/Proto04C/CompilerV2/BP_SubmarineBakedRuntimeActor_1.uasset` existe comme classe Blueprint. Le log PIE précédemment partagé mentionne une instance `BP_SubmarineBakedRuntimeActor_1_C_1` active dans le level, produisant des warnings `[LEGACY]`. L'instance est dans le `.umap` binaire non lisible depuis le terminal.

**Ce que le guide fait** : la section 5 demande d'ouvrir `L_FP_GeneratorRun`, d'inspecter le World Outliner, et de retirer toute instance de `BP_SubmarineBakedRuntimeActor_1` (ou tout actor émettant des `[LEGACY]` warnings). Le contrat de session (section 6, rule Log check) impose l'absence de ces warnings pour le pass.

---

## 1. Ordre d'exécution

1. Redémarrage propre éditeur — section 2
2. Vérification envelope FP — section 3
3. Matériaux PMCs — section 4 (dépend Blocker 1)
4. Nettoyage `L_FP_GeneratorRun` — section 5
5. Setup `BP_Submarine_FPRun` — section 6
6. BP_Door : vérification ou création — section 7
7. Itération forme avec `Rebuild From Spec` — section 8
8. Protocole PIE avec cheats — section 9
9. Validation `no [LEGACY] warnings` — section 10
10. Checklist finale — section 11

Chaque section a : action obligatoire, critère de validation, blocage éventuel.

---

## 2. Redémarrage éditeur

### Action obligatoire

1. Si l'éditeur UE est ouvert, fermer complètement (File > Exit ou Alt+F4). Live Coding empêche les nouvelles DLLs de charger tant que l'éditeur tourne.
2. Relancer l'éditeur via le raccourci ou en double-cliquant sur `C:\Dev\Sub3D\Sub3D.uproject`.

### Validation attendue

- Dans l'Output Log au démarrage, cherche une ligne `LogModuleManager: Module 'Sub3D' loaded` ou équivalent (non vérifié texte exact).
- Dans le menu Tools > Debug > Debugger, vérifier qu'aucun module en erreur n'apparaît.
- Ouvrir le Content Browser : aucune icône d'asset rouge/invalide dans `Content/Sub3D/FirstPlayableRun/`.

### Blocage éventuel

- Si l'éditeur refuse de lancer avec une erreur `Module could not be loaded` : le build C++ est corrompu. Revenir à `Build.bat Sub3DEditor Win64 Development -Project="..."` hors éditeur.

---

## 3. Vérification envelope FP

### Action obligatoire

1. Ouvrir le Content Browser, naviguer vers `Content/Sub3D/FirstPlayableRun/`.
2. Double-cliquer sur `DA_Envelope_FPRun` pour ouvrir le Data Asset editor.
3. Dans le panneau Details, vérifier la présence des champs suivants dans la catégorie `Envelope|BowStern` :
   - `Bow Cap Length Cm` (défaut 240)
   - `Stern Cap Length Cm` (défaut 320)
   - `Bow Sharpness` (défaut 1.0)
   - `Stern Sharpness` (défaut 1.0)
   - `Body Length Fraction` (défaut 0.55)
   - `Control Ring Count` (défaut 9)
4. Vérifier l'absence du champ `Radius Profile` (supprimé en M1.2).
5. Vérifier la présence des champs conservés : `Spine Length Cm`, `Default Radius Cm`, `Exterior Hull Offset Cm`, `Section Exponent`, `Width To Height Ratio`, `Bow Profile`, `Stern Profile`, `Bow Taper Fraction`, `Stern Taper Fraction`.
6. Ne rien modifier. Sauver avec Ctrl+S si l'éditeur signale que l'asset a été mis à jour vers le nouveau schéma (migration automatique des UPROPERTY).

### Validation attendue

- Tous les champs listés sont présents.
- `Radius Profile` n'apparaît nulle part dans l'asset.
- Sauvegarde réussie sans popup d'erreur.

### Blocage éventuel

- Si `Radius Profile` apparaît encore : le build C++ n'a pas été chargé. Retour section 2.
- Si l'un des 6 nouveaux champs manque : l'asset est d'une version antérieure et n'a pas migré. Re-save avec Ctrl+S. Si le champ reste absent après re-save, le build C++ n'inclut pas M1.2 — stop, vérifier `Source/Sub3D/Submarine/Generator/SubmarineGeneratorEnvelopeDef.h` contient bien les 6 champs.

### Hors scope

- Ne pas modifier les valeurs par défaut maintenant. L'itération de forme est en section 8.

---

## 4. Matériaux PMCs

Voir Blocker 1.

### Option A — Matériau minimal two-sided + BP wiring

Cette option impose d'ajouter une logique Blueprint sur `BP_Submarine_FPRun`. Durée estimée : 20 minutes.

#### Action obligatoire

1. Content Browser > click droit dans `Content/Sub3D/Material/` > Material.
2. Nommer `M_GeneratedHull_TwoSided`.
3. Double-cliquer pour ouvrir le Material Editor.
4. Dans Details > Material :
   - `Two Sided` : coché
   - `Shading Model` : `Default Lit`
5. Dans le graphe, tirer un Constant3Vector (raccourci `3` + click) → Base Color. Valeur RGB : (0.2, 0.2, 0.22) (gris métallique neutre, non vérifié comme valeur finale).
6. Apply + Save.
7. Répéter pour un second matériau `M_GeneratedInterior_TwoSided` avec RGB (0.4, 0.4, 0.42) ou un Constant3Vector différent pour distinguer intérieur et extérieur dans le viewport.

#### Wiring dans BP_Submarine_FPRun

1. Content Browser > double-clic sur `Content/Sub3D/FirstPlayableRun/BP_Submarine_FPRun`.
2. Passer en Event Graph.
3. Ajouter un event `ReceiveBeginPlay` (si absent). Connecter la sortie `Parent: BeginPlay` à une chaîne d'actions (drag output pin → Add Parent Call si pas déjà présent).
4. Après le Parent BeginPlay, ajouter :
   - Node `Get Generated Geometry` (variable du pawn parent — catégorie `Components`)
   - Node `Get Render Components` sur Generated Geometry (retourne TArray)
   - Node `For Each Loop` sur la liste
   - Dans le body du loop : `Set Material` sur l'élément → index 0 → matériau `M_GeneratedHull_TwoSided`
5. Compile + Save.

#### Validation attendue

- En PIE, les PMCs de la coque apparaissent avec le matériau assigné (couleur uniforme grise au lieu du quadrillage `WorldGridMaterial`).
- Vue sous différents angles : pas de faces manquantes (two-sided actif).

#### Blocage éventuel

- Si `Get Generated Geometry` n'apparaît pas dans les nodes : le variable est `VisibleAnywhere` dans `SubmarineBase.h` ligne 124, normalement accessible depuis BP. Si absent, vérifier que BP_Submarine_FPRun dérive bien de `ASubmarineBase` (Class Settings > Parent Class).
- Si le matériau ne s'applique pas (quadrillage toujours visible) : le loop ne trouve pas de composants. Les PMCs sont créés en BeginPlay (Step 4 de `ASubmarineBase::BeginPlay`). Le BP_BeginPlay avec `Parent: BeginPlay` s'exécute APRÈS le parent, donc après la création. Si ça ne marche pas, ajouter un délai `Delay 0.1` ou utiliser `OnPostPossessed` / un event custom.

### Option B — Accepter la dette visuelle

#### Action obligatoire

1. Ne rien faire sur les matériaux.
2. Ajouter une ligne dans `reports/backlog/post_fp_debt.md` (fichier à créer si absent) :
   ```
   - PMCs générés sans matériau assigné ; affichage WorldGridMaterial par défaut.
     À corriger post-FP : soit SetMaterial en C++ dans ApplySectionToPMC,
     soit wiring BP construction script sur BP_Submarine_FPRun.
   ```

#### Validation attendue

- La dette est documentée.
- Le FP est jouable fonctionnellement. L'aspect visuel reste quadrillé WorldGrid.

#### Recommandation

Pour un FP orienté validation structurelle et logs, **option B** suffit. L'option A est meilleure si tu veux un aspect lisible dans les prises de vue démo.

---

## 5. Nettoyage de L_FP_GeneratorRun

### Action obligatoire

1. File > Open Level > `Content/Maps/L_FP_GeneratorRun.umap`.
2. Ouvrir le World Outliner (Window > World Outliner si caché).
3. Dans la barre de filtre du Outliner, taper `Baked` puis `Runtime` puis `Legacy` pour identifier toute instance ou sous-dérivée de `BP_SubmarineBakedRuntimeActor_1`.
4. Pour chaque instance trouvée :
   - Click droit > Delete
   - Ou alternativement : Details panel > section Rendering > décocher `Actor Hidden In Game` ne suffit pas, il faut supprimer l'actor car son `BeginPlay` tire les warnings `[LEGACY]` indépendamment de la visibilité.
5. Sauver le level avec Ctrl+S.

### Validation attendue

- Le World Outliner du level FP ne contient plus aucun actor dont le nom commence par `BP_SubmarineBakedRuntimeActor`.
- Le level reste valide (pas de popup d'erreur de référence cassée au save).

### Blocage éventuel

- Si un autre actor de type legacy apparaît et émet `[LEGACY]` en PIE (vérification à faire en section 10), le supprimer de la même façon.
- Si l'instance est référencée par d'autres actors (ex : PlayerStart pointant dessus), UE affichera un popup. Décider au cas par cas : soit réassigner la référence vers `BP_Submarine_FPRun`, soit ignorer si la référence est morte.

### Hors scope

- Ne pas toucher au level L_FP01_RunShell.umap (legacy différent, non dans le scope FP actuel).
- Ne pas modifier la classe `BP_SubmarineBakedRuntimeActor_1` elle-même (Phase 7B).

---

## 6. Setup de BP_Submarine_FPRun

### Action obligatoire

1. Content Browser > double-clic sur `Content/Sub3D/FirstPlayableRun/BP_Submarine_FPRun`.
2. Vérifier la Class Parent dans Class Settings > Details. Doit être `SubmarineBase` (ou une classe qui en dérive, hors compiler actor legacy). Marqué **non vérifié** par lecture terminale ; à confirmer dans l'éditeur.
3. Passer en mode Class Defaults (bouton en haut à droite du BP editor).
4. Dans Details panel, localiser la catégorie `Submarine > Generator` :
   - `Generator Spec` : assigner `DA_SubGenSpec_FPRun` (glisser-déposer depuis le Content Browser ou menu déroulant)
   - `Generated Definition` : **laisser vide**. La chaîne BeginPlay construit le Definition à partir du Spec si ce champ est null. Assigner un Definition ici forcerait un chemin « pre-baked » contraire à l'objectif de validation generator-path.
   - `Generator Door Actor Class` : à assigner en section 7
5. Vérifier la présence des boutons dans la catégorie `Submarine > Generator > Debug` :
   - `Rebuild From Spec`
   - `Clear Generated State`
6. Compile + Save.

### Validation attendue

- Class Parent = `SubmarineBase` (ou dérivée propre).
- `Generator Spec` pointe vers `DA_SubGenSpec_FPRun`.
- `Generated Definition` est vide.
- Les deux boutons de debug sont visibles et cliquables dans le Details panel.

### Blocage éventuel

- Si Class Parent est `SubmarineCompilerActor` (legacy) ou un autre : stop. Ce guide ne couvre pas la re-parentage. Alternative : créer un nouveau BP héritant directement de `SubmarineBase` avec le même nom.
- Si `Generator Spec` n'apparaît pas : le BP est parenté à la mauvaise classe, ou le module Sub3D n'a pas rechargé. Retour section 2.
- Si `Rebuild From Spec` n'apparaît pas dans Debug : vérifier que la catégorie n'est pas collapsed. Sinon, le build C++ n'inclut pas Step 0.

### Hors scope

- Ne pas régler les composants visuels (HullMesh, SubmarineRoot) ici.
- Ne pas régler la replication ou les variables réseau.

---

## 7. BP_Door : vérification ou création

### Étape 7A — Vérifier BP_SubDoor existant (Blocker 2)

#### Action obligatoire

1. Content Browser > double-clic sur `Content/Sub3D/Blueprint/SubBP/BP_SubDoor`.
2. Dans Class Settings > Details > Parent Class, lire la valeur affichée.

#### Validation attendue

- `Parent Class = SubDoorActor` → aller à section 7C pour l'assigner.
- `Parent Class ≠ SubDoorActor` → sauter à 7B.

### Étape 7B — Créer BP_Door_FP depuis SubDoorActor (fallback)

#### Action obligatoire

1. Content Browser > clic droit dans `Content/Sub3D/FirstPlayableRun/` > Blueprint Class.
2. Dans la fenêtre All Classes, chercher `SubDoorActor`, sélectionner.
3. Nommer le nouveau BP `BP_Door_FP`.
4. Double-clic pour ouvrir.
5. Dans le Viewport du BP :
   - Sélectionner le composant `DoorMesh` (hérité de C++).
   - Dans Details > Static Mesh, assigner un mesh simple. Options acceptables (non vérifiées individuellement) :
     - `Engine/BasicShapes/Cube` (redimensionné via le composant Transform : Scale X=0.2, Y=1.0, Z=2.0 en mètres)
     - Un autre mesh box déjà présent dans `Content/`
   - Dans Details > Collision, laisser le profil hérité (`BlockAll` est déjà dans le constructeur C++).
6. Dans Class Defaults :
   - `Starts Closed` (catégorie `Door`) : laisser à la valeur par défaut, la valeur effective viendra de `FGeneratedConnectionDef.bStartsClosed` assigné par `InitializeFromConnectionDef`.
7. Compile + Save.

#### Validation attendue

- Le BP s'ouvre sans erreur.
- Le mesh est visible dans le viewport du BP.
- Pas d'erreur de compile.

### Étape 7C — Assigner à GeneratorDoorActorClass

#### Action obligatoire

1. Retour à `BP_Submarine_FPRun` (Content Browser > double-clic).
2. Class Defaults > Details > catégorie `Submarine > Generator` > `Generator Door Actor Class`.
3. Menu déroulant : sélectionner `BP_SubDoor` (si 7A OK) ou `BP_Door_FP` (si 7B utilisé).
4. Compile + Save.

#### Validation attendue

- Le champ `Generator Door Actor Class` affiche le nom du BP sélectionné, pas « None ».
- En PIE (plus tard section 9), l'Output Log contient une ligne `[SpawnDoorsFromDefinition] BP_Submarine_FPRun_C_X: spawned N doors from M connections` avec N > 0.

#### Blocage éventuel

- Si le menu déroulant n'affiche ni `BP_SubDoor` ni `BP_Door_FP` : le BP sélectionné en 7B n'hérite pas de `SubDoorActor`. Recommencer 7B.
- Si le log PIE affiche `GeneratorDoorActorClass not set` alors qu'on vient de l'assigner : le BP n'a pas été sauvé. Retour étape 4 de 7C.

### Hors scope

- Ne pas régler Interactable component (hérité de C++, déjà câblé en BeginPlay).
- Ne pas régler Replication (défini côté C++).

---

## 8. Itération forme avec Rebuild From Spec

### Action obligatoire

1. Ouvrir le level `L_FP_GeneratorRun` (section 5 effectuée).
2. Placer une instance de `BP_Submarine_FPRun` dans le level si absente :
   - Content Browser > drag & drop `BP_Submarine_FPRun` dans le viewport
   - Positionner à (0, 0, 200) en cm ou selon la logique du level
3. Sélectionner l'instance dans le World Outliner.
4. Dans Details panel, localiser `Submarine > Generator > Debug` :
   - Cliquer `Clear Generated State` pour partir d'un état propre
   - Cliquer `Rebuild From Spec`
5. Observer l'Output Log (Window > Developer Tools > Output Log si caché).

### Validation attendue — log

Lignes à trouver dans l'Output Log après clic :
```
LogSubGenerator: Generate: success | 5 compartments | 5 connections | 5 flood edges | 3 stations | 2 spawns
LogSubMeshBuilder: [ShapeStep1] Exterior hull emitted: V=... T=...
LogSubMeshBuilder: [ShapeStep1] Interior (non-airlock) compartments=4 | Walls V=... T=... | Floor V=... T=... | BowCap V=... T=... | SternCap V=... T=...
LogSubMeshBuilder: [ShapeStep1] Airlock compartments=1 | Walls V=... T=... | Floor V=... T=...
LogSubMeshBuilder: [ShapeStep1] Bulkheads=3 | Panel V=... T=...
LogSubMeshBuilder: [ShapeStep1] BuildMeshData complete: exterior=... verts | 5 interior meshes | 3 bulkheads
LogTemp: [BP_Submarine_FPRun_C_X] [ShapeStep1] BuildFromDefinition toggles: ExteriorHull=1 Interior=1 Bulkheads=1 Airlock=1
LogTemp: [BP_Submarine_FPRun_C_X] BuildExteriorHull: 1 render + N convex collision slices
LogTemp: [BP_Submarine_FPRun_C_X] BuildInteriorCompartments: ... sections built from 5 compartments
LogTemp: [BP_Submarine_FPRun_C_X] BuildBulkheads: 3 bulkheads built from 3 definitions
LogTemp: [BP_Submarine_FPRun_C_X] BuildFromDefinition: N render + M collision components created
LogTemp: [RebuildFromSpec] BP_Submarine_FPRun_C_X: complete. Compartments=5 Connections=5 Stations=3 Doors=0
```

Note sur le dernier log : `Doors=0` est attendu en contexte éditeur (pas de World PIE). Les doors apparaissent seulement pendant PIE, pas en editor Rebuild.

### Validation attendue — viewport

- Le sous-marin apparaît dans le viewport avec sa silhouette.
- Les compartiments intérieurs sont visibles (walls, floor).
- Les bulkheads apparaissent comme panneaux avec ouvertures de porte.
- Si Option A de section 4 est appliquée : matériau gris uniforme. Sinon : grille WorldGrid.

### Itération paramètres envelope

6. Ouvrir `DA_Envelope_FPRun` dans un second onglet.
7. Modifier un paramètre, ex. `Bow Cap Length Cm` : 240 → 400.
8. Ctrl+S pour sauver l'asset.
9. Retour au level, re-sélectionner l'instance `BP_Submarine_FPRun`.
10. Cliquer à nouveau `Rebuild From Spec`.
11. Le cap avant doit s'être allongé dans le viewport.

Paramètres utiles à itérer pour tester la chaîne :
- `Body Length Fraction` (0.1 à 0.9) — déplace les bulkheads
- `Bow Profile` (Rounded/Needle/Blunt/Bulbous/Tapered) — change la silhouette du cap avant
- `Bow Sharpness` (0.1 à 4.0) — change l'agressivité du profil
- `Default Radius Cm` (50+) — change le rayon général
- `Width To Height Ratio` (0.5 à 2.0) — aplatit ou étire la section

### Blocage éventuel

- Si le sous-marin ne s'affiche pas après Rebuild : lire l'Output Log pour une erreur `Generate: ...failed`. Cause probable : `Generator Spec` non assigné en section 6.
- Si les logs `[ShapeStep1]` apparaissent avec V=0 ou T=0 sur une section : pipeline cassé côté code — sortir du scope éditeur.
- Si le viewport n'update pas après Rebuild : essayer de quitter la sélection puis la reprendre, ou fermer/réouvrir le level.

### Hors scope

- Ne pas ajuster les bulkheads du Spec ici (ouvrir `DA_SubGenSpec_FPRun` est autorisé mais en lecture seule pour cette session ; modifier demanderait une validation supplémentaire).
- Ne pas tenter d'animer ou de déplacer les PMCs manuellement.

---

## 9. Protocole PIE avec cheats

### Pré-requis

- Sections 5, 6, 7, 8 complétées.
- `BP_Submarine_FPRun` placé dans `L_FP_GeneratorRun` avec `Generator Spec` assigné et `Generator Door Actor Class` assigné.
- Un `PlayerStart` présent dans le level (non vérifié dans le level actuel ; sinon en ajouter un).

### Action obligatoire — lancement PIE

1. Ouvrir le level `L_FP_GeneratorRun`.
2. Bouton Play (ou Alt+P) pour démarrer PIE.
3. Laisser l'Output Log visible en parallèle (onglet séparé ou dock en bas).

### Validation attendue — PIE init

Lignes à trouver dans l'Output Log pendant les premières secondes de PIE :
```
LogSubGenerator: Generate: success | 5 compartments | 5 connections | ...
LogSubMeshBuilder: [ShapeStep1] BuildMeshData complete: exterior=... verts | 5 interior meshes | 3 bulkheads
LogSubFlood: Initialized: 5 compartments, 5 edges
LogTemp: [BP_Submarine_FPRun_C_X] BuildFromDefinition: N render + M collision components created
LogTemp: [SpawnDoorsFromDefinition] BP_Submarine_FPRun_C_X: spawned N doors from M connections
```

Le compte `spawned N doors` doit être > 0 (attendu 3 à 5 selon le FP spec).

### Action obligatoire — cheats console

Ouvrir la console avec la touche au-dessus de Tab (sur clavier QWERTY : `` ` ``; sur clavier AZERTY : `²` ou `&`).

Exécuter les cheats dans l'ordre suivant :

#### Cheat 1 — Lister compartments

```
DevCheat_ListCompartments
```

Résultat attendu dans l'Output Log :
```
[DevCheat_ListCompartments] BP_Submarine_FPRun_C_X: 5 compartments
  Helm (type=1) capacity=... flood=0.00
  Crew (type=2) capacity=... flood=0.00
  Crew2 (type=2) capacity=... flood=0.00
  Engine (type=3) capacity=... flood=0.00
  Airlock (type=4) capacity=... flood=0.00
```

Note : les IDs exacts dépendent du nombre de bulkheads dans le Spec FP. Avec 3 bulkheads (cas du rapport PIE précédent), on attend `Helm`, `Crew`, `Crew2`, `Engine`, `Airlock`. Avec un nombre différent, adapter les IDs utilisés dans les cheats suivants.

#### Cheat 2 — Créer une brèche

```
DevCheat_CreateBreach Helm 500
```

Résultat attendu :
```
[DevCheat_CreateBreach] BP_Submarine_FPRun_C_X: created breach on 'Helm' at 500.0 L/s
```

Puis dans les ticks suivants : le niveau d'eau du compartiment `Helm` monte progressivement.

#### Cheat 3 — Vérifier le niveau

```
DevCheat_ListCompartments
```

La valeur `flood=` pour `Helm` doit être non nulle et croissante à chaque appel.

#### Cheat 4 — Forcer un niveau direct

```
DevCheat_SetFloodLevel Engine 0.5
```

Résultat attendu :
```
[DevCheat_SetFloodLevel] BP_Submarine_FPRun_C_X: set 'Engine' to 0.50
```

Le compartiment `Engine` passe à 50% noyé instantanément (bypass de la simulation).

#### Cheat 5 — Fermer une porte

Les IDs de connections générées sont (non vérifiés dans le Spec FP actuel ; attendus selon la logique `SubmarineGenerator.cpp`) :
- `Bulkhead_0`, `Bulkhead_1`, `Bulkhead_2` (bulkheads interiors, avec 3 bulkheads)
- `Airlock_Inner` (porte interne du sas)
- `Airlock_Outer` (hatch externe du sas)

Exécuter :
```
DevCheat_SetDoorClosed Bulkhead_0 true
```

Résultat attendu :
```
[DevCheat_SetDoorClosed] BP_Submarine_FPRun_C_X: set door 'Bulkhead_0' closed=1
```

#### Cheat 6 — Réparer toutes les brèches

```
DevCheat_RepairAllBreaches
```

Résultat attendu :
```
[DevCheat_RepairAllBreaches] BP_Submarine_FPRun_C_X: cleared breaches across 5 compartments
```

Les brèches précédemment créées doivent cesser d'alimenter les compartiments. Le niveau de flood baisse plus (si pompes actives) ou se stabilise.

#### Cheat 7 — Téléporter le crew

```
DevCheat_TeleportToCompartment Engine
```

Résultat attendu :
```
[DevCheat_TeleportToCompartment] ... -> 'Engine' at world (X, Y, Z)
```

Le pawn possédé (crew character) est instantanément déplacé au centre du compartiment `Engine` à hauteur walkable + 100 cm.

### Validation attendue — protocole complet

- Les 7 cheats produisent la sortie log attendue.
- Aucun cheat ne provoque de crash PIE.
- Les niveaux de flood réagissent aux inputs (visible via `DevCheat_ListCompartments` répété).

### Blocage éventuel

- Si `DevCheat_*` renvoie `No submarine or SubFlood resolved` : le crew character ne référence pas la sub. Cause probable : le pawn possédé n'est pas celui attendu, ou `ResolveCurrentSubmarine()` retourne null. Vérifier que le PlayerStart du level est bien câblé pour spawner un crew character.
- Si `SetFloodLevel` ne change pas l'affichage visuel du water plane : le système de visualisation des water planes dépend de `FloodWaterVisualsComponent` qui lit depuis SubFlood. Si les water planes n'apparaissent pas, ça sort du scope de ce guide (bug FloodWaterVisuals à investiguer hors-guide).
- Si `SetDoorClosed` renvoie l'ID `Bulkhead_0` mais que la porte ne réagit pas visuellement : vérifier que le `ASubDoorActor` correspondant a bien été spawné (log `[SpawnDoorsFromDefinition]` dit N>0). Si N=0, retour section 7C.

### Hors scope

- Interaction joueur avec les portes via clic/touche (dépend du `InteractableComponent` et du input mapping, non couvert par ce guide).
- Mécanique de réparation joueur (utilisation de `ClearBreachNearLocation` via un prompt interact) — non câblée en BP, scope post-FP.

---

## 10. Validation no [LEGACY] warnings

### Action obligatoire

1. Toujours en PIE, laisser tourner au moins 5 secondes.
2. Stopper PIE (bouton Stop ou Esc).
3. Dans l'Output Log, filtrer via la barre de recherche : `[LEGACY]`.
4. Lister les occurrences trouvées.

### Validation attendue

Aucune ligne `[LEGACY]` émise par un actor dont le nom contient `BP_Submarine_FPRun`.

Les lignes `[LEGACY]` interdites pour le FP actor principal (selon le contrat de session §6) :
- `ASubmarineBase::BeginPlay: initializing SubFlood from LayoutAsset`
- `USubFloodComponent::InitializeFromLayout`
- `UFloodWaterVisualsComponent: building water planes from LayoutAsset`
- `ASubCrewCharacter::ResolveCurrentCompartment` fallback

Si une de ces lignes apparaît avec `BP_Submarine_FPRun_C_X` dans le nom : la session est **un échec**. Causes possibles :
- `Generator Spec` non assigné en section 6 → le pipeline generator ne démarre pas → fallback LayoutAsset.
- `Generated Definition` assigné mais corrompu → Generate() retourne null → fallback LayoutAsset.
- Un composant FloodWaterVisuals ou CrewCharacter lit depuis LayoutAsset au lieu de SubFlood → bug runtime hors scope éditeur.

### Tolérances

- Les lignes `[LEGACY]` émises par un autre actor (ex. legacy actor non encore nettoyé en section 5) sont un échec de section 5, à corriger en retournant là-bas.
- Les lignes `[LEGACY]` à startup éditeur (avant PIE) ne comptent pas dans la validation PIE mais indiquent une dette globale.

### Blocage éventuel

- Si des `[LEGACY]` persistent malgré les sections 5 et 6 : lister les exactes, identifier l'actor émetteur (préfixe dans le log), et soit supprimer cet actor, soit escalader en code.

---

## 11. Checklist finale

Cocher chaque item après validation manuelle dans l'éditeur.

### Setup

- [ ] Éditeur UE fermé puis relancé proprement sur `Sub3D.uproject`
- [ ] `DA_Envelope_FPRun` ouvert, 6 nouveaux champs présents, `Radius Profile` absent
- [ ] Dette matériaux : soit Option A (matériaux two-sided + BP wiring) appliquée, soit Option B (backlog post-FP documenté)
- [ ] `L_FP_GeneratorRun` : aucune instance `BP_SubmarineBakedRuntimeActor_1` ou autre legacy
- [ ] `BP_Submarine_FPRun` : Class Parent = `SubmarineBase`, `Generator Spec` assigné, `Generated Definition` vide, `Generator Door Actor Class` assigné
- [ ] `BP_SubDoor` (ou `BP_Door_FP`) : hérite de `SubDoorActor`, possède un mesh assigné au composant `DoorMesh`
- [ ] Une instance `BP_Submarine_FPRun` placée dans `L_FP_GeneratorRun`

### Éditeur time validation

- [ ] Clic `Rebuild From Spec` produit les logs `[ShapeStep1]` exterior / interior / bulkheads / airlock / complete
- [ ] Le sous-marin apparaît dans le viewport après Rebuild
- [ ] Modification de `Bow Cap Length Cm` puis Rebuild change visiblement la forme du cap avant
- [ ] Modification de `Body Length Fraction` puis Rebuild change visiblement la répartition body/caps

### PIE time validation

- [ ] Lancement PIE : `LogSubGenerator: Generate: success` avec N compartments > 0
- [ ] Log `[SpawnDoorsFromDefinition]` : `spawned N doors from M connections` avec N > 0
- [ ] Log `LogSubFlood: Initialized: N compartments, M edges`
- [ ] Cheat `DevCheat_ListCompartments` liste tous les compartments avec leur type
- [ ] Cheat `DevCheat_CreateBreach Helm 500` crée une brèche observable
- [ ] Cheat `DevCheat_SetFloodLevel Engine 0.5` force un niveau
- [ ] Cheat `DevCheat_SetDoorClosed Bulkhead_0 true` passe sans erreur
- [ ] Cheat `DevCheat_RepairAllBreaches` retire les brèches
- [ ] Cheat `DevCheat_TeleportToCompartment Engine` déplace le crew
- [ ] Aucun crash pendant la session PIE

### Legacy-free validation

- [ ] Filtre Output Log `[LEGACY]` : aucune ligne émise par `BP_Submarine_FPRun_C_X`

### Pass criteria

Session FP éditeur **PASS** si :
- tous les items ci-dessus sont cochés
- ET aucun blocage non résolu identifié

Session FP éditeur **FAIL** si :
- un warning `[LEGACY]` est émis par le FP actor
- un cheat provoque un crash
- le pipeline `Rebuild From Spec` ne produit pas les logs attendus
- `SpawnDoorsFromDefinition` renvoie `0 doors` alors que le Spec a des bulkheads

---

## 12. Non vérifié

Points que ce guide énonce sans avoir pu les confirmer depuis le terminal :

1. **Parenté de `BP_SubDoor`** : l'asset existe à `Content/Sub3D/Blueprint/SubBP/BP_SubDoor.uasset` mais sa classe parente ne peut être lue qu'en ouvrant le BP dans l'éditeur. Voir section 7A.

2. **Présence d'une instance legacy `BP_SubmarineBakedRuntimeActor_1_C_1`** dans `L_FP_GeneratorRun.umap`. Le `.umap` binaire n'est pas lisible depuis le terminal. Seul le log PIE précédent partagé mentionne cet actor par son nom d'instance. Voir section 5.

3. **Présence d'un `PlayerStart` dans `L_FP_GeneratorRun`**. Le log PIE précédent contenait un warning `FindPlayerStart: PATHS NOT DEFINED or NO PLAYERSTART`, ce qui suggère soit absence, soit défaut de configuration. À vérifier en section 9 si le crew ne spawn pas.

4. **Valeurs exactes des IDs de connexion** `Bulkhead_0`, `Bulkhead_1`, `Bulkhead_2`, `Airlock_Inner`, `Airlock_Outer`. Noms dérivés de `SubmarineGenerator.cpp` lignes 378, 478, 491, et dépendent du nombre de bulkheads dans `DA_SubGenSpec_FPRun`. Avec 3 bulkheads les 5 IDs ci-dessus sont corrects. Avec un nombre différent, adapter (ex. 2 bulkheads → `Bulkhead_0`, `Bulkhead_1`).

5. **Valeurs exactes des IDs de compartments** `Helm`, `Crew`, `Crew2`, `Engine`, `Airlock`. Avec 4 compartments hull le mapping est `Helm` (index 0), `Crew` (index 1), `Crew2` (index 2), `Engine` (index 3). Avec 3 compartments : `Helm`, `Crew`, `Engine`. Avec 5+ : `Helm`, `Crew`, `Crew2`, `Crew3`, ..., `Engine`. Dérivé de `SubmarineGenerator.cpp` lignes 312-324.

6. **Valeurs numériques attendues dans les logs `[ShapeStep1]`** (V=..., T=...). Ces counts dépendent du Spec et des paramètres de l'envelope. Le log fournit la valeur courante ; il n'y a pas de valeur de référence à matcher.

7. **Action exacte du slider `Set Material`** dans le BP Event Graph (section 4 Option A). Le node s'appelle `Set Material` et prend `Element Index` + `Material`. Confirmation visuelle requise dans l'éditeur.

8. **Existence d'un mesh `Engine/BasicShapes/Cube`** à assigner au `DoorMesh` en section 7B. Mesh UE standard, présent dans toutes les installations UE mais à vérifier dans le Content Browser > View Options > Show Engine Content si non visible.

9. **Comportement exact des PMCs vis-à-vis de `SetMaterial`** après `CreateMeshSection_LinearColor`. L'API UE5 supporte `SetMaterial(Index, Material)` sur `UProceduralMeshComponent` mais le comportement en contexte runtime après BeginPlay n'a pas été testé dans ce projet.

---

## 13. Hors scope de ce guide

Les items suivants sont explicitement hors de ce guide et ne doivent pas être traités pendant la session éditeur :

- Création d'un `BP_Airlock` avec mesh, doors enfants, et `SpawnAirlockFromDefinition` runtime. Le compartiment airlock existe au niveau `FGeneratedCompartmentDef` et apparaît dans les logs, mais sans BP actor ni mesh visible. Scope post-FP.
- Ajout d'un interaction system joueur pour ouvrir/fermer les portes (input mapping + UMG prompt). Les doors sont spawnées avec un `InteractableComponent` mais le crew character input n'est pas couvert ici.
- Système de repair joueur (prompt UI + trigger sur breach location). Fonctions `ClearBreachNearLocation` existent côté C++ mais aucun câblage gameplay n'est présent.
- NavMesh ou AI crew. Le level ne contient pas de NavMesh et le crew FP est uniquement le joueur.
- Polish éclairage intérieur, audio ambiance, VFX breach.
- Suppression définitive du legacy LayoutAsset path dans `SubHullComponent` et `FloodWaterVisualsComponent` (Phase 7B, post-validation FP).
- Modification de `DA_SubGenSpec_FPRun` (nombre de bulkheads, positions, passages). Le Spec actuel est la ressource de validation.
- Tests automation (déjà validés côté C++ dans la session autonome, non réexécutables dans ce guide).

---

## 14. Retour de session attendu

Au terme de l'exécution de ce guide, remplir le format défini dans le contrat de session §6 :

```text
FP Editor Session Result

Level: L_FP_GeneratorRun
Submarine actor/BP: BP_Submarine_FPRun
GeneratorSpec: DA_SubGenSpec_FPRun
GeneratedDefinition path: runtime-generated

PIE result: <pass/fail>
Legacy warnings seen: <yes/no>

Validated:
- <liste des items passés de la checklist section 11>

Blockers:
- <liste des blockers résiduels>

Ready for next step:
- <yes/no>
```

Si `Ready for next step = yes`, déclencher la phase suivante :
- Retrait des fallbacks LayoutAsset dans le path principal (code)
- Préparation de Phase 7B (compile-time deprecation)

Si `Ready for next step = no`, ne pas avancer. Retourner les blockers pour résolution.
