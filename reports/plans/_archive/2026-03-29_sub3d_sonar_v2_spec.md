# Sub3D Sonar V2 — Canonical Spec

## 1. Scope
Sonar V2 définit le sonar gameplay coop pour les runs FP sans changer la machine à états run-phase existante.  
Objectif MVP1: navigation lisible et imparfaite via une carte topologique fixe, tracks flous basiques, et ping actif persistant.

## 2. Canon decisions
- Référentiel de carte: monde/route (pas sub-frame).
- Sous-marin affiché au centre, orientation variable.
- Topologie affichée ne pivote pas avec le sub.
- Source topo hybride:
  - seed coarse depuis `USonarFieldComponent` de la route.
  - enrichissement runtime via pings actifs + terrain scan.
- Rendu principal: 2.5D wireframe stylisé.

## 3. Runtime architecture
- `USubSonarComponent`:
  - reste la couche active ping (raycasts, cooldown, hold).
- `USubSonarSystemComponent`:
  - orchestre modes sonar.
  - calcule bruit propre.
  - lance passive sweep/terrain sweep.
  - fusionne topo world-frame.
  - publie tracks/topo répliqués.
- `USonarContactTrackerComponent`:
  - maintient les tracks et transitions d’état.
- `USonarNoiseEmitterComponent`:
  - signature passive des acteurs.
- `USonarAcousticVolumeComponent`:
  - modificateurs locaux ambiant/clutter/détection.

## 4. Modes
- `PassiveStandard`
- `PassiveFocusSector`
- `ActivePing`
- `TerrainScan`

## 5. Track model
- États:
  - `Suspected -> Tracked -> Classified -> Confirmed -> Lost`
- Structs:
  - `FSonarDetectionSample`
  - `FSonarTrack`
  - `FSonarSelfNoiseState`

## 6. Topological model
- Cellule runtime `FSonarTopoCell`:
  - `(GridX, GridY, HeightDm, Occupancy, Confidence, bFromSeed)`.
- `USubSonarSystemComponent` maintient une grille locale fenêtrée centrée sur le sub.
- Réplication: uniquement la fenêtre utile compressée (`ReplicatedTopoWindow`).

## 7. UI contract (Helm/Sonar Display)
- `USubSonarDisplayWidget` lit `USubSonarSystemComponent` + `USubSonarComponent`.
- Couches obligatoires:
  - fond CRT/grid/rings.
  - topologie wireframe fixe.
  - tracks tactiques.
  - sous-marin central + direction.
  - pulse actif.
- `USubHelmWidget` doit binder automatiquement la display au sonar/sonar system.

## 8. Controller/station API contract
Stables:
- `TriggerSonarPing()`
- `SetSonarPingHeld(bool)`

Nouveaux:
- `SetSonarMode(ESonarMode)`
- `SetSonarFocusBearing(float)`
- `SetSonarRangePreset(int32)`
- `MarkSonarPriorityTrack(int32, bool)`

## 9. Network/performance
- Serveur autoritaire:
  - détection, tracking, self-noise, cooldown, topo.
- Réplication:
  - tracks tactiques + runtime state + topo fenêtré.
- Non répliqué:
  - raycasts bruts et nuages complets.

Cadences ciblées:
- passive sweep: 2–5 Hz
- track maintenance: 4–10 Hz
- self-noise: 4–10 Hz
- UI refresh: 10–20 Hz
- active ping: on demand

## 10. Delivery phases
- MVP1:
  - `PassiveStandard + ActivePing + tracks flous + topo world-frame + UI lisible`
- MVP2:
  - `FocusSector + TerrainScan + volumes + priorités tracks + intégration postes`
- MVP3:
  - `anomalies + upgrades + dégâts sonar + faux positifs + réactions IA`
