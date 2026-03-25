# Sub3D Proto 03 - Guide Blender Debutant FR

Date: 2026-03-20  
Projet: `C:/Dev/Sub3D`

## 1. But

Ce guide sert a construire le **blockout jouable** du premier sous-marin dans Blender, meme si tu es debutant.

Il ne sert pas a faire un beau modele final.  
Il sert a obtenir un volume propre, a la bonne echelle, exportable dans Unreal pour tester:

- la circulation
- la lisibilite
- les proportions
- la place des stations

## 2. Definitions

`Blockout` : version tres simple du modele, faite avec des formes basiques.

`Viewport` : la vue 3D de Blender.

`Orthographic` : vue sans perspective, utile pour mesurer et aligner.

`Outliner` : panneau en haut a droite qui liste les objets.

`Collection` : dossier logique pour ranger les objets.

`Object Mode` : mode pour deplacer ou dupliquer un objet entier.

`Edit Mode` : mode pour modifier la forme d’un objet.

`Face` : surface d’un mesh.

`Edge` : arete entre deux points.

`Vertex` : point d’un mesh.

`Inset` : creer une bordure interieure sur une face.

`Extrude` : tirer une face pour creer du volume.

`Bevel` : casser un angle trop dur.

`Pivot` : point d’origine de l’objet.

## 3. Ce Que Tu Vas Construire

Tu vas construire seulement ceci:

- un volume interieur principal
- une salle de pilotage
- un couloir principal
- une alcove couchette/rangement
- une salle moteur/pompe
- deux ouvertures de bulkhead
- une coque exterieure tres simple
- 4 gros blocs de gameplay

Tu ne fais pas encore:

- details fins
- boulons
- cables modeles a la main
- textures finales
- belles fenetres complexes

## 4. Cibles De Dimensions

Travaille avec ces dimensions:

- Longueur exterieure: `14.8 m`
- Largeur exterieure: `5.2 m`
- Hauteur exterieure: `4.8 m`
- Longueur interieure utile: `12.2 m`
- Largeur utile max: `3.6 m`
- Hauteur interieure claire: `2.35 m`
- Couloir clair: `2.3 m`
- Helm room profondeur: `2.8 m`
- Couloir principal longueur: `5.8 m`
- Engine room profondeur: `3.2 m`
- Alcove laterale: `2.0 m x 2.2 m`

## 5. Preparation De Blender

### Etape 1 - Ouvrir un nouveau fichier propre

1. Lance Blender.
2. Clique sur `General`.
3. Dans la vue 3D, selectionne le cube par defaut.
4. Appuie sur `X`.
5. Clique `Delete`.

### Etape 2 - Regler l’unite

1. En bas a droite, clique l’onglet `Scene Properties`.
2. Ouvre la section `Units`.
3. Regle:
   - `Unit System` = `Metric`
   - `Unit Scale` = `1.000`
   - `Length` = `Meters`

### Etape 3 - Sauvegarder le fichier

1. Clique `File > Save As`.
2. Sauvegarde par exemple dans ton dossier de travail sous:
   - `Sub_Proto03_Blockout_A.blend`

## 6. Organisation Minimale

### Etape 4 - Creer les collections

1. Dans l’`Outliner` en haut a droite, clic droit sur `Scene Collection`.
2. Clique `New Collection`.
3. Cree ces collections:
   - `COL_Sub_Proto03_Blockout`
   - `COL_Sub_Proto03_Hull`
   - `COL_Sub_Proto03_Interior`
   - `COL_Sub_Proto03_Stations`
   - `COL_Sub_Proto03_Reference`

## 7. Navigation De Base A Connaitre

### Raccourcis utiles

- tourner autour: `clic molette maintenu`
- deplacer la vue: `Shift + clic molette`
- zoom: `molette`
- vue face: `Numpad 1`
- vue cote: `Numpad 3`
- vue dessus: `Numpad 7`
- orthographic/perspective: `Numpad 5`
- deplacer: `G`
- rotation: `R`
- scale: `S`
- annuler: `Ctrl + Z`
- object mode / edit mode: `Tab`

Si tu n’as pas de pavé numérique:

1. Va dans `Edit > Preferences`.
2. Clique `Input`.
3. Coche `Emulate Numpad`.

## 8. Creer Le Sol Interieur

### Etape 5 - Ajouter un cube de base

1. Clique dans la vue 3D.
2. Appuie sur `Shift + A`.
3. Clique `Mesh > Cube`.
4. Dans l’`Outliner`, renomme l’objet:
   - `SM_Sub_Proto03_InteriorShell_A`
5. Glisse-le dans la collection `COL_Sub_Proto03_Interior`.

### Etape 6 - Donner la bonne taille de base

1. Avec l’objet selectionne, appuie sur `N` pour ouvrir le panneau lateral si besoin.
2. Dans l’onglet `Item`, trouve `Dimensions`.
3. Entre:
   - `X = 12.2`
   - `Y = 3.6`
   - `Z = 2.35`

Important:

- `X` = longueur
- `Y` = largeur
- `Z` = hauteur

### Etape 7 - Poser le volume sur le sol

1. Toujours dans `Item > Location`, mets:
   - `Z = 1.175`

Pourquoi:

- la hauteur est `2.35`
- si le centre est a `1.175`, la base touche `Z = 0`

## 9. Creer La Forme Interieure Simple

### Etape 8 - Passer en mode edition

1. Selectionne `SM_Sub_Proto03_InteriorShell_A`.
2. Appuie sur `Tab`.
3. Tu es en `Edit Mode`.

### Etape 9 - Activer la selection de faces

1. En haut a gauche de la vue, clique l’icone `Face Select`.
2. Elle ressemble a un petit carre.

### Etape 10 - Creer la salle de pilotage arrondie

But:

- le sous-marin ne doit pas etre un simple rectangle
- l’avant doit etre un peu plus doux et plus lisible

1. Passe en vue dessus avec `Numpad 7`.
2. Selectionne la face avant.
3. Appuie sur `I` pour `Inset`.
4. Bouge legerement la souris vers l’interieur et clique.
5. Appuie sur `E` pour `Extrude`.
6. Deplace legerement vers l’avant sur `X`.
7. Recommence une ou deux fois avec petites valeurs.

Si tu preferes une methode plus simple:

1. Laisse la face avant plate pour l’instant.
2. On fera l’avant plus joli sur la coque exterieure, pas sur l’interieur.

Pour un debutant, cette option est souvent meilleure.

## 10. Decouper Les Zones Interieures

### Etape 11 - Ajouter des coupes

1. Appuie sur `Ctrl + R`.
2. Survole le mesh pour voir une ligne violette.
3. Clique une fois.
4. Bouge la coupe.
5. Clique encore pour la valider.

Fais des coupes dans la longueur pour marquer:

- fin helm room vers `X avant`
- fin corridor
- debut engine room

Ne cherche pas la perfection geometrique. Le but est juste de marquer les zones.

## 11. Creer L’Alcove Laterale

### Methode simple pour debutant

1. Passe en vue dessus: `Numpad 7`
2. En `Edit Mode`, selectionne une face laterale au milieu du volume.
3. Appuie sur `E`.
4. Tire cette face vers l’exterieur sur l’axe `Y`.

Dimension cible:

- profondeur laterale environ `2.0 m`
- longueur utile environ `2.2 m`

Si l’extrusion prend toute la longueur:

1. Annule avec `Ctrl + Z`.
2. Utilise `Ctrl + R` pour creer deux coupes autour de la zone.
3. Selectionne seulement la face centrale entre ces coupes.
4. Extrude ensuite.

## 12. Creer Les Ouvertures Des Bulkheads

### Etape 12 - Marquer les ouvertures

Tu ne fais pas encore les portes detaillees.  
Tu crees juste l’espace ou elles iront.

1. Passe en vue face ou cote selon ce qui est le plus lisible.
2. En `Edit Mode`, selectionne la face interieure de la cloison.
3. Appuie sur `I` pour `Inset`.
4. Cree une zone plus petite.
5. Ajuste pour viser environ:
   - largeur `1.2 m`
   - hauteur `2.1 m`
6. Appuie sur `X`.
7. Clique `Faces`.

Fais cela pour:

- Bulkhead A entre helm et corridor
- Bulkhead B entre corridor et engine/service

## 13. Creer Une Coque Exterieure Simple

### Etape 13 - Dupliquer l’interieur

1. Reviens en `Object Mode` avec `Tab`.
2. Selectionne `SM_Sub_Proto03_InteriorShell_A`.
3. Appuie sur `Shift + D`.
4. Clique pour confirmer.
5. Renomme la copie:
   - `SM_Sub_Proto03_Hull_A`
6. Glisse-la dans `COL_Sub_Proto03_Hull`.

### Etape 14 - Elargir la coque

1. Selectionne `SM_Sub_Proto03_Hull_A`.
2. Appuie sur `S`, puis `Y`.
3. Agrandis un peu.
4. Appuie sur `S`, puis `Z`.
5. Agrandis un peu.
6. Appuie sur `S`, puis `X`.
7. Agrandis un peu.

Objectif:

- la coque exterieure doit envelopper l’interieur
- elle ne doit pas le traverser

### Etape 15 - Rendre la coque plus credibile

Option simple:

1. En `Edit Mode`, selectionne quelques aretes exterieures.
2. Appuie sur `Ctrl + B` pour `Bevel`.
3. Fais de petits chanfreins.

Ne passe pas du temps sur les details.  
Le but est juste d’eviter l’effet “boite pure”.

## 14. Creer Les Grosse Masses De Gameplay

### Etape 16 - Helm console

1. `Shift + A > Mesh > Cube`
2. Renomme:
   - `SM_HelmConsole_A`
3. Mets les dimensions:
   - `X = 0.9`
   - `Y = 2.4`
   - `Z = 1.15`
4. Place-le dans la helm room, contre l’avant.

### Etape 17 - Engine block

1. `Shift + A > Mesh > Cube`
2. Renomme:
   - `SM_EngineBlock_A`
3. Mets les dimensions:
   - `X = 1.8`
   - `Y = 1.2`
   - `Z = 1.6`
4. Place-le dans la salle moteur.

Laisse au moins `1.2 m` de degagement sur une face.

### Etape 18 - Pump console

1. `Shift + A > Mesh > Cube`
2. Renomme:
   - `SM_PumpConsole_A`
3. Mets les dimensions:
   - `X = 1.2`
   - `Y = 0.7`
   - `Z = 1.2`
4. Place-le dans la zone moteur, mais distinct du bloc moteur.

### Etape 19 - Repair panel

1. `Shift + A > Mesh > Cube`
2. Renomme:
   - `SM_RepairPanel_A`
3. Mets les dimensions:
   - `X = 0.15`
   - `Y = 0.8`
   - `Z = 1.2`
4. Colle-le a un mur lisible du couloir ou de la salle moteur.

## 15. Placement Debutant Recommande

Tu peux travailler tres simplement:

- Helm console tout a l’avant
- Couloir vide au centre
- Alcove laterale au milieu
- Engine block a l’arriere d’un cote
- Pump console a l’arriere de l’autre cote
- Repair panel sur le mur du fond

Si tu gardes ca, tu resteras dans une bonne zone de lisibilite.

## 16. Verifier Les Volumes

### Etape 20 - Controle visuel rapide

Mets-toi en vue dessus `Numpad 7` et verifie:

- le couloir principal reste clair
- l’alcove ne mange pas le passage
- les stations sont bien distinguees
- il n’y a pas d’objet au milieu du chemin principal

### Etape 21 - Controle hauteur

Passe en vue cote `Numpad 3` et verifie:

- pas de console plus haute que necessaire
- pas de plafond trop bas
- pas de forme bizarre qui casserait la circulation

## 17. Ce Qu’il Ne Faut Pas Faire Maintenant

- ne modele pas de boulons
- ne fais pas de topologie propre pour film
- ne detaille pas les cables a la main
- ne fais pas 12 pieces differentes de porte
- ne fais pas des fenetres panoramiques
- ne fais pas de gelee decorative partout

Ton objectif est seulement:

- bon volume
- bonnes proportions
- bonne lisibilite

## 18. Export FBX Vers Unreal

### Etape 22 - Appliquer les transforms

Pour chaque objet important:

1. Selectionne l’objet.
2. Appuie sur `Ctrl + A`.
3. Clique `All Transforms`.

Fais-le pour:

- `SM_Sub_Proto03_InteriorShell_A`
- `SM_Sub_Proto03_Hull_A`
- stations principales

### Etape 23 - Export

1. Selectionne les objets a exporter.
2. Clique `File > Export > FBX (.fbx)`.
3. Dans la colonne de droite:
   - coche `Selected Objects`
   - `Forward` = `-Z Forward`
   - `Up` = `Y Up`
4. Choisis un dossier.
5. Clique `Export FBX`.

## 19. Verification

Le blockout est suffisant si:

- tu comprends immediatement ou est le helm
- le couloir est lisible sans explication
- la salle moteur se distingue du reste
- l’alcove donne une sensation d’habitat
- la coque exterieure semble entourer un vrai interieur

Si un de ces points ne marche pas, ne detaille pas. Corrige le blockout d’abord.

## 20. Reponse Attendue Pour Continuer

Quand tu as fait cette premiere passe, reviens avec:

- une capture vue dessus
- une capture vue cote
- une capture 3/4
- et dis juste:

`Blockout passe 1 termine. Analyse-le.`
