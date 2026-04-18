# Proto04D — Spec Technique : Water, Pressure, Confinement, Future Visual Water

Projet : Sub3D  
Date : 2026-03-28  
Status : Reference d'execution  
Scope : refonder la simulation eau/pression du sous-marin compile sur un modele hydraulique compartimente, serveur-only, deterministe  
Parent : `2026-03-27_sub3d_proto04C_spec_technique.md`  
Draft source : `2026-03-28_sub3d_proto04D_water_pressure_spec_draft.md`

---

## 0. Cadrage

### Objectif

`Proto04D` remplace le flooding actuel par une simulation hydraulique plus coherente, basee sur :

- compartiments
- portes / connexions internes
- breches vers l'exterieur
- pression externe
- pression interne
- debits
- pompes
- masse totale derivee de l'eau reelle

Le but n'est pas de faire une fluid sim 3D generale.  
Le but est de fournir une source de verite gameplay stable et reseau-compatible.

### Decision de haut niveau

Le systeme sera separe en 2 couches :

1. **Gameplay autoritaire serveur**
- hydraulique discrete compartimentee
- deterministe et tunable
- source de verite pour flooding, pression, pompes, masse, confinement

2. **Visuel futur client-side**
- non autoritaire
- branchable plus tard sur les etats gameplay repliques
- direction retenue : `heightfield 2D / shallow-water local par compartiment`

### Decisions fixees

Les points suivants sont fixes avant implementation :

- la pression interne relaxe simplement dans le temps
- une porte fermee est parfaitement etanche dans `Proto04D` initial
- la locomotion joueur s'appuie sur `WaterHeightCm`
- le HUD peut continuer d'utiliser `FloodLevel01`
- les combinaisons modifient au minimum :
  - tolerance pression
  - nage / deplacement sous eau
- la pression est modelisee explicitement comme :
  - `InternalPressure`
  - `ExternalPressure`
  - `PressureDelta`

### Ce que Proto04D n'est pas

- pas une fluid sim volumique 3D gameplay
- pas une replication de voxels/particules/grilles fluides
- pas encore la couche visuelle riche
- pas encore le centre de masse d'eau detaille avec trim/roll fin
- pas encore le pipeline complet de combinaisons/consommables/equipements

---

## 1. Etat des lieux

Proto04C a deja :

- degats -> breches
- portes runtime et confinement branche
- flooding de base
- feedback visuel et audio

Ce qui manque encore :

- pression gameplay robuste
- debit exterieur/interieur pilote par pression
- debit inter-compartiments pilote par hauteur/pression
- pompes raccordees a ce meme modele
- masse totale raccordee proprement a l'eau simulee
- interface stable pour la future eau visuelle

---

## 2. Contrat runtime a preserver

- le serveur reste seule autorite de simulation
- `USubHullComponent` reste le coeur de la sim structurelle/eau
- les portes restent la primitive de confinement
- la source de verite ne depend jamais d'un VFX ou d'une surface visuelle
- la masse d'eau derivee du systeme hydraulique est la seule masse de flooding exposee au movement
- la future couche visuelle lit les etats, mais ne modifie jamais le gameplay

---

## 3. Structures cibles

## 3.1 Etat compartiment

Chaque compartiment runtime doit pouvoir porter au minimum :

- `CompartmentId`
- `CapacityLiters`
- `CurrentWaterLiters`
- `WaterLevelNormalized`
- `WaterHeightCm`
- `FreeAirLiters`
- `InternalPressureKPa`
- `ExternalReferencePressureKPa`
- `PressureDeltaKPa`
- `FloodRateInLitersPerSec`
- `FloodRateOutLitersPerSec`
- `PumpRateOutLitersPerSec`
- `bPumpActive`
- `bFullyFlooded`
- `bPressureCritical`

## 3.2 Connexions internes

Chaque connexion hydraulique interne devra pouvoir porter :

- `ConnectionId`
- `CompartmentA`
- `CompartmentB`
- `ConnectionType`
- `OpenAreaCm2`
- `FlowResistance`
- `bOpen`
- `bSealed`

Dans `Proto04D` initial, une porte fermee signifie debit nul.

## 3.3 Breches externes

Chaque breche externe doit fournir au minimum :

- `SheetId`
- `CompartmentId`
- `OpenAreaCm2`
- `InscribedRadiusCm`
- `LocalCenter`
- `LocalNormal`
- `ExteriorDepthMeters`
- `ExteriorPressureKPa`

---

## 4. Variables gameplay principales

### Pression exterieure

Point de depart recommande :

- `ExteriorPressureAtm = 1.0 + DepthMeters / 10.0`

ou equivalent `kPa`.

### Pression interne

La pression interne :

- ne commute pas instantanement
- relaxe vers l'exterieur selon l'ouverture et l'etat du compartiment
- devient proche de l'exterieur quand le compartiment est franchement en communication avec l'ocean

### Debit exterieur -> compartiment

Le debit entrant doit augmenter avec :

- l'aire de breche
- le delta de pression
- le coefficient global de flooding

### Debit compartiment -> compartiment

Le debit entre compartiments doit dependre de :

- difference de hauteur d'eau
- difference de pression
- ouverture effective de la connexion

### Masse

`TotalFloodedMassKg` doit etre :

- la somme de l'eau de tous les compartiments
- la seule valeur de flooding lue par le movement

---

## 5. Ordre d'attaque recommande

Ordre d'implementation recommande :

1. `D.1` etats compartiment / pression
2. `D.2` pression exterieure et relaxation interne
3. `D.3` debit breches -> compartiments
4. `D.4` debit compartiments -> compartiments
5. `D.5` pompes
6. `D.6` masse -> movement
7. `D.7` locomotion / pression joueur
8. `D.8` contrat visuel futur

Raison :

- il faut d'abord stabiliser la source de verite hydraulique
- ensuite seulement raccorder movement et joueur
- enfin figer l'interface vers la future eau visuelle

---

## D.1 — Refonte des Etats de Compartiment

### Objectif

Etendre les etats de compartiment pour porter explicitement eau, hauteur, pression et debits.

### Fichiers cibles

- `Source/Sub3D/Submarine/StructuralHullTypes.h`
- `Source/Sub3D/Submarine/SubHullComponent.h`
- `Source/Sub3D/Submarine/SubHullComponent.cpp`

### Specification

Faire evoluer `FCompartmentRuntimeState` pour qu'il supporte :

- `CurrentWaterLiters`
- `WaterLevelNormalized`
- `WaterHeightCm`
- `InternalPressureKPa`
- `ExternalReferencePressureKPa`
- `PressureDeltaKPa`
- `FloodRateInLitersPerSec`
- `FloodRateOutLitersPerSec`
- `PumpRateOutLitersPerSec`
- flags d'etat utiles

`ExportCompartmentStates()` devra continuer de produire un payload lisible pour UI/HUD, avec au minimum :

- `FloodLevel01`
- masse/volume d'eau
- flags critiques

### Invariants machine

- un compartiment sec a `CurrentWaterLiters = 0`
- un compartiment plein a `CurrentWaterLiters <= CapacityLiters`
- `WaterHeightCm` est monotone avec `CurrentWaterLiters`
- `PressureDeltaKPa = ExternalReferencePressureKPa - InternalPressureKPa`

### Validations editeur

- debug log / debug HUD montrent hauteur d'eau, pression interne, delta de pression

### Non-objectifs

- pas encore de centre de masse d'eau par compartiment

---

## D.2 — Pression Exterieure et Relaxation Interne

### Objectif

Introduire un modele simple et stable `interne vs externe`.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubHullComponent.h`
- `Source/Sub3D/Submarine/SubHullComponent.cpp`

### Specification

Ajouter :

- calcul pression exterieure depuis profondeur
- calcul d'une pression cible par compartiment
- relaxation interne vers cette cible

La relaxation sera :

- simple
- stable
- dependante du degre d'ouverture vers l'exterieur

Parametres editor-exposed recommandes :

- `ExteriorPressurePer10mAtm`
- `InternalPressureRelaxationRate`
- `NominalHabitablePressureAtm`

### Invariants machine

- plus la profondeur augmente, plus la pression exterieure augmente
- un compartiment franchement ouvert vers l'exterieur tend vers la pression exterieure
- un compartiment isole tend vers la pression nominale ou y reste proche selon son etat

### Validations editeur

- a profondeur forte, une breche provoque une hausse lisible de pression interne

### Non-objectifs

- pas de thermodynamique complexe
- pas de modele gaz avance

---

## D.3 — Debits Breche -> Compartiment

### Objectif

Remplacer l'inflow actuel par un debit pilote par aire et delta de pression.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubHullComponent.cpp`

### Specification

Chaque breche ouverte vers l'exterieur injecte un debit qui depend au minimum de :

- `OpenAreaCm2`
- `PressureDeltaKPa`
- coefficient global de flooding

Editor parameters recommandes :

- `GlobalFloodInflowCoeff`
- `MaxFloodInflowLitersPerSecPerBreach`
- `DepthFloodDifficultyScale`

### Invariants machine

- une meme breche inonde plus vite a grande profondeur qu'a faible profondeur
- une breche plus large inonde plus vite qu'une breche plus petite
- sans breche ouverte vers l'exterieur, il n'y a pas d'inflow exterieur

### Validations editeur

- comparer meme breche a 10 m puis a 100 m : le compartiment monte plus vite a 100 m

### Non-objectifs

- pas de formule CFD precise

---

## D.4 — Debits Compartiment -> Compartiment

### Objectif

Faire des portes ouvertes de vraies connexions hydrauliques.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubHullComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineCompartmentComponent.*` si besoin de debug/export

### Specification

Pour chaque connexion interne ouverte :

- calculer un debit selon :
  - difference de hauteur d'eau
  - difference de pression
  - resistance de connexion

Pour une porte fermee :

- debit nul

Le systeme doit tendre vers un equilibre stable, sans oscillations explosives.

### Invariants machine

- une porte fermee coupe totalement le debit
- une porte ouverte permet un transfert
- si deux compartiments communicants restent ouverts assez longtemps, ils tendent vers un etat d'equilibre

### Validations editeur

- inonder A
- ouvrir vers B
- verifier transfert
- refermer
- verifier independance hydraulique

### Non-objectifs

- pas de fuite de porte fermee dans Proto04D initial

---

## D.5 — Pompes sur le Meme Modele

### Objectif

Brancher les pompes sur le meme systeme hydraulique, sans bypass.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubHullComponent.cpp`
- `Source/Sub3D/Submarine/SubmarineSystemsComponent.*`

### Specification

Une pompe active retire un debit connu du compartiment cible :

- `PumpRateOutLitersPerSec`
- borne par l'eau disponible

Le pompage doit se combiner naturellement avec :

- inflow exterieur
- transferts inter-compartiments

### Invariants machine

- une pompe ne peut pas retirer plus d'eau qu'il n'y en a
- une pompe active reduit bien la montee nette d'eau si sa capacite le permet

### Validations editeur

- breche + pompe active dans le compartiment : verifier ralentissement ou inversion du flooding

### Non-objectifs

- pas encore de cout energie detaille

---

## D.6 — Masse Totale et Movement

### Objectif

Recabler le movement sur la masse d'eau reelle issue de la sim hydraulique.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubHullComponent.cpp`
- `Source/Sub3D/Submarine/SubMovementComponent.cpp`

### Specification

`TotalFloodedMassKg` doit etre derive uniquement de :

- somme des volumes d'eau compartiment

Le movement lira cette valeur unique.

Priorite Proto04D :

- stabilite
- lisibilite
- comportement reseau propre

Pas encore :

- centre de masse d'eau longitudinal
- trim avance
- moments de roulis fins

### Invariants machine

- plus d'eau embarquee => masse plus grande
- masse derivee identique a la somme des compartiments

### Validations editeur

- logs movement montrent que la masse suit exactement l'etat des compartiments

### Non-objectifs

- pas encore de dynamique avancée de trim/roll

---

## D.7 — Effets Joueur : Pression, Eau, Nage

### Objectif

Brancher la nouvelle sim sur l'equipage.

### Fichiers cibles

- `Source/Sub3D/Submarine/SubCrewCharacter.*`
- composants de gameplay lies au joueur/equipement
- `SubHull` ou systeme equipement selon architecture existante

### Specification

Le joueur doit recevoir :

1. **effet pression**
- seuil selon combinaison
- temps de grace
- dommage ou mort si exposition trop longue

2. **effet eau**
- locomotion degradee selon `WaterHeightCm`
- passage en nage au-dessus d'un seuil de hauteur

Les combinaisons doivent modifier au minimum :

- tolerance pression
- comportement sous eau

### Invariants machine

- sans combinaison adequate, la pression tue apres delai
- une meilleure combinaison augmente la tolerance
- `WaterHeightCm` pilote la locomotion reelle

### Validations editeur

- joueur sans combinaison : sortie dans eau profonde -> mort apres courte exposition
- joueur en compartiment noye : locomotion degradee puis nage

### Non-objectifs

- pas encore de systeme medical detaille

---

## D.8 — Contrat pour la Future Eau Visuelle

### Objectif

FigER le contrat entre la sim gameplay et la future couche visuelle.

### Fichiers cibles

- documents d'architecture
- payloads d'etat repliques / delegates

### Specification

Le systeme gameplay devra fournir au minimum, par compartiment :

- `FloodLevel01`
- `WaterHeightCm`
- `InternalPressureKPa`
- `PressureDeltaKPa`
- `FloodRateInLitersPerSec`
- `PumpRateOutLitersPerSec`
- etats de porte / ouverture

La future couche visuelle pourra en deduire :

- niveau de surface
- slosh
- turbulence
- agitation pres des breches

Direction technique retenue :

- **heightfield 2D / shallow-water local par compartiment**

Options explicitement non retenues comme source de verite gameplay :

- SPH
- FLIP
- voxel fluid gameplay
- Navier-Stokes gameplay

### Invariants machine

- un changement futur de rendu ne doit pas casser le gameplay

### Validations editeur

- documenter le payload consomme par la future couche visuelle

### Non-objectifs

- pas d'implementation visuelle riche dans Proto04D

---

## 6. Tests et validation

### Tests machine minimaux

- compartiment sec / plein : invariants eau/hauteur valides
- porte fermee : aucun transfert
- porte ouverte : transfert positif
- grande profondeur : inflow plus fort
- masse totale = somme des compartiments
- pompe active : reduit le gain net d'eau

### Validations PIE

1. briser compartiment avant a faible profondeur
2. reproduire a grande profondeur
3. ouvrir/fermer une porte entre compartiments
4. verifier l'egalisation
5. activer une pompe
6. observer la masse et le comportement movement
7. tester joueur avec et sans combinaison

---

## 7. Risques

### Risque 1

Surcomplexifier la pression.

Reponse :

- garder un modele simple de relaxation interne/externe

### Risque 2

Creer une sim gameplay instable numeriquement.

Reponse :

- debits clamps
- pas de solveur exotique
- tests machine sur equilibre

### Risque 3

Melanger trop vite rendu et gameplay.

Reponse :

- D.8 borne explicitement le contrat et exclut la dependance visuelle

---

## 8. Definition of Done Proto04D

`Proto04D` est considere termine quand :

- l'eau est simulee par compartiments avec hauteur/pression/debit
- les breches inondent selon profondeur et pression
- les portes ouvertes permettent transfert, les portes fermees l'annulent
- les pompes utilisent le meme modele
- la masse totale d'eau est raccordee proprement au movement
- le joueur subit bien pression et changement de locomotion
- le contrat vers la future couche visuelle est explicitement fixe

