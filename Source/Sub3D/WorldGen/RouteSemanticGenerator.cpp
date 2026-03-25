#include "RouteSemanticGenerator.h"

void URouteSemanticGenerator::BuildZonesFromNodes(const TArray<FTraversalTopologyNode>& Nodes,
                                                   TArray<FRouteSemanticZone>& OutZones) const
{
	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.bIsDecorativeDisconnected)
		{
			continue;
		}

		FRouteSemanticZone Zone;
		const float R = Node.PreferredRadius;

		switch (Node.NodeType)
		{
		case ETopologyNodeType::StartCheckpointSpace:
		{
			FRouteSemanticZone StartDockZone;
			StartDockZone.ZoneType = ESemanticZoneType::StartCheckpointDock;
			StartDockZone.Importance = 5;
			StartDockZone.Bounds = FBox(Node.WorldPosition - FVector(R * 1.1f), Node.WorldPosition + FVector(R * 1.1f));
			OutZones.Add(StartDockZone);
			break;
		}

		case ETopologyNodeType::SplitAnchor:
		case ETopologyNodeType::MergeAnchor:
		case ETopologyNodeType::BranchTransit:
		{
			FRouteSemanticZone SafeZone;
			if (Node.BranchIntent == EBranchProfileIntent::OptionalDangerBranch)
			{
				SafeZone.ZoneType = ESemanticZoneType::CombatSpace;
			}
			else if (Node.BranchIntent == EBranchProfileIntent::OptionalResourceDetour
				|| Node.BranchIntent == EBranchProfileIntent::PocketChain)
			{
				SafeZone.ZoneType = ESemanticZoneType::SalvageSpace;
			}
			else
			{
				SafeZone.ZoneType = ESemanticZoneType::SafeTransit;
			}
			SafeZone.Importance = 1;
			SafeZone.Bounds = FBox(Node.WorldPosition - FVector(R), Node.WorldPosition + FVector(R));
			OutZones.Add(SafeZone);
			break;
		}

		case ETopologyNodeType::HubChamber:
			Zone.ZoneType  = ESemanticZoneType::CombatSpace;
			Zone.Importance = 3;
			Zone.Bounds    = FBox(Node.WorldPosition - FVector(R*1.5f), Node.WorldPosition + FVector(R*1.5f));
			OutZones.Add(Zone);
			break;

		case ETopologyNodeType::AmbushPocket:
			Zone.ZoneType  = ESemanticZoneType::CombatSpace;
			Zone.Importance = 2;
			Zone.Bounds    = FBox(Node.WorldPosition - FVector(R), Node.WorldPosition + FVector(R));
			OutZones.Add(Zone);
			break;

		case ETopologyNodeType::NarrowTransit:
		{
			FRouteSemanticZone SteathZone;
			SteathZone.ZoneType  = ESemanticZoneType::StealthSpace;
			SteathZone.Importance = 1;
			SteathZone.Bounds    = FBox(Node.WorldPosition - FVector(R*0.8f), Node.WorldPosition + FVector(R*0.8f));
			OutZones.Add(SteathZone);
			break;
		}

		case ETopologyNodeType::VerticalDrop:
		{
			FRouteSemanticZone HazardZone;
			HazardZone.ZoneType  = ESemanticZoneType::CurrentHazard;
			HazardZone.Importance = 1;
			HazardZone.Bounds    = FBox(Node.WorldPosition - FVector(R, R, R*3.f), Node.WorldPosition + FVector(R, R, R*3.f));
			OutZones.Add(HazardZone);
			break;
		}

		case ETopologyNodeType::ExitAnchor:
		case ETopologyNodeType::EndCheckpointSpace:
		{
			FRouteSemanticZone ExitZone;
			ExitZone.ZoneType  = Node.NodeType == ETopologyNodeType::EndCheckpointSpace
				? ESemanticZoneType::EndCheckpointDock
				: ESemanticZoneType::ExitGate;
			ExitZone.Importance = 5;
			ExitZone.Bounds    = FBox(Node.WorldPosition - FVector(R), Node.WorldPosition + FVector(R));
			OutZones.Add(ExitZone);
			break;
		}

		default:
		{
			// Default: SafeTransit for main path nodes
			if (!Node.bIsBranch || Node.bIsOptionalSideContent)
			{
				FRouteSemanticZone SafeZone;
				SafeZone.ZoneType  = ESemanticZoneType::SafeTransit;
				SafeZone.Importance = 0;
				SafeZone.Bounds    = FBox(Node.WorldPosition - FVector(R), Node.WorldPosition + FVector(R));
				OutZones.Add(SafeZone);
			}
			break;
		}
		}
	}
}

void URouteSemanticGenerator::BuildSocketsFromNodes(const TArray<FTraversalTopologyNode>& Nodes,
                                                     TArray<FMissionSocketDef>& OutSockets) const
{
	int32 WreckIdx    = 0;
	int32 ResourceIdx = 0;

	for (const FTraversalTopologyNode& Node : Nodes)
	{
		FMissionSocketDef Socket;
		Socket.WorldLocation  = Node.WorldPosition;
		Socket.WorldRotation  = FRotator::ZeroRotator;
		Socket.ClearanceRadius = Node.PreferredRadius * 0.5f;

		switch (Node.NodeType)
		{
		case ETopologyNodeType::WreckPocket:
			Socket.SocketID = FName(*FString::Printf(TEXT("Wreck_%02d"), WreckIdx++));
			Socket.SupportedMissionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Mission.Salvage")));
			Socket.SupportedMissionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Mission.Scan")));
			OutSockets.Add(Socket);
			break;

		case ETopologyNodeType::ResourcePocket:
			Socket.SocketID = FName(*FString::Printf(TEXT("Resource_%02d"), ResourceIdx++));
			Socket.SupportedMissionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Mission.Mine")));
			Socket.SupportedMissionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Mission.Collect")));
			OutSockets.Add(Socket);
			break;

		default:
			break;
		}
	}
}

bool URouteSemanticGenerator::BuildSemantics(const FRouteGenSpec& Spec,
                                              const TArray<FTraversalTopologyNode>& Nodes,
                                              const TArray<FTraversalSkeletonSegment>& Skeleton,
                                              FRouteSemanticModel& OutSemantic) const
{
	OutSemantic = FRouteSemanticModel();
	BuildZonesFromNodes(Nodes, OutSemantic.Zones);
	BuildSocketsFromNodes(Nodes, OutSemantic.MissionSockets);
	return true;
}
