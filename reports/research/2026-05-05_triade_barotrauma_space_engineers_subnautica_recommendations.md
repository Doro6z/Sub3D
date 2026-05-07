# Triade Barotrauma + Space Engineers + Subnautica pour Sub3D

Date: 2026-05-05
Projet: Sub3D
But: identifier les structures, logiques de gameplay, patterns techniques et limites utiles pour inspirer Sub3D sans copier du code source ni importer d'assets.

## Sources inspectees

### Barotrauma

- Installation locale: `G:\Steam\steamapps\common\Barotrauma`
- Source GitHub clone local: `C:\Dev\_reference\Barotrauma`
- Repo: `https://github.com/FakeFishGames/Barotrauma`
- Commit inspecte: `7446dc1`
- Rapport detaille: `reports/research/2026-05-05_barotrauma_design_inspiration.md`

Points verifies:

- Definitions d'items XML + composants charges dynamiquement.
- `ItemComponent` comme logique attachee a un item.
- Portes qui pilotent un `Gap`; le `Gap` simule le flux.
- Pompes a debit signe, dependantes de l'energie, de la condition et des signaux.
- `Repairable` avec condition, outil, temps, skill, sabotage/tinker.
- `ItemAssembly` comme assemblage editorial/prefab, pas comme type runtime unique.
- Animations data-driven par profils de locomotion.

### Space Engineers

- Source GitHub clone local: `C:\Dev\_reference\SpaceEngineers`
- Repo: `https://github.com/KeenSoftwareHouse/SpaceEngineers`
- Commit inspecte: `54f2f0f`
- Rapport detaille existant: `reports/research/2026-05-05_space_engineers_room_detection.md`

Points verifies:

- `MyGridGasSystem`: detection de pieces par flood-fill BFS sur grille voxel.
- `MyOxygenRoom`: scalar par room pour oxygene / pressurisation.
- `MyOxygenRoomLink`: indirection de pointeur pour merger des rooms sans mettre a jour toutes les cellules.
- `MyAirVent`: ventiler ou drainer une room avec ressources et controle.
- `MyComponentStack` / `MySlimBlock`: construction, integrite, composants manquants, reparation.
- `MyProductionBlock` / `MyAssembler`: queues de production, inventaires input/output, blueprints.

### Subnautica

- Installation locale: `C:\Games\Subnautica`
- Build local verifie:
  - `Subnautica_Data\StreamingAssets\__buildnumber.txt`: `14`
  - `Subnautica_Data\StreamingAssets\__buildtime.txt`: `7/10/2020 8:00:26 PM`
- Rapport detaille existant: `reports/research/2026-05-05_subnautica_flood_analysis.md`
- Donnees texte inspectees: `Subnautica_Data\StreamingAssets\SNUnmanagedData\LanguageFiles\English.json`

Points verifies:

- Categories de fabrication et progression: vehicules, modules Cyclops, pieces de base, upgrades, fragments.
- Messages Cyclops: breaches, power, fire suppression, decoys, engine state.
- Modules Cyclops: depth MK1/2/3, shield, sonar, docking repair, thermal reactor, engine efficiency, decoy tube, fire suppression.
- Progression par fragments et blueprints.
- Rapport local deja present: Subnautica ne simule pas une inondation interieure volumetrique comparable a Sub3D; les leaks sont surtout des FX relies a la sante.

## Verdict court

La meilleure direction pour Sub3D n'est pas de prendre un seul modele. Il faut composer:

- Barotrauma pour la logique d'items, d'interactions, de portes, pompes, reparations, assemblages et signaux.
- Subnautica pour le feeling first-person, la progression lisible, les upgrades de sous-marin et la liaison degats -> FX.
- Space Engineers pour les concepts de ressources, construction, integrite, production et indirection de rooms si Sub3D a un jour des compartments lies dynamiquement.

La simulation d'eau de Sub3D doit rester plus ambitieuse que Subnautica et plus adaptee a notre architecture que Space Engineers. Les patterns confirmes comme universels sont:

- etat par compartiment;
- porte comme barriere de flux;
- pompe comme transfert actif dependant d'un etat;
- degat qui active des effets visuels lisibles;
- modules/upgrades qui modifient des capacites du sous-marin.

Ce qui est unique ou presque pour Sub3D:

- water heightfield 3D par compartiment;
- cap mesh par compartiment;
- synchronisation visuelle cross-compartment aux frontieres;
- compartiments DA-authored avec topologie explicite plutot que grille voxel.

## Decision FP

Pour le First Playable, il ne faut pas ajouter un systeme generique d'items, de craft, de wiring ou d'inventaire complet. Le plan autoritaire actuel reste correct: fermer d'abord la boucle jouable avec sous-marin, portes, airlock, inondation, reparation simple et validation.

Actions conseillees FP:

1. Garder `USubFloodComponent` comme coeur de simulation eau.
2. Garder `ASubDoorActor` comme pont entre gameplay porte et flood graph.
3. Garder `BP_Airlock` et les autres assemblages critiques comme prefabs editeur explicites.
4. Ajouter ou renforcer uniquement les interactions indispensables: ouvrir/fermer, reparer, activer pompe/airlock si deja prevu.
5. Utiliser les inspirations Subnautica surtout en presentation: prompts first-person, sons, FX de fuite, et retour clair au joueur.

Decision explicite: pas de port de systeme Barotrauma-style complet avant FP.

## Matrix systeme par systeme

| Systeme | Barotrauma | Space Engineers | Subnautica | Decision Sub3D |
|---|---|---|---|---|
| Items | Item XML + composants dynamiques | Blocks + object builders + component stack | TechType + prefabs + categories | Post-FP DataAssets + composants UE, pas de grosse classe item |
| Interaction | Wiring/signals/ports | Terminal actions + resource sinks | First-person tools simples | FP direct; post-FP event bus type, pas wires physiques d'abord |
| Flood | Graduel via gaps en 2D | Oxygen rooms instant merge | Pas de vraie flood sim interieure | Garder Sub3D, plus ambitieux |
| Portes | Controle `Gap.Open` | Airtight barrier par face/cell | Bulkheads/base pieces | Porte = barriere de graphe + acteur interactif |
| Pompes | Debit signe + power + condition | Air vent drain/fill oxygen | Pas equivalent fort | Pompe post-FP avec debit signe et energie |
| Reparation | Repairable condition/tool/skill/time | Welding/component stack | Repair tool + leak FX | `URepairableComponent` post-FP; breach repair simple FP |
| Power | Grille cablee complexe | Resource sink/source | Batteries/power cells/modules | Bus simple post-FP; pas wiring complet |
| Progression | Fabrication, skills, roles, items | Blueprints, assemblers, composants | Fragments, blueprints, modules | Unlock registry + upgrades modules post-FP |
| Animation | Profils data-driven | Outils astronaut/build | First-person tool feel | Profils equipement/mouvement, pas logique branchee partout |
| Assemblages | `ItemAssembly` | Blueprints de blocks | Bases/Cyclops prefabbed | Prefabs UE editor-assigned pour stations et modules |

## Items et architecture runtime

### Ce que Barotrauma apporte

Barotrauma confirme que la bonne unite n'est pas "une classe item geante". Un item est un conteneur d'etat et d'identite, et son comportement vient de composants:

- `Repairable`;
- `Door`;
- `Pump`;
- `ItemContainer`;
- `ConnectionPanel`;
- `LightComponent`;
- `Wearable`;
- `Holdable`;
- `Projectile`;
- `Sonar`;
- `Engine`;
- `OxygenGenerator`;
- `Reactor`.

L'interet pour Sub3D est structurel: separer l'identite, la presentation, les interactions et les effets gameplay.

### Recommandation Sub3D post-FP

Creer des definitions type DataAsset, pas un systeme XML clone:

- `USubItemDefinition`
  - `ItemId`
  - `DisplayName`
  - `Category`
  - `Tags`
  - `Icon`
  - `MassKg`
  - `VolumeLiters`
  - `AllowedSlots`
  - `UnlockId`
  - `SpawnActorClass`
  - `BuildCost`

- Composants runtime:
  - `URepairableComponent`
  - `UPowerConsumerComponent`
  - `UPowerProducerComponent`
  - `UFluidPumpComponent`
  - `UContainerComponent`
  - `UDamageVfxBindingComponent`
  - `UUpgradeSlotComponent`
  - `UUnlockRequirementComponent`

Regle importante: un item ne doit pas contenir directement toute la logique. Il doit porter des composants clairs qui s'enregistrent dans les systemes du sous-marin.

### A eviter

Eviter de refaire avant FP:

- le wiring Barotrauma complet;
- les ports string libres;
- une economie d'items de 1000 definitions;
- une serialization custom prematuree;
- un systeme de skills complet.

Ce serait puissant plus tard, mais trop grand pour la boucle actuelle.

## Interactions, signaux et wiring

### Barotrauma

Barotrauma est tres fort pour les interactions systemiques:

- portes activees par signal;
- pompes pilotees par `toggle`, `set_active`, `set_speed`;
- panels avec `signal_in`, `signal_out`, `state_out`, `condition_out`;
- circuits qui permettent des comportements emergents.

### Space Engineers

Space Engineers expose surtout des controles de terminal et des capacites de block:

- activer/desactiver;
- changer un mode;
- consommer/produire une ressource;
- mettre a jour a intervalle reduit;
- synchroniser l'etat reseau.

### Subnautica

Subnautica garde l'interaction simple:

- viser;
- voir un prompt;
- utiliser un outil;
- debloquer une recette;
- installer un module.

### Decision Sub3D

Pour FP:

- appels directs;
- interfaces simples;
- pas de wiring physique;
- feedback clair au joueur.

Post-FP:

- introduire un event bus type pour les systemes internes;
- eviter les ports string libres au debut;
- commencer par des signaux enumeres:
  - `Open`
  - `Close`
  - `Toggle`
  - `SetActive`
  - `SetTargetLevel`
  - `SetFlowRate`
  - `PowerAvailable`
  - `ConditionChanged`
  - `AlarmTriggered`

Le wiring visuel facon Barotrauma peut devenir un module de gameplay plus tard, mais il ne doit pas etre la base technique initiale.

## Reparation, condition et degats

### Barotrauma: le meilleur modele gameplay

Barotrauma donne le pattern le plus interessant:

- condition numerique;
- seuil sous lequel l'objet ne fonctionne plus;
- deterioration;
- reparation par outil;
- temps de reparation;
- influence du skill;
- sabotage/tinker possible;
- sorties de signal liees a la condition.

### Space Engineers: le meilleur modele construction

Space Engineers distingue clairement:

- construction d'un block;
- integrite actuelle;
- composants requis;
- composants manquants;
- welding;
- grinding;
- etat fonctionnel selon build ratio et integrity ratio.

Ce modele est utile pour construire ou reparer des modules physiques, mais pas forcement pour chaque fuite ou chaque breche du sous-marin.

### Subnautica: le meilleur modele visuel

Subnautica est utile pour:

- pre-placer des points de fuite;
- activer un nombre de leaks proportionnel aux degats;
- lier l'outil de reparation a un point proche;
- rendre le degat lisible tres vite.

### Recommandation Sub3D

FP:

- une breche a un etat simple: ouverte, en reparation, reparee.
- un outil ou une interaction repare.
- un FX montre la fuite.
- la simulation flood continue pendant la reparation.

Post-FP:

- `URepairableComponent`
  - `Condition01`
  - `RepairThreshold01`
  - `RequiredToolTag`
  - `RepairDurationSeconds`
  - `bBlocksFunctionBelowThreshold`
  - `ConditionToVfxSeverityCurve`
  - `RepairMaterialCost`
  - `LastDamageSource`

- `UDamageVfxBindingComponent`
  - mappe condition, breach flow, electric wetness ou pressure vers FX;
  - supporte points pre-places;
  - peut spawn des FX dynamiques quand le point exact est procedural.

La combinaison la plus forte est: Barotrauma pour la condition/reparation, Subnautica pour la presentation des leaks.

## Flood, oxygene, pression

### Barotrauma

Barotrauma simule des flux graduels entre hulls via gaps. C'est la reference la plus proche en intention:

- une porte ne deplace pas l'eau elle-meme;
- la porte ouvre ou ferme un passage;
- le gap gere le flux;
- l'eau et la pression evoluent progressivement.

### Space Engineers

Space Engineers est utile conceptuellement, mais incompatible directement:

- la topologie vient d'une grille voxel;
- les rooms sont detectees par BFS;
- les merges sont instantanes;
- l'oxygene est un scalar par room;
- les cellules pointent vers un room link.

Ce systeme confirme les patterns generaux, mais ne remplace pas la topologie explicite de Sub3D.

### Subnautica

Subnautica ne valide pas une simulation flood volumetrique. Il valide surtout:

- l'importance de la lisibilite;
- l'efficacite de FX de fuite bien places;
- la pression/depth comme budget de progression;
- le fait qu'un jeu commercial peut choisir une solution tres simple pour les leaks.

### Decision Sub3D

Garder le modele actuel:

- compartiments DA-authored;
- graphe de flood explicite;
- transfert graduel;
- hauteur d'eau par compartiment;
- cap mesh / heightfield;
- synchronisation visuelle aux frontieres.

Ameliorations post-FP:

- pompe a debit signe;
- energie requise pour les pompes;
- oxygen scalar par compartiment apres stabilisation de l'eau;
- `RoomLink` style indirection uniquement si des compartments lies dynamiquement deviennent necessaires;
- async detection uniquement si `OnBreachesUpdated` ou un recalcul topologique devient mesurablement lent.

## Power et ressources

### Ce qu'il faut prendre

Space Engineers est la meilleure reference pour un modele propre producteur/consommateur:

- resource source;
- resource sink;
- production;
- consommation;
- etat fonctionnel selon energie disponible.

Barotrauma est excellent mais beaucoup plus complexe avec wiring physique et reseaux de puissance.

Subnautica est bon pour le gameplay lisible:

- batteries;
- power cells;
- modules qui consomment;
- base reactors;
- chargeurs.

### Recommandation Sub3D post-FP

Commencer par un bus simple:

- `USubPowerBusComponent`
  - `GeneratedWatts`
  - `LoadWatts`
  - `StoredJoules`
  - `BrownoutState`
  - `PriorityGroups`

- Interfaces:
  - `ISubPowerConsumer`
  - `ISubPowerProducer`
  - `ISubPowerStorage`

Premiers consommateurs:

- pompe;
- sonar;
- eclairage;
- portes motorisees;
- airlock;
- repair station;
- shield ou emergency seal post-FP.

Premiers producteurs/storages:

- batterie principale;
- generateur;
- chargeur externe;
- module thermique post-FP inspire Subnautica.

## Progression et unlocks

### Subnautica: reference principale

Subnautica est le meilleur modele pour une progression first-person sous-marine:

- fragments scannes;
- blueprints debloques;
- categories de fabrication simples;
- modules de vehicule;
- profondeur max comme gate de progression;
- upgrades lisibles et desirables.

Categories observees dans les fichiers de langue:

- constructor;
- vehicle upgrades;
- map room upgrades;
- Cyclops;
- Cyclops upgrades;
- base pieces;
- base rooms;
- base walls;
- exterior modules;
- interior modules;
- hull plates.

Modules Cyclops utiles comme inspiration:

- depth module MK1/MK2/MK3;
- shield;
- sonar;
- docking bay repair;
- thermal reactor;
- engine efficiency;
- decoy tube upgrade;
- fire suppression.

### Barotrauma

Barotrauma apporte:

- fabrication/deconstruction plus systemique;
- roles et skills;
- qualite/condition d'items;
- interactions entre machines.

C'est riche, mais plus tardif pour Sub3D.

### Space Engineers

Space Engineers apporte:

- blueprint definitions;
- assemblers;
- queue de production;
- inventaires input/output;
- couts composants;
- composants manquants.

C'est utile si Sub3D veut une construction plus industrielle.

### Recommandation Sub3D

Post-FP, construire une progression en trois couches:

1. Unlocks
   - `USubUnlockRegistry`
   - scan, salvage, mission, research ou event narratif.

2. Recipes
   - `USubRecipeDefinition`
   - inputs;
   - output;
   - station requise;
   - temps;
   - unlock requis.

3. Upgrades
   - `USubUpgradeDefinition`
   - slot compatible;
   - effet;
   - niveau;
   - incompatibilites ou non-stacking.

Upgrades prioritaires pour Sub3D:

- `DepthRating`
- `PumpCapacity`
- `BatteryReserve`
- `SonarResolution`
- `NoiseSignature`
- `DoorActuationSpeed`
- `RepairSpeed`
- `BulkheadIntegrity`
- `EmergencySeal`
- `FireSuppression`
- `AirlockAutomation`
- `OxygenReserve`

La profondeur est le meilleur premier gate. Elle est lisible, thematique et deja validee par Subnautica.

## Assemblages et prefabs

Barotrauma `ItemAssembly` confirme un pattern important: certaines structures doivent etre authoring-level, pas generees par un systeme d'items abstrait.

Pour Sub3D, garder cette philosophie:

- `BP_Airlock`
- pompe de cale;
- locker de plongee;
- casier outil de reparation;
- console sonar;
- console upgrades;
- hatch/docking port;
- bulkhead door;
- emergency pump station.

Ces objets peuvent utiliser des definitions plus tard, mais leur placement et leur wiring editorial doivent rester explicites pendant FP.

## Animation et feeling first-person

### Barotrauma

Barotrauma a un systeme 2D/ragdoll non portable directement, mais sa separation par profils est bonne:

- walk;
- run;
- swim slow;
- swim fast;
- crouch;
- diving suit;
- exosuit.

Le pattern utile est: l'equipement modifie les capacites de mouvement via data.

### Subnautica

Subnautica est la meilleure reference pour:

- outil en main;
- feedback immediat;
- scan/reparation;
- equipement qui change la mobilite ou la profondeur;
- vehicule first-person lisible.

### Space Engineers

Space Engineers est utile pour:

- outils de construction/reparation;
- etat de block sous le curseur;
- build integrity visible;
- rythme welding/grinding.

### Recommandation Sub3D

Post-FP:

- `UEquipmentCapabilityProfile`
  - `WalkSpeedMultiplier`
  - `SwimSpeedMultiplier`
  - `InteractionSpeedMultiplier`
  - `OxygenConsumptionMultiplier`
  - `PressureResistanceBonus`
  - `NoiseMultiplier`

Ne pas disperser ces effets dans chaque item. Le joueur equipe un outil ou une combinaison, et le profil modifie les systemes concernes.

## Classement des inspirations les plus utiles

1. Barotrauma `Repairable` + Subnautica leak FX
   - Valeur: tres haute.
   - Risque: faible.
   - Usage: rendre les degats reparables, visibles et systemiques.

2. Subnautica Cyclops upgrades
   - Valeur: tres haute.
   - Risque: faible.
   - Usage: progression du sous-marin, profondeur, defense, sonar, energie.

3. Barotrauma item components
   - Valeur: haute.
   - Risque: moyen si copie trop large.
   - Usage: architecture post-FP DataAssets + composants UE.

4. Barotrauma ItemAssembly
   - Valeur: haute.
   - Risque: faible.
   - Usage: prefabs editeur pour stations et modules.

5. Space Engineers component stack
   - Valeur: moyenne a haute.
   - Risque: moyen.
   - Usage: construction/reparation de modules, pas breches simples FP.

6. Space Engineers resource sink/source
   - Valeur: moyenne a haute.
   - Risque: faible si simplifie.
   - Usage: bus energie post-FP.

7. Space Engineers `MyOxygenRoomLink`
   - Valeur: niche.
   - Risque: faible si isole.
   - Usage: uniquement si linked compartments runtime.

8. Barotrauma wiring physique
   - Valeur: haute a long terme.
   - Risque: tres haut avant FP.
   - Usage: differer.

9. Space Engineers BFS room detection
   - Valeur: faible pour Sub3D.
   - Risque: haut si transpose.
   - Usage: rejeter pour notre architecture actuelle.

10. Subnautica no-flood internal model
    - Valeur: faible comme simulation.
    - Risque: haut si on reduit l'ambition.
    - Usage: garder seulement les FX et la lisibilite.

## Architecture recommandee post-FP

### Definitions

- `USubItemDefinition`
- `USubToolDefinition`
- `USubUpgradeDefinition`
- `USubRecipeDefinition`
- `USubUnlockDefinition`
- `USubStationDefinition`

### Composants runtime

- `URepairableComponent`
- `UDamageVfxBindingComponent`
- `UPowerConsumerComponent`
- `UPowerProducerComponent`
- `UPowerStorageComponent`
- `UFluidPumpComponent`
- `UUpgradeSlotComponent`
- `UContainerComponent`
- `UToolUseComponent`

### Services du sous-marin

- `USubFloodComponent` deja central pour eau.
- `USubPowerBusComponent` post-FP.
- `USubUnlockRegistryComponent` post-FP.
- `USubUpgradeManagerComponent` post-FP.
- `USubInventoryOrCargoComponent` post-FP.

### Interfaces utiles

- `ISubInteractable`
- `ISubRepairTarget`
- `ISubPowerConsumer`
- `ISubPowerProducer`
- `ISubFloodAffectingActor`
- `ISubUpgradeProvider`

## Roadmap proposee

### Phase FP actuelle

Objectif: fermer la boucle jouable sans systeme generique.

- Garder les portes et l'airlock explicites.
- Garder la reparation simple.
- Garder le flood comme systeme central.
- Ajouter seulement les feedbacks necessaires: prompt, audio, FX, et etats lisibles.
- Ne pas introduire inventory/craft/wiring generique.

### Post-FP 1: degats et reparation

- Ajouter `URepairableComponent`.
- Ajouter `UDamageVfxBindingComponent`.
- Ajouter outil de reparation first-person.
- Gerer temps de reparation, condition et retour visuel.
- Brancher breches, portes critiques, pompes, panneaux.

### Post-FP 2: upgrades de sous-marin

- Ajouter slots d'upgrade.
- Ajouter upgrades depth, pump, sonar, battery, repair speed.
- Ajouter non-stacking pour certains modules type depth.
- Utiliser progression Subnautica-style: scan/salvage/research -> unlock.

### Post-FP 3: energie simple

- Ajouter bus energie.
- Ajouter producteurs, stockage, consommateurs.
- Pompes et sonar dependent de l'energie.
- Brownout simple par priorite.

### Post-FP 4: items et craft leger

- Ajouter catalogue DataAsset.
- Ajouter recettes.
- Ajouter stations.
- Ajouter inventaire/cargo minimal.
- Ne pas encore faire wiring physique.

### Post-FP 5: systemique avance

- Signaux types.
- Assemblages complexes.
- Linked compartments runtime si besoin.
- Oxygen scalar par compartment.
- Async recalcul uniquement si profiling le justifie.

## Points d'attention importants

### 1. Ne pas confondre richesse et priorite

Barotrauma est riche parce que son architecture a ete construite autour d'items systemiques. Sub3D est actuellement construit autour du sous-marin comme frame mobile, du traversal, et du flood. Ajouter trop tot un systeme d'items general risque de deplacer le centre de gravite du projet.

### 2. Subnautica est excellent en perception, pas en flood

Subnautica donne un tres bon exemple de lisibilite joueur:

- categories claires;
- modules desirables;
- messages vocaux/systeme;
- feedbacks de degats;
- outil de reparation comprehensible.

Mais il ne faut pas imiter son absence de flood sim pour Sub3D. Le payoff unique de Sub3D est justement la simulation volumetrique lisible.

### 3. Space Engineers est bon pour construction/power, pas pour compartments

Le BFS voxel et les airtight faces sont mauvais fit pour Sub3D. En revanche:

- component stack;
- resource sink/source;
- assembler queue;
- room link indirection;

sont de vrais patterns reutilisables conceptuellement.

### 4. Le wiring Barotrauma est a differer

Un systeme de wiring peut devenir fantastique pour Sub3D, surtout en coop, mais il cree vite:

- beaucoup d'UI;
- beaucoup de replication;
- beaucoup de debugging;
- beaucoup d'etats caches;
- beaucoup de contenu a produire.

Le bon compromis post-FP est un signal bus type et limite.

### 5. Les prefabs editeur sont un avantage, pas une faiblesse

Pour Sub3D, des assets explicites comme `BP_Airlock` sont preferables a un systeme qui auto-construit tout. Les jeux references utilisent aussi beaucoup d'authoring manuel pour les elements critiques.

### 6. Les snippets doivent etre decomposes, pas portes

Le bon usage des sources externes est:

- identifier l'etat minimal;
- identifier les transitions;
- identifier les invariants;
- reimplementer en style Unreal/Sub3D;
- verifier par tests ou PIE.

Eviter les ports ligne-a-ligne. Meme quand un snippet est lisible, son contexte moteur, reseau, serialization et contenu ne correspond pas a Sub3D.

## Synthese finale

La strategie la plus forte est:

- garder l'ambition eau de Sub3D;
- prendre Barotrauma comme modele d'architecture systemique post-FP;
- prendre Subnautica comme modele de progression, upgrades et feeling first-person;
- prendre Space Engineers comme reference pour construction, integrite, power et production;
- ne pas importer leurs systemes lourds avant que le First Playable soit ferme.

Sub3D peut etre differenciant si le projet assume son coeur: un sous-marin 3D avec compartments authorés, flood visible, frontieres synchronisees, portes/pompes/reparations lisibles et progression par modules. Aucun des trois jeux inspectes ne combine exactement ce noyau.
