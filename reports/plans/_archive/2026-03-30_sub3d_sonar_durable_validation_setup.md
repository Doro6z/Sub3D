# Sub3D - Sonar Durable Validation Setup

Date: 2026-03-30
Status: Working implementation and gameplay validation handoff
Scope: durable Sonar V2 setup, editor wiring, validation map contract, gameplay validation protocol

## 1. Purpose

Ce document definit le setup durable a mettre en place pour travailler proprement sur le sonar et valider le gameplay.

Ce n'est pas un guide de demo rapide.
Ce n'est pas un guide de polish UI.

Le but est de verrouiller:
- une configuration runtime explicite
- une map de validation canonique
- des assets minimaux durables
- un wiring Blueprint stable
- un protocole PIE reproductible

La cible est la validation du gameplay sonar dans la boucle first playable:
- navigation sans vision externe fiable
- lecture topologique exploitable
- ping actif utile mais couteux
- tracks passifs exploitables
- comportement stable pendant la traverse

## 2. Canon and source hierarchy

Ordre de reference canonique:
1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
2. `reports/plans/2026-03-29_sub3d_sonar_v2_spec.md`
3. `reports/plans/2026-03-29_sub3d_sonar_v2_delta_from_v1.md`
4. `reports/plans/2026-03-29_sub3d_sonar_v2_editor_gameplay_handoff.md`
5. le code runtime dans `Source/Sub3D/Submarine/*`

Contraintes verrouillees:
- `ATraversalRouteActor` reste l'autorite route
- le sonar V2 ne devient pas une seconde autorite monde
- la topologie coarse seed vient de la route
- la logique sonar ne depend pas du mesh marching-cubes comme source de verite gameplay
- la validation se fait en PIE avec le vrai sous-marin et le vrai helm

## 3. Runtime truth already in code

Le code expose deja les pieces runtime suivantes:

- `USubSonarComponent`
  - ping actif
  - hold ping
  - persistance des points
  - cooldown
  - replication des `SonarPoints`

- `USubSonarSystemComponent`
  - modes sonar
  - passive sweep
  - terrain sweep
  - self-noise
  - coarse seed depuis la route
  - tracks replices
  - topo fenetree replicatee

- `USonarContactTrackerComponent`
  - maintenance des tracks

- `USonarNoiseEmitterComponent`
  - signatures passives des acteurs de test

- `USonarAcousticVolumeComponent`
  - modificateurs locaux de bruit/clutter/detection

- `USubSonarDisplayWidget`
  - affichage topo fixe
  - pulse
  - tracks
  - overlays UI

Classes data assets disponibles:
- `USonarSystemConfigData`
- `USonarSignatureData`
- `USonarEnvironmentProfileData`
- `USonarUpgradeData`

## 4. Validation philosophy

Le sonar n'est valide que si:
- il aide reellement le pilotage
- il force une lecture imparfaite mais exploitable
- il fonctionne dans un tunnel long, pas seulement dans un petit test statique
- il reste lisible pendant la manoeuvre
- il ne demande pas un cheat visuel externe

Le sonar n'est pas valide si:
- il est joli mais ne guide pas
- la carte bouge de facon instable
- le ping efface continuellement la lecture
- les tracks ne servent pas a la decision
- le self-noise n'a aucun effet perceptible

## 5. Content structure to create and keep

Structure recommandee a figer dans le projet:

- `/Game/Sub3D/Sonar/Data`
- `/Game/Sub3D/Sonar/Profiles`
- `/Game/Sub3D/Sonar/UI`
- `/Game/Sub3D/Sonar/Blueprints`
- `/Game/Sub3D/Sonar/Materials`
- `/Game/Sub3D/Sonar/Test`

Regle:
- ne pas disperser les assets sonar dans des dossiers Proto multiples
- garder un seul chemin stable pour la prod et la validation

## 6. Required durable data assets

Les assets suivants doivent exister avant toute validation gameplay serieuse.

### 6.1 System config

Creer:
- `DA_SonarSystemConfig_Default`

Type:
- `USonarSystemConfigData`

Chemin:
- `/Game/Sub3D/Sonar/Data/DA_SonarSystemConfig_Default`

Valeurs initiales recommandees:
- `RangePresetsCm = [9000, 15000, 22000]`
- `PassiveSweepIntervalS = 0.30`
- `PassiveMinDetectionScore = 0.12`
- `PassiveFocusHalfAngleDeg = 25`
- `PassiveTerrainWeight = 1.35`
- `TrackMaintenanceIntervalS = 0.15`
- `TrackDecayPerSecond = 0.08`
- `LostThresholdS = 2.5`
- `LostRetentionS = 6.0`
- `SelfNoiseUpdateIntervalS = 0.15`
- `SelfNoiseSpeedNormCmS = 600`
- `TopologyCellSizeCm = 500`
- `TopologyHalfWindowCells = 44`
- `MaxReplicatedTopoCells = 1500`
- `TopologyCellLifetimeS = 45`
- `TopologySeedPointBudget = 1600`

But de ces valeurs:
- assez lisible pour du gameplay
- assez stable pour la validation
- pas encore tunees pour le produit final

### 6.2 Contact signatures

Creer minimum:
- `DA_SonarSignature_Threat`
- `DA_SonarSignature_Neutral`
- `DA_SonarSignature_Structure`

Type:
- `USonarSignatureData`

Chemin:
- `/Game/Sub3D/Sonar/Profiles`

Preset de depart recommande:

`DA_SonarSignature_Threat`
- `ContactClass = MobileThreat`
- `BaseNoiseStrength = 0.90`
- `ActiveReflectivity = 0.80`
- `SignalStability = 0.50`

`DA_SonarSignature_Neutral`
- `ContactClass = MobileNeutral`
- `BaseNoiseStrength = 0.45`
- `ActiveReflectivity = 0.45`
- `SignalStability = 0.70`

`DA_SonarSignature_Structure`
- `ContactClass = StructureActive`
- `BaseNoiseStrength = 0.25`
- `ActiveReflectivity = 0.90`
- `SignalStability = 0.95`

### 6.3 Environment profile

Creer:
- `DA_SonarEnv_Default`

Type:
- `USonarEnvironmentProfileData`

Chemin:
- `/Game/Sub3D/Sonar/Profiles/DA_SonarEnv_Default`

Valeurs initiales:
- `AmbientNoiseBias = 0.0`
- `ClutterBias = 0.0`
- `PassiveDetectionModifier = 1.0`
- `ActivePingDistortion = 0.0`

### 6.4 Upgrade profile

Creer:
- `DA_SonarUpgrade_Mk1`

Type:
- `USonarUpgradeData`

Chemin:
- `/Game/Sub3D/Sonar/Profiles/DA_SonarUpgrade_Mk1`

Valeurs initiales:
- `RangeMultiplier = 1.0`
- `ClassificationMultiplier = 1.0`
- `SelfNoiseResistance = 1.0`
- `ActivePingCooldownMultiplier = 1.0`

## 7. Validation map contract

Utiliser une map de validation canonique unique:
- `L_FP01_RunShell`

Cette map doit servir a la fois:
- au sonar
- au helm
- a la traverse first playable

Elle ne doit pas devenir une scene decorative.

### 7.1 Required actors

La map doit contenir:
- `ATraversalRouteActor`
- `BP_Submarine_Compiler`
- `BP_HelmStation`
- au moins 2 `SubRunPhaseVolume`
- au moins 1 `SubBreachTriggerVolume`

Pour la validation sonar durable, ajouter:
- 1 groupe de cibles structurelles
- 1 groupe de cibles mobiles ou pseudo-mobiles
- 1 volume acoustique perturbe
- 1 section de route calme

### 7.2 Required spatial situations

La map doit permettre de tester au minimum:
- corridor droit
- virage doux
- virage plus serre
- zone plus large type chambre ou hub
- occlusion naturelle par la roche
- contact apres virage

### 7.3 World readability

Le niveau peut rester spartiate.

Autorise:
- fog/post-process minimal
- lights interieures du sous-marin
- ambience lisible

Interdit pour la validation sonar:
- decoration lourde qui parasite la lecture
- trop d'elements non gameplay
- effets qui rendent impossible l'analyse d'un bug sonar

## 8. BP_Submarine_Compiler wiring

Ouvrir `BP_Submarine_Compiler`.

Verifier la presence des composants:
- `Sonar` (`USubSonarComponent`)
- `SonarSystem` (`USubSonarSystemComponent`)

### 8.1 Sonar component settings

Dans `Sonar`:
- `bAccumulatePointsAcrossPings = true`
- `ContinuousPingIntervalS = 0.12`
- `PingCooldownS` a garder lisible pour le gameplay
- `PingMaxRangeCm` coherent avec les range presets

Valeurs de depart sures:
- `PingRayCountHorizontal = 24`
- `PingRayCountVertical = 12`
- `PingMaxRangeCm = 15000`
- `PropagationSpeedCmS = 3000`
- `PointPeakDurationS = 2.0`
- `PointFadeDurationS = 7.0`
- `PingCooldownS = 1.5`
- `PingHalfAngleDeg = 85`
- `MinAcceptedHitDistanceCm = 120`
- `PointRefreshRadiusCm = 140`
- `MaxRetainedPoints = 2400`

### 8.2 Sonar system settings

Dans `SonarSystem`:
- assigner `SystemConfig = DA_SonarSystemConfig_Default`
- `bEnablePassiveSweep = true`
- `bEnableRouteCoarseSeed = true`
- `bEnableDebugLogs = false` par defaut

En phase de validation instrumentee:
- `bEnableDebugLogs = true`

### 8.3 Runtime expectation

En PIE, on doit pouvoir verifier:
- seed topo depuis la route
- enrichissement par ping actif
- enrichissement passif via les noise emitters
- tracks presents meme sans nouveau ping actif

## 9. Player controller and input contract

Le controller doit garder une API stable.

### 9.1 Mandatory actions

Dans les Input Actions / IMC Helm:
- `IA_SonarPing`
- `IA_SonarModePassive`
- `IA_SonarModeFocus`
- `IA_SonarModeTerrain`
- `IA_SonarRangeNext`
- `IA_SonarRangePrev`

Si le hold ping est supporte par la meme action:
- `Pressed` -> `SetSonarPingHeld(true)`
- `Released` -> `SetSonarPingHeld(false)`

Sinon:
- garder un mapping distinct hold si necessaire

### 9.2 Mandatory routes

Dans `PC_SubPlayerController`:
- `IA_SonarPing` -> `TriggerSonarPing()`
- hold -> `SetSonarPingHeld(bool)`
- changement mode -> `SetSonarMode(...)`
- changement range -> `SetSonarRangePreset(...)`
- focus bearing si expose -> `SetSonarFocusBearing(...)`

Regle:
- ne pas contourner le controller en branchant toute la logique directement dans le widget

## 10. Helm widget contract

Le widget helm doit contenir un widget sonar explicite.

### 10.1 Required widget

Dans `WBP_SubHelm`:
- ajouter ou garder un widget derive de `USubSonarDisplayWidget`
- nom recommande: `SonarDisplay`

### 10.2 Binding rule

Le bind doit se faire via le flow C++ deja en place.

Attendu:
- le helm trouve `Sonar`
- le helm trouve `SonarSystem`
- le `SonarDisplay` est initialise avec les deux sources

### 10.3 Production rule

Pour le setup durable:
- `SonarDisplay` doit exister explicitement dans le WidgetTree
- eviter un comportement base sur auto-creation implicite

## 11. Sonar display contract

La vue sonar de validation doit rester une vue gameplay, pas un prototype de rendu libre.

### 11.1 Mandatory layers

La display doit montrer:
- la grille / anneaux
- la topo fixe monde/route
- le sous-marin au centre
- la direction du sous-marin
- le pulse actif
- les tracks
- les overlays CRT si voulus

### 11.2 Mandatory behavior

La display doit respecter:
- la topologie ne pivote pas avec le sous-marin
- l'icone du sous-marin pivote
- les pings n'effacent pas brutalement l'historique utile
- un re-ping refresh les zones deja vues
- le mur apres virage doit rester lisible comme geometrie utile, pas comme bruit total

### 11.3 Durable tuning baseline

Recommandations UI:
- `bDrawTopologyWireframe = true`
- `bDrawTracks = true`
- `bDrawSweepPulse = true`
- `RingCount = 4`
- `CenterTextureScale = 0.30` a `0.40`

## 12. Gameplay test actors to place

Il faut des acteurs de validation simples et durables.

### 12.1 Structure target

Placer un acteur de type structure avec:
- `SonarNoiseEmitterComponent`
- signature `DA_SonarSignature_Structure`

Position recommandee:
- visible en ligne directe dans un premier corridor

But:
- valider track structure stable
- valider echo actif fort

### 12.2 Neutral target

Placer un acteur neutral avec:
- `SonarNoiseEmitterComponent`
- signature `DA_SonarSignature_Neutral`

Position recommandee:
- a moyenne distance
- de preference sur le cote d'un corridor plus large

But:
- valider passif flou
- valider track non hostile

### 12.3 Threat target

Placer un acteur threat avec:
- `SonarNoiseEmitterComponent`
- signature `DA_SonarSignature_Threat`

Position recommandee:
- apres un virage ou semi-masque

But:
- valider utilite du ping
- valider difference passif / actif

## 13. Acoustic volume setup

Placer au moins un volume acoustique significatif.

### 13.1 Noisy cave volume

Ajouter un actor volume avec `SonarAcousticVolumeComponent`.

Valeurs recommandees:
- `AmbientNoiseBias = 0.35`
- `ClutterBias = 0.30`
- `PassiveDetectionModifier = 0.85`
- `ActivePingDistortion = 0.10`

But:
- degrader legerement la lisibilite passive
- rendre le joueur conscient du contexte acoustique

### 13.2 Quiet corridor volume

Optionnel mais utile:
- `AmbientNoiseBias = -0.05`
- `ClutterBias = -0.10`
- `PassiveDetectionModifier = 1.10`
- `ActivePingDistortion = 0.0`

But:
- montrer qu'un espace calme se lit mieux

## 14. Mandatory editor setup sequence

Ordre recommande pour un setup propre.

1. Verifier que `L_FP01_RunShell` s'ouvre sans rebuild parasite.
2. Verifier que `TraversalRouteActor` est present et route valide.
3. Ouvrir `BP_Submarine_Compiler`.
4. Assigner `DA_SonarSystemConfig_Default` a `SonarSystem`.
5. Verifier les valeurs runtime du composant `Sonar`.
6. Ouvrir `WBP_SubHelm`.
7. Verifier que `SonarDisplay` est present.
8. Verifier les bindings controller -> sonar.
9. Placer les trois cibles de test.
10. Placer au moins un volume acoustique.
11. Sauvegarder.
12. Lancer PIE.

## 15. PIE validation protocol

La validation gameplay doit se faire dans cet ordre.

### 15.1 Gate 1 - Boot validity

Verifier:
- spawn sub OK
- prise helm OK
- widget helm visible
- sonar display bind OK

Critere de rejet:
- sonar absent
- widget non bind
- input ping non route

### 15.2 Gate 2 - Active ping baseline

Depuis le helm:
- lancer un ping
- observer la topo
- relancer un second ping

Verifier:
- le ping est accepte
- les points apparaissent progressivement
- la topo seed route reste lisible
- le second ping ne purge pas brutalement la lecture utile

Critere de rejet:
- reset total de lecture
- carte instable
- aucune difference visible entre ping et non-ping

### 15.3 Gate 3 - Passive sweep baseline

Rouler lentement dans le corridor.

Verifier:
- apparition de tracks passifs sur les acteurs equipés
- tracks plus stables sur structure que sur neutral ou threat
- comportement coherent quand la vitesse augmente

Critere de rejet:
- aucun track passif exploitable
- tracks incoherents ou instantanement perdus sans cause

### 15.4 Gate 4 - Route-only navigation

Avancer uniquement via la lecture sonar.

Verifier:
- le joueur peut suivre la route sans vision externe fiable
- la carte ne tourne pas avec le sous-marin
- l'icone sub donne bien l'orientation
- les obstacles ou parois apres virage deviennent lisibles avant impact

Critere de rejet:
- sonar inutilisable pour piloter
- lecture impossible en virage

### 15.5 Gate 5 - Acoustic context

Traverser le volume acoustique.

Verifier:
- diminution de lisibilite perceptible
- clutter ou signal plus instable
- retour a un comportement plus propre en sortie

Critere de rejet:
- aucun effet gameplay visible du volume

### 15.6 Gate 6 - Hold ping

Maintenir l'action de ping.

Verifier:
- cadence continue respectee
- cooldown respecte
- lecture enrichie sans clignotement absurde
- perf stable

Critere de rejet:
- spam inutilisable
- perf degradee brutalement
- effacement permanent des infos

## 16. Gameplay checklist

Checklist a remplir a chaque passe PIE:

- `SonarDisplay` visible et bind
- ping actif fonctionnel
- hold ping fonctionnel
- topologie fixe lisible
- icone sub orientee correctement
- tracks passifs visibles
- differences structure / neutral / threat perceptibles
- self-noise perceptible quand la propulsion augmente
- volume acoustique perceptible
- navigation sonar-only possible
- aucune dependance a un cheat camera externe

## 17. Observability and logs

Quand une passe echoue, activer l'observabilite avant toute re-ecriture.

Activer:
- `SonarSystem.bEnableDebugLogs = true`

Surveiller:
- `LogSonar`
- `LogSubRun`
- `LogRouteGen`

Interpreter les problemes ainsi:

Cause probable:
- bind widget absent
- input non route
- config data non assignee
- pas de route coarse seed
- emitter mal configure

Contributeur possible:
- map trop bruitee
- tunnel trop complexe pour le niveau de tuning courant
- vitesse sub trop elevee pour la lisibilite MVP

Preoccupation differee:
- polish visuel
- materials CRT avances
- presentation finale de la station

## 18. GO / NO-GO criteria

GO seulement si:
- le sonar est explicitement configure via assets
- la map canonique suffit a valider la lecture
- la navigation sonar-only est possible sur une portion representative
- le ping actif et le passif servent a des decisions differentes
- les volumes acoustiques ont un effet perceptible
- le systeme reste stable sur plusieurs runs PIE

NO-GO si:
- le sonar depend encore d'un setup implicite fragile
- les assets ne sont pas crees
- le joueur pilote surtout a la vision externe
- les pings detruisent plus d'information qu'ils n'en donnent
- aucun contexte acoustique n'est perceptible

## 19. Immediate next implementation tranche

Ordre de travail recommande apres ce setup:

1. Creer les quatre data assets minimaux.
2. Les assigner explicitement dans `BP_Submarine_Compiler`.
3. Verifier `WBP_SubHelm` et `SonarDisplay`.
4. Placer les trois acteurs de test.
5. Placer au moins un volume acoustique.
6. Lancer une premiere passe PIE instrumentee.
7. Corriger uniquement les points qui cassent la lisibilite ou la boucle gameplay.

Ne pas passer au polish UI final avant que cette validation soit verte.
