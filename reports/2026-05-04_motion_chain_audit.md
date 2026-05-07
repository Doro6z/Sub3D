
# Audit Motion Chain Sub3D - 2026-05-04

Document produit pour un planner externe sans acces au code.

Regles de lecture:

- Workspace audite: `C:\Dev\Sub3D`.
- Audit statique uniquement: pas de PIE, pas de profiling runtime, pas d'UnrealClaude MCP.
- Le document authority-max reste `reports/plans/2026-04-10_first_playable_strategic_analysis.md`.
- Le code courant prime sur les memories. Les memories ont ete lues puis comparees au code.
- Le scope flood est limite au couplage motion: tick, `SetFloodImpactKg`, `GetTotalWaterMassKg`, et breaches uniquement quand elles modifient ensuite la masse.
- Le stock `UCharacterMovementComponent` n'est pas audite en interne; seuls les overrides et points Sub3D sont couverts.

Sources inspectees:

| Fichier | Role |
|---|---|
| `Source/Sub3D/Submarine/SubMovementComponent.h/.cpp` | physique math-based, fixed tick, interpolation root, client playback |
| `Source/Sub3D/Submarine/SubmarineBase.h/.cpp` | acteur replique, `RepState`, collision hull, damage bridge |
| `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h` | `FSubmarineCommandState`, `FSubmarineNetState` |
| `Source/Sub3D/Submarine/SubmarineSystemsComponent.h/.cpp` | helm commands, stabilization, pump bridge |
| `Source/Sub3D/Submarine/SubCrewMovementComponent.h/.cpp` | LGA, rebase, saved moves, SimProxy guards |
| `Source/Sub3D/Submarine/SubCrewCharacter.h/.cpp` | `CurrentSubmarine`, enter/handoff/bootstrap, camera sway |
| `Source/Sub3D/Submarine/SubCrewNetTypes.h/.cpp` | custom move payload grid state/handoff |
| `Source/Sub3D/Submarine/SubFloodComponent.h/.cpp` | flood mass push |
| `Source/Sub3D/Submarine/SubPlayerController.h/.cpp` | server RPC routes from helm |
| `Source/Sub3D/Debug/Sub3DDebugSettings.h/.cpp` | debug toggles |
| `Config/DefaultEngine.ini`, `Config/DefaultGame.ini` | fixed frame, collision, debug config |
| `Source/Sub3DCore/`, `Source/Sub3DRuntime/` | core/runtime data; no active motion integrator found |

Memories lues:

| Memory | Statut court |
|---|---|
| `project_architecture_revision_2026_03_24.md` | historique, stale sur 30 Hz/InteriorFrame |
| `project_stabilization_guards_2026_03_25.md` | partiel, utile pour comparer les 5 guards |
| `project_helm_cockpit_redesign_2026_04_18.md` | globalement coherent avec le route helm actuel |
| `project_physics_revision_2026_04_17.md` | coherent pour 60 Hz/profile physique, playback evolue |
| `project_fluidity_roadmap_2026_04_18.md` | partiellement supersede par buffered playback |
| `project_interior_collision_invariant_2026_04_18.md` | invariant important, details collision differents |
| `project_crew_embarked_failure_2026_04_21.md` | stale, remplace par LGA |
| `project_embarked_refactor_rolled_back_2026_04_20.md` | stale, historique rollback |
| `project_motion_chain_jitter_root_cause_2026_04_27.md` | a jour |
| `project_gameplay_vision_2026_04_01.md` | vision 1-16 joueurs, non prouvee runtime |

---

## 1. Vue d'ensemble - resume executif

Architecture globale observee:

- `ASubmarineBase` est l'acteur racine replique. Il desactive la replication UE native du mouvement avec `SetReplicateMovement(false)` et publie un snapshot custom `FSubmarineNetState`.
- `USubMovementComponent` simule le sub sur serveur avec une physique maison, en fixed-step 60 Hz, sans Chaos rigid body pour le mouvement principal.
- Cote autorite, `USubMovementComponent` applique la pose simulee sur le root actor et peut appliquer une interpolation visuelle entre fixed steps.
- Cote client non-autorite, `USubMovementComponent` ne simule pas; il consomme un buffer de snapshots et joue une pose retardee.
- Le crew embarque est gere par `USubCrewMovementComponent` via Local Grid Authority: REBASE depuis `GridSpaceTransform`, CMC stock, EXTRACT en sub-local.
- La chaine explicite actuelle est `SubFlood -> SubMovement -> CrewMovement`. Aucun `USubInteriorFrameComponent` ne reste dans `Source/`.
- Les commandes helm passent par `SubHelmWidget -> ASubPlayerController ServerRoute* -> USubmarineSystemsComponent -> USubMovementComponent`.
- Flood pousse une masse totale au movement via `SetFloodImpactKg(GetTotalWaterMassKg())`; movement ne tire pas directement l'etat flood.
- Hull damage influence movement seulement indirectement: impact/damage -> breach -> flood inflow -> masse flood -> masse totale.
- Aucune API actuelle ne permet une impulse externe au sub a un point. Hull breaking/recoil futurs n'ont pas de point d'injection dedie.

Niveau de maturite:

| Zone | Maturite | Preuve / commentaire |
|---|---|---|
| Physique sub serveur | a consolider | coherent et localise, mais substeps non bornes et API force future absente |
| Snapshot sub client | a consolider | buffer + `CubicInterp`; pas d'extrapolation avancee |
| Crew LGA | fragile mais structure | guards en place, trust model FP co-op |
| Flood -> movement | FP utilisable | push mass clair, pas de distribution/CG |
| Multiplayer | partiel | snapshots/RPC/custom moves existent; pas de validation runtime ici |
| Extension hull breaking | absent cote movement | pas d'impulse/mass modifier runtime generique |
| Water sloshing post-MVP | partiel | acceleration/rates lisibles, presented-state non unifie |

Etat multiplayer par lecture statique:

- Sub: server-authoritative, clients lisent `RepState`.
- Helm: inputs principaux RPC serveur, axes en `Server Unreliable`, toggles/targets en `Server Reliable`.
- Crew: owner client envoie `GridSpaceTransform` et `EmbarkState` dans custom saved moves; serveur les accepte en FP co-op.
- SimProxy crew: `SmoothCorrection` no-op en grid authority; cela evite un type de jitter mais retire une correction UE stock.
- Non verifie: PIE local, PIE multi-client, packet loss, 16 joueurs.

---

## 2. Stack motion chain - diagramme tick complet

Preuves de tick prerequisites:

| Relation | Localisation | Code cle |
|---|---|---|
| `SubFlood -> SubMovement` | `Source/Sub3D/Submarine/SubMovementComponent.cpp:99-126` | `AddTickPrerequisiteComponent(Sub->SubFlood)` |
| `SubMovement -> CrewMovement` | `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:1580-1610` | `AddTickPrerequisiteComponent(SubMov)` |

Pas de `AddTickPrerequisiteActor` trouve dans le scope.
Pas de prerequisite trouve entre `USubmarineSystemsComponent` et `USubMovementComponent`.
Pas de prerequisite trouve entre `USubHullBoundaryComponent` et `USubCrewMovementComponent`.

Diagramme server tick garanti par prereqs explicites:

```text
[Server frame T]

  ASubmarineBase::Tick
    - Super::Tick uniquement, pas de motion.

  USubmarineSystemsComponent::TickComponent
    - server-only par guard HasAuthority().
    - UpdateStabilization(DeltaTime).
    - PushPumpStateToHull().
    - pas de prereq explicite avec SubMovement.

  USubFloodComponent::TickComponent
    - server-only par guard HasAuthority().
    - AdvanceFlooding(DeltaTime).
    - SetFloodImpactKg(GetTotalWaterMassKg()).

  USubMovementComponent::TickComponent
    - branche serveur: fixed-step while loop.
    - SimulateStep(1 / FixedSimulationHz).
    - RefreshRepState() si au moins un substep.
    - interpolation visuelle root si activee.

  USubCrewMovementComponent::TickComponent
    - apres SubMovement si InitializeForSubmarine a ete appele.
    - REBASE / SIMULATE / EXTRACT.

  UE replication
    - envoie RepState selon NetUpdateFrequency et conditions reseau.
```

Diagramme client non-authority:

```text
[Client frame T]

  ASubmarineBase::OnRep_RepState
    -> SubMovement->HandleReplicatedNetState(RepState)
    -> QueueClientSnapshot / ResetClientPlayback

  USubMovementComponent::TickComponent
    -> EvaluateClientPlaybackPose(WorldNow)
    -> ApplyClientPlaybackPose(Location, Rotation)
    -> pas de simulation sub client

  USubCrewMovementComponent::TickComponent
    -> owner client: CMC + custom moves
    -> SimProxy: rebase depuis GridSpaceTransform replique
```

Classification:

| Tick / fonction | Server-only | Client-only | Both | Autorite/prediction |
|---|---:|---:|---:|---|
| `ASubmarineBase::Tick` | non | non | oui | pas de motion |
| `USubmarineSystemsComponent::TickComponent` | oui par guard | non | composant both | serveur autoritatif |
| `USubFloodComponent::TickComponent` | oui par guard | non | composant both | serveur autoritatif |
| `USubMovementComponent::TickComponent` | sim serveur | playback client | oui | sub non predit cote client |
| `USubCrewMovementComponent::TickComponent` | oui | oui | oui | owner client simule, serveur accepte state FP |
| `USubHullBoundaryComponent::TickComponent` | non determine statiquement | non determine statiquement | probable both | handoff aussi transporte par saved move |

Risque observe: `Systems` peut mettre a jour les ramps/autopilot apres `SubMovement` dans une frame, car aucun prereq ne force l'ordre.

---

## 3. Math-based physics (`USubMovementComponent`)

Etat interne principal:

| Etat | Type | Exposition | Role |
|---|---|---|---|
| `Velocity` | `FVector` | `BlueprintReadOnly` | vitesse monde cm/s |
| `LinearAcceleration` | `FVector` | `BlueprintReadOnly` | acceleration monde cm/s2 |
| `AngularAccelerationDeg` | `FVector` | `BlueprintReadOnly` | accel angulaire deg/s2, X roll unused |
| `YawRateDegPerSec` | float prive + getter | C++/BP getter | yaw rate |
| `PitchRateDegPerSec` | float prive + getter | C++/BP getter | pitch rate |
| `CurrentDepth` | float | `BlueprintReadOnly` | profondeur positive bas, metres |
| `FloodedMassKg` | float | `BlueprintReadOnly` | masse flood consommee |
| `FloodImpactKg` | float | `BlueprintReadOnly`, transient | push flood brut |
| `ForwardSpeedCmS` | float | `BlueprintReadOnly` | vitesse locale X |
| `EffectivePowerInput` | float | `BlueprintReadOnly` | throttle apres spool |
| `GlobalTargetFill` | float replique | public | fill ballast global |
| `Ballasts` | TArray replique | public | ballasts |
| `SimAccumulator` | float prive | interne | fixed-step accumulator |
| `SimFrameCounter` | int32 prive + getter | snapshot | frame sim authoritaire |

Parametres par defaut importants:

| Parametre | Valeur | Role |
|---|---:|---|
| `BaseMass` | 200000 kg | masse coque/equipement |
| `WaterDensity` | 1025 kg/m3 | eau mer |
| `SubmergedVolume` | 205.12 m3 | volume deplacement |
| `NeutralBuoyancyFill01` | 0.5 | fill ballast de reference |
| `BallastEffectScale` | 0.05 | contribution ballast effective |
| `FloodedMassInfluence` | 1.0 | multiplicateur flood mass |
| `MaxThrust` | 25000 N | thrust max |
| `MaxForwardSpeed` | 650 cm/s | clamp avant |
| `MaxReverseSpeed` | 250 cm/s | clamp arriere |
| `MaxVerticalSpeed` | 180 cm/s | clamp vertical |
| `FixedSimulationHz` | 60 Hz | fixed-step |

Performance profile:

- `ASubmarineBase::BeginPlay` applique `SubMovement->ApplyPerformanceProfileFromDefinition(GeneratedDefinition)` si definition generee.
- Les valeurs positives de `USubmarineDefinition` ecrasent `BaseMass`, `MaxForwardSpeed`, `MaxReverseSpeed`, `MaxVerticalSpeed`, `MaxThrust`.
- Si la masse change et neutral buoyancy auto est activee, `InitializeNeutralBuoyancy()` est relancee.

Integration:

```text
FixedSimDt = 1 / FixedSimulationHz
SimAccumulator += DeltaTime
while SimAccumulator >= FixedSimDt:
    SimulateStep(FixedSimDt)
    SimAccumulator -= FixedSimDt

Acceleration = forces / total mass
Velocity += Acceleration * DeltaTime
DeltaLocation = Velocity * DeltaTime
MoveWithSlide(DeltaLocation)
```

Forces / termes:

| Terme | Forme observee | Remarque |
|---|---|---|
| Thrust | `ForwardDir * (SpooledPower * MaxThrust * EngineHealth / TotalMass) * 100` | m/s2 vers cm/s2 |
| Buoyancy / gravity | `(BuoyancyN - GravityN) / TotalMass * 100` | axe Z |
| Drag | `-0.5 * rho * Cd * area * v * abs(v)` par axe local | world accel ensuite |
| Rudder | target yaw rate + response/damping | pas de force au point |
| Pitch/dive | dive plane + speed factor + ballast trim + BG moment | clamp pitch |
| Vertical from pitch | `LocalVelCm.X * sin(Pitch) * VerticalFromPitchFactor` | ajoute a local Z |
| Flood mass | masse totale, pas force directe | pas de CG par compartiment |
| External impulse | aucune API | absent |

`ComputeTotalMass()` additionne `BaseMass + EffectiveWaterMass + FloodedMassKg * FloodedMassInfluence`.

Dependance fixed frame:

- Le code dit que UE coalesce plusieurs `RefreshRepState` intra-frame.
- Si une frame batch 1-3 substeps, le client voit un seul snapshot avec distance variable.
- La mitigation actuelle est `bUseFixedFrameRate=True` et `FixedFrameRate=60.0`.

Fragilites:

- Pas de cap explicite du nombre de substeps par frame.
- `RefreshRepState()` se fait une fois apres la boucle fixed-step, pas par substep.
- Flood mass est scalaire; pas de centre de masse flood par compartiment.
- Collision movement est manuel par sweeps/slide.
- Les allocations dans `MoveWithSlide` sont visibles dans fixed-step.

---

## 4. Points d'injection externes

| Source | Methode appelee | Donnee | Quand | Autorite | Statut |
|---|---|---|---|---|---|
| `USubFloodComponent` | `SetFloodImpactKg(GetTotalWaterMassKg())` | masse kg | chaque tick flood serveur | serveur | existe |
| Helm UI | RPC `ServerRouteHelm*` | axes throttle/yaw/trim | input | serveur | existe |
| `USubmarineSystemsComponent` | `SetHelmThrottleCommand/Yaw/Trim` | `FSubmarineCommandState` + setters movement | RPC ou auto | serveur | existe |
| Stabilization | `SetPowerInput/Rudder/DivePlane` | ramps/autopilot | systems tick | serveur | existe, ordre tick non garanti |
| Ballast | `SetGlobalBallastTarget`, `SetBallastTargetByIndex` | fill | RPC/systems | serveur | existe |
| Pump | `SubFlood->SetPumpActive` | pump active/rate | systems | serveur | indirect via flood mass |
| Hull collision | `SubHull->ApplyHullImpact`, possible breach | damage/inflow | `OnHullHit` | serveur | indirect |
| `TakeDamage` | `ApplyHullImpact`, possible breach | damage/inflow | gameplay | serveur | indirect |
| Crew crossing | `CrewMov->Velocity +=/-= SubMovement->Velocity` | crew momentum | boundary | both possible | crew only |
| Sonar/camera/anim | reads `Velocity`/accel | read only | update/tick | mixed | partiel |
| Hull breaking futur | aucune API | force/impulse | futur | n/a | absent |
| Weapons recoil futur | aucune API | impulse/torque | futur | n/a | absent |
| Water sloshing futur | read acceleration/rates | read only | futur | n/a | partiel |

Force injection diagram:

```text
HelmWidget
  -> PlayerController ServerRoute* RPC
    -> Systems CommandState
      -> Movement inputs
      -> SimulateStep pulls GetCommandState()

Flood Tick server
  -> AdvanceFlooding
  -> GetTotalWaterMassKg
  -> SetFloodImpactKg
  -> ComputeTotalMass

Hull impact / damage
  -> Hull damage
  -> optional Flood.CreateBreach
  -> later flood mass push

Future hull breaking/recoil
  -> no current Movement API
```

API gaps futurs:

| Besoin | API actuelle | Statut |
|---|---|---|
| impulse lineaire | aucune methode publique trouvee | absent |
| impulse a un point / torque | aucune methode publique trouvee | absent |
| masse externe autre que flood | seulement flood scalar + ballasts | absent |
| distribution flood par compartiment dans CG | non observe | absent |
| acceleration monde | `LinearAcceleration` public | present |
| angular velocity monde | getters pitch/yaw + snapshot vector | partiel |
| world rotation | actor transform | present hors API motion |
| presented motion state | commentaire Phase B, pas d'accessor trouve | absent/partiel |
| event impact movement | pas de delegate movement | absent |

---

## 5. Replication

Ce qui est replique:

| Classe | Props motion/network |
|---|---|
| `ASubmarineBase` | `CurrentPilot`, `RepState`, `ExteriorTurret` |
| `USubMovementComponent` | `Ballasts`, `GlobalTargetFill` |
| `USubmarineSystemsComponent` | `CommandState`, `EngineHealth01`, `ElectricalHealth01` |
| `USubCrewMovementComponent` | `PostureAlpha`, `bIsRunning`, `GridSpaceTransform COND_SkipOwner`, `EmbarkState COND_SkipOwner`, ladder |
| `ASubCrewCharacter` | `CurrentSubmarine`, `bIsAtHelm`, `Health`, `CurrentCompartmentId COND_SkipOwner` |

Snapshot flow:

```text
Server SubMovement fixed steps
  -> ASubmarineBase::RefreshRepState()
  -> RepState replicated
Client ASubmarineBase::OnRep_RepState()
  -> SubMovement->HandleReplicatedNetState()
  -> QueueClientSnapshot / ResetClientPlayback
Client SubMovement Tick
  -> EvaluateClientPlaybackPose()
  -> CubicInterp location, Lerp rotation
  -> ApplyClientPlaybackPose()
```

Frequency:

- `SetNetUpdateFrequency(60.f)`.
- `SetMinNetUpdateFrequency(30.f)`.
- `SetReplicateMovement(false)`.
- `RefreshRepState()` est appele une fois apres la boucle de substeps si un substep a tourne.

Interpolation:

- Position: `FMath::CubicInterp` avec tangents `Velocity * SegmentSeconds`.
- Rotation: `FMath::Lerp` de rotators.
- Delay: `ClientPlaybackDelaySeconds = 0.10`.
- Buffer history: `ClientMaxBufferHistorySeconds = 0.50`.
- Stall reset: `ClientStallResetSeconds = 0.20`.
- Extrapolation avancee: non trouvee. Si render time sort du buffer, la fonction rend le premier ou dernier snapshot.

RPC motion:

| RPC | Reliability | Route |
|---|---|---|
| `ServerRouteHelmThrust` | Server Unreliable | Systems ou fallback Movement |
| `ServerRouteHelmThrottleRamp` | Server Unreliable | Systems |
| `ServerRouteHelmSteer` | Server Unreliable | Systems ou fallback Movement |
| `ServerRouteHelmRudderRamp` | Server Unreliable | Systems |
| `ServerRouteHelmDive` | Server Unreliable | Systems ou fallback Movement |
| `ServerRouteHelmDivePlaneRamp` | Server Unreliable | Systems |
| ballast/hold/auto/pump | Server Reliable | Systems/Flood |

SimProxy crew:

- `OnRep_GridSpaceTransform` met a jour yaw local et logs.
- `SmoothCorrection` no-op si `IsGridAuthoritative()`.
- `UpdateBasedMovement` et `UpdateBasedRotation` no-op si grid authority.
- `ServerCheckClientError` retourne `false` en grid authority.
- `MoveAutonomous` copie le grid state client vers le serveur.

---

## 6. Crew embarque - Local Grid Authority

Pseudo-code:

```text
TickComponent:
  if just gained sub and not grid-authoritative:
      seed GridSpaceTransform from actor pose relative to sub
      SetEmbarkState(Embarked)

  if lost sub while grid-authoritative:
      SetEmbarkState(Outside)
      GridSpaceTransform = Identity

  bIgnoreBaseRotation = IsGridAuthoritative()
  CharacterOwner->bUseControllerRotationYaw = !IsGridAuthoritative()

  if grid-authoritative:
      REBASE:
        world pos = SubTransform.TransformPosition(GridSpaceTransform.Location)
        world yaw = SubYaw + GridFacingYawDeg
        UpdatedComponent->SetWorldLocationAndRotation(world, sweep=false, TeleportPhysics)

  SIMULATE:
      Super::TickComponent()

  if grid-authoritative:
      EXTRACT:
        GridSpaceTransform.Location = SubTransform.InverseTransformPosition(CharacterWorld)
        GridSpaceTransform.Rotation = local yaw cache

  update relative frame, inertial state, support, brace, IK, logs
```

`GridSpaceTransform`:

- Type: `FTransform`.
- `ReplicatedUsing=OnRep_GridSpaceTransform`.
- `COND_SkipOwner`.
- Owner client conserve son etat via saved moves; SimProxy recoit OnRep.

`ECrewEmbarkState`:

- `Outside`.
- `Embarked`.
- `Transitioning`.
- `Transitioning` est reserve post-FP; FP fait des flips instantanes.

Invariants verifies:

| Invariant | Etat | Localisation |
|---|---|---|
| Rebase utilise `UpdatedComponent->SetWorldLocationAndRotation` | intact | `SubCrewMovementComponent.cpp:243` |
| `bSweep=false` + `TeleportPhysics` | intact | `SubCrewMovementComponent.cpp:243-246` |
| `bIgnoreBaseRotation = IsGridAuthoritative()` | intact | `SubCrewMovementComponent.cpp:196` |
| `UpdateBasedMovement` no-op en grid authority | intact | `SubCrewMovementComponent.cpp:533-540` |
| `UpdateBasedRotation` no-op en grid authority | intact | `SubCrewMovementComponent.cpp:544-551` |
| `SmoothCorrection` no-op en grid authority | intact | `SubCrewMovementComponent.cpp:554-566` |

Nuance: `EnterOnFootInSubmarine` peut utiliser `SetActorTransform` et un floor snap pendant le bootstrap. Ce n'est pas la boucle REBASE par tick.

`USubInteriorFrameComponent`:

- Aucun fichier `Source/Sub3D/Submarine/SubInteriorFrameComponent.h/.cpp`.
- Aucun symbole source `USubInteriorFrameComponent` dans `Source/`.
- Un commentaire dans `Source/Sub3D/Diagnostics/SubRelativeFrameExpectedTransformProvider.h:12` dit explicitement "no InteriorFrame middleman".
- Les references restantes sont dans `CLAUDE.md`, plans ou archives.

Conclusion: pas de residue code compile d'InteriorFrame dans la motion chain actuelle.

---

## 7. Les 5 stabilization guards

Source historique: `project_stabilization_guards_2026_03_25.md`.

| Guard historique | Etat actuel | Localisation / commentaire |
|---|---|---|
| Guard 1: `CurrentSubmarine` RepNotify relance init crew | intact | `SubCrewCharacter.h:87`, `SubCrewCharacter.cpp:475-485` |
| Guard 2: `InitializeForSubmarine` rebind/reinit contexte sub | intact mais etendu | `SubCrewMovementComponent.cpp:1580-1610` |
| Guard 3: tick order crew apres sub movement et InteriorFrame | modifie | equivalent actuel: `SubFlood -> SubMovement -> CrewMovement`, no InteriorFrame |
| Guard 4: interpolation commence depuis transform presente courant | remplace | ancien two-snapshot lerp non trouve; code actuel buffer authority-time + Hermite |
| Guard 5: stock CMC seul proprietaire du mouvement crew | modifie | CMC simule, mais LGA possede transport frame; based movement/smoothing no-op |

Bilan:

- 2 guards sont intactes comme ecrites.
- 1 guard est intacte sous forme actualisee.
- 2 guards sont remplacees par une architecture plus recente.
- La memory est partiellement stale et ne doit pas etre executee litteralement.

---

## 8. Project Settings dependances critiques

`Config/DefaultEngine.ini`:

| Setting | Valeur | Pourquoi | Validation |
|---|---:|---|---|
| `bUseFixedFrameRate` | `True` | evite batching variable de substeps par snapshot | warning runtime si false |
| `FixedFrameRate` | `60.000000` | match `FixedSimulationHz=60.f` | pas de warn specifique valeur != 60 trouve |
| `Submarine` channel | `ECC_GameTraceChannel1` | hull/movement collision | profiles |
| `SubInterior` channel | `ECC_GameTraceChannel2` | walkable interieur | profiles |
| `CompartmentProbe` channel | `ECC_GameTraceChannel3` | breach boundary overlap crew | profiles |
| `SubmarineHull` profile | QueryAndPhysics | hull collision | utilise par collision setup |
| `SubInteriorWalkable` profile | QueryOnly | crew floor/walkable | CMC |
| `CompartmentProbe` profile | QueryOnly, Pawn overlap | hull boundaries | `USubHullBoundaryComponent` |

`Config/DefaultGame.ini`:

- `bLogCrewJitter=False`.
- `bDrawCrewGridAuthority=True`.
- `bLogHullVisualBreaches=True`.
- `bLogFlood=True`.
- `bDrawCompartmentVolumes=True`.
- `bDrawCompartmentWater=True`.
- Incoherence: `CrewJitterWarnThresholdCm=25.000000` existe en config, mais le code expose `CrewJitterWarnVelocityCmPerSec`.

`Source/Sub3D/Sub3D.Build.cs` dependances motion utiles:

- `NetCore`.
- `PhysicsCore`.
- `DeveloperSettings`.
- `ProceduralMeshComponent`.
- `RuntimeSyncDiagnostics`.
- `Sub3DRuntime`.
- `Sub3DCore`.

---

## 9. Couplage avec autres systemes

| Couplage | Direction | Type | Fragilite |
|---|---|---|---|
| Flood -> Movement | push | tick prereq + setter | masse scalaire seulement |
| Movement -> Crew | read/rebase | tick prereq + transform/velocity reads | LGA trust model |
| Helm UI -> Movement | push via RPC/systems | widget -> controller -> systems -> movement | fallback Movement si Systems absent |
| Systems -> Movement | push/pull | setters + Movement pulls CommandState | tick order non garanti |
| Movement -> Snapshot | output | `RefreshRepState` | depend fixed frame |
| Snapshot -> Client Movement | OnRep/buffer | `HandleReplicatedNetState` | pas extrapolation avancee |
| Movement -> Sonar system | read | `SubMovement->Velocity.Size()` | OK dans `SubSonarSystemComponent` |
| Movement -> old noise/nav consumers | read indirect | `OwnerActor->GetVelocity()` | suspect avec movement custom |
| Movement -> Camera | via CrewMovement inertia | local player camera sway | pas de presented state unifie |
| Movement -> Animation | via CrewMovement inertia | anim instance | depend LGA |
| Hull impact -> Flood -> Movement | indirect | breach mass | pas impulse immediate |
| Crew -> Movement | none | crew lit sub velocity | pas de feedback force |
| Weapons/recoil -> Movement | none | absent | API absente |

Consumers `OwnerActor->GetVelocity()` trouves:

- `HelmNavigationDisplayComponent.cpp:118`.
- `SonarNoiseEmitterComponent.cpp:18`.
- `TunnelNavigationRuntimeComponent.cpp:804`, `1084`, `1447`, `1459`, `1471`.

Aucun override `ASubmarineBase::GetVelocity()` n'a ete trouve. A valider runtime.

---

## 10. Points d'extension pour systemes futurs

Hull breaking:

| Besoin | API actuelle | Statut |
|---|---|---|
| force impulse lineaire | aucune methode publique | absent |
| impulse a un point / torque | aucune methode publique | absent |
| masse structurelle runtime | `BaseMass` edit property, pas de delta API | absent |
| masse breach/flood | `SetFloodImpactKg` indirect | partiel |
| impact event | hull feedback, pas movement delegate | partiel hors movement |
| speed damage | `OnHullHit` lit `SubMovement->Velocity` | present |

Water sloshing post-MVP:

| Donnee | Accessible | Statut |
|---|---|---|
| `Velocity` monde | public BPReadOnly | present |
| `LinearAcceleration` monde | public BPReadOnly | present |
| angular velocity | getters pitch/yaw, snapshot vector | partiel |
| angular acceleration | `AngularAccelerationDeg` | present |
| world rotation | actor transform | present hors API motion |
| presented motion state | commentaire Phase B, pas d'accessor | absent/partiel |
| local inertial frame | `CrewMovement` local fields | present mais crew-specific |

Camera shake on impact:

- Existe via hull feedback director.
- Pas de delegate movement generique.

Damage system:

- Collision hull peut lire vitesse sub.
- Pas d'energie/impulse unifiee exposee par movement.

Weapons recoil:

- Aucun path trouve.

---

## 11. Dette / fragilite / TODO

TODO/dette dans scope:

| Localisation | Sujet | Impact |
|---|---|---|
| `SubCrewMovementComponent.h:25` | `Transitioning` reserve post-FP | handoff instant en FP |
| `SubCrewMovementComponent.h:342` | server trusts client grid pose | blocker MP production |
| `SubCrewMovementComponent.cpp:582` | `ServerCheckClientError` trust FP | correction client desactivee |
| `SubCrewMovementComponent.cpp:839` | `MoveAutonomous` trust model | meme dette |
| `SubFloodComponent.h:138` | doors/edges deferred post-FP | flood propagation simplifiee |
| `SubFloodComponent.cpp:367` | real breach geometry post-FP | boundary approximative |
| `SubCrewCharacter.cpp:302` | water immersion local-Z post-FP | crew/water |
| `SubCrewCharacter.cpp:751` | TODO death/ragdoll/respawn | crew gameplay |
| `SubMovementComponent.h:257` | legacy alias `SetThrustInput` | compatibility path |
| `SubmarineBase.cpp:316`, `SubFloodComponent.cpp:180`, `SubCrewCharacter.cpp:822` | LayoutAsset legacy fallbacks | stale authoring paths |
| `SubCrewCharacter.h:102/106` | deprecated board/disembark hard paths | legacy attach/detach |

Fragilites ajoutees par audit:

- Pas de `MaxSimulationStepsPerFrame`.
- `Systems` tick order non garanti avant `SubMovement`.
- Snapshot refresh une fois par frame, pas par substep.
- Rotation client interp via rotator Lerp, pas Slerp.
- Client buffer remove en tete, cout lineaire faible mais present.
- Allocations `TArray` dans sweep fixed-step.
- Consumers `OwnerActor->GetVelocity()` suspects.
- `DefaultGame.ini` contient un vieux nom de setting.
- `ServerCheckClientError` retourne false en grid authority.
- `SmoothCorrection` no-op supprime correction SimProxy stock.
- Flood mass scalaire, pas centre de masse.
- No impulse/force external API.
- No presented motion state public unifie.

Fluidity ceiling actuel:

- Le code n'est plus pure extrapolation: il a buffered playback + CubicInterp.
- Le plafond restant vient du pacing snapshot, du fixed frame, du root presentation et des consumers qui lisent un etat non presente.
- Si fixed frame saute, une frame peut batcher plusieurs substeps mais ne produire qu'un snapshot visible.

---

## 12. Snippets C++ verbatim


### 12.1 `USubMovementComponent.h` complet


Source: `Source/Sub3D/Submarine/SubMovementComponent.h (complete)`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineTypes.h"
#include "SubMovementComponent.generated.h"

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BaseMass = 200000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float WaterDensity = 1025.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float SubmergedVolume = 205.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bAutoNeutralBuoyancyOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeutralBuoyancyFill01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float NeutralBuoyancyMassBiasKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector DragCoefficients = FVector(0.18f, 1.2f, 1.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector CrossSections = FVector(2.f, 20.f, 20.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxThrust = 25000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxForwardSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxReverseSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxVerticalSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float IdleForwardSpeedDamping = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float LateralSpeedDamping = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float ContactVelocityDamping = 8.f;

	// Scales the yaw delta applied this tick when the previous step had a
	// blocking hit. 1.0 = stock behavior, 0.0 = fully frozen. Keeping some
	// yaw lets the pilot rotate away from the obstacle without the full
	// "rudder + wall" saccade.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ContactYawDampingFactor = 0.35f;

	// Max sweep/slide iterations for a single SimulateStep move call. 1 is
	// the stock one-shot slide; 2-3 produces noticeably smoother glide
	// along walls because the remaining motion is re-swept rather than
	// applied blind.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Contact", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxSlideIterations = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float RudderTurnRate = 15.f;

	// Minimum rudder authority at standstill (0..1). Allows very slow turn in place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RudderStandstillAuthority = 0.08f;

	// Speed at which rudder reaches full authority (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float RudderFullAuthoritySpeed = 350.f;

	// Yaw rate damping. Lower = more responsive, higher = more sluggish.
	// Halved from 2.0 for more hydrodynamic inertia feel; rudder also halved
	// so steady-state yaw rate at full input is unchanged (15/1 vs 30/2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float YawRateDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DivePlanePitchRate = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float HydroplaneAuthoritySpeed = 600.f;

	// Pitch rate damping. Halved from 2.2 for inertia feel; hydroplane input
	// halved in tandem so steady-state pitch rate at full input is unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchRateDamping = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchFromHydroplaneAccel = 12.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float VerticalFromPitchFactor = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxDivePlanePitch = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastPitchFactor = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bEnableBallastTrimPitch = true;

	// Halved alongside PitchRateDamping to preserve steady-state trim rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastTrimPitchRate = 4.f;

	// Amplifier on the ballast fill deviation from neutral. Values > 1 make
	// the player feel ballast changes more aggressively on vertical motion.
	// Neutral buoyancy is unaffected (deviation = 0 * anything = 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Ballast", meta = (ClampMin = "0.0"))
	float BallastEffectScale = 2.f;

	// How much pitch modulates the forward speed cap. 0 = no coupling.
	// Example: 0.2 gives factor 0.9 at pitch +30° (nose up) and 1.1 at -30°.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVmaxInfluence = 0.2f;

	// ── BG Restoring Moment ─────────────────────────────────────────────
	// Vertical distance from center of buoyancy to center of gravity (cm).
	// Positive = B above G = stable. Creates passive pitch return-to-level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Stability")
	float BG_DistanceCm = 30.f;

	// Strength of the BG restoring torque on pitch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Stability")
	float PitchRestorationDamping = 3.f;

	// ── Engine Spool ────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Thrust")
	float EngineSpoolUpRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Thrust")
	float EngineSpoolDownRate = 3.f;

	// Scales the effect of flooded-water mass (kg) on the sub's total mass
	// in ComputeTotalMass. 1.0 = each kg of interior water is 1 kg of gravity;
	// 0.0 = flood has no effect on vertical motion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Flooding")
	float FloodedMassInfluence = 1.f;

	// Authoritative input written by USubFloodComponent each tick: total interior
	// water mass in kg. Consumed by SimulateStep (fixed-tick 60 Hz) and folded into
	// ComputeTotalMass via FloodedMassInfluence. Server-only; clients receive the
	// resulting FloodedMassKg through FSubmarineNetState. Tick prereq in BeginPlay
	// guarantees SubFlood writes before SubMovement reads.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics|Flooding")
	float FloodImpactKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ballasts")
	TArray<FBallastTank> Ballasts;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Ballasts")
	float GlobalTargetFill = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float CurrentDepth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float FloodedMassKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float ForwardSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float EffectivePowerInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float YawRateDegPerSec = 0.f;

	// Authority-derived linear acceleration (cm/s^2, world space). Computed in
	// SimulateStep as (Velocity_post - Velocity_pre) / DeltaTime. Replicated via
	// FSubmarineNetState::LinearAcceleration. On non-authority, populated from the
	// snapshot stream; consumers should prefer GetPresentedLinearAcceleration() (Phase B)
	// once available.
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector LinearAcceleration = FVector::ZeroVector;

	// Authority-derived angular acceleration (deg/s^2, world space). Components mirror
	// AngularVelocity layout: X=roll-rate-derivative (unused), Y=pitch-rate-derivative,
	// Z=yaw-rate-derivative.
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector AngularAccelerationDeg = FVector::ZeroVector;

	// Server-authoritative sim rate. Higher = smoother visuals (smaller
	// extrapolation gap between sim steps) at the cost of CPU. 60 keeps
	// sub-tick visual jitter under 17 ms at any velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net")
	float FixedSimulationHz = 60.f;

	// ── Client snapshot playback (non-authority) ──────────────────────────
	// The client renders the sub at a fixed authority-time delay behind the newest
	// received snapshot. Local world time only advances the authority clock estimate;
	// segment choice and interpolation stay on the authority timeline derived from
	// SimFrame. Authority-max plan: see
	// reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md

	// Render-delay window in authority-time seconds. Render authority time =
	// EstimatedAuthorityNow - this delay. Must exceed typical inter-snapshot intervals
	// so the buffer usually contains a snapshot ahead of render time. 100 ms gives
	// about 3x headroom over a 30 Hz snapshot cadence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Playback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	double ClientPlaybackDelaySeconds = 0.10;

	// Maximum authority-time history retained in the buffer. Older entries are dropped.
	// Sized to cover the playback delay plus generous headroom; bounds memory.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Playback", meta = (ClampMin = "0.1"))
	double ClientMaxBufferHistorySeconds = 0.50;

	// If the real-time gap since the last received snapshot exceeds this, treat as a
	// stall (PIE alt-tab, genuine network drop) and reset playback by clearing the
	// buffer and snapping to the new pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Playback", meta = (ClampMin = "0.05"))
	double ClientStallResetSeconds = 0.20;

	// -------------------------------------------------------------------------
	// Input — server-authoritative, replicated to clients for visual feedback.
	// The PlayerController owning the helmsman routes raw input to the server
	// via ServerRPC; the server calls the Set*Input functions below, which
	// write to the replicated fields so every client sees the same rudder /
	// hydroplane / thrust value and can drive the visual mesh rotation off
	// those values with zero additional bandwidth work.
	//
	// For low-frequency input changes (rudder held, dive plane held), Replicated
	// floats are cheap: 3 × 4 bytes per dirty rep, well under the cost of the
	// existing FSubmarineNetState snapshot. If profiling ever requires tighter
	// packing, promote these to fixed-point int8 or fold them into
	// FSubmarineNetState — no public API change required.
	// -------------------------------------------------------------------------

	/** Set thrust input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetPowerInput(float Value);

	/** Alias kept for legacy callers; routes through SetPowerInput. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetThrustInput(float Value);

	/** Set rudder input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetRudderInput(float Value);

	/** Set dive plane input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetDivePlaneInput(float Value);

	/** Set interior flood-water mass in kg. Server-authoritative; written by USubFloodComponent. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetFloodImpactKg(float Value);

	/** Current rudder input, -1..1, replicated to all clients. Use this for mesh rotation. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetRudderInput() const { return RudderInput; }

	/** Current dive plane input, -1..1, replicated to all clients. Use this for mesh rotation. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetDivePlaneInput() const { return DivePlaneInput; }

	/** Current thrust input, -1..1, replicated to all clients. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetThrustInput() const { return ThrustInput; }

	void SetBallastTarget(int32 Index, float Target);
	void ResyncAllBallasts();
	void ApplyCommandState(const FSubmarineCommandState& CommandState);

	/** Copy authored performance fields (BaseMass, MaxSpeed, MaxThrust) from the
	  * Definition onto this component. Fields with a value of 0 are skipped.
	  * Recomputes neutral buoyancy if BaseMass changed. */
	void ApplyPerformanceProfileFromDefinition(const class USubmarineDefinition* Definition);

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeTotalMass() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeBuoyancyForce() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeCenterOfMassXOffset() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float GetPressureAtDepth(float DepthMeters) const;

	void HandleReplicatedNetState(const FSubmarineNetState& NewState);
	int32 GetSimFrameCounter() const { return SimFrameCounter; }
	float GetPitchRateDegPerSec() const { return PitchRateDegPerSec; }
	float GetYawRateDegPerSec() const { return YawRateDegPerSec; }

	// Server-side spooled engine power (-1..+1). Lags HelmThrottleCmd by
	// EngineSpoolUpRate / EngineSpoolDownRate. Read by the helm cockpit
	// for the throttle "spool meter" feedback.
	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float GetSpooledPower() const { return SpooledPower; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	bool HadBlockingHitLastStep() const { return bLastStepHadBlockingHit; }

private:
	// Helm input fields. Server-authoritative writes via SetThrustInput / SetRudderInput
	// / SetDivePlaneInput. NO LONGER replicated as separate UPROPERTYs — they ride in
	// FSubmarineNetState alongside pose/velocity so visual mesh consumers sample at the
	// same beat as the sub body. On non-authority the fields are written from the
	// snapshot stream in HandleReplicatedNetState.
	float ThrustInput = 0.f;
	float RudderInput = 0.f;
	float DivePlaneInput = 0.f;

	float SpooledPower = 0.f;
	float PitchRateDegPerSec = 0.f;
	float SimAccumulator = 0.f;
	int32 SimFrameCounter = 0;

	// Set at the end of each authority SimulateStep. Read publicly by
	// HadBlockingHitLastStep() and by SimulateStep to damp applied yaw.
	bool bLastStepHadBlockingHit = false;

	// ── Client-side snapshot playback (non-authority only) ────────────────────
	// Buffered render-delay playback per
	// reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md.
	// Each received snapshot is enqueued with an authority timeline derived from SimFrame,
	// plus its world-time receive time and a real-time receive time (the latter is used
	// only as a stall detector). EvaluateClientPlaybackPose estimates "authority now" from
	// the latest buffered snapshot and local world-time progression, then renders at
	// (AuthorityNow - ClientPlaybackDelaySeconds). Bracketing and Hermite interpolation run
	// on authority time, not on raw client receive spacing.
	struct FBufferedClientSubSnapshot
	{
		FSubmarineNetState State;
		double AuthorityTimeSeconds = 0.0;
		double WorldReceiveTime = 0.0;
		double RealReceiveTime = 0.0;
	};

	struct FClientPlaybackSample
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float Alpha = 0.f;
		float SegmentDistanceCm = 0.f;
		double SegmentSeconds = 0.0;
		double RenderAuthorityTimeSeconds = 0.0;
		double NewestAuthorityTimeSeconds = 0.0;
		bool bBufferUnderrun = false;
	};

	TArray<FBufferedClientSubSnapshot, TInlineAllocator<8>> ClientSnapshotBuffer;
	double ClientLastReceiveRealTime = 0.0;
	bool bHasReceivedClientSnapshot = false;

	double GetClientAuthorityStepSeconds() const;
	double GetClientAuthorityTimeSeconds(int32 SimFrame) const;
	double EstimateClientAuthorityNowSeconds(double WorldNow) const;
	void QueueClientSnapshot(const FSubmarineNetState& NewState, double WorldNow, double RealNow);
	void ResetClientPlayback(const FSubmarineNetState& NewState, double WorldNow, double RealNow, const TCHAR* Reason);
	bool EvaluateClientPlaybackPose(double WorldNow, FClientPlaybackSample& OutSample) const;
	void ApplyClientPlaybackPose(const FVector& Location, const FRotator& Rotation);

	// Sim-authoritative poses used to smooth the sub's visual render between fixed-tick
	// sim steps. CurrSim (updated each sim step) and PrevSim (the pose one step earlier)
	// bracket the Lerp that produces the rendered pose. Without this smoothing, at render
	// rates that differ from the sim rate (e.g. 45fps render vs 60Hz sim), the actor root
	// advances in discrete sim-step chunks (0, ~1, or ~2 chunks per render tick) which is
	// perceptible as jitter. The crew rebase reads the actor transform, which is this
	// interpolated pose — the rebase stays consistent because both the sub visual and the
	// crew see the same smoothed pose.
	FVector CurrSimLocation = FVector::ZeroVector;
	FRotator CurrSimRotation = FRotator::ZeroRotator;
	FVector PrevSimLocation = FVector::ZeroVector;
	FRotator PrevSimRotation = FRotator::ZeroRotator;
	bool bHasSimBuffer = false;
	bool bHasVisualOffset = false;

	// Per-render-frame pacing diagnostic (gated by bLogSubInterpPacing). Captured at end
	// of TickComponent so it reflects what the next subsystem (HUD, crew rebase, debug
	// labels) will read as the sub world transform this frame.
	FVector LastPacingEndLocation = FVector::ZeroVector;
	bool bHasLastPacingEndLocation = false;

	void UpdateBallasts(float DeltaTime);
	void SimulateStep(float DeltaTime);
	void ApplyPhysics(float DeltaTime);
	void InitializeNeutralBuoyancy();
};
```

### 12.2 `USubMovementComponent::BeginPlay` - prereq flood et fixed-frame warning


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:99-126`

```cpp
void USubMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeNeutralBuoyancy();

	// Tick order: SubFlood → SubMovement → CrewMovement.
	// SubFlood must advance and push FloodImpactKg before SimulateStep reads it.
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubFlood)
		{
			AddTickPrerequisiteComponent(Sub->SubFlood);
		}
	}

	// Guardrail: warn loudly on the server side if the project is running with variable
	// framerate. The sub physics loop emits one replication push per render frame, so
	// variable framerate produces variable-content snapshots that the client cannot
	// fully smooth — visible as trigger jitter on stair / breach. Production servers MUST
	// run with Engine.UseFixedFrameRate=true. PIE inherits the project setting.
	if (GetOwner() && GetOwner()->HasAuthority() && GEngine && !GEngine->bUseFixedFrameRate)
	{
		UE_LOG(LogSubMovement, Warning,
			TEXT("Sub3D requires Engine.UseFixedFrameRate=true (Project Settings → Engine → ")
			TEXT("General Settings → Framerate). Variable server framerate produces visible ")
			TEXT("client trigger jitter on stair/breach. Cf. memory/project_motion_chain_jitter_root_cause_2026_04_27.md"));
	}
}
```

### 12.3 `USubMovementComponent::SetFloodImpactKg`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:167-170`

```cpp
void USubMovementComponent::SetFloodImpactKg(float Value)
{
	FloodImpactKg = FMath::Max(0.f, Value);
}
```

### 12.4 `USubMovementComponent::TickComponent` - fixed loop et playback client


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:172-394`

```cpp
void USubMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->GetLevel())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	// Undo the previous frame's visual interpolation offset BEFORE the sim step so
	// the sim operates on the authoritative pose, not on the render-time lerped pose.
	if (bHasVisualOffset && Owner->HasAuthority())
	{
		Owner->SetActorLocationAndRotation(CurrSimLocation, CurrSimRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasVisualOffset = false;
	}

	// Diagnostic freeze: when set on the submarine instance (editor inspector or BP node),
	// zero velocities and skip the sim/interp pipeline. Useful to isolate crew rebase jitter
	// from sub-induced motion. The bootstrap pipeline does NOT touch this flag.
	if (const ASubmarineBase* Submarine = Cast<ASubmarineBase>(Owner))
	{
		if (Submarine->bFreezeMovementForTesting)
		{
			Velocity = FVector::ZeroVector;
			YawRateDegPerSec = 0.f;
			PitchRateDegPerSec = 0.f;
			CurrentDepth = FMath::Max(0.f, -Owner->GetActorLocation().Z / 100.f);

			if (Owner->HasAuthority())
			{
				if (ASubmarineBase* MutableSub = Cast<ASubmarineBase>(Owner))
				{
					MutableSub->RefreshRepState();
				}
			}

			return;
		}
	}

	if (!Owner->HasAuthority())
	{
		// Non-authority: buffered render-delay playback per
		// reports/plans/2026-04-27_submovement_non_authority_playback_refactor_and_audit.md.
		const UWorld* World = Owner->GetWorld();
		const double WorldNow = World ? World->GetTimeSeconds() : 0.0;
		const FVector RenderLocBefore = Owner->GetActorLocation();

		FClientPlaybackSample PlaybackSample;
		const bool bHasPose = EvaluateClientPlaybackPose(WorldNow, PlaybackSample);

		// Diagnostic: when the root-interpolation kill-switch is active, we DO compute the
		// playback sample (so logs and Embark/Compartment state stay coherent) but we do
		// NOT apply the interpolated pose to the actor root. Instead the actor root is
		// snapped to the newest received snapshot pose, which advances in discrete jumps
		// at the network update rate. This is the test for the GPT sim/presentation-conflict
		// hypothesis on the CLIENT side: if the trigger jitter disappears with this on,
		// the smoothed Owner.Transform is the cause; if it persists, the cause is below
		// the playback layer (CMC base handling, CMC floor detection, etc).
		const bool bDisableNonAuthRootInterp = IsSubRootInterpolationDisabled();
		if (bHasPose && !bDisableNonAuthRootInterp)
		{
			ApplyClientPlaybackPose(PlaybackSample.Location, PlaybackSample.Rotation);
		}
		else if (bDisableNonAuthRootInterp && ClientSnapshotBuffer.Num() > 0)
		{
			const FBufferedClientSubSnapshot& Newest = ClientSnapshotBuffer.Last();
			Owner->SetActorLocationAndRotation(
				Newest.State.WorldLocation, Newest.State.QuantizedRotation,
				false, nullptr, ETeleportType::TeleportPhysics);
		}

		const FVector RenderLocAfter = Owner->GetActorLocation();
		const float RenderStepCm = static_cast<float>((RenderLocAfter - RenderLocBefore).Size());

		if (GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement() && bHasPose)
		{
			const int32 BufferNum = ClientSnapshotBuffer.Num();
			if (PlaybackSample.bBufferUnderrun)
			{
				UE_LOG(
					LogSubMovement,
					Log,
					TEXT("Buffer underrun | QueueSize=%d | RenderAuth=%.4fs | NewestAuth=%.4fs"),
					BufferNum,
					PlaybackSample.RenderAuthorityTimeSeconds,
					PlaybackSample.NewestAuthorityTimeSeconds);
			}
			else
			{
				UE_LOG(
					LogSubMovement,
					Log,
					TEXT("Playback sample | Before=%s | After=%s | Alpha=%.2f | Seg=%.4fs | RenderStepCm=%.2f | SegmentCm=%.2f | QueueSize=%d"),
					*RenderLocBefore.ToCompactString(),
					*RenderLocAfter.ToCompactString(),
					PlaybackSample.Alpha,
					static_cast<float>(PlaybackSample.SegmentSeconds),
					RenderStepCm,
					PlaybackSample.SegmentDistanceCm,
					BufferNum);
			}
		}

		// Per-render-frame pacing diagnostic (independent toggle, untouched by plan refactor).
		if (GetDefault<USub3DDebugSettings>()->ShouldLogSubInterpPacing())
		{
			const float DxRender = bHasLastPacingEndLocation
				? static_cast<float>((RenderLocAfter - LastPacingEndLocation).Size())
				: 0.f;
			UE_LOG(
				LogSubMovement,
				Log,
				TEXT("Pacing | Role=NonAuth | dt=%.4f | Sub.X=%.2f | dx=%.2f | alpha=%.2f | seg=%.4fs | bufN=%d | underrun=%d"),
				DeltaTime,
				RenderLocAfter.X,
				DxRender,
				PlaybackSample.Alpha,
				static_cast<float>(PlaybackSample.SegmentSeconds),
				ClientSnapshotBuffer.Num(),
				PlaybackSample.bBufferUnderrun ? 1 : 0);
			LastPacingEndLocation = RenderLocAfter;
			bHasLastPacingEndLocation = true;
		}
		else
		{
			bHasLastPacingEndLocation = false;
		}

		return;
	}

	// Authoritative path. Sim runs at FixedSimulationHz (default 60Hz). At render rates
	// that differ from the sim rate, the actor root would otherwise advance in discrete
	// chunks (0/1/2 sim steps per render tick) which is perceptible as jitter. We smooth
	// the render pose via a Lerp(PrevSim, CurrSim, alpha) after the sim loop. The Undo
	// above runs at the start of next tick so the sim always starts from the authoritative
	// pose.
	//
	// CRITICAL: this loop emits ONE replication push per render frame, regardless of how
	// many sim steps fit in DeltaTime. UE coalesces multiple per-frame RefreshRepState
	// calls into one rep. So one snapshot may carry 1, 2 or 3 sim steps' worth of motion
	// depending on render dt. The CLIENT then traverses this variable-content segment,
	// producing visible jumps when the per-snapshot motion delta is uneven.
	//
	// REQUIRED PROJECT CONFIG: Engine.UseFixedFrameRate=true / FixedFrameRate=60
	// (Project Settings → Engine → General Settings → Framerate). Without it, PIE
	// variable framerate batches snapshots inconsistently and produces visible
	// trigger jitter (stair traversal, breach activation, etc.) on clients. See
	// memory/project_motion_chain_jitter_root_cause_2026_04_27.md.
	SimAccumulator += DeltaTime;
	const float FixedSimDt = FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.f / FixedSimulationHz) : (1.f / 30.f);
	bool bSimulated = false;

	while (SimAccumulator >= FixedSimDt)
	{
		PrevSimLocation = Owner->GetActorLocation();
		PrevSimRotation = Owner->GetActorRotation();
		SimulateStep(FixedSimDt);
		SimAccumulator -= FixedSimDt;
		++SimFrameCounter;
		bSimulated = true;
		bHasSimBuffer = true;
	}

	if (bSimulated)
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
		{
			// RepState must sample the authoritative pose, not the interp pose below.
			Sub->RefreshRepState();
		}
	}

	// Capture the latest sim pose as CurrSim, then apply the visual interp.
	CurrSimLocation = Owner->GetActorLocation();
	CurrSimRotation = Owner->GetActorRotation();

	const bool bDisableRootInterpolation = IsSubRootInterpolationDisabled();
	if (bDisableRootInterpolation)
	{
		bHasVisualOffset = false;
	}
	else if (bHasSimBuffer && !bLastStepHadBlockingHit)
	{
		const float Alpha = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
		const FVector InterpLocation = FMath::Lerp(PrevSimLocation, CurrSimLocation, Alpha);
		const FRotator InterpRotation = FMath::Lerp(PrevSimRotation, CurrSimRotation, Alpha);
		Owner->SetActorLocationAndRotation(InterpLocation, InterpRotation, false, nullptr, ETeleportType::None);
		bHasVisualOffset = true;
	}

	// ─── Per-render-frame pacing log (auth path) ───
	if (GetDefault<USub3DDebugSettings>()->ShouldLogSubInterpPacing())
	{
		const FVector EndLoc = Owner->GetActorLocation();
		const float DxRender = bHasLastPacingEndLocation
			? static_cast<float>((EndLoc - LastPacingEndLocation).Size())
			: 0.f;
		const float Alpha = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
		UE_LOG(
			LogSubMovement,
			Log,
			TEXT("Pacing | Role=Auth | dt=%.4f | Sub.X=%.2f | dx=%.2f | simAcc=%.4f | alpha=%.2f | simStep=%d"),
			DeltaTime,
			EndLoc.X,
			DxRender,
			SimAccumulator,
			Alpha,
			bSimulated ? 1 : 0);
		LastPacingEndLocation = EndLoc;
		bHasLastPacingEndLocation = true;
	}
	else
	{
		bHasLastPacingEndLocation = false;
	}
}
```

### 12.5 `USubMovementComponent::SimulateStep`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:396-446`

```cpp
void USubMovementComponent::SimulateStep(float DeltaTime)
{
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->Systems)
		{
			ApplyCommandState(Sub->Systems->GetCommandState());
		}
	}

	// Capture pre-step kinematic state so we can derive authoritative acceleration.
	// AngularVelocity layout in FSubmarineNetState mirrors (X=roll, Y=pitch, Z=yaw);
	// we drive yaw/pitch only, roll stays 0.
	const FVector PreStepVelocity = Velocity;
	const float PreStepYawRate = YawRateDegPerSec;
	const float PreStepPitchRate = PitchRateDegPerSec;

	FloodedMassKg = FloodImpactKg;
	UpdateBallasts(DeltaTime);
	ApplyPhysics(DeltaTime);

	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		LinearAcceleration = (Velocity - PreStepVelocity) / DeltaTime;
		AngularAccelerationDeg = FVector(
			0.f,
			(PitchRateDegPerSec - PreStepPitchRate) / DeltaTime,
			(YawRateDegPerSec - PreStepYawRate) / DeltaTime);
	}
}

void USubMovementComponent::UpdateBallasts(float DeltaTime)
{
	for (FBallastTank& Tank : Ballasts)
	{
		if (Tank.PumpState == EPumpState::Dead)
		{
			continue;
		}

		float FlowRate = Tank.PumpFlowRate;
		if (Tank.PumpState == EPumpState::Degraded)
		{
			FlowRate *= 0.5f;
		}

		const float Delta = Tank.TargetFill - Tank.FillLevel;
		const float Change = FMath::Sign(Delta) * FMath::Min(FMath::Abs(Delta), FlowRate * DeltaTime);
		Tank.FillLevel = FMath::Clamp(Tank.FillLevel + Change, 0.f, 1.f);
	}
}
```

### 12.6 `USubMovementComponent::ApplyPhysics`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:448-802`

```cpp
void USubMovementComponent::ApplyPhysics(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector LocalVelCm = OwnerTransform.InverseTransformVector(Velocity);
	const FVector LocalVelM = LocalVelCm / 100.f;
	const float ForwardSpeedAbs = FMath::Abs(LocalVelCm.X);

	// ── 1. Engine spool ─────────────────────────────────────────────────
	const float SpoolRate = (FMath::Abs(ThrustInput) > FMath::Abs(SpooledPower))
		? EngineSpoolUpRate : EngineSpoolDownRate;
	SpooledPower = FMath::FInterpTo(SpooledPower, ThrustInput, DeltaTime, SpoolRate);
	EffectivePowerInput = SpooledPower;

	// ── 2. Forces ───────────────────────────────────────────────────────
	const float TotalMass = ComputeTotalMass();
	const float InvMass = 1.f / FMath::Max(1.f, TotalMass);

	// Buoyancy vs gravity
	const float BuoyancyN = ComputeBuoyancyForce();
	const float GravityN = TotalMass * G_SI;
	const float NetVerticalN = BuoyancyN - GravityN;

	FVector Acceleration = FVector::ZeroVector;
	Acceleration.Z = (NetVerticalN * InvMass) * 100.f;

	// Thrust (uses spooled power, not raw input)
	float EngineHealth = 1.f;
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
	{
		if (Sub->Systems)
		{
			EngineHealth = FMath::Clamp(Sub->Systems->GetEngineHealth01(), 0.f, 1.f);
		}
	}
	const FVector ForwardDir = Owner->GetActorForwardVector();
	Acceleration += ForwardDir * (SpooledPower * MaxThrust * EngineHealth * InvMass) * 100.f;

	// Drag (quadratic, per-axis in local space — Fossen D(v) diagonal)
	FVector LocalDragN;
	LocalDragN.X = -0.5f * WaterDensity * DragCoefficients.X * CrossSections.X * LocalVelM.X * FMath::Abs(LocalVelM.X);
	LocalDragN.Y = -0.5f * WaterDensity * DragCoefficients.Y * CrossSections.Y * LocalVelM.Y * FMath::Abs(LocalVelM.Y);
	LocalDragN.Z = -0.5f * WaterDensity * DragCoefficients.Z * CrossSections.Z * LocalVelM.Z * FMath::Abs(LocalVelM.Z);

	const FVector WorldDragN = OwnerTransform.TransformVector(LocalDragN);
	Acceleration += (WorldDragN * InvMass) * 100.f;

	// ── 3. Rudder (gamified direct rate with speed authority) ────────────
	FRotator NewRotation = Owner->GetActorRotation();

	// Authority ramps from RudderStandstillAuthority (at speed=0) to 1.0 (at full authority speed).
	const float SpeedFraction = FMath::Clamp(ForwardSpeedAbs / FMath::Max(1.f, RudderFullAuthoritySpeed), 0.f, 1.f);
	const float SpeedAuthority = FMath::Lerp(RudderStandstillAuthority, 1.f, SpeedFraction);

	const float TargetYawRate = RudderInput * RudderTurnRate * SpeedAuthority;
	YawRateDegPerSec += (TargetYawRate - YawRateDegPerSec * YawRateDamping) * DeltaTime;
	YawRateDegPerSec = FMath::Clamp(YawRateDegPerSec, -RudderTurnRate, RudderTurnRate);

	// Option C — contact-aware yaw damping. The rate state itself keeps
	// tracking rudder input (so releasing contact feels immediate), but the
	// yaw actually committed to the rotation this tick is scaled down while
	// the previous step hit something. This kills the "full thrust + wall +
	// rudder" saccade where rotation kept driving the hull into geometry
	// the sweep then had to push back out.
	const float ContactYawScale = bLastStepHadBlockingHit
		? FMath::Clamp(ContactYawDampingFactor, 0.f, 1.f)
		: 1.f;
	NewRotation.Yaw += YawRateDegPerSec * DeltaTime * ContactYawScale;

	// ── 4. Pitch (hydroplane + ballast trim + BG restoring moment) ──────
	const float HydroplaneSpeedFactor = HydroplaneAuthoritySpeed > KINDA_SMALL_NUMBER
		? FMath::Clamp(ForwardSpeedAbs / HydroplaneAuthoritySpeed, 0.f, 1.f)
		: 1.f;

	float BallastTrimRateBias = 0.f;
	if (bEnableBallastTrimPitch && Ballasts.Num() >= 2)
	{
		const float FrontFill = Ballasts[0].FillLevel;
		const float RearFill = Ballasts.Last().FillLevel;
		const float TrimBias = RearFill - FrontFill;
		BallastTrimRateBias = TrimBias * BallastTrimPitchRate;
	}

	const float HydroplanePitchAccel = DivePlaneInput * PitchFromHydroplaneAccel * HydroplaneSpeedFactor;
	const float BallastPitchTarget = -ComputeCenterOfMassXOffset() / 100.f * BallastPitchFactor;
	const float BallastPitchRateCorrection = (BallastPitchTarget - NewRotation.Pitch) * 0.6f;

	// BG restoring moment: B above G creates pendulum-like return to level.
	// RestoreMoment = -BG * sin(pitch) * damping. This is the key physics
	// that makes submarines naturally return to level without hydroplane input.
	const float PitchRad = FMath::DegreesToRadians(NewRotation.Pitch);
	const float BG_RestoreDegPerSec2 = -BG_DistanceCm * FMath::Sin(PitchRad) * PitchRestorationDamping;

	PitchRateDegPerSec += (
		HydroplanePitchAccel +
		BallastTrimRateBias +
		BallastPitchRateCorrection +
		BG_RestoreDegPerSec2 -
		(PitchRateDegPerSec * PitchRateDamping)
	) * DeltaTime;

	NewRotation.Pitch = FMath::Clamp(
		NewRotation.Pitch + PitchRateDegPerSec * DeltaTime,
		-MaxDivePlanePitch,
		MaxDivePlanePitch
	);

	// ── 5. Pitch-to-vertical coupling ───────────────────────────────────
	const float VerticalFromPitchAccel = LocalVelCm.X * FMath::Sin(PitchRad) * VerticalFromPitchFactor;
	Acceleration.Z += VerticalFromPitchAccel;

	// ── 6. Integrate (semi-implicit Euler) ──────────────────────────────
	Velocity += Acceleration * DeltaTime;

	FVector ClampedLocalVelocity = OwnerTransform.InverseTransformVector(Velocity);

	// Pitch-dependent forward speed cap. Nose-down gets a gain (gravity helps),
	// nose-up gets a penalty (gravity fights us). PitchVmaxInfluence controls
	// the swing; defaults to 0.2 (so ±10% at ±MaxDivePlanePitch of ±30°).
	// Independent of input thrust — limits terminal speed, not acceleration.
	const float PitchSin = FMath::Sin(PitchRad);
	const float PitchFactor = FMath::Clamp(1.f - PitchSin * PitchVmaxInfluence,
		1.f - PitchVmaxInfluence, 1.f + PitchVmaxInfluence);
	const float EffectiveMaxForward = MaxForwardSpeed * PitchFactor;
	const float EffectiveMaxReverse = MaxReverseSpeed * PitchFactor;

	ClampedLocalVelocity.X = FMath::Clamp(ClampedLocalVelocity.X, -EffectiveMaxReverse, EffectiveMaxForward);
	if (FMath::Abs(SpooledPower) < 0.01f)
	{
		ClampedLocalVelocity.X = FMath::FInterpTo(ClampedLocalVelocity.X, 0.f, DeltaTime, IdleForwardSpeedDamping);
	}
	ClampedLocalVelocity.Y = FMath::FInterpTo(ClampedLocalVelocity.Y, 0.f, DeltaTime, LateralSpeedDamping);
	ClampedLocalVelocity.Z = FMath::Clamp(ClampedLocalVelocity.Z, -MaxVerticalSpeed, MaxVerticalSpeed);
	Velocity = OwnerTransform.TransformVector(ClampedLocalVelocity);

	ForwardSpeedCmS = ClampedLocalVelocity.X;

	// ── 7. Debug logging ────────────────────────────────────────────────
	if (GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement())
	{
		static float ServerDebugLogTimer = 0.f;
		ServerDebugLogTimer += DeltaTime;
		if (ServerDebugLogTimer >= 0.5f)
		{
			ServerDebugLogTimer = 0.f;
			UE_LOG(LogSubMovement, Log,
				TEXT("Physics | Spool=%.2f | Vel=%s | FwdSpd=%.1f | Yaw=%.2f | Pitch=%.2f | Depth=%.1f | Mass=%.0f | BG_Restore=%.2f"),
				SpooledPower,
				*Velocity.ToCompactString(),
				ForwardSpeedCmS,
				YawRateDegPerSec,
				PitchRateDegPerSec,
				CurrentDepth,
				TotalMass,
				BG_RestoreDegPerSec2);
		}
	}

	// ── 8. Move with collision sweep ────────────────────────────────────
	// The submarine root is a USceneComponent (no shape), so a direct
	// AddActorWorldOffset(..., bSweep=true, ...) would silently do nothing
	// because USceneComponent::MoveComponent ignores the sweep flag. We
	// instead run an explicit ComponentSweepMulti against the submarine's
	// designated movement collision component (GetMovementCollisionComponent
	// — typically HullMesh) and then translate the whole actor by the
	// adjusted delta. For this to block external geometry (Traversal Route,
	// walls, terrain), the submarine BP must assign a collidable static mesh
	// to HullMesh (or equivalent override of GetMovementCollisionComponent).
	const FVector DeltaLocation = Velocity * DeltaTime;
	bool bHadBlockingHit = false;

	ASubmarineBase* SubForSweep = Cast<ASubmarineBase>(Owner);
	UPrimitiveComponent* SweepShape = SubForSweep ? SubForSweep->GetMovementCollisionComponent() : nullptr;
	UWorld* World = Owner->GetWorld();
	const bool bCanSweep = World && SweepShape && SweepShape->IsCollisionEnabled();

	const auto MoveWithSlide = [Owner, SweepShape, World, bCanSweep, this](const FVector& MoveDelta, FVector& InOutVelocity)
	{
		if (MoveDelta.IsNearlyZero())
		{
			return false;
		}

		// Fallback: no sweep shape / collision disabled. Translate without a
		// hit test — same behavior as the original code when the sub has no
		// hull collision configured.
		if (!bCanSweep)
		{
			Owner->AddActorWorldOffset(MoveDelta, false, nullptr, ETeleportType::None);
			return false;
		}

		if (UsesComplexAsSimpleSweep(SweepShape))
		{
			if (GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps || GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement())
			{
				UE_LOG(
					LogSubMovement,
					Warning,
					TEXT("HullSweep unsupported | Comp=%s | Reason=UseComplexAsSimple cannot be used as the moving sweep shape in the current engine path"),
					*GetNameSafe(SweepShape));
			}

			Owner->AddActorWorldOffset(MoveDelta, false, nullptr, ETeleportType::None);
			return false;
		}

		FComponentQueryParams Params(SCENE_QUERY_STAT(SubHullSweep));
		Params.AddIgnoredActor(Owner);
		Params.bTraceComplex = SweepShape->bTraceComplexOnMove;

		TArray<AActor*> AttachedActors;
		Owner->GetAttachedActors(AttachedActors, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor)
			{
				Params.AddIgnoredActor(AttachedActor);
			}
		}

		// Iterative sweep → advance → slide → re-sweep. Each iteration consumes
		// the remaining portion of the move along one contact plane; on the
		// next pass we re-sweep the projected remainder rather than applying
		// it blind. This is what kills the "enter / exit / re-collide" chatter
		// on grazing contacts and inner corners. Bounded to MaxSlideIterations
		// so a pathological case (two near-parallel walls) can't live-lock.
		FVector RemainingDelta = MoveDelta;
		bool bAnyBlocking = false;
		const int32 IterationBound = FMath::Clamp(MaxSlideIterations, 1, 4);

		for (int32 Iteration = 0; Iteration < IterationBound; ++Iteration)
		{
			if (RemainingDelta.IsNearlyZero())
			{
				break;
			}

			const FVector Start = SweepShape->GetComponentLocation();
			const FVector End = Start + RemainingDelta;
			const FQuat SweepRot = SweepShape->GetComponentQuat();

			TArray<FHitResult> Hits;
			const bool bAnyHit = World->ComponentSweepMulti(Hits, SweepShape, Start, End, SweepRot, Params);
			FHitResult Blocking;
			bool bHasBlocking = false;
			if (bAnyHit)
			{
				for (const FHitResult& H : Hits)
				{
					if (!H.bBlockingHit)
					{
						continue;
					}

					if (IsSweepHitInternalToSubmarine(Owner, H))
					{
						if (GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps)
						{
							UE_LOG(LogSubMovement, Verbose,
								TEXT("HullSweep ignored internal hit | Comp=%s | OtherActor=%s | OtherComp=%s"),
								*GetNameSafe(SweepShape),
								*GetNameSafe(H.GetActor()),
								*GetNameSafe(H.GetComponent()));
						}
						continue;
					}

					Blocking = H;
					bHasBlocking = true;
					break;
				}
			}

			if (!bHasBlocking)
			{
				Owner->AddActorWorldOffset(RemainingDelta, false, nullptr, ETeleportType::None);
				RemainingDelta = FVector::ZeroVector;
				break;
			}

			bAnyBlocking = true;

			// Advance up to the hit, then depenetrate if the sweep started inside geometry.
			const float HitTime = FMath::Clamp(Blocking.Time, 0.f, 1.f);
			const FVector AdvanceDelta = RemainingDelta * HitTime;
			if (!AdvanceDelta.IsNearlyZero())
			{
				Owner->AddActorWorldOffset(AdvanceDelta, false, nullptr, ETeleportType::None);
			}

			if (Blocking.bStartPenetrating)
			{
				const FVector Depen = Blocking.Normal * FMath::Max(2.f, Blocking.PenetrationDepth + 1.f);
				Owner->AddActorWorldOffset(Depen, false, nullptr, ETeleportType::None);
			}

			// Project the leftover motion onto the hit plane and project the
			// outgoing velocity so subsequent iterations (and the next tick)
			// see a tangent-only velocity along every contact normal hit
			// this tick.
			const float RemainingFraction = 1.f - HitTime;
			RemainingDelta = FVector::VectorPlaneProject(RemainingDelta * RemainingFraction, Blocking.Normal);
			InOutVelocity = FVector::VectorPlaneProject(InOutVelocity, Blocking.Normal);

			if (GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps)
			{
				UE_LOG(LogSubMovement, Log,
					TEXT("HullSweep hit | Iter=%d | Comp=%s | OtherActor=%s | OtherComp=%s | Normal=%s | Time=%.3f"),
					Iteration,
					*GetNameSafe(SweepShape),
					*GetNameSafe(Blocking.GetActor()),
					*GetNameSafe(Blocking.GetComponent()),
					*Blocking.Normal.ToCompactString(),
					HitTime);
			}
		}

		// If iterations were exhausted and a non-zero remainder is still
		// pending, DROP it. The previous "apply unswept as fallback" path
		// could push the hull through a corner / double-wall geometry
		// because no sweep was performed for that final segment. The
		// player feels a tiny stick in true corners, but the sub never
		// teleports through walls. Sticky > tunneling.
		if (!RemainingDelta.IsNearlyZero() && GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps)
		{
			UE_LOG(LogSubMovement, Verbose,
				TEXT("HullSweep | iter exhausted, dropping residual=%s (mag=%.2f)"),
				*RemainingDelta.ToCompactString(), RemainingDelta.Size());
		}

		return bAnyBlocking;
	};

	const FVector HorizontalDelta = FVector(DeltaLocation.X, DeltaLocation.Y, 0.f);
	const FVector VerticalDelta = FVector(0.f, 0.f, DeltaLocation.Z);
	bHadBlockingHit |= MoveWithSlide(HorizontalDelta, Velocity);
	bHadBlockingHit |= MoveWithSlide(VerticalDelta, Velocity);
	if (bHadBlockingHit)
	{
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, ContactVelocityDamping);
	}

	// Latch contact state so the next SimulateStep (Option C — yaw damping)
	// and TickComponent (Option A — visual extrapolation gate) can react.
	bLastStepHadBlockingHit = bHadBlockingHit;

	Owner->SetActorRotation(NewRotation, ETeleportType::None);
	CurrentDepth = FMath::Max(0.f, -Owner->GetActorLocation().Z / 100.f);
}
```

### 12.7 `USubMovementComponent::ComputeTotalMass`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:804-823`

```cpp
float USubMovementComponent::ComputeTotalMass() const
{
	// Raw water mass and the water mass the tanks would hold at neutral fill.
	float WaterMass = 0.f;
	float NeutralWaterMass = 0.f;
	for (const FBallastTank& Tank : Ballasts)
	{
		WaterMass += Tank.FillLevel * Tank.Volume * WaterDensity;
		NeutralWaterMass += NeutralBuoyancyFill01 * Tank.Volume * WaterDensity;
	}

	// Amplify only the deviation from neutral. A full tank (or empty tank)
	// therefore pushes the sub harder on the vertical axis while a tank held
	// at NeutralBuoyancyFill01 still produces zero net effect (neutral stays
	// neutral regardless of scale).
	const float ScaledDeviation = (WaterMass - NeutralWaterMass) * BallastEffectScale;
	const float EffectiveWaterMass = NeutralWaterMass + ScaledDeviation;

	return BaseMass + EffectiveWaterMass + (FloodedMassKg * FloodedMassInfluence);
}
```

### 12.8 `USubMovementComponent::ApplyPerformanceProfileFromDefinition`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:825-865`

```cpp
void USubMovementComponent::ApplyPerformanceProfileFromDefinition(const USubmarineDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}

	bool bMassChanged = false;
	if (Definition->BaseMassKg > KINDA_SMALL_NUMBER)
	{
		BaseMass = Definition->BaseMassKg;
		bMassChanged = true;
	}
	if (Definition->MaxForwardSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxForwardSpeed = Definition->MaxForwardSpeedCmS;
	}
	if (Definition->MaxReverseSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxReverseSpeed = Definition->MaxReverseSpeedCmS;
	}
	if (Definition->MaxVerticalSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxVerticalSpeed = Definition->MaxVerticalSpeedCmS;
	}
	if (Definition->MaxThrustN > KINDA_SMALL_NUMBER)
	{
		MaxThrust = Definition->MaxThrustN;
	}

	if (bMassChanged)
	{
		InitializeNeutralBuoyancy();
	}
}

float USubMovementComponent::ComputeBuoyancyForce() const
{
	return WaterDensity * SubmergedVolume * G_SI;
}
```

### 12.9 `USubMovementComponent::ApplyCommandState`


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:904-910`

```cpp
void USubMovementComponent::ApplyCommandState(const FSubmarineCommandState& CommandState)
{
	SetThrustInput(CommandState.HelmThrottleCmd);
	SetRudderInput(CommandState.HelmYawCmd);
	SetDivePlaneInput(CommandState.HelmTrimCmd);
	GlobalTargetFill = CommandState.GlobalBallastTarget01;
}
```

### 12.10 `FSubmarineNetState` struct


Source: `Source/Sub3D/Submarine/SubmarineRuntimeTypes.h:93-125`

```cpp
	FVector_NetQuantize100 WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FRotator QuantizedRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 LinearVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 AngularVelocity = FVector::ZeroVector;

	// Authority-derived linear acceleration (cm/s^2, world space). Replicated so the
	// client presentation chain can sample acceleration AT THE PLAYBACK MOMENT instead of
	// re-deriving from finite differences on a smoothed-and-lagged transform.
	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 LinearAcceleration = FVector::ZeroVector;

	// Authority-derived angular acceleration (deg/s^2, world space).
	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 AngularAccelerationDeg = FVector::ZeroVector;

	// Helm input snapshots, replicated alongside pose/velocity so visual mesh consumers
	// (rudder, dive plane, thrust) sample at the same beat as the sub body. Replaces the
	// independent UPROPERTY(Replicated) streams previously on USubMovementComponent.
	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float RudderInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float DivePlaneInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float ThrustInput = 0.f;
```

### 12.11 `ASubmarineBase` constructor - replication settings


Source: `Source/Sub3D/Submarine/SubmarineBase.cpp:186-220`

```cpp
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	// Match the sim rate (60Hz) so each fixed-tick produces one snapshot. With 30Hz, each
	// snapshot covered 2 sim substeps; under flood-induced acceleration the inter-snapshot
	// motion delta grew large enough that any cadence variance produced visible jitter on
	// the client Hermite playback. 60Hz halves the delta and tightens the InterpDuration
	// variance window.
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);

	SubmarineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SubmarineRoot"));
	SetRootComponent(SubmarineRoot);

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(SubmarineRoot);

	MovementCollisionProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovementCollisionProxy"));
	MovementCollisionProxy->SetupAttachment(HullMesh);

	ApplyHullCollisionDefaults();

	SubMovement = CreateDefaultSubobject<USubMovementComponent>(TEXT("SubMovement"));
	SubHull = CreateDefaultSubobject<USubHullComponent>(TEXT("SubHull"));
	SubFlood = CreateDefaultSubobject<USubFloodComponent>(TEXT("SubFlood"));
	Systems = CreateDefaultSubobject<USubmarineSystemsComponent>(TEXT("Systems"));
	Compartments = CreateDefaultSubobject<USubmarineCompartmentComponent>(TEXT("Compartments"));
	StationManager = CreateDefaultSubobject<USubmarineStationManagerComponent>(TEXT("StationManager"));
	Radar = CreateDefaultSubobject<USubmarineRadarComponent>(TEXT("Radar"));
	BreachVfxManager = CreateDefaultSubobject<UBreachVfxManagerComponent>(TEXT("BreachVfxManager"));
	FloodWaterVisuals = CreateDefaultSubobject<UFloodWaterVisualsComponent>(TEXT("FloodWaterVisuals"));
	HullVisualDamage = CreateDefaultSubobject<USubHullVisualDamageComponent>(TEXT("HullVisualDamage"));
	DoorFloodVfx = CreateDefaultSubobject<UDoorFloodVfxComponent>(TEXT("DoorFloodVfx"));
	FeedbackManager = CreateDefaultSubobject<USubmarineFeedbackDirectorComponent>(TEXT("FeedbackManager"));
```

### 12.12 `ASubmarineBase::RefreshRepState`


Source: `Source/Sub3D/Submarine/SubmarineBase.cpp:1178-1219`

```cpp
void ASubmarineBase::RefreshRepState()
{
	RepState.WorldLocation = GetActorLocation();
	RepState.QuantizedRotation = GetActorRotation();

	if (SubMovement)
	{
		RepState.LinearVelocity = SubMovement->Velocity;
		RepState.AngularVelocity = FVector(0.f, SubMovement->GetPitchRateDegPerSec(), SubMovement->GetYawRateDegPerSec());
		RepState.LinearAcceleration = SubMovement->LinearAcceleration;
		RepState.AngularAccelerationDeg = SubMovement->AngularAccelerationDeg;
		RepState.RudderInput = SubMovement->GetRudderInput();
		RepState.DivePlaneInput = SubMovement->GetDivePlaneInput();
		RepState.ThrustInput = SubMovement->GetThrustInput();
		RepState.ForwardSpeed = FVector::DotProduct(SubMovement->Velocity, GetActorForwardVector());
		RepState.VerticalSpeed = SubMovement->Velocity.Z;
		RepState.DepthMeters = SubMovement->CurrentDepth;
		RepState.FloodedMassKg = SubMovement->FloodedMassKg;
		RepState.BallastGlobal01 = SubMovement->GlobalTargetFill;
		RepState.SimFrame = SubMovement->GetSimFrameCounter();
	}

	if (Systems)
	{
		RepState.MainTrim01 = Systems->GetCommandState().MainTrimBiasCmd;
		RepState.bPumpActive = Systems->GetCommandState().bPumpActive;
	}
}

float ASubmarineBase::GetCurrentDepthMeters() const
{
	return SubMovement ? SubMovement->CurrentDepth : 0.f;
}

FTransform ASubmarineBase::GetPrimaryCrewSpawnTransform() const
{
	return GetCrewSpawnTransformForSlot(0);
}

FTransform ASubmarineBase::GetCrewSpawnTransformForSlot(int32 SlotIndex) const
{
	const FName SocketName(*FString::Printf(TEXT("CrewSocket%d"), SlotIndex + 1));
```

### 12.13 `ASubmarineBase::OnRep_RepState`


Source: `Source/Sub3D/Submarine/SubmarineBase.cpp:1427-1435`

```cpp
void ASubmarineBase::OnRep_RepState()
{
	if (SubMovement)
	{
		SubMovement->HandleReplicatedNetState(RepState);
	}
}

void ASubmarineBase::HandleBreachesUpdatedForFlood(const TArray<FBreachClusterState>& Breaches)
```

### 12.14 Snapshot consume / playback


Source: `Source/Sub3D/Submarine/SubMovementComponent.cpp:912-1144`

```cpp
void USubMovementComponent::HandleReplicatedNetState(const FSubmarineNetState& NewState)
{
	AActor* Owner = GetOwner();
	if (!Owner || Owner->HasAuthority())
	{
		return;
	}

	const UWorld* World = Owner->GetWorld();
	const double WorldNow = World ? World->GetTimeSeconds() : 0.0;
	const double RealNow = FPlatformTime::Seconds();

	const double RealGapSeconds = bHasReceivedClientSnapshot ? (RealNow - ClientLastReceiveRealTime) : 0.0;

	if (!bHasReceivedClientSnapshot)
	{
		ResetClientPlayback(NewState, WorldNow, RealNow, TEXT("FirstSnapshot"));
	}
	else if (RealGapSeconds > ClientStallResetSeconds)
	{
		ResetClientPlayback(NewState, WorldNow, RealNow, TEXT("StallRecovery"));
	}
	else
	{
		QueueClientSnapshot(NewState, WorldNow, RealNow);
	}
}

double USubMovementComponent::GetClientAuthorityStepSeconds() const
{
	return FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.0 / static_cast<double>(FixedSimulationHz)) : (1.0 / 30.0);
}

double USubMovementComponent::GetClientAuthorityTimeSeconds(int32 SimFrame) const
{
	return static_cast<double>(SimFrame) * GetClientAuthorityStepSeconds();
}

double USubMovementComponent::EstimateClientAuthorityNowSeconds(double WorldNow) const
{
	if (ClientSnapshotBuffer.Num() == 0)
	{
		return 0.0;
	}

	const FBufferedClientSubSnapshot& Last = ClientSnapshotBuffer.Last();
	const double ReceiveElapsedSeconds = FMath::Max(0.0, WorldNow - Last.WorldReceiveTime);
	return Last.AuthorityTimeSeconds + ReceiveElapsedSeconds;
}

void USubMovementComponent::QueueClientSnapshot(const FSubmarineNetState& NewState, double WorldNow, double RealNow)
{
	const double AuthorityTimeSeconds = GetClientAuthorityTimeSeconds(NewState.SimFrame);
	const int32 PreviousNum = ClientSnapshotBuffer.Num();
	const FVector PreviousLoc = (PreviousNum > 0) ? FVector(ClientSnapshotBuffer.Last().State.WorldLocation) : FVector::ZeroVector;
	const int32 PreviousSimFrame = (PreviousNum > 0) ? ClientSnapshotBuffer.Last().State.SimFrame : NewState.SimFrame;
	const double PreviousAuthorityTime = (PreviousNum > 0) ? ClientSnapshotBuffer.Last().AuthorityTimeSeconds : AuthorityTimeSeconds;
	const double PreviousRealTime = (PreviousNum > 0) ? ClientSnapshotBuffer.Last().RealReceiveTime : RealNow;

	if (PreviousNum > 0)
	{
		const FBufferedClientSubSnapshot& Last = ClientSnapshotBuffer.Last();
		if (NewState.SimFrame <= Last.State.SimFrame || AuthorityTimeSeconds <= Last.AuthorityTimeSeconds)
		{
			if (GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement())
			{
				UE_LOG(
					LogSubMovement,
					Warning,
					TEXT("Snapshot dropped | Frame=%d | LastFrame=%d | AuthTime=%.4fs | LastAuth=%.4fs"),
					NewState.SimFrame,
					Last.State.SimFrame,
					AuthorityTimeSeconds,
					Last.AuthorityTimeSeconds);
			}
			return;
		}
	}

	ClientSnapshotBuffer.Add({NewState, AuthorityTimeSeconds, WorldNow, RealNow});
	ClientLastReceiveRealTime = RealNow;
	bHasReceivedClientSnapshot = true;

	// Drop entries older than the configured retention window on the authority timeline.
	const double OldestKeptAuthorityTime = AuthorityTimeSeconds - ClientMaxBufferHistorySeconds;
	while (ClientSnapshotBuffer.Num() > 2 && ClientSnapshotBuffer[0].AuthorityTimeSeconds < OldestKeptAuthorityTime)
	{
		ClientSnapshotBuffer.RemoveAt(0, 1, EAllowShrinking::No);
	}

	// Non-positional state is sampled directly each snapshot — HUD, debugger, flood reads,
	// rudder/dive-plane mesh visuals (until Phase B's smoothed presented values exist).
	Velocity = NewState.LinearVelocity;
	YawRateDegPerSec = NewState.AngularVelocity.Z;
	PitchRateDegPerSec = NewState.AngularVelocity.Y;
	LinearAcceleration = NewState.LinearAcceleration;
	AngularAccelerationDeg = NewState.AngularAccelerationDeg;
	RudderInput = NewState.RudderInput;
	DivePlaneInput = NewState.DivePlaneInput;
	ThrustInput = NewState.ThrustInput;
	CurrentDepth = NewState.DepthMeters;
	FloodedMassKg = NewState.FloodedMassKg;
	ForwardSpeedCmS = NewState.ForwardSpeed;

	if (GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement())
	{
		const double AuthorityGap = (PreviousNum > 0) ? (AuthorityTimeSeconds - PreviousAuthorityTime) : 0.0;
		const double RealGap = (PreviousNum > 0) ? (RealNow - PreviousRealTime) : 0.0;
		const int32 FrameGap = (PreviousNum > 0) ? (NewState.SimFrame - PreviousSimFrame) : 0;
		const float StepCm = (PreviousNum > 0) ? FVector::Distance(NewState.WorldLocation, PreviousLoc) : 0.f;
		UE_LOG(
			LogSubMovement,
			Log,
			TEXT("Snapshot queued | Frame=%d | Loc=%s | QueueSize=%d | StepCm=%.2f | FrameGap=%d | AuthGap=%.4fs | RealGap=%.4fs"),
			NewState.SimFrame,
			*FVector(NewState.WorldLocation).ToCompactString(),
			ClientSnapshotBuffer.Num(),
			StepCm,
			FrameGap,
			static_cast<float>(AuthorityGap),
			static_cast<float>(RealGap));
	}
}

void USubMovementComponent::ResetClientPlayback(const FSubmarineNetState& NewState, double WorldNow, double RealNow, const TCHAR* Reason)
{
	const double PreviousRealTime = ClientLastReceiveRealTime;
	const double RealGap = bHasReceivedClientSnapshot ? (RealNow - PreviousRealTime) : 0.0;
	const double AuthorityTimeSeconds = GetClientAuthorityTimeSeconds(NewState.SimFrame);

	ClientSnapshotBuffer.Reset();
	ClientSnapshotBuffer.Add({NewState, AuthorityTimeSeconds, WorldNow, RealNow});
	ClientLastReceiveRealTime = RealNow;
	bHasReceivedClientSnapshot = true;

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocationAndRotation(
			NewState.WorldLocation, NewState.QuantizedRotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	Velocity = NewState.LinearVelocity;
	YawRateDegPerSec = NewState.AngularVelocity.Z;
	PitchRateDegPerSec = NewState.AngularVelocity.Y;
	LinearAcceleration = NewState.LinearAcceleration;
	AngularAccelerationDeg = NewState.AngularAccelerationDeg;
	RudderInput = NewState.RudderInput;
	DivePlaneInput = NewState.DivePlaneInput;
	ThrustInput = NewState.ThrustInput;
	CurrentDepth = NewState.DepthMeters;
	FloodedMassKg = NewState.FloodedMassKg;
	ForwardSpeedCmS = NewState.ForwardSpeed;

	if (GetDefault<USub3DDebugSettings>()->ShouldLogSubMovement())
	{
		UE_LOG(
			LogSubMovement,
			Log,
			TEXT("Playback reset | Reason=%s | Frame=%d | AuthTime=%.4fs | RealGap=%.4fs"),
			Reason ? Reason : TEXT("Unknown"),
			NewState.SimFrame,
			AuthorityTimeSeconds,
			static_cast<float>(RealGap));
	}
}

bool USubMovementComponent::EvaluateClientPlaybackPose(double WorldNow, FClientPlaybackSample& OutSample) const
{
	OutSample = FClientPlaybackSample();

	const int32 BufferNum = ClientSnapshotBuffer.Num();
	if (BufferNum == 0)
	{
		return false;
	}

	const double RenderAuthorityTime = EstimateClientAuthorityNowSeconds(WorldNow) - ClientPlaybackDelaySeconds;
	const FBufferedClientSubSnapshot& First = ClientSnapshotBuffer[0];
	const FBufferedClientSubSnapshot& Last = ClientSnapshotBuffer.Last();
	OutSample.RenderAuthorityTimeSeconds = RenderAuthorityTime;
	OutSample.NewestAuthorityTimeSeconds = Last.AuthorityTimeSeconds;

	if (BufferNum == 1 || RenderAuthorityTime <= First.AuthorityTimeSeconds)
	{
		OutSample.Location = First.State.WorldLocation;
		OutSample.Rotation = First.State.QuantizedRotation;
		OutSample.bBufferUnderrun = true;
		return true;
	}

	if (RenderAuthorityTime >= Last.AuthorityTimeSeconds)
	{
		OutSample.Location = Last.State.WorldLocation;
		OutSample.Rotation = Last.State.QuantizedRotation;
		OutSample.Alpha = 1.f;
		OutSample.bBufferUnderrun = true;
		return true;
	}

	int32 IdxHigh = BufferNum - 1;
	while (IdxHigh > 0 && ClientSnapshotBuffer[IdxHigh].AuthorityTimeSeconds > RenderAuthorityTime)
	{
		--IdxHigh;
	}
	const int32 IdxA = IdxHigh;
	const int32 IdxB = FMath::Min(IdxHigh + 1, BufferNum - 1);
	const FBufferedClientSubSnapshot& A = ClientSnapshotBuffer[IdxA];
	const FBufferedClientSubSnapshot& B = ClientSnapshotBuffer[IdxB];
	const double SegmentSeconds = FMath::Max(B.AuthorityTimeSeconds - A.AuthorityTimeSeconds, 1e-6);
	const float Alpha = FMath::Clamp(static_cast<float>((RenderAuthorityTime - A.AuthorityTimeSeconds) / SegmentSeconds), 0.f, 1.f);
	const float SegmentSecondsF = static_cast<float>(SegmentSeconds);

	const FVector P0 = A.State.WorldLocation;
	const FVector P1 = B.State.WorldLocation;
	const FVector T0 = FVector(A.State.LinearVelocity) * SegmentSecondsF;
	const FVector T1 = FVector(B.State.LinearVelocity) * SegmentSecondsF;
	OutSample.Location = FMath::CubicInterp(P0, T0, P1, T1, Alpha);
	// Rotation: linear interp this phase. Hermite on rotation needs angular tangents
	// applied via slerp composition; revisit if rotational jitter becomes prominent.
	OutSample.Rotation = FMath::Lerp(A.State.QuantizedRotation, B.State.QuantizedRotation, Alpha);
	OutSample.Alpha = Alpha;
	OutSample.SegmentDistanceCm = FVector::Distance(P0, P1);
	OutSample.SegmentSeconds = SegmentSeconds;
	return true;
}

void USubMovementComponent::ApplyClientPlaybackPose(const FVector& Location, const FRotator& Rotation)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::None);
	}
```

### 12.15 Flood tick pushes mass


Source: `Source/Sub3D/Submarine/SubFloodComponent.cpp:254-276`

```cpp
void USubFloodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasAuthority(this) || CompartmentStates.Num() == 0)
	{
		return;
	}

	AdvanceFlooding(DeltaTime);
	MaybeLogWaterLevels(DeltaTime);

	// Push current interior water mass to SubMovement as an input. SubMovement
	// declares a tick prereq on us, so the fixed-tick integrator always reads
	// this freshly-advanced value. Kept inside the authority guard — only the
	// server drives the sim and the sub's physics.
	if (ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetFloodImpactKg(GetTotalWaterMassKg());
		}
	}
```

### 12.16 Flood mass conversion


Source: `Source/Sub3D/Submarine/SubFloodComponent.cpp:514-517`

```cpp
float USubFloodComponent::GetTotalWaterMassKg() const
{
	return GetTotalWaterLiters() * WaterDensityKgPerLiter;
}
```

### 12.17 `USubmarineSystemsComponent::TickComponent`


Source: `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:39-52`

```cpp
void USubmarineSystemsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UpdateStabilization(DeltaTime);
	PushPumpStateToHull();
}

void USubmarineSystemsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
```

### 12.18 Helm command setters


Source: `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:60-144`

```cpp
void USubmarineSystemsComponent::SetHelmThrottleCommand(float Value)
{
	CommandState.HelmThrottleCmd = FMath::Clamp(Value, -1.f, 1.f);
	NotifyManualInput(StabilizationAxisSpeed);

	// Absolute command from slider / UI. Cancel any active ramp so the key
	// ramp doesn't fight the widget value on the next stabilization tick.
	ThrottleRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetPowerInput(CommandState.HelmThrottleCmd);
		}
	}
}

void USubmarineSystemsComponent::SetThrottleRampIntent(float Intent)
{
	ThrottleRampIntent = FMath::Clamp(Intent, -1.f, 1.f);

	// While the key is held the player is actively commanding throttle; keep
	// AutoSpeed suspended the same way a slider movement would.
	if (!FMath::IsNearlyZero(ThrottleRampIntent))
	{
		NotifyManualInput(StabilizationAxisSpeed);
	}
}

void USubmarineSystemsComponent::SetRudderRampIntent(float Intent)
{
	RudderRampIntent = FMath::Clamp(Intent, -1.f, 1.f);
	// No NotifyManualInput here because rudder has no auto-equivalent today
	// (no AutoYaw axis in the suspend table). When auto-yaw is added in the
	// future, mirror the throttle pattern.
}

void USubmarineSystemsComponent::SetDivePlaneRampIntent(float Intent)
{
	DivePlaneRampIntent = FMath::Clamp(Intent, -1.f, 1.f);

	// Active player input on the dive plane should suspend AutoPitch the
	// same way moving the trim slider does.
	if (!FMath::IsNearlyZero(DivePlaneRampIntent))
	{
		NotifyManualInput(StabilizationAxisPitch);
	}
}

void USubmarineSystemsComponent::SetHelmYawCommand(float Value)
{
	CommandState.HelmYawCmd = FMath::Clamp(Value, -1.f, 1.f);

	// Absolute set from slider/UI. Cancel any active ramp so the key ramp
	// doesn't fight the widget value on the next stabilization tick.
	RudderRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetRudderInput(CommandState.HelmYawCmd);
		}
	}
}

void USubmarineSystemsComponent::SetHelmTrimCommand(float Value)
{
	CommandState.HelmTrimCmd = FMath::Clamp(Value, -1.f, 1.f);
	CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
	NotifyManualInput(StabilizationAxisPitch);

	// Absolute set from slider/UI. Cancel any active ramp.
	DivePlaneRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetDivePlaneInput(CommandState.HelmTrimCmd);
		}
	}
}
```

### 12.19 Ballast target setter


Source: `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:295-316`

```cpp
void USubmarineSystemsComponent::SetGlobalBallastTarget(float Target)
{
	CommandState.GlobalBallastTarget01 = FMath::Clamp(Target, 0.f, 1.f);
	NotifyManualInput(StabilizationAxisDepth);

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
			Sub->SubMovement->ResyncAllBallasts();
		}
	}
}

void USubmarineSystemsComponent::SetBallastTargetByIndex(int32 Index, float Target)
{
	NotifyManualInput(StabilizationAxisDepth);

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
```

### 12.20 `USubmarineSystemsComponent::UpdateStabilization`


Source: `Source/Sub3D/Submarine/SubmarineSystemsComponent.cpp:418-565`

```cpp
void USubmarineSystemsComponent::UpdateStabilization(float DeltaTime)
{
	SpeedSuspendTimer = FMath::Max(0.f, SpeedSuspendTimer - DeltaTime);
	DepthSuspendTimer = FMath::Max(0.f, DepthSuspendTimer - DeltaTime);
	PitchSuspendTimer = FMath::Max(0.f, PitchSuspendTimer - DeltaTime);

	ASubmarineBase* Sub = ResolveOwnerSubmarine();
	if (!Sub || !Sub->SubMovement)
	{
		return;
	}

	USubMovementComponent* Movement = Sub->SubMovement;

	// Throttle ramp: advance HelmThrottleCmd while the player holds the key.
	// Skipped when auto-speed is active (auto owns the throttle) so the two
	// systems don't fight. Intent is cleared by SetHelmThrottleCommand when
	// the slider takes over.
	if (!IsAutoSpeedActive() && !FMath::IsNearlyZero(ThrottleRampIntent))
	{
		const float Delta = ThrottleRampIntent * ThrottleRampRate * DeltaTime;
		CommandState.HelmThrottleCmd = FMath::Clamp(CommandState.HelmThrottleCmd + Delta, -1.f, 1.f);
		Movement->SetPowerInput(CommandState.HelmThrottleCmd);
	}

	if (IsAutoSpeedActive())
	{
		const float SpeedError = CommandState.TargetSpeedCmS - Movement->ForwardSpeedCmS;
		CommandState.HelmThrottleCmd = FMath::Clamp(SpeedError * AutoSpeedGain, -1.f, 1.f);
		Movement->SetPowerInput(CommandState.HelmThrottleCmd);
	}

	if (IsAutoDepthActive())
	{
		// Target zero vertical velocity rather than a specific depth. When
		// the sub drifts upward (Velocity.Z > 0), add ballast (fill > neutral)
		// to make it sink; when it drifts down, remove ballast. The
		// GlobalBallastTarget01 slider replicates, so every client sees the
		// auto-controller move it in real time — the visible "self-moving
		// slider" the player expects as feedback.
		const float VerticalSpeedCmS = Movement->Velocity.Z;
		const float NeutralFill = FMath::Clamp(Movement->NeutralBuoyancyFill01, 0.f, 1.f);
		const float TargetBallast = FMath::Clamp(
			NeutralFill + VerticalSpeedCmS * AutoDepthVelocityGain,
			0.f, 1.f);

		CommandState.GlobalBallastTarget01 = FMath::FInterpTo(
			CommandState.GlobalBallastTarget01,
			TargetBallast,
			DeltaTime,
			AutoDepthResponseRate
		);

		Movement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
		Movement->ResyncAllBallasts();
	}

	// Dive plane: priority order is AutoPitch > player ramp > auto-recenter.
	if (IsAutoPitchActive())
	{
		const float CurrentPitchDeg = FRotator::NormalizeAxis(Sub->GetActorRotation().Pitch);
		const float PitchErrorDeg = FMath::FindDeltaAngleDegrees(CurrentPitchDeg, CommandState.TargetPitchDeg);
		CommandState.HelmTrimCmd = FMath::Clamp(PitchErrorDeg * AutoPitchPlaneGain, -1.f, 1.f);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}
	else if (!FMath::IsNearlyZero(DivePlaneRampIntent))
	{
		const float Delta = DivePlaneRampIntent * DivePlaneRampRate * DeltaTime;
		CommandState.HelmTrimCmd = FMath::Clamp(CommandState.HelmTrimCmd + Delta, -1.f, 1.f);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}
	else if (!CommandState.bPlaneHoldEnabled)
	{
		CommandState.HelmTrimCmd = FMath::FInterpConstantTo(CommandState.HelmTrimCmd, 0.f, DeltaTime, PlaneReturnRate);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}

	// Rudder: player ramp wins over auto-recenter so holding A/D doesn't get
	// fought by RudderReturnRate. Release the key (Intent=0) and the existing
	// "no hold" auto-recenter resumes.
	if (!FMath::IsNearlyZero(RudderRampIntent))
	{
		const float Delta = RudderRampIntent * RudderRampRate * DeltaTime;
		CommandState.HelmYawCmd = FMath::Clamp(CommandState.HelmYawCmd + Delta, -1.f, 1.f);
		Movement->SetRudderInput(CommandState.HelmYawCmd);
	}
	else if (!CommandState.bRudderHoldEnabled)
	{
		CommandState.HelmYawCmd = FMath::FInterpConstantTo(CommandState.HelmYawCmd, 0.f, DeltaTime, RudderReturnRate);
		Movement->SetRudderInput(CommandState.HelmYawCmd);
	}
}
```

### 12.21 `USubCrewMovementComponent::TickComponent` - REBASE/SIMULATE/EXTRACT


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:144-372`

```cpp
void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!bReceivedMoveInputThisFrame)
	{
		LastMoveIntent = BuildMoveIntent(FVector2D::ZeroVector);
	}
	bReceivedMoveInputThisFrame = false;

	// Lazy authority latch — RISING-EDGE ONLY.
	// Fires only when submarine binding transitions false->true (spawn into sub, replication).
	// After that, EmbarkState is explicit (SetEmbarkState via EnterOnFoot, Board, Disembark,
	// HandleHullCrossing) and the latch must NOT re-set Embarked when the crew is legitimately
	// Outside (EVA) while still holding a sub pointer.
	const bool bHasSubmarineBindingNow = HasSubmarineBinding();
	const bool bJustGainedSubBinding = bHasSubmarineBindingNow && !bHadSubmarineBindingLastTick;

	if (bJustGainedSubBinding && !IsGridAuthoritative() && CharacterOwner)
	{
		if (const ASubmarineBase* Sub = GetCurrentSubmarine())
		{
			const FTransform SubTransform = Sub->GetActorTransform();
			const FVector LocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
			const FRotator WorldRot = CharacterOwner->GetActorRotation();
			const float LocalYaw = FRotator::NormalizeAxis(WorldRot.Yaw - SubTransform.Rotator().Yaw);
			GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
			GridFacingYawDeg = LocalYaw;
			DesiredGridFacingYawDeg = LocalYaw;
			GridFacingYawRateDegPerSec = 0.f;
			bHasGridFacingYaw = true;
			LastSubWorldTransform = SubTransform;
			SetEmbarkState(ECrewEmbarkState::Embarked);

			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("GridAuthority ON (lazy edge) | Sub=%s | LocalPos=%s | LocalYaw=%.2f"),
				*GetNameSafe(Sub),
				*LocalPos.ToCompactString(),
				LocalYaw);
		}
	}
	else if (!bHasSubmarineBindingNow && IsGridAuthoritative())
	{
		// Defensive: lost the sub pointer while still flagged grid-authoritative. Reset to Outside.
		SetEmbarkState(ECrewEmbarkState::Outside);
		ResetGridFacingYaw();
		UE_LOG(LogSubCrewMovement, Log, TEXT("GridAuthority OFF (sub lost) | submarine context ended"));
	}

	bHadSubmarineBindingLastTick = bHasSubmarineBindingNow;

	// Grid-space authority owns yaw while embarked; CMC's base-rotation carry is bypassed.
	bIgnoreBaseRotation = IsGridAuthoritative();
	if (CharacterOwner)
	{
		// While grid-authoritative, actor yaw is SubYaw + GridYaw. The local camera
		// still follows ControlRotation through FPSCamera/TPSCameraBoom, but the
		// Character must not copy ControlRotation back onto ActorYaw between rebases.
		CharacterOwner->bUseControllerRotationYaw = !IsGridAuthoritative();
	}

	// ─── TRACE capture: PRE-REBASE ───
	const bool bTraceMotionChain = IsCrewMotionChainTraceEnabled()
		&& CharacterOwner && CharacterOwner->IsLocallyControlled();
	FMotionChainSnapshot TracePreReb;
	FMotionChainSnapshot TracePostReb;
	FMotionChainSnapshot TracePostCMC;
	FMotionChainSnapshot TracePostExtract;
	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePreReb);
	}

	// ─── LADDER CLIMB (pre-rebase) ───
	// Drives GridSpaceTransform along the active ladder line; the rebase below then
	// sets the capsule to the resulting sub-relative pose. Skipped when not climbing.
	if (CurrentLadder && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		TickLadderClimb(DeltaTime);
	}

	// ─── REBASE (pre-CMC) ───
	// Teleport the capsule to the expected world pose so CMC sees a static world
	// around the character. Must use UpdatedComponent (not SetActorLocation) to
	// avoid triggering overlap/move events before the real CMC tick.
	if (IsGridAuthoritative() && HasSubmarineBinding() && UpdatedComponent && CharacterOwner)
	{
		if (const ASubmarineBase* Sub = GetCurrentSubmarine())
		{
			const FTransform SubTransform = Sub->GetActorTransform();
			UpdateGridFacingYaw(DeltaTime, SubTransform);

			const FVector RebasedWorldPos = SubTransform.TransformPosition(GridSpaceTransform.GetLocation());
			const FRotator LocalRot = GridSpaceTransform.Rotator();
			const FRotator SubRot = SubTransform.Rotator();
			// Yaw-only capsule rotation: the capsule must stay aligned with world gravity
			// so CMC's collision resolution behaves. Pitch/roll of the sub are cosmetic only.
			const FRotator RebasedWorldRot(0.f, FRotator::NormalizeAxis(SubRot.Yaw + LocalRot.Yaw), 0.f);

			UpdatedComponent->SetWorldLocationAndRotation(
				RebasedWorldPos, RebasedWorldRot.Quaternion(),
				/*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

			// This rebase is our authoritative moving-frame transport, not an
			// external displacement that CMC should diagnose on the next
			// PerformMovement call. Without this, CMC sees the sub-carried
			// position delta as bTeleportedSinceLastUpdate every frame while the
			// submarine moves. That forces floor validation/adjustment on moving
			// stair geometry and can create persistent correction jitter.
			LastUpdateLocation = UpdatedComponent->GetComponentLocation();
			LastUpdateRotation = UpdatedComponent->GetComponentQuat();
			bTeleportedSinceLastUpdate = false;

			// Carry the controller yaw by the sub's yaw delta so the locally-controlled
			// view stays anchored relative to the sub.
			if (CharacterOwner->IsLocallyControlled())
			{
				if (AController* C = CharacterOwner->GetController())
				{
					const FRotator PrevSubRot = LastSubWorldTransform.Rotator();
					const float DeltaYaw = FRotator::NormalizeAxis(SubRot.Yaw - PrevSubRot.Yaw);
					if (!FMath::IsNearlyZero(DeltaYaw, KINDA_SMALL_NUMBER))
					{
						FRotator CtrlRot = C->GetControlRotation();
						CtrlRot.Yaw = FRotator::NormalizeAxis(CtrlRot.Yaw + DeltaYaw);
						C->SetControlRotation(CtrlRot);
					}
				}
			}

			LastSubWorldTransform = SubTransform;
		}
	}

	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePostReb);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePostCMC);
	}

	TickPosture(DeltaTime);

	if (HasSubmarineBinding())
	{
		// ─── EXTRACT (post-CMC) ───
		// The CMC has applied input, gravity, and collision resolution in world space.
		// Project the new world pose back into sub-local space; that becomes the
		// authoritative GridSpaceTransform for next frame's rebase.
		if (IsGridAuthoritative() && CharacterOwner)
		{
			if (const ASubmarineBase* Sub = GetCurrentSubmarine())
			{
				const FTransform SubTransform = Sub->GetActorTransform();
				const FVector NewLocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
				const float LocalYaw = bHasGridFacingYaw
					? GridFacingYawDeg
					: GridSpaceTransform.Rotator().Yaw;
				GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), NewLocalPos);
			}
		}

		UpdateRelativeState(DeltaTime);
		UpdateInertialState();
		UpdateSupportState();
		UpdateLocomotionFrame();
		AttemptEmbarkedFloorRecovery(DeltaTime);
		UpdateBraceState();
		UpdateHandIKProbes();
		UpdateFootIKTraces();
		CheckAndLogBaseChange();
		LogPeriodicState(DeltaTime);
		DebugDrawState();

		if (bTraceMotionChain)
		{
			CaptureTraceSnapshot(TracePostExtract);
			EmitMotionChainTrace(TracePreReb, TracePostReb, TracePostCMC, TracePostExtract, DeltaTime);
			TracePrevPostExtract = TracePostExtract;
			bHasTracePrev = true;
		}
	}
	else
	{
		if (LastKnownBase.IsValid())
		{
			const UPrimitiveComponent* PreviousBase = LastKnownBase.Get();
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("Embark ended | clearing tracked base: %s on %s"),
				*GetNameSafe(PreviousBase),
				*GetNameSafe(PreviousBase ? PreviousBase->GetOwner() : nullptr));
		}

		LastKnownBase.Reset();
		LastEmbarkedFloorComponent = nullptr;
		RelativeLinearVelocity = FVector::ZeroVector;
		LocalSubLinearVelocity = FVector::ZeroVector;
		LocalSubLinearAcceleration = FVector::ZeroVector;
		LocalSubAngularVelocityDegrees = FVector::ZeroVector;
		LocalSubAngularAccelerationDegrees = FVector::ZeroVector;
		bHasValidEmbarkedFloor = false;
		bHasAcceptedEmbarkedBase = false;
		bNeedsEmbarkedFloorRecovery = false;
		SupportQuality01 = 0.f;
		bHasNearbyBraceSupport = false;
		NearbyBraceDistanceCm = 0.f;
		NearbyBraceWorldLocation = FVector::ZeroVector;
		NearbyBraceWorldNormal = FVector::ZeroVector;
		BraceQueryOrigin = FVector::ZeroVector;
		bHasPreviousRelativeLocation = false;
		PreviousRelativeLocation = FVector::ZeroVector;
		FloorRecoveryTimer = 0.f;
		DebugLogTimer = 0.f;
		LastSubWorldTransform = FTransform::Identity;
		GridSpaceTransform = FTransform::Identity;
		ResetGridFacingYaw();
	}

	UpdateLocomotionFrame();
	LogMotionChainTick(DeltaTime);
}
```

### 12.22 Crew based-movement and smoothing guards


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:533-590`

```cpp
void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	// In grid-space authority mode the rebase transports the crew; CMC's
	// base-carry must not also apply the base delta or we double-advance.
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// Same rule for rotation: the rebase owns the capsule yaw relative to the sub.
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::UpdateBasedRotation(FinalRotation, ReducedRotation);
}

void USubCrewMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	// Don't seed a MeshTranslationOffset while grid-authoritative. The rebase places the
	// actor at SubTransform × GridSpaceTransform each tick, which intentionally diverges
	// from ReplicatedMovement.Location (= server's world-space pose). If we let CMC build
	// an offset from that delta, SmoothClientPosition decays it toward zero each frame and
	// the mesh oscillates against the rebase → jitter on simulated-proxy peers. No-op in
	// grid mode prevents the offset from ever being created; mesh tracks actor exactly.
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation);
}

bool USubCrewMovementComponent::ServerCheckClientError(
	float ClientTimeStamp,
	float DeltaTime,
	const FVector& Accel,
	const FVector& ClientWorldLocation,
	const FVector& RelativeClientLocation,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	// World-space error check is meaningless when the crew is embarked: client reports
	// SubXf_client_interp * GridSpaceTransform_client, server computes SubXf_server_sim *
	// GridSpaceTransform_server, these ALWAYS differ by the sub interp lag. Bypassing the
	// check trusts the client's reported pose. Safe in cooperative FP; Phase 3.2 replaces
	// this with FSavedMove_Character + local-space validation.
	if (IsGridAuthoritative())
	{
		return false;
	}
	return Super::ServerCheckClientError(
		ClientTimeStamp, DeltaTime, Accel,
		ClientWorldLocation, RelativeClientLocation,
```

### 12.23 Crew custom move trust path


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:834-874`

```cpp
void USubCrewMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);

	// Server-side: after CMC processes the client's move, apply the client's reported
	// grid-space state directly (trust model for FP co-op — production would bound the
	// per-tick delta). The replicated UPROPERTY(COND_SkipOwner) then broadcasts to peers.
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_Authority && !CharacterOwner->IsLocallyControlled())
	{
		if (const FCharacterNetworkMoveData* CurrentMoveData = GetCurrentNetworkMoveData())
		{
			const FCharacterNetworkMoveData_SubCrew* SubMoveData = static_cast<const FCharacterNetworkMoveData_SubCrew*>(CurrentMoveData);
			GridSpaceTransform = SubMoveData->GridSpaceTransform;
			const ECrewEmbarkState ReportedState = static_cast<ECrewEmbarkState>(SubMoveData->EmbarkStateByte);
			if (ReportedState == ECrewEmbarkState::Embarked || ReportedState == ECrewEmbarkState::Transitioning)
			{
				GridFacingYawDeg = FRotator::NormalizeAxis(GridSpaceTransform.Rotator().Yaw);
				DesiredGridFacingYawDeg = GridFacingYawDeg;
				GridFacingYawRateDegPerSec = 0.f;
				bHasGridFacingYaw = true;
			}
			else
			{
				ResetGridFacingYaw();
			}

			if (ReportedState != EmbarkState)
			{
				SetEmbarkState(ReportedState);
			}

			// Handoff event bit: server mirrors the state flip already fired by the client.
			// Velocity blending was applied client-side; we just ensure the server state converges.
			if (SubMoveData->Handoff != ECrewHandoffKind::None)
			{
				UE_LOG(
					LogSubCrewMovement,
					Log,
					TEXT("ServerMove received handoff event | Kind=%d | Crew=%s"),
					static_cast<int32>(SubMoveData->Handoff),
					*GetNameSafe(CharacterOwner));
```

### 12.24 Crew inertial state read from SubMovement


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:886-909`

```cpp
void USubCrewMovementComponent::UpdateInertialState()
{
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	if (!Sub)
	{
		LocalSubLinearVelocity = FVector::ZeroVector;
		LocalSubLinearAcceleration = FVector::ZeroVector;
		LocalSubAngularVelocityDegrees = FVector::ZeroVector;
		LocalSubAngularAccelerationDegrees = FVector::ZeroVector;
		return;
	}

	const FTransform SubXf_Inertia = Sub->GetActorTransform();
	const USubMovementComponent* SubMov_Inertia = Sub->SubMovement;
	const FVector WorldLinVel = SubMov_Inertia ? SubMov_Inertia->Velocity : FVector::ZeroVector;
	const FVector WorldLinAcc = SubMov_Inertia ? SubMov_Inertia->LinearAcceleration : FVector::ZeroVector;
	const FVector WorldAngVelDeg = SubMov_Inertia ? FVector(0.f, SubMov_Inertia->GetPitchRateDegPerSec(), SubMov_Inertia->GetYawRateDegPerSec()) : FVector::ZeroVector;
	const FVector WorldAngAccDeg = SubMov_Inertia ? SubMov_Inertia->AngularAccelerationDeg : FVector::ZeroVector;
	LocalSubLinearVelocity = SubXf_Inertia.InverseTransformVectorNoScale(WorldLinVel);
	LocalSubLinearAcceleration = SubXf_Inertia.InverseTransformVectorNoScale(WorldLinAcc);
	LocalSubAngularVelocityDegrees = SubXf_Inertia.InverseTransformVectorNoScale(WorldAngVelDeg);
	LocalSubAngularAccelerationDegrees = SubXf_Inertia.InverseTransformVectorNoScale(WorldAngAccDeg);
}
```

### 12.25 Crew `InitializeForSubmarine` tick prereq


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:1580-1610`

```cpp
void USubCrewMovementComponent::InitializeForSubmarine()
{
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const bool bFrameValid = Sub && CharacterOwner;

	if (bFrameValid)
	{
		UpdateRelativeState(0.f);
		UpdateInertialState();
	}

	// Tick ordering fix: ensure this CMC ticks AFTER the submarine has moved.
	// Without this, the rebase reads a stale Sub->GetActorTransform() because the sub
	// hasn't simulated yet this frame, causing one-frame-lag jitter.
	bool bSubTickSet = false;

	if (Crew && Crew->CurrentSubmarine)
	{
		if (USubMovementComponent* SubMov = Crew->CurrentSubmarine->SubMovement)
		{
			AddTickPrerequisiteComponent(SubMov);
			bSubTickSet = true;
		}
	}

	LastKnownBase.Reset();
	LastEmbarkedFloorComponent = nullptr;
	RelativeLinearVelocity = FVector::ZeroVector;
	PreviousRelativeLocation = RelativeLocation;
	bHasPreviousRelativeLocation = bFrameValid;
```

### 12.26 Crew replication props


Source: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:1952-1964`

```cpp
void USubCrewMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubCrewMovementComponent, PostureAlpha);
	DOREPLIFETIME(USubCrewMovementComponent, bIsRunning);
	// Owner computes GridSpaceTransform / EmbarkState locally via its own rebase + hull boundary;
	// non-owning clients get the server-authoritative values for their peer-crew rendering.
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, GridSpaceTransform, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, EmbarkState, COND_SkipOwner);
	// Ladder climb state: owner predicts locally, peer SimProxy gets the replicated values.
	DOREPLIFETIME(USubCrewMovementComponent, CurrentLadder);
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, LadderClimbProgress01, COND_SkipOwner);
}
```

### 12.27 `ASubCrewCharacter::OnRep_CurrentSubmarine` and setter


Source: `Source/Sub3D/Submarine/SubCrewCharacter.cpp:475-525`

```cpp
void ASubCrewCharacter::OnRep_CurrentSubmarine()
{
	if (CurrentSubmarine)
	{
		if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
		{
			CrewMov->InitializeForSubmarine();
			CrewMov->RefreshEmbarkedFlooring();
		}
	}
	else
	{
		CurrentCompartment = nullptr;
		CurrentCompartmentId = NAME_None;
		ActiveCompartmentOverlaps.Reset();
		ResetEnvironmentalState();

		if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
		{
			CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
		}
	}
}

void ASubCrewCharacter::SetCurrentSubmarine(ASubmarineBase* Sub)
{
	if (CurrentSubmarine == Sub)
	{
		return;
	}

	// Forbid silent unbind. The only legitimate path that clears CurrentSubmarine is
	// DisembarkSubmarine, which raises bAllowSubmarineUnbind via TGuardValue. Any other
	// caller hitting nullptr here is an upstream bug (replication race, accidental BP wire,
	// sub destruction without disembark) and the ensure surfaces it immediately.
	ensureMsgf(Sub != nullptr || bAllowSubmarineUnbind,
		TEXT("SetCurrentSubmarine(nullptr) called outside DisembarkSubmarine. ")
		TEXT("Crew=%s | PrevSub=%s. Use DisembarkSubmarine for explicit unbind."),
		*GetName(), *GetNameSafe(CurrentSubmarine));

	CurrentSubmarine = Sub;

	if (!CurrentSubmarine)
	{
		CurrentCompartment = nullptr;
		CurrentCompartmentId = NAME_None;
		ActiveCompartmentOverlaps.Reset();
		ResetEnvironmentalState();
	}

	UE_LOG(LogSubCrew, Log, TEXT("SetCurrentSubmarine | Crew=%s | Sub=%s"), *GetName(), *GetNameSafe(CurrentSubmarine));
```

### 12.28 `ASubCrewCharacter::HandleHullCrossing`


Source: `Source/Sub3D/Submarine/SubCrewCharacter.cpp:313-365`

```cpp
void ASubCrewCharacter::HandleHullCrossing(USubHullBoundaryComponent* Boundary, bool bOutgoing)
{
	USubCrewMovementComponent* CrewMov = GetCrewMovement();
	ASubmarineBase* Sub = CurrentSubmarine;
	if (!CrewMov || !Sub)
	{
		return;
	}

	const FTransform SubXf = Sub->GetActorTransform();
	const FVector V_sub_world = Sub->SubMovement ? Sub->SubMovement->Velocity : FVector::ZeroVector;
	const FVector V_crew_world = CrewMov->Velocity;

	if (bOutgoing)
	{
		// Embarked -> Outside: the capsule is already at the correct world pose (rebase put it there).
		// Inject the sub's transport velocity so the crew keeps world momentum continuously.
		CrewMov->Velocity = V_crew_world + V_sub_world;
		CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
		CurrentCompartment = nullptr;

		// FP EVA: no ocean water-volume in the level yet, so force Flying + zero gravity. Once
		// a proper PhysicsVolume (water=true) is added, swap this for MOVE_Swimming with buoyancy.
		CrewMov->SetMovementMode(MOVE_Flying);
		CrewMov->GravityScale = 0.f;

		// Mark the handoff event so FSavedMove_SubCrew captures it into the next move packet;
		// the server mirrors the state flip on receive even if its own boundary missed the crossing.
		CrewMov->SetPendingHandoff(ECrewHandoffKind::Outgoing);
	}
	else
	{
		// Outside -> Embarked: seed GridSpaceTransform from the current world pose and subtract
		// sub velocity so the local-frame velocity reads as "crew motion relative to sub".
		const FVector LocalPos = SubXf.InverseTransformPosition(GetActorLocation());
		const float LocalYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw - SubXf.Rotator().Yaw);
		CrewMov->GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
		CrewMov->LastSubWorldTransform = SubXf;
		CrewMov->Velocity = V_crew_world - V_sub_world;
		CrewMov->SetEmbarkState(ECrewEmbarkState::Embarked);
		// CurrentCompartment is updated by the compartment overlap system when the capsule
		// reaches a UCompartmentVolumeComponent. The boundary's InsideCompartmentId is a hint
		// but not the authority.

		// Restore walking + gravity so the crew lands on the sub floor.
		CrewMov->SetMovementMode(MOVE_Walking);
		CrewMov->GravityScale = 1.f;

		// Mark the handoff event so the server converges on the Outside->Embarked state.
		CrewMov->SetPendingHandoff(ECrewHandoffKind::Incoming);
	}

	UE_LOG(
```

### 12.29 `ASubmarineBase::OnHullHit` - damage uses SubMovement velocity


Source: `Source/Sub3D/Submarine/SubmarineBase.cpp:1070-1147`

```cpp
	{
		return;
	}

	const AActor* HitActor = Hit.GetActor();
	if (HitActor == this
		|| (HitActor && (HitActor->GetOwner() == this || HitActor->GetAttachParentActor() == this || HitActor->IsAttachedTo(this)))
		|| (OtherComp && OtherComp->GetOwner() == this))
	{
		return;
	}

	const FVector ImpactNormal = !Hit.Normal.IsNearlyZero()
		? Hit.Normal.GetSafeNormal()
		: Hit.ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		return;
	}

	const FVector SelfVelocity = SubMovement ? SubMovement->Velocity : GetVelocity();
	const FVector OtherVelocity = OtherComp
		? OtherComp->GetComponentVelocity()
		: (OtherActor ? OtherActor->GetVelocity() : FVector::ZeroVector);
	const FVector RelativeVelocity = SelfVelocity - OtherVelocity;
	const float ApproachSpeedCmS = FMath::Max(0.f, FVector::DotProduct(RelativeVelocity, -ImpactNormal));

	if (ApproachSpeedCmS < HullCollisionDamageMinSpeedCmS)
	{
		if (GetDefault<USub3DDebugSettings>()->bLogSubHullCollisions)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Hull collision ignored | Speed=%.1f cm/s below threshold %.1f | Other=%s"),
				ApproachSpeedCmS,
				HullCollisionDamageMinSpeedCmS,
				*GetNameSafe(OtherActor));
		}
		return;
	}

	const float CatastrophicSpeedCmS = FMath::Max(HullCollisionDamageMinSpeedCmS + 1.f, HullCollisionCatastrophicSpeedCmS);
	const float SpeedAlpha = FMath::Clamp(
		(ApproachSpeedCmS - HullCollisionDamageMinSpeedCmS) / (CatastrophicSpeedCmS - HullCollisionDamageMinSpeedCmS),
		0.f,
		1.f);
	const float SpeedSeverity = FMath::Pow(SpeedAlpha, HullCollisionDamageExponent);
	const float SpeedDamage = HullCollisionDamageAtCatastrophicSpeed * SpeedSeverity;
	const float ImpulseDamage = NormalImpulse.Size() * HullImpactDamageScale;
	const float Damage = FMath::Max(SpeedDamage, ImpulseDamage);

	if (Damage < 1.f)
	{
		return;
	}

	const FVector LocalHitPosition = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);

	if (SubHull)
	{
		SubHull->ApplyHullImpact(LocalHitPosition, Damage, HullImpactRadiusCm);
	}

	// Direct collision→SubFlood when SubHull has no structural sheets (pure generator path).
	if (SubFlood && SubFlood->IsInitialized()
		&& SubHull && SubHull->GetStructuralSheets().Num() == 0
		&& GeneratedDefinition)
	{
		const FGeneratedCompartmentDef* Comp = GeneratedDefinition->FindCompartmentAtLocalLocation(LocalHitPosition);
		if (Comp)
		{
			const float Inflow = FMath::Clamp(
				Damage * DamageToBreachInflowScale,
				0.f,
				GeneratedDefinition->MaxExteriorInflowLitersPerSec);
			SubFlood->CreateBreach(Comp->CompartmentId, Inflow, LocalHitPosition);
		}
```


---

## 13. Schemas

Tick order:

```text
SERVER

  [Systems Tick]  (no explicit prereq with Movement)
        |
        | writes CommandState / Movement setters / Pump state
        v
  [Flood Tick] --prereq--> [SubMovement Tick] --prereq--> [CrewMovement Tick]
      |                       |                              |
      | AdvanceFlooding       | fixed-step while loop        | REBASE
      | SetFloodImpactKg      | ApplyPhysics                 | Super CMC
      |                       | RefreshRepState              | EXTRACT
      v                       v                              v
  flood mass scalar      root pose + RepState             GridSpaceTransform

CLIENT NON-AUTHORITY

  RepState OnRep
      -> Queue/Reset snapshot buffer
      -> Movement Tick EvaluateClientPlaybackPose
      -> ApplyClientPlaybackPose actor root
      -> CrewMovement rebase/extract using current sub transform
```

Force injection:

```text
[Helm input]
  Widget -> Controller RPC -> Systems CommandState -> Movement inputs

[Flood]
  Compartments -> TotalWaterMassKg -> Movement FloodImpactKg -> ComputeTotalMass

[Hull damage]
  Hit/Damage -> Hull damage -> optional Breach -> Flood inflow -> later mass

[Crew]
  Reads Movement Velocity for handoff; no feedback force to sub

[Future]
  Hull breaking / recoil have no direct Movement force API today
```

LGA pipeline:

```text
GridSpaceTransform (sub-local)
      |
      | REBASE
      v
World capsule pose, teleported no sweep
      |
      | SIMULATE: UCharacterMovementComponent
      v
World capsule pose after CMC
      |
      | EXTRACT
      v
GridSpaceTransform updated for replication / next tick
```

Replication:

```text
SERVER
  SubMovement fixed steps
     -> RefreshRepState
     -> FSubmarineNetState
     -> UE actor replication

CLIENT
  OnRep_RepState
     -> HandleReplicatedNetState
     -> QueueClientSnapshot / ResetClientPlayback
  Movement Tick
     -> estimate authority render time
     -> select bracketing snapshots
     -> CubicInterp location, Lerp rotation
     -> SetActorLocationAndRotation
```

---

## 14. Memory files a valider

| Memory | A jour | Verdict |
|---|---|---|
| `project_architecture_revision_2026_03_24.md` | NON | 30 Hz/InteriorFrame/two-snapshot stale. Code actuel: 60 Hz, LGA, no InteriorFrame, buffered playback. |
| `project_stabilization_guards_2026_03_25.md` | PARTIEL | RepNotify/init vrais; tick guard actualise; interp et CMC guard remplaces. |
| `project_helm_cockpit_redesign_2026_04_18.md` | OUI/PARTIEL | route helm existe; fallback direct Movement reste si Systems absent. |
| `project_physics_revision_2026_04_17.md` | OUI/PARTIEL | 60 Hz/profile/math physics confirmes; playback evolue. |
| `project_fluidity_roadmap_2026_04_18.md` | PARTIEL | diagnostic plafond pertinent; code actuel plus pure extrapolation. |
| `project_interior_collision_invariant_2026_04_18.md` | PARTIEL/NON | invariant utile; code actuel garde HullMesh QueryOnly Pawn Block avec proxy. |
| `project_crew_embarked_failure_2026_04_21.md` | NON | pre-LGA, stale. |
| `project_embarked_refactor_rolled_back_2026_04_20.md` | NON | rollback historique, supersede. |
| `project_motion_chain_jitter_root_cause_2026_04_27.md` | OUI | config + warning runtime confirment. |
| `project_gameplay_vision_2026_04_01.md` | VISION NON PROUVEE | cible 1-16, code partiel, pas de runtime proof. |

---

## 15. Performance statique

Movement par fixed substep:

- `SimulateStep` appelle `ApplyCommandState`, `UpdateBallasts`, `ApplyPhysics`.
- `ApplyPhysics` est O(1) hors collision.
- Collision fait jusqu'a deux `MoveWithSlide`: horizontal puis vertical.
- `MoveWithSlide` boucle `MaxSlideIterations`, clamp `1..4`; default observe dans header: 2.
- Borne statique par fixed step: jusqu'a `2 * MaxSlideIterations` sweeps; default 4, max 8.

Sub-stepping:

- `FixedSimulationHz=60.f`, donc `FixedSimDt=0.0166667`.
- Pas de cap explicite du nombre de substeps par render frame.
- Nominal fixed frame 60: 1 substep par frame.
- Hitch/setting modifie: `floor(SimAccumulator / FixedSimDt)` substeps, non borne.

Allocations visibles:

- `TArray<AActor*> AttachedActors` dans `MoveWithSlide`.
- `TArray<FHitResult> Hits` dans chaque iteration sweep.
- `ClientSnapshotBuffer` utilise `TInlineAllocator<8>` et `RemoveAt(..., EAllowShrinking::No)`.
- `AdvanceFlooding` contient des arrays temporaires, mais son interne est hors scope.

Complexite:

| Zone | Complexite | Risque |
|---|---|---|
| Movement sans collision | O(substeps) | faible |
| Movement avec collision | O(substeps * sweeps) | moyen |
| Client playback | O(buffer), buffer court | faible |
| Crew tick | probes/traces/IK/support | a profiler |
| Flood mass push | O(1) hors flood interne | faible |

---

## 16. Test bench / validation

Tests existants trouves par grep:

- Beaucoup de tests authoring/bake/runtime asset dans `Source/Sub3DTests`.
- `SubHullComponentTests.cpp` couvre au moins le seuil de damage collision avec `SubMovement->Velocity`.
- Tests sonar presents.
- Aucun test direct trouve pour `USubMovementComponent::SimulateStep`, snapshot playback, `FSubmarineNetState`, LGA `GridSpaceTransform`, `SetFloodImpactKg`, ou fixed frame dependency.

Runner documente dans `CLAUDE.md`:

```text
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Dev/Sub3D/Sub3D.uproject" -ExecCmds="Automation RunTests Sub3D" -Unattended -NullRHI -NoSound -Log
```

Debug toggles utiles:

| Toggle / CVar | Role |
|---|---|
| `bLogPresentationChain` | master motion-chain logs |
| `bLogMotionChainTrace` | PRE-rebase / POST-rebase / POST-CMC / POST-extract |
| `bLogCrewMovement` | crew logs |
| `bDrawCrewGridAuthority` | HUD validation grid/world/sub pose |
| `bLogCrewJitter` | jitter relative-frame warnings |
| `bLogSubMovement` | sub logs |
| `bLogSubInterpPacing` | dt, dx, alpha/simAcc, snapshot age |
| `bDisableSubRootVisualInterpolation` | kill-switch root interpolation |
| `bLogSubCollisionSweeps` | sweep logs |
| `bLogSubHullCollisions` | hull collision logs |
| `bLogFlood` | flood logs |
| `Sub3D.SubMovement.DisableRootInterpolation` | cvar root interpolation kill-switch |
| `Sub3D.Crew.MotionChainTrace` | cvar crew trace |
| `Sub3D.Crew.DisableCameraSway` | cvar camera sway |

Runtime guardrails:

| Localisation | Guardrail |
|---|---|
| `SubMovementComponent.cpp:118-125` | warning si fixed frame rate desactive |
| `SubCrewCharacter.cpp:510` | ensure si `CurrentSubmarine` cleared hors guard |
| `SubCrewMovementComponent.cpp:373-503` | aggregate/trace motion chain logs |
| `SubFloodComponent.cpp` | warnings init/breach/legacy |

Validation realisee:

- Memories lues.
- Fichiers du scope inspectes.
- `USubInteriorFrameComponent` absent du code source actuel.
- Tick prerequisites `SubFlood -> SubMovement` et `SubMovement -> CrewMovement` verifies.
- `bUseFixedFrameRate=True` et `FixedFrameRate=60` verifies.
- Warning runtime fixed frame rate verifie.
- 5 guards historiques compares au code.
- Consumers motion hors movement greppes.
- Tests existants greppes.

Non verifie:

- Pas de PIE local.
- Pas de multi-client PIE.
- Pas de profiling runtime.
- Pas d'inspection live editor/Blueprint assets.
- Pas de validation de `OwnerActor->GetVelocity()`.
- Pas de validation de jitter camera/crew runtime.
- Pas de validation replication loss/latency.

---

## 17. Conclusions auditables

1. La motion chain actuelle est plus recente que plusieurs memories.
2. La chaine effective est `Flood -> Movement -> Crew`, sans InteriorFrame.
3. Le sub movement est server-authoritative, fixed-step 60 Hz, snapshot custom, playback client retarde avec Hermite position.
4. `UseFixedFrameRate=True` reste une dependance dure: config et warning runtime presents.
5. LGA crew a les guards critiques, mais le trust model est explicitement FP co-op.
6. Hull breaking/recoil n'ont pas de point d'injection force/impulse.
7. Water/camera/FX peuvent lire certains signaux motion, mais pas un presented-state public unifie.
8. Fragilites majeures: no max substeps, Systems tick order non garanti, `OwnerActor->GetVelocity()` consumers, allocations sweep, no impulse API.
9. Aucun test automatique direct ne couvre le coeur fixed-step/playback/LGA.
10. Les plans InteriorFrame archives ne doivent plus guider l'execution actuelle.
