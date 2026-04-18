# Sub3D - Editor Handoff Complet (FP01 Run Shell + Helm/Sonar)

Date: 2026-03-29  
Owner: Runtime/Gameplay handoff  
Statut: Canonique pour preparation PIE de la boucle `Boarding -> Departure -> Traverse -> BreachCrisis -> Approach`

---

## 1) Source de verite (canon)

Conserver cet ordre de priorite:

1. `reports/plans/2026-03-29_sub3d_first_playable_run_spec.md`  
   - Canon du gameplay run (phases, gates, breach, success/failure).
2. `reports/plans/2026-03-29_sub3d_helm_and_sonar_v1_spec.md`  
   - Canon Helm/Sonar V1 (ping, points, reveal/fade, lean).
3. Ce document de handoff  
   - Canon de configuration editeur et de validation PIE.

Si conflit: le run spec + helm/sonar spec gagnent, ce handoff doit etre mis a jour.

---

## 2) Etat technique actuel (a ne pas recasser)

- `ASubGameMode` migre sur `AGameMode` et utilise `ASubGameState`.
- Bootstrap runtime actif:
  - Resolve world
  - Resolve submarine
  - Validate spawn collision
  - Spawn/embark crew
  - Transition `Boot -> Boarding`
- Volumes run actifs:
  - `ASubRunPhaseVolume` (`DepartureGate`, `ApproachZone`)
  - `ASubBreachTriggerVolume` (trigger breach one-shot)
- Sonar runtime actif:
  - `USubSonarComponent` sur `ASubmarineBase`
  - ping server-authority
  - points repliques
  - filtre anti self-hit / anti attached actors
  - sampling cone (spherical fibonacci)
- Widget Helm:
  - bind sonar auto dans `USubHelmWidget`
  - route ping via `RouteSonarPing()`
- Controller:
  - `ServerRouteSonarPing()`
  - lean local `SetSonarLeanActive()`

---

## 3) Preconditions session editeur

1. Ouvrir projet:
   - `c:\Dev\Sub3D\Sub3D.uproject`
2. Map de travail:
   - `Content/Maps/L_FP01_RunShell.umap`
3. World Settings:
   - GameMode Override = `BP_SubGamemode` (derive de `ASubGameMode`)
4. Build C++ propre avant PIE:
   - `Build.bat Sub3DEditor Win64 Development -Project='C:\Dev\Sub3D\Sub3D.uproject' -WaitMutex -NoHotReloadFromIDE`

Important:
- Les coordonnees negatives (X/Y/Z) ne sont pas un probleme en soi pour UE.
- Le referentiel profondeur gameplay doit etre defini metier (pas par signe Y ou Z du monde).

---

## 4) Contrat de level minimal (L_FP01_RunShell)

Le level shell doit contenir au minimum:

1. `BP_Submarine_Compiler` (1 exemplaire)
2. `TraversalRouteActor` (1 exemplaire)
3. `ASubRunPhaseVolume` pour `DepartureGate` (1 exemplaire)
4. `ASubRunPhaseVolume` pour `ApproachZone` (1 exemplaire)
5. `ASubBreachTriggerVolume` (1 exemplaire)

Pas de decoration obligatoire pour la passe runtime. Priorite: collisions + flow.

---

## 5) Setup route (TraversalRouteActor)

Dans Details du `TraversalRouteActor`:

1. Assigner:
   - Route Archetype = `DA_RouteArchetype_FP01Runshell`
   - Route Motif = `DA_RouteMotif_FP04Runshell`
2. Lancer generation route.
3. Lancer `BakeInEditor`.
4. Si besoin de mesh static de debug/visu:
   - lancer `BakeCurrentRouteToStaticMesh`
   - verifier que le mesh resultant n'introduit pas de decalage de transform.

Point critique:
- Le bake static mesh peut diverger de la route runtime si transform/pivot n'est pas aligne.
- Le runtime doit rester cale sur les transforms route actor (`Start/End/StartDock/EndDock`), pas sur une interpretation visuelle du mesh bake.

---

## 6) Setup submarine compiler (collision/spawn)

Dans `BP_Submarine_Compiler` (instance dans la map):

1. Lancer `CompileAndBuild`.
2. Verifier options collision externes:
   - `bUseExteriorCollisionProxy = true`
   - `bPreferGeneratedExteriorMeshCollision = false` (pour stabiliser la phase actuelle)
   - `bDebugLogExteriorCollisionProxy = true` temporairement pendant debug
3. Verifier proxy:
   - composant actif: `ExteriorCollisionProxy`
   - profile collision: `SubmarineHull`
4. Verifier socket spawn:
   - `CrewSpawnSocketP1` positionne a l'interieur du sub, sur surface marchable.

Validation attendue bootstrap:
- `ValidateSpawnCollision()` retourne true (via bootstrap `SubValidated`).
- Crew spawn + movement mode walking + movement base valide.

---

## 7) Setup volumes run

### 7.1 Departure gate

Actor: `ASubRunPhaseVolume`  
Parametres:
- `VolumeType = DepartureGate`
- `bConsumeAfterActivation = true`

Placement:
- Juste apres la zone de depart.
- Le sous-marin doit traverser ce volume quand la sortie dock est effectivement franchie.

Effet attendu:
- `Departure -> Traverse`

### 7.2 Approach zone

Actor: `ASubRunPhaseVolume`  
Parametres:
- `VolumeType = ApproachZone`
- `bConsumeAfterActivation = false` (zone persistante)

Placement:
- Zone finale de destination.

Effet attendu:
- Entry zone: `bSubmarineInApproachZone = true`
- Exit zone: `bSubmarineInApproachZone = false`
- Si phase `Traverse` et pas de breach active: `Traverse -> Approach`

### 7.3 Breach trigger

Actor: `ASubBreachTriggerVolume`  
Parametres:
- `bConsumeAfterTrigger = true`

Placement:
- A un point milieu de parcours (pas trop proche du depart).

Effet attendu:
- Trigger one-shot sur overlap sous-marin
- `Traverse -> BreachCrisis`

---

## 8) Wiring Blueprint Helm + Sonar (editeur)

## 8.1 Inputs (assets)

Etat observe:
- `IMC_Helm` existe
- `IA_SonarPing` et `IA_SonarLean` ne sont pas encore presents dans `Content/Sub3D/Input`

A faire:
1. Creer `IA_SonarPing` (Digital)
2. Creer `IA_SonarLean` (Digital Hold)
3. Ajouter dans `IMC_Helm`:
   - `Space` -> `IA_SonarPing`
   - `RightMouseButton` (ou autre touche) -> `IA_SonarLean`

## 8.2 Controller Blueprint (`PC_SubPlayerController`)

Dans Event Graph:

1. Bind `IA_SonarPing (Triggered)`:
   - appeler `ServerRouteSonarPing()`
2. Bind `IA_SonarLean`:
   - `Started` -> `SetSonarLeanActive(true)`
   - `Completed` + `Canceled` -> `SetSonarLeanActive(false)`
3. Implementer `BP_OnSonarLeanChanged(bool)`:
   - camera offset/fov local
   - overlay fullscreen sonar on/off
   - ne pas changer `CurrentControlMode`

## 8.3 Helm widget (`WBP_SubHelm`)

1. S'assurer que le sous-widget sonar est present (derive `USubSonarDisplayWidget`).
2. Renseigner la reference `SonarDisplay` attendue par `USubHelmWidget`.
3. Le bind C++ fait le reste (`TryBindSonarDisplay()` en tick/construct).

---

## 9) Lighting/Fog pour shell lisible (sans art pass)

Objectif: lisibilite PIE immediate, pas rendu final.

Setup recommande:

1. Ajouter un `PostProcessVolume` (Infinite Extent = true)
   - Exposure fixe (desactiver auto)
   - leger tint bleu/vert
   - contraste modere
2. Optionnel: `ExponentialHeightFog`
   - densite faible a moyenne
   - utiliser seulement si comportement stable avec ton setup monde
3. Eclairage minimal:
   - 1 Skylight faible
   - 1 Directional Light tres faible
   - sources internes dans le sub si besoin

Decision pratique:
- Si fog cree des artefacts dans ta scene, garder PostProcess seulement pour la passe gameplay.

---

## 10) Protocole PIE (passage obligatoire)

## 10.1 Sequence cible

1. Lancer PIE (1 joueur)
2. Verifier bootstrap:
   - phase passe a `Boarding`
   - crew embarque correctement dans le sub
3. Entrer au helm
4. Trigger `BeginDeparture` (UI bouton ou input existant)
5. Franchir `DepartureGate` -> `Traverse`
6. Trigger breach volume -> `BreachCrisis`
7. Stabiliser breach -> retour `Traverse` ou `Approach` selon zone
8. Entrer zone approach -> `Approach`

## 10.2 Logs a surveiller

- `LogSubRun` (phases + bootstrap + departure collision snapshot)
- `LogSubRunVolume` (volumes)
- `LogSubBreachTrigger` (trigger breach)
- `LogSubCrew` (spawn/base/movement mode)
- `LogSubController` (routing helm/sonar)
- `LogSonar` (ping accepted/rejected + points)
- `CollisionSweep` (si `bDebugLogCollisionSweeps=true`)

---

## 11) Checklist acceptance "Editor Ready"

Le level shell est considere pret si:

1. Save map < 30s (pas de regen PMCs involontaire au save)
2. Crew spawn stable, marche sur sol interieur (pas de chute)
3. Sous-marin collision tunnel valide (pas de push lateral parasite au `BeginDeparture`)
4. `DepartureGate` declenche `Departure -> Traverse`
5. `ASubBreachTriggerVolume` declenche exactement une fois
6. Stabilisation breach declenche sortie `BreachCrisis`
7. Helm widget affiche valeurs dynamiques (pas bloque a zero)
8. Sonar ping routable depuis helm (meme si polish visuel incomplet)

---

## 12) Problemes connus (etat 2026-03-29)

1. Automation commandline UE:
   - crash observe: `GenericWindow.cpp:113` (`GetRestoredDimensions...`)
   - impact: la validation automation sonar en `UnrealEditor-Cmd` n'est pas fiable dans cet environnement.
2. Docking V1:
   - transitions enum presentes
   - logique runtime docking detaillee non encore branchee dans le flow shell.
3. Inputs sonar:
   - `IA_SonarPing`/`IA_SonarLean` assets a creer et brancher dans `IMC_Helm`.

---

## 13) Plan de reprise recommande (ordre strict)

1. Stabiliser level shell (actors + collisions + volumes) et valider PIE phase flow.
2. Finaliser wiring input helm sonar en Blueprint.
3. Valider loop `Boarding -> Departure -> Traverse -> BreachCrisis -> Approach`.
4. Ensuite seulement: attaquer Docking V1 runtime.

Ne pas melanger correction flow runtime et polish visuel tant que la boucle n'est pas fiable.

---

## 14) Fichiers C++ pivots a garder sous surveillance

- `Source/Sub3D/Submarine/SubGameMode.h`
- `Source/Sub3D/Submarine/SubGameMode.cpp`
- `Source/Sub3D/Submarine/SubGameState.h`
- `Source/Sub3D/Submarine/SubGameState.cpp`
- `Source/Sub3D/Submarine/SubRunPhaseVolume.h`
- `Source/Sub3D/Submarine/SubRunPhaseVolume.cpp`
- `Source/Sub3D/Submarine/SubBreachTriggerVolume.h`
- `Source/Sub3D/Submarine/SubBreachTriggerVolume.cpp`
- `Source/Sub3D/Submarine/SubPlayerController.h`
- `Source/Sub3D/Submarine/SubPlayerController.cpp`
- `Source/Sub3D/Submarine/SubHelmWidget.h`
- `Source/Sub3D/Submarine/SubHelmWidget.cpp`
- `Source/Sub3D/Submarine/SubSonarComponent.h`
- `Source/Sub3D/Submarine/SubSonarComponent.cpp`
- `Source/Sub3D/Submarine/SubSonarDisplayWidget.h`
- `Source/Sub3D/Submarine/SubSonarDisplayWidget.cpp`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.h`
- `Source/Sub3D/SubCompiler/SubmarineCompilerActor.cpp`

---

## 15) Definition of Done du handoff editeur

Ce handoff est considere execute quand:

1. Le niveau `L_FP01_RunShell` suit le contrat de section 4.
2. Le flow PIE de section 10 passe sans blocage de phase.
3. Les collisions sub + crew sont stables.
4. Le wiring helm/sonar est en place cote Blueprint.
5. Les regressions sont tracables via les logs cites.

