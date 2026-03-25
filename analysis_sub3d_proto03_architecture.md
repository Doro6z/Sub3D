# Analyse: C++ Refoundation Architecture Draft (Proto03)

Cette analyse technique évalue la proposition [2026-03-21_sub3d_proto03_cpp_refoundation_architecture_draft.md](file:///c:/ACC/Projects/Sub3D/Plans/sub3d/2026-03-21_sub3d_proto03_cpp_refoundation_architecture_draft.md) au regard de la doctrine `Sub3D Proto03` (Submarine-first, Single Hull, Interior-state-first).

## 1. Alignement Canonique (Points Forts)
La proposition valide l'essentiel de la doctrine de base de conception :
- **Rejet du Chaos Physics (RigidBody)** : Adoption pertinente du modèle "Simulation Plateforme Autoritaire" via un mouvement cinématique serveur.
- **Single Hull strict** : Le couplage direct entre visuel du maillage et vérité logique (`USubHullComponent`) est évité.
- **Extériorisation du poste Réparation** : Le retrait de la `RepairStation` de la liste des stations fixes vers un modèle d'interaction contextuelle est exact.
- **Serveur-Autoritaire & Prédiction** : La base réseau (15-30 Hz / interpolation) est adaptée pour la nature d'un sous-marin lourd.

---

## 2. Défauts Détectés (Flaws)

### A. La Gestion de la Pompe Active (Pump) manquante
**Problème** : L'`Execution Handoff` verrouille une victoire de crise Proto03 autour de "engine/pump lisibles" et "pompe/fermeture/repair" pour stabiliser l'unité.
**Défaut** : Le document d'architecture ne cite la pompe qu'en note annexe (12.4). `FSubmarineCommandState` et `USubmarineSystemsComponent` ne contiennent pas l'état ou le contrôle de la pompe (ex: `bPumpActive`, `ExtractionRateLiters`). La pompe est indispensable à la boucle `flooding -> stabilisation`.
**Correction requise** : Intégrer la Pompe comme système primaire du `USubmarineSystemsComponent`, activable depuis l'`EngineStation` (ou un panneau dédié), et répliquer son état d'activation.

### B. Le Poids de l'Eau (Water Mass Weight) non intégré au mouvement
**Problème** : `FCompartmentState` calcule explicitement une notion de `WaterMassLiters` (8.3).
**Défaut** : Ce volume d'eau n'est relié à aucune dynamique dans la "Suggested Server Tick Pipeline" (10.7).
Si l'accumulation d'eau dans la coque ignore l'inertie, le trim ou l'Auto-Depth (Vertical Authority du `USubmarineMovementComponent`), la crise de voie d'eau se transforme en simple indicateur mortel déconnecté du pilotage. La masse d'eau doit agir explicitement sur la physique calculée en amont.

### C. Jitter du Mouvement Personnage Interne (Networked Base Locomotion)
**Problème** : Le Draft traite l'acteur principal comme une plateforme mobile interpolée, mais considère que `ASubCrewCharacter` s'occupera simplement du "traversal interieur" (7.9).
**Défaut** : Dans l'Unreal Engine, utiliser un `UCharacterMovementComponent` standard circulant à l'intérieur d'un acteur dont le Transform est sans cesse imposé via réseau (Server-Authoritative répliqué) va produire un énorme *jitter* (rubberbanding) client en coop. Ce point de friction majeur du moteur n'est ni discuté ni esquivé.
**Correction requise** : Prévoir la méthode technique (ex: *Root-shifting local / Stationary Interior Area* ou configuration stricte sur le *Base Movement* avec interpolation retardée locale) pour le C++ Movement.

---

## 3. Lacunes et Oublis Évidents (Gaps)

### A. Mécanique et Contrôle des Portes (Doors & Bulkheads)
Bien que le `FCompartmentState` contienne la valeur `bDoorToNextClosed` influant sur le "transfer par portes" (12.4), l'entité de la Porte n'existe nulle part dans le modèle Actor/Component.
- **Oubli** : Qui possède les portes logiques ? Si fermer une porte est fondamental pour isoler le sous-marin (la crise jouable visée), il faut structurer un `ASubDoorActor` ou des Sockets de `USubHullComponent`, avec l'interface d'interaction requise pour que l'avatar du Crew le déclenche en C++.

### B. Interaction d'Avatar, Réparation & Équipement
La réparation "n'est pas une station" (11.3), mais requiert d'agir sur le système.
- **Oubli** : Comment le corps du joueur avatar (`ASubCrewCharacter`) interagit-il physiquement sur une brèche ou un moteur pour exécuter `StandardRepair` ? Il n'y a aucune composante d'Equipement, ni d'Interface gérant le "Mode Outil". Le Draft présente l'Avatar C++ comme un simple composant de "traversal".
- **Outil annexe suggéré** : L'injection d'un composant d'Interaction natif (type `UInteractionComponent`) couplé à des `GameplayTags` C++ pour déclencher l'Action Repair temporaire ou maintenue (Overlap/LineTrace vers les `HullDamagePanels`).

### C. Scope Creep - Tourelle et Radar 3D (Turret & Radar)
**Problème** : Les sections 13 (`TurretStation`) et 14 (`Radar3D`) diluent le focus absolu sur la Coque demandée dans Proto03.
- **Oubli Stratégique** : Bien que listés comme Proto03, the "Core Playable Interior Loop" (impact -> breach -> flood -> fix) doit fonctionner *avant* que l'armement ou la vision sonore en dehors du bateau ne vienne pomper les ressources.
- **Correction requise** : Requalifier explicitement ces modules comme *Optionnels (Priority 4 / Post-Core)*, à n'intégrer qu'après le bouclage net de la voie d'eau et du mouvement inter-joueur basique.

### D. Pipeline de Choc Unreal
**Oubli** : L'architecture déploie `StructuralSheetField` mais perd de vue de se "plugger" aux événements de dégâts standard.
- **Outil annexe suggéré** : Brancher `USubmarineHullComponent` sur le framework natif `OnTakeRadialDamage` et `OnTakePointDamage` Unreal. Cela permettra à de futurs projectiles, chocs ou explosions d'interagir nativement sans recréer un pipeline entier de "World Collision Event To Sub Sub-system".
