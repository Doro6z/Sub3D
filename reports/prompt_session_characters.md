# Prompt — Session Personnages Sub3D

Colle ce prompt au début d'une nouvelle conversation Claude Code.

---

## Contexte

Je travaille sur Sub3D, un jeu de simulation sous-marin en Unreal Engine 5.7. Le projet est à `C:\Dev\Sub3D`. Le CLAUDE.md à la racine contient l'architecture complète.

Je suis sur le milestone First Playable. Le blockout du sous-marin est fait (mesh Blender, pas encore importé dans UE). Maintenant je dois mettre en place les personnages d'équipage.

## Décision prise

- **Rig** : Utiliser le skeleton du mannequin UE5 (SK_Mannequin) comme base de rig
- **Mesh custom** : OUI — créer les body meshes via script Blender (comme le sous-marin), basés sur les dimensions exactes du mannequin UE5. Pas du mannequin UE directement, mais des meshes custom qui utilisent son squelette.
- **Pipeline** : Script Blender génère les meshes → rigging sur le skeleton UE5 → import UE
- **Différenciation par rôle** :  accessoires modulaires (hats, beards, gear)
- **Animation** : procédurale via Control Rig + animations de base (idle/walk/run)


## Rôles d'équipage

5 rôles minimum pour le FP :
1. **Capitaine** — casquette, uniforme sombre
2. **Timonier** (helmsman) — casque léger, uniforme standard
3. **Mécanicien** (engineer) — casque/casquette, combinaison, outils
4. **Sonariste** — casque audio, uniforme technique
5. **Matelot** (crew) — bonnet, uniforme basique

## Ce que j'attends de cette session

### Phase 0 — Recherche dimensions mannequin UE5
- Extraire les dimensions EXACTES du mannequin UE5 (Manny/Quinn) :
  - Proportions du body : hauteur totale, largeur épaules, tour de taille, longueur bras/jambes
  - Positions des joints du skeleton : head, neck, spine, pelvis, shoulders, elbows, wrists, knees, ankles
  - Taille de la tête, des mains, des pieds
  - Socket positions (head, hands, pelvis)
- Ces dimensions deviennent les CONSTANTES du script Blender (comme HULL_CURVE pour le sous-marin)

### Phase 1 — Script Blender : Body Generator
- Même approche que le sous-marin : script Python Blender qui génère les meshes
- Dimensions calées sur le skeleton UE5 (joints aux bons endroits pour le skinning)
- Base body paramétrique (taille, corpulence, proportions) → générer 4 variants :
  - SM_Body_Crew1
  - SM_Body_Crew2
  - SM_Body_Crew3
  - SM_Body_Crew4
- Le mesh doit être compatible avec le skeleton UE5 pour le rigging

### Phase 2 — Script Blender : Accessoires (5 variants chaque)
- SM_Hat_x5 : casquette capitaine, bonnet, casque, béret, casque audio
- SM_Face_x5 : neutre, sérieux, inquiet, déterminé, fatigué (geometry-based expression)
- SM_Beard_x5 : rasé, barbe courte, barbe complète, moustache, bouc
- SM_Gear_x5 : ceinture outils, holster radio, gilet sauvetage, tablier mécanicien, harnais
- Tous dimensionnés pour s'emboîter sur le body via les socket positions du skeleton UE5

### Phase 3 — Rigging & Import
- Comment rigger les body meshes sur le skeleton UE5 dans Blender (workflow)
- Export FBX avec skeleton compatible
- Import dans UE5 avec retargeting
- Vérifier que les animations du mannequin fonctionnent sur les meshes custom

### Phase 3 — Animation procédurale
- Control Rig setup sur le mannequin
- Couches d'animation :
  - Base : idle/walk/run (du starter content ou Mixamo)
  - Procédural : look-at (regard vers une cible), head tracking
  - Procédural : lean/brace (compensation du mouvement du sous-marin via SubCrewMovementComponent)
  - Procédural : interaction stations (mains qui se posent sur les consoles via IK)
- Le SubCrewMovementComponent a déjà :
  - `BraceProbeDistanceCm = 90` (détection murs proches)
  - `BraceProbeHeightOffsetCm = 70`
  - `SupportQuality01` (qualité d'appui)
  - Transition walk → wade → swim basée sur immersion

### Phase 4 — Intégration gameplay
- Connecter le personnage au système de flood (walk→swim transition)
- Connecter aux stations (snap to station, play station anim)
- Connecter aux portes (interaction component existe déjà sur SubDoorActor)

## Contraintes techniques

- UE 5.7, C++ modules : Sub3D (Runtime), Sub3DCore, Sub3DRuntime
- Le crew character existe : `ASubCrewCharacter` dans `Source/Sub3D/Submarine/SubCrewCharacter.h`
- Le movement component existe : `USubCrewMovementComponent`
- Capsule : 88cm half-height, 42cm radius
- Eye height : 70cm au-dessus du root
- Collision channel : ECC_GameTraceChannel2 (SubInterior)
- Les stations sont : helm, engine, ballast (via `USubmarineStationManagerComponent`)

## Fichiers clés à lire

- `Source/Sub3D/Submarine/SubCrewCharacter.h/cpp`
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h/cpp`
- `Source/Sub3D/Submarine/SubmarineBase.h/cpp`
- `CLAUDE.md` (racine du projet)

## Référence : session précédente (sous-marin)

Dans la session blockout sous-marin, on a développé un pipeline de scripts Blender Python qui :
- Génère de la géométrie procédurale paramétrique
- Utilise des courbes de profil (spline cubique) pour les formes lisses
- Produit des meshes modulaires (coque, decks, bulkheads, props séparément)
- Exporte en FBX pour UE5

Les scripts sont dans `Source/scripts/hull_blockout/`. Le même pattern doit être réutilisé pour les personnages : un script `character_kit.py` qui génère les body variants et accessoires, avec les dimensions du mannequin UE5 comme constantes de référence.

## Ce que je ne veux PAS

- Pas de MetaHuman (trop lourd pour FP)
- Pas de marketplace assets (je veux contrôler le pipeline)
- Pas de tutoriels basiques sur le rigging/animation — je sais faire
- Pas de refactor du SubCrewCharacter existant sans raison
- Pas de mannequin UE5 utilisé tel quel comme mesh final — je veux des meshes custom qui utilisent son SKELETON
