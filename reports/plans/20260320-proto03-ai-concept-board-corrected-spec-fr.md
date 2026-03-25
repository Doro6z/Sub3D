# Sub3D Proto 03 - Planche IA Corrigee FR

Date: 2026-03-20  
Projet: `C:/Dev/Sub3D`

## 1. But

Cette specification sert a corriger la planche actuelle pour qu’elle soit utile a:

- la generation d’images IA coherentes
- la modelisation manuelle
- l’usage futur en `image-to-3d` sur des sous-elements

But principal:

- **ne pas demander une seule image fourre-tout**
- produire plusieurs planches propres, chacune avec un role clair

## 2. Verdict Sur La Planche Actuelle

La planche actuelle est bonne pour:

- valider une direction generale
- communiquer interieur + exterieur
- montrer la hierarchie des pieces

La planche actuelle n’est pas ideale pour:

- `image-to-3d` direct
- extraction de formes propres
- coherence DA finale

Problemes principaux:

- trop de contenus melanges dans une seule image
- style encore trop "capsule sci-fi propre"
- fenetres avant trop presentes
- interieur un peu trop net et neutre
- pas assez d’indices "civil industriel independant"

## 3. Ce Qu’Il Faut Corriger Visuellement

### A. Coque exterieure

Conserver:

- silhouette compacte
- logique de vrai volume interieur
- lisibilite front / rear / side

Corriger:

- rendre la coque moins "navette premium"
- ajouter des zones plus industrielles:
  - plaques d’acces
  - trappes de maintenance
  - rails
  - volumes techniques externes
- casser un peu la purete de la capsule
- garder une coque simple mais plus "workhorse"

### B. Fenetres

Conserver:

- une lecture du helm vers l’avant

Corriger:

- reduire la surface vitree
- rendre les vitrages plus epais
- preferer de petits panneaux renforcés
- donner une impression de verre couteux et protege

Regle:

- jamais de panoramique large
- jamais de nez entier en baie vitree

### C. Interieur

Conserver:

- lecture simple helm / couloir / engine room
- distinction des zones

Corriger:

- rendre le helm un peu plus dense
- rendre la zone moteur/pompe plus mecanique
- ajouter une sensation "maintenance quotidienne"
- garder des murs lisibles pour futures breches

### D. Props

Conserver:

- separation des gros props en bas de planche

Corriger:

- props plus industriels, moins lisses
- bulkhead door plus lourde
- helm console plus utilitaire
- engine block plus massif
- locker plus simple, moins sci-fi propre

## 4. Structure Corrigee De La Planche

Au lieu d’une seule planche, produis 3 planches distinctes.

## 5. Planche A - Exterieur Seulement

Contenu:

- vue cote gauche
- vue face
- vue arriere
- vue dessus
- vue 3/4

Contraintes:

- fond gris clair uniforme
- rendu clay gris
- pas d’eau
- pas de brouillard
- pas d’eclairage dramatique
- pas d’interieur visible
- pas de coupe

Direction visuelle:

- petit sous-marin d’exploration
- civil industriel independant
- compact
- pressure-rated
- rare reinforced windows
- maintenance seams
- rails and access panels
- no weapons

## 6. Planche B - Interieur Seulement

Contenu:

- top-down cutaway
- coupe laterale
- vue 3/4 interieure
- zones nommees:
  - HELM
  - MAIN CORRIDOR
  - SIDE NOOK
  - ENGINE / PUMP ROOM
  - REPAIR WALL

Contraintes:

- fond neutre
- rendu clay ou tres peu texture
- pas de personnages
- pas de decoration fine
- circulation parfaitement lisible

Direction:

- third-person navigation
- clear main spine
- one side nook only
- repair-friendly walls
- compact industrial habitat

## 7. Planche C - Props Principaux

Contenu:

- bulkhead door
- helm console
- pump console
- engine block
- repair panel
- locker
- bunk
- reinforced porthole

Contraintes:

- chaque objet isole
- meme echelle relative
- fond neutre
- vue 3/4 propre
- pas de mise en scene

## 8. Prompt Corrige - Exterieur

### FR

```text
Planche de concept propre pour production, sous-marin compact d’exploration civil industriel independant, petit equipage, coque pressurisee credible, style workhorse habitable, non militaire, non luxueux, fenetres rares petites et tres renforcees, plaques d’acces, trappes de maintenance, rails externes, capteurs et petit mat technique, silhouette compacte derivee d’un vrai interieur, vues multiples orthographiques et 3/4, fond gris clair neutre, rendu clay gris mat, sans eau, sans brouillard, sans FX, sans personnages, sans arme, sans texte decoratif, propre et lisible pour modelisation
```

### EN

```text
Clean production concept board, compact civilian industrial exploration submarine, small crew, believable pressure-rated hull, workhorse habitat-machine, not military, not luxury, rare small heavily reinforced windows, access panels, maintenance hatches, external rails, sensors and small technical mast, compact silhouette clearly derived from a real interior, multi-view orthographic and 3/4 view, neutral light gray background, matte gray clay render, no water, no fog, no FX, no characters, no weapons, no decorative text, clean and readable for modeling
```

## 9. Prompt Corrige - Interieur

### FR

```text
Planche de concept interieur de sous-marin pour production, vue top-down cutaway et coupe laterale d’un petit sous-marin d’exploration civil industriel independant, petit equipage, circulation third-person lisible, helm room, main corridor, side nook couchette rangement, engine and pump room, murs lisibles pour reparations et breches, ambiance machine-habitat compacte, fonctionnelle, vecue, non luxueuse, rendu clay gris ou tres faiblement materiau, fond neutre, sans personnages, sans FX, sans clutter excessif, plan propre et clair pour blockout 3D
```

### EN

```text
Production interior concept board, top-down cutaway and side cut of a compact civilian industrial exploration submarine, small crew, readable third-person circulation, helm room, main corridor, bunk and storage side nook, engine and pump room, readable walls for repairs and breaches, compact habitat-machine feeling, functional, lived-in, not luxurious, gray clay render or very light material indication, neutral background, no characters, no FX, no excessive clutter, clean and clear for 3D blockout
```

## 10. Prompt Corrige - Props

### FR

```text
Planche de props de production pour petit sous-marin d’exploration civil industriel independant, objets isoles sur fond neutre, meme langage visuel, bulkhead door lourde et pressurisee, helm console utilitaire, pump console, engine block compact et massif, repair panel mural, bunk simple, industrial locker, reinforced porthole epais, rendu clay gris mat, vues 3/4 propres, sans personnages, sans FX, sans eau, sans usure peinte excessive, lisible pour modelisation
```

### EN

```text
Production prop board for a compact civilian industrial exploration submarine, isolated objects on neutral background, consistent visual language, heavy pressure bulkhead door, utilitarian helm console, pump console, compact massive engine block, wall-mounted repair panel, simple bunk, industrial locker, thick reinforced porthole, matte gray clay render, clean 3/4 views, no characters, no FX, no water, no excessive painted wear, readable for modeling
```

## 11. Negative Prompt Recommande

### FR

```text
pas de style luxe, pas de yacht, pas de warship, pas de sous-marin militaire moderne, pas de cockpit panoramique, pas de grandes baies vitrees, pas de steampunk ornamental, pas de dieselpunk caricatural, pas de personnages, pas de brouillard, pas d’eau, pas d’explosion, pas de tentacules, pas de couleurs fortes, pas de logo, pas de texte, pas de fond complexe, pas de greeble spam, pas de rendu photorealiste dramatique
```

### EN

```text
no luxury style, no yacht, no warship, no modern military submarine, no panoramic cockpit, no large glass canopy, no ornamental steampunk, no caricature dieselpunk, no characters, no fog, no water, no explosions, no tentacles, no strong colors, no logos, no text, no complex background, no greeble spam, no dramatic photoreal rendering
```

## 12. Ce Qu’Il Faut Demander Au Modele Image

Demande d’abord:

- coherence
- lisibilite
- vues techniques

Ne demande pas d’abord:

- ambiance
- textures
- cinematic mood

Ordre recommande:

1. planche exterieure clay
2. planche interieure clay
3. planche props clay
4. seulement apres, une version mood paintover

## 13. Rappel Important

Pour le **sous-marin principal**, meme avec une bonne planche:

- l’image IA sert a guider
- elle ne remplace pas le blockout Blender
- elle ne remplace pas la verification Unreal

Pour `image-to-3d`, reserve plutot l’usage a:

- bulkhead door
- repair panel
- locker
- petits props secondaires

Pas au sous-marin principal entier.
