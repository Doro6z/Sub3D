# Handoff - Voxel Abyss Asset Pack

Date: 2026-04-23

Autorité plan consultée: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

## Scope

Créer un nouveau pack Blender Python entièrement voxel, inspiré des scripts characters existants, sans modifier les scripts Blender actuels.

Contraintes fermées:

- voxel principal aligné sur le crew: `2 cm`
- voxel de détail aligné sur les mains: `0.67 cm`
- au moins `10` scripts nouveaux
- package global entre `50` et `100` meshes
- ambiance combinée:
  - `The Abyss`
  - `deep sea`
  - `oceanic`
  - `Barotrauma`
  - `lovecraft`

Résultat livré:

- `12` scripts générateurs nouveaux
- `94` meshes définis et générés
- `1` script batch `build_all.py`
- `1` manifeste `pack_manifest.py`
- `1` cœur commun `core.py`

## Recherche synthétique

### Références de ton

- `The Abyss` a servi de direction pour l'opposition entre structures industrielles pressurisées et immensité noire sous-marine. Référence: [Britannica - The Abyss](https://www.britannica.com/topic/Abyss) et [IMDb plot summary](https://www.imdb.com/title/tt0096754/plotsummary/)
- `Barotrauma` a servi de direction pour les outposts, wrecks, ruines, progression dans des environnements dangereux, et coexistence entre sous-marins, cavernes et créatures hostiles. Référence officielle: [Barotrauma Features](https://barotraumagame.com/features/)
- Le pôle `Lovecraft` a été traduit en silhouettes fausses, disproportions, appendices anormaux et masses impossibles plutôt qu'en citation directe d'une créature existante. Référence de cadre: [Britannica - The Call of Cthulhu](https://www.britannica.com/topic/The-Call-of-Cthulhu)

### Références biologiques et environnementales

- Vie de cheminées hydrothermales: vers tubicoles, crevettes étranges, faune de source chaude, donc famille `vent_fields.py` et une partie de `abyssal_flora.py`. Références:
  - [Smithsonian Ocean - Hydrothermal Vent Creatures](https://ocean.si.edu/ocean-life/invertebrates/hydrothermal-vent-creatures)
  - [Smithsonian Ocean - Tubeworms on a Hydrothermal Vent](https://ocean.si.edu/ocean-life/invertebrates/tubeworms-hydrothermal-vent)
- Bioluminescence benthique et usages de lumière dans les profondeurs: base du langage lumineux cyan-vert réparti sur mobs, flore et vents. Référence:
  - [NOAA Ocean Exploration - Bioluminescence](https://oceanexplorer.noaa.gov/education/bioluminescence/)
- Coraux profonds, coraux bambou, formes en arbre, éventail, colonne, plume: base des familles `abyssal_flora.py` et du dressage des cavernes. Référence:
  - [NOAA Fisheries - Deep-Sea Coral Habitat](https://www.fisheries.noaa.gov/national/habitat-conservation/deep-sea-coral-habitat)
  - [NOAA Ocean Exploration - Bamboo Coral](https://oceanexplorer.noaa.gov/multimedia/okeanos-explorations-ex2104-dives-dive01-media-bamboo-coral/)

### Traduction concrète dans le pack

- `The Abyss` -> tours pressurisées, docks, passerelles, modules de sous-marin et silhouettes verticales éclairées.
- `Barotrauma` -> outposts fonctionnels, structures à clamps, flood gates, service spines, modules de docking et débris industriels.
- `Lovecraft` -> mobs non symétriques, fausses anatomies, grandes bouches, appendices fins, masses trop longues ou trop hautes.
- `Deep sea real` -> vents, tubeworms, bamboo corals, sea pens, mats chimiques, fluorescence locale.

## Architecture du pack

Dossier livré:

- `Scripts/Blender/voxel_abyss_pack/`

Fichiers de structure:

- `core.py`
  - helpers voxels
  - génération des faces exposées
  - matériaux
  - scène Blender
  - spawn d'assets multi-couches
- `pack_manifest.py`
  - manifeste des `12` scripts
  - liste explicite des `94` meshes
- `build_all.py`
  - batch global
  - pose les familles en grille de revue
- `README.md`
  - usage rapide

## Liste des scripts livrés

### 1. `mobs_scouts.py`

Assets: `8`

- `SM_VX_SiltLeech`
- `SM_VX_PincherMite`
- `SM_VX_GlimmerSkate`
- `SM_VX_RiftEel`
- `SM_VX_BlindCrawler`
- `SM_VX_NeedleLamprey`
- `SM_VX_SnoutCrab`
- `SM_VX_PulseMinnow`

Rôle:

- petits mobs rapides
- silhouettes de reconnaissance
- détails fins sur moustaches, pattes, mandibules

### 2. `mobs_brutes.py`

Assets: `8`

- `SM_VX_MawDrifter`
- `SM_VX_BarnacleBoar`
- `SM_VX_ShellbackRam`
- `SM_VX_AnchorJaw`
- `SM_VX_SlugBrute`
- `SM_VX_VentStalker`
- `SM_VX_MudGorger`
- `SM_VX_ReefCrusher`

Rôle:

- mobs moyens à gros
- masses blindées
- carapaces, charges, mandibules et pattes plus lourdes

### 3. `mobs_giants.py`

Assets: `6`

- `SM_VX_GigaVer`
- `SM_VX_TrenchTitan`
- `SM_VX_ChoirWhale`
- `SM_VX_RookLeviathan`
- `SM_VX_SpineCathedral`
- `SM_VX_MirrorKraken`

Rôle:

- grands monstres de setpiece
- un `GigaVer` explicite
- silhouettes de boss ou de très grande rencontre

### 4. `voxel_craniata.py`

Assets: `6`

- `SM_VX_Craniata_Blockout`
- `SM_VX_Craniata_Derelict`
- `SM_VX_Craniata_BowModule`
- `SM_VX_Craniata_Airlock`
- `SM_VX_Craniata_TurretNest`
- `SM_VX_Craniata_PropCluster`

Rôle:

- passe `Craniata` réinterprétée en voxel
- version intacte
- version brisée
- modules séparés exploitables dans l'environnement

### 5. `outposts.py`

Assets: `8`

- `SM_VX_Outpost_Tower`
- `SM_VX_Outpost_DockClamp`
- `SM_VX_Outpost_SonarHub`
- `SM_VX_Outpost_FloodGate`
- `SM_VX_Outpost_ServiceSpine`
- `SM_VX_Outpost_PressureDome`
- `SM_VX_Outpost_PipeBridge`
- `SM_VX_Outpost_MiningNest`

Rôle:

- structures d'avant-postes
- hubs sous-marins et docking
- pièces modulaires de base pour un kit environnemental

### 6. `cave_columns.py`

Assets: `8`

- `SM_VX_CaveArch_Tight`
- `SM_VX_CaveArch_Wide`
- `SM_VX_CaveArch_SplitPillar`
- `SM_VX_CaveArch_ToothGate`
- `SM_VX_CaveArch_RibTunnel`
- `SM_VX_CaveArch_Buttress`
- `SM_VX_CaveArch_ShelfSpine`
- `SM_VX_CaveArch_CathedralColumn`

Rôle:

- arches de cavernes
- colonnes et portes rocheuses
- grands volumes de lecture pour habiller la navigation

### 7. `stalactites.py`

Assets: `10`

- `SM_VX_Stalactite_LongA`
- `SM_VX_Stalactite_LongB`
- `SM_VX_Stalactite_ClusterA`
- `SM_VX_Stalactite_ClusterB`
- `SM_VX_Stalactite_BrokenA`
- `SM_VX_Stalactite_BrokenB`
- `SM_VX_Stalagmite_A`
- `SM_VX_Stalagmite_B`
- `SM_VX_RoofFang_A`
- `SM_VX_RoofFang_B`

Rôle:

- modules géants de plafond et de sol
- longues pointes
- versions cassées

### 8. `metal_intrusions.py`

Assets: `6`

- `SM_VX_MetalLance_Straight`
- `SM_VX_MetalLance_Twisted`
- `SM_VX_MetalLance_Crossbrace`
- `SM_VX_MetalLance_BuriedTruss`
- `SM_VX_MetalLance_RoofSpears`
- `SM_VX_MetalLance_JaggedPile`

Rôle:

- grosses barres métalliques
- structures qui percent le haut des cavernes
- masses industrielles agressives

### 9. `rock_formations.py`

Assets: `8`

- `SM_VX_Rock_BoulderA`
- `SM_VX_Rock_BoulderB`
- `SM_VX_Rock_ShelfA`
- `SM_VX_Rock_ShelfB`
- `SM_VX_Rock_NeedleA`
- `SM_VX_Rock_NeedleB`
- `SM_VX_Rock_WallChunkA`
- `SM_VX_Rock_FumaroleA`

Rôle:

- blocs rocheux
- étagères
- aiguilles
- chunks de paroi

### 10. `abyssal_flora.py`

Assets: `10`

- `SM_VX_Flora_TubewormPatch`
- `SM_VX_Flora_FanCoral`
- `SM_VX_Flora_BambooCoral`
- `SM_VX_Flora_SeaPen`
- `SM_VX_Flora_VentReeds`
- `SM_VX_Flora_SporePalm`
- `SM_VX_Flora_BulbAnemone`
- `SM_VX_Flora_LanternKelp`
- `SM_VX_Flora_BoneMoss`
- `SM_VX_Flora_BlackCoralShrub`

Rôle:

- flore abyssale et océanique
- formes verticales
- masses de fond
- lueurs organiques

### 11. `seafloor_clutter.py`

Assets: `8`

- `SM_VX_WreckPlate`
- `SM_VX_ChainNest`
- `SM_VX_AnchorBone`
- `SM_VX_PipeDebris`
- `SM_VX_EggCluster`
- `SM_VX_ShellPile`
- `SM_VX_CrateRemains`
- `SM_VX_RustedPanelGarden`

Rôle:

- clutter de sol
- épaves
- amas narratifs
- remplissage procédural secondaire

### 12. `vent_fields.py`

Assets: `8`

- `SM_VX_Vent_BlackSmoker`
- `SM_VX_Vent_WhiteSmoker`
- `SM_VX_Vent_ColdSeep`
- `SM_VX_Vent_SulfurMound`
- `SM_VX_Vent_BubbleSpire`
- `SM_VX_Vent_ChimneyBroken`
- `SM_VX_Vent_ChimneyRing`
- `SM_VX_Vent_ChemMat`

Rôle:

- poches géothermiques
- points lumineux
- centres d'intérêt pour biomes de cavernes

## Usage Blender

### Générer toute la bibliothèque

Script:

- `Scripts/Blender/voxel_abyss_pack/build_all.py`

Effet:

- réinitialise la scène
- pose les `12` familles dans une grille de revue
- permet un balayage rapide du pack

### Générer une seule famille

Ouvrir le script voulu puis `Alt+P`.

Chaque script:

- configure la scène en centimètres
- génère sa collection Blender dédiée
- assigne ses matériaux
- se centre sur sa famille

## Logique technique reprise des scripts character

- voxel principal `2 cm`
- détails via seconde grille en `0.67 cm`
- mesh construit uniquement à partir des faces exposées
- matériaux multi-slots
- rendu `shade_flat`

Cette logique est exactement celle recherchée:

- mêmes tailles de voxel que les characters
- détails fins à l'échelle des mains
- aucune conversion sculpt ou remesh intermédiaire

## Vérification réalisée

Vérifié localement:

- dossier et fichiers créés
- manifeste cohérent
- `12` scripts principaux présents
- total manifeste = `94` meshes
- scripts conçus pour être importables par `build_all.py`

Reste à vérifier dans Blender:

- validation visuelle des silhouettes
- validation des proportions dans ton viewport
- ajustement éventuel des espacements de revue
- export FBX selon ton pipeline de destination

## Non inclus volontairement

- export FBX automatique
- import UE automatique
- batch de collisions UE
- variations aléatoires runtime

Ce handoff ferme uniquement la partie:

- recherche
- définition du package
- écriture des scripts Blender
- organisation du pack

## Fichiers livrés

- `Scripts/Blender/voxel_abyss_pack/__init__.py`
- `Scripts/Blender/voxel_abyss_pack/core.py`
- `Scripts/Blender/voxel_abyss_pack/pack_manifest.py`
- `Scripts/Blender/voxel_abyss_pack/build_all.py`
- `Scripts/Blender/voxel_abyss_pack/mobs_scouts.py`
- `Scripts/Blender/voxel_abyss_pack/mobs_brutes.py`
- `Scripts/Blender/voxel_abyss_pack/mobs_giants.py`
- `Scripts/Blender/voxel_abyss_pack/voxel_craniata.py`
- `Scripts/Blender/voxel_abyss_pack/outposts.py`
- `Scripts/Blender/voxel_abyss_pack/cave_columns.py`
- `Scripts/Blender/voxel_abyss_pack/stalactites.py`
- `Scripts/Blender/voxel_abyss_pack/metal_intrusions.py`
- `Scripts/Blender/voxel_abyss_pack/rock_formations.py`
- `Scripts/Blender/voxel_abyss_pack/abyssal_flora.py`
- `Scripts/Blender/voxel_abyss_pack/seafloor_clutter.py`
- `Scripts/Blender/voxel_abyss_pack/vent_fields.py`
- `Scripts/Blender/voxel_abyss_pack/README.md`
- `reports/handoffs/2026-04-23_voxel_abyss_asset_pack_handoff.md`
