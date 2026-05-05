# Sub3D Water — Dossier d'analyse

**Date** : 2026-05-03
**Version** : 2.0 (allégée)
**Source** : conversation Claude.ai (Claude Opus 4.7 + humain) du 2-3 mai 2026
**Statut** : **dossier d'analyse**, pas une spec d'implémentation

> Ce document fournit le **contexte et l'analyse** issus d'une longue conversation entre l'humain et un agent externe (Claude Opus 4.7) qui n'avait pas accès au code du repo. Il ne contient **pas** de spec validée. Il contient :
> - L'analyse du problème et la vision produit
> - Le scénario de référence qui guide les décisions
> - Les alternatives examinées et pourquoi elles ont été retenues ou rejetées
> - Les données disponibles dans le DA `USubmarineDefinition`
> - Les questions architecturales ouvertes
> - Les risques identifiés
>
> Ce doc est **matière première**, pas conclusion. L'agent qui produira le plan d'implémentation l'utilise pour comprendre la trajectoire de la pensée et le contexte, puis fait ses propres choix techniques en confrontant ce qui suit au code réel.

---

## 1. Mission et scope

### 1.1 Mission

Construire le système de rendu d'eau intérieure pour Sub3D, capable de représenter de manière convaincante la masse d'eau qui circule entre les compartiments d'un sous-marin selon les états de portes (ouvertes/fermées), avec couplage fort à la simulation gameplay (`USubFloodComponent`) qui pilote les volumes et taux d'écoulement.

### 1.2 Critère de succès principal

Le joueur doit ressentir l'eau du sous-marin comme **un fluide unique cohérent** qui se redistribue naturellement entre compartiments connectés, pas comme des nappes indépendantes patchées par des FX. La continuité visuelle et la propagation des perturbations (vagues) doivent traverser les portes ouvertes sans discontinuité perceptible.

### 1.3 Scope

**Inclus** :
- Rendu de la surface d'eau dans tous les compartiments d'un sub
- Continuité visuelle entre compartiments connectés via portes ouvertes
- Écoulement visible quand delta de niveau
- Couplage avec `USubFloodComponent` comme source de vérité des niveaux
- Auto-génération du système depuis `USubmarineDefinition`
- Support multiplayer (server-authoritative pour les niveaux, client-side pour le visuel)

**Hors scope (post-MVP)** :
- Sloshing inertiel (forces de mouvement du sub appliquées au fluide)
- Caustiques projetées
- Postprocess underwater (distortion, color grading)
- Volumetric fog
- Foam particulaire dynamique avancé
- Hull breaking dynamique (système séparé qui suivra)

**À conserver comme contrainte** : l'architecture doit pouvoir accueillir le hull breaking plus tard sans refonte. La topologie des ouvertures doit pouvoir être modifiée runtime.

### 1.4 Non-objectifs explicites

- **Pas de simulation fluide réaliste AAA**. L'objectif est une masse d'eau stylisée, lisible, juicy.
- **Pas de FX qui camouflent** les transitions. Toute transition doit être physiquement crédible via le système central.
- **Pas de surcharge perf** pour des détails non perceptibles en gameplay coop.

---

## 2. Vision produit

### 2.1 Scénario de référence

Le scénario qui guide toutes les décisions techniques :

> Le joueur se trouve dans le **Compartiment B** (vide, niveau 0). Le **Compartiment A** voisin est rempli à 50%. Le joueur ouvre la porte qui sépare A et B. La porte met 0.25-0.5 secondes à s'ouvrir. Voici ce qui doit se passer :
>
> 1. Dès que la porte commence à s'ouvrir (fente), un filet d'eau passe par la fente avec pression
> 2. À mesure que la porte s'ouvre, l'écoulement augmente
> 3. L'eau de A se déverse vers B en générant des vagues à l'arrivée
> 4. Le niveau de A baisse, celui de B monte, jusqu'à équilibrage (~25% partout)
> 5. Pendant tout ce temps, la surface d'eau dans A et B doit donner l'impression d'être **un seul plan continu** quand vu à travers la porte
> 6. Les vagues générées par l'arrivée de l'eau dans B se propagent **dans B et à travers la porte vers A**
> 7. Quand niveaux égalisés, la surface est strictement continue d'un compartiment à l'autre, sans patch ni discontinuité

Ce scénario fixe les exigences. Tout système qui ne satisferait pas ces 7 points n'est pas acceptable pour Sub3D.

### 2.2 Cible visuelle

L'objectif n'est pas le réalisme fluide AAA mais une **masse d'eau stylisée** qui :
- Se lit clairement à l'écran (lisibilité gameplay prioritaire)
- Réagit visuellement aux événements (impacts, ouvertures, breaches, mouvements du sub)
- Génère des courants, déplacements, agitations crédibles
- Donne du feedback "juicy" au joueur
- Supporte des effets de surface (refraction, foam aux bords) en couches additionnelles

Le système peut être **rapide et exagéré** dans ses comportements tant que ça sert le gameplay. Les FX additionnels (Niagara, particules) viendront en couches par-dessus, pas comme substituts.

### 2.3 Couplage avec gameplay

`USubFloodComponent` calcule déjà :
- Volumes d'eau par compartiment
- Niveaux d'eau (`WaterLevelNormalized`, `WaterHeightCm`)
- Taux d'écoulement entre compartiments connectés
- État des portes (`bClosed`)
- Breaches (taux d'inflow extérieur)

Le système de rendu **lit** ces données et les traduit en visuel. Il ne recalcule rien. Cette discipline garantit que tout changement gameplay se reflète automatiquement dans le visuel.

---

## 3. Alternatives examinées dans la conversation

Durant la conversation, plusieurs approches ont été explorées et triées. Cette section liste **ce qui a été rejeté et pourquoi**, pour que l'agent comprenne la trajectoire de la pensée et n'ait pas à refaire le débat.

### 3.1 Plan d'eau global unique pour le sub entier

**Rejeté.** Le multi-deck rend cette approche impossible : un sub peut avoir un compartiment au deck supérieur inondé à 80% et un compartiment au deck inférieur à 30% simultanément. Il y a donc **plusieurs surfaces d'eau coexistantes à des Z différents**, ce qui contredit l'idée d'un plan global unique.

### 3.2 Fusion runtime de heightfields

**Rejeté.** L'idée était de fusionner dynamiquement les heightfields de compartiments connectés en un seul buffer quand portes ouvertes, et de splitter quand fermées. Trop complexe, pas de gain prouvé sur les approches plus simples.

### 3.3 Plans empilés alignés sur grille

**Rejeté.** Même limite que 3.1 — multi-deck nécessite plusieurs plans à des Z différents, pas une grille 2D unifiée.

### 3.4 Patch mesh aux portes (approche du proto Étape B)

**Rejeté.** L'humain refuse les patches qui camouflent visuellement les transitions. La continuité doit être physiquement crédible sans bricolage visible.

### 3.5 Cascade procédurale runtime

**Rejeté.** Idem — l'écoulement par les portes ne doit pas être un FX additionnel qui masque, mais émerger du comportement de la masse d'eau elle-même.

### 3.6 Simulation voxel cellulaire (cellular automaton)

**Considéré, recadré.** L'humain a explicitement écarté la simulation voxel comme système central pendant la conversation. La position : rester sur des approches plus simples (heightfield, surfaces maillées) et laisser l'agent local trancher si une autre approche serait meilleure.

> Note pour l'agent : le rapport de recherche externe (`deep-research-report.md`) considère le voxel/SPH/FLIP comme **risky en système central** mais utilisable **localement** pour des effets ponctuels. Cohérent avec la position humaine.

### 3.7 Simulation SPH/FLIP

**Rejeté comme système central.** Trop coûteux, complexité numérique, pas adapté au scope. Acceptable comme couche locale pour set-pieces.

### 3.8 Solveur 2D shallow-water local par compartiment

**Pas exploré dans la conversation, mais évoqué dans le rapport de recherche externe** comme "GO stratégique en seconde marche". À considérer par l'agent si une approche heightfield+sync s'avère insuffisante.

---

## 4. Données disponibles : SubmarineDefinition

### 4.1 Vue d'ensemble

`USubmarineDefinition` est un UDataAsset existant qui contient toute la topologie d'un sub. Il est déjà populé pour Craniata.

Selon l'audit du DA Craniata fait pendant la conversation :
- ✅ 9 compartiments authored avec bounds spatiales complètes (`HydroBoundsMin/Max`)
- ✅ 9 connections (Doors, Hatches, ExteriorHatch, Open) avec `LocalTransform`, `FlowAreaCm2`, `DoorWidthCm/HeightCm`
- ✅ FloodGraph avec topologie de connexité
- ✅ Validation `IsValid()` passe

### 4.2 Champs utiles pour le système eau

| Donnée DA | Usage potentiel |
|---|---|
| `Compartments[i].CompartmentId` | Identité unique |
| `Compartments[i].HydroBoundsMin/Max` | Volume à voxeliser au bake. Spawn de volumes runtime |
| `Compartments[i].MaxWaterHeightCm` | Hauteur max d'eau |
| `Compartments[i].WalkableFloorZCm` | Plancher en local |
| `Connections[i].ConnectionId` | Identité de la porte/hatch |
| `Connections[i].CompartmentA/B` | Lookup des compartiments connectés |
| `Connections[i].LocalTransform` | Position de la porte en sub-local |
| `Connections[i].FlowAreaCm2` | Aire de passage (Torricelli) |
| `Connections[i].DoorWidthCm/HeightCm` | Dimensions pour calculs visuels |
| `Connections[i].ConnectionType` | Door, Hatch, ExteriorHatch, Open |
| `Connections[i].bStartsClosed` | État initial |
| `FloodGraph.Edges[i]` | Topologie graph connexe |

### 4.3 Dette identifiée dans la conversation

| Dette | Recommandation |
|---|---|
| `FDerivedFloodVolume.BoundsMin/Max` non rempli | À nettoyer (retirer ou populer). Décision humaine : retirer si non utilisé |
| Deux sources de truth (DA vs `UCompartmentVolumeComponent` placés manuellement sur BP) | À trancher. Position conversation : DA = source unique. À confirmer en regardant le code |
| `SemanticType` peu utilisé | Pas une dette du système eau |
| Mesh sections vides (`InteriorMeshes`, `BulkheadMeshes`) | Pas un problème pour le système eau (Craniata est handmade) |

### 4.4 Questions ouvertes sur l'authoring

- L'authoring actuel de Craniata utilise-t-il `InitializeFromDefinition` ou `InitializeFromCompartmentVolumes` ?
- Y a-t-il déjà une migration prévue ou en cours vers DA-source-unique ?
- Le DA est-il consommé par d'autres systèmes en parallèle (autres que `USubFloodComponent`) ?

---

## 5. Couplage avec USubFloodComponent

### 5.1 Données existantes utilisables

D'après le header `SubFloodComponent.h` (transmis pendant la conversation) :

```
GetCompartmentWaterHeightCm(CompartmentId)  // niveau en cm
GetCompartmentFloodLevel01(CompartmentId)   // niveau normalisé 0..1
GetCompartmentWaterLiters(CompartmentId)
EdgeStates[]  // topologie + état portes (bClosed, PassageAreaCm2, etc.)
GetBreaches() // breaches actives avec position et taux
```

### 5.2 Modifications potentielles à USubFloodComponent

Pendant la conversation, deux ajouts ont été évoqués :

**Ajout taux d'écoulement par edge** : pour que le visuel reflète exactement le taux calculé par la sim. Mais à confirmer — peut-être le calcul est-il déjà fait en interne et il suffit de l'exposer.

**Ajout délégué OnDoorStateChangedReplicated** : pour déclencher des effets visuels (slosh) au moment exact de la transition. À vérifier si un mécanisme équivalent existe déjà via la réplication.

### 5.3 Questions ouvertes

- Le calcul des taux d'écoulement est-il déjà fait dans `AdvanceFlooding` ?
- La réplication actuelle expose-t-elle suffisamment d'info aux clients pour le visuel ?
- Y a-t-il un budget de modification de `USubFloodComponent` accepté par l'équipe ?

---

## 6. Questions architecturales ouvertes

Cette section liste les questions auxquelles l'agent doit répondre dans sa proposition d'architecture, en se basant sur le code et son analyse.

### 6.1 Granularité et organisation

- Combien de "rendeurs d'eau" par sub ? Un par compartiment ? Un global avec masques ? Hybride ?
- Les compartiments doivent-ils être des entités runtime distinctes ou des "régions" d'une entité globale ?
- Un manager central est-il nécessaire ou chaque compartiment peut-il être autonome ?

### 6.2 Représentation de la masse d'eau

- Heightfield 2D par compartiment (proto actuel) ?
- Solveur shallow-water local (suggéré dans le rapport de recherche) ?
- Surface maillée + slosh modal sans simulation continue ?
- Approche hybride avec couche d'agitation au-dessus d'une représentation simple ?

### 6.3 Continuité visuelle aux portes

- Sync au bord des heightfields (proposé par l'agent externe) ?
- Connecteurs visuels explicites avec flow maps (suggéré dans le rapport de recherche) ?
- Co-localisation de vertices au plan médian ?
- Combinaison ?

### 6.4 Multi-deck

- Comment gérer plusieurs decks dans le même sub avec hatches verticales ?
- Approche par "deck Z" indépendants ?
- Approche entièrement 3D ?
- Cascade visuelle aux hatches ouvertes ?

### 6.5 Bake vs runtime

- Quel niveau de précalcul est acceptable ?
- Le bake doit-il être par compartiment, par sub, ou hybride ?
- Quels assets persistants vs assets transitoires ?
- Comment gérer l'évolution future (hull breaking) qui ajoute des ouvertures runtime ?

### 6.6 Authoring et industrialisation

- L'archi proposée doit-elle être adaptée à plusieurs subs futurs sans refactor majeur ?
- Quelle stratégie d'auto-génération depuis `USubmarineDefinition` ?
- Quel niveau de validation/debug intégré (la conversation a montré qu'un système debug solide est essentiel) ?

### 6.7 Couplage shader

- Comment le matériau eau accède-t-il aux niveaux par compartiment ?
- MaterialParameterCollection ? Custom Primitive Data ? Lookup spatial ?
- Single Layer Water vs Substrate vs translucent classique ?

### 6.8 Niagara et FX additionnels

- Quels événements gameplay déclenchent du Niagara ?
- Niagara Data Channels pour mutualiser les events ?
- Quelle stratégie pour les FX par compartiment vs globaux ?

---

## 7. Migration depuis le proto — principes

### 7.1 État du proto

Le proto `Sub3DWaterProto` a validé en isolation :
- Pipeline algo : voxelisation parity raycast + SDF + Marching Squares interpolated + tessellation rings concentriques + blending entre slices
- Heightfield CPU 2D + équation des ondes + push texture R32_FLOAT
- Animation ambient via Custom HLSL Gerstner
- Architecture découplée Renderer / Baker / Bridge

Étape A du proto : ✅ validée. Étape B : partiellement, avec dette assumée (bridge mesh statique, skirts verticaux non implémentés).

### 7.2 Principes de migration

L'humain a explicitement validé les principes suivants :

- **Pas de polish supplémentaire du proto avant portage**. Le bridge mesh actuel reste imparfait.
- **Garder le proto dans le repo désactivé** jusqu'à validation finale du portage. Permet comparaison A/B.
- **Migration incrémentale par phase**, pas big-bang. Sauf si une refonte globale s'avère plus simple.

### 7.3 Ce qui peut être conservé (à confirmer par l'agent)

L'algorithme de bake et de tessellation du proto est validé techniquement. Si l'archi finale conserve une approche heightfield + cap mesh, ce code peut probablement être porté avec adaptations.

L'agent doit cependant **considérer librement** des architectures qui rendraient ce code inutile, si elles sont meilleures pour le gameplay.

### 7.4 Ce qui ne sera probablement pas porté tel quel

- `ARoomActor` (conteneur de test, spécifique au proto)
- `UDoorWaterBridge` du proto (la logique sera redistribuée selon l'archi finale)
- Bridge mesh statique
- `M_Phase0_Test` (matériau de test)

---

## 8. Risques identifiés

### 8.1 Risques techniques

| Risque | Mitigation possible |
|---|---|
| Coût heightfield/simulation × N compartiments en multiplayer 16 joueurs | LOD, désactivation pour compartiments hors vue. À budgétiser tôt |
| Continuité visuelle aux portes : approche techniquement difficile à parfaire | Accepter une imperfection résiduelle compensée par FX, ou investir lourd sur la continuité |
| Multi-deck : si plusieurs heightfields à des Z différents, sync inutile mais coût présent | Architecture qui distingue connexions horizontales (sync) vs verticales (gravité + FX) |
| Substrate + WPO + ProcMesh sur surface translucide large | Validé par le proto, mais à reconfirmer dans contexte Sub3D (lighting, fog) |
| Multiplayer désync visuelle entre clients | Acceptable si les niveaux sont répliqués (gameplay), heightfield peut être local |

### 8.2 Risques produit

| Risque | Mitigation possible |
|---|---|
| L'eau ne donne pas la sensation "fluide unique" même avec sync au bord | Investir sur des approches alternatives (shallow-water, modal slosh) si insuffisant |
| Le système rend bien sur Craniata mais pas sur subs futurs avec topologies différentes | Tester tôt sur géométries variées, design adaptable |
| Hull breaking futur incompatible avec l'archi choisie | Anticiper la possibilité d'ouvertures runtime non bakées dès le design |

### 8.3 Risques d'authoring

| Risque | Mitigation possible |
|---|---|
| Conflit entre DA-source-unique et authoring BP existant | Stratégie de migration explicite, période transitoire avec warnings |
| Outil de bake long ou complexe à utiliser | Editor Utility Widget bien designé, validation automatique |
| Évolution du système nécessite re-bake fréquent | Validation rapide, hash de versioning |

---

## 9. Références utiles

### 9.1 Documents de la conversation

- `Sub3D_Water_Conversation_Notes.md` — synthèse de la conversation avec les fondamentaux validés
- `2026-05-02_water_proto_audit.md` — audit complet du proto
- `Sub3DWaterProto_Implementation.md` — plan original du proto

### 9.2 Recherche externe

- `deep-research-report.md` (rapport ChatGPT deep research) — panorama des approches d'eau temps réel, classification GO/RISKY/NO-GO, suggestions d'architectures Alpha/Beta/Gamma

### 9.3 Code source à inspecter

- `Source/Sub3DWaterProto/` — proto à porter ou archiver
- `Source/Sub3D/Submarine/SubFloodComponent.{h,cpp}` — flood existant
- `Source/Sub3D/Submarine/SubmarineDefinition.{h,cpp}` — DA structure
- `Source/Sub3D/Submarine/SubHullBoundaryComponent.{h,cpp}` — frontières de coque (utile pour comprendre breach et anticiper hull breaking)
- `Content/Submarines/Craniata/` — BP et DA Craniata

---

## 10. Note finale à l'agent

Ce document fournit le **contexte** issu d'une conversation longue. Il ne fournit **pas** la spec.

Tu as l'avantage sur l'agent externe : tu lis le code, tu vois ce qui existe vraiment, tu peux proposer des architectures impossibles à concevoir sans cette connaissance.

Le rapport de recherche externe (Architecture Alpha / Beta / Gamma) est probablement une bonne source d'inspiration alternative à la proposition de la conversation Claude. À toi de juger quelle direction est la meilleure pour Sub3D **selon le code que tu vois**.

Les fondamentaux dans `Sub3D_Water_Conversation_Notes.md` sont les seules contraintes non négociables. Tout le reste — architectures, choix techniques, structures de données, naming, roadmap — t'appartient.

Bonne analyse.
