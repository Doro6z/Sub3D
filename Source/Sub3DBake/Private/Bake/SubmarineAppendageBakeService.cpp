#include "Bake/SubmarineAppendageBakeService.h"

#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Types/Sub3DCompiledTypes.h"

namespace Sub3DWave9
{
namespace
{
// ── Mesh helpers ─────────────────────────────────────────────────────────────

static void AddTriangle(FCompiledMeshSection& Sec,
    FVector3f A, FVector3f B, FVector3f C,
    FVector3f Normal)
{
    const int32 Base = Sec.Positions.Num();
    Sec.Positions.Add(A);
    Sec.Positions.Add(B);
    Sec.Positions.Add(C);
    Sec.Normals.Add(Normal);
    Sec.Normals.Add(Normal);
    Sec.Normals.Add(Normal);
    Sec.UV0.Add(FVector2f(0.0f, 0.0f));
    Sec.UV0.Add(FVector2f(1.0f, 0.0f));
    Sec.UV0.Add(FVector2f(0.5f, 1.0f));
    Sec.Indices.Add(Base);
    Sec.Indices.Add(Base + 1);
    Sec.Indices.Add(Base + 2);
}

static void AddQuad(FCompiledMeshSection& Sec,
    FVector3f A, FVector3f B, FVector3f C, FVector3f D,
    FVector3f Normal)
{
    // Two triangles: A-B-C and A-C-D
    const int32 Base = Sec.Positions.Num();
    Sec.Positions.Add(A);
    Sec.Positions.Add(B);
    Sec.Positions.Add(C);
    Sec.Positions.Add(D);
    Sec.Normals.Add(Normal);
    Sec.Normals.Add(Normal);
    Sec.Normals.Add(Normal);
    Sec.Normals.Add(Normal);
    Sec.UV0.Add(FVector2f(0.0f, 0.0f));
    Sec.UV0.Add(FVector2f(1.0f, 0.0f));
    Sec.UV0.Add(FVector2f(1.0f, 1.0f));
    Sec.UV0.Add(FVector2f(0.0f, 1.0f));
    Sec.Indices.Add(Base);
    Sec.Indices.Add(Base + 1);
    Sec.Indices.Add(Base + 2);
    Sec.Indices.Add(Base);
    Sec.Indices.Add(Base + 2);
    Sec.Indices.Add(Base + 3);
}
} // namespace

// ── SampleHullRadiusAtX ───────────────────────────────────────────────────────

float FSubmarineAppendageBakeService::SampleHullRadiusAtX(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    float WorldX)
{
    if (RingSequence.IsEmpty())
    {
        return Hull.DefaultHalfHeightCm;
    }

    const float Alpha = WorldX / FMath::Max(Hull.LengthCm, 1.0f);

    // Find surrounding rings
    for (int32 i = 1; i < RingSequence.Num(); ++i)
    {
        const Sub3DWave2::FGeneratedRingData& R0 = RingSequence[i - 1];
        const Sub3DWave2::FGeneratedRingData& R1 = RingSequence[i];

        if (Alpha >= R0.SpineAlpha && Alpha <= R1.SpineAlpha)
        {
            const float Span = R1.SpineAlpha - R0.SpineAlpha;
            if (Span < KINDA_SMALL_NUMBER)
            {
                return R0.RadiusCm;
            }
            const float T = (Alpha - R0.SpineAlpha) / Span;
            return FMath::Lerp(R0.RadiusCm, R1.RadiusCm, T);
        }
    }

    // Clamp to endpoints
    return Alpha <= RingSequence[0].SpineAlpha
        ? RingSequence[0].RadiusCm
        : RingSequence.Last().RadiusCm;
}

// ── BakeSail ─────────────────────────────────────────────────────────────────

bool FSubmarineAppendageBakeService::BakeSail(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    const FSailDef& Sail,
    FCompiledMeshSection& OutSection)
{
    if (!Sail.bEnabled || Hull.LengthCm <= 0.0f)
    {
        return false;
    }

    OutSection = FCompiledMeshSection();
    OutSection.SectionId = FName("Sail");

    const float CX = Sail.SpineAlpha * Hull.LengthCm;
    const float HullTopZ = SampleHullRadiusAtX(Hull, RingSequence, CX);
    const float BaseZ    = HullTopZ;
    const float TopZ     = BaseZ + Sail.HeightCm;
    const float HalfBeam = Sail.BeamCm * 0.5f + Sail.LateralOffsetCm;
    const float HalfBeamL = -Sail.BeamCm * 0.5f + Sail.LateralOffsetCm;

    // Leading/trailing sweep at top (positive X = aft)
    const float SwFwd = Sail.HeightCm * FMath::Tan(FMath::DegreesToRadians(Sail.LeadingEdgeSweepDeg));
    const float SwAft = Sail.HeightCm * FMath::Tan(FMath::DegreesToRadians(Sail.TrailingEdgeSweepDeg));

    const float BFwdX  = CX - Sail.BaseChordCm * 0.5f;
    const float BAftX  = CX + Sail.BaseChordCm * 0.5f;
    const float TFwdX  = CX - Sail.TopChordCm  * 0.5f + SwFwd;
    const float TAftX  = CX + Sail.TopChordCm  * 0.5f + SwAft;

    // 8 vertices: base 4, top 4 (indexed by face, so we duplicate for flat normals)
    // Port (Y+), Starboard (Y-)
    // Base: BL0=fwd-port, BL1=aft-port, BR0=aft-stbd, BR1=fwd-stbd
    // Top:  TL0=fwd-port, TL1=aft-port, TR0=aft-stbd, TR1=fwd-stbd

    // Bottom face (Z-)
    AddQuad(OutSection,
        FVector3f(BFwdX, HalfBeamL, BaseZ),
        FVector3f(BAftX, HalfBeamL, BaseZ),
        FVector3f(BAftX, HalfBeam,  BaseZ),
        FVector3f(BFwdX, HalfBeam,  BaseZ),
        FVector3f(0.0f, 0.0f, -1.0f));

    // Top face (Z+)
    AddQuad(OutSection,
        FVector3f(TFwdX, HalfBeam,  TopZ),
        FVector3f(TAftX, HalfBeam,  TopZ),
        FVector3f(TAftX, HalfBeamL, TopZ),
        FVector3f(TFwdX, HalfBeamL, TopZ),
        FVector3f(0.0f, 0.0f, 1.0f));

    // Forward face (X-)
    AddQuad(OutSection,
        FVector3f(BFwdX, HalfBeam,  BaseZ),
        FVector3f(BFwdX, HalfBeamL, BaseZ),
        FVector3f(TFwdX, HalfBeamL, TopZ),
        FVector3f(TFwdX, HalfBeam,  TopZ),
        FVector3f(-1.0f, 0.0f, 0.0f));

    // Aft face (X+)
    AddQuad(OutSection,
        FVector3f(BAftX, HalfBeamL, BaseZ),
        FVector3f(BAftX, HalfBeam,  BaseZ),
        FVector3f(TAftX, HalfBeam,  TopZ),
        FVector3f(TAftX, HalfBeamL, TopZ),
        FVector3f(1.0f, 0.0f, 0.0f));

    // Port side (Y+)
    AddQuad(OutSection,
        FVector3f(BFwdX, HalfBeam, BaseZ),
        FVector3f(TFwdX, HalfBeam, TopZ),
        FVector3f(TAftX, HalfBeam, TopZ),
        FVector3f(BAftX, HalfBeam, BaseZ),
        FVector3f(0.0f, 1.0f, 0.0f));

    // Starboard side (Y-)
    AddQuad(OutSection,
        FVector3f(BAftX, HalfBeamL, BaseZ),
        FVector3f(TAftX, HalfBeamL, TopZ),
        FVector3f(TFwdX, HalfBeamL, TopZ),
        FVector3f(BFwdX, HalfBeamL, BaseZ),
        FVector3f(0.0f, -1.0f, 0.0f));

    return OutSection.Positions.Num() > 0;
}

// ── BakeBowDome ───────────────────────────────────────────────────────────────

bool FSubmarineAppendageBakeService::BakeBowDome(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    const FBowSectionDef& BowSection,
    FCompiledMeshSection& OutSection)
{
    if (!BowSection.bEnabled || BowSection.SonarDomeType == ESub3DSonarDomeType::None)
    {
        return false;
    }

    OutSection = FCompiledMeshSection();
    OutSection.SectionId = FName("BowDome");

    const float DomeLenX   = BowSection.SonarDomeLengthCm;
    const float DomeRadYZ  = BowSection.SonarDomeDiamCm * 0.5f;
    constexpr int32 LatRings = 7;
    constexpr int32 LonSegs  = 12;

    // Half-ellipsoid: dome extends from X=0 backward (negative X = forward of hull bow)
    // φ = 0 (equator at bow ring, φ = π/2 = tip)
    TArray<FVector3f> PrevRing;
    PrevRing.SetNum(LonSegs);

    // Equator ring (at hull bow, X=0)
    for (int32 k = 0; k < LonSegs; ++k)
    {
        const float Theta = (float)k / (float)LonSegs * TWO_PI;
        PrevRing[k] = FVector3f(0.0f,
            DomeRadYZ * FMath::Cos(Theta),
            DomeRadYZ * FMath::Sin(Theta));
    }

    for (int32 i = 1; i <= LatRings; ++i)
    {
        const float Phi = ((float)i / (float)LatRings) * HALF_PI;
        const bool bTip = (i == LatRings);

        TArray<FVector3f> CurRing;
        CurRing.SetNum(LonSegs);

        const float X       = -DomeLenX  * FMath::Sin(Phi);
        const float R       =  DomeRadYZ * FMath::Cos(Phi);

        for (int32 k = 0; k < LonSegs; ++k)
        {
            const float Theta = (float)k / (float)LonSegs * TWO_PI;
            CurRing[k] = FVector3f(X,
                R * FMath::Cos(Theta),
                R * FMath::Sin(Theta));
        }

        if (bTip)
        {
            const FVector3f Tip(X, 0.0f, 0.0f);
            for (int32 k = 0; k < LonSegs; ++k)
            {
                const int32 Next = (k + 1) % LonSegs;
                const FVector3f A = PrevRing[k];
                const FVector3f B = PrevRing[Next];
                // Outward normal approx
                FVector3f N = FVector3f::CrossProduct(B - A, Tip - A);
                N.Normalize();
                AddTriangle(OutSection, A, B, Tip, N);
            }
        }
        else
        {
            for (int32 k = 0; k < LonSegs; ++k)
            {
                const int32 Next = (k + 1) % LonSegs;
                const FVector3f A = PrevRing[k];
                const FVector3f B = PrevRing[Next];
                const FVector3f C = CurRing[Next];
                const FVector3f D = CurRing[k];
                FVector3f N = FVector3f::CrossProduct(B - A, D - A);
                N.Normalize();
                AddQuad(OutSection, A, B, C, D, N);
            }
            PrevRing = CurRing;
        }
    }

    return OutSection.Positions.Num() > 0;
}

// ── BakeSternFairing ──────────────────────────────────────────────────────────

bool FSubmarineAppendageBakeService::BakeSternFairing(
    const FSubmarineHullDef& Hull,
    const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
    const FSternSectionDef& SternSection,
    FCompiledMeshSection& OutSection)
{
    if (!SternSection.bEnabled || SternSection.SternFairingLengthCm <= 0.0f)
    {
        return false;
    }

    OutSection = FCompiledMeshSection();
    OutSection.SectionId = FName("SternFairing");

    const float BaseX     = Hull.LengthCm;
    const float TipX      = BaseX + SternSection.SternFairingLengthCm;
    const float BaseRad   = SternSection.PropulsorDiamCm * 0.5f;
    constexpr int32 Segs  = 12;

    // Base ring at X=LengthCm + tip at X=TipX
    TArray<FVector3f> BaseRing;
    BaseRing.SetNum(Segs);
    for (int32 k = 0; k < Segs; ++k)
    {
        const float Theta = (float)k / (float)Segs * TWO_PI;
        BaseRing[k] = FVector3f(BaseX,
            BaseRad * FMath::Cos(Theta),
            BaseRad * FMath::Sin(Theta));
    }

    const FVector3f Tip(TipX, 0.0f, 0.0f);

    // Triangle fan from base ring to tip
    for (int32 k = 0; k < Segs; ++k)
    {
        const int32 Next = (k + 1) % Segs;
        const FVector3f A = BaseRing[k];
        const FVector3f B = BaseRing[Next];
        FVector3f N = FVector3f::CrossProduct(B - A, Tip - A);
        N.Normalize();
        AddTriangle(OutSection, A, B, Tip, N);
    }

    // Close base ring with a disk
    const FVector3f BaseCenter(BaseX, 0.0f, 0.0f);
    for (int32 k = 0; k < Segs; ++k)
    {
        const int32 Next = (k + 1) % Segs;
        AddTriangle(OutSection, BaseRing[k], BaseCenter, BaseRing[Next],
            FVector3f(-1.0f, 0.0f, 0.0f));
    }

    return OutSection.Positions.Num() > 0;
}

} // namespace Sub3DWave9
