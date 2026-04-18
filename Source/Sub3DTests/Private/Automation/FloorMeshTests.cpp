#include "Misc/AutomationTest.h"

#include "Bake/SubmarineFloorBakeService.h"
#include "Bake/SubmarineHullBakeService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"

// --------------------------------------------------------------------------
// Floor mesh geometry
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorMeshHasGeometryTest,
    "Sub3D.Wave4.FloorMeshHasGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorMeshNormalsUpTest,
    "Sub3D.Wave4.FloorMeshNormalsPointUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorMeshSectionIdMatchesRegionTest,
    "Sub3D.Wave4.FloorMeshSectionIdMatchesRegion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorMeshZCoordIsAtDeckHeightTest,
    "Sub3D.Wave4.FloorMeshZCoordIsAtDeckHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorMeshWidthConformsToHullTest,
    "Sub3D.Wave4.FloorMeshWidthConformsToHull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------
// Hull thickness
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullInteriorSmallerThanExteriorTest,
    "Sub3D.Wave2.InteriorHullSmallerThanExterior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullInteriorThicknessMatchesWallTest,
    "Sub3D.Wave2.InteriorHullThicknessMatchesWallThickness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullInteriorHasGeometryTest,
    "Sub3D.Wave2.InteriorHullHasGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static constexpr float TestHullLength = 7000.0f;
static constexpr float TestHalfWidth = 200.0f;
static constexpr float TestHalfHeight = 220.0f;
static constexpr float TestWallThickness = 15.0f;
static constexpr float TestDeckZ = -60.0f;

static FSub3DCompiledHullData BuildUniformHullData()
{
    FSub3DCompiledHullData HullData;
    HullData.LengthCm = TestHullLength;

    FSub3DCompiledHullSection Section;
    Section.HalfWidthCm = TestHalfWidth;
    Section.HalfHeightCm = TestHalfHeight;
    Section.WallThicknessCm = TestWallThickness;
    Section.SectionProfile = ESub3DSectionProfile::Ellipse;
    Section.SectionRoundness = 0.5f;

    Section.PositionX = 0.0f;
    HullData.Sections.Add(Section);
    Section.PositionX = TestHullLength;
    HullData.Sections.Add(Section);

    return HullData;
}

static FSubmarineHullDef BuildTestHullDef()
{
    FSubmarineHullDef Hull;
    Hull.LengthCm = TestHullLength;
    Hull.DefaultHalfWidthCm = TestHalfWidth;
    Hull.DefaultHalfHeightCm = TestHalfHeight;
    Hull.DefaultWallThicknessCm = TestWallThickness;
    Hull.DefaultSectionProfile = ESub3DSectionProfile::Ellipse;
    Hull.DefaultSectionRoundness = 0.5f;
    return Hull;
}

static TArray<FCompiledBayData> BuildSingleBay()
{
    TArray<FCompiledBayData> Bays;
    FCompiledBayData Bay;
    Bay.BayId = TEXT("Bay_Main");
    Bay.StartAlpha = 0.0f;
    Bay.EndAlpha = 1.0f;
    Bay.MaxDeckLevels = 2;
    Bays.Add(Bay);
    return Bays;
}

static bool BakeFloorForTest(
    TArray<FCompiledFloorRegionData>& OutFloors,
    const float DeckZ = TestDeckZ)
{
    const FSub3DCompiledHullData HullData = BuildUniformHullData();
    const TArray<FCompiledBayData> Bays = BuildSingleBay();

    TArray<FDeckLevelDef> DeckLevels;
    FDeckLevelDef Deck;
    Deck.DeckLevelId = TEXT("Deck_A");
    Deck.StructuralBayId = TEXT("Bay_Main");
    Deck.ZOffsetCm = DeckZ;
    DeckLevels.Add(Deck);

    TArray<FCompiledDeckData> CompiledDecks;
    TArray<FString> Errors;
    if (!Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(HullData, Bays, DeckLevels, CompiledDecks, Errors))
    {
        return false;
    }

    TArray<FFloorRegionDef> FloorRegions;
    FFloorRegionDef Region;
    Region.FloorRegionId = TEXT("Floor_Main");
    Region.DeckLevelId = TEXT("Deck_A");
    Region.StructuralBayId = TEXT("Bay_Main");
    Region.StartAlpha = 0.0f;
    Region.EndAlpha = 1.0f;
    FloorRegions.Add(Region);

    return Sub3DWave4::FSubmarineFloorBakeService::BakeFloorRegions(
        HullData, Bays, CompiledDecks, FloorRegions, OutFloors, Errors);
}

static TArray<FControlRingDef> BuildTwoRings(const FSubmarineHullDef& Hull)
{
    TArray<FControlRingDef> Rings;
    FControlRingDef A;
    A.ControlRingId = TEXT("A");
    A.PositionX = 0.0f;
    A.HalfWidthCm = Hull.DefaultHalfWidthCm;
    A.HalfHeightCm = Hull.DefaultHalfHeightCm;
    A.SectionProfile = Hull.DefaultSectionProfile;
    A.SectionRoundness = Hull.DefaultSectionRoundness;
    A.WallThicknessCm = Hull.DefaultWallThicknessCm;
    Rings.Add(A);

    FControlRingDef B = A;
    B.ControlRingId = TEXT("B");
    B.PositionX = Hull.LengthCm;
    Rings.Add(B);

    return Rings;
}
} // namespace

// ==========================================================================
// Floor mesh geometry tests
// ==========================================================================

bool FSub3DFloorMeshHasGeometryTest::RunTest(const FString& Parameters)
{
    TArray<FCompiledFloorRegionData> Floors;
    TestTrue(TEXT("BakeFloorRegions succeeds"), BakeFloorForTest(Floors));
    TestTrue(TEXT("At least one floor region"), Floors.Num() > 0);
    if (Floors.Num() == 0)
    {
        return false;
    }

    const FCompiledMeshSection& Mesh = Floors[0].FloorMesh;
    TestTrue(TEXT("Floor mesh has positions"), Mesh.Positions.Num() > 0);
    TestTrue(TEXT("Floor mesh has indices"), Mesh.Indices.Num() > 0);
    TestTrue(TEXT("Floor mesh indices are multiple of 3"), Mesh.Indices.Num() % 3 == 0);
    return true;
}

bool FSub3DFloorMeshNormalsUpTest::RunTest(const FString& Parameters)
{
    TArray<FCompiledFloorRegionData> Floors;
    TestTrue(TEXT("BakeFloorRegions succeeds"), BakeFloorForTest(Floors));
    if (Floors.Num() == 0 || Floors[0].FloorMesh.Normals.Num() == 0)
    {
        return false;
    }

    bool bAllUp = true;
    for (const FVector3f& Normal : Floors[0].FloorMesh.Normals)
    {
        if (!Normal.Equals(FVector3f(0.0f, 0.0f, 1.0f), 0.001f))
        {
            bAllUp = false;
            break;
        }
    }
    TestTrue(TEXT("All floor normals point up (0,0,1)"), bAllUp);
    return true;
}

bool FSub3DFloorMeshSectionIdMatchesRegionTest::RunTest(const FString& Parameters)
{
    TArray<FCompiledFloorRegionData> Floors;
    TestTrue(TEXT("BakeFloorRegions succeeds"), BakeFloorForTest(Floors));
    if (Floors.Num() == 0)
    {
        return false;
    }

    TestEqual(TEXT("Floor mesh SectionId matches region id"),
        Floors[0].FloorMesh.SectionId,
        FName(TEXT("Floor_Main")));
    return true;
}

bool FSub3DFloorMeshZCoordIsAtDeckHeightTest::RunTest(const FString& Parameters)
{
    const float DeckZ = -80.0f;
    TArray<FCompiledFloorRegionData> Floors;
    TestTrue(TEXT("BakeFloorRegions succeeds"), BakeFloorForTest(Floors, DeckZ));
    if (Floors.Num() == 0 || Floors[0].FloorMesh.Positions.Num() == 0)
    {
        return false;
    }

    bool bAllAtDeckZ = true;
    for (const FVector3f& Pos : Floors[0].FloorMesh.Positions)
    {
        if (!FMath::IsNearlyEqual(Pos.Z, DeckZ, 0.1f))
        {
            bAllAtDeckZ = false;
            break;
        }
    }
    TestTrue(TEXT("All floor vertices are at the deck Z height"), bAllAtDeckZ);
    return true;
}

bool FSub3DFloorMeshWidthConformsToHullTest::RunTest(const FString& Parameters)
{
    TArray<FCompiledFloorRegionData> Floors;
    TestTrue(TEXT("BakeFloorRegions succeeds"), BakeFloorForTest(Floors, TestDeckZ));
    if (Floors.Num() == 0 || Floors[0].FloorMesh.Positions.Num() == 0)
    {
        return false;
    }

    // Interior half-width at Z=-60 with ellipse: HW_interior = (TestHalfWidth - TestWallThickness)
    // * sqrt(1 - (DeckZ/HH_interior)^2)
    const float HH_i = TestHalfHeight - TestWallThickness;
    const float HW_i = TestHalfWidth - TestWallThickness;
    const float ZRatio = FMath::Abs(TestDeckZ) / HH_i;
    const float ExpectedHalfWidth = HW_i * FMath::Sqrt(FMath::Max(1.0f - ZRatio * ZRatio, 0.0f));

    float MaxAbsY = 0.0f;
    for (const FVector3f& Pos : Floors[0].FloorMesh.Positions)
    {
        MaxAbsY = FMath::Max(MaxAbsY, FMath::Abs(Pos.Y));
    }

    TestTrue(TEXT("Floor max Y matches interior hull half-width"),
        FMath::IsNearlyEqual(MaxAbsY, ExpectedHalfWidth, 1.0f));
    return true;
}

// ==========================================================================
// Hull thickness tests
// ==========================================================================

bool FSub3DHullInteriorSmallerThanExteriorTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHullDef();
    const TArray<FControlRingDef> Rings = BuildTwoRings(Hull);

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TArray<FString> Errors;
    TestTrue(TEXT("BuildRingSequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
        Hull, Rings, 16, RingSequence, Errors));

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = 16;
    Settings.bBakeCollision = false;

    FCompiledMeshSection Exterior;
    FCompiledMeshSection Interior;
    TestTrue(TEXT("BakeExteriorHull"), Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(Hull, RingSequence, Settings, Exterior));
    TestTrue(TEXT("BakeInteriorHull"), Sub3DWave2::FSubmarineHullBakeService::BakeInteriorHull(Hull, RingSequence, Settings, Interior));

    // Compute average radius of each mesh
    float ExtAvgRadius = 0.0f;
    for (const FVector3f& Pos : Exterior.Positions)
    {
        ExtAvgRadius += FMath::Sqrt(Pos.Y * Pos.Y + Pos.Z * Pos.Z);
    }
    if (Exterior.Positions.Num() > 0)
    {
        ExtAvgRadius /= static_cast<float>(Exterior.Positions.Num());
    }

    float IntAvgRadius = 0.0f;
    for (const FVector3f& Pos : Interior.Positions)
    {
        IntAvgRadius += FMath::Sqrt(Pos.Y * Pos.Y + Pos.Z * Pos.Z);
    }
    if (Interior.Positions.Num() > 0)
    {
        IntAvgRadius /= static_cast<float>(Interior.Positions.Num());
    }

    TestTrue(TEXT("Interior hull average radius < Exterior hull average radius"), IntAvgRadius < ExtAvgRadius);
    return true;
}

bool FSub3DHullInteriorThicknessMatchesWallTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHullDef();
    const TArray<FControlRingDef> Rings = BuildTwoRings(Hull);

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TArray<FString> Errors;
    TestTrue(TEXT("BuildRingSequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
        Hull, Rings, 16, RingSequence, Errors));

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = 16;
    Settings.bBakeCollision = false;

    FCompiledMeshSection Exterior;
    FCompiledMeshSection Interior;
    TestTrue(TEXT("BakeExteriorHull"), Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(Hull, RingSequence, Settings, Exterior));
    TestTrue(TEXT("BakeInteriorHull"), Sub3DWave2::FSubmarineHullBakeService::BakeInteriorHull(Hull, RingSequence, Settings, Interior));

    if (Exterior.Positions.Num() == 0 || Interior.Positions.Num() == 0)
    {
        return false;
    }

    // At the cylindrical midbody, max |Y| of exterior = HalfWidth, of interior = HalfWidth - WallThickness
    float ExtMaxY = 0.0f;
    float IntMaxY = 0.0f;
    for (const FVector3f& Pos : Exterior.Positions)
    {
        ExtMaxY = FMath::Max(ExtMaxY, FMath::Abs(Pos.Y));
    }
    for (const FVector3f& Pos : Interior.Positions)
    {
        IntMaxY = FMath::Max(IntMaxY, FMath::Abs(Pos.Y));
    }

    const float ActualThickness = ExtMaxY - IntMaxY;
    TestTrue(TEXT("Thickness ≈ WallThicknessCm (±2cm tolerance)"),
        FMath::IsNearlyEqual(ActualThickness, TestWallThickness, 2.0f));
    return true;
}

bool FSub3DHullInteriorHasGeometryTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHullDef();
    const TArray<FControlRingDef> Rings = BuildTwoRings(Hull);

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TArray<FString> Errors;
    TestTrue(TEXT("BuildRingSequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(
        Hull, Rings, 16, RingSequence, Errors));

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = 16;
    Settings.bBakeCollision = false;

    FCompiledMeshSection Interior;
    TestTrue(TEXT("BakeInteriorHull"), Sub3DWave2::FSubmarineHullBakeService::BakeInteriorHull(Hull, RingSequence, Settings, Interior));
    TestTrue(TEXT("Interior hull has vertices"), Interior.Positions.Num() > 0);
    TestTrue(TEXT("Interior hull has indices"), Interior.Indices.Num() > 0);
    return true;
}
