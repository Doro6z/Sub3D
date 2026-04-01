# Staging Sonar

## But

Document macro de suivi.

Il sert a garder une vision claire:
- de ce qui est deja implemente
- de ce qu'il reste a faire
- de l'ordre logique jusqu'a un systeme sonar fonctionnel et validable en gameplay

Il ne remplace pas les specs detaillees.
Il sert de reference courte de pilotage.

## Canon

References a garder:
- `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
- `reports/plans/2026-03-29_sub3d_sonar_v2_spec.md`
- `reports/plans/2026-03-29_sub3d_sonar_v2_delta_from_v1.md`
- `reports/plans/2026-03-30_sub3d_sonar_durable_validation_setup.md`

## Decision actuelle

Le sonar actuel est organise ainsi:

- `USubSonarComponent`
  - ping actif
  - hold ping
  - cooldown
  - points d'impact persistants

- `USubSonarSystemComponent`
  - modes sonar
  - passive sweep
  - self-noise
  - coarse seed route
  - tracks
  - topo runtime

- `USubSonarDisplayWidget`
  - ecran sonar principal actuel
  - topo fixe
  - pulse
  - tracks
  - overlays

Decision importante:
- on garde pour l'instant un seul ecran sonar principal
- on ne passe pas encore a la suite finale multi-ecrans helm navigation

La suite multi-ecrans viendra plus tard:
- ecran A: front cross-section
- ecran B: forward anticipation
- ecran C: tactical graph

## Ce qui est deja fait

### Runtime

- base sonar active en place
- systeme sonar V2 en place
- tracker en place
- emitters de bruit en place
- volumes acoustiques en place
- widget sonar principal en place

### Route / monde

- route shell first playable en place
- coarse seed route possible depuis la route
- validation PIE possible sur la map shell

### Inputs / station

- l'API runtime existe
- on peut piloter:
  - mode
  - range
  - focus bearing
  - ping
  - hold ping

Decision UI actuelle:
- les boutons widget peuvent piloter ces fonctions
- pas besoin de passer tout de suite par des `IA_*` si l'objectif est de valider la station

## Ce qui est en cours

### Wiring UI / station

- brancher des events BP de widget vers les API runtime:
  - `SetSonarMode(...)`
  - `SetRangePresetIndex(...)`
  - `SetFocusBearing(...)`
  - `TriggerSonarPing()`
  - `SetSonarPingHeld(...)`

### Setup durable

- creation des data assets sonar minimaux
- assignation explicite de ces assets sur le sous-marin
- verification du bind `SonarDisplay` dans le helm

## Ce qu'il reste a faire

## Phase 1 - Setup durable

Objectif:
- sortir du setup implicite

A faire:
- creer `DA_SonarSystemConfig_Default`
- creer `DA_SonarEnv_Default`
- creer `DA_SonarUpgrade_Mk1`
- creer les signatures minimum:
  - structure
  - neutral
  - threat
- assigner `DA_SonarSystemConfig_Default` au `SonarSystem`

Done quand:
- le sonar tourne avec une config asset explicite

## Phase 2 - Wiring helm propre

Objectif:
- avoir une station sonar pilotable proprement

A faire:
- verifier `WBP_SubHelm`
- garder un `SonarDisplay` explicite
- brancher les boutons UI
- verifier:
  - mode
  - range
  - focus
  - ping
  - hold ping

Done quand:
- tout le pilotage sonar passe par une route claire widget -> station/controller -> runtime

## Phase 3 - Map de validation gameplay

Objectif:
- disposer d'une map canonique de validation

A faire:
- garder `L_FP01_RunShell`
- garder un seul sub de reference
- garder un seul route actor de reference
- placer des cibles de test:
  - structure
  - neutral
  - threat
- placer au moins un volume acoustique

Done quand:
- on peut repeter les memes tests PIE a chaque iteration

## Phase 4 - Validation gameplay sonar

Objectif:
- prouver que le sonar aide reellement la navigation et la decision

Tests a faire:
- ping actif lisible
- hold ping stable
- tracks passifs utiles
- topo fixe lisible
- la carte ne pivote pas avec le sub
- seule l'icone sub pivote
- navigation possible sans vision externe fiable
- impact du self-noise perceptible
- impact des volumes acoustiques perceptible

Done quand:
- le joueur peut progresser dans le tunnel avec le sonar comme senseur principal

## Phase 5 - Corrections de lisibilite

Objectif:
- corriger seulement ce qui casse la boucle gameplay

Exemples:
- topo trop confuse
- tracks illisibles
- ping trop destructif
- range mal calibree
- trop peu de difference entre passif et actif
- manque d'effet du bruit propre

Done quand:
- la lecture sonar devient stable et exploitable

## Phase 6 - Future navigation suite

Hors scope immediat.

Cette phase viendra apres validation du sonar principal.

A faire plus tard:
- ecran A: front cross-section
- ecran B: forward anticipation
- ecran C: tactical graph

Condition d'entree:
- sonar principal deja valide en gameplay

## Regles simples

- ne pas casser `USubSonarDisplayWidget` tant que le sonar principal n'est pas valide
- ne pas melanger trop tot sonar tactique et navigation structuree
- ne pas polisher avant validation gameplay
- ne pas multiplier les maps de test
- ne pas dependre d'un setup implicite fragile

## Prochaine tranche concrete

Ordre recommande maintenant:

1. finir le wiring widget -> runtime
2. creer les data assets minimaux
3. les assigner sur le sous-marin
4. placer les cibles de test et volumes acoustiques
5. lancer une vraie passe PIE
6. lister les ecarts gameplay observes

## Definition de "systeme fonctionnel"

Le systeme sonar est considere fonctionnel quand:

- il est explicitement configure par assets
- il est pilotable depuis la station
- le ping actif marche
- le hold ping marche
- les tracks passifs marchent
- la topo est lisible
- la navigation sonar-only est possible sur la route shell
- les comportements restent stables sur plusieurs runs PIE
