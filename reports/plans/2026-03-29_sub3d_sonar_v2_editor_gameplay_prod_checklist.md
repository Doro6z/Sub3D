# Sub3D Sonar V2 — Checklist Prod (One-Page)

## 0) Références
- Canon: `reports/plans/2026-03-29_sub3d_sonar_v2_spec.md`
- Delta: `reports/plans/2026-03-29_sub3d_sonar_v2_delta_from_v1.md`
- Handoff complet: `reports/plans/2026-03-29_sub3d_sonar_v2_editor_gameplay_handoff.md`

## 1) Préflight code
1. Build `Sub3DEditor Win64 Development` = OK.
2. Vérifier composants sur `BP_Submarine_Compiler`:
3. `Sonar` présent.
4. `SonarSystem` présent.

## 2) Assets obligatoires à créer
1. `/Game/Sub3D/Sonar/Data/DA_SonarSystemConfig_Default` (`SonarSystemConfigData`).
2. `/Game/Sub3D/Sonar/Profiles/DA_SonarSignature_Threat` (`SonarSignatureData`).
3. `/Game/Sub3D/Sonar/Profiles/DA_SonarSignature_Neutral` (`SonarSignatureData`).
4. `/Game/Sub3D/Sonar/Profiles/DA_SonarSignature_Structure` (`SonarSignatureData`).
5. `/Game/Sub3D/Sonar/Profiles/DA_SonarEnv_Default` (`SonarEnvironmentProfileData`).
6. `/Game/Sub3D/Sonar/Profiles/DA_SonarUpgrade_Mk1` (`SonarUpgradeData`).

## 3) Assignations BP Sub
1. Dans `BP_Submarine_Compiler -> SonarSystem`:
2. `SystemConfig = DA_SonarSystemConfig_Default`.
3. `bEnablePassiveSweep = true`.
4. `bEnableRouteCoarseSeed = true`.
5. `bEnableDebugLogs = false`.
6. Dans `BP_Submarine_Compiler -> Sonar`:
7. `bAccumulatePointsAcrossPings = true`.
8. `PingCooldownS` réglé.
9. `ContinuousPingIntervalS` réglé.

## 4) Input/Controller
1. `IA_SonarPing Pressed -> TriggerSonarPing()`.
2. `IA_SonarPing Pressed/Released -> SetSonarPingHeld(true/false)`.
3. Ajouter actions mode/range si absentes:
4. `IA_SonarModePassive`, `IA_SonarModeFocus`, `IA_SonarModeTerrain`.
5. `IA_SonarRangeNext`, `IA_SonarRangePrev`.
6. Router vers:
7. `SetSonarMode(...)`.
8. `SetSonarFocusBearing(...)`.
9. `SetSonarRangePreset(...)`.
10. `MarkSonarPriorityTrack(...)`.

## 5) UI Helm
1. Dans `WBP_SubHelm`, inclure un `SubSonarDisplayWidget` (nom: `SonarDisplay`).
2. Assigner textures sonar (sub center, reticle, noise, smudge).
3. Vérifier appels:
4. `RouteSonarPing`.
5. `RouteSonarPingHeldStart/Stop`.
6. `RouteSetSonarMode`.
7. `RouteSetSonarRangePreset`.
8. `RouteMarkPriorityTrack`.

## 6) Map gameplay
1. `TraversalRouteActor` présent.
2. Route générée/bake valide (sonar field dispo).
3. Poser 2 à 3 acteurs test avec `SonarNoiseEmitterComponent`.
4. Poser 1 volume test avec `SonarAcousticVolumeComponent`.

## 7) Valeurs par défaut minimales
1. `RangePresetsCm = [9000,15000,22000]`.
2. `PassiveSweepIntervalS = 0.30`.
3. `PassiveMinDetectionScore = 0.12`.
4. `TrackDecayPerSecond = 0.08`.
5. `LostThresholdS = 2.5`.
6. `LostRetentionS = 6.0`.
7. `TopologyCellSizeCm = 500`.
8. `MaxReplicatedTopoCells = 1500`.

## 8) Validation PIE (GO/NO-GO)
1. GO si `IsSonarDisplayBound == true`.
2. GO si ping press affiche pulse.
3. GO si hold ping répète correctement.
4. GO si un nouveau ping n’efface pas la topo déjà révélée.
5. GO si rotation sub: topo fixe, seule orientation sub change.
6. GO si tracks passifs apparaissent sur émetteurs.
7. GO si tracks passent `Lost` puis disparaissent après retention.
8. GO si volume acoustique modifie lisiblement la détection.

## 9) Validation réseau (listen + 1 client)
1. GO si tracks cohérents entre server/client.
2. GO si topo fenêtrée cohérente.
3. GO si ping pilote visible côté client.
4. GO si pas de spam payload évident (`stat net`).

## 10) Debug rapide si échec
1. Activer `SonarSystem.bEnableDebugLogs = true`.
2. Vérifier seed topo route reçu.
3. Vérifier cadence passive/track/self-noise.
4. Corriger bindings widget/controller avant tout tuning.

## 11) Ordre de passage recommandé (strict)
1. Build.
2. Assets data.
3. Assignations BP Sub.
4. Input/controller.
5. Widget helm.
6. Emetteurs + volume.
7. PIE solo.
8. PIE réseau.
9. Tuning final.
