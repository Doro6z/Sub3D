# Jitter P0 — Execution Plan InteriorFrame → SubMov.Velocity — 2026-04-20

Plan d'exécution strictement borné pour le premier fix issu du doc backlog [2026-04-19_jitter_faults_and_solutions.md](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_faults_and_solutions.md).

**Ce document n'implémente que S1 du backlog.** S2 (instrumentation), S3 (addendum), S4 (yaw compensation), et le reste sont **hors scope**. Ils seront pris après validation empirique de S1.

---

## Objectif

Éliminer la double-dérivation numérique dans `USubInteriorFrameComponent` qui transforme un jitter pose sub-cm en accélération-spike de 192 000 cm/s², amplifiée ensuite en 3 cm de jitter caméra visible en FPS.

**Cause structurelle** (consensus 4 agents) : `LocalLinearVelocity = FrameLocationDelta / DeltaTime` dérive la pose visuelle au lieu de lire le vecteur `Velocity` sim qui est propre par construction (forces physiques, drag, damping intégrés analytiquement dans `ApplyPhysics`).

**Fix** : lire `Sub->SubMovement->Velocity` directement, fallback finite-diff si indisponible.

---

## Scope

### Fichiers autorisés à modifier
- `Source/Sub3D/Submarine/SubInteriorFrameComponent.h`
- `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp`

### Aucun autre fichier ne doit être touché

Interdit dans ce patch :
- `SubMovementComponent.h/.cpp` — pas de nouveau getter nécessaire, `Velocity` est déjà public.
- `SubCrewCharacter.cpp` — pas de modif CameraSway (résolu en aval par S1).
- `SubCrewMovementComponent.cpp` — pas de modif `ApplyYawCompensation` (c'est S4).
- `SubmarineBase.h/.cpp` — pas de modif.
- Pas de nouvelle CVar — S2 arrive ensuite.
- Pas de doc addendum — S3 arrive ensuite.

### Scope fonctionnel

**Change exactement ça** :

Dans `USubInteriorFrameComponent::TickComponent`, section qui calcule `LocalLinearVelocity` et `LocalLinearAcceleration` ([SubInteriorFrameComponent.cpp:71-72](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71)) :

**Avant** :
```cpp
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

**Après** :
```cpp
FVector WorldLinearVelocity = FrameLocationDelta / DeltaTime;  // fallback: finite diff (legacy behavior)
if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
{
    if (const USubMovementComponent* SubMov = Sub->SubMovement)
    {
        WorldLinearVelocity = SubMov->Velocity;  // sim-side, signal propre par construction
    }
}
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(WorldLinearVelocity);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

### Hors scope fonctionnel strict

- **Rotation / angular velocity** : `LocalAngularVelocityDegrees` reste en finite-diff sur `FrameRotationDelta`. Pas de fix. Le test cruise a Yaw=0 donc ce chemin est noop et on veut mesurer l'impact du fix linéaire seul. Angular sera S1b si résiduel.
- **FrameLocationDelta / FrameRotationDelta** : conservés tels quels. Ils servent à d'autres consumers (UpdateRelativeState via WorldToLocal, debug log) qui ont besoin du vrai delta visuel du sub.
- **Le fallback finite-diff** : conservé explicitement. Si le sub n'a pas de `SubMovement` (cas edge, test minimal), on garde l'ancien comportement.
- **`Cast<ASubmarineBase>(Owner)`** : nécessite include `SubmarineBase.h` dans le .cpp. À ajouter si absent. `SubMovementComponent.h` probablement déjà présent (via composant header de SubmarineBase).

---

## Implémentation — à faire par agent Opus

L'agent Opus doit :

1. Lire [SubInteriorFrameComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp) intégralement avant de modifier.
2. Lire le header [SubmarineBase.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.h) pour vérifier que `SubMovement` est bien membre public (c'est le cas, [SubmarineBase.h:66](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.h:66) `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") USubMovementComponent* SubMovement;`).
3. Lire [SubMovementComponent.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h) pour vérifier que `Velocity` est bien membre public FVector. (Il l'est, utilisé à [SubmarineBase.cpp:1025-1028](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:1025).)
4. Ajouter les includes nécessaires dans `SubInteriorFrameComponent.cpp` (`#include "SubmarineBase.h"` si absent — `SubMovementComponent.h` déjà présent via existant).
5. Appliquer la modification exacte décrite dans "Scope fonctionnel" ci-dessus. Pas d'autres changes.
6. **Ne pas** ajouter de commentaires massifs, de logs, de code défensif additionnel. Garder le diff minimal.
7. **Ne pas** modifier le .h sauf si un include s'y révèle nécessaire (peu probable — tout se fait dans le .cpp).
8. Reporter le diff final pour review avant build.

---

## Review — à faire par agent Sonnet

Après l'implémentation par Opus, l'agent Sonnet doit :

1. Lire le diff appliqué sur les deux fichiers.
2. Vérifier contre la checklist :
   - [ ] Le changement est **strictement** dans `USubInteriorFrameComponent::TickComponent`.
   - [ ] Aucun autre fichier n'est modifié.
   - [ ] `Cast<ASubmarineBase>(Owner)` et `Sub->SubMovement` sont gardés par nullcheck.
   - [ ] Le fallback `FrameLocationDelta / DeltaTime` est préservé.
   - [ ] `LocalAngularVelocityDegrees` reste inchangé (finite-diff).
   - [ ] Aucun nouveau CVar, aucun nouveau log.
   - [ ] Le include `SubmarineBase.h` est présent si `Cast<ASubmarineBase>` utilisé.
   - [ ] Pas de code defensif au-delà du nullcheck du Sub et de SubMov.
   - [ ] Le fichier compile à la lecture (pas de typo type/nom évident).
3. Rejeter le patch si un de ces points échoue, en listant précisément.
4. Valider si tout passe, sans amplifier le scope.

---

## Build

Après review Sonnet OK :

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" Sub3DEditor Win64 Development "C:/Dev/Sub3D/Sub3D.uproject"
```

Si build échoue → retour à Opus avec l'erreur. Si succès → relance éditeur.

---

## Test PIE

Procédure minimale :

1. PIE standalone sur carte cruise habituelle (Proto03 ou équivalent).
2. Embark crew au helm.
3. Console : `sub.LogVisualInterp 1` (trace déjà existante utile).
4. `t.MaxFPS 90` pour reproduire les conditions du test précédent.
5. Thrust +1, cap droit, 10 secondes.
6. Observer visuellement le jitter caméra FPS.
7. Stop PIE.
8. Dans `Saved/Logs/Sub3D.log`, chercher les `LogSubCrewMovement` avec `LocalAccel=...` et `LocalSubLinearAcceleration=...`.

---

## Critères de succès

Le fix est validé si **les deux** sont vrais :

### Critère 1 — Chute forte des spikes d'accélération

Avant (observé) : `LocalAccel=V(X=-192246.16, Z=70.12)` — valeurs non-physiques de l'ordre de 10^5 cm/s².

Après (attendu) : `LocalAccel` avec valeurs de l'ordre de 10^2 à 10^3 cm/s² max. Plausible physiquement (forces / masse du sub).

Quantitativement : chute d'au moins **2 ordres de grandeur** sur les spikes max observés sur 10 secondes de cruise.

### Critère 2 — Baisse nette du jitter caméra FPS

Observation visuelle en FPS view pendant cruise. Le jitter perçu comme "le monde oscille ±3 cm" doit être **nettement réduit** — idéalement sub-perceptible au regard naturel. La baisse doit être franche, pas marginale.

---

## Actions selon résultat

| Résultat | Prochaine étape |
|---|---|
| Critère 1 OK + Critère 2 OK | Succès. Passer à **S2** (instrumentation propre). |
| Critère 1 OK mais Critère 2 partiel | Signaler ce qui reste visible. Le bruit accel est coupé mais l'amplificateur peut avoir autre chose. Aller en **S2** pour instrument propre puis **S5** Phase 12 pour isoler. |
| Critère 1 partiel ou KO | Le fix n'a pas pris. Debug : a-t-il été compilé ? Le sub a-t-il bien `SubMovement` valide en runtime ? Retour à Opus. |
| Critère 1 OK mais régression (comportement sub changé) | `Velocity` mal interprétée ? Revert et investigation. |

---

## Après S1 validé

Ordre strict de la suite, à décider test par test (pas de batch) :

1. **S2** — instrumentation propre : BACKWARD sur forward, EXTERNAL_WRITE rotation, CVar `sub.DisableCameraSway`. [faults doc S2](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_faults_and_solutions.md).
2. **S5** — Phase 12 empirique avec instruments propres. Matrice tests A/B/C/D.
3. Selon résultats, prendre dans l'ordre : S4 (yaw compensation), S6 (multi-step), S7 (contact transition), S9 (BP inspect), S11 (undo teleport type).

S3 (addendum findings doc) peut être fait en parallèle de n'importe laquelle des étapes ci-dessus.

**Règle** : tant que les résiduels n'apparaissent pas en PIE ou en log, on n'attaque pas les étapes suivantes. Le backlog reste un backlog tant qu'on n'a pas besoin d'y piocher.
