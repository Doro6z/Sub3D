#include "Bake/SubmarineOpeningBakeService.h"

namespace Sub3DWave6
{
namespace
{
static bool HasFloorRegion(const TArray<FCompiledFloorRegionData>& Regions, const FName RegionId)
{
    for (const FCompiledFloorRegionData& Region : Regions)
    {
        if (Region.FloorRegionId == RegionId)
        {
            return true;
        }
    }

    return false;
}

static bool HasDeck(const TArray<FCompiledDeckData>& Decks, const FName DeckId)
{
    for (const FCompiledDeckData& Deck : Decks)
    {
        if (Deck.DeckLevelId == DeckId)
        {
            return true;
        }
    }

    return false;
}

static bool HasOpening(const TArray<FCompiledOpeningData>& Openings, const FName OpeningId)
{
    for (const FCompiledOpeningData& Opening : Openings)
    {
        if (Opening.OpeningId == OpeningId)
        {
            return true;
        }
    }

    return false;
}
} // namespace

bool FSubmarineOpeningBakeService::BakeOpenings(
    const TArray<FCompiledFloorRegionData>& FloorRegions,
    const TArray<FOpeningDef>& Openings,
    TArray<FCompiledOpeningData>& OutCompiledOpenings,
    TArray<FString>& OutErrors)
{
    OutCompiledOpenings.Reset();
    OutErrors.Reset();

    for (const FOpeningDef& Opening : Openings)
    {
        if (Opening.ParentFloorRegionId.IsNone() || !HasFloorRegion(FloorRegions, Opening.ParentFloorRegionId))
        {
            OutErrors.Add(FString::Printf(TEXT("Opening '%s' references missing Floor Region '%s'."), *Opening.OpeningId.ToString(), *Opening.ParentFloorRegionId.ToString()));
            continue;
        }

        if (Opening.SizeCm.X <= 0.0f || Opening.SizeCm.Y <= 0.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("Opening '%s' has invalid size."), *Opening.OpeningId.ToString()));
            continue;
        }

        FCompiledOpeningData& Compiled = OutCompiledOpenings.AddDefaulted_GetRef();
        Compiled.OpeningId = Opening.OpeningId;
        Compiled.Type = Opening.Type;
        Compiled.ParentFloorRegionId = Opening.ParentFloorRegionId;
        Compiled.LocalPosition = Opening.LocalPosition;
        Compiled.SizeCm = Opening.SizeCm;
    }

    return OutErrors.Num() == 0;
}

bool FSubmarineOpeningBakeService::BakeConnectors(
    const TArray<FCompiledDeckData>& Decks,
    const TArray<FCompiledOpeningData>& Openings,
    const TArray<FConnectorDef>& Connectors,
    TArray<FCompiledConnectorData>& OutCompiledConnectors,
    TArray<FString>& OutErrors)
{
    OutCompiledConnectors.Reset();
    OutErrors.Reset();

    for (const FConnectorDef& Connector : Connectors)
    {
        bool bValid = true;

        if (!HasDeck(Decks, Connector.FromDeckId) || !HasDeck(Decks, Connector.ToDeckId))
        {
            OutErrors.Add(FString::Printf(TEXT("Connector '%s' references missing decks."), *Connector.ConnectorId.ToString()));
            bValid = false;
        }

        for (const FName OpeningId : Connector.OpeningIds)
        {
            if (!HasOpening(Openings, OpeningId))
            {
                OutErrors.Add(FString::Printf(TEXT("Connector '%s' references missing Opening '%s'."), *Connector.ConnectorId.ToString(), *OpeningId.ToString()));
                bValid = false;
            }
        }

        if (Connector.WidthCm < 30.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("Connector '%s' width must be >= 30cm."), *Connector.ConnectorId.ToString()));
            bValid = false;
        }

        if (!bValid)
        {
            continue;
        }

        FCompiledConnectorData& Compiled = OutCompiledConnectors.AddDefaulted_GetRef();
        Compiled.ConnectorId = Connector.ConnectorId;
        Compiled.Type = Connector.Type;
        Compiled.OpeningIds = Connector.OpeningIds;
        Compiled.FromDeckId = Connector.FromDeckId;
        Compiled.ToDeckId = Connector.ToDeckId;
        Compiled.WidthCm = Connector.WidthCm;
    }

    return OutErrors.Num() == 0;
}

bool FSubmarineOpeningBakeService::BakeClosures(
    const TArray<FCompiledOpeningData>& Openings,
    const TArray<FClosureDef>& Closures,
    TArray<FCompiledClosureData>& OutCompiledClosures,
    TArray<FString>& OutErrors)
{
    OutCompiledClosures.Reset();
    OutErrors.Reset();

    for (const FClosureDef& Closure : Closures)
    {
        if (!HasOpening(Openings, Closure.OpeningId))
        {
            OutErrors.Add(FString::Printf(TEXT("Closure '%s' references missing Opening '%s'."), *Closure.ClosureId.ToString(), *Closure.OpeningId.ToString()));
            continue;
        }

        FCompiledClosureData& Compiled = OutCompiledClosures.AddDefaulted_GetRef();
        Compiled.ClosureId = Closure.ClosureId;
        Compiled.Type = Closure.Type;
        Compiled.OpeningId = Closure.OpeningId;
        Compiled.bClosedByDefault = Closure.bClosedByDefault;
    }

    return OutErrors.Num() == 0;
}
} // namespace Sub3DWave6