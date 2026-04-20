# Sub Jitter — Procédure d'Audit Architecture Complète — 2026-04-18

**But** : identifier de manière exhaustive **toutes** les sources potentielles du jitter visuel sur le sous-marin en mode **standalone** (pas de réseau). Le fait que le jitter persiste hors réseau exclut les classes de bugs liées à la réplication / interpolation client distant et pointe vers une **erreur structurelle** dans la pipeline de mouvement local / présentation / compensation.

**Méthode** : audit en 12 phases, chacune avec inputs / méthode / outputs / critères de décision. À la fin, un document de synthèse liste les causes confirmées, écarte les hypothèses invalidées, et propose des fixes priorisés.

**Scope** : tout ce qui peut influencer la pose visuelle du sub ou de ce qui s'y rattache (crew, caméra, mesh, widgets) entre l'output du sim et le rendu de la frame.

**Hors scope** : path remote (`InterpolateClient`), code de génération asset, éditeur, contenu authoring.

---

## Phase 0 — Préalables

### 0.1 État de référence
Avant de creuser, capturer les états reproductibles :
- Build à jour (rebuild OK 2026-04-18 21h00).
- Tests A/B/C du handoff `2026-04-18_jitter_debug_handoff.md` doivent être exécutés en parallèle si possible — leurs résultats orientent les phases ciblées.
- Carte de test : Proto03 ou équivalent, sans IA, sans physique externe.

### 0.2 Hypothèses de travail
Trois familles à départager :
- **H1** : un writer externe modifie l'actor root entre nos ticks (autre composant, anim, attach, hot-reload).
- **H2** : un reader sample la pose du sub à un mauvais moment (avant l'undo, ou pendant le sim) et propage des valeurs incohérentes en aval (crew, caméra).
- **H3** : la math de `Lerp(PrevSim, CurrSim, α)` produit des poses non-monotones dans certains cas edge (rotation wrap-around, init non initialisé, contact intermittent).

L'audit doit produire des preuves pour chacune.

---

## Phase 1 — Inventaire des composants sur ASubmarineBase

**Input** : `Source/Sub3D/Submarine/SubmarineBase.h/.cpp`, .uasset BP_Submarine_Craniata.

**Méthode** :
1. Lister tous les `UPROPERTY()` de type `*Component` dans `ASubmarineBase`.
2. Pour chaque, noter : type, parent attachment, role.
3. Inspecter le BP_Submarine_Craniata via le content browser pour les composants ajoutés en BP (non visibles en C++).
4. Identifier le **RootComponent** : USceneComponent ou UPrimitiveComponent ?
5. Identifier le `MovementCollisionComponent` retourné par `GetMovementCollisionComponent()`.

**Output** : tableau `[ComponentName | Class | Parent | Role | Tick? | Tick group]` au format markdown.

**Critère de décision** : si un composant a `bCanEverTick = true` et n'est pas dans la liste connue (SubMovement, InteriorFrame, Flood, etc.), c'est un suspect H1.

---

## Phase 2 — Inventaire des composants sur ASubCrewCharacter

**Input** : `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp`, BP_Crew.

**Méthode** :
1. Même méthode que Phase 1 sur le crew.
2. Identifier où la caméra est attachée (chaîne complète : Camera → SpringArm? → CharacterMesh? → CapsuleComponent? → ActorRoot? → Sub?).
3. Vérifier `bIgnoreBaseRotation`, `BasedMovement` setup.
4. Lister tous les `UFloatingPawnMovement` / `UCharacterMovementComponent` actifs.

**Output** : graphique d'attachement crew → sub (texte ou ASCII).

**Critère de décision** : si la chaîne d'attachement passe par un composant qui pourrait avoir un offset jittery (skeletal mesh anim, spring arm lag), suspect H2.

---

## Phase 3 — Audit static : tous les writers de la pose du sub

**Input** : codebase entière (`Source/`, `Plugins/`).

**Méthode** :
1. Grep `SetActorLocation`, `SetActorRotation`, `SetActorLocationAndRotation`, `SetActorTransform`, `AddActorWorldOffset`, `AddActorLocalOffset`, `AddActorWorldRotation`, `K2_SetActorLocation`, `TeleportTo` — tous filtrés sur les targets pouvant être le sub.
2. Pour chaque hit, déterminer : est-ce que la cible est `ASubmarineBase` ou un de ses composants root ? Quand est-ce appelé (tick, init, gameplay event) ? Avec quel `ETeleportType` ?
3. Vérifier aussi les `MoveComponent`, `MoveUpdatedComponent`, `SafeMoveUpdatedComponent` qui peuvent déplacer un composant racine.

**Output** : tableau `[File:Line | Caller | Target | Frequency | TeleportType | Notes]`.

**Critère de décision** : tout writer qui n'est pas mon `SubMovementComponent::TickComponent` (lignes 154 + 245) est un H1 confirmé.

---

## Phase 4 — Audit static : tous les readers de la pose du sub

**Input** : codebase.

**Méthode** :
1. Grep `GetActorLocation`, `GetActorRotation`, `GetActorTransform`, `GetActorForwardVector`, `K2_GetActorLocation` — sur cibles potentielles.
2. Pour chaque, déterminer : tick group, ordre relatif à `SubMovement.TickComponent`, fréquence d'appel.
3. Surtout repérer ceux qui lisent **et persistent** la valeur (cache / state) pour la réutiliser.

**Output** : tableau `[File:Line | Caller | Reads | Cached?]`.

**Critère de décision** : un reader qui sample avant SubMovement (donc lit la pose extrapolée de la frame précédente, pas restaurée par l'undo) propage du jitter en aval. Suspect H2.

---

## Phase 5 — Reconstruction du tick order

**Input** : tous les composants identifiés en Phases 1-2 + leurs `BeginPlay` (cherche `AddTickPrerequisiteComponent`, `AddTickPrerequisiteActor`, `PrimaryComponentTick.TickGroup`, `bAllowTickOnDedicatedServer`).

**Méthode** :
1. Pour chaque composant pertinent, noter : `TickGroup` (default = `TG_PrePhysics`), prérequis explicites, priorité éventuelle.
2. Construire un graphe partiel : qui dépend de qui. Sortir un ordre topologique partiel.
3. Identifier les ambiguïtés : composants sans dépendance déclarée → ordre indéterministe entre eux.

**Output** : graphe ASCII des dépendances, avec annotation `LIT_SUB`, `ECRIT_SUB`, `LIT_CREW`, `ECRIT_CREW` à côté de chaque nœud.

**Critère de décision** : tout composant qui ECRIT_SUB dans un tick group postérieur à SubMovement et avant InteriorFrame est un H1. Tout LIT_SUB ambigu est un H2 potentiel.

---

## Phase 6 — Audit des callbacks de réplication (relevant en standalone)

**Input** : `ASubmarineBase`, `FSubmarineNetState`, `OnRep_RepState`.

**Méthode** :
1. Vérifier toutes les `UFUNCTION()` `OnRep_*` sur ASubmarineBase et composants.
2. Confirmer qu'en standalone, `RefreshRepState` écrit `RepState` **mais** le replication callback ne fire pas (on est l'authority, pas un client).
3. Cas piège : `bRepNotify` config + `RepNotifyCondition::REPNOTIFY_Always` peut faire fire la callback **même côté serveur/standalone** sur certains setups.

**Output** : liste des `OnRep_*`, pour chacune : fire-en-standalone oui/non, et si oui, qu'écrit-elle.

**Critère de décision** : si une OnRep_ fire en standalone et écrit la pose / un offset, c'est un H1 majeur.

---

## Phase 7 — Audit des systèmes de compensation

**Input** : `SubCrewMovementComponent` (notamment `ApplyYawCompensation`), `SubInteriorFrameComponent`, tous les fichiers contenant "Compensation", "AntiJitter", "Smooth", "Lag".

**Méthode** :
1. Grep `Compensation|AntiJitter|Smooth|Lag|Damp|Stab` dans `Source/Sub3D/Submarine/`.
2. Pour chaque hit, lire le contexte : qui appelle, à quel tick, sur quoi ?
3. Tracer les state vars persistantes : si un système accumule un état (ex : `LastYaw` du sub conservé entre ticks pour calculer un delta), vérifier qu'il est bien synchro avec la pose réelle de la frame.

**Output** : tableau `[Système | Fichier | Compensation appliquée | Lit | Écrit | État persisté]`.

**Critère de décision** : un système de compensation qui lit la pose du sub à un moment où elle est dans un état transitoire (pendant l'undo, pendant le sim, post-interp) propage du jitter ou en crée. Suspects H2/H3.

---

## Phase 8 — Audit des side-effects de SetActorLocation

**Input** : la pipeline UE de movement.

**Méthode** :
1. Identifier ce que fait `SetActorLocationAndRotation(..., bSweep=false, ETeleportType::TeleportPhysics)` :
   - Update transform du root
   - Propage aux enfants attachés
   - Update physics state (rebody)
   - Trigger overlap update
   - Émission de `OnActorMoved` delegate
2. Idem pour `ETeleportType::None` : skip overlap + physics rebody, mais émission de `OnActorMoved` quand même.
3. Lister tous les `OnActorMoved`/`OnComponentMoved` listeners enregistrés sur le sub ou ses composants.
4. Vérifier si InteriorFrame ou un autre composant écoute ces events (pas seulement via tick).

**Output** : liste des listeners + side-effects recensés.

**Critère de décision** : un listener qui écrit la pose du sub (ou modifie une state qui sera lue à la frame suivante) en réponse à `OnActorMoved` est un coupable H1 caché.

---

## Phase 9 — Audit du mesh visuel et du squelette

**Input** : `ASubmarineBase` avec ses mesh components, `BP_Submarine_Craniata`.

**Méthode** :
1. Identifier toutes les `UStaticMeshComponent` / `USkeletalMeshComponent` sur le sub.
2. Pour chacune : a-t-elle un AnimBP ? Du root motion ? Une physique ?
3. Si SkeletalMesh : vérifier si `bComponentUseFixedSkelBounds` ou `bRootMotionFromAnim` qui pourrait écrire le transform parent.
4. Vérifier le `MovementCollisionProxy` : c'est un composant collidable, comment est-il animé ?

**Output** : tableau des meshes + propriétés à risque.

**Critère de décision** : un AnimBP avec root motion, ou un skeletal mesh qui bouge le parent via cinematics, est un H1.

---

## Phase 10 — Audit caméra

**Input** : `ASubCrewCharacter` chaîne caméra.

**Méthode** :
1. Identifier le composant caméra et ce qui l'attache au crew (SpringArm? Direct?).
2. Vérifier `bUseControllerRotationPitch/Yaw/Roll` sur le crew.
3. Vérifier override de `CalcCamera` ou usage d'un `APlayerCameraManager` custom.
4. Tester les flags `bUsePawnControlRotation`, `bDoCollisionTest` sur SpringArm.

**Output** : chaîne caméra exhaustive avec tous les transforms intermédiaires.

**Critère de décision** : si la caméra sample le sub à un mauvais moment (avant SubMovement.Tick), on voit le jitter à la caméra alors que le sub bouge smooth. Suspect H2.

---

## Phase 11 — Audit du graphe d'attachement crew → sub

**Input** : runtime state du crew quand embark.

**Méthode** :
1. Lire le code de `Embark` / `BoardSubmarine` (probablement dans SubCrewCharacter ou SubPlayerController).
2. Tracer la chaîne d'attachement : crew root → quel parent ? Bone name ? Socket ?
3. Vérifier `EAttachmentRule` pour chaque axe (KeepRelative / SnapToTarget / KeepWorld).
4. Vérifier le mode de `BasedMovement` du Character (UCharacterMovementComponent.MovementBaseUtility).

**Output** : diagramme d'attachement avec rules.

**Critère de décision** : si la chaîne passe par un composant non-cohérent (ex : skeletal mesh socket qui jitter), c'est H2. Si BasedMovement extrapole pose du base, c'est aussi H2.

---

## Phase 12 — Tests d'isolation empiriques (matrice)

**Input** : éditeur en PIE, CVars + toggles.

**Méthode** : matrice de tests à exécuter, chacun = standalone PIE 10s en cruise + observation jitter visuel + log SUB_TRACE.

| # | Toggle | Attendu | Observation |
|---|---|---|---|
| 12.1 | Baseline (interp ON, tout ON) | Jitter présent | (déjà mesuré) |
| 12.2 | `sub.DisableVisualInterp 1` | Si jitter disparaît → mon interp est cause; si jitter persiste → autre source | À mesurer (Test B handoff) |
| 12.3 | Disable `ApplyYawCompensation` (commenter ou mettre à 0) | Si jitter visiblement réduit → la compensation amplifie un signal jitter venant du sub | À mesurer |
| 12.4 | Disable InteriorFrame ticking | Si jitter inchangé → InteriorFrame n'est qu'un observateur | À mesurer |
| 12.5 | Free-cam externe (pas possess crew) | Jitter présent ou absent ? Différence avec possess = c'est dans la pipeline crew/caméra | (déjà mesuré, plus subtle hors possess) |
| 12.6 | Sub immobile (Velocity = 0) à plein thrust freezé | Si jitter présent → quelque chose bouge l'actor même sans sim | À mesurer |
| 12.7 | Sub bouge mais via `Velocity * dt` direct (bypass mon interp + bypass extrap) | Référence "minimum syndical" — si jitter, c'est en dehors complet | À mesurer (nécessite code temporaire) |

**Output** : résultats chiffrés par cellule.

**Critère de décision** : la matrice resserre les hypothèses. Combinaison de cellules → identification du coupable.

---

## Phase 13 — Synthèse

**Input** : sorties des Phases 1-12.

**Méthode** :
1. Croiser les listes : qui écrit, qui lit, quand, avec quels effets.
2. Pour chaque suspect identifié, évaluer la probabilité d'être le coupable principal vs un facteur aggravant.
3. Proposer 2-3 fixes ordonnés par impact / coût.

**Output** : `reports/analysis/2026-04-18_jitter_audit_findings.md` avec sections :
- Cause(s) confirmée(s)
- Hypothèses écartées (avec preuves)
- Fixes proposés ordonnés
- Risques résiduels

---

## Ordre d'exécution recommandé

Strictement séquentiel pour les phases statiques (1-11), parallélisable avec Phase 12 (qui demande des actions PIE de la part de l'utilisateur).

1. **Phase 1-2** : inventaire (15 min)
2. **Phase 3-4** : grep writers/readers (10 min)
3. **Phase 5** : reconstruct tick order (15 min)
4. **Phase 6** : OnRep audit (5 min)
5. **Phase 7** : compensation systems (10 min)
6. **Phase 8** : side-effects (10 min)
7. **Phase 9** : mesh + skeleton (10 min)
8. **Phase 10-11** : caméra + attachement crew (15 min)
9. **Phase 12** : tests empiriques (30-45 min, requiert action utilisateur en PIE)
10. **Phase 13** : synthèse (15 min)

**Total estimé** : 2-3h de travail audit, dont ~45 min côté utilisateur (Phase 12).

---

## Règles d'or pendant l'exécution

- **Ne pas conclure prématurément** : noter chaque suspect, valider via plusieurs phases avant de lui mettre la responsabilité.
- **Différencier root cause / contributeur / facteur aggravant** (cf. CLAUDE.md scope).
- **Citer file:line systématiquement** pour toute affirmation sur le code.
- **Pas de fix pendant l'audit** : noter les fixes potentiels, les implémenter en synthèse uniquement.
- **Si une phase révèle un cul-de-sac (impossible sans plus d'info), documenter et passer à la suivante** plutôt que bloquer.
