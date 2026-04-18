# Staging Helm Navigation

## But

Document macro de suivi pour la suite Helm Navigation.

Il sert a garder une vision claire:
- de l'objectif gameplay
- de ce qui existe deja
- de ce qui reste a faire
- de l'ordre logique jusqu'a une suite Helm Navigation exploitable

Il ne remplace pas les specs detaillees.
Il sert de reference courte de pilotage.

## Canon

References a garder:
- `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`
- `reports/plans/2026-03-30_sub3d_tunnel_nav_phase2_editor_validation_handoff.md`
- `reports/plans/2026-03-29_sub3d_first_playable_level_architecture.md`
- `reports/plans/staging/StagingSonar.md`

## Decision actuelle

Le sonar principal est valide en Phase 2.

Decision importante:
- on ne continue pas a charger le sonar principal avec des aides de pilotage structurees
- la suite suivante est une couche separee: Helm Navigation

Le sonar principal garde son role:
- information tactique imparfaite du monde exterieur
- lecture topologique globale
- tracks
- ping actif / passif

La Helm Navigation Suite prend un autre role:
- aide de pilotage
- clearance
- anticipation
- engagement corridor
- lecture graphe local

## Objectif produit

La Helm Navigation Suite doit aider le joueur a piloter un sous-marin massif dans des tunnels 3D, la ou le sonar global ne suffit pas.

Elle doit repondre a ces questions:
- suis-je centre dans le tunnel
- est-ce que je monte trop
- est-ce que je descends trop
- quelle est ma marge droite/gauche/haut/bas
- que se passe-t-il devant sur la route
- puis-je encore freiner
- suis-je en zone d'engagement sans demi-tour
- ou est le prochain hub / split / merge

## Runtime truth

La verite runtime reste:
- `ATraversalRouteActor` pour la route
- `UTunnelNavDataAsset` comme sidecar derive
- `UTunnelNavigationRuntimeComponent` comme couche de requetes helm

Regles:
- pas de nouvelle autorite route
- pas de dependance au mesh marching-cubes pour la logique helm
- pas de changement de replication

## Ce qui est deja fait

### Sidecar et runtime query

Deja en place:
- sidecar TunnelNav derive de la pipeline
- runtime query layer sur le sous-marin
- projection sur route
- coupe locale
- lookahead
- graph window
- restrictions
- warning de stop distance
- warning commitment / no turn
- drift heading vs velocity
- debug draw monde

### Validation editor

La validation editor de base existe deja:
- projection route
- cross-section locale
- drift
- lookahead
- restrictions par classe

## Ce qui n'est pas encore fait

Pas encore fait:
- vraie UI Helm Navigation dediee
- ecran front cross-section
- ecran forward anticipation
- ecran tactical graph
- indicateurs haut / bas lisibles en station
- packaging clair des infos pour l'humain au helm

## Vision de la suite Helm Navigation

La suite cible est composee de trois vues.

### Ecran A - Front Cross-Section

But:
- centrage fin
- hauteur
- derive laterale
- marge instantanee

Infos typiques:
- contour coupe locale
- silhouette sub
- heading vs velocity
- clearance left/right/up/down
- safe above / safe down

### Ecran B - Forward Anticipation

But:
- lecture devant le sub
- anticipation de virage
- anticipation de retrecissement
- engagement corridor
- stop distance

Infos typiques:
- profil avant le long de la route
- plafond / sol
- narrowing
- crash stop marker
- no-turn marker

### Ecran C - Tactical Graph

But:
- lecture macro locale
- prochains hubs
- prochains splits / merges
- awareness route

Infos typiques:
- edge courant
- noeuds voisins
- prochain hub
- prochain split
- branche canonique / optionnelle

## Regle de separation

- Sonar screen = monde exterieur tactique et imparfait
- Helm Navigation = aides de pilotage structurees

Ne pas melanger les deux trop tot.

Le sonar principal ne doit pas devenir une pseudo cross-section confuse.
La Helm Navigation doit venir comme une couche dediee.

## Idee gameplay importante

Les infos de navigation avancee peuvent devenir plus tard:
- un module achetable
- un module craftable
- un upgrade de sous-marin
- un poste coop plus specialise

Exemples:
- safe above / safe down
- meilleure anticipation
- meilleure lecture de graph local
- aide au centrage

Mais pour l'instant:
- on vise d'abord une version fonctionnelle de base

## Phase 1 - Packaging des donnees helm

Objectif:
- sortir des seules queries brutes

A faire:
- identifier les structs runtime a exposer proprement au futur widget
- garder une sortie lisible pour:
  - coupe locale
  - anticipation
  - graphe local
- verifier que les infos sont suffisantes pour la UI

Done quand:
- on sait alimenter une UI sans recalcul lourd dans le widget

## Phase 2 - Debug Helm Panels

Objectif:
- valider le gameplay avant tout polish

A faire:
- panneau debug A: front cross-section
- panneau debug B: forward anticipation
- panneau debug C: tactical graph

Regle:
- pas belle UI
- pas de materials fancy
- pas de polish
- seulement utilite

Done quand:
- les trois panneaux sont lisibles en PIE et utiles au pilotage

## Phase 3 - Front Cross-Section playable

Objectif:
- aider vraiment le pilotage fin

A faire:
- afficher la coupe locale
- afficher le sub projete
- afficher heading et velocity
- afficher left/right/up/down
- afficher safe above / safe down

Done quand:
- le joueur peut se recaler en tunnel et lire son clearance rapidement

## Phase 4 - Forward Anticipation playable

Objectif:
- aider le pilotage avance et le freinage

A faire:
- profil route-ahead
- lecture des virages
- lecture des retrecissements
- marker stop distance
- warning no-turn / commitment

Done quand:
- le joueur peut anticiper un danger avant de le voir sur le sonar global

## Phase 5 - Tactical Graph playable

Objectif:
- donner une conscience locale de la structure de route

A faire:
- edge courant
- noeuds proches
- prochain hub
- prochain split
- branche courante

Done quand:
- le joueur comprend mieux ou il est dans le systeme de tunnels

## Phase 6 - Integration produit

Objectif:
- brancher la Helm Navigation dans la boucle First Playable

A faire:
- verifier l'utilite dans Departure -> Traverse -> BreachCrisis
- verifier compatibilite avec les gros sous-marins
- verifier la lisibilite sans cheat camera
- verifier l'interet coop potentiel

Done quand:
- la suite aide reellement le gameplay et ne fait pas doublon avec le sonar principal

## Priorites immediates

Ordre recommande:

1. definir le contrat UI-ready des donnees helm
2. faire les trois panneaux debug
3. valider la lecture en PIE
4. corriger les ecarts de gameplay
5. seulement ensuite penser a une vraie presentation station

## Points a surveiller

- ne pas reintroduire une dependance aux triangles render
- ne pas multiplier les sources de verite
- ne pas faire une UI complexe avant validation gameplay
- ne pas surcharger le sonar principal avec des infos de clearance qui appartiennent a HelmNav

## Definition de "suite helm fonctionnelle"

La suite Helm Navigation est consideree fonctionnelle quand:
- elle donne une aide de clearance immediate
- elle donne une aide d'anticipation credible
- elle donne une awareness locale de route
- elle reste lisible pendant la manoeuvre
- elle aide sans remplacer entierement la tension du pilotage
