# Barotrauma — Design patterns inspirants pour Sub3D

**Date** : 2026-05-05
**Source** : analyse des fichiers XML de `G:\Steam\steamapps\common\Barotrauma\Content\` (game C# DLL only, source pas accessible — analyse via data files)
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
