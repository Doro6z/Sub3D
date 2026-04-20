# Jitter P0 Execution Plan - InteriorFrame Signal Cleanup - 2026-04-20

Reference d'autorite projet : [reports/plans/2026-04-10_first_playable_strategic_analysis.md](/C:/Dev/Sub3D/reports/plans/2026-04-10_first_playable_strategic_analysis.md)

## Objet

Corriger le symptome principal de jitter percu sans refactor large :

- conserver `SubMovement` comme source autoritaire du mouvement
- conserver `InteriorFrame` comme adaptateur de repere pour le crew
- retirer de `InteriorFrame` la derivation de vitesse a partir de la pose visuelle interpolee

Le but de cette phase n'est pas de fermer toute hypothese H1/H3. Le but est de supprimer la source de signal inertiel corrompu qui alimente le crew et la camera.

## Root cause retenue pour P0

Le signal `LocalSubLinearAcceleration` expose par `USubInteriorFrameComponent` est derive de :

- `FrameLocationDelta = CurrentLocation - PreviousLocation`
- puis `LocalLinearVelocity = FrameLocationDelta / DeltaTime`
- puis `LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime`

Ce calcul lit la pose actor apres presentation visuelle et non la vitesse sim autoritaire. Le resultat est un signal instable qui amplifie les variations d'interpolation du sub. Ce signal est ensuite lu par :

- `USubCrewMovementComponent`
- `ASubCrewCharacter::Tick` pour `CameraSway`
- `USubCrewAnimInstance`

## Scope P0

### Inclus

- `Source/Sub3D/Submarine/SubInteriorFrameComponent.h`
- `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp`

### Exclus

- refactor `BasedMovement`
- refactor yaw crew
- late-update camera
- inspection Blueprint
- changement du path replication
- nouveau systeme de smoothing generalise

## Strategie

P0 corrige le producteur du signal, pas les consumers.

Le patch doit faire ceci :

1. `USubInteriorFrameComponent` cache une reference vers `USubMovementComponent`.
2. `LocalLinearVelocity` est derive de `SubMovement->Velocity` transforme dans le repere local du sub.
3. `LocalLinearAcceleration` reste une derivee temporelle, mais appliquee a une vitesse sim-side propre.
4. `FrameLocationDelta` et `FrameRotationDelta` restent disponibles pour debug et conversions de frame.
5. Les consumers existants ne changent pas d'API.

## Patch exact par fichier

### 1. `Source/Sub3D/Submarine/SubInteriorFrameComponent.h`

Modifications requises :

- ajouter un forward declaration de `USubMovementComponent`
- ajouter un pointeur cache prive vers `USubMovementComponent`
- ajouter un flag prive indiquant si la vitesse lineaire de frame vient de la sim ou du fallback pose-delta

Etat cible :

- `WorldToLocal`, `LocalToWorld`, `GetFrameLocationDelta` et les getters publics restent inchanges
- aucune nouvelle API Blueprint n'est necessaire

## 2. `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp`

### `BeginPlay`

Modifications requises :

- resoudre `USubMovementComponent` via `Owner->FindComponentByClass<USubMovementComponent>()`
- conserver le prerequis `AddTickPrerequisiteComponent(SubMov)`
- initialiser le cache movement pour les ticks suivants
- logger clairement si `SubMovement` est absent

### `TickComponent`

Remplacement requis dans le bloc `DeltaTime > KINDA_SMALL_NUMBER` :

Etat actuel :

```cpp
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

Etat cible :

```cpp
if (CachedSubMovement.IsValid())
{
    LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(CachedSubMovement->Velocity);
    bUsingSimVelocity = true;
}
else
{
    LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
    bUsingSimVelocity = false;
}

LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

Contraintes :

- conserver le calcul actuel de `FrameLocationDelta`
- conserver le calcul actuel de `FrameRotationDelta`
- conserver le calcul actuel de `LocalAngularVelocityDegrees`
- ne pas changer la semantique de `bFrameValid`

### Debug logging

Le log `InteriorFrame` existant doit inclure la source de `LocalLinearVelocity` :

- `Source=SimVelocity`
- `Source=PoseDeltaFallback`

But :

- verification rapide en PIE sans ambiguity sur la branche active

## Verification requise

### Verification statique

- build compile-safe sur les deux fichiers modifies
- aucun changement de signature publique utilisee par les consumers
- aucun changement de tick prerequisite existant

### Verification runtime minimale

Executer en PIE standalone :

1. cruise stable en FPS view
2. `bDebugLogFrame = true` sur `InteriorFrame`
3. confirmer que le log indique `Source=SimVelocity`
4. comparer `LocalAccel` avant/apres sur la meme situation de cruise
5. verifier que la camera n'entre plus immediatement en clamp quasi permanent

### Verification symptome

Le resultat attendu de P0 est :

- reduction forte des spikes `LocalSubLinearAcceleration`
- reduction forte du jitter percu en FPS
- aucune regression sur `WorldToLocal` et `RelativeLocation` crew

## Resultat attendu apres P0

### Ce que P0 doit corriger

- la majeure partie du jitter percu par la camera FPS
- la majeure partie du bruit inertiel lu par le crew
- la majeure partie du bruit inertiel lu par l'anim crew

### Ce que P0 ne pretend pas corriger

- un writer Blueprint cache
- un drift rotation-only
- un residuel de presentation camera
- les cas contact/no-contact
- la perception finale si `CameraSway` est volontairement trop agressive meme avec un signal propre

## Suite immediate apres P0

Une fois P0 verifie :

1. mesurer le residuel sans changer d'architecture
2. si un jitter visuel significatif subsiste, traiter la presentation camera
3. seulement ensuite reprendre H1/H3 avec instrumentation corrigee

## Decision de mise en oeuvre

Ce plan part sur le plus petit patch coherent :

- producteur corrige dans `InteriorFrame`
- consumers inchanges
- verification courte en PIE

La phase est terminee seulement si le signal inertiel crew n'est plus derive de la pose visuelle interpolee.
