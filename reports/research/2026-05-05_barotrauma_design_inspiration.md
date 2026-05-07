# Barotrauma — Design patterns inspirants pour Sub3D

**Date** : 2026-05-05
**Source** : analyse des fichiers XML de `G:\Steam\steamapps\common\Barotrauma\Content\` + vérification du code source GitHub cloné dans `C:\Dev\_reference\Barotrauma` (`FakeFishGames/Barotrauma`, commit `7446dc1`)
**Contexte** : passe d'inspection avant Phase 2 du plan eau, pour identifier patterns à reprendre et à éviter

---

## 1. Architecture compartiments — `Hull` / `Gap` / `Door`

Barotrauma sépare proprement trois concepts :

| Entité Baro | Rôle | Équivalent Sub3D |
|---|---|---|
| `<Hull rect="x,y,w,h">` | Volume d'eau (rectangle 2D) | `UCompartmentVolumeComponent` (3D AABB) |
| `<Gap horizontal="false" rect="...">` | Passage potentiel entre hulls (géométrie sans logique) | `FGeneratedConnectionDef` (couplé avec door state) |
| `<Item identifier="door"> <Door IsOpen=".." OpeningSpeed="3"/> </Item>` | Acteur interactif qui gate un Gap | `ASubDoorActor` (déjà séparé) |

**Pattern clé** : Gap (topologie) **séparé** de Door (interaction). Quand la porte est cassée/disparue/non-installée, le Gap reste actif → eau peut passer.

**Application Sub3D** : on a déjà cette séparation conceptuelle (`FGeneratedConnectionDef` = gap, `ASubDoorActor` = entity). Ce qui pourrait être renforcé : la **Connection comme topologie pure** (toujours présente), **Door comme entité optionnelle** qui peut ne pas exister (ex: passage permanent type "Open"). C'est déjà supporté via `EConnectionType::Open` mais pas explicitement formalisé dans le code.

→ **À noter** dans le plan eau, pas d'action immédiate.

---

## 2. Linked Hulls — compound rooms

```xml
<Hull ID="13" rect="633,359,226,347" linked="39" RoomName="Airlock" />
<Hull ID="39" rect="633,679,188,320" linked="13" RoomName="Airlock" />
<Hull ID="74" rect="..." linked="73,289" />  <!-- multi-link -->
```

**Pattern** : graphe explicite bidirectionnel d'IDs. Hulls liés partagent water flow + oxygen sim. Chaque hull garde son propre `water` et `Oxygen` mais le simulateur traverse le graphe.

**Application Sub3D** : déjà couvert par notre proposition hybride dans le plan principal — voir Section 6 (Mécanisme 1 : same `CompartmentId` sur plusieurs volumes = compound implicite ; Mécanisme 2 : `LinkedCompartments` post-FP si besoin).

→ **Mécanisme 1 à intégrer Phase 2** (P2.3 union AABBs au bake).

---

## 3. WaterDetector — sensor pattern avec IO ports

```xml
<Item identifier="waterdetector" Tags="smallitem,sensor">
  <ConnectionPanel>
    <output name="state_out" />
    <output name="signal_out" />
  </ConnectionPanel>
</Item>
```

**Pattern** : un item-sensor expose un signal sur ses outputs (string-named ports). D'autres items (alarmes, lampes, pompes) lisent ces signaux via wires.

**Application Sub3D** : pas pour FP, mais excellent modèle pour **post-helm Phase 2 (intégration stations + hull breach reaction loop)** :
- HelmStation pourrait afficher un readout "flood level per compartment" via subscription au `USubFloodComponent`
- Future console "DamageControl" pourrait montrer des alarmes corrélées (water level threshold + breach detection)
- Pas besoin du système wired complet — juste des subscriptions C++ via delegates existants

→ **Référence pour design post-FP**. Aucune action FP.

---

## 4. Door component — IO + tool-gated unlocking

```xml
<Door OpeningSpeed="3" ClosingSpeed="3" ToggleCoolDown="1" IsOpen="False"
      ToggleWhenClicked="True" PickingTime="10" CanBePicked="True">
  <requireditem items="crowbar" type="Equipped" />
  <requireditem items="wrench" type="Equipped" />
  <requireditem items="screwdriver" type="Equipped" />
  <input name="toggle" />
  <input name="set_state" />
  <output name="state_out" />
  <output name="condition_out" />
</Door>
```

**Patterns** :
- `OpeningSpeed` / `ClosingSpeed` (séparés, asymétriques possibles)
- `ToggleCoolDown` (anti-spam)
- **Tools alternatifs** pour forcer (`crowbar`, `wrench`, `screwdriver`) — chaque outil = stratégie différente (forcer brutalement = `crowbar` sous pression élevée ; précis = `screwdriver`)
- `PickingTime` (temps d'interaction, scalable par skill)
- IO inputs/outputs pour automation

**Application Sub3D** :
- On a déjà l'asymétrie open/close via animation
- **Tool-gated unlocking** : intéressant pour gameplay narratif post-FP (door bloquée par eau, force avec crowbar, prend du temps)
- **PickingTime + skill scaling** : pattern rejoignable avec future skill system

→ **Pas FP**, à reprendre quand on développe le gameplay traversal/repair plus tard.

---

## 5. Repairable component — modèle pour breach repair (Phase post-FP)

```xml
<Repairable deteriorationspeed="0.125" mindeteriorationdelay="120"
            RepairThreshold="80" fixDurationHighSkill="5" fixDurationLowSkill="25"
            StressDeteriorationThreshold="0.9" MaxStressDeteriorationMultiplier="6">
  <RequiredSkill identifier="mechanical" level="55" />
  <RequiredItem items="wrench" type="equipped" />
  <ParticleEmitter particle="DarkSmoke" mincondition="0.0" maxcondition="80.0" />
</Repairable>
```

**Patterns** :
- `deteriorationspeed` + `mindeteriorationdelay` : dégradation passive avec délai aléatoire
- `RepairThreshold="80"` : on ne répare que si condition < threshold (pas de spam)
- `fixDuration*` scale par skill du personnage
- `StressDeterioration*` : extra dégradation si système overworked (= sous pression / surcharge)
- **VFX bound to condition range** via `mincondition`/`maxcondition` — fumée visible si condition < 80, plus intense < 15

**Application Sub3D** : modèle quasi-direct pour **boucle breach → réaction → réparation** (CLAUDE.md "post-helm roadmap" item 2).
- `USubHullComponent` a déjà `BreachClusters` + condition par cell
- Phase post-FP : ajouter
  - Tool-gated repair (crowbar/welder/wrench)
  - Skill-gated time
  - VFX bound to breach severity (déjà partiellement via `BreachVfxManagerComponent`)
  - Stress deterioration (sub sous attaque = dégradation accélérée)

→ **Note dans backlog post-FP**, mémoriser comme référence design.

---

## 6. Pump component — modèle pour ballast/bilge

```xml
<Pump maxflow="400" IsOn="False" flowpercentage="-100" powerconsumption="10">
  <StatusEffect type="InWater" target="This" IsOn="True">
    <Conditional Voltage="lte 0.0" targetcontainer="true" />
  </StatusEffect>
</Pump>
```

**Patterns** :
- `maxflow` (L/s) + `flowpercentage` ∈ [-100, +100] (signe = direction in/out)
- `powerconsumption` : tied to electrical network (post-FP feature)
- **StatusEffect-driven behavior** : pumps auto-turn-on when in water + voltage available

**Application Sub3D** : direct pour `USubBallastStation` future amélioration. On a déjà `DefaultPumpRateLitersPerSec = 100` dans `USubmarineDefinition`. Pourra :
- Ajouter direction (in/out) sur pumps
- Ajouter dépendance électrique (post-FP)
- Auto-activate logic via flood state subscription

→ **Bonne référence**. Petit refactor possible Phase 6+ (post-FP).

---

## 7. Hull semantic flags

```xml
<Hull AmbientLight="50,100,200,30" IsWetRoom="True" AvoidStaying="True"
      RoomName="RoomName.Airlock" Oxygen="78422" />
```

**Pattern** :
- `AmbientLight` color RGBA per hull → atmosphere variations
- `IsWetRoom` flag (airlock = eau permanente attendue)
- `AvoidStaying` AI hint (NPCs/bots évitent de squatter)
- `RoomName` = label affiché et utilisé pour mission scripting

**Application Sub3D** :
- ✅ `ESubCompartmentType` (Helm/Engine/Airlock/Generic) couvre déjà le côté semantic
- ✅ `LinkedAudioVolume` / `LinkedPostProcessVolume` (CLAUDE.md) couvrent le côté atmosphere — à activer post-FP
- 🆕 **`bIsWetRoom` flag** intéressant pour `USubmarineDefinition` ou `UCompartmentVolumeComponent` — sémantique "compartment où eau est attendue par défaut" (airlock, ballast, escape trunks)
- 🆕 **`AvoidStaying`** intéressant pour future AI crew (post-procedural-anim)

→ **Intégration mineure possible** : ajouter `bIsWetRoom: bool` à `FGeneratedCompartmentDef`. Coût : 5 min code. À faire quand on touchera le DA struct prochaine fois.

---

## 8. Système wired-event — référence post-FP "advanced mode"

Items ont des **ports nommés** (`toggle`, `set_state`, `state_out`, `power_in`, `condition_out`) connectés via wires physiques placés en éditeur. Logic gates : `andcomponent`, `orcomponent`, `notcomponent`, `addercomponent`, `signalcheckcomponent`. Les joueurs peuvent re-wire le sub avec un screwdriver.

**Application Sub3D** : **HORS SCOPE FP**. Réf intéressante pour mode avancé / outpost-building post-FP. On a déjà `Sub3DDebugSettings`, console commands, et architecture par-component avec delegates qui servirait de base si on voulait monter un système similaire.

→ **Pas pour le plan eau actuel**. Garder en tête comme inspiration future.

---

## Synthèse — qu'intégrer au plan eau / projet

### Action immédiate Phase 2 du plan eau

- **Mécanisme 1 (compound rooms via shared CompartmentId)** — déjà documenté dans le plan, à implémenter dans baker P2.3 + water plane spawn P3.4 (~+0.5j sur estim Phase 2).

### Petits ajouts opportunistes (à intégrer quand on touche les structs concernées)

- **`bIsWetRoom`** sur `FGeneratedCompartmentDef` (sémantique pour airlock / ballast / wet zones). 5 min code.
- **Door asymétrie open/close speed** dans `FGeneratedConnectionDef` (`OpeningSpeedCmS` / `ClosingSpeedCmS` séparés). Probablement déjà géré dans `ASubDoorActor` mais à confirmer.

### Notes pour roadmap post-FP

| Pattern Baro | Application Sub3D | Phase suggérée |
|---|---|---|
| `Repairable` (tool/skill/time + VFX-condition) | Hull breach repair gameplay loop | Post-helm roadmap step 2 (déjà au backlog) |
| `Pump` (flow direction + power dep) | Ballast/bilge pumps avec électricité | Post-FP (après hull breaking) |
| `Door` tool-gated unlocking | Doors bloquées par eau, force avec outils | Avec hull breaking |
| `WaterDetector` IO pattern | Stations affichent signaux flood/breach | Post-helm phase 3 (stations enrichies) |
| `AvoidStaying` AI hint | Crew AI behaviour post-procedural-anim | Procedural anim phase 2+ |
| Wired-event + logic gates | Mode "advanced" / outpost-building | Très long terme, hors roadmap actuelle |

### À éviter (pas forcément applicable à Sub3D 3D)

- **Hull rectangulaire 2D** — Barotrauma est side-scroller 2D, ses Hulls sont des `rect`. Sub3D est 3D et ses compartiments ont des formes complexes (cylindriques, courbes). **AABB + SDF + Marching Squares** (notre approche) est strictement plus expressif et adapté.
- **`linked` graphe bidirectionnel maintenu manuellement** — risque de désync. Notre approche `same CompartmentId = compound implicite` est plus robuste.

---

**Conclusion** : Barotrauma confirme la direction architecturale du plan eau Sub3D (Hull/Gap/Door séparation, compound rooms, semantic flags, IO patterns pour systèmes avancés). Quelques ajouts mineurs opportunistes (`bIsWetRoom`), et plusieurs références design pour la roadmap post-FP (Repairable, Pump direction, Door tool-gating).

**Action concrète suite à cette passe** : mettre à jour le plan eau Phase 2 avec compound rooms (déjà documenté). Pas d'autre changement code immédiat.

---

## 9. Water propagation algorithm (inféré — code C# DLL, pas accessible)

L'algo de propagation d'eau de Barotrauma n'est pas directement lisible (DLL only). Inféré depuis le pattern data (`<Hull water="N">`, `<Gap horizontal="bool" rect="...">`) + comportement gameplay observable :

| Aspect | Mécanisme Baro | Sub3D équivalent existant |
|---|---|---|
| Storage water par compartiment | `<Hull water="N">` valeur scalaire | `FCompartmentState.WaterLiters` ✅ |
| Passage entre compartiments | `<Gap horizontal="true/false" rect="W,H">` | `FFloodGraphEdge` + `PassageAreaCm2` ✅ |
| Direction du flux | `horizontal=true` = pressure-driven (Torricelli), `horizontal=false` = gravity-driven | `EConnectionType::Door` (horizontal) vs `Hatch` (vertical) ✅ |
| Door blocks gap | Item Door avec `IsOpen` couvre le rect du Gap | `FFloodEdgeState.bClosed` ✅ |
| Flow rate | Probablement `v = sqrt(2gh) × area` (Torricelli) avec damping | `USubFloodComponent::AdvanceFlooding` (Torricelli) ✅ |
| Surface eau visuelle | **Ligne 2D animée par hull (sin waves locales)** | Notre approche : cap mesh + heightfield CPU + boundary sync = **strictement plus ambitieux** |
| Cross-hull wave continuity | **ABSENTE** dans Barotrauma | Notre Phase 4 = saut qualitatif vs Baro |
| Crush pressure damage | Hulls prennent damage selon profondeur au-delà crush depth | Pas dans Sub3D FP, post-FP gameplay |

**Conclusions sur l'algo** :

1. ✅ **Sub3D = Barotrauma sur la sim** : per-compartment scalar level + Torricelli pour horizontal + gravity pour vertical + door state gates flow. Aucun design à modifier.
2. 🚀 **Sub3D > Barotrauma sur le visuel** : Barotrauma rend l'eau comme une simple ligne horizontale par hull avec des sin waves locales — pas de continuité cross-hull. Notre cap mesh + heightfield + boundary sync (Phase 2-4) est nettement plus ambitieux. Confirmation que notre vision (fluide unique entre compartiments) n'est PAS un standard du genre — c'est un objectif visuel qui nous différencie.
3. ⚠️ **Pas d'inspiration directe pour l'algo** : Baro confirme nos choix mais n'apporte rien que nous n'ayons déjà ou qui surpasserait notre approche heightfield.
4. 📌 **Crush pressure** : à mémoriser pour post-FP gameplay (boucle breach + damage par profondeur).

---

## 10. Extraction quantitative des fichiers vanilla

**Passe ajoutée** : analyse locale de `G:\Steam\steamapps\common\Barotrauma` le 2026-05-05.

**Fichiers lus** :
- `Content/ContentPackages/Vanilla.xml`
- `Content/Items/**/*.xml`
- `Content/Items/Assemblies/**/*.xml`
- `Content/Characters/**/*.xml`
- `Content/Submarines/*.sub` après décompression gzip en mémoire

**Limite** : aucun asset visuel/audio n'a été copié. Les chiffres ci-dessous viennent des définitions XML et des noms d'identifiants.

| Élément | Résultat |
|---|---:|
| Entrées `<Item file=...>` chargées par `Vanilla.xml` | 77 |
| Définitions `<Item>` parsées | 1153 |
| Variantes d'items (`variantof`) | 152 |
| Items cachés en menus | 138 |
| Entrées `<ItemAssembly file=...>` chargées | 49 |
| Sous-marins vanilla parsés | 22 |
| Entrées `<Character file=...>` chargées | 73 |
| Fichiers d'animation | 190 |
| Fichiers ragdoll | 58 |

### Répartition items

| Catégorie | Nombre | Lecture utile pour Sub3D |
|---|---:|---|
| `decorative` | 224 | Décor séparé du gameplay. Ne pas mélanger props et systèmes. |
| `equipment` | 117 | Base future pour inventaire/outils, hors FP. |
| `weapon` | 113 | Les armes sont des items, les tourelles sont des machines/stations. |
| `alien` | 92 | Même structure de composants, thème différent. |
| `misc` | 83 | Portes, hatches, docking, duct blocks. |
| `electrical` | 75 | Système électrique complet via ports nommés. |
| `material` | 72 | Crafting/deconstruction, hors FP. |
| `wrecked` | 67 | Variantes endommagées, très utile comme pattern post-FP. |
| `machine` | 22 | Pompes, moteurs, réacteur, oxygène. |

Le point important : la catégorie n'est pas la source de comportement. Le comportement vient surtout des composants enfants (`Door`, `Pump`, `Reactor`, `ConnectionPanel`, `Repairable`, `ItemContainer`, etc.).

### Composants gameplay principaux

| Composant XML | Nombre | Exemple d'usage | Application Sub3D |
|---|---:|---|---|
| `Holdable` | 343 | outils, armes, petits objets | Futur inventaire. Pas FP. |
| `LightComponent` | 278 | lampes, indicateurs, feedback état | Déjà pertinent pour stations et alarme flood. |
| `ItemContainer` | 241 | casiers, chargeurs, réacteur fuel rods | Pattern pour conteneurs/stations post-FP. |
| `ConnectionPanel` | 200 | ports nommés + wires | Ne pas porter en FP. Reprendre plus tard comme système de signaux. |
| `Wearable` | 108 | vêtements, diving suit | Future protection/pression/oxygène. |
| `Projectile` | 91 | munitions, tirs | Tourelles Sub3D peuvent rester station + projectile actor. |
| `Repairable` | 81 | état condition + outil + skill + VFX | Bon modèle pour repair breach post-FP. |
| `Door` | 26 | portes, hatches, alien doors | Sub3D a déjà `ASubDoorActor`. |
| `Turret` | 22 | armes montées | Sub3D a déjà `SubTurretStation` / `TurretActor`. |
| `Pump` | 9 | pump, smallpump, weakpoint leak | Sub3D a déjà `USubFloodComponent::SetPumpActive`. |
| `Sonar` | 7 | nav terminal, sonar monitor, handheld sonar | Sub3D est déjà plus avancé côté sonar. |
| `Engine` | 5 | moteur principal/shuttle | Sub3D a déjà commandes helm/engine. |
| `OxygenGenerator` | 4 | génération oxygène + tank refill | Post-FP, pas avant boucle flood jouable. |
| `Steering` | 3 | nav terminal | Sub3D a déjà helm station et commandes. |
| `Reactor` | 2 | réacteur sub/outpost | Post-FP, après électricité simplifiée. |

### Ports de connexion les plus fréquents

Barotrauma expose les systèmes via des ports textuels. Les plus fréquents :

| Entrées | Count | Sorties | Count |
|---|---:|---|---:|
| `toggle` | 82 | `signal_out` | 43 |
| `set_state` | 74 | `condition_out` | 37 |
| `power_in` | 57 | `state_out` | 37 |
| `signal_in` | 23 | `power_out` | 9 |
| `set_speed` / `set_targetlevel` | 6 chacun | `power_value_out` / `load_value_out` | 8 chacun |

**Conclusion** : pour Sub3D, si un système de signaux arrive un jour, il doit rester data-driven et nommé (`toggle`, `set_state`, `state_out`, `condition_out`). Pour FP, continuer avec appels C++ explicites et delegates existants.

---

## 11. Familles d'items utiles pour Sub3D

### Navigation / command

Items repérés : `navterminal`, `shuttlenavterminal`, `sonarmonitor`, `sonartransducer`, `statusmonitor`, `surveillancecenter`, `handheldsonar`, `handheldstatusmonitor`, `ruinscanner`.

**Pattern** :
- Un terminal n'est pas seulement une UI. C'est un item avec composants `Steering`, `Sonar`, `ConnectionPanel`, `Repairable`.
- Les monitors secondaires lisent des données et exposent aussi des ports.

**Sub3D** :
- Bon équivalent actuel : `ASubHelmStation`, `USubmarineSystemsComponent`, `USubSonarSystemComponent`, widgets Helm/Sonar.
- Ne pas créer un item générique maintenant. Garder les stations C++ séparées.
- Quand les stations deviennent modulaires, créer une définition de station avec capacités (`Helm`, `Sonar`, `Status`, `PumpControl`) plutôt qu'un héritage profond.

### Portes / hatches / docking

Items repérés : `door`, `windoweddoor`, `hatch`, `doorwbuttons`, `windoweddoorwbuttons`, `hatchwbuttons`, `dockingport`, `dockinghatch`, `ductblock`, variantes `wrecked`.

**Pattern** :
- Porte = logique d'interaction.
- Gap/hull = topologie flood.
- Les variantes avec boutons intègrent le contrôle local, pas un nouvel objet de topologie.
- Docking = power transfer + door/hatch state + ports.

**Sub3D** :
- La séparation actuelle `FGeneratedConnectionDef` + `ASubDoorActor` est correcte.
- `InitializeFromConnectionDef` existe déjà dans `ASubDoorActor`. Continuer à le traiter comme le chemin principal.
- À ajouter plus tard seulement : `OpeningSpeedCmS`, `ClosingSpeedCmS`, `ToggleCooldownSeconds`, `bCanBeForced`.

### Flood / pump / weakpoints

Items repérés : `pump`, `smallpump`, `pipeweakpoint1_water`, `wallpipeweakpoint`, `waterdetector`, `ductblock`.

**Pattern** :
- `Pump` a `maxflow`, direction via `flowpercentage`, consommation électrique.
- Les weakpoints utilisent aussi `Pump`, mais comme fuite active plutôt que pompe joueur.
- Les weakpoints ajoutent des `TriggerComponent` pour pousser/ralentir les personnages dans le flux.
- `WaterDetector` active des pumps/ducts via signal.

**Sub3D** :
- `USubFloodComponent` couvre déjà `CreateBreach`, `RemoveBreach`, `SetPumpActive`, et les edges.
- À reprendre post-FP : fuite = composant de flow local avec force appliquée aux crew proches, pas seulement un chiffre de litres/seconde.
- Pour FP, garder breach/pump simples. Les forces physiques du jet sont post-FP.

### Power

Items repérés : `reactor1`, `junctionbox`, `powerdistributor`, `battery`, `supercapacitor`, `relaycomponent`.

**Pattern** :
- Réacteur produit, junction boxes distribuent, batteries stockent, relays contrôlent.
- Presque tout expose `condition_out`, `power_value_out`, `load_value_out`.
- Les pannes électriques sont des états de condition + surcharge + chance de feu/choc.

**Sub3D** :
- Ne pas importer ce système avant FP.
- Pour une première version post-FP : un `UElectricalBusComponent` par sous-marin avec `Generation`, `Load`, `Battery`, `Faults`, puis branchement stations.
- Ne pas commencer par des wires physiques.

### Tools / equipment

Items repérés : `crowbar`, `wrench`, `screwdriver`, `weldingtool`, diving suits, oxygen tanks, fuel rods.

**Pattern** :
- Les outils ne contiennent pas la logique de réparation. Les composants `Door`, `Repairable`, `ConnectionPanel` déclarent les `RequiredItem`.
- Outil = clé d'accès + animation/temps + risque.

**Sub3D** :
- Pour le futur inventaire, éviter de mettre la logique dans l'item outil.
- Créer plutôt `URepairableComponent` / `UForceOpenComponent` qui demandent un tag outil (`Tool.Wrench`, `Tool.Cutter`, `Tool.Welder`).
- Pas nécessaire pour FP.

### Weapons / turrets

Items repérés : `coilgun`, `doublecoilgun`, `chaingun`, `railgun`, `flakcannon`, `pulselaser`, `depthchargetube`, loaders/ammo boxes.

**Pattern** :
- Turret montée = machine + item container ammo + connection panel + periscope/control.
- Ammo storage et weapon control restent séparés.

**Sub3D** :
- L'architecture `SubTurretStation` + `TurretActor` correspond bien.
- Quand les munitions arrivent : ne pas intégrer l'ammo au widget; créer un état ammo côté station/turret.

---

## 12. Item assemblies et sous-marins vanilla

Les `ItemAssembly` ne sont pas des nouveaux composants. Ce sont des prefabs éditoriaux : items placés + wires + links + gaps + structures.

Assemblies visibles utiles :

| Assembly | Items | Links | Lecture Sub3D |
|---|---:|---:|---|
| `Automatic Horizontal Airlock` | 16 | 16 | Sas = prefab de portes + pump + logique, pas générateur monolithique. |
| `Automatic Vertical Airlock` | 17 | 16 | Même pattern avec hatch. |
| `Bilge Pump` | 3 | 2 | Pump + water detector + wire. Très bon modèle de prefab simple. |
| `Backup Power Setup` | 7 | 8 | Battery + relay + junctionbox. Post-FP. |
| `Delayed Docking Hatch` | 25 | 24 | Docking fiable = logique dédiée, pas juste une porte. |
| `Wired Surveillance System` | 13 | 14 | Exemple de station multi-capteurs. |
| `Wall With Pipe Weakpoint` | 1 item + 1 structure | 0 | Weakpoint lié à une paroi endommageable. |

Les 22 sous-marins vanilla parsés contiennent au total :
- 10 397 items placés
- 4 133 wires
- 8 266 liens de connexion
- moyenne par sous-marin : 473 items, 188 wires, 376 liens

**Conclusion pour Sub3D** : ne pas viser une simulation de wiring façon Barotrauma pour FP. Le bon enseignement est la granularité des prefabs. Pour Sub3D : `BP_Airlock`, `BP_BilgePump`, `BP_DockingHatch`, `BP_DivingLocker` sont de meilleurs objectifs que "générateur qui sait tout faire".

---

## 13. Personnages, animation et ragdolls

### Personnages

Sur 73 personnages vanilla :
- 5 sont `Humanoid=True`
- 12 peuvent interagir
- 7 peuvent grimper
- 1 seul a `NeedsAir=True` : `Human`
- 5 peuvent parler
- 14 utilisent pathfinding
- 4 sont cachés au sonar

**Pattern utile** : les capacités fondamentales sont des flags data : `CanInteract`, `CanClimb`, `NeedsAir`, `CanSpeak`, `Noise`, `Visibility`, `HideInSonar`.

**Sub3D** :
- Pour les futurs NPC/creatures, créer des capabilities data assets plutôt que des sous-classes.
- `Noise` / `Visibility` mappe bien sur les systèmes Sub3D déjà présents (`SonarNoiseEmitterComponent`, sonar tracker).

### Animations

Fichiers d'animation parsés :

| Type | Count |
|---|---:|
| `SwimSlow` | 62 |
| `SwimFast` | 62 |
| `Walk` | 32 |
| `Run` | 29 |
| `Crouch` | 5 |

Animations humaines principales :

| Fichier | Type | MovementSpeed | CycleSpeed | Lecture |
|---|---|---:|---:|---|
| `HumanWalk.xml` | Walk | 1.7448974 | 3.1523502 | Base walk. |
| `HumanRun.xml` | Run | 4.7247353 | 1.8 | Run standard. |
| `HumanWalkDivingSuit.xml` | Walk | 1.1 | 4.05 | Suit = plus lent, cycle différent. |
| `HumanRunDivingSuit.xml` | Run | 2.5 | 2.8 | Suit limite la course. |
| `HumanWalkExosuit.xml` | Walk | 1.0 | 4.0 | Exosuit encore plus lourd. |
| `HumanRunExosuit.xml` | Run | 2.4 | 2.6 | Exosuit proche diving suit. |
| `HumanSwimSlow.xml` | SwimSlow | 1.5 | 3.640148 | Nage lente. |
| `HumanSwimFast.xml` | SwimFast | 2.5 | 3.640148 | Nage rapide. |
| `HumanCrouch.xml` | Crouch | 1.4841671 | 3.528752 | Crouch locomotion dédiée. |

**Pattern utile** :
- Barotrauma garde une animation paramétrique par mode (`Walk`, `Run`, `Swim`, `Crouch`) et par équipement lourd.
- Les valeurs sont data-driven : vitesse, cycle, step size, step lift, IK strength, climb speed.

**Sub3D** :
- `ECrewLocomotionStance` / `ECrewLocomotionGait` couvre déjà `Standing`, `Crouched`, `Prone`, `Swimming` et `Idle/Walk/Sprint/CrouchWalk/ProneCrawl/Swim`.
- `FAnimNode_CrewProcedural` applique déjà un ensemble fixe de bones procéduraux. C'est le bon équivalent 3D.
- À reprendre : ajouter plus tard des profils de locomotion par équipement (`Normal`, `DivingSuit`, `HeavySuit`) comme data, pas comme branches de code.
- À éviter : importer une logique ragdoll 2D. Elle ne s'applique pas directement au squelette UE.

### Ragdolls

58 ragdolls parsés. Le ragdoll humain a :
- 15 limbs
- 14 joints
- 2 colliders
- `CanWalk=True`
- `CanEnterSubmarine=True`

**Pattern utile** : même le modèle humain découpe les régions santé en `Head`, `Torso`, bras, jambes, avec multiplicateurs de dégâts par région.

**Sub3D** :
- Pas utile pour la locomotion FP.
- Utile plus tard pour damage model : zones santé simples (`Head`, `Torso`, `LeftArm`, `RightArm`, `LeftLeg`, `RightLeg`) avant toute simulation physique avancée.

---

## 14. Recommandations concrètes pour Sub3D

### À faire maintenant seulement si le fichier concerné est déjà touché

1. Ajouter `bIsWetRoom` à `FGeneratedCompartmentDef` quand on touchera `SubmarineDefinitionTypes.h`.
2. Ajouter des champs de vitesse porte (`OpeningSpeedCmS`, `ClosingSpeedCmS`) seulement quand une passe porte est déjà ouverte.
3. Garder `USubFloodComponent` comme source d'état flood. Ne pas introduire un système d'items avant la boucle FP.

### À faire post-FP

1. `URepairableComponent` inspiré de Barotrauma : condition, seuil, durée, outil requis, VFX par severity.
2. `UPowerBusComponent` simple : génération, load, battery, faults. Pas de wires physiques au départ.
3. `UEquipmentCapabilityProfile` pour crew : normal / diving suit / heavy suit, avec vitesses locomotion et oxygen/pressure flags.
4. Prefabs gameplay Unreal : `BP_Airlock`, `BP_BilgePump`, `BP_DockingHatch`, `BP_DivingLocker`.
5. Body-region damage data : tête/torse/bras/jambes, sans ragdoll physique avancé.

### À éviter

1. Ne pas copier la taxonomie complète de 1153 items. Sub3D n'a pas encore besoin d'un catalogue item générique.
2. Ne pas porter le système de wires physiques avant que les stations, flood, doors, repair et power simplifié soient jouables.
3. Ne pas faire du générateur un assembleur de tous les systèmes. Les assemblies Barotrauma confirment que les systèmes composés doivent vivre comme prefabs.
4. Ne pas déduire les besoins animation Sub3D depuis les ragdolls Barotrauma. L'inspiration utile est dans les paramètres de locomotion, pas dans la structure physique 2D.

---

## 15. Vérification contre le code source GitHub

Clone utilisé comme référence locale uniquement :

- Repo : `https://github.com/FakeFishGames/Barotrauma.git`
- Chemin local : `C:\Dev\_reference\Barotrauma`
- Commit inspecté : `7446dc1`
- Sparse checkout : `BarotraumaClient/ClientSource`, `BarotraumaShared/SharedSource`, `BarotraumaServer/ServerSource`, data partagée et contenu client.
- Aucun code ni asset Barotrauma n'a été copié dans Sub3D.

### Items et composants

Le code confirme que le comportement des items est bien composé par composants XML :

- `BarotraumaShared\SharedSource\Items\Item.cs:1260` ignore les sous-éléments purement descriptifs (`sprite`, `price`, `fabricate`, etc.), puis charge le reste comme composant.
- `BarotraumaShared\SharedSource\Items\Item.cs:1290` appelle `ItemComponent.Load(subElement, this)`.
- `BarotraumaShared\SharedSource\Items\Components\ItemComponent.cs:1019` résout le type de composant par réflexion depuis le nom XML, puis instancie le constructeur `(Item, ContentXElement)`.
- `BarotraumaShared\SharedSource\Items\Item.cs:1546` ajoute chaque composant à l'item, le met en cache par type, puis l'ajoute à la liste d'update seulement si nécessaire.

Correction par rapport à une lecture uniquement XML : la catégorie d'item est encore moins importante que prévu. Elle sert surtout à l'éditeur, à l'inventaire et aux filtres. Le gameplay vient du composant (`Door`, `Pump`, `Repairable`, `Powered`, `ConnectionPanel`, etc.).

Application Sub3D : ne pas créer un catalogue d'items générique pour le First Playable. Si un système d'items arrive post-FP, le bon équivalent Unreal est un `AActor` ou `UObject` item avec composants gameplay explicites, pas une hiérarchie profonde de classes.

### Door, Gap et Hull

Le code confirme et précise la séparation topologie / interaction :

- `BarotraumaShared\SharedSource\Items\Components\Door.cs:153` récupère ou crée le `Gap` lié à la door.
- `BarotraumaShared\SharedSource\Items\Components\Door.cs:489` pousse l'état d'ouverture de la door dans `LinkedGap.Open`.
- `BarotraumaShared\SharedSource\Items\Components\Door.cs:597` synchronise le gap lié : layer, door connectée, ouverture, lumière ambiante.
- `BarotraumaShared\SharedSource\Map\Gap.cs:373` contient l'update du gap, avec update sparse quand le flux est faible et update fréquent quand l'eau bouge.
- `BarotraumaShared\SharedSource\Map\Gap.cs:429` transfère aussi l'oxygène.
- `BarotraumaShared\SharedSource\Map\Gap.cs:431` différencie room-to-outside et room-to-room.
- `BarotraumaShared\SharedSource\Map\Gap.cs:461` calcule les transferts d'eau entre deux hulls selon ouverture, taille du gap, pression, volume d'eau et offset de sous-marin.
- `BarotraumaShared\SharedSource\Map\Hull.cs:301` garde `WaterVolume` borné par volume de compartiment compressible.
- `BarotraumaShared\SharedSource\Map\Hull.cs:883` met à jour oxygène, feu, surface d'eau et pression locale.

Correction : dans la première passe, `Gap` était décrit comme "géométrie sans logique". C'est trop faible. La door reste bien l'entité d'interaction, mais le `Gap` est aussi l'entité de simulation du passage : il calcule le flux entre hulls ou vers l'extérieur.

Application Sub3D : `FGeneratedConnectionDef` + `USubFloodComponent` est le bon équivalent. Ne pas déplacer la simulation de flux dans `ASubDoorActor`. La door doit seulement exposer son état, comme maintenant avec `SetDoorState`.

### Pump, water detector et machines

Le code source confirme les patterns utiles pour Sub3D :

- `BarotraumaShared\SharedSource\Items\Components\Machines\Pump.cs:46` expose `FlowPercentage` entre `-100` et `100`.
- `Pump.cs:58` expose `MaxFlow`.
- `Pump.cs:116` met à jour le hull courant, ajuste le débit auto si `TargetLevel` existe, puis modifie `CurrentHull.WaterVolume`.
- `Pump.cs:152` module le débit par power, condition de l'item et réparation/tinkering.
- `Pump.cs:232` reçoit des signaux `toggle`, `set_active`, `set_speed`.
- `WaterDetector.cs:62` lit le pourcentage d'eau du hull courant.
- `WaterDetector.cs:112` / `:123` publie des signaux `signal_out`, `water_%`, `high_pressure`.
- `Engine.cs:114` applique la force au sous-marin à partir de `targetForce`, voltage, condition et modifiers.
- `Engine.cs:270` reçoit `set_force` par signal.
- `Sonar.cs:49` sépare modes `Active` et `Passive`.
- `Sonar.cs:104` supporte les transducers externes comme source de signal sonar.

Application Sub3D : pour FP, garder `USubFloodComponent::SetPumpActive` et les cheats/protocoles existants. Post-FP, une pompe gameplay peut être un acteur simple avec `TargetCompartmentId`, `FlowRate`, `bPowered`, `bDamaged`, sans câblage physique.

### Wires, connection panels et power grid

Le système est riche mais trop large pour Sub3D avant FP :

- `ConnectionPanel.cs:13` garde une liste de ports `Connection`.
- `ConnectionPanel.cs:68` charge les ports depuis XML.
- `Wire.cs:55` garde deux connexions par wire.
- `Wire.cs:187` / `:209` connecte un wire à un port et marque les destinataires comme dirty.
- `Item.cs:2847` et `Item.cs:2870` parcourent les composants connectés directement ou récursivement via les panels.
- `Powered.cs:73` garde les connexions électriques dirty.
- `Powered.cs:75` garde les grids électriques.
- `Powered.cs:320` reconstruit les grids de puissance.
- `Powered.cs:479` résout load, power et voltage des grids.
- `PowerTransfer.cs:90` permet à des relais/junctions de couper ou transférer l'électricité.
- `PowerTransfer.cs:406` marque une connexion comme dirty quand le réseau change.

Application Sub3D : ne pas implémenter les wires physiques pour First Playable. Le plan autoritaire du 2026-04-10 garde la fermeture FP sur doors, airlock, flood, repair et validation. L'équivalent post-FP raisonnable est un bus électrique simple par sous-marin, pas un graphe éditable fil par fil.

### Repairable

Le code confirme que la réparation est un composant attaché à l'item/machine, pas une logique portée par l'outil :

- `Repairable.cs:41` définit une vitesse de détérioration.
- `Repairable.cs:83` définit un seuil sous lequel l'item devient réparable.
- `Repairable.cs:188` sépare `Repair`, `Sabotage` et `Tinker`.
- `Repairable.cs:245` initialise le délai de détérioration au chargement.
- `Repairable.cs:526` calcule un facteur de succès selon les skills requis.
- `Repairable.cs:551` transforme ce facteur en durée de réparation.
- `Repairable.cs:557` augmente ou restaure la condition pendant la réparation.
- `Repairable.cs:632` applique la détérioration quand l'item doit se dégrader.

Application Sub3D : le FP doit rester plus simple que Barotrauma. Pour fermer le protocole, l'action "Repair" peut seulement retirer la brèche proche et arrêter l'entrée d'eau. Post-FP, `URepairableComponent` peut reprendre les champs : `Condition`, `RepairThreshold`, `RepairDuration`, `RequiredToolTag`, `FailureRisk`, `StressMultiplier`.

### Item assemblies

Le code confirme que les assemblies sont des prefabs éditoriaux :

- `ItemAssemblyPrefab.cs:13` porte un TODO interne disant que les assemblies ne sont effectivement pas des entités.
- `ItemAssemblyPrefab.cs:64` repère les items contenus.
- `ItemAssemblyPrefab.cs:90` construit les entités d'affichage preview.
- `ItemAssemblyPrefab.cs:130` crée une instance en collant toutes les entités de l'assembly.
- `ItemAssemblyPrefab.cs:139` charge tous les `MapEntity`, applique un offset d'ID, déplace les entités et les rattache au sous-marin.
- `ClientSource\Map\ItemAssemblyPrefab.cs:50` sauvegarde une sélection d'entités comme `ItemAssembly`.

Application Sub3D : ça renforce la décision `BP_Airlock` du plan FP. Le sas ne doit pas être un nouveau générateur complet. C'est un prefab runtime/editor avec portes, collision, mesh et liaison flood.

### Animation et ragdoll

Le code source confirme que l'animation Barotrauma est paramétrique et data-driven :

- `AnimationParams.cs:13` définit les types `Walk`, `Run`, `SwimSlow`, `SwimFast`, `Crouch`.
- `AnimationParams.cs:125` définit `MovementSpeed`.
- `AnimationParams.cs:129` définit `CycleSpeed`.
- `AnimationParams.cs:184` dérive les noms de fichiers par espèce et type d'animation.
- `AnimationParams.cs:225` charge les paramètres d'animation par type depuis les fichiers.
- `HumanoidAnimations.cs:5` / `:16` / `:27` / `:47` / `:59` définissent les classes de paramètres humaines par mode.
- `AnimController.cs:62` choisit les paramètres courants selon eau, capacité de marche et forced animation type.
- `AnimController.cs:77` choisit walk/run/crouch côté grounded.
- `AnimController.cs:100` choisit swim slow/fast côté eau.
- `HumanoidAnimController.cs:697` avance le cycle de marche avec `CycleSpeed`.
- `HumanoidAnimController.cs:743` utilise IK pied/jambe pour poser les membres.
- `RagdollParams.cs:130` garde les colliders, limbs et joints sous forme de paramètres.

Application Sub3D : ne pas porter le ragdoll 2D. Pour Sub3D, l'inspiration utile est limitée aux profils de locomotion data : normal, diving suit, heavy suit, avec vitesse, cycle, effort, nage, crouch, oxygen/pressure flags. Cela s'aligne avec `FAnimNode_CrewProcedural` et les stances/gaits déjà listées plus haut.

### Synthèse après code source

Le code source ne change pas la recommandation FP. Il la durcit :

1. Continuer à fermer le First Playable avec les systèmes Sub3D existants : `USubFloodComponent`, `ASubDoorActor`, `BP_Airlock`, repair simple.
2. Ne pas ajouter un système d'items général avant que le protocole FP du plan `2026-04-10_first_playable_strategic_analysis.md` soit validé.
3. Garder les idées Barotrauma comme patrons post-FP : composants gameplay, assemblies/prefabs, bus électrique simple, repairable component, locomotion profiles.
4. Ne copier ni code ni assets. Les chemins Barotrauma ci-dessus sont uniquement des références d'analyse.
