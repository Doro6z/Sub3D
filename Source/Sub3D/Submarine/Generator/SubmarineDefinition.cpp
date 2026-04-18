#include "SubmarineDefinition.h"

const FGeneratedCompartmentDef* USubmarineDefinition::FindCompartment(FName CompartmentId) const
{
	for (const FGeneratedCompartmentDef& Comp : Compartments)
	{
		if (Comp.CompartmentId == CompartmentId)
		{
			return &Comp;
		}
	}
	return nullptr;
}

const FGeneratedConnectionDef* USubmarineDefinition::FindConnection(FName ConnectionId) const
{
	for (const FGeneratedConnectionDef& Conn : Connections)
	{
		if (Conn.ConnectionId == ConnectionId)
		{
			return &Conn;
		}
	}
	return nullptr;
}

TArray<const FGeneratedStationSlotDef*> USubmarineDefinition::GetStationsInCompartment(FName CompartmentId) const
{
	TArray<const FGeneratedStationSlotDef*> Result;
	for (const FGeneratedStationSlotDef& Slot : StationSlots)
	{
		if (Slot.CompartmentId == CompartmentId)
		{
			Result.Add(&Slot);
		}
	}
	return Result;
}

const FGeneratedCompartmentDef* USubmarineDefinition::FindCompartmentAtLocalLocation(const FVector& LocalPosition) const
{
	for (const FGeneratedCompartmentDef& Comp : Compartments)
	{
		if (LocalPosition.X >= Comp.HydroBoundsMin.X && LocalPosition.X <= Comp.HydroBoundsMax.X &&
			LocalPosition.Y >= Comp.HydroBoundsMin.Y && LocalPosition.Y <= Comp.HydroBoundsMax.Y &&
			LocalPosition.Z >= Comp.HydroBoundsMin.Z && LocalPosition.Z <= Comp.HydroBoundsMax.Z)
		{
			return &Comp;
		}
	}
	return nullptr;
}

bool USubmarineDefinition::IsValid() const
{
	if (Compartments.Num() == 0)
	{
		return false;
	}

	// -- Compartment IDs: non-None, unique, capacity > 0, bounds valid --
	{
		TSet<FName> SeenIds;
		for (const FGeneratedCompartmentDef& Comp : Compartments)
		{
			if (Comp.CompartmentId.IsNone())
			{
				return false;
			}
			bool bAlreadyInSet = false;
			SeenIds.Add(Comp.CompartmentId, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
			if (Comp.CapacityLiters <= 0.f)
			{
				return false;
			}
			// Bounds min must be strictly less than max on all axes
			if (Comp.HydroBoundsMin.X >= Comp.HydroBoundsMax.X ||
				Comp.HydroBoundsMin.Y >= Comp.HydroBoundsMax.Y ||
				Comp.HydroBoundsMin.Z >= Comp.HydroBoundsMax.Z)
			{
				return false;
			}
		}
	}

	// -- Flood graph: 1:1 bijection between volumes and compartments --
	if (FloodGraph.Volumes.Num() != Compartments.Num())
	{
		return false;
	}
	{
		TSet<FName> SeenVolumeIds;
		for (const FDerivedFloodVolume& Vol : FloodGraph.Volumes)
		{
			if (Vol.VolumeId.IsNone() || !FindCompartment(Vol.VolumeId))
			{
				return false;
			}
			bool bAlreadyInSet = false;
			SeenVolumeIds.Add(Vol.VolumeId, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
		}
	}

	// -- Flood graph edges: reference existing volumes or exterior,
	//    ClosureId (if set) must match a ConnectionId --
	for (const FFloodGraphEdge& Edge : FloodGraph.Edges)
	{
		if (Edge.VolumeA.IsNone())
		{
			return false;
		}
		if (!FindCompartment(Edge.VolumeA))
		{
			return false;
		}
		if (Edge.bExteriorEdge)
		{
			// Exterior edges must have VolumeB == NAME_None
			if (!Edge.VolumeB.IsNone())
			{
				return false;
			}
		}
		else
		{
			if (Edge.VolumeB.IsNone() || !FindCompartment(Edge.VolumeB))
			{
				return false;
			}
		}
		// If the edge has a ClosureId, it must reference an existing connection
		if (!Edge.ClosureId.IsNone() && !FindConnection(Edge.ClosureId))
		{
			return false;
		}
	}

	// -- Every non-Open connection must have a corresponding flood graph edge --
	for (const FGeneratedConnectionDef& Conn : Connections)
	{
		if (Conn.ConnectionType == EConnectionType::Open)
		{
			continue;
		}
		bool bHasEdge = false;
		for (const FFloodGraphEdge& Edge : FloodGraph.Edges)
		{
			if (Edge.ClosureId == Conn.ConnectionId)
			{
				bHasEdge = true;
				break;
			}
		}
		if (!bHasEdge)
		{
			return false;
		}
	}

	// -- Connections: IDs non-None, unique, exterior convention enforced --
	{
		TSet<FName> SeenConnIds;
		for (const FGeneratedConnectionDef& Conn : Connections)
		{
			if (Conn.ConnectionId.IsNone())
			{
				return false;
			}
			bool bAlreadyInSet = false;
			SeenConnIds.Add(Conn.ConnectionId, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
			if (!FindCompartment(Conn.CompartmentA))
			{
				return false;
			}

			const bool bIsExterior = (Conn.CompartmentB.IsNone());
			const bool bIsExteriorType = (Conn.ConnectionType == EConnectionType::ExteriorHatch);

			// Enforce: ExteriorHatch <-> CompartmentB == NAME_None
			if (bIsExterior != bIsExteriorType)
			{
				return false;
			}

			if (!bIsExterior && !FindCompartment(Conn.CompartmentB))
			{
				return false;
			}
		}
	}

	// -- Stations: IDs non-None, unique, reference existing compartments --
	{
		TSet<FName> SeenStationIds;
		for (const FGeneratedStationSlotDef& Slot : StationSlots)
		{
			if (Slot.StationId.IsNone() || Slot.CompartmentId.IsNone())
			{
				return false;
			}
			bool bAlreadyInSet = false;
			SeenStationIds.Add(Slot.StationId, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
			if (!FindCompartment(Slot.CompartmentId))
			{
				return false;
			}
		}
	}

	// -- Spawns: IDs non-None, unique --
	{
		TSet<FName> SeenSpawnIds;
		for (const FGeneratedSpawnPointDef& Spawn : SpawnPoints)
		{
			if (Spawn.SpawnId.IsNone())
			{
				return false;
			}
			bool bAlreadyInSet = false;
			SeenSpawnIds.Add(Spawn.SpawnId, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
		}
	}

	return true;
}
