# Sub Jitter — Contre-Audit Architecture — 2026-04-19
**Auteur** : Claude Sonnet 4.6  
**Source auditée** : `2026-04-18_jitter_full_architecture_audit_procedure.md` + `2026-04-18_jitter_audit_findings.md`  
**Méthode** : lecture directe des fichiers source cités, vérification systématique de chaque affirmation, calcul indépendant des pathologies signalées.

---

## Verdict global

L'audit est **structurellement solide** : tick order correct, H1 bien écarté statiquement, amplificateur caméra identifié et chiffré. Deux problèmes sérieux :

1. **La cause profonde du `+20.84 / -3.74` est explicable par l'audit statique** — elle n'exige pas Phase 12 pour être posée. L'audit l'a laissée en suspens alors que la math la prédit directement.
2. **Fix #1 est trop narrow** : la correction proposée au niveau CameraSway laisse tout le reste du système crew sur le signal corrompu. La correction doit monter d'un cran, au niveau InteriorFrame.

Les autres conclusions (tick order, H1, H6, mesh) sont vérifiées et correctes.

---

## Affirmations vérifiées — conformes

### Amplificateur caméra (Phase 7.2)

Confirmé ligne à ligne. [SubCrewCharacter.cpp:234-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:234) :

```cpp
Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale;   // 0.002
Sway.Y = CMC->LocalSubLinearAcceleration.Y * CameraSwayAccelScale;
Sway.Z = CMC->LocalSubAngularVelocityDegrees.Y * CameraSwayAngularScale; // 0.05
Sway = Sway.GetClampedToMaxSize(CameraSwayMaxCm);                      // 3 cm
FPSCamera->SetRelativeLocation(FVector(Sway.X, Sway.Y, PostureZ + Sway.Z));
```

`LocalSubLinearAcceleration` vient de [SubCrewMovementComponent.cpp:169](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:169) qui lit `Frame->GetLocalLinearAcceleration()`, lui-même calculé à [SubInteriorFrameComponent.cpp:72](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:72) :

```cpp
LocalLinearVelocity    = InverseTransform(FrameLocationDelta / DeltaTime);  // line 71
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;  // line 72
```

Chaîne de dérivation confirmée : **pose visuelle → vitesse (1ère dérivée) → accélération (2ème dérivée)**. Instabilité numérique confirmée. ✓

### Tick order (Phase 5)

Prereq SubMov → InteriorFrame confirmé à [SubInteriorFrameComponent.cpp:41](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:41) :
```cpp
AddTickPrerequisiteComponent(SubMov);
```

Ordre SubMov → InteriorFrame → CrewMov → CrewChar::Tick est correct. Tous les readers tickent après le write interp. ✓

### OnRep_RepState en standalone (Phase 6)

Confirmé : REPNOTIFY_OnChanged par défaut, authority ne fire pas. ✓

### Aucun writer externe C++ (Phase 3)

Inventaire des writers confirmé. Tous dans `USubMovementComponent::TickComponent` ou `SimulateStep`. Writers listés lignes 165, 265, 552, 567, 615, 627, 633, 686 confirmés aux offsets cités. ✓

### Pas de mesh ou skeletal qui écrit le root (Phase 9)

HullMesh = UStaticMeshComponent NoCollision, pas d'AnimBP, pas de root motion. ✓

---

## Corrections et problèmes non identifiés

### 1. La cause du `+20.84 / -3.74` est déductible statiquement — pas besoin de Phase 12 pour la poser

L'audit note : *"L'observation +20.84 / -3.74 reste non expliquée par H1 (écartée) ni H2 pur. Reste H3."*

**Cette conclusion est prématurée.** La math du système interp prédit exactement ce pattern, sans bug.

**Démonstration :**

Soit `step_N` le déplacement sim X par step. `alpha_N` ∈ [0,1] l'alpha d'interp à la fin du tick N. InteriorFrame lit la pose VISUELLE (post-interp) chaque tick, calcule `delta = visual_N - visual_{N-1}`.

```
visual_N   = CurrSim_{N-1} + alpha_N    * step_N
visual_{N-1} = CurrSim_{N-2} + alpha_{N-1} * step_{N-1}

delta_N = step_{N-1} * (1 - alpha_{N-1}) + alpha_N * step_N
```

Pour vitesse constante `step = s` :
```
delta_N = s * (1 - alpha_{N-1} + alpha_N)
```

Si `alpha_N >> alpha_{N-1}` → grand delta (exemple : alpha_prev=0.05, alpha=0.95 → delta ≈ 1.9 × s)  
Si `alpha_N << alpha_{N-1}` → petit delta (exemple : alpha_prev=0.95, alpha=0.05 → delta ≈ 0.1 × s)

**Résultat** : même à vitesse sim parfaitement constante, la vélocité apparente lue par InteriorFrame oscille entre ~0.1×s et ~1.9×s selon la variation d'alpha entre frames. Ceci génère une accélération spike systématique.

**La borne inférieure de delta est toujours ≥ 0** pour une vitesse sim constante — jamais de BACKWARD pur sur X. Si le log montre `-3.74` sur le crew, c'est la combinaison du sous-step du sub (small delta frame) + la contribution propre du crew via `UpdateRelativeState/BasedMovement`. La position crew world = position sub visuelle + offset relatif crew, les deux contribuent.

**Conclusion** : ce n'est pas H3 (bug math). C'est le comportement prévu du système d'interp lu par un dérivateur numérique. Il n'est pas nécessaire d'attendre Phase 12 pour poser ce diagnostic — Phase 12 le quantifiera, mais elle ne le révèlera pas.

---

### 2. InteriorFrame lit la pose VISUELLE, pas la pose SIM — c'est le vrai point d'injection

L'audit écrit : *"InteriorFrame tick est garanti AFTER SubMovement"*, donc OK. C'est vrai pour l'ordre, mais manque le point principal :

**InteriorFrame lit la pose INTERPOLÉE (visuelle), pas la vélocité sim.**

[SubInteriorFrameComponent.cpp:63-64](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:63) :
```cpp
const FVector CurrentLocation = Owner->GetActorLocation();  // post-interp
const FVector PreviousLocation = ...;                       // post-interp frame précédente
FrameLocationDelta = CurrentLocation - PreviousLocation;    // delta entre deux poses visuelles
LocalLinearVelocity = InverseTransform(FrameLocationDelta / DeltaTime);  // dérivée visuelle
```

`SubMovementComponent::Velocity` ([SubMovementComponent.h:167](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:167)) est le vecteur vélocité **sim**, tenu à jour en `ApplyPhysics`. Il est lisse par construction (forces physiques, drag, masse). Il n'a aucune des oscillations liées à l'interpolation visuelle.

**InteriorFrame ne lit pas `SubMovement.Velocity`. Il calcule sa propre vélocité par différence de position visuelle.**

Ce choix de conception fait que `LocalLinearVelocity` (et donc `LocalLinearAcceleration`) reflètent le pattern d'interp, pas la physique du sub. Tous les consumers de l'InteriorFrame obtiennent un signal contaminé :
- Camera sway (identifié)
- Inertie crew pour ApplyYawCompensation (partiellement identifié)
- SubCrewAnimInstance.cpp:123 — lecture du transform sub pour anim
- Tout futur consumer de `Frame->GetLocalLinearAcceleration()`

---

### 3. Fix #1 est trop narrow — il faut corriger au niveau InteriorFrame

L'audit propose :
> *Fix #1 : remplacer la double dérivation pour camera sway par un accès direct à l'accélération sim-side.*

Problème : cette correction isole CameraSway sur un signal propre mais laisse tout le reste du système sur le signal contaminé. L'AccelFromSim qui serait calculé dans SubMovement pour CameraSway uniquement serait du code ad hoc qui duplique la logique d'InteriorFrame.

**Fix plus propre** : exposer `SubMovement->Velocity` (déjà en world space) à `InteriorFrame`, et remplacer ligne 71 :

```cpp
// Actuel (instable) :
LocalLinearVelocity = InverseTransform(FrameLocationDelta / DeltaTime);

// Proposé (stable) :
if (USubMovementComponent* SubMov = ...)
    LocalLinearVelocity = InverseTransform(SubMov->Velocity);
```

L'accélération ligne 72 peut rester une dérivée, mais elle dérivera un signal lisse (physique) au lieu d'un signal bruité (interp). Coût : ~10 lignes. Scope : corrige TOUS les consumers en une fois.

**Fix #1 corrigé** : modifier `USubInteriorFrameComponent::TickComponent` pour lire `SubMovement->Velocity` au lieu de dériver de `FrameLocationDelta`. CameraSway n'a rien à modifier. Impact : tout consumer d'InteriorFrame accélération reçoit un signal correct.

---

### 4. Multi-step ticks : PrevSimLocation capturé INSIDE la boucle while

[SubMovementComponent.cpp:207-222](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:207) :

```cpp
while (SimAccumulator >= FixedSimDt)
{
    PrevSimLocation = Owner->GetActorLocation();  // ← capturé DANS la boucle
    SimulateStep(FixedSimDt);
    ...
}
// Après la boucle :
// PrevSimLocation = start du DERNIER step (pas du premier)
// AuthoritativeLocation = end du dernier step
```

Le commentaire dans le code le documente : *"When the loop runs multiple steps in one tick (catch-up after a frame spike), only the last two poses are retained."*

**Conséquence** : sur un tick à N=2 steps (catch-up), l'interp visuelle couvre uniquement le dernier step. L'avance nette du tick est `step_1 + step_2`, mais l'interp va de `mid` à `end` (ratio `step_2` seulement). Le tick précédent (single-step) avait interpolé `prev_end → mid` (via alpha_{N-1}). La delta InteriorFrame pour ce tick = `(mid + alpha_N * step_2) - (prev_end + alpha_{N-1} * step_{N-1})` = valeur anormalement petite si alpha_{N-1} était grand.

**Ce mécanisme est une source documentée de variance dans le delta apparent** — il n'est pas buggé, mais il contribue à l'oscillation de vélocité perçue par InteriorFrame. Non mentionné dans l'audit.

---

### 5. La détection BACKWARD mesure le déplacement VISUEL, pas le déplacement SIM

[SubMovementComponent.cpp:307-315](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:307) :

```cpp
if (bHasLastPostTick && TickDelta.X < -BackwardThreshold)
{
    // Log BACKWARD
}
```

`TickDelta = PostInterpLoc - LastPostTickLocation` — c'est le delta de la pose VISUELLE entre frames.

Comme démontré ci-dessus (point 1), cette quantité peut être négative même si le sub avance continuellement en sim space, si le pattern d'interp le crée. **BACKWARD ne prouve pas que le sim est allé en arrière** — il prouve que la pose visuelle a reculé. C'est précieux pour diagnostiquer l'amplificateur perceptuel, mais il faut distinguer :

- BACKWARD + EXTERNAL_WRITE → H1
- BACKWARD sans EXTERNAL_WRITE → H2/H3 visuel (sain, quantifier l'amplitude)
- BACKWARD avec `sub.DisableVisualInterp 1` → vrai backward sim (H1 ou H3 profond)

Cette nuance n'est pas explicitée dans l'audit. Phase 12 Test B (`DisableVisualInterp 1`) est la clé pour séparer les deux.

---

### 6. Suppression de l'interp au contact crée des discontinuités de vélocité InteriorFrame

[SubMovementComponent.cpp:259](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:259) :

```cpp
if (!bDisableInterp && bHasSimBuffer && SimAccumulator > KINDA_SMALL_NUMBER && !bLastStepHadBlockingHit)
{
    // write interp
}
```

Quand `bLastStepHadBlockingHit = true` → actor reste sur CurrSim (pas d'interp, alpha = 1.0 implicite).  
Quand contact disparaît → alpha reprend à sa valeur normale (ex. 0.3).

InteriorFrame voit :  
- Frame contact : `visual = CurrSim` (delta = full step = grand)  
- Frame post-contact : `visual = Lerp(CurrSim, CurrSim_next, 0.3)` (delta = 0.3 × step = petit)

La vélocité interiorFrame passe de `step/dt` à `0.3×step/dt` en une frame → spike d'accélération. Non identifié dans l'audit.

---

### 7. FRotator::operator- pour delta de rotation — edge case gimbal lock

[SubInteriorFrameComponent.cpp:67](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:67) :

```cpp
FrameRotationDelta = (CurrentRotation - PreviousRotation).GetNormalized();
```

`FRotator::operator-` est component-wise. `.GetNormalized()` ramène chaque composant à [-180, 180]. Correct pour yaw et roll. **Pour pitch** : si le sub dépasse ±90° (gimbal lock territory), UE peut représenter la même orientation avec des valeurs Pitch discontinues entre frames. En pratique le sub est limité à ~30° de pitch max ([SubMovementComponent.h:117](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:117)), donc ce cas ne se produit pas. Mais c'est une borne de validité non documentée.

---

### 8. BP_Submarine_Craniata non inspecté — caveat H1 sous-évalué

L'audit (Phase 1) exige : *"Inspecter le BP_Submarine_Craniata via le content browser pour les composants ajoutés en BP."*

Cette étape n'a pas pu être exécutée (fichiers .uasset non lisibles statiquement). La conclusion *"H1 statiquement écartée"* est donc partielle : elle vaut pour le C++, pas pour les composants BP. Un composant Blueprint avec tick non déclaré en C++ serait invisible à l'audit statique.

La formulation du findings doc (*"H1 statiquement écartée"*) est trop forte. Corriger en : *"H1 écartée sur le path C++ — composants BP_Submarine_Craniata non vérifiables par audit statique."*

---

## Hiérarchie des causes révisée

| Rang | Cause | Type | Effet observable | Nécessite Phase 12 ? |
|---|---|---|---|---|
| 1 | InteriorFrame dérive vélocité de la pose visuelle, pas de `SubMovement.Velocity` | Structurel | Spikes accélération proportionnels à variance alpha interp | Non — prouvé statiquement |
| 2 | CameraSway amplifie via double-dérivée puis clamp saturé | Amplificateur | 0.3 cm pose → 3 cm caméra visible | Non — prouvé statiquement |
| 3 | Variance alpha interp frame-to-frame (oscillation inhérente) | Structurel | Variance vélocité apparente même à vitesse constante | Non — déductible mathématiquement |
| 4 | Multi-step catch-up : PrevSimLocation = milieu du trajet | Documenté | Delta visuel anormalement petit après frame lente | Non — visible dans le code |
| 5 | Suppression interp au contact → transition discontinue | Edge case | Spike vélocité à chaque contact/décontact | Phase 12 (contact scenario) |
| 6 | Writer BP caché (non inspecté) | H1 résiduel | EXTERNAL_WRITE log si présent | Phase 12 Test A obligatoire |
| 7 | Bug math edge case (wrap, gimbal) | H3 | BACKWARD log sans EXTERNAL_WRITE, vitesse sim > 0 | Phase 12 Test B |

---

## Fixes révisés

### Fix #1 (P0) — Corriger InteriorFrame pour lire SubMovement.Velocity

**Fichier** : [SubInteriorFrameComponent.cpp:71](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71)

```cpp
// Avant (instable) :
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;

// Après (stable) :
if (USubMovementComponent* SubMov = Owner->FindComponentByClass<USubMovementComponent>())
{
    LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(SubMov->Velocity);
}
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

**Bénéfice** : tous les consumers (CameraSway, ApplyYawCompensation, anim, futurs) reçoivent un signal physiquement correct. `SubMov->Velocity` est public ([SubMovementComponent.h:167](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:167)), pas de nouveau getter nécessaire.  
**Coût** : ~8 lignes. Zéro impact en standalone si SubMov est toujours présent.  
**Risque** : l'accel dérivée de `SubMov->Velocity` sera toujours lisse — le CameraSway deviendra proportionnel à la vraie physique, pas à du bruit. Tester le feel en PIE.

### Fix #2 (P0, conditionnel si interp reste active) — Supprimer l'oscillation de FrameLocationDelta en ajoutant un smooth

Si pour une raison de design InteriorFrame doit conserver sa dérivée de pose (ex : pour tracker le mouvement visuel réel du sub perçu par le crew), ajouter un smooth exponentiel sur `LocalLinearVelocity` avant de dériver l'accélération. La dérivée sur un signal lissé ne spike pas. Exemple :

```cpp
const float SmoothAlpha = FMath::Clamp(DeltaTime / (DeltaTime + VelocitySmoothingTau), 0.f, 1.f);
LocalLinearVelocity = FMath::Lerp(PreviousLocalLinearVelocity, RawLocalLinearVelocity, SmoothAlpha);
```

Avec `VelocitySmoothingTau = 0.05f` (50 ms), les spikes d'une frame sont atténués de ~83%.

**Moins propre que Fix #1** — réserve ce fix pour les cas où Fix #1 change le feel de manière inacceptable.

### Fix #3 (P1) — Late-update caméra

Inchangé par rapport à l'audit original. Reste valide une fois Fix #1 appliqué.

### Fix #4 (P1) — Investiguer contact-transition spikes

Ajouter un guard dans InteriorFrame pour détecter et atténuer la discontinuité quand `bLastStepHadBlockingHit` change d'état. Par exemple, ne pas accumuler d'accélération sur la frame de transition.

---

## Ce qui n'a pas besoin de correction dans l'audit

- Phase 5 (tick order) — correcte, conclusion valide
- Phase 6 (OnRep) — correcte
- Phase 7.1 (ApplyYawCompensation) — correcte, noop au cruise sans rotation yaw
- Phase 7.3 (BasedMovement) — correcte, comportement UE attendu
- Phase 8 (side-effects) — correcte
- Phase 11 (attachement crew) — correcte

---

## Recommandation Phase 12

Phase 12 reste utile mais son interprétation change :

| Test | Ce qu'on cherche maintenant |
|---|---|
| 12.2 `DisableVisualInterp 1` | Confirme que le jitter visuel est 100% dans la pipeline interp+dérivée (devrait disparaître ou devenir sub-perceptible). Si jitter persiste → H1 BP ou H3 sim réel. |
| 12.3 Disable CameraSway | Confirme l'amplitude de l'amplificateur (devrait réduire drastiquement). |
| 12.6 Sub immobile | Confirme absence H1 — si EXTERNAL_WRITE log fire, chercher dans BP. |
| 12.7 Bypass interp total | Référence zéro-artefact : si jitter ici → H1 ou H3 sim pur. |

**Si Fix #1 (InteriorFrame → SubMov.Velocity) est appliqué avant Phase 12 :** la plupart des tests deviennent des confirmations de propreté plutôt que de diagnostics. Recommandé d'appliquer Fix #1, puis de faire Phase 12 Tests 12.2 et 12.6 pour fermer H1 BP résiduel.

---

## Synthèse

Le jitter perçu est quasi-certainement l'amplificateur caméra (structurel, pas un bug) qui transforme le bruit inhérent à la lecture de pose visuelle par InteriorFrame en oscillation caméra ±3 cm. La cause n'est pas un writer externe ni un bug de math — c'est l'architecture InteriorFrame qui différencie un signal d'interp au lieu du vecteur physique sim.

**Fix unique P0** : InteriorFrame lit `SubMovement.Velocity` au lieu de `FrameLocationDelta / dt`. Tout le reste se déroule en aval.
