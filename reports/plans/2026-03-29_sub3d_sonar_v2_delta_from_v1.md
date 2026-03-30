# Sonar V2 Delta From V1

Baseline historique: `reports/plans/2026-03-29_sub3d_helm_and_sonar_v1_spec.md`

## 1. Changement de représentation
V1:
- projection principale centrée sub-frame (tube guide visuel).

V2:
- carte topologique en world/route frame.
- sous-marin centré avec orientation propre.
- topologie ne pivote plus quand le sub pivote.

## 2. Changement d’architecture runtime
V1:
- ping actif piloté directement par `USubSonarComponent`.
- affichage surtout basé sur points de ping.

V2:
- `USubSonarComponent` conservé pour émission active.
- `USubSonarSystemComponent` ajouté comme orchestrateur modes+topo.
- `USonarContactTrackerComponent` dédié pour cycle de vie tracks.
- `USonarNoiseEmitterComponent` + `USonarAcousticVolumeComponent` ajoutés.

## 3. Changement gameplay
V1:
- ping et points persistants/fade.
- lisibilité dépendante de la vue de projection.

V2:
- tracks passifs flous (MVP1).
- bruit propre agrégé.
- mode passif/focus/terrain.
- priorisation tracks.

## 4. Changement réseau
V1:
- réplication centrée sur `SonarPoints`.

V2:
- réplication tactique:
  - runtime sonar state
  - tracks
  - topo fenêtré compressé
- pas de réplication raycasts bruts.

## 5. Conformance Sonar V2 -> FP
Objectif conformance avec run FP (sans modifier la state machine run):
- sonar-only navigation possible sur level shell.
- boucle `Departure -> Traverse -> BreachCrisis` jouable sans vision externe.
- pas de vision cheat: topo issue seed route + découverte runtime.

## 6. Migration guide
1. Conserver hooks input ping existants (`TriggerSonarPing`, `SetSonarPingHeld`).
2. Brancher `USubSonarSystemComponent` sur `ASubmarineBase`.
3. Binder `USubSonarDisplayWidget` via `USubHelmWidget::TryBindSonarDisplay`.
4. Exposer commandes mode/focus/range/priorité via `ASubPlayerController`.
5. Migrer UI Blueprint Helm pour lire les données du sonar system (tracks/topo/state).
