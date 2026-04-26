# Sub3D Crew Character Pipeline — Architecture Consolidee

**Date :** 2026-04-13
**Auteur :** Plan co-defini avec Claude, decisions architecturales du creative director.

---

## Resume executif

Ce document definit le pipeline complet du personnage jouable de Sub3D, de la modelisation Blender jusqu'a l'animation procedurale en jeu. Le personnage est un membre d'equipage de sous-marin en style voxel (Deep Rock Galactic / Teardown), jouable en FPS (defaut) et TPS (toggle).

**Logique globale :**
1. Un seul body type base (mesh voxel 180cm, T-pose) avec variations d'outfits par couleurs
2. Accessoires modulaires (visage, cheveux, barbe, equipement tete) en Static Mesh sur sockets
3. Rig Blender manuel compatible UE5 Mannequin (retarget possible)
4. Animations : base locomotion (idle, walk, swim) + fort layer procedural (pitch sous-marin, inertie, mains IK sur murs, posture dynamique)
5. Camera FPS detachee du skeleton (capsule) + TPS SpringArm avec collision interieur sub
6. Posture continue via scroll wheel (rampant → accroupi → debout)
7. Upper body oriente vers la camera, lower body suit la direction de deplacement (rotation clampee)

**Ce qui existe deja dans le code :**
- `ASubCrewCharacter` : pawn FPS fonctionnel, camera Z=70, capsule ~88cm, interaction, boarding, pression, immersion eau
- `USubCrewMovementComponent` : tracking frame sous-marin, compensation inertielle, detection sol, brace support (1 probe 90cm)
- `ASubPlayerController` : 3 modes de controle (OnFoot, HelmDriving, StationUI), input routing complet
- `USubMovementComponent` : physique sous-marin avec pitch/roll/yaw, vitesse, acceleration — tout disponible pour le procedural
- Enhanced Input configure, replication en place

**Ce qui n'existe PAS encore :**
- Mesh character custom (utilise Mannequin par defaut)
- Skeleton/Rig
- AnimBlueprint / AnimInstance
- Systeme de posture
- IK mains
- Camera TPS
- Systeme d'apparence / customisation

---

## Phase 0 — Assets Blender (FAIT)

### 0.1 Base body mesh
- Script : `character_crew.py` (v6 validated)
- 2cm voxels, 180cm, T-pose, tete chauve
- 5 outfits multi-materiaux : Crew, Engineer, Captain, Diver, Medic

### 0.2 Accessoires
- Script : `character_accessories.py`
- 6 visages relief (0.5cm voxels) : Stoic, Worried, Unhinged, Hardened, Visor, Blank
- 11 coupes de cheveux (2cm) : Bald, Buzzcut, Mohawk, Curly, SidePart, Braids, Long, Ponytail, Dreads, Receding, Messy
- 5 barbes (2cm) : Stubble, Full, Mustache, Goatee, Mutton
- 5 equipements tete (2cm) : HardHat, Headlamp, Beanie, Headset, GasMask

---

## Phase 1 — Rig Blender (manuel)

### 1.1 Skeleton
**Decision : compatible UE5 Mannequin.** Nommage des bones identique au skeleton UE5 pour permettre le retarget d'animations marketplace/Mixamo comme base de blend.

**Bones principaux (nommage UE5) :**
```
root
  pelvis
    spine_01
      spine_02
        spine_03
          spine_04
            spine_05
              neck_01
                neck_02
                  head
              clavicle_l → upperarm_l → lowerarm_l → hand_l
                (index/middle/ring/pinky/thumb chains)
              clavicle_r → upperarm_r → lowerarm_r → hand_r
    thigh_l → calf_l → foot_l → ball_l
    thigh_r → calf_r → foot_r → ball_r
```

**Joints de reference (positions dans le mesh voxel, cm) :**
```
pelvis:     (0, 0, 97)
spine_02:   (0, 0, 117)
spine_04:   (0, 0, 137)
neck_01:    (0, 0, 151)
head:       (0, 0, 161)
clavicle_r: (0, 16, 147)
upperarm_r: (0, 22, 147)   → T-pose horizontal
lowerarm_r: (0, 42, 147)
hand_r:     (0, 58, 147)
thigh_r:    (0, 10, 93)
calf_r:     (0, 10, 50)
foot_r:     (0, 10, 8)
ball_r:     (8, 10, 0)
```

### 1.2 Weight painting
- Automatique (Blender auto-weights) puis nettoyage manuel sur les zones critiques : epaules, hanches, coudes
- Le style voxel pardonne beaucoup — les deformations parfaites ne sont pas necessaires

### 1.3 Procedure Blender
1. Ouvrir `character_crew.py` output dans Blender
2. Ajouter Armature avec les bones nommes ci-dessus aux positions de reference
3. Parent mesh → Armature avec Automatic Weights
4. Tester : poser le bras, verifier pas de deformation cassee
5. Exporter : FBX, Selected Objects (mesh+armature), Scale 1.0, -Y Forward, Z Up, Only Deform Bones ON, Add Leaf Bones OFF

---

## Phase 2 — Import UE5 et Materials

### 2.1 Import
- Drag `SK_Crew_Basic.fbx` dans `Content/Sub3D/Characters/Meshes/Bodies/`
- Skeleton : None (premiere fois → cree `SKEL_Crew`)
- Variants (Engineer, Captain, etc.) : meme skeleton `SKEL_Crew`
- Accessoires : import comme Static Mesh dans `Faces/`, `Hair/`, `Beards/`, `Equipment/`

### 2.2 Material Architecture

**Master Material : `M_Crew_Master`**
```
Params:
  BaseColor (Vector3)
  Roughness (Scalar, default 0.85)
  Metallic  (Scalar, default 0.0)
Shading: Default Lit, Two-Sided OFF
```

**Material Instances (6 slots par body) :**
| Slot | Index | Exemple |
|------|-------|---------|
| SKIN | 0 | MI_Crew_Skin_Default (0.72, 0.55, 0.42) |
| SUIT | 1 | MI_Crew_Suit_Green (0.15, 0.28, 0.22) |
| SUIT_DK | 2 | MI_Crew_SuitDk (0.07, 0.16, 0.12) |
| BOOT | 3 | MI_Crew_Boot (0.08, 0.07, 0.06) |
| HAIR | 4 | MI_Crew_Hair (0.20, 0.12, 0.06) |
| ACCENT | 5 | MI_Crew_Accent (per-outfit) |

### 2.3 Sockets sur le skeleton
| Socket | Bone | Offset | Usage |
|--------|------|--------|-------|
| socket_face | head | (0,0,0) | SM_Face_* |
| socket_hair | head | (0,0,0) | SM_Hair_* |
| socket_beard | head | (0,0,0) | SM_Beard_* |
| socket_equip_head | head | (0,0,0) | SM_Equip_* |
| socket_equip_back | spine_02 | (-15,0,0) | Futur: sac, tank O2 |

### 2.4 Naming Convention
```
Content/Sub3D/Characters/
  Meshes/Bodies/     SK_Crew_{Outfit}.uasset
  Meshes/Faces/      SM_Face_{Type}.uasset
  Meshes/Hair/       SM_Hair_{Style}.uasset
  Meshes/Beards/     SM_Beard_{Type}.uasset
  Meshes/Equipment/  SM_Equip_{Type}.uasset
  Skeleton/          SKEL_Crew.uasset
  Animations/        ABP_Crew.uasset, CR_Crew_HandIK.uasset
  Materials/         M_Crew_Master.uasset, MI_Crew_*.uasset
  Data/              DA_CrewLook_*.uasset, DT_CrewAppearancePool.uasset
```

---

## Phase 3 — Animation Blueprint (ABP_Crew)

### 3.1 Architecture de l'AnimGraph

```
┌──────────────────────────────────────────────────────┐
│ AnimGraph                                             │
│                                                       │
│  ┌─────────────┐    ┌──────────────┐                 │
│  │ Locomotion  │───→│ Additive     │                 │
│  │ StateMachine│    │ Sub Motion   │                 │
│  └─────────────┘    │ (pitch/roll/ │                 │
│                     │  inertia)    │                 │
│                     └──────┬───────┘                 │
│                            │                         │
│                     ┌──────▼───────┐                 │
│                     │ Upper/Lower  │                 │
│                     │ Body Split   │                 │
│                     │ (Layered     │                 │
│                     │  Blend)      │                 │
│                     └──────┬───────┘                 │
│                            │                         │
│                     ┌──────▼───────┐                 │
│                     │ Posture      │                 │
│                     │ Blend        │                 │
│                     │ (Debout→     │                 │
│                     │  Ramper)     │                 │
│                     └──────┬───────┘                 │
│                            │                         │
│                     ┌──────▼───────┐                 │
│                     │ Control Rig  │                 │
│                     │ (Hand IK +   │                 │
│                     │  Foot IK +   │                 │
│                     │  Look At)    │                 │
│                     └──────────────┘                 │
└──────────────────────────────────────────────────────┘
```

### 3.2 Locomotion State Machine
```
Idle ←→ Walk ←→ Run
  ↓       ↓
Swim ←→ Tread
```
- **Idle** : pose neutre, leger balancement procedural
- **Walk** : animation de base (retargetee ou custom), vitesse blendee via BlendSpace 1D (0-300 cm/s)
- **Swim** : bras en brasse, procedural leg kick
- **Transitions** : basees sur `CharacterMovementComponent::Velocity.Size()` et `bIsSwimmingByFlood`

### 3.3 Additive Sub Motion (procedural)
Layer additif qui lit les donnees de `USubCrewMovementComponent` :

| Donnee source | Effet sur le corps |
|---|---|
| `LocalSubAngularVelocityDegrees.Y` (pitch) | Lean avant/arriere du upper body |
| `LocalSubAngularVelocityDegrees.X` (roll) | Lean lateral |
| `LocalSubLinearAcceleration` (forward) | Stumble forward/backward |
| `LocalSubLinearAcceleration` (lateral) | Stumble lateral |
| `SupportQuality01` | Amplitude du stumble (moins de support = plus d'oscillation) |
| `bHasNearbyBraceSupport` | Active le hand IK vers le brace point |

**Implementation** : `UAnimInstance` custom (`USubCrewAnimInstance`) qui lit ces valeurs chaque tick et les expose comme variables pour l'AnimGraph.

### 3.4 Upper/Lower Body Split
- **Bone split** au `spine_03` (taille)
- **Upper body** : oriente vers la camera (aim offset ou rotation procedurale). Clamp : ±90° yaw, ±45° pitch par rapport au lower body
- **Lower body** : suit la direction de deplacement (`Velocity.GetSafeNormal()`)
- **Blend** : quand l'ecart upper/lower depasse le clamp, le lower body pivote pour rattraper
- En idle (vitesse 0) : le lower body s'aligne lentement vers le upper

### 3.5 Posture System (scroll wheel)

**Variable** : `PostureAlpha` float 0→1 (0 = ramper, 0.5 = accroupi, 1 = debout)

**Effets par alpha :**
| Alpha | Capsule HalfHeight | Camera Z | Walk Speed Scale |
|---|---|---|---|
| 0.0 (ramper) | 30 cm | 25 cm | 0.3 |
| 0.5 (accroupi) | 50 cm | 55 cm | 0.6 |
| 1.0 (debout) | 88 cm | 70 cm | 1.0 |

**Interpolation** : `FMath::FInterpTo` a 5.0/s pour la capsule, instantane pour le scroll input
**Animation** : BlendSpace 1D parametre par `PostureAlpha`, blend entre pose debout → accroupi → ramper

**C++ (USubCrewMovementComponent) :**
```cpp
// Nouveau dans SubCrewMovementComponent
UPROPERTY(Replicated) float PostureAlpha = 1.0f;  // 0=prone, 1=standing
void SetPostureTarget(float Alpha);  // Clamped 0-1
void TickPosture(float DeltaTime);   // Interp capsule + camera Z
```

**Input** : scroll wheel envoie `+0.1` / `-0.1` par tick, clampe [0, 1]

---

## Phase 4 — Camera System

### 4.1 FPS (defaut)
- **Inchange** : `UCameraComponent` sur capsule root, Z interpole par PostureAlpha
- Head mesh cache en FPS (`SetOwnerNoSee(true)`)
- Pas de bobbing procedurale excessive (sous-marin = deja beaucoup de mouvement ambiant)

### 4.2 TPS (toggle)
- **Nouveau** : `USpringArmComponent` + `UCameraComponent` secondaire
- Attache au root, target offset Z = capsule height
- SpringArm length : 150-250cm (configurable)
- **Collision** : `bDoCollisionTest = true`, probe channel = `ECC_GameTraceChannel2` (interieur sous-marin)
- Camera lag : enable, lag speed 10.0 (smooth mais reactive)
- En TPS : head mesh visible, arms mesh visible

### 4.3 Switch FPS ↔ TPS
- Input : touche dediee (ex: V)
- Transition : `SetActive(true/false)` sur chaque camera
- `ASubCrewCharacter::bIsFirstPerson` bool, replique pour le mesh visibility des autres joueurs

---

## Phase 5 — Hand IK & Procedural Contact

### 5.1 Probe System (C++)
Extension de `USubCrewMovementComponent`, remplace le probe unique actuel :

```cpp
struct FHandIKProbeResult
{
    bool bHit;
    FVector WorldLocation;
    FVector WorldNormal;
    float Distance;
};

// 6 probes: LeftHand, RightHand, LeftShoulder, RightShoulder, LeftHip, RightHip
UPROPERTY() FHandIKProbeResult HandProbes[6];

void UpdateHandIKProbes();  // Called each tick
```

**Directions de probe :**
| Probe | Origine (relative au capsule) | Direction |
|---|---|---|
| LeftHand | (0, -30, PostureZ * 0.8) | Left + slight Forward |
| RightHand | (0, +30, PostureZ * 0.8) | Right + slight Forward |
| LeftShoulder | (0, -20, PostureZ * 0.9) | Left |
| RightShoulder | (0, +20, PostureZ * 0.9) | Right |
| LeftHip | (0, -15, PostureZ * 0.4) | Left |
| RightHip | (0, +15, PostureZ * 0.4) | Right |

**Distance** : 60cm (bras etendu realiste)
**Channel** : `ECC_GameTraceChannel2` (interieur sous-marin)

### 5.2 Control Rig (CR_Crew_HandIK)
- **Two Bone IK** par bras : target = probe WorldLocation si hit
- **Blend weight** : 0 si pas de hit, ramp up 0→1 sur 0.2s quand hit, ramp down sur 0.3s quand lost
- **Priorite** : Hand probe > Shoulder probe (main touche d'abord, epaule en dernier recours)
- **Doigts** : en posture paume ouverte a plat quand IK actif, poing au repos
- **Foot IK** : standard Two Bone IK pour foot placement sur les surfaces irregulieres du sub

### 5.3 Conditions d'activation
- IK actif seulement si `SupportQuality01 < 0.8` OU `LocalSubAngularVelocity.Size() > 2.0` (le sub bouge significativement)
- En mode Helm : IK desactive (mains sur les commandes)
- En mode Station : IK desactive

---

## Phase 6 — Character Appearance System

### 6.1 UCrewAppearanceComponent (C++)
```cpp
UCLASS()
class UCrewAppearanceComponent : public UActorComponent
{
    UPROPERTY() USkeletalMeshComponent* BodyMesh;
    UPROPERTY() UStaticMeshComponent* FaceMesh;
    UPROPERTY() UStaticMeshComponent* HairMesh;
    UPROPERTY() UStaticMeshComponent* BeardMesh;
    UPROPERTY() UStaticMeshComponent* EquipMesh;

    UFUNCTION(BlueprintCallable)
    void ApplyAppearance(const FCrewAppearanceState& State);

    UFUNCTION(BlueprintCallable)
    void RandomizeFromPool(UCrewAppearancePool* Pool);

    UPROPERTY(ReplicatedUsing=OnRep_Appearance)
    FCrewAppearanceState CurrentAppearance;
};
```

### 6.2 Randomisation
- Serveur choisit l'apparence au spawn dans `ASubGameMode`
- Replique `FCrewAppearanceState` (soft object paths)
- `OnRep` charge et applique les meshes cote client
- Seed deterministe par index joueur = consistent cross-clients

---

## Phase 7 — Integration finale

### 7.1 Modifications a ASubCrewCharacter
- Ajouter `UCrewAppearanceComponent`
- Ajouter `USpringArmComponent` + cam TPS
- Ajouter `bIsFirstPerson` bool (replique)
- Camera Z suit `PostureAlpha` en continu

### 7.2 Modifications a USubCrewMovementComponent
- Ajouter `PostureAlpha`, `SetPostureTarget()`, `TickPosture()`
- Etendre le probe system : `FHandIKProbeResult HandProbes[6]`
- `UpdateHandIKProbes()` chaque tick

### 7.3 Modifications a ASubPlayerController
- Input mapping scroll wheel → `PostureAlpha`
- Input mapping touche V → toggle FPS/TPS
- Routing dans l'AnimBP via le `USubCrewAnimInstance`

### 7.4 AnimBlueprint (ABP_Crew)
- `USubCrewAnimInstance` custom C++ qui lit :
  - `PostureAlpha`
  - `Speed`
  - `bIsSwimming`
  - `SubPitchDeg`, `SubRollDeg` (du movement component)
  - `SubAccelForward`, `SubAccelLateral`
  - `HandIKTargets[2]` (world positions)
  - `bHandIKActive[2]`
  - `UpperBodyYawOffset` (ecart upper/lower body)
- AnimGraph construit comme decrit en Phase 3

---

## Ordre d'implementation recommande

| Etape | Contenu | Prerequis |
|---|---|---|
| **1** | Export 1 body FBX depuis Blender, rig manuel, reimport | Phase 0 (fait) |
| **2** | Import UE5, creer SKEL_Crew, materials basiques | Etape 1 |
| **3** | Posture system (PostureAlpha, capsule dynamique, scroll input) | Etape 2 |
| **4** | ABP_Crew basique (idle + walk blend, posture blend) | Etape 2 |
| **5** | Camera TPS + switch FPS/TPS | Etape 2 |
| **6** | Upper/Lower body split + aim offset | Etape 4 |
| **7** | Sub motion additive (pitch/roll/inertia) | Etape 4 |
| **8** | Hand IK probe system + Control Rig | Etape 4 + 7 |
| **9** | Appearance component + randomisation | Etape 2 |
| **10** | Export tous les accessoires + tous les outfits | Etape 1 + 9 |

**Pour le First Playable** : etapes 1-4 suffisent. Le reste est polish post-FP mais l'architecture est posee pour ne pas avoir a refactorer.

---

## Fichiers source impactes

| Fichier | Modifications |
|---|---|
| `SubCrewCharacter.h/cpp` | + AppearanceComponent, + SpringArm TPS, + bIsFirstPerson, camera Z dynamique |
| `SubCrewMovementComponent.h/cpp` | + PostureAlpha, + TickPosture, + HandProbes[6], + UpdateHandIKProbes |
| `SubPlayerController.h/cpp` | + input scroll posture, + input toggle camera |
| **Nouveau** `SubCrewAnimInstance.h/cpp` | AnimInstance custom, lit movement data |
| **Nouveau** `CrewAppearanceComponent.h/cpp` | Systeme d'apparence modulaire |
| **Nouveau** `CrewAppearanceData.h` | Data Asset definition |
| `ABP_Crew` (Blueprint) | AnimGraph complet |
| `CR_Crew_HandIK` (Blueprint) | Control Rig pour IK mains/pieds |
