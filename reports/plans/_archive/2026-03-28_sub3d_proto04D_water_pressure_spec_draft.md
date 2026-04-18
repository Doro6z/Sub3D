# Proto04D — Spec Draft : Water, Pressure, Confinement, Future Visual Fluid

Projet : Sub3D  
Date : 2026-03-28  
Status : Draft de cadrage technique  
Scope : definir la prochaine couche de simulation eau/pression apres Proto04C  
Parent : `2026-03-27_sub3d_proto04C_spec_technique.md`

---

## 0. Intention

Le prochain vrai saut de qualite n'est plus la porte, la breche, ni le feedback.  
Le prochain vrai probleme systeme est la simulation eau/pression.

Le but de `Proto04D` n'est pas de faire une "vraie fluid sim" generale.  
Le but est de produire une simulation :

- autoritaire serveur
- deterministe et tunable
- coherent avec les compartiments et les portes
- exploitable par le movement, les pompes, les degats et la pression sur les joueurs
- branchable plus tard a une couche visuelle plus riche cote client

Decision directrice :

- la simulation gameplay restera **compartimentee / hydraulique / discrete**
- la future simulation visuelle pourra etre **client-side**, plus riche, non autoritaire

---

## 1. Constats sur l'etat actuel

Proto04C a valide la boucle minimale :

- degats -> breches
- breches -> flooding
- flooding -> feedback visuel / audio
- portes -> confinement logique branche

Mais l'etat actuel reste limite :

- l'eau est encore representee comme un `water plane` de compartiment
- la masse/flood/buoyancy sont encore trop simples
- la pression n'est pas encore un systeme gameplay complet
- le passage de l'eau entre compartiments doit devenir plus physique et mieux tune
- la future visualisation d'eau n'est pas encore decidee techniquement

---

## 2. Objectifs Proto04D

### 2.1 Gameplay serveur

Construire une simulation d'eau/pression compartimentee avec :

- hauteur d'eau par compartiment
- volume d'eau par compartiment
- pression exterieure liee a la profondeur
- pression interne de compartiment
- debit de fuite via breches selon pression et ouverture
- debit inter-compartiments via portes ouvertes
- confinement total ou quasi total si porte fermee
- pompes branchees proprement sur cette base
- masse totale derivee uniquement de l'eau reelle simulee

### 2.2 Gameplay joueur

Supporter proprement :

- sortie du sous-marin dans l'eau, sans combinaison : survie courte puis mort
- exposition d'un joueur a un compartiment sous trop forte pression
- variations de resistance selon type de combinaison
- degradation de deplacement selon niveau d'eau
- passage progressif course -> marche lourde -> nage

### 2.3 Architecture future

Preparer explicitement la future couche visuelle :

- surface d'eau plus credible
- agitation locale
- slosh / oscillation / tilt
- turbulence pres des breches
- sans faire dependre le gameplay du rendu

---

## 3. Contraintes d'architecture

### 3.1 A conserver

- le serveur est la seule autorite
- la simulation de gameplay ne depend jamais d'une simulation visuelle
- les portes et compartiments restent les primitives principales
- le mouvement du sous-marin doit consommer une masse d'eau derivee de l'etat hydraulique, pas une estimation parallele non raccordee

### 3.2 A eviter

- full fluid sim 3D serveur
- simulation non deterministe type particules gameplay
- replication massive de voxels, particules, grilles fluides
- couplage entre VFX et quantites gameplay

### 3.3 Contrat reseau cible

Le reseau doit transporter :

- etats compacts de compartiment
- etats de porte
- etats de breche utiles
- pression / eau / flood compactes

Il ne doit pas transporter :

- particules
- maillages d'eau
- surfaces d'onde de haute resolution
- solveur fluide voxel ou particulaire

---

## 4. Modele de simulation recommande

## 4.1 Niveau de simulation gameplay recommande

Recommendation principale :

**modele hydraulique par compartiment + connexions + pression**

Le sous-marin est traite comme un graphe de volumes relies :

- noeuds : compartiments
- aretes internes : portes / passages / ouvertures
- aretes externes : breches vers l'ocean

Chaque tick serveur, on met a jour :

1. etat des breches
2. pression exterieure locale
3. pression interne des compartiments
4. debits exterieur -> compartiment
5. debits compartiment -> compartiment
6. pompage
7. volume d'eau, hauteur d'eau, masse totale

Ce n'est pas une fluid sim volumique complete.  
C'est un modele hydraulique ferme, discret, stable et tunable.

---

## 4.2 Etat minimum par compartiment

Chaque compartiment devra porter au minimum :

- `CompartmentId`
- `CapacityLiters`
- `CurrentWaterLiters`
- `WaterLevel01`
- `FreeAirLiters`
- `InternalPressureKPa`
- `FloodRateInLitersPerSec`
- `FloodRateOutLitersPerSec`
- `PumpRateOutLitersPerSec`
- `bPumpActive`
- `bFullyFlooded`
- `bPressureCritical`

Variables derivees :

- `WaterMassKg = CurrentWaterLiters`
- `WaterHeightCm`
- `AirFraction01`
- `PressureDeltaVsExterior`

---

## 4.3 Etat minimum par connexion interne

Chaque connexion entre compartiments devra porter :

- `ConnectionId`
- `CompartmentA`
- `CompartmentB`
- `ConnectionType`
- `OpenAreaCm2`
- `bOpen`
- `bSealed`
- `FlowResistance`

Sources :

- une porte ouverte = connexion active
- une porte fermee = debit nul ou tres proche de zero
- une ouverture structurelle libre = connexion permanente

---

## 4.4 Etat minimum par breche externe

Chaque breche externe devra porter :

- `SheetId`
- `CompartmentId`
- `OpenAreaCm2`
- `InscribedRadiusCm`
- `LocalCenter`
- `LocalNormal`
- `ExteriorDepthMeters`
- `ExteriorPressureKPa`

---

## 5. Pression — cadrage gameplay

## 5.1 Pression exterieure

La pression exterieure derive de la profondeur.

Version gameplay recommandee :

- formule simple, monotone, stable
- pas besoin de haute precision scientifique

Base :

- `ExteriorPressureAtm = 1.0 + DepthMeters / 10.0`

Ou equivalent en kPa.

Le parametre de difficulte pourra ensuite scaler :

- vitesse d'inondation
- tolerance de pression joueur
- violence des ecoulements

---

## 5.2 Pression interne de compartiment

Decision recommandee :

la pression interne n'est pas binaire.  
Elle evolue selon l'etat d'ouverture vers l'exterieur et le ratio air/eau.

Modele simple recommande pour Proto04D :

- compartiment sain et sec : pression habitable nominale
- compartiment avec breche ouverte et eau entrante : la pression interne tend vers la pression exterieure
- compartiment completement inonde : pression interne ~= pression exterieure
- compartiment isole et presque sec : pression interne revient vers nominale seulement si le design veut autoriser cette relaxation

Decision de design a fixer :

- soit la pression interne suit un modele "quasi instantane"
- soit elle suit un modele relaxe dans le temps

Recommendation :

- **Proto04D** : relaxation simple et stable, pas instantanee pure
- cela laisse le temps au joueur de lire le danger

Decision fixee :

- `Proto04D` utilisera bien une **relaxation simple dans le temps**
- la pression sera explicitement modelisee comme un couple :
  - `pression interne`
  - `pression externe`
- le gameplay raisonnera ensuite sur leur ecart :
  - `PressureDelta = ExternalPressure - InternalPressure`

---

## 5.3 Pression et joueur

Le systeme pression doit couvrir 2 cas.

### Cas A — Joueur qui sort du sous-marin

Un joueur qui quitte le sous-marin et se retrouve dans l'eau :

- sans combinaison : quelques secondes de survie, puis mort
- avec combinaison legere : resistance profondeur faible
- avec combinaison lourde : resistance profondeur plus grande

Le danger n'est pas seulement "etre dans l'eau".  
Le danger est l'exposition a une pression externe superieure a la tolerance de l'equipement.

### Cas B — Joueur dans un compartiment sous pression

Un joueur dans un compartiment ferme qui monte vers une pression trop forte :

- subit un timer d'exposition
- peut survivre quelques secondes autour du seuil
- meurt si la pression reste au-dessus de sa tolerance

Variables gameplay recommandees par personnage/equipement :

- `MaxSafePressureAtm`
- `PressureGraceSeconds`
- `PressureDamagePerSecond`
- `bCanOperateFlooded`
- `SwimCapability`

Recommendation :

- pression = systeme de seuil + exposition dans le temps
- pas un kill instantane brut des que le seuil est depasse

Decision fixee :

- les combinaisons modifieront au minimum :
  - la tolerance a la pression
  - le comportement en nage / deplacement sous eau

---

## 6. Inondation et egalisation

## 6.1 Breche exterieure -> compartiment

La pression exterieure doit directement piloter la vitesse d'inondation.

Qualitativement :

- plus la profondeur est grande
- plus l'ouverture est large
- plus le debit entrant est fort

La "difficulty parameter" pourra scaler :

- coefficient global d'inflow
- severite de pression
- degradation equipage

Recommendation de formule gameplay :

- une formule simple de type :
  - debit = `BreachArea * PressureDelta * GlobalFloodCoeff`
- avec clamping et easing

Le but n'est pas la precision CFD.  
Le but est :

- monotonicite
- stabilite numerique
- tuning simple

---

## 6.2 Compartiment -> compartiment

Si un compartiment contient de l'eau et qu'une porte s'ouvre :

- l'eau doit couler vers le compartiment voisin
- jusqu'a un etat d'equilibre ou quasi-equilibre

Cet equilibre depend de :

- la hauteur d'eau des deux compartiments
- la pression des deux compartiments
- l'ouverture effective de la porte

Si la porte se referme :

- les 2 compartiments redeviennent hydrauliquement independants

Decision recommandee :

- les transferts inter-compartiments se font au niveau "connexion"
- pas besoin de simuler des paquets d'eau volumetriques

---

## 6.3 Porte fermee

Une porte fermee doit etre un confinement reel.

Modele recommande :

- par defaut : debit nul
- plus tard eventuellement :
  - fuite mineure si porte endommagee
  - etancheite non parfaite selon type de porte

Mais pas dans Proto04D initial.

---

## 6.4 Pompes

Les pompes doivent etre branchees sur ce meme modele.

Elles retirent un debit connu depuis un compartiment cible :

- `PumpRateOutLitersPerSec`
- consomme energie plus tard
- peut avoir un rendement degrade sous certaines pressions plus tard

Regle cle :

- pas de "pompe magique" hors simulation
- la pompe retire de l'eau d'un vrai volume de compartiment

---

## 7. Eau et deplacement joueur

La hauteur d'eau doit aussi piloter le locomotion state.

Recommendation initiale :

- `Flood < 0.20`
  - deplacement quasi normal
  - tres legere penalite
- `Flood >= 0.20 && Flood < 0.50`
  - diminution legere a moderee de vitesse
  - bruit / resistance visuelle
- `Flood >= 0.50 && Flood < SwimThreshold`
  - diminution nette
  - transitions plus lourdes
- `Flood >= SwimThreshold`
  - joueur passe en mode nage

Le `SwimThreshold` ne doit pas etre un simple pourcentage arbitraire si on peut l'eviter.  
Idealement, il doit dependre de la hauteur d'eau par rapport au capsule/player root.

Recommendation :

- garder `FloodLevel01` pour le HUD
- utiliser `WaterHeightCm` pour la locomotion reelle

---

## 8. Masse et flottabilite

Le sous-marin ne doit plus "sembler recevoir une force script arbitraire".

Objectif de Proto04D :

- la masse d'eau embarquee devient la seule source de `FloodedMassKg`
- `FloodedMassKg` alimente ensuite clairement le movement/buoyancy

Principes :

- `TotalFloodedMassKg = somme(CurrentWaterLiters de tous les compartiments)`
- la repartition longitudinale pourra plus tard influer sur trim/pitch
- dans Proto04D initial, la priorite est d'abord :
  - masse totale coherente
  - reponse stable
  - pas de saccades reseau

Pass suivant possible apres Proto04D initial :

- centre de masse de l'eau
- moment de tangage
- moment de roulis

---

## 9. Quelle technique utiliser plus tard pour la simulation visuelle ?

Avant de fixer Proto04D, il faut trancher la future couche visuelle.

Important :

- la technique visuelle future ne doit pas dicter la simulation gameplay
- elle doit consommer un etat derive de la sim gameplay

---

## 9.1 Comparatif des options

### Option A — Heightfield 2D simple

Definition :

- une surface d'eau 2D par compartiment
- eventuellement deformee par agitation et slosh

Avantages :

- tres peu couteux
- simple a authorer
- tres stable en reseau car purement client-side
- parfait pour une eau "surface libre" dans un compartiment

Limites :

- ne represente pas bien les volumes complexes pleins
- pas adapte a une salle entierement submergee
- pas adapte a des jets/recirculations 3D riches

Verdict :

- **excellent candidat pour Proto04D/04E visuel**
- pas suffisant comme seule solution a long terme si on veut de l'eau totalement volumique

---

### Option B — Heightfield 2D plus riche / shallow-water local

Definition :

- surface 2D par compartiment
- solveur leger de vagues / propagation / slosh

Avantages :

- beaucoup plus credible qu'un plane statique
- toujours relativement peu couteux
- bon fit pour compartiments partiellement inondes
- facile a piloter avec :
  - hauteur
  - turbulence
  - impulsion lors d'ouverture de porte
  - mouvement du sub

Limites :

- pas une vraie eau 3D
- ne gere pas bien un compartiment totalement noye

Verdict :

- **meilleure recommandation visuelle court/moyen terme**

---

### Option C — SPH

Definition :

- particules fluides independantes

Avantages :

- look "liquide" intuitif
- tres bien pour petits volumes libres ou splashs locaux

Limites :

- couteux
- difficile a rendre stable
- peu adapte au reseau gameplay
- mauvaise idee comme source de verite gameplay

Verdict :

- **non recommande comme base gameplay**
- utile seulement pour VFX locaux tres limites

---

### Option D — FLIP / PIC-FLIP

Definition :

- solveur hybride grille + particules

Avantages :

- qualite visuelle elevee
- bon pour grosses masses d'eau et splashs

Limites :

- tres cher
- tres lourd a integrer pour un interieur de sous-marin reseau
- tres mauvaise source de gameplay serveur

Verdict :

- **a exclure pour la source de verite gameplay**
- eventuellement utile seulement dans un contexte cinematique/offline, pas ici

---

### Option E — Voxel fluid / grid volumique 3D

Definition :

- volume 3D discret plein de cellules d'eau

Avantages :

- plus lisible que SPH/FLIP pour du gameplay structure
- peut bien epouser les volumes fermes
- potentiellement utile pour l'interieur du sous-marin

Limites :

- toujours cher
- replication impossible en brut
- beaucoup de tuning / authoring
- risque fort de faire doublon avec la sim compartimentee

Verdict :

- **possible plus tard comme couche visuelle locale**
- pas comme fondation Proto04D

---

### Option F — Grid Navier-Stokes / CFD lite

Definition :

- simulation fluide 3D basee equations de grille

Avantages :

- physiquement riche

Limites :

- beaucoup trop lourde pour l'objectif
- faible rendement gameplay
- integration reseau hostile

Verdict :

- **a exclure**

---

## 9.2 Recommendation finale

Recommendation tranchee :

### Gameplay serveur

Utiliser :

- **modele hydraulique par compartiment + connexions + pression**

### Visuel client-side futur

Utiliser :

- **surface libre par compartiment**
- idealement **heightfield 2D / shallow-water local**
- plus :
  - Niagara pour leaks / jets / mist
  - decal / mousse / turbulence pres des breches
  - agitation pilotee par l'etat gameplay

Donc :

- pas de SPH gameplay
- pas de FLIP gameplay
- pas de voxel gameplay
- pas de Navier-Stokes gameplay

Le meilleur compromis pour Sub3D est :

- **gameplay discret**
- **visuel continu**

---

## 10. Architecture cible en 2 couches

### Couche A — Sim gameplay autoritaire

Source de verite serveur :

- compartiments
- portes
- breches
- eau
- pression
- pompes
- masse

Sorties :

- `CurrentWaterLiters`
- `WaterHeightCm`
- `FloodLevel01`
- `InternalPressureKPa`
- `FloodRate`
- `PumpRate`
- `TotalFloodedMassKg`

### Couche B — Sim visuelle client-side

Consomme :

- etats compacts repliques
- transformations du sous-marin
- events de breach / porte / pompe

Produit :

- surface eau credible
- agitation / turbulence
- courants locaux suggeres
- rendu plus riche

Le gameplay ne lit jamais cette couche.

---

## 11. Proposition de decoupage Proto04D

### D.1 — Refonte des etats de compartiment

Ajouter :

- volume eau
- hauteur eau
- pression interne
- debits in/out

### D.2 — Pression exterieure et pressure model

Ajouter :

- profondeur -> pression
- relaxation de pression interne
- deltas de pression

### D.3 — Debits breche -> compartiment

Rebrancher l'inflow sur :

- aire de breche
- delta de pression
- coefficient global de difficulte

### D.4 — Debits compartiment -> compartiment

Modeliser les portes ouvertes comme connexions hydrauliques :

- debit selon hauteur/pression
- confinement total si porte fermee

### D.5 — Pompes

Pompes connectees au meme modele.

### D.6 — Masse et movement

Le movement consomme la masse d'eau derivee du systeme hydraulique.

### D.7 — Effets joueur

Brancher :

- pression sur joueur
- survivabilite selon combinaison
- penalite locomotion selon hauteur eau

### D.8 — Contrat visuel futur

Definir l'API entre :

- etats gameplay repliques
- future surface d'eau dynamique client-side

---

## 12. Invariants cibles

- une porte fermee coupe totalement le debit inter-compartiments
- une porte ouverte permet un debit vers un equilibre stable
- une breche a grande profondeur inonde plus vite qu'a faible profondeur
- la masse totale d'eau est egale a la somme des compartiments
- la pression tue un joueur seulement apres depassement de seuil + temps d'exposition
- changer de combinaison modifie la tolerance sans reauthorer toute la sim
- la visualisation future peut changer sans invalider la sim gameplay

---

## 13. Risques et garde-fous

### Risque 1 — Surcomplexifier la pression

Garde-fou :

- Proto04D doit rester gameplay-first
- pas de thermodynamique complexe

### Risque 2 — Faire une fluid sim reseau implicite

Garde-fou :

- ne jamais faire reposer le gameplay sur une sim particulaire/volumique client

### Risque 3 — Coupler trop vite movement et hydraulique

Garde-fou :

- d'abord stabiliser masse totale
- ensuite seulement centre de masse et moments

### Risque 4 — Choisir une technique visuelle trop lourde

Garde-fou :

- prendre `heightfield/shallow-water local` comme direction par defaut

---

## 14. Decision de cadrage

Decision recommandee pour la suite :

- considerer `Proto04C` comme base fonctionnelle
- ouvrir `Proto04D = Water & Pressure`
- implementer d'abord la **sim hydraulique gameplay serveur**
- reserver la **sim visuelle plus riche** a une couche client-side ulterieure

Decision technique recommande :

- **source de verite gameplay** = graphe hydraulique compartimente
- **future visualisation** = heightfield 2D / shallow-water local par compartiment

---

## 15. Questions a fixer avant implementation

1. La pression interne doit-elle relaxer lentement ou tendre quasi instantanement vers l'exterieur ?
2. Une porte fermee est-elle parfaitement etanche en Proto04D, oui ou non ?
3. Le passage nage doit-il etre base sur `FloodLevel01` ou `WaterHeightCm` reel du compartiment ?
4. Les combinaisons modifient-elles seulement la pression, ou aussi la nage et les courants ?
5. La pression doit-elle etre conceptualisee plus tard comme un simple etat "habitable" ou comme une opposition explicite `interne vs externe` ?

Decisions fixees :

1. relaxation simple dans le temps
2. oui, etanche parfaite en Proto04D initial
3. `WaterHeightCm` pour locomotion, `FloodLevel01` pour HUD
4. oui, au moins pression + nage
5. `interne vs externe`

Implication de la decision 5 :

- le systeme de pression ne sera pas code comme un simple flag de securite
- il devra porter explicitement :
  - `InternalPressure`
  - `ExternalPressure`
  - `PressureDelta`
- la lethality joueur, les debits d'inondation et les futurs equipements liront ce modele
