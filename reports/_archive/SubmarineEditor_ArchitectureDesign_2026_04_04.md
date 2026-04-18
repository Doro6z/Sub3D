# Sub3D — Architecture & Design du Submarine Hull Editor v2

**Version :** 2026-04-04
**Statut :** Document de référence — pré-implémentation Phase 2

---

## 1. Principe fondateur

> Chaque niveau de modification est un **contrat de mutation** avec son propre type de données, son propre éditeur, et sa propre durée de vie.
> La géométrie de coque est un fait établi. Tout ce qui vient après l'annote — il ne la reécrit pas.

Le système doit couvrir deux familles de sous-marins sans distinction de traitement :
- **Réalistes** : Suffren/Barracuda, A-26 Blekinge, Type 212, Ohio, Type VII — proportions et anatomie historiquement plausibles
- **Sci-fi / abstraits** : formes futuristes, appendices non-conventionnels, sections asymétriques, coques composites — la personnalisation ne doit pas être contrainte par la physique réelle

La seule règle commune : **les rings définissent la forme, pas l'inverse.**

---

## 2. Les quatre niveaux de modification

### Niveau A — Quasi figé (Dev Editor / Major Refit)
Géométrie de pression hull, rings, frame-rings, forme globale, appendices structurels lourds.
Modifier le Niveau A invalide B et C.

### Niveau B — Chantier naval
Structural Bays, Deck Levels, Floor Regions, Openings, Connectors verticaux.
Modifier B invalide C seulement.

### Niveau C — Large
Pressure Bulkheads, Internal Walls, Doors/Hatches, Equipment Modules, Room Tags.
Pas de rebake géométrique nécessaire.

### Niveau D — Runtime state only (no asset)
États ouverts/fermés, dégâts, breach, flood, mission states.
Struct serialisable en save game, jamais persistée en asset.

---

## 3. AuthoringAsset — structure avec marquage A/B/C

Un seul `USub3DSubmarineAuthoringAsset`, catégories UPROPERTY comme séparateurs logiques.
Bake paths distincts, flags de validation séparés.

```cpp
UCLASS(BlueprintType)
class SUB3DBAKE_API USub3DSubmarineAuthoringAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════
    // NIVEAU A — Hull Geometry (Dev Only)
    // Modifier invalide B et C. Verrouillé par bHullGeometryConfirmed.
    // ═══════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    FSubmarineHullDef Hull;

    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    TArray<FControlRingDef> ControlRings;

    UPROPERTY(EditAnywhere, Category="[A] Pressure Hull")
    TArray<FFrameRingDef> FrameRings;

    UPROPERTY(EditAnywhere, Category="[A] Sail & Kiosk")
    FSailDef Sail;

    UPROPERTY(EditAnywhere, Category="[A] Bow Section")
    FBowSectionDef BowSection;

    UPROPERTY(EditAnywhere, Category="[A] Stern Section")
    FSternSectionDef SternSection;

    UPROPERTY(EditAnywhere, Category="[A] Outer Casing")
    FOuterCasingDef OuterCasing;

    // Lock flags
    UPROPERTY(VisibleAnywhere, Category="[A] Lock State")
    bool bHullGeometryConfirmed = false;

    UPROPERTY(VisibleAnywhere, Category="[A] Lock State")
    FGuid HullGeometryHash; // invalide B/C si changé

    // ═══════════════════════════════════════════════════════════════
    // NIVEAU B — Structural Layout (Shipyard / Dev)
    // Modifier invalide C. Nécessite bHullGeometryConfirmed.
    // ═══════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, Category="[B] Structural Layout",
        meta=(EditCondition="bHullGeometryConfirmed"))
    TArray<FStructuralBayDef> StructuralBays;

    UPROPERTY(EditAnywhere, Category="[B] Structural Layout",
        meta=(EditCondition="bHullGeometryConfirmed"))
    TArray<FDeckLevelDef> DeckLevels;

    UPROPERTY(EditAnywhere, Category="[B] Structural Layout",
        meta=(EditCondition="bHullGeometryConfirmed"))
    TArray<FFloorRegionDef> FloorRegions;

    UPROPERTY(EditAnywhere, Category="[B] Openings & Connectors",
        meta=(EditCondition="bHullGeometryConfirmed"))
    TArray<FOpeningDef> Openings;

    UPROPERTY(EditAnywhere, Category="[B] Openings & Connectors",
        meta=(EditCondition="bHullGeometryConfirmed"))
    TArray<FConnectorDef> Connectors;

    UPROPERTY(VisibleAnywhere, Category="[B] Lock State")
    bool bLayoutConfirmed = false;

    // ═══════════════════════════════════════════════════════════════
    // NIVEAU C — Interior Config (Large access)
    // Pas de rebake de A ou B nécessaire.
    // ═══════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, Category="[C] Partitions")
    TArray<FPressureBulkheadDef> PressureBulkheads;

    UPROPERTY(EditAnywhere, Category="[C] Partitions")
    TArray<FInternalWallDef> InternalWalls;

    UPROPERTY(EditAnywhere, Category="[C] Closures")
    TArray<FClosureDef> Closures;

    UPROPERTY(EditAnywhere, Category="[C] Closures")
    TArray<FConnectorDef> VerticalConnectors;
};
```

**Bake cascade :**
```cpp
// Appels distincts — chacun ne lit que sa couche + les couches compilées au-dessus
BakeHullGeometry(Asset)   →  UCompiledHullGeometry       // lent, rare
BakeLayout(Asset)         →  UCompiledSubmarineLayout     // moyen, chantier
BakeConfig(Asset)         →  FSubmarineConfigSnapshot     // rapide, pas de mesh
// Niveau D = pas de bake, pure runtime struct
```

---

## 4. Paramétrage complet de l'enveloppe

### 4.1 Control Rings — la base, variable en taille

Les rings ne sont pas tous identiques. Chaque ring a ses propres dimensions indépendantes.
C'est ce qui permet les formes sci-fi : un ring très aplati au milieu, un ring circulaire en poupe, un ring en superellipse à l'avant.

```cpp
USTRUCT(BlueprintType)
struct FControlRingDef
{
    // Identité
    FName ControlRingId = NAME_None;
    float PositionX = 0.0f;             // position sur la spine en cm

    // Dimensions — indépendantes par ring
    float HalfWidthCm = 180.0f;         // demi-largeur (axe Y)
    float HalfHeightCm = 180.0f;        // demi-hauteur (axe Z)
    // Ratio HW/HH = 1.0 → cercle, <1.0 → ellipse aplatie, >1.0 → ellipse large
    // Sci-fi : peut avoir HW=400, HH=80 → forme lenticulaire

    // Profil de section — indépendant par ring
    ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;
    // Circle, Ellipse, Superellipse
    float SectionRoundness = 0.5f;       // 0.0=losange, 0.5=ellipse, 1.0=rect arrondi

    // Épaisseur de coque — peut varier par ring (zones renforcées)
    float WallThicknessCm = 12.0f;

    // Catégorie de visualisation dans l'éditeur
    ERingCategory Category = ERingCategory::Hull;
    // Hull, Kiosk, SternFin, BowAppendage, CustomSci-fi
    // → détermine la couleur dans le viewport
};
```

**Rendu des ring handles dans le viewport :**

| Taille ring | Rendu handle | Couleur |
|-------------|-------------|---------|
| HW = HH (cercle) | Torus parfait | 🟢 Vert |
| HW ≠ HH (ellipse) | Torus aplati selon le ratio | 🟢 Vert |
| Superellipse | Approximation polygonale | 🟢 Vert |
| Sélectionné | Même + surbrillance | 🟡 Jaune |
| Frame ring | Disque fin, semi-transparent | 🔵 Bleu |
| Kiosk ring | Torus petit, positionné en Z+ | 🟣 Violet |
| Erreur | Outline clignotant | 🔴 Rouge |

La **taille visuelle du handle** correspond exactement aux dimensions du ring — `HalfWidthCm × HalfHeightCm`. Dragging le bord du handle change le rayon. Dragging le centre change PositionX.

---

### 4.2 Sail / Kiosk

```cpp
USTRUCT(BlueprintType)
struct FSailDef
{
    bool bEnabled = false;

    // Position sur la hull
    float SpineAlpha = 0.38f;            // 0.30-0.50 typique, libre pour sci-fi
    float LateralOffsetCm = 0.0f;        // 0 = centré, non-zéro = asymétrique

    // Dimensions verticales
    float HeightCm = 450.0f;             // hauteur totale

    // Emprise au pied (sur la coque)
    float BaseForeLengthCm = 250.0f;     // longueur côté proue au pied
    float BaseAftLengthCm = 180.0f;      // longueur côté poupe au pied
    float BaseWidthCm = 80.0f;           // largeur au pied

    // Emprise au sommet (effilement)
    float TopForeLengthCm = 120.0f;      // longueur côté proue au sommet
    float TopAftLengthCm = 80.0f;        // longueur côté poupe au sommet
    float TopWidthCm = 60.0f;            // largeur au sommet

    // Forme
    float LeadingEdgeSweepDeg = 30.0f;   // angle d'attaque proue du kiosk
    float TrailingEdgeSweepDeg = 15.0f;  // angle d'attaque poupe du kiosk

    ESailShape Shape = ESailShape::Faired;
    // Faired   → profil hydrodynamique (Suffren, Virginia)
    // Cylindrical → tube vertical simple (SSK anciens, sci-fi industriel)
    // Teardrop → profilé larme d'eau (plus silencieux, sci-fi)
    // Custom   → défini par TArray<FSailRingDef> (petits rings = liberté totale)

    // CUSTOM : rings de kiosk (actif si Shape == Custom)
    TArray<FControlRingDef> KioskRings;  // même type que hull rings, mais scope kiosk

    // Raccordement avec la coque ← souvent oublié
    float HullBlendLengthCm = 60.0f;     // longueur de la zone de lissage
    // Sans ça : arête vive entre le kiosk et la coque = aspect plastique

    // Plans de plongée sur le kiosk (style américain)
    bool bHasFairwaterPlanes = false;
    float FairwaterPlaneSpanCm = 200.0f;
    float FairwaterPlaneChordCm = 60.0f;
    float FairwaterPlaneZOffsetCm = 120.0f;  // hauteur depuis base kiosk

    // Mâts (positions relatives, pas de géométrie générée — juste metadata)
    bool bHasPeriscope = true;
    bool bHasSnorkel = false;             // AIP/diesel seulement
    bool bHasESMMast = true;
    bool bHasCommunicationsMast = true;
    int32 CustomMastCount = 0;            // mâts supplémentaires (sci-fi)
};
```

---

### 4.3 Bow Section

```cpp
USTRUCT(BlueprintType)
struct FBowSectionDef
{
    // Dôme sonar
    ESonarDomeType SonarDome = ESonarDomeType::Spherical;
    // Spherical    → BQQ-5 / Rubis — sphère classique
    // Conformal    → Virginia class — suit la coque
    // ChinMounted  → LA class — bulbe sous l'étrave
    // None         → pas de sonar en proue (sci-fi, mini-sub)
    // Custom       → forme définie par rings supplémentaires

    float SonarDomeLengthCm = 120.0f;   // extension en proue vs profil hull
    float SonarDomeRadiusCm = 0.0f;     // 0 = suit la coque au point de raccord

    // Plans de plongée d'étrave
    EBowPlaneLocation BowPlanesLocation = EBowPlaneLocation::None;
    // None         → aucun plan d'étrave (Virginia, beaucoup de modernes)
    // HullMounted  → sur la coque (style européen — A-26, Type 212, Rubis)
    // SailMounted  → sur le kiosk (style américain — défini dans FSailDef)

    float BowPlanePositionAlpha = 0.18f; // position sur la spine
    float BowPlaneSpanCm = 220.0f;
    float BowPlaneChordCm = 55.0f;
    bool bBowPlanesRetractable = true;   // impacte la silhouette en surface
    // Retractable = flush avec la coque en immersion → profil net
    // Fixed = toujours sortis → plus simple, plus rustique / sci-fi industriel

    // Tubes lance-torpilles (définissent l'ouverture dans la coque)
    int32 TorpedoTubeCount = 4;          // 0, 4, 6, 8, ou nombre custom
    float TorpedoTubeDiameterCm = 53.3f; // 533mm standard, 650mm lourd, custom
    float TorpedoTubePositionAlpha = 0.08f;

    ETorpedoTubeArrangement TubeArrangement = ETorpedoTubeArrangement::Parallel;
    // Parallel → tubes côte à côte (Virginia, Suffren)
    // Angled   → tubes inclinés ~10° vers l'extérieur (WWII, certains SSK)
    // Fan      → disposition en éventail (certains anciens)
    // VLS      → tubes verticaux dans la section centrale (SSGN, custom)
    // None     → pas de tubes (certains sci-fi, SDV, recherche)

    // Panneaux externes de la proue (metadata pour le matériau)
    bool bHasFlankArrays = false;        // tableaux sonar latéraux
};
```

---

### 4.4 Stern Section ← la plus oubliée

```cpp
USTRUCT(BlueprintType)
struct FSternSectionDef
{
    // Surfaces de contrôle
    EControlSurfaceArrangement Arrangement = EControlSurfaceArrangement::Cruciform;
    // Cruciform (+) → standard NATO, simple à modéliser
    // XForm    (×)  → meilleure manœuvrabilité, Astute/Barracuda
    // YTail         → 3 gouvernes à 120° (rare, certains AUV)
    // TwinRudder    → deux gouvernes latérales (anciens designs)
    // None          → sans gouvernes (certains sci-fi, propulsion vectorielle)

    float RudderSpanCm = 180.0f;
    float RudderChordCm = 70.0f;
    float SternPlaneSpanCm = 160.0f;
    float SternPlaneChordCm = 65.0f;

    // Propulseur ← impact fort sur la silhouette en poupe
    EPropulsorType PropulsorType = EPropulsorType::SingleScrew;
    // SingleScrew   → hélice unique (la plupart des SSN/SSK)
    // PumpJet       → turbine carénée (Astute, Suffren, Virginia)
    //                  → silhouette bouée en poupe, beaucoup plus silencieux
    // TwinScrew     → deux hélices (WWII, certains anciens designs)
    // AUVPod        → nacelle externe (AUV/SDV, stations scientifiques sci-fi)
    // VectorThrust  → pas d'appendice visible (propulsion vectorielle sci-fi)
    // None          → aucune propulsion visible

    float PropellerDiameterCm = 180.0f;
    float PropellerHubDiameterCm = 30.0f;
    float ShaftLengthCm = 40.0f;          // extension de l'arbre hors coque
    float ShaftAngleDeg = 0.0f;           // inclinaison (positif = vers le bas)

    // Fairing du propulseur ← SOUVENT OUBLIÉ
    // C'est le cône qui entoure l'arbre entre la coque et l'hélice
    bool bHasSternFairing = true;
    float SternFairingLengthCm = 80.0f;   // longueur du cône
    // Sans ça : l'arbre sort brutalement de la coque — aspect non-fini

    // Réseau traîné (towed array) ← PRESQUE TOUJOURS OUBLIÉ
    // Présent sur quasi tous les sous-marins modernes
    // Petite ailette/bosse latérale pour guider le câble du sonar traîné
    bool bHasTowedArrayFairing = true;
    float TowedArrayFairingAlpha = 0.85f; // position sur la spine (proche de la poupe)
    ETowedArraySide TowedArraySide = ETowedArraySide::Starboard;
    float TowedArrayFairingHeightCm = 15.0f;
    float TowedArrayFairingLengthCm = 60.0f;

    // Options sci-fi / custom
    int32 ExtraAftAppendageCount = 0;     // appendices supplémentaires custom
};
```

---

### 4.5 Outer Casing ← le grand oublié

La différence entre une **coque simple** et une **coque double** :
- Coque simple : pression hull = enveloppe hydrodynamique (SSK modernes, sci-fi épuré)
- Coque double partielle : caisson hydrodynamique aux extrémités (OTAN typique)
- Coque double complète : caisson complet autour de la pression hull (designs russes, certains sci-fi)

```cpp
USTRUCT(BlueprintType)
struct FOuterCasingDef
{
    bool bEnabled = false;

    EOuterCasingType CasingType = EOuterCasingType::Partial;
    // Full    → caisson complet (Ohio, Oscar II, designs russes)
    // Partial → caisson aux extrémités seulement (Suffren, Virginia)
    // None    → pression hull = forme finale (A-26, Type 212, sci-fi épuré)

    // Zones couvertes (actif si Partial ou Full)
    float BowCasingEndAlpha = 0.15f;     // jusqu'où le caisson couvre l'étrave
    float SternCasingStartAlpha = 0.80f; // à partir d'où il reprend en poupe
    // Zone centrale = parallel midbody où casing = pression hull (ou absent)

    // Offset du caisson vs pressure hull
    float CasingOffsetCm = 25.0f;        // combien le caisson dépasse la pression hull
    // Crée l'espace pour les ballasts externes, câbles, tuyaux

    // Ballasts externes (dans l'espace casing)
    bool bHasExternalBallastTanks = false;
    // Impacte le gameplay (ballasts externes plus vulnérables aux dégâts)

    // Pont supérieur (deck casing)
    // La surface plate sur laquelle l'équipage marche en surface
    // ← OUBLIÉ SYSTÉMATIQUEMENT — définit la silhouette vue du dessus
    bool bHasDeckCasing = true;
    float DeckCasingWidthCm = 80.0f;     // largeur du plat-bord
    float DeckCasingStartAlpha = 0.20f;
    float DeckCasingEndAlpha = 0.85f;
    float DeckCasingHeightOffset = 2.0f; // légèrement au-dessus du hull radius en cm

    // Revêtement anéchoïque ← OUBLIÉ (visuel + gameplay furtivité)
    bool bHasAnechoicCoating = true;
    EAnechoicPattern CoatingPattern = EAnechoicPattern::Full;
    // Full      → couverture totale (Virginia, Astute)
    // Partial   → zones critiques seulement
    // PanelGrid → panneaux visibles avec joints (Suffren — très caractéristique)
    // Scaled    → petites écailles (certains designs russes)
    // None      → acier nu (WWII, certains sci-fi)
    // Custom    → pattern procédural (sci-fi)

    // Limber holes (trous de drainage dans le caisson externe)
    // Pas de géométrie générée mais important pour l'apparence
    bool bHasLimberHoles = true;
    ELimberHolePattern LimberHolePattern = ELimberHolePattern::Row;
    // Row → rangée de trous réguliers
    // Slot → fentes longitudinales
    // None → caisson étanche (rare, certain sci-fi pressurisé)
};
```

---

## 5. Architecture du preview interactif

### 5.1 Deux acteurs, deux rôles

```
USub3DSubmarineAuthoringAsset (asset, source de vérité)
        │
        ├─► ASubmarinePreviewActor (dans le niveau éditeur, temporaire)
        │         Ring handles interactifs + hot-path mesh preview
        │         Mis à jour à chaque modification de propriété
        │
        └─► ASubmarineRuntimeActor (après bake complet uniquement)
                  Données compilées, collision physique, gameplay
```

### 5.2 ASubmarinePreviewActor

```cpp
UCLASS(NotBlueprintable)
class ASubmarinePreviewActor : public AActor
{
    // Mesh de preview basse qualité (8-16 segments)
    UPROPERTY()
    UProceduralMeshComponent* PreviewHullMesh;     // wireframe ou solid translucide

    UPROPERTY()
    UProceduralMeshComponent* PreviewKioskMesh;    // si Sail activé

    // Ring handles — un composant par ring
    UPROPERTY()
    TArray<USubmarineRingHandleComponent*> RingHandles;

    // Dirty flag — true si la preview doit être régénérée
    bool bPreviewDirty = false;

    // Référence à l'asset édité
    TWeakObjectPtr<USub3DSubmarineAuthoringAsset> EditedAsset;

    void OnRingMoved(int32 RingIndex, float NewPositionX);
    void OnRingResized(int32 RingIndex, float NewHalfWidth, float NewHalfHeight);
    void RegeneratePreview();  // appelle FSubmarineHullPreviewService
};
```

### 5.3 USubmarineRingHandleComponent

```cpp
UCLASS()
class USubmarineRingHandleComponent : public UPrimitiveComponent
{
    // Données du ring représenté
    int32 RingIndex = INDEX_NONE;
    ERingCategory Category = ERingCategory::Hull;

    // Rendu : torus dont les dimensions = HalfWidth × HalfHeight du ring
    // → la taille visuelle du handle EST la taille réelle du ring
    float DisplayHalfWidth = 0.0f;    // mis à jour depuis FControlRingDef
    float DisplayHalfHeight = 0.0f;

    // Gizmo de translation (axe X seulement — PositionX)
    TSharedPtr<UCombinedTransformGizmo> TranslationGizmo;

    // Gizmo de scale (axes Y et Z — HalfWidth et HalfHeight)
    TSharedPtr<UCombinedTransformGizmo> ScaleGizmo;

    // Couleur selon catégorie et état
    FLinearColor GetDisplayColor() const;
    // Hull validé     → (0.3, 0.9, 0.3) vert
    // Sélectionné     → (1.0, 0.8, 0.0) jaune
    // Frame ring      → (0.2, 0.4, 0.9) bleu
    // Kiosk ring      → (0.6, 0.2, 0.9) violet
    // Erreur          → (0.9, 0.2, 0.2) rouge
    // Verrouillé [A]  → (0.4, 0.4, 0.4) gris
};
```

### 5.4 FSubmarineHullPreviewService

```cpp
namespace Sub3DPreview
{
struct FPreviewSettings
{
    int32 SplineRingCount = 8;       // rings interpolés (vs 48 en bake complet)
    int32 RadialSegments = 12;       // segments radiaux (vs 32 en bake)
    bool bGenerateNormals = false;   // pas nécessaire pour wireframe
    bool bIncludeInterior = false;   // uniquement extérieur en preview
    bool bIncludeCollision = false;
};

class FSubmarineHullPreviewService
{
public:
    // < 5ms target — appelé à chaque drag de ring handle
    static bool GeneratePreview(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        const FPreviewSettings& Settings,
        FCompiledMeshSection& OutPreviewMesh);
};
}
```

---

## 6. UI Layout complet

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Submarine Hull Authoring      [Load Preset]  [Preview Layout]  [Confirm Layout]│
├──────────────────┬──────────────────────────────────────┬───────────────────────┤
│  LEFT PANEL      │  VIEWPORT CENTER                     │  RIGHT PANEL          │
│                  │                                      │                       │
│ ▼ Presets        │  X Offset: [slider]                  │ ▼ Ring Settings       │
│                  │  ☑ Show Envelope  ☑ Control Rings   │  (ring sélectionné)   │
│   Default Sub    │                                      │                       │
│ ► Small Attack   │  ┌──────────────────────────────┐   │  ID     : Ring_Mid    │
│   Torpedo Sub    │  │   [3D VIEWPORT]              │   │  Pos X  : 3600 cm     │
│   Large Research │  │                              │   │  HalfW  : 200 cm  [↕] │
│   SSBN Ballistic │  │   🟢 Ring handles            │   │  HalfH  : 180 cm  [↕] │
│   WWII Type VII  │  │   Torus = taille réelle      │   │  Profile: Superellipse│
│   ── Sci-Fi ──   │  │   Hull wireframe smooth      │   │  Round  : 0.7         │
│   Leviathan      │  │   Côtes et annotations       │   │  Wall   : 12 cm       │
│   Manta Ray      │  │   500cm / 75m / labels       │   │  Cat    : Hull        │
│   Spine Class    │  │                              │   │  ──────────────────── │
│   + Custom       │  └──────────────────────────────┘   │                       │
│                  │  Front  Perspective  ◄──────► 75m   │ ▼ View Filters        │
│ ─────────────── │                                      │  ☑ Control Rings      │
│                  │                                      │  ☑ Frame Rings        │
│ ▼ Hull Shape     │  ▼ Validation Log                   │  ☑ Sail Envelope      │
│  [Curve editor   │    ✓ Ring sequence valid             │  ☑ Bow Section        │
│   radius/spine]  │    ✓ Sail position coherent         │  ☑ Stern Section      │
│                  │    ✗ Bow planes: span hors bornes   │  ☑ Outer Casing       │
│  Length 7200 cm  │    ✓ Stern surfaces coherent        │  ☐ Interior (wire)    │
│  MaxDiam  400 cm │    ✓ Wall thickness everywhere > 0  │                       │
│  Fineness   7.2  │                                      │ ▼ Bake Paths [A]      │
│  Profile  Myring │                                      │  Status: Unlocked     │
│                  │                                      │  [Bake Hull Geometry] │
│ ─────────────── │                                      │  [Bake Layout]        │
│                  │                                      │  [Bake Config]        │
│ ▼ Appendages     │                                      │                       │
│  + Sail / Kiosk  │                                      │ ▼ Level A Hash        │
│  + Bow Section   │                                      │  abc123... [⚠ dirty] │
│  + Stern Ctrl    │                                      │                       │
│  + Bow Planes    │                                      │                       │
│  + Outer Casing  │                                      │                       │
│                  │                                      │                       │
│ + Add Ring  [🗑] │                                      │                       │
│ ☑ Auto-Generate  │                                      │                       │
└──────────────────┴──────────────────────────────────────┴───────────────────────┘
```

---

## 7. Code couleur des ring handles dans le viewport

| État | Couleur | Description |
|------|---------|-------------|
| Hull validé | 🟢 Vert `(0.3, 0.9, 0.3)` | Ring de contrôle normal |
| Sélectionné | 🟡 Jaune `(1.0, 0.8, 0.0)` | Ring actif, gizmos visibles |
| Frame ring | 🔵 Bleu `(0.2, 0.4, 0.9)` | Ring structurel (bay boundary) |
| Kiosk ring | 🟣 Violet `(0.6, 0.2, 0.9)` | Ring de l'enveloppe secondaire |
| Sci-fi custom | ⚪ Blanc `(0.8, 0.8, 0.9)` | Ring de catégorie custom |
| Erreur | 🔴 Rouge `(0.9, 0.2, 0.2)` | Hors bornes ou conflit |
| Verrouillé [A] | ⬜ Gris `(0.4, 0.4, 0.4)` | Geometry confirmée, non-éditable |

**La taille visuelle du torus = les dimensions réelles du ring.**
Un ring HalfW=400, HalfH=80 s'affiche comme un torus très aplati. L'utilisateur voit directement la forme qui va être générée.

---

## 8. Presets couverts

| Preset | Profil | Fineness | Sail | BowPlanes | Propulseur | Casing | Notes |
|--------|--------|----------|------|-----------|-----------|--------|-------|
| Default Sub | Myring | 7.5 | Faired med | Hull ret. | Single screw | Partial | Générique NATO |
| Small Attack (SSK) | Series58 | 6.5 | Cylindrical | Sail-mount | Single screw | None | A-26/Type212 style |
| Torpedo Sub | Myring | 8.0 | Faired small | None | Single screw | None | Minimal, furtif |
| Large Research | Superellipse | 10 | Faired large | Hull fixed | Twin screw | Full | SSBN/SSGN style |
| WWII Type VII | Uniform | 8.5 | Cylindrical | None | Twin screw | None | Pont supérieur visible |
| — Sci-Fi — | | | | | | | |
| Leviathan | Custom | 5.0 | Custom tall | Multiple | VectorThrust | Full | Amygdale géante |
| Manta Ray | Superellipse N=6 | 4.0 | None | BowFin large | AUVPod×2 | None | Aplati, HH<<HW |
| Spine Class | Custom rings | 12.0 | Multiple | None | PumpJet | Scaled | Vertébré, segments |

Le système de rings libres permet **n'importe quelle forme** — un ring HalfW=800, HalfH=120 donne une section lenticulaire impossible sur un vrai sous-marin mais parfaitement valide pour du sci-fi.

---

## 9. Séquence d'implémentation recommandée

### Sprint 1 — ASubmarinePreviewActor + FSubmarineHullPreviewService
- Preview actor spawne dans le niveau
- Lit `Asset->ControlRings` (manuels ou générés par profil)
- Génère un mesh wireframe basse qualité (8 rings, 12 segments)
- Se met à jour sur `PostEditChangeProperty`
- **Objectif** : voir la forme changer en temps réel sans clic Bake

### Sprint 2 — Ring handles visuels
- Un `USubmarineRingHandleComponent` par ring
- Torus dont la taille = dimensions réelles du ring
- Drag sur axe X → `PositionX`
- Scale Y/Z → `HalfWidth / HalfHeight`
- **Objectif** : bouger un ring à la main et voir la coque se déformer

### Sprint 3 — Preset picker dans le toolkit
- Panel left avec liste de presets catégorisés
- Clic → `GenerateControlRingsFromProfile()` + refresh preview
- Editeur du profil : sliders `ParallelMidbodyFraction`, `NoseExponent`, etc.
- **Objectif** : sélectionner "Small Attack Sub" et voir la forme apparaître

### Sprint 4 — Appendices (Sail en premier)
- `FSailDef` éditable dans le panel left
- Preview du kiosk comme deuxième ProceduralMeshComponent sur le PreviewActor
- Si `Shape == Custom` : affiche des ring handles supplémentaires en violet
- **Objectif** : ajouter un kiosk et voir le résultat en preview

### Sprint 5 — Confirm Layout + phase transition
- Bouton "Confirm Layout" → `bHullGeometryConfirmed = true`, hash de la géométrie
- Rings verrouillés (affichés en gris, non-éditables)
- Transition vers l'interface Niveau B (Bays/Floors)
- **Objectif** : workflow complet Phase 1 → Phase 2

---

## 10. Ce qui reste intentionnellement hors scope pour l'instant

- Simulation hydrodynamique (drag, plongée) — les formes sont définies géométriquement, pas physiquement
- Mâts avec géométrie générée (metadata seulement pour l'instant)
- Multi-sous-marins dans le même asset
- Historique des modifications / undo étendu (UE5 transaction system gère ça)
- Export vers formats extérieurs (FBX, STEP)
