# Session prompt — Flood buoyancy through sub movement input pipeline

**Date** : 2026-04-24
**Branche cible** : `scripting` (ou nouvelle branche `flood-buoyancy-input`)
**À ouvrir en nouvelle session Claude Code** (ce prompt est self-contained — aucune dépendance sur la conversation précédente).

---

## Prompt à coller en tête de session

Tu travailles sur Sub3D (UE 5.7). Diagnostic et refactor d'un bug : quand l'eau rentre dans le sous-marin (via `USubFloodComponent`), le sub descend mais **provoque du jitter sur le crew** parce que le path de descente ne respecte pas le pipeline de mouvement standard.

## Contexte

Le sub a une architecture crew "local grid-space authority" (documentée `reports/plans/2026-04-21_local_grid_space_authority_architecture.md` + section "Crew embarked movement" de `CLAUDE.md`) :

- Le sub est frame autoritaire qui se déplace (mouvement custom via `USubMovementComponent`, pas de Chaos physics)
- Le crew embarqué est rebasé à chaque tick en grid-space : `SubWorldTransform * GridSpaceTransform`
- Ordre de tick strict : `SubMovement → InteriorFrame → CrewMovement`
- Input helm (ballast, yoke, telegraph, kill) passe par `USubMovementComponent` en fixed-tick 60 Hz, ramp inputs lissés

Le flood sim (`USubFloodComponent`) calcule la masse d'eau par compartment (`GetTotalWaterMassKg()`, [SubFloodComponent.cpp:486-489](Source/Sub3D/Submarine/SubFloodComponent.cpp#L486-L489)) **mais ne l'applique pas au pipeline standard**. Un autre chemin (à identifier précisément — probablement modification brute de Mass / Location / Velocity sur `ASubmarineBase` ou via physics actor) applique cette masse en dehors du fixed-tick, provoquant le jitter.

## Objectif de la session

Router l'effet "prise d'eau → descente du sub" via une **input dédiée** sur `USubMovementComponent`, traitée par le même pipeline que les inputs helm (ballast notamment). Le crew rebase à la fin de la chaîne → zéro jitter.

Nom suggéré pour l'input : `FloodImpact` ou `EffectiveBallastFromFlood` (à arbitrer en début de session en fonction de la sémantique physique que tu choisis).

## Work items attendus

### Phase A — Audit chemin actuel (30 min)

1. Trace où la masse d'eau calculée par `USubFloodComponent` affecte aujourd'hui le mouvement du sub. Grep `AddForce`, `SetMass`, `SetActorLocation`, `Velocity +=`, `Mass *=`, `GetTotalWaterMassKg` pour les call sites.
2. Identifie le hook actuel : est-ce que c'est `ASubmarineBase::Tick` qui lit `USubFloodComponent` ? Un `OnFloodStateChanged` delegate ? Un hard coupling dans `USubMovementComponent` ?
3. Rapporte : fichier+ligne du chemin actuel. Note si le chemin est dans le fixed-tick 60 Hz ou hors.

### Phase B — Design de l'input (30 min)

4. Propose le nom final et la sémantique :
   - Option 1 : `FloodImpactKg` — masse additionnelle, convertie en force descendante par `USubMovementComponent`
   - Option 2 : `EffectiveBallastOffsetLiters` — traite l'eau intérieure comme du ballast "parasite", lit par la même math que l'input ballast manuel du helm
   - Option 3 : `FloodBuoyancyForceZ` — force vertical world en cm/s² que `USubFloodComponent` calcule chaque tick et `USubMovementComponent` intègre
5. Argumente le choix en 3 lignes max. Garde ça simple, cohérent avec le reste du helm.

### Phase C — Implémentation (1–2 h)

6. Ajoute le nouveau field/API sur `USubMovementComponent` (EditDefault? UPROPERTY? BlueprintReadOnly ? — voir existants pour cohérence).
7. `USubFloodComponent` calcule à chaque tick la valeur à injecter et écrit dans ce field via `USubMovementComponent`.
8. Retire le chemin brut identifié en Phase A (modification directe de Mass / Location).
9. Vérifie l'ordre de tick : le write doit arriver AVANT que `USubMovementComponent` lise son input dans sa phase d'intégration. `SubFlood` peut être placé en tick prereq de `SubMovement` via `AddTickPrerequisiteComponent`.

### Phase D — Validation PIE (30 min)

10. Breach un compartment en PIE. Observe :
    - Sub descend progressivement (pas de saut d'un frame à l'autre)
    - Crew au helm : pas de jitter (pas de saut >50 cm par frame)
    - Crew en déplacement dans le sub : marche propre, pas de téléportation
11. Active `Project Settings > Sub3D Debug > bLogCrewJitter` pour confirmer zéro spike au-dessus de `CrewJitterWarnVelocityCmPerSec` (default 1200 cm/s).
12. Flood complet d'un compartment → sub doit atteindre une profondeur stabilisée (déf. équilibre ballast + buoyancy), pas descendre en accélération runaway.

## Contraintes

- **NE PAS** toucher à `USubFloodComponent::GetTotalWaterMassKg` ni au sim flood lui-même (cohérence replication déjà validée, voir `project_stabilization_guards_2026_03_25.md`).
- **NE PAS** toucher au rebase crew dans `USubCrewMovementComponent` (cohérence avec Phase 1 grid-space validée).
- **NE PAS** introduire Chaos physics. Tout reste math-based via `USubMovementComponent`.
- Respect CLAUDE.md "Scope and editing discipline" : pas de drive-by refactor, pas d'includes orphelins, pas d'invention API Unreal.

## Fichiers probablement touchés

- `Source/Sub3D/Submarine/SubMovementComponent.{h,cpp}` — ajout input + integration
- `Source/Sub3D/Submarine/SubFloodComponent.{h,cpp}` — write de l'input + retrait chemin actuel
- Éventuellement `Source/Sub3D/Submarine/SubmarineBase.{h,cpp}` — si Tick order ou hook central change

## Sortie attendue

- Commit propre avec message explicite (ex: `fix(flood): route buoyancy through sub movement input pipeline, zero crew jitter`)
- Validation PIE documentée (frame capture ou log) que le jitter crew a disparu
- Mise à jour `CLAUDE.md` si une nouvelle invariant est ajoutée

---

## Contexte additionnel pour la session

- Auto mode peut être activé pour l'implémentation
- Le user est solo dev (voir memory `user_role.md`), préfère ship coherent defaults + iterate en PIE plutôt que de tuner à l'aveugle
- La décision flood visuals Option C v2 est en parallèle (voir memory `project_flood_containment_decision_2026_04_24`) — cette session est indépendante, ne pas mélanger
