# Sub3D — Sonar V2 Editor + Gameplay Implementation Handoff (Human Guide)

## 0) Scope and canonical references

Ce document est le **guide d’implémentation humain** (éditeur + gameplay) pour brancher Sonar V2 de bout en bout.

Références canoniques:
- `reports/plans/2026-03-29_sub3d_sonar_v2_spec.md`
- `reports/plans/2026-03-29_sub3d_sonar_v2_delta_from_v1.md`
- `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`

## 1) État code actuellement implémenté (déjà en place)

Runtime C++:
- `USubSonarSystemComponent`: orchestration modes, passive sweep, topo world-frame, réplication tactique.
- `USonarContactTrackerComponent`: cycle des tracks.
- `USonarNoiseEmitterComponent`: sources de bruit passif.
- `USonarAcousticVolumeComponent`: modificateurs ambiants.
- `USubSonarComponent`: ping actif/hold/cooldown (conservé).
- `USubSonarDisplayWidget`: rendu topo fixe + tracks + sous-marin centré orienté.

API station/controller:
- Gardés stables:
  - `TriggerSonarPing()`
  - `SetSonarPingHeld(bool)`
- Ajoutés:
  - `SetSonarMode(ESonarMode)`
  - `SetSonarFocusBearing(float)`
  - `SetSonarRangePreset(int32)`
  - `MarkSonarPriorityTrack(int32,bool)`

Data assets C++ disponibles:
- `USonarSystemConfigData`
- `USonarSignatureData`
- `USonarEnvironmentProfileData`
- `USonarUpgradeData`

## 2) Arborescence assets recommandée

Créer (ou utiliser) cette structure:
- `/Game/Sub3D/Sonar/Data`
- `/Game/Sub3D/Sonar/Profiles`
- `/Game/Sub3D/Sonar/UI`
- `/Game/Sub3D/Sonar/Blueprints`
- `/Game/Sub3D/Sonar/Materials`

## 3) Création des assets data (ultra concret)

### 3.1 Sonar system config
Créer:
- `DA_SonarSystemConfig_Default` (type: `SonarSystemConfigData`)
Chemin:
- `/Game/Sub3D/Sonar/Data/DA_SonarSystemConfig_Default`

Valeurs de départ recommandées:
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

### 3.2 Signatures sonar
Créer minimum:
- `DA_SonarSignature_Threat`
- `DA_SonarSignature_Neutral`
- `DA_SonarSignature_Structure`
Type:
- `SonarSignatureData`
Chemin:
- `/Game/Sub3D/Sonar/Profiles/*`

Preset recommandé:
- Threat:
  - `ContactClass = MobileThreat`
  - `BaseNoiseStrength = 0.9`
  - `ActiveReflectivity = 0.8`
  - `SignalStability = 0.5`
- Neutral:
  - `ContactClass = MobileNeutral`
  - `BaseNoiseStrength = 0.45`
  - `ActiveReflectivity = 0.45`
  - `SignalStability = 0.7`
- Structure:
  - `ContactClass = StructureActive`
  - `BaseNoiseStrength = 0.25`
  - `ActiveReflectivity = 0.9`
  - `SignalStability = 0.95`

### 3.3 Environment profile
Créer:
- `DA_SonarEnv_Default`
Type:
- `SonarEnvironmentProfileData`
Chemin:
- `/Game/Sub3D/Sonar/Profiles/DA_SonarEnv_Default`

Valeurs de départ:
- `AmbientNoiseBias = 0.0`
- `ClutterBias = 0.0`
- `PassiveDetectionModifier = 1.0`
- `ActivePingDistortion = 0.0`

### 3.4 Upgrade profile
Créer:
- `DA_SonarUpgrade_Mk1`
Type:
- `SonarUpgradeData`
Chemin:
- `/Game/Sub3D/Sonar/Profiles/DA_SonarUpgrade_Mk1`

Valeurs:
- `RangeMultiplier = 1.0`
- `ClassificationMultiplier = 1.0`
- `SelfNoiseResistance = 1.0`
- `ActivePingCooldownMultiplier = 1.0`

## 4) Branches Blueprint/Actor à faire dans l’éditeur

## 4.1 BP_Submarine_Compiler (ou BP sub final)
Ouvrir `BP_Submarine_Compiler` et vérifier composants:
- `Sonar` (`USubSonarComponent`) présent.
- `SonarSystem` (`USubSonarSystemComponent`) présent.

Dans `SonarSystem`:
- assigne `SystemConfig = DA_SonarSystemConfig_Default`.
- `bEnablePassiveSweep = true`
- `bEnableRouteCoarseSeed = true`
- `bEnableDebugLogs = false` en prod, `true` en debug.

Dans `Sonar`:
- garde `bAccumulatePointsAcrossPings = true`
- `PingCooldownS` selon tuning gameplay.
- `ContinuousPingIntervalS` pour hold.

## 4.2 Player Controller BP (`PC_SubPlayerController`)
Bindings input (IMC Helm):
- IA_SonarPing Pressed -> `TriggerSonarPing()`
- IA_SonarPing Pressed/Released (hold) -> `SetSonarPingHeld(true/false)`

Ajouter actions sonar mode (si absentes):
- IA_SonarModePassive
- IA_SonarModeFocus
- IA_SonarModeTerrain
- IA_SonarRangeNext / IA_SonarRangePrev

Node mapping recommandé:
- `SetSonarMode(PassiveStandard / PassiveFocusSector / TerrainScan)`
- `SetSonarRangePreset(Index)`
- `SetSonarFocusBearing(BearingDeg)` depuis widget knob/slider.

## 4.3 Helm widget (`WBP_SubHelm`)
Dans WidgetTree:
- garder/ajouter un widget de type `SubSonarDisplayWidget`.
- le nommer clairement: `SonarDisplay`.

Dans l’instance `SubHelmWidget`:
- propriété `SonarDisplay` assignée (ou auto-discovery).
- `bAutoCreateSonarDisplayIfMissing = false` (préféré en prod).

Branching événements UI:
- bouton ping -> `RouteSonarPing()`
- hold ping down/up -> `RouteSonarPingHeldStart/Stop()`
- mode buttons -> `RouteSetSonarMode(...)`
- range selector -> `RouteSetSonarRangePreset(...)`
- priorité track -> `RouteMarkPriorityTrack(...)`

## 4.4 Sonar display widget (`WBP_SubRadar` ou sonar panel dédié)
Assigner textures existantes:
- `CenterSubTexture` (icône sub)
- `CenterReticleTexture` (réticule)
- `NoiseOverlayTexture`
- `SmudgeOverlayTexture`

Valeurs UI recommandées:
- `bDrawTopologyWireframe = true`
- `bDrawTracks = true`
- `bDrawSweepPulse = true`
- `RingCount = 4`
- `TopologyCellSizeCm = 500` (aligné config)
- `CenterTextureScale = 0.30~0.40`

Important:
- ne pas refaire une projection caméra locale.
- la projection doit rester via les données world-frame du sonar system.

## 4.5 Émetteurs de bruit (actors gameplay)
Pour chaque acteur détectable en passif:
- ajoute composant `SonarNoiseEmitterComponent`.
- règle:
  - `ContactClass`
  - `BaseNoiseStrength`
  - `ActiveReflectivity`
  - `VelocityNoiseScale`
  - `bLikelyHostile`

Exemples:
- Drone hostile:
  - `ContactClass = MobileThreat`
  - `BaseNoiseStrength = 0.85`
  - `VelocityNoiseScale = 0.003`
  - `bLikelyHostile = true`
- Faune:
  - `ContactClass = MobileNeutral`
  - `BaseNoiseStrength = 0.35`
  - `VelocityNoiseScale = 0.002`
  - `bLikelyHostile = false`

## 4.6 Volumes acoustiques
Créer des actors volumes (Box/Sphere) dans la map.
Sur chaque volume, ajoute:
- `SonarAcousticVolumeComponent`

Paramétrage:
- grotte bruitée:
  - `AmbientNoiseBias = 0.35`
  - `ClutterBias = 0.30`
  - `PassiveDetectionModifier = 0.85`
- corridor calme:
  - `AmbientNoiseBias = -0.05`
  - `ClutterBias = -0.10`
  - `PassiveDetectionModifier = 1.10`

## 4.7 Route seeding topo
Le seed topo runtime lit le `USonarFieldComponent` de `ATraversalRouteActor`.
Pré-requis:
- `TraversalRouteActor` présent en map.
- route générée (C6 sonar field disponible).
- `SonarSystem.bEnableRouteCoarseSeed = true`.

Debug action:
- appeler `ForceRebuildTopologyFromRoute()` sur le composant sonar system.

## 5) Branchage gameplay concret par phase

### 5.1 Boot/Boarding
- vérifier spawn sub + crew normal.
- helm station utilisable.
- sonar display bind OK (`IsSonarDisplayBound = true`).

### 5.2 Departure
- ping actif pour valider couloir initial.
- vérifier que la topo reste visible après pings successifs.
- vérifier que la carte ne pivote pas quand le sub tourne.

### 5.3 Traverse
- passive tracks visibles sur émetteurs.
- test mode `PassiveFocusSector` (améliore un secteur).
- test range presets (changement échelle sans rotation carte).

### 5.4 BreachCrisis
- gameplay sonar reste utilisable sous stress.
- `SelfNoiseState.AggregateNoise` augmente si propulsion/pump élevés.

## 6) Data tuning workflow (pour humain)

Ordre conseillé:
1. stabiliser lisibilité UI (`Dot/Track/Topo`).
2. calibrer détection passive (distance + noise penalties).
3. calibrer hold ping cadence/cooldown.
4. calibrer clutter volumes.
5. calibrer classes Threat/Neutral/Structure.

Règle:
- jamais tuner plusieurs axes simultanément.
- garder un sheet de tuning versionné (`v2_tuning_pass_01.csv` si besoin externe).

## 7) Validation PIE ultra concrète

Check rapide en 8 points:
1. Helm: `SonarDisplay` bound.
2. Ping press: pulse visible.
3. Hold ping: répétition conforme cooldown.
4. Nouveau ping: n’efface pas topo accumulée.
5. Rotation sub: topo fixe, seul repère sub/orientation change.
6. Passive emitter proche: apparition d’un track.
7. Track decay: passe `Lost` puis disparaît après retention.
8. Volume acoustique: variation perceptible de stabilité/détection.

## 8) Réseau (ce que l’humain doit vérifier en session coop)

En listen server + 1 client:
- même tracks (id/state approximativement cohérents).
- même topo fenêtre autour du sub.
- pas de spam massif visible dans stat net.
- ping lancé par pilote répercuté côté client.

## 9) Debug/observability

Activer temporairement:
- `SonarSystem.bEnableDebugLogs = true`

Attendus logs:
- route seed appliqué (points seed > 0 en map route).
- updates de self noise.
- updates tracks/topo à cadence stable.

Désactiver en production:
- `bEnableDebugLogs = false`

## 10) Gaps connus / hors périmètre immédiat

Déjà prévu mais pas verrouillé dans ce document:
- classification avancée multi-signal (MVP2+).
- anomalies/faux positifs localisés (MVP3).
- réactions IA au ping (MVP3).

## 11) Résumé actionnable immédiat (ordre exécution humain)

1. Créer les DataAssets section 3.
2. Assigner `DA_SonarSystemConfig_Default` sur le composant `SonarSystem` du BP sub.
3. Vérifier bindings input sonar dans `PC_SubPlayerController`.
4. Vérifier `WBP_SubHelm` + `SubSonarDisplayWidget` branchés.
5. Ajouter 2-3 `SonarNoiseEmitterComponent` sur acteurs de test.
6. Ajouter 1 volume acoustique de test.
7. Lancer PIE et exécuter la checklist section 7.
8. Ajuster tuning passif/range/cooldown, puis verrouiller une baseline.
