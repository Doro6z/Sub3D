# Recherche Technique — Script Blender Python pour Blockout Procédural de Sous-Marin

**Date** : 2026-04-12
**Audience** : Lead dev solo, pipeline Sub3D → UE 5.7
**Objectif** : Déterminer la meilleure approche pour générer un blockout de sous-marin à partir d'un plan technique multi-vues

---

## 1. Diagnostic Global

Le problème se décompose en 4 sous-problèmes indépendants :

1. **Interpréter le plan technique** → extraire des profils de sections transversales numériques
2. **Construire la coque** → lofter une surface entre ces sections
3. **Ajouter les intérieurs** → decks, bulkheads, compartiments
4. **Exporter** → FBX compatible UE5

Le plan technique fournit exactement ce qu'il faut pour l'approche par **stations-cadres** (frame stations) — c'est la méthode standard en architecture navale depuis le 18ème siècle. Les sections transversales du plan sont littéralement les stations.

---

## 2. Comparaison des Approches

### 2.1 Cinq approches évaluées

| Approche | Principe | Robustesse | Itérabilité | Complexité | Verdict |
|---|---|---|---|---|---|
| **A. Courbes + loft Blender** | Créer des courbes Bézier/NURBS, utiliser un addon de loft | Moyenne | Bonne (éditable dans le viewport) | Moyenne | Conversion mesh imprévisible |
| **B. Rings vertex + quad stitching** | Calculer les vertices de chaque section, coudre les quads entre sections adjacentes | **Haute** | **Haute** | Faible | **RECOMMANDÉ** |
| **C. BMesh bridge_loops** | Créer des edge loops dans BMesh, appeler `bridge_loops()` | Moyenne | Moyenne | Moyenne | Bon fallback |
| **D. Geometry Nodes** | Node tree paramétrique | Faible (binaire, pas versionnable) | Bonne en viewport | Haute | Pas adapté à du data-driven |
| **E. NURBS surface patches** | Surface NURBS entre profils | Haute mathématiquement | Faible (API Blender limitée) | Très haute | Overkill pour blockout |

### 2.2 Recommandation principale : Approche B — Rings + Quad Stitching

**Pourquoi :**
- Déterministe : mêmes inputs → même mesh, toujours
- Contrôle total de la topologie (quads propres, nombre de vertices prévisible)
- Pas de dépendance à des opérateurs Blender qui peuvent être context-dependent
- Fonctionne en headless (`blender --background`)
- Facile à débugger (chaque vertex a un index prévisible)
- Le code est linéaire : générer les vertices → générer les faces → créer le mesh

**L'algorithme :**
```
Pour N stations avec M vertices chacune :
  Vertices = N × M points (x, y, z)
  Faces = (N-1) × M quads :
    face(i, j) = (i*M + j, i*M + (j+1)%M, (i+1)*M + (j+1)%M, (i+1)*M + j)
```

C'est exactement ce que `SubmarineMeshBuilder.cpp` fait déjà dans le code UE. On porte la même logique en Python.

---

## 3. Comment Exploiter le Plan Multi-Vues

### 3.1 Ce qui peut être automatisé

| Étape | Automatisable ? | Méthode |
|---|---|---|
| Import des images de référence | **Oui** | `bpy.data.images.load()` → Empty IMAGE |
| Calibration des proportions | **Semi** | L'utilisateur fournit une dimension connue (longueur totale) |
| Extraction des profils de section | **Non** | Digitalisation manuelle ou semi-assistée |
| Placement des stations sur l'axe X | **Oui** | Positions normalisées × longueur |
| Interpolation entre stations | **Oui** | SciPy B-spline ou smoothstep |
| Génération du mesh | **Oui** | Quad stitching |

### 3.2 Workflow réaliste pour les sections transversales

Le plan technique montre 8-10 coupes transversales. Le workflow :

1. **Digitaliser** chaque coupe comme une liste de points (y, z) — manuellement dans Blender ou via un éditeur de points
2. **Stocker** dans un fichier JSON/CSV structuré
3. **Le script lit** ce fichier et resample chaque profil à M vertices uniformes
4. **Interpoler** des stations intermédiaires entre les coupes pour lisser la surface

**Format de données recommandé :**
```json
{
  "length_cm": 4400,
  "stations": [
    {"x_norm": 0.0, "profile": [[0, 0]]},
    {"x_norm": 0.1, "profile": [[120, -200], [180, -100], [200, 0], [180, 100], [120, 200], [0, 220]]},
    ...
  ]
}
```

### 3.3 Import et calibration des références

```python
# Charger une image comme Empty de référence
img = bpy.data.images.load(filepath)
empty = bpy.data.objects.new("Ref_SideView", None)
empty.empty_display_type = 'IMAGE'
empty.empty_image = img  # Blender 4.x+
empty.empty_display_size = known_length / 2.0  # Half-extent
empty.location = (0, 0, 0)
# Rotation pour vue latérale (XZ plane) : pas de rotation
# Rotation pour vue dessus (XY plane) : rotation 90° autour de X
```

### 3.4 Gestion des incohérences entre vues

Les vues d'un plan technique peuvent être légèrement incohérentes (erreurs de dessin, distorsion de scan). Stratégie :

- **Les sections transversales sont la source de vérité** — elles définissent la forme 3D
- La vue de côté sert à **placer les stations** sur l'axe X
- La vue de dessus sert à **vérifier** la largeur max (half-breadth)
- En cas de conflit : les sections transversales gagnent

---

## 4. Architecture du Script Recommandée

### 4.1 Structure de fichiers

```
sub3d_hull_blockout/
├── __init__.py              # Point d'entrée, setup scene
├── config.py                # Constantes (épaisseur coque, dimensions portes, etc.)
├── data/
│   └── stations.json        # Profils de sections digitalisés
├── core/
│   ├── profile.py           # Classe StationProfile : chargement, resampling, interpolation
│   ├── hull.py              # Classe HullBuilder : lofting, capping, solidify
│   ├── interior.py          # Classe InteriorBuilder : decks, bulkheads, compartiments
│   └── mesh_utils.py        # Helpers : from_pydata wrapper, normals, materials
├── io/
│   ├── reference.py         # Import images de référence
│   └── export.py            # Export FBX UE5-compatible
└── main.py                  # Orchestration : load → build → export
```

### 4.2 Classes principales

```python
@dataclass
class StationProfile:
    """Un profil de section transversale (demi-profil starboard)."""
    x_norm: float                    # Position normalisée [0, 1]
    points: list[tuple[float, float]]  # [(y, z), ...] de quille à sommet
    
    def resample(self, n_points: int) -> list[tuple[float, float]]:
        """Resample à exactement n_points avec interpolation B-spline."""
        ...
    
    def mirror(self) -> list[tuple[float, float]]:
        """Full profile : starboard + port (mirrored)."""
        ...


class HullBuilder:
    """Construit la coque par lofting entre stations."""
    def __init__(self, stations: list[StationProfile], length_cm: float):
        ...
    
    def interpolate_station(self, x_norm: float) -> StationProfile:
        """Station interpolée à n'importe quelle position X."""
        ...
    
    def build_mesh(self, n_rings: int, pts_per_ring: int) -> tuple[list, list]:
        """Retourne (vertices, faces) pour from_pydata."""
        ...
    
    def add_caps(self, verts, faces):
        """Ajoute les caps bow/stern (triangle fans)."""
        ...


class InteriorBuilder:
    """Construit decks, bulkheads, compartiments."""
    def __init__(self, hull: HullBuilder, config: dict):
        ...
    
    def build_deck(self, z: float, x_start: float, x_end: float) -> tuple:
        """Deck plate clippé à l'intérieur de la coque."""
        ...
    
    def build_bulkhead(self, x: float, z_min: float, z_max: float, 
                       door_w: float, door_h: float) -> tuple:
        """Bulkhead avec découpe porte, clippé à la coque."""
        ...
```

### 4.3 Pipeline de génération

```python
def main():
    # 1. Charger la config
    config = load_config("config.py")
    
    # 2. Charger les profils de stations
    stations = load_stations("data/stations.json")
    
    # 3. Construire la coque
    hull = HullBuilder(stations, config.length)
    hull_verts, hull_faces = hull.build_mesh(n_rings=100, pts_per_ring=32)
    hull_obj = create_mesh("SM_Hull", hull_verts, hull_faces)
    solidify(hull_obj, -config.hull_thickness)
    
    # 4. Construire les intérieurs
    interior = InteriorBuilder(hull, config)
    for deck in config.decks:
        deck_obj = interior.build_deck(deck.z, deck.x_start, deck.x_end)
        solidify(deck_obj, -deck.thickness)
    
    for i, bh in enumerate(config.bulkheads):
        bh_obj = interior.build_bulkhead(bh.x, bh.z_min, bh.z_max, bh.door_w, bh.door_h)
        solidify(bh_obj, config.bulkhead_thickness)
    
    # 5. Export
    export_fbx("output/submarine_blockout.fbx")
```

---

## 5. APIs Blender à Utiliser

### 5.1 Recommandé

| API | Usage | Pourquoi |
|---|---|---|
| `Mesh.from_pydata(verts, edges, faces)` | Construction du mesh | Simple, batch, déterministe |
| `bpy.data.objects.new(name, mesh)` | Création d'objets | Standard |
| `bpy.data.materials.new(name)` | Matériaux simples | Identification visuelle |
| `obj.modifiers.new("S", 'SOLIDIFY')` | Épaisseur coque/decks | Évite de gérer 2 surfaces |
| `bpy.ops.object.modifier_apply()` | Appliquer solidify | Nécessaire avant export |
| `bpy.ops.mesh.normals_make_consistent()` | Fix normals | En edit mode |
| `bpy.ops.export_scene.fbx()` | Export FBX | Avec les bons settings UE5 |
| `bpy.data.images.load()` | Charger référence | Pour calibration |
| `Empty` display type `IMAGE` | Afficher référence | Dans le viewport |
| `bpy.data.collections.new()` | Organiser les objets | Hull / Interior / Reference |

### 5.2 À éviter

| API | Pourquoi éviter |
|---|---|
| `bpy.ops.mesh.*` (sauf normals) | Context-dependent, lent, imprévisible en headless |
| `bmesh.ops.bridge_loops()` | Fonctionnel mais setup complexe, résultat moins prévisible |
| NURBS surfaces Blender | API limitée, conversion mesh imprévisible |
| Geometry Nodes via Python | Pas versionnable, overkill pour ce use case |
| `bpy.ops.curve.loft()` | N'existe pas nativement, nécessite addon |

### 5.3 Optionnel mais utile

| Outil | Usage |
|---|---|
| **SciPy** `splprep`/`splev` | B-spline interpolation des profils de station |
| **NumPy** arrays | Performance pour gros meshes (>10k verts) |
| `bmesh` | Post-processing (edge cleanup, merge doubles) |
| Collections Blender | Organiser les objets par catégorie |
| Custom Properties | Stocker des metadata sur les objets (compartment_id, deck_level) |

---

## 6. Niveau de Vérité pour le MVP

### 6.1 MVP (Phase 1 — 2 jours)

| Élément | Inclus | Détail |
|---|---|---|
| Coque extérieure | **OUI** | Lofted entre 8-12 stations, solidify 15cm |
| Caps bow/stern | **OUI** | Triangle fans simples |
| Deck plates (3) | **OUI** | Plats, clippés à la coque |
| Bulkhead template | **OUI** | Un seul, duplicable |
| Export FBX | **OUI** | Settings UE5 |
| Matériaux simples | **OUI** | Couleur par type |

### 6.2 Phase 2 (post-MVP)

| Élément | Détail |
|---|---|
| Compartiments individuels | Box-rooms positionnés |
| Découpe portes dans bulkheads | Grille avec exclusion |
| Sail/kiosque | Forme intégrée au profil ou objet séparé |
| Stations data-driven (JSON) | Remplacement des constantes hardcodées |
| Import images de référence | Calibration semi-automatique |

### 6.3 Hors scope

| Élément | Raison |
|---|---|
| Props intérieurs | Pas du blockout |
| UV mapping | Matériaux UE |
| LODs | Post-art |
| Détails extérieurs | Post-blockout |
| Simulation hydro | Pas pertinent |

---

## 7. Gestion des Intérieurs

### 7.1 Ordre de développement recommandé

1. **Coque seule** — valider la silhouette
2. **Coque + decks** — valider les niveaux
3. **Coque + decks + bulkhead template** — valider la structure
4. **Compartiments** — si nécessaire (peut être fait en Blender manuellement)

### 7.2 Clipping des intérieurs à la coque

Le problème rencontré dans les scripts précédents : les decks et bulkheads dépassent de la coque.

**Solution :** à chaque position (x, z) d'un vertex de deck/bulkhead, calculer le rayon intérieur de la coque et clipper :

```python
def hull_interior_half_width(x, z):
    """Demi-largeur intérieure de la coque à (x, z)."""
    station = hull.interpolate_station(x / hull.length)
    # Trouver le y max à cette hauteur z dans le profil
    for y_prof, z_prof in station.points:
        if abs(z_prof - z) < tolerance:
            return y_prof - hull_thickness
    return 0
```

---

## 8. Risques Techniques et Mitigations

| Risque | Impact | Probabilité | Mitigation |
|---|---|---|---|
| Profils digitalisés imprécis | Coque déformée | Haute | Vérification visuelle après chaque ajout de station |
| Nombre de points variable entre stations | Faces dégénérées, torsion | Haute | **Resampler TOUS les profils à M points** avant lofting |
| Solidify qui explose sur géométrie complexe | Mesh corrompu | Moyenne | Garder les profils simples, pas de concavités |
| Scripts trop couplés au plan spécifique | Non réutilisable | Moyenne | Data-driven (JSON), pas de dimensions hardcodées |
| Ambiguïté entre vues du plan | Incohérences | Moyenne | **Les sections transversales sont la source de vérité** |
| Performance sur gros mesh | Lenteur | Faible | `from_pydata` suffit pour <10k verts |
| Export FBX scale incorrecte | Mesh géant/minuscule dans UE | Moyenne | Toujours tester avec un cube de référence 100cm |

---

## 9. Pipeline d'Implémentation Recommandé

| Phase | Durée | Livrable | Dépendance |
|---|---|---|---|
| **Phase 1** : Digitaliser les sections du plan | 2h | `stations.json` avec 8-12 profils | Plan technique |
| **Phase 2** : Script squelette (config + mesh utils) | 1h | `config.py`, `mesh_utils.py` | — |
| **Phase 3** : HullBuilder (loft + caps) | 2h | Coque extérieure visible | Phase 1+2 |
| **Phase 4** : Solidify + matériaux | 30min | Coque épaisse avec couleur | Phase 3 |
| **Phase 5** : InteriorBuilder (decks clippés) | 1h | 3 decks dans la coque | Phase 3 |
| **Phase 6** : Bulkhead template | 30min | 1 bulkhead clippé | Phase 3 |
| **Phase 7** : Export FBX UE5 | 30min | FBX importable | Phase 4+5+6 |
| **Total** | **~8h** | Blockout complet exportable | |

---

## 10. Checklist de Préparation Avant Codage

- [ ] Plan technique en haute résolution (PNG/PDF)
- [ ] Dimension connue pour calibration (longueur totale du sous-marin)
- [ ] Sections transversales identifiées et numérotées
- [ ] Position X de chaque section mesurée sur le plan
- [ ] Chaque section digitalisée comme liste de points (y, z)
- [ ] Fichier `stations.json` créé avec tous les profils
- [ ] Décision sur le nombre de decks et leurs hauteurs Z
- [ ] Décision sur les dimensions des portes
- [ ] Blender 4.x+ installé avec SciPy (`pip install scipy` dans le Python de Blender)
- [ ] Dossier de travail créé avec la structure de fichiers recommandée

---

## 11. Recommandation Finale

### L'approche

**Rings vertex + quad stitching**, data-driven depuis un fichier JSON de stations. C'est l'approche la plus robuste, la plus simple à débugger, et la plus proche de ce que fait déjà le `SubmarineMeshBuilder.cpp` du projet UE.

### Les trade-offs assumés

| On accepte | On refuse |
|---|---|
| Digitalisation manuelle des sections (2h) | Extraction automatique depuis l'image (trop fragile) |
| Profils polygonaux (pas NURBS smooth) | Précision NURBS (overkill pour blockout) |
| Solidify pour l'épaisseur (pas de double surface) | Construction manuelle intérieur+extérieur (trop complexe) |
| Un seul matériau par type d'objet | UV mapping (pas nécessaire pour blockout) |
| Caps simples (triangle fan) | Caps sculptés (post-blockout) |

### Le premier script MVP à écrire

Un script unique (`main.py`) qui :
1. Lit `stations.json` (8-12 profils de sections)
2. Resample chaque profil à 32 points
3. Interpole 100 rings le long de l'axe X (smoothstep)
4. Construit le mesh via `from_pydata` (quad stitching)
5. Ajoute les caps (triangle fans)
6. Applique solidify -15cm
7. Crée 3 decks plats clippés à la coque
8. Crée 1 bulkhead template à X=0
9. Exporte en FBX

Temps estimé : **une demi-journée** pour le script MVP, à condition que `stations.json` soit prêt.

---

## Sources

- Blender Python Mesh API : docs.blender.org/api/current/bpy.types.Mesh.html
- BMesh Module : docs.blender.org/api/current/bmesh.html
- SURF Visualization Course (mesh construction) : surf-visualization.github.io/blender-course/api/meshes/
- Naval architecture Lines Plan : thenrg.org/articles/interpreting-line-drawings
- bpyhullgen (open-source hull generator) : github.com/edzop/bpyhullgen
- Blender-UE5 FBX Workflow : srogers4.github.io/blender-ue5-workflow
- Blender Multi-File Addon Architecture : b3d.interplanety.org/en/creating-multifile-add-on-for-blender/
- Blender Curves Scripting : behreajj.medium.com (Jeremy Behreandt)
- NURBS-Python (geomdl) : pypi.org/project/geomdl/
