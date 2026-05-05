#include "Graph/SubmarineFloodGraphBuilder.h"

namespace Sub3DWave8
{
namespace
{
static float ComputeBayCapacityLiters(const FCompiledBayData& Bay)
{
    const float LengthCm = FMath::Max(Bay.EndX - Bay.StartX, 1.0f);
    const float ApproxAreaCm2 = 250.0f * 250.0f;
    const float VolumeCm3 = LengthCm * ApproxAreaCm2;
    return VolumeCm3 / 1000.0f;
}

static FName MakeVolumeId(const FCompiledBayData& Bay)
{
    return FName(*FString::Printf(TEXT("DerivedFloodVolume_%s"), *Bay.BayId.ToString()));
}

static FName FindClosureForConnector(const FCompiledConnectorData& Connector, const TArray<FCompiledClosureData>& Closures)
{
    for (const FCompiledClosureData& Closure : Closures)
    {
        if (Connector.OpeningIds.Contains(Closure.OpeningId))
        {
            return Closure.ClosureId;
        }
    }

    return NAME_None;
}
} // namespace

bool FSubmarineFloodGraphBuilder::BuildFloodGraph(
    const TArray<FCompiledBayData>& StructuralBays,
    const TArray<FCompiledDeckData>& Decks,
    const TArray<FCompiledConnectorData>& Connectors,
    const TArray<FCompiledClosureData>& Closures,
    FCompiledFloodGraph& OutFloodGraph,
    TArray<FString>& OutErrors)
{
    OutFloodGraph = FCompiledFloodGraph();
    OutErrors.Reset();

    if (StructuralBays.IsEmpty())
    {
        OutErrors.Add(TEXT("Cannot derive flood graph without Structural Bays."));
        return false;
    }

    TMap<FName, FName> BayToVolume;
    for (const FCompiledBayData& Bay : StructuralBays)
    {
        FDerivedFloodVolume& Volume = OutFloodGraph.Volumes.AddDefaulted_GetRef();
        Volume.VolumeId = MakeVolumeId(Bay);
        Volume.CapacityLiters = ComputeBayCapacityLiters(Bay);
        BayToVolume.Add(Bay.BayId, Volume.VolumeId);
    }

    TMap<FName, FName> DeckToVolume;
    for (const FCompiledDeckData& Deck : Decks)
    {
        const FName* VolumeId = BayToVolume.Find(Deck.StructuralBayId);
        if (VolumeId)
        {
            DeckToVolume.Add(Deck.DeckLevelId, *VolumeId);
        }
    }

    for (const FCompiledConnectorData& Connector : Connectors)
    {
        const FName* FromVolume = DeckToVolume.Find(Connector.FromDeckId);
        const FName* ToVolume = DeckToVolume.Find(Connector.ToDeckId);

        if (!FromVolume || !ToVolume)
        {
            // Connector can still be valid for navigation without direct flood edge.
            continue;
        }

        FFloodGraphEdge& Edge = OutFloodGraph.Edges.AddDefaulted_GetRef();
        Edge.VolumeA = *FromVolume;
        Edge.VolumeB = *ToVolume;
        Edge.ClosureId = FindClosureForConnector(Connector, Closures);
        Edge.PassageAreaCm2 = FMath::Max(Connector.WidthCm * 180.0f, 0.0f);
        Edge.bExteriorEdge = false;
    }

    // Add one explicit exterior edge on the first volume for breach-ready routing.
    // [P0.3 cleanup] Previously sorted by BoundsMin.X to pick fore-most volume; that field
    // was removed (no other readers). Sub3DBake is legacy paused for FP — first-added volume
    // is acceptable here. Re-introduce a position-based pick if Sub3DBake is reactivated.
    if (!OutFloodGraph.Volumes.IsEmpty())
    {
        FFloodGraphEdge& ExteriorEdge = OutFloodGraph.Edges.AddDefaulted_GetRef();
        ExteriorEdge.VolumeA = OutFloodGraph.Volumes[0].VolumeId;
        ExteriorEdge.VolumeB = NAME_None;
        ExteriorEdge.ClosureId = NAME_None;
        ExteriorEdge.PassageAreaCm2 = 0.0f;
        ExteriorEdge.bExteriorEdge = true;
    }

    return true;
}
} // namespace Sub3DWave8
