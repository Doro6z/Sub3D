#include "ChunkAssembler.h"
#include "ChunkLibrary.h"

TArray<FChunkInstance> UChunkAssembler::AssembleChunks(
    const TArray<FTraversalGraphNode>& Graph,
    const UChunkLibrary* Library,
    int32 RouteSeed) const
{
    TArray<FChunkInstance> Instances;
    if (!Library || Graph.Num() == 0) 
    {
        UE_LOG(LogTemp, Error, TEXT("UChunkAssembler: Library is null or Graph is empty!"));
        return Instances;
    }

    UE_LOG(LogTemp, Display, TEXT("--- PASS 2: CHUNK ASSEMBLY START ---"));

    TSet<int32> ProcessedNodes;
    ProcessNode(0, FTransform::Identity, FChunkConnector(), true, Graph, Library, Instances, ProcessedNodes);

    UE_LOG(LogTemp, Display, TEXT("--- PASS 2: CHUNK ASSEMBLY COMPLETE (%d Instances) ---"), Instances.Num());
    return Instances;
}

void UChunkAssembler::ProcessNode(
    int32 NodeID,
    const FTransform& ParentWorldTransform,
    const FChunkConnector& ParentConnectorOut,
    bool bIsRoot,
    const TArray<FTraversalGraphNode>& Graph,
    const UChunkLibrary* Library,
    TArray<FChunkInstance>& OutInstances,
    TSet<int32>& ProcessedNodes) const
{
    if (ProcessedNodes.Contains(NodeID)) return;
    ProcessedNodes.Add(NodeID);

    const FTraversalGraphNode* Node = Graph.FindByPredicate([NodeID](const FTraversalGraphNode& N) { return N.NodeID == NodeID; });
    if (!Node) return;

    const FChunkTemplate* Template = SelectTemplate(Node->NodeType, Library, Node->NodeSeed);
    if (!Template) return;

    FChunkInstance NewInstance;
    NewInstance.ChunkID = Template->ChunkID;
    NewInstance.ChunkSeed = Node->NodeSeed;
    NewInstance.ConnectorIn = Template->ConnectorIn;
    NewInstance.ConnectorOut = Template->ConnectorOut;

    if (bIsRoot)
    {
        NewInstance.WorldTransform = ParentWorldTransform;
    }
    else
    {
        NewInstance.WorldTransform = ResolveNextTransform(ParentWorldTransform, ParentConnectorOut, Template->ConnectorIn);
    }

    OutInstances.Add(NewInstance);
    
    UE_LOG(LogTemp, Log, TEXT("Node %d: Assembled %s at %s"), 
        NodeID, *NewInstance.ChunkID.ToString(), *NewInstance.WorldTransform.GetLocation().ToString());

    // Recurse to children
    for (int32 NextID : Node->NextNodeIDs)
    {
        ProcessNode(NextID, NewInstance.WorldTransform, NewInstance.ConnectorOut, false, Graph, Library, OutInstances, ProcessedNodes);
    }
}

FTransform UChunkAssembler::ResolveNextTransform(
    const FTransform& CurrentWorldTransform,
    const FChunkConnector& ConnectorOut,
    const FChunkConnector& NextConnectorIn) const
{
    // 1. Find the World Transform of the current Outbound connector
    // ConnectorOut.LocalTransform is relative to CurrentWorldTransform
    FTransform ConnOutWorld = ConnectorOut.LocalTransform * CurrentWorldTransform;

    // 2. We want: ConnInWorld == ConnOutWorld
    // ConnInWorld = NextLocalIn * NextWorldTransform
    // NextWorldTransform = NextLocalIn^-1 * ConnOutWorld
    
    // Note: Since connectors face each other, the 'In' connector usually faces 0,0,1 or similar,
    // and the 'Out' usually faces 0,0,1 relative to local. 
    // We might need an 180 flip if the authoring has both 'In' and 'Out' facing "forward".
    // For now, assume "Snap" means matching transforms exactly (Socket alignment style).
    
    FTransform NextWorldTransform = NextConnectorIn.LocalTransform.Inverse() * ConnOutWorld;

    return NextWorldTransform;
}

const FChunkTemplate* UChunkAssembler::SelectTemplate(
    ENodeType NodeType,
    const UChunkLibrary* Library,
    int32 Seed) const
{
    TArray<int32> AbsoluteIndices;
    for(int32 i=0; i<Library->Templates.Num(); ++i)
    {
        if(Library->Templates[i].CompatibleNodeTypes.Contains(NodeType))
            AbsoluteIndices.Add(i);
    }
    
    if(AbsoluteIndices.Num() == 0) return nullptr;
    
    FRandomStream Stream(Seed);
    int32 RandomAbsoluteIndex = AbsoluteIndices[Stream.RandRange(0, AbsoluteIndices.Num() - 1)];
    return &Library->Templates[RandomAbsoluteIndex];
}
