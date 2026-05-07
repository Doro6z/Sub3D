# Craniata — Audit alignement `BP_Submarine_Craniata` ↔ `DA_SubDef_Craniata`

**Branche** : `water-proto` (HEAD `7b8ccca` — Phase 1 du plan eau).
**Date** : 2026-05-05.
**Périmètre** : audit READ-ONLY. Aucun asset/code modifié.

**Symptôme observé en PIE** (`Proto03_Sub_HullPrecision`, log `Saved/Logs/Sub3D.log` à `09:10:09`) :
- `EnsureCompartmentVolumesFromDefinition: auto-spawned 9 UCompartmentVolumeComponent(s) on BP_Submarine_Craniata_C_1` — l'auto-spawn DA voit zéro volume BP matchant un id DA et ressort 9 volumes additionnels.
- 6× `UDoorWaterBridge (BP_SubDoor_C_0..5): unresolved compartment IDs (A=None, B=None)` — six portes proto inactives faute de `CompartmentA/B` configurés.
- Au moins quatre debug labels `MainDeckID`, `MainDeck1ID`, `LowerDeckID`, `AL01` visibles à côté des labels DA (ces volumes BP coexistent avec les 9 auto-spawn).

---

## 1. Inventaire `BP_Submarine_Craniata`

Source : extraction du nom-table FName du `.uasset` (`Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata.uasset`, 275 290 octets) + `mcp__unrealclaude__unreal_blueprint_query`. La géométrie (Box extents / RelativeLocation) n'est pas extractible texte depuis le `.uasset` ; elle est cependant déductible des labels et corroborée par le précédent audit `reports/handoffs/2026-04-24_flood_visuals_containment_audit.md` (§1.1).

### 1.1 `UCompartmentVolumeComponent` placés (5)

| Variable BP        | `CompartmentId` (FName)   | Couverture inférée par nom |
|--------------------|---------------------------|----------------------------|
| `CV MainDeck`      | `MainDeckID`              | pont principal — partie avant + centrale (probable C_main_Bow ∪ C_main_Fwd) |
| `CV MainDeck1`     | `MainDeck1ID`             | pont principal — partie arrière (probable C_main_Aft) |
| `CV LowerDeck`     | `LowerDeckID`             | pont inférieur — un seul volume coiffant les 3 compartiments DA (C_lower_*) |
| `CV UpperDeck`     | `UpperDeck01`             | pont supérieur — un seul volume coiffant UpperFwd + UpperAft |
| `CV Airlock`       | `AL01`                    | sas — le seul cas 1↔1 spatial (C_upper_Airlock) |

Présence dans le name-table : `MainDeckID`, `MainDeck1ID`, `LowerDeckID`, `UpperDeck01`, `AL01` (et `Airlock01` qui apparaît une fois — vraisemblablement un `DisplayName` ou un label hérité, pas un `CompartmentId`).

### 1.2 Autres composants pertinents (extrait — pas exhaustif)

- 6 `ChildActorComponent → BP_SubDoor` : `BP_SubDoor_LowerD_BallastAft`, `BP_SubDoor_LowerD_BallastFwd`, `BP_SubDoor_MainD_Control`, `BP_SubDoor_MainD_Fwd`, `BP_SubDoor_UpperD_Airlock_Inner`, `BP_SubDoor_UpperD_Armory`.
- 1 `ChildActorComponent → BP_SubDoor_Exterior` : `BP_SubAirLockExit`.
- Stations : `BP_HelmStation`, `BP_BallastStation`, `BP_EngineStation`.
- 4 `CrewSpawnSocketP1..P4`.
- 1 `USubHullBoundaryComponent` nommé `SubHullBoundaryAirlock` (passage EVA — couplé au sas).
- Composants gameplay : `SubFlood` (USubFloodComponent), `SubMovement` (USubMovementComponent), `SubmarineCompartmentComponent`, `MovementCollisionProxy`, `HullMesh`, `FloodWaterVisuals` (legacy — voir `7b8ccca` qui le drop), `HelmNavigationDisplay`.

### 1.3 Note sur `BP_SubDoor` (parent class `ASubDoorActor`)

`BP_SubDoor` contient (vérifié dans le name-table) un `UDoorWaterBridge` en composant statique. C'est ce composant qui émet le warning `unresolved compartment IDs`. Les 6 instances de `BP_SubDoor` dans `BP_Submarine_Craniata` ont leur `ASubDoorActor::CompartmentA` et `CompartmentB` (FName, `EditAnywhere`, `SubDoorActor.h:42-46`) à `NAME_None`, d'où les 6 warnings (un par instance).

---

## 2. Inventaire `DA_SubDef_Craniata`

Source : `Content/DA_SubDef_Craniata.T3D` (dump T3D présent à la racine du repo). 9 compartiments + 9 connexions.

### 2.1 Compartiments

| Idx | `CompartmentId`       | DisplayName  | HydroBoundsMin (X, Y, Z)        | HydroBoundsMax (X, Y, Z)        | MaxWaterHeightCm |
|-----|-----------------------|--------------|---------------------------------|---------------------------------|------------------|
| 0   | `C_main_Bow`          | Bow          | (-1764, -200, -20)              | (-1159.2, 200, 180)             | 200              |
| 1   | `C_main_Fwd`          | Fwd          | (-1159.2, -200, -20)            | (537.6, 200, 180)               | 200              |
| 2   | `C_main_Aft`          | Aft          | (537.6, -200, -20)              | (1764, 200, 180)                | 200              |
| 3   | `C_upper_UpperFwd`    | UpperFwd     | (-1596, -200, 130)              | (594.8, 200, 330)               | 200              |
| 4   | `C_upper_Airlock`     | Airlock      | (594.8, -200, 130)              | (814.8, 200, 330)               | 200              |
| 5   | `C_upper_UpperAft`    | UpperAft     | (814.8, -200, 130)              | (1176, 200, 330)                | 200              |
| 6   | `C_lower_BallastFwd`  | BallastFwd   | (-1470, -200, -240)             | (-693, 200, -40)                | 200              |
| 7   | `C_lower_Hub`         | Hub          | (-693, -200, -240)              | (63, 200, -40)                  | 200              |
| 8   | `C_lower_BallastAft`  | BallastAft   | (63, -200, -240)                | (1260, 200, -40)                | 200              |

Dimensions hull : `HullLengthCm=4200`, `HullBeamCm=400`, `HullHeightCm=600`. L'origine BP est centrée (les compartiments s'étendent de ~-1764 à +1764 en X).

### 2.2 Connexions (rappel — utiles §5)

| ConnectionId                        | CompartmentA           | CompartmentB           | Type           |
|-------------------------------------|------------------------|------------------------|----------------|
| `N_lower_BallastFwd`                | `C_lower_BallastFwd`   | `C_lower_Hub`          | WatertightDoor |
| `N_lower_BallastAft`                | `C_lower_Hub`          | `C_lower_BallastAft`   | WatertightDoor |
| `N_main_Fwd`                        | `C_main_Bow`           | `C_main_Fwd`           | WatertightDoor |
| `N_main_Control`                    | `C_main_Fwd`           | `C_main_Aft`           | WatertightDoor |
| `N_vert_LowerAccess`                | `C_main_Fwd`           | `C_lower_Hub`          | Hatch          |
| `N_upper_UpperAirlock_Inner`        | `C_upper_UpperFwd`     | `C_upper_Airlock`      | Airlock        |
| `N_upper_UpperAirlock_Exit`         | `C_upper_Airlock`      | `C_upper_UpperAft`     | Airlock        |
| `N_upper_UpperAirlock_Exit__to_EXT` | `C_upper_UpperAft`     | (EXT)                  | ExteriorHatch  |
| `N_vert_UpperAccess`                | `C_upper_UpperFwd`     | `C_main_Fwd`           | Open (ladder)  |

---

## 3. Tableau de mapping DA ↔ BP

Heuristique : nom de la variable BP + label deck du DA. Aucun overlap géométrique numérique calculé (extents BP non extractibles texte) — recommandation : valider en PIE avec debug labels avant suppression.

| DA `CompartmentId`    | Volume BP couvrant (inféré)        | BP `CompartmentId` | Statut                          |
|-----------------------|------------------------------------|--------------------|---------------------------------|
| `C_main_Bow`          | `CV MainDeck`                      | `MainDeckID`       | mismatch — partagé              |
| `C_main_Fwd`          | `CV MainDeck`                      | `MainDeckID`       | mismatch — partagé              |
| `C_main_Aft`          | `CV MainDeck1`                     | `MainDeck1ID`      | mismatch — 1↔1 spatial          |
| `C_upper_UpperFwd`    | `CV UpperDeck`                     | `UpperDeck01`      | mismatch — partagé              |
| `C_upper_Airlock`     | `CV Airlock`                       | `AL01`             | mismatch — 1↔1 spatial          |
| `C_upper_UpperAft`    | `CV UpperDeck`                     | `UpperDeck01`      | mismatch — partagé              |
| `C_lower_BallastFwd`  | `CV LowerDeck`                     | `LowerDeckID`      | mismatch — partagé              |
| `C_lower_Hub`         | `CV LowerDeck`                     | `LowerDeckID`      | mismatch — partagé              |
| `C_lower_BallastAft`  | `CV LowerDeck`                     | `LowerDeckID`      | mismatch — partagé              |
| (orphelin DA — aucun) | — (aucun volume BP orphelin)       | —                  | —                               |
| (orphelin BP — aucun) | — (chaque CV BP est utilisé)       | —                  | —                               |

**Constat structurel** :
- Le BP a **5 volumes** (granularité par deck, sauf le pont principal coupé en deux).
- Le DA a **9 compartiments** (granularité fine par fonction : Bow / Fwd / Aft / BallastFwd / Hub / BallastAft / etc.).
- Aucun id BP ne match aucun id DA → `EnsureCompartmentVolumesFromDefinition` voit 0 couverture et auto-spawn les 9.
- Résultat runtime : **14 volumes coexistent** (5 BP + 9 auto-spawn). Les 5 BP ne matchent rien dans le DA donc la sim flood ne les utilise pas — mais ils restent là, attachés à `SubmarineRoot`, et leurs labels apparaissent dans le debug.

---

## 4. Origine du désalignement

Timeline `git log --follow` :

| Commit       | Date         | Asset                                | Action |
|--------------|--------------|--------------------------------------|--------|
| `6ce40dd`    | 2026-04-16   | `Craniata_Definition.json`           | ajout (Spec Extraction Bridge — 9 compartiments avec ids `C_<deck>_<position>`) |
| `917489b`    | 2026-04-18   | `BP_Submarine_Craniata.uasset`       | ajout |
| `917489b`    | 2026-04-18   | `DA_SubDef_Craniata.uasset`          | ajout |
| `13c8617`    | 2026-04-20   | les deux                             | mod |
| `dbb2ba8`    | (récent)     | les deux                             | mod (« wip(crew+motion): pre-water-proto save state ») |

Hypothèse confirmée par les memory entries du projet :

- `project_craniata_workflow_2026_04_17.md` : « Harvest script removed, Craniata BP rebuilt manually ». Le BP a donc été monté **à la main dans l'éditeur** (placement de 5 boxes par deck, ids autorés à la main : `MainDeckID`, etc.).
- `project_compose_failed_2026_04_16.md` : la composition headless JSON-transforms a échoué le 2026-04-16 ; fallback manuel par l'utilisateur.
- `project_def_python_writable_2026_04_16.md` : le `SubmarineDefinition` a été flippé `EditAnywhere` pour autoriser un import Python populant les 9 compartiments du DA depuis `Craniata_Definition.json`.

Conclusion : les **deux assets ont été créés indépendamment** :
- DA peuplé via script Python le 2026-04-16/17 à partir du `Craniata_Definition.json` (convention `C_<deck>_<position>`, 9 compartiments — granularité « subsystem »).
- BP monté à la main dans l'éditeur quelques jours après (convention `<Deck>ID` / `<Deck>01`, 5 volumes — granularité « deck »).
Aucune passe de réconciliation n'a été faite à l'époque parce que la sim flood marchait déjà avec les 5 volumes BP (priorité d'init du flood = volumes BP avant le commit `7b8ccca` du 2026-05-04, qui inverse cette priorité au profit du DA). Le désalignement n'était silencieux que tant que le DA ne pilotait pas le pipeline.

---

## 5. Audit warnings `UDoorWaterBridge`

**Verdict** : les 6 portes `BP_SubDoor_C_0..5` sont **les 6 child-actor instances de `BP_SubDoor` à l'intérieur de `BP_Submarine_Craniata`** (vérifié dans le name-table : `BP_SubDoor_LowerD_BallastAft`, `BP_SubDoor_LowerD_BallastFwd`, `BP_SubDoor_MainD_Control`, `BP_SubDoor_MainD_Fwd`, `BP_SubDoor_UpperD_Airlock_Inner`, `BP_SubDoor_UpperD_Armory`). Pas de leftover proto — ce sont bien les portes de prod.

Le warning vient de `UDoorWaterBridge::ResolveRenderers` (`Source/Sub3DWaterProto/Private/DoorWaterBridge.cpp:80`) :
```cpp
if (CachedDoor.IsValid())
{
    if (TargetA.IsNone()) { TargetA = CachedDoor->CompartmentA; }
    if (TargetB.IsNone()) { TargetB = CachedDoor->CompartmentB; }
}
if (TargetA.IsNone() || TargetB.IsNone())
{
    UE_LOG(... "unresolved compartment IDs");
    return;
}
```

Donc `CachedDoor->CompartmentA` et `CachedDoor->CompartmentB` (FName, `EditAnywhere`, `SubDoorActor.h:42-46`) sont à `NAME_None` sur les 6 instances `BP_SubDoor` placées dans le BP. C'est strictement une omission d'instance-data au moment du placement — `BP_SubDoor` lui-même est correct.

Mappage attendu (à partir des `Connections` du DA et des labels des child-actors BP) :

| Child actor BP                            | `CompartmentA` attendu (DA)  | `CompartmentB` attendu (DA)  | `ConnectionId` DA                |
|-------------------------------------------|------------------------------|------------------------------|-----------------------------------|
| `BP_SubDoor_LowerD_BallastFwd`            | `C_lower_BallastFwd`         | `C_lower_Hub`                | `N_lower_BallastFwd`             |
| `BP_SubDoor_LowerD_BallastAft`            | `C_lower_Hub`                | `C_lower_BallastAft`         | `N_lower_BallastAft`             |
| `BP_SubDoor_MainD_Fwd`                    | `C_main_Bow`                 | `C_main_Fwd`                 | `N_main_Fwd`                     |
| `BP_SubDoor_MainD_Control`                | `C_main_Fwd`                 | `C_main_Aft`                 | `N_main_Control`                 |
| `BP_SubDoor_UpperD_Airlock_Inner`         | `C_upper_UpperFwd`           | `C_upper_Airlock`            | `N_upper_UpperAirlock_Inner`     |
| `BP_SubDoor_UpperD_Armory`                | (pas dans DA)                | (pas dans DA)                | **leftover** — voir ci-dessous   |

**Note 1 : `Armory` n'a pas de connexion correspondante dans le DA**. Le DA a `N_upper_UpperAirlock_Exit` (Airlock ↔ UpperAft), pas un `Upper_Armory`. Le BP semble donc avoir une porte « Armory » qui se trouve au sein de `C_upper_UpperFwd` (split visuel non modélisé dans le DA), ou bien c'est un leftover d'un schéma d'iter différent (le `SM_BH_Upper_Armory` apparaît bien dans les meshes — bulkhead Armory existe). À clarifier avec l'utilisateur : soit ajouter une connexion `N_upper_Armory` (compartment split), soit retirer la porte Armory du BP.

**Note 2 : la porte du sas extérieur (`BP_SubAirLockExit` → parent `BP_SubDoor_Exterior`)** n'émet pas de warning dans le log fourni. Soit elle n'a pas de `UDoorWaterBridge` (sortie vers EXT — bridge inutile), soit elle a déjà ses ids configurés. À vérifier.

**Note 3 — limitation indépendante du data-fix** : même avec `CompartmentA/B` configurés, le bridge proto `UDoorWaterBridge::ResolveRenderers` va chercher des `ARoomActor` dans le level (`TActorIterator<ARoomActor>`, `DoorWaterBridge.cpp:92`). Aucun `ARoomActor` n'existe dans `Proto03_Sub_HullPrecision` (vérifié via `unreal_get_level_actors class_filter=RoomActor` → 0 acteur). Donc, jusqu'au portage Phase 3 prévu (commentaire en place : `// En portage Sub3D : remplacer par lookup sur ASubmarineBase manager`), le bridge reste inactif **sur Craniata** même si on configure `CompartmentA/B`. Conséquence : configurer les `CompartmentA/B` retire les warnings, **mais ne suffit pas à activer la sync au bord** — c'est cohérent avec la roadmap Phase 3 (`reports/plans/2026-05-04_water_implementation_plan.md`).

---

## 6. Recommandation

**Option B (recommandée) — supprimer les 5 volumes BP, laisser l'auto-spawn DA seul.**

| Critère                          | A (renommer 5 BP → 9 DA)                                               | **B (supprimer 5 BP)**                                              | C (split BP en 9)                       |
|----------------------------------|------------------------------------------------------------------------|---------------------------------------------------------------------|-----------------------------------------|
| Granularité résultante           | 5 volumes BP, 4 ids DA orphelins → toujours 4 auto-spawn               | **9 volumes auto-spawn, 0 BP**                                       | 9 volumes BP, 0 auto-spawn              |
| Travail asset                    | renommer 5 ids — facile mais résoud 0 mismatch spatial                 | **delete 5 components — 5 minutes dans BP editor**                  | placer 4 nouveaux boxes — long          |
| Géométrie effective              | mauvaise (1 volume BP couvre 2-3 compartiments DA)                     | **directe depuis HydroBoundsMin/Max — mêmes valeurs que la sim**     | hand-tuned                              |
| Risque                           | élevé : cohabitation 5 BP + 4 auto-spawn, 4 compartiments toujours unmatched | **faible : volumes auto-spawn alignés sur les bounds DA**            | moyen : 9 placements à valider          |
| Compatible Phase 2 bake offline  | non — granularité non alignée                                          | **oui — la baker prendra les 9 volumes auto-spawn**                  | oui                                     |

**Argumentation B** :
1. Les bounds du DA sont **déjà numériques et faisaient autorité** pour la sim flood depuis le 2026-05-04 (commit `7b8ccca`). Les 5 volumes BP ne servent plus à rien depuis la priorité d'init inversée.
2. `EnsureCompartmentVolumesFromDefinition` est idempotent — il ne fait rien si les volumes BP sont déjà alignés. En supprimant les 5 BP, on laisse le helper produire les 9 corrects.
3. Phase 2 (bake offline) attend des bounds **par compartiment** (1↔1 avec `C_<deck>_<position>`). Option B livre directement cette structure. Options A et C demandent du travail pour aboutir à la même chose.
4. Les seules données per-volume éditées dans le BP (regard sur le code : `LinkedAudioVolume`, `LinkedPostProcessVolume`, `O2Level01`) sont des **stubs post-FP** d'après `CLAUDE.md` — rien à perdre.

**Étapes pratiques (read-write, à exécuter par l'utilisateur)** :
1. Ouvrir `BP_Submarine_Craniata`.
2. Dans le SCS, supprimer les 5 components : `CV Airlock`, `CV LowerDeck`, `CV MainDeck`, `CV MainDeck1`, `CV UpperDeck`.
3. Compiler + sauvegarder le BP.
4. PIE sur `Proto03_Sub_HullPrecision`. Confirmer dans le log :
   - `EnsureCompartmentVolumesFromDefinition: auto-spawned 9 UCompartmentVolumeComponent(s)` (pareil qu'avant).
   - Plus aucun debug label `MainDeckID` / `MainDeck1ID` / `LowerDeckID` / `UpperDeck01` / `AL01` visible.
   - Les 9 labels DA (`C_main_Bow`, etc.) restent.
5. (Indépendant — fix des warnings doors) : sur les 6 child-actors `BP_SubDoor` du BP, configurer `CompartmentA/CompartmentB` selon le tableau §5. Décision séparée à prendre pour `BP_SubDoor_UpperD_Armory` (leftover, ou ajouter `N_upper_Armory` au DA).

**Reste hors scope de ce fix** :
- Tant que le `UDoorWaterBridge` continue d'itérer `ARoomActor`, les bridges restent visuellement inactifs sur Craniata même après §5. Le portage est planifié Phase 3 du plan eau.
- Suppression des `CV ...` ne touche pas au `USubHullBoundaryComponent` `SubHullBoundaryAirlock` ni aux portes — ce sont des composants distincts.

---

## Annexe — sources primaires

- DA : `Content/DA_SubDef_Craniata.T3D` (1-39) — dump T3D autoritaire des 9 compartiments + 9 connexions.
- BP : `Content/Sub3D/FirstPlayableRun/BP_Submarine_Craniata.uasset` (binaire, 275 290 octets) — name-table extrait via `[regex]'[\x20-\x7E]{4,}'` sur l'ASCII brut du fichier.
- Helper auto-spawn : `Source/Sub3D/Submarine/SubmarineBase.cpp:1506-1578`.
- Bridge proto : `Source/Sub3DWaterProto/Private/DoorWaterBridge.cpp:59-158` (`ResolveRenderers`).
- Door fields : `Source/Sub3D/Submarine/SubDoorActor.h:42-46`.
- Log PIE : `Saved/Logs/Sub3D.log` lignes contenant `EnsureCompartmentVolumesFromDefinition` (1 hit) et `unresolved compartment` (6 hits) à `[2026.05.05-09.10.09]`.
- Audit antérieur (corroboratif) : `reports/handoffs/2026-04-24_flood_visuals_containment_audit.md` §1.1 (« 5 compartiments : LowerDeckID, MainDeckID, MainDeck1ID, UpperDeck01, AL01 »).
Transfor

ms doors Je vais écrire un petit script Python d'extraction et le lancer en headless pour lire le SCS de Craniata.Extraction réussie. 6 composants `BP_SubDoor` trouvés sur Craniata.

| Component | Loc (X, Y, Z) cm | Rot (P, Y, R) ° | Scale |
|---|---|---|---|
| `BP_SubDoor_MainD_Fwd` | (1159.2, 0, 90) | (0, 180, 0) | 1,1,1 |
| `BP_SubDoor_MainD_Control` | (-537.6, 0, 90) | (0, 0, 0) | 1,1,1 |
| `BP_SubDoor_LowerD_BallastFwd` | (693, 0, -130) | (0, 180, 0) | 1,1,1 |
| `BP_SubDoor_LowerD_BallastAft` | (-63, 0, -130) | (0, 0, 0) | 1,1,1 |
| `BP_SubDoor_UpperD_Airlock_Inner` | (-594, 0, 389) | (0, 0, 0) | 1,1,1 |
| `BP_SubDoor_UpperD_Armory` | (943, -132, 389) | (0, 180, 0) | 1,1,1 |

Toutes les 6 sont des `ChildActorComponent` pointant `/Game/Sub3D/Blueprint/SubBP/BP_SubDoor.BP_SubDoor_C`.

**Notes :**
- Les locations sont en cm dans le repère local du `SubmarineRoot` du BP (origine centrée sur la coque).
- Les yaw=180° distinguent les portes orientées vers –X (Aft side opening) des portes à 0° (Fwd side opening).
- `BP_SubAirLockExit` (parent `BP_SubDoor_Exterior`) **n'est pas dans la liste** car son nom ne commence pas par `BP_SubDoor`. Si tu en as besoin, je relance avec un préfixe relâché (`BP_Sub`).
- Le JSON complet est écrit dans `reports/2026-05-05_craniata_door_local_transforms.json`.
- Side-note : 5/6 portes mappent proprement aux `Connections` du DA (`N_main_Fwd`, `N_main_Control`, `N_lower_BallastFwd`, `N_lower_BallastAft`, `N_upper_UpperAirlock_Inner`). `BP_SubDoor_UpperD_Armory` reste sans connexion DA correspondante — toujours le "leftover" identifié dans l'audit du 2026-05-05.