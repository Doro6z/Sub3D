# Sub3D — Submarine Editor Operator Guide

Version: 2026-04-04
Statut: référence opérationnelle pour clôturer le projet Sub Editor

---

## 1. Ce que fait le système

Le Submarine Editor est le seul chemin d'authoring prévu pour les sous-marins dans Sub3D.

Il prend un **USub3DSubmarineAuthoringAsset** comme entrée, le fait passer par un pipeline de bake structuré, et produit :

- `<AssetName>_BaseCompiled` — **UCompiledSubmarineBaseAsset** (données structurelles baked)
- `<AssetName>_RuntimeCompiled` — **UCompiledSubmarineRuntimeAsset** (données runtime baked)
- **ASubmarineRuntimeActor** spawné dans le niveau éditeur, chargé depuis le compiled runtime asset uniquement

Le runtime ne dépend jamais directement de l'authoring asset.

---

## 2. Prérequis

- Projet compilé avec les modules : `Sub3DBake`, `Sub3DRuntime`, `Sub3DEditor`
- Un niveau ouvert dans l'éditeur (pour spawner l'editor actor et le runtime actor)
- L'asset `USub3DSubmarineAuthoringAsset` sauvegardé dans le Content Browser (pas dans `/Temp/`)

---

## 3. Créer un authoring asset

Dans le Content Browser :

1. `Clic droit → Miscellaneous → Data Asset`
2. Sélectionner `Sub3DSubmarineAuthoringAsset`
3. Nommer l'asset (ex. `Sub_Typhoon_Auth`)
4. **Sauvegarder** l'dasset avant tout bake (`Ctrl+S`)

---

## 4. Ouvrir le toolkit

**Double-clic** sur l'asset `USub3DSubmarineAuthoringAsset` dans le Content Browser.

Le **Submarine Editor** s'ouvre avec 8 onglets correspondant aux phases d'authoring canoniques :

| Onglet | Phase | Données éditées |
|--------|-------|-----------------|
| Drydock | Vue d'ensemble | Toutes les propriétés + actions rapides |
| Ring Architecture | Phase 1 | Pressure Hull, Control Rings, Frame-Rings |
| Outer Envelope | Phase 2 | Kiosk / Sail |
| Bays | Phase 3 | Structural Bays + Draw debug |
| Decks / Floors | Phase 4 | Deck Levels, Floor Regions |
| Openings | Phase 5 | Openings, Connectors, Closures |
| Partitions | Phase 6 | Pressure Bulkheads, Internal Walls |
| Validate / Bake | Phase finale | Contrats, stats bake, actions |

---

## 5. Workflow standard

### 5.1 Phase Ring Architecture

Dans l'onglet **Ring Architecture** :

- `Hull.LengthCm` : longueur de la pressure hull en cm
- `Hull.MaxRadiusCm` : rayon maximum
- `Hull.WallThicknessCm` : épaisseur de coque
- `ControlRings` : liste des anneaux de contrôle

**Contraintes obligatoires :**
- Au moins 2 Control Rings
- `PositionX` strictement croissant dans `[0 .. Hull.LengthCm]`
- `RadiusCm > 0`, `WallThicknessCm > 0`

**Frame-Rings :**
- `SpineAlpha` dans `[0..1]`, strictement croissant
- `bIsBayBoundary = true` si ce ring doit délimiter une Structural Bay

### 5.2 Phase Outer Envelope

Dans l'onglet **Outer Envelope** :

- `bEnabled` : activer le kiosk / sail
- Dimensions et position relatives à la hull
- Le bake envelope est analytique, pas une surface de raccord finale

### 5.3 Phase Bays

Dans l'onglet **Bays** :

- Les Structural Bays peuvent être explicites (`StructuralBays`) ou dérivées des Frame-Rings `bIsBayBoundary`
- Bouton **Draw Structural Bays** : trace les bays dans le viewport après un bake réussi
- Chaque Bay définit combien de Deck Levels sont activables

### 5.4 Phase Decks / Floors

Dans l'onglet **Decks / Floors** :

- `DeckLevels` : niveaux logiques verticaux (HeightCm, BayId)
- `FloorRegions` : surfaces walkables concrètes (bande principale, side corridors, mezzanines)
- Ne pas investir dans le tuning fin avant que les Bays soient stables

### 5.5 Phase Openings

Dans l'onglet **Openings** :

- `Openings` : ouvertures géométriques (deck cutout, hatch opening, doorway)
- `Connectors` : éléments de circulation (ladder, ramp, passage)
- `Closures` : éléments de fermeture (door, hatch)

**Règle : ne jamais fusionner Opening / Connector / Closure conceptuellement.**

### 5.6 Phase Partitions

Dans l'onglet **Partitions** :

- `PressureBulkheads` : séparations majeures pour la propagation d'eau
- `InternalWalls` : séparations d'organisation

### 5.7 Validate / Bake

Dans l'onglet **Validate / Bake** :

#### Contrat Status
Affiche les violations de contrats détectées en temps réel (avant bake) :
- Control Rings strictement croissants
- PositionX dans les bornes
- Frame-Rings SpineAlpha valide
- Outer Envelope dimensions minimales

#### Last Bake Output
Affiche les statistiques du dernier bake réussi :
- Longueur hull, tris extérieur/intérieur/collision
- Outer Envelope tris
- Counts : Frame-Rings, Structural Bays, Deck Levels, Floor Regions
- Counts : Openings, Connectors, Closures, Partitions, Derived Flood Volumes

#### Actions

| Bouton | Effet |
|--------|-------|
| **Validate** | Valide l'authoring asset sans bake |
| **Bake Assets** | Bake base + runtime, persist les compiled assets |
| **Bake + Spawn Runtime Actor** | Bake base + runtime + spawn/update runtime actor dans le niveau |
| **Run PIE Navigation Smoke** | Lance un test de navigation PIE basique |

---

## 6. Assets produits

Après **Bake Assets** ou **Bake + Spawn Runtime Actor**, deux assets compilés sont créés automatiquement dans le même dossier que l'authoring asset :

```
Content/
  Submarines/
    Sub_Typhoon_Auth.uasset              ← authoring asset (source of truth)
    Sub_Typhoon_Auth_BaseCompiled.uasset ← UCompiledSubmarineBaseAsset
    Sub_Typhoon_Auth_RuntimeCompiled.uasset ← UCompiledSubmarineRuntimeAsset
```

Ces assets sont persistants et sauvegardables. Ils sont les seules sources de vérité runtime.

---

## 7. Runtime Actor

Après **Bake + Spawn Runtime Actor**, un `ASubmarineRuntimeActor` est spawné dans le niveau éditeur.

Il est nommé `<AssetName>_EditorActor_Runtime`.

Il contient :

| Composant | Rôle |
|-----------|------|
| `USubmarineHullComponent` | Données analytiques de coque |
| `USubmarineBreachRuntimeComponent` | Bindings de breach par partition |
| `USubmarineFloodRuntimeComponent` | Flood graph compilé |
| `USubmarineDoorRuntimeComponent` | États des closures |
| `UProceduralMeshComponent[]` | Render meshes (exterior, interior, envelope, partitions) |
| `UProceduralMeshComponent[]` | Collision meshes (proxy + partitions) |

Le runtime actor est initialisé depuis `UCompiledSubmarineRuntimeAsset` uniquement. Il ne lit pas l'authoring asset.

---

## 8. Tester en PIE

Après avoir spawné le runtime actor :

1. Placer un `ASubCrewCharacter` dans le niveau
2. Lancer PIE
3. Le runtime actor spawn ses meshes de collision → le personnage peut naviguer à l'intérieur
4. Le **Run PIE Navigation Smoke** dans le toolkit peut vérifier la connectivité basique

---

## 9. Contenu externe (hors scope du générateur)

Le générateur produit la **structure** du sous-marin. Il ne génère pas :

- Textures
- Matériaux
- Meshes de stations uniques
- Meshes de machinerie
- Meshes de propulseurs
- Meshes de stockage
- Tout asset art unique

Ces assets sont assignés manuellement en référençant le runtime actor ou ses composants en Blueprint.

---

## 10. Limites actuelles et deferred

Conformément au Plan Directeur, les éléments suivants sont **hors scope de cette phase** :

- Simulation de pression interne (le modèle est Breach → Flood → Propagation → Confinement)
- Remeshing booléen global runtime
- Freeform hull sculpting
- Simulateur hydrodynamique 6DOF réaliste
- UI finale cosmétique
- Économie et campagne finale

---

## 11. Diagnostic rapide

| Symptôme | Cause probable | Action |
|----------|---------------|--------|
| Bake échoue avec "must be a saved content asset" | Asset dans `/Temp/` ou non sauvegardé | Sauvegarder l'asset dans le Content Browser |
| Bake échoue avec erreur validation | Control Rings non croissants ou hors bornes | Vérifier le Contract Status dans le tab Validate/Bake |
| Pas de Structural Bays après bake | Aucun Frame-Ring `bIsBayBoundary` et aucun `StructuralBays` explicite | Marquer au moins un Frame-Ring comme bay boundary |
| Runtime actor invisible | Exterior hull vide (0 tris) | Vérifier que `ControlRings.Num() >= 2` et `RadiusCm > 0` |
| Double runtime actor dans le niveau | Résidu d'une session précédente | Supprimer manuellement l'ancien actor dans le niveau |

---

## 12. Ce qui est "Done" pour clôturer le Sub Editor

Le Sub Editor est considéré complet pour passer au gameplay quand :

- [x] Le toolkit est le point d'entrée d'authoring (double-clic sur authoring asset)
- [x] Chaque phase a son onglet dédié avec les propriétés filtrées
- [x] Validate / Bake / Spawn passent en un seul chemin stable
- [x] Le runtime actor est produit depuis les compiled assets uniquement
- [x] Les stats bake sont visibles dans le toolkit
- [x] Les contrats d'entrée (Ring ordering, Frame-Ring ordering, Envelope dimensions) sont visibles en temps réel
- [x] Le pipeline bake couvre : Hull → Rings → Envelope → Bays → Decks → Floors → Openings → Connectors → Closures → Partitions → FloodGraph
- [x] Le runtime actor initialise : HullComponent, BreachRuntimeComponent, FloodRuntimeComponent, DoorRuntimeComponent

**Ce qui reste pour la Phase 6 (validation finale) :**
- Automation pass review
- Bake determinism test avec un asset représentatif
- PIE navigation test end-to-end
- Nettoyage UX final si nécessaire

---

## 13. Prochaines étapes gameplay

Une fois le Sub Editor validé, les points d'entrée gameplay sont :

1. **SubmarineFloodRuntimeComponent** → brancher la logique Breach → Flood → Propagation → Confinement
2. **SubmarineBreachRuntimeComponent** → brancher les triggers de brèche (dégâts, explosions)
3. **SubmarineDoorRuntimeComponent** → brancher les états ouverts/fermés des Closures
4. **SubmarineHullComponent** → brancher les paramètres dérivés de pilotage (mass, drag, inertia)
5. **SubMovementComponent** → consommer les paramètres de HullComponent pour la navigation

Les Room Tags (dérivés du FloodGraph) servent d'identifiants de zones pour l'IA et les stations.
