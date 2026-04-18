#include "Misc/AutomationTest.h"

#include "Bake/SubmarineFloorBakeService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorHeadroomValidTest,
    "Sub3D.Wave4.FloorHeadroomValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorHeadroomRejectTest,
    "Sub3D.Wave4.FloorHeadroomReject",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorWidthRejectTest,
    "Sub3D.Wave4.FloorWidthReject",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFloorCountPerBayTest,
    "Sub3D.Wave4.FloorCountPerBay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FSub3DCompiledHullData BuildHullData(float HalfWidth = 200.0f)
{
    FSub3DCompiledHullData HullData;
    HullData.LengthCm = 7000.0f;

    FSub3DCompiledHullSection Section;
    Section.PositionX = 0.0f;
    Section.HalfWidthCm = HalfWidth;
    Section.HalfHeightCm = 220.0f;
    Section.WallThicknessCm = 10.0f;
    Section.SectionProfile = ESub3DSectionProfile::Ellipse;
    Section.SectionRoundness = 0.5f;
    HullData.Sections.Add(Section);

    Section.PositionX = 7000.0f;
    HullData.Sections.Add(Section);

    return HullData;
}

static TArray<FCompiledBayData> BuildBays()
{
    TArray<FCompiledBayData> Bays;

    FCompiledBayData Bay;
    Bay.BayId = TEXT("Bay_Main");
    Bay.StartAlpha = 0.0f;
    Bay.EndAlpha = 1.0f;
    Bay.StartX = 0.0f;
    Bay.EndX = 7000.0f;
    Bay.MaxDeckLevels = 2;
    Bays.Add(Bay);

    return Bays;
}
} // namespace

bool FSub3DFloorHeadroomValidTest::RunTest(const FString& Parameters)
{
    const FSub3DCompiledHullData HullData = BuildHullData();
    const TArray<FCompiledBayData> Bays = BuildBays();

    TArray<FDeckLevelDef> DeckLevels;
    FDeckLevelDef Lower;
    Lower.DeckLevelId = TEXT("Deck_A");
    Lower.StructuralBayId = TEXT("Bay_Main");
    Lower.ZOffsetCm = -120.0f;
    DeckLevels.Add(Lower);

    FDeckLevelDef Upper;
    Upper.DeckLevelId = TEXT("Deck_B");
    Upper.StructuralBayId = TEXT("Bay_Main");
    Upper.ZOffsetCm = 100.0f;
    DeckLevels.Add(Upper);

    TArray<FCompiledDeckData> CompiledDecks;
    TArray<FString> Errors;
    TestTrue(TEXT("BakeDeckLevels"), Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(HullData, Bays, DeckLevels, CompiledDecks, Errors));

    TArray<FFloorRegionDef> Regions;
    FFloorRegionDef Region;
    Region.FloorRegionId = TEXT("Floor_A");
    Region.DeckLevelId = TEXT("Deck_A");
    Region.StructuralBayId = TEXT("Bay_Main");
    Region.StartAlpha = 0.0f;
    Region.EndAlpha = 1.0f;
    Regions.Add(Region);

    TestTrue(TEXT("ValidateFloorRegionWalkability"), Sub3DWave4::FSubmarineFloorBakeService::ValidateFloorRegionWalkability(HullData, Bays, CompiledDecks, Regions, Errors));
    return true;
}

bool FSub3DFloorHeadroomRejectTest::RunTest(const FString& Parameters)
{
    const FSub3DCompiledHullData HullData = BuildHullData();
    const TArray<FCompiledBayData> Bays = BuildBays();

    TArray<FDeckLevelDef> DeckLevels;
    FDeckLevelDef Lower;
    Lower.DeckLevelId = TEXT("Deck_A");
    Lower.StructuralBayId = TEXT("Bay_Main");
    Lower.ZOffsetCm = 0.0f;
    DeckLevels.Add(Lower);

    FDeckLevelDef Upper;
    Upper.DeckLevelId = TEXT("Deck_B");
    Upper.StructuralBayId = TEXT("Bay_Main");
    Upper.ZOffsetCm = 120.0f;
    DeckLevels.Add(Upper);

    TArray<FCompiledDeckData> CompiledDecks;
    TArray<FString> Errors;
    TestTrue(TEXT("BakeDeckLevels"), Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(HullData, Bays, DeckLevels, CompiledDecks, Errors));

    TArray<FFloorRegionDef> Regions;
    FFloorRegionDef Region;
    Region.FloorRegionId = TEXT("Floor_A");
    Region.DeckLevelId = TEXT("Deck_A");
    Region.StructuralBayId = TEXT("Bay_Main");
    Region.StartAlpha = 0.0f;
    Region.EndAlpha = 1.0f;
    Regions.Add(Region);

    Errors.Reset();
    TestFalse(TEXT("ValidateFloorRegionWalkability rejects low headroom"), Sub3DWave4::FSubmarineFloorBakeService::ValidateFloorRegionWalkability(HullData, Bays, CompiledDecks, Regions, Errors));
    return true;
}

bool FSub3DFloorWidthRejectTest::RunTest(const FString& Parameters)
{
    const FSub3DCompiledHullData HullData = BuildHullData(25.0f);
    const TArray<FCompiledBayData> Bays = BuildBays();

    TArray<FDeckLevelDef> DeckLevels;
    FDeckLevelDef Deck;
    Deck.DeckLevelId = TEXT("Deck_A");
    Deck.StructuralBayId = TEXT("Bay_Main");
    Deck.ZOffsetCm = 0.0f;
    DeckLevels.Add(Deck);

    TArray<FCompiledDeckData> CompiledDecks;
    TArray<FString> Errors;
    TestTrue(TEXT("BakeDeckLevels"), Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(HullData, Bays, DeckLevels, CompiledDecks, Errors));

    TArray<FFloorRegionDef> Regions;
    FFloorRegionDef Region;
    Region.FloorRegionId = TEXT("Floor_A");
    Region.DeckLevelId = TEXT("Deck_A");
    Region.StructuralBayId = TEXT("Bay_Main");
    Region.StartAlpha = 0.0f;
    Region.EndAlpha = 1.0f;
    Regions.Add(Region);

    Errors.Reset();
    TestFalse(TEXT("ValidateFloorRegionWalkability rejects narrow width"), Sub3DWave4::FSubmarineFloorBakeService::ValidateFloorRegionWalkability(HullData, Bays, CompiledDecks, Regions, Errors));
    return true;
}

bool FSub3DFloorCountPerBayTest::RunTest(const FString& Parameters)
{
    const FSub3DCompiledHullData HullData = BuildHullData();
    const TArray<FCompiledBayData> Bays = BuildBays();

    TArray<FDeckLevelDef> DeckLevels;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FDeckLevelDef Deck;
        Deck.DeckLevelId = FName(*FString::Printf(TEXT("Deck_%d"), Index));
        Deck.StructuralBayId = TEXT("Bay_Main");
        Deck.ZOffsetCm = -120.0f + (Index * 120.0f);
        DeckLevels.Add(Deck);
    }

    TArray<FCompiledDeckData> CompiledDecks;
    TArray<FString> Errors;
    TestFalse(TEXT("BakeDeckLevels rejects deck count over bay max"), Sub3DWave4::FSubmarineFloorBakeService::BakeDeckLevels(HullData, Bays, DeckLevels, CompiledDecks, Errors));
    return true;
}