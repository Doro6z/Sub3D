# FP Systems Audit — 2026-04-17

Brut. Raw observations + questions. Pour combler les gaps ensuite.

---

## 1. Walkable system — POURQUOI pas que de la collision

Pourquoi la distinction :

- Collision = physics blocking. Capsule character bloquée par n'importe quelle primitive collidable.
- Walkable = gameplay semantics "c'est du sol".

Sans walkable filter, le crew peut se tenir debout sur :
- Le dessus des bulkheads (si collision full)
- Les cadres de porte
- Les tuyaux, les valves, les props
- Les coins de console

Avec walkable filter, on restreint "floor base acceptée" à une liste explicite. Le character movement utilise quand même la collision pour tout le reste (murs, obstacles). Le walkable ne concerne QUE "sur quoi je peux me tenir debout".

**Aussi et c'est le vrai intérêt** : `USubInteriorFrameComponent`. Le crew à l'intérieur du sub est attaché au "frame" moving avec le sub (le sous-marin bouge dans l'océan, le crew doit rester solidaire). Le walkable component = la référence pour attacher le crew au frame.

Usage code confirmé dans `SubCrewMovementComponent.cpp:147` :
```cpp
const TArray<UPrimitiveComponent*> WalkableComponents = Submarine->GetInteriorWalkableComponents();
return WalkableComponents.Num() == 0 || WalkableComponents.Contains(const_cast<UPrimitiveComponent*>(CandidateBase));
```

Le filter est "IsBaseWalkable" du character movement. Quand le MovementMode change via TriggerFloorCheck, la base candidate (= sur quoi la capsule repose) est testée via cette fonction. Si Result.Num()==0, on accepte tout (mode permissif par défaut pour ne pas bloquer les tests). Si Result.Num()>0, seuls les components listés sont acceptés.

**Path déjà en place pour Craniata** :

`ASubmarineBase::GetInteriorWalkableComponents()` ligne 1013 a DEUX paths :
1. Generator path (GeneratedGeometry->GetInteriorFloorCollisionComponents()) — vide pour Craniata
2. Manual path — scan tous les PrimitiveComponent, retient ceux qui ont le tag `ManualWalkableTag` (default `"HandmadeWalkable"`)

**Donc pour Craniata il suffit de tagger** les SM_* qu'on veut walkable avec `"HandmadeWalkable"`. Zero code à écrire.

Candidats à tagger :
- SM_Deck_main, SM_Deck_upper, SM_Deck_lower_main
- SM_Deck_engine_upper, SM_Deck_engine_lower
- SM_Stair_UpperToMain_UpperAccess (merged)
- SM_BallastPasserelle_Fwd, SM_BallastPasserelle_Aft
- SM_AftTopAccess_Deck, SM_AftTopAccess_Ramp, SM_AftTopAccess_Link
- SM_Ladder_* si on veut crew climbing (mais ladders sont plus custom logic normalement)

**À vérifier** : est-ce que `ComponentHasTag` retourne true sur les ChildActorComponent ? Je pense que non, le tag doit être sur le SMC. Si tu composes ton BP avec CAC BP_SubDoor/BP_Turret/BP_HelmStation, les SMC statiques (decks) restent comme des components directs du BP submarine — tag-able normalement.

---

## 2. Input routing — déjà câblé

Flow confirmé :

```
Client
  ├── IA_HelmThrust (EnhancedInput, axis -1..1)
  │     ↓ binding in BP_SubPlayerController or C++
  │     ↓
  ├── APlayerController local → ServerRouteHelmThrust(Value)   [Server, Unreliable RPC]
  │
Server
  ├── ASubPlayerController::ServerRouteHelmThrust_Implementation (line 215)
  │     ↓ resolves CurrentStation OR Submarine
  │     ↓
  ├── Submarine->SubMovement->SetThrustInput(Value)          [ligne 241]
  │     ↓ (après patch 2026-04-17: UFUNCTION BlueprintCallable BlueprintAuthorityOnly)
  │     ↓ clamp -1..1 + écrit dans UPROPERTY(Replicated) ThrustInput
  │     ↓
Replication
  ├── ThrustInput / RudderInput / DivePlaneInput remontent à tous les clients
  │
Clients (all)
  ├── BP_Submarine_Craniata event Tick
  │     ↓ SubMovement->GetRudderInput() * 30° → Yaw target
  │     ↓ RudderAssembly->SetRelativeRotation(RInterpTo(...))
  │     → visual rudder follows server state
```

Trois axes existent :
- `ServerRouteHelmThrust` → `SetThrustInput` (propeller)
- `ServerRouteHelmSteer` → `SetRudderInput` (rudder)
- `ServerRouteHelmDive` → `SetDivePlaneInput` (hydroplanes)

Autres Server RPC routes dans `ASubPlayerController` (inventaire non-exhaustif) :
- `ServerRouteRudderHoldEnabled / PlaneHoldEnabled / StabilizationMaster` — auto-piloting flags
- `ServerRouteAutoSpeedEnabled / AutoDepthEnabled / AutoPitchEnabled` — autopilot toggles
- `ServerRouteTargetSpeedCmS / TargetDepthMeters / TargetPitchDeg` — autopilot targets
- `ServerRouteBallastGlobal / ByIndex / Active` — ballast control
- `ServerRoutePumpActive / PumpPower` — pump
- `ServerRouteEngineBoost` — boost
- `ServerRouteTurretAim / TurretFire` — turret
- `ServerRouteDoorToggle(FName DoorId, bool bClosed)` — door interaction
- `ServerRouteSonarPing / SetSonarPingHeld` — sonar
- Sonar mode / focus bearing / range preset / priority track

**Side note** : le BP parent `BP_SubPlayerController` (mentionné dans le commentaire header) fait le binding Enhanced Input. Je ne peux pas le lire (pas d'MCP live). À vérifier coté BP : input actions bindées, mapping context actif selon CurrentControlMode.

---

## 3. Helm Station + Helm Widget

### `ASubHelmStation` (SubHelmStation.h/cpp)

Micro-classe. 20 lignes header, 14 lignes cpp. Hérite `ASubStationBase`. Le vrai travail est dans :
- `ASubStationBase` (je ne l'ai pas relu ici)
- le BP enfant `BP_HelmStation` qui fait la partie visuelle (mesh console + widget attaché)

Contract :
- `StationType = ESubStationType::Helm`
- `bExclusiveOccupancy = true` (un seul pilote)
- `CanEnterStation` requiert un `OwningSubmarine` non-null

### `USubHelmWidget` (SubHelmWidget.h/cpp)

461 lignes header, 1318 cpp. Gros. Ne pas toucher sans raison. Contient :

- `EHelmPanelRuntimeState` enum (Unbound / Warming / Degraded / Valid)
- `FHelmPerceptionPanelData`, `FHelmGuidancePanelData`, `FHelmSystemsPanelData` etc. — structs des données d'affichage par panel
- Références à `USubSonarDisplayWidget`, `UHelmNavigationDisplayWidget`, `UHelmControlPanelWidget`, `UHelmStatusStripWidget`, `UReconstructionViewWidget`, `UTacticalGraphViewWidget`
- Panels : perception (sonar), guidance (nav), systems, reconstruction (mapping), tactical (targets)

Le widget est un BP enfant configuré dans `WBP_SubHelm.uasset`. Je ne peux pas lire le graphe BP sans MCP live. **À vérifier** : quels panels sont actuellement rendus, lesquels sont bindés à des sources runtime, lesquels restent en "Unbound" par défaut.

**Gap probable** : la reconstruction view et la tactical graph view ont probablement besoin d'un mapping sonar passif actif pour être meaningful. Sur un FP minimal, perception + guidance + systems suffisent.

---

## 4. SubmarineBase — inventaire composants

Construits en C++ (CreateDefaultSubobject) dans le constructor. Chaque `BP_Submarine_*` enfant hérite cette hiérarchie :

**Root + physique**
- `SubmarineRoot` (USceneComponent) — root, rotate via root yaw compensation
- `HullMesh` (UStaticMeshComponent) — ancien single-mesh hull (Craniata utilise 141 SMC séparés, HullMesh reste vide)
- `SubMovement` — math-based physics, replicated net state, inputs
- `SubHull` — structural sheets + breach clusters
- `SubFlood` — flood simulation, reads Definition->FloodGraph
- `Systems` — aggregate systems (power, life support, etc.)
- `Compartments` — runtime compartment state
- `StationManager` — discovers attached stations, routes station events

**Sonar + radar + navigation**
- `Radar`
- `Sonar` (USubSonarComponent)
- `SonarSystem` (USubSonarSystemComponent)
- `TunnelNavigationRuntime`
- `HelmNavigationDisplay`

**Interior + VFX**
- `InteriorFrame` (USubInteriorFrameComponent) — moving frame pour le crew
- `BreachVfxManager`
- `FloodWaterVisuals`
- `HullVisualDamage`
- `DoorFloodVfx`
- `FeedbackManager` (USubmarineFeedbackDirectorComponent)

**Generated geometry** (legacy/inutilisé pour Craniata handmade)
- `GeneratedGeometry` — utilisé seulement si `GeneratorSpec` set. Pour Craniata c'est `None`.

**Sockets**
- `HelmSocket` — attach point statique, obsolète maintenant que le helm est un station actor
- `CrewSpawnSocketP1` — obsolète si on utilise `GeneratedDefinition->SpawnPoints`
- `TurretHardpoint` — obsolète si CAC BP_Turret

**Data assets**
- `GeneratorSpec` (TObjectPtr<USubmarineGeneratorSpec>) — null pour Craniata
- `GeneratedDefinition` (TObjectPtr<USubmarineDefinition>) — = DA_SubDef_Craniata
- `GeneratorDoorActorClass` (TSubclassOf<ASubDoorActor>) — = BP_SubDoor_C (peut être None si on passe en CAC-only)

**Pilot tracking**
- `CurrentPilot` (AActor*, replicated) — le crew au helm
- `RepState` (FSubmarineNetState, replicated) — snapshot physics pour interp client
- `ExteriorTurret` (ATurretActor*, replicated) — turret externe

---

## 5. Damage system — état et gap

### Paramètres existants sur `ASubmarineBase`

- `HullImpactDamageScale = 0.0001` — legacy, convert impulse → damage
- `HullImpactRadiusCm = 18`
- `HullWeaponDamageRadiusCm = 35`
- `DamageToBreachInflowScale = 5` — pure generator path conversion damage→flood inflow
- `HullCollisionDamageMinSpeedCmS = 300` — en dessous de 3 m/s collision ne breach pas (A4)
- `HullCollisionCatastrophicSpeedCmS = 1250` — 12.5 m/s = damage max
- `HullCollisionDamageAtCatastrophicSpeed = 220`
- `HullCollisionDamageExponent = 2.0` — curve entre min et catastrophic
- `OnHullHit` UFUNCTION — callback collision (needs to be bound à OnComponentHit des mesh components hull)

### `USubHullComponent`

`StructuralSheets` (TArray<FStructuralSheetDef>) — ne peuvent pas être peuplés automatiquement pour Craniata, il y a un fallback bounds-based. Selon le plan 2026-04-16 :

> Do not keep fallback bounds-based hull sheets as the intended Craniata solution.

Donc pour l'instant on reste sur le fallback. Le vrai path (structural sheets authored explicitly) est post-FP.

### Flow damage → breach → flood

```
OnHullHit() or TakeDamage(DamageAmount, ...)
  ↓
USubHullComponent internal damage accumulation
  ↓
StructuralSheets breach when threshold crossed
  ↓
SubHull emits "BreachesUpdated" event
  ↓
ASubmarineBase::HandleBreachesUpdatedForFlood (line 1060)
  ↓
Pour chaque breach cluster exterior-touching :
  - Compute inflow rate (damage * DamageToBreachInflowScale)
  - Apply to compartment via SubFlood
  ↓
SubFlood simulation fills water
```

### Gap pour FP

1. **Collision pas setup** : les 141 SMC ont `AutoGenerateCollision = OFF`. OnHullHit ne sera jamais appelé tant qu'aucun SMC ne collide.
2. **OnHullHit binding** : probablement bindé dynamiquement sur le HullMesh component. Pour Craniata qui n'a pas de HullMesh (141 SMC à la place), le binding doit être étendu.
3. **Structural sheets pour breach real** : skip pour FP, utiliser le path `DamageToBreachInflowScale` direct.

---

## 6. Collisions — à setup

**Aucun** des 141 SMC imported n'a de collision assignée. Tous en `AutoGenerateCollision=OFF` à l'import (volonté pour éviter des collisions parasites à l'import initial).

### Collision profiles custom déjà créés dans `Config/DefaultEngine.ini`

Deux Object Channels custom :
- `Submarine` (ECC_GameTraceChannel1) — default `Block`
- `SubInterior` (ECC_GameTraceChannel2) — default `Ignore`

Trois profiles custom (lignes 249-251 de DefaultEngine.ini) :

**`SubmarineHull`** — pour la coque extérieure
- `CollisionEnabled=QueryAndPhysics` → simulé physiquement, raycast, impact events
- `ObjectTypeName=Submarine`
- Réponses :
  - WorldStatic / WorldDynamic : Block
  - **Pawn : Ignore** ← le crew à l'intérieur ne collisionne PAS avec son propre hull
  - Visibility / Camera : Block
  - Submarine : Block (collision sub-vs-sub)
  - SubInterior : Ignore
- → c'est ce qui déclenche `OnHullHit` quand un projectile/sub externe touche

**`SubInteriorWalkable`** — pour decks, stairs, bulkheads, cassette, ballast passerelles
- `CollisionEnabled=QueryOnly` → pas de physique simulée, mais raycast et overlap OK
- `ObjectTypeName=SubInterior`
- Réponses :
  - WorldStatic / WorldDynamic : Block
  - **Pawn : Block** ← crew bloqué, crew peut se tenir debout dessus
  - Visibility / Camera : Block
  - Submarine : Ignore (un hull exterior ne collisionne pas avec les decks intérieurs)
  - SubInterior : Ignore (entre eux)

**`SubInteriorVisual`** — pour cadres, props, pipes, tanks ballast, etc.
- Identique à `SubInteriorWalkable` en responses (Pawn=Block)
- Intent sémantique différent : "bloque le crew mais n'est pas du sol walkable"

### Mapping final (script `setup_craniata_collisions.py`)

**Raison clé pour `UseComplexAsSimple` côté intérieur** : les assets ont été importés avec `AutoGenerateCollision=False`, donc aucune primitive simple (box/convex) n'existe. `Default` en QueryOnly interrogerait une collision vide → crew traverse. `UseComplexAsSimple` force les triangles mesh à servir de simple collision, seul moyen sans un pass de convex decomposition post-import.

| Prefix | Profile | Complexity |
|---|---|---|
| `SM_Hull_Weld_*` | NoCollision | Default |
| `SM_Hull` (tout sauf welds) | **SubmarineHull** | **UseComplexAsSimple** |
| `SM_Deck_*`, `SM_Stair_*`, `SM_Ladder_*`, `SM_BallastPasserelle_*`, `SM_AftTopAccess_*` | SubInteriorWalkable | **UseComplexAsSimple** |
| `SM_BH_*`, `SM_Airlock_Cassette` | SubInteriorWalkable | **UseComplexAsSimple** |
| `SM_BallastAccess_*` | SubInteriorWalkable | **UseComplexAsSimple** |
| `SM_Hatch_*` | SubInteriorWalkable | **UseComplexAsSimple** |
| `SM_Pipe_*`, `SM_Valve`, `SM_Junction`, `SM_Reactor`, `SM_Engine` | SubInteriorVisual | **UseComplexAsSimple** |
| `SM_Ballast_Service`, `SM_BallastTank_*`, `SM_Ballast_*` | SubInteriorVisual | **UseComplexAsSimple** |
| `SM_MooringLug_*` | SubmarineHull | Default |
| `SM_Door_*`, `SM_Airlock_Door_*`, `SM_HatchDoor_*` | NoCollision | — (remplacés par CAC) |
| `SM_Hydro_*`, `SM_Fin_*`, `SM_Rudder*`, `SM_Skeg` | NoCollision | — (rotation visuelle) |
| `SM_Propeller`, `SM_Propulsor`, `SM_Duct` | NoCollision | — |
| `SM_Periscope_*`, `SM_Antenna*`, `SM_Flag_*` | NoCollision | — |
| `SM_TurretStation_*`, `SM_TurretSocket_*`, `SM_Hardpoint_*`, `SM_Turret_*` | NoCollision | — (CAC) |
| Fallback `SM_*` | NoCollision | Default |

**MODE par défaut du script** : `"all"` — applique le mapping complet. PIE test 1 va donc tester en même temps le hull + le walkable crew.

### Automatisation — script créé

`Scripts/UE5/setup_craniata_collisions.py` — prefix-match sur les 141 assets, set `CollisionProfileName` + `CollisionTraceFlag` sur le body setup, save si changé.

Mode selector :
- `MODE = "hull_only"` → applique uniquement à `SM_Hull` (pour test PIE 1 minimal collision + OnHullHit)
- `MODE = "all"` → tout le mapping complet
- `MODE = "dry_run"` → log sans toucher

Run headless via `UnrealEditor-Cmd.exe -ExecutePythonScript=...` ou dans la Python Console UE.

### Pour le premier PIE test

MODE=`hull_only` → seul `SM_Hull` devient collidable avec profile `SubmarineHull` en `UseComplexAsSimple`. Suffisant pour :
- Valider que `OnHullHit` se déclenche au contact d'un projectile ou d'un sub externe
- Valider que le hull bloque les impacts
- Ne pas se faire ch* avec le crew qui reste en mode fantôme (pas de walkable setup)

Ensuite MODE=`all` pour enable le crew walking + interior props.

---

## 7. Exterior Door Actor — spec + code créé

### Contract

- Hérite `ASubDoorActor` — récupère `bClosed`, `SetDoorClosed`, `BP_OnDoorStateChanged`, `DoorId`, `CompartmentA/B`, `bLocked`, `bStartsClosed`, `OwningSubmarine`, `Interactable`, `DoorCollision`, `Root`
- Ajoute `BattantPort` + `BattantStbd` (UStaticMeshComponent) attachés au `Root`
- Paramètres BP-editable :
  - `BattantExtensionCm` (float, default 90) — distance latérale en Y local
  - `OpenDurationSec` (float, default 5) — durée d'animation
- Helpers BP :
  - `GetPortOpenOffsetLocal()` → `(0, -BattantExtensionCm, 0)`
  - `GetStbdOpenOffsetLocal()` → `(0, +BattantExtensionCm, 0)`

### Animation côté BP (à faire par l'user)

Dans `BP_SubDoor_Exterior` (enfant de `ASubDoorActor_Exterior`) :
- Event `BP_OnDoorStateChanged(bNowClosed)` :
  - Play Timeline (curve 0→1 sur OpenDurationSec)
  - Direction = `bNowClosed ? Reverse : Forward`
- Timeline Update :
  - `BattantPort->SetRelativeLocation(Lerp(ZeroVector, GetPortOpenOffsetLocal(), Alpha))`
  - `BattantStbd->SetRelativeLocation(Lerp(ZeroVector, GetStbdOpenOffsetLocal(), Alpha))`
- FX hooks possibles (à l'ouverture) :
  - Niagara spawn (eau qui coule, lumière ambiante qui change)
  - Audio cue (bruit mécanique, sifflement pression)
  - BreachVfxManager trigger si ouvert sous-marin

### Fichiers

- `Source/Sub3D/Submarine/SubDoorActor_Exterior.h` — créé
- `Source/Sub3D/Submarine/SubDoorActor_Exterior.cpp` — créé

### BP à faire

- `BP_SubDoor_Exterior` — BP enfant de `ASubDoorActor_Exterior`
- Setup :
  - Static Mesh sur `BattantPort` = `SM_Airlock_Door_Port`
  - Static Mesh sur `BattantStbd` = `SM_Airlock_Door_Stbd`
  - Pivot locaux au centre du battant (déjà fait dans Blender via `fix_all_pivots` fallback bbox-center)
  - Timeline d'animation
  - BP_OnDoorStateChanged → Play/Reverse timeline

### Placement dans `BP_Submarine_Craniata`

- Tes CAC actuelles `Door_SAS_Inner` / `Door_SAS_Exterior` → remplacer celle qui correspond au SAS exterior par `BP_SubDoor_Exterior` au lieu de `BP_SubDoor`
- Position BP-local `(0, 814.8, 140)` (`N_upper_UpperAirlock_Exit`)

---

## 8. Gap list — ordonnée priorité

### Fait (2026-04-17)

- **Portes bulkhead (6 interior doors)** placées manuellement dans `BP_Submarine_Craniata` via ChildActorComponent `BP_SubDoor` :
  - Door_Main_Fwd, Door_Main_Control (main deck bulkheads)
  - Door_Lower_BallastFwd, Door_Lower_BallastAft (lower deck bulkheads)
  - Door_Upper_UpperAirlock_Inner (SAS inner)
  - Door_Upper_Armory
- **Porte Exterior SAS** à placer (1 CAC `BP_SubDoor_Exterior` à l'emplacement `N_upper_UpperAirlock_Exit` = BP-local `(0, 814.8, 140)`)
- **Collision profiles custom** créés dans `Config/DefaultEngine.ini` : `SubmarineHull`, `SubInteriorWalkable`, `SubInteriorVisual` + 2 object channels (`Submarine`, `SubInterior`)
- **C++ build** OK post SubMovementComponent patch + SubDoorActor_Exterior

### P0 (bloque le FP — doit être fait avant PIE test 1)

1. **Collision bulk setup** — script `setup_craniata_collisions.py` prêt. Premier run en `MODE="hull_only"` pour PIE test 1 (SM_Hull en SubmarineHull UseComplexAsSimple)
2. **`BP_HelmStation` mesh console non assignée** — assigner un SM (ex: un console block) sinon visuellement crew interagit avec rien
3. **3 SMC `SM_TurretSocket_*` à remplacer par CACs `BP_TurretActor`** dans Craniata — turret loop bloquée sinon
4. **BP_SubDoor_Exterior** à créer (enfant `ASubDoorActor_Exterior`) + assigner meshes Port/Stbd + Timeline d'animation
5. **Placer le CAC exterior** dans `BP_Submarine_Craniata` à `(0, 814.8, 140)` BP-local + supprimer SMC statiques `SM_Airlock_Door_Port/Stbd`

### P1 (quality-of-life + PIE tests)

6. **Tag `HandmadeWalkable`** sur SM_Deck_* / SM_Stair_* / SM_BallastPasserelle_* / SM_AftTopAccess_* — sinon crew ne peut pas se tenir debout (blocker PIE test 2)
7. **BP_SubDoor : ajouter Timeline** + curve + paramètre `OpenDurationSec` pour remplacer le snap instantané actuel (en cours chez toi). Anim qualité FP.
8. **PIE Test 1** (tu as demandé celui-là en premier) : collision + damage OnHull uniquement sur `SM_Hull`. Tirer un projectile / impact à vitesse → vérifier OnHullHit fires, breach via `DevCheat_CreateBreach` pour bootstrap flood
9. **PIE Test 2** : collision full + walkable. Crew spawn → walk sur decks sans clipping
10. **PIE Test 3** : doors interactive via `IA_Interact` sur Crew → cast vers BP_SubDoor Interactable → `SetDoorClosed(toggle)` → animation
11. **Wire rudder/hydroplane visual** en BP event graph (`GetRudderInput() × 30°` → `SetRelativeRotation` avec `RInterpTo`)

### P2 (post-FP validation)

10. Structural sheets explicit pour Craniata — replace le fallback bounds-based
11. Breach VFX tuning (déjà wiré sur `BreachVfxManager`)
12. Helm widget panels — verify perception/guidance/systems bindings
13. Sonar interactive en PIE
14. Turret firing loop (code existe ATurretActor, BP à configurer)

### P3 (stretch pour FP+)

15. Damage system avec real structural sheets
16. Multi-player test (2-4 clients)
17. Save/Load state

---

## 9. Findings manuels BP inspection (2026-04-17, user-provided)

### `PC_SubPlayerController.uasset` (= BP_SubPlayerController)

Enhanced Input bindings confirmés (via K2Node_EnhancedInputActionEvent + ImplicitCast) :

| IA | Destination |
|---|---|
| `IA_Thrust` | `ServerRouteHelmThrust(Value)` |
| `IA_Rudder` | `ServerRouteHelmSteer(Value)` |
| `IA_DivePlane` | `ServerRouteHelmDive(Value)` |
| `IA_FireTurret` | `ServerRouteTurretFire(bHeld)` |
| `IA_ExitStation` | `ServerExitStation` |
| `IA_HelmDirectInputHold` / `IA_HelmMouseDirect` | probable local → `ServerRouteTurretAim` |
| `IA_SonarLean` / `IA_SonarPing` / `IA_RadarToggle` | routage local (pas de ServerRoute dans strings) |
| `IA_Look` / `IA_Move` | relayés au niveau PC |

**IMC switching** géré 100% en BP (respecte commentaire C++) :
- Custom function `ApplyControlMode(NewControlMode)` → `RemoveMappingContext` + `AddMappingContext`
- Event `BP_OnControlModeChanged` fires à chaque switch
- 3 IMCs : `IMC_OnFoot`, `IMC_Helm`, `IMC_StationUI` (mapping 1:1 avec l'enum)
- Station UI instance les WBP via `CreateStationWidgetFromType` : `WBP_SubBallast`, `WBP_SubEngine`, `WBP_SubHelm`, `WBP_SubRadar`, `WBP_SubTurret`

→ **Input wiring complet et correct.** Pas de gap P0 ici.

### `BP_HelmStation`

- Parent : `ASubHelmStation` (C++)
- Components :
  - `StaticMesh` (default scene root) — **MESH NON ASSIGNÉ** (gap bloquant)
  - `Interactable` (UInteractableComponent) — OnInteract → cast PC → `ApplyControlMode(StationUI)`
- **Pas de Widget 3D attaché** — `WBP_SubHelm` est créé en viewport via `CreateStationWidgetFromType`, pas comme WidgetComponent
- **Pas de socket "Embark/Sit/Seat"** — crew s'assoit par défaut via logique `ASubStationBase` (pas de pose custom BP)

**Gap P0** : assigner une mesh console avant démo FP. Trancher Widget 3D vs viewport widget.

### `WBP_SubHelm` (parent `USubHelmWidget` C++)

BindWidget détectés (matching `BindWidgetOptional` du .h lignes 357-377) :

| Champ C++ | Présent | Panel wrapper |
|---|---|---|
| `SonarDisplay` | ✓ | sous-widget custom |
| `ReconstructionView` | ✓ | `WBP_ResontructionView` (typo conservée) |
| `ForwardReconstructionView` | ✓ | `WBP_ResontructionView` |
| `TacticalGraphView` | ✓ | `WBP_TacticalGraphView` |
| `ControlStackPanel` | ✓ | `WBP_HelmControlPanel` |
| `StatusStripPanel` | ✓ | `WBP_HelmStatusStrip` |
| `HelmNavigationDisplay` (legacy) | ✗ | non instancié |

Autre panel : `WBP_SubRadar` (attaché en canvas, nom non listé dans BindWidget).

Bindings runtime visibles :
- `TurretAimCmd`, `TurretFireHeld` ← lus depuis `FSubmarineCommandState`
- `HelmYawCmd`, `TargetPitchDeg`, `bAutoPitchEnabled` ← même source
- `OwnerController` → cast `PC_SubPlayerController_C`
- `SubmarineSystemsComponent` ← cast depuis `SubmarineBase.Systems`

**Unbound** : `FHelmPerceptionPanelData`, `FHelmGuidancePanelData`, `FHelmAlertPanelData` ne sont pas bindées explicitement → probable `EHelmPanelRuntimeState::Unbound` au runtime, auto-discovered via `DiscoverWidgetReferencesFromTree()` avec `bAllowWidgetTreeFallbackDiscovery = true`.

**Gap P2** : vérifier en PIE que les panels Perception/Guidance/Alerts se bindent via fallback discovery. Sinon câbler explicitement.

### `BP_SubDoor`

- Parent : `ASubDoorActor` (C++)
- Components BP additionnels : aucun
- Material : `MI_Craniata_Door`
- Mesh default : `SM_Door_Lower_BallastAft` (override per-instance dans Craniata CAC)

**Animation open/close** :
- **PAS de Timeline** (aucune référence `Timeline_` ou `FloatCurve` dans CDO)
- Variables BP : `OpenLocation`, `CloseLocation`, `NewRotation` (+ split pins Pitch/Roll/Yaw)
- Event `BP_OnDoorStateChanged(bNowClosed)` → `K2_SetRelativeRotation` direct sur `DoorMesh`
- **⇒ Snap instantané. Pas d'interpolation. Pas de durée paramétrable. Pas de FX.**
- (user note : Timeline node BP en cours d'ajout)

**Gap P1** : ajouter Timeline + curve + paramètre `OpenDurationSec` pour aligner avec le pattern que j'ai mis dans `BP_SubDoor_Exterior`. Sans ça la sensation FP sera "snap" indigne d'un sub sim.

### `BP_Submarine_FPRun` vs `BP_Submarine_Craniata`

Parent commun : `ASubmarineBase`. Tous les native C++ components hérités à l'identique.

**Note importante : BP_Submarine_FPRun est LEGACY / inutile.** Gardé uniquement comme template source pour `harvest_level_to_bp.py` (qui l'a dupliqué pour créer Craniata). Production tourne sur Craniata.

Différences CDO :

| Propriété | FPRun | Craniata |
|---|---|---|
| `GeneratorSpec` | `DA_SubGenSpec_FPRun` | **`None`** |
| `GeneratedDefinition` | null (derived runtime) | **`DA_SubDef_Craniata`** |
| `GeneratorDoorActorClass` | `BP_SubDoor` | non override |
| `CrewSpawnSocket` | P1 seul | **P1..P4 (4 sockets)** |
| CAC doors | aucune (spawn runtime) | **9 CACs explicites** : `BP_SubDoor_LowerD_BallastAft/Fwd`, `MainD_Control`, `MainD_Fwd`, `UpperD_Airlock_Inner`, `UpperD_Armory`, `UpperD_1` |
| CAC exterior door | aucune | **`BP_SubAirLockExit`** → remplacé par **`BP_SubDoor_Exterior`** |
| Stations | refs classe seulement | CACs `BP_HelmStation`, `BP_EngineStation`, `BP_BallastStation` |
| Lights | aucun | CACs `BP_SubLight_Cold`, `BP_SubLight_Warm` + `PointLight` / `SpotLightComponent` |
| Turret sockets | aucun | **3 SMC placeholders** : `SM_TurretSocket_AftTop/FwdTop/FwdBottom` (pas encore de CAC BP_TurretActor) |
| 141 hull SMC | absent | **présents** (harvest_level_to_bp) |

Composants hérités **intentionnellement conservés** sur Craniata : tous les native C++ (Sub*, Radar, Sonar, InteriorFrame).

**Composants hérités mais INUTILISÉS sur Craniata** :
- `HullMesh` (UStaticMeshComponent) : vide, remplacé par les 141 SMC
- `HelmSocket` (USceneComponent) : obsolète, `BP_HelmStation` est un CAC
- `CrewSpawnSocketP1` : obsolète si on utilise `GeneratedDefinition->SpawnPoints` (à trancher)
- `TurretHardpoint` : obsolète, 3 turret sockets/CACs à placer

**Gap P1** : remplacer les 3 SMC `SM_TurretSocket_*` par CACs `BP_TurretActor` pour valider la turret loop.

### `BP_TurretActor`

- Parent : `ATurretActor` (C++)
- Components additionnels BP :
  - `SM_TurretBase1` (static mesh `Base_002`) — socle
  - `SM_TurretBase2` (static mesh `Top_Turret`) — tourelle rotative
- Inherited C++ : `Root`, `YawPivot`, `PitchPivot`, `TurretMesh`
- Aim logic : **100% C++** (`SetAimCommand`, `SetFireHeld`, `SetOnline` dans `TurretActor.h:75-82`). Aucune logique BP custom.
- Station wrapper : `BP_TurretStation` (parent `SubTurretStation` C++) → OnInteract → cast PC → `ApplyControlMode(StationUI)`

**Gap P1** : pas de CAC `BP_TurretActor` monté sur les 3 sockets de Craniata → turret loop pas testable en FP.

### `BP_SubmarineCrew` (= BP_SubmarineCrew.uasset)

- Parent : `ASubCrewCharacter` (C++)
- Camera setup (inherited C++) :
  - `FPSCamera` (CameraComponent)
  - `TPSCameraBoom` (SpringArm) + `TPSCamera`
  - Toggle via `ToggleCameraMode()` en C++
- `SkeletalMesh = SK_Crew_Basic`
- `AnimInstance = ABP_Crew`
- Composant BP-added : `RuntimeSyncDiagnosticsCrew` (plugin URuntimeSyncDiagnosticsComponent)

Enhanced Input bindings **sur le Crew** (pas sur le PC) :

| IA | Handler BP |
|---|---|
| `IA_Interact` | `TryPrimaryInteract` (+ appel C++ `Interact()`) |
| `IA_Repair` | `TryRepairFocusedTarget` (+ `PlayWorldCameraShake`) |
| `IA_Run` | `RequestRunStart` / `RequestRunStop` (down/up) |
| `IA_PostureScroll` | `AddPostureDelta(Delta)` |

**Observation** :
- `IA_Move` / `IA_Look` ne sont **PAS** bindés sur le Crew → gérés côté PC
- Crouch / jump / traversal → absents en BP, gérés en C++ via `USubCrewMovementComponent` ?

**À vérifier** : aligner l'IMC_OnFoot pour router `IA_Move`/`IA_Look` vers les bons handlers Crew (via PC relay ou directement).

---

## 10. Notes diverses

- UE 5.7.4 / Blender 5.0 — versions confirmées
- Plugin UnrealClaude monté via junction vers `C:/Dev/Tools/UnrealClaude/UnrealClaude/` (source checkout)
- `DA_SubDef_Craniata` populé via `import_sub_definition.py` (9 compartments, 9 connections, flood graph, 9 spawn points)
- `BP_Submarine_Craniata` composé via `harvest_level_to_bp.py` (141 components aux bonnes positions depuis level temp)
- Transforms Blender→BP-local mapping : `(x, y, z) → (-y, x - 2100, z)`, root yaw compensation `+90°`
- Coordinate contract documenté dans `compose_craniata_bp_v2.py`
- `FORCE_UNIT_SCALE = True` dans compose script — défense contre le scale baking Blender
- `core/pivots.py` appelle `fix_all_pivots` en fin de `main.py` — pivots gameplay-friendly (hinge pour portes, min X pour rudder, inner edge pour hydro, base center pour turrets)

---

## 11. Ce qui vient juste d'être ajouté / modifié dans cette passe

### Code C++
- `Source/Sub3D/Submarine/SubMovementComponent.h` — UFUNCTION setters + Replicated inputs + BlueprintPure getters
- `Source/Sub3D/Submarine/SubMovementComponent.cpp` — DOREPLIFETIME + setter impls en cpp
- `Source/Sub3D/Submarine/SubDoorActor_Exterior.h` — classe exterior door (nouveau)
- `Source/Sub3D/Submarine/SubDoorActor_Exterior.cpp` — impl (nouveau)

Build OK (validé 2026-04-17).

### Scripts
- `Scripts/UE5/setup_craniata_collisions.py` — bulk collision preset par prefix. MODE={hull_only | all | dry_run}

### Author manual (user)
- 6 CAC `BP_SubDoor` placés pour les portes bulkhead interior
- Reste : 1 CAC `BP_SubDoor_Exterior` à placer pour le SAS exit
- Collision profiles déjà déclarés dans `Config/DefaultEngine.ini`
