# Sub Jitter - Full Architecture Audit - Contre-audit 03 - GPT54 - 2026-04-19

**Statut** : contre-audit statique de `reports/analysis/2026-04-18_jitter_full_architecture_audit_procedure.md` et `reports/analysis/2026-04-18_jitter_audit_findings.md`. Aucun test PIE execute. Aucun fix applique.

**Contexte autoritaire** : ce contre-audit reste dans le cadre du plan `reports/plans/2026-04-10_first_playable_strategic_analysis.md`. Aucun changement d'architecture n'est propose ici.

## Verdict court

- **Confirme** : `ASubCrewCharacter::Tick` est bien un amplificateur perceptuel majeur via `CameraSway` derivee de `LocalSubLinearAcceleration`.
- **Contredit ou non prouve** : le premier audit sur-affirme la preuve sur quatre points : `BACKWARD`, chaine camera FPS, ordre de tick global, exclusion de H1.
- **Position corrigee** : le symptome visuel camera/crew est confirme; la root cause amont sur la pose du sub n'est pas encore prouvee.

## Findings prioritaires

### F1 - Le log `SUB_TRACE BACKWARD` ne prouve pas un recul reel du sous-marin

**Preuve code** :
- `TickDelta` est calcule en espace monde a partir de `PostInterpLoc - LastPostTickLocation` dans `Source/Sub3D/Submarine/SubMovementComponent.cpp:277-294`.
- L'anomalie `BACKWARD` ne teste que `TickDelta.X < -BackwardThreshold` dans `Source/Sub3D/Submarine/SubMovementComponent.cpp:308-313`.

**Impact** :
- Le motif cite dans le premier audit (`+20.84 / -3.74`) n'est pas une preuve d'inversion de mouvement sur l'axe avant du sub.
- Un `X` monde negatif peut apparaitre alors que le sub avance encore dans son axe local si son cap n'est pas aligne sur `+X` monde.

**Decision** :
- Le contre-audit invalide l'usage de `BACKWARD` comme preuve forte contre H2.
- La phase empirique doit mesurer un delta projete sur `Owner->GetActorForwardVector()`, pas sur `World X`.

### F2 - La chaine camera du premier audit est factuellement fausse sur le path FPS

**Preuve code** :
- `FPSCamera->SetupAttachment(GetRootComponent())` dans `Source/Sub3D/Submarine/SubCrewCharacter.cpp:97`.
- `TPSCameraBoom->SetupAttachment(GetRootComponent())` dans `Source/Sub3D/Submarine/SubCrewCharacter.cpp:102`.
- `TPSCamera->SetupAttachment(TPSCameraBoom, USpringArmComponent::SocketName)` dans `Source/Sub3D/Submarine/SubCrewCharacter.cpp:111`.

**Impact** :
- Le mesh skeletal n'est pas dans la chaine de transform de la camera FPS.
- Le `SpringArm` n'est pas dans la chaine FPS non plus.

**Decision** :
- Le soupcon "mesh/skeletal/spring arm porte le jitter FPS" est nettement affaibli.
- La chaine FPS correcte est `CapsuleRoot -> FPSCamera`.

### F3 - L'affirmation "tous les readers tickent apres SubMovement" n'est prouvee que partiellement

**Preuve code** :
- `USubInteriorFrameComponent` declare un prerequis explicite vers `SubMovement` dans `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:41`.
- `USubCrewMovementComponent` declare des prerequis vers `SubMovement` et `InteriorFrame` dans `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:612-618`.
- Le repo ne montre pas de prerequis explicite similaires pour `ASubCrewCharacter::Tick`, `USubmarineSystemsComponent`, `USubmarineRadarComponent` ou `UHelmNavigationDisplayComponent`.

**Impact** :
- Le chemin critique `SubMovement -> InteriorFrame -> CrewMovement` est bien prouve.
- Le graphe global "tous les readers apres SubMovement" reste une inference, pas une preuve exhaustive.

**Decision** :
- Il faut restreindre la conclusion a la pipeline critique confirmee.
- Le premier audit sur-vend la certitude de la reconstruction de tick order.

### F4 - H1 n'est pas "ecartee" par la preuve instrumentee actuelle

**Preuve code** :
- `EXTERNAL_WRITE` ne compare que `ExtDrift.Size()` sur la **position** dans `Source/Sub3D/Submarine/SubMovementComponent.cpp:296-304`.
- `LastPostTickRotation` est stocke dans `Source/Sub3D/Submarine/SubMovementComponent.cpp:319` mais jamais compare.
- Il existe au moins un writer standalone hors `SubMovement` au demarrage : `SubActor->SetActorTransform(SpawnTransformA)` dans `Source/Sub3D/Submarine/TraversalLevelManager.cpp:34`.
- La procedure demandait aussi une inspection des composants ajoutes en BP, mais le findings doc ne montre pas cette verification.

**Impact** :
- Une absence de `EXTERNAL_WRITE` n'exclut pas un writer rotation-only.
- H1 n'est pas totalement couverte tant que le path Blueprint n'est pas explicitement audite ou marque non verifie.

**Decision** :
- La formulation correcte est : "aucun writer externe trouve sur le path C++ runtime revele pendant la revue statique", pas "H1 ecartee".

### F5 - L'amplificateur perceptuel camera sway est bien confirme

**Preuve code** :
- `ASubCrewCharacter::Tick` applique `CameraSway` depuis `LocalSubLinearAcceleration` et `LocalSubAngularVelocityDegrees` dans `Source/Sub3D/Submarine/SubCrewCharacter.cpp:235-240`.
- `USubInteriorFrameComponent::TickComponent` derive `LocalLinearVelocity`, puis `LocalLinearAcceleration` a partir de la pose actor dans `Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:63-74`.

**Impact** :
- La these principale du premier audit sur l'amplificateur perceptuel reste correcte.
- Cela confirme une cause du **ressenti** du jitter en FPS, meme si cela ne prouve pas encore la cause amont sur la pose du sub.

**Decision** :
- Ce point reste le resultat le plus solide du premier audit.

### F6 - L'inventaire des writers de la camera est incomplet

**Preuve code** :
- `USubCrewMovementComponent::TickPosture` ecrit aussi `FPSCamera->SetRelativeLocation(...)` dans `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:710-747`.
- `ASubCrewCharacter::Tick` reecrit ensuite la position relative complete de la meme camera dans `Source/Sub3D/Submarine/SubCrewCharacter.cpp:226-240`.

**Impact** :
- Deux writers touchent `FPSCamera` dans la meme frame.
- Le second writer semble gagner, donc ce n'est probablement pas la root cause, mais l'inventaire architectural du premier audit est incomplet sur ce point.

### F7 - Le path animation consomme aussi l'acceleration bruitee

**Preuve code** :
- `USubCrewAnimInstance::ReadInputState` copie `LocalSubLinearAcceleration` dans `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:149-150`.
- `USubCrewAnimInstance::ComputeSubMotion` l'utilise pour le stumble dans `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:349-366`.

**Impact** :
- Il existe un second amplificateur perceptuel cote animation.
- Il est toutefois reduit par `Instability = 1.f - SupportQuality`, donc priorite inferieure au camera sway pour le symptome FPS rapporte.

## Review de la procedure

**Points solides** :
- Le scope est bien borne autour des writers/readers de transform, des compensations, de la camera, et de la matrice empirique.
- La separation H1 / H2 / H3 est exploitable.
- La regle "pas de fix pendant l'audit" est correcte.

**Lacunes a corriger** :
- La procedure doit valider la semantique des instruments de debug avant d'utiliser leurs logs comme preuve.
- Phase 12 doit mesurer le mouvement "forward" dans le repere du sub, pas via `World X`.
- H1 doit inclure une verification de drift rotation, pas seulement translation.
- L'inspection Blueprint demandee en Phase 1 doit etre explicitement marquee "faite" ou "non faite".
- Il manque un test empirique `camera sway OFF`, distinct de `visual interp OFF`.
- Coherence documentaire : le document de procedure annonce "12 phases" mais enumere `Phase 0` a `Phase 13`.

## Synthese corrigee

**Confirme** :
- La pipeline critique `SubMovement -> InteriorFrame -> CrewMovement -> FPSCamera` existe bien.
- `CameraSway` est un amplificateur perceptuel reel et significatif.

**Probable** :
- Une petite irregularite sur la pose presentee du sub peut etre agrandie par la derivation finie dans `InteriorFrame`, puis par la camera.

**Non prouve a ce stade** :
- Un vrai mouvement arriere du sous-marin sur son axe avant.
- Un writer externe entre ticks sur tout le path standalone.
- Une chaine FPS via mesh/spring arm.

**Classement revise des hypotheses** :
- **H2** reste viable. Le premier audit ne l'a pas invalidee.
- **H1** est affaiblie sur le path C++ relu, mais pas exclue.
- **H3** reste ouverte, surtout pour les edge cases d'interp/rotation, mais pas sur la base du seul `TickDelta.X`.

## Actions requises avant une conclusion de root cause

1. Remplacer le critere `BACKWARD` par une projection sur `Owner->GetActorForwardVector()`.
2. Ajouter un check rotation pour `EXTERNAL_WRITE`.
3. Executer un test `camera sway OFF` avant d'interpreter la perception crew/camera comme preuve de jitter actor.
4. Documenter explicitement si `BP_Submarine_Craniata` et `BP_SubmarineCrew` ajoutent du tick ou des writes de transform.

## Position finale

Le premier audit a correctement identifie le meilleur contributeur confirme au jitter percu : le `CameraSway` derive numeriquement de la pose visuelle du sub. En revanche, il surestime la force de preuve sur la motion "backward", la chaine camera FPS, l'ordre de tick global, et l'exclusion de H1. La bonne lecture du dossier aujourd'hui est : **amplificateur perceptuel confirme, root cause amont encore non prouvee**.
