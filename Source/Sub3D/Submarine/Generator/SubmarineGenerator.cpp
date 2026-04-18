#include "SubmarineGenerator.h"

#include "SubmarineDefinition.h"
#include "SubmarineDefinitionTypes.h"
#include "SubmarineGeneratorSpec.h"
#include "SubmarineGeneratorEnvelopeDef.h"
#include "Types/Sub3DFloodTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubGenerator, Log, All);

// --- Generate ----------------------------------------------------------------

USubmarineDefinition* USubmarineGenerator::Generate(const USubmarineGeneratorSpec* Spec)
{
	if (!Spec)
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: null spec"));
		return nullptr;
	}

	if (!Spec->Envelope)
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: spec has no envelope"));
		return nullptr;
	}

	if (Spec->BulkheadPositionsNormalized.Num() == 0)
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: spec has no bulkheads"));
		return nullptr;
	}

	if (Spec->Passages.Num() == 0)
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: spec has no passages (need one per bulkhead)"));
		return nullptr;
	}

	if (Spec->Passages.Num() != Spec->BulkheadPositionsNormalized.Num())
	{
		UE_LOG(LogSubGenerator, Warning, TEXT("Generate: passages count (%d) != bulkheads count (%d), will clamp"),
			Spec->Passages.Num(), Spec->BulkheadPositionsNormalized.Num());
	}

	USubmarineDefinition* Def = NewObject<USubmarineDefinition>();

	Def->WallThicknessCm = Spec->WallThicknessCm;

	// Step 1: Hull metrics
	if (!ResolveEnvelope(Spec->Envelope, Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: ResolveEnvelope failed"));
		return nullptr;
	}

	// Step 2: Compartments from bulkheads
	if (!DeriveCompartments(Spec->Envelope, Spec->BulkheadPositionsNormalized,
		Spec->WallThicknessCm, Spec->FloorDropBiasCm, Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: DeriveCompartments failed"));
		return nullptr;
	}

	// Step 3: Connections from passages
	if (!GenerateConnections(Spec, Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: GenerateConnections failed"));
		return nullptr;
	}

	// Step 4: Airlock
	if (!GenerateAirlock(Spec, Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: GenerateAirlock failed"));
		return nullptr;
	}

	// Step 5: Flood graph
	if (!BuildFloodGraph(Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: BuildFloodGraph failed"));
		return nullptr;
	}

	// Step 6: Stations
	if (!PlaceStations(Spec, Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: PlaceStations failed"));
		return nullptr;
	}

	// Step 7: Spawns
	if (!PlaceSpawns(Def))
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: PlaceSpawns failed"));
		return nullptr;
	}

	if (!Def->IsValid())
	{
		UE_LOG(LogSubGenerator, Error, TEXT("Generate: produced definition failed validation"));
		return nullptr;
	}

	UE_LOG(LogSubGenerator, Log,
		TEXT("Generate: success | %d compartments | %d connections | %d flood edges | %d stations | %d spawns"),
		Def->Compartments.Num(),
		Def->Connections.Num(),
		Def->FloodGraph.Edges.Num(),
		Def->StationSlots.Num(),
		Def->SpawnPoints.Num());

	return Def;
}

// --- Step 1: ResolveEnvelope -------------------------------------------------

bool USubmarineGenerator::ResolveEnvelope(const USubmarineGeneratorEnvelopeDef* Envelope, USubmarineDefinition* Def) const
{
	if (!Envelope || !Def)
	{
		return false;
	}

	const float SpineLength = FMath::Max(100.f, Envelope->SpineLengthCm);

	// Sample the envelope at the midpoint for nominal beam/height.
	// EvaluateRadius already incorporates the bow/stern taper since the M1.2
	// refactor; do not multiply by EvaluateBowSternTaper again.
	const float MidRadius = Envelope->EvaluateRadius(0.5f);
	const float EffectiveRadius = FMath::Max(50.f, MidRadius);
	const float HalfW = EffectiveRadius * FMath::Max(0.5f, Envelope->WidthToHeightRatio);

	Def->HullLengthCm = SpineLength;
	Def->HullBeamCm = HalfW * 2.f;
	Def->HullHeightCm = EffectiveRadius * 2.f;

	// Approximate submerged volume: integrate superellipse cross-sections along the spine.
	// V = integral of A(t) dt where A(t) = pi * a(t) * b(t) * (4/n)^(1/n) * Gamma(1+1/n)^2 / Gamma(1+2/n)
	// For a quick FP approximation, sample at regular intervals.
	constexpr int32 NumSamples = 32;
	float TotalVolumeCm3 = 0.f;
	const float DeltaT = 1.f / static_cast<float>(NumSamples);
	const float N = FMath::Max(0.5f, Envelope->SectionExponent);
	const float WH = FMath::Max(0.5f, Envelope->WidthToHeightRatio);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = (static_cast<float>(i) + 0.5f) * DeltaT;
		// EvaluateRadius already bakes in the taper since M1.2.
		const float R = Envelope->EvaluateRadius(T);
		const float A = R;
		const float B = R * WH;
		// Superellipse area: 4 * a * b * Gamma(1 + 1/n)^2 / Gamma(1 + 2/n)
		// Approximate using the rectangle area scaled by a fill factor.
		// For exponent n: fill = (2/n) * Beta(1/n, 1/n+1) but for FP, use the simpler:
		// Area ~ 4 * a * b * (1 - 0.36 / n) which is within 5% for n in [1, 8].
		const float FillFactor = FMath::Clamp(1.f - 0.36f / FMath::Max(1.f, N), 0.5f, 1.f);
		const float SectionArea = 4.f * A * B * FillFactor;
		TotalVolumeCm3 += SectionArea * SpineLength * DeltaT;
	}

	Def->SubmergedVolumeLiters = TotalVolumeCm3 / 1000.f;

	// Mass: use steel density approximation for a pressure hull.
	// Shell mass ~ surface area * thickness * steel density
	// For FP: rough estimate = volume * 0.12 (thin-shell submarine heuristic, ~120 kg/m^3 effective)
	const float VolumeCm3 = TotalVolumeCm3;
	const float VolumeM3 = VolumeCm3 / 1e6f;
	Def->BaseMassKg = VolumeM3 * 120.f;

	return true;
}

// --- Step 2: DeriveCompartments -----------------------------------------------

float USubmarineGenerator::ComputeFloorZ(
	const USubmarineGeneratorEnvelopeDef* Envelope,
	float NormalizedPosition,
	float WallThicknessCm,
	float FloorDropBiasCm) const
{
	// EvaluateRadius already bakes in the taper since M1.2. Floor sits at
	// the bottom of the cross-section, offset by wall thickness and drop bias.
	// The superellipse bottom is at -R. Floor is above that.
	const float R = Envelope->EvaluateRadius(NormalizedPosition);
	return -R + WallThicknessCm + FloorDropBiasCm;
}

float USubmarineGenerator::ComputeCompartmentVolumeLiters(
	const USubmarineGeneratorEnvelopeDef* Envelope,
	float StartNorm,
	float EndNorm,
	float FloorZLocal,
	float WallThicknessCm) const
{
	// Integrate the floodable cross-section area from StartNorm to EndNorm.
	// Floodable area = area of cross-section above FloorZLocal.
	constexpr int32 NumSamples = 16;
	const float DeltaT = (EndNorm - StartNorm) / static_cast<float>(NumSamples);
	const float SpineLength = Envelope->SpineLengthCm;
	float TotalVolumeCm3 = 0.f;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = StartNorm + (static_cast<float>(i) + 0.5f) * DeltaT;
		// EvaluateRadius already bakes in the taper since M1.2.
		const float R = Envelope->EvaluateRadius(T);
		const float InnerR = FMath::Max(10.f, R - WallThicknessCm);

		// Approximate floodable area by sampling vertical slices.
		// For each vertical position from FloorZLocal to top of section (+InnerR),
		// compute the horizontal width and integrate.
		constexpr int32 VertSamples = 16;
		const float TopZ = InnerR;
		const float BottomZ = FMath::Max(-InnerR, FloorZLocal);
		if (TopZ <= BottomZ)
		{
			continue;
		}

		const float DeltaZ = (TopZ - BottomZ) / static_cast<float>(VertSamples);
		float SliceArea = 0.f;
		for (int32 j = 0; j < VertSamples; ++j)
		{
			const float Z = BottomZ + (static_cast<float>(j) + 0.5f) * DeltaZ;
			const float HalfWidth = Envelope->EvaluateSectionHalfWidth(InnerR, Z);
			SliceArea += HalfWidth * 2.f * DeltaZ;
		}

		TotalVolumeCm3 += SliceArea * SpineLength * DeltaT;
	}

	return TotalVolumeCm3 / 1000.f;
}

ESubCompartmentType USubmarineGenerator::InferSemanticType(int32 CompartmentIndex, int32 TotalCompartments) const
{
	if (TotalCompartments <= 1)
	{
		return ESubCompartmentType::Helm;
	}

	// First compartment = Helm, last = Engine, middle = Crew (or Generic if many).
	if (CompartmentIndex == 0)
	{
		return ESubCompartmentType::Helm;
	}
	if (CompartmentIndex == TotalCompartments - 1)
	{
		return ESubCompartmentType::Engine;
	}
	return ESubCompartmentType::Crew;
}

bool USubmarineGenerator::DeriveCompartments(
	const USubmarineGeneratorEnvelopeDef* Envelope,
	const TArray<float>& BulkheadPositions,
	float WallThicknessCm,
	float FloorDropBiasCm,
	USubmarineDefinition* Def) const
{
	if (!Envelope || !Def || BulkheadPositions.Num() == 0)
	{
		return false;
	}

	const float SpineLength = Envelope->SpineLengthCm;

	// Compartments occupy the constant-radius body region of the envelope.
	// Body bounds are derived from BodyLengthFraction (authoritative control)
	// weighted by the bow/stern taper fractions. Build segment boundaries:
	// [BodyStart, Bulkhead0, Bulkhead1, ..., BodyEnd].
	float BodyStart = 0.f;
	float BodyEnd = 1.f;
	Envelope->GetBodyBounds(BodyStart, BodyEnd);

	const float MinPos = BodyStart + 0.01f;
	const float MaxPos = BodyEnd - 0.01f;

	TArray<float> Boundaries;
	Boundaries.Add(BodyStart);
	for (const float Pos : BulkheadPositions)
	{
		Boundaries.Add(FMath::Clamp(Pos, MinPos, MaxPos));
	}
	Boundaries.Add(BodyEnd);

	const int32 NumCompartments = Boundaries.Num() - 1;
	int32 CrewIndex = 0;

	for (int32 i = 0; i < NumCompartments; ++i)
	{
		const float StartNorm = Boundaries[i];
		const float EndNorm = Boundaries[i + 1];
		const float MidNorm = (StartNorm + EndNorm) * 0.5f;

		const float FloorZ = ComputeFloorZ(Envelope, MidNorm, WallThicknessCm, FloorDropBiasCm);
		// EvaluateRadius already bakes in the taper since M1.2. Within the
		// body region (MidNorm ∈ [BodyStart, BodyEnd]) it returns DefaultRadiusCm.
		const float MidR = Envelope->EvaluateRadius(MidNorm);
		const float InnerR = FMath::Max(10.f, MidR - WallThicknessCm);
		const float HalfW = Envelope->EvaluateSectionHalfWidth(InnerR, 0.f);

		FGeneratedCompartmentDef Comp;

		// Assign ID from semantic type. Crew compartments get a running index.
		const ESubCompartmentType SemanticType = InferSemanticType(i, NumCompartments);
		switch (SemanticType)
		{
		case ESubCompartmentType::Helm:
			Comp.CompartmentId = FName(TEXT("Helm"));
			break;
		case ESubCompartmentType::Engine:
			Comp.CompartmentId = FName(TEXT("Engine"));
			break;
		case ESubCompartmentType::Crew:
			Comp.CompartmentId = (CrewIndex == 0)
				? FName(TEXT("Crew"))
				: FName(*FString::Printf(TEXT("Crew%d"), CrewIndex + 1));
			++CrewIndex;
			break;
		default:
			Comp.CompartmentId = FName(*FString::Printf(TEXT("Compartment_%d"), i));
			break;
		}

		Comp.SemanticType = SemanticType;
		Comp.DisplayName = FText::FromName(Comp.CompartmentId);

		// Hydro bounds in submarine local space (X along spine, Y lateral, Z vertical).
		const float StartX = StartNorm * SpineLength;
		const float EndX = EndNorm * SpineLength;
		Comp.HydroBoundsMin = FVector(StartX, -HalfW, FloorZ);
		Comp.HydroBoundsMax = FVector(EndX, HalfW, InnerR);
		Comp.MaxWaterHeightCm = FMath::Max(1.f, InnerR - FloorZ);
		Comp.WalkableFloorZCm = FloorZ;
		Comp.CapacityLiters = ComputeCompartmentVolumeLiters(Envelope, StartNorm, EndNorm, FloorZ, WallThicknessCm);

		if (Comp.CapacityLiters <= 0.f)
		{
			UE_LOG(LogSubGenerator, Warning, TEXT("DeriveCompartments: compartment %s has zero capacity"), *Comp.CompartmentId.ToString());
			Comp.CapacityLiters = 1.f;
		}

		Def->Compartments.Add(Comp);
	}

	UE_LOG(LogSubGenerator, Log, TEXT("DeriveCompartments: %d compartments from %d bulkheads"), NumCompartments, BulkheadPositions.Num());
	return NumCompartments > 0;
}

// --- Step 3: GenerateConnections ---------------------------------------------

bool USubmarineGenerator::GenerateConnections(
	const USubmarineGeneratorSpec* Spec,
	USubmarineDefinition* Def) const
{
	if (!Spec || !Def)
	{
		return false;
	}

	const int32 NumBulkheads = Spec->BulkheadPositionsNormalized.Num();
	const int32 NumPassages = Spec->Passages.Num();

	for (int32 i = 0; i < NumBulkheads; ++i)
	{
		// Each bulkhead connects compartment[i] to compartment[i+1].
		if (i + 1 >= Def->Compartments.Num())
		{
			break;
		}

		const FBulkheadPassageDef& Passage = (i < NumPassages) ? Spec->Passages[i] : Spec->Passages.Last();

		FGeneratedConnectionDef Conn;
		Conn.ConnectionId = FName(*FString::Printf(TEXT("Bulkhead_%d"), i));
		Conn.CompartmentA = Def->Compartments[i].CompartmentId;
		Conn.CompartmentB = Def->Compartments[i + 1].CompartmentId;
		Conn.ConnectionType = Passage.PassageType;
		Conn.FlowAreaCm2 = Passage.DoorWidthCm * Passage.DoorHeightCm;
		Conn.DoorWidthCm = Passage.DoorWidthCm;
		Conn.DoorHeightCm = Passage.DoorHeightCm;
		Conn.bStartsClosed = (Passage.PassageType != EConnectionType::Open);

		// Place the connection at the bulkhead position on the spine.
		const float BulkheadX = Spec->BulkheadPositionsNormalized[i] * Spec->Envelope->SpineLengthCm;
		Conn.LocalTransform = FTransform(FVector(BulkheadX, 0.f, Def->Compartments[i].WalkableFloorZCm));

		Def->Connections.Add(Conn);
	}

	return true;
}

// --- Step 4: GenerateAirlock -------------------------------------------------

bool USubmarineGenerator::GenerateAirlock(
	const USubmarineGeneratorSpec* Spec,
	USubmarineDefinition* Def) const
{
	if (!Spec || !Def || !Spec->Envelope)
	{
		return false;
	}

	const float SpineLength = Spec->Envelope->SpineLengthCm;
	const float AirlockNorm = FMath::Clamp(Spec->AirlockPositionNormalized, 0.05f, 0.95f);
	const float AirlockX = AirlockNorm * SpineLength;

	// Find which compartment the airlock attaches to.
	FName HostCompartmentId = NAME_None;
	for (const FGeneratedCompartmentDef& Comp : Def->Compartments)
	{
		if (AirlockX >= Comp.HydroBoundsMin.X && AirlockX <= Comp.HydroBoundsMax.X)
		{
			HostCompartmentId = Comp.CompartmentId;
			break;
		}
	}

	if (HostCompartmentId.IsNone())
	{
		// Fallback: attach to the last compartment.
		if (Def->Compartments.Num() > 0)
		{
			HostCompartmentId = Def->Compartments.Last().CompartmentId;
		}
		else
		{
			UE_LOG(LogSubGenerator, Error, TEXT("GenerateAirlock: no compartments to attach to"));
			return false;
		}
	}

	// EvaluateRadius already bakes in the taper since M1.2.
	const float EffR = Spec->Envelope->EvaluateRadius(AirlockNorm);

	// Airlock dimensions.
	constexpr float AirlockLengthCm = 120.f;
	constexpr float AirlockWidthCm = 100.f;
	constexpr float AirlockHeightCm = 200.f;
	constexpr float AirlockDoorAreaCm2 = 80.f * 180.f;

	// Compute airlock local position based on side.
	FVector AirlockCenter(AirlockX, 0.f, 0.f);
	switch (Spec->AirlockSide)
	{
	case EAirlockSide::Port:
		AirlockCenter.Y = -(EffR + AirlockWidthCm * 0.5f);
		break;
	case EAirlockSide::Starboard:
		AirlockCenter.Y = EffR + AirlockWidthCm * 0.5f;
		break;
	case EAirlockSide::Top:
		AirlockCenter.Z = EffR + AirlockHeightCm * 0.5f;
		break;
	}

	// Create airlock compartment.
	FGeneratedCompartmentDef AirlockComp;
	AirlockComp.CompartmentId = FName(TEXT("Airlock"));
	AirlockComp.DisplayName = FText::FromString(TEXT("Airlock"));
	AirlockComp.SemanticType = ESubCompartmentType::Airlock;

	const FVector HalfExtent(AirlockLengthCm * 0.5f, AirlockWidthCm * 0.5f, AirlockHeightCm * 0.5f);
	AirlockComp.HydroBoundsMin = AirlockCenter - HalfExtent;
	AirlockComp.HydroBoundsMax = AirlockCenter + HalfExtent;
	AirlockComp.MaxWaterHeightCm = AirlockHeightCm;
	AirlockComp.WalkableFloorZCm = AirlockCenter.Z - HalfExtent.Z;
	AirlockComp.CapacityLiters = (AirlockLengthCm * AirlockWidthCm * AirlockHeightCm) / 1000.f;

	Def->Compartments.Add(AirlockComp);

	// Inner door: airlock <-> host compartment.
	FGeneratedConnectionDef InnerDoor;
	InnerDoor.ConnectionId = FName(TEXT("Airlock_Inner"));
	InnerDoor.CompartmentA = AirlockComp.CompartmentId;
	InnerDoor.CompartmentB = HostCompartmentId;
	InnerDoor.ConnectionType = EConnectionType::Door;
	InnerDoor.FlowAreaCm2 = AirlockDoorAreaCm2;
	InnerDoor.DoorWidthCm = 80.f;
	InnerDoor.DoorHeightCm = 180.f;
	InnerDoor.bStartsClosed = true;
	InnerDoor.LocalTransform = FTransform(AirlockCenter);
	Def->Connections.Add(InnerDoor);

	// Outer hatch: airlock <-> exterior (CompartmentB = NAME_None).
	FGeneratedConnectionDef OuterHatch;
	OuterHatch.ConnectionId = FName(TEXT("Airlock_Outer"));
	OuterHatch.CompartmentA = AirlockComp.CompartmentId;
	OuterHatch.CompartmentB = NAME_None;
	OuterHatch.ConnectionType = EConnectionType::ExteriorHatch;
	OuterHatch.FlowAreaCm2 = AirlockDoorAreaCm2;
	OuterHatch.bStartsClosed = true;
	OuterHatch.LocalTransform = FTransform(AirlockCenter);
	Def->Connections.Add(OuterHatch);

	UE_LOG(LogSubGenerator, Log, TEXT("GenerateAirlock: attached to %s at X=%.1f"), *HostCompartmentId.ToString(), AirlockX);
	return true;
}

// --- Step 5: BuildFloodGraph -------------------------------------------------

bool USubmarineGenerator::BuildFloodGraph(USubmarineDefinition* Def) const
{
	if (!Def)
	{
		return false;
	}

	FCompiledFloodGraph& Graph = Def->FloodGraph;
	Graph.Volumes.Reset();
	Graph.Edges.Reset();

	// One volume per compartment.
	for (const FGeneratedCompartmentDef& Comp : Def->Compartments)
	{
		FDerivedFloodVolume Vol;
		Vol.VolumeId = Comp.CompartmentId;
		Vol.CapacityLiters = Comp.CapacityLiters;
		Vol.BoundsMin = Comp.HydroBoundsMin;
		Vol.BoundsMax = Comp.HydroBoundsMax;
		Graph.Volumes.Add(Vol);
	}

	// One edge per connection.
	for (const FGeneratedConnectionDef& Conn : Def->Connections)
	{
		FFloodGraphEdge Edge;
		Edge.VolumeA = Conn.CompartmentA;
		Edge.VolumeB = Conn.CompartmentB;
		Edge.ClosureId = (Conn.ConnectionType != EConnectionType::Open) ? Conn.ConnectionId : NAME_None;
		Edge.PassageAreaCm2 = Conn.FlowAreaCm2;
		Edge.bExteriorEdge = Conn.CompartmentB.IsNone();
		Graph.Edges.Add(Edge);
	}

	return Graph.Volumes.Num() > 0;
}

// --- Step 6: PlaceStations ---------------------------------------------------

bool USubmarineGenerator::PlaceStations(
	const USubmarineGeneratorSpec* Spec,
	USubmarineDefinition* Def) const
{
	if (!Spec || !Def)
	{
		return false;
	}

	int32 StationIndex = 0;
	for (const ESubStationType StationType : Spec->RequestedStations)
	{
		if (StationType == ESubStationType::None)
		{
			continue;
		}

		// Determine which compartment to place this station in.
		FName TargetCompartmentId = NAME_None;
		switch (StationType)
		{
		case ESubStationType::Helm:
			// Helm station goes in the Helm compartment.
			for (const FGeneratedCompartmentDef& C : Def->Compartments)
			{
				if (C.SemanticType == ESubCompartmentType::Helm)
				{
					TargetCompartmentId = C.CompartmentId;
					break;
				}
			}
			break;

		case ESubStationType::Engine:
			// Engine station goes in the Engine compartment.
			for (const FGeneratedCompartmentDef& C : Def->Compartments)
			{
				if (C.SemanticType == ESubCompartmentType::Engine)
				{
					TargetCompartmentId = C.CompartmentId;
					break;
				}
			}
			break;

		default:
			// Other stations go in the first Crew compartment, or Generic.
			for (const FGeneratedCompartmentDef& C : Def->Compartments)
			{
				if (C.SemanticType == ESubCompartmentType::Crew)
				{
					TargetCompartmentId = C.CompartmentId;
					break;
				}
			}
			if (TargetCompartmentId.IsNone() && Def->Compartments.Num() > 0)
			{
				TargetCompartmentId = Def->Compartments[0].CompartmentId;
			}
			break;
		}

		if (TargetCompartmentId.IsNone())
		{
			UE_LOG(LogSubGenerator, Warning, TEXT("PlaceStations: no valid compartment for station type %d"), static_cast<int32>(StationType));
			continue;
		}

		const FGeneratedCompartmentDef* TargetComp = Def->FindCompartment(TargetCompartmentId);
		if (!TargetComp)
		{
			continue;
		}

		FGeneratedStationSlotDef Slot;
		Slot.StationId = FName(*FString::Printf(TEXT("Station_%s_%d"), *TargetCompartmentId.ToString(), StationIndex));
		Slot.StationType = StationType;
		Slot.CompartmentId = TargetCompartmentId;

		// Place at the center of the compartment, on the floor.
		const FVector Center = (TargetComp->HydroBoundsMin + TargetComp->HydroBoundsMax) * 0.5f;
		Slot.LocalTransform = FTransform(FVector(Center.X, Center.Y, TargetComp->WalkableFloorZCm));

		Def->StationSlots.Add(Slot);
		++StationIndex;
	}

	return true;
}

// --- Step 7: PlaceSpawns -----------------------------------------------------

bool USubmarineGenerator::PlaceSpawns(USubmarineDefinition* Def) const
{
	if (!Def)
	{
		return false;
	}

	// Place one Pilot spawn in the Helm compartment, and one Crew spawn in the first Crew compartment.
	const FGeneratedCompartmentDef* HelmComp = nullptr;
	const FGeneratedCompartmentDef* CrewComp = nullptr;

	for (const FGeneratedCompartmentDef& C : Def->Compartments)
	{
		if (!HelmComp && C.SemanticType == ESubCompartmentType::Helm)
		{
			HelmComp = &C;
		}
		if (!CrewComp && C.SemanticType == ESubCompartmentType::Crew)
		{
			CrewComp = &C;
		}
	}

	// Fallback: if no Helm found, use first compartment. If no Crew, use second (or first).
	if (!HelmComp && Def->Compartments.Num() > 0)
	{
		HelmComp = &Def->Compartments[0];
	}
	if (!CrewComp)
	{
		CrewComp = Def->Compartments.Num() > 1 ? &Def->Compartments[1] : HelmComp;
	}

	auto MakeSpawn = [](const FGeneratedCompartmentDef* Comp, FName Id, ESpawnRole Role) -> FGeneratedSpawnPointDef
	{
		FGeneratedSpawnPointDef Spawn;
		Spawn.SpawnId = Id;
		Spawn.Role = Role;
		if (Comp)
		{
			const FVector Center = (Comp->HydroBoundsMin + Comp->HydroBoundsMax) * 0.5f;
			Spawn.LocalTransform = FTransform(FVector(Center.X, Center.Y, Comp->WalkableFloorZCm));
		}
		return Spawn;
	};

	Def->SpawnPoints.Add(MakeSpawn(HelmComp, FName(TEXT("Spawn_Pilot")), ESpawnRole::Pilot));

	if (CrewComp && CrewComp != HelmComp)
	{
		Def->SpawnPoints.Add(MakeSpawn(CrewComp, FName(TEXT("Spawn_Crew")), ESpawnRole::Crew));
	}

	return Def->SpawnPoints.Num() > 0;
}
