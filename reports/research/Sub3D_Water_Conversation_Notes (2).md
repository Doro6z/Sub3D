# Sub3D Water — Notes de conversation et décisions tentatives

**Date** : 2026-05-03
**Source** : conversation Claude.ai (Claude Opus 4.7 + humain) du 2-3 mai 2026
**Statut** : **proposition à challenger**, pas spec figée

> Ce document n'est **pas** une spec d'architecture validée. C'est un compte-rendu de ce qui a été discuté entre l'humain et un agent externe (Claude Opus 4.7 sur claude.ai) qui n'a pas accès au code du repo. Les décisions listées ici sont des **propositions tentatives** qui doivent être confrontées au code réel et challengées librement.

---

## 0. Contexte de la conversation

L'humain (DXR) a discuté avec Claude Opus 4.7 sur claude.ai pendant plusieurs jours autour du système de rendu d'eau Sub3D, après que le proto `Sub3DWaterProto` ait livré ses validations Étape A.

La conversation est partie d'un audit du proto, a divergé sur plusieurs sujets (système de portes, fusion de heightfields, simulation voxel vs heightfield, multi-deck), puis a convergé vers une architecture proposée. Plusieurs alternatives ont été examinées et rejetées en cours de route (plan d'eau global unique, simulation voxel, refonte fusion-runtime).

L'agent externe **n'a pas accès au code source** du repo Sub3D. Ses propositions sont basées sur :
- L'audit du proto (`2026-05-02_water_proto_audit.md`)
- Le header de `USubFloodComponent` (transmis par l'humain pendant la discussion)
- Le T3D dump du DA Craniata (transmis par l'humain)
- Le code source du proto (transmis fragment par fragment)

**Ce qui veut dire** : les propositions qui suivent peuvent contenir des erreurs ou des incompréhensions du code réel. Ton rôle est de les challenger.

---

## 1. Sujets discutés et état des réflexions

### 1.1 Diagnostic du proto

État du proto au moment de la conversation :
- Pipeline algo (voxelisation, SDF, MS interpolated, rings tessellation, blending entre slices) : validé ✅
- Heightfield CPU + Gerstner WPO + sample texture : validé ✅
- Étape B (deux salles + porte) : partiellement bricolée. Bridge mesh statique en bas avec discontinuité visible.

L'humain a explicitement décidé de **ne pas finir le polish Étape B du proto**. Le portage vers Sub3D commence avec l'archi cible, pas en finissant le proto.

### 1.2 Tension principale identifiée

L'humain a exprimé une vision forte : *"l'eau doit donner la sensation d'un fluide unique entre compartiments connectés, pas de nappes patchées"*. Cette vision a été le fil conducteur de toute la conversation.

Avec le proto actuel : pas de continuité visuelle aux portes, pas d'écoulement visible, juste deux nappes indépendantes avec un bridge mesh moche. Insuffisant pour Sub3D.

### 1.3 Alternatives examinées et rejetées

| Alternative | Pourquoi rejetée |
|---|---|
| Fusion runtime de heightfields (merger/splitter dynamiquement) | Trop complexe, refonte structurelle, pas de gain prouvé |
| Plan d'eau global unique pour le sub entier | Multi-deck rend ça impossible (plusieurs surfaces à des Z différents simultanément) |
| Plans empilés alignés sur grille | Multi-deck rend ça impossible aussi |
| Simulation voxel grossière (cellular automaton) | Considéré, mais l'humain a recadré sur "rester simple, accepter compromis" |
| Voxel avec pression (shallow water) | Trop coûteux, complexité numérique |
| Patch mesh aux portes (proto Étape B) | L'humain refuse les patches qui camouflent — il veut un système qui paraît naturellement continu |
| Cascade procédurale runtime | Idem, refusé |

### 1.4 Cadre final accepté

L'humain a accepté un cadre minimaliste :
- Garder l'architecture du proto (un Renderer par compartiment) avec sync au bord pour les portes horizontales
- Accepter que les transitions de portes auront des injections vortex/splash + petits FX (slosh à l'ouverture, etc.)
- Faire confiance à l'égalisation rapide des niveaux par `USubFloodComponent`
- Faire confiance aux mouvements naturels (gravité, virages, impacts crew) pour générer du bruit visuel qui camoufle les imperfections

Hatches verticales (multi-deck) : pas de sync heightfield, écoulement géré par FloodComponent + injections visuelles.

---

## 2. Propositions architecturales tentatives

> Ces propositions ont été élaborées par l'agent externe sans accès au code. **Elles peuvent être incorrectes, sous-optimales, ou redondantes avec ce qui existe déjà**. À challenger en regardant le code.

### 2.1 Composants proposés

| Composant proposé | Statut | Question à se poser |
|---|---|---|
| `USubmarineWaterManager` | Nouveau composant attaché à `ASubmarineBase` | Est-ce que cette responsabilité ne pourrait pas être absorbée par un système existant ? |
| `USubmarineWaterDef` | Nouveau UDataAsset (équivalent extended de `URoomWaterBakedData` à l'échelle sub) | Est-ce que la structure de bake actuelle pourrait juste être étendue plutôt que refondue ? |
| `UCompartmentWaterRenderer` | Renommage de `URoomWaterRenderer` | OK probablement, mais vérifier si des dépendances actuelles utilisent le nom Room |
| `USubmarineWaterBakerLibrary` | Nouveau, refactor du baker proto | Vérifier si le bake actuel peut juste être paramétré plutôt que dupliqué |
| `M_CompartmentWater` | Remplace `M_Phase0_Test` | Le matériau actuel a déjà été itéré. Est-ce qu'un simple renommage / cleanup suffit ? |

### 2.2 Décisions tentatives à challenger

Numérotation D1-D16 utilisée dans la conversation (pour traçabilité, mais à remettre en question librement) :

| ID | Décision tentative | À challenger sur |
|---|---|---|
| **D1** | Granularité = section hermétique (1 compartiment = 1 espace clos avec portes étanches) | Cohérent avec l'authoring actuel ? Avec `USubFloodComponent` ? Vérifier sur Craniata |
| **D2** | Bake "tout ouvert" — silhouette prolongée dans embrasures jusqu'au plan médian | Faisable proprement avec le baker actuel ? Quel coût d'implémentation réel ? |
| **D3** | Co-localisation des vertices au plan médian de la porte (pas de patch mesh) | Précision réelle nécessaire pour que ça paraisse continu ? Le baker actuel garantit-il cette précision ? |
| **D4** | Heightfield + cap mesh uniquement, vortex/splash pour écoulement (pas de cascade procédurale) | Suffisant visuellement ou faut-il quand même une couche additionnelle ? |
| **D5** | Sync heightfield au bord avec grille globale alignée sur le sub | Le coût de l'alignement de grille vaut-il le bénéfice ? Y a-t-il des cas où l'alignement est impossible (compartiments orientés différemment) ? |
| **D6** | `USubmarineDefinition` = source unique de topologie | Le DA est-il assez complet ? L'usage actuel par `USubFloodComponent::InitializeFromCompartmentVolumes` (path BP) est-il à supprimer ? |
| **D7** | `USubFloodComponent` = source unique des niveaux | Vrai en théorie. Mais y a-t-il des cas où le visuel devrait piloter (genre debug, scénarios) ? |
| **D8** | Manager central `USubmarineWaterManager` orchestre tout | L'orchestration centralisée est-elle vraiment nécessaire, ou chaque Renderer pourrait-il être autonome ? |
| **D9** | DA = source unique, `UCompartmentVolumeComponent` placés à la main = warning ou ignoré | Quelle est la stratégie de migration pour Craniata qui utilise `InitializeFromCompartmentVolumes` ? |
| **D10** | `UDoorWaterBridge` du proto = déprécié | OK. Mais y a-t-il du code dans le proto à conserver malgré tout ? |
| **D11** | Proto reste désactivé dans le repo jusqu'à validation finale | Cohérent avec les conventions du projet ? Convention de désactivation existante ? |
| **D12** | Migration incrémentale par phase, pas big-bang | OK. Mais la décomposition proposée correspond-elle aux contraintes réelles ? |
| **D13** | `M_Phase0_Test` à remplacer | Vrai si le matériau actuel a des défauts. Sinon peut-être juste à renommer et nettoyer |
| **D14** | Lookup compartiment dans shader = test analytique pour Craniata | Est-ce qu'une approche plus simple existe ? Par ex assigner le matériau différemment par compartiment ? |
| **D15** | Retirer `FDerivedFloodVolume.BoundsMin/Max` (champs zéro non utilisés) | Vérifier qu'aucun code ne les lit |
| **D16** | Pas de polish proto avant portage | Cohérent avec discipline ? Le proto reste utilisable pour comparaison A/B |

### 2.3 Règles de référence proposées

Règles de **flooding** (gameplay) — à confirmer avec le code `USubFloodComponent` :

- R1 : Source d'eau = breaches OU portes/hatches connectées à compartiment qui a de l'eau
- R2 : Pression hydrostatique → compartiment plein avec hatch au plafond ouverte remplit le supérieur
- R3 : Égalisation horizontale (Door ouverte) via Torricelli, débit ∝ √Δh × FlowAreaCm2
- R4 : Écoulement vertical (Hatch ouverte) géré par gravité, sens dépend des niveaux
- R5 : Pumps drainent vers extérieur

Règles d'**authoring** (types de connections) :

- A1 : `Door` → porte horizontale même deck, sync heightfield possible
- A2 : `Hatch` → écoutille verticale entre decks, **pas** de sync heightfield, écoulement via FloodComponent + injections visuelles
- A3 : `ExteriorHatch` → vers extérieur, équivalent breach si ouverte sous l'eau
- A4 : `Open` → toujours ouverte, traitée comme Door ouverte permanente

Règles de **rendu** (visuel) :

- V1 : Niveau d'eau lu strictement de `USubFloodComponent`
- V2 : Continuité visuelle pour portes horizontales ouvertes (cap meshes co-localisés + sync heightfield)
- V3 : Discontinuité assumée pour hatches verticales et exterior hatches (compensé par FX)
- V4 : FX additionnels (slosh, splash, breach jet) en couches par-dessus, pas substituts
- V5 : Mouvement du sub (pitch/roll/gravité) → injections heightfield / hors scope MVP mais à prévoir

### 2.4 Architecture de bake proposée (à valider)

Pipeline tentatif :
1. Détermine grille globale du sub (origine sub-local, dimensions, cellsize default 25 cm)
2. Pour chaque compartiment du DA :
   - Indices de grille couverts
   - Voxelisation parity raycast inside/outside
   - SDF par 8 raycasts XY
   - Marching Squares interpolated
   - Chain segments, inset, resample N=64, AlignPolygonStart
   - Tessellation rings concentriques R=3
3. Pour chaque connection du DA :
   - Calcul cellules frontière côté A et côté B (indices grille globale)
4. Sauvegarde `USubmarineWaterDef`

À challenger :
- Le bake "à l'échelle du sub" est-il faisable en un raisonnable temps ? (Craniata = 9 compartiments × 12 slices × 168×16 cellules)
- Y a-t-il des cas où la grille globale crée des incohérences (compartiments tournés, forme complexe) ?
- L'algorithme d'extension de silhouette dans embrasures est-il trivial à implémenter ?

### 2.5 Architecture runtime proposée (à valider)

Tick d'orchestration :
1. `USubFloodComponent::TickComponent` (server) → calcule niveaux, taux, replicate
2. `USubmarineWaterManager::TickComponent` (server + client) :
   - `UpdateWaterLevels` : push `WaterHeightCm` aux Renderers
   - `UpdateFlowInjections` : injections vortex/splash sur portes ouvertes avec flow > seuil
   - `UpdateBreachInjections` : injections circulaires sur breaches
   - `SynchronizeBoundaryHeightfields` : sync au bord pour portes horizontales
3. `UCompartmentWaterRenderer::TickComponent` × N (client) :
   - `TickHeightfield` (équation des ondes)
   - `PushHeightfieldToTexture`

À challenger :
- Le tick côté server du Manager est-il nécessaire ? Le visuel pourrait être 100% client-side
- L'ordre des étapes 1-2-3 est-il correct ? Ou faut-il que les étapes du Manager se fassent **avant** TickHeightfield des Renderers ?
- Le coût total est-il acceptable sur Craniata ?

### 2.6 Modifications proposées à USubFloodComponent

| Modification | Pourquoi | À challenger |
|---|---|---|
| Ajout `FFloodEdgeState.CurrentFlowRateLitersPerSec` (replicated) | Le visuel a besoin du taux exact calculé par la sim | Le calcul est-il déjà fait en interne ? Suffit-il de l'exposer ? |
| Ajout `GetFlowRateForEdge(FName)` | API d'accès | Cohérent avec les conventions API du composant |
| Ajout délégué `OnDoorStateChangedReplicated` | Pour slosh à l'ouverture côté visuel | Déjà existant peut-être ? Sinon impact réplication ? |
| Suppression `FDerivedFloodVolume.BoundsMin/Max` | Dette nettoyée | Vérifier qu'aucun code ne les lit |

---

## 3. Roadmap proposée (à valider/ajuster)

Découpage tentatif en 5 phases :

**Phase 0 — Pré-portage** : skip (pas de polish proto avant portage)

**Phase 1 — Portage minimal** (~2-3 jours)
- Module/structure pour `USubmarineWaterManager`, `USubmarineWaterDef`, `UCompartmentWaterRenderer`
- Port `URoomWaterRenderer` → `UCompartmentWaterRenderer`
- `USubmarineWaterManager::SpawnRenderers` depuis le DA
- `UpdateWaterLevels` (lecture FloodComponent)
- Validation : 9 Renderers visibles sur Craniata, niveaux pilotables

**Phase 2 — Continuité multi-compartiments** (~1-2 jours)
- Refactor bake (grille globale + extension silhouettes)
- `FDoorBakedData` (cellules frontière)
- `SynchronizeBoundaryHeightfields`
- Validation : 2 compartiments avec porte ouverte → surface continue + vague qui traverse

**Phase 3 — Écoulement visible** (~1-2 jours)
- `CurrentFlowRate` dans `FFloodEdgeState`
- `UpdateFlowInjections` (vortex/splash)
- `OnDoorStateChanged` (slosh)
- Injection breach
- Validation : scénario CompA 50% / CompB 0% → écoulement visible

**Phase 4 — Validation et polish** (~1-2 jours)
- Tuning scales
- Test multiplayer
- Profiling Craniata complet
- Documentation utilisateur

**Phase 5 — Post-MVP** (mention seulement, pas dans le plan initial)
- Sloshing inertiel, foam aux bords, caustiques, postprocess underwater, optimisations

Estimation totale Phase 1-4 : 6-8 jours.

À challenger :
- Le découpage correspond-il à des unités vraiment commitables indépendamment ?
- Les estimations sont-elles réalistes par rapport à la complexité du repo ?
- Y a-t-il des dépendances cachées entre phases qui forcent un autre ordre ?

---

## 4. Ce que l'agent externe n'a pas pu vérifier

Liste honnête de ce qui a été supposé sans confirmation :

- L'authoring actuel de Craniata utilise `InitializeFromCompartmentVolumes` (path BP) — à confirmer
- `USubFloodComponent` calcule déjà les taux d'écoulement en interne (juste pas exposés) — à confirmer
- Le matériau `M_Phase0_Test` du proto est utilisable comme base — à confirmer
- Le module `Sub3DWaterProto` est isolé du reste du code Sub3D — à confirmer
- Les `UCompartmentVolumeComponent` placés en BP de Craniata sont compatibles avec l'extraction depuis le DA — à confirmer
- Le Substrate + WPO + ProcMesh fonctionne aussi bien dans le contexte Sub3D que dans le proto isolé — à confirmer
- Les `USubInteractionComponent::TraceFromView` modifications faites pendant le proto sont mergeables proprement — à confirmer

---

## 5. Questions ouvertes pour l'agent

Au moment de produire le plan ou de proposer des améliorations, voici les questions qui méritent une réponse basée sur le code :

1. **Source d'authoring Craniata** : DA ou BP volumes ? Quel chemin de migration si les deux coexistent ?
2. **Politique de réplication** : qu'est-ce qui doit être server-authoritative dans le système eau ? Manager ? Renderers ? Heightfield ?
3. **Cycle d'init** : à quel moment le manager spawne-t-il les Renderers ? `BeginPlay` du sub ? Plus tôt ? Y a-t-il des contraintes liées au PIE ou au cooking ?
4. **Substrate matériau** : le matériau `M_Phase0_Test` est-il vraiment portable, ou y a-t-il des subtilités liées aux contextes (Niagara, postprocess) à anticiper ?
5. **Bake offline** : où s'exécute le bake ? Editor uniquement ? Y a-t-il une convention pour les outils de bake dans le repo ?
6. **Gestion des erreurs** : que faire si le bake échoue sur un compartiment ? Si une porte n'a pas de cellules frontière calculables ? Stratégies de fallback ?
7. **Proto désactivé** : le module `Sub3DWaterProto` peut-il rester compilé sans charger en runtime ? Quel mécanisme ?
8. **Performance** : le coût simulé du système est de combien ? Y a-t-il déjà un budget perf défini pour le rendu eau ?

---

## 6. Liberté de l'agent

L'agent qui consomme ce document **n'est pas obligé** de produire le plan tel que pré-mâché par la conversation externe. Il peut :

- **Proposer une architecture différente** s'il identifie une meilleure approche basée sur le code
- **Rejeter des décisions** des sections 2.x si elles sont incompatibles avec le repo
- **Réordonner les phases** si la décomposition proposée n'est pas optimale
- **Identifier des composants existants** qui pourraient absorber des responsabilités plutôt que de créer du nouveau code
- **Réduire ou augmenter le scope** selon ce qu'il observe

## 14 fondamentaux non négociables (validés par l'humain plusieurs fois dans la conversation)

Ces 14 points sont les seules contraintes que l'agent **ne doit pas modifier**. Tout le reste de l'archi tentative est ouvert à analyse et challenge.

### Vision produit

1. **Sentiment de fluide unique** entre compartiments connectés via portes ouvertes. Pas deux nappes patchées.

2. **Refus des patches qui camouflent**. La continuité doit être physiquement crédible sans bricolage visuel. Pas de bridge mesh patch, pas de cascade procédurale, pas de Niagara central qui masque la transition.

3. **FX additionnels acceptés mais pas comme substituts**. Slosh à l'ouverture/fermeture, splash impacts, breach jet — OK en couches par-dessus. Mais ils ne portent pas l'illusion principale.

4. **Le visuel suit `USubFloodComponent`**. Le gameplay flood est la source de vérité des niveaux et des flux. Le rendu lit, ne calcule pas.

### Contraintes techniques

5. **Multi-deck est une exigence**. Compartiments à des Z différents simultanément. C'est ce qui a éliminé l'option "plan d'eau global unique".

6. **Compartiments restent indépendants** au niveau simulation. Pas de fusion runtime de heightfields. Sync au bord = OK, fusion = non.

7. **Heightfield + cap mesh comme système central**. Pas de simulation voxel, pas de SPH/FLIP. L'humain a explicitement recadré quand la conversation déviait sur du voxel.

8. **Sync horizontale aux portes ouvertes** (Niveau C dans la conversation : sync au bord avec cohérence multi-portes).

9. **Pas de sync verticale**. Hatches verticales gérées par FloodComponent (gameplay) + injections visuelles seulement.

### Stratégie produit

10. **Système adaptable à plusieurs subs**. Pas spécifique Craniata. Doit fonctionner pour Craniata + futurs subs avec topologies différentes.

11. **`USubmarineDefinition` comme source de vérité topologie**. Le DA pilote, pas l'authoring BP en double.

### Posture

12. **Ne pas faire de polish supplémentaire du proto avant portage**. On porte avec l'archi cible, le proto reste comme référence.

13. **Garder le proto dans le repo désactivé** jusqu'à validation finale.

14. **Migration incrémentale par phase**. Pas de big-bang.

---

Tout le reste — manager central vs autonome, structure exacte du bake, format des données, ordre des phases, lookup compartiment dans shader, etc. — est ouvert à l'analyse de l'agent.

---

## 7. Ce que tu produis

À partir de ce dossier + ton accès au code :

1. **Analyse** : compare les propositions de la conversation à l'état réel du code. Identifie incohérences, confirmations, surprises.

2. **Recommandation architecturale** : valide ou modifie l'archi proposée. Si tu modifies, justifie en référençant le code.

3. **Plan d'implémentation** : produis un plan concret découpé en phases, avec tâches numérotées, fichiers à modifier/créer, estimations honnêtes.

4. **Décisions ouvertes** : liste explicitement ce que tu n'as pas pu trancher seul et qui demande validation humaine.

5. **Risques** : liste explicitement les risques que tu identifies au-delà de ceux mentionnés dans la conversation.

Format de sortie suggéré : un fichier `Sub3D_Water_Implementation_Plan.md` (ou nom équivalent qui suit la convention du repo).

Le plan sera reviewé par l'humain. Pas d'exécution avant validation.

---

## 8. Documents joints / références

À lire pour contexte complet :
- `Sub3D_Water_Architecture.md` — proposition d'archi détaillée (à challenger)
- `2026-05-02_water_proto_audit.md` — audit du proto avec dette
- `Sub3DWaterProto_Implementation.md` — plan original du proto (référence pour les algos)

Code à inspecter :
- `Source/Sub3DWaterProto/` — le proto à porter
- `Source/Sub3D/Submarine/SubFloodComponent.{h,cpp}` — composant flood existant
- `Source/Sub3D/Submarine/SubmarineDefinition.{h,cpp}` — DA structure
- `Content/Submarines/Craniata/DA_SubDef_Craniata.uasset` — DA Craniata authored
- BP de Craniata — pour voir l'authoring actuel des volumes

---

## 9. Ajout 2026-05-07 - Water visuals, heightfield, extensions gameplay

**Source** : conversation du 2026-05-07 apres audit du plan `2026-05-04_water_implementation_plan.md`, relecture des chemins water/flood/hull existants, et observation des captures de bake/runtime.

**Statut** : note de conception. Ne remplace pas le plan d'implementation. Les points ci-dessous doivent rester subordonnes au plan authority-max `2026-04-10_first_playable_strategic_analysis.md` et au plan water courant.

### 9.1 Socle reel exploitable

Le socle actuel permet deja d'imaginer des extensions gameplay sans partir sur une IA complete :

- Compartiments, portes et flood donnent une consequence gameplay lisible : isoler, pomper, reparer.
- Les breaches et `OnBreachesUpdated` donnent un point d'accroche naturel pour degats de coque, VFX et reparations.
- Les water planes et le heightfield prevu peuvent afficher les consequences : jet, onde, perturbation, slosh.
- Les interactables existants peuvent servir a reparer, sceller, deloger ou declencher une contre-mesure.
- Le sonar/contact system peut annoncer une menace avant qu'elle devienne visible ou qu'elle cree une fuite.
- Le sous-marin comme moving frame permet d'attacher des evenements a des positions locales stables sur la coque.

Conclusion : le bon axe court terme n'est pas "monstre interieur avec pathfinding", mais "incident exterieur localise qui devient un probleme interieur via flood, breach, sonar, VFX et interaction joueur".

### 9.2 Quick wins gameplay depuis ce socle

1. **Contact sonar + choc coque + fuite**
   - Un contact hostile apparait au sonar.
   - Le joueur percoit un choc coque.
   - Un breach apparait dans un compartiment donne.
   - Le joueur doit isoler, pomper, reparer.
   - ROI eleve : beaucoup de gameplay avec peu de nouveau systeme.

2. **Attached Hull Threat**
   - Un actor externe approche le sous-marin.
   - Il s'attache a un point ou une zone locale de coque.
   - Tant qu'il est accroche, il augmente un compteur de dommage.
   - Au seuil, il cree ou aggrave une fuite.
   - Il produit bruit sonar, VFX, vibration, alarme.
   - Le joueur peut le deloger via station, tourelle, pulse, ou action de reparation interne.
   - Machine d'etat suffisante pour une premiere version : `Approach -> Latch -> Damage -> Detach / Killed / BreachCreated`.

3. **Breach parasite**
   - Variante encore plus proche du systeme water.
   - Une menace maintient une fuite active.
   - La reparation normale ne fonctionne pas tant que la source externe reste presente.
   - Le joueur doit d'abord deloger la source, puis reparer.

4. **Perturbation d'eau liee aux incidents**
   - Breach actif : jet et onde locale.
   - Impact sur coque : pulse dans le compartiment adjacent.
   - Monstre accroche : micro-ondes repetees, coups, vibration.
   - Porte forcee ou bulkhead sous stress : onde pres de la boundary.

Elements a deferer :

- Monstre qui entre dans le sous-marin.
- IA de poursuite dans les couloirs.
- NavMesh dynamique dans le sous-marin mobile.
- Navigation physique complexe sur la surface de coque.

### 9.3 Ajouts visuels autour de l'eau

La mousse et les eclaboussures sont utiles, mais elles doivent rester des couches de feedback et de masquage visuel. Elles ne doivent pas remplacer une correction du bake ou des bounds.

Niveaux recommandes :

1. **Intersection foam court terme**
   - Mousse claire autour des intersections eau / coque / bulkhead / props.
   - Implementable via `DepthFade`, `SceneDepth`, ou distance fields si disponibles.
   - Avantage : masque vite les coupures de bord.
   - Risque : peut mousser autour de tous les props, pas seulement la coque.

2. **Foam issue du bake**
   - Ajouter une donnee `DistanceToBoundary` ou equivalent dans le bake.
   - Le material utilise ce masque : proche contour = mousse, loin contour = eau normale.
   - Plus propre, parce que la mousse suit le contour authoritatif du compartiment.

3. **Niagara pour evenements localises**
   - Eclaboussures aux breaches.
   - Gouttes et jets sur bulkheads.
   - Impacts quand une menace tape la coque.
   - Petites projections aux portes ou ouvertures.

Decision de direction :

- Foam = lecture visuelle des limites et contacts.
- Niagara = evenements ponctuels.
- Heightfield = propagation d'ondes liee au gameplay.
- Gerstner = rides ambiantes seulement.

### 9.4 Diagnostic HLSL / Gerstner actuel

Les artefacts visibles en eventail au centre du plan d'eau ressemblent surtout a une topologie de mesh revelee par le WPO, pas a une erreur de formule Gerstner.

Causes probables :

- Amplitude initiale trop forte pour une eau interieure, notamment `A1 = 10` cm pour `L1 = 50` cm.
- Deplacement horizontal Gerstner trop visible.
- Mesh de cap avec structure radiale/concentric rings/fan.
- Vagues actives des l'apparition du plan, sans fade-in.
- Normales/geometrie qui rendent les triangles trop lisibles.

Recommandations :

- Reduire fortement les amplitudes ambiantes.
- Reduire `Q`, surtout en interieur.
- Garder le deplacement horizontal XY tres faible au debut.
- Ajouter un fade-in temporel.
- Ajouter un fade par profondeur d'eau.
- Ajouter un fade pres des bords.

Exemple d'intention shader :

```hlsl
float SpawnFade = saturate(TimeSinceCreated / 2.0);
float DepthFade = saturate(WaterDepthCm / 25.0);
float EdgeFade = saturate((DistanceToBoundaryCm - 10.0) / 40.0);

float AmpMask = SpawnFade * DepthFade * EdgeFade;
GerstnerOffset *= AmpMask;

// Eau interieure : limiter le deplacement horizontal pour ne pas reveler le mesh.
GerstnerOffset.xy *= 0.1;
```

Reglage de depart recommande pour une eau interieure :

```hlsl
// Petites rides ambiantes, pas houle oceanique.
A1 = 1.2;  L1 = 90.0;  Q1 = 0.05;
A2 = 0.6;  L2 = 140.0; Q2 = 0.03;
A3 = 0.35; L3 = 220.0; Q3 = 0.02;
```

Note : l'utilisateur a deja reduit legerement les parametres. Le prochain test utile est de couper presque tout le XY displacement, puis de verifier si l'artefact radial disparait ou devient acceptable.

### 9.5 Ondes realistes, rebonds et transmissions

Gerstner seul ne doit pas porter les rebonds coque/bulkhead/portes. Le heightfield est le bon support pour les ondes gameplay.

Direction proposee :

- Chaque compartiment garde sa grille heightfield.
- Les cellules solides reflechissent l'onde.
- Les portes ouvertes transmettent une partie de l'onde.
- Les portes fermees reflechissent presque tout.
- Les breaches injectent de l'energie locale.
- Les menaces accrochees peuvent injecter des pulses repetes.

Coefficients de depart :

```text
Hull / bulkhead ferme : reflect 0.8 a 0.95
Porte fermee          : transmit 0.0 a 0.05
Porte entrouverte     : transmit 0.2 a 0.4
Porte ouverte         : transmit 0.5 a 0.8
Breach actif          : inject impulse + directional flow
```

Objectif : une vague issue d'un breach doit pouvoir taper un bulkhead, revenir, traverser partiellement une porte ouverte, ou s'amortir dans un autre compartiment.

### 9.6 Regle de priorite

Ordre recommande :

1. Corriger la donnee de bake/bounds/contour. La mousse ne doit pas cacher une mauvaise containment.
2. Calmer le Gerstner ambient pour ne pas exposer la topologie du cap mesh.
3. Ajouter `EdgeFade` / `DistanceToBoundary` et foam de bord.
4. Ajouter pulses heightfield pour breaches, impacts et portes.
5. Prototyper `Attached Hull Threat` seulement apres que breach + visual feedback soient fiables.

---

**Fin du document. Bonne analyse.**
