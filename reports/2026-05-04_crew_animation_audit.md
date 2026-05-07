# Audit Crew + Character + Animation - Sub3D

Date: 2026-05-04  
Branche observee: `water-proto`  
Workspace: `C:\Dev\Sub3D`  
Mode: audit statique uniquement, aucun fix applique.  
Plan de reference prioritaire: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`.

## 0. Methode et limites

### Donnees inspectees

- Code C++ dans `Source/Sub3D/Submarine/`.
- Assets binaires via recherche de chaines dans `.uasset`.
- FBX source `Art/Characters/Export/SK_Crew_Basic.fbx`.
- Scripts Blender/UE5 lies au crew character.
- Plans et guides existants autour du character, du Local Grid Authority, de l'axis environment et de l'animation procedurale.

### Donnees non inspectees en live

- UnrealClaude MCP n'etait pas expose dans cette session. Aucun outil `mcp__unrealclaude__*` n'etait disponible.
- L'editeur Unreal n'a pas ete interroge en live.
- Le graphe exact de `ABP_Crew` n'a pas pu etre decompile en noeuds lisibles. Les informations AnimBP viennent des chaines binaires de l'asset et du C++ parent.
- Les transforms exacts de composants dans `BP_SubmarineCrew` n'ont pas pu etre lus avec certitude.
- Les import options completes UE pour le mesh/skeleton ne sont pas entierement lisibles par statique binaire.
- Le symptome "bras penches a droite" n'a pas ete reproduit en PIE pendant cet audit.

### Niveau de confiance

| Sujet | Confiance | Base |
|---|---:|---|
| C++ crew/movement/animation | Haute | fichiers texte inspectes |
| Skeleton et mesh metadata | Moyenne | chaines `.uasset` + FBX |
| AnimBP states et native parent | Moyenne | chaines `.uasset` |
| AnimGraph exact | Faible | asset binaire, a ouvrir en editeur |
| Cause finale bras penches | Moyenne-faible | plusieurs contributeurs probables, pas de verification PIE |

---

## 1. Vue d'ensemble - resume executif

- Le systeme crew est un prototype avance, pas un pipeline animation de production.
- Le mouvement interieur suit l'architecture Local Grid Authority: le sous-marin est le frame mobile, le crew stocke une pose locale, puis est rebase en monde chaque tick.
- Le systeme animation est majoritairement procedural. Il n'y a pas de set clair d'Animation Sequences, BlendSpaces ou Montages trouve sous `Content/Sub3D/Characters/Animation/`, hors `ABP_Crew.uasset`.
- `ABP_Crew` herite de `USubCrewAnimInstance` et reference `SK_Crew_Basic_Skeleton`.
- `USubCrewAnimInstance` calcule des rotations par bone pour walk, run, crouch, prone/crawl, swim, breathing, posture, sub motion, aim, hand IK et foot IK.
- `FAnimNode_CrewProcedural` applique une partie de ces rotations en C++ sur 18 bones.
- Le symptome des bras penches a droite est compatible avec un mismatch d'axes Blender/FBX/UE et/ou avec les rotations de repos hardcodees des bras.
- Les bras recoivent une pose de repos procedurale chaque frame hors swim: `ArmRestR = Roll -85`, `ArmRestL = Roll +85`, forearms `-10/+10`.
- Les clavicles et les hands ne sont pas modifies par le node procedural. Les bras dependent donc fortement de l'orientation importee des bones clavicle/upperarm/lowerarm.
- Les IK existent en donnees C++, mais l'application finale en AnimGraph doit etre validee en editeur. Le node procedural applique la translation pelvis, pas les offsets foot IK.
- Les inputs sont partages entre pawn crew et `PC_SubPlayerController`. Le user a raison: `ApplyCrewPlanarMoveInput` est encore expose dans `ASubCrewCharacter` et `USubCrewMovementComponent`, et `BP_SubmarineCrew` contient des references a `IA_Move`, `IA_Run`, `IA_PostureScroll`.
- `ECrewControlMode` ne contient pas `Swimming` actuellement. Il contient `OnFoot`, `HelmDriving`, `StationUI`.
- La partie network du grid movement privilegie la stabilite FP co-op: `ServerCheckClientError` ignore l'erreur monde en grid mode et le serveur truste le client sur `GridSpaceTransform`.
- La dette visible principale: pas d'ocean swim reel, pas de smoothing peer sur `OnRep_GridSpaceTransform`, posture sans clearance test, split input PC/pawn, AnimBP non auditable statiquement.

### Maturite

| Sous-systeme | Maturite observee | Commentaire |
|---|---|---|
| Local Grid Authority | WIP solide | architecture claire, logs, tick phases explicites |
| Crew environment axis | WIP fonctionnel | compartments, water immersion, pressure stubs |
| Crew animation | Prototype | beaucoup de procedural direct, peu d'assets anim auteur |
| IK | Prototype incomplet | probes et states presents, application finale incertaine |
| Swimming | Stub partiel | flood swim local, EVA ocean via Flying/zero gravity |
| Input routing | Transition incomplete | PlayerController et pawn se partagent encore des actions |
| Replication crew | FP co-op | trust client et skip owner assumés |

---

## 2. Pipeline asset character - Blender -> UE

### Assets principaux

| Role | Chemin |
|---|---|
| Skeletal mesh | `Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic.uasset` |
| Skeleton | `Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic_Skeleton.uasset` |
| Physics asset | `Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic_PhysicsAsset.uasset` |
| AnimBP | `Content/Sub3D/Characters/Animation/ABP_Crew.uasset` |
| Crew pawn BP | `Content/Sub3D/Blueprint/PlayerBP/BP_SubmarineCrew.uasset` |
| Source FBX | `Art/Characters/Export/SK_Crew_Basic.fbx` |
| Source Blender natif reference dans FBX | `C:\Users\coren\Documents\Blender\.blend\CharacterSubCrew.blend` |

### Skeleton

Informations lues dans `SK_Crew_Basic_Skeleton.uasset`:

- Asset type: `Skeleton`.
- Preview mesh: `/Game/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic`.
- Root armature: `Armature_Crew`.
- Bone count lu cote mesh: `27`.
- `AnimNotifyList` vide d'apres chaines binaires.
- `AnimSyncMarkerList` vide d'apres chaines binaires.
- `CompatibleSkeletonList` vide d'apres chaines binaires.
- Retargeting mode string visible: `EBoneTranslationRetargetingMode::Animation`.

### Hierarchie bones observee

Ordre et parents lues dans le binaire skeleton:

```text
Armature_Crew
root                         parent -1
pelvis                       parent root
spine_01                     parent pelvis
spine_02                     parent spine_01
spine_03                     parent spine_02
spine_04                     parent spine_03
spine_05                     parent spine_04
neck_01                      parent spine_05
neck_02                      parent neck_01
head                         parent neck_02
clavicle_r                   parent spine_05
upperarm_r                   parent clavicle_r
lowerarm_r                   parent upperarm_r
hand_r                       parent lowerarm_r
clavicle_l                   parent spine_05
upperarm_l                   parent clavicle_l
lowerarm_l                   parent upperarm_l
hand_l                       parent lowerarm_l
thigh_r                      parent pelvis
calf_r                       parent thigh_r
foot_r                       parent calf_r
ball_r                       parent foot_r
thigh_l                      parent pelvis
calf_l                       parent thigh_l
foot_l                       parent calf_l
ball_l                       parent foot_l
```

### Mesh

Informations lues dans `SK_Crew_Basic.uasset`:

| Propriete | Valeur observee |
|---|---:|
| Asset type | `SkeletalMesh` |
| Bones | 27 |
| LODs | 1 |
| Triangles | 14844 |
| Vertices | 11294 |
| MaxBoneInfluences | 6 |
| MorphTargets | 0 |
| NaniteEnabled | False |
| DNA | No DNA Attached |
| PhysicsAsset | `SK_Crew_Basic_PhysicsAsset` |
| Skeleton | `SK_Crew_Basic_Skeleton` |
| Import content type | `All` |

### Materials assets

| Asset | Role probable |
|---|---|
| `Content/Sub3D/Characters/Materials/M_Crew_Master.uasset` | master material cree par script |
| `MI_Crew_Skin.uasset` | skin |
| `MI_Crew_Suit.uasset` | suit |
| `MI_Crew_SuitDk.uasset` | suit dark |
| `MI_Crew_Boot.uasset` | boot |
| `MI_Crew_Accent.uasset` | accent |
| `Content/Sub3D/Characters/Meshes/Bodies/M_Skin.uasset` | material importe FBX |
| `M_Suit.uasset` | material importe FBX |
| `M_SuitDk.uasset` | material importe FBX |
| `M_Boot.uasset` | material importe FBX |
| `M_Accent.uasset` | material importe FBX |
| `M_Hair.uasset` | material importe FBX, pas d'instance MI correspondante trouvee |

Le script `Scripts/UE5/create_crew_materials.py` indique cinq material instances attendues:

```text
[0] MI_Crew_Skin
[1] MI_Crew_Suit
[2] MI_Crew_SuitDk
[3] MI_Crew_Boot
[4] MI_Crew_Accent
```

Le script Blender `Scripts/Blender/apply_crew_materials.py` declare pourtant six definitions: `M_Skin`, `M_Suit`, `M_SuitDk`, `M_Boot`, `M_Hair`, `M_Accent`.

Conclusion statique: le pipeline Blender connait un slot hair, le pipeline UE material instances ne cree pas `MI_Crew_Hair`. L'impact visuel exact depend des slots reellement assignes dans le mesh.

### Source FBX

Chemin: `Art/Characters/Export/SK_Crew_Basic.fbx`

Chaine binaire observee:

- Creator: `Blender (stable FBX IO) - 5.0.1 - 5.14.0`.
- Original native file: `C:\Users\coren\Documents\Blender\.blend\CharacterSubCrew.blend`.
- `Armature_Crew` present.
- `SM_Crew` present.
- Bones nommes comme le skeleton UE.

GlobalSettings FBX observes:

```text
UpAxis = 2
UpAxisSign = 1
FrontAxis = 1
FrontAxisSign = 1
CoordAxis = 0
CoordAxisSign = -1
UnitScaleFactor = 1
```

Le script export FBX `Scripts/Blender/export_crew_fbx.py` utilise:

```python
bpy.ops.export_scene.fbx(
    filepath=filepath,
    use_selection=True,
    apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y',
    axis_up='Z',
    object_types={'ARMATURE', 'MESH'},
    use_armature_deform_only=True,
    add_leaf_bones=False,
    mesh_smooth_type='FACE',
    use_mesh_modifiers=True,
    bake_anim=False,
)
```

### Import settings UE lisibles statiquement

Dans `SK_Crew_Basic.uasset`, l'AssetImportData reference:

```json
{
  "RelativeFilename": "../../../../../Art/Characters/Export/SK_Crew_Basic.fbx",
  "Timestamp": "1776145869",
  "FileMD5": "7d47e477eabeed098d26d2627ca95fce"
}
```

Options Interchange visibles:

```text
bBakePivotMeshes = false
bUseT0AsRefPose = false
bImportSkeletalMeshes = true
bUpdateSkeletonReferencePose = false
bImportAnimations = true
bImportMaterials = true
```

Options non confirmees statiquement:

- `bForceFrontXAxis`.
- `bConvertScene`.
- `bConvertSceneUnit`.
- `Bake Pivot in Vertex`.
- Retarget base pose exacte.

### Risque axes import

Le FBX contient des transforms locales non triviales sur le root et les bones de bras. Exemples observes dans le binaire:

- `root` a `Lcl Rotation` non nul.
- `root` a `Lcl Scaling` avec signes negatifs visibles.
- `clavicle_r`, `upperarm_r`, `clavicle_l`, `upperarm_l` ont des `Lcl Rotation` visibles.
- `upperarm_l` a une `Lcl Scaling` avec signe negatif visible.

Interpretation: ce n'est pas une preuve de bug a elle seule, mais c'est un contributeur credible au symptome "bras penches" si les rotations procedurales supposent un espace local symetrique et propre.

---

## 3. Skeleton + animations

### Assets animation trouves

Dans `Content/Sub3D/Characters/Animation/`:

| Asset | Type probable | Note |
|---|---|---|
| `ABP_Crew.uasset` | AnimBlueprint | seul asset animation trouve dans ce dossier |

Aucun asset statique trouve dans ce dossier avec nom typique:

- `AnimSequence`
- `BlendSpace`
- `AnimMontage`
- `AimOffset`
- `ControlRig`
- `IKRig`
- `IKRetargeter`

Conclusion: le pipeline actuel semble majoritairement procedural, avec un AnimBP qui pilote ou appelle le C++ procedural.

### ABP_Crew - metadata binaire

Chaine binaire observee:

- Asset path: `/Game/Sub3D/Characters/Animation/ABP_Crew`.
- Native parent: `SubCrewAnimInstance`.
- Generated class: `ABP_Crew_C`.
- Target skeleton: `/Game/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic_Skeleton`.
- State machine: `New State Machine`.
- States visibles: `Idle`, `Walk`, `Swim`.
- Transitions visibles: 6 transition nodes.
- Node custom visible: `AnimGraphNode_CrewProcedural`.
- Runtime node visible: `AnimNode_CrewProcedural`.
- `Default__SubCrewAnimInstance` present.
- `PreviewSkeletalMesh None` observe dans les chaines du skeleton; a verifier dans l'ABP.

### Animations disponibles

Liste exhaustive observee dans le scope character:

| Nom | Chemin | Type | Duree | Source |
|---|---|---|---|---|
| `ABP_Crew` | `Content/Sub3D/Characters/Animation/ABP_Crew.uasset` | AnimBlueprint | non applicable | UE asset |

Non observe:

- Walk sequence.
- Run sequence.
- Crouch walk sequence.
- Prone crawl sequence.
- Swim sequence.
- Idle additive sequence.
- Montage station.
- Montage interact.
- Montage repair.
- Montage ladder.
- Notify state.

### Retargeting

Retarget source non identifiee. Aucun autre skeleton source n'a ete trouve dans `Content/Sub3D/Characters/` pour un retarget mannequin -> crew. Le skeleton est custom Blender.

Zones a valider en editeur:

- Skeleton tree: verifier chaque bone roll visuellement.
- Retargeting Options: verifier translation retargeting par bone.
- Retarget base pose: verifier T-pose vs A-pose.
- AnimBP class settings: verifier post-process AnimBP absent/present.

---

## 4. Animation Blueprint

### ABP en jeu

`BP_SubmarineCrew.uasset` reference:

- `SK_Crew_Basic`.
- `ABP_Crew`.
- `MI_Crew_*`.

Donc l'ABP en jeu attendu est:

```text
Content/Sub3D/Characters/Animation/ABP_Crew.uasset
Native parent: USubCrewAnimInstance
Skeleton: SK_Crew_Basic_Skeleton
```

### State machine visible

Etat lisible depuis chaines binaires:

```text
New State Machine
  Idle
  Walk
  Swim
  6 transitions
```

Conditions exactes non lisibles statiquement. Hypothese a confirmer:

- `Idle` quand `bIsMoving == false && bIsSwimming == false`.
- `Walk` quand `bIsMoving == true && bIsSwimming == false`.
- `Swim` quand `bIsSwimming == true`.

### Variables exposees par `USubCrewAnimInstance`

Source: `Source/Sub3D/Submarine/SubCrewAnimInstance.h`.

Input state:

| Variable | Type | Category | Role |
|---|---|---|---|
| `Speed` | float | Crew/Input | vitesse 2D |
| `Direction` | float | Crew/Input | direction anim |
| `bIsMoving` | bool | Crew/Input | marche/idle |
| `bIsSwimming` | bool | Crew/Input | swim |
| `bIsRunning` | bool | Crew/Input | sprint |
| `PostureAlpha` | float | Crew/Input | 0 prone, 0.5 crouch, 1 standing |
| `UpperBodyYawOffset` | float | Crew/Input | twist upper body |
| `SubPitchDeg` | float | Crew/Input | nom trompeur, lit angular velocity Y |
| `SubRollDeg` | float | Crew/Input | nom trompeur, lit angular velocity X |
| `SubAccelForward` | float | Crew/Input | accel locale X |
| `SubAccelLateral` | float | Crew/Input | accel locale Y |
| `SupportQuality` | float | Crew/Input | floor support |
| `LocalTurnRateDegPerSec` | float | Crew/Input | yaw rate local |
| `LocomotionState` | `FCrewAnimLocomotionState` | Crew/Locomotion | bundle anim |
| `MoveIntent` | `FCrewMoveIntent` | Crew/Locomotion | input intent |
| `LocomotionFrame` | `FCrewLocomotionFrame` | Crew/Locomotion | frame mouvement |
| `Stance` | `ECrewLocomotionStance` | Crew/Locomotion | posture anim |
| `Gait` | `ECrewLocomotionGait` | Crew/Locomotion | gait anim |

Bone outputs:

| Variable | Bone cible |
|---|---|
| `Proc_Pelvis_Rot` | pelvis |
| `Proc_Pelvis_Offset` | pelvis translation |
| `Proc_Spine01_Rot` | spine_01 |
| `Proc_Spine02_Rot` | spine_02 |
| `Proc_Spine03_Rot` | spine_03 |
| `Proc_Spine04_Rot` | spine_04 |
| `Proc_Spine05_Rot` | spine_05 |
| `Proc_Neck01_Rot` | neck_01 |
| `Proc_Head_Rot` | head |
| `Proc_ThighR_Rot` | thigh_r |
| `Proc_ThighL_Rot` | thigh_l |
| `Proc_CalfR_Rot` | calf_r |
| `Proc_CalfL_Rot` | calf_l |
| `Proc_FootR_Rot` | foot_r |
| `Proc_FootL_Rot` | foot_l |
| `Proc_UpperarmR_Rot` | upperarm_r |
| `Proc_UpperarmL_Rot` | upperarm_l |
| `Proc_LowerarmR_Rot` | lowerarm_r |
| `Proc_LowerarmL_Rot` | lowerarm_l |

Bones non modifies par le node procedural C++ actuel:

- `root`
- `clavicle_r`
- `clavicle_l`
- `neck_02`
- `hand_r`
- `hand_l`
- `ball_r`
- `ball_l`

### Graph node custom

`ABP_Crew.uasset` contient `AnimGraphNode_CrewProcedural`. Le C++ correspondant:

- `Source/Sub3D/Submarine/AnimGraphNode_CrewProcedural.h`
- `Source/Sub3D/Submarine/AnimNode_CrewProcedural.h`
- `Source/Sub3D/Submarine/AnimNode_CrewProcedural.cpp`

Le node custom applique une pose de base puis ajoute les rotations stockees dans le snapshot C++.

---

## 5. Animations procedurales / IK

### Pipeline update

Source: `Source/Sub3D/Submarine/SubCrewAnimInstance.cpp:45`.

Ordre logique de `NativeUpdateAnimation`:

```text
Reset proc rotations and offsets
ReadInputState()
if bIsSwimming:
  ComputeSwimCycle()
else:
  ComputeWalkCycle()
  ComputeBreathing()
ComputePosture()
ComputeSubMotion()
ComputeUpperBodyAim()
ComputeHandIK()
ComputeFootIK()
if not swimming:
  add ArmRestR/L and ForearmRestR/L
```

### Locomotion walk/run

Source: `SubCrewAnimInstance.cpp:170`.

Le walk est un cycle sinusoidal base sur distance parcourue:

- `WalkLegSwingDeg = 30`
- `WalkArmSwingDeg = 20`
- `WalkPelvisBobCm = 2`
- `WalkCycleRate = 0.04`
- `WalkCalfBendMultiplier = 1.2`
- `WalkSpineLeanDeg = 3`

Le run est un blend quand vitesse et `bIsRunning` montent:

- `RunSpeedThreshold = 400`
- `RunMaxSpeed = 600`
- `RunLegSwingDeg = 50`
- `RunArmSwingDeg = 35`
- `RunPelvisBobCm = 4`
- `RunCycleRate = 0.06`
- `RunSpineLeanDeg = 8`

Observation critique: ce n'est pas une locomotion authored. C'est une approximation sinusoidale. Pour un gait credible, le systeme manque de:

- contacts de pied authorés ou calcules robustement;
- phase de double support;
- pelvis compensation laterale;
- rotation de foot derivee de pente;
- separation locomotion body/weapon/station;
- transitions authored ou courbes.

### Crouch et prone

Source: `SubCrewAnimInstance.h` et `.cpp`.

Posture scalar:

- `PostureAlpha = 0`: prone.
- `PostureAlpha = 0.5`: crouched.
- `PostureAlpha = 1`: standing.

Tuning crouch:

- `CrouchPelvisDropCm = 10`
- `CrouchThighFoldDeg = 26`
- `CrouchCalfFoldDeg = 42`
- `CrouchFootCompDeg = 14`
- `CrouchArmForwardDeg = 10`
- `CrouchElbowBendDeg = 18`

Tuning crawl:

- `CrawlCycleRate = 0.02`
- `CrawlLegSwingDeg = 18`
- `CrawlArmSwingDeg = 22`
- `CrawlPelvisBobCm = 1.5`
- `CrawlThighFoldDeg = 58`
- `CrawlCalfFoldDeg = 84`
- `CrawlFootCompDeg = 20`
- `CrawlArmForwardDeg = 22`
- `CrawlElbowBendDeg = 52`

Dette: posture physique et posture animation sont scalar-first. Il n'y a pas de vrai state machine stance authorée. Le CMC change capsule half-height avec sweep sur `SetCapsuleHalfHeight`, mais il n'y a pas de clearance test avant grow.

### Swim

Source: `SubCrewAnimInstance.cpp:466`.

Swim procedural:

- `SwimStrokeRate = 0.8`
- `SwimArmSwingDeg = 45`
- `SwimKickDeg = 30`
- Body lean via `MakeAxisRotator(60, SpineBendAxis, bNegateSpineBend)`.
- Neck/head counter rotation.
- Arms sweep symetrique.
- Legs flutter kick.
- Pelvis bob.

Limite: le swim est une animation procedural simple, pas liee a un vrai mode swimming global. `bIsSwimming` cote anim vient de `LocomotionFrame.bIsSwimming || Crew->bIsSwimmingByFlood`.

Source movement: `SubCrewMovementComponent.cpp:705` met `bSwimming` via `Crew->bIsSwimmingByFlood`.

Conclusion: le systeme detecte surtout le flood swim, pas un vrai control mode swimming exterieur.

### Axes remapping

Source: `SubCrewAnimInstance.h`.

Commentaire C++ explicite:

```text
AXIS REMAPPING (fix Blender->UE bone axis mismatch)
Swing = leg/arm forward-back motion
Twist = rotation around bone's long axis
Values: 0=Pitch, 1=Yaw, 2=Roll
```

Valeurs par defaut:

| Param | Default | Commentaire |
|---|---:|---|
| `LegSwingAxis` | 1 | Yaw |
| `ArmSwingAxis` | 1 | Yaw |
| `ElbowBendAxis` | 2 | Roll |
| `SpineBendAxis` | 1 | Yaw |
| `SpineTwistAxis` | 2 | Roll |
| `bNegateLegSwing` | false | |
| `bNegateArmSwing` | false | |
| `bNegateSpineBend` | false | |

Observation critique: la presence de ces remaps confirme que le pipeline compense deja un mismatch bone axes. Ce n'est pas mauvais en soi, mais c'est fragile pour un rig character.

### Arm rest pose

Source: `SubCrewAnimInstance.h` et `.cpp:108`.

Defaults:

```cpp
FRotator ArmRestR = FRotator(0.f, 0.f, -85.f);
FRotator ArmRestL = FRotator(0.f, 0.f, 85.f);
FRotator ForearmRestR = FRotator(0.f, 0.f, -10.f);
FRotator ForearmRestL = FRotator(0.f, 0.f, 10.f);
```

Applique chaque frame si `!bIsSwimming`:

```cpp
Proc_UpperarmR_Rot += ArmRestR;
Proc_UpperarmL_Rot += ArmRestL;
Proc_LowerarmR_Rot += ForearmRestR;
Proc_LowerarmL_Rot += ForearmRestL;
```

Impact sur bug bras: fort contributeur possible. Si les local axes UE des upperarms ne sont pas strictement miroir, `Roll -85/+85` ne donne pas des bras symetriques et peut pencher les deux bras dans une direction visuelle commune.

### Upper body aim

Source: `SubCrewAnimInstance.cpp:412`.

Logique observee:

- Lit `Crew->GetControlRotation()`.
- Calcule yaw/pitch camera.
- Applique un twist upper/lower body sous conditions de mouvement.
- Applique un head pitch depuis control pitch.

Risque: animation upper-body depend du control rotation. Cela ne devrait pas empecher le move maintenant si `BuildMoveIntent` utilise seulement yaw, mais l'apparence du haut du corps depend encore de camera/controller.

### Hand IK

Source C++:

- Probes dans `USubCrewMovementComponent::UpdateHandIKProbes` (`SubCrewMovementComponent.cpp:1797`).
- Consommation anim dans `USubCrewAnimInstance::ComputeHandIK` (`SubCrewAnimInstance.cpp:519`).

Etat observe:

- 6 probes: HandL, HandR, HipL, HipR, ShoulderL, ShoulderR.
- Des targets et weights sont exposes dans AnimInstance: `HandIK_L_Target`, `HandIK_R_Target`, weights, states.
- Le node procedural C++ n'applique pas les hand targets.
- Application finale probable via AnimGraph manuel, mais non prouvee statiquement.

Conclusion: hand IK data existe, application finale non verifiee.

### Foot IK

Source C++:

- Traces dans `USubCrewMovementComponent::UpdateFootIKTraces` (`SubCrewMovementComponent.cpp:1868`).
- Consommation anim dans `USubCrewAnimInstance::ComputeFootIK` (`SubCrewAnimInstance.cpp:618`).

Etat observe:

- Sockets attendus: `foot_r`, `foot_l`.
- Trace up: 18 cm.
- Trace down: 55 cm.
- Max offset: 35 cm.
- Interp speed: 14.
- Channel trace: `ECC_GameTraceChannel2`.
- En grid authoritative, filtre via `Submarine->IsInteriorWalkableComponent`.
- Si socket absent, fallback actor-right offset.

Limite critique:

- `AnimNode_CrewProcedural` applique `SnapshotPelvisOffset` mais pas `FootIK_R_Offset` ni `FootIK_L_Offset`.
- Si l'ABP ne contient pas de TwoBoneIK/ModifyBone supplementaire, le foot IK calcule n'a pas d'effet visible final.

---

## 6. Classes C++ - ASubCrewCharacter

Fichier: `Source/Sub3D/Submarine/SubCrewCharacter.h`  
Implementation: `Source/Sub3D/Submarine/SubCrewCharacter.cpp`

### Role

`ASubCrewCharacter` est le pawn character jouable. Il porte:

- components camera/interact/underwater;
- reference au sous-marin courant;
- helm state;
- interaction;
- health;
- pressure/water state;
- current compartment;
- hull crossing handoff;
- RPC helm legacy/direct;
- wrappers input movement/posture/run.

### Components exposes

| UPROPERTY | Type | Category | Note |
|---|---|---|---|
| `FPSCamera` | `UCameraComponent*` | Components | camera first person |
| `TPSCameraBoom` | `USpringArmComponent*` | Components | boom third person |
| `TPSCamera` | `UCameraComponent*` | Components | camera third person |
| `InteractionComponent` | `USubInteractionComponent*` | Components | interaction |
| `UnderwaterPP` | `UCrewUnderwaterPPComponent*` | Components | post-process underwater |
| `DefaultUnderwaterPPMaterial` | `UMaterialInterface` | Crew/Underwater | material BP-assigne |

### References et etats principaux

| UPROPERTY | Replication | Role |
|---|---|---|
| `CurrentSubmarine` | ReplicatedUsing `OnRep_CurrentSubmarine` | sub frame courant |
| `bIsAtHelm` | Replicated | helm ownership |
| `Health` | Replicated | health |
| `CurrentCompartmentId` | ReplicatedUsing `OnRep_CurrentCompartmentId` | environment zone |
| `CurrentCompartment` | Transient weak ptr | pointer runtime zone |
| `CurrentAmbientPressureKPa` | local visible | pressure |
| `CurrentWaterHeightCm` | local visible | water height |
| `CurrentWaterImmersion01` | local visible | immersion |
| `PressureExposureSeconds` | local visible | pressure exposure |
| `bPressureDangerous` | local visible | pressure flag |
| `bIsSwimmingByFlood` | local visible | flood swim flag |

### Public methods importantes

| Methode | Fichier:ligne | Role |
|---|---|---|
| `BeginPlay()` | `SubCrewCharacter.cpp:150` | init defaults, camera, overlap bindings |
| `Tick(float)` | `SubCrewCharacter.cpp:435` | camera mode, environment effects |
| `GetCrewMovement()` | header | cast typed movement |
| `ApplyCrewPlanarMoveInput(FVector2D)` | `SubCrewCharacter.cpp:380` | wrapper vers movement component |
| `ToggleCameraMode()` | cpp | switch FPS/TPS |
| `SetFirstPersonMode(bool)` | cpp | set camera mode |
| `SetCurrentSubmarine(ASubmarineBase*)` | `SubCrewCharacter.cpp:499` | bind reference sans transition physique |
| `EnterOnFootInSubmarine(...)` | `SubCrewCharacter.cpp:528` | teleport spawn/bootstrap inside sub |
| `BoardSubmarine(...)` | cpp around `:633` | legacy wrapper |
| `DisembarkSubmarine()` | cpp around `:642` | legacy hard-detach |
| `Interact()` | cpp | line trace interaction |
| `TakeHelm()/ReleaseHelm()/ForceHelm()` | cpp | helm assignment |
| `HandleHullCrossing(...)` | `SubCrewCharacter.cpp:313` | EVA handoff |
| `IsInWater()` | `SubCrewCharacter.cpp:294` | water query |
| `HasOxygen()` | `SubCrewCharacter.cpp:306` | oxygen query stub |
| `ServerSetPostureTarget(float)` | cpp | posture RPC |
| `ServerSetRunning(bool)` | cpp | run RPC |

### BeginPlay details

Source: `SubCrewCharacter.cpp:150`.

Observe:

- Caches default walk/swim speed.
- Init health.
- Configure camera/head visibility.
- Binds capsule overlap begin/end for compartment volumes.
- Seeds initial overlapping compartment volumes.
- Registers underwater PP material if present.

### Current compartment

Source: `SubCrewCharacter.cpp:238`.

`RecomputeCurrentCompartment()`:

- Parcourt `ActiveCompartmentOverlaps`.
- Ignore invalid/inactive volumes.
- Choisit le volume dont le centre est le plus proche du character.
- Met a jour `CurrentCompartment` et `CurrentCompartmentId`.

Dette: double overlap au bord entre deux compartments peut encore flip selon distance.

### IsInWater / HasOxygen

Source:

- `SubCrewCharacter.cpp:294`
- `SubCrewCharacter.cpp:306`

Comportement:

- `IsInWater`: si pas de compartment, considere ocean donc true.
- En compartment, retourne `CurrentWaterImmersion01 > 0.01`.
- TODO post-FP: local-Z vs compartment water height plus correct.
- `HasOxygen`: false en ocean, true si `CurrentCompartment->O2Level01 > 0`.
- O2 est stubbed a 1 cote compartment volume.

### Hull crossing

Source: `SubCrewCharacter.cpp:313`.

Comportement:

- Recupere `CrewMov`, `Sub`, transform sub, vitesse sub monde, vitesse crew monde.
- Outgoing:
  - `Velocity = V_crew_world + V_sub_world`.
  - `SetEmbarkState(Outside)`.
  - Clear current compartment.
  - `SetMovementMode(MOVE_Flying)`.
  - `GravityScale = 0`.
  - Pending handoff `Outgoing`.
- Incoming:
  - Calcule position locale via `SubXf.InverseTransformPosition`.
  - Calcule yaw local via world yaw - sub yaw.
  - Seed `GridSpaceTransform`.
  - `Velocity = V_crew_world - V_sub_world`.
  - `SetEmbarkState(Embarked)`.
  - `SetMovementMode(MOVE_Walking)`.
  - `GravityScale = 1`.
  - Pending handoff `Incoming`.

Dette: Outside utilise Flying + zero gravity comme stub ocean.

### Environment effects

Source:

- `SubCrewCharacter.cpp:876`
- `SubCrewCharacter.cpp:975`

`UpdateEnvironmentalEffects`:

- Reset environment state.
- Resolve current compartment.
- Calcule water height/immersion si current sub + compartment.
- Applique pressure effects.
- Applique water movement state.
- Log debug periodique.

`ApplyWaterMovementState`:

- Immersion < shallow: normal walk speed.
- Shallow/deep/near swim: speed multipliers.
- Immersion >= swim threshold: `bIsSwimmingByFlood = true`, `MOVE_Swimming`.
- Sinon si CMC en `MOVE_Swimming`, repasse `MOVE_Walking`.

---

## 7. Classes C++ - USubCrewMovementComponent

Fichier: `Source/Sub3D/Submarine/SubCrewMovementComponent.h`  
Implementation: `Source/Sub3D/Submarine/SubCrewMovementComponent.cpp`

### Role

`USubCrewMovementComponent` est le composant mouvement custom qui garde le crew dans un frame local sous-marin tout en reutilisant `UCharacterMovementComponent` pour la simulation.

Il gere:

- Local Grid Authority;
- `GridSpaceTransform`;
- `ECrewEmbarkState`;
- yaw local `GridFacingYawDeg`;
- movement intent;
- locomotion frame;
- support/floor state;
- inertial state du sub;
- hand probes;
- foot IK traces;
- posture capsule/camera;
- run;
- ladder climb;
- network saved moves custom.

### Enums

```cpp
UENUM(BlueprintType)
enum class ECrewPostureState : uint8
{
    Prone,
    Crouched,
    Standing
};

UENUM(BlueprintType)
enum class ECrewEmbarkState : uint8
{
    Outside,
    Embarked,
    Transitioning
};
```

Semantique:

- `Outside`: world-space, CMC native, ocean/EVA/world walking.
- `Embarked`: local grid-space rebase active.
- `Transitioning`: reserve pour handoff multi-tick post-FP; FP fait des flips instantanes.

### Local Grid state

| UPROPERTY | Replication | Role |
|---|---|---|
| `GridSpaceTransform` | ReplicatedUsing `OnRep_GridSpaceTransform`, `COND_SkipOwner` | pose locale submarine |
| `GridFacingYawDeg` | non replicated direct | yaw local courant |
| `DesiredGridFacingYawDeg` | non replicated direct | yaw cible |
| `GridFacingYawRateDegPerSec` | non replicated direct | yaw rate anim/debug |
| `EmbarkState` | Replicated `COND_SkipOwner` | outside/embarked/transitioning |

`IsGridAuthoritative()` retourne true pour `Embarked` ou `Transitioning`.

### Tick LGA

Source: `SubCrewMovementComponent.cpp:144`.

Phases principales:

1. Clear move intent si aucun input recu ce frame.
2. Lazy latch si binding sub gagne sans state grid.
3. Met `bIgnoreBaseRotation = IsGridAuthoritative`.
4. Met `CharacterOwner->bUseControllerRotationYaw = !IsGridAuthoritative`.
5. REBASE pre-CMC:
   - calcule `RebasedWorldPos = SubTransform.TransformPosition(GridSpaceTransform.GetLocation())`;
   - calcule world yaw = sub yaw + local grid yaw;
   - `UpdatedComponent->SetWorldLocationAndRotation(..., false, nullptr, ETeleportType::TeleportPhysics)`;
   - reset internals CMC `LastUpdateLocation`, `LastUpdateRotation`.
6. Option local owner:
   - carry controller yaw by sub yaw delta.
7. `Super::TickComponent`.
8. `TickPosture`.
9. EXTRACT post-CMC:
   - `GridSpaceTransform.Location = SubTransform.InverseTransformPosition(ActorWorldLocation)`;
   - `GridSpaceTransform.Rotation = yaw local`.
10. Update relative/inertial/support/brace/IK/debug.

### REBASE / SIMULATE / EXTRACT

ASCII detail:

```text
REBASE
  Input:
    SubWorldTransform
    GridSpaceTransform
    GridFacingYawDeg
  Output:
    Actor world pose set to SubWorldTransform * GridSpaceTransform
  CMC transport from MovementBase is bypassed.

SIMULATE
  Input:
    world actor pose just rebased
    CMC acceleration/input
    floor/movement mode
  Output:
    CMC-updated actor world pose and velocity

EXTRACT
  Input:
    actor world pose after CMC
    SubWorldTransform
  Output:
    GridSpaceTransform updated as local truth
```

### Movement input

Source:

- `BuildMoveIntent`: `SubCrewMovementComponent.cpp:639`.
- `ApplyCrewPlanarMoveInput`: `SubCrewMovementComponent.cpp:692`.

Comportement:

- Clamp axis.
- Lit `Controller->GetControlRotation().Yaw`.
- Ignore pitch/roll pour direction de mouvement.
- World forward/right depuis yaw only.
- Desired world yaw = control yaw.
- Si sub existe:
  - local move dir = sub yaw inverse du world move dir.
  - desired grid yaw = control yaw - sub yaw.
- Stocke `LastMoveIntent`.
- Appelle `AddInputVector(WorldMoveDirection * strength)`.

Observation par rapport au probleme camera:

- En C++, la direction avance ne depend pas du pitch camera. Donc "je regarde au sol et Z n'avance pas" ne vient pas de ce code si ce path est bien utilise.
- Le BP peut encore router l'input differemment. A valider dans `BP_SubmarineCrew` et `PC_SubPlayerController`.
- Le systeme reste controller/camera yaw driven. Le user veut retirer camera control direction; c'est une decision de design a traiter dans une passe future.

### Locomotion frame

Source: `SubCrewMovementComponent.cpp:705`.

Produit:

- world/local velocity;
- speed 2D;
- body yaw world/local;
- local turn rate;
- support quality;
- posture alpha;
- stance;
- gait;
- moving/running/swimming flags.

`bSwimming` est base sur `Crew->bIsSwimmingByFlood`, pas sur `ECrewControlMode` ni sur `MOVE_Swimming` general.

### Grid yaw

Source: `SubCrewMovementComponent.cpp:771`.

- Local owner uniquement: yaw desire vient de `LastMoveIntent.DesiredGridYawDeg`.
- Tourne avec `GridFacingTurnRateDegPerSec = 720`.
- Injecte yaw local dans `GridSpaceTransform`.

Observation: le crew peut se tourner si input pipeline alimente bien `LastMoveIntent`. Si le symptome "ne se tourne pas" persiste, verifier:

- `ApplyCrewPlanarMoveInput` appele chaque frame;
- pawn vs PC input split;
- `bUseControllerRotationYaw` desactive en grid mode;
- visual mesh rotation dans BP;
- ABP upper body compensation.

### Stock CMC overrides

| Override | Role |
|---|---|
| `UpdateBasedMovement` | no-op en grid authoritative pour eviter transport base |
| `UpdateBasedRotation` | no-op en grid authoritative |
| `SmoothCorrection` | supprime mesh offset corrections en grid authoritative |
| `ServerCheckClientError` | ignore world error en grid authoritative |
| `MoveAutonomous` | applique grid state client sur serveur |
| `GetPredictionData_Client` | saved moves custom |

`ServerCheckClientError` source: `SubCrewMovementComponent.cpp:569`.

Commentaire C++:

- En grid mode, la pose monde server/client differe car chacun rebase contre une pose sub interp differente.
- CMC default corrigerait constamment.
- Pour FP co-op, le check truste la pose client.

Dette: production doit valider et borner les deltas local-space.

### OnRep GridSpaceTransform

Source: `SubCrewMovementComponent.cpp:1107`.

Comportement:

- Stocke packet recu.
- Met `GridFacingYawDeg` directement depuis transform replique.
- Calcule yaw rate.
- Log debug si active.

Non observe:

- smoothing local-space peer.
- interpolation entre deux grid poses.

Dette: cohérent avec l'audit jitter du 2026-04-27. Les peers peuvent snap si replication arrive en marches.

### Support/floor

Source: `SubCrewMovementComponent.cpp:1164`.

Observe:

- Utilise `CurrentFloor` et MovementBase.
- `bHasValidEmbarkedFloor`.
- `bHasAcceptedEmbarkedBase`.
- `bNeedsEmbarkedFloorRecovery`.
- `SupportQuality01`.

`AttemptEmbarkedFloorRecovery` retourne immediatement quand grid authoritative pour ne pas se battre avec CMC.

### Posture

Source: `SubCrewMovementComponent.cpp:1692`.

Comportement:

- `PostureAlpha` interpole vers `PostureTarget`.
- Capsule half height lerp entre prone 30 et standing 88.
- `SetCapsuleHalfHeight(NewHalfHeight, true)`.
- Actor offset par delta half-height sans sweep explicite.
- FPS camera Z lerp entre 25 et 70.

Dette:

- Pas de clearance test avant croissance capsule.
- Crouch/prone purement scalar, pas de movement mode stance dedie.

### Foot IK traces

Source: `SubCrewMovementComponent.cpp:1868`.

Comportement:

- Reset si swimming.
- Uses socket `foot_r`/`foot_l` si existants.
- Sinon fallback actor location + right offset.
- Trace multi channel `ECC_GameTraceChannel2`.
- En grid authoritative filtre interior walkable components.
- Offset Z interpole.

### Tick prerequisites

Source: `SubCrewMovementComponent.cpp:1580`.

`InitializeForSubmarine()` ajoute un tick prerequisite sur `SubMovement` si present.

Drift par rapport a docs:

- Les docs parlent de chaine `SubFlood -> SubMovement -> InteriorFrame -> CrewMovement`.
- Dans le code inspecte, le prerequisite explicite visible cote crew est `SubMovement -> CrewMovement`.
- `InteriorFrame` n'est pas confirme dans ce chemin statique.

---

## 8. Classes C++ - Interaction & boundary

### USubInteractionComponent

Fichiers:

- `Source/Sub3D/Submarine/SubInteractionComponent.h`
- `Source/Sub3D/Submarine/SubInteractionComponent.cpp`

Role:

- Focus interactable.
- Primary interact.
- Repair trace.
- Server RPC interact.
- Trace depuis camera.

Key refs:

| Methode | Ligne |
|---|---|
| `TryPrimaryInteract` | `SubInteractionComponent.cpp:32` |
| `BeginToolAction` | `SubInteractionComponent.cpp:54` |
| `EndToolAction` | `SubInteractionComponent.cpp:58` |
| `PerformRepairTrace` | `SubInteractionComponent.cpp:62` |
| `TraceFromView` | `SubInteractionComponent.cpp:67` |
| `ServerTryPrimaryInteract` | `SubInteractionComponent.cpp:142` |
| `ResolvePrimaryInteractTarget` | `SubInteractionComponent.cpp:205` |

TraceFromView:

```text
Path 1:
  Owner is ASubCrewCharacter
  Uses Crew->GetActiveViewCamera()
  Distance = Crew->InteractDistance

Path 2:
  Owner is generic APawn
  Finds first UCameraComponent
  Distance = DefaultTraceDistance 500

Collision:
  LineTraceSingleByChannel(..., ECC_Visibility)
```

### USubHullBoundaryComponent

Fichiers:

- `Source/Sub3D/Submarine/SubHullBoundaryComponent.h`
- `Source/Sub3D/Submarine/SubHullBoundaryComponent.cpp`

Role:

- UBoxComponent servant de plane de crossing hull.
- Collision profile `CompartmentProbe`.
- QueryOnly overlap with pawn.
- Tracks side of crew capsule relative to component local +X normal.
- Fires crossing once while armed; rearmed on end overlap.

Key refs:

| Methode / champ | Ligne |
|---|---|
| `EHullBoundaryKind` | `SubHullBoundaryComponent.h:10` |
| `OnCapsuleCrossedHull` delegate | `SubHullBoundaryComponent.h:16` |
| constructor collision profile | `SubHullBoundaryComponent.cpp:6` |
| begin overlap binding | `SubHullBoundaryComponent.cpp:25` |
| crossing call to crew | `SubHullBoundaryComponent.cpp:54` |
| broadcast | `SubHullBoundaryComponent.cpp:55` |
| `ComputeSide` | `SubHullBoundaryComponent.cpp:91` |

Boundary kinds:

```cpp
enum class EHullBoundaryKind : uint8
{
    Airlock,
    Breach
};
```

### UCompartmentVolumeComponent

Fichiers:

- `Source/Sub3D/Submarine/CompartmentVolumeComponent.h`
- `Source/Sub3D/Submarine/CompartmentVolumeComponent.cpp`

Role:

- UBoxComponent volume environment.
- Channel `ECC_CompartmentProbe = ECC_GameTraceChannel3`.
- Profile `CompartmentProbe`.
- Provides compartment id/display/debug/color.
- Links audio/post-process volumes.
- Reads `USubFloodComponent` water height/level.
- Stub oxygen.

Key refs:

| Methode / champ | Ligne |
|---|---|
| `ECC_CompartmentProbe` | `CompartmentVolumeComponent.h:14` |
| constructor collision | `CompartmentVolumeComponent.cpp:11` |
| `O2Level01` | `CompartmentVolumeComponent.h:51` |
| `LinkedAudioVolume` | `CompartmentVolumeComponent.h:55` |
| `LinkedPostProcessVolume` | `CompartmentVolumeComponent.h:59` |
| `GetFlood` | `CompartmentVolumeComponent.cpp:110` |
| `GetWaterHeightCm` | `CompartmentVolumeComponent.cpp:128` |
| `GetWaterLevel01` | `CompartmentVolumeComponent.cpp:138` |
| `GetWaterSurfaceWorldLocation` | `CompartmentVolumeComponent.cpp:172` |

---

## 9. Couplage avec Submarine

### Chaines principales

```text
ASubmarineBase
  owns SubMovement and flood/environment components
  exposes world transform and velocity
  exposes interior walkable filtering

USubMovementComponent
  simulates submarine movement
  must tick before crew movement

USubCrewMovementComponent
  stores local GridSpaceTransform
  rebases crew into submarine world frame
  runs CMC
  extracts local truth

ASubCrewCharacter
  owns CurrentSubmarine pointer
  owns CurrentCompartment pointer
  handles hull crossing
  updates pressure/water state

USubCrewAnimInstance
  reads crew movement frame
  produces procedural bone rotations

FAnimNode_CrewProcedural
  applies bone rotations to output pose
```

### CurrentSubmarine

`ASubCrewCharacter::CurrentSubmarine`:

- Replicated using `OnRep_CurrentSubmarine`.
- Set via `SetCurrentSubmarine`.
- Used by movement component through `GetCurrentSubmarine`.
- Used by environment update to compute local compartment/water state.

`SetCurrentSubmarine` has a guard: clearing null outside legal disembark path triggers ensure/log.

### Embark/disembark

Current intended path:

```text
USubHullBoundaryComponent detects capsule side flip
  -> ASubCrewCharacter::HandleHullCrossing(Boundary, bOutgoing)
    outgoing:
      Embarked -> Outside
      add sub velocity
      MovementMode Flying
      GravityScale 0
    incoming:
      Outside -> Embarked
      seed GridSpaceTransform from current world pose
      subtract sub velocity
      MovementMode Walking
      GravityScale 1
```

Legacy paths:

- `BoardSubmarine` deprecated.
- `DisembarkSubmarine` deprecated hard-detach.

### Motion chain

```text
Sub world transform at frame N
  -> CrewMovement REBASE local pose into world
  -> Stock CMC simulates movement/collision/floor
  -> CrewMovement EXTRACTS new local pose
  -> AnimInstance reads LastLocomotionFrame
  -> Anim node applies procedural bones
```

---

## 10. Inputs & actions

### Enhanced Input assets trouves

On-foot:

| Asset |
|---|
| `Content/Sub3D/Input/IMC_OnFoot.uasset` |
| `Content/Sub3D/Input/ONFOOT/IA_Move.uasset` |
| `Content/Sub3D/Input/ONFOOT/IA_Look.uasset` |
| `Content/Sub3D/Input/ONFOOT/IA_Interact.uasset` |
| `Content/Sub3D/Input/ONFOOT/IA_Repair.uasset` |
| `Content/Sub3D/Input/ONFOOT/IA_Run.uasset` |
| `Content/Sub3D/Input/IA_PostureScroll.uasset` |
| `Content/Sub3D/Input/IA_ToggleCamera.uasset` |

Helm:

| Asset |
|---|
| `Content/Sub3D/Input/IMC_Helm.uasset` |
| `Content/Sub3D/Input/HELM/IA_DivePlane.uasset` |
| `Content/Sub3D/Input/HELM/IA_HelmDirectInputHold.uasset` |
| `Content/Sub3D/Input/HELM/IA_HelmMouseDirect.uasset` |
| `Content/Sub3D/Input/HELM/IA_RadarToggle.uasset` |
| `Content/Sub3D/Input/HELM/IA_Rudder.uasset` |
| `Content/Sub3D/Input/HELM/IA_SonarLean.uasset` |
| `Content/Sub3D/Input/HELM/IA_SonarPing.uasset` |
| `Content/Sub3D/Input/HELM/IA_Thrust.uasset` |

Station/UI:

| Asset |
|---|
| `Content/Sub3D/Input/IMC_StationUI.uasset` |
| `Content/Sub3D/Input/IA_ExitStation.uasset` |
| `Content/Sub3D/Input/IA_FireTurret.uasset` |

Non trouve:

- `IMC_Swimming`.
- `IA_SwimAscend`.
- `IA_SwimDescend`.
- `IA_SwimBoost`.

### PlayerController

Fichier: `Source/Sub3D/Submarine/SubPlayerController.h`.

`ECrewControlMode` actuel:

```cpp
enum class ECrewControlMode : uint8
{
    OnFoot,
    HelmDriving,
    StationUI
};
```

Pas de `Swimming`.

`PC_SubPlayerController.uasset` reference:

- `ApplyControlMode`.
- `BP_OnControlModeChanged`.
- `IMC_OnFoot`.
- `IMC_Helm`.
- `IMC_StationUI`.
- `IA_Move`.
- `IA_Look`.
- Many helm actions.
- String `On Foot IA_Move`.

### Pawn crew BP

`BP_SubmarineCrew.uasset` reference:

- `IA_Move`.
- `IA_Run`.
- `IA_PostureScroll`.
- `IA_Interact`.
- `IA_Repair`.
- Function `ApplyCrewPlanarMoveInput`.

Conclusion:

- L'input movement n'est pas entierement dans `PC_SubPlayerController`.
- Le pawn crew garde un wrapper C++ et le BP crew reference des input actions.
- Cela confirme le point user: `Apply Crew Planar Move Input` doit etre transfere cote PlayerController dans une passe future.

### Path input -> animation

Path observe:

```text
Enhanced Input IA_Move
  -> BP_SubmarineCrew and/or PC_SubPlayerController graph
  -> ASubCrewCharacter::ApplyCrewPlanarMoveInput
  -> USubCrewMovementComponent::ApplyCrewPlanarMoveInput
  -> BuildMoveIntent
  -> AddInputVector
  -> CMC movement
  -> UpdateLocomotionFrame
  -> USubCrewAnimInstance::ReadInputState
  -> ComputeWalkCycle / ComputeSwimCycle / etc.
  -> AnimNode_CrewProcedural
```

Ambiguite a valider en editeur:

- Le PC et le pawn peuvent tous deux contenir IA_Move references.
- Il faut inspecter les graphs pour savoir quel chemin est actif au runtime.

---

## 11. Replication

### ASubCrewCharacter

Replicated properties observees:

| Property | Mode | Role |
|---|---|---|
| `CurrentSubmarine` | `ReplicatedUsing=OnRep_CurrentSubmarine` | submarine binding |
| `bIsAtHelm` | `Replicated` | helm state |
| `Health` | `Replicated` | health |
| `CurrentCompartmentId` | `ReplicatedUsing=OnRep_CurrentCompartmentId` | environment zone id |

RPCs:

- `ServerSetPostureTarget(float)`.
- `ServerSetRunning(bool)`.
- `Server_SetThrust(float)`.
- `Server_SetRudder(float)`.
- `Server_SetDivePlane(float)`.
- `Server_SetBallastTarget(int32, float)`.
- `Server_ResyncBallasts(float)`.
- `Server_TakeHelm()`.
- `Server_ReleaseHelm()`.

### USubCrewMovementComponent

Replicated properties:

| Property | Mode | Role |
|---|---|---|
| `GridSpaceTransform` | ReplicatedUsing `OnRep_GridSpaceTransform`, skip owner | local grid pose |
| `EmbarkState` | Replicated, skip owner | movement state |
| `CurrentLadder` | Replicated | ladder |
| `LadderClimbProgress01` | ReplicatedUsing | ladder progress |
| `PostureAlpha` | Replicated | posture |
| `bIsRunning` | Replicated | run |

Network move custom:

Files:

- `Source/Sub3D/Submarine/SubCrewNetTypes.h`
- `Source/Sub3D/Submarine/SubCrewNetTypes.cpp`

Types:

- `ECrewHandoffKind`
- `FSavedMove_SubCrew`
- `FCharacterNetworkMoveData_SubCrew`
- `FCharacterNetworkMoveDataContainer_SubCrew`
- `FNetworkPredictionData_Client_SubCrew`

Serialized with saved moves:

- `GridSpaceTransform`.
- `EmbarkStateByte`.
- `Handoff`.

### Trust model

Source: `SubCrewMovementComponent.cpp:834` and `:569`.

- Server applies client-reported `GridSpaceTransform` in `MoveAutonomous`.
- Server skips default world-space error in grid authoritative.
- Comment explicitly says FP co-op trust model; production should bound deltas.

Implication:

- Owner rubberband reduced.
- Cheating/invalid local pose not prevented.
- Peer visual jitter can still occur due `OnRep_GridSpaceTransform` no smoothing.

---

## 12. Problemes identifies

### P1 - Bras penches a droite

Severity: visuel majeur, possiblement pipeline/root-cause animation.  
Repro demandee: PIE, observer crew en idle/walk. Non reproduit dans cet audit statique.

#### Symptome

Les bras du personnage sont systematiquement penches sur la droite.

#### Zones impliquees

| Zone | Evidence |
|---|---|
| FBX Blender | `Art/Characters/Export/SK_Crew_Basic.fbx` cree par Blender 5.0.1 |
| Export settings | `axis_forward='-Y'`, `axis_up='Z'`, `add_leaf_bones=False` |
| Import UE | `bUseT0AsRefPose=false`, `bUpdateSkeletonReferencePose=false` |
| Skeleton | 27 bones custom, clavicles/upperarms imported |
| Anim C++ | axis remapping "fix Blender->UE bone axis mismatch" |
| Arm rest | hardcoded Roll `-85/+85` every non-swim frame |
| Anim node | additive rotations in local pose with quaternion pre-multiply |
| ABP | custom `AnimGraphNode_CrewProcedural` plus Idle/Walk/Swim |

#### Hypotheses

1. Mismatch axes bone Blender/UE.
   - Evidence: axis remapping present en C++, FBX root/bones ont transforms non triviales.
   - Statut: plausible, non confirme.

2. Bone roll asymetrique sur clavicle/upperarm.
   - Evidence: `upperarm_l` et `upperarm_r` ont transforms FBX differentes, clavicles non controlees proceduralement.
   - Statut: plausible, a valider dans Skeleton viewer.

3. Arm rest procedural faux pour ce skeleton.
   - Evidence: `ArmRestR/L` applique des Rolls fixes de 85 deg hors swim.
   - Statut: plausible et fortement suspect.

4. Additive rotation order incorrect.
   - Evidence: `BoneXform.SetRotation(AddRot.Quaternion() * BoneXform.GetRotation())` dans `AnimNode_CrewProcedural.cpp:160`.
   - Statut: possible. Depend de l'espace attendu par `FRotator` genere.

5. ABP applique une couche supplementaire.
   - Evidence: ABP binaire, graph non lisible; states et custom node visibles.
   - Statut: indetermine.

6. Mesh component transform dans `BP_SubmarineCrew`.
   - Evidence: BP binaire seulement; component names visibles, transforms non lus.
   - Statut: a valider en editeur.

7. Import `Force Front XAxis` / `Convert Scene` incompatible.
   - Evidence: options non lisibles statiquement.
   - Statut: a valider en Reimport Options.

8. Retargeting source incorrect.
   - Evidence: aucun autre skeleton/IKRetargeter trouve; moins probable.
   - Statut: non confirme.

#### Verification editeur concrete

1. Ouvrir `SK_Crew_Basic`.
2. Dans Skeleton tree, activer axes bones.
3. Comparer `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`.
4. Desactiver temporairement l'AnimBP sur le mesh preview pour voir ref pose.
5. Ouvrir `ABP_Crew`, preview idle, puis forcer `ArmRestR/L` a 0 via details ou console `Anim`.
6. Commandes utiles:
   - `ShowDebug Animation`
   - `AnimList`
   - `Anim armrestr 0` ou param exact listé par `AnimList`
7. Verifier Reimport Options du `SK_Crew_Basic`:
   - Force Front XAxis.
   - Convert Scene.
   - Convert Scene Unit.
   - Use T0 As Ref Pose.
   - Update Skeleton Reference Pose.
8. Dans AnimGraph, verifier s'il y a Control Rig, Layered Blend per Bone, Transform Modify Bone supplementaires.

#### Verdict audit

Cause finale non confirmee. Le contributeur le plus probable cote code est la combinaison:

```text
Blender custom bone axes
+ explicit C++ axis remapping
+ arm rest Roll -85/+85 applique hors swim
+ clavicles non controlees
+ additive local rotations dans AnimNode_CrewProcedural
```

### P2 - Locomotion au sol peu credible

Severity: visuel majeur.  
Localisation: `SubCrewAnimInstance.cpp:170`, `.h` tuning.

Symptome:

- Walk/sprint/crouch/prone procedural sinusoides.
- Pas d'anim authored.
- Peu de notion de contact pied robuste.
- Peu de pelvis lateral.
- Pas de vrai gait state authoré.

Cause probable:

- System procedural direct cree pour FP validation, pas pour quality anim.

Repro:

1. PIE.
2. Marcher / courir / crouch / prone.
3. Observer foot sliding, bras, spine, pelvis.

### P3 - IK calcule mais application finale incertaine

Severity: fonctionnel/visuel.  
Localisation:

- Foot traces: `SubCrewMovementComponent.cpp:1868`.
- Foot anim read: `SubCrewAnimInstance.cpp:618`.
- Anim node apply: `AnimNode_CrewProcedural.cpp:128`.

Evidence:

- Foot IK offsets existent.
- `FAnimNode_CrewProcedural` applique pelvis translation uniquement.
- Pas de preuve statique que `ABP_Crew` applique `FootIK_R_Offset`/`FootIK_L_Offset`.
- Hand IK targets existent mais pas appliques par le node C++.

Verification:

- Ouvrir `ABP_Crew`.
- Rechercher `FootIK_R_Offset`, `FootIK_L_Offset`, `HandIK_L_Target`, `HandIK_R_Target`.
- Verifier TwoBoneIK/FABRIK/ControlRig/ModifyBone.

### P4 - Swimming n'est pas un control mode complet

Severity: design/fonctionnel.  
Localisation:

- `SubPlayerController.h:13` enum.
- `SubCrewCharacter.cpp:975`.
- `SubCrewMovementComponent.cpp:705`.

Evidence:

- `ECrewControlMode` ne contient pas Swimming.
- Pas de `IMC_Swimming` trouve.
- `bIsSwimming` anim vient surtout de `bIsSwimmingByFlood`.
- EVA ocean utilise `MOVE_Flying` + `GravityScale=0` en handoff outgoing.

Impact:

- Pas de separation claire entre OnFoot, FloodSwim, EVA swim.
- Les inputs swim 3D/ascend/descend/boost ne sont pas modelises.

### P5 - Input ownership split entre pawn et PlayerController

Severity: architecture/input.  
Localisation:

- `SubCrewCharacter.h:67`
- `SubCrewCharacter.cpp:380`
- `SubCrewMovementComponent.h`
- `BP_SubmarineCrew.uasset`
- `PC_SubPlayerController.uasset`

Evidence:

- `ASubCrewCharacter::ApplyCrewPlanarMoveInput`.
- `USubCrewMovementComponent::ApplyCrewPlanarMoveInput`.
- `BP_SubmarineCrew` reference `IA_Move`, `IA_Run`, `IA_PostureScroll`.
- `PC_SubPlayerController` reference `IA_Move`, `IA_Look`, `ApplyControlMode`, IMCs.

Impact:

- Difficile de garantir le path input unique.
- Risque de double binding ou de path mort.
- Bloque une architecture control mode propre.

### P6 - Peer smoothing grid absent

Severity: multiplayer visual.  
Localisation: `SubCrewMovementComponent.cpp:1107`.

Evidence:

- `OnRep_GridSpaceTransform` applique directement yaw/grid packet.
- Pas d'interpolation local-space visible.
- Ancien audit jitter 2026-04-27 pointe cette zone.

Impact:

- Peers peuvent voir snaps/jitter quand le sub bouge.

### P7 - Collision/rollback bulkhead deja observe par user, pas audite ici

Severity: movement/network functional.  
Localisation probable:

- collision setup hull/proxy/map assets;
- CMC local vs server correction;
- movement base/grid collision.

Statut:

- Hors mission actuelle, mais lie a `ServerCheckClientError`, collision proxies, and LGA.
- Pas reaudite ici car prompt cible crew+animation.

### P8 - Posture capsule grow sans clearance test

Severity: functional in tight submarine.  
Localisation: `SubCrewMovementComponent.cpp:1692`.

Evidence:

- Capsule half-height change direct.
- Pas de pre-check overhead.
- Actor offset by half-height delta.

Impact:

- Peut clipper dans ceiling/bulkhead quand on repasse debout.

### P9 - O2, audio/PP, ocean swim sont des stubs

Severity: expected FP debt.  
Localisation:

- `SubCrewCharacter.cpp:306`
- `CompartmentVolumeComponent.h:51`
- `SubCrewCharacter.cpp:334`
- `CrewUnderwaterPPComponent.cpp:137`

Impact:

- Environment character pas production.

---

## 13. Dette technique connue

### TODO / comments directs

| Fichier:ligne | Texte / dette |
|---|---|
| `SubCrewCharacter.cpp:302` | local-Z comparison vs WaterHeight post-FP refinement |
| `SubCrewCharacter.cpp:309` | O2 stub |
| `SubCrewCharacter.cpp:334` | no ocean water volume, Flying + zero gravity stub |
| `SubCrewCharacter.cpp:751` | death ragdoll/respawn TODO |
| `SubCrewCharacter.cpp:633` | BoardSubmarine legacy compatibility |
| `SubCrewCharacter.cpp:642` | DisembarkSubmarine legacy hard-detach |
| `SubCrewMovementComponent.cpp:123` | FP EVA swim stub |
| `SubCrewMovementComponent.cpp:839` | production would bound client grid delta |
| `SubCrewMovementComponent.h:25` | Transitioning reserved post-FP |
| `CompartmentVolumeComponent.h:51` | `O2Level01 = 1.f` stub |

### Hardcoded values suspects

| Valeur | Localisation | Note |
|---:|---|---|
| `InteractDistance = 250` | `ASubCrewCharacter` | gameplay tuning |
| `SwimThreshold01 = 0.85` | `ASubCrewCharacter` | water behavior |
| `SwimSpeedMultiplier = 0.35` | `ASubCrewCharacter` | flood swim slow |
| `CameraSwayAccelScale = 0.002` | `ASubCrewCharacter` | feel tuning |
| `GridFacingTurnRateDegPerSec = 720` | `USubCrewMovementComponent` | turn feel |
| `SnapThresholdCm = 200` | `USubCrewMovementComponent` | snap debug/legacy risk |
| `BraceProbeDistanceCm = 90` | `USubCrewMovementComponent` | brace feel |
| `StandingHalfHeight = 88` | `USubCrewMovementComponent` | character/collision |
| `ProneHalfHeight = 30` | `USubCrewMovementComponent` | prone collision |
| `RunSpeedMultiplier = 1.8` | `USubCrewMovementComponent` | movement feel |
| `FootIKTraceUpCm = 18` | `USubCrewMovementComponent` | IK |
| `FootIKTraceDownCm = 55` | `USubCrewMovementComponent` | IK |
| `ArmRestR/L = +/-85 roll` | `USubCrewAnimInstance` | high risk for arm bug |
| `ForearmRestR/L = +/-10 roll` | `USubCrewAnimInstance` | high risk for arm bug |

### Spec vs implementation drift

| Spec / plan | Implementation observee |
|---|---|
| tick prereq docs mention `SubFlood -> SubMovement -> InteriorFrame -> CrewMovement` | code crew shows explicit prerequisite on `SubMovement`; `InteriorFrame` not confirmed in this path |
| character pipeline guide mentions BP input wiring needed | BP still references input actions in crew pawn; PC also references IA_Move |
| procedural plan warned about anim node thread safety | current `AnimNode_CrewProcedural` uses `PreUpdate` snapshot, donc cette dette semble traitee |
| plan environment says ocean swim proper post-FP | current code confirme Flying/zero gravity stub |
| `ECrewEmbarkState::Transitioning` intended future blend | current comments confirment instant flips FP |

---

## 14. Snippets C++ verbatim / pseudo-verbatim utiles

Note: snippets limites aux zones critiques. Les headers complets sont dans les chemins references.

### ECrewEmbarkState

```cpp
UENUM(BlueprintType)
enum class ECrewEmbarkState : uint8
{
    Outside,
    Embarked,
    Transitioning
};
```

### FCrewMoveIntent

```cpp
USTRUCT(BlueprintType)
struct SUB3D_API FCrewMoveIntent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    FVector2D MoveAxis = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    FVector WorldMoveDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    FVector LocalMoveDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    float MoveInputStrength = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    float ControlYawDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    float MoveWorldYawDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    float DesiredWorldYawDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    float DesiredGridYawDeg = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
    bool bHasMoveInput = false;
};
```

### FCrewLocomotionFrame

```cpp
USTRUCT(BlueprintType)
struct SUB3D_API FCrewLocomotionFrame
{
    GENERATED_BODY()

    FVector WorldVelocity = FVector::ZeroVector;
    FVector LocalVelocity = FVector::ZeroVector;
    float Speed2D = 0.f;
    float DirectionDeg = 0.f;
    float BodyWorldYawDeg = 0.f;
    float BodyLocalYawDeg = 0.f;
    float LocalTurnRateDegPerSec = 0.f;
    float SupportQuality01 = 0.f;
    float PostureAlpha = 1.f;
    ECrewLocomotionStance Stance = ECrewLocomotionStance::Standing;
    ECrewLocomotionGait Gait = ECrewLocomotionGait::Idle;
    bool bIsGridAuthoritative = false;
    bool bIsMoving = false;
    bool bIsSwimming = false;
    bool bIsRunning = false;
};
```

### ASubCrewCharacter movement wrapper

Source: `SubCrewCharacter.cpp:380`.

```cpp
void ASubCrewCharacter::ApplyCrewPlanarMoveInput(FVector2D MoveAxis)
{
    if (USubCrewMovementComponent* CrewMove = GetCrewMovement())
    {
        CrewMove->ApplyCrewPlanarMoveInput(MoveAxis);
    }
}
```

### HandleHullCrossing resume code

Source: `SubCrewCharacter.cpp:313`.

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
    const FVector SubVelWorld = Sub->GetVelocity();
    const FVector CrewVelWorld = GetVelocity();

    if (bOutgoing)
    {
        CrewMov->Velocity = CrewVelWorld + SubVelWorld;
        CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
        CurrentCompartment.Reset();
        CurrentCompartmentId = NAME_None;
        CrewMov->SetMovementMode(MOVE_Flying);
        CrewMov->GravityScale = 0.f;
        CrewMov->SetPendingHandoff(ECrewHandoffKind::Outgoing);
    }
    else
    {
        const FVector LocalLocation = SubXf.InverseTransformPosition(GetActorLocation());
        const float LocalYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw - SubXf.Rotator().Yaw);
        CrewMov->GridSpaceTransform.SetLocation(LocalLocation);
        CrewMov->GridSpaceTransform.SetRotation(FRotator(0.f, LocalYaw, 0.f).Quaternion());
        CrewMov->LastSubWorldTransform = SubXf;
        CrewMov->Velocity = CrewVelWorld - SubVelWorld;
        CrewMov->SetEmbarkState(ECrewEmbarkState::Embarked);
        CrewMov->SetMovementMode(MOVE_Walking);
        CrewMov->GravityScale = 1.f;
        CrewMov->SetPendingHandoff(ECrewHandoffKind::Incoming);
    }
}
```

### BuildMoveIntent resume code

Source: `SubCrewMovementComponent.cpp:639`.

```cpp
FCrewMoveIntent USubCrewMovementComponent::BuildMoveIntent(FVector2D MoveAxis) const
{
    FCrewMoveIntent Intent;
    Intent.MoveAxis = MoveAxis.GetClampedToMaxSize(1.f);
    Intent.MoveInputStrength = Intent.MoveAxis.Size();
    Intent.bHasMoveInput = Intent.MoveInputStrength > KINDA_SMALL_NUMBER;

    const AController* Controller = CharacterOwner ? CharacterOwner->GetController() : nullptr;
    const float ControlYawDeg = Controller ? Controller->GetControlRotation().Yaw : 0.f;
    Intent.ControlYawDeg = ControlYawDeg;

    const FRotator ControlYawRot(0.f, ControlYawDeg, 0.f);
    const FVector Forward = ControlYawRot.Vector();
    const FVector Right = FRotationMatrix(ControlYawRot).GetScaledAxis(EAxis::Y);

    FVector WorldMoveDirection = Forward * Intent.MoveAxis.X + Right * Intent.MoveAxis.Y;
    WorldMoveDirection.Z = 0.f;
    Intent.WorldMoveDirection = WorldMoveDirection.GetSafeNormal();
    Intent.DesiredWorldYawDeg = ControlYawDeg;

    if (const ASubmarineBase* Sub = GetCurrentSubmarine())
    {
        const FRotator SubYawRot(0.f, Sub->GetActorRotation().Yaw, 0.f);
        Intent.LocalMoveDirection = SubYawRot.UnrotateVector(Intent.WorldMoveDirection);
        Intent.DesiredGridYawDeg = FRotator::NormalizeAxis(ControlYawDeg - SubYawRot.Yaw);
    }

    return Intent;
}
```

### REBASE/EXTRACT pseudo-code exact intention

Source: `SubCrewMovementComponent.cpp:144`.

```cpp
// Pre-CMC rebase
if (bGridAuth && Sub)
{
    UpdateGridFacingYaw(DeltaTime, SubTransform);
    const FVector RebasedWorldPos = SubTransform.TransformPosition(GridSpaceTransform.GetLocation());
    const FRotator RebasedWorldRot(0.f, SubYaw + GridFacingYawDeg, 0.f);
    UpdatedComponent->SetWorldLocationAndRotation(
        RebasedWorldPos,
        RebasedWorldRot,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

// Post-CMC extract
if (bGridAuth && Sub)
{
    GridSpaceTransform.SetLocation(SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation()));
    GridSpaceTransform.SetRotation(FRotator(0.f, GridFacingYawDeg, 0.f).Quaternion());
}
```

### Anim node apply rotation

Source: `AnimNode_CrewProcedural.cpp:128` and `:160`.

```cpp
void FAnimNode_CrewProcedural::Evaluate_AnyThread(FPoseContext& Output)
{
    BasePose.Evaluate(Output);

    if (!bBonesResolved)
    {
        ResolveBones(Output.Pose.GetBoneContainer());
    }

    for (const FCrewProceduralBone& Bone : ProceduralBones)
    {
        if (!Bone.CompactPoseIndex.IsValid())
        {
            continue;
        }

        FTransform& BoneXform = Output.Pose[Bone.CompactPoseIndex];
        const FRotator AddRot = GetSnapshotRotForBone(Bone);
        BoneXform.SetRotation(AddRot.Quaternion() * BoneXform.GetRotation());
    }

    if (PelvisCompactPoseIndex.IsValid())
    {
        FTransform& BoneXform = Output.Pose[PelvisCompactPoseIndex];
        BoneXform.AddToTranslation(SnapshotPelvisOffset);
    }
}
```

### Arm rest apply

Source: `SubCrewAnimInstance.cpp:108`.

```cpp
if (!bIsSwimming)
{
    Proc_UpperarmR_Rot += ArmRestR;
    Proc_UpperarmL_Rot += ArmRestL;
    Proc_LowerarmR_Rot += ForearmRestR;
    Proc_LowerarmL_Rot += ForearmRestL;
}
```

---

## 15. Schemas

### ECrewEmbarkState

```text
                 hull crossing incoming
        +--------------------------------------+
        |                                      v
  +-----------+       future blend       +------------+
  | Outside   | -----------------------> | Transition |
  | world CMC |                          | reserved   |
  +-----------+                          +------------+
        ^                                      |
        |                                      |
        +--------------------------------------+
                 hull crossing outgoing

Current FP implementation:

Outside <-> Embarked

Transitioning exists in enum but is not a multi-tick blend yet.
```

### Local Grid Authority

```text
Frame N:

SubMovement tick
  updates submarine world transform and velocity

CrewMovement tick
  REBASE:
    actor world pose = sub world pose * GridSpaceTransform

  SIMULATE:
    stock CharacterMovementComponent handles walking/collision/floor

  EXTRACT:
    GridSpaceTransform = inverse(sub world pose) * actor world pose

Anim update
  reads LocomotionFrame and MoveIntent
  generates procedural pose
```

### Tick prerequisites observees

```text
Documented in plans:

SubFlood -> SubMovement -> InteriorFrame -> CrewMovement

Observed in USubCrewMovementComponent::InitializeForSubmarine:

SubMovement -> CrewMovement

InteriorFrame dependency must be validated elsewhere if still expected.
```

### BP_SubmarineCrew component hierarchy observee par strings

```text
BP_SubmarineCrew
  native parent: SubCrewCharacter
  CharacterMesh0 / SkeletalMeshComponent
    SK_Crew_Basic
    ABP_Crew
  CharMoveComp / SubCrewMovementComponent
  SubInteractionComponent
  CrewUnderwaterPPComponent
  SpringArmComponent
  CameraComponent
  Arrow
```

Transforms exacts non lus statiquement.

### Procedural animation stack

```text
USubCrewMovementComponent
  LastMoveIntent
  LastLocomotionFrame
  HandProbes
  FootIK states

USubCrewAnimInstance::NativeUpdateAnimation
  ReadInputState
  Walk/Crawl/Swim cycle
  Breathing
  Posture
  SubMotion
  UpperBodyAim
  HandIK state
  FootIK state
  ArmRest pose

FAnimNode_CrewProcedural
  BasePose
  Add rotations to 18 bones
  Add pelvis translation
```

---

## 16. Plan d'extraction utile en editeur

### Verifier le bug bras

1. Ouvrir `Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic`.
2. Passer en Skeleton view.
3. Activer bone axes.
4. Inspecter:
   - `clavicle_l/r`
   - `upperarm_l/r`
   - `lowerarm_l/r`
   - `hand_l/r`
5. Ouvrir Retargeting Options.
6. Verifier si le ref pose est T-pose propre.
7. Dans preview mesh, jouer sans AnimBP si possible pour isoler ref pose.
8. Ouvrir `ABP_Crew`.
9. Inspecter AnimGraph:
   - State machine Idle/Walk/Swim.
   - `Crew Procedural Animation` node.
   - tout `Modify Bone`.
   - tout `Layered Blend per Bone`.
   - tout ControlRig.
   - tout IK node.
10. Dans PIE, utiliser:
    - `ShowDebug Animation`
    - `AnimList`
    - `Anim <param> <value>`

### Verifier import FBX

1. Ouvrir `SK_Crew_Basic`.
2. Asset menu -> Reimport Options.
3. Noter:
   - Force Front XAxis.
   - Convert Scene.
   - Convert Scene Unit.
   - Use T0 As Ref Pose.
   - Update Skeleton Reference Pose.
   - Import Meshes in Bone Hierarchy.
4. Comparer avec script export:
   - `axis_forward='-Y'`
   - `axis_up='Z'`
   - `apply_scale_options='FBX_SCALE_ALL'`
   - `add_leaf_bones=False`

### Verifier ABP variables

Dans `ABP_Crew`:

- Chercher `ArmRestR`, `ArmRestL`.
- Mettre temporairement roll 0 pour isoler la cause.
- Chercher `HandIK_L_Target` et `FootIK_R_Offset`.
- Verifier si ces variables sont branchees a des nodes IK.

### Export T3D / asset diagnostics

Le user a deja exporte du T3D pour `M_Phase0_Test`. Pour `BP_SubmarineCrew`, faire pareil si besoin:

- Clic droit asset -> Asset Actions -> Export, ou utiliser un utilitaire editor si present.
- Exporter uniquement pour inspection textuelle, ne pas reimport sans plan migration.

### Logs utiles

Console:

```text
ShowDebug Animation
ShowDebug Character
AnimList
Anim ArmSwingAxis 0
Anim ArmSwingAxis 1
Anim ArmSwingAxis 2
Anim ArmRestR 0
Anim ArmRestL 0
```

Les noms exacts de params sont ceux imprimes par `AnimList`.

---

## 17. Memory files / plans a valider

Les fichiers exacts demandes dans le prompt ne sont pas presents sous ces noms:

- `project_character_pipeline_2026_04_14.md` non trouve.
- `project_crew_embarked_failure_2026_04_21.md` non trouve.
- `project_embarked_refactor_rolled_back_2026_04_20.md` non trouve.
- `project_motion_chain_jitter_root_cause_2026_04_27.md` non trouve.

Fichiers equivalents / proches inspectes:

| Fichier | Statut vs code actuel | Notes |
|---|---|---|
| `reports/guides/2026-04-14_character_pipeline_editor_handoff.md` | Partiellement a jour | C++ procedural existe; BP/AnimGraph wiring reste a valider |
| `reports/plans/archive/character_pipeline_architecture.md` | Archive, partiellement stale | Plan initial utile pour intent, pas source actuelle |
| `reports/plans/2026-04-21_local_grid_space_authority_architecture.md` | Majoritairement a jour | REBASE/SIMULATE/EXTRACT confirme |
| `reports/plans/2026-04-22_crew_environment_axis.md` | Majoritairement a jour | Outside/Embarked + compartments confirme; ocean swim proper toujours stub |
| `reports/plans/2026-04-23_procedural_crew_animation_architecture_and_execution_plan.md` | Partiellement a jour | Thread-safety AnimNode semble corrigee par PreUpdate snapshot; autres faiblesses restent |
| `reports/plans/2026-04-27_motion_conditional_jitter_root_cause_audit.md` | Encore pertinent | OnRep grid sans smoothing confirme |
| `reports/plans/2026-04-10_first_playable_strategic_analysis.md` | Authority-max | Confirme scope FP, pas AI crew avant FP clos |

### Points du guide 2026-04-14 confirmes

- `USubCrewAnimInstance` existe.
- `FAnimNode_CrewProcedural` existe.
- Procedural walk/swim/posture/sub motion existe.
- Bone list attendue correspond au skeleton.
- Debug `Anim` / `AnimList` existe dans `ASubPlayerController`.

### Points a verifier en editeur

- `BP_SubmarineCrew` input wiring exact.
- `ABP_Crew` AnimGraph exact.
- Application finale hand IK et foot IK.
- Mesh component rotation dans BP.
- Import options FBX exactes.

---

## Appendix A - Fichiers audites

### Source C++ crew direct

```text
Source/Sub3D/Submarine/SubCrewCharacter.h
Source/Sub3D/Submarine/SubCrewCharacter.cpp
Source/Sub3D/Submarine/SubCrewMovementComponent.h
Source/Sub3D/Submarine/SubCrewMovementComponent.cpp
Source/Sub3D/Submarine/SubCrewAnimInstance.h
Source/Sub3D/Submarine/SubCrewAnimInstance.cpp
Source/Sub3D/Submarine/CrewLocomotionTypes.h
Source/Sub3D/Submarine/SubCrewNetTypes.h
Source/Sub3D/Submarine/SubCrewNetTypes.cpp
Source/Sub3D/Submarine/AnimNode_CrewProcedural.h
Source/Sub3D/Submarine/AnimNode_CrewProcedural.cpp
Source/Sub3D/Submarine/AnimGraphNode_CrewProcedural.h
Source/Sub3D/Submarine/SubInteractionComponent.h
Source/Sub3D/Submarine/SubInteractionComponent.cpp
Source/Sub3D/Submarine/SubHullBoundaryComponent.h
Source/Sub3D/Submarine/SubHullBoundaryComponent.cpp
Source/Sub3D/Submarine/CompartmentVolumeComponent.h
Source/Sub3D/Submarine/CompartmentVolumeComponent.cpp
Source/Sub3D/Submarine/SubPlayerController.h
Source/Sub3D/Submarine/SubPlayerController.cpp
Source/Sub3D/Submarine/CrewUnderwaterPPComponent.h
Source/Sub3D/Submarine/CrewUnderwaterPPComponent.cpp
Source/Sub3D/Submarine/CrewAnimDebugWidget.h
Source/Sub3D/Submarine/CrewAnimDebugWidget.cpp
```

### Assets UE character/input

```text
Content/Sub3D/Blueprint/PlayerBP/BP_SubmarineCrew.uasset
Content/Sub3D/Blueprint/PlayerBP/PC_SubPlayerController.uasset
Content/Sub3D/Characters/Animation/ABP_Crew.uasset
Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic.uasset
Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic_Skeleton.uasset
Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic_PhysicsAsset.uasset
Content/Sub3D/Characters/Materials/M_Crew_Master.uasset
Content/Sub3D/Characters/Materials/MI_Crew_Accent.uasset
Content/Sub3D/Characters/Materials/MI_Crew_Boot.uasset
Content/Sub3D/Characters/Materials/MI_Crew_Skin.uasset
Content/Sub3D/Characters/Materials/MI_Crew_Suit.uasset
Content/Sub3D/Characters/Materials/MI_Crew_SuitDk.uasset
Content/Sub3D/Input/IMC_OnFoot.uasset
Content/Sub3D/Input/IMC_Helm.uasset
Content/Sub3D/Input/IMC_StationUI.uasset
Content/Sub3D/Input/ONFOOT/IA_Move.uasset
Content/Sub3D/Input/ONFOOT/IA_Look.uasset
Content/Sub3D/Input/ONFOOT/IA_Interact.uasset
Content/Sub3D/Input/ONFOOT/IA_Repair.uasset
Content/Sub3D/Input/ONFOOT/IA_Run.uasset
Content/Sub3D/Input/IA_PostureScroll.uasset
Content/Sub3D/Input/IA_ToggleCamera.uasset
```

### Scripts

```text
Art/Characters/Export/SK_Crew_Basic.fbx
Scripts/Blender/export_crew_fbx.py
Scripts/Blender/apply_crew_materials.py
Scripts/Blender/blender_gen_crew_basemesh.py.txt
Scripts/Blender/hull_blockout/character_crew.py
Scripts/Blender/hull_blockout/character_crew_v6_validated.py
Scripts/Blender/hull_blockout/character_bodies.py
Scripts/Blender/hull_blockout/character_accessories.py
Scripts/Blender/hull_blockout/character_kit.py
Scripts/UE5/create_crew_materials.py
Scripts/UE5/assign_crew_materials.py
Scripts/UE5/wire_abp_modify_bones.py
```

---

## Appendix B - Questions ouvertes pour planner externe

Ces questions ne sont pas des fixes proposes dans ce rapport. Elles indiquent les zones ou une decision d'architecture est necessaire.

1. Est-ce que Sub3D veut garder un character 100% procedural, ou passer a un set authored minimal walk/run/crouch/prone/swim?
2. Est-ce que `USubCrewAnimInstance` doit rester le lieu principal du procedural, ou deleguer a Control Rig/IK Rig?
3. Est-ce que le skeleton custom Blender doit etre conserve, ou faut-il aligner sur UE mannequin/MetaHuman-compatible rig?
4. Est-ce que `ECrewControlMode` doit inclure `Swimming` comme mode input explicite?
5. Est-ce que flood swim et EVA swim sont un meme traversal domain ou deux domains distincts?
6. Est-ce que le PlayerController devient l'unique owner des inputs on-foot/swim/station?
7. Est-ce que LGA doit rester dans `USubCrewMovementComponent` ou etre separe en helper local frame reusable?
8. Est-ce que peer grid smoothing devient requis avant le prochain test multiplayer?
9. Est-ce que posture doit devenir enum/state first plutot que scalar first?
10. Est-ce que IK doit etre applique dans ABP graph, Control Rig, ou dans le node C++ procedural?

---

## Appendix C - Root cause candidates ranking

### Bras penches a droite

| Rang | Candidate | Pourquoi | Verification |
|---:|---|---|---|
| 1 | Arm rest roll hardcode incompatible avec bone axes | applique chaque frame, valeur tres forte `+/-85` | mettre ArmRest/ForearmRest a 0 |
| 2 | Bone roll asymetrique upperarm/clavicle | FBX montre transforms non triviales | skeleton viewer axes |
| 3 | Additive rotation order/local space | pre-multiply quaternion sur pose locale | inverser ordre en test local branch |
| 4 | Import axis settings | Blender `-Y/Z`, root scaling signe negatif | Reimport Options |
| 5 | AnimGraph layer supplementaire | ABP binaire non lu completement | inspect ABP |
| 6 | Mesh component rotation | BP transform non lu statiquement | inspect BP components |
| 7 | Retargeting | pas de source retarget trouvee | skeleton retarget manager |

### Locomotion mauvaise

| Rang | Candidate | Pourquoi |
|---:|---|---|
| 1 | Pas d'anims authored | uniquement procedural trouve |
| 2 | Foot IK non applique | traces existent, node ne translate pas feet |
| 3 | Pas de gait contact model | sine cycle simple |
| 4 | Posture scalar | crouch/prone derivees, non authorées |
| 5 | Camera/control yaw coupling | body/upper aim dependent controller |

---

## Appendix D - Conclusion factuelle

Le systeme crew actuel est coherent pour valider le FP de circulation dans un sous-marin mobile: LGA, compartments, hull crossing, water immersion et procedural anim existent. Il n'est pas encore une architecture animation solide.

Le bug des bras n'est pas un symptome isole. Il apparait dans un pipeline ou:

- le skeleton vient d'un FBX Blender custom;
- les axes bones sont deja compenses par des params C++;
- les bras recoivent une rotation de repos forte chaque frame;
- les clavicles ne sont pas controlees par le node procedural;
- l'AnimGraph exact reste a valider;
- aucune animation authored de base n'a ete trouvee.

Pour planifier une refonte, les donnees critiques sont donc:

```text
Submarine frame and crew locomotion are already structurally separated.
Animation quality is the weak layer.
Input ownership is not yet clean.
Swimming is not yet a control mode.
IK data exists but final application is uncertain.
```

