#pragma once

#include "CoreMinimal.h"
#include "Sub3DAppendageTypes.generated.h"

// ── Sail enums ───────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESub3DSailStyle : uint8
{
    TallNarrow      UMETA(DisplayName="Tall Narrow (SSN/SSBN)"),
    WideSlab        UMETA(DisplayName="Wide Slab (Soviet)"),
    FinnedAlpha     UMETA(DisplayName="Finned Alpha"),
    LowProfile      UMETA(DisplayName="Low Profile (AIP)"),
    Retracted       UMETA(DisplayName="Retracted hull-flush"),
    SciFiPod        UMETA(DisplayName="Sci-Fi Pod"),
    Custom          UMETA(DisplayName="Custom")
};

// ── Bow enums ────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESub3DSonarDomeType : uint8
{
    Hemisphere      UMETA(DisplayName="Hemisphere"),
    Conformal       UMETA(DisplayName="Conformal Teardrop"),
    Extended        UMETA(DisplayName="Extended Cylinder"),
    CylindricalFlat UMETA(DisplayName="Cylindrical Flat"),
    SciFiPlate      UMETA(DisplayName="Sci-Fi Forward Plate"),
    None            UMETA(DisplayName="None")
};

UENUM(BlueprintType)
enum class ESub3DBowPlaneConfig : uint8
{
    None            UMETA(DisplayName="None"),
    BowPlanes       UMETA(DisplayName="Bow Planes"),
    CanardFins      UMETA(DisplayName="Canard Fins (sci-fi)")
};

// ── Stern enums ──────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESub3DPropulsorType : uint8
{
    SevenBladedSkewback UMETA(DisplayName="7-Bladed Skewback"),
    PumpJet             UMETA(DisplayName="Pump Jet"),
    ContradRotating     UMETA(DisplayName="Contra-Rotating"),
    PoddedAzimuth       UMETA(DisplayName="Podded Azimuth"),
    MHD                 UMETA(DisplayName="MHD (magnetohydrodynamic)"),
    DualShaft           UMETA(DisplayName="Dual Shaft"),
    SciFiNacelle        UMETA(DisplayName="Sci-Fi Nacelle")
};

UENUM(BlueprintType)
enum class ESub3DControlSurfaceArrangement : uint8
{
    CrossPattern        UMETA(DisplayName="Cross (+) standard SSN"),
    XPattern            UMETA(DisplayName="X Pattern — Type 212/Collins"),
    YPattern            UMETA(DisplayName="Y Stern — ALFA class"),
    SingleRudder        UMETA(DisplayName="Single Rudder"),
    SciFiVectorNozzle   UMETA(DisplayName="Sci-Fi Vector Nozzle")
};

// ── Casing enums ─────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESub3DAnechoicCoating : uint8
{
    None            UMETA(DisplayName="None"),
    PartialStrakes  UMETA(DisplayName="Partial Strakes"),
    FullBody        UMETA(DisplayName="Full Body"),
    Conformal       UMETA(DisplayName="Conformal flush-mount"),
    SciFiActive     UMETA(DisplayName="Sci-Fi Active")
};

// ── FSailDef ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSailDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bEnabled"))
    float SpineAlpha = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(EditCondition="bEnabled"))
    float LateralOffsetCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(EditCondition="bEnabled"))
    ESub3DSailStyle Style = ESub3DSailStyle::TallNarrow;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float HeightCm = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float BaseChordCm = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="10.0", EditCondition="bEnabled"))
    float TopChordCm = 140.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="10.0", EditCondition="bEnabled"))
    float BeamCm = 85.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="0.0", ClampMax="45.0", EditCondition="bEnabled"))
    float LeadingEdgeSweepDeg = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="0.0", ClampMax="45.0", EditCondition="bEnabled"))
    float TrailingEdgeSweepDeg = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Fairwater Planes", meta=(EditCondition="bEnabled"))
    bool bHasFairwaterPlanes = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Fairwater Planes", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float FairwaterSpanCm = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Fairwater Planes", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float FairwaterChordCm = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Fairwater Planes", meta=(ClampMin="-30.0", ClampMax="30.0", EditCondition="bEnabled"))
    float FairwaterDihedralDeg = 0.0f;

    /** Blend zone between sail base and hull surface — prevents hard edge. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail", meta=(ClampMin="0.0", ClampMax="200.0", EditCondition="bEnabled"))
    float HullBlendLengthCm = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Masts", meta=(ClampMin="0", ClampMax="12", EditCondition="bEnabled"))
    int32 MastCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Masts", meta=(EditCondition="bEnabled"))
    bool bHasSailHatch = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Sail|Masts", meta=(ClampMin="40.0", EditCondition="bEnabled"))
    float SailHatchDiamCm = 65.0f;
};

// ── FBowSectionDef ───────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SUB3DCORE_API FBowSectionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Sonar Dome", meta=(EditCondition="bEnabled"))
    ESub3DSonarDomeType SonarDomeType = ESub3DSonarDomeType::Conformal;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Sonar Dome", meta=(ClampMin="30.0", EditCondition="bEnabled"))
    float SonarDomeLengthCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Sonar Dome", meta=(ClampMin="30.0", EditCondition="bEnabled"))
    float SonarDomeDiamCm = 340.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Planes", meta=(EditCondition="bEnabled"))
    ESub3DBowPlaneConfig BowPlaneConfig = ESub3DBowPlaneConfig::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Planes", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float BowPlaneSpanCm = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Planes", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float BowPlaneChordCm = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Planes", meta=(ClampMin="0.0", ClampMax="0.3", EditCondition="bEnabled"))
    float BowPlaneSpineAlpha = 0.06f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Torpedo Tubes", meta=(ClampMin="0", ClampMax="12", EditCondition="bEnabled"))
    int32 TorpedoTubeCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Torpedo Tubes", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float TorpedoTubeDiamCm = 53.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Torpedo Tubes", meta=(EditCondition="bEnabled"))
    bool bTubesAngled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Torpedo Tubes", meta=(ClampMin="0.0", ClampMax="30.0", EditCondition="bEnabled"))
    float TubeAngleDeg = 8.0f;

    /** Whether bow planes can retract — visual + gameplay (can jam under damage). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Planes", meta=(EditCondition="bEnabled"))
    bool bBowPlanesRetractable = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Bow|Keel", meta=(EditCondition="bEnabled"))
    bool bHasKeel = false;
};

// ── FSternSectionDef ─────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSternSectionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Control Surfaces", meta=(EditCondition="bEnabled"))
    ESub3DControlSurfaceArrangement ControlArrangement = ESub3DControlSurfaceArrangement::CrossPattern;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Control Surfaces", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float RudderSpanCm = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Control Surfaces", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float RudderChordCm = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Control Surfaces", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float ElevatorSpanCm = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Control Surfaces", meta=(ClampMin="20.0", EditCondition="bEnabled"))
    float ElevatorChordCm = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Propulsor", meta=(EditCondition="bEnabled"))
    ESub3DPropulsorType PropulsorType = ESub3DPropulsorType::SevenBladedSkewback;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Propulsor", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float PropulsorDiamCm = 380.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Propulsor", meta=(ClampMin="0.0", EditCondition="bEnabled"))
    float SternFairingLengthCm = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Propulsor", meta=(EditCondition="bEnabled"))
    bool bHasPropGuard = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Towed Array", meta=(EditCondition="bEnabled"))
    bool bHasTowedArrayFairing = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Towed Array", meta=(ClampMin="0.5", ClampMax="1.0", EditCondition="bEnabled"))
    float TowedArraySpineAlpha = 0.88f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Towed Array", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float TowedArrayLengthCm = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Appendages|Stern|Towed Array", meta=(ClampMin="5.0", EditCondition="bEnabled"))
    float TowedArrayDiamCm = 14.0f;
};

// ── Casing type enum ────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESub3DOuterCasingType : uint8
{
    None            UMETA(DisplayName="None (single hull)"),
    Partial         UMETA(DisplayName="Partial (NATO style)"),
    Full            UMETA(DisplayName="Full Double Hull (Soviet style)")
};

// ── FOuterCasingDef ──────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SUB3DCORE_API FOuterCasingDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing", meta=(EditCondition="bEnabled"))
    ESub3DOuterCasingType CasingType = ESub3DOuterCasingType::Partial;

    /** How far the outer casing stands off from the pressure hull. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing", meta=(ClampMin="0.0", ClampMax="100.0", EditCondition="bEnabled"))
    float CasingOffsetCm = 25.0f;

    /** Spine alpha where the bow casing ends (forward coverage). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing", meta=(ClampMin="0.0", ClampMax="0.5", EditCondition="bEnabled"))
    float BowCasingEndAlpha = 0.15f;

    /** Spine alpha where the stern casing begins (aft coverage). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing", meta=(ClampMin="0.5", ClampMax="1.0", EditCondition="bEnabled"))
    float SternCasingStartAlpha = 0.80f;

    /** External main ballast tanks between pressure hull and outer casing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Ballast", meta=(EditCondition="bEnabled"))
    bool bHasExternalBallastTanks = false;

    /** Fraction of casing volume used by ballast tanks (0..1). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Ballast", meta=(ClampMin="0.0", ClampMax="0.6", EditCondition="bEnabled"))
    float BallastTankFraction = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Deck", meta=(EditCondition="bEnabled"))
    bool bHasDeckCasing = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Deck", meta=(ClampMin="50.0", EditCondition="bEnabled"))
    float DeckCasingWidthCm = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Deck", meta=(ClampMin="5.0", EditCondition="bEnabled"))
    float DeckCasingHeightCm = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Deck", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bEnabled"))
    float DeckCasingFwdAlpha = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Deck", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bEnabled"))
    float DeckCasingAftAlpha = 0.92f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Limber Holes", meta=(ClampMin="0", ClampMax="20", EditCondition="bEnabled"))
    int32 LimberHoleCountPerSide = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Anechoic", meta=(EditCondition="bEnabled"))
    ESub3DAnechoicCoating AnechoicType = ESub3DAnechoicCoating::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Ballast Vents", meta=(EditCondition="bEnabled"))
    bool bHasMainVentTrunks = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="[A] Outer Envelope|Casing|Ballast Vents", meta=(ClampMin="1", ClampMax="8", EditCondition="bEnabled"))
    int32 VentTrunkCount = 4;
};
