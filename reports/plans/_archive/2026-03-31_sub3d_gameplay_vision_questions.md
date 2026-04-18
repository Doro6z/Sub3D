# Sub3D - Questions de Vision Gameplay

Date: 2026-03-31
Contexte: Decisions a prendre pour informer l'implementation de stabilisation

Ces questions doivent etre tranchees par le directeur gameplay/creative.
Chaque question impacte directement le code a ecrire.

---

## A. PHYSIQUE DU SOUS-MARIN

### A1. Inertie et masse ressentie

Le sous-marin actuel (BaseMass = 200 000 kg) a un feeling relativement agile grace au DragX custom tres bas (0.045 * 0.05 = 0.00225 effectif en mouvement). Il glisse longtemps apres un thrust.

**Question**: Quel feeling vise-t-on ?
- a) **Paquebot**: inertie massive, le sub met 5-10 secondes a s'arreter, virages larges. Le joueur doit anticiper beaucoup. Tension = freinage/engagement.
- b) **Torpilleur**: reponse rapide, le sub tourne vite et freine vite. Le joueur peut manoeuvrer dans des tunnels etroits. Tension = reflexes/precision.
- c) **Hybride actuel**: glisse longtemps en ligne droite mais ralentit vite en-dessous de 120 cm/s (IdleDampingSpeedThreshold). Compromis entre les deux.

Réponse : **Realiste (au-dela de (a))**. BaseMass = 2 200 000 kg. Le sous-marin est un vrai sous-marin nucleaire. Inertie massive, virages larges, anticipation requise. Le joueur gere la puissance moteur (levier thrust W/S), pas la vitesse directement. La vitesse est une consequence de la puissance appliquee, du drag, et de la masse. Systeme de **stabilisation/dampeners** : auto-speed, auto-depth, auto-pitch, avec checkboxes par systeme et toggle master. L'input joueur override temporairement le mode auto. Le rudder n'a aucun effet a l'arret — autorite proportionnelle a la vitesse, pivot au stern (realiste). Pas de rotation sur place.

**Impact code**: Les coefficients DragCoefficients, LateralSpeedDamping, IdleDampingSpeedThreshold, MaxThrust, ContactVelocityDamping doivent etre coherents avec la reponse. BaseMass doit passer a 2 200 000. Implementer le modele thrust→force→velocity (pas de vitesse directe). Implementer le systeme stabilisation/dampeners dans SubmarineSystemsComponent. Implementer l'autorite rudder proportionnelle a la vitesse.

### A2. Vitesse maximale du sous-marin

Actuellement: MaxForwardSpeed = 650 cm/s (23.4 km/h), MaxReverseSpeed = 250 cm/s.

**Question**: Est-ce que ces vitesses sont bonnes pour la taille des tunnels et la longueur des segments ?
- Si le sub fait 35m de long et que les tunnels font 20-50m de diametre, 650 cm/s est-il trop rapide pour la lisibilite ?
- Faut-il un rapport entre taille du sub et vitesse max ?

Réponse : Cruise = 600-750 cm/s (12-15 noeuds). Flank (max) = 1250 cm/s (25 noeuds). Le sub fait 35m de long, tunnels 20-50m de diametre. La vitesse est adaptee — le joueur controle la puissance, pas la vitesse directement. Jauge de puissance + vitesse reconstruite affichee sur le helm UI. Le rapport taille/vitesse est correct pour les tunnels.

### A3. Pitch et hydroplanes

Le sub monte/descend via les hydroplanes (DivePlaneInput). Le pitch max est 30 degres.

**Question**: Le pitch doit-il etre visible depuis l'interieur ?
- a) **Oui, full pitch**: le joueur voit le sol du sub s'incliner. Immersion forte mais peut causer le mal de mer en VR/FPS.
- b) **Pitch attenue visuellement**: le sub pitch physiquement mais la camera compense partiellement (ex: 50% du pitch). Compromis.
- c) **Pitch masque**: la camera reste parfaitement horizontale par rapport au sol du sub. Le joueur ne "sent" pas le pitch visuellement.

Réponse : **(a) Full pitch visible**. Le joueur voit le sol s'incliner. L'immersion prime. Les hydroplanes sont des commandes simples (up/down), le pitch resulte du modele physique. En mode stabilisation auto-pitch actif, le sub maintient l'assiette automatiquement. L'input joueur override temporairement.

**Impact code**: Determine directement l'implementation de la camera en Phase 4. Option (a) = pas de compensation pitch. La camera suit le referentiel du sous-marin completement. Le systeme de stabilisation auto-pitch doit etre implemente pour eviter l'inconfort quand le joueur ne pilote pas activement.

### A4. Collision murale et degats

Actuellement: quand le sub touche un mur (monde), `ContactVelocityDamping = 8.0` amortit la vitesse et `OnHullHit` applique des degats via SubHull.

**Question**: Quel est le gameplay voulu pour les collisions murales ?
- a) **Punitive**: degats importants, le joueur doit eviter les murs a tout prix.
- b) **Informative**: degats faibles, la collision sert surtout de feedback ("tu as touche"). Le sub glisse le long du mur.
- c) **Critique a haute vitesse seulement**: degats proportionnels a la vitesse. A basse vitesse, le sub peut frotter sans dommage. A haute vitesse, c'est catastrophique.

Réponse : **(c) Critique a haute vitesse seulement**. Degats proportionnels a la vitesse. A basse vitesse, le sub peut frotter les murs sans dommage significatif. A haute vitesse (flank), les collisions sont catastrophiques. Seuil de vitesse minimum pour les degats.

**Impact code**: Determine le scaling de `HullImpactDamageScale`, l'ajout potentiel d'un seuil de vitesse minimum pour les degats, et le feedback associe (ecran shake, alarme, etc.). Implementer une courbe de degats speed-dependent avec seuil minimum.

### A5. Ballast gameplay ou automatique

Les ballasts controlent la flottabilite (montee/descente passive). Actuellement: 2 tanks (avant/arriere), pump rate, fill level.

**Question**: Le joueur gere-t-il les ballasts directement ?
- a) **Manuel**: le joueur a des commandes explicites pour remplir/vider chaque tank. Gameplay de sous-marinier.
- b) **Semi-auto**: un bouton "plongee" et un bouton "surface" gerent tout. Les ballasts sont un systeme interne.
- c) **Full auto**: les ballasts s'ajustent automatiquement pour maintenir la profondeur voulue. Pas d'interaction directe.

Réponse : **Hybride manuel/auto via dampeners**. Les ballasts sont controlables manuellement (commandes individuelles avant/arriere) mais le systeme de stabilisation/dampeners peut les gerer automatiquement (auto-depth). Le joueur a des checkboxes par systeme (auto-speed, auto-depth, auto-pitch) + un toggle master stabilisation. L'input manuel override temporairement le mode auto.

**Impact code**: Expose les commandes ballast dans l'UI helm ET implemente l'auto-depth dans le systeme dampeners de SubmarineSystemsComponent. Les deux modes coexistent.

---

## B. CHARACTER ET VUE

### B1. FPS pure ou FPS/TPS hybride

Les screenshots montrent une vue FPS et une vue TPS (3eme personne).

**Question**: Quelle est la vue principale ?
- a) **FPS pure**: pas de mesh visible du character en vue normale. Le joueur voit ses mains au maximum.
- b) **FPS avec corps visible**: le joueur voit ses jambes, son corps quand il regarde en bas.
- c) **FPS/TPS toggle**: le joueur peut basculer entre FPS et TPS. Les deux doivent etre jouables.

Réponse : **(a) FPS pure**. Pas de mesh body visible en jeu. Le joueur voit ses mains au maximum. Pas de TPS. La vue TPS ne sera pas maintenue. Decision finale 2026-04-01.

**Impact code**: Pas de mesh character complet requis. Focus sur camera + mains uniquement. Le systeme TPS peut etre retire ou ignore. Simplifie Phase 6 (feel FPS).

### B2. Mouvement dans le sous-marin

**Question**: Quel repertoire de mouvement pour le crew ?
- a) **Walk only**: pas de sprint, pas de crouch, pas de jump. Sous-marin = espace confine.
- b) **Walk + Sprint**: sprint pour l'urgence (courir vers une breach). Pas de crouch/jump.
- c) **Walk + Sprint + Crouch**: crouch pour passer sous des tuyaux, dans des espaces etroits.
- d) **Full**: walk + sprint + crouch + jump. Maximum de liberte.

Réponse : **(d) Full** — walk + sprint + crouch + jump. Walk est la base. Sprint consomme de la stamina et a un cout (bruit, fatigue). Le crouch est un **systeme procedural par scroll wheel** : la molette controle la hauteur de stance en continu (de tiptoe a crawl), avec un blend d'animation procedural. Pas de crouch binaire. Le sub est assez grand pour se tenir debout dans les compartiments principaux, mais les zones confinees (bow, stern, passages techniques) necessitent de se baisser.

**Impact code**: Chaque option ajoute des animations, des etats dans le CMC, et potentiellement des interactions (crouch sous un obstacle, etc.). Le systeme de crouch procedural par scroll wheel est specifique — il faut un parametre continu de stance height dans le CMC, pas un etat binaire crouch/stand.

### B3. Interaction avec les stations

**Question**: Comment le joueur interagit-il avec une station (helm, sonar, etc.) ?
- a) **Snap en place**: le joueur s'approche et "snap" en position fixe devant la station. Movement desactive.
- b) **Libre**: le joueur reste debout et libre de bouger devant la station. Les inputs sont routes mais il peut s'eloigner.
- c) **Assise**: le joueur s'assied sur un siege. Animation sit + camera fixee. Il doit "se lever" pour partir.

Réponse : **(a) Snap en place**. Le joueur s'approche, interagit, et snap en position fixe devant la station. Movement desactive. Pour quitter, il doit "release" la station. Les chaises/sieges sont du RP/decoration uniquement — pas de systeme d'assise gameplay. Simple et fiable.

**Impact code**: Option (a) est la plus simple — disable movement, set camera. Implementer un systeme d'interaction station avec: approach trigger, snap transform, input routing, release. Pas besoin d'animations sit. Les chaises sont des props visuels sans gameplay.

### B4. Echelle du character vs sous-marin

Le sous-marin fait environ 35m de long, 5m de diametre, avec un rayon effectif de ~250 cm.
La capsule character fait ~88 cm de demi-hauteur (176 cm total).

**Question**: L'echelle actuelle est-elle correcte ?
- Le character doit-il pouvoir se tenir debout partout dans le sub ?
- Y a-t-il des zones basses ou le character doit baisser la tete ?
- Le plafond est-il assez haut pour que le joueur ne se sente pas confine de maniere inconfortable ?

Réponse : Le sub est assez grand dans les compartiments principaux pour se tenir debout. Des zones confinees existent au bow et stern, et dans les passages techniques. Le systeme de **crouch procedural par scroll wheel** (voir B2) gere cela — le joueur ajuste sa hauteur manuellement. Pas de crouch automatique force — le joueur choisit. La capsule character (176 cm) est correcte pour l'echelle actuelle (rayon effectif ~250 cm).

**Impact code**: Le systeme de crouch procedural (B2) couvre le besoin. Pas besoin de ceiling detection automatique pour le first playable. Eventuellement, un feedback visuel/sonore quand la tete est proche du plafond.

---

## C. ENVELOPPE ET VISUEL

### C1. Style visuel de la coque

**Question**: Quel style pour la coque exterieure ?
- a) **Lisse et propre**: surfaces courbes, peu de details geometriques. Style sci-fi clean.
- b) **Industriel**: plaques, rivets, soudures visibles. Style realiste sous-marin.
- c) **Modulaire visible**: on voit les compartiments depuis l'exterieur (lignes de separation). Architecture lisible.

Réponse : **(b) Industriel**. Plaques, rivets, soudures visibles. Style realiste sous-marin. L'aspect visuel se fait principalement en material/texture, pas en geometrie procedurale. Le mesh generator n'a pas besoin de subdivisions supplementaires pour ce style.

**Impact code**: (b) se fait en material/texture. La geometrie procedurale actuelle est suffisante. L'effort est cote shader/material, pas cote SubmarineGeometryBuilder.

### C2. Visibilite interieur depuis l'exterieur

**Question**: Doit-on voir l'interieur du sous-marin depuis l'exterieur ?
- a) **Non, jamais**: la coque est opaque. L'interieur est un espace ferme.
- b) **Oui, via des hublots**: certaines zones ont des vitres.
- c) **Transparent en mode debug seulement**: opaque en jeu, transparent en editor.

Réponse : **(b) Oui, via des hublots/fenetres**. Certaines zones ont des vitres — le joueur peut voir dehors depuis l'interieur et inversement. Ajoute de l'immersion et du feedback visuel sur l'environnement exterieur.

**Impact code**: Il faut gerer la transparence et le material des hublots. Les murs interieurs doivent etre rendus meme quand la camera est exterieure (visibilite bidirectionnelle). Le geometry builder doit prevoir des emplacements pour les hublots dans les anneaux de coque.

### C3. Separations visuelles entre compartiments

**Question**: Les compartiments sont-ils visuellement distincts ?
- a) **Oui, cloisons epaisses**: les bulkheads sont des murs solides visibles.
- b) **Subtil**: lignes au sol, changement de couleur/eclairage entre compartiments.
- c) **Seamless**: l'interieur est un espace continu, les compartiments sont une notion logique seulement.

Réponse : **(a) Cloisons epaisses**. Les bulkheads sont des murs solides visibles. Les portes sont **fixes au compile time** — definies dans le LayoutAsset (DoorDef + BulkheadSheetId), compilees une fois, pas modifiables en jeu. Decision finale 2026-04-01 : pas de placement dynamique de porte en runtime pour le FP.

**Impact code**: (a) est deja implemente via AppendBoxPrism. Le contrat DoorDef existant dans SubmarineLayoutAsset est suffisant. Pas de CSG runtime, pas de mesh remeshing dynamique. Le placement dynamique de porte est une feature post-FP.

---

## D. COOP ET MULTI-JOUEUR

### D1. Combien de joueurs maximum dans un sous-marin ?

**Question**: Combien de CrewSpawnSocket faut-il prevoir ?
- 1 joueur (solo)
- 2-4 joueurs (coop classique)
- 5+ joueurs (coop large)

Réponse : **5+ joueurs (coop large)**. Le reseau doit supporter 1 a 16 joueurs. Le first playable cible 4 joueurs. Le sous-marin doit avoir assez de stations et d'espace pour que 5+ joueurs aient chacun quelque chose a faire.

**Impact code**: Determine le nombre de sockets a generer, le nombre de stations, la taille minimale des compartiments pour que N joueurs tiennent dedans. Prevoir au minimum 4-5 CrewSpawnSockets, extensible a 16. Le network doit etre pense pour 16 des le depart.

### D2. Roles specialises ou polyvalents

**Question**: Chaque joueur a-t-il un role fixe (pilote, sonar operator, ingenieur) ou tous peuvent tout faire ?
- a) **Roles fixes**: le pilote pilote, l'operateur sonar scanne, l'ingenieur repare.
- b) **Polyvalent**: n'importe qui peut prendre n'importe quelle station.
- c) **Progression**: commence polyvalent, se specialise avec l'experience/equipement.

Réponse : **Hybride (a)+(b)** — Roles fixes avec flexibilite. Systeme d'assignation de role (pilote, operateur sonar, ingenieur, etc.) mais n'importe qui PEUT prendre n'importe quelle station en cas d'urgence. Les roles donnent des bonus/efficacite sur la station assignee. Le role assignment est un systeme a implementer.

**Impact code**: Il faut un systeme de role assignment (quel joueur est assigne a quel role). N'importe qui peut TakeHelm (systeme actuel OK pour le base case) mais les roles ajoutent un layer de permissions/bonus. Pour le first playable, le systeme polyvalent actuel suffit — le role assignment est post-FP.

---

## FORMAT DE REPONSE ATTENDU

Pour chaque question, repondre:
- La lettre choisie (a, b, c, d)
- Eventuellement un commentaire court sur le "pourquoi"
- Si "je ne sais pas encore", indiquer la priorite de la decision (bloquant / peut attendre)

Les reponses A1-A5 et B1-B4 sont les plus urgentes pour l'implementation.
Les reponses C1-C3 et D1-D2 peuvent attendre la fin de la stabilisation.
