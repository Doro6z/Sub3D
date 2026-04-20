# Sub Fluidity Architecture — Roadmap 100% Smooth — 2026-04-18

Référence d'autorité projet : [reports/plans/2026-04-10_first_playable_strategic_analysis.md](/C:/Dev/Sub3D/reports/plans/2026-04-10_first_playable_strategic_analysis.md)
Note complémentaire (déjà codée) : [reports/analysis/2026-04-18_sub_movement_smoothing_options.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_sub_movement_smoothing_options.md)

---

## Objet

Formaliser pourquoi le sous-marin continue à saccader malgré les options A/B/C déjà implémentées, et lister concrètement ce qu'il reste à faire pour atteindre un feeling 100% lisse côté client.

Le doc `sub_movement_smoothing_options` traitait du comportement au contact.
Ce doc traite de la fluidité **hors contact**, à vitesse de croisière, partout — là où reste du jitter structurel.

---

## TL;DR

- Les options A/B/C déjà codées réduisent le jitter de **contact**.
- Le jitter **hors contact** n'est pas traité sur le **path autorité-locale** (standalone + listen-server). Il vient d'un choix structurel : l'**extrapolation** visuelle entre deux pas de sim, dans [SubMovementComponent.cpp:176](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:176).
- Le path remote (clients distants), lui, utilise déjà un dual-buffer `PrevSnapshot` → `TargetSnapshot` : [SubMovementComponent.h:303](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:303) et lerp [SubMovementComponent.cpp:785](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:785). Un problème résiduel plus petit y subsiste (slew angulaire non plafonné, cf. Point 8). Le teleport `InterpSnapDistanceCm` n'est **pas** un problème — c'est un filet de sécurité pour rattrapages catastrophiques à garder tel quel.
- Pour viser 100% lisse côté autorité-locale, il faut **porter le même modèle de présentation au path local** : bufferiser deux sorties de sim consécutives et lerp entre elles. Coût : ≈ 16.6 ms de latence visuelle à 60 Hz. Gain : lissage total.
- Les autres leviers (caméra late-update, découplage mesh visuel, prédiction helmsman, crew en frame passenger) sont des gains additionnels, plus petits individuellement mais cumulés significatifs.

---

## Constat fondamental

Le sim tourne à pas fixe `FixedSimulationHz = 60` : [SubMovementComponent.h:188](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:188).
Le render tourne à framerate variable (30 → 144 Hz).

Entre deux pas de sim, le code **extrapole** vers l'avant :
- [SubMovementComponent.cpp:186](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:186)

```
ExtrapolatedLocation = AuthoritativeLocation + Velocity * SimAccumulator
ExtrapolatedRotation.Yaw   += YawRateDegPerSec   * SimAccumulator
ExtrapolatedRotation.Pitch += PitchRateDegPerSec * SimAccumulator
```

Cette pose extrapolée est affichée au render, puis **annulée** en début du tick suivant avant que le sim ne reprenne la main :
- [SubMovementComponent.cpp:119](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:119)

C'est ce cycle `predict → undo → re-predict` qui produit le jitter quand la vélocité change entre deux pas.

---

## Pourquoi 100% smooth est impossible avec extrapolation

L'extrapolation est une **prédiction linéaire** : position future = position actuelle + vitesse × temps.

Elle est exacte **seulement si** la vitesse et le taux angulaire restent parfaitement constants entre le moment de la prédiction et le moment de la vérification.

Or notre sim applique en permanence :
- drag quadratique par axe local ([SubMovementComponent.cpp:281](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:281))
- damping idle sur la vitesse avant ([SubMovementComponent.cpp:365](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:365))
- couplage pitch → vertical ([SubMovementComponent.cpp:344](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:344))
- restauration BG sur le pitch ([SubMovementComponent.cpp:325](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:325))
- damping yaw rate ([SubMovementComponent.cpp:296](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:296))

Donc la vitesse change **à chaque pas de sim**, par construction. L'extrapolation à `Velocity × SimAccumulator` est **systématiquement fausse**, d'un petit montant par pas. Ce petit montant × vitesse = l'amplitude du jitter visible.

**Tu ne peux pas tuner l'extrapolation vers 0. Tu ne peux que la remplacer.**

---

## Ce que l'architecture actuelle fait bien

Avant de proposer des ajouts, inventaire de ce qui est déjà en place et ne demande pas à être touché :

- Sim authoritative serveur à pas fixe 60 Hz : [SubMovementComponent.cpp:160](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:160)
- Snapshot répliqué `FSubmarineNetState` : [SubmarineRuntimeTypes.h:88](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineRuntimeTypes.h:88)
- Interp + extrapolation clamped côté client distant (`InterpolateClient`) : [SubMovementComponent.cpp:773](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:773)
- `USubInteriorFrameComponent` expose `WorldToLocal` / `LocalToWorld` et le delta sub : [SubInteriorFrameComponent.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.h)
- Options A/B/C contact-aware : [reports/analysis/2026-04-18_sub_movement_smoothing_options.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_sub_movement_smoothing_options.md)
- Sub-crew yaw compensation (manuel mais fonctionnel) : [SubCrewMovementComponent.cpp:376](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:376)

Ce socle est sain. Les fixes proposés ci-dessous viennent **sur** ce socle, pas à la place.

---

## Sources de jitter restantes, classées par impact

### 1. Extrapolation visuelle sur le path autorité-locale — **impact majeur**

**Portée :** path autorité-locale uniquement (standalone, listen-server host, pawn autoritaire). Le path remote a déjà une interpolation entre snapshots reçus — voir [SubMovementComponent.cpp:785](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:785).
**Source :** [SubMovementComponent.cpp:186](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:186).
**Problème :** cf. section `Pourquoi 100% smooth est impossible`. Entre deux pas de sim, la pose affichée est prédite à `Velocity × SimAccumulator`, puis corrigée au pas suivant quand la vitesse réelle a changé.
**Fix :** bufferiser deux sorties de sim consécutives au niveau du path local (`PrevSimLocation`, `CurrSimLocation`) et afficher `Lerp(Prev, Curr, SimAccumulator / FixedSimDt)`. Même principe que le dual-buffer remote, mais appliqué à la fréquence sim au lieu de la fréquence snapshot.

Les deux points sont connus, zéro prédiction. Les changements de vitesse entre pas disparaissent visuellement.

**Couplage avec le mécanisme d'undo existant :** aujourd'hui l'undo d'extrapolation en début de tick ([SubMovementComponent.cpp:119](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:119)) restaure la pose authoritative avant que le sim reprenne. Si on écrit la pose interpolée sur l'actor root, il faut un undo-interpolation symétrique qui restaure `CurrSim` en début de tick. Deux chemins d'implémentation possibles :
- **Chemin A** (Phase 1 seule) : conserver le pattern actuel, écrire la pose interpolée sur l'actor root, renommer la sémantique de `bHasVisualExtrapolation` en `bHasVisualOffset` et restaurer `CurrSim` au début du tick suivant. Pas de nouveau scene component.
- **Chemin B** (Phase 1 + 3 fusionnées) : introduire `VisualRoot` (cf. Point 3), laisser l'actor root sur `CurrSim`, faire vivre l'interpolation sur le `VisualRoot`. Élimine le besoin d'undo. Plus de lignes au total mais logique plus propre.

Phase 1 part sur Chemin A par défaut (moins intrusif, ship plus vite). Chemin B peut être retenu si Point 3 est programmé dans la foulée.

**Coût :** 1 pas de sim de latence visuelle = 16.6 ms à 60 Hz. Invisible en coop chill.
**Taille de patch (Chemin A) :** ≈ 30-40 lignes dans `TickComponent` + 4 membres d'état (`PrevSimLocation`, `PrevSimRotation`, `CurrSimLocation`, `CurrSimRotation`) + renommage flag.
**Risque :** faible. Aucune API externe, aucun contrat réseau modifié.
**Priorité :** **P0**.

### 2. Caméra non late-updated — **impact moyen à gros selon le FPS**

**Problème :** l'actor est mis à jour en fin de tick. La caméra lit le transform au moment où l'engine la sample (peut être avant l'update ou après, selon l'ordre de tick). Une frame de décalage ponctuelle entre sub et caméra = perception de flottement.
**Fix :** override `ACharacter::CalcCamera` ou `FSceneViewExtensionBase::SetupView` pour recalculer la caméra **à l'instant du render**, en lisant la dernière pose interpolée.
**Coût :** négligeable, 1 frame de cohérence gagnée systématiquement.
**Taille de patch :** ≈ 50 lignes.
**Priorité :** **P0**. Complémentaire au point 1.

### 3. Mesh visuel couplé à la racine physique — **impact moyen**

**Problème :** la racine actor est la racine physique. Chaque correction de collision (dépénétration, slide) snappe la racine. Le mesh visible suit en rigide.
**Fix :** introduire un `VisualRoot` (scene component enfant) qui interpole en continu vers la racine physique avec un taux de rattrapage borné. Le mesh visible attaché au `VisualRoot` glisse doucement sur les micro-corrections.
**Coût :** petit décalage visuel (mesh en retard de quelques cm sur la physique) — invisible si taux de rattrapage bien choisi.
**Taille de patch :** ≈ 80-100 lignes.
**Risque :** moyen. Il faut vérifier que les sockets (turrets, caméras TPS) suivent le `VisualRoot` et non la racine physique.
**Priorité :** **P1**.

### 4. Invariant d'ordre de tick entre sim, réplication et extrapolation — **contrainte d'implémentation**

> Note : ce point **n'est pas une source de jitter indépendante**. C'est une contrainte à respecter pendant le refactor du Point 1. Rangé ici par proximité de code, pas par nature.

**État actuel :** `RefreshRepState` samples directement `GetActorLocation()` / `GetActorRotation()` sur l'actor : [SubmarineBase.cpp:1018](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:1018). Ce n'est **pas** un read de `AuthoritativeLocation` stocké.
**Pourquoi c'est safe aujourd'hui :** l'ordre dans `TickComponent` appelle `RefreshRepState` **avant** que l'extrapolation visuelle écrive la pose sur l'actor — [SubMovementComponent.cpp:168](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:168). La pose samplée est donc bien l'état de fin de sim, non-extrapolé.
**Risque après refactor du point 1 :** quand on remplacera l'extrapolation par l'interpolation dual-buffer, la pose affichée deviendra une pose intermédiaire (entre `PrevSim` et `CurrSim`). L'actor root peut soit rester sur `CurrSim` (et c'est un `VisualRoot` enfant qui interpole — cf. point 3), soit être directement déplacé sur la pose interpolée. Dans le second cas, il faut **soit** préserver strictement l'ordre `RefreshRepState` avant écriture visuelle, **soit** faire lire à `RefreshRepState` une source explicite (`CurrSim`) plutôt que `GetActorLocation()`.
**Fix :** au moment du refactor point 1, choisir explicitement la source de vérité que `RefreshRepState` doit sampler, et documenter l'invariant.
**Priorité :** **P1** (couplé au point 1, pas indépendant).

### 5. Absence de prédiction client-side pour le helmsman — **impact gros sur le pilote**

**Portée :** ne concerne que la topologie **dedicated-server + remote-client**. En standalone ou listen-server host, le helmsman local **est** l'autorité — rien à prédire, la fluidité vient intégralement des Points 1/2/3/6.

**Problème :** la chaîne `input local → ServerRPC → sim → replication → interp` impose une latence perçue ≈ 1 pas de sim + 1 trame réseau, même en 0 ping. Le pilote sent son rudder comme "collant".
**Fix :** le helmsman tourne **sa propre copie** du sim en local comme prédiction. Le serveur reste autoritaire. Correction douce (slew, pas snap) en cas de divergence au-delà d'un seuil.
**Coût :** implémentation non triviale. Divergence à gérer. Debug plus complexe.
**Taille de patch :** une semaine de taf sérieux.
**Priorité :** **P1** pour le feeling pilote, **P2** si le feeling actuel reste acceptable après P0.

### 6. Crew en world transform compensé, pas en frame passenger — **impact moyen**

**Problème :** aujourd'hui le crew est dans le world frame, et `ApplyYawCompensation` applique le delta sub à l'actor : [SubCrewMovementComponent.cpp:376](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:376). C'est un patch fonctionnel, mais le crew hérite du jitter world-space du sub.
**Fix :** stocker `RelativeLocation` / `RelativeRotation` comme vérité crew, faire tourner le CMC crew dans le référentiel sub, et composer `SubTransform * CrewLocal` au render. Si le sub interpole smooth (point 1), le crew hérite du smooth gratuitement.
**Effet secondaire positif :** permet de retirer `ApplyYawCompensation` et le `bIgnoreBaseRotation` dynamique.
**Coût :** refactor moyen du CMC crew. Impact sur aim anim, HandIK, FootIK à vérifier.
**Taille de patch :** ≈ 200-300 lignes.
**Priorité :** **P1** après P0.

### 7. Précision float aux grandes distances — **impact nul à 5 km, gros à 50 km**

**Problème :** float32 a ≈ 1 cm de résolution au-delà de 10 km de l'origine. Chaque tick la position snappe au float représentable le plus proche → jitter au cm visible.
**Diagnostic :** logger la position sub à vitesse de croisière à 10 km de l'origine. Si la mantisse sature, le jitter est visible.
**Fix :** activer Large World Coordinates (UE 5.x natif), ou origin rebasing périodique.
**Priorité :** **P3**. Non bloquant tant que le joueur reste < 10 km. À revoir au niveau design carte.

### 8. Slew angulaire non plafonné sur le lerp remote — **impact moyen sur observateurs**

**État actuel du path remote :**
- Le client distant lerp `PrevSnapshot` → `TargetSnapshot` pour position et rotation sur `InterpDuration` : [SubMovementComponent.cpp:785](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:785). Ce n'est pas un snap.
- Un teleport snap **séparé** est déclenché uniquement quand `SnapshotDistanceCm > InterpSnapDistanceCm` : [SubMovementComponent.cpp:764](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:764).

**Problème résiduel :** quand un snapshot arrive avec un gros delta angulaire (exemple : pilote qui rudder fort, packet perdu, puis rattrapage), le lerp complète ce delta en `InterpDuration` (≈ 50 ms). Visuellement, c'est une rotation rapide mais lissée — pas un twitch de 1 frame, mais quand même un "à-coup" perceptible sur observateur.
**Fix :** plafonner le slew angulaire par frame (par exemple 60°/s max) en post-traitement du lerp. Les overshoots deviennent du drift lent, pas des rotations rapides.
**Gate de snap :** garder `InterpSnapDistanceCm` tel quel — il sert aux rattrapages catastrophiques (respawn, téléport), pas au cas de désynchro normale.
**Priorité :** **P2**. Complément de l'Option E du doc smoothing_options.

---

## Feuille de route

### Phase 1 — gain immédiat, standalone + local player (≈ 2 jours)

- **Point 1** : extrapolation → interpolation dual-buffer sur le path autorité-locale.
- **Point 2** : caméra late-update.
- **Point 4** : préserver (ou remplacer explicitement) l'invariant d'ordre de tick entre sim, `RefreshRepState` et écriture visuelle.

Livrables :
- `TickComponent` refactorisé avec double snapshot sim + lerp.
- Override `CalcCamera` sur `ASubCrewCharacter`.
- Invariant documenté : soit `RefreshRepState` reste appelé **avant** toute écriture de pose visuelle sur l'actor (statu quo, aujourd'hui safe par `SubMovementComponent.cpp:168`), soit `RefreshRepState` est refactorisé pour sampler une source de vérité explicite (ex: `CurrSim` bufferisé) plutôt que `GetActorLocation()` — voir [SubmarineBase.cpp:1018](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:1018).

Validation :
- PIE standalone, pousser à vitesse max (`MaxForwardSpeed = 650 cm/s`), observer absence de step visible.
- Varier le framerate (30 / 60 / 144 Hz) via t.MaxFPS, vérifier la stabilité.

### Phase 2 — consolidation visuelle + crew (≈ 5 jours)

- **Point 3** : mesh visuel décroché de la racine physique.
- **Point 6** : crew en frame passenger, retrait de `ApplyYawCompensation`.

Livrables :
- `VisualRoot` scene component, mesh + sockets attachés dessus.
- Refactor `USubCrewMovementComponent` pour opérer dans le référentiel sub.
- Sub-crew yaw compensation retirée proprement.

Validation :
- PIE standalone, embark + rotation sub à plein régime, vérifier que le crew reste rigidement calé.
- Vérifier Anim aim / HandIK / FootIK inchangés ou améliorés.

### Phase 3 — réseau (≈ 1-2 semaines)

- **Point 5** : prédiction client-side helmsman.
- **Point 8** : rate-limit sur les corrections remote.

Livrables :
- Duplication partielle du sim côté helmsman pour le path autonomous proxy.
- Réconciliation serveur avec slew progressif.
- Plafond de slew sur remote.

Validation :
- PIE réseau 2 clients, 100-200 ms de lag simulé (via `NetEmulation`), feeling pilote et observateur.

---

## Ce que cette note tranche

- Le jitter résiduel **n'est pas un bug à fixer**, c'est une conséquence structurelle de l'extrapolation. Le tuner ne le supprimera pas.
- Pour atteindre "100% lisse peu importe la vitesse", **il faut passer à l'interpolation dual-buffer**. Pas d'autre chemin.
- Les options A/B/C déjà codées sont conservées — elles traitent le contact, problème différent.
- La latence visuelle de 16.6 ms introduite par l'interpolation est **acceptable** dans le cadre coop chill défini au plan stratégique.

---

## Ce que cette note ne tranche pas

- Le budget de dev pour la phase 3 (prédiction helmsman). Décision à prendre en fin de phase 1+2, quand on saura si le feeling pilote est suffisant sans prédiction.
- Le choix caméra : late-update via override `CalcCamera` vs `FSceneViewExtension`. Les deux marchent, le plus propre en C++ dépend de contraintes à regarder au moment de l'implémentation.
- Le comportement du `VisualRoot` pendant les téléports / boarding / disembark. À spécifier lors de la phase 2.

---

## Proposition d'ordre d'exécution

**Étape 0 (préalable, hors roadmap)** : valider en PIE les options A/B/C déjà codées. Ajuster `ContactYawDampingFactor` et `MaxSlideIterations` si besoin. Ce n'est pas une phase en soi, juste un checkpoint avant d'attaquer les refactors structurels.

Ensuite, dans l'ordre :

1. **Phase 1** (interpolation dual-buffer + caméra late-update + invariant d'ordre de tick). C'est le vrai saut qualité.
2. Évaluer le feeling après Phase 1 avant de s'engager sur Phase 2/3.
3. **Phase 2** si le crew ressent encore du décrochage interne.
4. **Phase 3** uniquement si le feeling réseau est problématique dans les tests 2 joueurs.

Cet ordre maximise la valeur par jour de dev : les plus gros gains pour le moins de complexité en premier.
