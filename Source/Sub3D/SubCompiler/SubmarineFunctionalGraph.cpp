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
}

bool USubmarineFunctionalGraph::ValidateGraph(TArray<FLayoutValidationMessage>& OutMessages) const
{
	OutMessages.Reset();

	if (Compartments.Num() < 2)
	{
		AddValidationMessage(
			OutMessages,
			ELayoutValidationSeverity::Error,
			NAME_None,
			TEXT("Minimum 2 compartiments"));
		return false;
	}

	TSet<FName> UniqueIds;
	TSet<FName> ValidIds;
	TMap<FName, TArray<FName>> Adjacency;

	for (const FCompartmentNode& Compartment : Compartments)
	{
		if (Compartment.CompartmentId.IsNone())
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				NAME_None,
				TEXT("CompartmentId ne peut pas etre vide"));
			continue;
		}

		if (UniqueIds.Contains(Compartment.CompartmentId))
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				Compartment.CompartmentId,
				FString::Printf(TEXT("CompartmentId duplique: %s"), *Compartment.CompartmentId.ToString()));
			continue;
		}

		UniqueIds.Add(Compartment.CompartmentId);
		ValidIds.Add(Compartment.CompartmentId);
		Adjacency.Add(Compartment.CompartmentId, TArray<FName>());
	}

	for (const FPassageEdge& Passage : Passages)
	{
		if (Passage.FromCompartmentId.IsNone() || Passage.ToCompartmentId.IsNone())
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				NAME_None,
				TEXT("Chaque passage doit referencer deux compartiments valides"));
			continue;
		}

		if (Passage.FromCompartmentId == Passage.ToCompartmentId)
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				Passage.FromCompartmentId,
				FString::Printf(TEXT("Passage auto-reference interdit pour %s"), *Passage.FromCompartmentId.ToString()));
			continue;
		}

		if (!ValidIds.Contains(Passage.FromCompartmentId) || !ValidIds.Contains(Passage.ToCompartmentId))
		{
			AddValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Error,
				NAME_None,
				FString::Printf(
					TEXT("Passage invalide: %s -> %s"),
					*Passage.FromCompartmentId.ToString(),
					*Passage.ToCompartmentId.ToString()));
			continue;
		}

		Adjacency.FindOrAdd(Passage.FromCompartmentId).AddUnique(Passage.ToCompartmentId);
		Adjacency.FindOrAdd(Passage.ToCompartmentId).AddUnique(Passage.FromCompartmentId);
	}

	bool bHasError = OutMessages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
	{
		return Message.Severity == ELayoutValidationSeverity::Error;
	});

	if (!bHasError && ValidIds.Num() > 0)
	{
		TArray<FName> Queue;
		TSet<FName> Visited;

		const FName StartId = *ValidIds.CreateConstIterator();
		Queue.Add(StartId);
		Visited.Add(StartId);

		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			const FName CurrentId = Queue[Index];
			const TArray<FName>* Neighbors = Adjacency.Find(CurrentId);
			if (!Neighbors)
			{
				continue;
			}

			for (const FName NeighborId : *Neighbors)
			{
				if (!Visited.Contains(NeighborId))
				{
					Visited.Add(NeighborId);
					Queue.Add(NeighborId);
				}
			}
		}

		for (const FName CompartmentId : ValidIds)
		{
			if (!Visited.Contains(CompartmentId))
			{
				AddValidationMessage(
					OutMessages,
					ELayoutValidationSeverity::Error,
					CompartmentId,
					FString::Printf(TEXT("Compartiment isole: %s"), *CompartmentId.ToString()));
			}
		}
	}

	return !OutMessages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
	{
		return Message.Severity == ELayoutValidationSeverity::Error;
	});
}
