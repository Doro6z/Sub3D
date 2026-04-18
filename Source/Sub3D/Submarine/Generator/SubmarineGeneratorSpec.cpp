#include "SubmarineGeneratorSpec.h"

#if WITH_EDITOR
void USubmarineGeneratorSpec::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Pad or trim Passages to match bulkhead count before sorting
	while (Passages.Num() < BulkheadPositionsNormalized.Num())
	{
		Passages.Add(FBulkheadPassageDef());
	}
	if (Passages.Num() > BulkheadPositionsNormalized.Num())
	{
		Passages.SetNum(BulkheadPositionsNormalized.Num());
	}

	// Clamp positions to (0, 1) exclusive
	for (int32 i = 0; i < BulkheadPositionsNormalized.Num(); ++i)
	{
		BulkheadPositionsNormalized[i] = FMath::Clamp(BulkheadPositionsNormalized[i], 0.01f, 0.99f);
	}

	// Sort by position, keeping each Passage paired with its bulkhead
	if (BulkheadPositionsNormalized.Num() > 1)
	{
		// Build index array, sort by position, reorder both arrays
		TArray<int32> Order;
		Order.SetNum(BulkheadPositionsNormalized.Num());
		for (int32 i = 0; i < Order.Num(); ++i)
		{
			Order[i] = i;
		}
		Order.Sort([this](int32 A, int32 B)
		{
			return BulkheadPositionsNormalized[A] < BulkheadPositionsNormalized[B];
		});

		TArray<float> SortedPositions;
		TArray<FBulkheadPassageDef> SortedPassages;
		SortedPositions.Reserve(Order.Num());
		SortedPassages.Reserve(Order.Num());
		for (int32 Idx : Order)
		{
			SortedPositions.Add(BulkheadPositionsNormalized[Idx]);
			SortedPassages.Add(Passages[Idx]);
		}
		BulkheadPositionsNormalized = MoveTemp(SortedPositions);
		Passages = MoveTemp(SortedPassages);
	}
}
#endif
