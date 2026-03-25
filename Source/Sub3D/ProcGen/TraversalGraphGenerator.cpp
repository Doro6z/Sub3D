#include "TraversalGraphGenerator.h"

TArray<FTraversalGraphNode> UTraversalGraphGenerator::GenerateGraph(const FTraversalGenSpec& Spec) const
{
    TArray<FTraversalGraphNode> Graph;
    int32 RouteSeed = HashInts(Spec.CampaignSeed, Spec.RouteID);

    UE_LOG(LogTemp, Display, TEXT("--- PASS 1: GRAPH GENERATION START (Seed: %d) ---"), RouteSeed);

    // Determine number of nodes (8-12)
    FRandomStream Stream(RouteSeed);
    int32 NumNodes = Stream.RandRange(8, 12);
    UE_LOG(LogTemp, Log, TEXT("Generating %d nodes..."), NumNodes);
    
    // Pass 1: Sequence generation
    for (int32 i = 0; i < NumNodes; ++i)
    {
        FTraversalGraphNode Node;
        Node.NodeID = i;
        Node.PacingPosition = (float)i / (float)(NumNodes - 1);
        Node.NodeSeed = HashInts(RouteSeed, i);
        Node.NodeType = SelectNodeTypeForPacing(Node.PacingPosition, Node.NodeSeed);
        
        UE_LOG(LogTemp, Verbose, TEXT("Node %d: Type=%s, Pacing=%.2f, Seed=%d"), 
            Node.NodeID, *UEnum::GetValueAsString(Node.NodeType), Node.PacingPosition, Node.NodeSeed);

        // Linear connection
        if (i < NumNodes - 1)
        {
            Node.NextNodeIDs.Add(i + 1);
        }
        
        Graph.Add(Node);
    }

    // Pass 2: Optional Fork logic (simplified for Proto 03)
    // We attempt to add one fork around 70-80% mark
    int32 ForkTargetIndex = FMath::RoundToInt(NumNodes * 0.75f);
    if (Graph.IsValidIndex(ForkTargetIndex))
    {
        FTraversalGraphNode& ForkNode = Graph[ForkTargetIndex];
        ForkNode.bIsFork = true;
        
        // Add a side reward node
        FTraversalGraphNode RewardNode;
        RewardNode.NodeID = Graph.Num();
        RewardNode.PacingPosition = ForkNode.PacingPosition;
        RewardNode.NodeSeed = HashInts(RouteSeed, RewardNode.NodeID);
        RewardNode.NodeType = ENodeType::DeadEndReward;
        RewardNode.bIsDeadEnd = true;
        
        ForkNode.NextNodeIDs.Add(RewardNode.NodeID);
        Graph.Add(RewardNode);

        UE_LOG(LogTemp, Log, TEXT("Added Fork at Node %d -> DeadEndReward Node %d"), ForkNode.NodeID, RewardNode.NodeID);
    }

    UE_LOG(LogTemp, Display, TEXT("--- PASS 1: GRAPH GENERATION COMPLETE (%d Nodes) ---"), Graph.Num());
    return Graph;
}

ENodeType UTraversalGraphGenerator::SelectNodeTypeForPacing(float PacingPos, int32 Seed) const
{
    FRandomStream Stream(Seed);

    // 0-15% : Start / Buffer
    if (PacingPos <= 0.15f) return ENodeType::StartBuffer;
    
    // 15-40% : Rise (Open or Wide)
    if (PacingPos <= 0.40f)
    {
        return Stream.RandRange(0, 1) == 0 ? ENodeType::OpenWaterTransit : ENodeType::CanyonWide;
    }
    
    // 40-70% : Tension (Narrow or Vertical)
    if (PacingPos <= 0.70f)
    {
        return Stream.RandRange(0, 1) == 0 ? ENodeType::CanyonNarrow : ENodeType::VerticalDrop;
    }
    
    // 70-90% : Peak (Hubs)
    if (PacingPos <= 0.90f)
    {
        return ENodeType::HubCavern;
    }
    
    // 90-100% : Exit
    return ENodeType::ExitRelief;
}

int32 UTraversalGraphGenerator::HashInts(int32 A, int32 B) const
{
    // Simple FNV-1a 32-bit hash
    const uint32 FNV_OFFSET_BASIS = 2166136261U;
    const uint32 FNV_PRIME = 16777619U;

    uint32 Hash = FNV_OFFSET_BASIS;
    
    // Process A
    Hash ^= (uint32)A;
    Hash *= FNV_PRIME;
    
    // Process B
    Hash ^= (uint32)B;
    Hash *= FNV_PRIME;
    
    return (int32)Hash;
}
