# Jitter Audit — Comparaison 4 Agents — 2026-04-19

Comparaison des 4 audits indépendants sur le jitter sub :
- **A1 (moi, Sonnet 4.7 1M)** : [2026-04-18_jitter_audit_findings.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_audit_findings.md)
- **A2 (Gemini 3.1 Pro)** : [contreaudit_02_Gemini3Pro.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_full_architecture_audit_contreaudit_02_Gemini3Pro.md)
- **A3 (Sonnet 4.6)** : [contreaudit_01_sonnet.md](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_full_architecture_audit_contreaudit_01_sonnet.md)
- **A4 (GPT54)** : [contreaudit_03_GPT54.md](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_full_architecture_audit_contreaudit_03_GPT54.md)

---

## Matrice de convergence par sujet

| # | Sujet | A1 (moi) | A2 (Gemini) | A3 (Sonnet 4.6) | A4 (GPT54) | Verdict |
|---|---|---|---|---|---|---|
| 1 | Camera Sway = amplificateur perceptuel via double-dérivation | ✓ identifié | ✓ confirmé | ✓ confirmé ligne par ligne | ✓ confirmé (F5) | **CONSENSUS 4/4** |
| 2 | InteriorFrame dérive vélocité de la pose visuelle (vs lire `SubMov.Velocity`) | implicite | confirmé | **prouvé math** | confirmé (F5) | **CONSENSUS 4/4 sur le fait, A3 va plus loin sur l'implication** |
| 3 | H1 (writer externe) écartée | "statiquement écartée" | "écartée" | partial: "écartée sur path C++, BP non vérifiable" | "pas écartée" — BP pas vérifié, EXTERNAL_WRITE rotation manquant, TraversalLevelManager existe | **A3+A4 convergent sur ma sur-affirmation** |
| 4 | Tick order : tous readers tickent après SubMov | affirmé général | confirmé | confirmé sur path critique | partial : prereq prouvé seulement pour InteriorFrame + CrewMov ; non prouvé pour Radar / SystemsComp / HelmNavDisplay | **A4 catch mon over-claim** |
| 5 | Le pattern `+20.84 / -3.74` reste explicable | "non expliqué, reste H3" | n/a | "math interp prédit la variance, mais borne inf ≥ 0 → -3.74 vrai unexplained" | "World X ≠ forward, faut projeter sur GetActorForwardVector" | **Désaccord** : aucun ne ferme cleanly. A3 mathématise l'intervalle [0, 2s], A4 conteste la mesure. Mon interprétation H3 reste viable mais non confirmée. |
| 6 | Fix #1 = changer CameraSway pour utiliser sim accel | proposé (~30 lignes, scope limité) | validé tel quel | **CONTRE-PROPOSITION** : changer InteriorFrame pour lire `SubMov.Velocity` (~8 lignes, scope global) | n/a | **A3 améliore mon fix** : un seul point de correction, fixe tous les consumers en une fois |
| 7 | Chaîne attachement FPSCamera | "(probablement Mesh ou Capsule, à vérifier en BP)" — j'ai hedgé | n/a | n/a | **CORRECTION FACTUELLE** : `FPSCamera->SetupAttachment(GetRootComponent())` ([SubCrewCharacter.cpp:97](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:97)) — donc Capsule, PAS Mesh | **A4 corrige** mon hedge ; chaîne FPS = Capsule → FPSCamera (pas de Mesh, pas de SpringArm) |
| 8 | Multi-step catch-up (PrevSim capturé inside while loop) | non mentionné | non mentionné | **identifié** : "interp couvre seulement le dernier step, l'avance nette est step_1+step_2 → variance" | non mentionné | **ISOLÉ A3** |
| 9 | Discontinuité de vélocité InteriorFrame à transition contact | non mentionné | non mentionné | **identifié** : "frame contact = full step, frame post-contact = 0.3*step → spike" | non mentionné | **ISOLÉ A3** |
| 10 | AnimInstance consomme aussi `LocalSubLinearAcceleration` (2nd amplificateur) | mentionné (anim lit transform sub) mais pas profond | non mentionné | mentionné | **détaillé** (F7) : `ReadInputState` copie l'accel, `ComputeSubMotion` l'utilise pour stumble | **A3+A4 convergent** sur ce 2e amplificateur |
| 11 | TickPosture (CMC) ALSO writes FPSCamera | non mentionné | non mentionné | non mentionné | **identifié** (F6) : 2 writers concurrent sur FPSCamera dans la même frame, le second gagne | **ISOLÉ A4** |
| 12 | EXTERNAL_WRITE détecteur incomplet (rotation pas testée) | non mentionné | non mentionné | non mentionné | **identifié** (F4) : `LastPostTickRotation` stocké mais jamais comparé | **ISOLÉ A4** |
| 13 | BACKWARD détecteur incomplet (World X ≠ forward sub) | non mentionné | non mentionné | nuance "Phase 12 doit séparer types" | **identifié** (F1) : faut projeter sur `Owner->GetActorForwardVector()` | **A4 catch un instrument bug** |
| 14 | OnRep_RepState ne fire pas en standalone | ✓ | confirmé | confirmé | n/a explicite | **CONSENSUS 3/3 explicite** |
| 15 | OnHullHit + side-effects sub safe | ✓ | n/a | confirmé implicite | n/a | **OK 2/2 explicite** |
| 16 | Mesh / Skeletal / Root motion absent du sub | ✓ | n/a | confirmé | n/a | **OK 2/2 explicite** |
| 17 | FRotator gimbal lock edge case | non mentionné | non mentionné | mentionné, écarté en pratique (pitch limité 30°) | non mentionné | **ISOLÉ A3, non bloquant** |
| 18 | Procédure d'audit a des incohérences (12 phases annoncées, 0-13 énumérées) | mon erreur | n/a | n/a | **identifié** (méta) | **A4 review méta de la procédure** |
| 19 | Phase 12 = bloquante pour conclure | OUI | "ne pas skip, faire avant fix" | **NON** : "déductible statiquement, faire fix #1 puis confirmer" | "fix les détecteurs d'abord, puis faire" | **Désaccord 4-way** |

---

## Convergences fortes (consensus 4/4 ou 3/3 explicite)

### C1 — Camera Sway est l'amplificateur perceptuel principal
Tous les agents identifient indépendamment que `ASubCrewCharacter::Tick` ([SubCrewCharacter.cpp:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235)) prend `LocalSubLinearAcceleration` (déjà bruité par double-dérivation), le multiplie par `CameraSwayAccelScale = 0.002`, et clamp à 3 cm. Le clamp se sature constamment quand le signal accel est spike → 3 cm de jitter caméra perçus.

### C2 — InteriorFrame dérive numériquement la pose visuelle
[SubInteriorFrameComponent.cpp:71-72](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71) — `LocalLinearVelocity = (loc - prev_loc)/dt` puis `LocalLinearAcceleration = (vel - prev_vel)/dt`. Double dérivation finie sur signal pose visuelle. Mathématiquement instable.

### C3 — Tick order sur le path critique est correct
Prereq SubMov → InteriorFrame → CrewMov vérifié à [SubInteriorFrameComponent.cpp:41](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:41) et [SubCrewMovementComponent.cpp:612-618](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:612). Pas de race condition sur cette chaîne.

### C4 — OnRep_RepState ne fire pas en standalone
REPNOTIFY_OnChanged + auth ne fire pas ses propres OnRep. Pas un writer.

### C5 — Pas de mesh/skeletal/anim qui write l'actor sub
HullMesh = static no-collision, pas de SkeletalMesh sur le sub, pas de root motion. ✓

---

## Sub-consensus (2/4 forts) — corrections de mon doc

### S1 — H1 sur-affirmée par moi (A3 + A4)
Ma formulation "H1 statiquement écartée" est trop forte selon A3 et A4. La bonne formulation :
> "H1 écartée sur le path C++ runtime relu pendant la revue. Composants Blueprint ajoutés sur `BP_Submarine_Craniata` non vérifiables par audit statique → H1 résiduel possible côté BP."

A4 ajoute : `TraversalLevelManager.cpp:34` est techniquement un writer standalone (timer 0.2s post-BeginPlay). J'avais noté qu'il était "hors test cruise" mais c'est un caveat à expliciter.

### S2 — AnimInstance est un 2e amplificateur (A3 + A4)
A4 (F7) : `USubCrewAnimInstance::ReadInputState` ([SubCrewAnimInstance.cpp:149-150](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:149)) copie `LocalSubLinearAcceleration`, puis `ComputeSubMotion` ([:349-366](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:349)) l'utilise pour le stumble du crew. Atténué par `Instability = 1 - SupportQuality`, donc moindre que CameraSway, mais c'est un 2e amplificateur réel. **Si je fix CameraSway sans fix InteriorFrame, l'anim stumble continuerait à jitter.** Ça renforce le Fix #1 de A3.

### S3 — Tick order global non prouvé exhaustivement (A4)
J'avais dit "tous les readers tickent après SubMov". A4 (F3) : prouvé seulement pour InteriorFrame + CrewMov + ASubCrewCharacter (par chaîne de prereq). Pour Radar / SystemsComponent / HelmNavigationDisplay → pas de prereq explicite, ordre tick déterministe par Tick Group default mais pas garanti. **Réformuler en "path critique prouvé" + "autres = inférence selon TG_PrePhysics default".**

---

## Findings isolés — uniques à un agent

### Isolé A3 (Sonnet 4.6)

**A3.1 — Math de variance d'alpha** : démonstration formelle que `delta_N = s * (1 - alpha_{N-1} + alpha_N)` pour 1 step, donc delta peut osciller entre 0 et 2s à vitesse sim constante. C'est NOMINAL, pas un bug. Mon doc traitait cette variance comme un mystère.

**A3.2 — Multi-step catch-up** : `PrevSimLocation` capturé INSIDE le while loop fait que sur une frame catch-up à 2 steps, l'interp couvre seulement le dernier step alors que l'avance nette = 2 steps. Source de variance documentée mais non identifiée par moi.

**A3.3 — Contact transition spike** : quand `bLastStepHadBlockingHit` change d'état entre frames, alpha effectif passe de 1.0 (interp suppress) à valeur normale (ex 0.3) → spike de delta perceptible par InteriorFrame.

**A3.4 — Fix #1 amélioré** : modifier `USubInteriorFrameComponent::TickComponent` pour lire `SubMov->Velocity` au lieu de dériver de `FrameLocationDelta`. **8 lignes, scope global, fixe TOUS les consumers en une fois** (CameraSway + AnimStumble + ApplyYawCompensation + tout futur).

### Isolé A4 (GPT54)

**A4.1 — FPS camera chain factuelle** : `FPSCamera->SetupAttachment(GetRootComponent())` ([SubCrewCharacter.cpp:97](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:97)) — donc Capsule, pas Mesh. Pas de SpringArm sur FPS. Mon doc avait hedgé "(probablement Mesh ou Capsule)" mais A4 l'a vérifié. **Ça invalide une hypothèse "anim mesh porte le jitter sur FPS" qu'on aurait pu soupçonner.**

**A4.2 — TickPosture écrit aussi FPSCamera** : [SubCrewMovementComponent.cpp:710-747](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:710) fait `FPSCamera->SetRelativeLocation(...)`, puis `ASubCrewCharacter::Tick` réécrit la position complète à [:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235). 2 writers en compétition dans la même frame. Le second gagne, donc pas la cause directe, mais c'est une dette d'archi à noter.

**A4.3 — Détecteur EXTERNAL_WRITE incomplet** : `LastPostTickRotation` stocké mais jamais utilisé en comparaison. Donc un writer **rotation-only** échapperait au log. À fixer dans l'instrumentation.

**A4.4 — Détecteur BACKWARD incomplet** : `TickDelta.X < threshold` mesure le World X. Un sub avec heading désaligné de +X verrait des deltas X négatifs alors qu'il avance dans son axe local. À projeter sur `Owner->GetActorForwardVector()` avant comparaison. (Note : dans le test cruise du user, Yaw=0.00 donc X = forward, mais le détecteur reste fragile en général.)

---

## Désaccords entre agents

### D1 — Statut du `-3.74`

| Agent | Position |
|---|---|
| A1 (moi) | Non expliqué statiquement, reste H1 ou H3. Phase 12 obligatoire. |
| A2 (Gemini) | N'aborde pas explicitement. |
| A3 (Sonnet 4.6) | Math interp donne intervalle [0, 2s] ⇒ -3.74 IMPOSSIBLE par interp seule. Mais hand-wave "combinaison sub sous-step + crew BasedMovement". Pas vraiment résolu. |
| A4 (GPT54) | `TickDelta.X` ≠ déplacement forward réel. Sub Yaw pourrait être désaligné. (Argument valide en général, pas dans le test précis avec Yaw=0.) |

**Verdict** : aucun audit statique ne ferme `-3.74`. Soit Phase 12 le résoudra (EXTERNAL_WRITE log fire ou pas), soit il y a un bug d'instrumentation (A4) ou un edge case math (A3). **Aucun agent n'a la réponse définitive.** C'est le trou de l'audit statique global.

### D2 — Phase 12 bloquante ou pas

| Agent | Position |
|---|---|
| A1 (moi) | Bloquante pour conclure. |
| A2 (Gemini) | Pas skip, faire AVANT fix. |
| A3 (Sonnet 4.6) | Pas bloquante. Fix #1 d'abord, Phase 12 en confirmation après. |
| A4 (GPT54) | Fix les instruments d'abord (rotation check, forward projection), puis Phase 12. |

**Verdict** : A3 a la position la plus pragmatique. Si Fix #1 (au niveau InteriorFrame) coupe le problème à la source, Phase 12 devient confirmation pas diagnostic. Mais A4 a raison sur les instruments à fixer avant de tirer des conclusions.

---

## Le vrai problème, après confrontation

### Cause structurelle (consensus) :

**`USubInteriorFrameComponent` calcule `LocalLinearVelocity` et `LocalLinearAcceleration` par double dérivation finie de `Owner->GetActorLocation()`** ([SubInteriorFrameComponent.cpp:71-72](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71)).

C'est l'erreur architecturale fondamentale. La pose visuelle du sub est **par construction** un signal qui :
- A des micro-variations dues à l'interp (variance d'alpha frame-to-frame, démontrée math par A3).
- Subit des micro-modifications par le sweep de collision en ApplyPhysics même hors contact.
- Subit potentiellement des modifications BP non auditées (caveat A3+A4).

Différencier numériquement ce signal une fois donne une vélocité bruitée ; deux fois donne une accélération qui spike massivement (192k cm/s² observé en log).

### Amplificateurs perceptuels (consensus) :

1. **`ASubCrewCharacter::Tick` Camera Sway** : prend l'accel bruitée, la multiplie par 0.002, clamp à 3 cm. Saturé en permanence sur les spikes → 3 cm de jitter caméra visible.
2. **`USubCrewAnimInstance::ComputeSubMotion`** (A4 F7) : prend l'accel pour le stumble. Atténué mais existant.
3. (Et tout futur consumer de `Frame->GetLocalLinearAcceleration()`.)

### Cause amont incertaine (gap)

Le pattern `+20.84 / -3.74` n'est pas pleinement expliqué par l'audit statique. Aucun des 4 agents ne le résout définitivement. Trois pistes restantes pour Phase 12 :
- **H1 résiduel BP** : un composant Blueprint sur `BP_Submarine_Craniata` qui écrit la pose. Aucun de nous n'a pu vérifier (asset binaire).
- **H3 edge case math** : mon code a un bug que la lecture statique n'a pas révélé.
- **Instrumentation buggée** (A4) : le log `BACKWARD` ne mesure pas ce qu'on croit (mais Yaw=0 dans le test précis, donc cette piste est faible pour CE test).

---

## Le vrai fix, après confrontation

### Fix unique P0 (A3, validé par croisement) :

Modifier `USubInteriorFrameComponent::TickComponent` pour lire `SubMovement->Velocity` au lieu de dériver de `FrameLocationDelta`.

```cpp
// AVANT (instable, source de tous les amplificateurs)
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;

// APRÈS (stable)
if (USubMovementComponent* SubMov = Owner->FindComponentByClass<USubMovementComponent>())
{
    LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(SubMov->Velocity);
}
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

**Bénéfices** :
- Fixe **tous** les consumers d'accel en une fois (CameraSway, AnimStumble, ApplyYawCompensation indirect, futurs).
- ~8 lignes, zéro impact path remote (où SubMov est aussi présent et `Velocity` est répliqué).
- Coût négligeable, risque faible.

**Mon Fix #1 d'origine** (changer CameraSway pour utiliser sim accel) est plus narrow et duplique de la logique. **Je retire mon Fix #1 et adopte celui de A3.**

### Avant de poser ce fix, fix les instruments (A4)

Pour que Phase 12 reste utile en confirmation :

1. Détecteur BACKWARD : projeter `TickDelta` sur `Owner->GetActorForwardVector()` avant comparaison au seuil.
2. Détecteur EXTERNAL_WRITE : ajouter check rotation similaire (compare `LastPostTickRotation` vs `PreUndoRot`).
3. Ajouter un toggle séparé `sub.DisableCameraSway` pour le Test 12 supplémentaire (séparer le test "sway off" du test "interp off").

~30 lignes additionnelles dans `SubMovementComponent.cpp` + `SubCrewCharacter.cpp`.

### Phase 12 devient confirmation après ces fixes

Avec instruments propres + Fix #1 InteriorFrame appliqué, Phase 12 sert à :
- Confirmer que jitter visuel disparaît ou devient sub-perceptible (visuel seul).
- Confirmer absence d'EXTERNAL_WRITE (clôture H1 BP).
- Confirmer absence de BACKWARD forward-axis (clôture H3).

Si tout est clean → fix validé. Si reste du jitter → on a au moins un signal propre pour creuser.

---

## Récap de mes erreurs corrigées par les autres

| # | Mon erreur | Source de la correction | Sévérité |
|---|---|---|---|
| E1 | "H1 statiquement écartée" trop fort | A3 + A4 | Moyenne — formulation à corriger |
| E2 | "Tous readers tickent après SubMov" sur-affirmé | A4 (F3) | Faible — vrai sur path critique, inférence pour le reste |
| E3 | Chaîne FPS hedgé "Mesh ou Capsule" | A4 (F2) | Faible — hedge raisonnable, mais pouvait être vérifié dans le code |
| E4 | Fix #1 trop narrow (CameraSway only) | A3 | **Forte** — propose meilleure solution structurelle |
| E5 | Pas mention de l'AnimInstance comme 2e amplificateur | A3 + A4 (F7) | Moyenne — j'ai vu le reader mais pas son rôle d'amplificateur |
| E6 | Pas mention de TickPosture comme 2e writer FPSCamera | A4 (F6) | Faible — dette d'archi non bloquante |
| E7 | Pas mention de l'instrument BACKWARD/EXTERNAL_WRITE incomplet | A4 (F1, F4) | **Forte** — bug d'instrumentation que j'ai écrit moi-même et pas vu |
| E8 | Pas math formelle de la variance d'alpha | A3 | Moyenne — j'ai hand-wave "amplitude oscillante", A3 a chiffré |
| E9 | Pas mention multi-step catch-up source de variance | A3 | Moyenne — vrai mécanisme, pas identifié |
| E10 | Pas mention contact transition spike | A3 | Moyenne — edge case réel, à corriger en Phase 2 |

---

## Décisions à prendre

1. **Adopter Fix #1 version A3 (InteriorFrame)** au lieu du mien. Décision : OUI sauf objection.
2. **Fixer les instruments** (forward projection + rotation check + sway-off toggle) AVANT Phase 12. Décision : OUI, requis sinon Phase 12 produit des données non interprétables.
3. **Phase 12 reste utile** mais devient confirmation post-fix au lieu de bloquante pré-fix. Décision : OUI selon A3.
4. **Mettre à jour mes findings doc** avec les corrections E1-E10 ? Je peux soit éditer le doc original, soit créer un addendum. Je recommande addendum pour traçabilité.
5. **`-3.74` reste partiellement non résolu** : si Fix #1 InteriorFrame supprime l'amplificateur, le `-3.74` sur la pose sub elle-même reste à investiguer. Mais c'est sub-cm donc invisible si l'amplificateur tombe. **Le vrai jitter perçu disparaîtra avant que la cause sub soit identifiée.**

---

## Synthèse en une phrase

**Les 4 agents convergent unanimement sur la cause perceptuelle (CameraSway × double-dérivation), Sonnet 4.6 propose le fix structurellement le plus propre (lire `SubMov.Velocity` dans InteriorFrame, fixe tout downstream en 8 lignes), GPT54 catch deux bugs d'instrumentation que j'ai introduits moi-même dans mon propre détecteur, et personne — dont moi — ne ferme cleanly l'origine du `-3.74` observé sur la pose sub elle-même.**
