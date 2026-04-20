# Jitter — Scan Méthodique des Failles & Solutions — 2026-04-19

Agrégation systématique de toutes les failles identifiées par les 4 audits (A1 Sonnet 4.7 1M, A2 Gemini 3.1 Pro, A3 Sonnet 4.6, A4 GPT54). Chaque faille est cataloguée, chiffrée, tracée à sa source, et associée à une solution priorisée.

Référence : [2026-04-19_jitter_audit_4agents_comparison.md](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_audit_4agents_comparison.md).

---

## Structure

- **Failles F1-F13** : chaque problème identifié, avec location code, description, sévérité, source.
- **Gaps G1-G2** : trous non résolus par l'audit statique.
- **Solutions S1-S11** : propositions concrètes avec code, impact, coût.
- **Matrice de priorité** : ordre d'exécution.

---

## Failles — Catégorie A : Architecture / Logique

### F1 — InteriorFrame dérive numériquement la pose visuelle [CRITIQUE]

**Source** : 4/4 (consensus).

**Location** : [SubInteriorFrameComponent.cpp:71-72](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71)

```cpp
LocalLinearVelocity     = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

**Problème** : double dérivation finie d'un signal `Owner->GetActorLocation()` qui varie micro-frame par frame (alpha interp variance, sweep collision, potentiels writers BP). Chaque dérivation amplifie le bruit par `1/DeltaTime`. À 90 FPS, 0.3 cm de jitter pose devient accél ≈ 192 000 cm/s² observés en log.

**Sévérité** : CRITIQUE — cause structurelle de tout ce qui suit.

**Résout par** : S1.

---

### F2 — CameraSway saturé consomme le signal F1 [MAJEURE]

**Source** : 4/4.

**Location** : [SubCrewCharacter.cpp:235-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:235)

```cpp
Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale;  // 0.002
Sway.Y = CMC->LocalSubLinearAcceleration.Y * CameraSwayAccelScale;
Sway.Z = CMC->LocalSubAngularVelocityDegrees.Y * CameraSwayAngularScale;  // 0.05
Sway = Sway.GetClampedToMaxSize(CameraSwayMaxCm);  // 3 cm
FPSCamera->SetRelativeLocation(FVector(Sway.X, Sway.Y, PostureZ + Sway.Z));
```

**Problème** : le clamp à 3 cm se sature en permanence sur les accélérations-spikes de F1 (192k * 0.002 = 384 cm → clampé à 3). La caméra oscille constamment à la limite.

**Sévérité** : MAJEURE — c'est ce que le joueur VOIT.

**Résout par** : S1 auto (le signal consommé devient propre). Alternative S5b si S1 ne suffit pas.

---

### F3 — AnimInstance stumble consomme aussi F1 [MOYENNE]

**Source** : 2/4 (A3, A4).

**Location** : [SubCrewAnimInstance.cpp:149-150, 349-366](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:149)

**Problème** : `ReadInputState` copie `LocalSubLinearAcceleration`, `ComputeSubMotion` l'utilise pour le stumble du crew. Atténué par `Instability = 1 - SupportQuality` mais reste un 2e amplificateur visible dans l'animation.

**Sévérité** : MOYENNE — si on fix F2 sans fix F1, l'anim continuerait à jitter.

**Résout par** : S1 auto.

---

### F4 — ApplyYawCompensation dépend d'un delta yaw dérivé [MOYENNE]

**Source** : 1/4 implicite (A3 signale la propagation générale).

**Location** : [SubCrewMovementComponent.cpp:394](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:394)

```cpp
const float YawDelta = Frame->GetFrameRotationDelta().Yaw;
```

`GetFrameRotationDelta` vient du même mécanisme que F1 : `(CurrentRotation - PreviousRotation).GetNormalized()` sur la pose visuelle.

**Problème** : si le sub yaw jitter (pas dans le cruise test Yaw=0 mais possible en rotation), le crew yaw inherits et tourne par micro-steps.

**Sévérité** : MOYENNE — pas visible dans le test cruise actuel mais symétrique à F1 pour la rotation.

**Résout par** : S4.

---

### F5 — Multi-step catch-up : PrevSim capturé dans la boucle [FAIBLE]

**Source** : 1/4 (A3).

**Location** : [SubMovementComponent.cpp:207-222](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:207)

```cpp
while (SimAccumulator >= FixedSimDt)
{
    PrevSimLocation = Owner->GetActorLocation();  // capturé INSIDE while
    SimulateStep(FixedSimDt);
    ...
}
```

**Problème** : sur une frame catch-up à N steps, `PrevSimLocation` finit par valoir la pose au DÉBUT du dernier step (pas du premier). L'interp couvre seulement le dernier step alors que l'avance nette = N steps. Discontinuité dans la vélocité apparente lue par InteriorFrame.

**Sévérité** : FAIBLE à framerate stable, MOYENNE en cas de hitch.

**Résout par** : S6.

---

### F6 — Contact transition discontinuity [MOYENNE]

**Source** : 1/4 (A3).

**Location** : [SubMovementComponent.cpp:259](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:259)

```cpp
if (!bDisableInterp && bHasSimBuffer && SimAccumulator > KINDA_SMALL_NUMBER && !bLastStepHadBlockingHit)
```

**Problème** : quand `bLastStepHadBlockingHit` bascule :
- Contact→non-contact : alpha effectif passe de 1.0 (interp suppress) à valeur normale (ex 0.3). Delta InteriorFrame passe de `step/dt` à `0.3*step/dt` en 1 frame.
- Non-contact→contact : symétrique.
Spike d'accélération à chaque transition.

**Sévérité** : MOYENNE — visible aux collisions (Option A du doc fluidity est la source directe).

**Résout par** : S7.

---

### F7 — TickPosture et Tick écrivent tous deux FPSCamera [FAIBLE]

**Source** : 1/4 (A4 F6).

**Location** : 
- [SubCrewMovementComponent.cpp:710-747](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:710) (écrit FPSCamera)
- [SubCrewCharacter.cpp:226-240](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp:226) (réécrit FPSCamera complet)

**Problème** : deux writers dans la même frame sur le même composant. Le second (Tick) gagne. Le premier est donc dead-code (ou overwriting utile selon timing).

**Sévérité** : FAIBLE — fonctionne mais archi confuse.

**Résout par** : S8.

---

## Failles — Catégorie B : Instrumentation (mon propre code)

### F8 — Détecteur BACKWARD mesure World X [FORTE]

**Source** : 1/4 (A4 F1).

**Location** : [SubMovementComponent.cpp:307-315](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:307)

```cpp
if (bHasLastPostTick && TickDelta.X < -BackwardThreshold)
{
    UE_LOG(LogSubMovement, Error, TEXT("SUB_TRACE BACKWARD | ..."));
}
```

**Problème** : `TickDelta.X` est en World X. Si le sub a un heading différent de +X, un déplacement forward peut donner un delta X négatif. Le log "BACKWARD" peut fire sans sim regression réelle.

**Note** : dans le test cruise du user, Yaw=0.00 exactement, donc X = forward. Ce bug n'invalide pas les observations du test spécifique. Mais biaise toute généralisation.

**Sévérité** : FORTE — biaise Phase 12 si test fait avec heading ≠ 0.

**Résout par** : S2a.

---

### F9 — Détecteur EXTERNAL_WRITE ne check pas la rotation [FORTE]

**Source** : 1/4 (A4 F4).

**Location** : [SubMovementComponent.cpp:296-304](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:296) + [:319](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:319) (rotation stockée mais jamais comparée)

```cpp
const FVector ExtDrift = bHasLastPostTick ? (PreUndoLoc - LastPostTickLocation) : FVector::ZeroVector;
...
if (bHasLastPostTick && ExtDrift.Size() > ExtDriftThreshold) { ... log EXTERNAL_WRITE ... }
...
LastPostTickRotation = PostInterpRot;  // stocké
bHasLastPostTick = true;               // mais jamais utilisé pour comparaison
```

**Problème** : un writer rotation-only (ex : un composant BP qui force un yaw snap) n'écrit pas la location mais écrit la rotation. Mon détecteur ne le verrait pas.

**Sévérité** : FORTE — trou de couverture volontaire.

**Résout par** : S2b.

---

### F10 — Pas de toggle pour disable CameraSway seul [MOYENNE]

**Source** : 1/4 (A4 "test camera sway OFF manquant").

**Problème** : Phase 12 actuelle ne permet de toggle que l'interp globale (`sub.DisableVisualInterp`). Impossible de séparer « test l'amplificateur seul » (sway off + interp on) de « test le signal source seul » (sway on + interp off).

**Sévérité** : MOYENNE — empêche une ségrégation propre cause vs amplificateur.

**Résout par** : S2c.

---

## Failles — Catégorie C : Documentation

### F11 — Procedure "12 phases" mais énumérée 0-13 [TRIVIALE]

**Source** : 1/4 (A4 méta).

**Location** : [2026-04-18_jitter_full_architecture_audit_procedure.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_full_architecture_audit_procedure.md)

**Sévérité** : TRIVIALE.

**Résout par** : S10.

---

### F12 — Findings doc sur-affirme [MOYENNE]

**Source** : 2/4 (A3 + A4).

**Location** : [2026-04-18_jitter_audit_findings.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_jitter_audit_findings.md)

Formulations à corriger :
- "H1 statiquement écartée" → "écartée sur path C++ runtime relu, BP non vérifié".
- "Tous les readers tickent après SubMov" → "path critique prouvé, reste = inférence par default TG_PrePhysics".

**Sévérité** : MOYENNE — crédibilité et lecture future.

**Résout par** : S3.

---

### F13 — BP_Submarine_Craniata jamais inspecté [MOYENNE]

**Source** : 2/4 (A3 + A4).

**Problème** : la procédure Phase 1 exige l'inspection BP, aucun audit ne l'a fait (asset binaire non lisible statiquement).

**Sévérité** : MOYENNE — laisse un trou H1 ouvert.

**Résout par** : S9.

---

## Gaps non résolus par l'audit statique

### G1 — "-3.74" sur la pose sub non expliqué [INCONNUE]

**Source** : 4/4 (aucun ferme cleanly).

**Problème** : le pattern observé `+20.84 / -3.74` sur `LogSubInteriorFrame` du sub lui-même. La math interp donne intervalle [0, 2s] sur delta per-tick à vitesse constante. Donc delta négatif impossible théoriquement. Trois pistes restantes :
- H1 résiduel BP.
- H3 edge case math que personne n'a trouvé.
- Bug d'instrumentation — mais Yaw=0 dans le test donc F8 ne s'applique pas directement.

**Sévérité** : INCONNUE — dépend de la cause.

**Résout par** : Phase 12 après S1+S2 (S5).

---

### G2 — Undo utilise TeleportType::TeleportPhysics [HYPOTHÉTIQUE]

**Source** : implicite dans 4 audits.

**Location** : [SubMovementComponent.cpp:165](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:165)

```cpp
Owner->SetActorLocationAndRotation(AuthoritativeLocation, AuthoritativeRotation, false, nullptr, ETeleportType::TeleportPhysics);
```

**Problème** : `TeleportPhysics` reset le physics state. Possibles side-effects (overlap events, physics body sync, collision state). Entre l'undo et le write interp, des callbacks pourraient fire.

**Sévérité** : HYPOTHÉTIQUE — non confirmé.

**Résout par** : S11 (test diagnostic).

---

## Solutions — Priorisées

### S1 [P0] — Fix structural InteriorFrame → lire SubMov.Velocity (résout F1, F2, F3 en cascade)

**Fichier** : [SubInteriorFrameComponent.cpp:71](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:71)

```cpp
// AVANT (instable)
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;

// APRÈS (stable)
FVector WorldVelocity = FrameLocationDelta / DeltaTime;  // fallback
if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
{
    if (Sub->SubMovement)
    {
        WorldVelocity = Sub->SubMovement->Velocity;  // signal sim pur
    }
}
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(WorldVelocity);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
```

**Impact** : fixe tous les consumers d'accel en cascade. 0.3 cm de jitter pose reste sub-perceptible.

**Coût** : ~10 lignes. Ajouter include `SubmarineBase.h` si pas déjà.

**Risque** : faible. `Velocity` est membre public ([SubMovementComponent.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h)), pas besoin de getter. Sur path remote, `Velocity` est répliqué dans FSubmarineNetState et mis à jour par InterpolateClient — donc même chemin de fix marche pour remote.

**Side-effect** : accélération angulaire aussi — même fix pour `LocalAngularVelocity` si consommée (à vérifier).

---

### S2 [P0] — Fix instrumentation (résout F8, F9, F10)

**S2a — BACKWARD projection forward** : [SubMovementComponent.cpp:307](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:307)

```cpp
// AVANT
if (bHasLastPostTick && TickDelta.X < -BackwardThreshold)

// APRÈS
const FVector Forward = Owner->GetActorForwardVector();
const float ForwardDelta = FVector::DotProduct(TickDelta, Forward);
if (bHasLastPostTick && ForwardDelta < -BackwardThreshold)
```

Log update pour montrer `ForwardDelta` au lieu de `TickDelta.X`.

**S2b — EXTERNAL_WRITE rotation check** : [SubMovementComponent.cpp:296](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:296)

```cpp
// Ajouter après ExtDrift:
FRotator ExtRotDrift = bHasLastPostTick 
    ? (PreUndoRot - LastPostTickRotation).GetNormalized() 
    : FRotator::ZeroRotator;
const float RotDriftMag = FMath::Max3(
    FMath::Abs(ExtRotDrift.Pitch),
    FMath::Abs(ExtRotDrift.Yaw),
    FMath::Abs(ExtRotDrift.Roll));

// Ajouter à la condition EXTERNAL_WRITE :
const float RotDriftThreshold = CVarSubTraceExternalRotDriftDeg.GetValueOnGameThread();
if (bHasLastPostTick && (ExtDrift.Size() > ExtDriftThreshold || RotDriftMag > RotDriftThreshold))
{
    // Log avec les deux composantes
}
```

Ajouter CVar `sub.TraceExternalRotDriftDeg` (défaut 0.01°).

**S2c — Toggle CameraSway** : nouveau CVar dans [SubCrewCharacter.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewCharacter.cpp)

```cpp
static TAutoConsoleVariable<bool> CVarSubCrewDisableCameraSway(
    TEXT("sub.DisableCameraSway"),
    false,
    TEXT("Skip the FPS camera sway computation to isolate the perceptual amplifier in Phase 12."),
    ECVF_Default);

// Dans Tick, entourer le bloc Sway de :
if (!CVarSubCrewDisableCameraSway.GetValueOnGameThread())
{
    // ... existing sway code
}
else
{
    // Reset à posture Z only (no sway)
    FPSCamera->SetRelativeLocation(FVector(0.f, 0.f, PostureZ));
}
```

**Coût total** : ~40 lignes entre les 3.

---

### S3 [P0] — Addendum findings doc (résout F12)

Créer [2026-04-19_jitter_audit_findings_addendum.md](/C:/Dev/Sub3D/reports/analysis/2026-04-19_jitter_audit_findings_addendum.md) qui :
- Corrige "H1 statiquement écartée" en formulation nuancée.
- Intègre findings isolés A3 (F5, F6, math variance) et A4 (F8, F9, F7).
- Référence comparison doc et faults doc comme sources.

**Coût** : ~1 doc.

---

### S4 [P1] — Fix F4 ApplyYawCompensation utilise sim yaw rate

**Fichier** : [SubCrewMovementComponent.cpp:394](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:394)

```cpp
// AVANT
const float YawDelta = Frame->GetFrameRotationDelta().Yaw;

// APRÈS
float YawDelta = 0.f;
if (const ASubmarineBase* Sub = Frame->GetOwner() ? Cast<ASubmarineBase>(Frame->GetOwner()) : nullptr)
{
    if (Sub->SubMovement)
    {
        YawDelta = Sub->SubMovement->GetYawRateDegPerSec() * DeltaTime;
    }
}
```

Alternative si getter GetYawRateDegPerSec n'existe pas : exposer le membre ou ajouter getter.

**Impact** : rotation crew suit sim yaw directement, pas dérivé de pose. Cohérent avec S1.

**Coût** : ~15 lignes. Pourrait nécessiter signature change de ApplyYawCompensation pour recevoir DeltaTime.

---

### S5 [P1] — Phase 12 post S1+S2

Exécuter la matrice empirique du handoff AVEC les nouveaux instruments :

| Test | CVars | Observation attendue si S1 a fixé |
|---|---|---|
| A — baseline | `sub.LogVisualInterp 1` | Aucun EXTERNAL_WRITE ni BACKWARD. Jitter caméra sub-perceptible. |
| B — interp off | `sub.DisableVisualInterp 1` | Identique à A, confirme que l'interp n'est pas la source. |
| C — sway off | `sub.DisableCameraSway 1` | Si A et B montrent toujours du jitter visible sur le SUB (pas la caméra), le problème n'est pas perceptuel mais réel → continuer vers S9/S11. |
| D — combo | `DisableVisualInterp 1` + `DisableCameraSway 1` | Référence "minimum" : ce qui reste est intrinsèque à la sim. |

**Résout** : G1 par observation.

**Coût** : ~30 min user PIE.

---

### S5b [P1 conditionnel] — Lowpass filter sur LocalSubLinearVelocity si S1 insuffisant

Si S1 laisse encore du bruit résiduel (peu probable mais possible), ajouter smoothing :

```cpp
const float SmoothTau = VelocitySmoothingTau;  // ex 0.05s
const float Alpha = FMath::Clamp(DeltaTime / (DeltaTime + SmoothTau), 0.f, 1.f);
LocalLinearVelocity = FMath::Lerp(PreviousLocalLinearVelocity, RawLocalLinearVelocity, Alpha);
```

**Coût** : ~6 lignes + 1 UPROPERTY.

**Note** : moins propre que S1 pur. À utiliser seulement si S1 a un feel "trop calme" ou incomplet.

---

### S6 [P2] — Fix F5 Multi-step catch-up

**Fichier** : [SubMovementComponent.cpp:207-222](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:207)

**Option A (simple, préservée)** : capturer PrevSim AVANT la boucle, pas dans.

```cpp
// Avant la boucle
FVector StartOfTickLocation = Owner->GetActorLocation();
FRotator StartOfTickRotation = Owner->GetActorRotation();

while (SimAccumulator >= FixedSimDt)
{
    SimulateStep(FixedSimDt);
    SimAccumulator -= FixedSimDt;
    ++SimFrameCounter;
    ++StepsThisTick;
    bSimulated = true;
    bHasSimBuffer = true;
}

if (StepsThisTick > 0)
{
    PrevSimLocation = StartOfTickLocation;  // pose début du tick
    PrevSimRotation = StartOfTickRotation;
}
```

**Impact** : sur catch-up à N steps, PrevSim = début du tick, CurrSim = fin du tick. Interp couvre le trajet complet au lieu seulement du dernier step. Lissage des catch-up frames.

**Risque** : sur vitesse constante, équivalent au comportement actuel. Sur hitch, rend les recoveries plus smooth.

**Coût** : ~10 lignes.

---

### S7 [P2] — Fix F6 Contact transition

**Fichier** : [SubMovementComponent.cpp:259](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:259)

**Option recommandée** : au lieu de suppress interp binairement, garder un alpha effectif qui smooth entre les deux régimes.

```cpp
// Ajouter membre
float ContactFadeAlpha = 0.f;  // 0 = full interp, 1 = contact hold

// Chaque tick :
const float ContactFadeTau = 0.08f;  // 80 ms transition
const float TargetFade = bLastStepHadBlockingHit ? 1.f : 0.f;
const float FadeAlpha = FMath::Clamp(DeltaTime / ContactFadeTau, 0.f, 1.f);
ContactFadeAlpha = FMath::Lerp(ContactFadeAlpha, TargetFade, FadeAlpha);

if (bHasSimBuffer && SimAccumulator > KINDA_SMALL_NUMBER)
{
    const float RawAlpha = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
    const float EffectiveAlpha = FMath::Lerp(RawAlpha, 1.f, ContactFadeAlpha);
    // ... Lerp avec EffectiveAlpha
}
```

**Impact** : transition contact/non-contact devient un ramp de 80ms au lieu d'un step. Élimine les spikes.

**Coût** : ~15 lignes + 1 UPROPERTY pour tau.

---

### S8 [P2] — Fix F7 FPSCamera 2 writers

**Fichier** : [SubCrewMovementComponent.cpp:710-747](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:710)

Analyser si TickPosture a un but spécifique (z-only probable pour prone transition). Option simple : confirmer dead-code et supprimer. Option sûre : laisser TickPosture écrire Z seul, Tick écrit X/Y+sway.

À décider après lecture complète de TickPosture (non fait).

**Coût** : 5-20 lignes selon choix.

---

### S9 [P2] — Inspecter BP_Submarine_Craniata (résout F13 et potentiellement G1)

Ouvrir `Content/.../BP_Submarine_Craniata` dans l'éditeur. Documenter :
- Composants ajoutés en BP (au-delà des C++).
- Event Graph : tout Event Tick, Begin Play, Construction.
- Tout SetActorLocation / SetActorTransform / AddWorldOffset dans les events.

Si writer BP identifié → fix (le désactiver ou le cadrer proprement).

Si aucun writer BP → H1 BP clôturée.

**Coût** : 15-30 min user, reporting.

---

### S10 [P3] — Fix F11 procedure

Renuméroter Phase 0-13 en Phase 1-14, ou ajuster le header "12 phases" en "14 phases". Trivial.

---

### S11 [P3 conditionnel] — Tester undo avec TeleportType::None (résout G2 si applicable)

Si après S1+S2+S5, il reste des EXTERNAL_WRITE anomalies non expliqués, tester :

**Fichier** : [SubMovementComponent.cpp:165](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:165)

```cpp
// Essai : remplacer TeleportPhysics par None pour l'undo
Owner->SetActorLocationAndRotation(AuthoritativeLocation, AuthoritativeRotation, false, nullptr, ETeleportType::None);
```

Si EXTERNAL_WRITE disparaît → le problème était un side-effect du reset physics. Chercher le listener qui réagissait.

**Coût** : 1 ligne + retest.

---

## Matrice de priorité et dépendances

```
P0 (IMMÉDIAT, tout en parallèle) :
├── S1 — Fix InteriorFrame (8 lignes)       [résout F1, F2, F3]
├── S2 — Fix instrumentation (40 lignes)    [résout F8, F9, F10]
│    ├── S2a — BACKWARD projection forward
│    ├── S2b — EXTERNAL_WRITE rotation
│    └── S2c — CVar DisableCameraSway
└── S3 — Addendum findings doc              [résout F12]

P1 (APRÈS P0, phase suivante) :
├── S4 — Fix ApplyYawCompensation (15 lignes) [résout F4, dépend de S1]
└── S5 — Phase 12 exécution                  [résout G1, dépend de S1+S2]

P1 conditionnel :
└── S5b — Lowpass filter InteriorFrame       [si S1 insuffisant]

P2 (DETTE, avant feature suivante) :
├── S6 — Fix multi-step catch-up             [F5]
├── S7 — Fix contact transition              [F6]
├── S8 — Fix FPSCamera 2 writers             [F7]
└── S9 — Inspect BP Craniata                 [F13, G1 résiduel]

P3 (TRIVIAL) :
├── S10 — Renommer procedure phases          [F11]
└── S11 — Test undo TeleportType::None       [G2, si EXTERNAL_WRITE persiste]
```

---

## Coût total estimé

| Phase | Solutions | Lignes code | Docs | Temps dev | Temps PIE user |
|---|---|---|---|---|---|
| P0 | S1+S2+S3 | ~50 | 1 addendum | 1h | 0 |
| P1 | S4+S5 | ~15 | 0 | 30 min | 30 min |
| P2 | S6+S7+S8+S9 | ~50 | 0 | 2h | 30 min |
| P3 | S10+S11 | ~1 | proc update | 10 min | 10 min |
| **Total** | **11 fixes** | **~115 lignes** | **2 docs** | **~4h** | **~70 min** |

---

## Décision requise

Ordre que je propose :

1. **Exécuter P0 tout de suite** : S1 + S2 + S3. Pas d'arbitrage à faire, c'est propre.
2. **Puis P1 avant autre chose** : S4 + S5. Phase 12 avec instruments propres, on verra si G1 se ferme ou s'il faut creuser.
3. **Selon résultats S5** :
   - Si jitter perçu disparaît et EXTERNAL_WRITE clean → S9 optionnel, S11 skip, aller en P2 à loisir.
   - Si EXTERNAL_WRITE ou BACKWARD fire → S9 immédiat, puis S11 si besoin.

**Question à trancher** : veux-tu que j'attaque P0 maintenant (en un batch) ou que tu valides chaque solution individuellement avant implémentation ?
