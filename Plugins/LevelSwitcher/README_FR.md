# Level Switcher

> Outil editeur pour la navigation rapide entre niveaux dans Unreal Editor

## Vue d'ensemble
Level Switcher est un outil editeur concu pour ameliorer l'ergonomie et reduire le temps de navigation dans les projets multi-maps.
Il scanne le projet pour lister tous les niveaux et les centralise dans un panneau dedie avec favoris et niveau de demarrage.
Acces via Window > Level Switcher ou l'icone de la toolbar.

## Fonctionnalites principales
- Scanner et lister tous les niveaux (assets UWorld) dans un panneau unique
- Rechercher les niveaux par nom ou chemin
- Favoris avec reorganisation par glisser-deposer
- Niveau de demarrage (menu deroulant, glisser-deposer, ou menu contextuel)
- Actions contextuelles : niveau de demarrage, favoris, ouvrir dans Content Browser, copier la reference, reveler dans l'Explorateur (Windows)

## Installation
### Option 1 : Fab
1. Installer depuis Fab
2. Activer dans Edit > Plugins > Level Switcher
3. Redemarrer Unreal Editor

### Option 2 : Manuelle
1. Copier le dossier `LevelSwitcher` dans `VotreProjet/Plugins`
2. Activer dans Edit > Plugins > Level Switcher
3. Redemarrer Unreal Editor

## Demarrage rapide
1. Ouvrir Window > Level Switcher (ou utiliser l'icone toolbar)
2. Double-cliquer un niveau pour l'ouvrir
3. Cliquer l'etoile pour ajouter/retirer des favoris
4. Utiliser le menu Startup ou le clic droit pour definir le niveau de demarrage

## Specifications techniques
- Moteur : Unreal Engine 5.0+
- Type : Editeur uniquement (zero surcharge runtime)
- Plateforme : Win64
- Dependances : Aucune

## Contenu inclus
- Outil editeur Level Switcher
- Documentation (README)
- Code source (C++)

## Documentation
- Docs/LevelSwitcher_UserGuide.md

## Changelog
### v1.0.0 - YYYY-MM-DD
- Publication initiale
