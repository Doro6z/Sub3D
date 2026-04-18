# Sub3D - Editor & Gameplay Intent Contract (Hull & Decks First)

Date: 2026-04-03
Status: Agreed Intent & Vision
Context: Définition de l'expérience d'authoring et de la vérité gameplay pour le prototype 03 et au-delà.

---

## 1. Contrat d'Intentions Techniques

Ce contrat définit les vérités immuables du système de construction des sous-marins pour Sub3D, garantissant que le jeu reste un jeu de "gestion de crise de sous-marin" et non une simulation de fluides/gaz complexe.

### 1.1. Vérité Spatiale Primaire : La Coque (Pressure Hull)
Le sous-marin est la coque. C'est elle qui définit le volume, l'enveloppe de collision avec le monde (drag, inertie, portance) et la surface exposée aux attaques.
- La coque ne "découle" pas de l'intérieur.
- Toutes les brèches se font d'abord sur cette coque.

### 1.2. Système de Primitives Tripartites (The Triple-Primitive Model)
Pour garantir une flexibilité totale de design tout en conservant une cohérence structurelle et de gameplay :
- **Structural Frames (Anneaux/Frames) :** Vérité géométrique pour la résistance au flambage (HP structurelle). Ils sont **non-étanches** et n'impactent pas le flood. Ils servent de points d'ancrage pour l'équipement.
- **Pressure Bulkheads (Cloisons Étanches) :** Scindent la coque en zones pressurisables et inondables. Ils définissent les nœuds du **Flood Graph**.
- **Internal Walls (Cloisons Légères) :** Éléments de layout purement visuels ou fonctionnels (cabines, rangements). Ils n'impactent pas la survie du navire en cas d'inondation.

### 1.3. Géométrie Analytique (Analytical Truth)
La physique du navire ne doit jamais dépendre du maillage de rendu (instable). Les données de pilotage sont calculées via l'**Evaluator Analytique** :
- **Volume Déplacé (Buoyancy) :** Masse d'eau déplacée basée sur l'intégrale de section.
- **Inertie & Masse Ajoutée (Hydrodynamic Inertia) :** Calculée selon le ratio de finesse de la coque.
- **Profil de Traînée (Drag) :** Déduit de la géométrie de la coque pour influencer la vitesse maximale et la manœuvrabilité.

### 1.4. Simulation de Flood "Graphe & Orifice"
- **Orifice Flow :** La propagation de l'eau entre compartiments suit une loi de débit simplifiée selon l'aire d'ouverture (Portes/Écoutilles) et l'égalisation des niveaux.
- **Perméabilité :** Chaque compartiment possède un facteur de perméabilité (ex: 0.8) représentant le volume réellement remplissable compte tenu de l'encombrement interne.

---

## 2. Découpage des Étapes d'Authoring (Les 3 Stages)

Le pipeline de création, tant pour les designers que pour d'éventuels joueurs, est scindé en 3 phases unidirectionnelles strictes.

### Stage A — Base Submarine (The Drydock Phase)
L'utilisateur sculpte la coquille et les planchers.
- **Authoring :** Forme de la coque exterieure/interieure, génération des *Structural Rings*.
- **Decks :** Agencement des ponts sous forme de "Bandes de navigation" (Deck bands, local widenings, side corridors, exclusions).
- **Ouvertures (Cuts) :** Trous verticaux pour prévoir les échelles et écoutilles.
- **Résultat :** Une topologie spatiale figée ("The Raw Hull").

### Stage B — Interior Partitioning (The Outfitting Phase)
L'utilisateur peuple l'espace vide.
- **Fermetures :** Placement des Bulkheads étanches.
- **Divisions légères :** Cloisons internes non étanches (ex: murs de cabines).
- **Circulation :** Placement des portes, échelles, mezzanines.
- **Fonctionnel :** Stations, pompes, moteurs.

### Stage C — Derived Gameplay Volumes (The Runtime Compilation)
Le système compile automatiquement les données pour le gameplay.
- **Calcul :** Parcours de l'espace depuis la coque jusqu'aux bulkheads/portes.
- **Derivation :** Création du graphe de compartiments isolables (Flood Graph).
- **Résultat :** Les volumes de confinement sont créés implicitement sans que l'utilisateur n'ait eu à tracer de "boite de pièce".

---

## 3. Décision UX Cruciale : Le "Drydock Lock" (UX 1)

Pour garantir la faisabilité technique et une UX robuste :
- **Règle :** Une fois le **Stage A** terminé et validé, la géométrie de la coque est *Locked*.
- **Concept :** On ne peut pas "allonger le sous-marin de 5 mètres" une fois qu'on a commencé à poser des lits, de la tuyauterie et des cloisons dans le **Stage B**.
- **Avantages :** 
  - Simplification extrême du code de l'éditeur (pas de solver de relocalisation de props).
  - Paradigme industriel crédible (on ne redimensionne pas un navire en cours d'équipement).
  - Parfait pour itérer rapidement sur les mécaniques de flood avec un "cylindre vide" avant même de coder tout le système d'ameublement.

---

## 4. Vision Technique des Decks

Les ponts ne sont pas de simples cylindres coupés. L'outil devra supporter :
- **Deck Bands :** Un couloir central ou latéral.
- **Local Exclusions :** Un trou rectangulaire grand ouvert au centre de la pièce (ex: pour voir le réacteur en dessous ou la machinerie).
- **Split levels :** Des demi-étages locaux au sein d'une même section de coque.
