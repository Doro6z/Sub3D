#include "ChunkLibrary.h"

TArray<FChunkTemplate> UChunkLibrary::GetCompatibleTemplates(ENodeType NodeType) const
{
    TArray<FChunkTemplate> Compatible;
    for (const FChunkTemplate& Template : Templates)
    {
        if (Template.CompatibleNodeTypes.Contains(NodeType))
        {
            Compatible.Add(Template);
        }
    }
    return Compatible;
}

bool UChunkLibrary::AreConnectorsCompatible(const FChunkConnector& ConnectorOut, const FChunkConnector& ConnectorIn) const
{
    if (ConnectorIn.UsableWidth < ConnectorOut.UsableWidth || ConnectorIn.UsableHeight < ConnectorOut.UsableHeight)
    {
        return false;
    }

    // Biome check: Optional basic mask check
    if ((ConnectorOut.BiomeMask & ConnectorIn.BiomeMask) == 0)
    {
        return false;
    }

    return true;
}

void UChunkLibrary::ResetToDefaults()
{
    Templates.Empty();

    // 1. TunnelWide
    {
        FChunkTemplate TunnelWide;
        TunnelWide.ChunkID = TEXT("TunnelWide");
        TunnelWide.CompatibleNodeTypes = { ENodeType::CanyonWide, ENodeType::OpenWaterTransit, ENodeType::StartBuffer };
        
        TunnelWide.CarveParams.PrimaryShape = ECarveShape::OvalTunnel;
        TunnelWide.CarveParams.BaseExtents = FVector(25000.f, 40000.f, 30000.f);
        TunnelWide.CarveParams.NoiseAmplitude = 4000.f; 
        TunnelWide.CarveParams.NoiseFrequency = 0.0001f;
        TunnelWide.CarveParams.RadialSegments = 48;
        TunnelWide.CarveParams.LengthSegments = 64;

        TunnelWide.ConnectorIn.Shape = EConnectorShape::Wide;
        TunnelWide.ConnectorIn.UsableWidth = 40000.f;
        TunnelWide.ConnectorIn.UsableHeight = 30000.f;
        TunnelWide.ConnectorIn.LocalTransform = FTransform::Identity;

        TunnelWide.ConnectorOut = TunnelWide.ConnectorIn;
        TunnelWide.ConnectorOut.LocalTransform = FTransform(FVector(TunnelWide.CarveParams.BaseExtents.X, 0.f, 0.f));
        
        TunnelWide.LengthMeters = 250.f;
        Templates.Add(TunnelWide);
    }

    // 2. TunnelNarrow
    {
        FChunkTemplate TunnelNarrow;
        TunnelNarrow.ChunkID = TEXT("TunnelNarrow");
        TunnelNarrow.CompatibleNodeTypes = { ENodeType::CanyonNarrow, ENodeType::AmbushChoke };
        
        TunnelNarrow.CarveParams.PrimaryShape = ECarveShape::OvalTunnel;
        TunnelNarrow.CarveParams.BaseExtents = FVector(15000.f, 27000.f, 20000.f);
        TunnelNarrow.CarveParams.NoiseAmplitude = 1500.f;
        TunnelNarrow.CarveParams.NoiseFrequency = 0.0002f;
        TunnelNarrow.CarveParams.RadialSegments = 40;
        TunnelNarrow.CarveParams.LengthSegments = 40;

        TunnelNarrow.ConnectorIn.Shape = EConnectorShape::Oval;
        TunnelNarrow.ConnectorIn.UsableWidth = 27000.f;
        TunnelNarrow.ConnectorIn.UsableHeight = 20000.f;
        TunnelNarrow.ConnectorIn.LocalTransform = FTransform::Identity;

        TunnelNarrow.ConnectorOut = TunnelNarrow.ConnectorIn;
        TunnelNarrow.ConnectorOut.LocalTransform = FTransform(FVector(TunnelNarrow.CarveParams.BaseExtents.X, 0.f, 0.f));
        
        TunnelNarrow.LengthMeters = 150.f;
        Templates.Add(TunnelNarrow);
    }

    // 3. CavernHub (Spheroid is centered)
    {
        FChunkTemplate CavernHub;
        CavernHub.ChunkID = TEXT("CavernHub");
        CavernHub.CompatibleNodeTypes = { ENodeType::HubCavern, ENodeType::DeadEndReward, ENodeType::ExitRelief };
        
        CavernHub.CarveParams.PrimaryShape = ECarveShape::Spheroid;
        CavernHub.CarveParams.BaseExtents = FVector(40000.f, 60000.f, 40000.f);
        CavernHub.CarveParams.NoiseAmplitude = 7500.f;
        CavernHub.CarveParams.NoiseFrequency = 0.00008f;
        CavernHub.CarveParams.RadialSegments = 64;
        CavernHub.CarveParams.LengthSegments = 64;

        float HalfLen = CavernHub.CarveParams.BaseExtents.X / 2.0f;
        
        CavernHub.ConnectorIn.Shape = EConnectorShape::Spheroidal;
        CavernHub.ConnectorIn.UsableWidth = 60000.f;
        CavernHub.ConnectorIn.UsableHeight = 40000.f;
        CavernHub.ConnectorIn.LocalTransform = FTransform(FVector(-HalfLen, 0.f, 0.f));

        CavernHub.ConnectorOut = CavernHub.ConnectorIn;
        CavernHub.ConnectorOut.LocalTransform = FTransform(FVector(HalfLen, 0.f, 0.f));
        
        CavernHub.LengthMeters = 400.f;
        Templates.Add(CavernHub);
    }

    // 4. VerticalDrop
    {
        FChunkTemplate VerticalDrop;
        VerticalDrop.ChunkID = TEXT("VerticalDrop");
        VerticalDrop.CompatibleNodeTypes = { ENodeType::VerticalDrop };
        
        VerticalDrop.CarveParams.PrimaryShape = ECarveShape::VerticalDrop;
        VerticalDrop.CarveParams.BaseExtents = FVector(30000.f, 35000.f, 27500.f);
        VerticalDrop.CarveParams.NoiseAmplitude = 3000.f;
        VerticalDrop.CarveParams.NoiseFrequency = 0.0001f;
        VerticalDrop.CarveParams.RadialSegments = 48;
        VerticalDrop.CarveParams.LengthSegments = 48;

        VerticalDrop.ConnectorIn.Shape = EConnectorShape::Vertical;
        VerticalDrop.ConnectorIn.UsableWidth = 35000.f;
        VerticalDrop.ConnectorIn.UsableHeight = 27500.f;
        VerticalDrop.ConnectorIn.LocalTransform = FTransform::Identity;

        VerticalDrop.ConnectorOut = VerticalDrop.ConnectorIn;
        VerticalDrop.ConnectorOut.LocalTransform = FTransform(FVector(VerticalDrop.CarveParams.BaseExtents.X, 0.f, 0.f));
        
        VerticalDrop.LengthMeters = 300.f;
        Templates.Add(VerticalDrop);
    }

    Modify(); // Mark as dirty for editor save
    UE_LOG(LogTemp, Display, TEXT("UChunkLibrary: Reset to 4 default chunk templates complete."));
}
