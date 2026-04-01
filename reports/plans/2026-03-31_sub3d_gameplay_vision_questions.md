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

Réponse : 

**Impact code**: Les coefficients DragCoefficients, LateralSpeedDamping, IdleDampingSpeedThreshold, MaxThrust, ContactVelocityDamping doivent etre coherents avec la reponse.

### A2. Vitesse maximale du sous-marin

Actuellement: MaxForwardSpeed = 650 cm/s (23.4 km/h), MaxReverseSpeed = 250 cm/s.

**Question**: Est-ce que ces vitesses sont bonnes pour la taille des tunnels et la longueur des segments ?
- Si le sub fait 35m de long et que les tunnels font 20-50m de diametre, 650 cm/s est-il trop rapide pour la lisibilite ?
- Faut-il un rapport entre taille du sub et vitesse max ?

Réponse : 

### A3. Pitch et hydroplanes

Le sub monte/descend via les hydroplanes (DivePlaneInput). Le pitch max est 30 degres.

**Question**: Le pitch doit-il etre visible depuis l'interieur ?
- a) **Oui, full pitch**: le joueur voit le sol du sub s'incliner. Immersion forte mais peut causer le mal de mer en VR/FPS.
- b) **Pitch attenue visuellement**: le sub pitch physiquement mais la camera compense partiellement (ex: 50% du pitch). Compromis.
- c) **Pitch masque**: la camera reste parfaitement horizontale par rapport au sol du sub. Le joueur ne "sent" pas le pitch visuellement.

Réponse : 

**Impact code**: Determine directement l'implementation de la camera en Phase 4. Option (a) = pas de compensation pitch. Option (c) = compensation complete. Option (b) = interpolation partielle.

### A4. Collision murale et degats

Actuellement: quand le sub touche un mur (monde), `ContactVelocityDamping = 8.0` amortit la vitesse et `OnHullHit` applique des degats via SubHull.

**Question**: Quel est le gameplay voulu pour les collisions murales ?
- a) **Punitive**: degats importants, le joueur doit eviter les murs a tout prix.
- b) **Informative**: degats faibles, la collision sert surtout de feedback ("tu as touche"). Le sub glisse le long du mur.
- c) **Critique a haute vitesse seulement**: degats proportionnels a la vitesse. A basse vitesse, le sub peut frotter sans dommage. A haute vitesse, c'est catastrophique.

Réponse : 

**Impact code**: Determine le scaling de `HullImpactDamageScale`, l'ajout potentiel d'un seuil de vitesse minimum pour les degats, et le feedback associe (ecran shake, alarme, etc.).

### A5. Ballast gameplay ou automatique

Les ballasts controlent la flottabilite (montee/descente passive). Actuellement: 2 tanks (avant/arriere), pump rate, fill level.

**Question**: Le joueur gere-t-il les ballasts directement ?
- a) **Manuel**: le joueur a des commandes explicites pour remplir/vider chaque tank. Gameplay de sous-marinier.
- b) **Semi-auto**: un bouton "plongee" et un bouton "surface" gerent tout. Les ballasts sont un systeme interne.
- c) **Full auto**: les ballasts s'ajustent automatiquement pour maintenir la profondeur voulue. Pas d'interaction directe.

Réponse : 

**Impact code**: Determine si on expose les commandes ballast individuelles dans l'UI helm ou si on les cache derriere un systeme automatique dans SubmarineSystemsComponent.

---

## B. CHARACTER ET VUE

### B1. FPS pure ou FPS/TPS hybride

Les screenshots montrent une vue FPS et une vue TPS (3eme personne).

**Question**: Quelle est la vue principale ?
- a) **FPS pure**: pas de mesh visible du character en vue normale. Le joueur voit ses mains au maximum.
- b) **FPS avec corps visible**: le joueur voit ses jambes, son corps quand il regarde en bas.
- c) **FPS/TPS toggle**: le joueur peut basculer entre FPS et TPS. Les deux doivent etre jouables.

Réponse : 

**Impact code**: Si TPS, il faut un mesh character complet + animations idle/walk/run/interact. Si FPS pure, on peut se concentrer sur la camera et les mains. Si toggle, les deux doivent etre maintenus.

### B2. Mouvement dans le sous-marin

**Question**: Quel repertoire de mouvement pour le crew ?
- a) **Walk only**: pas de sprint, pas de crouch, pas de jump. Sous-marin = espace confine.
- b) **Walk + Sprint**: sprint pour l'urgence (courir vers une breach). Pas de crouch/jump.
- c) **Walk + Sprint + Crouch**: crouch pour passer sous des tuyaux, dans des espaces etroits.
- d) **Full**: walk + sprint + crouch + jump. Maximum de liberte.

Réponse : 

**Impact code**: Chaque option ajoute des animations, des etats dans le CMC, et potentiellement des interactions (crouch sous un obstacle, etc.).

### B3. Interaction avec les stations

**Question**: Comment le joueur interagit-il avec une station (helm, sonar, etc.) ?
- a) **Snap en place**: le joueur s'approche et "snap" en position fixe devant la station. Movement desactive.
- b) **Libre**: le joueur reste debout et libre de bouger devant la station. Les inputs sont routes mais il peut s'eloigner.
- c) **Assise**: le joueur s'assied sur un siege. Animation sit + camera fixee. Il doit "se lever" pour partir.

Réponse : 

**Impact code**: Option (a) est la plus simple — disable movement, set camera. Option (c) est la plus immersive mais demande des animations + un systeme de siege. Option (b) est entre les deux mais peut causer des bugs de collision quand le joueur bouge en mode station.

### B4. Echelle du character vs sous-marin

Le sous-marin fait environ 35m de long, 5m de diametre, avec un rayon effectif de ~250 cm.
La capsule character fait ~88 cm de demi-hauteur (176 cm total).

**Question**: L'echelle actuelle est-elle correcte ?
- Le character doit-il pouvoir se tenir debout partout dans le sub ?
- Y a-t-il des zones basses ou le character doit baisser la tete ?
- Le plafond est-il assez haut pour que le joueur ne se sente pas confine de maniere inconfortable ?

Réponse : 

**Impact code**: Si des zones basses existent, il faut un systeme de crouch automatique ou un ceiling detection. Si le plafond est uniformement haut, on peut simplifier.

---

## C. ENVELOPPE ET VISUEL

### C1. Style visuel de la coque

**Question**: Quel style pour la coque exterieure ?
- a) **Lisse et propre**: surfaces courbes, peu de details geometriques. Style sci-fi clean.
- b) **Industriel**: plaques, rivets, soudures visibles. Style realiste sous-marin.
- c) **Modulaire visible**: on voit les compartiments depuis l'exterieur (lignes de separation). Architecture lisible.

Réponse : 

**Impact code**: (a) demande plus de subdivisions dans le mesh generator. (b) peut se faire en material/texture. (c) impacte la generation des rings (variation de rayon entre compartiments).

### C2. Visibilite interieur depuis l'exterieur

**Question**: Doit-on voir l'interieur du sous-marin depuis l'exterieur ?
- a) **Non, jamais**: la coque est opaque. L'interieur est un espace ferme.
- b) **Oui, via des hublots**: certaines zones ont des vitres.
- c) **Transparent en mode debug seulement**: opaque en jeu, transparent en editor.

Réponse : 

**Impact code**: Si (a), les murs interieurs n'ont pas besoin d'etre rendus quand la camera est exterieure (culling possible). Si (b), il faut gerer la transparence et le material des hublots.

### C3. Separations visuelles entre compartiments

**Question**: Les compartiments sont-ils visuellement distincts ?
- a) **Oui, cloisons epaisses**: les bulkheads sont des murs solides visibles.
- b) **Subtil**: lignes au sol, changement de couleur/eclairage entre compartiments.
- c) **Seamless**: l'interieur est un espace continu, les compartiments sont une notion logique seulement.

Réponse : 

**Impact code**: (a) est deja implemente via AppendBoxPrism pour les bulkheads. (c) demanderait de supprimer les bulkheads visuels et de ne garder que la collision pour les portes.

---

## D. COOP ET MULTI-JOUEUR

### D1. Combien de joueurs maximum dans un sous-marin ?

**Question**: Combien de CrewSpawnSocket faut-il prevoir ?
- 1 joueur (solo)
- 2-4 joueurs (coop classique)
- 5+ joueurs (coop large)

Réponse : 

**Impact code**: Determine le nombre de sockets a generer, le nombre de stations, la taille minimale des compartiments pour que N joueurs tiennent dedans.

### D2. Roles specialises ou polyvalents

**Question**: Chaque joueur a-t-il un role fixe (pilote, sonar operator, ingenieur) ou tous peuvent tout faire ?
- a) **Roles fixes**: le pilote pilote, l'operateur sonar scanne, l'ingenieur repare.
- b) **Polyvalent**: n'importe qui peut prendre n'importe quelle station.
- c) **Progression**: commence polyvalent, se specialise avec l'experience/equipement.

Réponse : 

**Impact code**: Si (a), il faut un systeme de role assignment. Si (b), le systeme actuel (n'importe qui peut TakeHelm) suffit. Si (c), il faut un systeme de skills/permissions.

---

## FORMAT DE REPONSE ATTENDU

Pour chaque question, repondre:
- La lettre choisie (a, b, c, d)
- Eventuellement un commentaire court sur le "pourquoi"
- Si "je ne sais pas encore", indiquer la priorite de la decision (bloquant / peut attendre)

Les reponses A1-A5 et B1-B4 sont les plus urgentes pour l'implementation.
Les reponses C1-C3 et D1-D2 peuvent attendre la fin de la stabilisation.
