#include "SubmarineLayoutSolver.h"

#include "SubmarineEnvelopeDef.h"
#include "SubmarineFunctionalGraph.h"

namespace
{
void AddValidationMessage(
	TArray<FLayoutValidationMessage>& OutMessages,
	ELayoutValidationSeverity Severity,
	FName RelatedId,
	const FString& Message)
{
	FLayoutValidationMessage ValidationMessage;
	ValidationMessage.Severity = Severity;
	ValidationMessage.RelatedId = RelatedId;
	ValidationMessage.Message = FText::FromString(Message);
	OutMessages.Add(MoveTemp(ValidationMessage));
}

const FPassageEdge* FindPassage(
	const USubmarineFunctionalGraph* Graph,
	FName FirstCompartmentId,
	FName SecondCompartmentId)
{
	if (!Graph)
	{
		return nullptr;
	}

	return Graph->Passages.FindByPredicate([FirstCompartmentId, SecondCompartmentId](const FPassageEdge& Passage)
	{
		return (Passage.FromCompartmentId == FirstCompartmentId && Passage.ToCompartmentId == SecondCompartmentId)
			|| (Passage.FromCompartmentId == SecondCompartmentId && Passage.ToCompartmentId == FirstCompartmentId);
	});
}
}

bool USubmarineLayoutSolver::Solve(
	const USubmarineEnvelopeDef* Envelope,
	const USubmarineFunctionalGraph* Graph,
	FSubmarineLayoutSolution& OutSolution,
	TArray<FLayoutValidationMessage>& OutMessages)
{
	OutSolution = FSubmarineLayoutSolution();
	OutMessages.Reset();

	if (!Envelope)
	{
		AddValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Envelope invalide"));
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	if (!Graph)
	{
		AddValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Graph invalide"));
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	if (!Graph->ValidateGraph(OutMessages))
	{
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	// ---- Separate locked and free compartments ----

	TArray<FCompartmentNode> LockedCompartments;
	TArray<FCompartmentNode> FreeCompartments;
	for (const FCompartmentNode& Compartment : Graph->Compartments)
	{
		if (Compartment.bLocked)
		{
			LockedCompartments.Add(Compartment);
		}
		else
		{
			FreeCompartments.Add(Compartment);
		}
	}

	const int32 TotalCompartmentCount = LockedCompartments.Num() + FreeCompartments.Num();
	if (TotalCompartmentCount > Envelope->MaxCompartments)
	{
		AddValidationMessage(
			OutMessages,
			ELayoutValidationSeverity::Error,
			NAME_None,
			FString::Printf(TEXT("Trop de compartiments (%d > max %d)"), TotalCompartmentCount, Envelope->MaxCompartments));
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	// ---- Phase 1: Validate locks ----

	LockedCompartments.Sort([](const FCompartmentNode& A, const FCompartmentNode& B)
	{
		return A.LockedSpineRangeCm.X < B.LockedSpineRangeCm.X;
	});

	for (const FCompartmentNode& Lock : LockedCompartments)
	{
		if (Lock.LockedSpineRangeCm.IsNearlyZero())
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				Lock.CompartmentId,
				FString::Printf(TEXT("Lock range non defini pour %s"), *Lock.CompartmentId.ToString()));
		}
		else if (Lock.LockedSpineRangeCm.X >= Lock.LockedSpineRangeCm.Y)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				Lock.CompartmentId,
				FString::Printf(TEXT("Lock range inverse pour %s (%.1f >= %.1f)"),
					*Lock.CompartmentId.ToString(),
					Lock.LockedSpineRangeCm.X,
					Lock.LockedSpineRangeCm.Y));
		}
		else
		{
			if (Lock.LockedSpineRangeCm.X < -KINDA_SMALL_NUMBER || Lock.LockedSpineRangeCm.Y > Envelope->SpineLengthCm + KINDA_SMALL_NUMBER)
			{
				AddValidationMessage(
					OutMessages,
					ELayoutValidationSeverity::Error,
					Lock.CompartmentId,
					FString::Printf(TEXT("Lock range hors enveloppe pour %s ([%.1f, %.1f] vs spine %.1f)"),
						*Lock.CompartmentId.ToString(),
						Lock.LockedSpineRangeCm.X,
						Lock.LockedSpineRangeCm.Y,
						Envelope->SpineLengthCm));
			}

			const float LockLength = Lock.LockedSpineRangeCm.Y - Lock.LockedSpineRangeCm.X;
			if (LockLength < Lock.MinLengthCm - KINDA_SMALL_NUMBER)
			{
				AddValidationMessage(
					OutMessages,
					ELayoutValidationSeverity::Warning,
					Lock.CompartmentId,
					FString::Printf(TEXT("Lock %s plus court que MinLength (%.1f < %.1f)"),
						*Lock.CompartmentId.ToString(),
						LockLength,
						Lock.MinLengthCm));
			}
		}
	}

	for (int32 LockIndex = 0; LockIndex + 1 < LockedCompartments.Num(); ++LockIndex)
	{
		const FCompartmentNode& A = LockedCompartments[LockIndex];
		const FCompartmentNode& B = LockedCompartments[LockIndex + 1];
		if (A.LockedSpineRangeCm.Y > B.LockedSpineRangeCm.X + KINDA_SMALL_NUMBER)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				A.CompartmentId,
				FString::Printf(TEXT("Locks %s et %s se chevauchent ([%.1f,%.1f] vs [%.1f,%.1f])"),
					*A.CompartmentId.ToString(),
					*B.CompartmentId.ToString(),
					A.LockedSpineRangeCm.X, A.LockedSpineRangeCm.Y,
					B.LockedSpineRangeCm.X, B.LockedSpineRangeCm.Y));
		}
	}

	const bool bHasLockErrors = OutMessages.ContainsByPredicate([](const FLayoutValidationMessage& Msg)
	{
		return Msg.Severity == ELayoutValidationSeverity::Error;
	});
	if (bHasLockErrors)
	{
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	// ---- Phase 2: Build gaps between locks ----

	struct FGap
	{
		float Start;
		float End;
		TArray<int32> FreeIndices;
	};

	TArray<FGap> Gaps;
	float PreviousEnd = 0.f;
	for (const FCompartmentNode& Lock : LockedCompartments)
	{
		const float GapStart = PreviousEnd;
		const float GapEnd = Lock.LockedSpineRangeCm.X;
		if (GapEnd - GapStart > KINDA_SMALL_NUMBER)
		{
			FGap Gap;
			Gap.Start = GapStart;
			Gap.End = GapEnd;
			Gaps.Add(Gap);
		}
		PreviousEnd = Lock.LockedSpineRangeCm.Y;
	}
	if (Envelope->SpineLengthCm - PreviousEnd > KINDA_SMALL_NUMBER)
	{
		FGap Gap;
		Gap.Start = PreviousEnd;
		Gap.End = Envelope->SpineLengthCm;
		Gaps.Add(Gap);
	}

	// ---- Phase 3: Assign free compartments to gaps ----

	FreeCompartments.Sort([](const FCompartmentNode& A, const FCompartmentNode& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority < B.Priority;
		}
		return A.CompartmentId.LexicalLess(B.CompartmentId);
	});

	// Priority duplicate warning (all compartments)
	{
		TMap<int32, TArray<FName>> CompartmentsByPriority;
		for (const FCompartmentNode& Compartment : FreeCompartments)
		{
			CompartmentsByPriority.FindOrAdd(Compartment.Priority).Add(Compartment.CompartmentId);
		}
		for (const TPair<int32, TArray<FName>>& Pair : CompartmentsByPriority)
		{
			if (Pair.Value.Num() <= 1)
			{
				continue;
			}
			TArray<FString> Ids;
			Ids.Reserve(Pair.Value.Num());
			for (const FName CompartmentId : Pair.Value)
			{
				Ids.Add(CompartmentId.ToString());
			}
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Warning,
				Pair.Value[0],
				FString::Printf(
					TEXT("Priorite %d dupliquee pour %s. Le solveur lineaire departage ensuite par CompartmentId."),
					Pair.Key,
					*FString::Join(Ids, TEXT(", "))));
		}
	}

	// Check total free min length fits in available gaps
	float TotalFreeMinLength = 0.f;
	for (const FCompartmentNode& Comp : FreeCompartments)
	{
		TotalFreeMinLength += Comp.MinLengthCm;
	}
	float TotalGapLength = 0.f;
	for (const FGap& Gap : Gaps)
	{
		TotalGapLength += (Gap.End - Gap.Start);
	}
	if (TotalFreeMinLength > TotalGapLength + KINDA_SMALL_NUMBER)
	{
		AddValidationMessage(
			OutMessages,
			ELayoutValidationSeverity::Error,
			NAME_None,
			FString::Printf(
				TEXT("Compartiments libres trop longs pour les gaps disponibles (besoin %.1f cm, disponible %.1f cm)"),
				TotalFreeMinLength,
				TotalGapLength));
		OutSolution.ValidationMessages = OutMessages;
		return false;
	}

	// Greedy left-to-right assignment of free compartments to gaps
	TArray<float> GapAssignedMinLength;
	GapAssignedMinLength.SetNumZeroed(Gaps.Num());
	int32 CurrentGapIndex = 0;

	for (int32 FreeIndex = 0; FreeIndex < FreeCompartments.Num(); ++FreeIndex)
	{
		const float MinLen = FreeCompartments[FreeIndex].MinLengthCm;
		bool bAssigned = false;

		while (CurrentGapIndex < Gaps.Num())
		{
			const float GapLen = Gaps[CurrentGapIndex].End - Gaps[CurrentGapIndex].Start;
			if (GapAssignedMinLength[CurrentGapIndex] + MinLen <= GapLen + KINDA_SMALL_NUMBER)
			{
				Gaps[CurrentGapIndex].FreeIndices.Add(FreeIndex);
				GapAssignedMinLength[CurrentGapIndex] += MinLen;
				bAssigned = true;
				break;
			}
			++CurrentGapIndex;
		}

		if (!bAssigned)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				FreeCompartments[FreeIndex].CompartmentId,
				FString::Printf(
					TEXT("Compartiment %s ne peut pas etre place dans les gaps disponibles"),
					*FreeCompartments[FreeIndex].CompartmentId.ToString()));
			OutSolution.ValidationMessages = OutMessages;
			return false;
		}
	}

	// ---- Place all compartments in spine order ----

	OutSolution.Compartments.Reserve(TotalCompartmentCount);
	OutSolution.Stations.Reserve(TotalCompartmentCount * 2);
	OutSolution.Bulkheads.Reserve(FMath::Max(0, TotalCompartmentCount - 1));

	int32 TotalStationCount = 0;
	int32 TotalDoorCount = 0;
	int32 TotalCrewCapacity = 0;
	float EstimatedVolumeCm3 = 0.f;
	float BallastCapacityLiters = 0.f;

	auto PlaceOneCompartment = [&](const FCompartmentNode& Compartment, float SpineStart, float SpineEnd)
	{
		const float ActualLengthCm = SpineEnd - SpineStart;
		const float SpineMid = (SpineStart + SpineEnd) * 0.5f;
		const float NormalizedPos = (Envelope->SpineLengthCm > KINDA_SMALL_NUMBER)
			? FMath::Clamp(SpineMid / Envelope->SpineLengthCm, 0.f, 1.f)
			: 0.f;

		const float EffectiveRadius = Envelope->EvaluateRadius(NormalizedPos);
		const float RequestedFloorOffset = -FMath::Max(0.f, Envelope->FloorDropBiasCm);
		const float LowestAllowedFloorOffset = -EffectiveRadius;
		const float HighestAllowedFloorOffset = FMath::Max(LowestAllowedFloorOffset, EffectiveRadius - Compartment.MinHeightCm);
		const float FloorOffset = FMath::Clamp(RequestedFloorOffset, LowestAllowedFloorOffset, HighestAllowedFloorOffset);
		const float ClearanceHeight = EffectiveRadius - FloorOffset;
		const float FloorHalfWidth = FMath::Sqrt(FMath::Max(0.f, FMath::Square(EffectiveRadius) - FMath::Square(FloorOffset)));
		const float FloorWidth = FloorHalfWidth * 2.f;

		if (FloorWidth < Compartment.MinWidthCm)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Warning,
				Compartment.CompartmentId,
				FString::Printf(
					TEXT("Compartiment %s : largeur au sol %.1f cm < minimum %.1f cm"),
					*Compartment.CompartmentId.ToString(),
					FloorWidth,
					Compartment.MinWidthCm));
		}

		if (ClearanceHeight < Compartment.MinHeightCm)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				Compartment.CompartmentId,
				FString::Printf(
					TEXT("Compartiment %s : hauteur libre %.1f cm < minimum %.1f cm"),
					*Compartment.CompartmentId.ToString(),
					ClearanceHeight,
					Compartment.MinHeightCm));
		}

		FCompartmentPlacement Placement;
		Placement.CompartmentId = Compartment.CompartmentId;
		Placement.Type = Compartment.Type;
		Placement.SpineStartCm = SpineStart;
		Placement.SpineEndCm = SpineEnd;
		Placement.EffectiveRadiusCm = EffectiveRadius;
		Placement.FloorOffsetCm = FloorOffset;
		Placement.ClearanceHeightCm = ClearanceHeight;
		Placement.FloorWidthCm = FloorWidth;
		OutSolution.Compartments.Add(Placement);

		const float CompartmentMidX = SpineMid;
		for (int32 StationIndex = 0; StationIndex < Compartment.RequiredSystems.Num(); ++StationIndex)
		{
			const EWallSide WallSide = (StationIndex % 2 == 0) ? EWallSide::Port : EWallSide::Starboard;
			const float WallY = (WallSide == EWallSide::Port ? -1.f : 1.f) * FMath::Max(0.f, FloorHalfWidth - 20.f);
			const float Yaw = (WallSide == EWallSide::Port) ? 90.f : -90.f;

			FStationPlacement StationPlacement;
			StationPlacement.StationType = Compartment.RequiredSystems[StationIndex];
			StationPlacement.CompartmentId = Compartment.CompartmentId;
			StationPlacement.WallSide = WallSide;
			StationPlacement.ClearanceRectCm = FVector2D(100.f, 120.f);
			StationPlacement.LocalTransform = FTransform(
				FRotator(0.f, Yaw, 0.f),
				FVector(CompartmentMidX, WallY, FloorOffset));
			OutSolution.Stations.Add(StationPlacement);
			++TotalStationCount;
		}

		TotalCrewCapacity += FMath::Max(0, Compartment.CrewCapacity);
		EstimatedVolumeCm3 += ActualLengthCm * FloorWidth * ClearanceHeight;
		if (Compartment.Type == ECompartmentType::Ballast)
		{
			BallastCapacityLiters += (ActualLengthCm * FloorWidth * ClearanceHeight) / 1000.f;
		}
	};

	auto PlaceGapCompartments = [&](const FGap& Gap)
	{
		if (Gap.FreeIndices.Num() == 0)
		{
			return;
		}

		float GapMinLength = 0.f;
		for (int32 FI : Gap.FreeIndices)
		{
			GapMinLength += FreeCompartments[FI].MinLengthCm;
		}
		const float GapLength = Gap.End - Gap.Start;
		const float GapSlack = GapLength - GapMinLength;
		float Cursor = Gap.Start;

		for (int32 FI : Gap.FreeIndices)
		{
			const FCompartmentNode& Comp = FreeCompartments[FI];
			const float Bonus = (GapMinLength > KINDA_SMALL_NUMBER)
				? GapSlack * (Comp.MinLengthCm / GapMinLength)
				: 0.f;
			const float ActualLength = Comp.MinLengthCm + Bonus;
			PlaceOneCompartment(Comp, Cursor, Cursor + ActualLength);
			Cursor += ActualLength;
		}
	};

	// Interleave gaps and locks in spine order
	int32 LockIdx = 0;
	int32 GapIdx = 0;
	while (GapIdx < Gaps.Num() || LockIdx < LockedCompartments.Num())
	{
		const bool bGapNext = (GapIdx < Gaps.Num())
			&& (LockIdx >= LockedCompartments.Num() || Gaps[GapIdx].Start < LockedCompartments[LockIdx].LockedSpineRangeCm.X);

		if (bGapNext)
		{
			PlaceGapCompartments(Gaps[GapIdx]);
			++GapIdx;
		}
		else if (LockIdx < LockedCompartments.Num())
		{
			const FCompartmentNode& Lock = LockedCompartments[LockIdx];
			PlaceOneCompartment(Lock, Lock.LockedSpineRangeCm.X, Lock.LockedSpineRangeCm.Y);
			++LockIdx;
		}
	}

	// ---- Bulkheads (same as before) ----

	TMap<FName, int32> CompartmentIndicesById;
	CompartmentIndicesById.Reserve(OutSolution.Compartments.Num());
	for (int32 CompartmentIndex = 0; CompartmentIndex < OutSolution.Compartments.Num(); ++CompartmentIndex)
	{
		CompartmentIndicesById.Add(OutSolution.Compartments[CompartmentIndex].CompartmentId, CompartmentIndex);
	}

	for (const FPassageEdge& Passage : Graph->Passages)
	{
		const int32* FromIndex = CompartmentIndicesById.Find(Passage.FromCompartmentId);
		const int32* ToIndex = CompartmentIndicesById.Find(Passage.ToCompartmentId);
		if (!FromIndex || !ToIndex)
		{
			continue;
		}

		if (FMath::Abs(*FromIndex - *ToIndex) != 1)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Warning,
				Passage.FromCompartmentId,
				FString::Printf(
					TEXT("Passage %s -> %s non adjacent dans l'ordre resolu (%d, %d). Le solveur lineaire ne generera pas de porte ici."),
					*Passage.FromCompartmentId.ToString(),
					*Passage.ToCompartmentId.ToString(),
					*FromIndex,
					*ToIndex));
		}
	}

	for (int32 CompartmentIndex = 0; CompartmentIndex + 1 < OutSolution.Compartments.Num(); ++CompartmentIndex)
	{
		const FCompartmentPlacement& ForeComp = OutSolution.Compartments[CompartmentIndex];
		const FCompartmentPlacement& AftComp = OutSolution.Compartments[CompartmentIndex + 1];
		const FPassageEdge* Passage = FindPassage(Graph, ForeComp.CompartmentId, AftComp.CompartmentId);

		FBulkheadPlacement Bulkhead;
		Bulkhead.SpinePositionCm = ForeComp.SpineEndCm;
		Bulkhead.RadiusCm = FMath::Min(ForeComp.EffectiveRadiusCm, AftComp.EffectiveRadiusCm);
		Bulkhead.ForeCompartmentId = ForeComp.CompartmentId;
		Bulkhead.AftCompartmentId = AftComp.CompartmentId;

		if (Passage)
		{
			Bulkhead.PassageType = Passage->Type;
			Bulkhead.DoorWidthCm = Passage->MinWidthCm;
			Bulkhead.DoorHeightCm = Passage->MinHeightCm;
			const float FloorLevel = FMath::Max(ForeComp.FloorOffsetCm, AftComp.FloorOffsetCm);
			Bulkhead.DoorOffsetCm = FVector2D(0.f, FloorLevel + (Bulkhead.DoorHeightCm * 0.5f));

			if (Bulkhead.PassageType != EPassageType::SealedBulkhead)
			{
				++TotalDoorCount;
			}
		}
		else
		{
			Bulkhead.PassageType = EPassageType::SealedBulkhead;
			Bulkhead.DoorWidthCm = 0.f;
			Bulkhead.DoorHeightCm = 0.f;
			Bulkhead.DoorOffsetCm = FVector2D::ZeroVector;

			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Warning,
				ForeComp.CompartmentId,
				FString::Printf(
					TEXT("Pas de passage explicite entre %s et %s : cloison scellee"),
					*ForeComp.CompartmentId.ToString(),
					*AftComp.CompartmentId.ToString()));
		}

		OutSolution.Bulkheads.Add(Bulkhead);
	}

	// ---- Metrics ----

	OutSolution.Metrics.TotalLengthCm = Envelope->SpineLengthCm;
	OutSolution.Metrics.EstimatedMassKg = EstimatedVolumeCm3 * 0.001f;
	OutSolution.Metrics.EstimatedVolumeLiters = EstimatedVolumeCm3 / 1000.f;
	OutSolution.Metrics.BallastCapacityLiters = BallastCapacityLiters;
	OutSolution.Metrics.CompartmentCount = OutSolution.Compartments.Num();
	OutSolution.Metrics.DoorCount = TotalDoorCount;
	OutSolution.Metrics.StationCount = TotalStationCount;
	OutSolution.Metrics.TotalCrewCapacity = TotalCrewCapacity;
	OutSolution.ValidationMessages = OutMessages;

	return !OutSolution.HasErrors();
}
