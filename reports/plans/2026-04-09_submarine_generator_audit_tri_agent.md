# Rapport Review 3 Agents - Sous-marin Générateur (Phases 1 à 5)

Ce document consolide les audits de code réalisés sur l'implémentation des phases 1 à 5 du générateur Sub3D, en croisant les retours de trois agents d'assistance (Opus 4.6, GPT 5.4, Gemini 3.1 Pro).

---

## Review OPUS 4.6 (Phases 1 à 5)

**HIGH**
*   **H1 (5B - SubmarineMeshBuilder.cpp):** *Double taper on exterior hull.* Les rayons des knot rings incluent déjà `EvaluateBowSternTaper`, puis ce taper est réappliqué après la subdivision. La coque (bow/stern) reçoit le taper au carré.
*   **H2 (5B - SubmarineMeshBuilder.cpp):** *Hatch type falls to default.* `EConnectionType::Hatch` correspond de façon erronée au défaut `EPassageType::WatertightDoor` au lieu d'un vrai Hatch. Manque l'explicit case.
*   **H3 (1-4 - SubFloodComponent.cpp):** *bClientInitialized not reset on empty.* Si le serveur réinitialise, le client OnRep voit toujours true et ne refire jamais `OnFloodInitialized`. Les consumers bloquent sur de l'ancienne donnée.
*   **H4 (Cross - Tous les actors runtime):** *Mesh data generated but never consumed.* `ExteriorHullMesh`, `InteriorMeshes`, `BulkheadMeshes` sont calculées par le builder mais aucun acteur runtime ne les lit pour construire son `UProceduralMeshComponent`.

**MEDIUM**
*   **M1 (1-4 - SubFloodComponent.h):** *FloodRateIn/FloodRateOut force dirty every tick.* Les valeurs transitoires résident dans le struct répliqué, forçant la réplication totale réseau chaque frame, causant un gaspillage de bande passante.
*   **M2 (5A - SubmarineGenerator.cpp):** Si le sous-marin n'a qu'un compartiment, il est taggé "Generic" et non "Helm", nommant le compartiment `Compartment_0` et causant potentiellement un fallback sur le placement des stations.
*   **M3 (Cross):** Mapping EPassageType incomplet. Hatch et ExteriorHatch non explicitement mappés.
*   **M4 (Cross):** Les dimensions de l'Airlock sont hardcodées (4 constexpr au lieu d'être lues depuis le Spec).
*   **M5 (Cross):** Chaîne d'inclusion transitive. `SubmarineDefinition.h` tire `ProceduralMeshComponent.h` par erreur héritée de la géométrie builder.
*   **M6 (Cross - SubmarineDefinition.h):** `DefaultPumpRateLitersPerSec` est une "dead data" jamais lue par les consommateurs.
*   **M7 (1-4 - SubCrewCharacter.cpp):** La dépendance au SubHull (oracle spatial) du crew n'est pas proprement signalée par des TODO / tags de la phase 5.

**LOW**
*   **L1:** Airlock flow area vs dimensions mesh non appariés (14400 vs 16200).
*   **L2:** `bClientInitialized` setté sur l'authority, nom porteur de confusion.
*   **L3:** `IsInitialized()` vérifie `Num()>0` alors que le flag est asynchrone (potentiel désaccord d'état).
*   **L4:** Pas de garde contre les duplicates bulkheads rapprochés en spec.
*   **L5:** Limite hardcodée à 2 spawns points, bien que suffisant pour le moment (FP).
*   **L6:** Flux/pump liés aux valeurs hardcodées, ignorant la définition `USubmarineGeneratorSpec`.
*   **L7:** Manque de validation mesh (HasMeshData) sur la définition finale.

---

## Review GPT 5.4 (Phase 5b uniquement)

**HIGH**
*   L'Airlock existe bien dans la définition et le flood graph, mais **aucune géométrie n'est générée** pour lui dans la phase 5B (exclu par SubmarineMeshBuilder aux lignes 101, 168, 434, 438, 680, 689, 696). C'est un point bloquant pour le Playable, le joueur ne peut pas le traverser visuellement.
*   **Double Taper des extremités :** Confirmation du diagnostic d'Opus. Le taper du "bow" et du "stern" s'évalue puis se multiplie récursivement, créant un pincement esthétiquement incorrect qui casse la concordance logique du volume de la coque.

**MEDIUM**
*   L'`USubmarineDefinition` casse la pureté de son contrat data en réintroduisant une dépendance C++ (include) directe vers du mesh modeling : instanciant `SubCompilerTypes.h` et `ProceduralMeshComponent.h`. 

---

## Review GPT 5.4 (Phases 1 à 5 complet)

**HIGH**
*   **Portes débranchées du Flood Path :** L'actuel basculement runtime `SubDoorActor` ne touche qu'à l'ancienne façade `SubmarineCompartmentComponent`. Le graphe de `USubFloodComponent::SetDoorState` n'est appelé null part. Fermer une porte ne bloque donc pas l'eau.
*   **Bridge de dommage inexistant :** Le lien de transfert "Hull Breach -> Flood Inflow" manque. `SubHull->OnBreachesUpdated` notifie simplement la VFX. L'eau ne montera jamais suite à un vrai choc de collision in-game.

**MEDIUM**
*   **Silos morts (Données) :** Les `StationSlots` et `SpawnPoints` calculés résident en mémoire mais le runtime ne les exploite pas. `SubmarineBase` instancie toujours selon ses sockets editor arbitraires. La data de la DataAsset n'a pas détrôné l'ancienne vérité des acteurs.
*   **Cible Pompe erronée :** Le système de pompage cible `HullMain`. Le nouveau générateur produit `Helm`, `Crew`, `Engine`. L'action pompe tournera donc en silent fail car le compartiment cible n'existe pas.
*   **Dépendance Spatial SubCrewCharacter :** Pour immerger les PNJ, le `SubCrewCharacter` s'en remet encore à `SubHullComponent` spatialement avant de demander l'eau réelle.

**LOW**
*   Façade `SubmarineCompartmentComponent` : Confusion d'unités persistante (GetTotalWaterMassKg est un alias masqué de litres).

---

## Review Gemini 3.1 Pro (Audit final Phases 1-5)

Après croisement des audits Opus et GPT-5.4, mon focus architectural relève les points de friction profonds suivants à affiner avant de passer à l'implémentation finale pour sécuriser le Proto03 :

**1. Empreinte Mémoire des UDataAsset (Architecture Globale) :**
L'inclusion directe de géométries massives (`TArray<FVector>` pour les mesh vertices, UProceduralMesh params) dans `USubmarineDefinition` risque d'alourdir considérablement la mémoire au runtime. Un `UDataAsset` UE charge tous ses arrays en mémoire lors de son accès. **Action recommandée :** Exclure l'include de `ProceduralMeshComponent.h` de la Definition. Si on stocke la forme brute pour collision et Rendu au niveau d'un Asset, ce devrait être dans un container Mesh distinct qui est instancié à la compilation (ex: StaticMesh), et ne garder que la "Sémantique Méta" (Bounds, Id, Nodes) dans l'UDataAsset.

**2. Synchronisation de la boucle 'Tick' du réseau de fuite d'eau (Flood Graph) :**
Le flagrant constat qu'Opus dresse sur `FloodRateIn/Out` qui "force le dirty chaque tick" de l'objet de réplication confirme qu'un acteur réplique passivement un gradient entier d'eau. La simulation d'eau ne devrait envoyer des Deltas réseaux que lorsque l'évolution change de seuil critique (ou via un RPC de state-snapshot régulier). 
*Solution à planifier :* Laisser le tick calculer le transfert d'eau localement sur les Clients via les `EdgeStates`, et faire du serveur l'arbitre des portes/brèches. Si l'inflow et les edges sont synchros, le volume d'eau simulé sur le client correspondra sans forcer le réseau à pousser du `float` frame-par-frame.

**3. Manque du "Resolver de Breach" Spatiale (High Impact) :**
S'appuyant sur l'audit 1-5 GPT : Quand le `SubHullComponent` crée une brèche physique 3D, comment la convertit-il en `CompartmentId` ? Il faut impérativement une fonction `FGeneratedCompartmentDef* FindCompartmentAtLocation(WorldPos)` dans la routine du `USubFloodComponent`. Sans elle, l'intention de combler "Bridge de dommage inexistant" bloquera.

**Conclusion de Synthèse :** 
L'ensemble de l'architecture "Data-Driven" (Phases 1-3) tourne, mais la "Dé-façaderisation" (Phases 4-5) a échoué. Le générateur crée la définition, mais les acteurs runtime ignorent cette définition, car le câblage n'est pas fait (Portes, Pompes, Spawns, Collision Hull). La prochaine étape n'est **pas d'écrire de la logique métier (nouvelle feature)**, mais purement une **Phase d'Intégration et Nettoyage de contrat (Wiring)** avec les C++ existants.
