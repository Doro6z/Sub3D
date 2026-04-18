# Proto04A — Spec Technique : Compilateur de Sous-Marin (Outil Interne)

Projet : Sub3D
Date : 2026-03-26
Status : Reference d'execution
Scope : **Proto04A = outil interne developpeur.** Pas un editeur joueur. Pas d'UX polish.
Parent : `2026-03-26_sub3d_proto04_submarine_compiler_architecture.md`

---

## 0. Cadrage

### Ce qu'est Proto04A

Un outil interne qui permet de :
1. Definir la structure d'un sous-marin via des DataAssets (graphe fonctionnel + enveloppe)
2. Resoudre le layout (solver)
3. Generer la geometrie procedurale (ProceduralMesh)
4. Produire un `USubmarineLayoutAsset` consommable par le runtime existant
5. Entrer dans le sous-marin en PIE pour valider

### Ce que Proto04A n'est PAS

- Pas un editeur joueur avec drag & drop
- Pas une UI polish
- Pas un systeme de sauvegarde/chargement de designs
- Pas un systeme de progression ou d'achat
- Pas un generateur de coque exterieure detaillee

### Niveau d'autonomie IA

**Forte sous contraintes.** L'IA peut implementer, compiler, fixer, tester les invariants machine. Mais :
- Les taches floues ou dependantes du ressenti necessitent une validation humaine explicite
- Chaque tache separe les **invariants machine** (verifiables par code) des **validations editeur** (verifiables par un humain dans UE5)
- Chaque tache liste des **non-objectifs** pour eviter le scope creep

### Contrat runtime a preserver

Proto04A doit rester compatible avec le runtime actuel tant que `SubHullComponent`
consomme seulement `Compartments` et `StructuralSheets`.

Regles verrouillees :
- Pour toute porte watertight generee, `DoorId == BulkheadSheetId` en Proto04A.
- Les `RequiredSystems` et `StationSlots` utilisent les valeurs de `ESubStationType`
  existantes (`Helm`, `Engine`, `Pump`, `Ballast`, `Turret`) et non des alias comme
  `HelmStation` ou `BallastStation`.
- Les `StructuralSheets` restent la source de verite de simulation. La geometrie
  procedurale s'aligne dessus ; elle ne les definit pas.
- La politique de couverture des sheets doit etre explicite dans le compilateur.
  Pour le MVP Proto04A : `Port`, `Starboard`, `Top`, `Bottom` par compartiment,
  plus une sheet de cloison par frontiere de compartiment. Les caps `Bow` et `Stern`
  sont hors scope du MVP tant qu'ils ne sont pas necessaires au gameplay teste.

---

## 1. Structures de donnees C++

Tous les fichiers dans `Source/Sub3D/SubCompiler/`.

### 1.1 Enums

**Fichier : `SubCompilerTypes.h`**

```cpp
UENUM(BlueprintType)
enum class ECompartmentType : uint8
{
    Helm        UMETA(DisplayName = "Helm"),
    Engine      UMETA(DisplayName = "Engine"),
    Ballast     UMETA(DisplayName = "Ballast"),
    Airlock     UMETA(DisplayName = "Airlock"),
    Corridor    UMETA(DisplayName = "Corridor"),
    Storage     UMETA(DisplayName = "Storage"),
    Crew        UMETA(DisplayName = "Crew"),
    Medical     UMETA(DisplayName = "Medical")
};

UENUM(BlueprintType)
enum class EPassageType : uint8
{
    WatertightDoor   UMETA(DisplayName = "Watertight Door"),
    Hatch            UMETA(DisplayName = "Hatch"),
    Open             UMETA(DisplayName = "Open"),
    SealedBulkhead   UMETA(DisplayName = "Sealed Bulkhead")
};

UENUM(BlueprintType)
enum class EWallSide : uint8
{
    Port        UMETA(DisplayName = "Port"),
    Starboard   UMETA(DisplayName = "Starboard"),
    Bow         UMETA(DisplayName = "Bow"),
    Stern       UMETA(DisplayName = "Stern"),
    Floor       UMETA(DisplayName = "Floor"),
    Ceiling     UMETA(DisplayName = "Ceiling")
};

UENUM(BlueprintType)
enum class ELayoutValidationSeverity : uint8
{
    OK,
    Warning,
    Error
};
```

### 1.2 Structs — Graphe fonctionnel

**Dans `SubCompilerTypes.h`**

```cpp
USTRUCT(BlueprintType)
struct FCompartmentNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CompartmentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ECompartmentType Type = ECompartmentType::Corridor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100.0"))
    float MinLengthCm = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "80.0"))
    float MinWidthCm = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "180.0"))
    float MinHeightCm = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<ESubStationType> RequiredSystems;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CrewCapacity = 2;

    // 0 = bow, higher = stern. Determines placement order on spine.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FPassageEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName FromCompartmentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ToCompartmentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EPassageType Type = EPassageType::WatertightDoor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "60.0"))
    float MinWidthCm = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "140.0"))
    float MinHeightCm = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float PressureRatingATM = 10.f;
};
```

### 1.3 Structs — Layout Solution

**Dans `SubCompilerTypes.h`**

```cpp
USTRUCT(BlueprintType)
struct FCompartmentPlacement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName CompartmentId;

    UPROPERTY(BlueprintReadOnly)
    ECompartmentType Type = ECompartmentType::Corridor;

    // Position along the spine axis (X in local space)
    UPROPERTY(BlueprintReadOnly)
    float SpineStartCm = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float SpineEndCm = 0.f;

    // Envelope radius at this spine segment (average of start/end)
    UPROPERTY(BlueprintReadOnly)
    float EffectiveRadiusCm = 180.f;

    // Floor position relative to spine center (negative = below center)
    UPROPERTY(BlueprintReadOnly)
    float FloorOffsetCm = -90.f;

    // Usable interior height
    UPROPERTY(BlueprintReadOnly)
    float ClearanceHeightCm = 200.f;

    // Usable interior width at floor level
    UPROPERTY(BlueprintReadOnly)
    float FloorWidthCm = 300.f;
};

USTRUCT(BlueprintType)
struct FBulkheadPlacement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float SpinePositionCm = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float RadiusCm = 180.f;

    UPROPERTY(BlueprintReadOnly)
    EPassageType PassageType = EPassageType::WatertightDoor;

    // Door center offset within the bulkhead disc (Y = lateral, Z = vertical)
    UPROPERTY(BlueprintReadOnly)
    FVector2D DoorOffsetCm = FVector2D(0.f, 0.f);

    UPROPERTY(BlueprintReadOnly)
    float DoorWidthCm = 90.f;

    UPROPERTY(BlueprintReadOnly)
    float DoorHeightCm = 180.f;

    // Which compartments this bulkhead separates
    UPROPERTY(BlueprintReadOnly)
    FName ForeCompartmentId;

    UPROPERTY(BlueprintReadOnly)
    FName AftCompartmentId;
};

USTRUCT(BlueprintType)
struct FStationPlacement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ESubStationType StationType;

    UPROPERTY(BlueprintReadOnly)
    FName CompartmentId;

    UPROPERTY(BlueprintReadOnly)
    FTransform LocalTransform;

    UPROPERTY(BlueprintReadOnly)
    EWallSide WallSide = EWallSide::Port;

    // Clearance zone in front of the station (width, depth)
    UPROPERTY(BlueprintReadOnly)
    FVector2D ClearanceRectCm = FVector2D(100.f, 120.f);
};

USTRUCT(BlueprintType)
struct FLayoutValidationMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ELayoutValidationSeverity Severity = ELayoutValidationSeverity::OK;

    UPROPERTY(BlueprintReadOnly)
    FName RelatedId;

    UPROPERTY(BlueprintReadOnly)
    FText Message;
};

USTRUCT(BlueprintType)
struct FSubmarineBuildMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float TotalLengthCm = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float EstimatedMassKg = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float EstimatedVolumeLiters = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float BallastCapacityLiters = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 CompartmentCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 DoorCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 StationCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalCrewCapacity = 0;
};
```

### 1.4 DataAssets

**Fichier : `SubmarineEnvelopeDef.h`**

```cpp
UCLASS(BlueprintType)
class SUB3D_API USubmarineEnvelopeDef : public UDataAsset
{
    GENERATED_BODY()
public:
    // Total length of the spine in cm
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "500.0"))
    float SpineLengthCm = 1520.f;

    // Radius as function of normalized spine position (0=bow, 1=stern)
    // Y-axis = radius in cm
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FRuntimeFloatCurve RadiusProfile;

    // Fallback if curve is empty
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "50.0"))
    float DefaultRadiusCm = 180.f;

    // Maximum number of compartments this envelope can hold
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 MaxCompartments = 6;

    // Evaluate radius at normalized spine position [0,1]
    float EvaluateRadius(float NormalizedPosition) const;
};
```

**Fichier : `SubmarineFunctionalGraph.h`**

```cpp
UCLASS(BlueprintType)
class SUB3D_API USubmarineFunctionalGraph : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graph")
    TArray<FCompartmentNode> Compartments;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graph")
    TArray<FPassageEdge> Passages;

    // Validate graph integrity (no orphan nodes, no duplicate IDs, connectivity)
    bool ValidateGraph(TArray<FLayoutValidationMessage>& OutMessages) const;
};
```

### 1.5 Extensions a USubmarineLayoutAsset

**Fichier existant : `SubmarineLayoutAsset.h` — a etendre**

Champs a ajouter :

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Doors")
    TArray<FDoorDef> Doors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Stations")
    TArray<FStationSlotDef> StationSlots;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Metrics")
    FSubmarineBuildMetrics Metrics;
```

Avec :

```cpp
USTRUCT(BlueprintType)
struct FDoorDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName DoorId;

    // Proto04A runtime contract: DoorId must match BulkheadSheetId so the
    // existing flooding propagation can query door state by sheet id.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName BulkheadSheetId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EPassageType PassageType = EPassageType::WatertightDoor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform LocalTransform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float WidthCm = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HeightCm = 180.f;
};

USTRUCT(BlueprintType)
struct FStationSlotDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESubStationType StationType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CompartmentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform LocalTransform;
};
```

Regle de compilation Proto04A :
- `DoorId` et `BulkheadSheetId` doivent etre initialises avec la meme valeur
  pour toute porte watertight issue d'une cloison compilee.
- `StationType` doit reprendre directement les valeurs de `ESubStationType`
  definies dans `SubmarineTypes.h`.

---

## 2. Algorithme du solver

### 2.1 Vue d'ensemble

Le solver prend un `USubmarineEnvelopeDef` + `USubmarineFunctionalGraph` et produit une `FSubmarineLayoutSolution` contenant les placements de compartiments, cloisons, et stations.

**Fichier : `SubmarineLayoutSolver.h/.cpp`**

```cpp
UCLASS()
class SUB3D_API USubmarineLayoutSolver : public UObject
{
    GENERATED_BODY()
public:
    // Solve layout from inputs. Returns true if solution is valid (no errors, warnings allowed).
    bool Solve(
        const USubmarineEnvelopeDef* Envelope,
        const USubmarineFunctionalGraph* Graph,
        FSubmarineLayoutSolution& OutSolution,
        TArray<FLayoutValidationMessage>& OutMessages
    );
};
```

Ou `FSubmarineLayoutSolution` est :

```cpp
USTRUCT(BlueprintType)
struct FSubmarineLayoutSolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FCompartmentPlacement> Compartments;

    UPROPERTY(BlueprintReadOnly)
    TArray<FBulkheadPlacement> Bulkheads;

    UPROPERTY(BlueprintReadOnly)
    TArray<FStationPlacement> Stations;

    UPROPERTY(BlueprintReadOnly)
    FSubmarineBuildMetrics Metrics;

    UPROPERTY(BlueprintReadOnly)
    TArray<FLayoutValidationMessage> ValidationMessages;

    bool HasErrors() const;
    bool IsValid() const;
};
```

### 2.2 Pseudo-code du solver

```
FUNCTION Solve(Envelope, Graph) -> LayoutSolution:

    // ── STEP 1 : Validation du graphe ──────────────────────────
    Messages = Graph.ValidateGraph()
    IF Messages contient des Error:
        RETURN solution vide + messages

    // ── STEP 2 : Tri des compartiments ─────────────────────────
    SortedCompartments = Graph.Compartments triés par Priority croissant
    // Priority 0 = bow, Priority N = stern

    // ── STEP 3 : Calcul de l'espace disponible ────────────────
    TotalMinLength = SOMME(SortedCompartments[i].MinLengthCm)
    IF TotalMinLength > Envelope.SpineLengthCm:
        EMIT Error "Compartiments trop longs pour l'enveloppe
                     (besoin {TotalMinLength}cm, disponible {SpineLengthCm}cm)"
        RETURN
    IF SortedCompartments.Num() > Envelope.MaxCompartments:
        EMIT Error "Trop de compartiments ({N} > max {MaxCompartments})"
        RETURN

    SlackCm = Envelope.SpineLengthCm - TotalMinLength
    // Slack = espace restant a distribuer

    // ── STEP 4 : Placement sequentiel sur le spine ─────────────
    CurrentSpinePosition = 0.0
    FOR EACH Compartment IN SortedCompartments:
        // Distribuer le slack proportionnellement a MinLength
        BonusCm = SlackCm * (Compartment.MinLengthCm / TotalMinLength)
        ActualLengthCm = Compartment.MinLengthCm + BonusCm

        SpineStart = CurrentSpinePosition
        SpineEnd = CurrentSpinePosition + ActualLengthCm
        SpineMid = (SpineStart + SpineEnd) / 2.0

        // Evaluer le rayon d'enveloppe au milieu du compartiment
        NormalizedPos = SpineMid / Envelope.SpineLengthCm
        EnvelopeRadius = Envelope.EvaluateRadius(NormalizedPos)

        // Calculer le plancher (chord a mi-hauteur du cercle)
        // Le plancher est place de sorte que ClearanceHeight >= MinHeightCm
        // Dans un cercle de rayon R, si le plancher est a hauteur H sous le centre :
        //   ClearanceHeight = R + (R - H) ... simplifie : on place le floor
        //   pour maximiser la largeur au sol tout en garantissant MinHeightCm
        FloorOffset = -(EnvelopeRadius - Compartment.MinHeightCm / 2.0)
        FloorOffset = CLAMP(FloorOffset, -(EnvelopeRadius * 0.7), 0.0)

        // Largeur au sol = chord du cercle a la hauteur du plancher
        FloorY = EnvelopeRadius + FloorOffset  // distance du bas du cercle au floor
        IF FloorY <= 0 OR FloorY >= 2*EnvelopeRadius:
            EMIT Error "Compartiment {Id} : plancher hors enveloppe"
            CONTINUE
        FloorHalfWidth = SQRT(EnvelopeRadius^2 - (FloorOffset)^2)
        FloorWidth = FloorHalfWidth * 2.0

        ClearanceHeight = EnvelopeRadius - FloorOffset
        // le plafond est le haut du cercle = centre + R = FloorOffset + ClearanceHeight

        IF FloorWidth < Compartment.MinWidthCm:
            EMIT Warning "Compartiment {Id} : largeur au sol {FloorWidth}cm
                          < minimum {MinWidthCm}cm"

        IF ClearanceHeight < Compartment.MinHeightCm:
            EMIT Error "Compartiment {Id} : hauteur libre {ClearanceHeight}cm
                        < minimum {MinHeightCm}cm"

        EMIT Placement:
            CompartmentId = Compartment.CompartmentId
            SpineStartCm = SpineStart
            SpineEndCm = SpineEnd
            EffectiveRadiusCm = EnvelopeRadius
            FloorOffsetCm = FloorOffset
            ClearanceHeightCm = ClearanceHeight
            FloorWidthCm = FloorWidth

        CurrentSpinePosition = SpineEnd
    END FOR

    // ── STEP 5 : Generation des cloisons ───────────────────────
    FOR i = 0 TO SortedCompartments.Num() - 2:
        ForeComp = Placements[i]
        AftComp  = Placements[i + 1]
        SpinePos = ForeComp.SpineEndCm  // = AftComp.SpineStartCm

        // Trouver le passage correspondant dans le graphe
        Passage = Graph.FindPassage(ForeComp.CompartmentId, AftComp.CompartmentId)
        IF Passage == null:
            EMIT Warning "Pas de passage entre {ForeComp.Id} et {AftComp.Id}
                          — cloison scellee"
            PassageType = SealedBulkhead
        ELSE:
            PassageType = Passage.Type

        // Rayon de la cloison = min des rayons des deux compartiments
        BulkheadRadius = MIN(ForeComp.EffectiveRadiusCm, AftComp.EffectiveRadiusCm)

        DoorWidth = Passage ? Passage.MinWidthCm : 0.0
        DoorHeight = Passage ? Passage.MinHeightCm : 0.0

        // Porte centree lateralement, posee sur le plancher
        FloorLevel = MAX(ForeComp.FloorOffsetCm, AftComp.FloorOffsetCm)
        DoorCenterZ = FloorLevel + DoorHeight / 2.0

        EMIT BulkheadPlacement:
            SpinePositionCm = SpinePos
            RadiusCm = BulkheadRadius
            PassageType = PassageType
            DoorOffsetCm = (0.0, DoorCenterZ)
            DoorWidthCm = DoorWidth
            DoorHeightCm = DoorHeight
            ForeCompartmentId = ForeComp.CompartmentId
            AftCompartmentId = AftComp.CompartmentId
    END FOR

    // ── STEP 6 : Placement des stations ────────────────────────
    FOR EACH Compartment IN SortedCompartments:
        Placement = FindPlacement(Compartment.CompartmentId)
        FOR EACH StationType IN Compartment.RequiredSystems:
            // Place sur la paroi Port ou Starboard, en alternant
            WallSide = (StationIndex % 2 == 0) ? Port : Starboard

            // Position le long du spine : centre du compartiment
            StationX = (Placement.SpineStartCm + Placement.SpineEndCm) / 2.0

            // Position laterale : contre la paroi, face vers le centre
            WallY = (WallSide == Port) ? -(Placement.FloorWidthCm / 2.0 - 20.0)
                                        :  (Placement.FloorWidthCm / 2.0 - 20.0)
            StationZ = Placement.FloorOffsetCm

            // Rotation : face vers le centre du compartiment
            StationYaw = (WallSide == Port) ? 90.0 : -90.0

            // Verifier clearance : 120cm devant la station, pas de chevauchement
            // avec d'autres stations ou la porte la plus proche
            ClearanceOK = CheckClearance(StationX, WallY, ClearanceRect,
                                         ExistingStations, NearestDoor)
            IF NOT ClearanceOK:
                // Decaler le long du spine
                StationX += 80.0
                ClearanceOK = CheckClearance(...)
                IF NOT ClearanceOK:
                    EMIT Warning "Station {StationType} dans {CompartmentId} :
                                  clearance insuffisante"

            EMIT StationPlacement:
                StationType = StationType
                CompartmentId = Compartment.CompartmentId
                LocalTransform = Transform(StationX, WallY, StationZ, 0, StationYaw, 0)
                WallSide = WallSide
                ClearanceRectCm = (100.0, 120.0)

            StationIndex++
        END FOR
    END FOR

    // ── STEP 7 : Calcul des metriques ──────────────────────────
    Metrics.TotalLengthCm = Placements.Last().SpineEndCm
    Metrics.CompartmentCount = Placements.Num()
    Metrics.DoorCount = Bulkheads filtres par PassageType != SealedBulkhead
    Metrics.StationCount = Stations.Num()
    Metrics.TotalCrewCapacity = SOMME(Compartments[i].CrewCapacity)
    Metrics.EstimatedVolumeLiters = SOMME pour chaque compartiment :
        PI * EffectiveRadiusCm^2 * LengthCm / 1000.0  (approximation cylindrique)
    Metrics.EstimatedMassKg = EstimatedVolumeLiters * 0.8
        (approximation : coque acier 80% du volume en masse equivalente)

    RETURN Solution
END FUNCTION
```

### 2.3 Validation du graphe

```
FUNCTION ValidateGraph(Graph) -> Messages:

    // Pas d'ID vide
    FOR EACH Comp IN Compartments:
        IF Comp.CompartmentId.IsNone():
            EMIT Error "Compartiment avec ID vide"

    // Pas d'ID dupliques
    IF duplicates in Compartments[].CompartmentId:
        EMIT Error "IDs dupliques : {list}"

    // Chaque passage reference des compartiments existants
    FOR EACH Passage IN Passages:
        IF Passage.FromCompartmentId not in Compartments:
            EMIT Error "Passage reference compartiment inconnu {From}"
        IF Passage.ToCompartmentId not in Compartments:
            EMIT Error "Passage reference compartiment inconnu {To}"

    // Connectivite : tous les compartiments atteignables depuis le premier
    Visited = BFS depuis Compartments[0] via Passages
    IF Visited.Num() < Compartments.Num():
        Unreachable = Compartments - Visited
        EMIT Error "Compartiments isoles : {list}"

    // Au moins 2 compartiments
    IF Compartments.Num() < 2:
        EMIT Error "Minimum 2 compartiments requis"

    // Priorities uniques (pas obligatoire mais Warning si dupliquees)
    IF duplicates in Compartments[].Priority:
        EMIT Warning "Priorities dupliquees — ordre de placement ambigu"

END FUNCTION
```

---

## 3. Build Recipe Compiler

**Fichier : `SubmarineBuildCompiler.h/.cpp`**

Prend une `FSubmarineLayoutSolution` et produit un `USubmarineLayoutAsset` rempli.

```
FUNCTION CompileBuildRecipe(Solution, Envelope) -> USubmarineLayoutAsset:

    Asset = new USubmarineLayoutAsset

    // ── Compartiments ──────────────────────────────────────────
    FOR EACH Placement IN Solution.Compartments:
        FSubCompartmentDef Def
        Def.CompartmentId = Placement.CompartmentId
        Def.DisplayName = FText from CompartmentId
        Def.CapacityLiters = PI * (Placement.EffectiveRadiusCm/100)^2
                             * (Placement.SpineEndCm - Placement.SpineStartCm)/100 * 1000
        Asset.Compartments.Add(Def)

    // ── Structural Sheets (coques exterieures) ─────────────────
    FOR EACH Placement IN Solution.Compartments:
        Length = Placement.SpineEndCm - Placement.SpineStartCm
        MidX = (Placement.SpineStartCm + Placement.SpineEndCm) / 2.0
        R = Placement.EffectiveRadiusCm

        // Sheet Port
        AddSheet(
            Id = "{CompartmentId}_Port",
            ParentCompartment = Placement.CompartmentId,
            AdjacentCompartment = NAME_None,
            Origin = (MidX, -R, 0),
            Normal = (0, -1, 0),
            TangentX = (1, 0, 0),
            TangentY = (0, 0, 1),
            Size = (Length, R * 2.0),
            bCanOpenToExterior = true
        )

        // Sheet Starboard : miroir de Port
        // Sheet Top : Normal (0,0,1)
        // Sheet Bottom : Normal (0,0,-1)
        // (4 sheets par compartiment)

    // ── Structural Sheets (cloisons internes) ──────────────────
    FOR EACH Bulkhead IN Solution.Bulkheads:
        AddSheet(
            Id = "Bulkhead_{ForeId}_{AftId}",
            ParentCompartment = Bulkhead.ForeCompartmentId,
            AdjacentCompartment = Bulkhead.AftCompartmentId,
            Origin = (Bulkhead.SpinePositionCm, 0, 0),
            Normal = (1, 0, 0),
            TangentX = (0, 1, 0),
            TangentY = (0, 0, 1),
            Size = (Bulkhead.RadiusCm * 2, Bulkhead.RadiusCm * 2),
            bCanOpenToExterior = false
        )

    // ── Doors ──────────────────────────────────────────────────
    FOR EACH Bulkhead IN Solution.Bulkheads:
        IF Bulkhead.PassageType != SealedBulkhead:
            FDoorDef Door
            Door.DoorId = "Bulkhead_{ForeId}_{AftId}"
            Door.BulkheadSheetId = "Bulkhead_{ForeId}_{AftId}"
            Door.PassageType = Bulkhead.PassageType
            Door.LocalTransform = Transform at (SpinePos, DoorOffset.X, DoorOffset.Y)
            Door.WidthCm = Bulkhead.DoorWidthCm
            Door.HeightCm = Bulkhead.DoorHeightCm
            Asset.Doors.Add(Door)

    // ── Station Slots ──────────────────────────────────────────
    FOR EACH Station IN Solution.Stations:
        FStationSlotDef Slot
        Slot.StationId = "{StationType}_{CompartmentId}"
        Slot.StationType = Station.StationType
        Slot.CompartmentId = Station.CompartmentId
        Slot.LocalTransform = Station.LocalTransform
        Asset.StationSlots.Add(Slot)

    // ── Metrics ────────────────────────────────────────────────
    Asset.Metrics = Solution.Metrics

    RETURN Asset
END FUNCTION
```

---

## 4. Geometry Builder

**Fichier : `SubmarineGeometryBuilder.h/.cpp`**

Prend une `FSubmarineLayoutSolution` et genere des `UProceduralMeshComponent` sections.

### 4.1 Interieur : extrusion de section

Pour chaque compartiment, generer un tube tronque (plancher plat) :

```
FUNCTION BuildCompartmentInterior(Placement) -> MeshData:

    R = Placement.EffectiveRadiusCm
    Length = Placement.SpineEndCm - Placement.SpineStartCm
    FloorY = Placement.FloorOffsetCm
    NumSegments = 24  // segments angulaires du cercle

    // Generer le profil de section (cercle tronque par le plancher)
    Profile = []
    FOR i = 0 TO NumSegments:
        Angle = (i / NumSegments) * 2 * PI
        Y = R * cos(Angle)
        Z = R * sin(Angle)

        // Tronquer au plancher
        IF Z < FloorY:
            Z = FloorY
            // Projeter Y sur le chord a cette hauteur
            // (on garde le Y du cercle, Z clamp au floor)

        Profile.Add(Y, Z)

    // Dedupliquer les points consecutifs identiques (zone du plancher)
    Profile = RemoveConsecutiveDuplicates(Profile)

    // Extruder le profil sur 2 sections (avant, arriere)
    Vertices = []
    Triangles = []

    X_Front = Placement.SpineStartCm
    X_Back = Placement.SpineEndCm

    FOR EACH Point IN Profile:
        Vertices.Add(X_Front, Point.Y, Point.Z)  // ring avant
        Vertices.Add(X_Back, Point.Y, Point.Z)    // ring arriere

    // Connecter les rings en quads (2 triangles par quad)
    FOR i = 0 TO Profile.Num() - 1:
        i0 = i * 2        // front current
        i1 = i * 2 + 1    // back current
        i2 = ((i+1) % Profile.Num()) * 2      // front next
        i3 = ((i+1) % Profile.Num()) * 2 + 1  // back next

        // Normals pointing INWARD (interior shell)
        Triangles.Add(i0, i2, i1)  // triangle 1
        Triangles.Add(i1, i2, i3)  // triangle 2

    // Plancher : 2 triangles couvrant le rectangle du sol
    // (genere separement pour avoir une normal (0,0,1) propre)

    // UVs : U = position le long du spine [0,1], V = position angulaire [0,1]

    RETURN MeshData(Vertices, Triangles, Normals, UVs)
END FUNCTION
```

### 4.2 Cloisons : disque avec decoupe

```
FUNCTION BuildBulkhead(Bulkhead) -> MeshData:

    R = Bulkhead.RadiusCm
    NumSegments = 24
    DoorW = Bulkhead.DoorWidthCm / 2.0
    DoorH = Bulkhead.DoorHeightCm
    DoorCenterY = Bulkhead.DoorOffsetCm.X
    DoorBottomZ = Bulkhead.DoorOffsetCm.Y - DoorH / 2.0
    DoorTopZ = Bulkhead.DoorOffsetCm.Y + DoorH / 2.0

    // Generer un disque triangule avec un trou rectangulaire
    // Approche : generer le contour exterieur (cercle) et le contour interieur
    // (rectangle de la porte), puis trianguler l'anneau entre les deux.

    // Contour exterieur : cercle
    OuterRing = []
    FOR i = 0 TO NumSegments:
        Angle = (i / NumSegments) * 2 * PI
        OuterRing.Add(R * cos(Angle), R * sin(Angle))

    // Contour interieur : rectangle de la porte (4 coins)
    // Seulement si PassageType != SealedBulkhead
    IF Bulkhead.PassageType != SealedBulkhead:
        InnerRing = [
            (DoorCenterY - DoorW, DoorBottomZ),
            (DoorCenterY + DoorW, DoorBottomZ),
            (DoorCenterY + DoorW, DoorTopZ),
            (DoorCenterY - DoorW, DoorTopZ)
        ]
        Triangulate anneau OuterRing -> InnerRing (ear clipping ou fan)
    ELSE:
        Triangulate disque plein OuterRing (fan depuis le centre)

    // Position : X = Bulkhead.SpinePositionCm
    // Normal : (1, 0, 0) ou (-1, 0, 0) selon le cote

    RETURN MeshData
END FUNCTION
```

### 4.3 Coque exterieure : sweep d'enveloppe

```
FUNCTION BuildExteriorHull(Envelope, Solution) -> MeshData:

    NumSpineSamples = 32  // echantillons le long du spine
    NumRadialSegments = 24

    FOR s = 0 TO NumSpineSamples:
        T = s / NumSpineSamples
        X = T * Envelope.SpineLengthCm
        R = Envelope.EvaluateRadius(T)

        FOR r = 0 TO NumRadialSegments:
            Angle = (r / NumRadialSegments) * 2 * PI
            Y = R * cos(Angle)
            Z = R * sin(Angle)
            Vertices.Add(X, Y, Z)
            // Normal = (Y/R, Z/R) normalized, pointing outward

    // Connecter en quads comme pour l'interieur
    // UVs : U = T (spine), V = angle/2PI

    RETURN MeshData
END FUNCTION
```

### 4.4 Collision

```
FUNCTION BuildCollision(Solution) -> CollisionData:

    // Interior walkable : simplified version of interior geometry
    // - Floor planes per compartment
    // - Wall simplified to box colliders
    // - Collision profile : SubInteriorWalkable

    // Hull exterior : convex decomposition of exterior mesh
    // - Collision profile : SubmarineHull

    // Door frames : box colliders at bulkhead positions
    // - Opening = no collider where the door is
END FUNCTION
```

---

## 5. Taches d'implementation

### Phase 0 — Fondation donnees

#### Tache 0.1 : SubCompilerTypes.h

**Input** : Section 1 de cette spec
**Output** : `Source/Sub3D/SubCompiler/SubCompilerTypes.h`
**Contenu** : Tous les enums et structs de la section 1.1 a 1.3

**Invariants machine** :
- Compile sans erreur ni warning
- Chaque struct est USTRUCT(BlueprintType) et a GENERATED_BODY()
- Chaque enum est UENUM(BlueprintType)
- Les meta ClampMin sont presentes sur les champs dimensionnels

**Validation editeur** :
- Les noms des champs sont lisibles dans un detail panel UE5

**Non-objectifs** :
- Pas de logique, pas de fonctions membres (sauf constructeurs par defaut)
- Pas de replication
- Pas de serialisation custom

---

#### Tache 0.2 : USubmarineEnvelopeDef

**Input** : Section 1.4
**Output** : `Source/Sub3D/SubCompiler/SubmarineEnvelopeDef.h` et `.cpp`

**Invariants machine** :
- Compile sans erreur
- `EvaluateRadius(0.5f)` sur un asset vide retourne `DefaultRadiusCm`
- `EvaluateRadius(T)` pour T dans [0,1] retourne toujours > 0
- Le DataAsset est creable via clic-droit dans le Content Browser (necessaire : factory ou `BlueprintType`)

**Validation editeur** :
- Creer un DA dans le Content Browser, ouvrir, verifier que les champs apparaissent
- Editer la courbe `RadiusProfile`, verifier que `EvaluateRadius` retourne les bonnes valeurs

**Non-objectifs** :
- Pas de preview visuelle de l'enveloppe
- Pas de validation des valeurs (juste ClampMin dans les meta)

---

#### Tache 0.3 : USubmarineFunctionalGraph

**Input** : Section 1.4
**Output** : `Source/Sub3D/SubCompiler/SubmarineFunctionalGraph.h` et `.cpp`

**Invariants machine** :
- Compile sans erreur
- `ValidateGraph()` sur un graphe vide retourne Error "Minimum 2 compartiments"
- `ValidateGraph()` sur un graphe valide (MVP 4 compartiments) retourne aucune Error
- `ValidateGraph()` detecte les IDs dupliques
- `ValidateGraph()` detecte les compartiments isoles (BFS)

**Validation editeur** :
- Creer un DA, ajouter 4 FCompartmentNode et 3 FPassageEdge, verifier que les tableaux sont editables

**Non-objectifs** :
- Pas d'UI custom pour le graphe
- Pas de visualisation des connexions

---

#### Tache 0.4 : Extension USubmarineLayoutAsset

**Input** : Section 1.5
**Output** : Modification de `Source/Sub3D/Submarine/SubmarineLayoutAsset.h`

**Invariants machine** :
- Compile sans erreur
- Les nouveaux champs (Doors, StationSlots, Metrics) sont accessibles
- `SubHullComponent::InitializeFromLayout()` continue de fonctionner sans regression
  (les champs existants Compartments et StructuralSheets sont inchanges)

**Validation editeur** :
- Ouvrir un LayoutAsset existant — pas de crash, les anciens champs sont la

**Non-objectifs** :
- Pas de remplissage automatique des nouveaux champs
- Pas de migration des assets existants

---

#### Tache 0.5 : Graphe MVP hardcode

**Input** : Section 6 du document d'architecture (MVP 4 compartiments)
**Output** : Un `USubmarineEnvelopeDef` et un `USubmarineFunctionalGraph` crees comme assets par defaut dans le Content Browser ou hardcodes dans une fonction de test

**Invariants machine** :
- Le graphe MVP passe `ValidateGraph()` sans erreurs
- 4 compartiments, 3 passages, priorities 0-3
- Enveloppe : 1520cm, rayon ~180cm

**Validation editeur** :
- Les DA sont visibles et editables dans le Content Browser

**Non-objectifs** :
- Pas de systeme de templates
- Pas d'UI de selection

---

### Phase 1 — Solver

#### Tache 1.1 : USubmarineLayoutSolver

**Input** : Section 2 de cette spec (pseudo-code complet)
**Output** : `Source/Sub3D/SubCompiler/SubmarineLayoutSolver.h` et `.cpp`

**Invariants machine** :
- `Solve()` avec le graphe MVP produit 4 FCompartmentPlacement
- Placements ordonnes par SpineStartCm croissant
- Aucun chevauchement : Placements[i].SpineEndCm <= Placements[i+1].SpineStartCm
- Somme des longueurs <= Envelope.SpineLengthCm
- 3 FBulkheadPlacement generes aux frontieres
- Chaque BulkheadPlacement a DoorWidthCm >= 80
- Stations placees dans les compartiments correspondants
- `HasErrors()` retourne false sur le graphe MVP
- `Solve()` avec un graphe invalide (IDs dupliques) retourne HasErrors() == true
- `Solve()` avec une enveloppe trop petite retourne HasErrors() == true
- FloorWidthCm > 0 pour chaque compartiment
- ClearanceHeightCm >= MinHeightCm pour chaque compartiment

**Validation editeur** :
- Log les placements dans la console UE5 pour verification visuelle des valeurs

**Non-objectifs** :
- Pas d'optimisation (solver lineaire, pas de backtracking)
- Pas de placement interactif
- Pas de contraintes avancees (centre de masse, stabilite)

---

#### Tache 1.2 : USubmarineBuildCompiler

**Input** : Section 3 de cette spec
**Output** : `Source/Sub3D/SubCompiler/SubmarineBuildCompiler.h` et `.cpp`

**Invariants machine** :
- Produit un `USubmarineLayoutAsset` a partir d'une `FSubmarineLayoutSolution`
- L'asset produit a Compartments.Num() == Solution.Compartments.Num()
- Le compilateur applique explicitement la politique de couverture MVP :
  `Port`, `Starboard`, `Top`, `Bottom` par compartiment, plus 1 sheet de cloison
  par frontiere de compartiment
- Chaque sheet exterieure a bCanOpenToExterior == true
- Chaque sheet cloison a AdjacentCompartmentId != NAME_None
- Chaque `FDoorDef` watertight respecte `DoorId == BulkheadSheetId`
- `SubHullComponent::InitializeFromLayout(CompiledAsset)` s'execute sans crash
- Apres initialisation, SubHullComponent a le bon nombre de CompartmentStates

**Validation editeur** :
- Inspecter l'asset compile dans le detail panel

**Non-objectifs** :
- Pas de serialisation sur disque (l'asset est en memoire uniquement pour Proto04A)
- Pas de metriques avancees

---

### Phase 2 — Geometrie procedurale

#### Tache 2.1 : USubmarineGeometryBuilder — Interieur

**Input** : Section 4.1
**Output** : `Source/Sub3D/SubCompiler/SubmarineGeometryBuilder.h` et `.cpp` (partie interieur)

**Invariants machine** :
- Genere un `UProceduralMeshComponent` par compartiment
- Chaque mesh a Vertices.Num() > 0 et Triangles.Num() > 0
- Aucun triangle degenere (aire > 0)
- Normals pointent vers l'interieur (dot product avec direction vers centre < 0)
- Le plancher est horizontal (tous les vertices du floor ont le meme Z)

**Validation editeur** :
- Placer l'acteur dans le level, verifier visuellement que les tubes sont la
- Entrer en PIE, marcher a l'interieur, verifier la hauteur sous plafond

**Non-objectifs** :
- Pas d'UVs texturees (graybox material suffit)
- Pas de smoothing groups
- Pas de LODs

---

#### Tache 2.2 : USubmarineGeometryBuilder — Cloisons

**Input** : Section 4.2
**Output** : Extension de `SubmarineGeometryBuilder.cpp`

**Invariants machine** :
- Genere un ProceduralMeshSection par cloison
- La decoupe porte est presente (pas de triangles dans la zone porte)
- Les dimensions du trou correspondent a DoorWidthCm et DoorHeightCm

**Validation editeur** :
- Verifier visuellement que le trou est au bon endroit et a la bonne taille
- Passer a travers la porte en PIE

**Non-objectifs** :
- Pas de porte physique (juste le trou)
- Pas de frame/cadre de porte

---

#### Tache 2.3 : USubmarineGeometryBuilder — Coque exterieure

**Input** : Section 4.3
**Output** : Extension de `SubmarineGeometryBuilder.cpp`

**Invariants machine** :
- Mesh ferme (watertight) — chaque edge partage exactement 2 triangles
- Normals pointent vers l'exterieur
- Le mesh suit le RadiusProfile de l'enveloppe

**Validation editeur** :
- Verifier la silhouette exterieure en vue orbitale
- Verifier qu'il n'y a pas de trous visuels

**Non-objectifs** :
- Pas de details (kiosque, helice, gouvernail)
- Pas de materiaux

---

#### Tache 2.4 : Collision

**Input** : Section 4.4
**Output** : Extension de `SubmarineGeometryBuilder.cpp`

**Invariants machine** :
- Les colliders de plancher sont generes pour chaque compartiment
- Les colliders des murs et cloisons sont generes
- L'ouverture de porte reste libre de collision quand la cloison n'est pas scellee
- Le profil de collision interieur est `SubInteriorWalkable`
- Le profil de collision exterieur est `SubmarineHull`

**Validation editeur** :
- PIE : marcher du bow au stern a travers les 3 portes
- PIE : verifier que les murs bloquent

**Non-objectifs** :
- Pas de collision precise (box approximation acceptable)
- Pas de portes ouvrables/fermables

---

### Phase 3 — Integration runtime

#### Tache 3.1 : Acteur compilateur

**Input** : Toutes les taches precedentes
**Output** : `Source/Sub3D/SubCompiler/SubmarineCompilerActor.h/.cpp`
Un acteur placable dans un level qui :
1. Reference un `USubmarineEnvelopeDef` et un `USubmarineFunctionalGraph`
2. Au BeginPlay ou via un bouton dans le detail panel : Solve → Compile → BuildGeometry
3. Spawn un `ASubmarineBase` configure avec le BuildRecipe compile
4. Le joueur peut entrer dans le sous-marin en PIE

**Invariants machine** :
- L'acteur compile sans erreur
- Le solveur, le build compiler et le geometry builder sont appeles dans l'ordre
- SubHullComponent est initialise (CompartmentStates, SheetStates non-vides)
- `ApplyHullImpact` sur une position de coque cree au moins une breach cluster
- `AdvanceFlooding` augmente l'eau dans le compartiment touche

**Validation editeur** :
- PIE : tester le cycle complet (compiler → entrer → marcher → endommager → inonder)

**Non-objectifs** :
- Pas de recompilation a chaud (il faut relancer PIE)
- Pas d'UI joueur
- Pas de spawn de stations (instanciation manuelle ou Phase 3.2)

---

#### Tache 3.2 : Instanciation des stations

**Input** : StationSlots du BuildRecipe
**Output** : Extension de `SubmarineCompilerActor` ou de `ASubmarineBase`

**Invariants machine** :
- Chaque StationSlot produit un acteur station du bon type
- Les stations sont positionnees aux LocalTransform specifies
- Les acteurs stations sont attaches au sous-marin compile
- Le StationManager les detecte via `DiscoverAttachedStations()`

**Validation editeur** :
- PIE : interagir avec chaque station
- Verifier que les positions sont coherentes (pas dans un mur, pas hors du compartiment)

**Non-objectifs** :
- Pas de meshes de stations (placeholder cube acceptable)
- Pas de fonctionnalite complete des stations (juste occupation)

---

## 6. Structure de fichiers cible

```
Source/Sub3D/SubCompiler/
    SubCompilerTypes.h              ← Tache 0.1
    SubmarineEnvelopeDef.h/.cpp     ← Tache 0.2
    SubmarineFunctionalGraph.h/.cpp ← Tache 0.3
    SubmarineLayoutSolver.h/.cpp    ← Tache 1.1
    SubmarineBuildCompiler.h/.cpp   ← Tache 1.2
    SubmarineGeometryBuilder.h/.cpp ← Taches 2.1-2.4
    SubmarineCompilerActor.h/.cpp   ← Tache 3.1
```

Module : `Sub3D` (pas de nouveau module, memes dependances).
Dependance requise deja presente : `ProceduralMeshComponent`.

---

## 7. Donnees de test MVP

Enveloppe :
```
SpineLengthCm = 1520.0
RadiusProfile = courbe plate a 180.0 (ou DefaultRadiusCm = 180.0)
MaxCompartments = 6
```

Graphe :
```
Compartments:
  [0] Id="Ballast_Fwd",   Type=Ballast,  MinLen=250, Priority=0, Systems=[Ballast]
  [1] Id="Helm",          Type=Helm,     MinLen=280, Priority=1, Systems=[Helm]
  [2] Id="Engine",        Type=Engine,   MinLen=320, Priority=2, Systems=[Engine, Pump]
  [3] Id="Airlock_Aft",   Type=Airlock,  MinLen=200, Priority=3, Systems=[Turret]

Passages:
  [0] From="Ballast_Fwd", To="Helm",        Type=WatertightDoor, Width=90, Height=180
  [1] From="Helm",        To="Engine",       Type=WatertightDoor, Width=90, Height=180
  [2] From="Engine",      To="Airlock_Aft",  Type=WatertightDoor, Width=90, Height=180
```

Resultat attendu du solver :
```
Compartments (approximatif, le slack est distribue) :
  Ballast_Fwd : [0, ~362]       length ~362
  Helm        : [~362, ~703]    length ~341
  Engine      : [~703, ~1093]   length ~390
  Airlock_Aft : [~1093, ~1520]  length ~427 (herite du slack restant potentiel)

  Note: la distribution exacte depend du ratio MinLength/TotalMinLength.
  TotalMinLength = 250+280+320+200 = 1050. Slack = 1520-1050 = 470.
  Bonus Ballast = 470 * 250/1050 = 111.9 → Length = 361.9
  Bonus Helm    = 470 * 280/1050 = 125.3 → Length = 405.3
  Bonus Engine  = 470 * 320/1050 = 143.2 → Length = 463.2
  Bonus Airlock = 470 * 200/1050 = 89.5  → Length = 289.5
  Total = 361.9 + 405.3 + 463.2 + 289.5 = 1519.9 ≈ 1520 ✓

Bulkheads : 3, aux positions ~362, ~767, ~1231
Stations : 5 (Ballast, Helm, Engine, Pump, Turret)
```

---

## 8. Critere de validation Proto04A

Proto04A est **termine** quand :

1. Un `USubmarineEnvelopeDef` et un `USubmarineFunctionalGraph` sont editables dans le Content Browser
2. Le solver produit un layout valide a partir du graphe MVP
3. Le build compiler produit un `USubmarineLayoutAsset` consommable par SubHullComponent
4. La geometrie procedurale est visible en PIE (interieur + cloisons + coque)
5. Le crew peut marcher du bow au stern a travers les 3 portes
6. Un impact sur la coque cree une breche visible en debug draw
7. L'eau monte dans le compartiment touche
8. Les stations sont presentes et occupables

**Pas requis** : beau, polish, joueur-facing, sauvegardable, performant.
